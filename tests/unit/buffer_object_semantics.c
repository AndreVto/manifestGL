#include "mgl/gl.h"
#include "abi/proc_address.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_native;
    GLuint last_bound;
    GLenum last_bind_target;
    GLenum last_data_target;
    GLsizeiptr last_data_size;
    GLenum last_data_usage;
    GLenum last_subdata_target;
    GLintptr last_subdata_offset;
    GLsizeiptr last_subdata_size;
    unsigned gen_calls;
    unsigned delete_calls;
    unsigned bind_calls;
    unsigned data_calls;
    unsigned subdata_calls;
} cap;

static void fake_gen_buffers(GLsizei n, GLuint *names) {
    GLsizei i;
    ++cap.gen_calls;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_native;
}

static void fake_delete_buffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
    ++cap.delete_calls;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    cap.last_bind_target = target;
    cap.last_bound = name;
    ++cap.bind_calls;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)data;
    cap.last_data_target = target;
    cap.last_data_size = size;
    cap.last_data_usage = usage;
    ++cap.data_calls;
}

static void fake_buffer_subdata(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    (void)data;
    cap.last_subdata_target = target;
    cap.last_subdata_offset = offset;
    cap.last_subdata_size = size;
    ++cap.subdata_calls;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "buffer object semantics failure %d: %s\n", n, message);
    return n;
}

#define EXPECT_BEGIN_END_ERROR(NUM, STMT)                                                          \
    do {                                                                                           \
        glBegin(GL_POINTS);                                                                        \
        STMT;                                                                                      \
        glEnd();                                                                                   \
        if (glGetError() != GL_INVALID_OPERATION)                                                  \
            return fail((NUM), #STMT " begin/end legality");                                       \
    } while (0)

/* The name index must remain correct with many objects, hash collisions,
 * deletion, and a deleted name whose old storage is still texture-retained. */
static int test_buffer_index(struct mgl_context *ctx) {
    enum { BUFFER_COUNT = 2200 };
    struct mgl_buffer *objects[BUFFER_COUNT];
    struct mgl_buffer *retained, *replacement;
    const GLuint first_name = 0x10000000u;
    unsigned i;

    if (mgl_buffer_find(ctx, 0u) != NULL)
        return fail(80, "zero-name lookup");
    for (i = 0u; i < BUFFER_COUNT; ++i) {
        objects[i] = mgl_buffer_ensure(ctx, first_name + i, 0);
        if (objects[i] == NULL || objects[i]->name != first_name + i)
            return fail(81, "bulk buffer creation");
    }
    for (i = 0u; i < BUFFER_COUNT; ++i)
        if (mgl_buffer_find(ctx, first_name + i) != objects[i])
            return fail(82, "hash collision lookup");

    for (i = 0u; i < BUFFER_COUNT; i += 3u)
        mgl_buffer_remove(ctx, first_name + i);
    for (i = 0u; i < BUFFER_COUNT; ++i)
        if (mgl_buffer_find(ctx, first_name + i) != ((i % 3u) == 0u ? NULL : objects[i]))
            return fail(83, "hash collision removal");

    /* A deleted object may remain allocated for an attached texture, but
     * mgl_buffer_find must immediately stop returning its old identity. */
    retained = mgl_buffer_ensure(ctx, 0x20000000u, 0);
    if (retained == NULL)
        return fail(84, "retained object allocation");
    mgl_buffer_retain_texture(retained);
    mgl_buffer_remove(ctx, retained->name);
    if (!retained->delete_pending || mgl_buffer_find(ctx, retained->name) != NULL)
        return fail(85, "retained deleted name still indexed");
    replacement = mgl_buffer_ensure(ctx, retained->name, 0);
    if (replacement == NULL || replacement == retained ||
        mgl_buffer_find(ctx, retained->name) != replacement)
        return fail(86, "deleted buffer name re-creation");
    mgl_buffer_release_texture(ctx, retained);
    if (mgl_buffer_find(ctx, replacement->name) != replacement)
        return fail(87, "retired old buffer unlinked replacement");
    mgl_buffer_remove(ctx, replacement->name);
    if (mgl_buffer_find(ctx, 0x20000000u) != NULL)
        return fail(88, "replacement deletion");
    for (i = 0u; i < BUFFER_COUNT; ++i) {
        mgl_buffer_remove(ctx, first_name + i);
        if (mgl_buffer_find(ctx, first_name + i) != NULL)
            return fail(89, "bulk buffer cleanup");
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint buffers[3] = {0u, 0u, 0u};
    GLuint list;
    GLuint generated_in_list = 0u;
    GLuint scratch_name = 0x12345678u;
    unsigned char initial[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    unsigned char replacement[4] = {20u, 21u, 22u, 23u};
    unsigned char list_a[4] = {31u, 32u, 33u, 34u};
    unsigned char list_b[4] = {41u, 42u, 43u, 44u};
    unsigned char patch[2] = {91u, 92u};
    unsigned char out[8] = {0u};
    unsigned char *mapped;
    void *pointer = NULL;
    GLint value;
    unsigned gen_after_compile;
    unsigned delete_after_compile;
    unsigned data_before_replay;
    unsigned bind_before_replay;
    const char *proc_names[] = {
        "glGenBuffers",  "glDeleteBuffers",        "glBindBuffer",
        "glBufferData",  "glBufferSubData",        "glGetBufferSubData",
        "glMapBuffer",   "glMapBufferRange",       "glFlushMappedBufferRange",
        "glUnmapBuffer", "glGetBufferParameteriv", "glGetBufferPointerv"};
    unsigned i;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.next_native = 1000u;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesBufferSubData = fake_buffer_subdata;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 16, 16);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glGenBuffers(3, buffers);
    if (buffers[0] == 0u || buffers[1] == 0u || buffers[2] == 0u || buffers[0] == buffers[1] ||
        buffers[1] == buffers[2])
        return fail(2, "generated interface names");
    if (glIsBuffer(buffers[0]) != GL_FALSE || glGetError() != GL_NO_ERROR)
        return fail(3, "generated names are not realized objects");

    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    if (!glIsBuffer(buffers[0]) || glGetError() != GL_NO_ERROR)
        return fail(4, "bind realizes buffer");
    if (cap.last_bind_target != GL_ARRAY_BUFFER || cap.last_bound == buffers[0])
        return fail(5, "interface/native name translation");

    glBufferData(GL_ARRAY_BUFFER, 8, initial, GL_DYNAMIC_DRAW);
    if (glGetError() != GL_NO_ERROR || cap.last_data_target != GL_ARRAY_BUFFER ||
        cap.last_data_size != 8 || cap.last_data_usage != GL_DYNAMIC_DRAW)
        return fail(6, "buffer data native realization");
    value = -1;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &value);
    if (value != 8)
        return fail(7, "buffer size query");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_USAGE, &value);
    if (value != (GLint)GL_DYNAMIC_DRAW)
        return fail(8, "buffer usage query");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_ACCESS, &value);
    if (value != (GLint)GL_READ_WRITE)
        return fail(9, "initial buffer access query");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAPPED, &value);
    if (value != GL_FALSE)
        return fail(10, "initial mapped query");

    mapped = glMapBufferRange(GL_ARRAY_BUFFER, 0, 4, 0u);
    if (mapped != NULL || glGetError() != GL_INVALID_OPERATION)
        return fail(11, "map requires read or write bit");
    mapped = glMapBufferRange(GL_ARRAY_BUFFER, 0, 4, GL_MAP_READ_BIT | 0x80000000u);
    if (mapped != NULL || glGetError() != GL_INVALID_VALUE)
        return fail(12, "map unknown access bit");
    mapped = glMapBufferRange(GL_ARRAY_BUFFER, 0, 4, GL_MAP_READ_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
    if (mapped != NULL || glGetError() != GL_INVALID_OPERATION)
        return fail(13, "read plus invalidate rejected");
    mapped = glMapBufferRange(GL_ARRAY_BUFFER, 0, 4, GL_MAP_READ_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    if (mapped != NULL || glGetError() != GL_INVALID_OPERATION)
        return fail(14, "flush explicit requires write");
    mapped = glMapBufferRange(GL_ARRAY_BUFFER, 7, 2, GL_MAP_READ_BIT);
    if (mapped != NULL || glGetError() != GL_INVALID_VALUE)
        return fail(15, "map range bounds");

    mapped = glMapBufferRange(GL_ARRAY_BUFFER, 2, 2, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    if (mapped == NULL || glGetError() != GL_NO_ERROR)
        return fail(16, "explicit write mapping");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_ACCESS, &value);
    if (value != (GLint)GL_WRITE_ONLY)
        return fail(17, "write-only access state");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_ACCESS_FLAGS, &value);
    if ((GLbitfield)value != (GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT))
        return fail(18, "map access flags state");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_OFFSET, &value);
    if (value != 2)
        return fail(19, "map offset state");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_LENGTH, &value);
    if (value != 2)
        return fail(20, "map length state");
    glGetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER, &pointer);
    if (pointer != mapped)
        return fail(21, "map pointer state");
    mapped[0] = 70u;
    mapped[1] = 71u;
    glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, 2);
    if (glGetError() != GL_NO_ERROR || cap.last_subdata_target != GL_ARRAY_BUFFER ||
        cap.last_subdata_offset != 2 || cap.last_subdata_size != 2)
        return fail(22, "explicit flush lowering");
    if (!glUnmapBuffer(GL_ARRAY_BUFFER) || glGetError() != GL_NO_ERROR)
        return fail(23, "unmap explicit mapping");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_ACCESS, &value);
    if (value != (GLint)GL_WRITE_ONLY)
        return fail(24, "unmap preserves access enum");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_ACCESS_FLAGS, &value);
    if (value != 0)
        return fail(25, "unmap resets access flags");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_OFFSET, &value);
    if (value != 0)
        return fail(26, "unmap resets offset");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_LENGTH, &value);
    if (value != 0)
        return fail(27, "unmap resets length");
    glGetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER, &pointer);
    if (pointer != NULL)
        return fail(28, "unmap resets pointer");

    mapped = glMapBufferRange(GL_ARRAY_BUFFER, 0, 2, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
    if (mapped == NULL)
        return fail(29, "read/write mapping");
    glBufferSubData(GL_ARRAY_BUFFER, 4, 2, patch);
    if (glGetError() != GL_NO_ERROR)
        return fail(30, "non-overlapping subdata while mapped");
    glBufferSubData(GL_ARRAY_BUFFER, 1, 2, patch);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(31, "overlapping subdata while mapped");
    memset(out, 0, sizeof(out));
    glGetBufferSubData(GL_ARRAY_BUFFER, 4, 2, out);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(32, "get subdata while mapped");

    glBufferData(GL_ARRAY_BUFFER, 4, replacement, GL_STATIC_READ);
    if (glGetError() != GL_NO_ERROR)
        return fail(33, "BufferData reinitializes mapped store");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAPPED, &value);
    if (value != GL_FALSE)
        return fail(34, "BufferData clears mapped state");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_ACCESS, &value);
    if (value != (GLint)GL_READ_WRITE)
        return fail(35, "BufferData resets access enum");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_ACCESS_FLAGS, &value);
    if (value != 0)
        return fail(36, "BufferData resets access flags");
    memset(out, 0, sizeof(out));
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, 4, out);
    if (memcmp(out, replacement, 4) != 0 || glGetError() != GL_NO_ERROR)
        return fail(37, "reinitialized shadow contents");

    mapped = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (mapped == NULL || glGetError() != GL_NO_ERROR)
        return fail(38, "whole buffer map");
    mapped[0] = 55u;
    {
        unsigned before = cap.subdata_calls;
        if (!glUnmapBuffer(GL_ARRAY_BUFFER))
            return fail(39, "whole buffer unmap");
        if (cap.subdata_calls != before + 1u || cap.last_subdata_offset != 0 ||
            cap.last_subdata_size != 4)
            return fail(40, "unmap writeback lowering");
    }

    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);
    mapped = glMapBufferRange(GL_ARRAY_BUFFER, 0, 0, GL_MAP_READ_BIT);
    if (mapped != NULL || glGetError() != GL_INVALID_OPERATION)
        return fail(41, "zero-length range map rejected");
    pointer = (void *)1;
    glGetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER, &pointer);
    if (pointer != NULL || glGetError() != GL_NO_ERROR)
        return fail(42, "failed map preserves null pointer");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAPPED, &value);
    if (value != GL_FALSE || glGetError() != GL_NO_ERROR)
        return fail(43, "failed map does not set mapped state");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_LENGTH, &value);
    if (value != 0 || glGetError() != GL_NO_ERROR)
        return fail(44, "failed map does not set map length");
    mapped = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE);
    if (mapped != NULL || glGetError() != GL_INVALID_OPERATION)
        return fail(45, "zero-sized whole buffer map rejected");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAPPED, &value);
    if (value != GL_FALSE || glGetError() != GL_NO_ERROR)
        return fail(46, "failed whole-buffer map preserves unmapped state");

    glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, 0);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(47, "flush requires mapped explicit range");
    mapped = glMapBufferRange(GL_ARRAY_BUFFER, 0, 0, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    if (mapped != NULL || glGetError() != GL_INVALID_OPERATION)
        return fail(48, "zero-length explicit map rejected");
    glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, 0);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(49, "failed map did not enable explicit flush");
    if (cap.subdata_calls == 0u)
        return fail(50, "nonzero mappings still dispatched native updates");

    glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
    glBufferData(GL_ARRAY_BUFFER, 4, list_b, GL_STATIC_DRAW);
    list = glGenLists(1);
    if (list == 0u)
        return fail(52, "display-list allocation");
    glNewList(list, GL_COMPILE);
    glGenBuffers(1, &generated_in_list);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
    glBufferData(GL_ARRAY_BUFFER, 4, list_a, GL_DYNAMIC_DRAW);
    value = -1;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &value);
    glDeleteBuffers(1, &generated_in_list);
    glEndList();
    if (generated_in_list == 0u || value != 4 || glGetError() != GL_NO_ERROR)
        return fail(53, "buffer commands execute during list compilation");
    gen_after_compile = cap.gen_calls;
    delete_after_compile = cap.delete_calls;
    glBufferData(GL_ARRAY_BUFFER, 4, list_b, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    data_before_replay = cap.data_calls;
    bind_before_replay = cap.bind_calls;
    glCallList(list);
    if (cap.gen_calls != gen_after_compile || cap.delete_calls != delete_after_compile ||
        cap.data_calls != data_before_replay || cap.bind_calls != bind_before_replay)
        return fail(54, "buffer commands are not retained in display lists");
    value = -1;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &value);
    if ((GLuint)value != buffers[0])
        return fail(55, "display-list replay preserves buffer binding");
    glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
    memset(out, 0, sizeof(out));
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, 4, out);
    if (memcmp(out, list_b, 4) != 0)
        return fail(56, "display-list replay does not repeat BufferData");

    value = 0x55aa;
    EXPECT_BEGIN_END_ERROR(57, glGenBuffers(-1, &scratch_name));
    if (scratch_name != 0x12345678u)
        return fail(58, "begin/end GenBuffers changed output");
    EXPECT_BEGIN_END_ERROR(59, glDeleteBuffers(-1, NULL));
    EXPECT_BEGIN_END_ERROR(60, glBindBuffer(0xdeadbeefu, 0u));
    EXPECT_BEGIN_END_ERROR(61, glBufferData(0xdeadbeefu, -1, NULL, 0xdeadbeefu));
    EXPECT_BEGIN_END_ERROR(62, glBufferSubData(0xdeadbeefu, -1, -1, NULL));
    EXPECT_BEGIN_END_ERROR(63, glGetBufferSubData(0xdeadbeefu, -1, -1, NULL));
    mapped = (unsigned char *)1;
    EXPECT_BEGIN_END_ERROR(64, mapped = glMapBuffer(0xdeadbeefu, 0xdeadbeefu));
    if (mapped != NULL)
        return fail(65, "begin/end MapBuffer return");
    mapped = (unsigned char *)1;
    EXPECT_BEGIN_END_ERROR(66, mapped = glMapBufferRange(0xdeadbeefu, -1, -1, 0u));
    if (mapped != NULL)
        return fail(67, "begin/end MapBufferRange return");
    EXPECT_BEGIN_END_ERROR(68, glFlushMappedBufferRange(0xdeadbeefu, -1, -1));
    {
        GLboolean result = GL_TRUE;
        EXPECT_BEGIN_END_ERROR(69, result = glUnmapBuffer(0xdeadbeefu));
        if (result != GL_FALSE)
            return fail(70, "begin/end UnmapBuffer return");
    }
    EXPECT_BEGIN_END_ERROR(71, glGetBufferParameteriv(0xdeadbeefu, 0xdeadbeefu, &value));
    if (value != 0x55aa)
        return fail(72, "begin/end parameter query changed output");
    pointer = (void *)1;
    EXPECT_BEGIN_END_ERROR(73, glGetBufferPointerv(0xdeadbeefu, 0xdeadbeefu, &pointer));
    if (pointer != (void *)1)
        return fail(74, "begin/end pointer query changed output");

    value = 0x1234;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, 0xdeadbeefu, &value);
    if (glGetError() != GL_INVALID_ENUM || value != 0x1234)
        return fail(75, "invalid buffer parameter query");

    for (i = 0; i < sizeof(proc_names) / sizeof(proc_names[0]); ++i) {
        if (mgl_proc_address(proc_names[i]) == NULL)
            return fail(76, proc_names[i]);
    }

    glBindBuffer(GL_ARRAY_BUFFER, buffers[2]);
    glBufferData(GL_ARRAY_BUFFER, 4, replacement, GL_STATIC_DRAW);
    glDeleteBuffers(1, &buffers[2]);
    value = -1;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &value);
    if (value != 0 || glIsBuffer(buffers[2]) != GL_FALSE)
        return fail(77, "delete current binding lifecycle");

    if (test_buffer_index(&ctx) != 0)
        return 90;

    glDeleteLists(list, 1);
    glDeleteBuffers(2, buffers);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL buffer object semantics unit test: PASS");
    return 0;
}

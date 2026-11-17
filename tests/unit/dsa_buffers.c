#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_buffer;
    GLuint last_bound;
    GLenum last_target;
    GLintptr last_offset;
    GLsizeiptr last_size;
    unsigned data_calls;
    unsigned subdata_calls;
} cap;

static void fake_gen(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_buffer;
}

static void fake_delete(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind(GLenum target, GLuint name) {
    cap.last_target = target;
    cap.last_bound = name;
}

static void fake_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)data;
    (void)usage;
    cap.last_target = target;
    cap.last_size = size;
    ++cap.data_calls;
}

static void fake_subdata(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    (void)data;
    cap.last_target = target;
    cap.last_offset = offset;
    cap.last_size = size;
    ++cap.subdata_calls;
}

static int fail(int n, const char *msg) {
    fprintf(stderr, "DSA buffer failure %d: %s\n", n, msg);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_buffer *bound_obj;
    GLuint buffers[3] = {0u, 0u, 0u};
    unsigned char initial[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    unsigned char out[8] = {0};
    unsigned char patch[2] = {9, 10};
    unsigned char *mapped;
    void *ptr = NULL;
    GLint iv = 0;
    GLint64 i64 = 0;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.next_buffer = 500u;
    native.glesGenBuffers = fake_gen;
    native.glesDeleteBuffers = fake_delete;
    native.glesBindBuffer = fake_bind;
    native.glesBufferData = fake_data;
    native.glesBufferSubData = fake_subdata;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glCreateBuffers(3, buffers);
    if (!glIsBuffer(buffers[0]) || !glIsBuffer(buffers[1]) || !glIsBuffer(buffers[2]))
        return fail(2, "create buffers realizes names");
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, 4, initial, GL_STATIC_DRAW);
    bound_obj = mgl_buffer_find(&ctx, buffers[0]);
    if (bound_obj == NULL)
        return fail(3, "bound object lookup");

    glNamedBufferData(buffers[1], 8, initial, GL_DYNAMIC_DRAW);
    glGetNamedBufferParameteriv(buffers[1], GL_BUFFER_ACCESS, &iv);
    if (iv != (GLint)GL_READ_WRITE)
        return fail(20, "named initial access state");
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &iv);
    if ((GLuint)iv != buffers[0] || cap.last_bound != bound_obj->native_name || cap.data_calls < 2u)
        return fail(4, "named data preserves binding");
    glGetNamedBufferSubData(buffers[1], 0, 8, out);
    if (memcmp(out, initial, 8) != 0)
        return fail(5, "named get data");

    glNamedBufferSubDataEXT(buffers[1], 3, 2, patch);
    memset(out, 0, sizeof(out));
    glGetNamedBufferSubDataEXT(buffers[1], 0, 8, out);
    if (out[3] != 9 || out[4] != 10)
        return fail(6, "EXT named subdata aliases");
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &iv);
    if ((GLuint)iv != buffers[0] || cap.last_bound != bound_obj->native_name)
        return fail(7, "subdata preserves binding");

    mapped = glMapNamedBufferRange(buffers[1], 2, 4, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    if (mapped == NULL)
        return fail(8, "named map range");
    glGetNamedBufferParameteriv(buffers[1], GL_BUFFER_ACCESS, &iv);
    if (iv != (GLint)GL_WRITE_ONLY)
        return fail(21, "named mapped access state");
    glGetNamedBufferParameteriv(buffers[1], GL_BUFFER_ACCESS_FLAGS, &iv);
    if ((GLbitfield)iv != (GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT))
        return fail(22, "named mapped access flags");
    mapped[0] = 21;
    mapped[1] = 22;
    glGetNamedBufferPointerv(buffers[1], GL_BUFFER_MAP_POINTER, &ptr);
    if (ptr != mapped)
        return fail(9, "map pointer query");
    glFlushMappedNamedBufferRange(buffers[1], 0, 2);
    if (cap.last_offset != 2 || cap.last_size != 2 || cap.subdata_calls == 0u)
        return fail(10, "flush explicit native realization");
    if (!glUnmapNamedBuffer(buffers[1]))
        return fail(11, "unmap");
    glGetNamedBufferParameteriv(buffers[1], GL_BUFFER_ACCESS, &iv);
    if (iv != (GLint)GL_WRITE_ONLY)
        return fail(23, "named unmap preserves access enum");
    glGetNamedBufferParameteriv(buffers[1], GL_BUFFER_ACCESS_FLAGS, &iv);
    if (iv != 0)
        return fail(24, "named unmap resets access flags");
    /* Negative flush ranges are GL_INVALID_VALUE even without a mapping. */
    {
        const unsigned before = cap.subdata_calls;
        glFlushMappedNamedBufferRange(buffers[1], -1, 1);
        if (glGetError() != GL_INVALID_VALUE || cap.subdata_calls != before)
            return fail(33, "unmapped named flush negative offset precedence");
        glFlushMappedNamedBufferRange(buffers[1], 0, -1);
        if (glGetError() != GL_INVALID_VALUE || cap.subdata_calls != before)
            return fail(34, "unmapped named flush negative length precedence");
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, -1, 1);
        if (glGetError() != GL_INVALID_VALUE || cap.subdata_calls != before)
            return fail(35, "unmapped bound flush negative offset precedence");
    }
    mapped = glMapNamedBufferRange(buffers[1], 0, 4, 0u);
    if (mapped != NULL || mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(25, "named map access validation");
    mapped = glMapNamedBuffer(buffers[1], GL_READ_WRITE);
    if (mapped == NULL || mapped[0] != initial[0])
        return fail(18, "whole named map");
    mapped[0] = 31;
    if (!glUnmapNamedBuffer(buffers[1]))
        return fail(19, "whole named unmap");
    memset(out, 0, sizeof(out));
    glGetNamedBufferSubData(buffers[1], 0, 8, out);
    if (out[2] != 21 || out[3] != 22)
        return fail(12, "mapped writes canonical shadow");

    /* A mapped byte range does not prohibit named updates to disjoint storage. */
    mapped = glMapNamedBufferRange(buffers[1], 1, 2, GL_MAP_READ_BIT);
    if (mapped == NULL || glGetError() != GL_NO_ERROR)
        return fail(26, "mapping for nonoverlapping named update");
    {
        unsigned before = cap.subdata_calls;
        const unsigned char disjoint[2] = {71, 73};
        const unsigned char forbidden[2] = {81, 83};
        glNamedBufferSubData(buffers[1], 5, 2, disjoint);
        if (glGetError() != GL_NO_ERROR || cap.subdata_calls != before + 1u ||
            cap.last_target != GL_ARRAY_BUFFER || cap.last_offset != 5 || cap.last_size != 2 ||
            cap.last_bound != bound_obj->native_name)
            return fail(27, "disjoint named update reaches GLES and restores binding");
        glNamedBufferSubData(buffers[1], 2, 2, forbidden);
        if (glGetError() != GL_INVALID_OPERATION || cap.subdata_calls != before + 1u)
            return fail(28, "overlapping named update rejected before GLES");
        glNamedBufferSubData(buffers[1], 7, 2, forbidden);
        if (glGetError() != GL_INVALID_VALUE || cap.subdata_calls != before + 1u)
            return fail(29, "out-of-range named update rejected before GLES");
        glGetNamedBufferParameteriv(buffers[1], GL_BUFFER_MAPPED, &iv);
        if (iv != GL_TRUE || glGetError() != GL_NO_ERROR || mapped[1] != 21)
            return fail(30, "failed named updates preserve active mapping");
    }
    if (!glUnmapNamedBuffer(buffers[1]) || glGetError() != GL_NO_ERROR)
        return fail(31, "mapping survives nonoverlapping update");
    memset(out, 0, sizeof(out));
    glGetNamedBufferSubData(buffers[1], 0, 8, out);
    if (glGetError() != GL_NO_ERROR || out[5] != 71 || out[6] != 73 || out[3] != 22)
        return fail(32, "named update changes only disjoint shadow bytes");

    glNamedBufferData(buffers[2], 8, NULL, GL_STATIC_COPY);
    glCopyNamedBufferSubData(buffers[1], buffers[2], 1, 2, 5);
    memset(out, 0, sizeof(out));
    glGetNamedBufferSubData(buffers[2], 0, 8, out);
    {
        unsigned char src[8] = {0};
        glGetNamedBufferSubData(buffers[1], 0, 8, src);
        if (memcmp(out + 2, src + 1, 5) != 0)
            return fail(13, "named copy");
    }

    glGetNamedBufferParameteriv(buffers[1], GL_BUFFER_SIZE, &iv);
    if (iv != 8)
        return fail(14, "size query");
    glGetNamedBufferParameteri64v(buffers[1], GL_BUFFER_SIZE, &i64);
    if (i64 != 8)
        return fail(15, "64-bit size query");
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &iv);
    if ((GLuint)iv != buffers[0])
        return fail(16, "DSA never changes interface binding");

    glNamedBufferData(0xdeadbeefu, 4, initial, GL_STATIC_DRAW);
    {
        GLuint created = 0u;
        glCreateBuffers(1, &created);
        if (created == 0u || glIsBuffer(created) != GL_TRUE)
            return fail(36, "create buffers with prior pending error");
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
            return fail(17, "unknown named buffer rejected");
        glDeleteBuffers(1, &created);
    }

    glDeleteBuffers(3, buffers);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL DSA buffer unit test: PASS");
    return 0;
}

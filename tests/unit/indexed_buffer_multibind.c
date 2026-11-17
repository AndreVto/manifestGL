#include "mgl/gl.h"
#include "abi/proc_address.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_buffer;
    GLuint generic_native;
    GLenum generic_target;
    GLenum base_target;
    GLenum range_target;
    GLuint base_native[MGL_MAX_INDEXED_BUFFER_BINDINGS];
    unsigned base_calls[MGL_MAX_INDEXED_BUFFER_BINDINGS];
    GLuint range_native[MGL_MAX_INDEXED_BUFFER_BINDINGS];
    GLintptr range_offset[MGL_MAX_INDEXED_BUFFER_BINDINGS];
    GLsizeiptr range_size[MGL_MAX_INDEXED_BUFFER_BINDINGS];
    unsigned range_calls[MGL_MAX_INDEXED_BUFFER_BINDINGS];
} cap;

static void fake_gen_buffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_buffer;
}

static void fake_delete_buffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    cap.generic_target = target;
    cap.generic_native = name;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)target;
    (void)size;
    (void)data;
    (void)usage;
}

static void fake_get_limits(GLenum pname, GLint *out) {
    if (pname == GL_MAX_UNIFORM_BUFFER_BINDINGS)
        *out = 24;
    else if (pname == GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS)
        *out = 32;
    else if (pname == GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS)
        *out = 4;
    else if (pname == GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS)
        *out = 12;
    else
        *out = 0;
}

static void fake_bind_base(GLenum target, GLuint index, GLuint buffer) {
    cap.base_target = target;
    if (index < MGL_MAX_INDEXED_BUFFER_BINDINGS) {
        cap.base_native[index] = buffer;
        ++cap.base_calls[index];
    }
}

static void fake_bind_range(GLenum target, GLuint index, GLuint buffer, GLintptr offset,
                            GLsizeiptr size) {
    cap.range_target = target;
    if (index < MGL_MAX_INDEXED_BUFFER_BINDINGS) {
        cap.range_native[index] = buffer;
        cap.range_offset[index] = offset;
        cap.range_size[index] = size;
        ++cap.range_calls[index];
    }
}

static int fail(int n, const char *message) {
    fprintf(stderr, "indexed buffer multi-bind failure %d: %s\n", n, message);
    return n;
}

static int get_i(GLenum pname, GLuint index) {
    GLint value = -1;
    glGetIntegeri_v(pname, index, &value);
    return value;
}

static long long get_i64(GLenum pname, GLuint index) {
    GLint64 value = -1;
    glGetInteger64i_v(pname, index, &value);
    return (long long)value;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint buffers[4] = {0u, 0u, 0u, 0u};
    GLuint pair[2];
    GLuint trio[3];
    GLintptr offsets[3];
    GLsizeiptr sizes[3];
    GLint generic = -1;
    GLint limit = -1;
    unsigned char bytes[256] = {0u};
    struct mgl_buffer *obj1;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.next_buffer = 1000u;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesBindBufferBase = fake_bind_base;
    native.glesBindBufferRange = fake_bind_range;
    native.glesGetIntegerv = fake_get_limits;
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

    glGenBuffers(4, buffers);
    if (buffers[0] == 0u || buffers[1] == 0u || buffers[2] == 0u || buffers[3] == 0u)
        return fail(2, "buffer generation");
    glBindBuffer(GL_UNIFORM_BUFFER, buffers[0]);
    glBufferData(GL_UNIFORM_BUFFER, 32, bytes, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, buffers[1]);
    glBufferData(GL_UNIFORM_BUFFER, 64, bytes, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, buffers[2]);
    glBufferData(GL_UNIFORM_BUFFER, 96, bytes, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, buffers[3]);
    glBufferData(GL_UNIFORM_BUFFER, 128, bytes, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, buffers[0]);

    pair[0] = buffers[1];
    pair[1] = buffers[2];
    glBindBuffersBase(GL_UNIFORM_BUFFER, 2u, 2, pair);
    if (glGetError() != GL_NO_ERROR)
        return fail(3, "base multi-bind error");
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &generic);
    if ((GLuint)generic != buffers[0])
        return fail(4, "base multi-bind changed general binding");
    if ((GLuint)get_i(GL_UNIFORM_BUFFER_BINDING, 2u) != buffers[1] ||
        (GLuint)get_i(GL_UNIFORM_BUFFER_BINDING, 3u) != buffers[2])
        return fail(5, "base indexed names");
    if (get_i(GL_UNIFORM_BUFFER_SIZE, 2u) != 64 || get_i64(GL_UNIFORM_BUFFER_SIZE, 3u) != 96)
        return fail(6, "base indexed sizes");
    obj1 = mgl_buffer_find(&ctx, buffers[1]);
    if (obj1 == NULL || cap.base_native[2] != obj1->native_name || cap.base_calls[2] != 1u)
        return fail(7, "base native realization");

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[1]);
    glBufferData(GL_UNIFORM_BUFFER, 123, bytes, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, buffers[0]);
    if (get_i(GL_UNIFORM_BUFFER_SIZE, 2u) != 123 || get_i64(GL_UNIFORM_BUFFER_SIZE, 2u) != 123)
        return fail(8, "base binding size did not follow resized buffer");

    glBindBufferBase(GL_UNIFORM_BUFFER, 0u, buffers[3]);
    if ((GLuint)get_i(GL_UNIFORM_BUFFER_BINDING, 0u) != buffers[3] ||
        get_i(GL_UNIFORM_BUFFER_SIZE, 0u) != 128)
        return fail(9, "single base binding");
    glBufferData(GL_UNIFORM_BUFFER, 200, bytes, GL_DYNAMIC_DRAW);
    if (get_i(GL_UNIFORM_BUFFER_SIZE, 0u) != 200)
        return fail(10, "single base live size");
    glBindBuffer(GL_UNIFORM_BUFFER, buffers[0]);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers[0]);
    offsets[0] = 8;
    offsets[1] = 16;
    sizes[0] = 24;
    sizes[1] = 32;
    glBindBuffersRange(GL_SHADER_STORAGE_BUFFER, 1u, 2, pair, offsets, sizes);
    if (glGetError() != GL_NO_ERROR)
        return fail(11, "range multi-bind error");
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &generic);
    if ((GLuint)generic != buffers[0])
        return fail(12, "range multi-bind changed general binding");
    if ((GLuint)get_i(GL_SHADER_STORAGE_BUFFER_BINDING, 1u) != buffers[1] ||
        (GLuint)get_i(GL_SHADER_STORAGE_BUFFER_BINDING, 2u) != buffers[2])
        return fail(13, "range indexed names");
    if (get_i64(GL_SHADER_STORAGE_BUFFER_START, 1u) != 8 ||
        get_i64(GL_SHADER_STORAGE_BUFFER_SIZE, 1u) != 24 ||
        get_i64(GL_SHADER_STORAGE_BUFFER_START, 2u) != 16 ||
        get_i64(GL_SHADER_STORAGE_BUFFER_SIZE, 2u) != 32)
        return fail(14, "range indexed values");
    if (cap.range_offset[1] != 8 || cap.range_size[1] != 24 || cap.range_calls[1] != 1u)
        return fail(15, "range native realization");

    glBindBufferBase(GL_UNIFORM_BUFFER, 5u, buffers[0]);
    glBindBuffer(GL_UNIFORM_BUFFER, buffers[0]);
    trio[0] = buffers[3];
    trio[1] = 0xdeadbeefu;
    trio[2] = buffers[2];
    glBindBuffersBase(GL_UNIFORM_BUFFER, 4u, 3, trio);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(16, "invalid multi-bind name error");
    if ((GLuint)get_i(GL_UNIFORM_BUFFER_BINDING, 4u) != buffers[3] ||
        (GLuint)get_i(GL_UNIFORM_BUFFER_BINDING, 5u) != buffers[0] ||
        (GLuint)get_i(GL_UNIFORM_BUFFER_BINDING, 6u) != buffers[2])
        return fail(17, "per-binding error isolation");
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &generic);
    if ((GLuint)generic != buffers[0])
        return fail(18, "invalid multi-bind changed general binding");

    glBindBuffersBase(GL_UNIFORM_BUFFER, 23u, 2, pair);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(19, "first+count validation");
    glBindBufferBase(GL_UNIFORM_BUFFER, 20u, buffers[0]);
    if (glGetError() != GL_NO_ERROR ||
        (GLuint)get_i(GL_UNIFORM_BUFFER_BINDING, 20u) != buffers[0] ||
        cap.base_native[20] == 0u)
        return fail(29, "native UBO index above 15");
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 30u, buffers[1]);
    if (glGetError() != GL_NO_ERROR ||
        (GLuint)get_i(GL_SHADER_STORAGE_BUFFER_BINDING, 30u) != buffers[1])
        return fail(30, "native SSBO index above 15");
    glBindBufferBase(GL_UNIFORM_BUFFER, 24u, buffers[0]);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(31, "index at native UBO limit");
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 24u, &limit);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(34, "query beyond native UBO index limit");
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &limit);
    if (limit != 24)
        return fail(32, "reported native UBO capacity");
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &limit);
    if (limit != 32)
        return fail(33, "reported native SSBO capacity");
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers[0]);
    glBindBuffersBase(GL_UNIFORM_BUFFER, 0u, -1, pair);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(20, "negative count validation");
    glBindBuffersBase(GL_ARRAY_BUFFER, 0u, 1, pair);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(21, "multi-bind target validation");

    glBindBuffersBase(GL_UNIFORM_BUFFER, 2u, 2, NULL);
    if (glGetError() != GL_NO_ERROR || get_i(GL_UNIFORM_BUFFER_BINDING, 2u) != 0 ||
        get_i(GL_UNIFORM_BUFFER_BINDING, 3u) != 0)
        return fail(22, "base NULL unbind");
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &generic);
    if ((GLuint)generic != buffers[0])
        return fail(23, "base NULL changed general binding");

    glBindBuffersRange(GL_SHADER_STORAGE_BUFFER, 1u, 2, NULL, NULL, NULL);
    if (glGetError() != GL_NO_ERROR || get_i(GL_SHADER_STORAGE_BUFFER_BINDING, 1u) != 0 ||
        get_i(GL_SHADER_STORAGE_BUFFER_BINDING, 2u) != 0)
        return fail(24, "range NULL unbind");
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &generic);
    if ((GLuint)generic != buffers[0])
        return fail(25, "range NULL changed general binding");

    pair[0] = buffers[1];
    pair[1] = buffers[2];
    offsets[0] = 0;
    offsets[1] = 0;
    sizes[0] = -1;
    sizes[1] = 16;
    glBindBuffersRange(GL_SHADER_STORAGE_BUFFER, 8u, 2, pair, offsets, sizes);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(26, "per-range invalid size error");
    if (get_i(GL_SHADER_STORAGE_BUFFER_BINDING, 8u) != 0 ||
        (GLuint)get_i(GL_SHADER_STORAGE_BUFFER_BINDING, 9u) != buffers[2])
        return fail(27, "per-range error isolation");

    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buffers[0]);
    glGetIntegerv(GL_ATOMIC_COUNTER_BUFFER_BINDING, &generic);
    glGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, &limit);
    if (glGetError() != GL_NO_ERROR || (GLuint)generic != buffers[0] || limit != 12)
        return fail(35, "atomic generic binding and limit");
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 7u, buffers[1]);
    if (glGetError() != GL_NO_ERROR ||
        (GLuint)get_i(GL_ATOMIC_COUNTER_BUFFER_BINDING, 7u) != buffers[1] ||
        get_i64(GL_ATOMIC_COUNTER_BUFFER_SIZE, 7u) != 123 ||
        cap.base_target != GL_ATOMIC_COUNTER_BUFFER)
        return fail(36, "atomic indexed base binding");
    glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, 8u, buffers[2], 12, 20);
    if (glGetError() != GL_NO_ERROR ||
        (GLuint)get_i(GL_ATOMIC_COUNTER_BUFFER_BINDING, 8u) != buffers[2] ||
        get_i64(GL_ATOMIC_COUNTER_BUFFER_START, 8u) != 12 ||
        get_i64(GL_ATOMIC_COUNTER_BUFFER_SIZE, 8u) != 20 ||
        cap.range_target != GL_ATOMIC_COUNTER_BUFFER)
        return fail(37, "atomic indexed range binding");
    glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, 8u, buffers[2], 2, 20);
    if (glGetError() != GL_INVALID_VALUE ||
        get_i64(GL_ATOMIC_COUNTER_BUFFER_START, 8u) != 12)
        return fail(38, "atomic range alignment preserves binding");
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 12u, buffers[1]);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(39, "atomic native binding limit");
    pair[0] = buffers[1];
    pair[1] = buffers[3];
    glBindBuffersBase(GL_ATOMIC_COUNTER_BUFFER, 9u, 2, pair);
    if (glGetError() != GL_NO_ERROR ||
        (GLuint)get_i(GL_ATOMIC_COUNTER_BUFFER_BINDING, 9u) != buffers[1] ||
        (GLuint)get_i(GL_ATOMIC_COUNTER_BUFFER_BINDING, 10u) != buffers[3])
        return fail(40, "atomic base multi-bind");
    offsets[0] = 4;
    offsets[1] = 8;
    sizes[0] = 16;
    sizes[1] = 24;
    glBindBuffersRange(GL_ATOMIC_COUNTER_BUFFER, 9u, 2, pair, offsets, sizes);
    if (glGetError() != GL_NO_ERROR ||
        get_i64(GL_ATOMIC_COUNTER_BUFFER_START, 9u) != 4 ||
        get_i64(GL_ATOMIC_COUNTER_BUFFER_SIZE, 10u) != 24)
        return fail(41, "atomic range multi-bind");
    glBindBuffersBase(GL_ATOMIC_COUNTER_BUFFER, 9u, 2, NULL);
    if (glGetError() != GL_NO_ERROR ||
        get_i(GL_ATOMIC_COUNTER_BUFFER_BINDING, 9u) != 0 ||
        get_i(GL_ATOMIC_COUNTER_BUFFER_BINDING, 10u) != 0)
        return fail(42, "atomic multi-unbind");
    glGetIntegerv(GL_ATOMIC_COUNTER_BUFFER_BINDING, &generic);
    if ((GLuint)generic != buffers[2])
        return fail(43, "atomic multibind changed generic binding");

    if (mgl_proc_address("glBindBuffersBase") == NULL ||
        mgl_proc_address("glBindBuffersRange") == NULL)
        return fail(28, "proc-address coverage");

    glDeleteBuffers(4, buffers);
    glGetIntegerv(GL_ATOMIC_COUNTER_BUFFER_BINDING, &generic);
    if (glGetError() != GL_NO_ERROR || generic != 0 ||
        get_i(GL_ATOMIC_COUNTER_BUFFER_BINDING, 7u) != 0 ||
        get_i(GL_ATOMIC_COUNTER_BUFFER_BINDING, 8u) != 0)
        return fail(44, "deletion clears atomic indexed binding");
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL indexed buffer multi-bind unit test: PASS");
    return 0;
}

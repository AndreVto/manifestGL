#include "mgl/gl.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static GLuint next_name;
static GLuint array_binding;
static GLuint read_binding;
static GLuint write_binding;
static unsigned char native_data[8][64];
static unsigned maps;
static unsigned unmaps;
static unsigned flushes;
static unsigned copies;
static unsigned subdatas;

static GLuint bound(GLenum target) {
    if (target == GL_ARRAY_BUFFER)
        return array_binding;
    if (target == GL_COPY_READ_BUFFER)
        return read_binding;
    if (target == GL_COPY_WRITE_BUFFER)
        return write_binding;
    return 0u;
}

static void gen(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i)
        names[i] = ++next_name;
}

static void bind(GLenum target, GLuint name) {
    if (target == GL_ARRAY_BUFFER)
        array_binding = name;
    if (target == GL_COPY_READ_BUFFER)
        read_binding = name;
    if (target == GL_COPY_WRITE_BUFFER)
        write_binding = name;
}

static void data(GLenum target, GLsizeiptr size, const void *source, GLenum usage) {
    GLuint name = bound(target);
    (void)usage;
    if (name == 0u || name >= 8u || size > 64 || size < 0)
        return;
    if (source == NULL)
        memset(native_data[name], 0, (size_t)size);
    else
        memcpy(native_data[name], source, (size_t)size);
}

static void subdata(GLenum target, GLintptr offset, GLsizeiptr size, const void *source) {
    GLuint name = bound(target);
    if (name == 0u || name >= 8u || offset < 0 || size < 0 || offset + size > 64)
        return;
    ++subdatas;
    memcpy(native_data[name] + offset, source, (size_t)size);
}

static void *map(GLenum target, GLintptr offset, GLsizeiptr size, GLbitfield access) {
    GLuint name = bound(target);
    if (name == 0u || name >= 8u || offset < 0 || size <= 0 || offset + size > 64 ||
        (access & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) == 0u)
        return NULL;
    ++maps;
    return native_data[name] + offset;
}

static GLboolean unmap(GLenum target) {
    if (bound(target) == 0u)
        return GL_FALSE;
    ++unmaps;
    return GL_TRUE;
}

static void flush(GLenum target, GLintptr offset, GLsizeiptr size) {
    if (bound(target) == 0u || offset < 0 || size < 0)
        return;
    ++flushes;
}

static void copy(GLenum read_target, GLenum write_target, GLintptr read_offset,
                 GLintptr write_offset, GLsizeiptr size) {
    GLuint src = bound(read_target);
    GLuint dst = bound(write_target);
    if (src == 0u || dst == 0u || src >= 8u || dst >= 8u || size < 0)
        return;
    ++copies;
    memmove(native_data[dst] + write_offset, native_data[src] + read_offset, (size_t)size);
}

static int fail(int code) {
    fprintf(stderr, "ANGLE native forwarding failed: %d\n", code);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_buffer *obj;
    GLuint ids[3] = {0u};
    unsigned char initial[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    unsigned char out[8] = {0u};
    unsigned char *p;
    void *pointer = NULL;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenBuffers = gen;
    native.glesBindBuffer = bind;
    native.glesBufferData = data;
    native.glesBufferSubData = subdata;
    native.glesMapBufferRange = map;
    native.glesUnmapBuffer = unmap;
    native.glesFlushMappedBufferRange = flush;
    native.glesCopyBufferSubData = copy;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1);
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.angle_lazy_buffer_shadows = GL_TRUE;
    mgl_context_set_current_for_test(&ctx);
    glCreateBuffers(3, ids);
    glBindBuffer(GL_ARRAY_BUFFER, ids[0]);
    glBufferData(GL_ARRAY_BUFFER, 8, initial, GL_DYNAMIC_DRAW);
    obj = mgl_buffer_find(&ctx, ids[0]);
    if (obj == NULL || obj->shadow != NULL)
        return fail(2);
    p = glMapBufferRange(GL_ARRAY_BUFFER, 2, 4, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    if (p != native_data[obj->native_name] + 2)
        return fail(3);
    p[0] = 42u;
    glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, 1);
    if (flushes != 1 || subdatas != 0u)
        return fail(4);
    if (!glUnmapBuffer(GL_ARRAY_BUFFER) || unmaps != 1u || obj->shadow != NULL)
        return fail(5);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, 8, out);
    if (out[2] != 42u || obj->shadow != NULL || maps != 2u || unmaps != 2u)
        return fail(6);
    glNamedBufferData(ids[1], 8, initial, GL_DYNAMIC_DRAW);
    obj = mgl_buffer_find(&ctx, ids[1]);
    if (obj == NULL || obj->shadow != NULL || array_binding != ids[0])
        return fail(7);
    p = glMapNamedBufferRange(ids[1], 1, 3, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    if (p != native_data[obj->native_name] + 1 || array_binding != ids[0])
        return fail(8);
    glGetNamedBufferPointerv(ids[1], GL_BUFFER_MAP_POINTER, &pointer);
    if (pointer != p)
        return fail(9);
    p[0] = 70u;
    glFlushMappedNamedBufferRange(ids[1], 0, 1);
    if (flushes != 2 || subdatas != 0u || array_binding != ids[0])
        return fail(10);
    if (!glUnmapNamedBuffer(ids[1]) || array_binding != ids[0])
        return fail(11);
    glGetNamedBufferSubData(ids[1], 0, 8, out);
    if (out[1] != 70u || obj->shadow != NULL || array_binding != ids[0])
        return fail(12);
    glCopyNamedBufferSubData(ids[1], ids[0], 1, 3, 3);
    glGetNamedBufferSubData(ids[0], 0, 8, out);
    if (copies != 1u || subdatas != 0u || out[3] != 70u || read_binding != 0u ||
        write_binding != 0u || array_binding != ids[0])
        return fail(13);
    ctx.angle_lazy_buffer_shadows = GL_FALSE;
    glNamedBufferData(ids[2], 8, initial, GL_DYNAMIC_DRAW);
    obj = mgl_buffer_find(&ctx, ids[2]);
    if (obj == NULL || obj->shadow == NULL)
        return fail(14);
    p = glMapNamedBuffer(ids[2], GL_READ_WRITE);
    if (p != native_data[obj->native_name] || obj->shadow != NULL)
        return fail(15);
    p[0] = 99u;
    if (!glUnmapNamedBuffer(ids[2]))
        return fail(16);
    glGetNamedBufferSubData(ids[2], 0, 8, out);
    if (out[0] != 99u || obj->shadow != NULL)
        return fail(17);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(22);
    glDeleteBuffers(3, ids);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

#include "mgl/gl.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static GLuint next_native;
static GLuint bound_array;
static GLuint bound_copy_read;
static GLuint bound_element;
static unsigned char gpu_data[64];
static int map_count;
static int unmap_count;

static void gen(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i)
        names[i] = ++next_native;
}

static void bind(GLenum target, GLuint name) {
    if (target == GL_ARRAY_BUFFER)
        bound_array = name;
    if (target == GL_ELEMENT_ARRAY_BUFFER)
        bound_element = name;
    if (target == GL_COPY_READ_BUFFER)
        bound_copy_read = name;
}

static void data(GLenum target, GLsizeiptr size, const void *source, GLenum usage) {
    (void)usage;
    if ((target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) ||
        (target == GL_ARRAY_BUFFER && bound_array == 0u) ||
        (target == GL_ELEMENT_ARRAY_BUFFER && bound_element == 0u) ||
        size > (GLsizeiptr)sizeof(gpu_data))
        return;
    if (source != NULL)
        memcpy(gpu_data, source, (size_t)size);
    else
        memset(gpu_data, 0, (size_t)size);
}

static void subdata(GLenum target, GLintptr offset, GLsizeiptr size, const void *source) {
    if (target != GL_ARRAY_BUFFER || bound_array == 0u || offset < 0 || size < 0 ||
        (size_t)offset + (size_t)size > sizeof(gpu_data))
        return;
    memcpy(gpu_data + offset, source, (size_t)size);
}

static void *map(GLenum target, GLintptr offset, GLsizeiptr size, GLbitfield access) {
    if (offset < 0 || size <= 0 || (size_t)offset + (size_t)size > sizeof(gpu_data))
        return NULL;
    if (target == GL_COPY_READ_BUFFER) {
        if (bound_copy_read == 0u || access != GL_MAP_READ_BIT)
            return NULL;
    } else if (target == GL_ARRAY_BUFFER) {
        if (bound_array == 0u || (access & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) == 0u)
            return NULL;
    } else
        return NULL;
    ++map_count;
    return gpu_data + offset;
}

static GLboolean unmap(GLenum target) {
    if (target != GL_COPY_READ_BUFFER && target != GL_ARRAY_BUFFER)
        return GL_FALSE;
    ++unmap_count;
    return GL_TRUE;
}

static int fail(int step) {
    fprintf(stderr, "ANGLE lazy buffer shadow failed: %d\n", step);
    return step;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_buffer *obj;
    GLuint name = 0u;
    const unsigned char input[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    const unsigned char patch[2] = {91u, 92u};
    unsigned char out[8] = {0u};
    unsigned char *mapped;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenBuffers = gen;
    native.glesBindBuffer = bind;
    native.glesBufferData = data;
    native.glesBufferSubData = subdata;
    native.glesMapBufferRange = map;
    native.glesUnmapBuffer = unmap;
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

    glGenBuffers(1, &name);
    glBindBuffer(GL_ARRAY_BUFFER, name);
    glBufferData(GL_ARRAY_BUFFER, 8, input, GL_DYNAMIC_DRAW);
    obj = mgl_buffer_find(&ctx, name);
    if (obj == NULL || obj->shadow != NULL || memcmp(gpu_data, input, 8u) != 0)
        return fail(2);
    glBufferSubData(GL_ARRAY_BUFFER, 2, 2, patch);
    if (obj->shadow != NULL || gpu_data[2] != 91u || gpu_data[3] != 92u)
        return fail(3);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, 8, out);
    if (obj->shadow != NULL || map_count != 1 || unmap_count != 1 ||
        memcmp(out, gpu_data, 8u) != 0 || bound_copy_read != 0u)
        return fail(4);
    mapped = glMapBufferRange(GL_ARRAY_BUFFER, 0, 8, GL_MAP_WRITE_BIT);
    if (mapped == NULL || mapped[2] != 91u)
        return fail(5);
    mapped[0] = 71u;
    if (!glUnmapBuffer(GL_ARRAY_BUFFER) || gpu_data[0] != 71u)
        return fail(6);
    glBufferData(GL_ARRAY_BUFFER, 8, input, GL_DYNAMIC_DRAW);
    if (obj->shadow != NULL || memcmp(gpu_data, input, 8u) != 0)
        return fail(7);
    ctx.angle_lazy_buffer_shadows = GL_FALSE;
    glBufferData(GL_ARRAY_BUFFER, 8, input, GL_DYNAMIC_DRAW);
    if (obj->shadow == NULL || memcmp(obj->shadow, input, 8u) != 0)
        return fail(8);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(9);
    {
        GLuint index_name = 0u;
        ctx.angle_lazy_buffer_shadows = GL_TRUE;
        glGenBuffers(1, &index_name);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_name);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 8, input, GL_STATIC_DRAW);
        obj = mgl_buffer_find(&ctx, index_name);
        if (obj == NULL || obj->shadow != NULL || memcmp(gpu_data, input, 8u) != 0)
            return fail(10);
        if (!mgl_buffer_shadow_ensure(&ctx, obj) || obj->shadow == NULL ||
            memcmp(obj->shadow, input, 8u) != 0 || bound_copy_read != 0u)
            return fail(11);
        glDeleteBuffers(1, &index_name);
    }
    glDeleteBuffers(1, &name);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

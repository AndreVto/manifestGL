#include "mgl/gl.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint next_name;
static GLuint bound_array;
static GLuint bound_copy_read;
static GLuint bound_copy_write;
static unsigned indirect_dispatch_calls;
static unsigned char staging_data[16];
static int persistent_mapping_live;
static int storage_calls;
static int map_calls;
static int unmap_calls;
static int subdata_calls;
static GLbitfield storage_flags;
static unsigned char device_data[16];

static void gen(GLsizei n, GLuint *names) {
    for (GLsizei i = 0; i < n; ++i)
        names[i] = ++next_name;
}

static const GLubyte *get_string(GLenum name) {
    return name == GL_EXTENSIONS ? (const GLubyte *)"GL_EXT_buffer_storage" : NULL;
}

static void bind(GLenum target, GLuint name) {
    if (target == GL_ARRAY_BUFFER)
        bound_array = name;
    if (target == GL_COPY_READ_BUFFER)
        bound_copy_read = name;
    if (target == GL_COPY_WRITE_BUFFER)
        bound_copy_write = name;
}

static void storage(GLenum target, GLsizeiptr length, const void *source, GLbitfield flags) {
    if (target != GL_ARRAY_BUFFER || length != 16)
        return;
    storage_calls++;
    storage_flags = flags;
    if (source != NULL)
        memcpy(device_data, source, (size_t)length);
}

static void *map(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield flags) {
    if (offset < 0 || length <= 0 || (size_t)offset + (size_t)length > sizeof(device_data))
        return NULL;
    if (target == GL_ARRAY_BUFFER) {
        if (bound_array == 0u || offset != 0 || length != 16 ||
            ((flags & GL_MAP_READ_BIT) == 0u && (flags & GL_MAP_PERSISTENT_BIT) == 0u))
            return NULL;
        persistent_mapping_live = 1;
    } else if (target == GL_COPY_READ_BUFFER) {
        if (bound_copy_read == 0u || flags != GL_MAP_READ_BIT ||
            (bound_copy_read == bound_array && persistent_mapping_live))
            return NULL;
    } else
        return NULL;
    map_calls++;
    return (target == GL_COPY_READ_BUFFER && bound_copy_read != bound_array
                ? staging_data : device_data) + offset;
}

static GLboolean unmap(GLenum target) {
    if (target != GL_ARRAY_BUFFER && target != GL_COPY_READ_BUFFER)
        return GL_FALSE;
    unmap_calls++;
    if (target == GL_ARRAY_BUFFER)
        persistent_mapping_live = 0;
    return GL_TRUE;
}

static void delete_buffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void buffer_data(GLenum target, GLsizeiptr size, const void *src, GLenum usage) {
    (void)target;
    (void)size;
    (void)src;
    (void)usage;
}

static void copy_buffer(GLenum read_target, GLenum write_target, GLintptr src, GLintptr dst,
                        GLsizeiptr size) {
    if (read_target == GL_COPY_READ_BUFFER && write_target == GL_COPY_WRITE_BUFFER &&
        bound_copy_read == bound_array && bound_copy_write != 0u &&
        bound_copy_write != bound_copy_read && src == 0 && dst == 0 && size == 16) {
        memcpy(staging_data, device_data, sizeof(staging_data));
    }
}

static void dispatch_indirect(GLintptr offset) {
    if (offset == 0)
        ++indirect_dispatch_calls;
}

static void subdata(GLenum target, GLintptr offset, GLsizeiptr length, const void *src) {
    (void)target;
    (void)offset;
    (void)length;
    (void)src;
    subdata_calls++;
}

static int check(struct mgl_context *ctx, int condition, int code) {
    GLenum err = mgl_error_take(&ctx->errors);
    if (!condition || err != GL_NO_ERROR) {
        fprintf(stderr, "step %d: condition %d error %04x\n", code, condition, err);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint ids[2];
    unsigned char contents[16] = {1, 2, 3, 4, 5};
    unsigned char out = 0;
    unsigned char *ptr;
    GLint mapped_state = GL_FALSE;
    struct mgl_program *program;
    struct mgl_shader *shader;
    GLbitfield flags = GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT |
                       GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    int result;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenBuffers = gen;
    native.glesDeleteBuffers = delete_buffers;
    native.glesBufferData = buffer_data;
    native.glesCopyBufferSubData = copy_buffer;
    native.glesDispatchComputeIndirect = dispatch_indirect;
    native.glesBindBuffer = bind;
    native.glesGetString = get_string;
    native.glesBufferStorageEXT = storage;
    native.glesMapBufferRange = map;
    native.glesUnmapBuffer = unmap;
    native.glesBufferSubData = subdata;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);
    glCreateBuffers(2, ids);
    glBindBuffer(GL_ARRAY_BUFFER, ids[0]);
    glNamedBufferStorage(ids[1], 16, contents, flags);
    result = check(&ctx,
                   storage_calls == 1 && storage_flags == flags &&
                       bound_array == mgl_buffer_find(&ctx, ids[0])->native_name,
                   2);
    if (result)
        return result;
    glBindBuffer(GL_ARRAY_BUFFER, ids[1]);
    ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, flags & ~GL_DYNAMIC_STORAGE_BIT);
    result = check(&ctx, ptr == device_data && map_calls == 1 && ptr[0] == 1, 3);
    if (result)
        return result;
    program = calloc(1, sizeof(*program));
    shader = calloc(1, sizeof(*shader));
    if (program == NULL || shader == NULL)
        return 6;
    shader->type = GL_COMPUTE_SHADER;
    program->name = 8u;
    program->linked = GL_TRUE;
    program->linked_compute = GL_TRUE;
    program->attached[0] = shader;
    program->attached_count = 1u;
    program->next = ctx.share_group->programs;
    ctx.share_group->programs = program;
    ctx.current_program = program->name;

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, ids[1]);
    glDispatchComputeIndirect(0);
    result = check(&ctx, indirect_dispatch_calls == 1, 6);
    if (result)
        return result;
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, ids[1]);
    memset(ptr, 0, 16);
    glDrawArraysIndirect(GL_POINTS, (const void *)(uintptr_t)0u);
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAPPED, &mapped_state);
    result = check(&ctx, ptr == device_data && mapped_state == GL_TRUE, 7);
    if (result)
        return result;
    ptr[2] = 81;
    result = check(
        &ctx, glUnmapBuffer(GL_ARRAY_BUFFER) == GL_TRUE && unmap_calls == 2 && subdata_calls == 0,
        4);
    if (result)
        return result;
    glGetBufferSubData(GL_ARRAY_BUFFER, 2, 1, &out);
    result = check(&ctx, out == 81, 5);
    if (result)
        return result;

    ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, GL_MAP_READ_BIT);
    result = check(&ctx, ptr == device_data, 8);
    if (result)
        return result;
    glDispatchComputeIndirect(0);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return 9;
    glDrawArraysIndirect(GL_POINTS, (const void *)(uintptr_t)0u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return 10;
    result = check(&ctx, glUnmapBuffer(GL_ARRAY_BUFFER) == GL_TRUE, 11);
    if (result)
        return result;
    glDeleteBuffers(2, ids);
    program->attached_count = 0u;
    free(shader);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

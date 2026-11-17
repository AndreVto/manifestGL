#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/objects.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_buffer;
    unsigned dispatch_calls;
    unsigned indirect_calls;
    GLintptr indirect_offset;
    GLuint native_copy_read;
    GLuint native_copy_write;
    unsigned copies;
    GLuint groups[3];
    unsigned barrier_calls;
    GLbitfield barrier_bits;
    unsigned region_calls;
    GLbitfield region_bits;
    GLenum indexed_target;
    GLuint indexed_index;
    GLuint indexed_buffer;
    GLintptr indexed_offset;
    GLsizeiptr indexed_size;
} cap;

static GLuint gpu_command[3] = {9u, 10u, 11u};
static unsigned char staged_command[sizeof(gpu_command)];

static void fake_gen_buffers(GLsizei n, GLuint *buffers) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        buffers[i] = ++cap.next_buffer;
}

static void fake_delete_buffers(GLsizei n, const GLuint *buffers) {
    (void)n;
    (void)buffers;
}

static void fake_bind_buffer(GLenum target, GLuint buffer) {
    if (target == GL_COPY_READ_BUFFER)
        cap.native_copy_read = buffer;
    else if (target == GL_COPY_WRITE_BUFFER)
        cap.native_copy_write = buffer;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)target;
    (void)size;
    (void)data;
    (void)usage;
}

static void fake_bind_buffer_base(GLenum target, GLuint index, GLuint buffer) {
    cap.indexed_target = target;
    cap.indexed_index = index;
    cap.indexed_buffer = buffer;
    cap.indexed_offset = 0;
    cap.indexed_size = 0;
}

static void fake_bind_buffer_range(GLenum target, GLuint index, GLuint buffer, GLintptr offset,
                                   GLsizeiptr size) {
    cap.indexed_target = target;
    cap.indexed_index = index;
    cap.indexed_buffer = buffer;
    cap.indexed_offset = offset;
    cap.indexed_size = size;
}

static void fake_link_program(GLuint program) {
    (void)program;
}

static void fake_get_programiv(GLuint program, GLenum pname, GLint *value) {
    (void)program;
    *value = pname == GL_LINK_STATUS ? GL_TRUE : 0;
}

static GLint fake_get_location(GLuint program, const GLchar *name) {
    (void)program;
    (void)name;
    return -1;
}

static void fake_detach_shader(GLuint program, GLuint shader) {
    (void)program;
    (void)shader;
}

static void fake_use_program(GLuint program) {
    (void)program;
}

static void fake_dispatch(GLuint x, GLuint y, GLuint z) {
    ++cap.dispatch_calls;
    cap.groups[0] = x;
    cap.groups[1] = y;
    cap.groups[2] = z;
}

static void fake_copy(GLenum read, GLenum write, GLintptr src, GLintptr dst, GLsizeiptr len) {
    if (read == GL_COPY_READ_BUFFER && write == GL_COPY_WRITE_BUFFER &&
        src == 0 && dst == 0 && len == (GLsizeiptr)sizeof(gpu_command)) {
        memcpy(staged_command, gpu_command, sizeof(gpu_command));
        ++cap.copies;
    }
}

static void *fake_map(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    if (target != GL_COPY_READ_BUFFER || offset < 0 ||
        (size_t)offset + (size_t)length > sizeof(gpu_command) || access != GL_MAP_READ_BIT)
        return NULL;
    if (cap.native_copy_read != 0u && cap.native_copy_read != 1u)
        return staged_command + (size_t)offset;
    return (unsigned char *)gpu_command + (size_t)offset;
}

static GLboolean fake_unmap(GLenum target) {
    return target == GL_COPY_READ_BUFFER ? GL_TRUE : GL_FALSE;
}

static void fake_dispatch_indirect(GLintptr indirect) {
    ++cap.indirect_calls;
    cap.indirect_offset = indirect;
}

static void fake_barrier(GLbitfield bits) {
    ++cap.barrier_calls;
    cap.barrier_bits = bits;
}

static void fake_barrier_region(GLbitfield bits) {
    ++cap.region_calls;
    cap.region_bits = bits;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "compute execution failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    struct mgl_shader *shader;
    GLuint buffer = 0u;
    GLuint command[3] = {5u, 6u, 7u};
    GLint value = 0;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesBindBufferBase = fake_bind_buffer_base;
    native.glesBindBufferRange = fake_bind_buffer_range;
    native.glesDispatchCompute = fake_dispatch;
    native.glesLinkProgram = fake_link_program;
    native.glesGetProgramiv = fake_get_programiv;
    native.glesGetUniformLocation = fake_get_location;
    native.glesGetAttribLocation = fake_get_location;
    native.glesDetachShader = fake_detach_shader;
    native.glesUseProgram = fake_use_program;
    native.glesMemoryBarrier = fake_barrier;
    native.glesMemoryBarrierByRegion = fake_barrier_region;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    program = calloc(1u, sizeof(*program));
    shader = calloc(1u, sizeof(*shader));
    if (program == NULL || shader == NULL)
        return fail(2, "program allocation");
    shader->name = 11u;
    shader->type = GL_COMPUTE_SHADER;
    shader->compiled = GL_TRUE;
    shader->native_name = 110u;
    shader->attach_count = 1u;
    shader->next = ctx.share_group->shaders;
    ctx.share_group->shaders = shader;
    program->name = 7u;
    program->native_name = 70u;
    program->attached[0] = shader;
    program->attached_count = 1u;
    program->next = ctx.share_group->programs;
    ctx.share_group->programs = program;
    glLinkProgram(program->name);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || !program->linked ||
        !program->linked_compute)
        return fail(21, "compute stage recorded at successful link");
    glUseProgram(program->name);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || !ctx.current_program_executable)
        return fail(22, "compute executable installed");

    glDispatchCompute(2u, 3u, 4u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.dispatch_calls != 1u ||
        cap.groups[0] != 2u || cap.groups[1] != 3u || cap.groups[2] != 4u)
        return fail(3, "direct dispatch");

    glDetachShader(program->name, shader->name);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || program->attached_count != 0u ||
        !program->linked_compute)
        return fail(23, "detachment preserves compute executable identity");
    ctx.immediate.active = GL_TRUE;
    glDispatchCompute(1u, 1u, 1u);
    glDispatchComputeIndirect(0);
    ctx.immediate.active = GL_FALSE;
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION ||
        mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.dispatch_calls != 1u)
        return fail(15, "compute dispatch during immediate mode");
    glDispatchCompute(2u, 3u, 4u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.dispatch_calls != 2u)
        return fail(16, "detached compute shader preserves linked stage");
    program->linked_compute = GL_FALSE;
    glDispatchCompute(1u, 1u, 1u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || cap.dispatch_calls != 2u)
        return fail(17, "graphics program cannot dispatch compute");
    program->linked_compute = GL_TRUE;
    cap.dispatch_calls = 1u;

    glGenBuffers(1, &buffer);
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, buffer);
    glBufferData(GL_DISPATCH_INDIRECT_BUFFER, (GLsizeiptr)sizeof(command), command, GL_STATIC_DRAW);
    glGetIntegerv(GL_DISPATCH_INDIRECT_BUFFER_BINDING, &value);
    if ((GLuint)value != buffer)
        return fail(4, "dispatch indirect binding query");
    glDispatchComputeIndirect(0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.dispatch_calls != 2u ||
        cap.groups[0] != 5u || cap.groups[1] != 6u || cap.groups[2] != 7u)
        return fail(5, "indirect dispatch from interface shadow");
    native.glesMapBufferRange = fake_map;
    native.glesUnmapBuffer = fake_unmap;
    glDispatchComputeIndirect(0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.dispatch_calls != 3u ||
        cap.groups[0] != 9u || cap.groups[1] != 10u || cap.groups[2] != 11u)
        return fail(11, "GPU command bytes override stale shadow");
    {
        struct mgl_buffer *obj = mgl_buffer_find(&ctx, buffer);
        obj->native_immutable = GL_TRUE;
        obj->storage_flags = 0u;
        native.glesCopyBufferSubData = fake_copy;
        glDispatchComputeIndirect(0);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.dispatch_calls != 4u ||
            cap.groups[0] != 9u || cap.groups[1] != 10u || cap.groups[2] != 11u ||
            cap.copies != 1u)
            return fail(12, "immutable GPU command uses staging copy");
        obj->native_immutable = GL_FALSE;
    }
    native.glesDispatchComputeIndirect = fake_dispatch_indirect;
    ctx.immediate.active = GL_TRUE;
    glDispatchComputeIndirect(0);
    ctx.immediate.active = GL_FALSE;
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || cap.indirect_calls != 0u)
        return fail(18, "native indirect dispatch during immediate mode");
    glDispatchComputeIndirect(0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.indirect_calls != 1u ||
        cap.indirect_offset != 0 || cap.dispatch_calls != 4u)
        return fail(10, "native dispatch does not use CPU shadow");
    glDispatchComputeIndirect(2);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(6, "indirect alignment validation");

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3u, buffer);
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 3u, &value);
    if ((GLuint)value != buffer || cap.indexed_target != GL_SHADER_STORAGE_BUFFER ||
        cap.indexed_index != 3u)
        return fail(7, "shader storage base binding");
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 2u, buffer, 0, (GLsizeiptr)sizeof(command));
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_SIZE, 2u, &value);
    if (value != (GLint)sizeof(command) || cap.indexed_offset != 0 ||
        cap.indexed_size != (GLsizeiptr)sizeof(command))
        return fail(8, "shader storage range binding");

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.barrier_calls != 1u ||
        cap.barrier_bits != (GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT))
        return fail(9, "memory barrier");
    glMemoryBarrierByRegion(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.region_calls != 1u ||
        cap.region_bits != (GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT))
        return fail(10, "regional barrier");
    glMemoryBarrierByRegion(GL_COMMAND_BARRIER_BIT);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(11, "regional barrier validation");

    if (glXGetProcAddress((const GLubyte *)"glDispatchCompute") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMemoryBarrier") == NULL)
        return fail(12, "proc address coverage");

    glDeleteBuffers(1, &buffer);
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 3u, &value);
    if (value != 0)
        return fail(13, "buffer deletion clears indexed storage binding");

    program->linked = GL_FALSE;
    ctx.current_program_executable = GL_TRUE;
    glDispatchCompute(1u, 1u, 1u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(19, "installed compute executable survives failed relink");
    ctx.current_program_executable = GL_FALSE;
    glDispatchCompute(1u, 1u, 1u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(20, "failed link without an installed executable");
    ctx.current_program = 0u;
    glDispatchCompute(1u, 1u, 1u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(14, "dispatch requires compute program");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL compute execution unit test: PASS");
    return 0;
}

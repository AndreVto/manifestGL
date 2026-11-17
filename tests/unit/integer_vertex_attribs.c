#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "shader/shader_store.h"
#include "vertex/draw_snapshot.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_buffer;
    unsigned char *array_data;
    size_t array_size;
    GLuint ipointer_location;
    GLint ipointer_size;
    GLenum ipointer_type;
    size_t ipointer_offset;
    unsigned ipointer_calls;
    GLuint signed_location;
    GLint signed_value[4];
    unsigned signed_calls;
    GLuint unsigned_location;
    GLuint unsigned_value[4];
    unsigned unsigned_calls;
    unsigned draw_calls;
} cap;

static void fake_get_integerv(GLenum pname, GLint *value) {
    *value = pname == GL_MAX_VERTEX_ATTRIBS ? 16 : 0;
}

static void fake_gen_buffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_buffer;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)usage;
    if (target != GL_ARRAY_BUFFER)
        return;
    free(cap.array_data);
    cap.array_data = NULL;
    cap.array_size = 0u;
    if (size > 0) {
        cap.array_data = malloc((size_t)size);
        if (cap.array_data != NULL) {
            memcpy(cap.array_data, data, (size_t)size);
            cap.array_size = (size_t)size;
        }
    }
}

static void fake_vertex_attrib_pointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                       GLsizei stride, const void *pointer) {
    (void)index;
    (void)size;
    (void)type;
    (void)normalized;
    (void)stride;
    (void)pointer;
}

static void fake_vertex_attrib_ipointer(GLuint index, GLint size, GLenum type, GLsizei stride,
                                        const void *pointer) {
    (void)stride;
    cap.ipointer_location = index;
    cap.ipointer_size = size;
    cap.ipointer_type = type;
    cap.ipointer_offset = (size_t)(uintptr_t)pointer;
    ++cap.ipointer_calls;
}

static void fake_enable_vertex_attrib(GLuint index) {
    (void)index;
}

static void fake_disable_vertex_attrib(GLuint index) {
    (void)index;
}

static void fake_vertex_attrib4fv(GLuint index, const GLfloat *v) {
    (void)index;
    (void)v;
}

static void fake_vertex_attrib_i4iv(GLuint index, const GLint *v) {
    cap.signed_location = index;
    memcpy(cap.signed_value, v, sizeof(cap.signed_value));
    ++cap.signed_calls;
}

static void fake_vertex_attrib_i4uiv(GLuint index, const GLuint *v) {
    cap.unsigned_location = index;
    memcpy(cap.unsigned_value, v, sizeof(cap.unsigned_value));
    ++cap.unsigned_calls;
}

static void fake_use_program(GLuint program) {
    (void)program;
}

static void fake_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    (void)mode;
    (void)first;
    (void)count;
    ++cap.draw_calls;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "integer vertex attrib failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLint source[2][2] = {{101, -202}, {303, -404}};
    GLint original[2][2];
    GLuint current_u[4] = {4000000001u, 17u, 23u, 29u};
    GLuint queried_u[4] = {0u, 0u, 0u, 0u};
    GLint integer_flag = 0;
    struct mgl_draw_snapshot *snapshot;
    unsigned before;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    memcpy(original, source, sizeof(original));

    native.glesGetIntegerv = fake_get_integerv;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesVertexAttribPointer = fake_vertex_attrib_pointer;
    native.glesVertexAttribIPointer = fake_vertex_attrib_ipointer;
    native.glesEnableVertexAttribArray = fake_enable_vertex_attrib;
    native.glesDisableVertexAttribArray = fake_disable_vertex_attrib;
    native.glesVertexAttrib4fv = fake_vertex_attrib4fv;
    native.glesVertexAttribI4iv = fake_vertex_attrib_i4iv;
    native.glesVertexAttribI4uiv = fake_vertex_attrib_i4uiv;
    native.glesUseProgram = fake_use_program;
    native.glesDrawArrays = fake_draw_arrays;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.native_max_vertex_attribs = 16;
    mgl_context_set_current_for_test(&ctx);

    program = calloc(1, sizeof(*program));
    if (program == NULL)
        return fail(2, "program allocation");
    mgl_program_init_metadata(program);
    program->name = 71u;
    program->native_name = 171u;
    program->linked = GL_TRUE;
    program->active_generic_mask = (UINT32_C(1) << 5) | (UINT32_C(1) << 6);
    program->active_generic_type[5] = GL_INT_VEC2;
    program->active_generic_type[6] = GL_UNSIGNED_INT_VEC4;
    program->next = ctx.share_group->programs;
    ctx.share_group->programs = program;
    ctx.current_program = program->name;

    glVertexAttribIPointer(5u, 2, GL_INT, 0, source);
    glEnableVertexAttribArray(5u);
    glVertexAttribI4uiv(6u, current_u);
    glGetVertexAttribiv(5u, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &integer_flag);
    if (integer_flag != 1)
        return fail(3, "integer-array query");
    glGetVertexAttribIuiv(6u, GL_CURRENT_VERTEX_ATTRIB, queried_u);
    if (memcmp(queried_u, current_u, sizeof(current_u)) != 0)
        return fail(4, "exact unsigned current value");

    glDrawArrays(GL_POINTS, 0, 2);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(5, "integer draw interface error");
    if (cap.draw_calls != 1u || cap.ipointer_calls != 1u || cap.ipointer_location != 5u ||
        cap.ipointer_size != 2 || cap.ipointer_type != GL_INT || cap.ipointer_offset != 0u) {
        return fail(6, "integer pointer realization");
    }
    if (cap.array_size != sizeof(source) || memcmp(cap.array_data, source, sizeof(source)) != 0) {
        return fail(7, "integer array packed bytes");
    }
    if (cap.unsigned_calls == 0u || cap.unsigned_location != 6u ||
        memcmp(cap.unsigned_value, current_u, sizeof(current_u)) != 0) {
        return fail(8, "unsigned disabled current realization");
    }

    glDisableVertexAttribArray(5u);
    glVertexAttribI4i(5u, -11, 22, -33, 44);
    before = cap.signed_calls;
    glDrawArrays(GL_POINTS, 0, 1);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(9, "signed current draw error");
    if (cap.signed_calls != before + 1u || cap.signed_location != 5u ||
        cap.signed_value[0] != -11 || cap.signed_value[1] != 22 || cap.signed_value[2] != -33 ||
        cap.signed_value[3] != 44) {
        return fail(10, "signed disabled current realization");
    }

    glVertexAttribIPointer(5u, 2, GL_INT, 0, source);
    glEnableVertexAttribArray(5u);
    snapshot = mgl_draw_snapshot_capture_arrays(&ctx, GL_POINTS, 0, 2);
    if (snapshot == NULL)
        return fail(11, "integer draw snapshot capture");
    memset(source, 0x5a, sizeof(source));
    before = cap.draw_calls;
    mgl_draw_snapshot_replay(&ctx, snapshot);
    mgl_draw_snapshot_destroy(snapshot);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(12, "integer snapshot replay error");
    if (cap.draw_calls != before + 1u || cap.array_size != sizeof(original) ||
        memcmp(cap.array_data, original, sizeof(original)) != 0) {
        return fail(13, "integer snapshot did not own compile-time bytes");
    }

    if (glXGetProcAddress((const GLubyte *)"glVertexAttribIPointer") == NULL ||
        glXGetProcAddress((const GLubyte *)"glVertexAttribI4uiv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetVertexAttribIuiv") == NULL) {
        return fail(16, "proc-address coverage");
    }

    free(cap.array_data);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("integer vertex attribute semantics passed");
    return 0;
}

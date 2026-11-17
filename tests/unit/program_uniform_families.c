#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/objects.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint active_native_program;
    unsigned use_count;
    unsigned float_calls;
    GLuint float_program;
    GLint float_location;
    GLfloat float_values[2];
    unsigned matrix_calls;
    GLfloat matrix_values[4];
    GLuint matrix_program;
    GLboolean matrix_transpose;
    unsigned native_calls;
    GLuint native_program;
    GLint native_location;
    GLuint native_values[4];
    unsigned vector4_calls;
    GLuint vector4_program;
    GLint vector4_location;
    GLuint vector4_values[4];
} cap;

static void fake_use_program(GLuint program) {
    cap.active_native_program = program;
    ++cap.use_count;
}

static void fake_program_uniform2f(GLuint program, GLint location, GLfloat a, GLfloat b) {
    ++cap.float_calls;
    cap.float_program = program;
    cap.float_location = location;
    cap.float_values[0] = a;
    cap.float_values[1] = b;
}

static void fake_program_uniform3ui(GLuint program, GLint location, GLuint a, GLuint b, GLuint c) {
    ++cap.native_calls;
    cap.native_program = program;
    cap.native_location = location;
    cap.native_values[0] = a;
    cap.native_values[1] = b;
    cap.native_values[2] = c;
}

static void fake_program_uniform4ui(GLuint program, GLint location, GLuint a, GLuint b, GLuint c,
                                    GLuint d) {
    ++cap.vector4_calls;
    cap.vector4_program = program;
    cap.vector4_location = location;
    cap.vector4_values[0] = a;
    cap.vector4_values[1] = b;
    cap.vector4_values[2] = c;
    cap.vector4_values[3] = d;
}

static void fake_program_matrix2(GLuint program, GLint location, GLsizei count, GLboolean transpose,
                                 const GLfloat *value) {
    (void)location;
    if (count != 1)
        return;
    ++cap.matrix_calls;
    cap.matrix_program = program;
    cap.matrix_transpose = transpose;
    memcpy(cap.matrix_values, value, sizeof(cap.matrix_values));
}

static int fail(int code, const char *message) {
    fprintf(stderr, "program uniform families failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *current;
    struct mgl_program *target;
    GLfloat values[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));

    native.glesUseProgram = fake_use_program;
    native.glesProgramUniform2f = fake_program_uniform2f;
    native.glesProgramUniform3ui = fake_program_uniform3ui;
    native.glesProgramUniform4ui = fake_program_uniform4ui;
    native.glesProgramUniformMatrix2fv = fake_program_matrix2;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    current = calloc(1u, sizeof(*current));
    target = calloc(1u, sizeof(*target));
    if (current == NULL || target == NULL)
        return fail(2, "program allocation");
    current->name = 7u;
    current->native_name = 77u;
    current->linked = GL_TRUE;
    target->name = 8u;
    target->native_name = 88u;
    target->linked = GL_TRUE;
    current->next = target;
    target->next = ctx.share_group->programs;
    ctx.share_group->programs = current;
    ctx.current_program = 7u;
    cap.active_native_program = 77u;

    glProgramUniform2f(8u, 4, 1.5f, -2.5f);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.float_calls != 1u ||
        cap.float_program != 88u || cap.float_location != 4 ||
        cap.float_values[0] != 1.5f || cap.float_values[1] != -2.5f ||
        cap.use_count != 0u || ctx.current_program != 7u)
        return fail(3, "direct program uniform updates without changing current program");

    glProgramUniform2f(7u, 5, 3.0f, 4.0f);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.float_calls != 2u ||
        cap.float_program != 77u || cap.float_location != 5 ||
        cap.float_values[0] != 3.0f || cap.float_values[1] != 4.0f || cap.use_count != 0u)
        return fail(4, "uniform update on current program");

    glProgramUniform3ui(8u, 6, 10u, 20u, 30u);
    if (cap.native_calls != 1u || cap.native_program != 88u || cap.native_location != 6 ||
        cap.native_values[2] != 30u || cap.active_native_program != 77u)
        return fail(5, "native direct program-uniform path");

    {
        unsigned before = cap.float_calls;
        glProgramUniform2f(8u, -1, 7.0f, 8.0f);
        if (cap.float_calls != before || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(6, "minus-one location ignored after program validation");
    }

    glProgramUniform2fv(8u, 1, -1, values);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(7, "negative count validation");
    glProgramUniformMatrix2fv(8u, 1, 1, GL_TRUE, values);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.matrix_calls != 1u ||
        cap.matrix_program != 88u || cap.matrix_transpose != GL_FALSE ||
        cap.matrix_values[0] != 1.0f || cap.matrix_values[1] != 3.0f ||
        cap.matrix_values[2] != 2.0f || cap.matrix_values[3] != 4.0f)
        return fail(8, "transpose true forwarded as column-major GLES data");
    {
        unsigned old_matrix = cap.matrix_calls;
        unsigned old_float = cap.float_calls;
        glBegin(GL_POINTS);
        glProgramUniform2f(8u, 4, 2.0f, 3.0f);
        glProgramUniformMatrix2fv(8u, 1, -1, GL_TRUE, values);
        glEnd();
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || cap.matrix_calls != old_matrix ||
            cap.float_calls != old_float)
            return fail(14, "Begin/End error precedence and native preservation");
    }

    glProgramUniform2f(999u, 1, 1.0f, 2.0f);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(9, "invalid program validation");

    ctx.transform_feedback_active = GL_TRUE;
    ctx.transform_feedback_paused = GL_FALSE;
    glProgramUniform2f(8u, 2, 5.0f, 6.0f);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.float_calls != 3u ||
        cap.float_program != 88u || cap.float_location != 2 ||
        cap.float_values[0] != 5.0f || cap.float_values[1] != 6.0f ||
        cap.use_count != 0u || ctx.current_program != 7u)
        return fail(10, "direct uniform update during active transform feedback");

    glProgramUniform3ui(8u, 7, 1u, 2u, 3u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.native_calls != 2u)
        return fail(11, "native path during active transform feedback");
    ctx.transform_feedback_active = GL_FALSE;

    glProgramUniform4ui(8u, 3, 1u, 2u, 3u, 4u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.vector4_calls != 1u ||
        cap.vector4_program != 88u || cap.vector4_location != 3 ||
        cap.vector4_values[0] != 1u || cap.vector4_values[1] != 2u ||
        cap.vector4_values[2] != 3u || cap.vector4_values[3] != 4u)
        return fail(12, "native unsigned vector uniform");

    if (glXGetProcAddress((const GLubyte *)"glProgramUniform2f") == NULL ||
        glXGetProcAddress((const GLubyte *)"glProgramUniformMatrix4x3fv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glProgramUniform4uiv") == NULL)
        return fail(13, "proc-address coverage");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL program uniform families unit test: PASS");
    return 0;
}

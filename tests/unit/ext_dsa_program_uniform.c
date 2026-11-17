#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/objects.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    unsigned scalar_f_calls;
    unsigned vector_f_calls;
    unsigned vector_i_calls;
    unsigned matrix_calls;
    GLuint program;
    GLint location;
    GLsizei count;
    GLfloat scalar_f_values[4];
    GLfloat vector_f_values[32];
    GLfloat matrix_values[32];
    GLint vector_i_values[16];
} cap;

static void fake_program_uniform2f(GLuint program, GLint location, GLfloat a, GLfloat b) {
    ++cap.scalar_f_calls;
    cap.program = program;
    cap.location = location;
    cap.count = 1;
    cap.scalar_f_values[0] = a;
    cap.scalar_f_values[1] = b;
}

static void fake_program_uniform2fv(GLuint program, GLint location, GLsizei count,
                                    const GLfloat *value) {
    size_t n = (size_t)count * 2u;
    size_t i;
    ++cap.vector_f_calls;
    cap.program = program;
    cap.location = location;
    cap.count = count;
    if (n > 32u)
        n = 32u;
    for (i = 0u; i < n; ++i)
        cap.vector_f_values[i] = value[i];
}

static void fake_program_uniform3iv(GLuint program, GLint location, GLsizei count,
                                    const GLint *value) {
    size_t n = (size_t)count * 3u;
    size_t i;
    ++cap.vector_i_calls;
    cap.program = program;
    cap.location = location;
    cap.count = count;
    if (n > 16u)
        n = 16u;
    for (i = 0u; i < n; ++i)
        cap.vector_i_values[i] = value[i];
}

static void fake_program_uniform_matrix3fv(GLuint program, GLint location, GLsizei count,
                                           GLboolean transpose, const GLfloat *value) {
    size_t n = (size_t)count * 9u;
    size_t i;
    (void)transpose;
    ++cap.matrix_calls;
    cap.program = program;
    cap.location = location;
    cap.count = count;
    if (n > 32u)
        n = 32u;
    for (i = 0u; i < n; ++i)
        cap.matrix_values[i] = value[i];
}

static int fail(int code, const char *message) {
    fprintf(stderr, "EXT DSA program-uniform failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLuint list_owned;
    GLuint list_late;
    GLuint list_compile_execute;
    GLuint list_transpose;
    GLfloat vec[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    GLint ivec[3] = {7, 8, 9};
    GLfloat matrix[9] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
    unsigned before;
    size_t i;
    static const char *const proc_names[] = {
        "glProgramUniform1fEXT",        "glProgramUniform2fEXT",
        "glProgramUniform3fEXT",        "glProgramUniform4fEXT",
        "glProgramUniform1iEXT",        "glProgramUniform2iEXT",
        "glProgramUniform3iEXT",        "glProgramUniform4iEXT",
        "glProgramUniform1fvEXT",       "glProgramUniform2fvEXT",
        "glProgramUniform3fvEXT",       "glProgramUniform4fvEXT",
        "glProgramUniform1ivEXT",       "glProgramUniform2ivEXT",
        "glProgramUniform3ivEXT",       "glProgramUniform4ivEXT",
        "glProgramUniformMatrix2fvEXT", "glProgramUniformMatrix3fvEXT",
        "glProgramUniformMatrix4fvEXT"};

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesProgramUniform2f = fake_program_uniform2f;
    native.glesProgramUniform2fv = fake_program_uniform2fv;
    native.glesProgramUniform3iv = fake_program_uniform3iv;
    native.glesProgramUniformMatrix3fv = fake_program_uniform_matrix3fv;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    program = calloc(1u, sizeof(*program));
    if (program == NULL)
        return fail(2, "program allocation");
    program->name = 8u;
    program->native_name = 88u;
    program->linked = GL_TRUE;
    program->next = ctx.share_group->programs;
    ctx.share_group->programs = program;

    glProgramUniform2fEXT(8u, 4, 1.5f, -2.5f);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.scalar_f_calls != 1u ||
        cap.program != 88u || cap.location != 4 || cap.scalar_f_values[0] != 1.5f ||
        cap.scalar_f_values[1] != -2.5f)
        return fail(3, "direct alias uses canonical program-uniform path");

    list_owned = glGenLists(1);
    if (list_owned == 0u)
        return fail(4, "list allocation");
    before = cap.vector_f_calls + cap.vector_i_calls + cap.matrix_calls;
    glNewList(list_owned, GL_COMPILE);
    glProgramUniform2fvEXT(8u, 5, 2, vec);
    glProgramUniform3ivEXT(8u, 6, 1, ivec);
    glProgramUniformMatrix3fvEXT(8u, 7, 1, GL_FALSE, matrix);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        cap.vector_f_calls + cap.vector_i_calls + cap.matrix_calls != before)
        return fail(5, "GL_COMPILE suppresses EXT uniform execution");

    for (i = 0u; i < 4u; ++i)
        vec[i] = 100.0f + (GLfloat)i;
    for (i = 0u; i < 3u; ++i)
        ivec[i] = 100 + (GLint)i;
    for (i = 0u; i < 9u; ++i)
        matrix[i] = 200.0f + (GLfloat)i;
    glCallList(list_owned);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.vector_f_calls != 1u || cap.count != 1 ||
        cap.matrix_calls != 1u || cap.vector_i_calls != 1u)
        return fail(6, "owned EXT uniform commands replay");
    if (cap.vector_f_values[0] != 1.0f || cap.vector_f_values[1] != 2.0f ||
        cap.vector_f_values[2] != 3.0f || cap.vector_f_values[3] != 4.0f ||
        cap.vector_i_values[0] != 7 || cap.vector_i_values[1] != 8 || cap.vector_i_values[2] != 9 ||
        cap.matrix_values[0] != 1.0f || cap.matrix_values[8] != 9.0f)
        return fail(7, "vector and matrix payloads owned at compile time");
    glCallList(list_owned);
    if (cap.vector_f_calls != 2u || cap.matrix_calls != 2u || cap.vector_i_calls != 2u)
        return fail(8, "repeat replay");

    list_late = glGenLists(1);
    glNewList(list_late, GL_COMPILE);
    glProgramUniform2fEXT(8u, 9, 9.0f, 10.0f);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(9, "late-binding list compilation");
    program->linked = GL_FALSE;
    before = cap.scalar_f_calls;
    glCallList(list_late);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || cap.scalar_f_calls != before)
        return fail(10, "program link state resolved at replay");
    program->linked = GL_TRUE;
    glCallList(list_late);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.scalar_f_calls != before + 1u ||
        cap.location != 9 || cap.scalar_f_values[0] != 9.0f || cap.scalar_f_values[1] != 10.0f)
        return fail(11, "relinked program executes recorded scalar");

    before = cap.vector_f_calls;
    glNewList(glGenLists(1), GL_COMPILE);
    glProgramUniform2fvEXT(8u, 1, -1, vec);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE || cap.vector_f_calls != before)
        return fail(12, "negative count rejected while compiling");

    before = cap.matrix_calls;
    list_transpose = glGenLists(1);
    glNewList(list_transpose, GL_COMPILE);
    glProgramUniformMatrix3fvEXT(8u, 1, 1, GL_TRUE, matrix);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.matrix_calls != before)
        return fail(13, "transpose is legal and deferred during compilation");
    glCallList(list_transpose);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.matrix_calls != before + 1u ||
        cap.matrix_values[0] != 200.0f || cap.matrix_values[1] != 203.0f ||
        cap.matrix_values[2] != 206.0f || cap.matrix_values[3] != 201.0f ||
        cap.matrix_values[8] != 208.0f)
        return fail(17, "transpose list replay converts row-major payload");

    list_compile_execute = glGenLists(1);
    before = cap.scalar_f_calls;
    glNewList(list_compile_execute, GL_COMPILE_AND_EXECUTE);
    glProgramUniform2fEXT(8u, 11, 11.0f, 12.0f);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.scalar_f_calls != before + 1u)
        return fail(14, "compile-and-execute executes once during compilation");
    glCallList(list_compile_execute);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.scalar_f_calls != before + 2u)
        return fail(15, "compile-and-execute retained replay command");

    for (i = 0u; i < sizeof(proc_names) / sizeof(proc_names[0]); ++i) {
        if (glXGetProcAddress((const GLubyte *)proc_names[i]) == NULL)
            return fail(16, "proc-address coverage");
    }

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL EXT DSA program-uniform unit test: PASS");
    return 0;
}

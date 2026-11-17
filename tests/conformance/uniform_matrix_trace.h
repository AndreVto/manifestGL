#ifndef MGL_UNIFORM_MATRIX_TRACE_H
#define MGL_UNIFORM_MATRIX_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>
/* Existing matrix helpers use real linked Mesa GLSL state or independently
 * capture each GLES column-major upload in the host fixture. */
static GLuint matrix_trace_prepare(void);
static GLint matrix_trace_location(int index);
static void matrix_trace_fetch(int index, GLfloat *values);
static void matrix_trace_finish(GLuint program);
static int matrix_trace_equals(const GLfloat *a, const GLfloat *b, int n);
static unsigned uniform_matrix_failures;

static void uniform_matrix_row(const char *label, GLenum error_expected, int valid) {
    GLenum error = glGetError();
    if (error != error_expected || !valid) {
        fprintf(stderr, "core matrix reference %s: error=0x%x expected=0x%x valid=%d\n", label,
                (unsigned)error, (unsigned)error_expected, valid);
        ++uniform_matrix_failures;
    }
    printf("um_%s 0x%04x %d\n", label, (unsigned)error, valid);
}

#define CHECK_UPLOAD(label, index, call, expected, count)                                          \
    do {                                                                                           \
        call;                                                                                      \
        uniform_matrix_row(label "_upload", GL_NO_ERROR, 1);                                       \
        matrix_trace_fetch(index, actual);                                                         \
        uniform_matrix_row(label "_value", GL_NO_ERROR,                                            \
                           matrix_trace_equals(actual, expected, count));                          \
    } while (0)

static int uniform_matrix_trace_trace(void) {
    GLuint program = matrix_trace_prepare();
    GLfloat actual[16] = {0};
    const GLfloat raw4[4] = {1, 2, 3, 4}, expected4[4] = {1, 3, 2, 4};
    const GLfloat raw9[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9}, expected9[9] = {1, 4, 7, 2, 5, 8, 3, 6, 9};
    const GLfloat raw6[6] = {1, 2, 3, 4, 5, 6};
    const GLfloat expected23[6] = {1, 3, 5, 2, 4, 6};
    const GLfloat expected32[6] = {1, 4, 2, 5, 3, 6};
    const GLfloat raw8[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    const GLfloat expected24[8] = {1, 3, 5, 7, 2, 4, 6, 8};
    const GLfloat expected42[8] = {1, 5, 2, 6, 3, 7, 4, 8};
    const GLfloat raw12[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    const GLfloat expected34[12] = {1, 4, 7, 10, 2, 5, 8, 11, 3, 6, 9, 12};
    const GLfloat expected43[12] = {1, 5, 9, 2, 6, 10, 3, 7, 11, 4, 8, 12};
    const GLfloat raw24[24] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                               13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    const GLfloat expected43b[12] = {13, 17, 21, 14, 18, 22, 15, 19, 23, 16, 20, 24};
    const GLfloat raw16[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const GLfloat expected16[16] = {1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16};
    uniform_matrix_row("setup", GL_NO_ERROR, program != 0u);
    if (program == 0u)
        return 0;
    glUseProgram(program);
    uniform_matrix_row("bind", GL_NO_ERROR, 1);
    CHECK_UPLOAD("mat22", 4, glUniformMatrix2fv(matrix_trace_location(4), 1, GL_TRUE, raw4),
                 expected4, 4);
    CHECK_UPLOAD("mat33", 5, glUniformMatrix3fv(matrix_trace_location(5), 1, GL_TRUE, raw9),
                 expected9, 9);
    CHECK_UPLOAD("mat44", 3, glUniformMatrix4fv(matrix_trace_location(3), 1, GL_TRUE, raw16),
                 expected16, 16);
    CHECK_UPLOAD("mat23", 0, glUniformMatrix2x3fv(matrix_trace_location(0), 1, GL_TRUE, raw6),
                 expected23, 6);
    CHECK_UPLOAD("mat32", 6, glUniformMatrix3x2fv(matrix_trace_location(6), 1, GL_TRUE, raw6),
                 expected32, 6);
    CHECK_UPLOAD("mat24", 7, glUniformMatrix2x4fv(matrix_trace_location(7), 1, GL_TRUE, raw8),
                 expected24, 8);
    CHECK_UPLOAD("mat42", 8, glUniformMatrix4x2fv(matrix_trace_location(8), 1, GL_TRUE, raw8),
                 expected42, 8);
    CHECK_UPLOAD("mat34", 9, glUniformMatrix3x4fv(matrix_trace_location(9), 1, GL_TRUE, raw12),
                 expected34, 12);
    glUniformMatrix4x3fv(matrix_trace_location(1), 2, GL_TRUE, raw24);
    uniform_matrix_row("mat43_count2_upload", GL_NO_ERROR, 1);
    matrix_trace_fetch(1, actual);
    uniform_matrix_row("mat43_count2_first", GL_NO_ERROR,
                       matrix_trace_equals(actual, expected43, 12));
    matrix_trace_fetch(2, actual);
    uniform_matrix_row("mat43_count2_second", GL_NO_ERROR,
                       matrix_trace_equals(actual, expected43b, 12));
    glUniformMatrix4fv(matrix_trace_location(3), 1, GL_FALSE, raw16);
    uniform_matrix_row("notranspose_upload", GL_NO_ERROR, 1);
    matrix_trace_fetch(3, actual);
    uniform_matrix_row("notranspose_value", GL_NO_ERROR, matrix_trace_equals(actual, raw16, 16));
    glUniformMatrix2fv(matrix_trace_location(4), -1, GL_TRUE, raw4);
    uniform_matrix_row("negative_count", GL_INVALID_VALUE, 1);
    glBegin(GL_POINTS);
    glUniformMatrix2fv(matrix_trace_location(4), -1, GL_TRUE, raw4);
    glEnd();
    uniform_matrix_row("begin_count_precedence", GL_INVALID_OPERATION, 1);
    glBegin(GL_POINTS);
    glUniformMatrix2fv(matrix_trace_location(4), 1, GL_TRUE, raw4);
    glUniformMatrix3x2fv(-1, 1, GL_TRUE, raw6);
    glUniform1f(-1, 1.0f);
    glEnd();
    uniform_matrix_row("begin_no_native", GL_INVALID_OPERATION, 1);
    matrix_trace_fetch(4, actual);
    uniform_matrix_row("begin_preserved", GL_NO_ERROR, matrix_trace_equals(actual, expected4, 4));
    glUseProgram(0);
    uniform_matrix_row("unbind", GL_NO_ERROR, 1);
    matrix_trace_finish(program);
    uniform_matrix_row("cleanup", GL_NO_ERROR, 1);
    return uniform_matrix_failures == 0u;
}

#undef CHECK_UPLOAD
#endif

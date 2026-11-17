#ifndef MGL_PROGRAM_UNIFORM_MATRIX_TRACE_H
#define MGL_PROGRAM_UNIFORM_MATRIX_TRACE_H

#include "mgl/gl.h"
#include <stdio.h>
#include <string.h>

/* Each runner obtains the same observable matrix values through its own
 * independent uniform-storage implementation. */
static GLuint matrix_trace_prepare(void);
static GLint matrix_trace_location(int index);
static void matrix_trace_fetch(int index, GLfloat *values);
static void matrix_trace_finish(GLuint program);

static int matrix_trace_equals(const GLfloat *actual, const GLfloat *expected, int n) {
    int i;
    for (i = 0; i < n; ++i)
        if (actual[i] != expected[i])
            return 0;
    return 1;
}

static unsigned matrix_trace_failures;

static void matrix_trace_row(const char *name, int observed) {
    GLenum error = glGetError();
    GLenum expected = GL_NO_ERROR;
    if (strcmp(name, "negative_count") == 0)
        expected = GL_INVALID_VALUE;
    if (strncmp(name, "begin_", 6) == 0)
        expected = GL_INVALID_OPERATION;
    if (error != expected ||
        ((strcmp(name, "setup") == 0 || strncmp(name, "value_", 6) == 0) && !observed)) {
        fprintf(stderr, "matrix reference unexpected state/error at %s: 0x%x %d\n", name,
                (unsigned)error, observed);
        ++matrix_trace_failures;
    }
    printf("pm_%s 0x%04x %d\n", name, (unsigned)error, observed);
}

static int matrix_trace_trace(void) {
    GLuint program = matrix_trace_prepare();
    GLint location23 = matrix_trace_location(0);
    GLint location43 = matrix_trace_location(1);
    GLint location44 = matrix_trace_location(3);
    GLfloat values[16] = {0};
    const GLfloat row22[4] = {1, 2, 3, 4};
    const GLfloat col22[4] = {1, 3, 2, 4};
    const GLfloat row33[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    const GLfloat col33[9] = {1, 4, 7, 2, 5, 8, 3, 6, 9};
    const GLfloat row32[6] = {1, 2, 3, 4, 5, 6};
    const GLfloat col32[6] = {1, 4, 2, 5, 3, 6};
    const GLfloat row24[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    const GLfloat col24[8] = {1, 3, 5, 7, 2, 4, 6, 8};
    const GLfloat row42[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    const GLfloat col42[8] = {1, 5, 2, 6, 3, 7, 4, 8};
    const GLfloat row34[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    const GLfloat col34[12] = {1, 4, 7, 10, 2, 5, 8, 11, 3, 6, 9, 12};
    const GLfloat row23[6] = {1, 2, 3, 4, 5, 6};
    const GLfloat col23[6] = {1, 3, 5, 2, 4, 6};
    const GLfloat row43[24] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                               13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    const GLfloat col43a[12] = {1, 5, 9, 2, 6, 10, 3, 7, 11, 4, 8, 12};
    const GLfloat col43b[12] = {13, 17, 21, 14, 18, 22, 15, 19, 23, 16, 20, 24};
    const GLfloat col44[16] = {1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16};
    const GLfloat row44[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    matrix_trace_row("setup",
                     program != 0u && location23 >= 0 && location43 >= 0 && location44 >= 0);
    glProgramUniformMatrix2fv(program, matrix_trace_location(4), 1, GL_TRUE, row22);
    matrix_trace_row("transpose_22", 0);
    matrix_trace_fetch(4, values);
    matrix_trace_row("value_22", matrix_trace_equals(values, col22, 4));
    glProgramUniformMatrix3fv(program, matrix_trace_location(5), 1, GL_TRUE, row33);
    matrix_trace_row("transpose_33", 0);
    matrix_trace_fetch(5, values);
    matrix_trace_row("value_33", matrix_trace_equals(values, col33, 9));
    glProgramUniformMatrix3x2fv(program, matrix_trace_location(6), 1, GL_TRUE, row32);
    matrix_trace_row("transpose_32", 0);
    matrix_trace_fetch(6, values);
    matrix_trace_row("value_32", matrix_trace_equals(values, col32, 6));
    glProgramUniformMatrix2x4fv(program, matrix_trace_location(7), 1, GL_TRUE, row24);
    matrix_trace_row("transpose_24", 0);
    matrix_trace_fetch(7, values);
    matrix_trace_row("value_24", matrix_trace_equals(values, col24, 8));
    glProgramUniformMatrix4x2fv(program, matrix_trace_location(8), 1, GL_TRUE, row42);
    matrix_trace_row("transpose_42", 0);
    matrix_trace_fetch(8, values);
    matrix_trace_row("value_42", matrix_trace_equals(values, col42, 8));
    glProgramUniformMatrix3x4fv(program, matrix_trace_location(9), 1, GL_TRUE, row34);
    matrix_trace_row("transpose_34", 0);
    matrix_trace_fetch(9, values);
    matrix_trace_row("value_34", matrix_trace_equals(values, col34, 12));

    glProgramUniformMatrix2x3fv(program, location23, 1, GL_TRUE, row23);
    matrix_trace_row("transpose_23", 0);
    matrix_trace_fetch(0, values);
    matrix_trace_row("value_23", matrix_trace_equals(values, col23, 6));

    glProgramUniformMatrix4x3fv(program, location43, 2, GL_TRUE, row43);
    matrix_trace_row("transpose_43_count2", 0);
    matrix_trace_fetch(1, values);
    matrix_trace_row("value_43_0", matrix_trace_equals(values, col43a, 12));
    matrix_trace_fetch(2, values);
    matrix_trace_row("value_43_1", matrix_trace_equals(values, col43b, 12));

    glProgramUniformMatrix4fv(program, location44, 1, GL_TRUE, row44);
    matrix_trace_row("transpose_44", 0);
    matrix_trace_fetch(3, values);
    matrix_trace_row("value_44", matrix_trace_equals(values, col44, 16));
    glProgramUniformMatrix4fv(program, location44, 1, GL_FALSE, row44);
    matrix_trace_row("notranspose_44", 0);
    matrix_trace_fetch(3, values);
    matrix_trace_row("value_no_transpose", matrix_trace_equals(values, row44, 16));

    glProgramUniformMatrix2x3fv(program, location23, -1, GL_TRUE, row23);
    matrix_trace_row("negative_count", 0);
    glBegin(GL_POINTS);
    glProgramUniformMatrix2x3fv(program, location23, -1, GL_TRUE, row23);
    glEnd();
    matrix_trace_row("begin_bad_count_precedence", 0);
    glBegin(GL_POINTS);
    glProgramUniformMatrix4x3fv(program, location43, 2, GL_TRUE, row43);
    glProgramUniform1f(program, -1, 25.0f);
    glProgramUniform2iv(program, -1, 0, NULL);
    glEnd();
    matrix_trace_row("begin_mutation_prevented", 0);
    matrix_trace_fetch(1, values);
    matrix_trace_row("value_43_preserved", matrix_trace_equals(values, col43a, 12));
    glProgramUniformMatrix2x3fv(program, -1, 1, GL_TRUE, row23);
    matrix_trace_row("negative_location_ignored", 0);
    matrix_trace_finish(program);
    matrix_trace_row("cleanup", 0);
    return matrix_trace_failures == 0u;
}

#endif

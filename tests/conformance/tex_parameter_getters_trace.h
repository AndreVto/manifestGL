#ifndef MGL_TEX_PARAMETER_GETTERS_TRACE_H
#define MGL_TEX_PARAMETER_GETTERS_TRACE_H

#include "mgl/gl.h"
#include <stdio.h>

static void getter_iv(const char *step, GLenum target, GLenum pname) {
    GLint value = -31415;
    GLenum error;
    glGetTexParameteriv(target, pname, &value);
    error = glGetError();
    printf("getter_%s %d 0x%04x\n", step, value, (unsigned)error);
}

static void getter_fv(const char *step, GLenum target, GLenum pname) {
    GLfloat value = -31415.0f;
    GLenum error;
    glGetTexParameterfv(target, pname, &value);
    error = glGetError();
    printf("getter_%s %.3f 0x%04x\n", step, value, (unsigned)error);
}

static void getter_iiv(const char *step, GLenum target, GLenum pname) {
    GLint value[4] = {-31415, -31415, -31415, -31415};
    GLenum error;
    glGetTexParameterIiv(target, pname, value);
    error = glGetError();
    printf("getter_%s %d %d %d %d 0x%04x\n", step, value[0], value[1], value[2], value[3],
           (unsigned)error);
}

static void getter_iuiv(const char *step, GLenum target, GLenum pname) {
    GLuint value[4] = {31415u, 31415u, 31415u, 31415u};
    GLenum error;
    glGetTexParameterIuiv(target, pname, value);
    error = glGetError();
    printf("getter_%s %u %u %u %u 0x%04x\n", step, value[0], value[1], value[2], value[3],
           (unsigned)error);
}

static void tex_parameter_getters_trace(void) {
    const GLint border[4] = {31, 37, 41, 43};
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 8111u);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 2.75f);
    getter_iv("fraction_positive", GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD);
    getter_fv("fraction_float", GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, -2.75f);
    getter_iv("fraction_negative", GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 2.5f);
    getter_iv("positive_half_even", GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, -2.5f);
    getter_iv("negative_half_even", GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 3.5f);
    getter_iv("positive_half_odd", GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, -3.5f);
    getter_iv("negative_half_odd", GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 1.625f);
    getter_iv("max_fraction", GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD);
    glTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    getter_iiv("integer_border", GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR);
    getter_iuiv("unsigned_border", GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR);
    getter_iv("invalid_target", 0u, GL_TEXTURE_WRAP_S);
    getter_fv("invalid_pname", GL_TEXTURE_2D, 0u);

    /* Use a dedicated value for the Begin/End cases so the trace observes
     * both the error and whether the output pointer was written. */
    {
        GLint value = -31415;
        glBegin(GL_POINTS);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &value);
        glEnd();
        printf("getter_begin_iv %d 0x%04x\n", value, (unsigned)glGetError());
    }
    {
        GLfloat value = -31415.0f;
        glBegin(GL_POINTS);
        glGetTexParameterfv(0u, 0u, &value);
        glEnd();
        printf("getter_begin_fv_invalid_target %.3f 0x%04x\n", value, (unsigned)glGetError());
    }
    {
        GLint value[4] = {-31415, -31415, -31415, -31415};
        glBegin(GL_POINTS);
        glGetTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, value);
        glEnd();
        printf("getter_begin_iiv %d %d %d %d 0x%04x\n", value[0], value[1], value[2], value[3],
               (unsigned)glGetError());
    }
    {
        GLuint value[4] = {31415u, 31415u, 31415u, 31415u};
        glBegin(GL_POINTS);
        glGetTexParameterIuiv(0u, GL_TEXTURE_BORDER_COLOR, value);
        glEnd();
        printf("getter_begin_iuiv_invalid_target %u %u %u %u 0x%04x\n", value[0], value[1],
               value[2], value[3], (unsigned)glGetError());
    }
    getter_iv("after_begin", GL_TEXTURE_2D, GL_TEXTURE_WRAP_S);
}
#endif

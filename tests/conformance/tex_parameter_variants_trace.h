#ifndef MGL_TEX_PARAMETER_VARIANTS_TRACE_H
#define MGL_TEX_PARAMETER_VARIANTS_TRACE_H

#include "mgl/gl.h"
#include <limits.h>
#include <stdio.h>

static void parameter_variants_emit(const char *step) {
    GLfloat lod = -999.0f;
    GLfloat border[4] = {-999.0f, -999.0f, -999.0f, -999.0f};
    GLint wrap = -1;
    GLenum error;
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, &lod);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap);
    error = glGetError();
    printf("variant_%s %.3f %.3f %.3f %.3f %.3f %d 0x%04x\n", step, lod, border[0], border[1],
           border[2], border[3], wrap, (unsigned)error);
}

static void tex_parameter_variants_trace(void) {
    const GLfloat first_border[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    const GLfloat second_border[4] = {0.125f, 0.25f, 0.375f, 0.5f};
    const GLint integer_border[4] = {INT_MAX, -INT_MAX, 0, INT_MAX};
    const GLint zero_border[4] = {0, 0, 0, 0};
    const GLint wrap_repeat = GL_REPEAT;
    const GLfloat invalid_border[4] = {0.75f, 0.75f, 0.75f, 0.75f};
    GLuint list;

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 8094u);
    parameter_variants_emit("initial");
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 2.5f);
    parameter_variants_emit("float_lod");
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, first_border);
    parameter_variants_emit("float_border");
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, integer_border);
    parameter_variants_emit("integer_border");
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, &first_border[0]);
    parameter_variants_emit("vector_scalar");
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap_repeat);
    parameter_variants_emit("integer_scalar");

    glBegin(GL_POINTS);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 7.5f);
    glEnd();
    parameter_variants_emit("begin_float");
    glBegin(GL_POINTS);
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, zero_border);
    glEnd();
    parameter_variants_emit("begin_integer_vector");
    glBegin(GL_POINTS);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, invalid_border);
    glEnd();
    parameter_variants_emit("begin_float_vector");
    glBegin(GL_POINTS);
    glTexParameterf(0u, GL_TEXTURE_MAX_LOD, 3.0f);
    glEnd();
    parameter_variants_emit("begin_invalid_target");

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 4.5f);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, second_border);
    glEndList();
    parameter_variants_emit("compile_deferred");
    glCallList(list);
    parameter_variants_emit("compile_replay");
    glDeleteLists(list, 1);

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBegin(GL_POINTS);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 8.5f);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, invalid_border);
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, zero_border);
    glEnd();
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 5.5f);
    glEndList();
    parameter_variants_emit("compile_illegal_deferred");
    glCallList(list);
    parameter_variants_emit("compile_illegal_replay");
    glDeleteLists(list, 1);

    list = glGenLists(1);
    glNewList(list, GL_COMPILE_AND_EXECUTE);
    glBegin(GL_POINTS);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, invalid_border);
    glEnd();
    glEndList();
    parameter_variants_emit("compile_execute_illegal");
    glCallList(list);
    parameter_variants_emit("compile_execute_replay");
    glDeleteLists(list, 1);
}
#endif

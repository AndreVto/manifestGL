#ifndef MGL_TEX_PARAMETER_INTEGER_TRACE_H
#define MGL_TEX_PARAMETER_INTEGER_TRACE_H

#include "mgl/gl.h"
#include <stdio.h>

static void integer_parameter_emit(const char *step) {
    GLint signed_border[4] = {-99, -99, -99, -99};
    GLuint unsigned_border[4] = {99u, 99u, 99u, 99u};
    GLenum error;
    glGetTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, signed_border);
    glGetTexParameterIuiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, unsigned_border);
    error = glGetError();
    printf("integer_%s %d %d %d %d %u %u %u %u 0x%04x\n", step, signed_border[0], signed_border[1],
           signed_border[2], signed_border[3], unsigned_border[0], unsigned_border[1],
           unsigned_border[2], unsigned_border[3], (unsigned)error);
}

static void tex_parameter_integer_trace(void) {
    const GLint signed_first[4] = {3, 5, 7, 9};
    const GLint signed_second[4] = {11, 13, 17, 19};
    const GLuint unsigned_first[4] = {2u, 4u, 6u, 8u};
    GLuint unsigned_second[4] = {10u, 12u, 14u, 16u};
    GLint signed_list[4] = {11, 13, 17, 19};
    const GLint signed_illegal[4] = {21, 23, 25, 27};
    const GLuint unsigned_illegal[4] = {22u, 24u, 26u, 28u};
    GLuint list;

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 8095u);
    glTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, signed_first);
    integer_parameter_emit("signed_set");
    glTexParameterIuiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, unsigned_first);
    integer_parameter_emit("unsigned_set");
    glTexParameterIiv(0u, GL_TEXTURE_BORDER_COLOR, signed_illegal);
    integer_parameter_emit("invalid_target");

    glBegin(GL_POINTS);
    glTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, signed_illegal);
    glEnd();
    integer_parameter_emit("begin_signed");
    glBegin(GL_POINTS);
    glTexParameterIuiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, unsigned_illegal);
    glEnd();
    integer_parameter_emit("begin_unsigned");
    glBegin(GL_POINTS);
    glTexParameterIuiv(0u, GL_TEXTURE_BORDER_COLOR, unsigned_illegal);
    glEnd();
    integer_parameter_emit("begin_invalid_target");

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, signed_list);
    glTexParameterIuiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, unsigned_second);
    glEndList();
    signed_list[0] = signed_list[1] = signed_list[2] = signed_list[3] = 99;
    unsigned_second[0] = unsigned_second[1] = unsigned_second[2] = unsigned_second[3] = 99u;
    integer_parameter_emit("compile_deferred");
    glCallList(list);
    integer_parameter_emit("compile_replayed");
    glDeleteLists(list, 1);

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBegin(GL_POINTS);
    glTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, signed_illegal);
    glTexParameterIuiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, unsigned_illegal);
    glEnd();
    glTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, signed_second);
    glEndList();
    integer_parameter_emit("compile_illegal_deferred");
    glCallList(list);
    integer_parameter_emit("compile_illegal_replayed");
    glDeleteLists(list, 1);

    list = glGenLists(1);
    glNewList(list, GL_COMPILE_AND_EXECUTE);
    glBegin(GL_POINTS);
    glTexParameterIuiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, unsigned_illegal);
    glEnd();
    glEndList();
    integer_parameter_emit("compile_execute_illegal");
    glCallList(list);
    integer_parameter_emit("compile_execute_replayed");
    glDeleteLists(list, 1);
}
#endif

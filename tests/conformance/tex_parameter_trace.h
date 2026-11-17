#ifndef MGL_TEX_PARAMETER_TRACE_H
#define MGL_TEX_PARAMETER_TRACE_H

#include "mgl/gl.h"
#include <stdio.h>

static void tex_parameter_emit(const char *step) {
    GLint wrap = -1;
    GLenum error;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap);
    error = glGetError();
    printf("param_%s %d 0x%04x\n", step, wrap, (unsigned)error);
}

static void tex_parameter_trace(void) {
    GLuint list;
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 8080u);
    tex_parameter_emit("initial");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    tex_parameter_emit("valid");
    glTexParameteri(0u, GL_TEXTURE_WRAP_S, GL_REPEAT);
    tex_parameter_emit("invalid_target");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0);
    tex_parameter_emit("invalid_value");
    glTexParameteri(GL_TEXTURE_2D, 0u, GL_REPEAT);
    tex_parameter_emit("invalid_pname");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, -1);
    tex_parameter_emit("negative_base_level");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, 0x7fffffff);
    tex_parameter_emit("invalid_compare_func");
    glBegin(GL_POINTS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glEnd();
    tex_parameter_emit("begin_valid");
    glBegin(GL_POINTS);
    glTexParameteri(0u, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glEnd();
    tex_parameter_emit("begin_bad_target");
    glBegin(GL_POINTS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0);
    glEnd();
    tex_parameter_emit("begin_bad_value");

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glEndList();
    tex_parameter_emit("compile_deferred");
    glCallList(list);
    tex_parameter_emit("compile_replayed");
    glDeleteLists(list, 1);

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBegin(GL_POINTS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glEnd();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glEndList();
    tex_parameter_emit("compile_illegal_deferred");
    glCallList(list);
    tex_parameter_emit("compile_illegal_replayed");
    glDeleteLists(list, 1);

    list = glGenLists(1);
    glNewList(list, GL_COMPILE_AND_EXECUTE);
    glBegin(GL_POINTS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glEnd();
    glEndList();
    tex_parameter_emit("compile_execute_illegal");
    glCallList(list);
    tex_parameter_emit("compile_execute_replayed");
    glDeleteLists(list, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 8081u);
    tex_parameter_emit("unit2_isolated");
    glActiveTexture(GL_TEXTURE1);
    tex_parameter_emit("unit1_unchanged");
}
#endif

#ifndef MGL_ACTIVE_TEXTURE_TRACE_H
#define MGL_ACTIVE_TEXTURE_TRACE_H

#include "mgl/gl.h"
#include <stdio.h>

static void active_texture_emit(const char *step) {
    GLint selected = -1;
    GLenum error;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &selected);
    error = glGetError();
    printf("%s 0x%04x 0x%04x\n", step, (unsigned)selected, (unsigned)error);
}

static void active_texture_trace(void) {
    GLuint list;

    active_texture_emit("initial");
    glActiveTexture(GL_TEXTURE1);
    active_texture_emit("select1");
    glActiveTexture(0u);
    active_texture_emit("invalid_enum_low");
    glActiveTexture(GL_TEXTURE0 + 4096u);
    active_texture_emit("invalid_enum_high");

    glBegin(GL_POINTS);
    glActiveTexture(GL_TEXTURE2);
    glEnd();
    active_texture_emit("begin_valid_enum");
    glBegin(GL_POINTS);
    glActiveTexture(0u);
    glEnd();
    active_texture_emit("begin_invalid_enum");

    glActiveTexture(GL_TEXTURE3);
    active_texture_emit("select3");
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glActiveTexture(GL_TEXTURE1);
    glEndList();
    active_texture_emit("compile_deferred");
    glCallList(list);
    active_texture_emit("compile_replayed");
    glDeleteLists(list, 1);

    list = glGenLists(1);
    glNewList(list, GL_COMPILE_AND_EXECUTE);
    glActiveTexture(GL_TEXTURE2);
    glEndList();
    active_texture_emit("compile_and_execute");
    glActiveTexture(GL_TEXTURE0);
    glCallList(list);
    active_texture_emit("compile_and_execute_replayed");
    glDeleteLists(list, 1);

    glActiveTexture(GL_TEXTURE0);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBegin(GL_POINTS);
    glActiveTexture(GL_TEXTURE3);
    glEnd();
    glEndList();
    active_texture_emit("compile_illegal_begin");
    glCallList(list);
    active_texture_emit("compile_illegal_replayed");
    glDeleteLists(list, 1);

    glActiveTexture(GL_TEXTURE0);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE_AND_EXECUTE);
    glBegin(GL_POINTS);
    glActiveTexture(GL_TEXTURE3);
    glEnd();
    glEndList();
    active_texture_emit("compile_execute_illegal_begin");
    glCallList(list);
    active_texture_emit("compile_execute_illegal_replayed");
    glDeleteLists(list, 1);

    glActiveTexture(GL_TEXTURE0);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBegin(GL_POINTS);
    glActiveTexture(0u);
    glEnd();
    glEndList();
    active_texture_emit("compile_invalid_enum_begin");
    glCallList(list);
    active_texture_emit("compile_invalid_enum_replayed");
    glDeleteLists(list, 1);

    /* Errors must not swallow later legal commands in the same list. */
    glActiveTexture(GL_TEXTURE0);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBegin(GL_POINTS);
    glActiveTexture(GL_TEXTURE3);
    glEnd();
    glActiveTexture(GL_TEXTURE1);
    glEndList();
    active_texture_emit("continued_list_deferred");
    glCallList(list);
    active_texture_emit("continued_list_replayed");
    glDeleteLists(list, 1);
}

#endif

#ifndef MGL_BIND_TEXTURE_TRACE_H
#define MGL_BIND_TEXTURE_TRACE_H

#include "mgl/gl.h"
#include <stdio.h>

static void bind_texture_emit(const char *step) {
    GLint active = -1;
    GLint bound = -1;
    GLenum error;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    error = glGetError();
    printf("bind_%s 0x%04x %d 0x%04x\n", step, (unsigned)active, bound, (unsigned)error);
}

static void bind_texture_trace(void) {
    GLuint list;
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 101u);
    bind_texture_emit("unit1");
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 202u);
    bind_texture_emit("unit2");
    glActiveTexture(GL_TEXTURE1);
    bind_texture_emit("isolated_unit1");

    glBindTexture(0u, 303u);
    bind_texture_emit("invalid_target");
    glBegin(GL_POINTS);
    glBindTexture(GL_TEXTURE_2D, 303u);
    glEnd();
    bind_texture_emit("begin_valid_target");
    glBegin(GL_POINTS);
    glBindTexture(0u, 303u);
    glEnd();
    bind_texture_emit("begin_invalid_target");

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBindTexture(GL_TEXTURE_2D, 404u);
    glEndList();
    bind_texture_emit("compile_deferred");
    glCallList(list);
    bind_texture_emit("compile_replayed");
    glDeleteLists(list, 1);

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBegin(GL_POINTS);
    glBindTexture(GL_TEXTURE_2D, 505u);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 606u);
    glEndList();
    bind_texture_emit("compile_illegal_deferred");
    glCallList(list);
    bind_texture_emit("compile_illegal_replayed");
    glDeleteLists(list, 1);

    list = glGenLists(1);
    glNewList(list, GL_COMPILE_AND_EXECUTE);
    glBegin(GL_POINTS);
    glBindTexture(GL_TEXTURE_2D, 707u);
    glEnd();
    glEndList();
    bind_texture_emit("compile_execute_illegal");
    glCallList(list);
    bind_texture_emit("compile_execute_replayed");
    glDeleteLists(list, 1);

    glActiveTexture(GL_TEXTURE2);
    bind_texture_emit("unit2_unchanged");
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0u);
    bind_texture_emit("unbind_unit1");
    glActiveTexture(GL_TEXTURE2);
    bind_texture_emit("unit2_after_unbind");
}

#endif

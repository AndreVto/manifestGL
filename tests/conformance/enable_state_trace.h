#ifndef MGL_ENABLE_STATE_TRACE_H
#define MGL_ENABLE_STATE_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void enable_trace_emit(const char *step) {
    GLenum error = glGetError();
    GLboolean blend = glIsEnabled(GL_BLEND);
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean blend1 = glIsEnabledi(GL_BLEND, 1u);
    GLenum query_error = glGetError();
    printf("enable_%s %u %u %u 0x%04x 0x%04x\n", step, (unsigned)blend, (unsigned)scissor,
           (unsigned)blend1, (unsigned)error, (unsigned)query_error);
}

static void enable_state_trace(void) {
    GLuint list;
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisablei(GL_BLEND, 1u);
    enable_trace_emit("initial");
    glEnable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);
    glEnablei(GL_BLEND, 1u);
    enable_trace_emit("enabled");
    glDisablei(GL_BLEND, 1u);
    glDisable(GL_SCISSOR_TEST);
    enable_trace_emit("disabled_selected");

    glBegin(GL_POINTS);
    glDisable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);
    glEnablei(GL_BLEND, 1u);
    glEnd();
    enable_trace_emit("illegal_changes");

    glBegin(GL_POINTS);
    glEnable(0u);
    glEnd();
    enable_trace_emit("illegal_cap_precedence");
    glBegin(GL_POINTS);
    glEnablei(0u, 500u);
    glEnd();
    enable_trace_emit("illegal_index_precedence");

    glBegin(GL_POINTS);
    (void)glIsEnabled(GL_BLEND);
    glEnd();
    enable_trace_emit("illegal_is_enabled");
    glBegin(GL_POINTS);
    (void)glIsEnabledi(GL_BLEND, 1u);
    glEnd();
    enable_trace_emit("illegal_is_enabled_i");
    glBegin(GL_POINTS);
    (void)glIsEnabledi(0u, 500u);
    glEnd();
    enable_trace_emit("illegal_is_enabled_invalid");

    glDisable(GL_BLEND);
    glDisablei(GL_BLEND, 1u);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glEnable(GL_BLEND);
    glEnablei(GL_BLEND, 1u);
    glDisable(GL_SCISSOR_TEST);
    glEndList();
    enable_trace_emit("compiled_deferred");
    glCallList(list);
    enable_trace_emit("compiled_replay");

    glDisable(GL_BLEND);
    glDisablei(GL_BLEND, 1u);
    glNewList(list, GL_COMPILE);
    glBegin(GL_POINTS);
    glEnable(GL_BLEND);
    glEnablei(GL_BLEND, 1u);
    glEnd();
    glEndList();
    enable_trace_emit("compiled_illegal_deferred");
    glCallList(list);
    enable_trace_emit("compiled_illegal_replay");

    glNewList(list, GL_COMPILE_AND_EXECUTE);
    glEnable(GL_BLEND);
    glEnablei(GL_BLEND, 1u);
    glEndList();
    enable_trace_emit("compile_execute");
    glDeleteLists(list, 1);
    glDisableIndexedEXT(GL_BLEND, 1u);
    enable_trace_emit("ext_disabled");
    glEnableIndexedEXT(GL_BLEND, 1u);
    enable_trace_emit("ext_enabled");
    glBegin(GL_POINTS);
    glDisableIndexedEXT(GL_BLEND, 1u);
    glEnd();
    enable_trace_emit("ext_illegal_disable");
    glBegin(GL_POINTS);
    (void)glIsEnabledIndexedEXT(GL_BLEND, 1u);
    glEnd();
    enable_trace_emit("ext_illegal_query");
    glBegin(GL_POINTS);
    glEnableIndexedEXT(0u, 500u);
    glEnd();
    enable_trace_emit("ext_illegal_invalid_cap");
    glDisableIndexedEXT(GL_BLEND, 1u);
    glDisable(GL_BLEND);
    glDisablei(GL_BLEND, 1u);
    glDisable(GL_SCISSOR_TEST);
    enable_trace_emit("cleanup");
}
#endif

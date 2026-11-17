#ifndef MGL_SELECTION_STATE_TRACE_H
#define MGL_SELECTION_STATE_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

/* Name-stack selection commands must not alter mode, stack, or user buffer
 * during a primitive. Observe only specified GL errors and legal state. */
static void selection_state_trace(void) {
    GLuint hits[32] = {0};
    GLint mode = 0, depth = 0;
    glSelectBuffer(32, hits);
    printf("sel_buffer_setup 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glSelectBuffer(-1, hits);
    glEnd();
    printf("sel_buffer_invalid_inside 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glRenderMode(0xDEAD);
    glEnd();
    printf("sel_mode_invalid_inside 0x%04x\n", (unsigned)glGetError());
    glGetIntegerv(GL_RENDER_MODE, &mode);
    printf("sel_mode_preserved %d 0x%04x\n", mode == (GLint)GL_RENDER, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glInitNames();
    glEnd();
    printf("sel_init_inside 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glPushName(4);
    glEnd();
    printf("sel_push_inside 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glPopName();
    glEnd();
    printf("sel_pop_inside 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glLoadName(8);
    glEnd();
    printf("sel_load_inside 0x%04x\n", (unsigned)glGetError());
    glRenderMode(GL_SELECT);
    printf("sel_mode_enter 0x%04x\n", (unsigned)glGetError());
    glInitNames();
    glPushName(10);
    glPushName(20);
    glGetIntegerv(GL_NAME_STACK_DEPTH, &depth);
    printf("sel_stack_setup %d 0x%04x\n", depth, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glPopName();
    glLoadName(12);
    glPushName(30);
    glEnd();
    printf("sel_stack_inside 0x%04x\n", (unsigned)glGetError());
    glGetIntegerv(GL_NAME_STACK_DEPTH, &depth);
    printf("sel_stack_preserved %d 0x%04x\n", depth, (unsigned)glGetError());
    glLoadName(21);
    glPopName();
    glGetIntegerv(GL_NAME_STACK_DEPTH, &depth);
    printf("sel_stack_recovery %d 0x%04x\n", depth, (unsigned)glGetError());
    glRenderMode(GL_RENDER);
    printf("sel_mode_exit 0x%04x\n", (unsigned)glGetError());
}
#endif

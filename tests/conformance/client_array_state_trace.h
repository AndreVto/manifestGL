#ifndef MGL_CLIENT_ARRAY_STATE_TRACE_H
#define MGL_CLIENT_ARRAY_STATE_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>
#include <stdint.h>

static void client_array_trace_emit(const char *step) {
    printf("client_%s 0x%04x\n", step, (unsigned)glGetError());
}

static void client_array_state_trace(void) {
    glVertexPointer(3, GL_FLOAT, 12, (const void *)(uintptr_t)0x1000);
    client_array_trace_emit("vertex_valid");
    glNormalPointer(GL_FLOAT, 12, (const void *)(uintptr_t)0x1100);
    client_array_trace_emit("normal_valid");
    glColorPointer(4, GL_FLOAT, 16, (const void *)(uintptr_t)0x1200);
    client_array_trace_emit("color_valid");
    glSecondaryColorPointer(3, GL_FLOAT, 12, (const void *)(uintptr_t)0x1300);
    client_array_trace_emit("secondary_valid");
    glFogCoordPointer(GL_FLOAT, 4, (const void *)(uintptr_t)0x1400);
    client_array_trace_emit("fog_valid");
    glTexCoordPointer(2, GL_FLOAT, 8, (const void *)(uintptr_t)0x1500);
    client_array_trace_emit("texture_valid");
    glEnableClientState(GL_VERTEX_ARRAY);
    client_array_trace_emit("enable_valid");
    glDisableClientState(GL_VERTEX_ARRAY);
    client_array_trace_emit("disable_valid");
    glInterleavedArrays(GL_T2F_V3F, 0, (const void *)(uintptr_t)0x1600);
    client_array_trace_emit("interleaved_valid");

    glBegin(GL_POINTS);
    glVertexPointer(3, GL_FLOAT, 12, (const void *)(uintptr_t)0x1700);
    glEnd();
    client_array_trace_emit("begin_vertex");
    glBegin(GL_POINTS);
    glNormalPointer(GL_FLOAT, 12, (const void *)(uintptr_t)0x1700);
    glEnd();
    client_array_trace_emit("begin_normal");
    glBegin(GL_POINTS);
    glColorPointer(4, GL_FLOAT, 16, (const void *)(uintptr_t)0x1700);
    glEnd();
    client_array_trace_emit("begin_color");
    glBegin(GL_POINTS);
    glSecondaryColorPointer(3, GL_FLOAT, 12, (const void *)(uintptr_t)0x1700);
    glEnd();
    client_array_trace_emit("begin_secondary");
    glBegin(GL_POINTS);
    glFogCoordPointer(GL_FLOAT, 4, (const void *)(uintptr_t)0x1700);
    glEnd();
    client_array_trace_emit("begin_fog");
    glBegin(GL_POINTS);
    glTexCoordPointer(2, GL_FLOAT, 8, (const void *)(uintptr_t)0x1700);
    glEnd();
    client_array_trace_emit("begin_texture");
    glBegin(GL_POINTS);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnd();
    client_array_trace_emit("begin_enable");
    glBegin(GL_POINTS);
    glDisableClientState(GL_VERTEX_ARRAY);
    glEnd();
    client_array_trace_emit("begin_disable");
    glBegin(GL_POINTS);
    glInterleavedArrays(GL_T2F_V3F, 0, (const void *)(uintptr_t)0x1700);
    glEnd();
    client_array_trace_emit("begin_interleaved");
    glBegin(GL_POINTS);
    glVertexPointer(9, 0u, -1, NULL);
    glEnd();
    client_array_trace_emit("begin_bad_vertex_precedence");
    glBegin(GL_POINTS);
    glInterleavedArrays(0u, -1, NULL);
    glEnd();
    client_array_trace_emit("begin_bad_interleaved_precedence");
    glVertexPointer(3, 0u, 0, NULL);
    client_array_trace_emit("invalid_vertex_type");
    glNormalPointer(0u, 0, NULL);
    client_array_trace_emit("invalid_normal_type");
    glColorPointer(4, 0u, 0, NULL);
    client_array_trace_emit("invalid_color_type");
    glTexCoordPointer(2, 0u, 0, NULL);
    client_array_trace_emit("invalid_texture_type");
    glInterleavedArrays(0u, 0, NULL);
    client_array_trace_emit("invalid_interleaved_format");
}
#endif

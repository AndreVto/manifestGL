#ifndef MGL_LEGACY_STATE_TRACE_H
#define MGL_LEGACY_STATE_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void legacy_state_trace(void) {
    GLfloat width = -1.0f;
    GLint alignment = -1;
    GLfloat map_values[2] = {0.20f, 0.80f};
    GLfloat map_out[2] = {-7.0f, -7.0f};
    GLuint map_u32[2] = {0u, ~0u};
    GLushort map_u16[2] = {0u, 65535u};
    GLuint out_u32[2] = {17u, 19u};
    GLushort out_u16[2] = {17u, 19u};
    GLfloat scale = -1.0f;

    glLineWidth(2.0f);
    glPushAttrib(GL_LINE_BIT);
    printf("legacy_push_valid 0x%04x\n", (unsigned)glGetError());
    glLineWidth(3.0f);
    glBegin(GL_POINTS);
    glPopAttrib();
    glEnd();
    glGetFloatv(GL_LINE_WIDTH, &width);
    printf("legacy_pop_begin %.2f 0x%04x\n", width, (unsigned)glGetError());
    glPopAttrib();
    glGetFloatv(GL_LINE_WIDTH, &width);
    printf("legacy_pop_after %.2f 0x%04x\n", width, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glPushAttrib(0x80000000u);
    glEnd();
    printf("legacy_push_begin_invalid 0x%04x\n", (unsigned)glGetError());
    glPopAttrib();
    printf("legacy_push_did_not_grow 0x%04x\n", (unsigned)glGetError());

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    printf("legacy_client_push_valid 0x%04x\n", (unsigned)glGetError());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBegin(GL_POINTS);
    glPopClientAttrib();
    glEnd();
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    printf("legacy_client_pop_begin %d 0x%04x\n", alignment, (unsigned)glGetError());
    glPopClientAttrib();
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    printf("legacy_client_pop_after %d 0x%04x\n", alignment, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glEnd();
    printf("legacy_client_push_begin 0x%04x\n", (unsigned)glGetError());
    glPopClientAttrib();
    printf("legacy_client_push_did_not_grow 0x%04x\n", (unsigned)glGetError());

    glPixelMapfv(GL_PIXEL_MAP_R_TO_R, 2, map_values);
    glGetPixelMapfv(GL_PIXEL_MAP_R_TO_R, map_out);
    printf("legacy_map_float_set %.3f %.3f 0x%04x\n", map_out[0], map_out[1],
           (unsigned)glGetError());
    glBegin(GL_POINTS);
    glPixelMapfv(GL_PIXEL_MAP_R_TO_R, 0, map_values);
    glEnd();
    glGetPixelMapfv(GL_PIXEL_MAP_R_TO_R, map_out);
    printf("legacy_map_float_begin %.3f %.3f 0x%04x\n", map_out[0], map_out[1],
           (unsigned)glGetError());
    map_out[0] = -7.0f;
    glBegin(GL_POINTS);
    glGetPixelMapfv(GL_PIXEL_MAP_R_TO_R, map_out);
    glEnd();
    printf("legacy_map_get_begin %.3f 0x%04x\n", map_out[0], (unsigned)glGetError());
    glPixelMapuiv(GL_PIXEL_MAP_R_TO_R, 2, map_u32);
    glGetPixelMapfv(GL_PIXEL_MAP_R_TO_R, map_out);
    printf("legacy_map_uint_set %.3f %.3f 0x%04x\n", map_out[0], map_out[1],
           (unsigned)glGetError());
    glBegin(GL_POINTS);
    glPixelMapuiv(0xDEADu, 2, map_u32);
    glEnd();
    printf("legacy_map_uint_begin_bad 0x%04x\n", (unsigned)glGetError());
    glPixelMapusv(GL_PIXEL_MAP_R_TO_R, 2, map_u16);
    glGetPixelMapfv(GL_PIXEL_MAP_R_TO_R, map_out);
    printf("legacy_map_ushort_set %.3f %.3f 0x%04x\n", map_out[0], map_out[1],
           (unsigned)glGetError());
    glBegin(GL_POINTS);
    glPixelMapusv(GL_PIXEL_MAP_R_TO_R, 0, map_u16);
    glEnd();
    printf("legacy_map_ushort_begin_bad 0x%04x\n", (unsigned)glGetError());
    out_u32[0] = 17u;
    glBegin(GL_POINTS);
    glGetPixelMapuiv(GL_PIXEL_MAP_R_TO_R, out_u32);
    glEnd();
    printf("legacy_map_get_uint_begin %u 0x%04x\n", out_u32[0], (unsigned)glGetError());
    out_u16[0] = 17u;
    glBegin(GL_POINTS);
    glGetPixelMapusv(GL_PIXEL_MAP_R_TO_R, out_u16);
    glEnd();
    printf("legacy_map_get_ushort_begin %u 0x%04x\n", (unsigned)out_u16[0], (unsigned)glGetError());

    glPixelTransferf(GL_RED_SCALE, 1.5f);
    glGetFloatv(GL_RED_SCALE, &scale);
    printf("legacy_transfer_float_set %.2f 0x%04x\n", scale, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glPixelTransferf(0xDEADu, 0.5f);
    glEnd();
    glGetFloatv(GL_RED_SCALE, &scale);
    printf("legacy_transfer_float_begin %.2f 0x%04x\n", scale, (unsigned)glGetError());
    glPixelTransferi(GL_RED_SCALE, 2);
    glGetFloatv(GL_RED_SCALE, &scale);
    printf("legacy_transfer_integer_set %.2f 0x%04x\n", scale, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glPixelTransferi(GL_RED_SCALE, 3);
    glEnd();
    glGetFloatv(GL_RED_SCALE, &scale);
    printf("legacy_transfer_integer_begin %.2f 0x%04x\n", scale, (unsigned)glGetError());
    glNewList(9099u, GL_COMPILE);
    glBegin(GL_POINTS);
    glPixelTransferf(GL_RED_SCALE, 9.0f);
    glEnd();
    glEndList();
    printf("legacy_list_transfer_compile 0x%04x\n", (unsigned)glGetError());
    glCallList(9099u);
    printf("legacy_list_transfer_replay 0x%04x\n", (unsigned)glGetError());
    glGetFloatv(GL_RED_SCALE, &scale);
    printf("legacy_list_transfer_preserved %.2f 0x%04x\n", scale, (unsigned)glGetError());
    glDeleteLists(9099u, 1);

    glNewList(9100u, GL_COMPILE);
    glBegin(GL_POINTS);
    glPushAttrib(GL_LINE_BIT);
    glEnd();
    glEndList();
    printf("legacy_list_push_compile 0x%04x\n", (unsigned)glGetError());
    glCallList(9100u);
    printf("legacy_list_push_replay 0x%04x\n", (unsigned)glGetError());
    glPopAttrib();
    printf("legacy_list_push_stack_unchanged 0x%04x\n", (unsigned)glGetError());
    glDeleteLists(9100u, 1);

    glNewList(9101u, GL_COMPILE);
    glBegin(GL_POINTS);
    glPixelMapfv(GL_PIXEL_MAP_R_TO_R, 2, map_values);
    glEnd();
    glEndList();
    printf("legacy_list_map_compile 0x%04x\n", (unsigned)glGetError());
    glCallList(9101u);
    printf("legacy_list_map_replay 0x%04x\n", (unsigned)glGetError());
    glGetPixelMapfv(GL_PIXEL_MAP_R_TO_R, map_out);
    printf("legacy_list_map_preserved %.3f %.3f 0x%04x\n", map_out[0], map_out[1],
           (unsigned)glGetError());
    glDeleteLists(9101u, 1);
}
#endif

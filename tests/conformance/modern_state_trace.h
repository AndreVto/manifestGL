#ifndef MGL_MODERN_STATE_TRACE_H
#define MGL_MODERN_STATE_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void modern_state_trace(void) {
    GLint value = -1;
    glPrimitiveRestartIndex(17u);
    glGetIntegerv(GL_PRIMITIVE_RESTART_INDEX, &value);
    printf("modern_restart_set %d 0x%04x\n", value, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glPrimitiveRestartIndex(29u);
    glEnd();
    glGetIntegerv(GL_PRIMITIVE_RESTART_INDEX, &value);
    printf("modern_restart_begin %d 0x%04x\n", value, (unsigned)glGetError());
    glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
    glGetIntegerv(GL_PROVOKING_VERTEX, &value);
    printf("modern_provoking_set %d 0x%04x\n", value, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glProvokingVertex(GL_LAST_VERTEX_CONVENTION);
    glEnd();
    glGetIntegerv(GL_PROVOKING_VERTEX, &value);
    printf("modern_provoking_begin %d 0x%04x\n", value, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glProvokingVertex(0u);
    glEnd();
    glGetIntegerv(GL_PROVOKING_VERTEX, &value);
    printf("modern_provoking_begin_invalid %d 0x%04x\n", value, (unsigned)glGetError());
    glProvokingVertex(0u);
    printf("modern_provoking_invalid 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glBindImageTexture(0u, 0u, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    glEnd();
    printf("modern_image_begin 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glBindImageTexture(0u, 0u, -1, GL_FALSE, 0, 0u, GL_RGBA8);
    glEnd();
    printf("modern_image_begin_invalid 0x%04x\n", (unsigned)glGetError());
    glBindImageTexture(0u, 0u, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    printf("modern_image_valid 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    glEnd();
    printf("modern_barrier_begin 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glMemoryBarrier(0x80000000u);
    glEnd();
    printf("modern_barrier_begin_invalid 0x%04x\n", (unsigned)glGetError());
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    printf("modern_barrier_valid 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glMemoryBarrierByRegion(GL_TEXTURE_FETCH_BARRIER_BIT);
    glEnd();
    printf("modern_region_begin 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glMemoryBarrierByRegion(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    glEnd();
    printf("modern_region_begin_invalid 0x%04x\n", (unsigned)glGetError());
    glMemoryBarrierByRegion(GL_TEXTURE_FETCH_BARRIER_BIT);
    printf("modern_region_valid 0x%04x\n", (unsigned)glGetError());
    glMemoryBarrierByRegion(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    printf("modern_region_outside_invalid 0x%04x\n", (unsigned)glGetError());
}
#endif

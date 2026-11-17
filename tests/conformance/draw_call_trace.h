#ifndef MGL_DRAW_CALL_TRACE_H
#define MGL_DRAW_CALL_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

/* Test error precedence inside glBegin/glEnd, even for otherwise no-op draws. */
#define DRAW_TRACE_CHECK(label, call)                                                              \
    do {                                                                                           \
        GLenum draw_error;                                                                         \
        glBegin(GL_POINTS);                                                                        \
        call;                                                                                      \
        glEnd();                                                                                   \
        draw_error = glGetError();                                                                 \
        printf("draw_%s 0x%04x\n", label, (unsigned)draw_error);                                   \
    } while (0)

static void draw_call_trace_trace(void) {
    const GLint starts[] = {0};
    const GLsizei counts[] = {0};
    const void *indices[] = {NULL};
    const GLint bases[] = {0};
    DRAW_TRACE_CHECK("arrays_zero", glDrawArrays(GL_POINTS, 0, 0));
    DRAW_TRACE_CHECK("arrays_invalid", glDrawArrays(0u, -1, -1));
    DRAW_TRACE_CHECK("elements_zero", glDrawElements(GL_POINTS, 0, GL_UNSIGNED_BYTE, NULL));
    DRAW_TRACE_CHECK("elements_invalid", glDrawElements(0u, -1, 0u, NULL));
    DRAW_TRACE_CHECK("range_zero", glDrawRangeElements(GL_POINTS, 0, 1, 0, GL_UNSIGNED_BYTE, NULL));
    DRAW_TRACE_CHECK("range_invalid", glDrawRangeElements(0u, 2, 1, -1, 0u, NULL));
    DRAW_TRACE_CHECK("multi_arrays", glMultiDrawArrays(GL_POINTS, starts, counts, 1));
    DRAW_TRACE_CHECK("multi_arrays_invalid", glMultiDrawArrays(0u, starts, counts, -1));
    DRAW_TRACE_CHECK("multi_elements",
                     glMultiDrawElements(GL_POINTS, counts, GL_UNSIGNED_BYTE, indices, 1));
    DRAW_TRACE_CHECK("multi_elements_invalid", glMultiDrawElements(0u, counts, 0u, indices, -1));
    DRAW_TRACE_CHECK("base_vertex",
                     glDrawElementsBaseVertex(GL_POINTS, 0, GL_UNSIGNED_BYTE, NULL, 0));
    DRAW_TRACE_CHECK("range_base",
                     glDrawRangeElementsBaseVertex(GL_POINTS, 0, 1, 0, GL_UNSIGNED_BYTE, NULL, 0));
    DRAW_TRACE_CHECK("multi_base", glMultiDrawElementsBaseVertex(
                                       GL_POINTS, counts, GL_UNSIGNED_BYTE, indices, 1, bases));
    DRAW_TRACE_CHECK("arrays_instanced", glDrawArraysInstanced(GL_POINTS, 0, 0, 1));
    DRAW_TRACE_CHECK("arrays_instanced_invalid", glDrawArraysInstanced(0u, -1, -1, -1));
    DRAW_TRACE_CHECK("elements_instanced",
                     glDrawElementsInstanced(GL_POINTS, 0, GL_UNSIGNED_BYTE, NULL, 1));
    DRAW_TRACE_CHECK("elements_instanced_invalid", glDrawElementsInstanced(0u, -1, 0u, NULL, -1));
    DRAW_TRACE_CHECK("base_instanced",
                     glDrawElementsInstancedBaseVertex(GL_POINTS, 0, GL_UNSIGNED_BYTE, NULL, 1, 0));
    DRAW_TRACE_CHECK("arrays_indirect", glDrawArraysIndirect(GL_POINTS, NULL));
    DRAW_TRACE_CHECK("elements_indirect", glDrawElementsIndirect(GL_POINTS, GL_UNSIGNED_INT, NULL));

    glDrawArrays(GL_POINTS, 0, 0);
    printf("draw_recovered 0x%04x\n", (unsigned)glGetError());
    glDrawArrays(0xffffu, 0, 0);
    printf("draw_outside_bad_mode 0x%04x\n", (unsigned)glGetError());
    glDrawElements(GL_POINTS, -1, GL_UNSIGNED_BYTE, NULL);
    printf("draw_outside_bad_count 0x%04x\n", (unsigned)glGetError());
    glDrawRangeElements(GL_POINTS, 3, 2, 0, GL_UNSIGNED_BYTE, NULL);
    printf("draw_outside_bad_range 0x%04x\n", (unsigned)glGetError());
    glDrawArraysInstanced(GL_POINTS, 0, 0, -1);
    printf("draw_outside_bad_instances 0x%04x\n", (unsigned)glGetError());
    glDrawElements(GL_POINTS, 0, GL_UNSIGNED_BYTE, NULL);
    printf("draw_outside_elements_zero 0x%04x\n", (unsigned)glGetError());
}

#undef DRAW_TRACE_CHECK
#endif

#ifndef MGL_VERTEX_ATTRIB_STATE_TRACE_H
#define MGL_VERTEX_ATTRIB_STATE_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>
#include <stdint.h>

static void vertex_attrib_trace_emit(const char *label) {
    GLenum error = glGetError();
    GLint enabled = -1, size = -1, stride = -1, type = -1, integer = -1, divisor = -1;
    void *pointer = NULL;
    glGetVertexAttribiv(2u, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
    glGetVertexAttribiv(2u, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
    glGetVertexAttribiv(2u, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
    glGetVertexAttribiv(2u, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
    glGetVertexAttribiv(2u, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &integer);
    glGetVertexAttribiv(2u, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &divisor);
    glGetVertexAttribPointerv(2u, GL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer);
    printf("attrib_%s 0x%04x %d %d %d 0x%x %d %d %lu 0x%04x\n", label, (unsigned)error, enabled,
           size, stride, (unsigned)type, integer, divisor, (unsigned long)(uintptr_t)pointer,
           (unsigned)glGetError());
}

static void vertex_attrib_state_trace(void) {
    vertex_attrib_trace_emit("initial");
    glVertexAttribPointer(2u, 3, GL_FLOAT, GL_TRUE, 12, (const void *)(uintptr_t)0x1000);
    vertex_attrib_trace_emit("float_pointer");
    glEnableVertexAttribArray(2u);
    glVertexAttribDivisor(2u, 5u);
    vertex_attrib_trace_emit("enabled_divisor");
    glVertexAttribIPointer(2u, 2, GL_INT, 8, (const void *)(uintptr_t)0x2000);
    vertex_attrib_trace_emit("integer_pointer");
    glDisableVertexAttribArray(2u);
    vertex_attrib_trace_emit("disabled");

    glBegin(GL_POINTS);
    glVertexAttribPointer(2u, 4, GL_UNSIGNED_BYTE, GL_FALSE, 4, (const void *)(uintptr_t)0x3000);
    glEnd();
    vertex_attrib_trace_emit("begin_float_pointer");
    glBegin(GL_POINTS);
    glVertexAttribIPointer(2u, 4, GL_UNSIGNED_INT, 16, (const void *)(uintptr_t)0x4000);
    glEnd();
    vertex_attrib_trace_emit("begin_integer_pointer");
    glBegin(GL_POINTS);
    glEnableVertexAttribArray(2u);
    glEnd();
    vertex_attrib_trace_emit("begin_enable");
    glBegin(GL_POINTS);
    glDisableVertexAttribArray(2u);
    glEnd();
    vertex_attrib_trace_emit("begin_disable");
    glBegin(GL_POINTS);
    glVertexAttribDivisor(2u, 12u);
    glEnd();
    vertex_attrib_trace_emit("begin_divisor");
    glBegin(GL_POINTS);
    glVertexAttribPointer(9999u, 9, 0u, GL_FALSE, -1, NULL);
    glEnd();
    vertex_attrib_trace_emit("begin_invalid_precedence");
    glBegin(GL_POINTS);
    glGetVertexAttribiv(2u, GL_VERTEX_ATTRIB_ARRAY_SIZE, &(GLint){-5});
    glEnd();
    vertex_attrib_trace_emit("begin_query");
    glBegin(GL_POINTS);
    glGetVertexAttribPointerv(9999u, 0u, &(void *){NULL});
    glEnd();
    vertex_attrib_trace_emit("begin_pointer_query_precedence");

    glVertexAttribPointer(2u, 0, GL_FLOAT, GL_FALSE, 0, NULL);
    vertex_attrib_trace_emit("invalid_size");
    glVertexAttribIPointer(2u, 2, GL_FLOAT, 0, NULL);
    vertex_attrib_trace_emit("invalid_integer_type");
    glVertexAttribPointer(2u, 2, 0u, GL_FALSE, 0, NULL);
    vertex_attrib_trace_emit("invalid_float_type");
    glEnableVertexAttribArray(9999u);
    vertex_attrib_trace_emit("invalid_index");
    glVertexAttribPointer(2u, 4, GL_FLOAT, GL_FALSE, 20, (const void *)(uintptr_t)0x5000);
    glEnableVertexAttribArray(2u);
    glVertexAttribDivisor(2u, 0u);
    vertex_attrib_trace_emit("recovered");
    {
        GLfloat current[4] = {-9.0f, -9.0f, -9.0f, -9.0f};
        GLenum error;
        glBegin(GL_POINTS);
        glVertexAttrib4f(2u, 0.25f, 0.5f, 0.75f, 1.0f);
        glEnd();
        error = glGetError();
        glGetVertexAttribfv(2u, GL_CURRENT_VERTEX_ATTRIB, current);
        printf("attrib_current_inside_begin 0x%04x %.3f %.3f %.3f %.3f 0x%04x\n", (unsigned)error,
               current[0], current[1], current[2], current[3], (unsigned)glGetError());
    }
}
#endif

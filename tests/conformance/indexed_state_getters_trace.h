#ifndef MGL_INDEXED_STATE_GETTERS_TRACE_H
#define MGL_INDEXED_STATE_GETTERS_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void indexed_state_getters_trace(void) {
    GLboolean mask[4] = {9, 9, 9, 9};
    GLboolean boolean_value = 9;
    GLint integer_value = -777;
    GLint64 integer64_value = -777;
    GLfloat float_value = -777.0f;
    GLdouble double_value = -777.0;

    glColorMaski(1u, GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
    glGetBooleani_v(GL_COLOR_WRITEMASK, 1u, mask);
    printf("indexed_mask_set %u%u%u%u 0x%04x\n", mask[0], mask[1], mask[2], mask[3],
           (unsigned)glGetError());
    glBegin(GL_POINTS);
    glColorMaski(1u, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnd();
    glGetBooleani_v(GL_COLOR_WRITEMASK, 1u, mask);
    printf("indexed_mask_begin %u%u%u%u 0x%04x\n", mask[0], mask[1], mask[2], mask[3],
           (unsigned)glGetError());
    glBegin(GL_POINTS);
    glColorMaski(~0u, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnd();
    printf("indexed_mask_begin_bad_index 0x%04x\n", (unsigned)glGetError());

    glBlendFuncSeparatei(1u, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
    glGetIntegeri_v(GL_BLEND_SRC_RGB, 1u, &integer_value);
    printf("indexed_blend_func_set %d 0x%04x\n", integer_value, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glBlendFunci(1u, GL_ZERO, GL_ZERO);
    glEnd();
    glGetIntegeri_v(GL_BLEND_SRC_RGB, 1u, &integer_value);
    printf("indexed_blend_func_begin %d 0x%04x\n", integer_value, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glBlendFuncSeparatei(~0u, 0u, 0u, 0u, 0u);
    glEnd();
    printf("indexed_blend_func_begin_bad_index 0x%04x\n", (unsigned)glGetError());
    glBlendFunci(1u, 0xDEADu, GL_ONE);
    printf("indexed_blend_func_invalid 0x%04x\n", (unsigned)glGetError());

    glBlendEquationSeparatei(1u, GL_FUNC_SUBTRACT, GL_FUNC_ADD);
    glGetIntegeri_v(GL_BLEND_EQUATION_RGB, 1u, &integer_value);
    printf("indexed_blend_eq_set %d 0x%04x\n", integer_value, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glBlendEquationi(1u, GL_FUNC_ADD);
    glEnd();
    glGetIntegeri_v(GL_BLEND_EQUATION_RGB, 1u, &integer_value);
    printf("indexed_blend_eq_begin %d 0x%04x\n", integer_value, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glBlendEquationSeparatei(~0u, 0u, 0u);
    glEnd();
    printf("indexed_blend_eq_begin_invalid 0x%04x\n", (unsigned)glGetError());

    glGetBooleanv(GL_DEPTH_WRITEMASK, &boolean_value);
    printf("getter_boolean_valid %u 0x%04x\n", boolean_value, (unsigned)glGetError());
    boolean_value = 9;
    glBegin(GL_POINTS);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &boolean_value);
    glEnd();
    printf("getter_boolean_begin %u 0x%04x\n", boolean_value, (unsigned)glGetError());
    glGetIntegerv(GL_DEPTH_FUNC, &integer_value);
    printf("getter_integer_valid %d 0x%04x\n", integer_value, (unsigned)glGetError());
    integer_value = -777;
    glBegin(GL_POINTS);
    glGetIntegerv(GL_DEPTH_FUNC, &integer_value);
    glEnd();
    printf("getter_integer_begin %d 0x%04x\n", integer_value, (unsigned)glGetError());
    integer_value = -777;
    glBegin(GL_POINTS);
    glGetIntegerv(0xDEADu, &integer_value);
    glEnd();
    printf("getter_integer_begin_bad_enum %d 0x%04x\n", integer_value, (unsigned)glGetError());
    glGetFloatv(GL_LINE_WIDTH, &float_value);
    printf("getter_float_valid %.3f 0x%04x\n", float_value, (unsigned)glGetError());
    float_value = -777.0f;
    glBegin(GL_POINTS);
    glGetFloatv(GL_LINE_WIDTH, &float_value);
    glEnd();
    printf("getter_float_begin %.3f 0x%04x\n", float_value, (unsigned)glGetError());
    glGetDoublev(GL_DEPTH_CLEAR_VALUE, &double_value);
    printf("getter_double_valid %.3f 0x%04x\n", double_value, (unsigned)glGetError());
    double_value = -777.0;
    glBegin(GL_POINTS);
    glGetDoublev(GL_DEPTH_CLEAR_VALUE, &double_value);
    glEnd();
    printf("getter_double_begin %.3f 0x%04x\n", double_value, (unsigned)glGetError());
    mask[0] = 9;
    glBegin(GL_POINTS);
    glGetBooleani_v(GL_COLOR_WRITEMASK, 1u, mask);
    glEnd();
    printf("getter_boolean_indexed_begin %u 0x%04x\n", mask[0], (unsigned)glGetError());
    integer_value = -777;
    glBegin(GL_POINTS);
    glGetIntegeri_v(GL_BLEND_SRC_RGB, 1u, &integer_value);
    glEnd();
    printf("getter_integer_indexed_begin %d 0x%04x\n", integer_value, (unsigned)glGetError());
    integer_value = -777;
    glBegin(GL_POINTS);
    glGetIntegeri_v(GL_BLEND_SRC_RGB, ~0u, &integer_value);
    glEnd();
    printf("getter_integer_indexed_begin_bad_index %d 0x%04x\n", integer_value,
           (unsigned)glGetError());
    glGetInteger64i_v(GL_UNIFORM_BUFFER_BINDING, 0u, &integer64_value);
    printf("getter_integer64_indexed_valid %lld 0x%04x\n", (long long)integer64_value,
           (unsigned)glGetError());
    integer64_value = -777;
    glBegin(GL_POINTS);
    glGetInteger64i_v(GL_UNIFORM_BUFFER_BINDING, 0u, &integer64_value);
    glEnd();
    printf("getter_integer64_indexed_begin %lld 0x%04x\n", (long long)integer64_value,
           (unsigned)glGetError());
    float_value = -777.0f;
    glBegin(GL_POINTS);
    glGetFloatIndexedvEXT(GL_BLEND, 1u, &float_value);
    glEnd();
    printf("getter_float_indexed_begin %.3f 0x%04x\n", float_value, (unsigned)glGetError());
    double_value = -777.0;
    glBegin(GL_POINTS);
    glGetDoubleIndexedvEXT(GL_BLEND, 1u, &double_value);
    glEnd();
    printf("getter_double_indexed_begin %.3f 0x%04x\n", double_value, (unsigned)glGetError());
    glNewList(9077u, GL_COMPILE);
    glBegin(GL_POINTS);
    glColorMaski(1u, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBlendFunci(1u, GL_ZERO, GL_ZERO);
    glBlendEquationi(1u, GL_FUNC_ADD);
    glEnd();
    glEndList();
    printf("indexed_list_compile 0x%04x\n", (unsigned)glGetError());
    glCallList(9077u);
    printf("indexed_list_replay 0x%04x\n", (unsigned)glGetError());
    glGetBooleani_v(GL_COLOR_WRITEMASK, 1u, mask);
    glGetIntegeri_v(GL_BLEND_SRC_RGB, 1u, &integer_value);
    printf("indexed_list_preserved %u%u%u%u %d 0x%04x\n", mask[0], mask[1], mask[2], mask[3],
           integer_value, (unsigned)glGetError());
    glDeleteLists(9077u, 1);
}
#endif

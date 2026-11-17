#ifndef MGL_INDEXED_FAMILY_BATCH_TRACE_H
#define MGL_INDEXED_FAMILY_BATCH_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void bt_error(const char *label) {
    printf("bt_%s 0x%x\n", label, (unsigned)glGetError());
}

static void indexed_family_batch_trace(void) {
    GLuint vao = 0;
    GLboolean bv = 0;
    GLint iv = 0;
    GLfloat fv = 0.0f;
    GLdouble dv = 0.0;
    void *ptr = NULL;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    (void)glGetError();
    glEnableIndexedEXT(GL_BLEND, 1u);
    bt_error("glEnableIndexedEXT_valid");
    glBegin(GL_POINTS);
    glEnableIndexedEXT(0xDEADu, ~0u);
    glEnd();
    bt_error("glEnableIndexedEXT_begin_bad");
    glDisableIndexedEXT(GL_BLEND, 1u);
    bt_error("glDisableIndexedEXT_valid");
    glBegin(GL_POINTS);
    glDisableIndexedEXT(0xDEADu, ~0u);
    glEnd();
    bt_error("glDisableIndexedEXT_begin_bad");
    (void)glIsEnabledIndexedEXT(GL_BLEND, 1u);
    bt_error("glIsEnabledIndexedEXT_valid");
    glBegin(GL_POINTS);
    (void)glIsEnabledIndexedEXT(0xDEADu, ~0u);
    glEnd();
    bt_error("glIsEnabledIndexedEXT_begin_bad");
    glGetBooleanIndexedvEXT(GL_BLEND, 1u, &bv);
    bt_error("glGetBooleanIndexedvEXT_valid");
    glBegin(GL_POINTS);
    glGetBooleanIndexedvEXT(0xDEADu, ~0u, &bv);
    glEnd();
    bt_error("glGetBooleanIndexedvEXT_begin_bad");
    glGetIntegerIndexedvEXT(GL_BLEND, 1u, &iv);
    bt_error("glGetIntegerIndexedvEXT_valid");
    glBegin(GL_POINTS);
    glGetIntegerIndexedvEXT(0xDEADu, ~0u, &iv);
    glEnd();
    bt_error("glGetIntegerIndexedvEXT_begin_bad");
    glGetFloatIndexedvEXT(GL_BLEND, 1u, &fv);
    bt_error("glGetFloatIndexedvEXT_valid");
    glBegin(GL_POINTS);
    glGetFloatIndexedvEXT(0xDEADu, ~0u, &fv);
    glEnd();
    bt_error("glGetFloatIndexedvEXT_begin_bad");
    glGetDoubleIndexedvEXT(GL_BLEND, 1u, &dv);
    bt_error("glGetDoubleIndexedvEXT_valid");
    glBegin(GL_POINTS);
    glGetDoubleIndexedvEXT(0xDEADu, ~0u, &dv);
    glEnd();
    bt_error("glGetDoubleIndexedvEXT_begin_bad");
    glColorMaskIndexedEXT(1u, GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
    bt_error("glColorMaskIndexedEXT_valid");
    glBegin(GL_POINTS);
    glColorMaskIndexedEXT(~0u, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnd();
    bt_error("glColorMaskIndexedEXT_begin_bad");
    glBlendFunciEXT(1u, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    bt_error("glBlendFunciEXT_valid");
    glBegin(GL_POINTS);
    glBlendFunciEXT(~0u, 0xDEADu, GL_ZERO);
    glEnd();
    bt_error("glBlendFunciEXT_begin_bad");
    glBlendEquationiEXT(1u, GL_FUNC_SUBTRACT);
    bt_error("glBlendEquationiEXT_valid");
    glBegin(GL_POINTS);
    glBlendEquationiEXT(~0u, 0xDEADu);
    glEnd();
    bt_error("glBlendEquationiEXT_begin_bad");
    glBlendFuncSeparateiEXT(1u, GL_ONE, GL_ZERO, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    bt_error("glBlendFuncSeparateiEXT_valid");
    glBegin(GL_POINTS);
    glBlendFuncSeparateiEXT(~0u, 0xDEADu, GL_ZERO, GL_ZERO, GL_ZERO);
    glEnd();
    bt_error("glBlendFuncSeparateiEXT_begin_bad");
    glBlendEquationSeparateiEXT(1u, GL_FUNC_ADD, GL_FUNC_SUBTRACT);
    bt_error("glBlendEquationSeparateiEXT_valid");
    glBegin(GL_POINTS);
    glBlendEquationSeparateiEXT(~0u, 0xDEADu, GL_FUNC_ADD);
    glEnd();
    bt_error("glBlendEquationSeparateiEXT_begin_bad");
    glBlendFunciARB(1u, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    bt_error("glBlendFunciARB_valid");
    glBegin(GL_POINTS);
    glBlendFunciARB(~0u, 0xDEADu, GL_ZERO);
    glEnd();
    bt_error("glBlendFunciARB_begin_bad");
    glBlendEquationiARB(1u, GL_FUNC_SUBTRACT);
    bt_error("glBlendEquationiARB_valid");
    glBegin(GL_POINTS);
    glBlendEquationiARB(~0u, 0xDEADu);
    glEnd();
    bt_error("glBlendEquationiARB_begin_bad");
    glBlendFuncSeparateiARB(1u, GL_ONE, GL_ZERO, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    bt_error("glBlendFuncSeparateiARB_valid");
    glBegin(GL_POINTS);
    glBlendFuncSeparateiARB(~0u, 0xDEADu, GL_ZERO, GL_ZERO, GL_ZERO);
    glEnd();
    bt_error("glBlendFuncSeparateiARB_begin_bad");
    glBlendEquationSeparateiARB(1u, GL_FUNC_ADD, GL_FUNC_SUBTRACT);
    bt_error("glBlendEquationSeparateiARB_valid");
    glBegin(GL_POINTS);
    glBlendEquationSeparateiARB(~0u, 0xDEADu, GL_FUNC_ADD);
    glEnd();
    bt_error("glBlendEquationSeparateiARB_begin_bad");
    glEnableClientStateIndexedEXT(GL_TEXTURE_COORD_ARRAY, 1u);
    bt_error("glEnableClientStateIndexedEXT_valid");
    glBegin(GL_POINTS);
    glEnableClientStateIndexedEXT(0xDEADu, ~0u);
    glEnd();
    bt_error("glEnableClientStateIndexedEXT_begin_bad");
    glDisableClientStateIndexedEXT(GL_TEXTURE_COORD_ARRAY, 1u);
    bt_error("glDisableClientStateIndexedEXT_valid");
    glBegin(GL_POINTS);
    glDisableClientStateIndexedEXT(0xDEADu, ~0u);
    glEnd();
    bt_error("glDisableClientStateIndexedEXT_begin_bad");
    glEnableClientStateiEXT(GL_TEXTURE_COORD_ARRAY, 1u);
    bt_error("glEnableClientStateiEXT_valid");
    glBegin(GL_POINTS);
    glEnableClientStateiEXT(0xDEADu, ~0u);
    glEnd();
    bt_error("glEnableClientStateiEXT_begin_bad");
    glDisableClientStateiEXT(GL_TEXTURE_COORD_ARRAY, 1u);
    bt_error("glDisableClientStateiEXT_valid");
    glBegin(GL_POINTS);
    glDisableClientStateiEXT(0xDEADu, ~0u);
    glEnd();
    bt_error("glDisableClientStateiEXT_begin_bad");
    glGetPointerIndexedvEXT(GL_TEXTURE_COORD_ARRAY_POINTER, 1u, &ptr);
    bt_error("glGetPointerIndexedvEXT_valid");
    glBegin(GL_POINTS);
    glGetPointerIndexedvEXT(0xDEADu, ~0u, &ptr);
    glEnd();
    bt_error("glGetPointerIndexedvEXT_begin_bad");
    glEnableVertexArrayEXT(vao, GL_VERTEX_ARRAY);
    bt_error("glEnableVertexArrayEXT_valid");
    glBegin(GL_POINTS);
    glEnableVertexArrayEXT(0u, 0xDEADu);
    glEnd();
    bt_error("glEnableVertexArrayEXT_begin_bad");
    glDisableVertexArrayEXT(vao, GL_VERTEX_ARRAY);
    bt_error("glDisableVertexArrayEXT_valid");
    glBegin(GL_POINTS);
    glDisableVertexArrayEXT(0u, 0xDEADu);
    glEnd();
    bt_error("glDisableVertexArrayEXT_begin_bad");
    glEnableVertexArrayAttrib(vao, 1u);
    bt_error("glEnableVertexArrayAttrib_valid");
    glBegin(GL_POINTS);
    glEnableVertexArrayAttrib(0u, ~0u);
    glEnd();
    bt_error("glEnableVertexArrayAttrib_begin_bad");
    glDisableVertexArrayAttrib(vao, 1u);
    bt_error("glDisableVertexArrayAttrib_valid");
    glBegin(GL_POINTS);
    glDisableVertexArrayAttrib(0u, ~0u);
    glEnd();
    bt_error("glDisableVertexArrayAttrib_begin_bad");
    glEnableVertexArrayAttribEXT(vao, 1u);
    bt_error("glEnableVertexArrayAttribEXT_valid");
    glBegin(GL_POINTS);
    glEnableVertexArrayAttribEXT(0u, ~0u);
    glEnd();
    bt_error("glEnableVertexArrayAttribEXT_begin_bad");
    glDisableVertexArrayAttribEXT(vao, 1u);
    bt_error("glDisableVertexArrayAttribEXT_valid");
    glBegin(GL_POINTS);
    glDisableVertexArrayAttribEXT(0u, ~0u);
    glEnd();
    bt_error("glDisableVertexArrayAttribEXT_begin_bad");
    /* Invalid indices outside a primitive exercise the distinct value/error
     * validation paths (the Begin/End cases above exercise precedence). */
    glEnableIndexedEXT(GL_BLEND, ~0u);
    bt_error("enable_blend_bad_index");
    glGetIntegerIndexedvEXT(GL_BLEND, ~0u, &iv);
    bt_error("get_blend_bad_index");
    glEnableClientStateIndexedEXT(GL_TEXTURE_COORD_ARRAY, ~0u);
    bt_error("enable_client_bad_index");
    glDisableClientStateiEXT(GL_TEXTURE_COORD_ARRAY, ~0u);
    bt_error("disable_client_bad_index");
    glEnableVertexArrayAttrib(vao, ~0u);
    bt_error("named_attrib_bad_index");
    glDisableVertexArrayAttribEXT(vao, ~0u);
    bt_error("named_attrib_ext_bad_index");
    glEnableVertexArrayEXT(0u, GL_VERTEX_ARRAY);
    bt_error("named_vao_bad_name");
    glDisableIndexedEXT(GL_BLEND, 1u);
    glEnableIndexedEXT(GL_BLEND, 2u);
    bt_error("blend_setup");
    {
        GLboolean b1 = glIsEnabledIndexedEXT(GL_BLEND, 1u),
                  b2 = glIsEnabledIndexedEXT(GL_BLEND, 2u);
        GLenum error = glGetError();
        printf("bt_blend_isolation %d %d 0x%x\n", (int)b1, (int)b2, (unsigned)error);
    }
    glGetIntegerIndexedvEXT(GL_BLEND, 2u, &iv);
    printf("bt_blend_get %d 0x%x\n", iv, (unsigned)glGetError());
    glEnableClientStateIndexedEXT(GL_TEXTURE_COORD_ARRAY, 1u);
    {
        GLboolean enabled = glIsEnabledIndexedEXT(GL_TEXTURE_COORD_ARRAY, 1u);
        GLenum error = glGetError();
        printf("bt_client_enabled %d 0x%x\n", (int)enabled, (unsigned)error);
    }
    glClientActiveTexture(GL_TEXTURE1);
    {
        GLboolean enabled = glIsEnabled(GL_TEXTURE_COORD_ARRAY);
        GLenum error = glGetError();
        printf("bt_client_legacy_enabled %d 0x%x\n", (int)enabled, (unsigned)error);
    }
    glDisableClientStateiEXT(GL_TEXTURE_COORD_ARRAY, 1u);
    {
        GLboolean enabled = glIsEnabledIndexedEXT(GL_TEXTURE_COORD_ARRAY, 1u);
        GLenum error = glGetError();
        printf("bt_client_disabled %d 0x%x\n", (int)enabled, (unsigned)error);
    }
    {
        GLboolean enabled = glIsEnabled(GL_TEXTURE_COORD_ARRAY);
        GLenum error = glGetError();
        printf("bt_client_legacy_disabled %d 0x%x\n", (int)enabled, (unsigned)error);
    }
    glClientActiveTexture(GL_TEXTURE0);
    glBindVertexArray(0u);
    glBindVertexArray(vao);
    (void)glGetError();
    glDeleteVertexArrays(1, &vao);
}
#endif

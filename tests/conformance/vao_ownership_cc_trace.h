#ifndef MGL_VAO_OWNERSHIP_CC_TRACE_H
#define MGL_VAO_OWNERSHIP_CC_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>
#include <stdint.h>

/* The unsuffixed mGL compatibility names have the same signature and
 * semantics as EXT_direct_state_access on Mesa's GLX compatibility driver. */
#ifdef MGL_MESA_REFERENCE_EXT_ALIASES
#define glEnableVertexArray glEnableVertexArrayEXT
#define glDisableVertexArray glDisableVertexArrayEXT
#define glEnableClientStateIndexed glEnableClientStateIndexedEXT
#define glDisableClientStateIndexed glDisableClientStateIndexedEXT
#define glEnableClientStatei glEnableClientStateiEXT
#define glDisableClientStatei glDisableClientStateiEXT
#define glGetPointerIndexedv glGetPointerIndexedvEXT
#endif
static void cc_err(const char *name) {
    printf("cc_%s 0x%x\n", name, (unsigned)glGetError());
}

static void cc_bool(const char *name, GLboolean v) {
    printf("cc_%s %d 0x%x\n", name, (int)v, (unsigned)glGetError());
}

static void cc_int(const char *name, GLint v) {
    printf("cc_%s %d 0x%x\n", name, (int)v, (unsigned)glGetError());
}

static void cc_ptr(const char *name, const void *v) {
    printf("cc_%s %lu 0x%x\n", name, (unsigned long)(uintptr_t)v, (unsigned)glGetError());
}

static void vao_ownership_cc_trace(void) {
    GLuint vao[2] = {0, 0};
    GLint iv = -1;
    GLboolean bv = 0;
    void *ptr = NULL;
    glGenVertexArrays(2, vao);
    cc_err("gen");
    glBindVertexArray(vao[0]);
    cc_err("bind_first");
    glClientActiveTexture(GL_TEXTURE1);
    cc_err("client_unit1");
    glVertexPointer(3, GL_FLOAT, 0, (const void *)(uintptr_t)0x1230);
    cc_err("vertex_pointer");
    glColorPointer(4, GL_FLOAT, 0, (const void *)(uintptr_t)0x1240);
    cc_err("color_pointer");
    glNormalPointer(GL_FLOAT, 0, (const void *)(uintptr_t)0x1250);
    cc_err("normal_pointer");
    glTexCoordPointer(2, GL_FLOAT, 0, (const void *)(uintptr_t)0x1260);
    cc_err("texcoord_pointer");
    glSecondaryColorPointer(3, GL_FLOAT, 0, (const void *)(uintptr_t)0x1270);
    cc_err("secondary_pointer");
    glFogCoordPointer(GL_FLOAT, 0, (const void *)(uintptr_t)0x1280);
    cc_err("fog_pointer");
    glVertexPointer(3, GL_FLOAT, 0, NULL);
    cc_err("vertex_pointer_null_valid");
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &ptr);
    cc_ptr("vertex_pointer_null_readback", ptr);
    glVertexPointer(3, GL_FLOAT, 0, (const void *)(uintptr_t)0x1f00);
    cc_err("vertex_pointer_reject_nonnull");
    glEnableClientState(GL_VERTEX_ARRAY);
    cc_err("enable_vertex");
    glEnableClientState(GL_COLOR_ARRAY);
    cc_err("enable_color");
    glEnableClientState(GL_NORMAL_ARRAY);
    cc_err("enable_normal");
    glEnableClientState(GL_SECONDARY_COLOR_ARRAY);
    cc_err("enable_secondary");
    glEnableClientState(GL_FOG_COORD_ARRAY);
    cc_err("enable_fog");
    glEnableClientStateIndexed(GL_TEXTURE_COORD_ARRAY, 1);
    cc_err("enable_tex1");
    glIsEnabled(GL_TEXTURE_COORD_ARRAY);
    cc_bool("tex1_enabled", glIsEnabled(GL_TEXTURE_COORD_ARRAY));
    glClientActiveTexture(GL_TEXTURE0);
    cc_err("client_unit0");
    cc_bool("tex0_disabled", glIsEnabled(GL_TEXTURE_COORD_ARRAY));
    glEnableClientStatei(GL_TEXTURE_COORD_ARRAY, 0);
    cc_err("enable_tex0_i");
    cc_bool("tex0_enabled", glIsEnabled(GL_TEXTURE_COORD_ARRAY));
    glDisableClientStateiEXT(GL_TEXTURE_COORD_ARRAY, 0);
    cc_err("disable_tex0_i_ext");
    cc_bool("tex0_disabled_again", glIsEnabled(GL_TEXTURE_COORD_ARRAY));
    glClientActiveTexture(GL_TEXTURE1);
    cc_err("client_unit1_again");
    cc_bool("tex1_still_enabled", glIsEnabled(GL_TEXTURE_COORD_ARRAY));
    glDisableClientStateIndexedEXT(GL_TEXTURE_COORD_ARRAY, 1);
    cc_err("disable_tex1_indexed_ext");
    cc_bool("tex1_disabled", glIsEnabled(GL_TEXTURE_COORD_ARRAY));
    glEnableVertexArray(vao[0], GL_TEXTURE0 + 2);
    cc_err("named_enable_tex2");
    glClientActiveTexture(GL_TEXTURE2);
    cc_err("client_unit2");
    cc_bool("named_tex2_enabled", glIsEnabled(GL_TEXTURE_COORD_ARRAY));
    glDisableVertexArray(vao[0], GL_TEXTURE0 + 2);
    cc_err("named_disable_tex2");
    cc_bool("named_tex2_disabled", glIsEnabled(GL_TEXTURE_COORD_ARRAY));
    glEnableVertexArrayEXT(vao[0], GL_VERTEX_ARRAY);
    cc_err("named_enable_vertex_ext");
    cc_bool("named_vertex_enabled", glIsEnabled(GL_VERTEX_ARRAY));
    glDisableVertexArrayEXT(vao[0], GL_VERTEX_ARRAY);
    cc_err("named_disable_vertex_ext");
    cc_bool("named_vertex_disabled", glIsEnabled(GL_VERTEX_ARRAY));
    glEnableVertexArrayAttrib(vao[0], 3);
    cc_err("enable_attrib_dsa");
    glGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &iv);
    cc_int("attrib3_enabled", iv);
    glDisableVertexArrayAttribEXT(vao[0], 3);
    cc_err("disable_attrib_ext");
    glGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &iv);
    cc_int("attrib3_disabled", iv);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, (const void *)(uintptr_t)0x1290);
    cc_err("attrib_pointer");
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    cc_err("attrib_pointer_null_valid");
    glGetVertexAttribPointerv(2, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);
    cc_ptr("attrib_pointer_null_readback", ptr);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, (const void *)(uintptr_t)0x1f20);
    cc_err("attrib_pointer_reject_nonnull");
    glVertexAttribIPointer(4, 4, GL_INT, 0, (const void *)(uintptr_t)0x12A0);
    cc_err("integer_attrib_pointer_no_buffer");
    glInterleavedArrays(GL_V3F, 0, (const void *)(uintptr_t)0x12B0);
    cc_err("interleaved_no_buffer");
    glEnableVertexAttribArray(2);
    cc_err("enable_attrib2");
    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &iv);
    cc_int("attrib2_enabled", iv);
    glDisableVertexAttribArray(2);
    cc_err("disable_attrib2");
    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &iv);
    cc_int("attrib2_disabled", iv);
    glBindVertexArray(vao[1]);
    cc_err("bind_second");
    cc_bool("second_color_disabled", glIsEnabled(GL_COLOR_ARRAY));
    cc_bool("second_vertex_disabled", glIsEnabled(GL_VERTEX_ARRAY));
    glClientActiveTexture(GL_TEXTURE1);
    cc_err("second_unit1");
    cc_bool("second_tex1_disabled", glIsEnabled(GL_TEXTURE_COORD_ARRAY));
    glEnableVertexArray(vao[0], GL_COLOR_ARRAY);
    cc_err("enable_first_while_second_bound");
    cc_bool("second_color_preserved", glIsEnabled(GL_COLOR_ARRAY));
    glBindVertexArray(vao[0]);
    cc_err("rebind_first");
    cc_bool("first_color_enabled", glIsEnabled(GL_COLOR_ARRAY));
    glDisableVertexArray(vao[0], GL_COLOR_ARRAY);
    cc_err("disable_first_color");
    cc_bool("first_color_disabled", glIsEnabled(GL_COLOR_ARRAY));
    glEnableClientStateIndexedEXT(GL_TEXTURE_COORD_ARRAY, 99999u);
    cc_err("indexed_invalid_index");
    glDisableClientStateIndexed(GL_TEXTURE_COORD_ARRAY, 99999u);
    cc_err("indexed_disable_invalid_index");
    glEnableVertexArray(0, GL_VERTEX_ARRAY);
    cc_err("named_zero_name");
    glDisableVertexArray(vao[0], 0xDEADu);
    cc_err("named_invalid_enum");
    glEnableVertexArrayAttribEXT(vao[0], ~0u);
    cc_err("named_bad_attrib");
    glBegin(GL_POINTS);
    glEnableVertexArray(vao[0], GL_COLOR_ARRAY);
    glDisableVertexArray(vao[0], GL_COLOR_ARRAY);
    glEnableClientStateIndexed(GL_TEXTURE_COORD_ARRAY, 1);
    glDisableClientStatei(GL_TEXTURE_COORD_ARRAY, 1);
    glEnableVertexArrayAttrib(vao[0], 1);
    glDisableVertexArrayAttrib(vao[0], 1);
    glEnd();
    /* Error state accumulates only the first error; isolate Begin/End cases. */
    cc_err("begin_rejected_first");
    glBegin(GL_POINTS);
    glEnableVertexArray(vao[0], 0xDEADu);
    glEnd();
    cc_err("begin_named_bad_enum");
    glBegin(GL_POINTS);
    glDisableVertexArray(vao[0], 0xDEADu);
    glEnd();
    cc_err("begin_named_disable_bad_enum");
    glBegin(GL_POINTS);
    glEnableClientStateIndexed(0xDEADu, 99999u);
    glEnd();
    cc_err("begin_indexed_bad");
    glBegin(GL_POINTS);
    glDisableClientStatei(0xDEADu, 99999u);
    glEnd();
    cc_err("begin_indexed_disable_bad");
    glBegin(GL_POINTS);
    glGetPointerIndexedv(0xDEADu, 99999u, &ptr);
    glEnd();
    cc_err("begin_indexed_pointer_bad");
    glBegin(GL_POINTS);
    glEnableVertexArrayAttrib(vao[0], ~0u);
    glEnd();
    cc_err("begin_attrib_bad");
    cc_bool("color_after_rejections", glIsEnabled(GL_COLOR_ARRAY));
    {
        GLuint buffer = 0;
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER, 512, NULL, GL_STATIC_DRAW);
        cc_err("vbo_setup");
        glVertexPointer(3, GL_FLOAT, 0, (const void *)(uintptr_t)0x20);
        cc_err("vbo_vertex_pointer");
        glGetPointerv(GL_VERTEX_ARRAY_POINTER, &ptr);
        cc_ptr("vbo_vertex_offset", ptr);
        glClientActiveTexture(GL_TEXTURE1);
        glTexCoordPointer(2, GL_FLOAT, 0, (const void *)(uintptr_t)0x28);
        cc_err("vbo_texcoord_pointer");
        glGetPointerIndexedv(GL_TEXTURE_COORD_ARRAY_POINTER, 1, &ptr);
        cc_ptr("vbo_texcoord_indexed", ptr);
        glGetPointerIndexedvEXT(GL_TEXTURE_COORD_ARRAY_POINTER, 1, &ptr);
        cc_ptr("vbo_texcoord_indexed_ext", ptr);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, (const void *)(uintptr_t)0x30);
        cc_err("vbo_attrib_pointer");
        glGetVertexAttribPointerv(2, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);
        cc_ptr("vbo_attrib_offset", ptr);
        glVertexAttribIPointer(4, 4, GL_INT, 0, (const void *)(uintptr_t)0x40);
        cc_err("vbo_int_attrib_pointer");
        glGetVertexAttribPointerv(4, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);
        cc_ptr("vbo_int_attrib_offset", ptr);
        glInterleavedArrays(GL_V3F, 0, (const void *)(uintptr_t)0x50);
        cc_err("vbo_interleaved");
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &buffer);
    }
    glClientActiveTexture(GL_TEXTURE0);
    glBindVertexArray(0);
    glVertexPointer(3, GL_FLOAT, 0, (const void *)(uintptr_t)0x1500);
    cc_err("default_vao_client_pointer");
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &ptr);
    cc_ptr("default_vao_client_readback", ptr);
    glDeleteVertexArrays(2, vao);
    cc_err("cleanup");
}
#ifdef MGL_MESA_REFERENCE_EXT_ALIASES
#undef glEnableVertexArray
#undef glDisableVertexArray
#undef glEnableClientStateIndexed
#undef glDisableClientStateIndexed
#undef glEnableClientStatei
#undef glDisableClientStatei
#undef glGetPointerIndexedv
#endif
#endif

#ifndef MGL_MULTITEX_ALIAS_CB_TRACE_H
#define MGL_MULTITEX_ALIAS_CB_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>
/* The unsuffixed mGL compatibility aliases share the EXT DSA signature and
 * contract. Native desktop Mesa exports the EXT names, not these mGL aliases.
 * Keep both runners executing the same semantic operations. */
#ifdef MGL_MESA_REFERENCE_EXT_ALIASES
#define glMultiTexParameteri glMultiTexParameteriEXT
#define glMultiTexParameteriv glMultiTexParameterivEXT
#define glMultiTexParameterf glMultiTexParameterfEXT
#define glMultiTexParameterfv glMultiTexParameterfvEXT
#define glGetMultiTexParameteriv glGetMultiTexParameterivEXT
#define glGetMultiTexParameterfv glGetMultiTexParameterfvEXT
#define glMultiTexImage2D glMultiTexImage2DEXT
#define glMultiTexSubImage2D glMultiTexSubImage2DEXT
#define glMultiTexImage3D glMultiTexImage3DEXT
#define glMultiTexSubImage3D glMultiTexSubImage3DEXT
#define glCompressedMultiTexImage2D glCompressedMultiTexImage2DEXT
#define glCompressedMultiTexSubImage2D glCompressedMultiTexSubImage2DEXT
#define glCompressedMultiTexImage3D glCompressedMultiTexImage3DEXT
#define glCompressedMultiTexSubImage3D glCompressedMultiTexSubImage3DEXT
#define glCopyMultiTexImage2D glCopyMultiTexImage2DEXT
#define glCopyMultiTexSubImage2D glCopyMultiTexSubImage2DEXT
#define glCopyMultiTexSubImage3D glCopyMultiTexSubImage3DEXT
#define glGetMultiTexImage glGetMultiTexImageEXT
#define glGetCompressedMultiTexImage glGetCompressedMultiTexImageEXT
#define glGetMultiTexLevelParameteriv glGetMultiTexLevelParameterivEXT
#define glGetMultiTexLevelParameterfv glGetMultiTexLevelParameterfvEXT
#define glGetBooleanIndexedv glGetBooleanIndexedvEXT
#define glGetIntegerIndexedv glGetIntegerIndexedvEXT
#define glGetFloatIndexedv glGetFloatIndexedvEXT
#define glGetDoubleIndexedv glGetDoubleIndexedvEXT
#endif
static void cb_err(const char *case_name) {
    printf("cb_%s 0x%04x\n", case_name, (unsigned)glGetError());
}

static void cb_i(const char *case_name, GLint value) {
    printf("cb_%s %d 0x%04x\n", case_name, value, (unsigned)glGetError());
}

static void cb_f(const char *case_name, GLfloat value) {
    printf("cb_%s %.2f 0x%04x\n", case_name, (double)value, (unsigned)glGetError());
}

static void multitex_alias_cb_trace(void) {
    GLuint t[4] = {0};
    GLint i = 0, iv[4] = {GL_CLAMP_TO_EDGE, 0, 0, 0};
    GLfloat f = 0.f, fv[4] = {2.5f, 0, 0, 0};
    GLboolean b = 0;
    GLdouble d = 0;
    const GLubyte texels[16] = {11, 12, 13, 14, 21, 22, 23, 24, 31, 32, 33, 34, 41, 42, 43, 44};
    GLubyte result[16] = {0};
    const GLubyte block[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    GLubyte compressed[8] = {0};
    glGenTextures(4, t);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, t[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, t[1]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, t[2]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, t[3]);
    cb_err("setup");
    glMultiTexParameteri(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    cb_err("Parameteri");
    glMultiTexParameteriv(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, iv);
    cb_err("Parameteriv");
    glMultiTexParameterf(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 1.25f);
    cb_err("Parameterf");
    glMultiTexParameterfv(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, fv);
    cb_err("Parameterfv");
    glGetMultiTexParameteriv(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &i);
    cb_i("GetParamiv", i);
    glGetMultiTexParameterfv(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, &f);
    cb_f("GetParamfv", f);
    glActiveTexture(GL_TEXTURE0);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &i);
    cb_i("unit0_unchanged", i);
    glMultiTexImage2D(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      texels);
    cb_err("Image2D");
    glMultiTexSubImage2D(GL_TEXTURE1, GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                         texels + 4);
    cb_err("SubImage2D");
    glGetMultiTexLevelParameteriv(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &i);
    cb_i("Leveliv_width", i);
    glGetMultiTexLevelParameterfv(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &f);
    cb_f("Levelfv_height", f);
    glGetMultiTexImage(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, result);
    printf("cb_GetImage %u %u %u %u 0x%04x\n", result[0], result[1], result[4], result[7],
           (unsigned)glGetError());
    glMultiTexImage3D(GL_TEXTURE1, GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 1, 0, GL_RGBA,
                      GL_UNSIGNED_BYTE, texels);
    cb_err("Image3D");
    glMultiTexSubImage3D(GL_TEXTURE1, GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                         texels);
    cb_err("SubImage3D");
    glGetMultiTexLevelParameteriv(GL_TEXTURE1, GL_TEXTURE_3D, 0, GL_TEXTURE_DEPTH, &i);
    cb_i("depth3D", i);
    glBegin(GL_POINTS);
    glMultiTexParameteri(0xDEADu, 0xDEADu, 0xDEADu, 0);
    glEnd();
    cb_err("Parameteri_begin");
    glBegin(GL_POINTS);
    glMultiTexImage2D(0xDEADu, 0xDEADu, -1, 0, -1, -1, -1, 0, 0, NULL);
    glEnd();
    cb_err("Image2D_begin");
    glBegin(GL_POINTS);
    glMultiTexImage3D(0xDEADu, 0xDEADu, -1, 0, -1, -1, -1, -1, 0, 0, NULL);
    glEnd();
    cb_err("Image3D_begin");
    glBegin(GL_POINTS);
    glMultiTexSubImage2D(0xDEADu, 0xDEADu, -1, 0, 0, -1, -1, 0, 0, NULL);
    glEnd();
    cb_err("SubImage2D_begin");
    glBegin(GL_POINTS);
    glMultiTexSubImage3D(0xDEADu, 0xDEADu, -1, 0, 0, 0, -1, -1, -1, 0, 0, NULL);
    glEnd();
    cb_err("SubImage3D_begin");
    glBegin(GL_POINTS);
    glGetMultiTexParameteriv(0xDEADu, 0xDEADu, 0xDEADu, &i);
    glEnd();
    cb_err("GetParamiv_begin");
    glBegin(GL_POINTS);
    glGetMultiTexImage(0xDEADu, 0xDEADu, -1, 0, 0, result);
    glEnd();
    cb_err("GetImage_begin");
    glBegin(GL_POINTS);
    glGetMultiTexLevelParameteriv(0xDEADu, 0xDEADu, -1, 0xDEADu, &i);
    glEnd();
    cb_err("Leveliv_begin");
    /* Compressed storage uses a different texture so valid and invalid upload
     * paths cannot overwrite the uncompressed readback above. */
    glMultiTexImage2D(GL_TEXTURE0, GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      NULL);
    cb_err("image_unit0");
    glCompressedMultiTexImage2D(GL_TEXTURE1, GL_TEXTURE_2D, 1, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8,
                                block);
    cb_err("CompressedImage2D");
    glCompressedMultiTexSubImage2D(GL_TEXTURE1, GL_TEXTURE_2D, 1, 0, 0, 4, 4,
                                   GL_COMPRESSED_RED_RGTC1, 8, block);
    cb_err("CompressedSub2D");
    glGetCompressedMultiTexImage(GL_TEXTURE1, GL_TEXTURE_2D, 1, compressed);
    printf("cb_GetCompressed %u %u 0x%04x\n", compressed[0], compressed[1], (unsigned)glGetError());
    glCompressedMultiTexImage3D(GL_TEXTURE1, GL_TEXTURE_3D, 1, GL_COMPRESSED_RED_RGTC1, 4, 4, 1, 0,
                                8, block);
    cb_err("CompressedImage3D");
    glCompressedMultiTexSubImage3D(GL_TEXTURE1, GL_TEXTURE_3D, 1, 0, 0, 0, 4, 4, 1,
                                   GL_COMPRESSED_RED_RGTC1, 8, block);
    cb_err("CompressedSub3D");
    glBegin(GL_POINTS);
    glCompressedMultiTexImage2D(0xDEADu, 0xDEADu, -1, 0, -1, -1, -1, -1, NULL);
    glEnd();
    cb_err("CompressedImage2D_begin");
    glBegin(GL_POINTS);
    glCompressedMultiTexImage3D(0xDEADu, 0xDEADu, -1, 0, -1, -1, -1, -1, -1, NULL);
    glEnd();
    cb_err("CompressedImage3D_begin");
    glBegin(GL_POINTS);
    glCompressedMultiTexSubImage2D(0xDEADu, 0xDEADu, -1, 0, 0, -1, -1, 0, -1, NULL);
    glEnd();
    cb_err("CompressedSub2D_begin");
    glBegin(GL_POINTS);
    glCompressedMultiTexSubImage3D(0xDEADu, 0xDEADu, -1, 0, 0, 0, -1, -1, -1, 0, -1, NULL);
    glEnd();
    cb_err("CompressedSub3D_begin");
    /* Copy legality first; do not treat incompatible default framebuffer
     * contents as a valid cross-native pixel reference. */
    glBegin(GL_POINTS);
    glCopyMultiTexImage2D(0xDEADu, 0xDEADu, -1, 0, 0, 0, -1, -1, -1);
    glEnd();
    cb_err("CopyImage2D_begin");
    glBegin(GL_POINTS);
    glCopyMultiTexSubImage2D(0xDEADu, 0xDEADu, -1, 0, 0, 0, 0, -1, -1);
    glEnd();
    cb_err("CopySub2D_begin");
    glBegin(GL_POINTS);
    glCopyMultiTexSubImage3D(0xDEADu, 0xDEADu, -1, 0, 0, 0, 0, 0, -1, -1);
    glEnd();
    cb_err("CopySub3D_begin");

    glBegin(GL_POINTS);
    glMultiTexParameteriv(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, iv);
    glEnd();
    cb_err("paramiv_valid_begin");
    glBegin(GL_POINTS);
    glMultiTexParameterf(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 1.f);
    glEnd();
    cb_err("paramf_valid_begin");
    glBegin(GL_POINTS);
    glMultiTexParameterfv(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, fv);
    glEnd();
    cb_err("paramfv_valid_begin");
    glBegin(GL_POINTS);
    glGetMultiTexParameterfv(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, &f);
    glEnd();
    cb_err("getparamfv_valid_begin");
    glBegin(GL_POINTS);
    glMultiTexImage2D(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      texels);
    glEnd();
    cb_err("image2_valid_begin");
    glBegin(GL_POINTS);
    glMultiTexSubImage2D(GL_TEXTURE1, GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                         texels);
    glEnd();
    cb_err("subimage2_valid_begin");
    glBegin(GL_POINTS);
    glMultiTexImage3D(GL_TEXTURE1, GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 1, 0, GL_RGBA,
                      GL_UNSIGNED_BYTE, texels);
    glEnd();
    cb_err("image3_valid_begin");
    glBegin(GL_POINTS);
    glMultiTexSubImage3D(GL_TEXTURE1, GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                         texels);
    glEnd();
    cb_err("subimage3_valid_begin");
    glBegin(GL_POINTS);
    glGetMultiTexImage(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, result);
    glEnd();
    cb_err("getimage_valid_begin");
    glBegin(GL_POINTS);
    glGetCompressedMultiTexImage(GL_TEXTURE1, GL_TEXTURE_2D, 1, compressed);
    glEnd();
    cb_err("getcompressed_valid_begin");
    glBegin(GL_POINTS);
    glGetMultiTexLevelParameteriv(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &i);
    glEnd();
    cb_err("getleveliv_valid_begin");
    glBegin(GL_POINTS);
    glGetMultiTexLevelParameterfv(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &f);
    glEnd();
    cb_err("getlevelfv_valid_begin");
    glBegin(GL_POINTS);
    glCopyMultiTexImage2D(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 1, 1, 0);
    glEnd();
    cb_err("copyimage_valid_begin");
    glBegin(GL_POINTS);
    glCopyMultiTexSubImage2D(GL_TEXTURE1, GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
    glEnd();
    cb_err("copysub2_valid_begin");
    glBegin(GL_POINTS);
    glCopyMultiTexSubImage3D(GL_TEXTURE1, GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 1, 1);
    glEnd();
    cb_err("copysub3_valid_begin");
    glEnablei(GL_BLEND, 1);
    glGetBooleanIndexedv(GL_BLEND, 1, &b);
    printf("cb_BoolIndexed %d 0x%04x\n", (int)b, (unsigned)glGetError());
    glGetIntegerIndexedv(GL_BLEND, 1, &i);
    cb_i("IntegerIndexed", i);
    glGetFloatIndexedv(GL_BLEND, 1, &f);
    cb_f("FloatIndexed", f);
    glGetDoubleIndexedv(GL_BLEND, 1, &d);
    printf("cb_DoubleIndexed %.2f 0x%04x\n", d, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glGetBooleanIndexedv(0xDEADu, 999u, &b);
    glEnd();
    cb_err("BoolIndexed_begin");
    glBegin(GL_POINTS);
    glGetIntegerIndexedv(0xDEADu, 999u, &i);
    glEnd();
    cb_err("IntegerIndexed_begin");
    glBegin(GL_POINTS);
    glGetFloatIndexedv(0xDEADu, 999u, &f);
    glEnd();
    cb_err("FloatIndexed_begin");
    glBegin(GL_POINTS);
    glGetDoubleIndexedv(0xDEADu, 999u, &d);
    glEnd();
    cb_err("DoubleIndexed_begin");
    glDisablei(GL_BLEND, 1);

    glMultiTexParameteri(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0xDEADu);
    cb_err("invalid_wrap_value");
    glMultiTexParameterf(GL_TEXTURE1, 0xDEADu, GL_TEXTURE_MIN_LOD, 1.f);
    cb_err("invalid_parameter_target");
    glMultiTexImage2D(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_RGBA8, -1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      texels);
    cb_err("invalid_image_width");
    glGetMultiTexLevelParameteriv(GL_TEXTURE1, GL_TEXTURE_2D, -1, GL_TEXTURE_WIDTH, &i);
    cb_err("invalid_level");
    glGetMultiTexParameterfv(0xDEADu, GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, &f);
    cb_err("invalid_query_unit");
    glGetMultiTexLevelParameteriv(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &i);
    cb_i("width_after_rejection", i);
    {
        GLuint list = glGenLists(1);
        glNewList(list, GL_COMPILE);
        glBegin(GL_POINTS);
        glMultiTexParameteri(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glEnd();
        glEndList();
        cb_err("list_compile_param");
        glCallList(list);
        cb_err("list_replay_param");
        glDeleteLists(list, 1);
    }
    glActiveTexture(GL_TEXTURE0);
    glDeleteTextures(4, t);
    cb_err("finish");
}
#ifdef MGL_MESA_REFERENCE_EXT_ALIASES
#undef glMultiTexParameteri
#undef glMultiTexParameteriv
#undef glMultiTexParameterf
#undef glMultiTexParameterfv
#undef glGetMultiTexParameteriv
#undef glGetMultiTexParameterfv
#undef glMultiTexImage2D
#undef glMultiTexSubImage2D
#undef glMultiTexImage3D
#undef glMultiTexSubImage3D
#undef glCompressedMultiTexImage2D
#undef glCompressedMultiTexSubImage2D
#undef glCompressedMultiTexImage3D
#undef glCompressedMultiTexSubImage3D
#undef glCopyMultiTexImage2D
#undef glCopyMultiTexSubImage2D
#undef glCopyMultiTexSubImage3D
#undef glGetMultiTexImage
#undef glGetCompressedMultiTexImage
#undef glGetMultiTexLevelParameteriv
#undef glGetMultiTexLevelParameterfv
#undef glGetBooleanIndexedv
#undef glGetIntegerIndexedv
#undef glGetFloatIndexedv
#undef glGetDoubleIndexedv
#endif
#endif

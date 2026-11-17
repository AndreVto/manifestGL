#ifndef MGL_TEXTURE_DSA_PARAMETER_BATCH_TRACE_H
#define MGL_TEXTURE_DSA_PARAMETER_BATCH_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void bs_error(const char *label) {
    printf("bs_%s 0x%x\n", label, (unsigned)glGetError());
}

static void bs_iv(const char *label, GLint value) {
    printf("bs_%s %d 0x%x\n", label, (int)value, (unsigned)glGetError());
}

static void bs_fv(const char *label, GLfloat value) {
    printf("bs_%s %.3f 0x%x\n", label, (double)value, (unsigned)glGetError());
}

static void bs_uv(const char *label, GLuint value) {
    printf("bs_%s %u 0x%x\n", label, (unsigned)value, (unsigned)glGetError());
}

static void texture_dsa_parameter_batch_trace(void) {
    GLuint tex = 0, tex_other = 0, tex3 = 0, comp2 = 0, comp3 = 0;
    const GLubyte compressed_block[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    const GLubyte pixel[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    GLint vi[4] = {GL_CLAMP_TO_EDGE, 0, 0, 0}, borderi[4] = {0, 0, 0, 0},
          oi[4] = {-77, -77, -77, -77};
    GLfloat vf[4] = {2.0f, 0, 0, 0}, of[4] = {-77, -77, -77, -77};
    GLuint vu[4] = {0u, 0u, 0u, 0u}, ou[4] = {0};
    glActiveTexture(GL_TEXTURE0);
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glCreateTextures(GL_TEXTURE_2D, 1, &tex_other);
    glCreateTextures(GL_TEXTURE_3D, 1, &tex3);
    glCreateTextures(GL_TEXTURE_2D, 1, &comp2);
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &comp3);
    glBindTexture(GL_TEXTURE_2D, tex_other);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindTexture(GL_TEXTURE_3D, tex3);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, comp2);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D_ARRAY, comp3);
    glActiveTexture(GL_TEXTURE0);
    (void)glGetError();
    glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    bs_error("glTextureParameteri_valid");
    glBegin(GL_POINTS);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glEnd();
    bs_error("glTextureParameteri_begin");
    glTextureParameteriv(tex, GL_TEXTURE_WRAP_S, vi);
    bs_error("glTextureParameteriv_valid");
    glBegin(GL_POINTS);
    glTextureParameteriv(tex, GL_TEXTURE_WRAP_S, vi);
    glEnd();
    bs_error("glTextureParameteriv_begin");
    glTextureParameterf(tex, GL_TEXTURE_MIN_LOD, 2.0f);
    bs_error("glTextureParameterf_valid");
    glBegin(GL_POINTS);
    glTextureParameterf(tex, GL_TEXTURE_MIN_LOD, 2.0f);
    glEnd();
    bs_error("glTextureParameterf_begin");
    glTextureParameterfv(tex, GL_TEXTURE_MIN_LOD, vf);
    bs_error("glTextureParameterfv_valid");
    glBegin(GL_POINTS);
    glTextureParameterfv(tex, GL_TEXTURE_MIN_LOD, vf);
    glEnd();
    bs_error("glTextureParameterfv_begin");
    glTextureParameterIiv(tex, GL_TEXTURE_BORDER_COLOR, borderi);
    bs_error("glTextureParameterIiv_valid");
    glBegin(GL_POINTS);
    glTextureParameterIiv(tex, GL_TEXTURE_BORDER_COLOR, borderi);
    glEnd();
    bs_error("glTextureParameterIiv_begin");
    glTextureParameterIuiv(tex, GL_TEXTURE_BORDER_COLOR, vu);
    bs_error("glTextureParameterIuiv_valid");
    glBegin(GL_POINTS);
    glTextureParameterIuiv(tex, GL_TEXTURE_BORDER_COLOR, vu);
    glEnd();
    bs_error("glTextureParameterIuiv_begin");
    oi[0] = 0;
    glGetTextureParameteriv(tex, GL_TEXTURE_WRAP_S, oi);
    bs_iv("glGetTextureParameteriv_valid", oi[0]);
    glBegin(GL_POINTS);
    glGetTextureParameteriv(tex, GL_TEXTURE_WRAP_S, oi);
    glEnd();
    bs_error("glGetTextureParameteriv_begin");
    of[0] = 0;
    glGetTextureParameterfv(tex, GL_TEXTURE_MIN_LOD, of);
    bs_fv("glGetTextureParameterfv_valid", of[0]);
    glBegin(GL_POINTS);
    glGetTextureParameterfv(tex, GL_TEXTURE_MIN_LOD, of);
    glEnd();
    bs_error("glGetTextureParameterfv_begin");
    oi[0] = 0;
    glGetTextureParameterIiv(tex, GL_TEXTURE_BORDER_COLOR, oi);
    bs_iv("glGetTextureParameterIiv_valid", oi[0]);
    glBegin(GL_POINTS);
    glGetTextureParameterIiv(tex, GL_TEXTURE_BORDER_COLOR, oi);
    glEnd();
    bs_error("glGetTextureParameterIiv_begin");
    ou[0] = 0;
    glGetTextureParameterIuiv(tex, GL_TEXTURE_BORDER_COLOR, ou);
    bs_uv("glGetTextureParameterIuiv_valid", ou[0]);
    glBegin(GL_POINTS);
    glGetTextureParameterIuiv(tex, GL_TEXTURE_BORDER_COLOR, ou);
    glEnd();
    bs_error("glGetTextureParameterIuiv_begin");
    glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    bs_error("glTextureParameteriEXT_valid");
    glBegin(GL_POINTS);
    glTextureParameteriEXT(tex, 0xDEADu, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glEnd();
    bs_error("glTextureParameteriEXT_begin");
    glTextureParameterivEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, vi);
    bs_error("glTextureParameterivEXT_valid");
    glBegin(GL_POINTS);
    glTextureParameterivEXT(tex, 0xDEADu, GL_TEXTURE_WRAP_S, vi);
    glEnd();
    bs_error("glTextureParameterivEXT_begin");
    glTextureParameterfEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 2.0f);
    bs_error("glTextureParameterfEXT_valid");
    glBegin(GL_POINTS);
    glTextureParameterfEXT(tex, 0xDEADu, GL_TEXTURE_MIN_LOD, 2.0f);
    glEnd();
    bs_error("glTextureParameterfEXT_begin");
    glTextureParameterfvEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, vf);
    bs_error("glTextureParameterfvEXT_valid");
    glBegin(GL_POINTS);
    glTextureParameterfvEXT(tex, 0xDEADu, GL_TEXTURE_MIN_LOD, vf);
    glEnd();
    bs_error("glTextureParameterfvEXT_begin");
    glTextureParameterIivEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderi);
    bs_error("glTextureParameterIivEXT_valid");
    glBegin(GL_POINTS);
    glTextureParameterIivEXT(tex, 0xDEADu, GL_TEXTURE_BORDER_COLOR, borderi);
    glEnd();
    bs_error("glTextureParameterIivEXT_begin");
    glTextureParameterIuivEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, vu);
    bs_error("glTextureParameterIuivEXT_valid");
    glBegin(GL_POINTS);
    glTextureParameterIuivEXT(tex, 0xDEADu, GL_TEXTURE_BORDER_COLOR, vu);
    glEnd();
    bs_error("glTextureParameterIuivEXT_begin");
    oi[0] = 0;
    glGetTextureParameterivEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, oi);
    bs_iv("glGetTextureParameterivEXT_valid", oi[0]);
    glBegin(GL_POINTS);
    glGetTextureParameterivEXT(tex, 0xDEADu, GL_TEXTURE_WRAP_S, oi);
    glEnd();
    bs_error("glGetTextureParameterivEXT_begin");
    of[0] = 0;
    glGetTextureParameterfvEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, of);
    bs_fv("glGetTextureParameterfvEXT_valid", of[0]);
    glBegin(GL_POINTS);
    glGetTextureParameterfvEXT(tex, 0xDEADu, GL_TEXTURE_MIN_LOD, of);
    glEnd();
    bs_error("glGetTextureParameterfvEXT_begin");
    oi[0] = 0;
    glGetTextureParameterIivEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, oi);
    bs_iv("glGetTextureParameterIivEXT_valid", oi[0]);
    glBegin(GL_POINTS);
    glGetTextureParameterIivEXT(tex, 0xDEADu, GL_TEXTURE_BORDER_COLOR, oi);
    glEnd();
    bs_error("glGetTextureParameterIivEXT_begin");
    ou[0] = 0;
    glGetTextureParameterIuivEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, ou);
    bs_uv("glGetTextureParameterIuivEXT_valid", ou[0]);
    glBegin(GL_POINTS);
    glGetTextureParameterIuivEXT(tex, 0xDEADu, GL_TEXTURE_BORDER_COLOR, ou);
    glEnd();
    bs_error("glGetTextureParameterIuivEXT_begin");
    glTextureImage2DEXT(tex, GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    bs_error("glTextureImage2DEXT_valid");
    glBegin(GL_POINTS);
    glTextureImage2DEXT(tex, GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glEnd();
    bs_error("glTextureImage2DEXT_begin");
    glTextureSubImage2DEXT(tex, GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    bs_error("glTextureSubImage2DEXT_valid");
    glBegin(GL_POINTS);
    glTextureSubImage2DEXT(tex, GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glEnd();
    bs_error("glTextureSubImage2DEXT_begin");
    glTextureImage3DEXT(tex3, GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        pixel);
    bs_error("glTextureImage3DEXT_valid");
    glBegin(GL_POINTS);
    glTextureImage3DEXT(tex3, GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        pixel);
    glEnd();
    bs_error("glTextureImage3DEXT_begin");
    glTextureSubImage3DEXT(tex3, GL_TEXTURE_3D, 0, 0, 0, 0, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                           pixel);
    bs_error("glTextureSubImage3DEXT_valid");
    glBegin(GL_POINTS);
    glTextureSubImage3DEXT(tex3, GL_TEXTURE_3D, 0, 0, 0, 0, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                           pixel);
    glEnd();
    bs_error("glTextureSubImage3DEXT_begin");
    glMultiTexImage2DEXT(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, pixel);
    bs_error("glMultiTexImage2DEXT_valid");
    glBegin(GL_POINTS);
    glMultiTexImage2DEXT(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, pixel);
    glEnd();
    bs_error("glMultiTexImage2DEXT_begin");
    glMultiTexSubImage2DEXT(GL_TEXTURE1, GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                            pixel);
    bs_error("glMultiTexSubImage2DEXT_valid");
    glBegin(GL_POINTS);
    glMultiTexSubImage2DEXT(GL_TEXTURE1, GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                            pixel);
    glEnd();
    bs_error("glMultiTexSubImage2DEXT_begin");
    glMultiTexImage3DEXT(GL_TEXTURE1, GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 1, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, pixel);
    bs_error("glMultiTexImage3DEXT_valid");
    glBegin(GL_POINTS);
    glMultiTexImage3DEXT(GL_TEXTURE1, GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 1, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, pixel);
    glEnd();
    bs_error("glMultiTexImage3DEXT_begin");
    glMultiTexSubImage3DEXT(GL_TEXTURE1, GL_TEXTURE_3D, 0, 0, 0, 0, 2, 2, 1, GL_RGBA,
                            GL_UNSIGNED_BYTE, pixel);
    bs_error("glMultiTexSubImage3DEXT_valid");
    glBegin(GL_POINTS);
    glMultiTexSubImage3DEXT(GL_TEXTURE1, GL_TEXTURE_3D, 0, 0, 0, 0, 2, 2, 1, GL_RGBA,
                            GL_UNSIGNED_BYTE, pixel);
    glEnd();
    bs_error("glMultiTexSubImage3DEXT_begin");
    {
        GLint width = -1;
        glGetTextureLevelParameteriv(tex, 0, GL_TEXTURE_WIDTH, &width);
        bs_iv("tex2_width", width);
    }
    {
        GLint width = -1;
        glGetTextureLevelParameteriv(tex3, 0, GL_TEXTURE_WIDTH, &width);
        bs_iv("tex3_width", width);
    }
    glCompressedTextureImage2DEXT(comp2, GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8,
                                  compressed_block);
    bs_error("glCompressedTextureImage2DEXT_valid");
    glBegin(GL_POINTS);
    glCompressedTextureImage2DEXT(comp2, GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8,
                                  compressed_block);
    glEnd();
    bs_error("glCompressedTextureImage2DEXT_begin");
    glCompressedTextureSubImage2DEXT(comp2, GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_COMPRESSED_RED_RGTC1,
                                     8, compressed_block);
    bs_error("glCompressedTextureSubImage2DEXT_valid");
    glBegin(GL_POINTS);
    glCompressedTextureSubImage2DEXT(comp2, GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_COMPRESSED_RED_RGTC1,
                                     8, compressed_block);
    glEnd();
    bs_error("glCompressedTextureSubImage2DEXT_begin");
    glCompressedTextureImage3DEXT(comp3, GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 1,
                                  0, 8, compressed_block);
    bs_error("glCompressedTextureImage3DEXT_valid");
    glBegin(GL_POINTS);
    glCompressedTextureImage3DEXT(comp3, GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 1,
                                  0, 8, compressed_block);
    glEnd();
    bs_error("glCompressedTextureImage3DEXT_begin");
    glCompressedTextureSubImage3DEXT(comp3, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 4, 4, 1,
                                     GL_COMPRESSED_RED_RGTC1, 8, compressed_block);
    bs_error("glCompressedTextureSubImage3DEXT_valid");
    glBegin(GL_POINTS);
    glCompressedTextureSubImage3DEXT(comp3, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 4, 4, 1,
                                     GL_COMPRESSED_RED_RGTC1, 8, compressed_block);
    glEnd();
    bs_error("glCompressedTextureSubImage3DEXT_begin");
    glCompressedMultiTexImage2DEXT(GL_TEXTURE2, GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0,
                                   8, compressed_block);
    bs_error("glCompressedMultiTexImage2DEXT_valid");
    glBegin(GL_POINTS);
    glCompressedMultiTexImage2DEXT(GL_TEXTURE2, GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0,
                                   8, compressed_block);
    glEnd();
    bs_error("glCompressedMultiTexImage2DEXT_begin");
    glCompressedMultiTexSubImage2DEXT(GL_TEXTURE2, GL_TEXTURE_2D, 0, 0, 0, 4, 4,
                                      GL_COMPRESSED_RED_RGTC1, 8, compressed_block);
    bs_error("glCompressedMultiTexSubImage2DEXT_valid");
    glBegin(GL_POINTS);
    glCompressedMultiTexSubImage2DEXT(GL_TEXTURE2, GL_TEXTURE_2D, 0, 0, 0, 4, 4,
                                      GL_COMPRESSED_RED_RGTC1, 8, compressed_block);
    glEnd();
    bs_error("glCompressedMultiTexSubImage2DEXT_begin");
    glCompressedMultiTexImage3DEXT(GL_TEXTURE3, GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 4,
                                   4, 1, 0, 8, compressed_block);
    bs_error("glCompressedMultiTexImage3DEXT_valid");
    glBegin(GL_POINTS);
    glCompressedMultiTexImage3DEXT(GL_TEXTURE3, GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 4,
                                   4, 1, 0, 8, compressed_block);
    glEnd();
    bs_error("glCompressedMultiTexImage3DEXT_begin");
    glCompressedMultiTexSubImage3DEXT(GL_TEXTURE3, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 4, 4, 1,
                                      GL_COMPRESSED_RED_RGTC1, 8, compressed_block);
    bs_error("glCompressedMultiTexSubImage3DEXT_valid");
    glBegin(GL_POINTS);
    glCompressedMultiTexSubImage3DEXT(GL_TEXTURE3, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 4, 4, 1,
                                      GL_COMPRESSED_RED_RGTC1, 8, compressed_block);
    glEnd();
    bs_error("glCompressedMultiTexSubImage3DEXT_begin");
    glBegin(GL_POINTS);
    glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glEnd();
    bs_error("ext_begin_valid_target");
    {
        GLint v = -1;
        glGetTextureParameteriv(tex, GL_TEXTURE_WRAP_S, &v);
        bs_iv("ext_begin_preserves_wrap", v);
    }
    {
        GLint active = -1, bound = -1, wrap = -1;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap);
        printf("bs_preserve_binding %d %d %d 0x%x\n", active == GL_TEXTURE0, bound == tex_other,
               wrap == GL_REPEAT, (unsigned)glGetError());
    }
    glDeleteTextures(1, &tex);
    glDeleteTextures(1, &tex_other);
    glDeleteTextures(1, &tex3);
    glDeleteTextures(1, &comp2);
    glDeleteTextures(1, &comp3);
    (void)glGetError();
}
#endif

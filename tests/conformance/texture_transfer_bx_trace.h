#ifndef MGL_TEXTURE_TRANSFER_BX_TRACE_H
#define MGL_TEXTURE_TRANSFER_BX_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>
#include <string.h>

static void bx_error(const char *id) {
    printf("bx_%s 0x%04x\n", id, (unsigned)glGetError());
}

static void bx_value(const char *id, GLint value) {
    printf("bx_%s %d 0x%04x\n", id, value, (unsigned)glGetError());
}

static void texture_transfer_bx_trace(void) {
    GLuint tex[5] = {0};
    const GLenum u = GL_TEXTURE1;
    GLubyte pixels[4 * 4 * 4], subs[4 * 4 * 4], result[4 * 4 * 4], rgtc[16], rgsub[16];
    GLint iv[4] = {0};
    GLfloat fv[4] = {0};
    unsigned i;
    for (i = 0; i < sizeof(pixels); ++i) {
        pixels[i] = (GLubyte)(i + 3u);
        subs[i] = (GLubyte)(100u + i);
        result[i] = 0;
    }
    for (i = 0; i < sizeof(rgtc); ++i) {
        rgtc[i] = 0;
        rgsub[i] = 0;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelTransferf(GL_RED_SCALE, 1.f);
    glPixelTransferf(GL_GREEN_SCALE, 1.f);
    glPixelTransferf(GL_BLUE_SCALE, 1.f);
    glPixelTransferf(GL_ALPHA_SCALE, 1.f);
    glPixelTransferf(GL_RED_BIAS, 0.f);
    glPixelTransferf(GL_GREEN_BIAS, 0.f);
    glPixelTransferf(GL_BLUE_BIAS, 0.f);
    glPixelTransferf(GL_ALPHA_BIAS, 0.f);
    glGenTextures(5, tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glBindTexture(GL_TEXTURE_3D, tex[1]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex[2]);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex[3]);
    glActiveTexture(GL_TEXTURE0);
    (void)glGetError();
    glMultiTexImage2DEXT(u, GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    bx_error("multitex_image2_legal");
    glBegin(GL_POINTS);
    glMultiTexImage2DEXT(0xDEADu, GL_TEXTURE_2D, -1, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         pixels);
    glEnd();
    bx_error("multitex_image2_begin_bad");
    glMultiTexSubImage2DEXT(u, GL_TEXTURE_2D, 0, 1, 1, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, subs);
    bx_error("multitex_sub2_legal");
    glBegin(GL_POINTS);
    glMultiTexSubImage2DEXT(0xDEADu, GL_TEXTURE_2D, -1, -1, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                            subs);
    glEnd();
    bx_error("multitex_sub2_begin_bad");
    glMultiTexImage3DEXT(u, GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         pixels);
    bx_error("multitex_image3_legal");
    glBegin(GL_POINTS);
    glMultiTexImage3DEXT(0xDEADu, GL_TEXTURE_3D, -1, GL_RGBA8, 2, 2, 2, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, pixels);
    glEnd();
    bx_error("multitex_image3_begin_bad");
    glMultiTexSubImage3DEXT(u, GL_TEXTURE_3D, 0, 0, 0, 0, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, subs);
    bx_error("multitex_sub3_legal");
    glBegin(GL_POINTS);
    glMultiTexSubImage3DEXT(0xDEADu, GL_TEXTURE_3D, -1, 0, 0, 0, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                            subs);
    glEnd();
    bx_error("multitex_sub3_begin_bad");
    glTextureImage2DEXT(tex[0], GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        pixels);
    bx_error("named_image2_legal");
    glBegin(GL_POINTS);
    glTextureImage2DEXT(tex[0], 0xDEADu, -1, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glEnd();
    bx_error("named_image2_begin_bad");
    glTextureSubImage2DEXT(tex[0], GL_TEXTURE_2D, 0, 1, 1, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, subs);
    bx_error("named_sub2_legal");
    glBegin(GL_POINTS);
    glTextureSubImage2DEXT(tex[0], 0xDEADu, -1, -1, 1, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, subs);
    glEnd();
    bx_error("named_sub2_begin_bad");
    glTextureImage3DEXT(tex[1], GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        pixels);
    bx_error("named_image3_legal");
    glBegin(GL_POINTS);
    glTextureImage3DEXT(tex[1], 0xDEADu, -1, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        pixels);
    glEnd();
    bx_error("named_image3_begin_bad");
    glTextureSubImage3DEXT(tex[1], GL_TEXTURE_3D, 0, 0, 0, 1, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                           subs);
    bx_error("named_sub3_legal");
    glBegin(GL_POINTS);
    glTextureSubImage3DEXT(tex[1], 0xDEADu, -1, 0, 0, 1, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, subs);
    glEnd();
    bx_error("named_sub3_begin_bad");
    glCompressedMultiTexImage2DEXT(GL_TEXTURE2, GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0,
                                   8, rgtc);
    bx_error("multitex_compressed2_legal");
    glBegin(GL_POINTS);
    glCompressedMultiTexImage2DEXT(0xDEADu, GL_TEXTURE_2D, -1, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8,
                                   rgtc);
    glEnd();
    bx_error("multitex_compressed2_begin_bad");
    glCompressedMultiTexSubImage2DEXT(GL_TEXTURE2, GL_TEXTURE_2D, 0, 0, 0, 4, 4,
                                      GL_COMPRESSED_RED_RGTC1, 8, rgsub);
    bx_error("multitex_compressed_sub2_legal");
    glBegin(GL_POINTS);
    glCompressedMultiTexSubImage2DEXT(0xDEADu, GL_TEXTURE_2D, -1, 0, 0, 4, 4,
                                      GL_COMPRESSED_RED_RGTC1, 8, rgsub);
    glEnd();
    bx_error("multitex_compressed_sub2_begin_bad");
    glCompressedMultiTexImage3DEXT(GL_TEXTURE2, GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 4,
                                   4, 2, 0, 16, rgtc);
    bx_error("multitex_compressed3_legal");
    glBegin(GL_POINTS);
    glCompressedMultiTexImage3DEXT(0xDEADu, GL_TEXTURE_3D, -1, GL_COMPRESSED_RED_RGTC1, 4, 4, 2, 0,
                                   16, rgtc);
    glEnd();
    bx_error("multitex_compressed3_begin_bad");
    glCompressedMultiTexSubImage3DEXT(GL_TEXTURE2, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 4, 4, 1,
                                      GL_COMPRESSED_RED_RGTC1, 8, rgsub);
    bx_error("multitex_compressed_sub3_legal");
    glBegin(GL_POINTS);
    glCompressedMultiTexSubImage3DEXT(0xDEADu, GL_TEXTURE_3D, -1, 0, 0, 0, 4, 4, 1,
                                      GL_COMPRESSED_RED_RGTC1, 8, rgsub);
    glEnd();
    bx_error("multitex_compressed_sub3_begin_bad");
    glCompressedTextureImage2DEXT(tex[4], GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8,
                                  rgtc);
    bx_error("named_compressed2_legal");
    glBegin(GL_POINTS);
    glCompressedTextureImage2DEXT(tex[4], 0xDEADu, -1, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8, rgtc);
    glEnd();
    bx_error("named_compressed2_begin_bad");
    glCompressedTextureSubImage2DEXT(tex[4], GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_COMPRESSED_RED_RGTC1,
                                     8, rgsub);
    bx_error("named_compressed_sub2_legal");
    glBegin(GL_POINTS);
    glCompressedTextureSubImage2DEXT(tex[4], 0xDEADu, -1, 0, 0, 4, 4, GL_COMPRESSED_RED_RGTC1, 8,
                                     rgsub);
    glEnd();
    bx_error("named_compressed_sub2_begin_bad");
    glBegin(GL_POINTS);
    glCopyMultiTexImage2DEXT(0xDEADu, 0xDEADu, -1, GL_RGBA8, 0, 0, -1, -1, 1);
    glEnd();
    bx_error("copy_multitex_image2_begin_bad");
    glBegin(GL_POINTS);
    glCopyMultiTexSubImage2DEXT(0xDEADu, 0xDEADu, -1, -1, 0, 0, 0, -1, -1);
    glEnd();
    bx_error("copy_multitex_sub2_begin_bad");
    glBegin(GL_POINTS);
    glCopyMultiTexSubImage3DEXT(0xDEADu, 0xDEADu, -1, -1, 0, 0, 0, 0, -1, -1);
    glEnd();
    bx_error("copy_multitex_sub3_begin_bad");
    glBegin(GL_POINTS);
    glCopyTextureImage2DEXT(tex[0], 0xDEADu, -1, GL_RGBA8, 0, 0, -1, -1, 1);
    glEnd();
    bx_error("copy_named_image2_begin_bad");
    glBegin(GL_POINTS);
    glCopyTextureSubImage2DEXT(tex[0], 0xDEADu, -1, -1, -1, 0, 0, -1, -1);
    glEnd();
    bx_error("copy_named_sub2_begin_bad");
    glBegin(GL_POINTS);
    glCopyTextureSubImage3DEXT(tex[1], 0xDEADu, -1, -1, -1, -1, 0, 0, -1, -1);
    glEnd();
    bx_error("copy_named_sub3_begin_bad");
    glGenerateTextureMipmapEXT(tex[0], GL_TEXTURE_2D);
    bx_error("mipmap_named_legal");
    glBegin(GL_POINTS);
    glGenerateTextureMipmapEXT(tex[0], 0xDEADu);
    glEnd();
    bx_error("mipmap_named_begin_bad");
    glGenerateMultiTexMipmapEXT(u, GL_TEXTURE_2D);
    bx_error("mipmap_multi_legal");
    glBegin(GL_POINTS);
    glGenerateMultiTexMipmapEXT(0xDEADu, 0xDEADu);
    glEnd();
    bx_error("mipmap_multi_begin_bad");
    memset(result, 0, sizeof(result));
    iv[0] = -991;
    fv[0] = -991.f;
    glGetMultiTexImageEXT(u, GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, result);
    bx_error("get_multi_image_legal");
    bx_value("get_multi_image_data", result[0]);
    glBegin(GL_POINTS);
    glGetMultiTexImageEXT(0xDEADu, GL_TEXTURE_2D, -1, GL_RGBA, GL_UNSIGNED_BYTE, result);
    glEnd();
    bx_error("get_multi_image_begin_bad");
    memset(result, 0, sizeof(result));
    iv[0] = -991;
    fv[0] = -991.f;
    glGetMultiTexLevelParameterivEXT(u, GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, iv);
    bx_error("get_multi_width_iv_legal");
    bx_value("get_multi_width_iv_value", iv[0]);
    glBegin(GL_POINTS);
    glGetMultiTexLevelParameterivEXT(0xDEADu, 0xDEADu, -1, GL_TEXTURE_WIDTH, iv);
    glEnd();
    bx_error("get_multi_width_iv_begin_bad");
    memset(result, 0, sizeof(result));
    iv[0] = -991;
    fv[0] = -991.f;
    glGetMultiTexLevelParameterfvEXT(u, GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, fv);
    bx_error("get_multi_width_fv_legal");
    bx_value("get_multi_width_fv_value", (GLint)fv[0]);
    glBegin(GL_POINTS);
    glGetMultiTexLevelParameterfvEXT(0xDEADu, 0xDEADu, -1, GL_TEXTURE_WIDTH, fv);
    glEnd();
    bx_error("get_multi_width_fv_begin_bad");
    memset(result, 0, sizeof(result));
    iv[0] = -991;
    fv[0] = -991.f;
    glGetCompressedMultiTexImageEXT(GL_TEXTURE2, GL_TEXTURE_2D, 0, result);
    bx_error("get_multi_compressed_legal");
    bx_value("get_multi_compressed_data", result[0]);
    glBegin(GL_POINTS);
    glGetCompressedMultiTexImageEXT(0xDEADu, 0xDEADu, -1, result);
    glEnd();
    bx_error("get_multi_compressed_begin_bad");
    memset(result, 0, sizeof(result));
    iv[0] = -991;
    fv[0] = -991.f;
    glGetTextureImageEXT(tex[0], GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, result);
    bx_error("get_named_image_legal");
    bx_value("get_named_image_data", result[0]);
    glBegin(GL_POINTS);
    glGetTextureImageEXT(tex[0], 0xDEADu, -1, GL_RGBA, GL_UNSIGNED_BYTE, result);
    glEnd();
    bx_error("get_named_image_begin_bad");
    memset(result, 0, sizeof(result));
    iv[0] = -991;
    fv[0] = -991.f;
    glGetCompressedTextureImageEXT(tex[4], GL_TEXTURE_2D, 0, result);
    bx_error("get_named_compressed_legal");
    bx_value("get_named_compressed_data", result[0]);
    glBegin(GL_POINTS);
    glGetCompressedTextureImageEXT(tex[4], 0xDEADu, -1, result);
    glEnd();
    bx_error("get_named_compressed_begin_bad");
    memset(result, 0, sizeof(result));
    iv[0] = -991;
    fv[0] = -991.f;
    glGetTextureLevelParameterivEXT(tex[0], GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, iv);
    bx_error("get_named_width_iv_legal");
    bx_value("get_named_width_iv_value", iv[0]);
    glBegin(GL_POINTS);
    glGetTextureLevelParameterivEXT(tex[0], 0xDEADu, -1, GL_TEXTURE_WIDTH, iv);
    glEnd();
    bx_error("get_named_width_iv_begin_bad");
    memset(result, 0, sizeof(result));
    iv[0] = -991;
    fv[0] = -991.f;
    glGetTextureLevelParameterfvEXT(tex[0], GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, fv);
    bx_error("get_named_width_fv_legal");
    bx_value("get_named_width_fv_value", (GLint)fv[0]);
    glBegin(GL_POINTS);
    glGetTextureLevelParameterfvEXT(tex[0], 0xDEADu, -1, GL_TEXTURE_WIDTH, fv);
    glEnd();
    bx_error("get_named_width_fv_begin_bad");
    glActiveTexture(GL_TEXTURE0);
    iv[0] = -1;
    glGetIntegerv(GL_ACTIVE_TEXTURE, iv);
    bx_value("active_unit_restored", iv[0] == (GLint)GL_TEXTURE0);
    glActiveTexture(GL_TEXTURE1);
    iv[0] = -1;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, iv);
    bx_value("tex1_width_preserved", iv[0]);
    glActiveTexture(GL_TEXTURE2);
    iv[0] = -1;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, iv);
    bx_value("tex2_width_preserved", iv[0]);
    glActiveTexture(GL_TEXTURE0);
    glDeleteTextures(5, tex);
    bx_error("cleanup");
}
#endif

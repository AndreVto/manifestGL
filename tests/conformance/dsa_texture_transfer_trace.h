#ifndef MGL_DSA_TEXTURE_TRANSFER_TRACE_H
#define MGL_DSA_TEXTURE_TRANSFER_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>
#include <string.h>

static void nt_error(const char *name) {
    printf("nt_%s 0x%04x\n", name, (unsigned)glGetError());
}

static void nt_level(const char *name, GLuint tex, GLint level) {
    GLint w = -91;
    glGetTextureLevelParameteriv(tex, level, GL_TEXTURE_WIDTH, &w);
    printf("nt_%s %d 0x%04x\n", name, w, (unsigned)glGetError());
}

static void dsa_texture_transfer_trace(void) {
    GLuint tex2 = 0, tex3 = 0, compressed = 0;
    const GLubyte patch[16] = {12, 34, 56, 255, 78, 90, 111, 255, 1, 2, 3, 255, 4, 5, 6, 255};
    const GLubyte block[8] = {18, 71, 2, 0, 0, 0, 0, 0};
    GLint binding_before = -1, binding_after = -1;
    GLint size = -91;
    GLfloat fsize = -91.0f;
    GLubyte guard[64];
    glPixelTransferf(GL_RED_SCALE, 1.0f);
    glPixelTransferf(GL_GREEN_SCALE, 1.0f);
    glPixelTransferf(GL_BLUE_SCALE, 1.0f);
    glPixelTransferf(GL_ALPHA_SCALE, 1.0f);
    glPixelTransferf(GL_RED_BIAS, 0.0f);
    glPixelTransferf(GL_GREEN_BIAS, 0.0f);
    glPixelTransferf(GL_BLUE_BIAS, 0.0f);
    glPixelTransferf(GL_ALPHA_BIAS, 0.0f);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glCreateTextures(GL_TEXTURE_2D, 1, &tex2);
    glCreateTextures(GL_TEXTURE_3D, 1, &tex3);
    glCreateTextures(GL_TEXTURE_2D, 1, &compressed);
    nt_error("setup_create");
    glTextureStorage2D(tex2, 3, GL_RGBA8, 4, 4);
    nt_error("setup_2d_storage");
    glTextureStorage3D(tex3, 2, GL_RGBA8, 2, 2, 2);
    nt_error("setup_3d_storage");
    glBindTexture(GL_TEXTURE_2D, compressed);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8, block);
    nt_error("setup_compressed");
    glBindTexture(GL_TEXTURE_2D, 0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding_before);
    nt_error("setup_binding");

    glBegin(GL_POINTS);
    glTextureStorage2D(tex2, 2, GL_RGBA8, 2, 2);
    glEnd();
    nt_error("storage2_begin");
    glBegin(GL_POINTS);
    glTextureStorage3D(0, -1, 0, -1, -1, -1);
    glEnd();
    nt_error("storage3_precedence");

    glBegin(GL_POINTS);
    glTextureSubImage2D(tex2, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, patch);
    glEnd();
    nt_error("sub2_begin");
    glBegin(GL_POINTS);
    glTextureSubImage2D(0, -1, -1, -1, -1, -1, 0, 0, NULL);
    glEnd();
    nt_error("sub2_precedence");
    glBegin(GL_POINTS);
    glTextureSubImage3D(tex3, 0, 0, 0, 0, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, patch);
    glEnd();
    nt_error("sub3_begin");
    glBegin(GL_POINTS);
    glTextureSubImage3D(0, -1, -1, -1, -1, -1, -1, -1, 0, 0, NULL);
    glEnd();
    nt_error("sub3_precedence");

    glBegin(GL_POINTS);
    glCompressedTextureSubImage2D(compressed, 0, 0, 0, 4, 4, GL_COMPRESSED_RED_RGTC1, 8, block);
    glEnd();
    nt_error("compressed_sub2_begin");
    glBegin(GL_POINTS);
    glCompressedTextureSubImage3D(tex3, 0, 0, 0, 0, 4, 4, 1, GL_COMPRESSED_RED_RGTC1, 8, block);
    glEnd();
    nt_error("compressed_sub3_begin");
    glBegin(GL_POINTS);
    glCompressedTextureSubImage2D(0, -1, -1, -1, -1, -1, 0, -1, NULL);
    glEnd();
    nt_error("compressed_sub2_precedence");
    glBegin(GL_POINTS);
    glGenerateTextureMipmap(tex2);
    glEnd();
    nt_error("mipmap_begin");
    nt_level("mipmap_not_generated", tex2, 1);

    glBegin(GL_POINTS);
    glCopyTextureSubImage2D(tex2, 0, 0, 0, 0, 0, 1, 1);
    glEnd();
    nt_error("copy2_begin");
    glBegin(GL_POINTS);
    glCopyTextureSubImage3D(tex3, 0, 0, 0, 0, 0, 0, 1, 1);
    glEnd();
    nt_error("copy3_begin");

    memset(guard, 171, sizeof(guard));
    glBegin(GL_POINTS);
    glGetTextureImage(tex2, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLsizei)sizeof(guard), guard);
    glEnd();
    printf("nt_image_begin %u %u 0x%04x\n", (unsigned)guard[0], (unsigned)guard[63],
           (unsigned)glGetError());
    memset(guard, 171, sizeof(guard));
    glBegin(GL_POINTS);
    glGetCompressedTextureImage(compressed, 0, (GLsizei)sizeof(guard), guard);
    glEnd();
    printf("nt_compressed_begin %u %u 0x%04x\n", (unsigned)guard[0], (unsigned)guard[63],
           (unsigned)glGetError());
    glBegin(GL_POINTS);
    glGetTextureLevelParameteriv(tex2, 0, GL_TEXTURE_WIDTH, &size);
    glEnd();
    printf("nt_level_iv_begin %d 0x%04x\n", size, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glGetTextureLevelParameterfv(tex2, 0, GL_TEXTURE_WIDTH, &fsize);
    glEnd();
    printf("nt_level_fv_begin %d 0x%04x\n", (int)fsize, (unsigned)glGetError());

    glTextureSubImage2D(tex2, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, patch);
    nt_error("sub2_valid");
    glTextureSubImage3D(tex3, 0, 0, 0, 0, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, patch);
    nt_error("sub3_valid");
    memset(guard, 171, sizeof(guard));
    glGetTextureImage(tex2, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLsizei)sizeof(guard), guard);
    printf("nt_image_valid %u %u %u %u 0x%04x\n", (unsigned)guard[0], (unsigned)guard[1],
           (unsigned)guard[2], (unsigned)guard[3], (unsigned)glGetError());
    nt_level("level_valid", tex2, 0);
    glGetTextureLevelParameterfv(tex3, 0, GL_TEXTURE_DEPTH, &fsize);
    printf("nt_level3_fv_valid %d 0x%04x\n", (int)fsize, (unsigned)glGetError());
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding_after);
    printf("nt_binding_restored %d %d 0x%04x\n", binding_before, binding_after,
           (unsigned)glGetError());
    glDeleteTextures(1, &tex2);
    glDeleteTextures(1, &tex3);
    glDeleteTextures(1, &compressed);
    nt_error("cleanup");
}
#endif

#ifndef MGL_TEX_STORAGE_MIPMAP_READBACK_TRACE_H
#define MGL_TEX_STORAGE_MIPMAP_READBACK_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>
#include <string.h>

static void sm_error(const char *tag) {
    printf("sm_%s 0x%04x\n", tag, (unsigned)glGetError());
}

static void sm_level(const char *tag, GLenum target, GLint level) {
    GLint w = -33, h = -33, d = -33;
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &h);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_DEPTH, &d);
    printf("sm_%s %d %d %d 0x%04x\n", tag, w, h, d, (unsigned)glGetError());
}

static void sm_compressed(const char *tag) {
    GLubyte bytes[8];
    unsigned i;
    memset(bytes, 0xa5, sizeof(bytes));
    glGetCompressedTexImage(GL_TEXTURE_2D, 0, bytes);
    printf("sm_%s", tag);
    for (i = 0; i < 8; ++i)
        printf(" %u", (unsigned)bytes[i]);
    printf(" 0x%04x\n", (unsigned)glGetError());
}

static void tex_storage_mipmap_readback_trace(void) {
    static const GLubyte compressed[8] = {18, 71, 2, 0, 0, 0, 0, 0};
    static const GLubyte rgba[64] = {
        4,   8,   12,  255, 16,  20,  24,  255, 28,  32,  36,  255, 40,  44,  48,  255,
        52,  56,  60,  255, 64,  68,  72,  255, 76,  80,  84,  255, 88,  92,  96,  255,
        100, 104, 108, 255, 112, 116, 120, 255, 124, 128, 132, 255, 136, 140, 144, 255,
        148, 152, 156, 255, 160, 164, 168, 255, 172, 176, 180, 255, 184, 188, 192, 255};
    glActiveTexture(GL_TEXTURE0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    sm_error("setup");

    glBindTexture(GL_TEXTURE_2D, 9601);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8, compressed);
    sm_error("compressed_define");
    sm_compressed("compressed_read");
    {
        GLubyte guard[8] = {171, 171, 171, 171, 171, 171, 171, 171};
        glBegin(GL_POINTS);
        glGetCompressedTexImage(GL_TEXTURE_2D, 0, guard);
        glEnd();
        printf("sm_compressed_begin_guard %u %u 0x%04x\n", (unsigned)guard[0], (unsigned)guard[1],
               (unsigned)glGetError());
    }
    glBegin(GL_POINTS);
    glGetCompressedTexImage(0u, -1, NULL);
    glEnd();
    sm_error("compressed_begin_precedence");
    sm_compressed("compressed_after_begin");

    glBindTexture(GL_TEXTURE_2D, 9602);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    sm_error("mipmap_base");
    sm_level("mipmap_before", GL_TEXTURE_2D, 1);
    glBegin(GL_POINTS);
    glGenerateMipmap(GL_TEXTURE_2D);
    glEnd();
    sm_error("mipmap_begin");
    sm_level("mipmap_after_begin", GL_TEXTURE_2D, 1);
    glBegin(GL_POINTS);
    glGenerateMipmap(0u);
    glEnd();
    sm_error("mipmap_begin_precedence");
    glGenerateMipmap(GL_TEXTURE_2D);
    sm_error("mipmap_valid");
    sm_level("mipmap_after_valid", GL_TEXTURE_2D, 1);

    glBindTexture(GL_TEXTURE_2D, 9603);
    glBegin(GL_POINTS);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 4, 4);
    glEnd();
    sm_error("storage2_begin");
    sm_level("storage2_after_begin", GL_TEXTURE_2D, 0);
    glBegin(GL_POINTS);
    glTexStorage2D(0u, 0, 0u, -1, -1);
    glEnd();
    sm_error("storage2_begin_precedence");
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 4, 4);
    sm_error("storage2_valid");
    sm_level("storage2_level0", GL_TEXTURE_2D, 0);
    sm_level("storage2_level2", GL_TEXTURE_2D, 2);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 4, 4);
    sm_error("storage2_redefine");

    glBindTexture(GL_TEXTURE_3D, 9604);
    glBegin(GL_POINTS);
    glTexStorage3D(GL_TEXTURE_3D, 3, GL_RGBA8, 4, 4, 2);
    glEnd();
    sm_error("storage3_begin");
    sm_level("storage3_after_begin", GL_TEXTURE_3D, 0);
    glBegin(GL_POINTS);
    glTexStorage3D(0u, 0, 0u, -1, -1, -1);
    glEnd();
    sm_error("storage3_begin_precedence");
    glTexStorage3D(GL_TEXTURE_3D, 3, GL_RGBA8, 4, 4, 2);
    sm_error("storage3_valid");
    sm_level("storage3_level0", GL_TEXTURE_3D, 0);
    sm_level("storage3_level2", GL_TEXTURE_3D, 2);
    glTexStorage3D(GL_TEXTURE_3D, 3, GL_RGBA8, 4, 4, 2);
    sm_error("storage3_redefine");
}
#endif

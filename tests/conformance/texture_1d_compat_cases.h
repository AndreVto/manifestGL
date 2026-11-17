#ifndef TEXTURE_1D_COMPAT_TRACE_H
#define TEXTURE_1D_COMPAT_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void texture_1d_compat_trace(void) {
    GLuint a, b, fbo;
    GLint value;
    const unsigned char rgb[6] = {10, 20, 30, 40, 50, 60};
    const unsigned char bgr[3] = {1, 2, 3};
    unsigned char readback[6] = {0};
    glGenTextures(1, &a);
    glGenTextures(1, &b);
    glBindTexture(GL_TEXTURE_2D, b);
    glBindTexture(GL_TEXTURE_1D, a);
    printf("bind_1d %04x ", glGetError());
    glGetIntegerv(GL_TEXTURE_BINDING_1D, &value);
    printf("%d %04x\n", (int)(value == (GLint)a), glGetError());
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB8, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    printf("image_1d %04x\n", glGetError());
    glTexSubImage1D(GL_TEXTURE_1D, 0, 1, 1, GL_BGR, GL_UNSIGNED_BYTE, bgr);
    printf("sub_bgr %04x\n", glGetError());
    glGetTexImage(GL_TEXTURE_1D, 0, GL_RGB, GL_UNSIGNED_BYTE, readback);
    printf("readback %u,%u,%u,%u,%u,%u %04x\n", readback[0], readback[1], readback[2], readback[3],
           readback[4], readback[5], glGetError());
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    printf("param_1d %04x\n", glGetError());
    glGetTexLevelParameteriv(GL_TEXTURE_1D, 0, GL_TEXTURE_WIDTH, &value);
    printf("width %d %04x\n", value, glGetError());
    glGetTexLevelParameteriv(GL_TEXTURE_1D, 0, GL_TEXTURE_HEIGHT, &value);
    printf("height %d %04x\n", value, glGetError());
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
    printf("binding_2d %d %04x\n", (int)(value == (GLint)b), glGetError());
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture1D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_1D, a, 0);
    printf("attach_1d %04x\n", glGetError());
    glTexImage1D(GL_TEXTURE_2D, 0, GL_RGB8, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    printf("bad_target %04x\n", glGetError());
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB8, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    printf("bad_width %04x\n", glGetError());
    glBindTexture(GL_TEXTURE_1D, b);
    printf("bad_bind %04x\n", glGetError());
    glBindTexture(GL_TEXTURE_1D, a);
    glTexStorage1D(GL_TEXTURE_1D, 1, GL_RGBA8, 2);
    printf("storage_after_mutable %04x\n", glGetError());
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &a);
    glDeleteTextures(1, &b);
}
#endif

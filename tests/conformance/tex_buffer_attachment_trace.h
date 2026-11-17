#ifndef MGL_TEX_BUFFER_ATTACHMENT_TRACE_H
#define MGL_TEX_BUFFER_ATTACHMENT_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void tba_check(const char *step) {
    GLint binding = -17, width = -17, internal = -17;
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_DATA_STORE_BINDING, &binding);
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal);
    printf("tba_%s %d %d 0x%x 0x%x\n", step, binding != 0, width, (unsigned)internal,
           (unsigned)glGetError());
}

static void tba_error(const char *step) {
    printf("tba_%s 0x%x\n", step, (unsigned)glGetError());
}

static void tex_buffer_attachment_trace(void) {
    GLuint b = 0;
    GLuint t = 0;
    glGenBuffers(1, &b);
    glBindBuffer(GL_TEXTURE_BUFFER, b);
    glBufferData(GL_TEXTURE_BUFFER, 1024, NULL, GL_STATIC_DRAW);
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_BUFFER, t);
    tba_error("setup");
    tba_error("initial");
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, b);
    tba_check("attached_full");
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_R32UI, b, 0, 256);
    tba_check("attached_range");
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_R32UI, b, -1, 4);
    tba_check("invalid_range");
    glBegin(GL_POINTS);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, 0);
    glEnd();
    tba_check("illegal_full");
    glBegin(GL_POINTS);
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_R32UI, 0, 0, 0);
    glEnd();
    tba_check("illegal_range");
    glBegin(GL_POINTS);
    glTexBuffer(0, 0, 0);
    glEnd();
    tba_error("illegal_full_precedence");
    glBegin(GL_POINTS);
    glTexBufferRange(0, 0, 0, -1, -1);
    glEnd();
    tba_error("illegal_range_precedence");
    glTextureBuffer(t, GL_R32UI, b);
    tba_check("named_full");
    glTextureBufferRange(t, GL_R32UI, b, 0, 256);
    tba_check("named_range");
    glBegin(GL_POINTS);
    glTextureBuffer(t, GL_R32UI, 0);
    glEnd();
    tba_check("named_illegal_full");
    glBegin(GL_POINTS);
    glTextureBufferRange(t, GL_R32UI, 0, 0, 0);
    glEnd();
    tba_check("named_illegal_range");
    glBegin(GL_POINTS);
    glTextureBuffer(0, 0, 0);
    glEnd();
    tba_error("named_illegal_precedence");
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, 0);
    tba_check("detached");
    glBegin(GL_POINTS);
    glDeleteTextures(1, &t);
    glEnd();
    printf("tba_illegal_delete %u 0x%x\n", (unsigned)glIsTexture(t), (unsigned)glGetError());
    glBegin(GL_POINTS);
    glDeleteTextures(-1, NULL);
    glEnd();
    tba_error("illegal_delete_precedence");
    glDeleteTextures(1, &t);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);
    glDeleteBuffers(1, &b);
    tba_error("teardown");
}
#endif

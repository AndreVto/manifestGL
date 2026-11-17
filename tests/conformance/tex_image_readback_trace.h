#ifndef MGL_TEX_IMAGE_READBACK_TRACE_H
#define MGL_TEX_IMAGE_READBACK_TRACE_H

#include "mgl/gl.h"
#include <stdio.h>
#include <string.h>

static void tex_image_error_trace(const char *tag, GLenum target, GLint level, GLenum format,
                                  GLenum type) {
    GLubyte result[8] = {201, 202, 203, 204, 205, 206, 207, 208};
    glGetTexImage(target, level, format, type, result);
    /* The result of an erroneous query is not specified: compare the error only. */
    printf("image_%s 0x%04x\n", tag, (unsigned)glGetError());
}

static void tex_image_readback_trace(void) {
    const GLubyte initial[8] = {13, 34, 56, 78, 91, 123, 145, 167};
    GLubyte output[8] = {0};
    GLenum err;
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 8321);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, initial);
    printf("image_upload 0x%04x\n", (unsigned)glGetError());
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, output);
    err = glGetError();
    printf("image_valid %u %u %u %u %u %u %u %u 0x%04x\n", (unsigned)output[0], (unsigned)output[1],
           (unsigned)output[2], (unsigned)output[3], (unsigned)output[4], (unsigned)output[5],
           (unsigned)output[6], (unsigned)output[7], (unsigned)err);
    tex_image_error_trace("invalid_target", 0u, 0, GL_RGBA, GL_UNSIGNED_BYTE);
    tex_image_error_trace("negative_level", GL_TEXTURE_2D, -1, GL_RGBA, GL_UNSIGNED_BYTE);
    tex_image_error_trace("invalid_format", GL_TEXTURE_2D, 0, 0u, GL_UNSIGNED_BYTE);
    tex_image_error_trace("invalid_type", GL_TEXTURE_2D, 0, GL_RGBA, 0u);
    {
        GLubyte guard[8] = {201, 202, 203, 204, 205, 206, 207, 208};
        glBegin(GL_POINTS);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, guard);
        glEnd();
        printf("image_begin_valid %u %u 0x%04x\n", (unsigned)guard[0], (unsigned)guard[7],
               (unsigned)glGetError());
    }
    {
        GLubyte guard[8] = {201, 202, 203, 204, 205, 206, 207, 208};
        glBegin(GL_POINTS);
        glGetTexImage(0u, -1, 0u, 0u, guard);
        glEnd();
        printf("image_begin_invalid %u %u 0x%04x\n", (unsigned)guard[0], (unsigned)guard[7],
               (unsigned)glGetError());
    }
    memset(output, 0, sizeof(output));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, output);
    printf("image_after_begin %u %u 0x%04x\n", (unsigned)output[0], (unsigned)output[7],
           (unsigned)glGetError());
}

#endif

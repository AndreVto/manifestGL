#ifndef MGL_TEX_LEVEL_QUERY_TRACE_H
#define MGL_TEX_LEVEL_QUERY_TRACE_H

#include "mgl/gl.h"
#include <stdio.h>

static void level_query_iv(const char *tag, GLenum target, GLint level, GLenum pname) {
    GLint result = -31415;
    glGetTexLevelParameteriv(target, level, pname, &result);
    {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR)
            printf("level_%s %d 0x%04x\n", tag, result, (unsigned)error);
        else
            printf("level_%s error 0x%04x\n", tag, (unsigned)error);
    }
}

static void level_query_fv(const char *tag, GLenum target, GLint level, GLenum pname) {
    GLfloat result = -31415.0f;
    glGetTexLevelParameterfv(target, level, pname, &result);
    {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR)
            printf("level_%s %.3f 0x%04x\n", tag, result, (unsigned)error);
        else
            printf("level_%s error 0x%04x\n", tag, (unsigned)error);
    }
}

static void tex_level_query_trace(void) {
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 8123u);
    level_query_iv("undefined_width", GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    printf("level_image_error 0x%04x\n", (unsigned)glGetError());
    level_query_iv("width", GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH);
    level_query_iv("height", GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT);
    level_query_fv("width_float", GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH);
    level_query_iv("internal_format", GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT);
    level_query_iv("undefined_mip", GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH);
    level_query_iv("negative_level", GL_TEXTURE_2D, -1, GL_TEXTURE_WIDTH);
    level_query_fv("negative_level_float", GL_TEXTURE_2D, -1, GL_TEXTURE_HEIGHT);
    level_query_iv("bad_target", 0u, 0, GL_TEXTURE_WIDTH);
    level_query_fv("bad_pname", GL_TEXTURE_2D, 0, 0u);
    {
        GLint result = -31415;
        glBegin(GL_POINTS);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &result);
        glEnd();
        printf("level_begin_valid %d 0x%04x\n", result, (unsigned)glGetError());
    }
    {
        GLfloat result = -31415.0f;
        glBegin(GL_POINTS);
        glGetTexLevelParameterfv(0u, -1, 0u, &result);
        glEnd();
        printf("level_begin_invalid 0x%04x\n", (unsigned)glGetError());
    }
    level_query_iv("after_begin", GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH);
}

#endif

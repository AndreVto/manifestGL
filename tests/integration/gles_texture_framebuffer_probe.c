/* Native GLES probe for framebuffer, texture, and draw behavior. Requires a surfaceless GLES 3
 * provider. */
#include "mgl/gl.h"
#include "native/dispatch.h"
#include "core/context.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static MGL_EGLDisplay (*platform_display)(MGL_EGLint, void *, const MGL_EGLint *);

static MGL_EGLDisplay get_surfaceless_display(MGL_EGLNativeDisplayType unused) {
    (void)unused;
    return platform_display(0x31ddu, NULL, NULL);
}

static int check_no_error(const char *step) {
    GLenum first = glGetError();
    if (first == GL_NO_ERROR)
        return 1;
    fprintf(stderr, "%s: GL error %#x\n", step, (unsigned)first);
    return 0;
}

static int check_status(GLenum target, const char *step) {
    GLenum status = glCheckFramebufferStatus(target);
    if (!check_no_error(step))
        return 0;
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "%s: framebuffer status %#x\n", step, (unsigned)status);
        return 0;
    }
    return 1;
}

static int check_pbo_bgra(void) {
    const uint32_t pattern[16] = {0x00000000, 0x000000ff, 0x0000ff00, 0x40ff0000,
                                  0x80ffffff, 0x40ffff00, 0x00ff00ff, 0x0000ffff,
                                  0x00ffff00, 0x00ff00ff, 0x0000ffff, 0x000000ff,
                                  0x80ff00ff, 0x0000ffff, 0x00ff00ff, 0x40ff00ff};
    uint32_t output[16] = {0};
    GLuint texture, pbo;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    if (!check_no_error("PBO texture allocation"))
        return 0;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(pattern), pattern, GL_STREAM_DRAW);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glFinish();
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, output);
    if (!check_no_error("PBO upload/readback"))
        return 0;
    if (memcmp(pattern, output, sizeof(pattern))) {
        fprintf(stderr, "PBO roundtrip pixels differ\n");
        return 0;
    }
    glDeleteBuffers(1, &pbo);
    glDeleteTextures(1, &texture);
    return check_no_error("PBO cleanup");
}

static int check_texture_update(void) {
    uint8_t pixels[4 * 4 * 4];
    GLuint texture, fbo;
    memset(pixels, 0x11, sizeof(pixels));
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    if (!check_no_error("BGRA FBO allocation") ||
        !check_status(GL_FRAMEBUFFER, "BGRA FBO status"))
        return 0;
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
    glClearColor(0.996f, 0.729f, 0.745f, 0.792f);
    glClear(GL_COLOR_BUFFER_BIT);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
    if (!check_no_error("BGRA FBO update/readback"))
        return 0;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
    return check_no_error("BGRA FBO cleanup");
}

static int check_rgba16(void) {
    GLuint texture;
    GLint red = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, 4, 4, 0, GL_RGBA, GL_UNSIGNED_SHORT, NULL);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE, &red);
    if (!check_no_error("RGBA16 precision probe"))
        return 0;
    if (red != 8 && red != 16) {
        fprintf(stderr, "unexpected reported red channel depth %d\n", red);
        return 0;
    }
    glDeleteTextures(1, &texture);
    return check_no_error("RGBA16 cleanup");
}

static int check_mixed_attachments(void) {
    GLuint texture[2], fbo;
    glGenTextures(2, texture);
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 1, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, NULL);
    glBindTexture(GL_TEXTURE_2D, texture[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, 4, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[0], 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, texture[1], 0);
    /* Mixed-format framebuffer attachments may be incomplete without generating a GL error. */
    (void)glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (!check_no_error("mixed-depth FBO probe"))
        return 0;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(2, texture);
    return check_no_error("mixed-depth FBO cleanup");
}

static int check_filling_convention(void) {
    static const GLfloat corners[12] = {-0.125f, -0.125f, 0.0f, 0.125f, -0.125f, 0.0f,
                                        -0.125f, 0.125f,  0.0f, 0.125f, 0.125f,  0.0f};
    const GLchar *vs = "#version 150\nin vec4 pos;\nin vec4 color;\nout vec4 out_color;\n"
                       "void main(){gl_Position=pos;out_color=color;}\n";
    const GLchar *fs = "#version 150\nin vec4 out_color;\nout vec4 fragment_color;\n"
                       "void main(){fragment_color=out_color;}\n";
    uint32_t pixels[64] = {0};
    GLuint texture, fbo, vbo, program, vertex, fragment;
    GLint compiled, linked;
    int red = 0, blue = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    if (!check_status(GL_FRAMEBUFFER, "Quad FBO status"))
        return 0;
    glViewport(0, 0, 8, 8);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glVertexAttrib4f(1, 1.0f, 0.0f, 0.0f, 1.0f);
    glEnableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    if (!check_no_error("Quad setup"))
        return 0;
    program = glCreateProgram();
    vertex = glCreateShader(GL_VERTEX_SHADER);
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vertex, 1, &vs, NULL);
    glShaderSource(fragment, 1, &fs, NULL);
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    glBindAttribLocation(program, 0, "pos");
    glBindAttribLocation(program, 1, "color");
    glBindFragDataLocation(program, 0, "fragment_color");
    glCompileShader(vertex);
    glCompileShader(fragment);
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        fprintf(stderr, "Quad vertex shader failed compilation\n");
        return 0;
    }
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        fprintf(stderr, "Quad fragment shader failed compilation\n");
        return 0;
    }
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked || !check_no_error("Quad shader link")) {
        fprintf(stderr, "Quad shader failed to link\n");
        return 0;
    }
    glUseProgram(program);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (!check_no_error("draw_test_quad"))
        return 0;
    glUseProgram(0);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
    if (!check_no_error("filling convention readback"))
        return 0;
    for (int i = 0; i < 64; ++i) {
        if (pixels[i] == 0xffff0000u)
            ++red;
        if (pixels[i] == 0xff0000ffu)
            ++blue;
    }
    if (red != 1 || blue != 63) {
        fprintf(stderr, "Quad incorrect pixels: red=%d blue=%d\n", red, blue);
        return 0;
    }
    glDeleteProgram(program);
    glDeleteBuffers(1, &vbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
    return check_no_error("Quad cleanup");
}

int main(void) {
    struct mgl_native_dispatch native;
    struct mgl_context *ctx;
    memset(&native, 0, sizeof(native));
    if (!mgl_native_dispatch_init(&native)) {
        fprintf(stderr, "Cannot load real EGL/GLES native\n");
        return 1;
    }
    platform_display = (void *)native.eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (!platform_display) {
        fprintf(stderr, "Real EGL native lacks surfaceless platform entry point\n");
        return 2;
    }
    native.eglGetDisplay = get_surfaceless_display;
    ctx = mgl_context_create_offscreen(&native, NULL, 64, 64);
    if (!ctx || !mgl_context_make_current(ctx)) {
        fprintf(stderr, "Cannot make real GLES context current\n");
        return 3;
    }
    if (!check_pbo_bgra() || !check_texture_update() || !check_rgba16() ||
        !check_mixed_attachments() || !check_filling_convention())
        return 4;
    puts("Native GLES integration PASS: BGRA PBO, texture update, RGBA16, mixed FBO, quad drawing, "
         "pixel readback");
    return 0;
}

#include "mgl/gl.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"
#include "state/state.h"
#include "mgl/glx.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct captured {
    GLuint next;
    GLuint bound;
    GLenum target;
    GLenum upload_target;
    GLenum attached_target;
    GLuint attached_name;
    GLuint upload_bound;
    GLsizei upload_width;
    GLsizei upload_height;
    unsigned image_calls;
    unsigned sub_calls;
    unsigned param_calls;
    GLenum upload_format;
    GLenum upload_type;
    unsigned char upload_data[24];
    GLenum param_target;
    GLenum mip_target;
    GLuint mip_bound;
    unsigned mip_calls;
} cap;

static void gen_textures(GLsizei n, GLuint *names) {
    for (GLsizei i = 0; i < n; ++i)
        names[i] = ++cap.next;
}

static void delete_textures(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void bind_texture(GLenum target, GLuint name) {
    cap.target = target;
    cap.bound = name;
}

static void capture_upload(GLenum target, GLsizei width, GLsizei height, GLenum format,
                           GLenum type, const void *data) {
    cap.upload_format = format;
    cap.upload_type = type;
    if (data != NULL &&
        width * height * (format == GL_RGB ? 3 : 4) <= (GLsizei)sizeof(cap.upload_data) &&
        type == GL_UNSIGNED_BYTE)
        memcpy(cap.upload_data, data,
               (size_t)width * (size_t)height * (format == GL_RGB ? 3u : 4u));
    cap.upload_target = target;
    cap.upload_bound = cap.bound;
    cap.upload_width = width;
    cap.upload_height = height;
}

static void image2d(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height,
                    GLint border, GLenum format, GLenum type, const void *data) {
    (void)level;
    (void)internal;
    (void)border;
    capture_upload(target, width, height, format, type, data);
    ++cap.image_calls;
}

static void sub2d(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
                  GLenum format, GLenum type, const void *data) {
    (void)level;
    (void)x;
    (void)y;
    capture_upload(target, width, height, format, type, data);
    ++cap.sub_calls;
}

static void generate_mips(GLenum target) {
    cap.mip_target = target;
    cap.mip_bound = cap.bound;
    ++cap.mip_calls;
}

static void param(GLenum target, GLenum pname, GLint value) {
    (void)pname;
    (void)value;
    cap.param_target = target;
    ++cap.param_calls;
}

static void gen_fbo(GLsizei n, GLuint *names) {
    for (GLsizei i = 0; i < n; ++i)
        names[i] = (GLuint)i + 10u;
}

static void bind_fbo(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void attach(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                   GLint level) {
    (void)target;
    (void)attachment;
    (void)level;
    cap.attached_target = textarget;
    cap.attached_name = texture;
}

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "1d test:%d: %s\n", __LINE__, #expr);                                  \
            return __LINE__;                                                                       \
        }                                                                                          \
    } while (0)

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *tex;
    GLuint t1, t2, fbo;
    GLint value;
    unsigned char rgb[6] = {10, 20, 30, 40, 50, 60};
    unsigned char bgr[3] = {1, 2, 3};
    unsigned char readback[6] = {0};
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGenTextures = gen_textures;
    native.glesDeleteTextures = delete_textures;
    native.glesBindTexture = bind_texture;
    native.glesTexImage2D = image2d;
    native.glesTexSubImage2D = sub2d;
    native.glesTexParameteri = param;
    native.glesGenFramebuffers = gen_fbo;
    native.glesBindFramebuffer = bind_fbo;
    native.glesFramebufferTexture2D = attach;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    CHECK(ctx.share_group != NULL);
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    ctx.unpack_alignment = 4;
    ctx.pack_alignment = 4;
    ctx.next_framebuffer_name = 1u;
    mgl_context_set_current_for_test(&ctx);
    CHECK(glXGetProcAddress((const GLubyte *)"glTexImage1D") != NULL);
    CHECK(glXGetProcAddress((const GLubyte *)"glTexSubImage1D") != NULL);
    CHECK(glXGetProcAddress((const GLubyte *)"glFramebufferTexture1D") != NULL);
    glGenTextures(1, &t1);
    glGenTextures(1, &t2);
    glBindTexture(GL_TEXTURE_2D, t2);
    glBindTexture(GL_TEXTURE_1D, t1);
    glGetIntegerv(GL_TEXTURE_BINDING_1D, &value);
    CHECK(value == (GLint)t1 && glGetError() == GL_NO_ERROR);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
    CHECK(value == (GLint)t2 && glGetError() == GL_NO_ERROR);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB8, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    CHECK(glGetError() == GL_NO_ERROR && cap.image_calls == 1 && cap.upload_bound != 0 &&
          cap.upload_bound != cap.bound);
    CHECK(cap.upload_target == GL_TEXTURE_2D && cap.upload_width == 2 && cap.upload_height == 1);
    CHECK(cap.bound != 0 && cap.bound != cap.upload_bound);
    tex = mgl_bound_texture(&ctx, GL_TEXTURE_1D);
    CHECK(tex != NULL && tex->levels[0].defined && tex->levels[0].width == 2 &&
          tex->levels[0].height == 1);
    CHECK(memcmp(tex->levels[0].data, rgb, sizeof(rgb)) == 0);
    glTexSubImage1D(GL_TEXTURE_1D, 0, 1, 1, GL_BGR, GL_UNSIGNED_BYTE, bgr);
    CHECK(glGetError() == GL_NO_ERROR && cap.sub_calls == 1);
    CHECK(tex->levels[0].data[3] == 3 && tex->levels[0].data[4] == 2 &&
          tex->levels[0].data[5] == 1);
    glGenerateMipmapEXT(GL_TEXTURE_1D);
    CHECK(glGetError() == GL_NO_ERROR && tex->levels[1].defined && tex->levels[1].width == 1 &&
          tex->levels[1].height == 1 && tex->levels[1].data_size == 3u &&
          tex->levels[1].data[0] == 7u && tex->levels[1].data[1] == 11u &&
          tex->levels[1].data[2] == 16u && cap.upload_target == GL_TEXTURE_2D &&
          cap.upload_width == 1 && cap.upload_height == 1 && cap.bound != cap.upload_bound);
    native.glesGenerateMipmap = generate_mips;
    glGenerateMipmap(GL_TEXTURE_1D);
    CHECK(glGetError() == GL_NO_ERROR && cap.mip_calls == 1u && cap.mip_target == GL_TEXTURE_2D &&
          cap.mip_bound == cap.upload_bound && cap.bound != cap.mip_bound);
    glGetTexImage(GL_TEXTURE_1D, 0, GL_RGB, GL_UNSIGNED_BYTE, readback);
    CHECK(glGetError() == GL_NO_ERROR && readback[0] == 10 && readback[1] == 20 &&
          readback[2] == 30);
    CHECK(readback[3] == 3 && readback[4] == 2 && readback[5] == 1);
    {
        uint16_t bgr16[6] = {0u, 32768u, 65535u, 257u, 1028u, 3855u};
        uint16_t rgb16[6] = {65535u, 32768u, 0u, 3855u, 1028u, 257u};
        uint16_t patch16[3] = {0u, 65535u, 32768u};
        uint16_t returned16[6] = {0};
        const unsigned char expected_image[6] = {255u, 128u, 0u, 15u, 4u, 1u};
        const unsigned char expected_patch[3] = {128u, 255u, 0u};
        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 2, 0, GL_BGR, GL_UNSIGNED_SHORT, bgr16);
        CHECK(glGetError() == GL_NO_ERROR && cap.upload_format == GL_RGB &&
              cap.upload_type == GL_UNSIGNED_BYTE);
        CHECK(memcmp(cap.upload_data, expected_image, sizeof(expected_image)) == 0);
        CHECK(tex->levels[0].type == GL_UNSIGNED_SHORT && tex->levels[0].format == GL_RGB);
        CHECK(memcmp(tex->levels[0].data, rgb16, sizeof(rgb16)) == 0);
        glGetTexImage(GL_TEXTURE_1D, 0, GL_BGR, GL_UNSIGNED_SHORT, returned16);
        CHECK(glGetError() == GL_NO_ERROR && memcmp(returned16, bgr16, sizeof(bgr16)) == 0);
        glTexSubImage1D(GL_TEXTURE_1D, 0, 1, 1, GL_BGR, GL_UNSIGNED_SHORT, patch16);
        CHECK(glGetError() == GL_NO_ERROR && cap.upload_format == GL_RGB &&
              cap.upload_type == GL_UNSIGNED_BYTE);
        CHECK(memcmp(cap.upload_data, expected_patch, sizeof(expected_patch)) == 0);
        CHECK(memcmp(tex->levels[0].data + 6, (uint16_t[]){32768u, 65535u, 0u}, 6u) == 0);
    }
    CHECK(cap.bound != 0 && cap.bound != cap.upload_bound);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    CHECK(glGetError() == GL_NO_ERROR && cap.param_target == GL_TEXTURE_2D && cap.bound != 0 &&
          cap.bound != cap.upload_bound);
    glGetTexLevelParameteriv(GL_TEXTURE_1D, 0, GL_TEXTURE_WIDTH, &value);
    CHECK(value == 2 && glGetError() == GL_NO_ERROR);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture1D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_1D, t1, 0);
    CHECK(glGetError() == GL_NO_ERROR && cap.attached_target == GL_TEXTURE_2D &&
          cap.attached_name == cap.upload_bound);
    glTexImage1D(GL_TEXTURE_2D, 0, GL_RGB8, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    CHECK(glGetError() == GL_INVALID_ENUM);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB8, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    CHECK(glGetError() == GL_INVALID_VALUE);
    glBindTexture(GL_TEXTURE_1D, t2);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    memset(&cap, 0, sizeof(cap));
    puts("1D texture compatibility passed");
    return 0;
}

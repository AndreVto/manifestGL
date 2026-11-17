#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static GLuint next_texture = 1200u;
static GLenum native_active_texture = GL_TEXTURE0;
static unsigned read_calls;
static unsigned char read_seed;

static GLenum fake_get_error(void) {
    return GL_NO_ERROR;
}

static void fake_gen_textures(GLsizei n, GLuint *textures) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        textures[i] = next_texture++;
}

static void fake_delete_textures(GLsizei n, const GLuint *textures) {
    (void)n;
    (void)textures;
}

static void fake_active_texture(GLenum texture) {
    native_active_texture = texture;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    (void)target;
    (void)texture;
}

static void fake_tex_image_2d(GLenum target, GLint level, GLint internalformat, GLsizei width,
                              GLsizei height, GLint border, GLenum format, GLenum type,
                              const void *pixels) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)border;
    (void)format;
    (void)type;
    (void)pixels;
}

static void fake_tex_sub_image_2d(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                  GLsizei width, GLsizei height, GLenum format, GLenum type,
                                  const void *pixels) {
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)width;
    (void)height;
    (void)format;
    (void)type;
    (void)pixels;
}

static void fake_tex_image_3d(GLenum target, GLint level, GLint internalformat, GLsizei width,
                              GLsizei height, GLsizei depth, GLint border, GLenum format,
                              GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)depth;
    (void)border;
    (void)format;
    (void)type;
    (void)pixels;
}

static void fake_tex_sub_image_3d(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                  GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                  GLenum format, GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)zoffset;
    (void)width;
    (void)height;
    (void)depth;
    (void)format;
    (void)type;
    (void)pixels;
}

static void fake_bind_framebuffer(GLenum target, GLuint framebuffer) {
    (void)target;
    (void)framebuffer;
}

static void fake_bind_buffer(GLenum target, GLuint buffer) {
    (void)target;
    (void)buffer;
}

static void fake_pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
}

static void fake_read_pixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                             GLenum type, void *pixels) {
    unsigned char *dst = pixels;
    size_t i, bytes = (size_t)width * (size_t)height * 4u;
    (void)x;
    (void)y;
    (void)format;
    (void)type;
    ++read_calls;
    for (i = 0u; i < bytes; ++i)
        dst[i] = (unsigned char)(read_seed + (unsigned char)i);
}

static int fail(int code, const char *message) {
    fprintf(stderr, "EXT DSA MultiTex copy failure %d: %s\n", code, message);
    return code;
}

static int expect_image(GLenum texunit, GLenum target, const unsigned char *expected,
                        size_t bytes) {
    unsigned char out[32];
    memset(out, 0, sizeof(out));
    glGetMultiTexImage(texunit, target, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    return mgl_error_take(&mgl_context_current()->errors) == GL_NO_ERROR &&
           memcmp(out, expected, bytes) == 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint textures[6] = {0u, 0u, 0u, 0u, 0u, 0u};
    const unsigned char zero_volume[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    unsigned char expected[8];
    unsigned char out[8];
    GLuint list;
    unsigned before_reads;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetError = fake_get_error;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexImage2D = fake_tex_image_2d;
    native.glesTexSubImage2D = fake_tex_sub_image_2d;
    native.glesTexImage3D = fake_tex_image_3d;
    native.glesTexSubImage3D = fake_tex_sub_image_3d;
    native.glesBindFramebuffer = fake_bind_framebuffer;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesPixelStorei = fake_pixel_store;
    native.glesReadPixels = fake_read_pixels;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.unpack_alignment = 4;
    ctx.pack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glGenTextures(6, textures);
    if (textures[0] == 0u || textures[5] == 0u)
        return fail(2, "texture generation");

    glActiveTexture(GL_TEXTURE1);
    glBindMultiTexture(GL_TEXTURE3, GL_TEXTURE_2D, textures[0]);
    glBindMultiTexture(GL_TEXTURE4, GL_TEXTURE_2D, textures[1]);
    glBindMultiTexture(GL_TEXTURE5, GL_TEXTURE_3D, textures[2]);
    glBindMultiTexture(GL_TEXTURE6, GL_TEXTURE_CUBE_MAP, textures[3]);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || ctx.compat.active_texture != 1u ||
        native_active_texture != GL_TEXTURE1)
        return fail(3, "explicit bindings disturbed selector");

    read_seed = 10u;
    glCopyMultiTexImage2DEXT(GL_TEXTURE3, GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 2, 1, 0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != 1u ||
        ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(4, "CopyMultiTexImage2DEXT execution/selector");
    if (!expect_image(GL_TEXTURE3, GL_TEXTURE_2D,
                      (const unsigned char[]){10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u}, 8u))
        return fail(5, "CopyMultiTexImage2DEXT destination");

    read_seed = 30u;
    glCopyMultiTexImage2D(GL_TEXTURE4, GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 2, 1, 0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != 2u ||
        ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(6, "unsuffixed CopyMultiTexImage2D");
    if (!expect_image(GL_TEXTURE4, GL_TEXTURE_2D,
                      (const unsigned char[]){30u, 31u, 32u, 33u, 34u, 35u, 36u, 37u}, 8u))
        return fail(7, "unsuffixed destination");

    read_seed = 50u;
    glCopyMultiTexSubImage2DEXT(GL_TEXTURE3, GL_TEXTURE_2D, 0, 1, 0, 0, 0, 1, 1);
    memcpy(expected, (const unsigned char[]){10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u}, 8u);
    memcpy(expected + 4u, (const unsigned char[]){50u, 51u, 52u, 53u}, 4u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != 3u ||
        !expect_image(GL_TEXTURE3, GL_TEXTURE_2D, expected, sizeof(expected)) ||
        ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(8, "CopyMultiTexSubImage2DEXT");

    glMultiTexImage3D(GL_TEXTURE5, GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 2, 0, GL_RGBA,
                      GL_UNSIGNED_BYTE, zero_volume);
    read_seed = 70u;
    glCopyMultiTexSubImage3DEXT(GL_TEXTURE5, GL_TEXTURE_3D, 0, 0, 0, 1, 0, 0, 1, 1);
    memset(out, 0, sizeof(out));
    glGetMultiTexImage(GL_TEXTURE5, GL_TEXTURE_3D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != 4u ||
        memcmp(out, (const unsigned char[]){0u, 0u, 0u, 0u, 70u, 71u, 72u, 73u}, 8u) != 0 ||
        ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(9, "CopyMultiTexSubImage3DEXT");

    read_seed = 90u;
    glCopyMultiTexImage2DEXT(GL_TEXTURE6, GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA8, 0, 0, 1, 1,
                             0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != 5u ||
        !expect_image(GL_TEXTURE6, GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                      (const unsigned char[]){90u, 91u, 92u, 93u}, 4u) ||
        ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(10, "explicit-unit cube-face copy");

    glActiveTexture(GL_TEXTURE6);
    read_seed = 100u;
    glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA8, 0, 0, 1, 1, 0);
    memset(out, 0, sizeof(out));
    glGetTexImage(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != 6u ||
        memcmp(out, (const unsigned char[]){100u, 101u, 102u, 103u}, 4u) != 0)
        return fail(11, "legacy cube-face CopyTexImage2D");
    read_seed = 110u;
    glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, 0, 0, 0, 0, 1, 1);
    memset(out, 0, sizeof(out));
    glGetTexImage(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != 7u ||
        memcmp(out, (const unsigned char[]){110u, 111u, 112u, 113u}, 4u) != 0)
        return fail(12, "legacy cube-face CopyTexSubImage2D");
    glActiveTexture(GL_TEXTURE1);

    glBindMultiTexture(GL_TEXTURE3, GL_TEXTURE_2D, textures[0]);
    list = glGenLists(1);
    if (list == 0u)
        return fail(13, "list allocation");
    before_reads = read_calls;
    glNewList(list, GL_COMPILE);
    glCopyMultiTexImage2DEXT(GL_TEXTURE3, GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 1, 1, 0);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != before_reads ||
        ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(14, "display-list compile executed copy or disturbed selector");
    glBindMultiTexture(GL_TEXTURE3, GL_TEXTURE_2D, textures[4]);
    read_seed = 130u;
    glCallList(list);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != before_reads + 1u ||
        !expect_image(GL_TEXTURE3, GL_TEXTURE_2D, (const unsigned char[]){130u, 131u, 132u, 133u},
                      4u) ||
        ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(15, "display-list unit capture/replay-time framebuffer/late binding");

    before_reads = read_calls;
    glCopyMultiTexImage2D(GL_TEXTURE0 + MGL_TEXTURE_UNITS, GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 1, 1,
                          0);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || read_calls != before_reads ||
        ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(16, "out-of-range texunit error/selector");
    glCopyMultiTexImage2D(GL_TEXTURE3, GL_RENDERBUFFER, 0, GL_RGBA8, 0, 0, 1, 1, 0);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM || read_calls != before_reads ||
        ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(17, "invalid target error/selector");
    glCopyMultiTexImage2D(GL_TEXTURE6, GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA8, 0, 0, 2, 1, 0);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE || read_calls != before_reads ||
        ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(18, "cube square validation before framebuffer read");

    if (glXGetProcAddress((const GLubyte *)"glCopyMultiTexImage2D") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCopyMultiTexImage2DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCopyMultiTexSubImage2D") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCopyMultiTexSubImage2DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCopyMultiTexSubImage3D") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCopyMultiTexSubImage3DEXT") == NULL)
        return fail(19, "proc-address exposure");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL EXT DSA MultiTex copy unit test: PASS");
    return 0;
}

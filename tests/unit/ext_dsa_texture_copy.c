#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

static GLuint next_native_texture = 1700u;
static GLenum native_active_texture = GL_TEXTURE0;
static GLuint last_bound_2d;
static GLuint last_bound_3d;
static GLuint last_bound_cube;
static unsigned read_calls;
static unsigned char read_seed;

static GLenum fake_get_error(void) {
    return GL_NO_ERROR;
}

static void fake_gen_textures(GLsizei n, GLuint *textures) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        textures[i] = next_native_texture++;
}

static void fake_delete_textures(GLsizei n, const GLuint *textures) {
    (void)n;
    (void)textures;
}

static void fake_active_texture(GLenum texture) {
    native_active_texture = texture;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    if (target == GL_TEXTURE_2D)
        last_bound_2d = texture;
    else if (target == GL_TEXTURE_3D)
        last_bound_3d = texture;
    else if (target == GL_TEXTURE_CUBE_MAP)
        last_bound_cube = texture;
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
    fprintf(stderr, "EXT DSA named texture copy failure %d: %s\n", code, message);
    return code;
}

static int get_named_rgba(GLuint texture, unsigned char *out, GLsizei bytes) {
    memset(out, 0, (size_t)bytes);
    glGetTextureImage(texture, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes, out);
    return mgl_error_take(&mgl_context_current()->errors) == GL_NO_ERROR;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint names[6] = {0u, 0u, 0u, 0u, 0u, 0u};
    struct mgl_texture *saved_obj;
    unsigned char out[8];
    const unsigned char zero_volume[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
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

    glGenTextures(6, names);
    if (names[0] == 0u || names[5] == 0u)
        return fail(2, "texture generation");

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, names[0]);
    saved_obj = mgl_texture_find(&ctx, names[0]);
    if (saved_obj == NULL || !saved_obj->realized)
        return fail(3, "saved binding realization");

    list = glGenLists(1);
    if (list == 0u)
        return fail(4, "display-list allocation");
    before_reads = read_calls;
    glNewList(list, GL_COMPILE);
    glCopyTextureImage2DEXT(names[1], GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 1, 1, 0);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != before_reads ||
        glIsTexture(names[1]) != GL_FALSE)
        return fail(5, "GL_COMPILE executed or realized named texture");

    read_seed = 20u;
    glCallList(list);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != before_reads + 1u ||
        glIsTexture(names[1]) != GL_TRUE || ctx.compat.active_texture != 2u ||
        ctx.texture_2d_binding[2] != names[0] || native_active_texture != GL_TEXTURE2 ||
        last_bound_2d != saved_obj->native_name)
        return fail(6, "named image replay/selector restoration");
    if (!get_named_rgba(names[1], out, 4) ||
        memcmp(out, (const unsigned char[]){20u, 21u, 22u, 23u}, 4u) != 0)
        return fail(7, "named image replay-time framebuffer contents");

    read_seed = 30u;
    glCopyTextureImage2DEXT(0u, GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 1, 1, 0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || ctx.texture_2d_binding[2] != names[0] ||
        native_active_texture != GL_TEXTURE2 || last_bound_2d != saved_obj->native_name)
        return fail(8, "default-object copy disturbed binding");
    glBindTexture(GL_TEXTURE_2D, 0u);
    memset(out, 0, sizeof(out));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(out, (const unsigned char[]){30u, 31u, 32u, 33u}, 4u) != 0)
        return fail(9, "default texture zero update");
    glBindTexture(GL_TEXTURE_2D, names[0]);

    glBindTexture(GL_TEXTURE_3D, names[2]);
    before_reads = read_calls;
    glCopyTextureImage2DEXT(names[2], GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 1, 1, 0);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || read_calls != before_reads ||
        ctx.texture_2d_binding[2] != names[0])
        return fail(10, "target mismatch must fail before framebuffer read");

    read_seed = 40u;
    glCopyTextureSubImage2DEXT(names[1], GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || !get_named_rgba(names[1], out, 4) ||
        memcmp(out, (const unsigned char[]){40u, 41u, 42u, 43u}, 4u) != 0 ||
        ctx.texture_2d_binding[2] != names[0])
        return fail(11, "named 2D subimage copy");

    glDeleteLists(list, 1);
    list = glGenLists(1);
    before_reads = read_calls;
    glNewList(list, GL_COMPILE);
    glCopyTextureSubImage2DEXT(names[1], GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != before_reads ||
        !get_named_rgba(names[1], out, 4) ||
        memcmp(out, (const unsigned char[]){40u, 41u, 42u, 43u}, 4u) != 0)
        return fail(12, "named 2D subimage GL_COMPILE executed");
    read_seed = 60u;
    glCallList(list);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != before_reads + 1u ||
        !get_named_rgba(names[1], out, 4) ||
        memcmp(out, (const unsigned char[]){60u, 61u, 62u, 63u}, 4u) != 0)
        return fail(13, "named 2D subimage list replay");

    glBindTexture(GL_TEXTURE_3D, names[3]);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, zero_volume);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(14, "3D setup");
    read_seed = 70u;
    glCopyTextureSubImage3DEXT(names[3], GL_TEXTURE_3D, 0, 0, 0, 1, 0, 0, 1, 1);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || !get_named_rgba(names[3], out, 8) ||
        memcmp(out, (const unsigned char[]){0u, 0u, 0u, 0u, 70u, 71u, 72u, 73u}, 8u) != 0)
        return fail(15, "named 3D subimage copy");

    glDeleteLists(list, 1);
    list = glGenLists(1);
    before_reads = read_calls;
    glNewList(list, GL_COMPILE);
    glCopyTextureSubImage3DEXT(names[3], GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 1, 1);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != before_reads)
        return fail(16, "named 3D subimage GL_COMPILE executed");
    read_seed = 80u;
    glCallList(list);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || read_calls != before_reads + 1u ||
        !get_named_rgba(names[3], out, 8) ||
        memcmp(out, (const unsigned char[]){80u, 81u, 82u, 83u, 70u, 71u, 72u, 73u}, 8u) != 0)
        return fail(17, "named 3D subimage list replay");

    glBindTexture(GL_TEXTURE_CUBE_MAP, names[4]);
    glBindTexture(GL_TEXTURE_CUBE_MAP, names[5]);
    read_seed = 90u;
    glCopyTextureImage2DEXT(names[4], GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA8, 0, 0, 1, 1, 0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || ctx.texture_cube_binding[2] != names[5])
        return fail(18, "cube-face named copy/binding restoration");
    glBindTexture(GL_TEXTURE_CUBE_MAP, names[4]);
    memset(out, 0, sizeof(out));
    glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(out, (const unsigned char[]){90u, 91u, 92u, 93u}, 4u) != 0)
        return fail(19, "cube-face named copy contents");

    before_reads = read_calls;
    glCopyTextureImage2DEXT(names[4], GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA8, 0, 0, 2, 1, 0);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE || read_calls != before_reads)
        return fail(20, "cube square validation before framebuffer read");

    if (glXGetProcAddress((const GLubyte *)"glCopyTextureImage2DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCopyTextureSubImage2DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCopyTextureSubImage3DEXT") == NULL)
        return fail(21, "proc-address exposure");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL EXT DSA named texture copy unit test: PASS");
    return 0;
}

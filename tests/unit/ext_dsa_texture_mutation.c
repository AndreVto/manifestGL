#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

static GLuint next_native_texture = 1800u;
static GLenum native_active_texture = GL_TEXTURE0;
static GLuint last_bound_2d;
static GLuint last_bound_3d;
static GLuint last_bound_cube;

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

static void fake_bind_buffer(GLenum target, GLuint buffer) {
    (void)target;
    (void)buffer;
}

static void fake_pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "EXT DSA named texture mutation failure %d: %s\n", code, message);
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
    GLuint names[7] = {0u, 0u, 0u, 0u, 0u, 0u, 0u};
    struct mgl_texture *saved_2d_obj;
    struct mgl_texture *saved_3d_obj;
    struct mgl_texture *saved_cube_obj;
    unsigned char out[16];
    unsigned char capture_src[12] = {1u, 2u, 3u, 4u, 20u, 21u, 22u, 23u, 30u, 31u, 32u, 33u};
    unsigned char sub_src[4] = {40u, 41u, 42u, 43u};
    unsigned char volume[8] = {50u, 51u, 52u, 53u, 60u, 61u, 62u, 63u};
    unsigned char volume_sub[4] = {70u, 71u, 72u, 73u};
    GLuint list;

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
    native.glesBindBuffer = fake_bind_buffer;
    native.glesPixelStorei = fake_pixel_store;

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

    glGenTextures(7, names);
    if (names[0] == 0u || names[6] == 0u)
        return fail(2, "texture generation");

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, names[0]);
    saved_2d_obj = mgl_texture_find(&ctx, names[0]);
    if (saved_2d_obj == NULL || !saved_2d_obj->realized)
        return fail(3, "saved 2D realization");

    list = glGenLists(1);
    if (list == 0u)
        return fail(4, "display-list allocation");
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 3);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 1);
    glNewList(list, GL_COMPILE);
    glTextureImage2DEXT(names[1], GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        capture_src);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || glIsTexture(names[1]) != GL_FALSE)
        return fail(5, "GL_COMPILE realized named texture");

    capture_src[4] = 90u;
    capture_src[5] = 91u;
    capture_src[6] = 92u;
    capture_src[7] = 93u;
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glCallList(list);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || glIsTexture(names[1]) != GL_TRUE ||
        ctx.compat.active_texture != 3u || ctx.texture_2d_binding[3] != names[0] ||
        native_active_texture != GL_TEXTURE3 || last_bound_2d != saved_2d_obj->native_name)
        return fail(6, "named image replay/binding restoration");
    if (!get_named_rgba(names[1], out, 4) ||
        memcmp(out, (const unsigned char[]){20u, 21u, 22u, 23u}, 4u) != 0)
        return fail(7, "display-list payload was not captured at compile time");

    glTextureSubImage2DEXT(names[1], GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                           sub_src);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || !get_named_rgba(names[1], out, 4) ||
        memcmp(out, sub_src, 4u) != 0 || ctx.texture_2d_binding[3] != names[0])
        return fail(8, "direct named 2D subimage");

    glDeleteLists(list, 1);
    list = glGenLists(1);
    sub_src[0] = 80u;
    sub_src[1] = 81u;
    sub_src[2] = 82u;
    sub_src[3] = 83u;
    glNewList(list, GL_COMPILE);
    glTextureSubImage2DEXT(names[1], GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                           sub_src);
    glEndList();
    sub_src[0] = 1u;
    sub_src[1] = 1u;
    sub_src[2] = 1u;
    sub_src[3] = 1u;
    glCallList(list);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || !get_named_rgba(names[1], out, 4) ||
        memcmp(out, (const unsigned char[]){80u, 81u, 82u, 83u}, 4u) != 0)
        return fail(9, "named 2D subimage list payload capture");

    glTextureImage2DEXT(0u, GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        (const unsigned char[]){100u, 101u, 102u, 103u});
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || ctx.texture_2d_binding[3] != names[0] ||
        last_bound_2d != saved_2d_obj->native_name)
        return fail(10, "default texture update disturbed binding");
    glBindTexture(GL_TEXTURE_2D, 0u);
    memset(out, 0, sizeof(out));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(out, (const unsigned char[]){100u, 101u, 102u, 103u}, 4u) != 0)
        return fail(11, "texture zero default-object semantics");
    glBindTexture(GL_TEXTURE_2D, names[0]);

    glBindTexture(GL_TEXTURE_3D, names[2]);
    glTextureImage2DEXT(names[2], GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        (const unsigned char[]){1u, 2u, 3u, 4u});
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION ||
        ctx.texture_2d_binding[3] != names[0])
        return fail(12, "existing target mismatch");

    glTextureImage3DEXT(names[3], GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        volume);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || !get_named_rgba(names[3], out, 8) ||
        memcmp(out, volume, 8u) != 0)
        return fail(13, "named 3D image");
    glBindTexture(GL_TEXTURE_3D, names[4]);
    saved_3d_obj = mgl_texture_find(&ctx, names[4]);
    if (saved_3d_obj == NULL)
        return fail(14, "saved 3D object");

    glDeleteLists(list, 1);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glTextureSubImage3DEXT(names[3], GL_TEXTURE_3D, 0, 0, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                           volume_sub);
    glEndList();
    volume_sub[0] = 9u;
    volume_sub[1] = 9u;
    volume_sub[2] = 9u;
    volume_sub[3] = 9u;
    glCallList(list);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || !get_named_rgba(names[3], out, 8) ||
        memcmp(out, (const unsigned char[]){50u, 51u, 52u, 53u, 70u, 71u, 72u, 73u}, 8u) != 0 ||
        ctx.texture_3d_binding[3] != names[4] || last_bound_3d != saved_3d_obj->native_name)
        return fail(15, "named 3D subimage replay/binding restoration");

    glBindTexture(GL_TEXTURE_CUBE_MAP, names[5]);
    glBindTexture(GL_TEXTURE_CUBE_MAP, names[6]);
    saved_cube_obj = mgl_texture_find(&ctx, names[6]);
    glTextureImage2DEXT(names[5], GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA8, 1, 1, 0, GL_RGBA,
                        GL_UNSIGNED_BYTE, (const unsigned char[]){110u, 111u, 112u, 113u});
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || ctx.texture_cube_binding[3] != names[6] ||
        saved_cube_obj == NULL || last_bound_cube != saved_cube_obj->native_name)
        return fail(16, "cube-face named image/binding restoration");
    glBindTexture(GL_TEXTURE_CUBE_MAP, names[5]);
    memset(out, 0, sizeof(out));
    glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(out, (const unsigned char[]){110u, 111u, 112u, 113u}, 4u) != 0)
        return fail(17, "cube-face named image contents");

    if (glXGetProcAddress((const GLubyte *)"glTextureImage2DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTextureSubImage2DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTextureImage3DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTextureSubImage3DEXT") == NULL)
        return fail(18, "proc-address exposure");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL EXT DSA named texture mutation unit test: PASS");
    return 0;
}

#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static GLuint next_texture = 900u;
static GLenum native_active_texture = GL_TEXTURE0;

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

static void fake_pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
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

static int fail(int code, const char *message) {
    fprintf(stderr, "EXT DSA MultiTex mutation failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint textures[7] = {0u, 0u, 0u, 0u, 0u, 0u, 0u};
    const unsigned char image_a[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    const unsigned char image_b[8] = {11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u};
    const unsigned char patch[4] = {91u, 92u, 93u, 94u};
    unsigned char list_payload[8] = {21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u};
    const unsigned char volume[8] = {31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u};
    const unsigned char volume_patch[4] = {71u, 72u, 73u, 74u};
    const unsigned char compressed_a[8] = {41u, 42u, 43u, 44u, 45u, 46u, 47u, 48u};
    const unsigned char compressed_b[8] = {51u, 52u, 53u, 54u, 55u, 56u, 57u, 58u};
    const unsigned char compressed_3d[16] = {61u, 62u, 63u, 64u, 65u, 66u, 67u, 68u,
                                             69u, 70u, 71u, 72u, 73u, 74u, 75u, 76u};
    const unsigned char compressed_3d_patch[8] = {81u, 82u, 83u, 84u, 85u, 86u, 87u, 88u};
    unsigned char compressed_3d_list_payload[16] = {101u, 102u, 103u, 104u, 105u, 106u, 107u, 108u,
                                                    109u, 110u, 111u, 112u, 113u, 114u, 115u, 116u};
    unsigned char expected[16];
    unsigned char out[32];
    GLuint list, compressed_list;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesPixelStorei = fake_pixel_store;
    native.glesTexImage3D = fake_tex_image_3d;
    native.glesTexSubImage3D = fake_tex_sub_image_3d;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.unpack_alignment = 4;
    ctx.pack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glGenTextures(7, textures);
    if (textures[0] == 0u || textures[1] == 0u || textures[2] == 0u || textures[3] == 0u ||
        textures[4] == 0u || textures[5] == 0u || textures[6] == 0u)
        return fail(2, "texture generation");

    glActiveTexture(GL_TEXTURE1);
    glBindMultiTexture(GL_TEXTURE3, GL_TEXTURE_2D, textures[0]);
    glBindMultiTexture(GL_TEXTURE4, GL_TEXTURE_2D, textures[1]);
    glBindMultiTexture(GL_TEXTURE5, GL_TEXTURE_3D, textures[2]);
    glBindMultiTexture(GL_TEXTURE6, GL_TEXTURE_2D, textures[3]);
    glBindMultiTexture(GL_TEXTURE7, GL_TEXTURE_2D_ARRAY, textures[5]);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || ctx.compat.active_texture != 1u ||
        native_active_texture != GL_TEXTURE1)
        return fail(3, "selector-free bindings");

    glMultiTexImage2D(GL_TEXTURE3, GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      image_a);
    glMultiTexImage2DEXT(GL_TEXTURE4, GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, image_b);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || ctx.compat.active_texture != 1u ||
        native_active_texture != GL_TEXTURE1)
        return fail(4, "2D image selector preservation");
    memset(out, 0, sizeof(out));
    glGetMultiTexImage(GL_TEXTURE3, GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || memcmp(out, image_a, sizeof(image_a)) != 0)
        return fail(5, "2D image unit identity");
    memset(out, 0, sizeof(out));
    glGetMultiTexImage(GL_TEXTURE4, GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || memcmp(out, image_b, sizeof(image_b)) != 0)
        return fail(6, "2D EXT image unit identity");

    glMultiTexSubImage2DEXT(GL_TEXTURE3, GL_TEXTURE_2D, 0, 1, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                            patch);
    memcpy(expected, image_a, sizeof(image_a));
    memcpy(expected + 4, patch, sizeof(patch));
    memset(out, 0, sizeof(out));
    glGetMultiTexImage(GL_TEXTURE3, GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || memcmp(out, expected, sizeof(image_a)) != 0 ||
        ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(7, "2D subimage");

    glMultiTexImage3DEXT(GL_TEXTURE5, GL_TEXTURE_3D, 0, GL_RGBA8, 2, 1, 1, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, volume);
    glMultiTexSubImage3D(GL_TEXTURE5, GL_TEXTURE_3D, 0, 1, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                         volume_patch);
    memcpy(expected, volume, sizeof(volume));
    memcpy(expected + 4, volume_patch, sizeof(volume_patch));
    memset(out, 0, sizeof(out));
    glGetMultiTexImage(GL_TEXTURE5, GL_TEXTURE_3D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || memcmp(out, expected, sizeof(volume)) != 0 ||
        ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(8, "3D image/subimage");

    glCompressedMultiTexImage2DEXT(GL_TEXTURE6, GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
                                   4, 4, 0, (GLsizei)sizeof(compressed_a), compressed_a);
    memset(out, 0, sizeof(out));
    glGetCompressedMultiTexImage(GL_TEXTURE6, GL_TEXTURE_2D, 0, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(out, compressed_a, sizeof(compressed_a)) != 0)
        return fail(9, "compressed image");
    glCompressedMultiTexSubImage2D(GL_TEXTURE6, GL_TEXTURE_2D, 0, 0, 0, 4, 4,
                                   GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, (GLsizei)sizeof(compressed_b),
                                   compressed_b);
    memset(out, 0, sizeof(out));
    glGetCompressedMultiTexImageEXT(GL_TEXTURE6, GL_TEXTURE_2D, 0, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(out, compressed_b, sizeof(compressed_b)) != 0 || ctx.compat.active_texture != 1u ||
        native_active_texture != GL_TEXTURE1)
        return fail(10, "compressed subimage");

    glCompressedMultiTexImage3DEXT(GL_TEXTURE7, GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 4,
                                   4, 2, 0, (GLsizei)sizeof(compressed_3d), compressed_3d);
    memset(out, 0, sizeof(out));
    glGetCompressedMultiTexImage(GL_TEXTURE7, GL_TEXTURE_2D_ARRAY, 0, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(out, compressed_3d, sizeof(compressed_3d)) != 0 || ctx.compat.active_texture != 1u ||
        native_active_texture != GL_TEXTURE1)
        return fail(18, "compressed 3D array image");

    glCompressedMultiTexSubImage3D(GL_TEXTURE7, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, 4, 4, 1,
                                   GL_COMPRESSED_RED_RGTC1, (GLsizei)sizeof(compressed_3d_patch),
                                   compressed_3d_patch);
    memcpy(expected, compressed_3d, sizeof(compressed_3d));
    memcpy(expected + 8, compressed_3d_patch, sizeof(compressed_3d_patch));
    memset(out, 0, sizeof(out));
    glGetCompressedMultiTexImageEXT(GL_TEXTURE7, GL_TEXTURE_2D_ARRAY, 0, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(out, expected, sizeof(expected)) != 0 || ctx.compat.active_texture != 1u ||
        native_active_texture != GL_TEXTURE1)
        return fail(19, "compressed 3D array subimage");

    glCompressedMultiTexImage3D(GL_TEXTURE7, GL_TEXTURE_3D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 2, 0,
                                (GLsizei)sizeof(compressed_3d), compressed_3d);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || ctx.compat.active_texture != 1u ||
        native_active_texture != GL_TEXTURE1)
        return fail(20, "compressed true-3D rejection preserves selector");

    compressed_list = glGenLists(1);
    if (compressed_list == 0u)
        return fail(21, "compressed list allocation");
    glNewList(compressed_list, GL_COMPILE);
    glCompressedMultiTexImage3D(GL_TEXTURE7, GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 4, 4,
                                2, 0, (GLsizei)sizeof(compressed_3d_list_payload),
                                compressed_3d_list_payload);
    glEndList();
    memset(compressed_3d_list_payload, 0xee, sizeof(compressed_3d_list_payload));
    glBindMultiTexture(GL_TEXTURE7, GL_TEXTURE_2D_ARRAY, textures[6]);
    glCallList(compressed_list);
    memset(out, 0, sizeof(out));
    glGetCompressedMultiTexImage(GL_TEXTURE7, GL_TEXTURE_2D_ARRAY, 0, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(out,
               (const unsigned char[]){101u, 102u, 103u, 104u, 105u, 106u, 107u, 108u, 109u, 110u,
                                       111u, 112u, 113u, 114u, 115u, 116u},
               16u) != 0 ||
        ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(22, "compressed 3D display-list payload/unit capture");
    glBindMultiTexture(GL_TEXTURE7, GL_TEXTURE_2D_ARRAY, textures[5]);
    memset(out, 0, sizeof(out));
    glGetCompressedMultiTexImage(GL_TEXTURE7, GL_TEXTURE_2D_ARRAY, 0, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || memcmp(out, expected, sizeof(expected)) != 0)
        return fail(23, "compressed 3D display-list late binding");

    list = glGenLists(1);
    if (list == 0u)
        return fail(11, "list allocation");
    glNewList(list, GL_COMPILE);
    glMultiTexImage2D(GL_TEXTURE3, GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      list_payload);
    glEndList();
    memset(list_payload, 0xee, sizeof(list_payload));
    glBindMultiTexture(GL_TEXTURE3, GL_TEXTURE_2D, textures[4]);
    glCallList(list);
    memset(out, 0, sizeof(out));
    glGetMultiTexImage(GL_TEXTURE3, GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(out, (const unsigned char[]){21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u}, 8u) != 0 ||
        ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(12, "display-list payload/unit capture");
    glBindMultiTexture(GL_TEXTURE3, GL_TEXTURE_2D, textures[0]);
    memset(out, 0, sizeof(out));
    glGetMultiTexImage(GL_TEXTURE3, GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(out, (const unsigned char[]){1u, 2u, 3u, 4u, 91u, 92u, 93u, 94u}, 8u) != 0)
        return fail(13, "display-list late binding");

    glMultiTexImage2D(GL_TEXTURE_2D, GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      image_a);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM || ctx.compat.active_texture != 1u ||
        native_active_texture != GL_TEXTURE1)
        return fail(14, "invalid texunit preservation");
    glMultiTexImage2D(GL_TEXTURE3, GL_RENDERBUFFER, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      image_a);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM || ctx.compat.active_texture != 1u ||
        native_active_texture != GL_TEXTURE1)
        return fail(15, "invalid target preservation");

    if (glXGetProcAddress((const GLubyte *)"glMultiTexImage2D") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexImage2DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexSubImage2D") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexSubImage2DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexImage3D") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexImage3DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexSubImage3D") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexSubImage3DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCompressedMultiTexImage2D") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCompressedMultiTexImage2DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCompressedMultiTexSubImage2D") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCompressedMultiTexSubImage2DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCompressedMultiTexImage3D") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCompressedMultiTexImage3DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCompressedMultiTexSubImage3D") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCompressedMultiTexSubImage3DEXT") == NULL)
        return fail(16, "proc-address exposure");

    glDeleteLists(list, 1);
    glDeleteLists(compressed_list, 1);
    glDeleteTextures(7, textures);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(17, "unexpected final GL error");
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL EXT DSA MultiTex mutation unit test: PASS");
    return 0;
}

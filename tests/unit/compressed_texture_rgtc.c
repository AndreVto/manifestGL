#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    int rgtc_extension;
    unsigned tex_image_2d_calls;
    unsigned tex_image_3d_calls;
    unsigned compressed_2d_calls;
    unsigned compressed_3d_calls;
    unsigned compressed_sub_3d_calls;
    GLenum target;
    GLint internal_format;
    GLenum format;
    GLenum type;
    GLsizei width;
    GLsizei height;
    GLsizei depth;
    unsigned char upload[256];
    size_t upload_size;
    unsigned native_storage_2d_calls;
    unsigned native_storage_3d_calls;
    unsigned native_sub_2d_calls;
    unsigned decoded_sub_2d_calls;
    unsigned decoded_sub_3d_calls;
    GLenum native_storage_format;
} cap;

static const GLubyte *fake_get_string(GLenum name) {
    static const GLubyte with_ext[] = "GL_EXT_texture_compression_rgtc GL_EXT_mock";
    static const GLubyte without_ext[] = "GL_EXT_mock";
    if (name != GL_EXTENSIONS)
        return (const GLubyte *)"";
    return cap.rgtc_extension ? with_ext : without_ext;
}

static void fake_active_texture(GLenum texture) {
    (void)texture;
}

static void fake_gen_textures(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_texture;
}

static void fake_delete_textures(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_texture(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
}

static void fake_tex_storage_2d(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                                GLsizei height) {
    (void)target;
    (void)levels;
    (void)width;
    (void)height;
    ++cap.native_storage_2d_calls;
    cap.native_storage_format = internalformat;
}

static void fake_tex_storage_3d(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                                GLsizei height, GLsizei depth) {
    (void)target;
    (void)levels;
    (void)width;
    (void)height;
    (void)depth;
    ++cap.native_storage_3d_calls;
    cap.native_storage_format = internalformat;
}

static void fake_compressed_sub_2d(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                   GLsizei width, GLsizei height, GLenum format, GLsizei size,
                                   const void *data) {
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)width;
    (void)height;
    (void)format;
    (void)size;
    (void)data;
    ++cap.native_sub_2d_calls;
}

static void capture_upload(const void *data, size_t bytes) {
    cap.upload_size = bytes;
    memset(cap.upload, 0, sizeof(cap.upload));
    if (data != NULL && bytes <= sizeof(cap.upload))
        memcpy(cap.upload, data, bytes);
}

static void fake_tex_sub_2d(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                            GLsizei height, GLenum format, GLenum type, const void *data) {
    size_t components = format == GL_RGBA ? 4u : (format == GL_RG ? 2u : 1u);
    (void)level;
    (void)xoffset;
    (void)yoffset;
    ++cap.decoded_sub_2d_calls;
    cap.target = target;
    cap.format = format;
    cap.type = type;
    cap.width = width;
    cap.height = height;
    capture_upload(data, (size_t)width * (size_t)height * components);
}

static void fake_tex_sub_3d(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                            GLsizei width, GLsizei height, GLsizei depth, GLenum format,
                            GLenum type, const void *data) {
    size_t components = format == GL_RGBA ? 4u : (format == GL_RG ? 2u : 1u);
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)zoffset;
    ++cap.decoded_sub_3d_calls;
    cap.target = target;
    cap.format = format;
    cap.type = type;
    cap.width = width;
    cap.height = height;
    cap.depth = depth;
    capture_upload(data, (size_t)width * (size_t)height * (size_t)depth * components);
}

static void fake_tex_image_2d(GLenum target, GLint level, GLint internal_format, GLsizei width,
                              GLsizei height, GLint border, GLenum format, GLenum type,
                              const void *data) {
    size_t components = format == GL_RG ? 2u : 1u;
    (void)level;
    (void)border;
    ++cap.tex_image_2d_calls;
    cap.target = target;
    cap.internal_format = internal_format;
    cap.format = format;
    cap.type = type;
    cap.width = width;
    cap.height = height;
    cap.depth = 1;
    capture_upload(data, (size_t)width * (size_t)height * components);
}

static void fake_tex_image_3d(GLenum target, GLint level, GLint internal_format, GLsizei width,
                              GLsizei height, GLsizei depth, GLint border, GLenum format,
                              GLenum type, const void *data) {
    size_t components = format == GL_RGBA ? 4u : (format == GL_RG ? 2u : 1u);
    (void)level;
    (void)border;
    ++cap.tex_image_3d_calls;
    cap.target = target;
    cap.internal_format = internal_format;
    cap.format = format;
    cap.type = type;
    cap.width = width;
    cap.height = height;
    cap.depth = depth;
    capture_upload(data, (size_t)width * (size_t)height * (size_t)depth * components);
}

static void fake_compressed_image_2d(GLenum target, GLint level, GLenum format, GLsizei width,
                                     GLsizei height, GLint border, GLsizei image_size,
                                     const void *data) {
    (void)level;
    (void)border;
    ++cap.compressed_2d_calls;
    cap.target = target;
    cap.format = format;
    cap.width = width;
    cap.height = height;
    cap.depth = 1;
    capture_upload(data, (size_t)image_size);
}

static void fake_compressed_image_3d(GLenum target, GLint level, GLenum format, GLsizei width,
                                     GLsizei height, GLsizei depth, GLint border,
                                     GLsizei image_size, const void *data) {
    (void)level;
    (void)border;
    ++cap.compressed_3d_calls;
    cap.target = target;
    cap.format = format;
    cap.width = width;
    cap.height = height;
    cap.depth = depth;
    capture_upload(data, (size_t)image_size);
}

static void fake_compressed_sub_3d(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                   GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                   GLenum format, GLsizei image_size, const void *data) {
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)zoffset;
    ++cap.compressed_sub_3d_calls;
    cap.target = target;
    cap.format = format;
    cap.width = width;
    cap.height = height;
    cap.depth = depth;
    capture_upload(data, (size_t)image_size);
}

static int fail(int code, const char *message) {
    fprintf(stderr, "compressed RGTC failure %d: %s\n", code, message);
    return code;
}

static int expect_error(struct mgl_context *ctx, GLenum expected, int code, const char *message) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr, "compressed RGTC failure %d: %s (got 0x%04x expected 0x%04x)\n", code,
                message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

static int all_bytes_equal(const unsigned char *data, size_t begin, size_t end,
                           unsigned char value) {
    size_t i;
    for (i = begin; i < end; ++i)
        if (data[i] != value)
            return 0;
    return 1;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj;
    GLuint tex2d = 0u, array_tex = 0u, cube_tex = 0u, list_tex = 0u, list = 0u;
    unsigned char red_block[8] = {255u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    unsigned char direct_block[8] = {77u, 1u, 0u, 0u, 0u, 0u, 0u, 0u};
    unsigned char array_data[16] = {255u, 0u,   0u, 0u, 0u, 0u, 0u, 0u,
                                    0u,   255u, 0u, 0u, 0u, 0u, 0u, 0u};
    unsigned char array_expected[16];
    unsigned char sub_block[8] = {64u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    unsigned char readback[16];
    unsigned char list_data[16];
    unsigned char list_expected[16];
    GLint value = 0;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    memcpy(array_expected, array_data, sizeof(array_expected));
    memcpy(list_data, array_data, sizeof(list_data));
    memcpy(list_expected, list_data, sizeof(list_expected));

    native.glesGetString = fake_get_string;
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesPixelStorei = fake_pixel_store;
    native.glesTexImage2D = fake_tex_image_2d;
    native.glesTexSubImage2D = fake_tex_sub_2d;
    native.glesTexSubImage3D = fake_tex_sub_3d;
    native.glesTexImage3D = fake_tex_image_3d;
    native.glesCompressedTexImage2D = fake_compressed_image_2d;
    native.glesCompressedTexImage3D = fake_compressed_image_3d;
    native.glesCompressedTexSubImage3D = fake_compressed_sub_3d;
    native.glesCompressedTexSubImage2D = fake_compressed_sub_2d;
    native.glesTexStorage2D = fake_tex_storage_2d;
    native.glesTexStorage3D = fake_tex_storage_3d;

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

    glGenTextures(1, &tex2d);
    glBindTexture(GL_TEXTURE_2D, tex2d);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8, red_block);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 2, "software RGTC1 upload")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, tex2d);
    if (obj == NULL || !obj->levels[0].defined || !obj->levels[0].compressed ||
        obj->levels[0].internal_format != GL_COMPRESSED_RED_RGTC1 ||
        obj->levels[0].data_size != 8u || memcmp(obj->levels[0].data, red_block, 8u) != 0)
        return fail(3, "canonical RGTC1 ownership");
    if (cap.tex_image_2d_calls != 1u || cap.compressed_2d_calls != 0u ||
        cap.internal_format != (GLint)GL_R8 || cap.format != GL_RED ||
        cap.type != GL_UNSIGNED_BYTE || cap.upload_size != 16u ||
        !all_bytes_equal(cap.upload, 0u, 16u, 255u))
        return fail(4, "RGTC1 software decode lowering");

    memset(readback, 0, sizeof(readback));
    glGetCompressedTexImage(GL_TEXTURE_2D, 0, readback);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 5, "RGTC1 compressed readback")) != 0 ||
        memcmp(readback, red_block, 8u) != 0)
        return fail(6, "RGTC1 compressed readback payload");

    cap.rgtc_extension = 1;
    glCompressedTexImage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8, direct_block);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 7, "direct RGTC native path")) != 0)
        return rc;
    if (cap.compressed_2d_calls != 1u || cap.upload_size != 8u ||
        memcmp(cap.upload, direct_block, 8u) != 0)
        return fail(8, "direct RGTC native lowering");

    cap.rgtc_extension = 0;
    glGenTextures(1, &cube_tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube_tex);
    glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8,
                           red_block);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 9, "RGTC cube-face upload")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, cube_tex);
    if (obj == NULL || !obj->cube_levels[0][0].defined || !obj->cube_levels[0][0].compressed ||
        obj->cube_levels[0][0].data_size != 8u ||
        memcmp(obj->cube_levels[0][0].data, red_block, 8u) != 0)
        return fail(10, "canonical RGTC cube-face ownership");
    memset(readback, 0, sizeof(readback));
    glGetCompressedTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, readback);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 11, "RGTC cube-face readback")) != 0 ||
        memcmp(readback, red_block, 8u) != 0)
        return fail(12, "RGTC cube-face readback payload");

    glGenTextures(1, &array_tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, array_tex);
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 2, 0, 16,
                           array_data);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 9, "RGTC array upload")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, array_tex);
    if (obj == NULL || !obj->levels[0].defined || !obj->levels[0].compressed ||
        obj->levels[0].depth != 2 || obj->levels[0].data_size != sizeof(array_expected) ||
        memcmp(obj->levels[0].data, array_expected, sizeof(array_expected)) != 0)
        return fail(10, "canonical RGTC array ownership");
    if (cap.tex_image_3d_calls != 1u || cap.compressed_3d_calls != 0u ||
        cap.target != GL_TEXTURE_2D_ARRAY || cap.internal_format != (GLint)GL_R8 ||
        cap.format != GL_RED || cap.type != GL_UNSIGNED_BYTE || cap.width != 4 || cap.height != 4 ||
        cap.depth != 2 || cap.upload_size != 32u || !all_bytes_equal(cap.upload, 0u, 16u, 255u) ||
        !all_bytes_equal(cap.upload, 16u, 32u, 0u))
        return fail(11, "RGTC array software decode lowering");

    glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_DEPTH, &value);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 12, "array depth query")) != 0 || value != 2)
        return fail(13, "array depth value");
    glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_COMPRESSED, &value);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 14, "array compressed query")) != 0 || value != 1)
        return fail(15, "array compressed value");
    glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &value);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 16, "array size query")) != 0 || value != 16)
        return fail(17, "array compressed size value");
    memset(readback, 0, sizeof(readback));
    glGetCompressedTexImage(GL_TEXTURE_2D_ARRAY, 0, readback);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 18, "array compressed readback")) != 0 ||
        memcmp(readback, array_expected, sizeof(array_expected)) != 0)
        return fail(19, "array compressed readback payload");

    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, 4, 4, 1, GL_COMPRESSED_RED_RGTC1, 8,
                              sub_block);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 20, "array layer subimage")) != 0)
        return rc;
    memcpy(array_expected + 8u, sub_block, 8u);
    if (memcmp(obj->levels[0].data, array_expected, sizeof(array_expected)) != 0)
        return fail(21, "array subimage canonical placement");
    if (cap.tex_image_3d_calls != 2u || cap.upload_size != 32u ||
        !all_bytes_equal(cap.upload, 0u, 16u, 255u) || !all_bytes_equal(cap.upload, 16u, 32u, 64u))
        return fail(22, "array subimage fallback reupload");

    cap.rgtc_extension = 1;
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 1, GL_COMPRESSED_RED_RGTC1, 4, 4, 2, 0, 16,
                           array_data);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 31, "direct RGTC array native path")) != 0)
        return rc;
    if (cap.compressed_3d_calls != 1u || cap.target != GL_TEXTURE_2D_ARRAY ||
        cap.upload_size != 16u || memcmp(cap.upload, array_data, 16u) != 0)
        return fail(32, "direct RGTC array upload lowering");
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 1, 0, 0, 1, 4, 4, 1, GL_COMPRESSED_RED_RGTC1, 8,
                              sub_block);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 33, "direct RGTC array subimage")) != 0)
        return rc;
    if (cap.compressed_sub_3d_calls != 1u || cap.upload_size != 8u ||
        memcmp(cap.upload, sub_block, 8u) != 0)
        return fail(34, "direct RGTC array subimage lowering");
    cap.rgtc_extension = 0;

    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 1, 0, 0, 3, 4, 1, GL_COMPRESSED_RED_RGTC1, 8,
                              sub_block);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 23, "RGTC block alignment validation")) != 0)
        return rc;
    if (memcmp(obj->levels[0].data, array_expected, sizeof(array_expected)) != 0)
        return fail(24, "invalid subimage mutated canonical storage");

    glGenTextures(1, &list_tex);
    list = glGenLists(1);
    if (list_tex == 0u || list == 0u)
        return fail(25, "display-list names");
    glNewList(list, GL_COMPILE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, list_tex);
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 2, 0, 16,
                           list_data);
    glEndList();
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 26, "RGTC array list compile")) != 0)
        return rc;
    memset(list_data, 0x7bu, sizeof(list_data));
    glCallList(list);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 27, "RGTC array list replay")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, list_tex);
    if (obj == NULL || !obj->levels[0].compressed ||
        obj->levels[0].data_size != sizeof(list_expected) ||
        memcmp(obj->levels[0].data, list_expected, sizeof(list_expected)) != 0)
        return fail(28, "RGTC array list payload ownership");

    glBindTexture(GL_TEXTURE_3D, array_tex);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 29, "texture target immutability setup")) ==
        0) {
    } else {
        return rc;
    }
    {
        GLuint tex3d = 0u;
        glGenTextures(1, &tex3d);
        glBindTexture(GL_TEXTURE_3D, tex3d);
        glCompressedTexImage3D(GL_TEXTURE_3D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 1, 0, 8, red_block);
        if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 30,
                               "RGTC true-3D format restriction")) != 0)
            return rc;
        obj = mgl_texture_find(&ctx, tex3d);
        if (obj == NULL || obj->levels[0].defined)
            return fail(35, "RGTC 3D error changed texture");
        glCompressedTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 4, 4, 1, GL_COMPRESSED_RED_RGTC1, 8,
                                  sub_block);
        if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 36,
                               "RGTC true-3D subimage restriction")) != 0)
            return rc;
    }
    {
        GLuint tex3d = 0u;
        unsigned char dxt1[16] = {0x00u, 0xf8u, 0u, 0u, 0u, 0u, 0u, 0u,
                                  0x00u, 0xf8u, 0u, 0u, 0u, 0u, 0u, 0u};
        unsigned char dxt1_sub[8] = {0xe0u, 0x07u, 0u, 0u, 0u, 0u, 0u, 0u};
        glGenTextures(1, &tex3d);
        glBindTexture(GL_TEXTURE_3D, tex3d);
        glCompressedTexImage3DARB(GL_TEXTURE_3D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 2, 0, 16,
                                  dxt1);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 38, "DXT1 true-3D upload")) != 0)
            return rc;
        if (cap.target != GL_TEXTURE_3D || cap.internal_format != GL_RGBA8 ||
            cap.format != GL_RGBA || cap.upload_size != 128u || cap.upload[0] != 255u ||
            cap.upload[1] != 0u || cap.upload[2] != 0u || cap.upload[3] != 255u ||
            cap.upload[64] != 255u)
            return fail(39, "DXT1 true-3D decoded pixels");
        glCompressedTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 1, 4, 4, 1,
                                  GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 8, dxt1_sub);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 40, "DXT1 true-3D subimage")) != 0)
            return rc;
        if (cap.upload_size != 128u || cap.upload[0] != 255u || cap.upload[64] != 0u ||
            cap.upload[65] != 255u || cap.upload[66] != 0u)
            return fail(41, "DXT1 true-3D subimage decoded pixels");
    }

    {
        GLuint tex3d = 0u;
        unsigned char dxt1[16] = {0x00u, 0xf8u, 0u, 0u, 0u, 0u, 0u, 0u,
                                  0x00u, 0xf8u, 0u, 0u, 0u, 0u, 0u, 0u};
        unsigned char green[8] = {0xe0u, 0x07u, 0u, 0u, 0u, 0u, 0u, 0u};
        glGenTextures(1, &tex3d);
        glBindTexture(GL_TEXTURE_3D, tex3d);
        glCompressedTexImage3D(GL_TEXTURE_3D, 0, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, 4, 4, 2, 0, 16,
                               dxt1);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 42, "sRGB DXT1 3D upload")) != 0)
            return rc;
        if (cap.target != GL_TEXTURE_3D || cap.internal_format != GL_SRGB8_ALPHA8 ||
            cap.format != GL_RGBA || cap.upload_size != 128u || cap.upload[0] != 255u ||
            cap.upload[3] != 255u || cap.upload[64] != 255u)
            return fail(43, "sRGB DXT1 3D preserved sRGB storage");
        glCompressedTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 1, 4, 4, 1,
                                  GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, 8, green);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 44, "sRGB DXT1 3D subimage")) != 0)
            return rc;
        if (cap.internal_format != GL_SRGB8_ALPHA8 || cap.upload_size != 128u ||
            cap.upload[0] != 255u || cap.upload[64] != 0u || cap.upload[65] != 255u ||
            cap.upload[67] != 255u)
            return fail(45, "sRGB DXT1 3D subimage redecodes as sRGB");
    }

    {
        GLuint storage_tex = 0u, storage_array = 0u;
        unsigned char result[16] = {0u};
        glCreateTextures(GL_TEXTURE_2D, 1, &storage_tex);
        cap.rgtc_extension = 0;
        glTextureStorage2D(storage_tex, 1, GL_COMPRESSED_RED_RGTC1, 4, 4);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 46, "RGTC immutable fallback storage")) != 0)
            return rc;
        obj = mgl_texture_find(&ctx, storage_tex);
        if (obj == NULL || !obj->immutable_format || !obj->native_immutable_storage ||
            obj->native_compressed_storage || cap.native_storage_format != GL_R8 ||
            !obj->levels[0].compressed || obj->levels[0].data_size != 8u)
            return fail(47, "RGTC fallback storage shadow and GPU format");
        glCompressedTextureSubImage2D(storage_tex, 0, 0, 0, 4, 4, GL_COMPRESSED_RED_RGTC1, 8,
                                      red_block);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 55, "RGTC immutable fallback upload")) != 0 ||
            cap.decoded_sub_2d_calls != 1u || cap.upload_size != 16u ||
            !all_bytes_equal(cap.upload, 0u, 16u, 255u) || cap.native_sub_2d_calls != 0u)
            return fail(56, "RGTC fallback uses decoded subimage");
        glGetCompressedTextureImage(storage_tex, 0, 8, result);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 57, "RGTC immutable fallback readback")) != 0 ||
            memcmp(result, red_block, 8u) != 0)
            return fail(58, "RGTC immutable fallback compressed shadow");
        cap.rgtc_extension = 1;
        glCreateTextures(GL_TEXTURE_2D, 1, &storage_tex);
        glTextureStorage2D(storage_tex, 1, GL_COMPRESSED_RED_RGTC1, 4, 4);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 48, "RGTC native storage")) != 0)
            return rc;
        obj = mgl_texture_find(&ctx, storage_tex);
        if (!obj->native_immutable_storage || !obj->levels[0].compressed ||
            obj->levels[0].data_size != 8u || cap.native_storage_2d_calls != 2u ||
            cap.native_storage_format != GL_COMPRESSED_RED_RGTC1)
            return fail(49, "RGTC immutable compressed metadata and native dispatch");
        glCompressedTextureSubImage2D(storage_tex, 0, 0, 0, 4, 4, GL_COMPRESSED_RED_RGTC1, 8,
                                      red_block);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 50, "RGTC native immutable subimage")) != 0)
            return rc;
        glGetCompressedTextureImage(storage_tex, 0, 8, result);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 51, "RGTC immutable readback")) != 0 ||
            memcmp(result, red_block, 8u) != 0 || cap.native_sub_2d_calls != 1u)
            return fail(52, "RGTC immutable native upload and readback");
        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &storage_array);
        glTextureStorage3D(storage_array, 1, GL_COMPRESSED_RED_RGTC1, 4, 4, 2);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 53, "RGTC array native storage")) != 0)
            return rc;
        obj = mgl_texture_find(&ctx, storage_array);
        if (!obj->native_immutable_storage || obj->levels[0].data_size != 16u ||
            cap.native_storage_3d_calls != 1u)
            return fail(54, "RGTC immutable array metadata and native dispatch");
    }
    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL RGTC compressed texture unit test: PASS");
    return 0;
}

#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    GLuint next_texture;
    const char *extensions;
    GLenum storage_format;
    GLenum upload_target;
    GLenum upload_format;
    GLenum upload_type;
    GLint upload_x, upload_y, upload_z;
    GLsizei upload_width, upload_height, upload_depth;
    unsigned tex_image, tex_sub, compressed_sub, storage_2d, storage_3d;
    GLenum next_error;
    size_t upload_bytes;
    unsigned char bytes[512];
} cap;

static const GLubyte *get_string(GLenum name) {
    return name == GL_EXTENSIONS ? (const GLubyte *)cap.extensions : (const GLubyte *)"";
}

static void active_texture(GLenum target) {
    (void)target;
}

static void gen_textures(GLsizei count, GLuint *out) {
    GLsizei i;
    for (i = 0; i < count; ++i)
        out[i] = ++cap.next_texture;
}

static void delete_textures(GLsizei count, const GLuint *names) {
    (void)count;
    (void)names;
}

static void bind_texture(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void pixel_store(GLenum name, GLint value) {
    (void)name;
    (void)value;
}

static GLenum get_error(void) {
    GLenum error = cap.next_error;
    cap.next_error = GL_NO_ERROR;
    return error;
}

static void upload(GLenum target, GLint x, GLint y, GLint z, GLsizei width, GLsizei height,
                   GLsizei depth, GLenum format, GLenum type, const void *data, size_t size) {
    cap.upload_target = target;
    cap.upload_x = x;
    cap.upload_y = y;
    cap.upload_z = z;
    cap.upload_width = width;
    cap.upload_height = height;
    cap.upload_depth = depth;
    cap.upload_format = format;
    cap.upload_type = type;
    cap.upload_bytes = size;
    if (size <= sizeof(cap.bytes) && data != NULL)
        memcpy(cap.bytes, data, size);
}

static size_t pixels_size(GLsizei width, GLsizei height, GLsizei depth, GLenum format,
                          GLenum type) {
    size_t components = format == GL_RGBA ? 4u : format == GL_RGB ? 3u : format == GL_RG ? 2u : 1u;
    return (size_t)width * (size_t)height * (size_t)depth * components *
           (type == GL_HALF_FLOAT ? 2u : 1u);
}

static void tex_storage_2d(GLenum target, GLsizei levels, GLenum format, GLsizei width,
                           GLsizei height) {
    (void)target;
    (void)levels;
    (void)width;
    (void)height;
    ++cap.storage_2d;
    cap.storage_format = format;
}

static void tex_storage_3d(GLenum target, GLsizei levels, GLenum format, GLsizei width,
                           GLsizei height, GLsizei depth) {
    (void)target;
    (void)levels;
    (void)width;
    (void)height;
    (void)depth;
    ++cap.storage_3d;
    cap.storage_format = format;
}

static void tex_image_2d(GLenum target, GLint level, GLint internalformat, GLsizei width,
                         GLsizei height, GLint border, GLenum format, GLenum type,
                         const void *data) {
    (void)level;
    (void)internalformat;
    (void)border;
    ++cap.tex_image;
    if (data)
        upload(target, 0, 0, 0, width, height, 1, format, type, data,
               pixels_size(width, height, 1, format, type));
}

static void tex_image_3d(GLenum target, GLint level, GLint internalformat, GLsizei width,
                         GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type,
                         const void *data) {
    (void)level;
    (void)internalformat;
    (void)border;
    ++cap.tex_image;
    if (data)
        upload(target, 0, 0, 0, width, height, depth, format, type, data,
               pixels_size(width, height, depth, format, type));
}

static void tex_sub_2d(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
                       GLenum format, GLenum type, const void *data) {
    (void)level;
    ++cap.tex_sub;
    upload(target, x, y, 0, width, height, 1, format, type, data,
           pixels_size(width, height, 1, format, type));
}

static void tex_sub_3d(GLenum target, GLint level, GLint x, GLint y, GLint z, GLsizei width,
                       GLsizei height, GLsizei depth, GLenum format, GLenum type,
                       const void *data) {
    (void)level;
    ++cap.tex_sub;
    upload(target, x, y, z, width, height, depth, format, type, data,
           pixels_size(width, height, depth, format, type));
}

static void compressed_image_2d(GLenum target, GLint level, GLenum format, GLsizei width,
                                GLsizei height, GLint border, GLsizei size, const void *data) {
    (void)level;
    (void)border;
    upload(target, 0, 0, 0, width, height, 1, format, 0, data, (size_t)size);
}

static void compressed_sub_2d(GLenum target, GLint level, GLint x, GLint y, GLsizei width,
                              GLsizei height, GLenum format, GLsizei size, const void *data) {
    (void)level;
    ++cap.compressed_sub;
    upload(target, x, y, 0, width, height, 1, format, 0, data, (size_t)size);
}

static void compressed_sub_3d(GLenum target, GLint level, GLint x, GLint y, GLint z, GLsizei width,
                              GLsizei height, GLsizei depth, GLenum format, GLsizei size,
                              const void *data) {
    (void)level;
    ++cap.compressed_sub;
    upload(target, x, y, z, width, height, depth, format, 0, data, (size_t)size);
}

static int check(struct mgl_context *ctx, int condition, int line) {
    GLenum error = mgl_error_take(&ctx->errors);
    if (!condition || error != GL_NO_ERROR) {
        fprintf(stderr, "compressed storage failure at %d (GL 0x%x)\n", line, (unsigned)error);
        return 0;
    }
    return 1;
}

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!check(&ctx, (test), __LINE__))                                                        \
            return 1;                                                                              \
    } while (0)

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj;
    const GLenum formats[] = {
        GL_COMPRESSED_RGB_S3TC_DXT1_EXT,  GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
        GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_COMPRESSED_RED_RGTC1,
        GL_COMPRESSED_SIGNED_RG_RGTC2,    GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT,
        GL_COMPRESSED_RGBA_BPTC_UNORM,    GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM};
    const GLenum fallbacks[] = {GL_RGBA8,     GL_RGBA8,  GL_RGBA8, GL_R8,
                                GL_RG8_SNORM, GL_RGB16F, GL_RGBA8, GL_SRGB8_ALPHA8};
    GLuint tex;
    unsigned char block[16] = {0x00u, 0xf8u};
    unsigned char result[64];
    unsigned i;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.extensions = "GL_EXT_mock";
    native.glesGetString = get_string;
    native.glesActiveTexture = active_texture;
    native.glesGenTextures = gen_textures;
    native.glesDeleteTextures = delete_textures;
    native.glesBindTexture = bind_texture;
    native.glesPixelStorei = pixel_store;
    native.glesGetError = get_error;
    native.glesTexStorage2D = tex_storage_2d;
    native.glesTexStorage3D = tex_storage_3d;
    native.glesTexImage2D = tex_image_2d;
    native.glesTexImage3D = tex_image_3d;
    native.glesTexSubImage2D = tex_sub_2d;
    native.glesTexSubImage3D = tex_sub_3d;
    native.glesCompressedTexImage2D = compressed_image_2d;
    native.glesCompressedTexSubImage2D = compressed_sub_2d;
    native.glesCompressedTexSubImage3D = compressed_sub_3d;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.unpack_alignment = 4;
    ctx.pack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    for (i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
        size_t bytes = i == 0u || i == 3u ? 8u : 16u;
        unsigned before_images = cap.tex_image, before_sub = cap.tex_sub;
        glCreateTextures(GL_TEXTURE_2D, 1, &tex);
        glTextureStorage2D(tex, 2, formats[i], 8, 4);
        obj = mgl_texture_find(&ctx, tex);
        CHECK(obj && obj->immutable_format && obj->native_immutable_storage &&
              !obj->native_compressed_storage && obj->levels[0].compressed &&
              obj->levels[0].data_size == 2u * bytes && cap.storage_format == fallbacks[i] &&
              cap.tex_image == before_images);
        glCompressedTextureSubImage2D(tex, 0, 4, 0, 4, 4, formats[i], (GLsizei)bytes, block);
        CHECK(cap.tex_sub == before_sub + 1u && cap.upload_x == 4 && cap.upload_y == 0 &&
              cap.upload_width == 4 && cap.upload_height == 4 && cap.tex_image == before_images);
        memset(result, 0, sizeof(result));
        glGetCompressedTextureImage(tex, 0, (GLsizei)sizeof(result), result);
        CHECK(memcmp(result + bytes, block, bytes) == 0 &&
              memcmp(result, (unsigned char[16]){0}, bytes) == 0);
        {
            GLuint destination;
            glCreateTextures(GL_TEXTURE_2D, 1, &destination);
            glTextureStorage2D(destination, 2, formats[i], 8, 4);
            CHECK(1);
            glCopyImageSubData(tex, GL_TEXTURE_2D, 0, 4, 0, 0, destination, GL_TEXTURE_2D, 0, 0, 0,
                               0, 4, 4, 1);
            CHECK(cap.upload_format == (i == 3u   ? GL_RED
                                        : i == 4u ? GL_RG
                                        : i == 5u ? GL_RGB
                                                  : GL_RGBA) &&
                  cap.upload_x == 0);
            memset(result, 0, sizeof(result));
            glGetCompressedTextureImage(destination, 0, (GLsizei)sizeof(result), result);
            CHECK(memcmp(result, block, bytes) == 0);
            glDeleteTextures(1, &destination);
        }
        glDeleteTextures(1, &tex);
        CHECK(1);
    }
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &tex);
    glTextureStorage3D(tex, 1, GL_COMPRESSED_RGBA_BPTC_UNORM, 4, 4, 2);
    obj = mgl_texture_find(&ctx, tex);
    CHECK(obj && obj->levels[0].data_size == 32u && cap.storage_format == GL_RGBA8);
    glCompressedTextureSubImage3D(tex, 0, 0, 0, 1, 4, 4, 1, GL_COMPRESSED_RGBA_BPTC_UNORM, 16,
                                  block);
    CHECK(cap.upload_target == GL_TEXTURE_2D_ARRAY && cap.upload_z == 1 && cap.upload_depth == 1 &&
          cap.upload_format == GL_RGBA);
    memset(result, 0, sizeof(result));
    glGetCompressedTextureImage(tex, 0, 32, result);
    CHECK(memcmp(result + 16, block, 16) == 0);
    glDeleteTextures(1, &tex);

    glCreateTextures(GL_TEXTURE_3D, 1, &tex);
    glTextureStorage3D(tex, 1, GL_COMPRESSED_RGBA_BPTC_UNORM, 4, 4, 2);
    CHECK(cap.storage_format == GL_RGBA8);
    glCompressedTextureSubImage3D(tex, 0, 0, 0, 1, 4, 4, 1, GL_COMPRESSED_RGBA_BPTC_UNORM, 16,
                                  block);
    CHECK(cap.upload_target == GL_TEXTURE_3D && cap.upload_z == 1 && cap.upload_depth == 1 &&
          cap.upload_format == GL_RGBA);
    glDeleteTextures(1, &tex);

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex);
    glTextureStorage2D(tex, 1, GL_COMPRESSED_RED_RGTC1, 4, 4);
    CHECK(cap.storage_format == GL_R8);
    glCompressedTextureSubImage3D(tex, 0, 0, 0, 1, 4, 4, 1, GL_COMPRESSED_RED_RGTC1, 8, block);
    CHECK(cap.upload_target == GL_TEXTURE_CUBE_MAP_NEGATIVE_X && cap.upload_format == GL_RED);
    glDeleteTextures(1, &tex);

    glCreateTextures(GL_TEXTURE_1D, 1, &tex);
    glTextureStorage1D(tex, 1, GL_COMPRESSED_RGBA_BPTC_UNORM, 4);
    CHECK(mgl_error_take(&ctx.errors) == GL_INVALID_ENUM);
    obj = mgl_texture_find(&ctx, tex);
    CHECK(obj && !obj->immutable_format);
    glCompressedTextureSubImage1D(tex, 0, 0, 4, GL_COMPRESSED_RGBA_BPTC_UNORM, 16, block);
    CHECK(mgl_error_take(&ctx.errors) == GL_INVALID_ENUM);
    glDeleteTextures(1, &tex);

    glCreateTextures(GL_TEXTURE_3D, 1, &tex);
    glTextureStorage3D(tex, 1, GL_COMPRESSED_RED_RGTC1, 4, 4, 2);
    CHECK(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION);
    obj = mgl_texture_find(&ctx, tex);
    CHECK(obj && !obj->immutable_format);
    glDeleteTextures(1, &tex);

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 5, GL_COMPRESSED_RGBA_BPTC_UNORM, 4, 4);
    CHECK(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION);
    obj = mgl_texture_find(&ctx, tex);
    CHECK(obj && !obj->immutable_format);
    cap.next_error = GL_OUT_OF_MEMORY;
    glTextureStorage2D(tex, 1, GL_COMPRESSED_RGBA_BPTC_UNORM, 4, 4);
    CHECK(mgl_error_take(&ctx.errors) == GL_OUT_OF_MEMORY);
    CHECK(!obj->immutable_format && !obj->levels[0].defined);
    glTextureStorage2D(tex, 1, GL_COMPRESSED_RGBA_BPTC_UNORM, 4, 4);
    CHECK(obj->immutable_format && obj->levels[0].defined);
    glDeleteTextures(1, &tex);

    cap.extensions = "GL_EXT_texture_compression_rgtc GL_EXT_texture_compression_s3tc "
                     "GL_EXT_texture_compression_bptc";
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 1, GL_COMPRESSED_RG_RGTC2, 4, 4);
    obj = mgl_texture_find(&ctx, tex);
    CHECK(obj && obj->native_compressed_storage && cap.storage_format == GL_COMPRESSED_RG_RGTC2);
    glCompressedTextureSubImage2D(tex, 0, 0, 0, 4, 4, GL_COMPRESSED_RG_RGTC2, 16, block);
    CHECK(cap.compressed_sub != 0u && cap.upload_format == GL_COMPRESSED_RG_RGTC2);
    glDeleteTextures(1, &tex);

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &tex);
    glTextureStorage3D(tex, 1, GL_COMPRESSED_RGBA_BPTC_UNORM, 4, 4, 2);
    obj = mgl_texture_find(&ctx, tex);
    CHECK(obj && obj->native_compressed_storage &&
          cap.storage_format == GL_COMPRESSED_RGBA_BPTC_UNORM);
    {
        unsigned before = cap.compressed_sub;
        glCompressedTextureSubImage3D(tex, 0, 0, 0, 1, 4, 4, 1, GL_COMPRESSED_RGBA_BPTC_UNORM, 16,
                                      block);
        CHECK(cap.compressed_sub == before + 1u && cap.upload_target == GL_TEXTURE_2D_ARRAY &&
              cap.upload_z == 1 && cap.upload_format == GL_COMPRESSED_RGBA_BPTC_UNORM);
    }
    glDeleteTextures(1, &tex);

    cap.extensions = "";
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 1, GL_COMPRESSED_RED_RGTC1, 4, 4);
    obj = mgl_texture_find(&ctx, tex);
    CHECK(obj && !obj->native_compressed_storage && cap.storage_format == GL_R8);
    glCompressedTextureSubImage2D(tex, 0, 0, 0, 4, 4, GL_COMPRESSED_RED_RGTC1, 8, block);
    CHECK(cap.upload_format == GL_RED && cap.upload_bytes == 16u);
    glDeleteTextures(1, &tex);

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("compressed BCn immutable storage transcoder: PASS");
    return 0;
}

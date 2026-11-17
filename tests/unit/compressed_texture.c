#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    GLuint next_buffer;
    unsigned compressed_upload_calls;
    unsigned compressed_sub_calls;
    unsigned decoded_upload_calls;
    GLenum decoded_internal, decoded_format, decoded_type;
    unsigned char decoded[4u * 4u * 6u];
    unsigned char upload[128];
    size_t upload_size;
    GLenum upload_format;
    GLsizei upload_width;
    GLsizei upload_height;
} cap;

static void fake_active_texture(GLenum texture) {
    (void)texture;
}

static const GLubyte *fake_get_string(GLenum name) {
    return name == GL_EXTENSIONS ? (const GLubyte *)"GL_EXT_texture_compression_s3tc" : NULL;
}

static const GLubyte *fake_get_string_empty(GLenum name) {
    return name == GL_EXTENSIONS ? (const GLubyte *)"" : NULL;
}

static const GLubyte *fake_get_string_srgb(GLenum name) {
    return name == GL_EXTENSIONS
               ? (const GLubyte
                      *)"GL_EXT_texture_compression_s3tc GL_EXT_texture_compression_s3tc_srgb"
               : NULL;
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

static void fake_compressed_image(GLenum target, GLint level, GLenum format, GLsizei width,
                                  GLsizei height, GLint border, GLsizei image_size,
                                  const void *data) {
    (void)target;
    (void)level;
    (void)border;
    ++cap.compressed_upload_calls;
    cap.upload_size = (size_t)image_size;
    cap.upload_format = format;
    cap.upload_width = width;
    cap.upload_height = height;
    if (image_size > 0 && data != NULL && (size_t)image_size <= sizeof(cap.upload)) {
        memcpy(cap.upload, data, (size_t)image_size);
    }
}

static void fake_compressed_sub(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                GLsizei width, GLsizei height, GLenum format, GLsizei image_size,
                                const void *data) {
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    ++cap.compressed_sub_calls;
    fake_compressed_image(GL_TEXTURE_2D, level, format, width, height, 0, image_size, data);
}

static void fake_uncompressed_image(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                    GLsizei height, GLint border, GLenum format, GLenum type,
                                    const void *data) {
    size_t bytes_per_pixel = type == GL_HALF_FLOAT ? 6u : 4u;
    (void)target;
    (void)level;
    (void)border;
    ++cap.decoded_upload_calls;
    cap.decoded_internal = (GLenum)internalformat;
    cap.decoded_format = format;
    cap.decoded_type = type;
    if (data != NULL && width == 4 && height == 4)
        memcpy(cap.decoded, data, 16u * bytes_per_pixel);
}

static void fake_gen_buffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_buffer;
}

static void fake_delete_buffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)target;
    (void)size;
    (void)data;
    (void)usage;
}

static void fake_buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size,
                                 const void *data) {
    (void)target;
    (void)offset;
    (void)size;
    (void)data;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "compressed texture failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj;
    struct mgl_buffer *pbo_obj;
    GLuint texture = 0u;
    GLuint pbo = 0u;
    GLuint list_texture = 0u;
    GLuint list = 0u;
    GLint value = 0;
    unsigned char source[32];
    unsigned char expected[32];
    unsigned char readback[32];
    unsigned char sub[8] = {201u, 202u, 203u, 204u, 205u, 206u, 207u, 208u};
    unsigned char pbo_source[25];
    unsigned char pbo_expected[16];
    unsigned char list_source[8] = {31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u};
    const unsigned char list_expected[8] = {31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u};
    size_t i;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    for (i = 0u; i < sizeof(source); ++i)
        source[i] = (unsigned char)(i + 1u);
    memcpy(expected, source, sizeof(expected));
    memset(pbo_source, 0xee, sizeof(pbo_source));
    for (i = 0u; i < sizeof(pbo_expected); ++i) {
        pbo_expected[i] = (unsigned char)(100u + i);
        pbo_source[5u + i] = pbo_expected[i];
    }

    native.glesGetString = fake_get_string;
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesPixelStorei = fake_pixel_store;
    native.glesTexImage2D = fake_uncompressed_image;
    native.glesCompressedTexImage2D = fake_compressed_image;
    native.glesCompressedTexSubImage2D = fake_compressed_sub;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesBufferSubData = fake_buffer_sub_data;

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

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 8, 8, 0,
                           (GLsizei)sizeof(source), source);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(2, "base upload error");
    obj = mgl_bound_texture_2d(&ctx);
    if (obj == NULL || !obj->levels[0].defined || !obj->levels[0].compressed ||
        obj->levels[0].depth != 1 || obj->levels[0].data_size != sizeof(expected) ||
        memcmp(obj->levels[0].data, expected, sizeof(expected)) != 0)
        return fail(3, "canonical compressed level");
    if (cap.compressed_upload_calls != 1u || cap.upload_size != sizeof(expected) ||
        cap.upload_format != GL_COMPRESSED_RGBA_S3TC_DXT1_EXT || cap.upload_width != 8 ||
        cap.upload_height != 8 || memcmp(cap.upload, expected, sizeof(expected)) != 0)
        return fail(4, "native compressed upload");

    memset(source, 0xa5, sizeof(source));
    memset(readback, 0, sizeof(readback));
    glGetCompressedTexImage(GL_TEXTURE_2D, 0, readback);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(readback, expected, sizeof(expected)) != 0)
        return fail(5, "canonical compressed readback ownership");
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &value);
    if (value != 1)
        return fail(6, "compressed level query");
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &value);
    if (value != (GLint)sizeof(expected))
        return fail(7, "compressed image size query");

    memset(readback, 0xe7, sizeof(readback));
    glGetnCompressedTexImage(GL_TEXTURE_2D, 0, (GLsizei)sizeof(expected) - 1, readback);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || readback[0] != 0xe7u)
        return fail(41, "bounded compressed readback rejects insufficient size");
    glGetnCompressedTexImage(GL_TEXTURE_2D, 0, (GLsizei)sizeof(expected), readback);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(readback, expected, sizeof(expected)) != 0)
        return fail(42, "bounded compressed readback succeeds with exact size");
    mgl_error_set(&ctx.errors, GL_INVALID_ENUM);
    memset(readback, 0xe7, sizeof(readback));
    glGetnCompressedTexImage(GL_TEXTURE_2D, 0, (GLsizei)sizeof(expected), readback);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM ||
        memcmp(readback, expected, sizeof(expected)) != 0)
        return fail(44, "pending error does not suppress bounded readback");
    glGetnCompressedTexImage(GL_TEXTURE_2D, 0, -1, readback);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(43, "bounded compressed readback validates negative size");

    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 4, 4, 4, 4, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
                              (GLsizei)sizeof(sub), sub);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(8, "block subimage error");
    memcpy(expected + 24u, sub, sizeof(sub));
    if (memcmp(obj->levels[0].data, expected, sizeof(expected)) != 0)
        return fail(9, "block subimage canonical placement");
    if (cap.compressed_sub_calls != 1u || cap.upload_size != sizeof(sub) ||
        memcmp(cap.upload, sub, sizeof(sub)) != 0)
        return fail(10, "native compressed subimage");

    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, (GLsizeiptr)sizeof(pbo_source), pbo_source,
                 GL_STATIC_DRAW);
    obj->levels[1].shadow_stale = GL_TRUE;
    glCompressedTexImage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 8, 4, 0,
                           (GLsizei)sizeof(pbo_expected), (const void *)(uintptr_t)5u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || !obj->levels[1].compressed ||
        obj->levels[1].depth != 1 || obj->levels[1].shadow_stale ||
        obj->levels[1].data_size != sizeof(pbo_expected) ||
        memcmp(obj->levels[1].data, pbo_expected, sizeof(pbo_expected)) != 0)
        return fail(11, "PBO compressed upload uses logical offset");

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0u);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, 32, NULL, GL_STATIC_DRAW);
    glGetCompressedTexImage(GL_TEXTURE_2D, 1, (void *)(uintptr_t)3u);
    pbo_obj = mgl_buffer_find(&ctx, pbo);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || pbo_obj == NULL || pbo_obj->shadow == NULL ||
        memcmp(pbo_obj->shadow + 3u, pbo_expected, sizeof(pbo_expected)) != 0)
        return fail(12, "PBO compressed readback uses logical offset");
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0u);

    glGenTextures(1, &list_texture);
    list = glGenLists(1);
    if (list_texture == 0u || list == 0u)
        return fail(13, "list names");
    glNewList(list, GL_COMPILE);
    glBindTexture(GL_TEXTURE_2D, list_texture);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
                           (GLsizei)sizeof(list_source), list_source);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(14, "compressed list compile");
    memset(list_source, 0x77, sizeof(list_source));
    glCallList(list);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(15, "compressed list replay");
    obj = mgl_texture_find(&ctx, list_texture);
    if (obj == NULL || !obj->levels[0].compressed ||
        obj->levels[0].data_size != sizeof(list_expected) ||
        memcmp(obj->levels[0].data, list_expected, sizeof(list_expected)) != 0)
        return fail(16, "compressed list did not own compile-time payload");
    memset(obj->levels[0].data, 0, obj->levels[0].data_size);
    glCallList(list);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(obj->levels[0].data, list_expected, sizeof(list_expected)) != 0)
        return fail(17, "compressed list repeated replay ownership");

    glDeleteLists(list, 1);

    {
        const unsigned char red_bc1[8] = {0x00u, 0xf8u, 0x00u, 0x00u};
        const unsigned char green_bc1[8] = {0xe0u, 0x07u, 0x00u, 0x00u};
        const unsigned char zero_bc3[16] = {0u};
        const unsigned char zero_bc7[16] = {0u};
        unsigned compressed_before = cap.compressed_upload_calls;
        GLuint fallback = 0u;
        unsigned i_pixel;
        native.glesGetString = fake_get_string_empty;
        glGenTextures(1, &fallback);
        glBindTexture(GL_TEXTURE_2D, fallback);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 4, 4, 0,
                               (GLsizei)sizeof(red_bc1), red_bc1);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.decoded_upload_calls != 1u ||
            cap.decoded_internal != GL_RGBA8 || cap.decoded_format != GL_RGBA ||
            cap.decoded_type != GL_UNSIGNED_BYTE ||
            cap.compressed_upload_calls != compressed_before)
            return fail(18, "BC1 missing extension falls back to RGBA8");
        for (i_pixel = 0u; i_pixel < 16u; ++i_pixel)
            if (cap.decoded[4u * i_pixel] != 255u || cap.decoded[4u * i_pixel + 1u] != 0u ||
                cap.decoded[4u * i_pixel + 2u] != 0u || cap.decoded[4u * i_pixel + 3u] != 255u)
                return fail(19, "BC1 decoded color");
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 8,
                                  green_bc1);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.decoded_upload_calls != 2u ||
            cap.decoded[0] != 0u || cap.decoded[1] != 255u || cap.decoded[2] != 0u ||
            cap.compressed_upload_calls != compressed_before)
            return fail(20, "BC1 fallback subimage redecodes current level");
        glGetCompressedTexImage(GL_TEXTURE_2D, 0, readback);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || memcmp(readback, green_bc1, 8u) != 0)
            return fail(21, "BC1 original compressed data retained");
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 4, 4, 0, 16,
                               zero_bc3);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.decoded_upload_calls != 3u ||
            cap.compressed_upload_calls != compressed_before)
            return fail(22, "BC3 falls back when unsupported");
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM, 4, 4, 0, 16,
                               zero_bc7);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.decoded_upload_calls != 4u ||
            cap.decoded_internal != GL_RGBA8 || cap.compressed_upload_calls != compressed_before)
            return fail(23, "BC7 falls back when unsupported");
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, 4, 4, 0, 16,
                               zero_bc7);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.decoded_upload_calls != 5u ||
            cap.decoded_internal != GL_RGB16F || cap.decoded_format != GL_RGB ||
            cap.decoded_type != GL_HALF_FLOAT || cap.compressed_upload_calls != compressed_before)
            return fail(24, "BC6H falls back to RGB16F");
        glDeleteTextures(1, &fallback);
    }

    {
        const GLenum formats[] = {
            GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT,
            GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT};
        const unsigned char red_bc1[8] = {0x00u, 0xf8u, 0x00u, 0x00u};
        const unsigned char green_bc1[8] = {0xe0u, 0x07u, 0x00u, 0x00u};
        const unsigned char red_bc2[16] = {0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu,
                                           0x00u, 0xf8u, 0x00u, 0x00u, 0u,    0u,    0u,    0u};
        const unsigned char red_bc3[16] = {0xffu, 0xffu, 0u,    0u,    0u, 0u, 0u, 0u,
                                           0x00u, 0xf8u, 0x00u, 0x00u, 0u, 0u, 0u, 0u};
        const unsigned char *blocks[] = {red_bc1, red_bc1, red_bc2, red_bc3};
        GLuint tex = 0u;
        size_t k;
        native.glesGetString = fake_get_string;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        for (k = 0u; k < sizeof(formats) / sizeof(formats[0]); ++k) {
            size_t bytes = k < 2u ? 8u : 16u;
            unsigned old_decoded = cap.decoded_upload_calls;
            unsigned old_compressed = cap.compressed_upload_calls;
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, formats[k], 4, 4, 0, (GLsizei)bytes,
                                   blocks[k]);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                cap.decoded_upload_calls != old_decoded + 1u ||
                cap.compressed_upload_calls != old_compressed ||
                cap.decoded_internal != GL_SRGB8_ALPHA8 || cap.decoded_format != GL_RGBA ||
                cap.decoded_type != GL_UNSIGNED_BYTE || cap.decoded[0] != 255u ||
                cap.decoded[1] != 0u || cap.decoded[2] != 0u || cap.decoded[3] != 255u)
                return fail(25,
                            "sRGB S3TC fallback must use sRGB storage with correct color/alpha");
            memset(readback, 0, sizeof(readback));
            glGetCompressedTexImage(GL_TEXTURE_2D, 0, readback);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                memcmp(readback, blocks[k], bytes) != 0)
                return fail(26, "sRGB S3TC compressed payload readback");
        }
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, formats[0], 4, 4, 0, 7, red_bc1);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
            return fail(27, "sRGB S3TC block byte-size validation");
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, formats[0], 4, 4, 0, 8, red_bc1);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(28, "sRGB BC1 base for subimage");
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, formats[0], 8, green_bc1);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.decoded_internal != GL_SRGB8_ALPHA8 ||
            cap.decoded[0] != 0u || cap.decoded[1] != 255u || cap.decoded[2] != 0u ||
            cap.decoded[3] != 255u)
            return fail(29, "sRGB BC1 fallback subimage");
        memset(readback, 0, sizeof(readback));
        glGetCompressedTexImage(GL_TEXTURE_2D, 0, readback);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || memcmp(readback, green_bc1, 8u) != 0)
            return fail(30, "sRGB BC1 subimage compressed data retained");
        native.glesGetString = fake_get_string_srgb;
        for (k = 0u; k < sizeof(formats) / sizeof(formats[0]); ++k) {
            size_t bytes = k < 2u ? 8u : 16u;
            unsigned old_decoded = cap.decoded_upload_calls;
            unsigned old_compressed = cap.compressed_upload_calls;
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, formats[k], 4, 4, 0, (GLsizei)bytes,
                                   blocks[k]);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                cap.compressed_upload_calls != old_compressed + 1u ||
                cap.decoded_upload_calls != old_decoded || cap.upload_format != formats[k])
                return fail(31, "native sRGB S3TC native upload");
        }
        glDeleteTextures(1, &tex);
    }

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL compressed texture unit test: PASS");
    return 0;
}

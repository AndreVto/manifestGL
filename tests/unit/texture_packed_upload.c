#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static struct {
    GLuint next;
    GLenum target, format, type;
    GLint internal;
    unsigned image_calls, subimage_calls;
    unsigned char bytes[64];
} cap;

static void fake_gen(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i)
        names[i] = ++cap.next;
}

static void fake_bind(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_delete(GLsizei count, const GLuint *names) {
    (void)count;
    (void)names;
}

static void fake_active(GLenum texture) {
    (void)texture;
}

static void fake_parameter(GLenum target, GLenum name, GLint value) {
    (void)target;
    (void)name;
    (void)value;
}

static void capture(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height,
                    GLint border, GLenum format, GLenum type, const void *data) {
    size_t count = format == GL_RGB ? 3u : 4u;
    size_t size = (size_t)width * (size_t)height * count;
    (void)level;
    (void)border;
    if (type == GL_FLOAT)
        size *= sizeof(float);
    else if (type == GL_UNSIGNED_INT_2_10_10_10_REV)
        size = (size_t)width * (size_t)height * 4u;
    cap.target = target;
    cap.internal = internal;
    cap.format = format;
    cap.type = type;
    ++cap.image_calls;
    if (data != NULL && size <= sizeof(cap.bytes))
        memcpy(cap.bytes, data, size);
}

static void capture_3d(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height,
                       GLsizei depth, GLint border, GLenum format, GLenum type, const void *data) {
    capture(target, level, internal, width, height * depth, border, format, type, data);
}

static void capture_sub(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
                        GLenum format, GLenum type, const void *data) {
    (void)x;
    (void)y;
    capture(target, level, 0, width, height, 0, format, type, data);
    ++cap.subimage_calls;
}

static void capture_sub_3d(GLenum target, GLint level, GLint x, GLint y, GLint z, GLsizei width,
                           GLsizei height, GLsizei depth, GLenum format, GLenum type,
                           const void *data) {
    (void)x;
    (void)y;
    (void)z;
    capture(target, level, 0, width, height * depth, 0, format, type, data);
    ++cap.subimage_calls;
}

static int check(int condition, const char *label) {
    if (condition)
        return 1;
    fprintf(stderr, "packed upload: %s\n", label);
    return 0;
}

static int check_bytes(const unsigned char *expected, size_t count, const char *label) {
    if (memcmp(cap.bytes, expected, count) == 0)
        return 1;
    fprintf(stderr, "packed upload %s: got", label);
    for (size_t i = 0u; i < count; ++i)
        fprintf(stderr, " %02x", cap.bytes[i]);
    fputs("\n", stderr);
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    const unsigned char rgb_expected[] = {255u, 0u, 255u};
    const unsigned char rgba_expected[] = {255u, 0u, 68u, 170u};
    const uint8_t rgb332 = 0xe3u, rgb332_rev = 0xc7u;
    const uint16_t rgb565 = 0xf81fu, rgb565_rev = 0xf81fu;
    const uint16_t rgba4444 = 0xf04au, rgba4444_rev = 0xa40fu;
    const uint16_t rgba5551 = 0xf802u, rgba5551_rev = 0x041fu;
    const uint32_t rgba8888 = 0xff0044aau, rgba8888_rev = 0xaa4400ffu;
    const uint32_t rgba1010102 = 0xffc003ffu, rgba1010102_rev = 0xcff003ffu;
    const uint32_t rgbfloat = (0x1e0u << 22u) | (0x3c0u << 11u) | 0x3c0u;
    const uint32_t rgbe5 = (16u << 27u) | (256u << 18u) | (256u << 9u) | 256u;

    const struct {
        const char *label;
        GLenum format, type;
        const void *data;
        unsigned char expected[4];
        size_t size;
    } cases[] = {
        {"332", GL_RGB, GL_UNSIGNED_BYTE_3_3_2, &rgb332, {255u, 0u, 255u}, 3u},
        {"332 rev", GL_RGB, GL_UNSIGNED_BYTE_2_3_3_REV, &rgb332_rev, {255u, 0u, 255u}, 3u},
        {"565", GL_RGB, GL_UNSIGNED_SHORT_5_6_5, &rgb565, {255u, 0u, 255u}, 3u},
        {"565 rev", GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, &rgb565_rev, {255u, 0u, 255u}, 3u},
        {"4444", GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, &rgba4444, {255u, 0u, 68u, 170u}, 4u},
        {"4444 rev",
         GL_RGBA,
         GL_UNSIGNED_SHORT_4_4_4_4_REV,
         &rgba4444_rev,
         {255u, 0u, 68u, 170u},
         4u},
        {"5551", GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, &rgba5551, {255u, 0u, 8u, 0u}, 4u},
        {"5551 rev", GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, &rgba5551_rev, {255u, 0u, 8u, 0u}, 4u},
        {"8888", GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, &rgba8888, {255u, 0u, 68u, 170u}, 4u},
        {"8888 rev",
         GL_RGBA,
         GL_UNSIGNED_INT_8_8_8_8_REV,
         &rgba8888_rev,
         {255u, 0u, 68u, 170u},
         4u},
        {"1010102", GL_RGBA, GL_UNSIGNED_INT_10_10_10_2, &rgba1010102, {255u, 0u, 64u, 255u}, 4u},
        {"1010102 rev",
         GL_RGBA,
         GL_UNSIGNED_INT_2_10_10_10_REV,
         &rgba1010102_rev,
         {255u, 0u, 64u, 255u},
         4u},
        {"10f11f", GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, &rgbfloat, {255u, 255u, 255u}, 3u},
        {"9995", GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, &rgbe5, {255u, 255u, 255u}, 3u},
    };

    const uint16_t bgra16[4] = {65535u, 1u, 32768u, 65535u};
    const uint16_t swap565 = 0x1ff8u;
    const GLfloat bgra_float[4] = {0.5f, 1.5f, 2.0f, 1.0f};
    const GLfloat bgr_float[3] = {0.5f, 1.5f, 2.0f};
    const uint8_t row_packed[6] = {0u, 0xe3u, 0u, 0u, 0u, 0u};
    const uint32_t hdrfloat = (0x1c0u << 22u) | (0x3c0u << 11u) | 0x400u;
    GLuint tex;
    int ok = 1;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGenTextures = fake_gen;
    native.glesBindTexture = fake_bind;
    native.glesDeleteTextures = fake_delete;
    native.glesActiveTexture = fake_active;
    native.glesTexParameteri = fake_parameter;
    native.glesTexImage2D = capture;
    native.glesTexImage3D = capture_3d;
    native.glesTexSubImage2D = capture_sub;
    native.glesTexSubImage3D = capture_sub_3d;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 1;
    mgl_context_set_current_for_test(&ctx);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        glTexImage2D(GL_TEXTURE_2D, 0, cases[i].format == GL_RGB ? GL_RGB8 : GL_RGBA8, 1, 1, 0,
                     cases[i].format, cases[i].type, cases[i].data);
        ok &= check(glGetError() == GL_NO_ERROR, cases[i].label);
        ok &= check(cap.type == GL_UNSIGNED_BYTE && cap.format == cases[i].format, cases[i].label);
        ok &= check_bytes(cases[i].expected, cases[i].size, cases[i].label);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, 1, 1, 0, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2,
                 &rgba1010102);
    ok &= check(glGetError() == GL_NO_ERROR && cap.type == GL_UNSIGNED_INT_2_10_10_10_REV,
                "10-bit storage keeps precision");
    ok &= check(memcmp(cap.bytes, &rgba1010102_rev, 4u) == 0, "10-bit packed reversal");
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2,
                    &rgba1010102);
    ok &= check(glGetError() == GL_NO_ERROR && cap.type == GL_UNSIGNED_INT_2_10_10_10_REV,
                "10-bit subimage keeps precision");
    ok &= check(memcmp(cap.bytes, &rgba1010102_rev, 4u) == 0, "10-bit subimage bits");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, &rgba4444);
    ok &= check(glGetError() == GL_INVALID_OPERATION, "invalid packed format/type error");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_expected);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE_2_3_3_REV, &rgb332_rev);
    ok &= check(glGetError() == GL_NO_ERROR && cap.subimage_calls > 0u, "packed subimage");
    ok &= check(cap.format == GL_RGBA && cap.type == GL_UNSIGNED_BYTE,
                "subimage keeps existing image format");
    ok &= check(cap.bytes[0] == rgb_expected[0] && cap.bytes[2] == rgb_expected[2],
                "packed subimage values");
    glTextureSubImage2D(tex, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4_REV, &rgba4444_rev);
    ok &= check(glGetError() == GL_NO_ERROR && cap.bytes[0] == 255u && cap.bytes[2] == 68u,
                "DSA packed subimage");
    glTextureSubImage2DEXT(tex, GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA,
                           GL_UNSIGNED_SHORT_4_4_4_4_REV, &rgba4444_rev);
    ok &= check(glGetError() == GL_NO_ERROR && cap.format == GL_RGBA, "EXT DSA packed subimage");
    ok &= check(cap.bytes[0] == 255u && cap.bytes[2] == 68u, "EXT DSA packed values");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, 1, 1, 0, GL_BGRA, GL_UNSIGNED_SHORT, bgra16);
    ok &= check(glGetError() == GL_NO_ERROR && cap.format == GL_RGBA, "BGRA unsigned short upload");
    {
        struct mgl_texture *obj = mgl_texture_find(&ctx, tex);
        uint16_t value = 0u;
        if (obj != NULL && obj->levels[0].data != NULL)
            memcpy(&value, obj->levels[0].data, 2u);
        ok &= check(value == 32768u && cap.bytes[0] == 128u, "BGRA 16-bit red component");
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_BGRA, GL_FLOAT, bgra_float);
    ok &= check(glGetError() == GL_NO_ERROR && cap.format == GL_RGBA && cap.type == GL_FLOAT,
                "BGRA float component order");
    {
        float values[4] = {0};
        memcpy(values, cap.bytes, sizeof(values));
        ok &=
            check(values[0] == 2.0f && values[1] == 1.5f && values[2] == 0.5f && values[3] == 1.0f,
                  "BGRA float values");
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 1, 1, 0, GL_BGR, GL_FLOAT, bgr_float);
    ok &= check(glGetError() == GL_NO_ERROR && cap.format == GL_RGBA && cap.type == GL_FLOAT,
                "BGR float component order");
    {
        float values[4] = {0};
        memcpy(values, cap.bytes, sizeof(values));
        ok &=
            check(values[0] == 2.0f && values[1] == 1.5f && values[2] == 0.5f, "BGR float values");
    }
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 3);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE_3_3_2, row_packed);
    ok &= check(glGetError() == GL_NO_ERROR, "packed row layout");
    ok &= check(memcmp(cap.bytes, (unsigned char[]){255u, 0u, 255u, 0u, 0u, 0u}, 6u) == 0,
                "packed row layout values");
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 1, 1, 0, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV,
                 &hdrfloat);
    ok &= check(glGetError() == GL_NO_ERROR && cap.type == GL_FLOAT && cap.format == GL_RGBA,
                "HDR packed upload retains float values");
    {
        float values[4] = {0};
        memcpy(values, cap.bytes, sizeof(values));
        ok &= check(fabsf(values[0] - 2.0f) < 0.00001f && fabsf(values[1] - 1.0f) < 0.00001f &&
                        fabsf(values[2] - 0.5f) < 0.00001f,
                    "HDR values above one");
    }
    glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, &swap565);
    ok &= check(glGetError() == GL_NO_ERROR, "packed byte swap");
    ok &= check_bytes(rgb_expected, 3u, "packed byte swap values");
    glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_1D, tex);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB8, 1, 0, GL_RGB, GL_UNSIGNED_BYTE_3_3_2, &rgb332);
    ok &= check(glGetError() == GL_NO_ERROR && cap.target == GL_TEXTURE_2D, "1D packed image");
    ok &= check_bytes(rgb_expected, 3u, "1D packed image values");
    glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 1, GL_RGB, GL_UNSIGNED_BYTE_2_3_3_REV, &rgb332_rev);
    ok &= check(glGetError() == GL_NO_ERROR, "1D packed subimage");
    ok &= check_bytes(rgb_expected, 3u, "1D packed subimage values");
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB8, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE_3_3_2, &rgb332);
    ok &= check(glGetError() == GL_NO_ERROR && cap.target == GL_TEXTURE_3D && cap.format == GL_RGB,
                "3D packed image");
    ok &= check_bytes(rgb_expected, 3u, "3D packed image data");
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV,
                    &rgb565_rev);
    ok &= check(glGetError() == GL_NO_ERROR, "3D packed subimage");
    ok &= check_bytes(rgb_expected, 3u, "3D packed subimage data");
    glTextureSubImage3D(tex, 0, 0, 0, 0, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE_2_3_3_REV, &rgb332_rev);
    ok &= check(glGetError() == GL_NO_ERROR, "3D DSA packed subimage");
    ok &= check_bytes(rgb_expected, 3u, "3D DSA packed subimage data");
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return ok ? 0 : 1;
}

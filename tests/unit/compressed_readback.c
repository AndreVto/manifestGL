#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static GLuint next_texture = 1000u;

static const GLubyte *get_string(GLenum name) {
    (void)name;
    return (const GLubyte *)"";
}

static GLenum get_error(void) {
    return GL_NO_ERROR;
}

static void gen_textures(GLsizei n, GLuint *textures) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        textures[i] = next_texture++;
}

static void delete_textures(GLsizei n, const GLuint *textures) {
    (void)n;
    (void)textures;
}

static void active_texture(GLenum texture) {
    (void)texture;
}

static void bind_texture(GLenum target, GLuint texture) {
    (void)target;
    (void)texture;
}

static void pixel_store(GLenum name, GLint param) {
    (void)name;
    (void)param;
}

static void tex_image_2d(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height,
                         GLint border, GLenum format, GLenum type, const void *data) {
    (void)target;
    (void)level;
    (void)internal;
    (void)width;
    (void)height;
    (void)border;
    (void)format;
    (void)type;
    (void)data;
}

static void tex_image_3d(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height,
                         GLsizei depth, GLint border, GLenum format, GLenum type,
                         const void *data) {
    (void)target;
    (void)level;
    (void)internal;
    (void)width;
    (void)height;
    (void)depth;
    (void)border;
    (void)format;
    (void)type;
    (void)data;
}

static int check(struct mgl_context *ctx, int ok, int line) {
    GLenum error = mgl_error_take(&ctx->errors);
    if (!ok || error != GL_NO_ERROR) {
        fprintf(stderr, "compressed readback failed at %d, error 0x%x\n", line, (unsigned)error);
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
    const unsigned char red[8] = {0x00u, 0xf8u};
    const unsigned char green[8] = {0xe0u, 0x07u};
    const unsigned char snorm[8] = {0x81u, 0x00u};
    const unsigned char bc6[16] = {0u};
    const unsigned char bc7[16] = {0u};
    const unsigned char rgtc2[16] = {200u, 100u, 0u, 0u, 0u, 0u, 0u, 0u,
                                     180u, 90u,  0u, 0u, 0u, 0u, 0u, 0u};
    unsigned char layers[16], pixels[6u * 4u * 4u * 4u], small[16], padded[64];
    float float_pixels[16u * 4u];
    GLuint tex, array, cube, rgtc, bptc, bc7_texture, rg_texture;
    unsigned i, face;

    static const struct {
        GLenum format, type;
        uint32_t expected;
        unsigned bytes;
    } packed_cases[] = {
        {GL_RGB, GL_UNSIGNED_BYTE_3_3_2, 0xe0u, 1u},
        {GL_RGB, GL_UNSIGNED_BYTE_2_3_3_REV, 0x07u, 1u},
        {GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0xf800u, 2u},
        {GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, 0x001fu, 2u},
        {GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 0xf00fu, 2u},
        {GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4_REV, 0xf00fu, 2u},
        {GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 0xf801u, 2u},
        {GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, 0x801fu, 2u},
        {GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, 0xff0000ffu, 4u},
        {GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, 0xff0000ffu, 4u},
        {GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, 0xffff0000u, 4u},
        {GL_RGBA, GL_UNSIGNED_INT_10_10_10_2, 0xffc00003u, 4u},
        {GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 0xc00003ffu, 4u},
        {GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, 0x000003c0u, 4u},
        {GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, 0x80000100u, 4u},
    };

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetString = get_string;
    native.glesGetError = get_error;
    native.glesActiveTexture = active_texture;
    native.glesGenTextures = gen_textures;
    native.glesDeleteTextures = delete_textures;
    native.glesBindTexture = bind_texture;
    native.glesPixelStorei = pixel_store;
    native.glesTexImage2D = tex_image_2d;
    native.glesTexImage3D = tex_image_3d;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0, 8, red);
    CHECK(1);
    memset(pixels, 0, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    for (i = 0u; i < 16u; ++i)
        if (pixels[i * 4u] != 255u || pixels[i * 4u + 1u] != 0u || pixels[i * 4u + 2u] != 0u ||
            pixels[i * 4u + 3u] != 255u)
            break;
    CHECK(i == 16u);
    memset(pixels, 0xa5, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_GREEN, GL_UNSIGNED_BYTE, pixels);
    CHECK(pixels[0] == 0u && pixels[1] == 0u && pixels[15] == 0u && pixels[16] == 0xa5u);
    glGetTextureSubImage(tex, 0, 0, 0, 0, 1, 1, 1, GL_BLUE, GL_UNSIGNED_BYTE,
                         (GLsizei)sizeof(pixels), pixels);
    CHECK(pixels[0] == 0u && pixels[1] == 0u);
    for (i = 0u; i < sizeof(packed_cases) / sizeof(packed_cases[0]); ++i) {
        uint32_t packed = 0u;
        memset(pixels, 0xa5, sizeof(pixels));
        glGetTextureImage(tex, 0, packed_cases[i].format, packed_cases[i].type,
                          (GLsizei)sizeof(pixels), pixels);
        memcpy(&packed, pixels, packed_cases[i].bytes);
        if (packed != packed_cases[i].expected || pixels[packed_cases[i].bytes] == 0xa5u) {
            fprintf(stderr, "packed case %u: got 0x%x, expected 0x%x\n", i, (unsigned)packed,
                    (unsigned)packed_cases[i].expected);
            return 4;
        }
        CHECK(1);
    }
    memset(pixels, 0xa5, sizeof(pixels));
    glGetnTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 31, pixels);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || pixels[0] != 0xa5u)
        return 5;
    glGetnTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 32, pixels);
    CHECK(pixels[0] == 0x01u && pixels[1] == 0xf8u);
    glPixelStorei(GL_PACK_SWAP_BYTES, 1);
    memset(pixels, 0xa5, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, pixels);
    CHECK(pixels[0] == 0x00u && pixels[1] == 0x1fu && pixels[2] == 0x00u && pixels[3] == 0x1fu);
    memset(pixels, 0xa5, sizeof(pixels));
    glGetTextureSubImage(tex, 0, 1, 1, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV,
                         (GLsizei)sizeof(pixels), pixels);
    CHECK(pixels[0] == 0xc0u && pixels[1] == 0x00u && pixels[2] == 0x03u && pixels[3] == 0xffu &&
          pixels[4] == 0xa5u);
    glPixelStorei(GL_PACK_SWAP_BYTES, 0);
    memset(pixels, 0xa5, sizeof(pixels));
    glGetTextureSubImage(tex, 0, 0, 0, 0, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE_2_3_3_REV,
                         (GLsizei)sizeof(pixels), pixels);
    CHECK(pixels[0] == 0x07u && pixels[1] == 0xa5u);
    memset(pixels, 0xa5, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE_3_3_2, pixels);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || pixels[0] != 0xa5u)
        return 6;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGR, GL_UNSIGNED_SHORT_5_6_5, pixels);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || pixels[0] != 0xa5u)
        return 7;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGR, GL_UNSIGNED_BYTE, pixels);
    CHECK(pixels[0] == 0u && pixels[1] == 0u && pixels[2] == 255u);
    glGetTexImage(GL_TEXTURE_BUFFER, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixels);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return 8;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, float_pixels);
    CHECK(float_pixels[0] == 1.0f && float_pixels[1] == 0.0f && float_pixels[3] == 1.0f);
    memset(pixels, 0xabu, sizeof(pixels));
    glGetnTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, 63, pixels);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || pixels[0] != 0xabu)
        return 2;
    glGetnTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, 64, pixels);
    CHECK(pixels[0] == 255u && pixels[3] == 255u);
    glGetTextureImage(tex, 0, GL_RGBA, GL_UNSIGNED_BYTE, 64, pixels);
    CHECK(pixels[0] == 255u && pixels[3] == 255u);
    glGetTextureImage(tex, 0, GL_RGBA, GL_UNSIGNED_BYTE, 63, pixels);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return 3;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_HALF_FLOAT, float_pixels);
    CHECK(((const unsigned short *)float_pixels)[0] == 0x3c00u &&
          ((const unsigned short *)float_pixels)[1] == 0u &&
          ((const unsigned short *)float_pixels)[3] == 0x3c00u);
    memset(padded, 0xabu, sizeof(padded));
    glPixelStorei(GL_PACK_ROW_LENGTH, 4);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 1);
    glPixelStorei(GL_PACK_SKIP_ROWS, 1);
    glGetTextureSubImage(tex, 0, 1, 1, 0, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, 64, padded);
    CHECK(padded[20] == 255u && padded[21] == 0u && padded[23] == 255u && padded[0] == 0xabu &&
          padded[16] == 0xabu && padded[32] == 0xabu && padded[36] == 255u);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);

    glGenTextures(1, &array);
    glBindTexture(GL_TEXTURE_2D_ARRAY, array);
    memcpy(layers, red, 8u);
    memcpy(layers + 8u, green, 8u);
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 2, 0, 16,
                           layers);
    CHECK(1);
    memset(pixels, 0, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    CHECK(pixels[0] == 255u && pixels[1] == 0u && pixels[64] == 0u && pixels[65] == 255u);
    glGetTextureSubImage(array, 0, 0, 0, 1, 4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, 64, pixels);
    CHECK(pixels[0] == 0u && pixels[1] == 255u);
    memset(pixels, 0xa5, sizeof(pixels));
    glGetTextureSubImage(array, 0, 0, 0, 1, 1, 1, 1, GL_GREEN, GL_UNSIGNED_BYTE,
                         (GLsizei)sizeof(pixels), pixels);
    CHECK(pixels[0] == 255u && pixels[1] == 0xa5u);
    glGetTextureSubImage(array, 0, 0, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4,
                         (GLsizei)sizeof(pixels), pixels);
    {
        uint16_t result;
        memcpy(&result, pixels, sizeof(result));
        CHECK(result == 0x0f0fu);
    }
    glGetTextureSubImage(array, 0, 0, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4_REV,
                         (GLsizei)sizeof(pixels), pixels);
    {
        uint16_t result;
        memcpy(&result, pixels, sizeof(result));
        CHECK(result == 0xf0f0u);
    }
    glGetTextureSubImage(array, 0, 0, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
                         (GLsizei)sizeof(pixels), pixels);
    {
        uint32_t result;
        memcpy(&result, pixels, sizeof(result));
        CHECK(result == 0x00ff00ffu);
    }
    glGetTextureSubImage(array, 0, 0, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV,
                         (GLsizei)sizeof(pixels), pixels);
    {
        uint32_t result;
        memcpy(&result, pixels, sizeof(result));
        CHECK(result == 0xff00ff00u);
    }

    glGenTextures(1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    for (face = 0u; face < 6u; ++face)
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0,
                               GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0, 8,
                               (face & 1u) != 0u ? green : red);
    CHECK(1);
    memset(pixels, 0, sizeof(pixels));
    glGetTextureImage(cube, 0, GL_RGBA, GL_UNSIGNED_BYTE, sizeof(pixels), pixels);
    for (face = 0u; face < 6u; ++face)
        if (pixels[face * 64u] != ((face & 1u) != 0u ? 0u : 255u) ||
            pixels[face * 64u + 1u] != ((face & 1u) != 0u ? 255u : 0u))
            break;
    CHECK(face == 6u);
    glGetTextureSubImage(cube, 0, 0, 0, 1, 4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, 64, pixels);
    CHECK(pixels[0] == 0u && pixels[1] == 255u);

    glGenTextures(1, &rgtc);
    glBindTexture(GL_TEXTURE_2D, rgtc);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_SIGNED_RED_RGTC1, 4, 4, 0, 8, snorm);
    CHECK(1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, float_pixels);
    CHECK(float_pixels[0] == -1.0f && float_pixels[15] == -1.0f);
    glGetTextureImage(rgtc, 0, GL_RED, GL_UNSIGNED_BYTE, 16, small);
    CHECK(small[0] == 0u && small[15] == 0u);

    glGenTextures(1, &rg_texture);
    glBindTexture(GL_TEXTURE_2D, rg_texture);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RG_RGTC2, 4, 4, 0, 16, rgtc2);
    CHECK(1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_UNSIGNED_BYTE, pixels);
    CHECK(pixels[0] == 200u && pixels[1] == 180u && pixels[2] == 200u);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, pixels);
    {
        uint32_t result;
        memcpy(&result, pixels, sizeof(result));
        CHECK((result & 0x7ffu) == 932u && ((result >> 11u) & 0x7ffu) == 922u &&
              ((result >> 22u) & 0x3ffu) == 0u);
    }
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, pixels);
    {
        uint32_t result;
        memcpy(&result, pixels, sizeof(result));
        CHECK((result & 0x1ffu) == 402u && ((result >> 9u) & 0x1ffu) == 361u &&
              ((result >> 18u) & 0x1ffu) == 0u && ((result >> 27u) & 31u) == 15u);
    }
    glGetTextureSubImage(rg_texture, 0, 2, 2, 0, 1, 1, 1, GL_RGBA, GL_FLOAT, 16, float_pixels);
    CHECK(fabsf(float_pixels[0] - 200.0f / 255.0f) < 0.0001f &&
          fabsf(float_pixels[1] - 180.0f / 255.0f) < 0.0001f && float_pixels[2] == 0.0f &&
          float_pixels[3] == 1.0f);

    glGenTextures(1, &bc7_texture);
    glBindTexture(GL_TEXTURE_2D, bc7_texture);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM, 4, 4, 0, 16, bc7);
    CHECK(1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    CHECK(1);

    glGenTextures(1, &bptc);
    glBindTexture(GL_TEXTURE_2D, bptc);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, 4, 4, 0, 16,
                           bc6);
    CHECK(1);
    glGetTextureImage(bptc, 0, GL_RGB, GL_FLOAT, 16u * 3u * sizeof(float), float_pixels);
    CHECK(isfinite(float_pixels[0]) && isfinite(float_pixels[1]) && isfinite(float_pixels[2]));

    glBindTexture(GL_TEXTURE_2D, 0);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

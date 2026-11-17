#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static GLuint next_native_texture = 100u;

static void fake_gen_textures(GLsizei n, GLuint *out) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        out[i] = ++next_native_texture;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    (void)target;
    (void)texture;
}

static void fake_delete_textures(GLsizei n, const GLuint *textures) {
    (void)n;
    (void)textures;
}

static void fake_active_texture(GLenum texture) {
    (void)texture;
}

static void fake_pixel_store(GLenum pname, GLint value) {
    (void)pname;
    (void)value;
}

static void fake_tex_parameteri(GLenum target, GLenum pname, GLint value) {
    (void)target;
    (void)pname;
    (void)value;
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

static int error_is(struct mgl_context *ctx, GLenum expected, const char *label) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr, "%s: error 0x%x, expected 0x%x\n", label, actual, expected);
        return 0;
    }
    return 1;
}

static int near(float value, double expected, double tolerance, const char *label) {
    if (!isfinite(value) || fabs((double)value - expected) > tolerance) {
        fprintf(stderr, "%s: got %.9g, expected %.9g\n", label, value, expected);
        return 0;
    }
    return 1;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    const unsigned char rgba8[] = {17u, 86u, 204u, 250u};
    const unsigned char integer[] = {250u, 40u, 18u, 7u};
    float floats[12];
    unsigned char bytes[64];
    unsigned short shorts[8];
    GLuint tex, cube, depth_tex;
    unsigned face;
    int ok = 1;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesActiveTexture = fake_active_texture;
    native.glesPixelStorei = fake_pixel_store;
    native.glesTexParameteri = fake_tex_parameteri;
    native.glesTexImage2D = fake_tex_image_2d;
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
    ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba8);
    ok &= error_is(&ctx, GL_NO_ERROR, "RGBA8 setup");
    memset(floats, 0, sizeof(floats));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGR, GL_FLOAT, floats);
    ok &= error_is(&ctx, GL_NO_ERROR, "BGR float");
    ok &= near(floats[0], 204.0 / 255.0, 1e-6, "BGR blue");
    ok &= near(floats[1], 86.0 / 255.0, 1e-6, "BGR green");
    ok &= near(floats[2], 17.0 / 255.0, 1e-6, "BGR red");
    memset(floats, 0, sizeof(floats));
    glGetnTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_FLOAT, 16, floats);
    ok &= error_is(&ctx, GL_NO_ERROR, "BGRA bounded float");
    ok &= near(floats[0], 204.0 / 255.0, 1e-6, "BGRA blue");
    ok &= near(floats[3], 250.0 / 255.0, 1e-6, "BGRA alpha");
    memset(bytes, 0xa5, sizeof(bytes));
    glGetTextureImage(tex, 0, GL_GREEN, GL_UNSIGNED_BYTE, 1, bytes);
    ok &= error_is(&ctx, GL_NO_ERROR, "green DSA");
    ok &= bytes[0] == 86u && bytes[1] == 0xa5u;
    glGetTextureSubImage(tex, 0, 0, 0, 0, 1, 1, 1, GL_BLUE, GL_UNSIGNED_BYTE, 1, bytes);
    ok &= error_is(&ctx, GL_NO_ERROR, "blue subimage");
    ok &= bytes[0] == 204u;
    memset(shorts, 0, sizeof(shorts));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGR, GL_UNSIGNED_SHORT, shorts);
    ok &= error_is(&ctx, GL_NO_ERROR, "BGR ushort");
    ok &= shorts[0] == 204u * 257u && shorts[1] == 86u * 257u;
    glPixelStorei(GL_PACK_SWAP_BYTES, 1);
    memset(bytes, 0xa5, sizeof(bytes));
    glGetTextureImage(tex, 0, GL_BGR, GL_UNSIGNED_SHORT, 6, bytes);
    ok &= error_is(&ctx, GL_NO_ERROR, "BGR swapped ushort");
    ok &= bytes[0] == 204u && bytes[1] == 204u;
    ok &= bytes[6] == 0xa5u;
    glPixelStorei(GL_PACK_SWAP_BYTES, 0);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 1, 1, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, integer);
    ok &= error_is(&ctx, GL_NO_ERROR, "integer setup");
    memset(shorts, 0, sizeof(shorts));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGR_INTEGER, GL_UNSIGNED_SHORT, shorts);
    ok &= error_is(&ctx, GL_NO_ERROR, "BGR integer");
    ok &= shorts[0] == 18u && shorts[1] == 40u && shorts[2] == 250u;
    memset(bytes, 0, sizeof(bytes));
    glGetTextureImage(tex, 0, GL_BGRA_INTEGER, GL_UNSIGNED_BYTE, 4, bytes);
    ok &= error_is(&ctx, GL_NO_ERROR, "BGRA integer DSA");
    ok &= bytes[0] == 18u && bytes[1] == 40u && bytes[2] == 250u && bytes[3] == 7u;
    glGetTextureSubImage(tex, 0, 0, 0, 0, 1, 1, 1, GL_GREEN_INTEGER, GL_UNSIGNED_BYTE, 1, bytes);
    ok &= error_is(&ctx, GL_NO_ERROR, "green integer subimage");
    ok &= bytes[0] == 40u;
    glGetTextureSubImage(tex, 0, 0, 0, 0, 1, 1, 1, GL_BLUE_INTEGER, GL_UNSIGNED_BYTE, 1, bytes);
    ok &= error_is(&ctx, GL_NO_ERROR, "blue integer subimage");
    ok &= bytes[0] == 18u;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA_INTEGER, GL_FLOAT, floats);
    ok &= error_is(&ctx, GL_INVALID_ENUM, "integer float rejected");

    glGenTextures(1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    for (face = 0u; face < 6u; ++face) {
        unsigned char pixel[4] = {17u, (unsigned char)(face + 20u), 204u, 250u};
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA8, 1, 1, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, pixel);
    }
    ok &= error_is(&ctx, GL_NO_ERROR, "cube setup");
    memset(bytes, 0xa5, sizeof(bytes));
    glGetTextureImage(cube, 0, GL_GREEN, GL_UNSIGNED_BYTE, sizeof(bytes), bytes);
    ok &= error_is(&ctx, GL_NO_ERROR, "cube green DSA");
    for (face = 0u; face < 6u; ++face)
        ok &= bytes[face * 4u] == face + 20u;
    ok &= bytes[24] == 0xa5u;

    glGenTextures(1, &depth_tex);
    glBindTexture(GL_TEXTURE_2D, depth_tex);
    {
        const uint32_t depth_stencil = 0x800000a5u;
        uint32_t word = 0u;
        float depth_value = 0.0f;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, 1, 1, 0, GL_DEPTH_STENCIL,
                     GL_UNSIGNED_INT_24_8, &depth_stencil);
        ok &= error_is(&ctx, GL_NO_ERROR, "depth24 stencil8 setup");
        glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, &word);
        ok &= error_is(&ctx, GL_NO_ERROR, "depth-stencil packed readback");
        ok &= word == depth_stencil;
        glGetTextureImage(depth_tex, 0, GL_DEPTH_COMPONENT, GL_FLOAT, sizeof(depth_value),
                          &depth_value);
        ok &= error_is(&ctx, GL_NO_ERROR, "depth from packed texture");
        ok &= near(depth_value, 8388608.0 / 16777215.0, 1e-6, "packed depth");
        memset(bytes, 0xa5, sizeof(bytes));
        glGetTextureSubImage(depth_tex, 0, 0, 0, 0, 1, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, 1,
                             bytes);
        ok &= error_is(&ctx, GL_NO_ERROR, "stencil from packed texture");
        ok &= bytes[0] == 0xa5u && bytes[1] == 0xa5u;
        glPixelStorei(GL_PACK_SWAP_BYTES, 1);
        memset(bytes, 0xa5, sizeof(bytes));
        glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, bytes);
        ok &= error_is(&ctx, GL_NO_ERROR, "packed depth-stencil byte swap");
        ok &= bytes[0] == 0x80u && bytes[1] == 0u && bytes[2] == 0u && bytes[3] == 0xa5u;
        glPixelStorei(GL_PACK_SWAP_BYTES, 0);
    }
    {
        const struct {
            float depth;
            uint32_t stencil;
        } input = {0.25f, 0xc3u};

        unsigned char result[8] = {0u};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, 1, 1, 0, GL_DEPTH_STENCIL,
                     GL_FLOAT_32_UNSIGNED_INT_24_8_REV, &input);
        ok &= error_is(&ctx, GL_NO_ERROR, "depth32f stencil8 setup");
        glGetnTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 8,
                       result);
        ok &= error_is(&ctx, GL_NO_ERROR, "depth32f stencil8 bounded readback");
        ok &= memcmp(result, &input, 8) == 0;
        memset(bytes, 0xa5, sizeof(bytes));
        glPixelStorei(GL_PACK_SWAP_BYTES, 1);
        glGetTextureImage(depth_tex, 0, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 8,
                          bytes);
        ok &= error_is(&ctx, GL_NO_ERROR, "depth32f stencil8 swapped DSA readback");
        ok &= bytes[0] == 0x3eu && bytes[1] == 0x80u && bytes[2] == 0u && bytes[3] == 0u;
        ok &= bytes[4] == 0u && bytes[5] == 0u && bytes[6] == 0u && bytes[7] == 0xc3u;
        glPixelStorei(GL_PACK_SWAP_BYTES, 0);
    }
    {
        const unsigned char stencil = 0x7du;
        memset(bytes, 0xa5, sizeof(bytes));
        glTexImage2D(GL_TEXTURE_2D, 0, GL_STENCIL_INDEX8, 1, 1, 0, GL_STENCIL_INDEX,
                     GL_UNSIGNED_BYTE, &stencil);
        ok &= error_is(&ctx, GL_NO_ERROR, "stencil8 setup");
        glGetTexImage(GL_TEXTURE_2D, 0, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, bytes);
        ok &= error_is(&ctx, GL_NO_ERROR, "stencil8 readback");
        ok &= bytes[0] == stencil && bytes[1] == 0xa5u;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    for (face = 0u; face < 6u; ++face) {
        const uint32_t packed = 0x80000000u | (face + 1u);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH24_STENCIL8, 1, 1, 0,
                     GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, &packed);
    }
    ok &= error_is(&ctx, GL_NO_ERROR, "packed cube setup");
    memset(bytes, 0xa5, sizeof(bytes));
    glGetTextureImage(cube, 0, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, sizeof(bytes), bytes);
    ok &= error_is(&ctx, GL_NO_ERROR, "packed cube stencil readback");
    for (face = 0u; face < 6u; ++face)
        ok &= bytes[face * 4u] == face + 1u;
    ok &= bytes[24] == 0xa5u;

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    if (!ok)
        return 1;
    puts("mGL texture readback formats: PASS");
    return 0;
}

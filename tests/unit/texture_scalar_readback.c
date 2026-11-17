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

static uint16_t half_word(const unsigned char *bytes) {
    uint16_t word;
    memcpy(&word, bytes, sizeof(word));
    return word;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    const unsigned char rgba8[4] = {114u, 32u, 200u, 255u};
    const uint16_t half_input = 0x3a00u;
    const float float_input[4] = {0.25f, -2.0f, 1.5f, 10.0f};
    const unsigned char integer_input[4] = {250u, 1u, 5u, 255u};
    float values[24];
    uint16_t half_values[4], short_values[4];
    uint32_t uint_values[4];
    unsigned char byte_values[24], guards[32];
    GLuint tex, cube;
    unsigned face;
    int success = 1;

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
    success &= error_is(&ctx, GL_NO_ERROR, "RGBA8 upload");
    memset(values, 0, sizeof(values));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, values);
    success &= error_is(&ctx, GL_NO_ERROR, "RGBA8 GL_FLOAT readback");
    success &= near(values[0], 114.0 / 255.0, 1e-6, "float red");
    success &= near(values[1], 32.0 / 255.0, 1e-6, "float green");
    success &= near(values[2], 200.0 / 255.0, 1e-6, "float blue");
    success &= near(values[3], 1.0, 1e-6, "float alpha");
    glGetnTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_HALF_FLOAT, 8, half_values);
    success &= error_is(&ctx, GL_NO_ERROR, "half float bounded readback");
    success &= half_values[3] == 0x3c00u;
    memset(values, 0, sizeof(values));
    glGetTextureImage(tex, 0, GL_RGBA, GL_FLOAT, 16, values);
    success &= error_is(&ctx, GL_NO_ERROR, "DSA GL_FLOAT readback");
    success &= near(values[0], 114.0 / 255.0, 1e-6, "DSA red");
    glGetTextureSubImage(tex, 0, 0, 0, 0, 1, 1, 1, GL_RGBA, GL_FLOAT, 16, values);
    success &= error_is(&ctx, GL_NO_ERROR, "subimage GL_FLOAT readback");
    success &= near(values[2], 200.0 / 255.0, 1e-6, "subimage blue");

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_SHORT, short_values);
    success &= error_is(&ctx, GL_NO_ERROR, "GL_UNSIGNED_SHORT readback");
    success &= short_values[0] == 114u * 257u && short_values[2] == 200u * 257u;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_INT, uint_values);
    success &= error_is(&ctx, GL_NO_ERROR, "GL_UNSIGNED_INT readback");
    success &= uint_values[0] == 114u * 16843009u;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_BYTE, byte_values);
    success &= error_is(&ctx, GL_NO_ERROR, "GL_BYTE readback");
    success &= (int8_t)byte_values[0] == (int8_t)57 && (int8_t)byte_values[3] == 127;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_SHORT, short_values);
    success &= error_is(&ctx, GL_NO_ERROR, "GL_SHORT readback");
    success &= (int16_t)short_values[3] == 32767;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, byte_values);
    success &= error_is(&ctx, GL_NO_ERROR, "BGRA readback");
    success &= byte_values[0] == 200u && byte_values[1] == 32u && byte_values[2] == 114u;

    glPixelStorei(GL_PACK_SWAP_BYTES, 1);
    memset(guards, 0x77, sizeof(guards));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, guards + 4);
    success &= error_is(&ctx, GL_NO_ERROR, "float byte swap");
    {
        float first = 114.0f / 255.0f;
        unsigned char raw[4];
        memcpy(raw, &first, 4);
        success &= guards[4] == raw[3] && guards[5] == raw[2] && guards[6] == raw[1] &&
                   guards[7] == raw[0];
    }
    success &= guards[3] == 0x77u && guards[20] == 0x77u;
    glPixelStorei(GL_PACK_SWAP_BYTES, 0);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, 1, 1, 0, GL_RED, GL_HALF_FLOAT, &half_input);
    success &= error_is(&ctx, GL_NO_ERROR, "R16F upload");
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, values);
    success &= error_is(&ctx, GL_NO_ERROR, "R16F to float");
    success &= near(values[0], 0.75, 1e-6, "R16F value");
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, byte_values);
    success &= error_is(&ctx, GL_NO_ERROR, "R16F to byte");
    success &= byte_values[0] == 191u;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_HALF_FLOAT, half_values);
    success &= error_is(&ctx, GL_NO_ERROR, "R16F to half");
    success &= half_values[0] == half_input;
    glPixelStorei(GL_PACK_SWAP_BYTES, 1);
    memset(guards, 0x77, sizeof(guards));
    glGetTextureSubImage(tex, 0, 0, 0, 0, 1, 1, 1, GL_RED, GL_HALF_FLOAT, (GLsizei)sizeof(guards),
                         guards);
    success &= error_is(&ctx, GL_NO_ERROR, "half float DSA byte swap");
    success &= guards[0] == 0x3au && guards[1] == 0x00u && guards[2] == 0x77u;
    glPixelStorei(GL_PACK_SWAP_BYTES, 0);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, float_input);
    success &= error_is(&ctx, GL_NO_ERROR, "RGBA32F upload");
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, values);
    success &= error_is(&ctx, GL_NO_ERROR, "RGBA32F float");
    success &= near(values[0], 0.25, 1e-6, "float 0.25");
    success &= near(values[1], -2.0, 1e-6, "float negative");
    success &= near(values[2], 1.5, 1e-6, "float greater than one");
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_BYTE, byte_values);
    success &= error_is(&ctx, GL_NO_ERROR, "RGBA32F to byte");
    success &= (int8_t)byte_values[0] == 32 && (int8_t)byte_values[1] == -127 &&
               (int8_t)byte_values[2] == 127;

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 1, 1, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,
                 integer_input);
    success &= error_is(&ctx, GL_NO_ERROR, "RGBA8UI upload");
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, short_values);
    success &= error_is(&ctx, GL_NO_ERROR, "integer readback");
    success &= short_values[0] == 250u && short_values[1] == 1u && short_values[3] == 255u;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, byte_values);
    success &= error_is(&ctx, GL_INVALID_OPERATION, "integer format mismatch");

    glGenTextures(1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    for (face = 0u; face < 6u; ++face) {
        unsigned char channel = (unsigned char)((face + 1u) * 32u);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_R8, 1, 1, 0, GL_RED,
                     GL_UNSIGNED_BYTE, &channel);
    }
    success &= error_is(&ctx, GL_NO_ERROR, "cube setup");
    memset(guards, 0x77, sizeof(guards));
    glGetTextureImage(cube, 0, GL_RED, GL_FLOAT, (GLsizei)sizeof(guards), guards);
    success &= error_is(&ctx, GL_NO_ERROR, "cube float DSA");
    for (face = 0u; face < 6u; ++face) {
        float v;
        memcpy(&v, guards + face * sizeof(float), sizeof(float));
        success &= near(v, ((face + 1u) * 32u) / 255.0, 1e-6, "cube face");
    }
    success &= guards[24] == 0x77u;

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    if (!success)
        return 1;
    puts("mGL scalar texture readback unit test: PASS");
    return 0;
}

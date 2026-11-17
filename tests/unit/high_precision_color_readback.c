#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static GLenum last_type;
static unsigned read_calls;
static GLenum forced_error;
static int norm16_enabled = 1;
static GLenum rejected_type;
static GLint pack_row_length, pack_skip_pixels, pack_skip_rows, pack_alignment;

static const GLubyte *native_extensions(GLenum name) {
    return name == GL_EXTENSIONS && norm16_enabled ? (const GLubyte *)"GL_EXT_texture_norm16"
                                                   : (const GLubyte *)"";
}

static void bind_framebuffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void pixel_store(GLenum pname, GLint value) {
    if (pname == GL_PACK_ROW_LENGTH)
        pack_row_length = value;
    if (pname == GL_PACK_SKIP_PIXELS)
        pack_skip_pixels = value;
    if (pname == GL_PACK_SKIP_ROWS)
        pack_skip_rows = value;
    if (pname == GL_PACK_ALIGNMENT)
        pack_alignment = value;
}

static GLenum get_error(void) {
    GLenum error = forced_error;
    forced_error = GL_NO_ERROR;
    return error;
}

static void native_read_pixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                               GLenum type, void *pixels) {
    (void)x;
    (void)y;
    ++read_calls;
    last_type = type;
    if (type == rejected_type) {
        forced_error = GL_INVALID_OPERATION;
        return;
    }
    if (format != GL_RGBA || width != 1 || height != 1 || pack_alignment != 1 ||
        pack_row_length != 0 || pack_skip_pixels != 0 || pack_skip_rows != 0) {
        forced_error = GL_INVALID_OPERATION;
        return;
    }
    if (type == GL_UNSIGNED_SHORT) {
        const uint16_t data[4] = {1u, 257u, 32768u, 65535u};
        memcpy(pixels, data, sizeof(data));
    } else if (type == GL_UNSIGNED_INT_2_10_10_10_REV) {
        const uint32_t data = 1u | (257u << 10u) | (777u << 20u) | (2u << 30u);
        memcpy(pixels, &data, sizeof(data));
    } else if (type == GL_UNSIGNED_BYTE) {
        const unsigned char data[4] = {0u, 1u, 128u, 255u};
        memcpy(pixels, data, sizeof(data));
    } else if (type == GL_FLOAT) {
        const float data[4] = {0.25f, 0.5f, 0.75f, 1.0f};
        memcpy(pixels, data, sizeof(data));
    } else {
        forced_error = GL_INVALID_ENUM;
    }
}

static int check(int condition, int code, const char *description) {
    if (condition)
        return 0;
    fprintf(stderr, "high precision color readback %d: %s\n", code, description);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_framebuffer *fb;
    struct mgl_texture *texture;
    uint16_t u16[4] = {0};
    float fp[4] = {0};
    unsigned char guarded[32];
    uint32_t packed;
    int result;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetString = native_extensions;
    native.glesBindFramebuffer = bind_framebuffer;
    native.glesBindBuffer = bind_buffer;
    native.glesPixelStorei = pixel_store;
    native.glesGetError = get_error;
    native.glesReadPixels = native_read_pixels;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 64, 64);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    pack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    texture = mgl_texture_ensure(&ctx, 41u, GL_TEXTURE_2D, 0);
    fb = mgl_framebuffer_ensure(&ctx, 31u, 0);
    if (texture == NULL || fb == NULL)
        return check(0, 2, "attachment creation");
    texture->levels[0].internal_format = GL_RGBA16;
    texture->levels[0].defined = GL_TRUE;
    fb->color_texture = texture->name;
    fb->color_texture_target = GL_TEXTURE_2D;
    fb->color_texture_level = 0;
    fb->read_buffer = GL_COLOR_ATTACHMENT0;
    ctx.read_framebuffer = fb->name;

    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT, u16);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && last_type == GL_UNSIGNED_SHORT &&
                       u16[0] == 1u && u16[1] == 257u && u16[2] == 32768u && u16[3] == 65535u,
                   3, "normalized 16-bit values retained");
    if (result)
        return result;

    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, fp);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR &&
                       fabsf(fp[0] - 1.0f / 65535.0f) < 0.0000001f &&
                       fabsf(fp[1] - 257.0f / 65535.0f) < 0.0000001f &&
                       fabsf(fp[2] - 32768.0f / 65535.0f) < 0.0000001f,
                   4, "normalized 16-bit float conversion");
    if (result)
        return result;

    memset(guarded, 0xa5, sizeof(guarded));
    glReadnPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT, 7, guarded + 2);
    result = check(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION && guarded[2] == 0xa5u, 5,
                   "bounded normalized 16-bit read");
    if (result)
        return result;
    glReadnPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT, 8, guarded + 2);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && guarded[1] == 0xa5u &&
                       guarded[2] == 1u && guarded[3] == 0u && guarded[10] == 0xa5u,
                   6, "bounded high-precision read and guards");
    if (result)
        return result;

    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glPixelStorei(GL_PACK_ROW_LENGTH, 3);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 1);
    glPixelStorei(GL_PACK_SKIP_ROWS, 1);
    glPixelStorei(GL_PACK_SWAP_BYTES, GL_TRUE);
    {
        unsigned char output[64];
        memset(output, 0xa5, sizeof(output));
        glReadnPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT, 39, output);
        result = check(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION && output[32] == 0xa5u,
                       12, "bounded high-precision pixel pack span");
        if (result)
            return result;
        glReadnPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT, 40, output);
        result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && output[31] == 0xa5u &&
                           output[32] == 0u && output[33] == 1u && output[34] == 1u &&
                           output[35] == 1u && output[39] == 255u && output[40] == 0xa5u &&
                           pack_row_length == 3 && pack_skip_pixels == 1 && pack_skip_rows == 1 &&
                           pack_alignment == 4,
                       13, "high-precision byte swap and pixel pack restoration");
        if (result)
            return result;
        rejected_type = GL_UNSIGNED_SHORT;
        memset(output, 0xa5, sizeof(output));
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT, output);
        result = check(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION && output[32] == 0xa5u,
                       14, "native wide read failure does not use RGBA8 fallback");
        if (result)
            return result;
        rejected_type = 0u;
    }
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);

    texture->levels[0].internal_format = GL_RG16;
    glReadPixels(0, 0, 1, 1, GL_RG, GL_UNSIGNED_SHORT, u16);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && last_type == GL_UNSIGNED_SHORT &&
                       u16[0] == 1u && u16[1] == 257u,
                   7, "RG16 precision");
    if (result)
        return result;

    norm16_enabled = 0;
    read_calls = 0u;
    glReadPixels(0, 0, 1, 1, GL_RG, GL_UNSIGNED_SHORT, u16);
    result = check(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION && read_calls == 0u, 8,
                   "16-bit source without native extension");
    if (result)
        return result;
    norm16_enabled = 1;

    texture->levels[0].internal_format = GL_RGB10_A2;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, &packed);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR &&
                       last_type == GL_UNSIGNED_INT_2_10_10_10_REV &&
                       packed == (1u | (257u << 10u) | (777u << 20u) | (2u << 30u)),
                   9, "packed 10:10:10:2 exact roundtrip");
    if (result)
        return result;

    glReadPixels(0, 0, 1, 1, GL_BGRA, GL_FLOAT, fp);
    result = check(
        mgl_error_take(&ctx.errors) == GL_NO_ERROR && fabsf(fp[0] - 777.0f / 1023.0f) < 0.000001f &&
            fabsf(fp[1] - 257.0f / 1023.0f) < 0.000001f &&
            fabsf(fp[2] - 1.0f / 1023.0f) < 0.000001f && fabsf(fp[3] - 2.0f / 3.0f) < 0.000001f,
        10, "10-bit source float channel order");
    if (result)
        return result;

    texture->levels[0].internal_format = GL_RGBA8;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, guarded);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && last_type == GL_UNSIGNED_BYTE &&
                       guarded[0] == 0u && guarded[1] == 1u,
                   11, "ordinary 8-bit read unchanged");
    if (result)
        return result;

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL high precision readback: PASS");
    return 0;
}

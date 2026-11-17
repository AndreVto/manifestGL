#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static GLint pack_row_length;
static GLint pack_skip_pixels;
static GLint pack_skip_rows;
static GLint pack_alignment;
static unsigned native_reads;
static int bad_native_layout;

static void native_bind_framebuffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void native_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static GLenum native_get_error(void) {
    return GL_NO_ERROR;
}

static void native_pixel_store(GLenum pname, GLint value) {
    if (pname == GL_PACK_ROW_LENGTH)
        pack_row_length = value;
    if (pname == GL_PACK_SKIP_PIXELS)
        pack_skip_pixels = value;
    if (pname == GL_PACK_SKIP_ROWS)
        pack_skip_rows = value;
    if (pname == GL_PACK_ALIGNMENT)
        pack_alignment = value;
}

static void native_read_pixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                               GLenum type, void *pixels) {
    unsigned char *dst = pixels;
    size_t index;
    (void)x;
    (void)y;
    ++native_reads;
    if (format != GL_RGBA || type != GL_UNSIGNED_BYTE || pack_alignment != 1 ||
        pack_row_length != 0 || pack_skip_pixels != 0 || pack_skip_rows != 0)
        bad_native_layout = 1;
    for (index = 0u; index < (size_t)width * (size_t)height; ++index) {
        const unsigned char colors[4] = {10u, 20u, 30u, 40u};
        memcpy(dst + index * 4u, colors, 4u);
    }
}

static int check(int condition, int code, const char *description) {
    if (condition)
        return 0;
    fprintf(stderr, "color readback %d: %s\n", code, description);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    unsigned char output[128];

    const struct {
        GLenum format;
        unsigned char values[4];
        unsigned length;
    } formats[] = {{GL_RED, {10u}, 1u},
                   {GL_GREEN, {20u}, 1u},
                   {GL_BLUE, {30u}, 1u},
                   {GL_RG, {10u, 20u}, 2u},
                   {GL_RGB, {10u, 20u, 30u}, 3u},
                   {GL_BGR, {30u, 20u, 10u}, 3u},
                   {GL_RGBA, {10u, 20u, 30u, 40u}, 4u},
                   {GL_BGRA, {30u, 20u, 10u, 40u}, 4u},
                   {GL_LUMINANCE_ALPHA, {10u, 40u}, 2u}};

    const struct {
        GLenum type;
        GLenum format;
        uint32_t expected;
        size_t size;
    } packed[] = {{GL_UNSIGNED_BYTE_3_3_2, GL_RGB, 0x04u, 1u},
                  {GL_UNSIGNED_BYTE_2_3_3_REV, GL_RGB, 0x08u, 1u},
                  {GL_UNSIGNED_SHORT_5_6_5, GL_RGB, 0x08a4u, 2u},
                  {GL_UNSIGNED_SHORT_5_6_5_REV, GL_RGB, 0x20a1u, 2u},
                  {GL_UNSIGNED_SHORT_4_4_4_4, GL_RGBA, 0x1122u, 2u},
                  {GL_UNSIGNED_SHORT_4_4_4_4_REV, GL_RGBA, 0x2211u, 2u},
                  {GL_UNSIGNED_SHORT_5_5_5_1, GL_RGBA, 0x0888u, 2u},
                  {GL_UNSIGNED_SHORT_1_5_5_5_REV, GL_RGBA, 0x1041u, 2u},
                  {GL_UNSIGNED_INT_8_8_8_8, GL_RGBA, 0x0a141e28u, 4u},
                  {GL_UNSIGNED_INT_8_8_8_8_REV, GL_RGBA, 0x281e140au, 4u}};

    size_t index;
    int result;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesBindFramebuffer = native_bind_framebuffer;
    native.glesBindBuffer = native_bind_buffer;
    native.glesPixelStorei = native_pixel_store;
    native.glesReadPixels = native_read_pixels;
    native.glesGetError = native_get_error;
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
    for (index = 0u; index < sizeof(formats) / sizeof(formats[0]); ++index) {
        memset(output, 0xee, sizeof(output));
        glReadPixels(0, 0, 1, 1, formats[index].format, GL_UNSIGNED_BYTE, output);
        result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR &&
                           memcmp(output, formats[index].values, formats[index].length) == 0 &&
                           output[formats[index].length] == 0xeeu,
                       10 + (int)index, "channel layout");
        if (result)
            return result;
    }
    {
        uint16_t wide[4] = {0};
        float floats[4] = {0};
        glReadPixels(0, 0, 1, 1, GL_BGRA, GL_UNSIGNED_SHORT, wide);
        result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && wide[0] == 7710u &&
                           wide[1] == 5140u && wide[2] == 2570u && wide[3] == 10280u,
                       20, "unsigned short channel transfer");
        if (result)
            return result;
        glReadPixels(0, 0, 1, 1, GL_RG, GL_FLOAT, floats);
        result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR &&
                           fabsf(floats[0] - 10.0f / 255.0f) < 0.00001f &&
                           fabsf(floats[1] - 20.0f / 255.0f) < 0.00001f,
                       21, "floating point channel transfer");
        if (result)
            return result;
    }
    {
        int8_t signed_byte = 0;
        int16_t signed_short = 0;
        uint32_t scalar = 0u;
        glReadPixels(0, 0, 1, 1, GL_RED, GL_BYTE, &signed_byte);
        result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && signed_byte == 4, 22,
                       "signed byte color conversion");
        if (result)
            return result;
        glReadPixels(0, 0, 1, 1, GL_RED, GL_SHORT, &signed_short);
        result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && signed_short == 1284, 23,
                       "signed short color conversion");
        if (result)
            return result;
        glReadPixels(0, 0, 1, 1, GL_GREEN, GL_UNSIGNED_INT, &scalar);
        result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && scalar == 336860180u, 24,
                       "unsigned 32bit color conversion");
        if (result)
            return result;
    }
    for (index = 0u; index < sizeof(packed) / sizeof(packed[0]); ++index) {
        uint32_t word = 0u;
        glReadPixels(0, 0, 1, 1, packed[index].format, packed[index].type, &word);
        result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR &&
                           (packed[index].size == 1u   ? word & 0xffu
                            : packed[index].size == 2u ? word & 0xffffu
                                                       : word) == packed[index].expected,
                       30 + (int)index, "packed transfer");
        if (result)
            return result;
    }
    {
        uint32_t floating_bits = 0u;
        uint16_t half[3] = {0};
        glReadPixels(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, &floating_bits);
        result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && floating_bits != 0u, 40,
                       "packed unsigned float 11-11-10 transfer");
        if (result)
            return result;
        glReadPixels(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, &floating_bits);
        result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && floating_bits != 0u, 41,
                       "packed shared exponent RGB9E5 transfer");
        if (result)
            return result;
        glReadPixels(0, 0, 1, 1, GL_RGB, GL_HALF_FLOAT, half);
        result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && half[0] != 0u &&
                           half[1] != 0u && half[2] != 0u,
                       42, "half float RGB transfer");
        if (result)
            return result;
        glPixelStorei(GL_PACK_SWAP_BYTES, GL_TRUE);
        glReadPixels(0, 0, 1, 1, GL_RGB, GL_HALF_FLOAT, output);
        result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR &&
                           output[0] == (unsigned char)(half[0] >> 8u) &&
                           output[1] == (unsigned char)half[0],
                       43, "half-float pack swap bytes");
        if (result)
            return result;
        glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
    }
    memset(output, 0xacu, sizeof(output));
    glReadnPixels(0, 0, 1, 1, GL_BGR, GL_UNSIGNED_SHORT, 5, output);
    result = check(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION && output[0] == 0xacu, 45,
                   "bounded read rejects undersized BGR short");
    if (result)
        return result;
    glPixelStorei(GL_PACK_ROW_LENGTH, 3);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 1);
    glPixelStorei(GL_PACK_SKIP_ROWS, 1);
    memset(output, 0xacu, sizeof(output));
    glReadnPixels(0, 0, 1, 1, GL_BGR, GL_UNSIGNED_BYTE, 12, output);
    result = check(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION && output[0] == 0xacu, 46,
                   "bounded read includes pixel-pack offset");
    if (result)
        return result;
    glReadnPixels(0, 0, 1, 1, GL_BGR, GL_UNSIGNED_BYTE, (GLsizei)sizeof(output), output);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && output[0] == 0xacu &&
                       output[15] == 30u && output[16] == 20u && output[17] == 10u &&
                       pack_row_length == 3 && pack_skip_pixels == 1 && pack_skip_rows == 1,
                   47, "pixel-pack layout and native restoration");
    if (result)
        return result;
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glReadPixels(0, 0, 1, 1, GL_RG, GL_UNSIGNED_INT_8_8_8_8, output);
    result =
        check(mgl_error_take(&ctx.errors) == GL_INVALID_ENUM, 48, "packed format compatibility");
    if (result)
        return result;
    if (bad_native_layout || native_reads == 0u)
        return check(0, 49, "native read uses tight pack state");
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL color readback formats: PASS");
    return 0;
}

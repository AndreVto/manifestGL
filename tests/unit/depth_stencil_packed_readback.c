#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int extension_enabled = 1, depth_enabled = 1, stencil_enabled = 1;
static unsigned read_calls;
static int reject_float_depth, reject_integer_depth;
static GLenum last_format, last_type, read_error;
static GLuint pack_binding = 99u;
static GLint pack_alignment = 4, pack_row_length, pack_skip_pixels, pack_skip_rows;
static GLenum last_framebuffer_target;

static const GLubyte *get_string(GLenum name) {
    static char extensions[128];
    if (name != GL_EXTENSIONS)
        return (const GLubyte *)"";
    extensions[0] = 0;
    if (extension_enabled)
        strcat(extensions, "GL_NV_read_depth_stencil ");
    if (depth_enabled)
        strcat(extensions, "GL_NV_read_depth ");
    if (stencil_enabled)
        strcat(extensions, "GL_NV_read_stencil ");
    return (const GLubyte *)extensions;
}

static void bind_framebuffer(GLenum target, GLuint name) {
    (void)name;
    last_framebuffer_target = target;
}

static void bind_buffer(GLenum target, GLuint name) {
    if (target == GL_PIXEL_PACK_BUFFER)
        pack_binding = name;
}

static void pixel_store(GLenum name, GLint value) {
    if (name == GL_PACK_ALIGNMENT)
        pack_alignment = value;
    else if (name == GL_PACK_ROW_LENGTH)
        pack_row_length = value;
    else if (name == GL_PACK_SKIP_PIXELS)
        pack_skip_pixels = value;
    else if (name == GL_PACK_SKIP_ROWS)
        pack_skip_rows = value;
}

static GLenum get_error(void) {
    GLenum error = read_error;
    read_error = GL_NO_ERROR;
    return error;
}

static void read_pixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
                        void *data) {
    size_t pixel, count = (size_t)width * (size_t)height;
    (void)x;
    (void)y;
    ++read_calls;
    last_format = format;
    last_type = type;
    if (pack_binding != 0u || pack_alignment != 1 || pack_row_length != 0 ||
        pack_skip_pixels != 0 || pack_skip_rows != 0) {
        read_error = GL_INVALID_OPERATION;
        return;
    }
    if (format == GL_DEPTH_COMPONENT &&
        ((type == GL_FLOAT && reject_float_depth) ||
         (type == GL_UNSIGNED_INT && reject_integer_depth))) {
        read_error = type == GL_FLOAT ? GL_INVALID_OPERATION : GL_INVALID_ENUM;
        return;
    }
    if (type == GL_UNSIGNED_INT_24_8) {
        uint32_t *output = data;
        for (pixel = 0u; pixel < count; ++pixel)
            output[pixel] = 0x80000003u + (uint32_t)pixel;
    } else if (type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV) {
        uint32_t *output = data;
        float depth = 0.5f;
        for (pixel = 0u; pixel < count; ++pixel) {
            memcpy(&output[2u * pixel], &depth, sizeof(depth));
            output[2u * pixel + 1u] = 7u;
        }
    } else if (format == GL_DEPTH_COMPONENT && type == GL_UNSIGNED_SHORT) {
        uint16_t *output = data;
        for (pixel = 0u; pixel < count; ++pixel)
            output[pixel] = (uint16_t)(0x3456u + pixel);
    } else if (format == GL_DEPTH_COMPONENT && type == GL_UNSIGNED_INT) {
        uint32_t *output = data;
        for (pixel = 0u; pixel < count; ++pixel)
            output[pixel] = 0x56789abcu + (uint32_t)pixel;
    } else if (format == GL_DEPTH_COMPONENT && type == GL_FLOAT) {
        float *output = data;
        for (pixel = 0u; pixel < count; ++pixel)
            output[pixel] = 0.75f;
    } else if (format == GL_STENCIL_INDEX && type == GL_UNSIGNED_BYTE) {
        uint8_t *output = data;
        for (pixel = 0u; pixel < count; ++pixel)
            output[pixel] = (uint8_t)(7u + pixel);
    } else {
        read_error = GL_INVALID_ENUM;
    }
}

static int fail(int code, const char *message) {
    fprintf(stderr, "packed depth/stencil readback %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    uint32_t packed = 0u, float_packed[2] = {0u, 0u};
    uint32_t guarded[3] = {0xa5a5a5a5u, 0xa5a5a5a5u, 0xa5a5a5a5u};
    unsigned char layout[64];
    unsigned char bytes[4], swapped[4], stencil = 0u;
    uint16_t depth16 = 0u;
    uint32_t depth32 = 0u;
    float depthfloat = 0.0f;
    unsigned before;
    size_t i;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetString = get_string;
    native.glesGetError = get_error;
    native.glesReadPixels = read_pixels;
    native.glesBindFramebuffer = bind_framebuffer;
    native.glesBindBuffer = bind_buffer;
    native.glesPixelStorei = pixel_store;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 64, 64);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glReadPixels(1, 1, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, &packed);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || packed != 0x80000003u ||
        last_format != GL_DEPTH_STENCIL || last_type != GL_UNSIGNED_INT_24_8 ||
        pack_binding != 0u || pack_alignment != 4 || last_framebuffer_target != GL_READ_FRAMEBUFFER)
        return fail(2, "native 24/8 readback and state restoration");

    before = read_calls;
    glReadnPixels(1, 1, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 3, &guarded[1]);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || read_calls != before ||
        guarded[1] != 0xa5a5a5a5u)
        return fail(3, "bounded 24/8 rejection");
    glReadnPixels(1, 1, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 4, &guarded[1]);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || guarded[0] != 0xa5a5a5a5u ||
        guarded[1] != 0x80000003u || guarded[2] != 0xa5a5a5a5u)
        return fail(4, "bounded 24/8 data and guards");

    glReadPixels(1, 1, 1, 1, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, float_packed);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || float_packed[0] != 0x3f000000u ||
        float_packed[1] != 7u || last_type != GL_FLOAT_32_UNSIGNED_INT_24_8_REV)
        return fail(5, "native floating depth/stencil readback");

    memset(layout, 0xa5, sizeof(layout));
    glPixelStorei(GL_PACK_ROW_LENGTH, 3);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 1);
    glPixelStorei(GL_PACK_SKIP_ROWS, 1);
    glReadPixels(0, 0, 2, 2, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, layout);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || pack_row_length != 3 ||
        pack_skip_pixels != 1 || pack_skip_rows != 1 || pack_alignment != 4)
        return fail(6, "pack state restoration");
    for (i = 0u; i < sizeof(layout); ++i) {
        if ((i >= 16u && i < 24u) || (i >= 28u && i < 36u))
            continue;
        if (layout[i] != 0xa5)
            return fail(7, "pixel pack guard");
    }
    memcpy(&packed, layout + 16u, sizeof(packed));
    if (packed != 0x80000003u)
        return fail(8, "packed row start");
    memcpy(&packed, layout + 28u, sizeof(packed));
    if (packed != 0x80000005u)
        return fail(9, "packed second row");

    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SWAP_BYTES, GL_TRUE);
    packed = 0x80000003u;
    memcpy(bytes, &packed, sizeof(bytes));
    for (i = 0u; i < sizeof(bytes); ++i)
        swapped[i] = bytes[3u - i];
    memset(bytes, 0, sizeof(bytes));
    glReadPixels(0, 0, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, bytes);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || memcmp(bytes, swapped, sizeof(bytes)) != 0)
        return fail(10, "packed byte swap");
    {
        unsigned char packed_float_bytes[8] = {0};
        unsigned char expected_float_bytes[8];
        uint32_t float_words[2] = {0x3f000000u, 7u};
        unsigned char depth_guard[6] = {0xa5u, 0xa5u, 0u, 0u, 0xa5u, 0xa5u};
        unsigned char stencil_guard[3] = {0xa5u, 0u, 0xa5u};
        memcpy(expected_float_bytes, float_words, sizeof(float_words));
        for (i = 0u; i < 2u; ++i) {
            unsigned char temp = expected_float_bytes[i];
            expected_float_bytes[i] = expected_float_bytes[3u - i];
            expected_float_bytes[3u - i] = temp;
            temp = expected_float_bytes[4u + i];
            expected_float_bytes[4u + i] = expected_float_bytes[7u - i];
            expected_float_bytes[7u - i] = temp;
        }
        glReadPixels(0, 0, 1, 1, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV,
                     packed_float_bytes);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(packed_float_bytes, expected_float_bytes, sizeof(packed_float_bytes)) != 0)
            return fail(21, "floating packed depth/stencil swaps both 32-bit components");
        glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, depth_guard + 2u);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || depth_guard[0] != 0xa5u ||
            depth_guard[1] != 0xa5u || depth_guard[2] != 0x34u || depth_guard[3] != 0x56u ||
            depth_guard[4] != 0xa5u || depth_guard[5] != 0xa5u)
            return fail(22, "16-bit depth swaps within pixel and preserves guards");
        glReadPixels(0, 0, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencil_guard + 1u);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || stencil_guard[0] != 0xa5u ||
            stencil_guard[1] != 7u || stencil_guard[2] != 0xa5u)
            return fail(23, "8-bit stencil preserves adjacent guard bytes");
    }
    glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);

    before = read_calls;
    glReadPixels(0, 0, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_BYTE, &packed);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM || read_calls != before)
        return fail(11, "invalid depth/stencil format pair");
    extension_enabled = 0;
    packed = 0xa5a5a5a5u;
    glReadPixels(0, 0, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, &packed);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || read_calls != before ||
        packed != 0xa5a5a5a5u)
        return fail(12, "extension gating");
    extension_enabled = 1;

    glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, &depth16);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || depth16 != 0x3456u ||
        last_format != GL_DEPTH_COMPONENT || last_type != GL_UNSIGNED_SHORT)
        return fail(13, "native unsigned-short depth readback");
    glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, &depth32);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || depth32 != 0x56789abcu ||
        last_type != GL_UNSIGNED_INT)
        return fail(14, "native unsigned-int depth readback");
    glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depthfloat);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || depthfloat != 0.75f || last_type != GL_FLOAT)
        return fail(15, "native float depth readback");
    reject_float_depth = 1;
    depthfloat = -1.0f;
    glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depthfloat);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        depthfloat != (float)((double)0x56789abcu / (double)UINT32_MAX) ||
        last_format != GL_DEPTH_COMPONENT || last_type != GL_UNSIGNED_INT)
        return fail(24, "normalized integer depth readback converts to float");
    reject_integer_depth = 1;
    depthfloat = -1.0f;
    glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depthfloat);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || depthfloat != -1.0f)
        return fail(25, "unsupported depth readback preserves native error and destination");
    reject_float_depth = 0;
    reject_integer_depth = 0;
    glReadPixels(0, 0, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &stencil);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || stencil != 7u ||
        last_format != GL_STENCIL_INDEX || last_type != GL_UNSIGNED_BYTE)
        return fail(16, "native unsigned-byte stencil readback");

    glReadnPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 1, &depth16);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || depth16 != 0x3456u)
        return fail(17, "bounded depth rejection");
    glReadnPixels(0, 0, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, 1, &stencil);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || stencil != 7u)
        return fail(18, "bounded stencil readback");

    depth_enabled = 0;
    depth16 = 0xa5a5u;
    glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, &depth16);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || depth16 != 0xa5a5u)
        return fail(19, "depth extension gating");
    depth_enabled = 1;
    stencil_enabled = 0;
    stencil = 0xa5u;
    glReadPixels(0, 0, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &stencil);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || stencil != 0xa5u)
        return fail(20, "stencil extension gating");
    stencil_enabled = 1;

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

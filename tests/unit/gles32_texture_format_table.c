#include "mgl/types.h"

#include <stddef.h>
#include <stdio.h>

extern size_t mgl_gles_native_upload_pixel_size(GLenum internal, GLenum format, GLenum type);

struct format_case {
    GLenum internal;
    GLenum format;
    GLenum type;
    size_t size;
};

int main(void) {
    static const struct format_case valid[] = {
        {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4u},
        {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_BYTE, 4u},
        {GL_RGBA4, GL_RGBA, GL_UNSIGNED_BYTE, 4u},
        {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, 4u},
        {GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, 4u},
        {GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 2u},
        {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 2u},
        {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 4u},
        {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 4u},
        {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8u},
        {GL_RGBA32F, GL_RGBA, GL_FLOAT, 16u},
        {GL_RGBA16F, GL_RGBA, GL_FLOAT, 16u},
        {GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, 4u},
        {GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, 4u},
        {GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, 8u},
        {GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT, 8u},
        {GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, 16u},
        {GL_RGBA32I, GL_RGBA_INTEGER, GL_INT, 16u},
        {GL_RGB10_A2UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, 4u},
        {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, 3u},
        {GL_RGB565, GL_RGB, GL_UNSIGNED_BYTE, 3u},
        {GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE, 3u},
        {GL_RGB8_SNORM, GL_RGB, GL_BYTE, 3u},
        {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2u},
        {GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, 4u},
        {GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, 4u},
        {GL_RGB16F, GL_RGB, GL_HALF_FLOAT, 6u},
        {GL_R11F_G11F_B10F, GL_RGB, GL_HALF_FLOAT, 6u},
        {GL_RGB9_E5, GL_RGB, GL_HALF_FLOAT, 6u},
        {GL_RGB32F_ARB, GL_RGB, GL_FLOAT, 12u},
        {GL_RGB16F, GL_RGB, GL_FLOAT, 12u},
        {GL_R11F_G11F_B10F, GL_RGB, GL_FLOAT, 12u},
        {GL_RGB9_E5, GL_RGB, GL_FLOAT, 12u},
        {GL_RGB8UI, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, 3u},
        {GL_RGB8I, GL_RGB_INTEGER, GL_BYTE, 3u},
        {GL_RGB16UI, GL_RGB_INTEGER, GL_UNSIGNED_SHORT, 6u},
        {GL_RGB16I, GL_RGB_INTEGER, GL_SHORT, 6u},
        {GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT, 12u},
        {GL_RGB32I, GL_RGB_INTEGER, GL_INT, 12u},
        {GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2u},
        {GL_RG8_SNORM, GL_RG, GL_BYTE, 2u},
        {GL_RG16F, GL_RG, GL_HALF_FLOAT, 4u},
        {GL_RG32F, GL_RG, GL_FLOAT, 8u},
        {GL_RG16F, GL_RG, GL_FLOAT, 8u},
        {GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE, 2u},
        {GL_RG8I, GL_RG_INTEGER, GL_BYTE, 2u},
        {GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT, 4u},
        {GL_RG16I, GL_RG_INTEGER, GL_SHORT, 4u},
        {GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, 8u},
        {GL_RG32I, GL_RG_INTEGER, GL_INT, 8u},
        {GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1u},
        {GL_R8_SNORM, GL_RED, GL_BYTE, 1u},
        {GL_R16F, GL_RED, GL_HALF_FLOAT, 2u},
        {GL_R32F, GL_RED, GL_FLOAT, 4u},
        {GL_R16F, GL_RED, GL_FLOAT, 4u},
        {GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, 1u},
        {GL_R8I, GL_RED_INTEGER, GL_BYTE, 1u},
        {GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, 2u},
        {GL_R16I, GL_RED_INTEGER, GL_SHORT, 2u},
        {GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, 4u},
        {GL_R32I, GL_RED_INTEGER, GL_INT, 4u},
        {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 2u},
        {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 4u},
        {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 4u},
        {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, 4u},
        {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 4u},
        {GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 8u},
        {GL_STENCIL_INDEX8, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, 1u},
        {GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, 4u},
        {GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 2u},
        {GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 2u},
        {GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, 3u},
        {GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2u},
    };
    static const struct format_case invalid[] = {
        {GL_RGB8, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0u},
        {GL_SRGB8, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0u},
        {GL_RGB8, GL_RGBA, GL_UNSIGNED_BYTE, 0u},
        {GL_RGBA8, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 0u},
        {GL_RGBA8, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 0u},
        {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 0u},
        {GL_RGB8_SNORM, GL_RGB, GL_UNSIGNED_BYTE, 0u},
        {GL_RGB10_A2, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, 0u},
        {GL_RGB10_A2UI, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 0u},
        {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0u},
        {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0u},
        {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 0u},
        {GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 0u},
        {GL_R32F, GL_RED, GL_HALF_FLOAT, 0u},
        {GL_R8, GL_RED, GL_BYTE, 0u},
        {GL_RGB, GL_RGB, GL_FLOAT, 0u},
        {GL_RED, GL_RED, GL_UNSIGNED_BYTE, 0u},
        {GL_RG, GL_RG, GL_UNSIGNED_BYTE, 0u},
        {GL_STENCIL_INDEX8, GL_STENCIL_INDEX, GL_UNSIGNED_INT, 0u},
        {GL_RGBA8I, GL_RGBA, GL_BYTE, 0u}};
    size_t i, j, got;
    for (i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i) {
        got = mgl_gles_native_upload_pixel_size(valid[i].internal, valid[i].format, valid[i].type);
        if (got != valid[i].size) {
            fprintf(stderr, "valid format row %zu expected %zu got %zu\n", i, valid[i].size, got);
            return 1;
        }
    }
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        got = mgl_gles_native_upload_pixel_size(invalid[i].internal, invalid[i].format,
                                                invalid[i].type);
        if (got != 0u) {
            fprintf(stderr, "invalid format row %zu returned %zu\n", i, got);
            return 2;
        }
    }
    for (i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i) {
        for (j = 0; j < sizeof(valid) / sizeof(valid[0]); ++j) {
            size_t expected = 0u;
            size_t k;
            for (k = 0; k < sizeof(valid) / sizeof(valid[0]); ++k) {
                if (valid[k].internal == valid[i].internal && valid[k].format == valid[j].format &&
                    valid[k].type == valid[j].type) {
                    expected = valid[k].size;
                    break;
                }
            }
            got = mgl_gles_native_upload_pixel_size(valid[i].internal, valid[j].format,
                                                    valid[j].type);
            if (got != expected) {
                fprintf(stderr, "cross format row %zu type row %zu expected %zu got %zu\n", i, j,
                        expected, got);
                return 3;
            }
        }
    }
    return 0;
}

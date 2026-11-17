#ifndef MGL_TEXTURE_VIEW_FORMATS_H
#define MGL_TEXTURE_VIEW_FORMATS_H

#include "mgl/types.h"

static inline GLenum mgl_texture_view_format_class(GLenum format) {
    switch (format) {
        case GL_RGBA32F:
        case GL_RGBA32UI:
        case GL_RGBA32I:
            return GL_VIEW_CLASS_128_BITS;
        case GL_RGB32F_ARB:
        case GL_RGB32UI:
        case GL_RGB32I:
            return GL_VIEW_CLASS_96_BITS;
        case GL_RGBA16F:
        case GL_RG32F:
        case GL_RGBA16UI:
        case GL_RG32UI:
        case GL_RGBA16I:
        case GL_RG32I:
        case GL_RGBA16:
        case GL_RGBA16_SNORM:
            return GL_VIEW_CLASS_64_BITS;
        case GL_RGB16:
        case GL_RGB16_SNORM:
        case GL_RGB16F:
        case GL_RGB16UI:
        case GL_RGB16I:
            return GL_VIEW_CLASS_48_BITS;
        case GL_RG16F:
        case GL_R11F_G11F_B10F:
        case GL_R32F:
        case GL_RGB10_A2UI:
        case GL_RGBA8UI:
        case GL_RG16UI:
        case GL_R32UI:
        case GL_RGBA8I:
        case GL_RG16I:
        case GL_R32I:
        case GL_RGB10_A2:
        case GL_RGBA8:
        case GL_RG16:
        case GL_RGBA8_SNORM:
        case GL_RG16_SNORM:
        case GL_SRGB8_ALPHA8:
        case GL_RGB9_E5:
            return GL_VIEW_CLASS_32_BITS;
        case GL_RGB8:
        case GL_RGB8_SNORM:
        case GL_SRGB8:
        case GL_RGB8UI:
        case GL_RGB8I:
            return GL_VIEW_CLASS_24_BITS;
        case GL_R16F:
        case GL_RG8UI:
        case GL_R16UI:
        case GL_RG8I:
        case GL_R16I:
        case GL_RG8:
        case GL_R16:
        case GL_RG8_SNORM:
        case GL_R16_SNORM:
            return GL_VIEW_CLASS_16_BITS;
        case GL_R8UI:
        case GL_R8I:
        case GL_R8:
        case GL_R8_SNORM:
            return GL_VIEW_CLASS_8_BITS;
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
            return GL_VIEW_CLASS_S3TC_DXT1_RGB;
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
            return GL_VIEW_CLASS_S3TC_DXT1_RGBA;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
            return GL_VIEW_CLASS_S3TC_DXT3_RGBA;
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            return GL_VIEW_CLASS_S3TC_DXT5_RGBA;
        case GL_COMPRESSED_RED_RGTC1:
        case GL_COMPRESSED_SIGNED_RED_RGTC1:
            return GL_VIEW_CLASS_RGTC1_RED;
        case GL_COMPRESSED_RG_RGTC2:
        case GL_COMPRESSED_SIGNED_RG_RGTC2:
            return GL_VIEW_CLASS_RGTC2_RG;
        case GL_COMPRESSED_RGBA_BPTC_UNORM:
        case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:
            return GL_VIEW_CLASS_BPTC_UNORM;
        case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT:
        case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:
            return GL_VIEW_CLASS_BPTC_FLOAT;
        default:
            return GL_NONE;
    }
}

static inline int mgl_texture_view_formats_compatible(GLenum original, GLenum view) {
    GLenum original_class = mgl_texture_view_format_class(original);
    return original == view ||
           (original_class != GL_NONE && original_class == mgl_texture_view_format_class(view));
}

#endif

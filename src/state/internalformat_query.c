#include "mgl/gl.h"
#include "state/internalformat_query.h"
#include "texture/view_formats.h"
#include "texture/renderable_formats.h"
#include "texture/texture_store.h"

#include "core/context.h"
#include "core/render_diag.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct mgl_internalformat_info {
    GLint bits[6];
    GLenum component_type;
    GLboolean supported;
    GLboolean filterable;
    GLboolean color_renderable;
    GLboolean image;
    GLboolean srgb;
    GLboolean compressed;
    GLboolean integer;
    GLint block_bytes;
};

static struct mgl_internalformat_info internalformat_info(GLenum format) {
    struct mgl_internalformat_info info = {0};
    info.component_type = GL_UNSIGNED_NORMALIZED;
    info.filterable = GL_TRUE;
    info.color_renderable = GL_TRUE;
    switch (format) {
        case GL_R8:
            info.bits[0] = 8;
            break;
        case GL_RG8:
            info.bits[0] = info.bits[1] = 8;
            break;
        case GL_RGB8:
            info.bits[0] = info.bits[1] = info.bits[2] = 8;
            break;
        case GL_RGBA8:
            info.bits[0] = info.bits[1] = info.bits[2] = info.bits[3] = 8;
            info.image = GL_TRUE;
            break;
        case GL_SRGB8:
            info.bits[0] = info.bits[1] = info.bits[2] = 8;
            info.srgb = GL_TRUE;
            break;
        case GL_SRGB8_ALPHA8:
            info.bits[0] = info.bits[1] = info.bits[2] = info.bits[3] = 8;
            info.srgb = GL_TRUE;
            break;
        case GL_R16:
            info.bits[0] = 16;
            break;
        case GL_RG16:
            info.bits[0] = info.bits[1] = 16;
            break;
        case GL_RGBA16:
            info.bits[0] = info.bits[1] = info.bits[2] = info.bits[3] = 16;
            break;
        case GL_RGB16:
            info.bits[0] = info.bits[1] = info.bits[2] = 16;
            info.color_renderable = GL_FALSE;
            break;
        case GL_R16_SNORM:
            info.bits[0] = 16;
            info.component_type = GL_SIGNED_NORMALIZED;
            break;
        case GL_RG16_SNORM:
            info.bits[0] = info.bits[1] = 16;
            info.component_type = GL_SIGNED_NORMALIZED;
            break;
        case GL_RGBA16_SNORM:
            info.bits[0] = info.bits[1] = info.bits[2] = info.bits[3] = 16;
            info.component_type = GL_SIGNED_NORMALIZED;
            break;
        case GL_RGB16_SNORM:
            info.bits[0] = info.bits[1] = info.bits[2] = 16;
            info.component_type = GL_SIGNED_NORMALIZED;
            info.color_renderable = GL_FALSE;
            break;
        case GL_R8_SNORM:
            info.bits[0] = 8;
            info.component_type = GL_SIGNED_NORMALIZED;
            break;
        case GL_RG8_SNORM:
            info.bits[0] = info.bits[1] = 8;
            info.component_type = GL_SIGNED_NORMALIZED;
            break;
        case GL_RGB8_SNORM:
            info.bits[0] = info.bits[1] = info.bits[2] = 8;
            info.component_type = GL_SIGNED_NORMALIZED;
            break;
        case GL_RGBA8_SNORM:
            info.bits[0] = info.bits[1] = info.bits[2] = info.bits[3] = 8;
            info.component_type = GL_SIGNED_NORMALIZED;
            break;
        case GL_R16F:
            info.bits[0] = 16;
            info.component_type = GL_FLOAT;
            info.image = GL_TRUE;
            break;
        case GL_RG16F:
            info.bits[0] = info.bits[1] = 16;
            info.component_type = GL_FLOAT;
            info.image = GL_TRUE;
            break;
        case GL_RGB16F:
            info.bits[0] = info.bits[1] = info.bits[2] = 16;
            info.component_type = GL_FLOAT;
            break;
        case GL_RGBA16F:
            info.bits[0] = info.bits[1] = info.bits[2] = info.bits[3] = 16;
            info.component_type = GL_FLOAT;
            info.image = GL_TRUE;
            break;
        case GL_R32F:
            info.bits[0] = 32;
            info.component_type = GL_FLOAT;
            info.image = GL_TRUE;
            break;
        case GL_RG32F:
            info.bits[0] = info.bits[1] = 32;
            info.component_type = GL_FLOAT;
            info.image = GL_TRUE;
            break;
        case GL_RGB32F_ARB:
            info.bits[0] = info.bits[1] = info.bits[2] = 32;
            info.component_type = GL_FLOAT;
            break;
        case GL_RGBA32F:
            info.bits[0] = info.bits[1] = info.bits[2] = info.bits[3] = 32;
            info.component_type = GL_FLOAT;
            info.image = GL_TRUE;
            break;
        case GL_R8I:
        case GL_R8UI:
            info.bits[0] = 8;
            goto integer_format;
        case GL_RG8I:
        case GL_RG8UI:
            info.bits[0] = info.bits[1] = 8;
            goto integer_format;
        case GL_RGB8I:
        case GL_RGB8UI:
            info.bits[0] = info.bits[1] = info.bits[2] = 8;
            goto integer_format;
        case GL_RGBA8I:
        case GL_RGBA8UI:
            info.bits[0] = info.bits[1] = info.bits[2] = info.bits[3] = 8;
            info.image = GL_TRUE;
            goto integer_format;
        case GL_R16I:
        case GL_R16UI:
            info.bits[0] = 16;
            info.image = GL_TRUE;
            goto integer_format;
        case GL_RG16I:
        case GL_RG16UI:
            info.bits[0] = info.bits[1] = 16;
            info.image = GL_TRUE;
            goto integer_format;
        case GL_RGBA16I:
        case GL_RGBA16UI:
            info.bits[0] = info.bits[1] = info.bits[2] = info.bits[3] = 16;
            info.image = GL_TRUE;
            goto integer_format;
        case GL_R32I:
        case GL_R32UI:
            info.bits[0] = 32;
            info.image = GL_TRUE;
            goto integer_format;
        case GL_RG32I:
        case GL_RG32UI:
            info.bits[0] = info.bits[1] = 32;
            info.image = GL_TRUE;
            goto integer_format;
        case GL_RGBA32I:
        case GL_RGBA32UI:
            info.bits[0] = info.bits[1] = info.bits[2] = info.bits[3] = 32;
            info.image = GL_TRUE;
            goto integer_format;
        case GL_RGB565:
            info.bits[0] = 5;
            info.bits[1] = 6;
            info.bits[2] = 5;
            break;
        case GL_RGBA4:
            info.bits[0] = info.bits[1] = info.bits[2] = info.bits[3] = 4;
            break;
        case GL_RGB5_A1:
            info.bits[0] = info.bits[1] = info.bits[2] = 5;
            info.bits[3] = 1;
            break;
        case GL_R11F_G11F_B10F:
            info.bits[0] = info.bits[1] = 11;
            info.bits[2] = 10;
            info.component_type = GL_FLOAT;
            break;
        case GL_RGB9_E5:
            info.bits[0] = info.bits[1] = info.bits[2] = 9;
            info.component_type = GL_FLOAT;
            break;
        case GL_RGB5:
            info.bits[0] = info.bits[1] = info.bits[2] = 5;
            break;
        case GL_RGB10_A2:
            info.bits[0] = info.bits[1] = info.bits[2] = 10;
            info.bits[3] = 2;
            info.image = GL_TRUE;
            break;
        case GL_RGB10_A2UI:
            info.bits[0] = info.bits[1] = info.bits[2] = 10;
            info.bits[3] = 2;
            goto integer_format;
        case GL_DEPTH_COMPONENT16:
            info.bits[4] = 16;
            goto depth_format;
        case GL_DEPTH_COMPONENT24:
            info.bits[4] = 24;
            goto depth_format;
        case GL_DEPTH_COMPONENT32:
            info.bits[4] = 32;
            info.component_type = GL_FLOAT;
            goto depth_format;
        case GL_DEPTH_COMPONENT32F:
            info.bits[4] = 32;
            info.component_type = GL_FLOAT;
            goto depth_format;
        case GL_DEPTH24_STENCIL8:
            info.bits[4] = 24;
            info.bits[5] = 8;
            goto depth_format;
        case GL_DEPTH32F_STENCIL8:
            info.bits[4] = 32;
            info.bits[5] = 8;
            info.component_type = GL_FLOAT;
            goto depth_format;
        case GL_STENCIL_INDEX8:
            info.bits[5] = 8;
            goto depth_format;
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
            info.bits[3] = 1;
            info.block_bytes = 8;
            goto compressed_format;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            info.bits[3] = 8;
            info.block_bytes = 16;
            goto compressed_format;
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RED_RGTC1:
        case GL_COMPRESSED_SIGNED_RED_RGTC1:
            info.block_bytes = 8;
            goto compressed_format;
        case GL_COMPRESSED_RG_RGTC2:
        case GL_COMPRESSED_SIGNED_RG_RGTC2:
        case GL_COMPRESSED_RGBA_BPTC_UNORM:
        case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:
        case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT:
        case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:
            info.block_bytes = 16;
            goto compressed_format;
        default:
            return info;
    }
    goto supported_format;
integer_format:
    info.integer = GL_TRUE;
    info.filterable = GL_FALSE;
    info.component_type = format == GL_R8I || format == GL_RG8I || format == GL_RGB8I ||
                                  format == GL_RGBA8I || format == GL_R16I || format == GL_RG16I ||
                                  format == GL_RGBA16I || format == GL_R32I || format == GL_RG32I ||
                                  format == GL_RGBA32I
                              ? GL_INT
                              : GL_UNSIGNED_INT;
    goto supported_format;
depth_format:
    info.color_renderable = GL_FALSE;
    info.image = GL_FALSE;
    goto supported_format;
compressed_format:
    info.bits[0] = info.bits[1] = info.bits[2] = 8;
    info.color_renderable = GL_FALSE;
    info.compressed = GL_TRUE;
    info.srgb = format == GL_COMPRESSED_SRGB_S3TC_DXT1_EXT ||
                format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT ||
                format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT ||
                format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT ||
                format == GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
    if (format == GL_COMPRESSED_RED_RGTC1 || format == GL_COMPRESSED_SIGNED_RED_RGTC1)
        info.bits[1] = info.bits[2] = 0;
    if (format == GL_COMPRESSED_RG_RGTC2 || format == GL_COMPRESSED_SIGNED_RG_RGTC2)
        info.bits[2] = 0;
    if (format == GL_COMPRESSED_SIGNED_RED_RGTC1 || format == GL_COMPRESSED_SIGNED_RG_RGTC2)
        info.component_type = GL_SIGNED_NORMALIZED;
    if (format == GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT ||
        format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT)
        info.component_type = GL_FLOAT;
supported_format:
    info.supported = GL_TRUE;
    return info;
}

GLint mgl_texture_level_component_value(GLenum format, GLenum pname) {
    struct mgl_internalformat_info info;
    unsigned component;
    switch (format) {
        case GL_RED:
            format = GL_R8;
            break;
        case GL_RG:
            format = GL_RG8;
            break;
        case GL_RGB:
            format = GL_RGB8;
            break;
        case GL_RGBA:
            format = GL_RGBA8;
            break;
        case GL_DEPTH_COMPONENT:
            format = GL_DEPTH_COMPONENT16;
            break;
        case GL_DEPTH_STENCIL:
            format = GL_DEPTH24_STENCIL8;
            break;
        default:
            break;
    }
    info = internalformat_info(format);
    switch (pname) {
        case GL_TEXTURE_STENCIL_SIZE:
            return info.bits[5];
        case GL_TEXTURE_SHARED_SIZE:
            return format == GL_RGB9_E5 ? 5 : 0;
        case GL_TEXTURE_RED_SIZE:
        case GL_TEXTURE_RED_TYPE:
            component = 0u;
            break;
        case GL_TEXTURE_GREEN_SIZE:
        case GL_TEXTURE_GREEN_TYPE:
            component = 1u;
            break;
        case GL_TEXTURE_BLUE_SIZE:
        case GL_TEXTURE_BLUE_TYPE:
            component = 2u;
            break;
        case GL_TEXTURE_ALPHA_SIZE:
        case GL_TEXTURE_ALPHA_TYPE:
            component = 3u;
            break;
        case GL_TEXTURE_DEPTH_SIZE:
        case GL_TEXTURE_DEPTH_TYPE:
            component = 4u;
            break;
        default:
            return 0;
    }
    if (pname == GL_TEXTURE_RED_TYPE || pname == GL_TEXTURE_GREEN_TYPE ||
        pname == GL_TEXTURE_BLUE_TYPE || pname == GL_TEXTURE_ALPHA_TYPE ||
        pname == GL_TEXTURE_DEPTH_TYPE)
        return info.bits[component] ? (GLint)info.component_type : (GLint)GL_NONE;
    return info.bits[component];
}

static int valid_target(GLenum target) {
    switch (target) {
        case GL_TEXTURE_1D:
        case GL_TEXTURE_1D_ARRAY:
        case GL_TEXTURE_2D:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_CUBE_MAP:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
        case GL_TEXTURE_RECTANGLE:
        case GL_TEXTURE_BUFFER:
        case GL_RENDERBUFFER:
        case GL_TEXTURE_2D_MULTISAMPLE:
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
            return 1;
        default:
            return 0;
    }
}

static int valid_pname(GLenum pname) {
    switch (pname) {
        case GL_SAMPLES:
        case GL_NUM_SAMPLE_COUNTS:
        case GL_INTERNALFORMAT_SUPPORTED:
        case GL_INTERNALFORMAT_PREFERRED:
        case GL_INTERNALFORMAT_RED_SIZE:
        case GL_INTERNALFORMAT_GREEN_SIZE:
        case GL_INTERNALFORMAT_BLUE_SIZE:
        case GL_INTERNALFORMAT_ALPHA_SIZE:
        case GL_INTERNALFORMAT_DEPTH_SIZE:
        case GL_INTERNALFORMAT_STENCIL_SIZE:
        case GL_INTERNALFORMAT_SHARED_SIZE:
        case GL_INTERNALFORMAT_RED_TYPE:
        case GL_INTERNALFORMAT_GREEN_TYPE:
        case GL_INTERNALFORMAT_BLUE_TYPE:
        case GL_INTERNALFORMAT_ALPHA_TYPE:
        case GL_INTERNALFORMAT_DEPTH_TYPE:
        case GL_INTERNALFORMAT_STENCIL_TYPE:
        case GL_MAX_WIDTH:
        case GL_MAX_HEIGHT:
        case GL_MAX_DEPTH:
        case GL_MAX_LAYERS:
        case GL_MAX_COMBINED_DIMENSIONS:
        case GL_COLOR_COMPONENTS:
        case GL_DEPTH_COMPONENTS:
        case GL_STENCIL_COMPONENTS:
        case GL_COLOR_RENDERABLE:
        case GL_DEPTH_RENDERABLE:
        case GL_STENCIL_RENDERABLE:
        case GL_FRAMEBUFFER_RENDERABLE:
        case GL_FRAMEBUFFER_RENDERABLE_LAYERED:
        case GL_FRAMEBUFFER_BLEND:
        case GL_READ_PIXELS:
        case GL_READ_PIXELS_FORMAT:
        case GL_READ_PIXELS_TYPE:
        case GL_TEXTURE_IMAGE_FORMAT:
        case GL_TEXTURE_IMAGE_TYPE:
        case GL_GET_TEXTURE_IMAGE_FORMAT:
        case GL_GET_TEXTURE_IMAGE_TYPE:
        case GL_MIPMAP:
        case GL_MANUAL_GENERATE_MIPMAP:
        case GL_AUTO_GENERATE_MIPMAP:
        case GL_COLOR_ENCODING:
        case GL_SRGB_READ:
        case GL_SRGB_WRITE:
        case GL_FILTER:
        case GL_VERTEX_TEXTURE:
        case GL_TESS_CONTROL_TEXTURE:
        case GL_TESS_EVALUATION_TEXTURE:
        case GL_GEOMETRY_TEXTURE:
        case GL_FRAGMENT_TEXTURE:
        case GL_COMPUTE_TEXTURE:
        case GL_TEXTURE_SHADOW:
        case GL_TEXTURE_GATHER:
        case GL_TEXTURE_GATHER_SHADOW:
        case GL_SHADER_IMAGE_LOAD:
        case GL_SHADER_IMAGE_STORE:
        case GL_SHADER_IMAGE_ATOMIC:
        case GL_IMAGE_TEXEL_SIZE:
        case GL_IMAGE_COMPATIBILITY_CLASS:
        case GL_IMAGE_PIXEL_FORMAT:
        case GL_IMAGE_PIXEL_TYPE:
        case GL_IMAGE_FORMAT_COMPATIBILITY_TYPE:
        case GL_SIMULTANEOUS_TEXTURE_AND_DEPTH_TEST:
        case GL_SIMULTANEOUS_TEXTURE_AND_STENCIL_TEST:
        case GL_SIMULTANEOUS_TEXTURE_AND_DEPTH_WRITE:
        case GL_SIMULTANEOUS_TEXTURE_AND_STENCIL_WRITE:
        case GL_TEXTURE_COMPRESSED:
        case GL_TEXTURE_COMPRESSED_BLOCK_WIDTH:
        case GL_TEXTURE_COMPRESSED_BLOCK_HEIGHT:
        case GL_TEXTURE_COMPRESSED_BLOCK_SIZE:
        case GL_CLEAR_BUFFER:
        case GL_CLEAR_TEXTURE:
        case GL_TEXTURE_VIEW:
        case GL_VIEW_COMPATIBILITY_CLASS:
            return 1;
        default:
            return 0;
    }
}

static int multisample_target(GLenum target) {
    return target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY ||
           target == GL_RENDERBUFFER;
}

static int supported_combination(struct mgl_context *ctx, GLenum target, GLenum format,
                                 const struct mgl_internalformat_info *info) {
    if (target == GL_TEXTURE_1D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY)
        return 0;
    if (!info->supported)
        return 0;
    switch (format) {
        case GL_R16:
        case GL_RG16:
        case GL_RGB16:
        case GL_RGBA16:
        case GL_R16_SNORM:
        case GL_RG16_SNORM:
        case GL_RGB16_SNORM:
        case GL_RGBA16_SNORM:
            if (ctx->native_dispatch == NULL ||
                !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                        "GL_EXT_texture_norm16"))
                return 0;
            break;
        default:
            break;
    }
    if (target == GL_RENDERBUFFER || target == GL_TEXTURE_2D_MULTISAMPLE ||
        target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        return mgl_renderable_storage_format(ctx, format);
    if (target == GL_TEXTURE_BUFFER)
        return !info->compressed && !info->bits[4] && !info->bits[5];
    if (target == GL_TEXTURE_3D)
        return !info->compressed && !info->bits[5];
    if (target == GL_TEXTURE_1D)
        return !info->compressed && !info->bits[5];
    return 1;
}

static GLint limit_for_target(struct mgl_context *ctx, GLenum target, GLenum pname) {
    GLenum limit = GL_MAX_TEXTURE_SIZE;
    GLint value = 0;
    if (pname == GL_MAX_WIDTH && target == GL_RENDERBUFFER)
        limit = GL_MAX_RENDERBUFFER_SIZE;
    else if (target == GL_TEXTURE_BUFFER)
        limit = GL_MAX_TEXTURE_BUFFER_SIZE;
    else if (target == GL_RENDERBUFFER)
        limit = GL_MAX_RENDERBUFFER_SIZE;
    else if (target == GL_TEXTURE_3D)
        limit = GL_MAX_3D_TEXTURE_SIZE;
    else if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY)
        limit = GL_MAX_CUBE_MAP_TEXTURE_SIZE;
    if (pname == GL_MAX_LAYERS || (pname == GL_MAX_HEIGHT && target == GL_TEXTURE_1D_ARRAY) ||
        (pname == GL_MAX_DEPTH &&
         (target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY ||
          target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)))
        limit = GL_MAX_ARRAY_TEXTURE_LAYERS;
    if (limit == GL_MAX_TEXTURE_BUFFER_SIZE)
        value = MGL_MAX_TEXTURE_BUFFER_SIZE;
    else if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegerv != NULL)
        ctx->native_dispatch->glesGetIntegerv(limit, &value);
    return value > 0 ? value : 0;
}

static GLint unsupported_value(GLenum pname) {
    switch (pname) {
        case GL_INTERNALFORMAT_SUPPORTED:
        case GL_COLOR_COMPONENTS:
        case GL_DEPTH_COMPONENTS:
        case GL_STENCIL_COMPONENTS:
        case GL_COLOR_RENDERABLE:
        case GL_DEPTH_RENDERABLE:
        case GL_STENCIL_RENDERABLE:
        case GL_MIPMAP:
        case GL_TEXTURE_COMPRESSED:
            return GL_FALSE;
        case GL_NUM_SAMPLE_COUNTS:
        case GL_INTERNALFORMAT_RED_SIZE:
        case GL_INTERNALFORMAT_GREEN_SIZE:
        case GL_INTERNALFORMAT_BLUE_SIZE:
        case GL_INTERNALFORMAT_ALPHA_SIZE:
        case GL_INTERNALFORMAT_DEPTH_SIZE:
        case GL_INTERNALFORMAT_STENCIL_SIZE:
        case GL_INTERNALFORMAT_SHARED_SIZE:
        case GL_MAX_WIDTH:
        case GL_MAX_HEIGHT:
        case GL_MAX_DEPTH:
        case GL_MAX_LAYERS:
        case GL_MAX_COMBINED_DIMENSIONS:
        case GL_IMAGE_TEXEL_SIZE:
        case GL_TEXTURE_COMPRESSED_BLOCK_WIDTH:
        case GL_TEXTURE_COMPRESSED_BLOCK_HEIGHT:
        case GL_TEXTURE_COMPRESSED_BLOCK_SIZE:
            return 0;
        default:
            return GL_NONE;
    }
}

static int target_dimensions(GLenum target) {
    switch (target) {
        case GL_TEXTURE_BUFFER:
        case GL_TEXTURE_1D:
            return 1;
        case GL_RENDERBUFFER:
        case GL_TEXTURE_1D_ARRAY:
        case GL_TEXTURE_2D:
        case GL_TEXTURE_RECTANGLE:
        case GL_TEXTURE_CUBE_MAP:
        case GL_TEXTURE_2D_MULTISAMPLE:
            return 2;
        default:
            return 3;
    }
}

static GLint64 combined_dimensions(struct mgl_context *ctx, GLenum target, GLenum format,
                                   int supported) {
    GLint64 result;
    GLint width, height = 1, depth = 1;
    if (!supported)
        return 0;
    width = limit_for_target(ctx, target, GL_MAX_WIDTH);
    if (target_dimensions(target) >= 2)
        height = limit_for_target(ctx, target, GL_MAX_HEIGHT);
    if (target_dimensions(target) >= 3)
        depth = limit_for_target(ctx, target, GL_MAX_DEPTH);
    if (width <= 0 || height <= 0 || depth <= 0)
        return 0;
    result = (GLint64)width * height * depth;
    if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY)
        result *= 6;
    if (multisample_target(target) && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesGetInternalformativ != NULL) {
        GLint count = 0, samples = 0;
        ctx->native_dispatch->glesGetInternalformativ(target, format, GL_NUM_SAMPLE_COUNTS, 1,
                                                      &count);
        if (count > 0) {
            ctx->native_dispatch->glesGetInternalformativ(target, format, GL_SAMPLES, 1, &samples);
            if (samples > 0)
                result *= samples;
        }
    }
    return result;
}

static GLint native_limit(struct mgl_context *ctx, GLenum pname) {
    GLint value = 0;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegerv != NULL)
        ctx->native_dispatch->glesGetIntegerv(pname, &value);
    return value > 0 ? value : 0;
}

static int sampleable_target(struct mgl_context *ctx, GLenum target) {
    const struct mgl_native_dispatch *native = ctx->native_dispatch;
    if (native == NULL || native->glesBindTexture == NULL)
        return 0;
    switch (target) {
        case GL_TEXTURE_1D:
        case GL_TEXTURE_2D:
        case GL_TEXTURE_RECTANGLE:
        case GL_TEXTURE_CUBE_MAP:
            return native->glesTexImage2D != NULL;
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
            return native->glesTexImage3D != NULL;
        case GL_TEXTURE_BUFFER:
            return native->glesTexBufferEXT != NULL;
        default:
            return 0;
    }
}

static int framebuffer_support(struct mgl_context *ctx, GLenum target, GLenum format,
                               const struct mgl_internalformat_info *info) {
    const struct mgl_native_dispatch *native = ctx->native_dispatch;
    (void)info;
    if (native == NULL || !mgl_renderable_storage_format(ctx, format))
        return 0;
    if (target == GL_RENDERBUFFER)
        return native->glesRenderbufferStorage != NULL;
    if (multisample_target(target))
        return target == GL_TEXTURE_2D_MULTISAMPLE
                   ? native->glesTexStorage2DMultisample != NULL &&
                         native->glesFramebufferTexture2D != NULL
                   : native->glesTexStorage3DMultisample != NULL &&
                         native->glesFramebufferTextureLayer != NULL;
    if (target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY ||
        target == GL_TEXTURE_CUBE_MAP_ARRAY)
        return sampleable_target(ctx, target) && native->glesFramebufferTextureLayer != NULL;
    return sampleable_target(ctx, target) && native->glesFramebufferTexture2D != NULL;
}

static int image_support(struct mgl_context *ctx, GLenum target, GLenum format) {
    const struct mgl_native_dispatch *native = ctx->native_dispatch;
    if (native == NULL || native->glesBindImageTexture == NULL ||
        native_limit(ctx, GL_MAX_IMAGE_UNITS) == 0)
        return 0;
    switch (format) {
        case GL_RGBA32F:
        case GL_RGBA16F:
        case GL_R32F:
        case GL_RGBA32UI:
        case GL_RGBA16UI:
        case GL_RGBA8UI:
        case GL_R32UI:
        case GL_RGBA32I:
        case GL_RGBA16I:
        case GL_RGBA8I:
        case GL_R32I:
        case GL_RGBA8:
        case GL_RGBA8_SNORM:
            break;
        default:
            return 0;
    }
    switch (target) {
        case GL_TEXTURE_1D:
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP:
            return native->glesTexStorage2D != NULL;
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
            return native->glesTexStorage3D != NULL;
        default:
            return 0;
    }
}

static GLint image_compatibility_class(GLenum format) {
    switch (format) {
        case GL_RGBA32F:
        case GL_RGBA32I:
        case GL_RGBA32UI:
            return GL_IMAGE_CLASS_4_X_32;
        case GL_RGBA16F:
        case GL_RGBA16I:
        case GL_RGBA16UI:
            return GL_IMAGE_CLASS_4_X_16;
        case GL_RGBA8:
        case GL_RGBA8_SNORM:
        case GL_RGBA8I:
        case GL_RGBA8UI:
            return GL_IMAGE_CLASS_4_X_8;
        case GL_R32F:
        case GL_R32I:
        case GL_R32UI:
            return GL_IMAGE_CLASS_1_X_32;
        default:
            return GL_NONE;
    }
}

static GLint image_pixel_format(GLenum format) {
    switch (format) {
        case GL_RGBA32I:
        case GL_RGBA32UI:
        case GL_RGBA16I:
        case GL_RGBA16UI:
        case GL_RGBA8I:
        case GL_RGBA8UI:
            return GL_RGBA_INTEGER;
        case GL_R32I:
        case GL_R32UI:
            return GL_RED_INTEGER;
        case GL_R32F:
            return GL_RED;
        default:
            return GL_RGBA;
    }
}

static GLint image_pixel_type(GLenum format) {
    switch (format) {
        case GL_RGBA32F:
        case GL_R32F:
            return GL_FLOAT;
        case GL_RGBA16F:
            return GL_HALF_FLOAT;
        case GL_RGBA32I:
        case GL_R32I:
            return GL_INT;
        case GL_RGBA32UI:
        case GL_R32UI:
            return GL_UNSIGNED_INT;
        case GL_RGBA16I:
            return GL_SHORT;
        case GL_RGBA16UI:
            return GL_UNSIGNED_SHORT;
        case GL_RGBA8I:
        case GL_RGBA8_SNORM:
            return GL_BYTE;
        case GL_RGBA8UI:
        case GL_RGBA8:
            return GL_UNSIGNED_BYTE;
        default:
            return GL_NONE;
    }
}

static GLint texture_view_class(struct mgl_context *ctx, GLenum target, GLenum format) {
    const struct mgl_native_dispatch *native = ctx->native_dispatch;
    if (native == NULL || native->glesTextureView == NULL || native->glesGenTextures == NULL ||
        (target != GL_TEXTURE_2D && target != GL_TEXTURE_2D_ARRAY &&
         target != GL_TEXTURE_CUBE_MAP && target != GL_TEXTURE_CUBE_MAP_ARRAY) ||
        ((target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY)
             ? native->glesTexStorage3D == NULL : native->glesTexStorage2D == NULL) ||
        (!mgl_native_dispatch_has_gles_extension(native, "GL_EXT_texture_view") &&
         !mgl_native_dispatch_has_gles_extension(native, "GL_OES_texture_view")))
        return GL_NONE;
    return (GLint)mgl_texture_view_format_class(format);
}

static GLint texture_stage_support(struct mgl_context *ctx, GLenum target, GLenum pname) {
    GLenum limit;
    if (!sampleable_target(ctx, target))
        return GL_NONE;
    switch (pname) {
        case GL_VERTEX_TEXTURE:
            limit = GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS;
            break;
        case GL_TESS_CONTROL_TEXTURE:
            limit = GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS;
            break;
        case GL_TESS_EVALUATION_TEXTURE:
            limit = GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS;
            break;
        case GL_GEOMETRY_TEXTURE:
            limit = GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS;
            break;
        case GL_FRAGMENT_TEXTURE:
            limit = GL_MAX_TEXTURE_IMAGE_UNITS;
            break;
        case GL_COMPUTE_TEXTURE:
            limit = GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS;
            break;
        default:
            return GL_NONE;
    }
    return native_limit(ctx, limit) != 0 ? GL_FULL_SUPPORT : GL_NONE;
}

static GLint query_value(struct mgl_context *ctx, GLenum target, GLenum format, GLenum pname,
                         const struct mgl_internalformat_info *info, int supported) {
    GLint value = 0;
    if (pname == GL_INTERNALFORMAT_SUPPORTED)
        return supported ? GL_TRUE : GL_FALSE;
    if (pname == GL_NUM_SAMPLE_COUNTS)
        return 0;
    if (pname == GL_SAMPLES)
        return 0;
    if (!supported)
        return unsupported_value(pname);
    switch (pname) {
        case GL_INTERNALFORMAT_PREFERRED:
            return (GLint)format;
        case GL_INTERNALFORMAT_RED_SIZE:
            return info->bits[0];
        case GL_INTERNALFORMAT_GREEN_SIZE:
            return info->bits[1];
        case GL_INTERNALFORMAT_BLUE_SIZE:
            return info->bits[2];
        case GL_INTERNALFORMAT_ALPHA_SIZE:
            return info->bits[3];
        case GL_INTERNALFORMAT_DEPTH_SIZE:
            return info->bits[4];
        case GL_INTERNALFORMAT_STENCIL_SIZE:
            return info->bits[5];
        case GL_INTERNALFORMAT_SHARED_SIZE:
            return format == GL_RGB9_E5 ? 5 : 0;
        case GL_INTERNALFORMAT_RED_TYPE:
            return info->bits[0] ? (GLint)info->component_type : (GLint)GL_NONE;
        case GL_INTERNALFORMAT_GREEN_TYPE:
            return info->bits[1] ? (GLint)info->component_type : (GLint)GL_NONE;
        case GL_INTERNALFORMAT_BLUE_TYPE:
            return info->bits[2] ? (GLint)info->component_type : (GLint)GL_NONE;
        case GL_INTERNALFORMAT_ALPHA_TYPE:
            return info->bits[3] ? (GLint)info->component_type : (GLint)GL_NONE;
        case GL_INTERNALFORMAT_DEPTH_TYPE:
            return info->bits[4] ? (GLint)info->component_type : (GLint)GL_NONE;
        case GL_INTERNALFORMAT_STENCIL_TYPE:
            return info->bits[5] ? GL_UNSIGNED_INT : (GLint)GL_NONE;
        case GL_COLOR_COMPONENTS:
            return info->bits[0] || info->bits[1] || info->bits[2] || info->bits[3] ? GL_TRUE
                                                                                    : GL_FALSE;
        case GL_DEPTH_COMPONENTS:
            return info->bits[4] != 0 ? GL_TRUE : GL_FALSE;
        case GL_STENCIL_COMPONENTS:
            return info->bits[5] != 0 ? GL_TRUE : GL_FALSE;
        case GL_COLOR_RENDERABLE:
            return framebuffer_support(ctx, target, format, info) && info->color_renderable;
        case GL_DEPTH_RENDERABLE:
            return framebuffer_support(ctx, target, format, info) && info->bits[4] != 0;
        case GL_STENCIL_RENDERABLE:
            return framebuffer_support(ctx, target, format, info) && info->bits[5] != 0;
        case GL_FRAMEBUFFER_RENDERABLE:
            return framebuffer_support(ctx, target, format, info) ? GL_FULL_SUPPORT
                                                                  : (GLint)GL_NONE;
        case GL_FRAMEBUFFER_BLEND:
            return framebuffer_support(ctx, target, format, info) && info->color_renderable &&
                           !info->integer && ctx->native_dispatch->glesBlendFunc != NULL
                       ? GL_FULL_SUPPORT
                       : (GLint)GL_NONE;
        case GL_MAX_WIDTH:
        case GL_MAX_HEIGHT:
        case GL_MAX_DEPTH:
            if (target_dimensions(target) < (pname == GL_MAX_WIDTH    ? 1
                                             : pname == GL_MAX_HEIGHT ? 2
                                                                      : 3))
                return 0;
            return limit_for_target(ctx, target, pname);
        case GL_MAX_LAYERS:
            return target == GL_TEXTURE_1D_ARRAY || target == GL_TEXTURE_2D_ARRAY ||
                           target == GL_TEXTURE_CUBE_MAP_ARRAY ||
                           target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY
                       ? limit_for_target(ctx, target, pname)
                       : 0;
        case GL_MAX_COMBINED_DIMENSIONS:
            return (GLint)(GLuint)combined_dimensions(ctx, target, format, supported);
        case GL_COLOR_ENCODING:
            return info->srgb ? GL_SRGB : GL_LINEAR;
        case GL_TEXTURE_COMPRESSED:
            return info->compressed;
        case GL_TEXTURE_COMPRESSED_BLOCK_WIDTH:
        case GL_TEXTURE_COMPRESSED_BLOCK_HEIGHT:
            return info->compressed ? 4 : 0;
        case GL_TEXTURE_COMPRESSED_BLOCK_SIZE:
            return info->block_bytes;
        case GL_MIPMAP:
            return target != GL_RENDERBUFFER && !multisample_target(target) &&
                   target != GL_TEXTURE_BUFFER && target != GL_TEXTURE_RECTANGLE;
        case GL_MANUAL_GENERATE_MIPMAP:
            return target != GL_RENDERBUFFER && !multisample_target(target) &&
                           target != GL_TEXTURE_BUFFER && target != GL_TEXTURE_RECTANGLE
                       ? GL_FULL_SUPPORT
                       : (GLint)GL_NONE;
        case GL_FILTER:
            return info->filterable && sampleable_target(ctx, target) && target != GL_TEXTURE_BUFFER
                       ? GL_FULL_SUPPORT
                       : (GLint)GL_NONE;
        case GL_VERTEX_TEXTURE:
        case GL_FRAGMENT_TEXTURE:
        case GL_COMPUTE_TEXTURE:
            return texture_stage_support(ctx, target, pname);
        case GL_TEXTURE_GATHER:
            return sampleable_target(ctx, target) &&
                           (target == GL_TEXTURE_2D || target == GL_TEXTURE_2D_ARRAY ||
                            target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY)
                       ? GL_CAVEAT_SUPPORT
                       : (GLint)GL_NONE;
        case GL_SHADER_IMAGE_LOAD:
        case GL_SHADER_IMAGE_STORE:
            return image_support(ctx, target, format) ? GL_FULL_SUPPORT : (GLint)GL_NONE;
        case GL_SHADER_IMAGE_ATOMIC:
            return (format == GL_R32I || format == GL_R32UI) && image_support(ctx, target, format)
                       ? GL_FULL_SUPPORT
                       : (GLint)GL_NONE;
        case GL_SRGB_READ:
            return info->srgb && sampleable_target(ctx, target) ? GL_FULL_SUPPORT : (GLint)GL_NONE;
        case GL_SRGB_WRITE:
            return info->srgb && framebuffer_support(ctx, target, format, info) ? GL_CAVEAT_SUPPORT
                                                                                : (GLint)GL_NONE;
        case GL_READ_PIXELS:
            return framebuffer_support(ctx, target, format, info) &&
                           ctx->native_dispatch->glesReadPixels != NULL
                       ? GL_FULL_SUPPORT
                       : (GLint)GL_NONE;
        case GL_READ_PIXELS_FORMAT:
        case GL_TEXTURE_IMAGE_FORMAT:
        case GL_GET_TEXTURE_IMAGE_FORMAT:
            if (info->bits[4] && info->bits[5])
                return GL_DEPTH_STENCIL;
            if (info->bits[4])
                return GL_DEPTH_COMPONENT;
            if (info->bits[5])
                return GL_STENCIL_INDEX;
            if (info->bits[3])
                return GL_RGBA;
            if (info->bits[2])
                return GL_RGB;
            return info->bits[1] ? GL_RG : GL_RED;
        case GL_READ_PIXELS_TYPE:
        case GL_TEXTURE_IMAGE_TYPE:
        case GL_GET_TEXTURE_IMAGE_TYPE:
            if (info->bits[4] && info->bits[5])
                return GL_UNSIGNED_INT_24_8;
            return info->component_type == GL_FLOAT ? (GLint)GL_FLOAT
                   : info->integer                  ? (GLint)info->component_type
                                                    : (GLint)GL_UNSIGNED_BYTE;
        case GL_TESS_CONTROL_TEXTURE:
        case GL_TESS_EVALUATION_TEXTURE:
        case GL_GEOMETRY_TEXTURE:
            return texture_stage_support(ctx, target, pname);
        case GL_TEXTURE_SHADOW:
            return info->bits[4] && !info->bits[5] && sampleable_target(ctx, target)
                       ? GL_FULL_SUPPORT
                       : (GLint)GL_NONE;
        case GL_TEXTURE_GATHER_SHADOW:
            return info->bits[4] && !info->bits[5] && sampleable_target(ctx, target) &&
                           (target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP)
                       ? GL_CAVEAT_SUPPORT
                       : (GLint)GL_NONE;
        case GL_FRAMEBUFFER_RENDERABLE_LAYERED:
            return (target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY ||
                    target == GL_TEXTURE_CUBE_MAP_ARRAY) &&
                           framebuffer_support(ctx, target, format, info) &&
                           ctx->native_dispatch->glesFramebufferTexture != NULL
                       ? GL_FULL_SUPPORT
                       : (GLint)GL_NONE;
        case GL_AUTO_GENERATE_MIPMAP:
            return GL_NONE;
        case GL_IMAGE_TEXEL_SIZE:
            return image_support(ctx, target, format)
                       ? info->bits[0] + info->bits[1] + info->bits[2] + info->bits[3]
                       : 0;
        case GL_IMAGE_COMPATIBILITY_CLASS:
            return image_support(ctx, target, format) ? image_compatibility_class(format)
                                                      : (GLint)GL_NONE;
        case GL_IMAGE_PIXEL_FORMAT:
            return image_support(ctx, target, format) ? image_pixel_format(format) : (GLint)GL_NONE;
        case GL_IMAGE_PIXEL_TYPE:
            return image_support(ctx, target, format) ? image_pixel_type(format) : (GLint)GL_NONE;
        case GL_IMAGE_FORMAT_COMPATIBILITY_TYPE:
            return image_support(ctx, target, format) ? GL_IMAGE_FORMAT_COMPATIBILITY_BY_SIZE
                                                      : (GLint)GL_NONE;
        case GL_SIMULTANEOUS_TEXTURE_AND_DEPTH_TEST:
            return info->bits[4] && sampleable_target(ctx, target) ? GL_FULL_SUPPORT
                                                                   : (GLint)GL_NONE;
        case GL_SIMULTANEOUS_TEXTURE_AND_STENCIL_TEST:
            return info->bits[5] && sampleable_target(ctx, target) ? GL_FULL_SUPPORT
                                                                   : (GLint)GL_NONE;
        case GL_SIMULTANEOUS_TEXTURE_AND_DEPTH_WRITE:
        case GL_SIMULTANEOUS_TEXTURE_AND_STENCIL_WRITE:
            return GL_NONE;
        case GL_CLEAR_BUFFER:
            return GL_NONE;
        case GL_CLEAR_TEXTURE:
            return !info->compressed && sampleable_target(ctx, target) ? GL_CAVEAT_SUPPORT
                                                                       : (GLint)GL_NONE;
        case GL_TEXTURE_VIEW:
            return texture_view_class(ctx, target, format) != GL_NONE ? GL_CAVEAT_SUPPORT
                                                                      : (GLint)GL_NONE;
        case GL_VIEW_COMPATIBILITY_CLASS:
            return texture_view_class(ctx, target, format);
        default:
            break;
    }
    return value;
}

void glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize,
                           GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_internalformat_info info;
    int supported;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_target(target) || !valid_pname(pname)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (bufSize == 0)
        return;
    if (params == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    info = internalformat_info(internalformat);
    supported = supported_combination(ctx, target, internalformat, &info);
    if (pname == GL_NUM_SAMPLE_COUNTS || pname == GL_SAMPLES) {
        if (!supported || !multisample_target(target) || ctx->native_dispatch == NULL ||
            ctx->native_dispatch->glesGetInternalformativ == NULL) {
            if (pname == GL_NUM_SAMPLE_COUNTS)
                params[0] = 0;
            return;
        }
        ctx->native_dispatch->glesGetInternalformativ(
            target, mgl_texture_native_storage_internalformat(ctx, internalformat), pname,
            bufSize, params);
        return;
    }
    if (pname == GL_MAX_COMBINED_DIMENSIONS) {
        uint64_t dimensions = (uint64_t)combined_dimensions(ctx, target, internalformat, supported);
        params[0] = (GLint)(GLuint)dimensions;
        if (bufSize > 1)
            params[1] = (GLint)(GLuint)(dimensions >> 32);
        return;
    }
    params[0] = query_value(ctx, target, internalformat, pname, &info, supported);
}

void glGetInternalformati64v(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize,
                             GLint64 *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLint *values;
    GLint count = 0;
    GLsizei size, i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_target(target) || !valid_pname(pname)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (bufSize == 0)
        return;
    if (params == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (pname == GL_MAX_COMBINED_DIMENSIONS) {
        struct mgl_internalformat_info info = internalformat_info(internalformat);
        params[0] =
            combined_dimensions(ctx, target, internalformat,
                                supported_combination(ctx, target, internalformat, &info));
        return;
    }
    if (pname != GL_SAMPLES) {
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetInternalformativ(target, internalformat, pname, 1, &count);
        if (ctx->errors.sequence == error_sequence_before)
            params[0] = count;
        return;
    }
    uint64_t sample_count_sequence = ctx->errors.sequence;
    glGetInternalformativ(target, internalformat, GL_NUM_SAMPLE_COUNTS, 1, &count);
    if (ctx->errors.sequence != sample_count_sequence || count <= 0)
        return;
    size = bufSize < count ? bufSize : count;
    values = calloc((size_t)size, sizeof(*values));
    if (values == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetInternalformativ(target, internalformat, GL_SAMPLES, size, values);
    if (ctx->errors.sequence == error_sequence_before)
        for (i = 0; i < size; ++i)
            params[i] = values[i];
    free(values);
}

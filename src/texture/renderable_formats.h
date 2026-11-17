#ifndef MGL_RENDERABLE_FORMATS_H
#define MGL_RENDERABLE_FORMATS_H

#include "core/context.h"

static inline int mgl_renderable_storage_format(const struct mgl_context *ctx, GLenum format) {
    switch (format) {
        case GL_R8:
        case GL_RG8:
        case GL_RGB8:
        case GL_RGBA8:
        case GL_SRGB8_ALPHA8:
        case GL_RGB565:
        case GL_RGB5_A1:
        case GL_RGBA4:
        case GL_RGB10_A2:
        case GL_RGB10_A2UI:
        case GL_R11F_G11F_B10F:
        case GL_R16F:
        case GL_RG16F:
        case GL_RGBA16F:
        case GL_R32F:
        case GL_RG32F:
        case GL_RGBA32F:
        case GL_R8I:
        case GL_R8UI:
        case GL_RG8I:
        case GL_RG8UI:
        case GL_RGBA8I:
        case GL_RGBA8UI:
        case GL_R16I:
        case GL_R16UI:
        case GL_RG16I:
        case GL_RG16UI:
        case GL_RGBA16I:
        case GL_RGBA16UI:
        case GL_R32I:
        case GL_R32UI:
        case GL_RG32I:
        case GL_RG32UI:
        case GL_RGBA32I:
        case GL_RGBA32UI:
        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT24:
        case GL_DEPTH_COMPONENT32:
        case GL_DEPTH_COMPONENT32F:
        case GL_DEPTH24_STENCIL8:
        case GL_DEPTH32F_STENCIL8:
        case GL_STENCIL_INDEX8:
        case GL_RGB5:
        case GL_RGB16F:
        case GL_RGB32F_ARB:
            return 1;
        case GL_R8_SNORM:
        case GL_RG8_SNORM:
        case GL_RGBA8_SNORM:
            return ctx != NULL && ctx->native_dispatch != NULL &&
                   mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                          "GL_EXT_render_snorm");
        case GL_R16:
        case GL_RG16:
        case GL_RGBA16:
            return ctx != NULL && ctx->native_dispatch != NULL &&
                   mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                          "GL_EXT_texture_norm16");
        case GL_R16_SNORM:
        case GL_RG16_SNORM:
        case GL_RGBA16_SNORM:
            return ctx != NULL && ctx->native_dispatch != NULL &&
                   mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                          "GL_EXT_texture_norm16") &&
                   mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                          "GL_EXT_render_snorm");
        default:
            return 0;
    }
}

#endif

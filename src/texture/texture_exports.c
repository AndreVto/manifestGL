#include "mgl/gl.h"
#include "texture/view_formats.h"
#include "texture/renderable_formats.h"

#include "core/context.h"
#include "core/render_diag.h"
#include "state/internalformat_query.h"
#include "objects/object_store.h"
#include "objects/sampler.h"
#include "list/display_list.h"
#include "texture/texture_store.h"
#include "texture/depth_readback.h"
#include "texture/pixel_layout.h"
#include "../../third_party/bcdec.h"

#include <limits.h>
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static GLuint next_texture_name(struct mgl_share_group *group) {
    GLuint name;
    do {
        name =
            (GLuint)atomic_fetch_add_explicit(&group->next_texture_name, 1u, memory_order_relaxed);
    } while (name == 0u);
    return name;
}

static int cube_face_index(GLenum target) {
    if (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
        return (int)(target - GL_TEXTURE_CUBE_MAP_POSITIVE_X);
    return -1;
}

static int valid_target(GLenum target) {
    return target == GL_TEXTURE_2D;
}

static int valid_2d_image_target(GLenum target) {
    return target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE ||
           cube_face_index(target) >= 0;
}

static int valid_3d_target(GLenum target) {
    return target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY ||
           target == GL_TEXTURE_CUBE_MAP_ARRAY;
}

static int valid_create_target(GLenum target) {
    return target == GL_TEXTURE_1D || target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE ||
           target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY ||
           target == GL_TEXTURE_BUFFER || target == GL_TEXTURE_CUBE_MAP ||
           target == GL_TEXTURE_CUBE_MAP_ARRAY ||
           target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
}

static int valid_bind_target(GLenum target) {
    return target == GL_TEXTURE_1D || target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE ||
           target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY ||
           target == GL_TEXTURE_BUFFER || target == GL_TEXTURE_CUBE_MAP ||
           target == GL_TEXTURE_CUBE_MAP_ARRAY ||
           target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
}

static int valid_level_target(GLenum target) {
    return (valid_bind_target(target) && target != GL_TEXTURE_CUBE_MAP) ||
           cube_face_index(target) >= 0;
}

static GLenum ext_dsa_texture_object_target(GLenum target) {
    return cube_face_index(target) >= 0 ? GL_TEXTURE_CUBE_MAP : target;
}

static int ext_dsa_existing_texture_target_ok(struct mgl_context *ctx, GLuint texture,
                                              GLenum target) {
    struct mgl_texture *obj;
    GLenum object_target;
    int ok = 1;
    if (ctx == NULL || texture == 0u)
        return 1;
    object_target = ext_dsa_texture_object_target(target);
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    obj = mgl_texture_find(ctx, texture);
    if (obj != NULL && obj->target != 0u && obj->target != object_target)
        ok = 0;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    if (!ok)
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    return ok;
}

static struct mgl_texture *ext_dsa_texture_resolve(struct mgl_context *ctx, GLuint texture,
                                                   GLenum target) {
    struct mgl_texture *obj;
    GLenum object_target;
    if (ctx == NULL)
        return NULL;
    object_target = ext_dsa_texture_object_target(target);
    if (texture == 0u)
        return mgl_default_texture(ctx, object_target);
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    obj = mgl_texture_find(ctx, texture);
    if (obj != NULL && obj->target != 0u && obj->target != object_target) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    obj = mgl_texture_ensure(ctx, texture, object_target, 1);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    if (obj == NULL || !obj->realized) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return NULL;
    }
    return obj;
}

static int native_texture_buffer_supported(const struct mgl_context *ctx) {
    return ctx != NULL && ctx->native_dispatch != NULL &&
           ctx->native_dispatch->glesTexBufferEXT != NULL;
}

static struct mgl_texture *dsa_texture_checked(struct mgl_context *ctx, GLuint texture);
static void dsa_native_bind_texture(struct mgl_context *ctx, const struct mgl_texture *obj);
static void dsa_native_restore_texture(struct mgl_context *ctx, GLenum target);
static GLuint *active_texture_binding_slot(struct mgl_context *ctx, GLenum target);

static size_t texture_buffer_texel_size(GLenum internalformat) {
    switch (internalformat) {
        case GL_R8:
        case GL_R8I:
        case GL_R8UI:
            return 1u;
        case GL_R16:
        case GL_R16F:
        case GL_R16I:
        case GL_R16UI:
        case GL_RG8:
        case GL_RG8I:
        case GL_RG8UI:
            return 2u;
        case GL_R32F:
        case GL_R32I:
        case GL_R32UI:
        case GL_RG16:
        case GL_RG16F:
        case GL_RG16I:
        case GL_RG16UI:
        case GL_RGBA8:
        case GL_RGBA8I:
        case GL_RGBA8UI:
            return 4u;
        case GL_RG32F:
        case GL_RG32I:
        case GL_RG32UI:
        case GL_RGBA16:
        case GL_RGBA16F:
        case GL_RGBA16I:
        case GL_RGBA16UI:
            return 8u;
        case GL_RGBA32F:
        case GL_RGBA32I:
        case GL_RGBA32UI:
            return 16u;
        default:
            return 0u;
    }
}

static struct mgl_texture_level *texture_level_for_target(struct mgl_texture *obj, GLenum target,
                                                          GLint level) {
    int face;
    if (obj == NULL || level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS)
        return NULL;
    face = cube_face_index(target);
    if (face >= 0)
        return &obj->cube_levels[face][level];
    return &obj->levels[level];
}

static GLenum texture_upload_level_internalformat(struct mgl_context *ctx, GLenum target,
                                                  GLint level) {
    struct mgl_texture *obj;
    struct mgl_texture_level *image;
    GLenum binding_target = target;
    if (ctx == NULL || level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS)
        return GL_RGBA8;
    if (cube_face_index(target) >= 0)
        binding_target = GL_TEXTURE_CUBE_MAP;
    obj = mgl_bound_texture(ctx, binding_target);
    image = texture_level_for_target(obj, target, level);
    return image != NULL && image->defined ? image->internal_format : GL_RGBA8;
}

static int valid_format_type(GLenum format, GLenum type, size_t *bytes_per_pixel) {
    {
        size_t channels = 0u, scalar_size = 0u;
        if (format == GL_RED || format == GL_RED_INTEGER)
            channels = 1u;
        else if (format == GL_RG || format == GL_RG_INTEGER)
            channels = 2u;
        else if (format == GL_RGB || format == GL_RGB_INTEGER)
            channels = 3u;
        else if (format == GL_RGBA || format == GL_RGBA_INTEGER)
            channels = 4u;
        if (type == GL_BYTE || type == GL_UNSIGNED_BYTE)
            scalar_size = 1u;
        else if (type == GL_SHORT || type == GL_UNSIGNED_SHORT || type == GL_HALF_FLOAT)
            scalar_size = 2u;
        else if (type == GL_INT || type == GL_UNSIGNED_INT || type == GL_FLOAT)
            scalar_size = 4u;
        if (channels != 0u && scalar_size != 0u &&
            ((format == GL_RED_INTEGER || format == GL_RG_INTEGER || format == GL_RGB_INTEGER ||
              format == GL_RGBA_INTEGER)
                 ? (type != GL_HALF_FLOAT && type != GL_FLOAT)
                 : 1)) {
            if (bytes_per_pixel != NULL)
                *bytes_per_pixel = channels * scalar_size;
            return 1;
        }
    }
    if ((format == GL_BGR || format == GL_BGRA) &&
        (type == GL_BYTE || type == GL_UNSIGNED_BYTE || type == GL_SHORT ||
         type == GL_UNSIGNED_SHORT || type == GL_INT || type == GL_UNSIGNED_INT ||
         type == GL_HALF_FLOAT || type == GL_FLOAT)) {
        size_t scalar = type == GL_BYTE || type == GL_UNSIGNED_BYTE ? 1u
                        : type == GL_SHORT || type == GL_UNSIGNED_SHORT || type == GL_HALF_FLOAT
                            ? 2u
                            : 4u;
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = (format == GL_BGR ? 3u : 4u) * scalar;
        return 1;
    }
    if (format == GL_RGB &&
        (type == GL_UNSIGNED_BYTE_3_3_2 || type == GL_UNSIGNED_BYTE_2_3_3_REV)) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 1u;
        return 1;
    }
    if (format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5_REV) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 2u;
        return 1;
    }
    if ((format == GL_RGBA || format == GL_BGRA) &&
        (type == GL_UNSIGNED_SHORT_4_4_4_4 || type == GL_UNSIGNED_SHORT_4_4_4_4_REV ||
         type == GL_UNSIGNED_SHORT_5_5_5_1 || type == GL_UNSIGNED_SHORT_1_5_5_5_REV)) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 2u;
        return 1;
    }
    if ((format == GL_RGBA || format == GL_BGRA) &&
        (type == GL_UNSIGNED_INT_8_8_8_8 || type == GL_UNSIGNED_INT_8_8_8_8_REV ||
         type == GL_UNSIGNED_INT_10_10_10_2 || type == GL_UNSIGNED_INT_2_10_10_10_REV)) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 4u;
        return 1;
    }
    if (format == GL_DEPTH_STENCIL && type == GL_UNSIGNED_INT_24_8) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 4u;
        return 1;
    }
    if (format == GL_STENCIL_INDEX && type == GL_UNSIGNED_BYTE) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 1u;
        return 1;
    }
    if (format == GL_RGBA &&
        (type == GL_UNSIGNED_INT_8_8_8_8 || type == GL_UNSIGNED_INT_8_8_8_8_REV)) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 4u;
        return 1;
    }
    if (format == GL_RGBA &&
        (type == GL_UNSIGNED_SHORT_4_4_4_4 || type == GL_UNSIGNED_SHORT_5_5_5_1)) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 2u;
        return 1;
    }
    if ((format == GL_RGBA && type == GL_UNSIGNED_INT_2_10_10_10_REV) ||
        (format == GL_RGBA_INTEGER && type == GL_UNSIGNED_INT_2_10_10_10_REV) ||
        (format == GL_RGB &&
         (type == GL_UNSIGNED_INT_10F_11F_11F_REV || type == GL_UNSIGNED_INT_5_9_9_9_REV))) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 4u;
        return 1;
    }
    if (format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 2u;
        return 1;
    }
    if (format == GL_BGRA && (type == GL_UNSIGNED_INT_8_8_8_8_REV || type == GL_UNSIGNED_BYTE)) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 4u;
        return 1;
    }
    if (format == GL_ALPHA || format == GL_LUMINANCE || format == GL_LUMINANCE_ALPHA) {
        if (type == GL_UNSIGNED_BYTE || type == GL_HALF_FLOAT || type == GL_FLOAT) {
            size_t channels = format == GL_LUMINANCE_ALPHA ? 2u : 1u;
            if (bytes_per_pixel != NULL)
                *bytes_per_pixel = channels * (type == GL_UNSIGNED_BYTE ? 1u
                                               : type == GL_HALF_FLOAT  ? 2u
                                                                        : 4u);
            return 1;
        }
    }
    if ((format == GL_RED || format == GL_LUMINANCE) &&
        (type == GL_UNSIGNED_BYTE || type == GL_HALF_FLOAT)) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = type == GL_HALF_FLOAT ? 2u : 1u;
        return 1;
    }
    if (format == GL_RG && (type == GL_UNSIGNED_BYTE || type == GL_HALF_FLOAT)) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = type == GL_HALF_FLOAT ? 4u : 2u;
        return 1;
    }
    if (format == GL_LUMINANCE_ALPHA && type == GL_UNSIGNED_BYTE) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 2u;
        return 1;
    }
    if (format == GL_RGB && (type == GL_FLOAT || type == GL_HALF_FLOAT)) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = type == GL_FLOAT ? 12u : 6u;
        return 1;
    }
    if ((format == GL_RGB || format == GL_BGR) &&
        (type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT)) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = type == GL_UNSIGNED_SHORT ? 6u : 3u;
        return 1;
    }
    if (format == GL_RGBA && type == GL_UNSIGNED_BYTE) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 4u;
        return 1;
    }
    if (format == GL_COLOR_INDEX && type == GL_UNSIGNED_BYTE) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 1u;
        return 1;
    }
    if (format == GL_RGBA && (type == GL_UNSIGNED_SHORT || type == GL_HALF_FLOAT)) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 8u;
        return 1;
    }
    if (format == GL_RGBA && type == GL_FLOAT) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 16u;
        return 1;
    }
    if (format == GL_DEPTH_STENCIL && type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV) {
        if (bytes_per_pixel != NULL)
            *bytes_per_pixel = 8u;
        return 1;
    }
    if (format == GL_DEPTH_COMPONENT) {
        if (type == GL_UNSIGNED_BYTE) {
            if (bytes_per_pixel != NULL)
                *bytes_per_pixel = 1u;
            return 1;
        }
        if (type == GL_UNSIGNED_SHORT) {
            if (bytes_per_pixel != NULL)
                *bytes_per_pixel = 2u;
            return 1;
        }
        if (type == GL_UNSIGNED_INT || type == GL_FLOAT) {
            if (bytes_per_pixel != NULL)
                *bytes_per_pixel = 4u;
            return 1;
        }
    }
    return 0;
}

static int invalid_packed_upload_pair(GLenum format, GLenum type) {
    switch (type) {
        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_BYTE_2_3_3_REV:
        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
        case GL_UNSIGNED_INT_5_9_9_9_REV:
            return format != GL_RGB;
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            return format != GL_RGBA && format != GL_BGRA &&
                   !(format == GL_RGBA_INTEGER && type == GL_UNSIGNED_INT_2_10_10_10_REV);
        default:
            return 0;
    }
}

static GLint mutable_2d_internalformat(GLint internalformat) {
    if (internalformat == (GLint)GL_LUMINANCE)
        return (GLint)GL_LUMINANCE8;
    if (internalformat == (GLint)GL_ALPHA)
        return (GLint)GL_ALPHA;
    if (internalformat == (GLint)GL_LUMINANCE_ALPHA)
        return (GLint)GL_LUMINANCE8_ALPHA8;
    if (internalformat == (GLint)GL_INTENSITY)
        return (GLint)GL_INTENSITY8;
    if (internalformat == (GLint)GL_COMPRESSED_RGB_S3TC_DXT1_EXT)
        return (GLint)GL_RGB8;
    if (internalformat == (GLint)GL_COMPRESSED_SRGB_S3TC_DXT1_EXT)
        return (GLint)GL_SRGB8;
    if (internalformat == (GLint)GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT ||
        internalformat == (GLint)GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT ||
        internalformat == (GLint)GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT)
        return (GLint)GL_SRGB8_ALPHA8;
    if (internalformat == (GLint)GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ||
        internalformat == (GLint)GL_COMPRESSED_RGBA_S3TC_DXT3_EXT ||
        internalformat == (GLint)GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
        return (GLint)GL_RGBA8;
    return internalformat;
}

static GLenum narrow_storage_format(GLenum internalformat) {
    switch (internalformat) {
        case GL_ALPHA16F_ARB:
        case GL_LUMINANCE16F_ARB:
            return GL_R16F;
        case GL_LUMINANCE_ALPHA16F_ARB:
            return GL_RG16F;
        case GL_ALPHA32F_ARB:
        case GL_LUMINANCE32F_ARB:
            return GL_R32F;
        case GL_LUMINANCE_ALPHA32F_ARB:
            return GL_RG32F;
        default:
            return internalformat;
    }
}

static int integer_storage_format(GLenum internalformat) {
    switch (internalformat) {
        case GL_R8I:
        case GL_R8UI:
        case GL_R16I:
        case GL_R16UI:
        case GL_R32I:
        case GL_R32UI:
        case GL_RG8I:
        case GL_RG8UI:
        case GL_RG16I:
        case GL_RG16UI:
        case GL_RG32I:
        case GL_RG32UI:
        case GL_RGB8I:
        case GL_RGB8UI:
        case GL_RGB16I:
        case GL_RGB16UI:
        case GL_RGB32I:
        case GL_RGB32UI:
        case GL_RGBA8I:
        case GL_RGBA8UI:
        case GL_RGBA16I:
        case GL_RGBA16UI:
        case GL_RGBA32I:
        case GL_RGBA32UI:
        case GL_RGB10_A2UI:
            return 1;
        default:
            return 0;
    }
}

static int integer_external_format(GLenum format) {
    return format == GL_RED_INTEGER || format == GL_RG_INTEGER || format == GL_RGB_INTEGER ||
           format == GL_RGBA_INTEGER;
}

static int valid_internal(GLint internal_format) {
    return internal_format == (GLint)GL_RGB || internal_format == (GLint)GL_RGBA ||
           internal_format == (GLint)GL_ALPHA || internal_format == (GLint)GL_ALPHA8 ||
           internal_format == (GLint)GL_RGB16F || internal_format == (GLint)GL_RGB32F_ARB ||
           internal_format == (GLint)GL_ALPHA16F_ARB ||
           internal_format == (GLint)GL_LUMINANCE16F_ARB ||
           internal_format == (GLint)GL_LUMINANCE_ALPHA16F_ARB ||
           internal_format == (GLint)GL_ALPHA32F_ARB ||
           internal_format == (GLint)GL_LUMINANCE32F_ARB ||
           internal_format == (GLint)GL_LUMINANCE_ALPHA32F_ARB || internal_format == (GLint)GL_R8 ||
           internal_format == (GLint)GL_RG8 || internal_format == (GLint)GL_R16F ||
           internal_format == (GLint)GL_RG16F || internal_format == (GLint)GL_RGB5 ||
           internal_format == (GLint)GL_LUMINANCE8 ||
           internal_format == (GLint)GL_LUMINANCE8_ALPHA8 ||
           internal_format == (GLint)GL_INTENSITY8 || internal_format == (GLint)GL_RGB8 ||
           internal_format == (GLint)GL_SRGB8 || internal_format == (GLint)GL_RGBA8 ||
           internal_format == (GLint)GL_SRGB8_ALPHA8 || internal_format == (GLint)GL_RGBA16 ||
           internal_format == (GLint)GL_RGBA16F || internal_format == (GLint)GL_RGBA32F ||
           internal_format == (GLint)GL_DEPTH_COMPONENT ||
           internal_format == (GLint)GL_DEPTH_COMPONENT16 ||
           internal_format == (GLint)GL_DEPTH_COMPONENT24 ||
           internal_format == (GLint)GL_DEPTH_COMPONENT32 ||
           internal_format == (GLint)GL_DEPTH_COMPONENT32F ||
           internal_format == (GLint)GL_DEPTH32F_STENCIL8 ||
           internal_format == (GLint)GL_DEPTH24_STENCIL8 ||
           internal_format == (GLint)GL_STENCIL_INDEX8 || internal_format == (GLint)GL_R16 ||
           internal_format == (GLint)GL_RG16 || internal_format == (GLint)GL_RGB16 ||
           internal_format == (GLint)GL_R16_SNORM || internal_format == (GLint)GL_RG16_SNORM ||
           internal_format == (GLint)GL_RGB16_SNORM || internal_format == (GLint)GL_RGBA16_SNORM ||
           internal_format == (GLint)GL_R8_SNORM || internal_format == (GLint)GL_RG8_SNORM ||
           internal_format == (GLint)GL_RGB8_SNORM || internal_format == (GLint)GL_RGBA8_SNORM ||
           internal_format == (GLint)GL_R32F || internal_format == (GLint)GL_RG32F ||
           internal_format == (GLint)GL_RGBA4 || internal_format == (GLint)GL_RGB5_A1 ||
           internal_format == (GLint)GL_RGB565 || internal_format == (GLint)GL_RGB10_A2 ||
           internal_format == (GLint)GL_R11F_G11F_B10F || internal_format == (GLint)GL_RGB9_E5 ||
           integer_storage_format((GLenum)internal_format);
}

static int format_matches_internal(GLenum format, GLint internal_format) {
    if (integer_storage_format((GLenum)internal_format))
        return integer_external_format(format);
    if (integer_external_format(format))
        return 0;
    if (internal_format == (GLint)GL_R16 || internal_format == (GLint)GL_RG16 ||
        internal_format == (GLint)GL_R16_SNORM || internal_format == (GLint)GL_RG16_SNORM ||
        internal_format == (GLint)GL_R8_SNORM || internal_format == (GLint)GL_RG8_SNORM ||
        internal_format == (GLint)GL_R32F || internal_format == (GLint)GL_RG32F)
        return format == GL_RED || format == GL_RG || format == GL_RGB || format == GL_RGBA;
    if (internal_format == (GLint)GL_RGB16 || internal_format == (GLint)GL_RGB16_SNORM ||
        internal_format == (GLint)GL_RGB8_SNORM || internal_format == (GLint)GL_R11F_G11F_B10F ||
        internal_format == (GLint)GL_RGB9_E5 || internal_format == (GLint)GL_RGB565)
        return format == GL_RGB || format == GL_RGBA;
    if (internal_format == (GLint)GL_RGBA16_SNORM || internal_format == (GLint)GL_RGBA8_SNORM ||
        internal_format == (GLint)GL_RGBA4 || internal_format == (GLint)GL_RGB5_A1 ||
        internal_format == (GLint)GL_RGB10_A2)
        return format == GL_RGBA || format == GL_BGRA;
    if (internal_format == (GLint)GL_DEPTH24_STENCIL8)
        return format == GL_DEPTH_STENCIL;
    if (internal_format == (GLint)GL_STENCIL_INDEX8)
        return format == GL_STENCIL_INDEX;
    if (internal_format == (GLint)GL_R8 || internal_format == (GLint)GL_RG8 ||
        internal_format == (GLint)GL_R16F || internal_format == (GLint)GL_RG16F)
        return format == GL_RED || format == GL_RG || format == GL_RGB || format == GL_RGBA ||
               format == GL_BGR || format == GL_BGRA;
    if (internal_format == (GLint)GL_ALPHA || internal_format == (GLint)GL_ALPHA8)
        return format == GL_ALPHA;
    if (internal_format == (GLint)GL_RGB16F || internal_format == (GLint)GL_RGB32F_ARB)
        return format == GL_RGB || format == GL_BGR || format == GL_RGBA || format == GL_BGRA;
    if (internal_format == (GLint)GL_ALPHA16F_ARB || internal_format == (GLint)GL_ALPHA32F_ARB)
        return format == GL_ALPHA;
    if (internal_format == (GLint)GL_LUMINANCE16F_ARB ||
        internal_format == (GLint)GL_LUMINANCE32F_ARB)
        return format == GL_LUMINANCE;
    if (internal_format == (GLint)GL_LUMINANCE_ALPHA16F_ARB ||
        internal_format == (GLint)GL_LUMINANCE_ALPHA32F_ARB)
        return format == GL_LUMINANCE_ALPHA;
    if (internal_format == (GLint)GL_LUMINANCE8)
        return format == GL_RED || format == GL_LUMINANCE;
    if (internal_format == (GLint)GL_LUMINANCE8_ALPHA8)
        return format == GL_LUMINANCE_ALPHA;
    if (internal_format == (GLint)GL_INTENSITY8)
        return format == GL_RED || format == GL_RG || format == GL_RGB || format == GL_RGBA ||
               format == GL_BGR || format == GL_BGRA || format == GL_LUMINANCE ||
               format == GL_LUMINANCE_ALPHA;
    if (format == GL_DEPTH_STENCIL)
        return internal_format == (GLint)GL_DEPTH32F_STENCIL8;
    if (format == GL_DEPTH_COMPONENT) {
        return internal_format == (GLint)GL_DEPTH_COMPONENT ||
               internal_format == (GLint)GL_DEPTH_COMPONENT16 ||
               internal_format == (GLint)GL_DEPTH_COMPONENT24 ||
               internal_format == (GLint)GL_DEPTH_COMPONENT32 ||
               internal_format == (GLint)GL_DEPTH_COMPONENT32F;
    }
    if (internal_format == (GLint)GL_RGBA16 || internal_format == (GLint)GL_RGBA16F)
        return format == GL_RGBA || format == GL_BGRA;
    if (internal_format == (GLint)GL_RGBA32F)
        return format == GL_RGBA || format == GL_BGRA;
    if (format == GL_COLOR_INDEX)
        return internal_format == (GLint)GL_RGBA || internal_format == (GLint)GL_RGBA8;
    if (internal_format == (GLint)GL_RGB5)
        return format == GL_RGB;
    return format == GL_RGB || format == GL_BGR || format == GL_RGBA || format == GL_BGRA
               ? (internal_format == (GLint)GL_RGB || internal_format == (GLint)GL_RGBA ||
                  internal_format == (GLint)GL_RGB8 || internal_format == (GLint)GL_SRGB8 ||
                  internal_format == (GLint)GL_RGBA8 || internal_format == (GLint)GL_SRGB8_ALPHA8)
               : 0;
}

static void depth_store(unsigned char *dst, GLenum type, double value);

/* GL_UNSIGNED_INT_8_8_8_8 stores RGBA channels most-significant first.
 * On little-endian Android the incoming bytes are A,B,G,R. The interface
 * keeps all normalized color shadows as RGBA8, independent of client type. */

/* Packed BGRA/REV arrives in B, G, R, A byte order on Android's little-endian ABI.
 * Normalize the CPU texture shadow and the GLES upload to RGBA8. */

/* Packed RGBA/REV has R in the low byte of the 32-bit word. Decode
 * via integer shifts so the CPU shadow is also portable to big-endian hosts. */

/* Normalize packed and BGR/BGRA client data once for both the CPU texture shadow and native
 * upload, including tight DSA and display-list paths. */
static void convert_texture_color_texel(const unsigned char *src, GLenum src_format,
                                        GLenum src_type, unsigned char *dst, GLenum dst_format,
                                        GLenum dst_type);

static int packed_upload_color_type(GLenum type) {
    switch (type) {
        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_BYTE_2_3_3_REV:
        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
        case GL_UNSIGNED_INT_5_9_9_9_REV:
            return 1;
        default:
            return 0;
    }
}

static int normalize_upload_color(unsigned char **data, size_t *data_size, GLenum *format,
                                  GLenum *type, GLenum internalformat) {
    GLenum dst_format = *format == GL_RGB ? GL_RGB : GL_RGBA;
    GLenum dst_type = GL_UNSIGNED_BYTE;
    size_t source_bytes, target_bytes, pixels, i;
    unsigned char *converted;
    if (*data_size == 0u || *data == NULL)
        return 1;
    if (!packed_upload_color_type(*type) ||
        (*format != GL_RGB && *format != GL_RGBA && *format != GL_BGRA))
        return 1;
    if (*format == GL_RGBA_INTEGER)
        return 1;
    if (*format == GL_RGB && *type == GL_UNSIGNED_SHORT_5_6_5 &&
        (internalformat == GL_RGB5 || internalformat == GL_RGB565))
        return 1;
    if (*format == GL_RGBA && *type == GL_UNSIGNED_SHORT_4_4_4_4 && internalformat == GL_RGBA4)
        return 1;
    if (*format == GL_RGBA && *type == GL_UNSIGNED_SHORT_5_5_5_1 && internalformat == GL_RGB5_A1)
        return 1;
    if (*type == GL_UNSIGNED_INT_2_10_10_10_REV && *format == GL_RGBA &&
        internalformat == GL_RGB10_A2)
        return 1;
    if ((*type == GL_UNSIGNED_INT_10_10_10_2 || *type == GL_UNSIGNED_INT_2_10_10_10_REV) &&
        internalformat == GL_RGB10_A2)
        dst_type = GL_UNSIGNED_INT_2_10_10_10_REV;
    if ((*type == GL_UNSIGNED_INT_10F_11F_11F_REV || *type == GL_UNSIGNED_INT_5_9_9_9_REV) &&
        (internalformat == GL_R11F_G11F_B10F || internalformat == GL_RGB9_E5 ||
         internalformat == GL_RGB16F || internalformat == GL_RGB32F_ARB))
        dst_type = GL_FLOAT;
    if (!valid_format_type(*format, *type, &source_bytes) || source_bytes == 0u ||
        *data_size % source_bytes != 0u)
        return 0;
    pixels = *data_size / source_bytes;
    target_bytes = dst_type == GL_FLOAT                         ? 12u
                   : dst_type == GL_UNSIGNED_INT_2_10_10_10_REV ? 4u
                   : dst_format == GL_RGB                       ? 3u
                                                                : 4u;
    if (pixels > SIZE_MAX / target_bytes)
        return 0;
    converted = malloc(pixels ? pixels * target_bytes : 1u);
    if (converted == NULL)
        return 0;
    for (i = 0u; i < pixels; ++i)
        convert_texture_color_texel(*data + i * source_bytes, *format, *type,
                                    converted + i * target_bytes, dst_format, dst_type);
    free(*data);
    *data = converted;
    *data_size = pixels * target_bytes;
    *format = dst_format;
    *type = dst_type;
    return 1;
}

static int needs_color_normalization(GLenum format, GLenum type) {
    return format == GL_BGR || format == GL_BGRA ||
           (packed_upload_color_type(type) && (format == GL_RGB || format == GL_RGBA));
}

static void normalize_tight_color(unsigned char *pixels, size_t size, GLenum *format,
                                  GLenum *type) {
    size_t element = *type == GL_BYTE || *type == GL_UNSIGNED_BYTE ? 1u
                     : *type == GL_SHORT || *type == GL_UNSIGNED_SHORT || *type == GL_HALF_FLOAT
                         ? 2u
                         : 4u;
    size_t components = *format == GL_BGR ? 3u : 4u;
    size_t stride = element * components, i, c;
    if (*format != GL_BGR && *format != GL_BGRA)
        return;
    for (i = 0u; i + stride <= size; i += stride) {
        for (c = 0u; c < element; ++c) {
            unsigned char value = pixels[i + c];
            pixels[i + c] = pixels[i + 2u * element + c];
            pixels[i + 2u * element + c] = value;
        }
    }
    *format = *format == GL_BGR ? GL_RGB : GL_RGBA;
}

static int color_transfer_active(const struct mgl_pixel_transfer_state *p) {
    return p->map_color || p->red_scale != 1.0f || p->red_bias != 0.0f || p->green_scale != 1.0f ||
           p->green_bias != 0.0f || p->blue_scale != 1.0f || p->blue_bias != 0.0f ||
           p->alpha_scale != 1.0f || p->alpha_bias != 0.0f;
}

static double color_transfer_component(const struct mgl_pixel_transfer_state *p, unsigned channel,
                                       double input, int indexed_source) {
    const GLfloat scales[4] = {p->red_scale, p->green_scale, p->blue_scale, p->alpha_scale};
    const GLfloat biases[4] = {p->red_bias, p->green_bias, p->blue_bias, p->alpha_bias};
    double value = input * (double)scales[channel] + (double)biases[channel];
    if (value < 0.0)
        value = 0.0;
    if (value > 1.0)
        value = 1.0;
    if (p->map_color && !indexed_source) {
        const struct mgl_pixel_map_table *map = &p->maps[6u + channel];
        if (map->size > 0) {
            unsigned index = (unsigned)nearbyint(value * (double)(map->size - 1));
            value = map->values[index];
        }
    }
    if (value < 0.0)
        value = 0.0;
    if (value > 1.0)
        value = 1.0;
    return value;
}

static int transform_texture_color(struct mgl_context *ctx, unsigned char **data, size_t *data_size,
                                   GLenum *format, GLenum *type, int indexed_source) {
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_TEX_COLOR_TRANSFORM);
    size_t pixels, channels, i, c;
    unsigned char *converted;
    if (!color_transfer_active(&ctx->pixel_transfer) || *format == GL_DEPTH_COMPONENT ||
        *format == GL_COLOR_INDEX || *data_size == 0u)
        return 1;
    if (*format == GL_RGB && *type == GL_UNSIGNED_SHORT_5_6_5) {
        pixels = *data_size / 2u;
        if (pixels > SIZE_MAX / 3u)
            return 0;
        converted = malloc(pixels * 3u);
        if (converted == NULL)
            return 0;
        for (i = 0; i < pixels; ++i) {
            uint16_t packed;
            memcpy(&packed, *data + i * 2u, 2u);
            converted[i * 3u] = (unsigned char)((((packed >> 11u) & 31u) * 255u + 15u) / 31u);
            converted[i * 3u + 1u] = (unsigned char)((((packed >> 5u) & 63u) * 255u + 31u) / 63u);
            converted[i * 3u + 2u] = (unsigned char)(((packed & 31u) * 255u + 15u) / 31u);
        }
        free(*data);
        *data = converted;
        *data_size = pixels * 3u;
        *type = GL_UNSIGNED_BYTE;
    }
    if (*type == GL_UNSIGNED_SHORT && *format == GL_RGBA) {
        pixels = *data_size / 8u;
        for (i = 0; i < pixels; ++i) {
            for (c = 0u; c < 4u; ++c) {
                uint16_t sample;
                double mapped;
                memcpy(&sample, *data + (i * 4u + c) * 2u, 2u);
                mapped = color_transfer_component(&ctx->pixel_transfer, (unsigned)c,
                                                  (double)sample / 65535.0, indexed_source);
                sample = (uint16_t)nearbyint(mapped * 65535.0);
                memcpy(*data + (i * 4u + c) * 2u, &sample, 2u);
            }
        }
        return 1;
    }
    if (*type != GL_UNSIGNED_BYTE)
        return 1;
    switch (*format) {
        case GL_RED:
        case GL_LUMINANCE:
            channels = 1u;
            break;
        case GL_LUMINANCE_ALPHA:
            channels = 2u;
            break;
        case GL_RGB:
            channels = 3u;
            break;
        case GL_RGBA:
            channels = 4u;
            break;
        default:
            return 1;
    }
    pixels = *data_size / channels;
    for (i = 0u; i < pixels; ++i) {
        for (c = 0u; c < channels; ++c) {
            unsigned channel = (*format == GL_LUMINANCE_ALPHA && c == 1u) ? 3u : (unsigned)c;
            double mapped =
                color_transfer_component(&ctx->pixel_transfer, channel,
                                         (double)(*data)[i * channels + c] / 255.0, indexed_source);
            (*data)[i * channels + c] = (unsigned char)nearbyint(mapped * 255.0);
        }
    }
    return 1;
}

/* Repack the native upload payload to match GLES sized storage while preserving the
 * client-format CPU shadow. */
static GLenum gles_color_storage_format(GLenum internalformat) {
    switch (internalformat) {
        case GL_RGB:
        case GL_RGB5:
        case GL_RGB8:
        case GL_SRGB8:
        case GL_RGB32F_ARB:
            return GL_RGB;
        case GL_RGBA:
        case GL_RGBA8:
        case GL_SRGB8_ALPHA8:
            return GL_RGBA;
        default:
            return 0u;
    }
}

static unsigned char *depth_integer_to_float(const void *data, size_t pixels, GLenum type) {
    unsigned char *out;
    size_t i;
    if (pixels > SIZE_MAX / sizeof(float) || (pixels && data == NULL))
        return NULL;
    out = malloc(pixels ? pixels * sizeof(float) : 1u);
    if (out == NULL)
        return NULL;
    for (i = 0; i < pixels; ++i) {
        float value;
        if (type == GL_UNSIGNED_BYTE)
            value = (float)((const unsigned char *)data)[i] / 255.0f;
        else if (type == GL_UNSIGNED_SHORT) {
            uint16_t raw;
            memcpy(&raw, (const unsigned char *)data + i * 2u, 2u);
            value = (float)raw / 65535.0f;
        } else {
            uint32_t raw;
            memcpy(&raw, (const unsigned char *)data + i * 4u, 4u);
            value = (float)((double)raw / 4294967295.0);
        }
        memcpy(out + i * sizeof(float), &value, sizeof(value));
    }
    return out;
}

static uint16_t float_to_half_bits(float value);

static unsigned char *repack_color_byte_float_rgba(const void *data, size_t pixels, GLenum format,
                                                   GLenum type) {
    size_t i, c, channels = format == GL_RGB ? 3u : 4u;
    size_t component = type == GL_HALF_FLOAT ? sizeof(uint16_t) : sizeof(float);
    const unsigned char *src = (const unsigned char *)data;
    unsigned char *dst;
    if (pixels > SIZE_MAX / (4u * component) || (pixels != 0u && data == NULL))
        return NULL;
    dst = malloc(pixels ? pixels * 4u * component : 1u);
    if (dst == NULL)
        return NULL;
    for (i = 0u; i < pixels; ++i) {
        for (c = 0u; c < 4u; ++c) {
            float value = c == 3u && channels == 3u ? 1.0f : (float)src[i * channels + c] / 255.0f;
            if (type == GL_HALF_FLOAT) {
                uint16_t sample = float_to_half_bits(value);
                memcpy(dst + (i * 4u + c) * component, &sample, component);
            } else {
                memcpy(dst + (i * 4u + c) * component, &value, component);
            }
        }
    }
    return dst;
}

static unsigned char *repack_rgb_float_rgba(const void *src, size_t pixels, GLenum type) {
    size_t i, component = type == GL_FLOAT ? sizeof(float) : sizeof(uint16_t);
    unsigned char *dst;
    if (pixels > SIZE_MAX / (4u * component))
        return NULL;
    dst = malloc(pixels ? pixels * 4u * component : 1u);
    if (dst == NULL)
        return NULL;
    for (i = 0u; i < pixels; ++i) {
        memcpy(dst + i * 4u * component, (const unsigned char *)src + i * 3u * component,
               3u * component);
        if (component == sizeof(float)) {
            float one = 1.0f;
            memcpy(dst + (i * 4u + 3u) * component, &one, component);
        } else {
            uint16_t one = UINT16_C(0x3c00);
            memcpy(dst + (i * 4u + 3u) * component, &one, component);
        }
    }
    return dst;
}

static unsigned char *rgb16_to_rgb8(const void *data, size_t pixels) {
    const unsigned char *src = (const unsigned char *)data;
    unsigned char *dst;
    size_t i;
    if (pixels > SIZE_MAX / 3u || (pixels && data == NULL))
        return NULL;
    dst = malloc(pixels ? pixels * 3u : 1u);
    if (dst == NULL)
        return NULL;
    for (i = 0u; i < pixels * 3u; ++i) {
        uint16_t sample;
        memcpy(&sample, src + i * 2u, sizeof(sample));
        dst[i] = (unsigned char)(((uint32_t)sample + 128u) / 257u);
    }
    return dst;
}

static unsigned char *repack_rgb_rgba_for_gles(const unsigned char *src, size_t pixels, GLenum from,
                                               GLenum to) {
    size_t i, from_bpp, to_bpp;
    unsigned char *dst;
    if (from == to || (from != GL_RGB && from != GL_RGBA) || (to != GL_RGB && to != GL_RGBA))
        return NULL;
    from_bpp = from == GL_RGB ? 3u : 4u;
    to_bpp = to == GL_RGB ? 3u : 4u;
    if (pixels > SIZE_MAX / to_bpp || (pixels != 0u && src == NULL))
        return NULL;
    dst = malloc(pixels != 0u ? pixels * to_bpp : 1u);
    if (dst == NULL)
        return NULL;
    for (i = 0u; i < pixels; ++i) {
        memcpy(dst + i * to_bpp, src + i * from_bpp, 3u);
        if (to_bpp == 4u)
            dst[i * 4u + 3u] = 255u;
    }
    return dst;
}

/* The GLES norm16 fallback uses converted GPU data, retains 16-bit CPU data, and reports realized
 * channel precision. */
static int rgba16_gpu_native(const struct mgl_context *ctx) {
    return ctx == NULL || ctx->native_dispatch == NULL ||
           mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_EXT_texture_norm16");
}

static unsigned char *depth8_to_depth16(const void *data, size_t bytes) {
    const unsigned char *src = (const unsigned char *)data;
    unsigned char *dst;
    size_t i;
    if (bytes > SIZE_MAX / sizeof(uint16_t))
        return NULL;
    dst = malloc(bytes != 0u ? bytes * sizeof(uint16_t) : 1u);
    if (dst == NULL)
        return NULL;
    for (i = 0u; i < bytes; ++i) {
        uint16_t value = (uint16_t)((uint16_t)src[i] * 257u);
        memcpy(dst + i * sizeof(value), &value, sizeof(value));
    }
    return dst;
}

static unsigned char *rgba8_to_half_float(const void *data, size_t bytes) {
    const unsigned char *src = (const unsigned char *)data;
    unsigned char *dst;
    size_t i;
    if (bytes > SIZE_MAX / sizeof(uint16_t) || (bytes != 0u && src == NULL))
        return NULL;
    dst = malloc(bytes != 0u ? bytes * sizeof(uint16_t) : 1u);
    if (dst == NULL)
        return NULL;
    for (i = 0u; i < bytes; ++i) {
        uint32_t bits, mantissa, remainder, exponent;
        uint16_t result;
        float normalized = (float)src[i] / 255.0f;
        memcpy(&bits, &normalized, sizeof(bits));
        exponent = (bits >> 23u) & 255u;
        mantissa = (bits & 0x7fffffu) >> 13u;
        remainder = bits & 0x1fffu;
        if (remainder > 0x1000u || (remainder == 0x1000u && (mantissa & 1u)))
            ++mantissa;
        if (mantissa >= 0x400u) {
            mantissa = 0u;
            ++exponent;
        }
        result = exponent == 0u ? 0u : (uint16_t)(((exponent - 112u) << 10u) | mantissa);
        memcpy(dst + i * sizeof(result), &result, sizeof(result));
    }
    return dst;
}

static uint16_t float_to_half_bits(float value) {
    uint32_t bits, mantissa, half_mantissa, remainder;
    unsigned sign, raw_exponent;
    int exponent;
    memcpy(&bits, &value, sizeof(bits));
    sign = (bits >> 16u) & 0x8000u;
    raw_exponent = (bits >> 23u) & 255u;
    mantissa = bits & 0x7fffffu;
    if (raw_exponent == 255u)
        return (uint16_t)(sign | 0x7c00u | (mantissa ? 0x200u : 0u));
    exponent = (int)raw_exponent - 112;
    if (exponent >= 31)
        return (uint16_t)(sign | 0x7c00u);
    if (exponent <= 0) {
        unsigned shift;
        if (exponent < -10)
            return (uint16_t)sign;
        mantissa |= 0x800000u;
        shift = (unsigned)(14 - exponent);
        half_mantissa = mantissa >> shift;
        remainder = mantissa & ((1u << shift) - 1u);
        if (remainder > (1u << (shift - 1u)) ||
            (remainder == (1u << (shift - 1u)) && (half_mantissa & 1u)))
            ++half_mantissa;
        return (uint16_t)(sign | half_mantissa);
    }
    half_mantissa = mantissa >> 13u;
    remainder = mantissa & 0x1fffu;
    if (remainder > 0x1000u || (remainder == 0x1000u && (half_mantissa & 1u)))
        ++half_mantissa;
    if (half_mantissa == 0x400u) {
        half_mantissa = 0u;
        ++exponent;
    }
    if (exponent >= 31)
        return (uint16_t)(sign | 0x7c00u);
    return (uint16_t)(sign | ((unsigned)exponent << 10u) | half_mantissa);
}

static double half_bits_to_double(uint16_t half) {
    unsigned exponent = (half >> 10u) & 31u;
    unsigned mantissa = half & 1023u;
    double value = exponent == 0u    ? ldexp((double)mantissa, -24)
                   : exponent == 31u ? (mantissa ? 0.0 : INFINITY)
                                     : ldexp((double)(1024u + mantissa), (int)exponent - 25);
    return half & 0x8000u ? -value : value;
}

static unsigned char *repack_narrow_color_upload(const void *data, size_t pixels,
                                                 GLenum source_format, GLenum source_type,
                                                 GLenum internal_format, GLenum *upload_format,
                                                 GLenum *upload_type) {
    const unsigned char *src = (const unsigned char *)data;
    size_t in_channels, out_channels, in_bytes, out_bytes, i, channel;
    int use_half = internal_format == GL_R16F || internal_format == GL_RG16F;
    int use_float = internal_format == GL_R32F || internal_format == GL_RG32F;
    unsigned char *dst;
    GLenum result_format;
    if (internal_format != GL_R8 && internal_format != GL_RG8 && internal_format != GL_R16F &&
        internal_format != GL_RG16F && internal_format != GL_R32F && internal_format != GL_RG32F)
        return NULL;
    result_format =
        internal_format == GL_R8 || internal_format == GL_R16F || internal_format == GL_R32F
            ? GL_RED
            : GL_RG;
    in_channels = source_format == GL_RED    ? 1u
                  : source_format == GL_RG   ? 2u
                  : source_format == GL_RGB  ? 3u
                  : source_format == GL_RGBA ? 4u
                                             : 0u;
    out_channels = result_format == GL_RED ? 1u : 2u;
    in_bytes = source_type == GL_UNSIGNED_BYTE                                    ? 1u
               : source_type == GL_HALF_FLOAT || source_type == GL_UNSIGNED_SHORT ? 2u
               : source_type == GL_FLOAT                                          ? 4u
                                                                                  : 0u;
    out_bytes = use_half ? 2u : use_float ? 4u : 1u;
    if (!in_channels || !in_bytes || (pixels && src == NULL) ||
        pixels > SIZE_MAX / (out_channels * out_bytes))
        return NULL;
    dst = malloc(pixels ? pixels * out_channels * out_bytes : 1u);
    if (dst == NULL)
        return NULL;
    for (i = 0; i < pixels; ++i) {
        for (channel = 0; channel < out_channels; ++channel) {
            double value = 0.0;
            if (channel < in_channels) {
                const unsigned char *sample = src + (i * in_channels + channel) * in_bytes;
                if (source_type == GL_UNSIGNED_BYTE)
                    value = (double)*sample / 255.0;
                else if (source_type == GL_UNSIGNED_SHORT) {
                    uint16_t v;
                    memcpy(&v, sample, sizeof(v));
                    value = (double)v / 65535.0;
                } else if (source_type == GL_HALF_FLOAT) {
                    uint16_t v;
                    memcpy(&v, sample, sizeof(v));
                    value = half_bits_to_double(v);
                } else {
                    float v;
                    memcpy(&v, sample, sizeof(v));
                    value = (double)v;
                }
            }
            if (use_float) {
                float single = (float)value;
                memcpy(dst + (i * out_channels + channel) * 4u, &single, 4u);
            } else if (use_half) {
                uint16_t half = float_to_half_bits((float)value);
                memcpy(dst + (i * out_channels + channel) * 2u, &half, 2u);
            } else {
                if (!(value >= 0.0))
                    value = 0.0;
                if (value > 1.0)
                    value = 1.0;
                dst[i * out_channels + channel] = (unsigned char)nearbyint(value * 255.0);
            }
        }
    }
    *upload_format = result_format;
    *upload_type = use_float ? GL_FLOAT : use_half ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE;
    return dst;
}

static unsigned char *color_index_to_rgba8(const struct mgl_context *ctx, const unsigned char *data,
                                           size_t bytes) {
    unsigned char *dst;
    size_t i, c;
    if (ctx == NULL || bytes > SIZE_MAX / 4u || (bytes != 0u && data == NULL))
        return NULL;
    dst = malloc(bytes != 0u ? bytes * 4u : 1u);
    if (dst == NULL)
        return NULL;
    for (i = 0u; i < bytes; ++i) {
        int64_t value = data[i];
        unsigned index;
        if (ctx->pixel_transfer.index_shift > 0 && ctx->pixel_transfer.index_shift < 32)
            value <<= (unsigned)ctx->pixel_transfer.index_shift;
        else if (ctx->pixel_transfer.index_shift < 0 && ctx->pixel_transfer.index_shift > -32)
            value >>= (unsigned)-ctx->pixel_transfer.index_shift;
        value += ctx->pixel_transfer.index_offset;
        if (value < 0)
            value = 0;
        index = (unsigned)value;
        /* Color-index to RGBA conversion applies I_TO_R/G/B/A maps. I_TO_I is used for
         * index-to-index transfers. */
        for (c = 0u; c < 4u; ++c) {
            const struct mgl_pixel_map_table *map = &ctx->pixel_transfer.maps[2u + c];
            double mapped = map->size > 0 ? map->values[index & (unsigned)(map->size - 1)] : 0.0;
            if (mapped < 0.0)
                mapped = 0.0;
            if (mapped > 1.0)
                mapped = 1.0;
            dst[i * 4u + c] = (unsigned char)(mapped * 255.0 + 0.5);
        }
    }
    return dst;
}

static unsigned char *rgba16_to_rgba8(const void *data, size_t bytes) {
    const unsigned char *src = (const unsigned char *)data;
    unsigned char *dst;
    size_t i;
    if (bytes % 2u != 0u || (bytes != 0u && src == NULL))
        return NULL;
    dst = malloc(bytes / 2u ? bytes / 2u : 1u);
    if (dst == NULL)
        return NULL;
    for (i = 0u; i < bytes / 2u; ++i) {
        uint16_t sample;
        memcpy(&sample, src + i * 2u, sizeof(sample));
        dst[i] = (unsigned char)(((uint32_t)sample + 128u) / 257u);
    }
    return dst;
}

static int checked_mul(size_t a, size_t b, size_t *out) {
    if (a != 0u && b > SIZE_MAX / a)
        return 0;
    *out = a * b;
    return 1;
}

static size_t pixel_element_size(GLenum type) {
    switch (type) {
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:
        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_BYTE_2_3_3_REV:
            return 1u;
        case GL_UNSIGNED_SHORT:
        case GL_SHORT:
        case GL_HALF_FLOAT:
        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            return 2u;
        case GL_UNSIGNED_INT:
        case GL_INT:
        case GL_FLOAT:
        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
        case GL_UNSIGNED_INT_5_9_9_9_REV:
            return 4u;
        case GL_UNSIGNED_INT_24_8:
            return 4u;
        case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
            return 4u;
        default:
            return 0u;
    }
}

static void swap_elements(unsigned char *data, size_t bytes, size_t element_size) {
    size_t offset;
    if (data == NULL || element_size <= 1u)
        return;
    for (offset = 0u; offset + element_size <= bytes; offset += element_size) {
        size_t i;
        for (i = 0u; i < element_size / 2u; ++i) {
            unsigned char tmp = data[offset + i];
            data[offset + i] = data[offset + element_size - 1u - i];
            data[offset + element_size - 1u - i] = tmp;
        }
    }
}

static const unsigned char *pixel_source(struct mgl_context *ctx, const void *pixels,
                                         size_t bytes) {
    if (ctx->pixel_unpack_buffer != 0u) {
        struct mgl_buffer *buffer = mgl_buffer_find(ctx, ctx->pixel_unpack_buffer);
        size_t offset = (size_t)(uintptr_t)pixels;
        if (buffer == NULL || buffer->mapped || offset > buffer->size ||
            bytes > buffer->size - offset)
            return NULL;
        return mgl_buffer_shadow_ensure(ctx, buffer) ? buffer->shadow + offset : NULL;
    }
    return (const unsigned char *)pixels;
}

static const unsigned char *tight_borrow_2d(struct mgl_context *ctx, const void *pixels,
                                            GLsizei width, GLsizei height, size_t bytes_per_pixel,
                                            size_t element_size, size_t *out_size) {
    struct mgl_pixel_layout layout;
    size_t tight;
    const unsigned char *src;
    if ((pixels == NULL && ctx->pixel_unpack_buffer == 0u) ||
        (ctx->unpack_swap_bytes && element_size > 1u) ||
        !mgl_pixel_layout_2d(width, height, bytes_per_pixel, ctx->unpack_alignment,
                             ctx->unpack_row_length, ctx->unpack_skip_pixels, ctx->unpack_skip_rows,
                             &layout) ||
        !checked_mul(layout.row_bytes, (size_t)height, &tight) || tight == 0u ||
        (height > 1 && layout.row_stride != layout.row_bytes))
        return NULL;
    src = pixel_source(ctx, pixels, layout.span_bytes);
    if (src == NULL)
        return NULL;
    *out_size = tight;
    return src + layout.start_offset;
}

static const unsigned char *tight_borrow_3d(struct mgl_context *ctx, const void *pixels,
                                            GLsizei width, GLsizei height, GLsizei depth,
                                            size_t bytes_per_pixel, size_t element_size,
                                            size_t *out_size) {
    struct mgl_pixel_layout layout;
    size_t slice_bytes, tight;
    const unsigned char *src;
    if ((pixels == NULL && ctx->pixel_unpack_buffer == 0u) ||
        (ctx->unpack_swap_bytes && element_size > 1u) ||
        !mgl_pixel_layout_3d(width, height, depth, bytes_per_pixel, ctx->unpack_alignment,
                             ctx->unpack_row_length, ctx->unpack_image_height,
                             ctx->unpack_skip_pixels, ctx->unpack_skip_rows,
                             ctx->unpack_skip_images, &layout) ||
        !checked_mul(layout.row_bytes, (size_t)height, &slice_bytes) ||
        !checked_mul(slice_bytes, (size_t)depth, &tight) || tight == 0u ||
        (height > 1 && layout.row_stride != layout.row_bytes) ||
        (depth > 1 && layout.image_stride != slice_bytes))
        return NULL;
    src = pixel_source(ctx, pixels, layout.span_bytes);
    if (src == NULL)
        return NULL;
    *out_size = tight;
    return src + layout.start_offset;
}

static unsigned char *tight_copy(struct mgl_context *ctx, const void *pixels, GLsizei width,
                                 GLsizei height, size_t bytes_per_pixel, size_t element_size,
                                 size_t *out_size) {
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_TEX_UNPACK);
    struct mgl_pixel_layout layout;
    size_t tight;
    const unsigned char *src;
    unsigned char *dst;
    GLsizei y;
    if (!mgl_pixel_layout_2d(width, height, bytes_per_pixel, ctx->unpack_alignment,
                             ctx->unpack_row_length, ctx->unpack_skip_pixels, ctx->unpack_skip_rows,
                             &layout) ||
        !checked_mul(layout.row_bytes, (size_t)height, &tight))
        return NULL;
    *out_size = tight;
    if (tight == 0u)
        return calloc(1u, 1u);
    if (pixels == NULL && ctx->pixel_unpack_buffer == 0u)
        return calloc(1u, tight);
    src = pixel_source(ctx, pixels, layout.span_bytes);
    if (src == NULL)
        return NULL;
    src += layout.start_offset;
    dst = malloc(tight);
    if (dst == NULL)
        return NULL;
    for (y = 0; y < height; ++y)
        memcpy(dst + (size_t)y * layout.row_bytes, src + (size_t)y * layout.row_stride,
               layout.row_bytes);
    if (ctx->unpack_swap_bytes)
        swap_elements(dst, tight, element_size);
    return dst;
}

static unsigned char *tight_copy_3d(struct mgl_context *ctx, const void *pixels, GLsizei width,
                                    GLsizei height, GLsizei depth, size_t bytes_per_pixel,
                                    size_t element_size, size_t *out_size) {
    struct mgl_pixel_layout layout;
    size_t tight_slice, tight;
    const unsigned char *src;
    unsigned char *dst;
    GLsizei z, y;
    if (!mgl_pixel_layout_3d(width, height, depth, bytes_per_pixel, ctx->unpack_alignment,
                             ctx->unpack_row_length, ctx->unpack_image_height,
                             ctx->unpack_skip_pixels, ctx->unpack_skip_rows,
                             ctx->unpack_skip_images, &layout) ||
        !checked_mul(layout.row_bytes, (size_t)height, &tight_slice) ||
        !checked_mul(tight_slice, (size_t)depth, &tight))
        return NULL;
    *out_size = tight;
    if (tight == 0u)
        return calloc(1u, 1u);
    if (pixels == NULL && ctx->pixel_unpack_buffer == 0u)
        return calloc(1u, tight);
    src = pixel_source(ctx, pixels, layout.span_bytes);
    if (src == NULL)
        return NULL;
    src += layout.start_offset;
    dst = malloc(tight);
    if (dst == NULL)
        return NULL;
    for (z = 0; z < depth; ++z) {
        for (y = 0; y < height; ++y) {
            memcpy(dst + (size_t)z * tight_slice + (size_t)y * layout.row_bytes,
                   src + (size_t)z * layout.image_stride + (size_t)y * layout.row_stride,
                   layout.row_bytes);
        }
    }
    if (ctx->unpack_swap_bytes)
        swap_elements(dst, tight, element_size);
    return dst;
}

static const unsigned char *tight_source_2d(struct mgl_context *ctx, const void *pixels,
                                            GLsizei width, GLsizei height, size_t bytes_per_pixel,
                                            size_t element_size, size_t *size,
                                            unsigned char **owned) {
    const unsigned char *direct;
    *owned = NULL;
    direct = tight_borrow_2d(ctx, pixels, width, height, bytes_per_pixel, element_size, size);
    if (direct != NULL)
        return direct;
    *owned = tight_copy(ctx, pixels, width, height, bytes_per_pixel, element_size, size);
    return *owned;
}

static const unsigned char *tight_source_3d(struct mgl_context *ctx, const void *pixels,
                                            GLsizei width, GLsizei height, GLsizei depth,
                                            size_t bytes_per_pixel, size_t element_size,
                                            size_t *size, unsigned char **owned) {
    const unsigned char *direct;
    *owned = NULL;
    direct =
        tight_borrow_3d(ctx, pixels, width, height, depth, bytes_per_pixel, element_size, size);
    if (direct != NULL)
        return direct;
    *owned = tight_copy_3d(ctx, pixels, width, height, depth, bytes_per_pixel, element_size, size);
    return *owned;
}

static GLuint native_unpack_buffer(struct mgl_context *ctx) {
    struct mgl_buffer *buffer = mgl_buffer_find(ctx, ctx->pixel_unpack_buffer);
    return buffer != NULL ? buffer->native_name : 0u;
}

static GLuint native_pack_buffer(struct mgl_context *ctx) {
    struct mgl_buffer *buffer = mgl_buffer_find(ctx, ctx->pixel_pack_buffer);
    return buffer != NULL ? buffer->native_name : 0u;
}

static void native_upload_begin_source(struct mgl_context *ctx, int use_pbo) {
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_TEX_UPLOAD_BEGIN);
    if (ctx->native_dispatch->glesBindBuffer != NULL)
        mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_UNPACK_BUFFER,
                                        use_pbo ? native_unpack_buffer(ctx) : 0u);
    if (ctx->native_dispatch->glesPixelStorei != NULL)
        ctx->native_dispatch->glesPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

static void native_upload_begin(struct mgl_context *ctx) {
    native_upload_begin_source(ctx, 0);
}

static int native_upload_can_use_pbo(struct mgl_context *ctx, int pbo_source, GLenum source_format,
                                     GLenum source_type, GLenum gpu_format, GLenum gpu_type,
                                     const void *original_data, const void *gpu_data) {
    return pbo_source && ctx->native_dispatch->glesBindBuffer != NULL &&
           ctx->native_dispatch->glesBufferData != NULL && ctx->pixel_unpack_buffer != 0u &&
           gpu_format == source_format && gpu_type == source_type && original_data != NULL &&
           original_data == gpu_data && native_unpack_buffer(ctx) != 0u;
}

static int texture_pbo_source_offset(struct mgl_context *ctx, const unsigned char *source,
                                     size_t element_size, size_t *offset) {
    struct mgl_buffer *buffer;
    uintptr_t base, address;
    if (ctx->pixel_unpack_buffer == 0u || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesBindBuffer == NULL ||
        ctx->native_dispatch->glesBufferData == NULL || element_size == 0u)
        return 0;
    buffer = mgl_buffer_find(ctx, ctx->pixel_unpack_buffer);
    if (buffer == NULL || buffer->native_name == 0u || buffer->shadow == NULL)
        return 0;
    base = (uintptr_t)buffer->shadow;
    address = (uintptr_t)source;
    if (address < base || address - base > buffer->size)
        return 0;
    *offset = (size_t)(address - base);
    return *offset % element_size == 0u;
}

static void native_upload_end(struct mgl_context *ctx) {
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_TEX_UPLOAD_END);
    if (ctx->native_dispatch->glesPixelStorei != NULL)
        ctx->native_dispatch->glesPixelStorei(GL_UNPACK_ALIGNMENT, ctx->unpack_alignment);
    if (ctx->native_dispatch->glesBindBuffer != NULL)
        mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_UNPACK_BUFFER, native_unpack_buffer(ctx));
    if (ctx->native_dispatch->glesGetError != NULL) {
        GLenum error = ctx->native_dispatch->glesGetError();
        if (error != GL_NO_ERROR) {
            mgl_error_set(&ctx->errors, error);
        }
    }
}

static int immutable_storage_format(GLenum internalformat, GLenum *format, GLenum *type,
                                    size_t *bytes_per_pixel) {
    static const struct {
        GLenum internalformat, format, type;
        size_t bytes_per_pixel;
    } formats[] = {{GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1u},
                   {GL_R8_SNORM, GL_RED, GL_BYTE, 1u},
                   {GL_R16, GL_RED, GL_UNSIGNED_SHORT, 2u},
                   {GL_R16_SNORM, GL_RED, GL_SHORT, 2u},
                   {GL_R16F, GL_RED, GL_HALF_FLOAT, 2u},
                   {GL_R32F, GL_RED, GL_FLOAT, 4u},
                   {GL_R8I, GL_RED_INTEGER, GL_BYTE, 1u},
                   {GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, 1u},
                   {GL_R16I, GL_RED_INTEGER, GL_SHORT, 2u},
                   {GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, 2u},
                   {GL_R32I, GL_RED_INTEGER, GL_INT, 4u},
                   {GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, 4u},
                   {GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2u},
                   {GL_RG8_SNORM, GL_RG, GL_BYTE, 2u},
                   {GL_RG16, GL_RG, GL_UNSIGNED_SHORT, 4u},
                   {GL_RG16_SNORM, GL_RG, GL_SHORT, 4u},
                   {GL_RG16F, GL_RG, GL_HALF_FLOAT, 4u},
                   {GL_RG32F, GL_RG, GL_FLOAT, 8u},
                   {GL_RG8I, GL_RG_INTEGER, GL_BYTE, 2u},
                   {GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE, 2u},
                   {GL_RG16I, GL_RG_INTEGER, GL_SHORT, 4u},
                   {GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT, 4u},
                   {GL_RG32I, GL_RG_INTEGER, GL_INT, 8u},
                   {GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, 8u},
                   {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2u},
                   {GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2u},
                   {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, 3u},
                   {GL_RGB8_SNORM, GL_RGB, GL_BYTE, 3u},
                   {GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE, 3u},
                   {GL_RGB16, GL_RGB, GL_UNSIGNED_SHORT, 6u},
                   {GL_RGB16_SNORM, GL_RGB, GL_SHORT, 6u},
                   {GL_RGB16F, GL_RGB, GL_HALF_FLOAT, 6u},
                   {GL_RGB32F_ARB, GL_RGB, GL_FLOAT, 12u},
                   {GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, 4u},
                   {GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, 4u},
                   {GL_RGB8I, GL_RGB_INTEGER, GL_BYTE, 3u},
                   {GL_RGB8UI, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, 3u},
                   {GL_RGB16I, GL_RGB_INTEGER, GL_SHORT, 6u},
                   {GL_RGB16UI, GL_RGB_INTEGER, GL_UNSIGNED_SHORT, 6u},
                   {GL_RGB32I, GL_RGB_INTEGER, GL_INT, 12u},
                   {GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT, 12u},
                   {GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 2u},
                   {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 2u},
                   {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 4u},
                   {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4u},
                   {GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, 4u},
                   {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, 4u},
                   {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, 8u},
                   {GL_RGBA16_SNORM, GL_RGBA, GL_SHORT, 8u},
                   {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8u},
                   {GL_RGBA32F, GL_RGBA, GL_FLOAT, 16u},
                   {GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, 4u},
                   {GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, 4u},
                   {GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT, 8u},
                   {GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, 8u},
                   {GL_RGBA32I, GL_RGBA_INTEGER, GL_INT, 16u},
                   {GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, 16u},
                   {GL_RGB10_A2UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, 4u},
                   {GL_LUMINANCE8, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1u},
                   {GL_LUMINANCE8_ALPHA8, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 2u},
                   {GL_ALPHA16F_ARB, GL_ALPHA, GL_HALF_FLOAT, 2u},
                   {GL_LUMINANCE16F_ARB, GL_LUMINANCE, GL_HALF_FLOAT, 2u},
                   {GL_LUMINANCE_ALPHA16F_ARB, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT, 4u},
                   {GL_ALPHA32F_ARB, GL_ALPHA, GL_FLOAT, 4u},
                   {GL_LUMINANCE32F_ARB, GL_LUMINANCE, GL_FLOAT, 4u},
                   {GL_LUMINANCE_ALPHA32F_ARB, GL_LUMINANCE_ALPHA, GL_FLOAT, 8u},
                   {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 2u},
                   {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 4u},
                   {GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, 4u},
                   {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, 4u},
                   {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 4u},
                   {GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 8u},
                   {GL_STENCIL_INDEX8, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, 1u}};

    size_t i;
    if (format == NULL || type == NULL || bytes_per_pixel == NULL)
        return 0;
    for (i = 0u; i < sizeof(formats) / sizeof(formats[0]); ++i) {
        if (formats[i].internalformat == internalformat) {
            *format = formats[i].format;
            *type = formats[i].type;
            *bytes_per_pixel = formats[i].bytes_per_pixel;
            return 1;
        }
    }
    return 0;
}

/* Select GLES storage while retaining the desktop format in metadata.
 * RGB16F uses RGBA16F for the color-attachment path; luminance formats
 * use R/RG storage with sampling swizzles. */
static GLenum native_storage_internalformat(const struct mgl_context *ctx, GLenum internalformat) {
    if (internalformat == GL_LUMINANCE8 || internalformat == GL_INTENSITY8 ||
        internalformat == GL_ALPHA || internalformat == GL_ALPHA8)
        return GL_R8;
    if (internalformat == GL_RGB16F)
        return GL_RGBA16F;
    if (internalformat == GL_DEPTH_COMPONENT32)
        return GL_DEPTH_COMPONENT32F;
    if (internalformat == GL_RGB32F_ARB)
        internalformat = GL_RGBA32F;
    internalformat = narrow_storage_format(internalformat);
    if ((internalformat == GL_R32F || internalformat == GL_RG32F || internalformat == GL_RGBA32F) &&
        ctx != NULL && ctx->native_dispatch != NULL &&
        !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                "GL_OES_texture_float_linear"))
        return internalformat == GL_R32F    ? GL_R16F
               : internalformat == GL_RG32F ? GL_RG16F
                                            : GL_RGBA16F;
    if (internalformat == GL_LUMINANCE8_ALPHA8)
        return GL_RG8;
    if (internalformat == GL_RGB5)
        return GL_RGB565;
    return internalformat;
}

GLenum mgl_texture_native_storage_internalformat(const struct mgl_context *ctx,
                                                 GLenum internalformat) {
    return native_storage_internalformat(ctx, internalformat);
}

static GLenum native_storage_external_format(GLenum format) {
    if (format == GL_LUMINANCE || format == GL_ALPHA)
        return GL_RED;
    return format == GL_LUMINANCE_ALPHA ? GL_RG : format;
}

static void native_apply_legacy_luminance_swizzle(struct mgl_context *ctx, GLenum target,
                                                  GLenum internalformat, GLenum previous_format) {
    int luminance =
        internalformat == GL_LUMINANCE8 || internalformat == GL_LUMINANCE8_ALPHA8 ||
        internalformat == GL_LUMINANCE16F_ARB || internalformat == GL_LUMINANCE32F_ARB ||
        internalformat == GL_LUMINANCE_ALPHA16F_ARB || internalformat == GL_LUMINANCE_ALPHA32F_ARB;
    int alpha_only = internalformat == GL_ALPHA || internalformat == GL_ALPHA8 ||
                     internalformat == GL_ALPHA16F_ARB || internalformat == GL_ALPHA32F_ARB;
    int intensity = internalformat == GL_INTENSITY8;
    int was_legacy =
        previous_format == GL_LUMINANCE8 || previous_format == GL_LUMINANCE8_ALPHA8 ||
        previous_format == GL_LUMINANCE16F_ARB || previous_format == GL_LUMINANCE32F_ARB ||
        previous_format == GL_LUMINANCE_ALPHA16F_ARB ||
        previous_format == GL_LUMINANCE_ALPHA32F_ARB || previous_format == GL_INTENSITY8 ||
        previous_format == GL_ALPHA || previous_format == GL_ALPHA8 ||
        previous_format == GL_ALPHA16F_ARB || previous_format == GL_ALPHA32F_ARB;
    if ((!luminance && !intensity && !alpha_only && !was_legacy) || ctx == NULL ||
        ctx->native_dispatch == NULL || ctx->native_dispatch->glesTexParameteri == NULL)
        return;
    ctx->native_dispatch->glesTexParameteri(target, GL_TEXTURE_SWIZZLE_R,
                                            (GLint)(alpha_only ? GL_ZERO : GL_RED));
    ctx->native_dispatch->glesTexParameteri(target, GL_TEXTURE_SWIZZLE_G,
                                            (GLint)(alpha_only               ? GL_ZERO
                                                    : luminance || intensity ? GL_RED
                                                                             : GL_GREEN));
    ctx->native_dispatch->glesTexParameteri(target, GL_TEXTURE_SWIZZLE_B,
                                            (GLint)(alpha_only               ? GL_ZERO
                                                    : luminance || intensity ? GL_RED
                                                                             : GL_BLUE));
    ctx->native_dispatch->glesTexParameteri(
        target, GL_TEXTURE_SWIZZLE_A,
        (GLint)(alpha_only || intensity ? GL_RED
                : !luminance            ? GL_ALPHA
                : (internalformat == GL_LUMINANCE8 || internalformat == GL_LUMINANCE16F_ARB ||
                   internalformat == GL_LUMINANCE32F_ARB)
                    ? GL_ONE
                    : GL_GREEN));
}

static GLint immutable_effective_base_level(const struct mgl_texture *obj) {
    GLint last;
    if (obj == NULL || !obj->immutable_format || obj->immutable_levels <= 0)
        return obj != NULL ? obj->base_level : 0;
    last = obj->immutable_levels - 1;
    if (obj->base_level < 0)
        return 0;
    if (obj->base_level > last)
        return last;
    return obj->base_level;
}

static GLint immutable_effective_max_level(const struct mgl_texture *obj) {
    GLint base, last, level;
    if (obj == NULL || !obj->immutable_format || obj->immutable_levels <= 0)
        return obj != NULL ? obj->max_level : 0;
    base = immutable_effective_base_level(obj);
    last = obj->immutable_levels - 1;
    level = obj->max_level;
    if (level < base)
        level = base;
    if (level > last)
        level = last;
    return level;
}

static void native_realize_immutable_level_range(struct mgl_context *ctx,
                                                 const struct mgl_texture *obj, GLenum target) {
    if (ctx == NULL || obj == NULL || !obj->immutable_format || obj->native_immutable_storage ||
        ctx->native_dispatch == NULL || ctx->native_dispatch->glesTexParameteri == NULL)
        return;
    ctx->native_dispatch->glesTexParameteri(target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target,
                                            GL_TEXTURE_BASE_LEVEL,
                                            immutable_effective_base_level(obj));
    ctx->native_dispatch->glesTexParameteri(target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target,
                                            GL_TEXTURE_MAX_LEVEL,
                                            immutable_effective_max_level(obj));
}

static void clear_texture_image_storage(struct mgl_texture *obj) {
    unsigned level, face;
    if (obj == NULL || obj->view_root != NULL)
        return;
    for (level = 0u; level < MGL_MAX_TEXTURE_LEVELS; ++level) {
        free(obj->levels[level].data);
        memset(&obj->levels[level], 0, sizeof(obj->levels[level]));
        for (face = 0u; face < 6u; ++face) {
            free(obj->cube_levels[face][level].data);
            memset(&obj->cube_levels[face][level], 0, sizeof(obj->cube_levels[face][level]));
        }
    }
}

static int read_rendered_color_texture(struct mgl_context *ctx, const struct mgl_texture *obj,
                                       GLint level, GLsizei width, GLsizei height,
                                       unsigned char **out_pixels);
static int materialize_stale_color_shadow(struct mgl_context *ctx, struct mgl_texture *obj,
                                          GLint level, struct mgl_texture_level *dst);

static void free_storage_level_array(struct mgl_texture_level levels[MGL_MAX_TEXTURE_LEVELS]) {
    unsigned level;
    for (level = 0u; level < MGL_MAX_TEXTURE_LEVELS; ++level)
        free(levels[level].data);
}

static void free_storage_cube_array(struct mgl_texture_level levels[6][MGL_MAX_TEXTURE_LEVELS]) {
    unsigned face, level;
    for (face = 0u; face < 6u; ++face)
        for (level = 0u; level < MGL_MAX_TEXTURE_LEVELS; ++level)
            free(levels[face][level].data);
}

static int initialize_storage_level(struct mgl_texture_level *dst, GLenum internalformat,
                                    GLenum format, GLenum type, GLsizei width, GLsizei height,
                                    GLsizei depth, size_t bytes_per_pixel) {
    size_t bytes;
    if (dst == NULL || !checked_mul((size_t)width, (size_t)height, &bytes) ||
        !checked_mul(bytes, (size_t)depth, &bytes) || !checked_mul(bytes, bytes_per_pixel, &bytes))
        return 0;
    memset(dst, 0, sizeof(*dst));
    dst->data = calloc(bytes != 0u ? bytes : 1u, 1u);
    if (dst->data == NULL)
        return 0;
    dst->data_size = bytes;
    dst->width = width;
    dst->height = height;
    dst->depth = depth;
    dst->internal_format = internalformat;
    dst->format = format;
    dst->type = type;
    dst->defined = GL_TRUE;
    dst->compressed = GL_FALSE;
    return 1;
}

static GLint storage_max_levels(GLenum target, GLsizei width, GLsizei height, GLsizei depth) {
    GLsizei largest = width > height ? width : height;
    GLint levels = 1;
    if (target == GL_TEXTURE_3D && depth > largest)
        largest = depth;
    while (largest > 1) {
        largest /= 2;
        ++levels;
    }
    return levels;
}

static size_t compressed_block_bytes(GLenum format);
static int compressed_format_native(const struct mgl_native_dispatch *native, GLenum format);
static int compressed_format_is_rgtc(GLenum format);
static GLenum compressed_fallback_internalformat(GLenum format);
static int compressed_fallback_pixel_format(GLenum format, GLenum *external, GLenum *type);

static int mgl_compressed_storage(struct mgl_context *ctx, struct mgl_texture *obj, GLenum target,
                                  GLsizei levels, GLenum internalformat, GLsizei width,
                                  GLsizei height, GLsizei depth) {
    struct mgl_texture_level staged[MGL_MAX_TEXTURE_LEVELS];
    struct mgl_texture_level cube_staged[6][MGL_MAX_TEXTURE_LEVELS];
    GLsizei w = width, h = height, d = depth;
    GLenum native_format, external, type;
    GLsizei level;
    unsigned face;
    int cube = target == GL_TEXTURE_CUBE_MAP;
    int array = target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY;
    int three_d = target == GL_TEXTURE_3D;
    int native_compressed;
    GLboolean native_immutable_storage = GL_FALSE;
    GLenum native_error = GL_NO_ERROR;
    size_t block_bytes = compressed_block_bytes(internalformat);
    if (block_bytes == 0u)
        return 0;
    if (target == GL_TEXTURE_1D || (target != GL_TEXTURE_2D && !cube && !array && !three_d)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (three_d && compressed_format_is_rgtc(internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (levels < 1 || width < 1 || height < 1 || depth < 1) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (target == GL_TEXTURE_CUBE_MAP_ARRAY &&
        (width != height || depth % 6 != 0)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (levels > (GLsizei)MGL_MAX_TEXTURE_LEVELS ||
        levels > storage_max_levels(target, width, height, depth)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (cube && width != height) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (obj->name == 0u || obj->immutable_format || obj->target != target) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    native_format = compressed_fallback_internalformat(internalformat);
    if (native_format == 0u ||
        !compressed_fallback_pixel_format(internalformat, &external, &type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    native_compressed =
        !three_d && ctx->native_dispatch != NULL &&
        compressed_format_native(ctx->native_dispatch, internalformat) &&
        (array || three_d ? ctx->native_dispatch->glesTexStorage3D != NULL &&
                                ctx->native_dispatch->glesCompressedTexSubImage3D != NULL
                          : ctx->native_dispatch->glesTexStorage2D != NULL &&
                                ctx->native_dispatch->glesCompressedTexSubImage2D != NULL);
    if (ctx->native_dispatch != NULL &&
        (ctx->native_dispatch->glesBindTexture == NULL ||
         (array || three_d
              ? (ctx->native_dispatch->glesTexStorage3D == NULL &&
                 ctx->native_dispatch->glesTexImage3D == NULL) ||
                    (!native_compressed && ctx->native_dispatch->glesTexSubImage3D == NULL)
              : (ctx->native_dispatch->glesTexStorage2D == NULL &&
                 ctx->native_dispatch->glesTexImage2D == NULL) ||
                    (!native_compressed && ctx->native_dispatch->glesTexSubImage2D == NULL)))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    memset(staged, 0, sizeof(staged));
    memset(cube_staged, 0, sizeof(cube_staged));
    for (level = 0; level < levels; ++level) {
        size_t blocks, bytes;
        unsigned count = cube ? 6u : 1u;
        for (face = 0u; face < count; ++face) {
            struct mgl_texture_level *item = cube ? &cube_staged[face][level] : &staged[level];
            if (!checked_mul(((size_t)w + 3u) / 4u, ((size_t)h + 3u) / 4u, &blocks) ||
                !checked_mul(blocks, block_bytes, &bytes) || !checked_mul(bytes, (size_t)d, &bytes))
                goto oom;
            item->data = calloc(bytes != 0u ? bytes : 1u, 1u);
            if (item->data == NULL)
                goto oom;
            item->data_size = bytes;
            item->width = w;
            item->height = h;
            item->depth = d;
            item->internal_format = internalformat;
            item->format = internalformat;
            item->defined = GL_TRUE;
            item->compressed = GL_TRUE;
        }
        if (w > 1)
            w /= 2;
        if (h > 1)
            h /= 2;
        if (three_d && d > 1)
            d /= 2;
    }
    if (ctx->native_dispatch != NULL) {
        mgl_texture_bind_native_unit(ctx, target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target,
                                     obj->native_name, ctx->compat.active_texture);
        if (array || three_d) {
            if (ctx->native_dispatch->glesTexStorage3D != NULL) {
                ctx->native_dispatch->glesTexStorage3D(
                    target, levels, native_compressed ? internalformat : native_format, width,
                    height, depth);
                native_immutable_storage = GL_TRUE;
            } else {
                w = width;
                h = height;
                d = depth;
                native_upload_begin(ctx);
                for (level = 0; level < levels; ++level) {
                    ctx->native_dispatch->glesTexImage3D(target, level, (GLint)native_format, w, h,
                                                         d, 0, external, type, NULL);
                    if (w > 1)
                        w /= 2;
                    if (h > 1)
                        h /= 2;
                    if (three_d && d > 1)
                        d /= 2;
                }
                if (ctx->native_dispatch->glesGetError != NULL)
                    native_error = ctx->native_dispatch->glesGetError();
                native_upload_end(ctx);
            }
        } else if (ctx->native_dispatch->glesTexStorage2D != NULL) {
            ctx->native_dispatch->glesTexStorage2D(
                target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target, levels,
                native_compressed ? internalformat : native_format, width, height);
            native_immutable_storage = GL_TRUE;
        } else {
            w = width;
            h = height;
            native_upload_begin(ctx);
            for (level = 0; level < levels; ++level) {
                if (cube) {
                    for (face = 0u; face < 6u; ++face)
                        ctx->native_dispatch->glesTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                                                             level, (GLint)native_format, w, h, 0,
                                                             external, type, NULL);
                } else {
                    ctx->native_dispatch->glesTexImage2D(
                        target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target, level,
                        (GLint)native_format, w, h, 0, external, type, NULL);
                }
                if (w > 1)
                    w /= 2;
                if (h > 1)
                    h /= 2;
            }
            if (ctx->native_dispatch->glesGetError != NULL)
                native_error = ctx->native_dispatch->glesGetError();
            native_upload_end(ctx);
        }
        if (native_error == GL_NO_ERROR && native_immutable_storage &&
            ctx->native_dispatch->glesGetError != NULL)
            native_error = ctx->native_dispatch->glesGetError();
        if (native_error != GL_NO_ERROR) {
            free_storage_level_array(staged);
            free_storage_cube_array(cube_staged);
            mgl_error_set(&ctx->errors, native_error);
            return 0;
        }
    }
    clear_texture_image_storage(obj);
    if (cube)
        memcpy(obj->cube_levels, cube_staged, sizeof(cube_staged));
    else
        memcpy(obj->levels, staged, sizeof(staged));
    obj->immutable_format = GL_TRUE;
    obj->immutable_levels = levels;
    obj->native_compressed_storage = native_compressed ? GL_TRUE : GL_FALSE;
    obj->native_immutable_storage = native_immutable_storage;
    if (ctx->native_dispatch != NULL && !native_immutable_storage)
        native_realize_immutable_level_range(ctx, obj, target);
    if (target == GL_TEXTURE_1D && ctx->native_dispatch != NULL) {
        struct mgl_texture *bound2d = mgl_bound_texture(ctx, GL_TEXTURE_2D);
        mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D,
                                     bound2d != NULL ? bound2d->native_name : 0u,
                                     ctx->compat.active_texture);
    }
    return 1;
oom:
    free_storage_level_array(staged);
    free_storage_cube_array(cube_staged);
    mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
    return 0;
}

static int texture_storage_2d(struct mgl_context *ctx, struct mgl_texture *obj, GLenum target,
                              GLsizei levels, GLenum internalformat, GLsizei width,
                              GLsizei height) {
    struct mgl_texture_level temp[MGL_MAX_TEXTURE_LEVELS];
    struct mgl_texture_level cube_temp[6][MGL_MAX_TEXTURE_LEVELS];
    GLenum format, type;
    size_t bytes_per_pixel;
    GLsizei level_width = width, level_height = height;
    GLint level;
    unsigned face;
    int cube = target == GL_TEXTURE_CUBE_MAP;
    if (ctx == NULL || obj == NULL)
        return 0;
    if (compressed_block_bytes(internalformat) != 0u)
        return mgl_compressed_storage(ctx, obj, target, levels, internalformat, width, height, 1);
    if (target != GL_TEXTURE_1D && target != GL_TEXTURE_2D && target != GL_TEXTURE_CUBE_MAP) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (!immutable_storage_format(internalformat, &format, &type, &bytes_per_pixel)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if ((internalformat == GL_R16 || internalformat == GL_RG16 || internalformat == GL_RGB16 ||
         internalformat == GL_RGBA16 || internalformat == GL_R16_SNORM ||
         internalformat == GL_RG16_SNORM || internalformat == GL_RGB16_SNORM ||
         internalformat == GL_RGBA16_SNORM) &&
        (ctx->native_dispatch == NULL ||
         !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_EXT_texture_norm16"))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (levels < 1 || width < 1 || height < 1) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (cube && width != height) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (levels > (GLsizei)MGL_MAX_TEXTURE_LEVELS ||
        levels > storage_max_levels(target, width, height, 1)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (obj->name == 0u || obj->immutable_format || obj->target != target) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if ((internalformat == GL_LUMINANCE8 || internalformat == GL_LUMINANCE8_ALPHA8) &&
        (ctx->native_dispatch == NULL || ctx->native_dispatch->glesTexParameteri == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    memset(temp, 0, sizeof(temp));
    memset(cube_temp, 0, sizeof(cube_temp));
    for (level = 0; level < levels; ++level) {
        if (cube) {
            for (face = 0u; face < 6u; ++face) {
                if (!initialize_storage_level(&cube_temp[face][level], internalformat, format, type,
                                              level_width, level_height, 1, bytes_per_pixel)) {
                    free_storage_cube_array(cube_temp);
                    mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                    return 0;
                }
            }
        } else if (!initialize_storage_level(&temp[level], internalformat, format, type,
                                             level_width, level_height, 1, bytes_per_pixel)) {
            free_storage_level_array(temp);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        if (level_width > 1)
            level_width /= 2;
        if (level_height > 1)
            level_height /= 2;
    }
    clear_texture_image_storage(obj);
    if (cube)
        memcpy(obj->cube_levels, cube_temp, sizeof(cube_temp));
    else
        memcpy(obj->levels, temp, sizeof(temp));
    obj->immutable_format = GL_TRUE;
    obj->immutable_levels = levels;
    obj->native_immutable_storage = GL_FALSE;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindTexture != NULL)
        mgl_texture_bind_native_unit(ctx, target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target,
                                     obj->native_name, ctx->compat.active_texture);
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesTexStorage2D != NULL) {
        ctx->native_dispatch->glesTexStorage2D(
            target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target, levels,
            native_storage_internalformat(ctx, internalformat), width, height);
        obj->native_immutable_storage = GL_TRUE;
    } else if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesTexImage2D != NULL) {
        level_width = width;
        level_height = height;
        native_upload_begin(ctx);
        for (level = 0; level < levels; ++level) {
            if (cube) {
                for (face = 0u; face < 6u; ++face)
                    ctx->native_dispatch->glesTexImage2D(
                        GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level,
                        (GLint)native_storage_internalformat(ctx, internalformat), level_width,
                        level_height, 0, native_storage_external_format(format), type, NULL);
            } else {
                ctx->native_dispatch->glesTexImage2D(
                    mgl_texture_native_target(target), level,
                    (GLint)native_storage_internalformat(ctx, internalformat), level_width,
                    level_height, 0, native_storage_external_format(format), type, NULL);
            }
            if (level_width > 1)
                level_width /= 2;
            if (level_height > 1)
                level_height /= 2;
        }
        native_upload_end(ctx);
        native_realize_immutable_level_range(ctx, obj, target);
    }
    if (ctx->native_dispatch != NULL)
        native_apply_legacy_luminance_swizzle(ctx, mgl_texture_native_target(target),
                                              internalformat, 0u);
    if (target == GL_TEXTURE_1D && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesBindTexture != NULL) {
        struct mgl_texture *bound2d = mgl_bound_texture(ctx, GL_TEXTURE_2D);
        mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D,
                                     bound2d != NULL ? bound2d->native_name : 0u,
                                     ctx->compat.active_texture);
    }
    return 1;
}

static int texture_storage_3d(struct mgl_context *ctx, struct mgl_texture *obj, GLenum target,
                              GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height,
                              GLsizei depth) {
    struct mgl_texture_level temp[MGL_MAX_TEXTURE_LEVELS];
    GLenum format, type;
    size_t bytes_per_pixel;
    GLsizei level_width = width, level_height = height, level_depth = depth;
    GLint level;
    if (ctx == NULL || obj == NULL)
        return 0;
    if (compressed_block_bytes(internalformat) != 0u)
        return mgl_compressed_storage(ctx, obj, target, levels, internalformat, width, height,
                                      depth);
    if (!valid_3d_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (!immutable_storage_format(internalformat, &format, &type, &bytes_per_pixel)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if ((internalformat == GL_R16 || internalformat == GL_RG16 || internalformat == GL_RGB16 ||
         internalformat == GL_RGBA16 || internalformat == GL_R16_SNORM ||
         internalformat == GL_RG16_SNORM || internalformat == GL_RGB16_SNORM ||
         internalformat == GL_RGBA16_SNORM) &&
        (ctx->native_dispatch == NULL ||
         !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_EXT_texture_norm16"))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (levels < 1 || width < 1 || height < 1 || depth < 1) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (target == GL_TEXTURE_CUBE_MAP_ARRAY &&
        (width != height || depth % 6 != 0)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (levels > (GLsizei)MGL_MAX_TEXTURE_LEVELS ||
        levels > storage_max_levels(target, width, height, depth)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (obj->name == 0u || obj->immutable_format || obj->target != target ||
        ctx->native_dispatch == NULL || ctx->native_dispatch->glesTexImage3D == NULL ||
        ((internalformat == GL_LUMINANCE8 || internalformat == GL_LUMINANCE8_ALPHA8) &&
         ctx->native_dispatch->glesTexParameteri == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    memset(temp, 0, sizeof(temp));
    for (level = 0; level < levels; ++level) {
        if (!initialize_storage_level(&temp[level], internalformat, format, type, level_width,
                                      level_height, level_depth, bytes_per_pixel)) {
            free_storage_level_array(temp);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        if (level_width > 1)
            level_width /= 2;
        if (level_height > 1)
            level_height /= 2;
        if (target == GL_TEXTURE_3D && level_depth > 1)
            level_depth /= 2;
    }
    clear_texture_image_storage(obj);
    memcpy(obj->levels, temp, sizeof(temp));
    obj->immutable_format = GL_TRUE;
    obj->immutable_levels = levels;
    obj->native_immutable_storage = GL_FALSE;
    if (ctx->native_dispatch->glesBindTexture != NULL)
        mgl_texture_bind_native_unit(ctx, target, obj->native_name, ctx->compat.active_texture);
    if (ctx->native_dispatch->glesTexStorage3D != NULL) {
        ctx->native_dispatch->glesTexStorage3D(target, levels,
                                               native_storage_internalformat(ctx, internalformat),
                                               width, height, depth);
        obj->native_immutable_storage = GL_TRUE;
    } else {
        level_width = width;
        level_height = height;
        level_depth = depth;
        native_upload_begin(ctx);
        for (level = 0; level < levels; ++level) {
            ctx->native_dispatch->glesTexImage3D(
                target, level, (GLint)native_storage_internalformat(ctx, internalformat),
                level_width, level_height, level_depth, 0, native_storage_external_format(format),
                type, NULL);
            if (level_width > 1)
                level_width /= 2;
            if (level_height > 1)
                level_height /= 2;
            if (target == GL_TEXTURE_3D && level_depth > 1)
                level_depth /= 2;
        }
        native_upload_end(ctx);
        native_realize_immutable_level_range(ctx, obj, target);
    }
    native_apply_legacy_luminance_swizzle(ctx, target, internalformat, 0u);
    return 1;
}

void glGenTextures(GLsizei n, GLuint *textures) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0 || textures == NULL)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < n; ++i) {
        GLuint name = next_texture_name(ctx->share_group);
        struct mgl_texture *obj = mgl_texture_ensure(ctx, name, 0u, 0);
        if (obj == NULL) {
            textures[i] = 0u;
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        } else
            textures[i] = name;
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glDeleteTextures(GLsizei n, const GLuint *textures) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    unsigned unit;
    int active_unbound_1d = 0;
    int active_unbound_2d = 0;
    int active_unbound_3d = 0;
    int active_unbound_array = 0;
    int active_unbound_cube = 0;
    int active_unbound_cube_array = 0;
    int active_unbound_ms = 0;
    int active_unbound_msa = 0;
    int active_unbound_buffer = 0;
    if (ctx == NULL)
        return;
    /* Begin/End rejection precedes validation and preserves texture attachments, retained
     * buffers, and object lifetime. */
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0 || textures == NULL)
        return;
    mgl_texture_mark_all_bindings_dirty(ctx);
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < n; ++i) {
        if (textures[i] == 0u)
            continue;
        for (unit = 0u; unit < mgl_programmable_texture_limit(ctx); ++unit) {
            if (ctx->texture_1d_binding[unit] == textures[i]) {
                ctx->texture_1d_binding[unit] = 0u;
                ctx->texture_2d_prefer_1d[unit] = GL_FALSE;
                if (unit == ctx->compat.active_texture)
                    active_unbound_1d = 1;
            }
            if (ctx->texture_rectangle_binding[unit] == textures[i]) {
                ctx->texture_rectangle_binding[unit] = 0u;
                if (unit == ctx->compat.active_texture)
                    active_unbound_2d = 1;
                ctx->texture_2d_prefer_rectangle[unit] = GL_FALSE;
            }
            if (ctx->texture_2d_binding[unit] == textures[i]) {
                ctx->texture_2d_binding[unit] = 0u;
                if (unit == ctx->compat.active_texture)
                    active_unbound_2d = 1;
            }
            if (ctx->texture_3d_binding[unit] == textures[i]) {
                ctx->texture_3d_binding[unit] = 0u;
                if (unit == ctx->compat.active_texture)
                    active_unbound_3d = 1;
            }
            if (ctx->texture_2d_array_binding[unit] == textures[i]) {
                ctx->texture_2d_array_binding[unit] = 0u;
                if (unit == ctx->compat.active_texture)
                    active_unbound_array = 1;
            }
            if (ctx->texture_cube_binding[unit] == textures[i]) {
                ctx->texture_cube_binding[unit] = 0u;
                if (unit == ctx->compat.active_texture)
                    active_unbound_cube = 1;
            }
            if (ctx->texture_cube_array_binding[unit] == textures[i]) {
                ctx->texture_cube_array_binding[unit] = 0u;
                if (unit == ctx->compat.active_texture)
                    active_unbound_cube_array = 1;
            }
            if (ctx->texture_2d_multisample_binding[unit] == textures[i]) {
                ctx->texture_2d_multisample_binding[unit] = 0u;
                if (unit == ctx->compat.active_texture)
                    active_unbound_ms = 1;
            }
            if (ctx->texture_2d_multisample_array_binding[unit] == textures[i]) {
                ctx->texture_2d_multisample_array_binding[unit] = 0u;
                if (unit == ctx->compat.active_texture)
                    active_unbound_msa = 1;
            }
            if (ctx->texture_buffer_binding[unit] == textures[i]) {
                ctx->texture_buffer_binding[unit] = 0u;
                if (unit == ctx->compat.active_texture)
                    active_unbound_buffer = 1;
            }
        }
        for (unit = 0u; unit < MGL_IMAGE_UNIT_CAPACITY; ++unit) {
            if (ctx->image_units[unit].texture == textures[i]) {
                ctx->image_units[unit].texture = 0u;
                ctx->image_units[unit].level = 0;
                ctx->image_units[unit].layered = GL_FALSE;
                ctx->image_units[unit].layer = 0;
                ctx->image_units[unit].access = GL_READ_ONLY;
                ctx->image_units[unit].format = GL_R8;
            }
        }
        mgl_texture_remove(ctx, textures[i]);
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    if ((active_unbound_1d || active_unbound_2d || active_unbound_3d || active_unbound_array ||
         active_unbound_cube || active_unbound_cube_array || active_unbound_ms ||
         active_unbound_msa || active_unbound_buffer) &&
        ctx->native_dispatch != NULL && ctx->native_dispatch->glesActiveTexture != NULL &&
        ctx->native_dispatch->glesBindTexture != NULL) {
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + ctx->compat.active_texture);
        if (active_unbound_2d)
            mgl_texture_bind_native(ctx, GL_TEXTURE_2D, 0u);
        if (active_unbound_3d && ctx->native_dispatch->glesTexImage3D != NULL)
            mgl_texture_bind_native(ctx, GL_TEXTURE_3D, 0u);
        if (active_unbound_array && ctx->native_dispatch->glesTexImage3D != NULL)
            mgl_texture_bind_native(ctx, GL_TEXTURE_2D_ARRAY, 0u);
        if (active_unbound_cube)
            mgl_texture_bind_native(ctx, GL_TEXTURE_CUBE_MAP, 0u);
        if (active_unbound_cube_array)
            mgl_texture_bind_native(ctx, GL_TEXTURE_CUBE_MAP_ARRAY, 0u);
        if (active_unbound_ms && ctx->native_dispatch->glesTexStorage2DMultisample != NULL)
            mgl_texture_bind_native(ctx, GL_TEXTURE_2D_MULTISAMPLE, 0u);
        if (active_unbound_msa && ctx->native_dispatch->glesTexStorage3DMultisample != NULL)
            mgl_texture_bind_native(ctx, GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 0u);
        if (active_unbound_buffer && native_texture_buffer_supported(ctx))
            mgl_texture_bind_native(ctx, GL_TEXTURE_BUFFER, 0u);
    }
}

void glBindTexture(GLenum target, GLuint texture) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_TEXTURE_STATE);
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_BIND_TEXTURE);
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glBindTexture",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"texture", (uint64_t)(uintptr_t)texture}},
                        2u);
    struct mgl_texture *obj = NULL;
    GLuint native_name = 0u;
    if (ctx == NULL)
        return;
    /* Commands compiled during Begin/End are deferred to list replay, where normal legality
     * checks apply. */
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_bind_texture(ctx, target, texture))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_bind_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_bind_texture(ctx, target, texture))
        return;
    if (texture != 0u) {
        (void)pthread_mutex_lock(&ctx->share_group->lock);
        obj = mgl_texture_find(ctx, texture);
        if (obj != NULL && obj->target != 0u && obj->target != target) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        obj = mgl_texture_ensure(ctx, texture, target, 1);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        if (obj == NULL || obj->native_name == 0u) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        native_name = obj->native_name;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesActiveTexture != NULL &&
        ctx->native_dispatch->glesBindTexture != NULL) {
        unsigned unit = ctx->compat.active_texture;
        unsigned index = mgl_native_texture_target_index(target);
        GLuint current = 0u;
        if (target == GL_TEXTURE_2D)
            current = ctx->texture_2d_binding[unit];
        else if (target == GL_TEXTURE_RECTANGLE)
            current = ctx->texture_rectangle_binding[unit];
        else if (target == GL_TEXTURE_3D)
            current = ctx->texture_3d_binding[unit];
        else if (target == GL_TEXTURE_2D_ARRAY)
            current = ctx->texture_2d_array_binding[unit];
        else if (target == GL_TEXTURE_BUFFER)
            current = ctx->texture_buffer_binding[unit];
        else if (target == GL_TEXTURE_CUBE_MAP)
            current = ctx->texture_cube_binding[unit];
        else if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
            current = ctx->texture_cube_array_binding[unit];
        else if (target == GL_TEXTURE_2D_MULTISAMPLE)
            current = ctx->texture_2d_multisample_binding[unit];
        else if (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
            current = ctx->texture_2d_multisample_array_binding[unit];
        if (current == texture && index < MGL_NATIVE_TEXTURE_TARGET_COUNT &&
            (target != GL_TEXTURE_2D ||
             (!ctx->texture_2d_prefer_rectangle[unit] && !ctx->texture_2d_prefer_1d[unit])) &&
            (target != GL_TEXTURE_RECTANGLE || ctx->texture_2d_prefer_rectangle[unit]) &&
            (ctx->native_texture_known[unit] & (1u << index)) != 0u &&
            ctx->native_texture_names[unit][index] == native_name)
            return;
    }
    if (target == GL_TEXTURE_1D) {
        ctx->texture_1d_binding[ctx->compat.active_texture] = texture;
        ctx->texture_2d_prefer_1d[ctx->compat.active_texture] = texture != 0u;
        ctx->texture_2d_prefer_rectangle[ctx->compat.active_texture] = GL_FALSE;
    } else if (target == GL_TEXTURE_2D)
        ctx->texture_2d_binding[ctx->compat.active_texture] = texture;
    else if (target == GL_TEXTURE_RECTANGLE)
        ctx->texture_rectangle_binding[ctx->compat.active_texture] = texture;
    if (target == GL_TEXTURE_RECTANGLE || target == GL_TEXTURE_2D) {
        ctx->texture_2d_prefer_rectangle[ctx->compat.active_texture] =
            target == GL_TEXTURE_RECTANGLE && texture != 0u;
        ctx->texture_2d_prefer_1d[ctx->compat.active_texture] = GL_FALSE;
    } else if (target == GL_TEXTURE_3D)
        ctx->texture_3d_binding[ctx->compat.active_texture] = texture;
    else if (target == GL_TEXTURE_2D_ARRAY)
        ctx->texture_2d_array_binding[ctx->compat.active_texture] = texture;
    else if (target == GL_TEXTURE_BUFFER)
        ctx->texture_buffer_binding[ctx->compat.active_texture] = texture;
    else if (target == GL_TEXTURE_CUBE_MAP)
        ctx->texture_cube_binding[ctx->compat.active_texture] = texture;
    else if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
        ctx->texture_cube_array_binding[ctx->compat.active_texture] = texture;
    else if (target == GL_TEXTURE_2D_MULTISAMPLE)
        ctx->texture_2d_multisample_binding[ctx->compat.active_texture] = texture;
    else if (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        ctx->texture_2d_multisample_array_binding[ctx->compat.active_texture] = texture;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesActiveTexture != NULL) {
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + ctx->compat.active_texture);
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindTexture != NULL &&
        (target != GL_TEXTURE_BUFFER || native_texture_buffer_supported(ctx)))
        mgl_texture_bind_canonical_native_unit(ctx, mgl_texture_native_target(target), native_name,
                                               ctx->compat.active_texture);
    if (target == GL_TEXTURE_RECTANGLE && obj != NULL && !obj->rectangle_native_initialized &&
        ctx->native_dispatch != NULL && ctx->native_dispatch->glesTexParameteri != NULL) {
        ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        obj->rectangle_native_initialized = GL_TRUE;
    }
}

static GLint texture_buffer_offset_alignment(const struct mgl_context *ctx) {
    GLint alignment = 1;
    if (ctx != NULL && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesTexBufferRangeEXT != NULL &&
        ctx->native_dispatch->glesGetIntegerv != NULL) {
        GLint native_alignment = 0;
        ctx->native_dispatch->glesGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT,
                                              &native_alignment);
        if (native_alignment > 0)
            alignment = native_alignment;
    }
    return alignment;
}

static int texture_buffer_attach(struct mgl_context *ctx, struct mgl_texture *texture,
                                 GLenum internalformat, GLuint buffer, GLintptr offset,
                                 GLsizeiptr size, int whole_buffer) {
    struct mgl_buffer *new_buffer = NULL;
    struct mgl_buffer *old_buffer;
    GLint alignment;
    if (ctx == NULL || texture == NULL)
        return 0;
    if (texture_buffer_texel_size(internalformat) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }

    alignment = (!whole_buffer && buffer != 0u) ? texture_buffer_offset_alignment(ctx) : 1;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    if (buffer != 0u) {
        new_buffer = mgl_buffer_find(ctx, buffer);
        if (new_buffer == NULL || !new_buffer->realized) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        if (whole_buffer) {
            offset = 0;
            size = (GLsizeiptr)new_buffer->size;
        } else {
            if (offset < 0 || size <= 0 || alignment <= 0 || (offset % alignment) != 0 ||
                (size_t)offset > new_buffer->size ||
                (size_t)size > new_buffer->size - (size_t)offset) {
                (void)pthread_mutex_unlock(&ctx->share_group->lock);
                mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
                return 0;
            }
        }
    } else {
        offset = 0;
        size = 0;
    }

    old_buffer = texture->buffer_texture_buffer;
    if (new_buffer != old_buffer) {
        if (new_buffer != NULL)
            mgl_buffer_retain_texture(new_buffer);
        texture->buffer_texture_buffer = new_buffer;
        if (old_buffer != NULL)
            mgl_buffer_release_texture(ctx, old_buffer);
    }
    texture->buffer_texture_internal_format = internalformat;
    texture->buffer_texture_offset = offset;
    texture->buffer_texture_size = size;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);

    if (native_texture_buffer_supported(ctx) &&
        mgl_texture_buffer_native_format_supported(internalformat)) {
        GLuint native_buffer = new_buffer != NULL ? new_buffer->native_name : 0u;
        if (new_buffer == NULL || whole_buffer ||
            (offset == 0 && size == (GLsizeiptr)new_buffer->size)) {
            if (ctx->native_dispatch->glesTexBufferEXT != NULL)
                ctx->native_dispatch->glesTexBufferEXT(GL_TEXTURE_BUFFER, internalformat,
                                                       native_buffer);
        } else if (ctx->native_dispatch->glesTexBufferRangeEXT != NULL) {
            ctx->native_dispatch->glesTexBufferRangeEXT(GL_TEXTURE_BUFFER, internalformat,
                                                        native_buffer, offset, size);
        }
    }
    return 1;
}

void glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glTexBuffer",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"buffer", (uint64_t)(uintptr_t)buffer}},
        3u);
    struct mgl_texture *texture;
    if (ctx == NULL)
        return;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_BUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    texture = mgl_bound_texture(ctx, GL_TEXTURE_BUFFER);
    if (texture == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)texture_buffer_attach(ctx, texture, internalformat, buffer, 0, 0, 1);
}

void glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset,
                      GLsizeiptr size) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glTexBufferRange",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"buffer", (uint64_t)(uintptr_t)buffer},
                                       {"offset", (uint64_t)(uintptr_t)offset},
                                       {"size", (uint64_t)(uintptr_t)size}},
        5u);
    struct mgl_texture *texture;
    if (ctx == NULL)
        return;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_BUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    texture = mgl_bound_texture(ctx, GL_TEXTURE_BUFFER);
    if (texture == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)texture_buffer_attach(ctx, texture, internalformat, buffer, offset, size, 0);
}

void glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || obj->target != GL_TEXTURE_BUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    dsa_native_bind_texture(ctx, obj);
    (void)texture_buffer_attach(ctx, obj, internalformat, buffer, 0, 0, 1);
    dsa_native_restore_texture(ctx, GL_TEXTURE_BUFFER);
}

void glTextureBufferEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glTextureBufferEXT",
        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                       {"target", (uint64_t)(uintptr_t)target},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"buffer", (uint64_t)(uintptr_t)buffer}},
        4u);
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (target != GL_TEXTURE_BUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_texture_buffer_ext(ctx, texture, target, internalformat, buffer))
        return;
    if (texture == 0u) {
        obj = mgl_default_texture(ctx, GL_TEXTURE_BUFFER);
    } else {
        (void)pthread_mutex_lock(&ctx->share_group->lock);
        obj = mgl_texture_find(ctx, texture);
        if (obj != NULL && obj->target != 0u && obj->target != GL_TEXTURE_BUFFER) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        obj = mgl_texture_ensure(ctx, texture, GL_TEXTURE_BUFFER, 1);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        if (obj == NULL || !obj->realized) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
    }
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    dsa_native_bind_texture(ctx, obj);
    (void)texture_buffer_attach(ctx, obj, internalformat, buffer, 0, 0, 1);
    dsa_native_restore_texture(ctx, GL_TEXTURE_BUFFER);
}

void glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset,
                          GLsizeiptr size) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || obj->target != GL_TEXTURE_BUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    dsa_native_bind_texture(ctx, obj);
    (void)texture_buffer_attach(ctx, obj, internalformat, buffer, offset, size, 0);
    dsa_native_restore_texture(ctx, GL_TEXTURE_BUFFER);
}

void glTextureBufferRangeEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer,
                             GLintptr offset, GLsizeiptr size) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glTextureBufferRangeEXT",
        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                       {"target", (uint64_t)(uintptr_t)target},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"buffer", (uint64_t)(uintptr_t)buffer},
                                       {"offset", (uint64_t)(uintptr_t)offset},
                                       {"size", (uint64_t)(uintptr_t)size}},
        6u);
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (target != GL_TEXTURE_BUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (texture == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    obj = mgl_texture_find(ctx, texture);
    if (obj == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (obj->target != 0u && obj->target != GL_TEXTURE_BUFFER) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = mgl_texture_ensure(ctx, texture, GL_TEXTURE_BUFFER, 1);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    if (obj == NULL || obj->native_name == 0u) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    dsa_native_bind_texture(ctx, obj);
    (void)texture_buffer_attach(ctx, obj, internalformat, buffer, offset, size, 0);
    dsa_native_restore_texture(ctx, GL_TEXTURE_BUFFER);
}

GLboolean glIsTexture(GLuint texture) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    GLboolean result = GL_FALSE;
    if (ctx == NULL)
        return GL_FALSE;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    if (texture == 0u)
        return GL_FALSE;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    obj = mgl_texture_find(ctx, texture);
    if (obj != NULL && obj->realized)
        result = GL_TRUE;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return result;
}

static GLfloat clamp_texture_priority(GLfloat priority) {
    if (priority < 0.0f)
        return 0.0f;
    if (priority > 1.0f)
        return 1.0f;
    return priority;
}

GLboolean glAreTexturesResident(GLsizei n, const GLuint *textures, GLboolean *residences) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return GL_FALSE;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return GL_FALSE;
    }
    if (n == 0)
        return GL_TRUE;
    if (textures == NULL || residences == NULL)
        return GL_FALSE;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < n; ++i) {
        struct mgl_texture *obj;
        if (textures[i] == 0u) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return GL_FALSE;
        }
        obj = mgl_texture_find(ctx, textures[i]);
        if (obj == NULL || !obj->realized || obj->target == 0u) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return GL_FALSE;
        }
    }
    for (i = 0; i < n; ++i)
        residences[i] = GL_TRUE;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return GL_TRUE;
}

void glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLfloat *priorities) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0 || textures == NULL || priorities == NULL) {
        if (n == 0)
            (void)mgl_list_record_prioritize_textures(ctx, 0, textures, priorities);
        return;
    }
    if (mgl_list_record_prioritize_textures(ctx, n, textures, priorities))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < n; ++i) {
        struct mgl_texture *obj;
        if (textures[i] == 0u)
            continue;
        obj = mgl_texture_find(ctx, textures[i]);
        if (obj == NULL || !obj->realized || obj->target == 0u)
            continue;
        obj->priority = clamp_texture_priority(priorities[i]);
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glPixelStorei(GLenum pname, GLint param) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glPixelStorei",
                        (const struct mgl_error_arg[]){{"pname", (uint64_t)(uintptr_t)pname},
                                                       {"param", (uint64_t)(uintptr_t)param}},
                        2u);
    GLint *slot = NULL;
    int alignment = 0;
    if (ctx == NULL)
        return;
    /* Pixel-store calls within Begin/End preserve pack/unpack state and issue no GLES work. */
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    switch (pname) {
        case GL_UNPACK_SWAP_BYTES:
            ctx->unpack_swap_bytes = param != 0 ? GL_TRUE : GL_FALSE;
            return;
        case GL_UNPACK_LSB_FIRST:
            ctx->unpack_lsb_first = param != 0 ? GL_TRUE : GL_FALSE;
            return;
        case GL_PACK_SWAP_BYTES:
            ctx->pack_swap_bytes = param != 0 ? GL_TRUE : GL_FALSE;
            return;
        case GL_PACK_LSB_FIRST:
            ctx->pack_lsb_first = param != 0 ? GL_TRUE : GL_FALSE;
            return;
        case GL_UNPACK_ALIGNMENT:
            slot = &ctx->unpack_alignment;
            alignment = 1;
            break;
        case GL_PACK_ALIGNMENT:
            slot = &ctx->pack_alignment;
            alignment = 1;
            break;
        case GL_UNPACK_ROW_LENGTH:
            slot = &ctx->unpack_row_length;
            break;
        case GL_UNPACK_SKIP_ROWS:
            slot = &ctx->unpack_skip_rows;
            break;
        case GL_UNPACK_SKIP_PIXELS:
            slot = &ctx->unpack_skip_pixels;
            break;
        case GL_UNPACK_IMAGE_HEIGHT:
            slot = &ctx->unpack_image_height;
            break;
        case GL_UNPACK_SKIP_IMAGES:
            slot = &ctx->unpack_skip_images;
            break;
        case GL_PACK_ROW_LENGTH:
            slot = &ctx->pack_row_length;
            break;
        case GL_PACK_SKIP_ROWS:
            slot = &ctx->pack_skip_rows;
            break;
        case GL_PACK_SKIP_PIXELS:
            slot = &ctx->pack_skip_pixels;
            break;
        case GL_PACK_IMAGE_HEIGHT:
            slot = &ctx->pack_image_height;
            break;
        case GL_PACK_SKIP_IMAGES:
            slot = &ctx->pack_skip_images;
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
    }
    if (alignment) {
        if (param != 1 && param != 2 && param != 4 && param != 8) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
    } else if (param < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    *slot = param;
    /* Row/skip/image modes are interface-owned. mGL repacks to tight storage before
     * native operations, so forwarding them would contaminate internal GLES calls. */
    if (alignment && ctx->native_dispatch != NULL && ctx->native_dispatch->glesPixelStorei != NULL)
        ctx->native_dispatch->glesPixelStorei(pname, param);
}

void glPixelStoref(GLenum pname, GLfloat param) {
    struct mgl_context *ctx = mgl_context_current();
    GLint rounded;
    if (ctx == NULL)
        return;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname == GL_UNPACK_SWAP_BYTES || pname == GL_UNPACK_LSB_FIRST ||
        pname == GL_PACK_SWAP_BYTES || pname == GL_PACK_LSB_FIRST) {
        glPixelStorei(pname, param != 0.0f ? 1 : 0);
        return;
    }
    if (param > (GLfloat)INT_MAX || param < (GLfloat)INT_MIN) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (param >= 0.0f)
        rounded = (GLint)(param + 0.5f);
    else
        rounded = (GLint)(param - 0.5f);
    glPixelStorei(pname, rounded);
}

static int mgl_texture_apply_image_2d_tight_impl(struct mgl_context *ctx, GLenum target,
                                                 GLint level, GLint internalformat, GLsizei width,
                                                 GLsizei height, GLint border, GLenum format,
                                                 GLenum type, const void *data, size_t data_size,
                                                 int pbo_source, size_t pbo_offset) {
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_TEX_IMAGE_APPLY_2D);
    const GLenum original_format = format, original_type = type;
    struct mgl_texture *obj;
    struct mgl_texture_level *dst;
    GLenum previous_internalformat = 0u;
    size_t components = 0u;
    size_t expected = 0u;
    unsigned char *owned = NULL;
    if (ctx == NULL)
        return 0;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    internalformat = mutable_2d_internalformat(internalformat);
    if (!(valid_2d_image_target(target) || target == GL_TEXTURE_1D) ||
        !valid_format_type(format, type, &components) || !valid_internal(internalformat) ||
        !format_matches_internal(format, internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS ||
        (target == GL_TEXTURE_RECTANGLE && level != 0) || width < 0 || height < 0 || border != 0 ||
        (cube_face_index(target) >= 0 && width != height) ||
        !checked_mul((size_t)width, components, &expected) ||
        !checked_mul(expected, (size_t)height, &expected) || expected != data_size) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    obj = mgl_bound_texture(ctx, target == GL_TEXTURE_1D ? GL_TEXTURE_1D
                                 : target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE
                                     ? target
                                     : GL_TEXTURE_CUBE_MAP);
    if (obj == NULL || !obj->realized || obj->immutable_format) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_TEX_SHADOW_IMAGE_2D);
        if (data_size != 0u) {
            owned = malloc(data_size);
            if (owned == NULL) {
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            memcpy(owned, data, data_size);
        } else {
            owned = calloc(1u, 1u);
            if (owned == NULL) {
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
        }
        dst = texture_level_for_target(obj, target, level);
        if (dst == NULL) {
            free(owned);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        if (needs_color_normalization(format, type)) {
            if (!normalize_upload_color(&owned, &data_size, &format, &type,
                                        (GLenum)internalformat)) {
                free(owned);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            normalize_tight_color(owned, data_size, &format, &type);
        }
        if (internalformat == (GLint)GL_INTENSITY8) {
            GLenum intensity_format = native_storage_external_format(format), intensity_type = type;
            unsigned char *intensity =
                repack_narrow_color_upload(owned, (size_t)width * (size_t)height, intensity_format,
                                           type, GL_R8, &intensity_format, &intensity_type);
            if (intensity == NULL) {
                free(owned);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            free(owned);
            owned = intensity;
            data_size = (size_t)width * (size_t)height;
            format = intensity_format;
            type = intensity_type;
        }
        previous_internalformat = dst->internal_format;
        free(dst->data);
        dst->data = owned;
        dst->data_size = data_size;
        dst->width = width;
        dst->height = height;
        dst->depth = 1;
        dst->internal_format = (GLenum)internalformat;
        dst->format = format;
        dst->type = type;
        dst->defined = GL_TRUE;
        dst->compressed = GL_FALSE;
        dst->shadow_stale = GL_FALSE;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesTexImage2D != NULL) {
        GLenum gpu_format = format, gpu_type = type;
        GLint gpu_internal = (GLint)native_storage_internalformat(ctx, (GLenum)internalformat);
        gpu_format = native_storage_external_format(gpu_format);
        if (internalformat == (GLint)GL_RGB5 && format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5)
            gpu_internal = (GLint)GL_RGB565;
        const void *gpu_data = data_size == 0u
                                   ? NULL
                                   : (!needs_color_normalization(original_format, original_type) &&
                                              internalformat != (GLint)GL_INTENSITY8
                                          ? data
                                          : owned);
        unsigned char *gpu_owned = NULL;
        if (internalformat == (GLint)GL_RGBA16 && !rgba16_gpu_native(ctx))
            gpu_internal = (GLint)GL_RGBA8;
        if (gpu_format == GL_RGB && gpu_type == GL_UNSIGNED_SHORT &&
            gles_color_storage_format((GLenum)internalformat) != 0u) {
            gpu_owned = rgb16_to_rgb8(gpu_data, (size_t)width * (size_t)height);
            if (gpu_owned == NULL) {
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            gpu_data = gpu_owned;
            gpu_type = GL_UNSIGNED_BYTE;
        }
        if (format == GL_RGBA && type == GL_UNSIGNED_BYTE && internalformat == (GLint)GL_RGBA16 &&
            rgba16_gpu_native(ctx)) {
            gpu_owned = depth8_to_depth16(owned, data_size);
            if (gpu_owned == NULL) {
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            gpu_data = gpu_owned;
            gpu_type = GL_UNSIGNED_SHORT;
        }
        if (format == GL_RGBA && type == GL_UNSIGNED_BYTE && internalformat == (GLint)GL_RGBA16F) {
            gpu_owned = rgba8_to_half_float(owned, data_size);
            if (gpu_owned == NULL) {
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            gpu_data = gpu_owned;
            gpu_type = GL_HALF_FLOAT;
        }
        if (internalformat == (GLint)GL_RGB16F || internalformat == (GLint)GL_RGB32F_ARB) {
            if (gpu_type == GL_UNSIGNED_BYTE && (gpu_format == GL_RGB || gpu_format == GL_RGBA)) {
                GLenum float_type =
                    native_storage_internalformat(ctx, (GLenum)internalformat) == GL_RGBA16F
                        ? GL_HALF_FLOAT
                        : GL_FLOAT;
                unsigned char *expanded = repack_color_byte_float_rgba(
                    gpu_data, (size_t)width * (size_t)height, gpu_format, float_type);
                if (expanded == NULL) {
                    free(gpu_owned);
                    mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                    return 0;
                }
                free(gpu_owned);
                gpu_owned = expanded;
                gpu_data = expanded;
                gpu_format = GL_RGBA;
                gpu_type = float_type;
            }
        }
        if ((internalformat == (GLint)GL_RGB16F || internalformat == (GLint)GL_RGB32F_ARB) &&
            gpu_format == GL_RGB && (gpu_type == GL_FLOAT || gpu_type == GL_HALF_FLOAT)) {
            gpu_owned = repack_rgb_float_rgba(gpu_data, (size_t)width * (size_t)height, gpu_type);
            if (gpu_owned == NULL) {
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            gpu_data = gpu_owned;
            gpu_format = GL_RGBA;
        }
        if (internalformat == (GLint)GL_DEPTH_COMPONENT32 && format == GL_DEPTH_COMPONENT &&
            type != GL_FLOAT) {
            gpu_owned = depth_integer_to_float(owned, (size_t)width * (size_t)height, type);
            if (gpu_owned == NULL) {
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            gpu_data = gpu_owned;
            gpu_type = GL_FLOAT;
        }
        if (format == GL_DEPTH_COMPONENT && type == GL_UNSIGNED_BYTE &&
            internalformat != (GLint)GL_DEPTH_COMPONENT32) {
            gpu_owned = depth8_to_depth16(owned, data_size);
            if (gpu_owned == NULL) {
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            gpu_data = gpu_owned;
            gpu_type = GL_UNSIGNED_SHORT;
            gpu_internal = (GLint)GL_DEPTH_COMPONENT16;
        }
        if (format == GL_RGBA && type == GL_UNSIGNED_SHORT &&
            (internalformat != (GLint)GL_RGBA16 || !rgba16_gpu_native(ctx))) {
            gpu_owned = rgba16_to_rgba8(owned, data_size);
            if (gpu_owned == NULL) {
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            gpu_data = gpu_owned;
            gpu_type = GL_UNSIGNED_BYTE;
        }
        if (gpu_type == GL_UNSIGNED_BYTE && (gpu_format == GL_RGB || gpu_format == GL_RGBA)) {
            GLenum required = gles_color_storage_format((GLenum)internalformat);
            if (required != 0u && required != gpu_format) {
                unsigned char *repacked = repack_rgb_rgba_for_gles(
                    gpu_data, (size_t)width * (size_t)height, gpu_format, required);
                if (repacked == NULL) {
                    free(gpu_owned);
                    mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                    return 0;
                }
                free(gpu_owned);
                gpu_owned = repacked;
                gpu_data = gpu_owned;
                gpu_format = required;
            }
        }
        if (internalformat == (GLint)GL_R8 || internalformat == (GLint)GL_RG8 ||
            internalformat == (GLint)GL_R16F || internalformat == (GLint)GL_RG16F ||
            narrow_storage_format((GLenum)internalformat) != (GLenum)internalformat) {
            unsigned char *narrow = repack_narrow_color_upload(
                gpu_data, (size_t)width * (size_t)height, gpu_format, gpu_type,
                narrow_storage_format((GLenum)internalformat), &gpu_format, &gpu_type);
            if (narrow == NULL) {
                free(gpu_owned);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            free(gpu_owned);
            gpu_owned = narrow;
            gpu_data = narrow;
        }
        if ((target == GL_TEXTURE_1D || target == GL_TEXTURE_RECTANGLE ||
             target == GL_TEXTURE_2D) &&
            ctx->native_dispatch->glesBindTexture != NULL)
            mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D, obj->native_name,
                                         ctx->compat.active_texture);
        const int use_pbo = native_upload_can_use_pbo(
            ctx, pbo_source, original_format, original_type, gpu_format, gpu_type, data, gpu_data);
        native_upload_begin_source(ctx, use_pbo);
        {
            MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_TEX_IMAGE_2D);
            ctx->native_dispatch->glesTexImage2D(
                mgl_texture_native_target(target), level, gpu_internal, width, height, border,
                gpu_format, gpu_type, (use_pbo ? (const void *)(uintptr_t)pbo_offset : gpu_data));
        }
        native_upload_end(ctx);
        native_apply_legacy_luminance_swizzle(ctx, mgl_texture_native_target(target),
                                              (GLenum)internalformat, previous_internalformat);
        if (target == GL_TEXTURE_1D && ctx->native_dispatch->glesBindTexture != NULL) {
            struct mgl_texture *bound2d = mgl_bound_texture(ctx, GL_TEXTURE_2D);
            mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D,
                                         bound2d != NULL ? bound2d->native_name : 0u,
                                         ctx->compat.active_texture);
        }
        free(gpu_owned);
    }
    return 1;
}

int mgl_texture_apply_image_2d_tight(struct mgl_context *ctx, GLenum target, GLint level,
                                     GLint internalformat, GLsizei width, GLsizei height,
                                     GLint border, GLenum format, GLenum type, const void *data,
                                     size_t data_size) {
    return mgl_texture_apply_image_2d_tight_impl(ctx, target, level, internalformat, width, height,
                                                 border, format, type, data, data_size, 0, 0u);
}

static int subimage_format_compatible(GLenum internalformat, GLenum format) {
    if (integer_storage_format(internalformat))
        return integer_external_format(format);
    if (integer_external_format(format))
        return 0;
    if (internalformat == GL_DEPTH24_STENCIL8 || internalformat == GL_DEPTH32F_STENCIL8)
        return format == GL_DEPTH_STENCIL;
    if (internalformat == GL_STENCIL_INDEX8)
        return format == GL_STENCIL_INDEX;
    if (internalformat == GL_DEPTH_COMPONENT || internalformat == GL_DEPTH_COMPONENT16 ||
        internalformat == GL_DEPTH_COMPONENT24 || internalformat == GL_DEPTH_COMPONENT32 ||
        internalformat == GL_DEPTH_COMPONENT32F)
        return format == GL_DEPTH_COMPONENT;
    return format == GL_RGB || format == GL_RGBA || format == GL_BGR || format == GL_BGRA ||
           format == GL_ALPHA || format == GL_RED || format == GL_RG || format == GL_LUMINANCE ||
           format == GL_LUMINANCE_ALPHA;
}

static double subimage_unit(double value) {
    if (!(value >= 0.0))
        return 0.0;
    return value > 1.0 ? 1.0 : value;
}

static uint16_t subimage_pack_half(double value) {
    return float_to_half_bits((float)subimage_unit(value));
}

static void subimage_read_pixel(const unsigned char *src, GLenum format, GLenum type,
                                double rgba[4]) {
    double channels[4] = {0.0, 0.0, 0.0, 1.0};
    size_t count, i;
    if (format == GL_DEPTH_COMPONENT) {
        uint16_t v16;
        uint32_t v32;
        float vf;
        if (type == GL_UNSIGNED_BYTE)
            channels[0] = (double)src[0] / 255.0;
        else if (type == GL_UNSIGNED_SHORT) {
            memcpy(&v16, src, sizeof(v16));
            channels[0] = (double)v16 / 65535.0;
        } else if (type == GL_UNSIGNED_INT) {
            memcpy(&v32, src, sizeof(v32));
            channels[0] = (double)v32 / 4294967295.0;
        } else {
            memcpy(&vf, src, sizeof(vf));
            channels[0] = subimage_unit(vf);
        }
        rgba[0] = channels[0];
        rgba[1] = rgba[2] = 0.0;
        rgba[3] = 1.0;
        return;
    }
    if (format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5) {
        uint16_t v;
        memcpy(&v, src, sizeof(v));
        channels[0] = (double)((v >> 11u) & 31u) / 31.0;
        channels[1] = (double)((v >> 5u) & 63u) / 63.0;
        channels[2] = (double)(v & 31u) / 31.0;
    } else {
        count =
            format == GL_RGBA
                ? 4u
                : (format == GL_RGB ? 3u
                                    : (format == GL_LUMINANCE_ALPHA || format == GL_RG ? 2u : 1u));
        for (i = 0; i < count; ++i) {
            if (type == GL_UNSIGNED_SHORT) {
                uint16_t v;
                memcpy(&v, src + i * 2u, sizeof(v));
                channels[i] = (double)v / 65535.0;
            } else if (type == GL_HALF_FLOAT) {
                uint16_t half;
                memcpy(&half, src + i * 2u, 2u);
                channels[i] = half_bits_to_double(half);
            } else if (type == GL_FLOAT) {
                float value;
                memcpy(&value, src + i * sizeof(value), sizeof(value));
                channels[i] = value;
            } else
                channels[i] = (double)src[i] / 255.0;
        }
    }
    rgba[0] = channels[0];
    rgba[1] = format == GL_LUMINANCE || format == GL_LUMINANCE_ALPHA ? channels[0] : channels[1];
    rgba[2] = format == GL_LUMINANCE || format == GL_LUMINANCE_ALPHA ? channels[0] : channels[2];
    rgba[3] = format == GL_ALPHA  ? channels[0]
              : format == GL_RGBA ? channels[3]
                                  : (format == GL_LUMINANCE_ALPHA ? channels[1] : 1.0);
}

static void subimage_write_pixel(unsigned char *dst, GLenum format, GLenum type,
                                 const double rgba[4]) {
    size_t count =
        format == GL_RGBA
            ? 4u
            : (format == GL_RGB ? 3u : (format == GL_LUMINANCE_ALPHA || format == GL_RG ? 2u : 1u));
    size_t i;
    if (format == GL_DEPTH_COMPONENT) {
        count = 1u;
    } else if (format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5) {
        uint16_t v = (uint16_t)(((unsigned)(subimage_unit(rgba[0]) * 31.0 + 0.5) << 11u) |
                                ((unsigned)(subimage_unit(rgba[1]) * 63.0 + 0.5) << 5u) |
                                (unsigned)(subimage_unit(rgba[2]) * 31.0 + 0.5));
        memcpy(dst, &v, sizeof(v));
        return;
    }
    for (i = 0; i < count; ++i) {
        double original = format == GL_ALPHA                        ? rgba[3]
                          : format == GL_LUMINANCE_ALPHA && i == 1u ? rgba[3]
                                                                    : rgba[i];
        double v = type == GL_FLOAT || type == GL_HALF_FLOAT ? original : subimage_unit(original);
        if (type == GL_UNSIGNED_BYTE)
            dst[i] = (unsigned char)(v * 255.0 + 0.5);
        else if (type == GL_HALF_FLOAT) {
            uint16_t sample = subimage_pack_half(v);
            memcpy(dst + i * 2u, &sample, sizeof(sample));
        } else if (type == GL_UNSIGNED_SHORT) {
            uint16_t sample = (uint16_t)(v * 65535.0 + 0.5);
            memcpy(dst + i * 2u, &sample, sizeof(sample));
        } else if (type == GL_UNSIGNED_INT) {
            uint32_t sample = (uint32_t)(v * 4294967295.0 + 0.5);
            memcpy(dst + i * 4u, &sample, sizeof(sample));
        } else if (type == GL_FLOAT) {
            float sample = (float)v;
            memcpy(dst + i * 4u, &sample, sizeof(sample));
        }
    }
}

static size_t integer_component_bytes(GLenum type) {
    return type == GL_BYTE || type == GL_UNSIGNED_BYTE     ? 1u
           : type == GL_SHORT || type == GL_UNSIGNED_SHORT ? 2u
           : type == GL_INT || type == GL_UNSIGNED_INT     ? 4u
                                                           : 0u;
}

static int64_t integer_pixel_value(const unsigned char *data, GLenum type) {
    if (type == GL_BYTE) {
        int8_t v;
        memcpy(&v, data, 1u);
        return v;
    }
    if (type == GL_UNSIGNED_BYTE) {
        uint8_t v;
        memcpy(&v, data, 1u);
        return v;
    }
    if (type == GL_SHORT) {
        int16_t v;
        memcpy(&v, data, 2u);
        return v;
    }
    if (type == GL_UNSIGNED_SHORT) {
        uint16_t v;
        memcpy(&v, data, 2u);
        return v;
    }
    if (type == GL_INT) {
        int32_t v;
        memcpy(&v, data, 4u);
        return v;
    }
    if (type == GL_UNSIGNED_INT) {
        uint32_t v;
        memcpy(&v, data, 4u);
        return v;
    }
    return 0;
}

static void integer_pixel_store(unsigned char *data, GLenum type, int64_t value) {
    if (type == GL_BYTE) {
        int8_t v = (int8_t)(value < INT8_MIN ? INT8_MIN : value > INT8_MAX ? INT8_MAX : value);
        memcpy(data, &v, 1u);
    } else if (type == GL_UNSIGNED_BYTE) {
        uint8_t v = (uint8_t)(value < 0 ? 0 : value > UINT8_MAX ? UINT8_MAX : value);
        memcpy(data, &v, 1u);
    } else if (type == GL_SHORT) {
        int16_t v = (int16_t)(value < INT16_MIN   ? INT16_MIN
                              : value > INT16_MAX ? INT16_MAX
                                                  : value);
        memcpy(data, &v, 2u);
    } else if (type == GL_UNSIGNED_SHORT) {
        uint16_t v = (uint16_t)(value < 0 ? 0 : value > UINT16_MAX ? UINT16_MAX : value);
        memcpy(data, &v, 2u);
    } else if (type == GL_INT) {
        int32_t v = (int32_t)(value < INT32_MIN   ? INT32_MIN
                              : value > INT32_MAX ? INT32_MAX
                                                  : value);
        memcpy(data, &v, 4u);
    } else if (type == GL_UNSIGNED_INT) {
        uint32_t v = (uint32_t)(value < 0 ? 0 : value > UINT32_MAX ? UINT32_MAX : value);
        memcpy(data, &v, 4u);
    }
}

static unsigned char *subimage_convert_integer(const unsigned char *src, size_t pixels,
                                               GLenum src_format, GLenum src_type, size_t src_bpp,
                                               GLenum dst_format, GLenum dst_type, size_t dst_bpp) {
    size_t src_channels = src_format == GL_RED_INTEGER   ? 1u
                          : src_format == GL_RG_INTEGER  ? 2u
                          : src_format == GL_RGB_INTEGER ? 3u
                                                         : 4u;
    size_t dst_channels = dst_format == GL_RED_INTEGER   ? 1u
                          : dst_format == GL_RG_INTEGER  ? 2u
                          : dst_format == GL_RGB_INTEGER ? 3u
                                                         : 4u;
    size_t src_size = integer_component_bytes(src_type);
    size_t dst_size = integer_component_bytes(dst_type);
    unsigned char *out;
    size_t i, c;
    if (src_size == 0u || dst_size == 0u || src_bpp != src_channels * src_size ||
        dst_bpp != dst_channels * dst_size || pixels > SIZE_MAX / dst_bpp ||
        (pixels && src == NULL))
        return NULL;
    out = malloc(pixels ? pixels * dst_bpp : 1u);
    if (out == NULL)
        return NULL;
    for (i = 0u; i < pixels; ++i) {
        for (c = 0u; c < dst_channels; ++c) {
            int64_t value = c < src_channels
                                ? integer_pixel_value(src + i * src_bpp + c * src_size, src_type)
                                : (c == 3u ? 1 : 0);
            integer_pixel_store(out + i * dst_bpp + c * dst_size, dst_type, value);
        }
    }
    return out;
}

static unsigned char *subimage_convert_shadow(const unsigned char *src, size_t pixels,
                                              GLenum src_format, GLenum src_type, size_t src_bpp,
                                              GLenum dst_format, GLenum dst_type, size_t dst_bpp) {
    unsigned char *out;
    size_t i;
    if (integer_external_format(src_format) && integer_external_format(dst_format))
        return subimage_convert_integer(src, pixels, src_format, src_type, src_bpp, dst_format,
                                        dst_type, dst_bpp);
    if (pixels > SIZE_MAX / dst_bpp || (pixels && src == NULL))
        return NULL;
    out = malloc(pixels ? pixels * dst_bpp : 1u);
    if (out == NULL)
        return NULL;
    for (i = 0u; i < pixels; ++i) {
        double rgba[4];
        subimage_read_pixel(src + i * src_bpp, src_format, src_type, rgba);
        subimage_write_pixel(out + i * dst_bpp, dst_format, dst_type, rgba);
    }
    return out;
}

static GLenum subimage_canonical_format(GLenum internalformat, GLenum fallback) {
    switch (internalformat) {
        case GL_RGBA:
        case GL_RGBA8:
        case GL_SRGB8_ALPHA8:
        case GL_RGBA16:
        case GL_RGBA16F:
            return GL_RGBA;
        case GL_R8:
        case GL_R16F:
        case GL_R32F:
            return GL_RED;
        case GL_RG8:
        case GL_RG16F:
        case GL_RG32F:
            return GL_RG;
        case GL_ALPHA16F_ARB:
        case GL_ALPHA32F_ARB:
            return GL_ALPHA;
        case GL_LUMINANCE16F_ARB:
        case GL_LUMINANCE32F_ARB:
            return GL_LUMINANCE;
        case GL_LUMINANCE_ALPHA16F_ARB:
        case GL_LUMINANCE_ALPHA32F_ARB:
            return GL_LUMINANCE_ALPHA;
        case GL_RGB:
        case GL_RGB5:
        case GL_RGB8:
        case GL_SRGB8:
        case GL_RGB32F_ARB:
            return GL_RGB;
        case GL_LUMINANCE8:
            return GL_LUMINANCE;
        case GL_LUMINANCE8_ALPHA8:
            return GL_LUMINANCE_ALPHA;
        case GL_INTENSITY8:
            return GL_RED;
        case GL_DEPTH_COMPONENT:
        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT24:
        case GL_DEPTH_COMPONENT32:
        case GL_DEPTH_COMPONENT32F:
            return GL_DEPTH_COMPONENT;
        default:
            return fallback;
    }
}

static int subimage_prepare_shadow(struct mgl_texture_level *dst, const unsigned char *data,
                                   size_t pixels, GLenum format, GLenum type, size_t components,
                                   unsigned char **out_patch, size_t *out_components) {
    GLenum target_format = dst->format, target_type = dst->type;
    size_t target_components, old_components, total_pixels, expected_size;
    unsigned char *patch = NULL, *image = NULL;
    *out_patch = NULL;
    if (!valid_format_type(dst->format, dst->type, &old_components))
        return 0;
    if (format != dst->format || type != dst->type) {
        target_format = subimage_canonical_format(dst->internal_format, dst->format);
        if (type == GL_FLOAT && target_type == GL_UNSIGNED_BYTE &&
            (dst->internal_format == GL_RGBA16 || dst->internal_format == GL_RGB16 ||
             dst->internal_format == GL_R16 || dst->internal_format == GL_RG16 ||
             dst->internal_format == GL_RGB10_A2 || dst->internal_format == GL_RGBA16F ||
             dst->internal_format == GL_RGBA32F || dst->internal_format == GL_RGB16F ||
             dst->internal_format == GL_RGB32F_ARB || dst->internal_format == GL_R16F ||
             dst->internal_format == GL_RG16F))
            target_type = GL_FLOAT;
        if (!valid_format_type(target_format, target_type, &target_components))
            target_type = GL_UNSIGNED_BYTE;
    }
    if (!valid_format_type(target_format, target_type, &target_components) ||
        !checked_mul((size_t)dst->width, (size_t)dst->height, &total_pixels) ||
        !checked_mul(total_pixels, (size_t)dst->depth, &total_pixels) ||
        !checked_mul(total_pixels, old_components, &expected_size) ||
        dst->data_size != expected_size)
        return 0;
    if (format != target_format || type != target_type) {
        patch = subimage_convert_shadow(data, pixels, format, type, components, target_format,
                                        target_type, target_components);
        if (patch == NULL)
            return 0;
    }
    if (target_format != dst->format || target_type != dst->type) {
        image =
            subimage_convert_shadow(dst->data, total_pixels, dst->format, dst->type, old_components,
                                    target_format, target_type, target_components);
        if (image == NULL) {
            free(patch);
            return 0;
        }
        free(dst->data);
        dst->data = image;
        dst->data_size = total_pixels * target_components;
        dst->format = target_format;
        dst->type = target_type;
    }
    *out_patch = patch;
    *out_components = target_components;
    return 1;
}

static GLenum validate_sub_image_2d_destination(struct mgl_texture *obj, GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset, GLsizei width,
                                                GLsizei height, GLenum format,
                                                struct mgl_texture_level **out_level) {
    struct mgl_texture_level *dst;
    size_t dst_components = 0u;
    if (obj == NULL || !obj->realized)
        return GL_INVALID_OPERATION;
    dst = texture_level_for_target(obj, target, level);
    if (dst == NULL || !dst->defined)
        return GL_INVALID_OPERATION;
    if (dst->compressed || !valid_format_type(dst->format, dst->type, &dst_components) ||
        !subimage_format_compatible(dst->internal_format, format))
        return GL_INVALID_OPERATION;
    if (xoffset > dst->width || yoffset > dst->height || width > dst->width - xoffset ||
        height > dst->height - yoffset)
        return GL_INVALID_VALUE;
    if (out_level != NULL)
        *out_level = dst;
    return GL_NO_ERROR;
}

static int mgl_texture_apply_sub_image_2d_tight_impl(struct mgl_context *ctx, GLenum target,
                                                     GLint level, GLint xoffset, GLint yoffset,
                                                     GLsizei width, GLsizei height, GLenum format,
                                                     GLenum type, const void *data,
                                                     size_t data_size, int pbo_source,
                                                     size_t pbo_offset) {
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_TEX_SUB_IMAGE_APPLY_2D);
    const GLenum original_format = format, original_type = type;
    struct mgl_texture *obj;
    struct mgl_texture_level *dst;
    GLenum validation;
    size_t components = 0u, dst_components = 0u, expected = 0u, row_bytes, y;
    unsigned char *normalized = NULL, *converted = NULL;
    if (ctx == NULL)
        return 0;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (!(valid_2d_image_target(target) || target == GL_TEXTURE_1D) ||
        !valid_format_type(format, type, &components)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS ||
        (target == GL_TEXTURE_RECTANGLE && level != 0) || xoffset < 0 || yoffset < 0 || width < 0 ||
        height < 0 || !checked_mul((size_t)width, components, &expected) ||
        !checked_mul(expected, (size_t)height, &expected) || expected != data_size) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (needs_color_normalization(format, type)) {
        normalized = malloc(data_size != 0u ? data_size : 1u);
        if (normalized == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        if (data_size != 0u)
            memcpy(normalized, data, data_size);
        if (!normalize_upload_color(&normalized, &data_size, &format, &type,
                                    texture_upload_level_internalformat(ctx, target, level))) {
            free(normalized);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        normalize_tight_color(normalized, data_size, &format, &type);
        if (!valid_format_type(format, type, &components)) {
            free(normalized);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        data = normalized;
    }
    obj = mgl_bound_texture(ctx, target == GL_TEXTURE_1D ? GL_TEXTURE_1D
                                 : target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE
                                     ? target
                                     : GL_TEXTURE_CUBE_MAP);
    validation = validate_sub_image_2d_destination(obj, target, level, xoffset, yoffset, width,
                                                   height, format, &dst);
    if (validation != GL_NO_ERROR) {
        free(normalized);
        mgl_error_set(&ctx->errors, validation);
        return 0;
    }
    {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_TEX_SHADOW_SUB_IMAGE_2D);
        if (dst->shadow_stale && !materialize_stale_color_shadow(ctx, obj, level, dst)) {
            free(normalized);
            return 0;
        }
        if (!subimage_prepare_shadow(dst, data, (size_t)width * (size_t)height, format, type,
                                     components, &converted, &dst_components)) {
            free(normalized);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        row_bytes = (size_t)width * dst_components;
        for (y = 0u; y < (size_t)height; ++y) {
            size_t dst_off = ((size_t)yoffset + y) * (size_t)dst->width * dst_components +
                             (size_t)xoffset * dst_components;
            const unsigned char *patch = converted != NULL ? converted : data;
            memcpy(dst->data + dst_off, patch + y * row_bytes, row_bytes);
        }
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesTexSubImage2D != NULL) {
        GLenum gpu_type = dst->type;
        GLenum gpu_format = native_storage_external_format(dst->format);
        const void *gpu_data = converted != NULL ? converted : data;
        size_t gpu_bytes = (size_t)width * (size_t)height * dst_components;
        unsigned char *gpu_owned = NULL;
        if (gpu_format == GL_RGB && gpu_type == GL_UNSIGNED_SHORT &&
            gles_color_storage_format(dst->internal_format) != 0u) {
            gpu_owned = rgb16_to_rgb8(gpu_data, (size_t)width * (size_t)height);
            if (gpu_owned == NULL) {
                free(converted);
                free(normalized);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            gpu_data = gpu_owned;
            gpu_type = GL_UNSIGNED_BYTE;
        }
        if (dst->format == GL_RGBA && dst->type == GL_UNSIGNED_BYTE &&
            dst->internal_format == GL_RGBA16 && rgba16_gpu_native(ctx)) {
            gpu_owned = depth8_to_depth16(gpu_data, gpu_bytes);
            if (gpu_owned == NULL) {
                free(converted);
                free(normalized);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            gpu_data = gpu_owned;
            gpu_type = GL_UNSIGNED_SHORT;
        }
        if (dst->format == GL_RGBA && dst->type == GL_UNSIGNED_BYTE &&
            dst->internal_format == GL_RGBA16F) {
            gpu_owned = rgba8_to_half_float(gpu_data, gpu_bytes);
            if (gpu_owned == NULL) {
                free(converted);
                free(normalized);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            gpu_data = gpu_owned;
            gpu_type = GL_HALF_FLOAT;
        }
        if (dst->internal_format == GL_RGB16F || dst->internal_format == GL_RGB32F_ARB) {
            if (gpu_type == GL_UNSIGNED_BYTE && (gpu_format == GL_RGB || gpu_format == GL_RGBA)) {
                GLenum float_type =
                    native_storage_internalformat(ctx, dst->internal_format) == GL_RGBA16F
                        ? GL_HALF_FLOAT
                        : GL_FLOAT;
                unsigned char *expanded = repack_color_byte_float_rgba(
                    gpu_data, (size_t)width * (size_t)height, gpu_format, float_type);
                if (expanded == NULL) {
                    free(gpu_owned);
                    free(converted);
                    free(normalized);
                    mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                    return 0;
                }
                free(gpu_owned);
                gpu_owned = expanded;
                gpu_data = expanded;
                gpu_format = GL_RGBA;
                gpu_type = float_type;
            }
        }
        if ((dst->internal_format == GL_RGB16F || dst->internal_format == GL_RGB32F_ARB) &&
            gpu_format == GL_RGB && (gpu_type == GL_FLOAT || gpu_type == GL_HALF_FLOAT)) {
            gpu_owned = repack_rgb_float_rgba(gpu_data, (size_t)width * (size_t)height, gpu_type);
            if (gpu_owned == NULL) {
                free(converted);
                free(normalized);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            gpu_data = gpu_owned;
            gpu_format = GL_RGBA;
        }
        if (dst->internal_format == GL_DEPTH_COMPONENT32 && dst->format == GL_DEPTH_COMPONENT &&
            dst->type != GL_FLOAT) {
            gpu_owned = depth_integer_to_float(gpu_data, (size_t)width * (size_t)height, dst->type);
            if (gpu_owned == NULL) {
                free(converted);
                free(normalized);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            gpu_data = gpu_owned;
            gpu_type = GL_FLOAT;
        }
        if (dst->format == GL_DEPTH_COMPONENT && dst->type == GL_UNSIGNED_BYTE &&
            dst->internal_format != GL_DEPTH_COMPONENT32) {
            gpu_owned = depth8_to_depth16(gpu_data, gpu_bytes);
            if (gpu_owned == NULL) {
                free(converted);
                free(normalized);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            gpu_data = gpu_owned;
            gpu_type = GL_UNSIGNED_SHORT;
        }
        if (dst->format == GL_RGBA && dst->type == GL_UNSIGNED_SHORT &&
            (dst->internal_format != GL_RGBA16 || !rgba16_gpu_native(ctx))) {
            gpu_owned = rgba16_to_rgba8(gpu_data, gpu_bytes);
            if (gpu_owned == NULL) {
                free(converted);
                free(normalized);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            gpu_type = GL_UNSIGNED_BYTE;
            gpu_data = gpu_owned;
        }
        if (gpu_type == GL_UNSIGNED_BYTE && (gpu_format == GL_RGB || gpu_format == GL_RGBA)) {
            GLenum required = gles_color_storage_format(dst->internal_format);
            if (required != 0u && required != gpu_format) {
                unsigned char *repacked = repack_rgb_rgba_for_gles(
                    gpu_data, (size_t)width * (size_t)height, gpu_format, required);
                if (repacked == NULL) {
                    free(gpu_owned);
                    free(converted);
                    free(normalized);
                    mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                    return 0;
                }
                free(gpu_owned);
                gpu_owned = repacked;
                gpu_data = gpu_owned;
                gpu_format = required;
            }
        }
        if (dst->internal_format == GL_R8 || dst->internal_format == GL_RG8 ||
            dst->internal_format == GL_R16F || dst->internal_format == GL_RG16F ||
            narrow_storage_format(dst->internal_format) != dst->internal_format) {
            unsigned char *narrow = repack_narrow_color_upload(
                gpu_data, (size_t)width * (size_t)height, gpu_format, gpu_type,
                narrow_storage_format(dst->internal_format), &gpu_format, &gpu_type);
            if (narrow == NULL) {
                free(gpu_owned);
                free(converted);
                free(normalized);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            free(gpu_owned);
            gpu_owned = narrow;
            gpu_data = narrow;
        }
        if ((target == GL_TEXTURE_1D || target == GL_TEXTURE_RECTANGLE ||
             target == GL_TEXTURE_2D) &&
            ctx->native_dispatch->glesBindTexture != NULL)
            mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D, obj->native_name,
                                         ctx->compat.active_texture);
        const int use_pbo = native_upload_can_use_pbo(
            ctx, pbo_source, original_format, original_type, gpu_format, gpu_type, data, gpu_data);
        native_upload_begin_source(ctx, use_pbo);
        {
            MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_TEX_SUB_IMAGE_2D);
            ctx->native_dispatch->glesTexSubImage2D(
                mgl_texture_native_target(target), level, xoffset, yoffset, width, height,
                gpu_format, gpu_type, (use_pbo ? (const void *)(uintptr_t)pbo_offset : gpu_data));
        }
        native_upload_end(ctx);
        if (target == GL_TEXTURE_1D && ctx->native_dispatch->glesBindTexture != NULL) {
            struct mgl_texture *bound2d = mgl_bound_texture(ctx, GL_TEXTURE_2D);
            mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D,
                                         bound2d != NULL ? bound2d->native_name : 0u,
                                         ctx->compat.active_texture);
        }
        free(gpu_owned);
    }
    free(converted);
    free(normalized);
    return 1;
}

int mgl_texture_apply_sub_image_2d_tight(struct mgl_context *ctx, GLenum target, GLint level,
                                         GLint xoffset, GLint yoffset, GLsizei width,
                                         GLsizei height, GLenum format, GLenum type,
                                         const void *data, size_t data_size) {
    return mgl_texture_apply_sub_image_2d_tight_impl(ctx, target, level, xoffset, yoffset, width,
                                                     height, format, type, data, data_size, 0, 0u);
}

void glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border,
                  GLenum format, GLenum type, const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glTexImage1D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"border", (uint64_t)(uintptr_t)border},
                                       {"format", (uint64_t)(uintptr_t)format},
                                       {"type", (uint64_t)(uintptr_t)type},
                                       {"pixels", (uint64_t)(uintptr_t)pixels}},
        8u);
    size_t pixel_bytes = 0u, size = 0u;
    unsigned char *data;
    if (ctx == NULL)
        return;
    if (invalid_packed_upload_pair(format, type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_1D && target != GL_PROXY_TEXTURE_1D) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!(valid_format_type(format, type, &pixel_bytes) ||
          (format == GL_BGR && type == GL_UNSIGNED_BYTE && (pixel_bytes = 3u))) ||
        !valid_internal(internalformat) || !format_matches_internal(format, internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || border != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (target == GL_PROXY_TEXTURE_1D) {
        struct mgl_texture_level *proxy = &ctx->proxy_texture_1d_levels[level];
        GLint limit = 0;
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegerv != NULL)
            ctx->native_dispatch->glesGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
        if (limit <= 0)
            limit = 2048;
        memset(proxy, 0, sizeof(*proxy));
        if (width <= (limit >> level)) {
            proxy->defined = GL_TRUE;
            proxy->width = width;
            proxy->height = 1;
            proxy->depth = 1;
            proxy->internal_format = (GLenum)internalformat;
            proxy->format = format;
            proxy->type = type;
        }
        return;
    }
    if (format != GL_BGR && !needs_color_normalization(format, type) &&
        !color_transfer_active(&ctx->pixel_transfer)) {
        const unsigned char *direct =
            tight_borrow_2d(ctx, pixels, width, 1, pixel_bytes, pixel_element_size(type), &size);
        if (direct != NULL) {
            (void)mgl_texture_apply_image_2d_tight(ctx, target, level, internalformat, width, 1,
                                                   border, format, type, direct, size);
            return;
        }
    }
    data = tight_copy(ctx, pixels, width, 1, pixel_bytes, pixel_element_size(type), &size);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (needs_color_normalization(format, type)) {
        if (!normalize_upload_color(&data, &size, &format, &type, (GLenum)internalformat)) {
            free(data);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        normalize_tight_color(data, size, &format, &type);
    }
    if ((pixels != NULL || ctx->pixel_unpack_buffer != 0u) &&
        !transform_texture_color(ctx, &data, &size, &format, &type, 0)) {
        free(data);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    (void)mgl_texture_apply_image_2d_tight(ctx, target, level, internalformat, width, 1, border,
                                           format, type, data, size);
    free(data);
}

void glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format,
                     GLenum type, const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTexSubImage1D",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"format", (uint64_t)(uintptr_t)format},
                                                       {"type", (uint64_t)(uintptr_t)type},
                                                       {"pixels", (uint64_t)(uintptr_t)pixels}},
                        7u);
    size_t pixel_bytes = 0u, size = 0u;
    unsigned char *data;
    if (ctx == NULL)
        return;
    if (invalid_packed_upload_pair(format, type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_1D ||
        !(valid_format_type(format, type, &pixel_bytes) ||
          (format == GL_BGR && type == GL_UNSIGNED_BYTE && (pixel_bytes = 3u)))) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || width < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (format != GL_BGR && !needs_color_normalization(format, type) &&
        !color_transfer_active(&ctx->pixel_transfer)) {
        const unsigned char *direct =
            tight_borrow_2d(ctx, pixels, width, 1, pixel_bytes, pixel_element_size(type), &size);
        if (direct != NULL) {
            (void)mgl_texture_apply_sub_image_2d_tight(ctx, target, level, xoffset, 0, width, 1,
                                                       format, type, direct, size);
            return;
        }
    }
    data = tight_copy(ctx, pixels, width, 1, pixel_bytes, pixel_element_size(type), &size);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (needs_color_normalization(format, type)) {
        if (!normalize_upload_color(&data, &size, &format, &type,
                                    texture_upload_level_internalformat(ctx, target, level))) {
            free(data);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        normalize_tight_color(data, size, &format, &type);
    }
    if (!transform_texture_color(ctx, &data, &size, &format, &type, 0)) {
        free(data);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    (void)mgl_texture_apply_sub_image_2d_tight(ctx, target, level, xoffset, 0, width, 1, format,
                                               type, data, size);
    free(data);
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                  GLint border, GLenum format, GLenum type, const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_TEX_IMAGE_2D);
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glTexImage2D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"border", (uint64_t)(uintptr_t)border},
                                       {"format", (uint64_t)(uintptr_t)format},
                                       {"type", (uint64_t)(uintptr_t)type}},
        8u);
    size_t components = 0u, data_size = 0u;
    unsigned char *data;
    int suppress;
    int indexed_source = format == GL_COLOR_INDEX;
    uint64_t perf_started = mgl_perf_diag_now();
    GLint storage_internalformat = mutable_2d_internalformat(internalformat);
    if (ctx == NULL)
        return;
    if (invalid_packed_upload_pair(format, type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    /* Begin/End rejection precedes source-pixel access and image mutation. Compiled commands are
     * validated at replay. */
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_tex_image_2d(ctx, target, level, internalformat, width, height, border,
                                         format, type, NULL, 0u))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!(valid_2d_image_target(target) || target == GL_PROXY_TEXTURE_2D) ||
        !valid_format_type(format, type, &components) || !valid_internal(storage_internalformat) ||
        !format_matches_internal(format, storage_internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS ||
        (target == GL_TEXTURE_RECTANGLE && level != 0) || width < 0 || height < 0 || border != 0 ||
        (cube_face_index(target) >= 0 && width != height)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (target == GL_PROXY_TEXTURE_2D) {
        struct mgl_texture_level *proxy = &ctx->proxy_texture_2d_levels[level];
        GLint limit = 0;
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegerv != NULL)
            ctx->native_dispatch->glesGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
        if (limit <= 0)
            limit = 2048;
        /* Proxy textures store queryable dimensions without allocating GLES storage; oversized
         * levels remain undefined. */
        memset(proxy, 0, sizeof(*proxy));
        if (width <= (limit >> level) && height <= (limit >> level)) {
            proxy->defined = GL_TRUE;
            proxy->width = width;
            proxy->height = height;
            proxy->depth = 1;
            proxy->internal_format = (GLenum)internalformat;
            proxy->format = format;
            proxy->type = type;
        }
        return;
    }
    if (!needs_color_normalization(format, type) && format != GL_COLOR_INDEX &&
        !color_transfer_active(&ctx->pixel_transfer)) {
        const unsigned char *direct = tight_borrow_2d(ctx, pixels, width, height, components,
                                                      pixel_element_size(type), &data_size);
        if (direct != NULL) {
            suppress =
                mgl_list_record_tex_image_2d(ctx, target, level, internalformat, width, height,
                                             border, format, type, direct, data_size);
            if (!suppress) {
                size_t offset = 0u;
                int pbo_source =
                    texture_pbo_source_offset(ctx, direct, pixel_element_size(type), &offset);
                (void)mgl_texture_apply_image_2d_tight_impl(ctx, target, level, internalformat,
                                                            width, height, border, format, type,
                                                            direct, data_size, pbo_source, offset);
            }
            mgl_perf_diag_texture(ctx, perf_started, data_size);
            return;
        }
    }
    data = tight_copy(ctx, pixels, width, height, components, pixel_element_size(type), &data_size);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (needs_color_normalization(format, type)) {
        if (!normalize_upload_color(&data, &data_size, &format, &type, (GLenum)internalformat)) {
            free(data);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        normalize_tight_color(data, data_size, &format, &type);
    }
    if (format == GL_COLOR_INDEX && type == GL_UNSIGNED_BYTE) {
        unsigned char *converted = color_index_to_rgba8(ctx, data, data_size);
        if (converted == NULL) {
            free(data);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        free(data);
        data = converted;
        data_size *= 4u;
        format = GL_RGBA;
    }
    if ((pixels != NULL || ctx->pixel_unpack_buffer != 0u) &&
        !transform_texture_color(ctx, &data, &data_size, &format, &type, indexed_source)) {
        free(data);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    suppress = mgl_list_record_tex_image_2d(ctx, target, level, internalformat, width, height,
                                            border, format, type, data, data_size);
    if (!suppress)
        (void)mgl_texture_apply_image_2d_tight(ctx, target, level, internalformat, width, height,
                                               border, format, type, data, data_size);
    free(data);
    mgl_perf_diag_texture(ctx, perf_started, data_size);
}

struct mgl_gles_upload_format {
    GLenum internal;
    GLenum format;
    GLenum type;
    size_t bytes_per_pixel;
};

size_t mgl_gles_native_upload_pixel_size(GLenum internal, GLenum format, GLenum type) {
    static const struct mgl_gles_upload_format formats[] = {
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
        {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, 8u},
        {GL_RGBA16_SNORM, GL_RGBA, GL_SHORT, 8u},
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
        {GL_RGB16, GL_RGB, GL_UNSIGNED_SHORT, 6u},
        {GL_RGB16_SNORM, GL_RGB, GL_SHORT, 6u},
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
        {GL_RG16, GL_RG, GL_UNSIGNED_SHORT, 4u},
        {GL_RG16_SNORM, GL_RG, GL_SHORT, 4u},
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
        {GL_R16, GL_RED, GL_UNSIGNED_SHORT, 2u},
        {GL_R16_SNORM, GL_RED, GL_SHORT, 2u},
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
    size_t i;
    for (i = 0u; i < sizeof(formats) / sizeof(formats[0]); ++i) {
        if (formats[i].internal == internal && formats[i].format == format &&
            formats[i].type == type)
            return formats[i].bytes_per_pixel;
    }
    return 0u;
}

static int angle_native_texture_shadow_readable(const struct mgl_texture_level *dst) {
    if (dst->type != GL_UNSIGNED_BYTE)
        return 0;
    if ((dst->internal_format == GL_RGBA8 || dst->internal_format == GL_RGBA) &&
        dst->format == GL_RGBA)
        return 1;
    if ((dst->internal_format == GL_RGB8 || dst->internal_format == GL_RGB) &&
        dst->format == GL_RGB)
        return 1;
    if ((dst->internal_format == GL_R8 || dst->internal_format == GL_RED) && dst->format == GL_RED)
        return 1;
    if ((dst->internal_format == GL_RG8 || dst->internal_format == GL_RG) && dst->format == GL_RG)
        return 1;
    return 0;
}

static int materialize_stale_color_shadow(struct mgl_context *ctx, struct mgl_texture *obj,
                                          GLint level, struct mgl_texture_level *dst) {
    unsigned char *rgba = NULL;
    size_t pixels, bytes, channels, i;
    if (!dst->shadow_stale)
        return 1;
    if (!angle_native_texture_shadow_readable(dst) ||
        !checked_mul((size_t)dst->width, (size_t)dst->height, &pixels))
        return 0;
    channels = dst->format == GL_RED   ? 1u
               : dst->format == GL_RG  ? 2u
               : dst->format == GL_RGB ? 3u
                                       : 4u;
    if (!checked_mul(pixels, channels, &bytes) || dst->data_size != bytes ||
        !read_rendered_color_texture(ctx, obj, level, dst->width, dst->height, &rgba))
        return 0;
    if (channels == 4u)
        memcpy(dst->data, rgba, bytes);
    else
        for (i = 0u; i < pixels; ++i)
            memcpy(dst->data + i * channels, rgba + i * 4u, channels);
    free(rgba);
    dst->shadow_stale = GL_FALSE;
    return 1;
}

static int angle_native_sub_image(struct mgl_context *ctx, GLenum target, GLint level,
                                  GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,
                                  GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                  const void *pixels) {
    struct mgl_texture *obj;
    struct mgl_texture_level *dst;
    struct mgl_buffer *pbo = NULL;
    struct mgl_pixel_layout layout;
    const int is_3d = target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY ||
                      target == GL_TEXTURE_CUBE_MAP_ARRAY;
    const int is_cube = cube_face_index(target) >= 0;
    const int has_pbo = ctx->pixel_unpack_buffer != 0u;
    const int can_read_color = !is_3d && !is_cube && target == GL_TEXTURE_2D;
    size_t bpp;
    int lazy_shadow;
    GLenum native_target;
    if (!ctx->angle_direct_texture_passthrough || ctx->compiling_list != NULL ||
        ctx->unpack_swap_bytes || ctx->unpack_lsb_first ||
        color_transfer_active(&ctx->pixel_transfer) ||
        (is_3d ? !valid_3d_target(target) : !(target == GL_TEXTURE_2D || is_cube)) ||
        ctx->native_dispatch == NULL || ctx->native_dispatch->glesBindBuffer == NULL ||
        ctx->native_dispatch->glesPixelStorei == NULL ||
        ctx->native_dispatch->glesGetError == NULL ||
        (is_3d ? ctx->native_dispatch->glesTexSubImage3D == NULL
               : ctx->native_dispatch->glesTexSubImage2D == NULL) ||
        level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width <= 0 || height <= 0 || depth <= 0)
        return 0;
    native_target = is_cube ? GL_TEXTURE_CUBE_MAP : target;
    obj = mgl_bound_texture(ctx, native_target);
    dst = texture_level_for_target(obj, target, level);
    if (obj == NULL || !obj->realized || obj->view_root != NULL || dst == NULL || !dst->defined ||
        dst->compressed || dst->data == NULL ||
        native_storage_internalformat(ctx, dst->internal_format) != dst->internal_format ||
        xoffset > dst->width || yoffset > dst->height || zoffset > dst->depth ||
        width > dst->width - xoffset || height > dst->height - yoffset ||
        depth > dst->depth - zoffset)
        return 0;
    bpp = mgl_gles_native_upload_pixel_size(dst->internal_format, format, type);
    if (bpp == 0u)
        return 0;
    lazy_shadow = can_read_color && angle_native_texture_shadow_readable(dst) &&
                  ctx->native_dispatch->glesReadPixels != NULL &&
                  ctx->native_dispatch->glesGenFramebuffers != NULL &&
                  ctx->native_dispatch->glesDeleteFramebuffers != NULL &&
                  ctx->native_dispatch->glesBindFramebuffer != NULL &&
                  ctx->native_dispatch->glesFramebufferTexture2D != NULL &&
                  ctx->native_dispatch->glesCheckFramebufferStatus != NULL;
    if (!lazy_shadow) {
        size_t texels, bytes;
        if (has_pbo || dst->shadow_stale || dst->format != format || dst->type != type ||
            !checked_mul((size_t)dst->width, (size_t)dst->height, &texels) ||
            !checked_mul(texels, (size_t)dst->depth, &texels) ||
            !checked_mul(texels, bpp, &bytes) || dst->data_size != bytes)
            return 0;
    }
    if (is_3d) {
        if (!mgl_pixel_layout_3d(width, height, depth, bpp, ctx->unpack_alignment,
                                 ctx->unpack_row_length, ctx->unpack_image_height,
                                 ctx->unpack_skip_pixels, ctx->unpack_skip_rows,
                                 ctx->unpack_skip_images, &layout))
            return 0;
    } else if (!mgl_pixel_layout_2d(width, height, bpp, ctx->unpack_alignment,
                                    ctx->unpack_row_length, ctx->unpack_skip_pixels,
                                    ctx->unpack_skip_rows, &layout))
        return 0;
    if (has_pbo) {
        pbo = mgl_buffer_find(ctx, ctx->pixel_unpack_buffer);
        if (pbo == NULL || pbo->native_name == 0u || pbo->mapped ||
            (size_t)(uintptr_t)pixels > pbo->size ||
            layout.span_bytes > pbo->size - (size_t)(uintptr_t)pixels)
            return 0;
    } else if (pixels == NULL)
        return 0;
    mgl_texture_bind_native_unit(ctx, native_target, obj->native_name, ctx->compat.active_texture);
    mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_UNPACK_BUFFER, has_pbo ? pbo->native_name : 0u);
    ctx->native_dispatch->glesPixelStorei(GL_UNPACK_ALIGNMENT, ctx->unpack_alignment);
    ctx->native_dispatch->glesPixelStorei(GL_UNPACK_ROW_LENGTH, ctx->unpack_row_length);
    ctx->native_dispatch->glesPixelStorei(GL_UNPACK_SKIP_ROWS, ctx->unpack_skip_rows);
    ctx->native_dispatch->glesPixelStorei(GL_UNPACK_SKIP_PIXELS, ctx->unpack_skip_pixels);
    if (is_3d) {
        ctx->native_dispatch->glesPixelStorei(GL_UNPACK_IMAGE_HEIGHT, ctx->unpack_image_height);
        ctx->native_dispatch->glesPixelStorei(GL_UNPACK_SKIP_IMAGES, ctx->unpack_skip_images);
        ctx->native_dispatch->glesTexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                                                height, depth, format, type, pixels);
    } else {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_TEX_SUB_IMAGE_2D);
        ctx->native_dispatch->glesTexSubImage2D(target, level, xoffset, yoffset, width, height,
                                                format, type, pixels);
    }
    ctx->native_dispatch->glesPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    ctx->native_dispatch->glesPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    ctx->native_dispatch->glesPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    if (is_3d) {
        ctx->native_dispatch->glesPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
        ctx->native_dispatch->glesPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
    }
    mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_UNPACK_BUFFER, native_unpack_buffer(ctx));
    {
        GLenum error = ctx->native_dispatch->glesGetError();
        if (error != GL_NO_ERROR) {
            mgl_error_set(&ctx->errors, error);
            return 1;
        }
    }
    if (lazy_shadow) {
        dst->shadow_stale = GL_TRUE;
        obj->used_as_color_attachment = GL_TRUE;
    } else {
        const unsigned char *src = (const unsigned char *)pixels + layout.start_offset;
        size_t row_bytes = (size_t)width * bpp;
        size_t row, layer;
        for (layer = 0u; layer < (size_t)depth; ++layer) {
            for (row = 0u; row < (size_t)height; ++row) {
                size_t dest_offset =
                    (((layer + (size_t)zoffset) * (size_t)dst->height + (row + (size_t)yoffset)) *
                         (size_t)dst->width +
                     (size_t)xoffset) *
                    bpp;
                memcpy(dst->data + dest_offset,
                       src + layer * layout.image_stride + row * layout.row_stride, row_bytes);
            }
        }
    }
    mgl_perf_diag_angle_texture(ctx, has_pbo, 0);
    return 1;
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                     GLsizei height, GLenum format, GLenum type, const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_TEX_SUB_IMAGE_2D);
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTexSubImage2D",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height},
                                                       {"format", (uint64_t)(uintptr_t)format},
                                                       {"type", (uint64_t)(uintptr_t)type}},
                        8u);
    size_t components = 0u, data_size = 0u;
    unsigned char *data;
    int suppress;
    int indexed_source = format == GL_COLOR_INDEX;
    uint64_t perf_started = mgl_perf_diag_now();
    if (ctx == NULL)
        return;
    if (invalid_packed_upload_pair(format, type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }

    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_tex_sub_image_2d(ctx, target, level, xoffset, yoffset, width, height,
                                             format, type, NULL, 0u))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_2d_image_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (angle_native_sub_image(ctx, target, level, xoffset, yoffset, 0, width, height, 1, format,
                               type, pixels)) {
        mgl_perf_diag_texture(ctx, perf_started, 0u);
        return;
    }
    if (!valid_format_type(format, type, &components)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!needs_color_normalization(format, type) && format != GL_COLOR_INDEX &&
        !color_transfer_active(&ctx->pixel_transfer)) {
        const unsigned char *direct = tight_borrow_2d(ctx, pixels, width, height, components,
                                                      pixel_element_size(type), &data_size);
        if (direct != NULL) {
            suppress = mgl_list_record_tex_sub_image_2d(ctx, target, level, xoffset, yoffset, width,
                                                        height, format, type, direct, data_size);
            if (!suppress) {
                size_t offset = 0u;
                int pbo_source =
                    texture_pbo_source_offset(ctx, direct, pixel_element_size(type), &offset);
                (void)mgl_texture_apply_sub_image_2d_tight_impl(
                    ctx, target, level, xoffset, yoffset, width, height, format, type, direct,
                    data_size, pbo_source, offset);
            }
            mgl_perf_diag_texture(ctx, perf_started, data_size);
            return;
        }
    }
    data = tight_copy(ctx, pixels, width, height, components, pixel_element_size(type), &data_size);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (needs_color_normalization(format, type)) {
        if (!normalize_upload_color(&data, &data_size, &format, &type,
                                    texture_upload_level_internalformat(ctx, target, level))) {
            free(data);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        normalize_tight_color(data, data_size, &format, &type);
    }
    if (format == GL_COLOR_INDEX && type == GL_UNSIGNED_BYTE) {
        unsigned char *converted = color_index_to_rgba8(ctx, data, data_size);
        if (converted == NULL) {
            free(data);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        free(data);
        data = converted;
        data_size *= 4u;
        format = GL_RGBA;
    }
    if (!transform_texture_color(ctx, &data, &data_size, &format, &type, indexed_source)) {
        free(data);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    suppress = mgl_list_record_tex_sub_image_2d(ctx, target, level, xoffset, yoffset, width, height,
                                                format, type, data, data_size);
    if (!suppress)
        (void)mgl_texture_apply_sub_image_2d_tight(ctx, target, level, xoffset, yoffset, width,
                                                   height, format, type, data, data_size);
    free(data);
    mgl_perf_diag_texture(ctx, perf_started, data_size);
}

static int mgl_texture_apply_image_3d_tight_impl(struct mgl_context *ctx, GLenum target,
                                                 GLint level, GLint internalformat, GLsizei width,
                                                 GLsizei height, GLsizei depth, GLint border,
                                                 GLenum format, GLenum type, const void *data,
                                                 size_t data_size, int pbo_source,
                                                 size_t pbo_offset) {
    const GLenum original_format = format, original_type = type;
    struct mgl_texture *obj;
    struct mgl_texture_level *dst;
    GLenum previous_internalformat = 0u;
    size_t components = 0u, expected = 0u;
    unsigned char *owned;
    if (ctx == NULL)
        return 0;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (!valid_3d_target(target) || !valid_format_type(format, type, &components) ||
        !valid_internal(internalformat) || !format_matches_internal(format, internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (target == GL_TEXTURE_CUBE_MAP_ARRAY &&
        (width != height || depth <= 0 || depth % 6 != 0)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        depth < 0 || border != 0 || !checked_mul((size_t)width, components, &expected) ||
        !checked_mul(expected, (size_t)height, &expected) ||
        !checked_mul(expected, (size_t)depth, &expected) || expected != data_size) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL || !obj->realized || obj->immutable_format) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesTexImage3D == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    owned = data_size != 0u ? malloc(data_size) : calloc(1u, 1u);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    if (data_size != 0u)
        memcpy(owned, data, data_size);
    if (needs_color_normalization(format, type)) {
        if (!normalize_upload_color(&owned, &data_size, &format, &type, (GLenum)internalformat)) {
            free(owned);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        normalize_tight_color(owned, data_size, &format, &type);
    }
    dst = texture_level_for_target(obj, target, level);
    if (dst == NULL) {
        free(owned);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    previous_internalformat = dst->internal_format;
    free(dst->data);
    memset(dst, 0, sizeof(*dst));
    dst->data = owned;
    dst->data_size = data_size;
    dst->width = width;
    dst->height = height;
    dst->depth = depth;
    dst->internal_format = (GLenum)internalformat;
    dst->format = format;
    dst->type = type;
    dst->defined = GL_TRUE;
    dst->compressed = GL_FALSE;
    {
        GLenum gpu_format = native_storage_external_format(format);
        GLenum gpu_type = type;
        GLint gpu_internal = (GLint)native_storage_internalformat(ctx, (GLenum)internalformat);
        const void *gpu_data = data_size == 0u
                                   ? NULL
                                   : (!needs_color_normalization(original_format, original_type) &&
                                              internalformat != (GLint)GL_INTENSITY8
                                          ? data
                                          : owned);
        unsigned char *gpu_owned = NULL;
        unsigned char *repacked = NULL;
        if (internalformat == (GLint)GL_RGBA16 && !rgba16_gpu_native(ctx))
            gpu_internal = (GLint)GL_RGBA8;
        if (gpu_format == GL_RGB && gpu_type == GL_UNSIGNED_SHORT &&
            gles_color_storage_format((GLenum)internalformat) != 0u) {
            gpu_owned = rgb16_to_rgb8(gpu_data, (size_t)width * (size_t)height * (size_t)depth);
            gpu_type = GL_UNSIGNED_BYTE;
        } else if (format == GL_RGBA && type == GL_UNSIGNED_BYTE &&
                   internalformat == (GLint)GL_RGBA16 && rgba16_gpu_native(ctx)) {
            gpu_owned = depth8_to_depth16(owned, data_size);
            gpu_type = GL_UNSIGNED_SHORT;
        } else if (format == GL_RGBA && type == GL_UNSIGNED_BYTE &&
                   internalformat == (GLint)GL_RGBA16F) {
            gpu_owned = rgba8_to_half_float(owned, data_size);
            gpu_type = GL_HALF_FLOAT;
        } else if (internalformat == (GLint)GL_DEPTH_COMPONENT32 && format == GL_DEPTH_COMPONENT &&
                   type != GL_FLOAT) {
            gpu_owned =
                depth_integer_to_float(owned, (size_t)width * (size_t)height * (size_t)depth, type);
            gpu_type = GL_FLOAT;
        } else if (format == GL_DEPTH_COMPONENT && type == GL_UNSIGNED_BYTE) {
            gpu_owned = depth8_to_depth16(owned, data_size);
            gpu_type = GL_UNSIGNED_SHORT;
            gpu_internal = (GLint)GL_DEPTH_COMPONENT16;
        } else if (format == GL_RGBA && type == GL_UNSIGNED_SHORT &&
                   (internalformat != (GLint)GL_RGBA16 || !rgba16_gpu_native(ctx))) {
            gpu_owned = rgba16_to_rgba8(owned, data_size);
            gpu_type = GL_UNSIGNED_BYTE;
        }
        if (gpu_owned == NULL && gpu_type != type) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        if (gpu_owned != NULL)
            gpu_data = gpu_owned;
        if (internalformat == (GLint)GL_RGB16F || internalformat == (GLint)GL_RGB32F_ARB) {
            if (gpu_type == GL_UNSIGNED_BYTE && (gpu_format == GL_RGB || gpu_format == GL_RGBA)) {
                GLenum float_type =
                    native_storage_internalformat(ctx, (GLenum)internalformat) == GL_RGBA16F
                        ? GL_HALF_FLOAT
                        : GL_FLOAT;
                unsigned char *expanded = repack_color_byte_float_rgba(
                    gpu_data, (size_t)width * (size_t)height * (size_t)depth, gpu_format,
                    float_type);
                if (expanded == NULL) {
                    free(gpu_owned);
                    mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                    return 0;
                }
                free(gpu_owned);
                gpu_owned = expanded;
                gpu_data = expanded;
                gpu_format = GL_RGBA;
                gpu_type = float_type;
            }
        }
        if ((internalformat == (GLint)GL_RGB16F || internalformat == (GLint)GL_RGB32F_ARB) &&
            gpu_format == GL_RGB && (gpu_type == GL_FLOAT || gpu_type == GL_HALF_FLOAT)) {
            unsigned char *expanded = repack_rgb_float_rgba(
                gpu_data, (size_t)width * (size_t)height * (size_t)depth, gpu_type);
            if (expanded == NULL) {
                free(gpu_owned);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            free(gpu_owned);
            gpu_owned = expanded;
            gpu_data = expanded;
            gpu_format = GL_RGBA;
        }
        if (gpu_type == GL_UNSIGNED_BYTE && (gpu_format == GL_RGB || gpu_format == GL_RGBA)) {
            GLenum required = gles_color_storage_format((GLenum)internalformat);
            if (required != 0u && required != gpu_format) {
                repacked = repack_rgb_rgba_for_gles(
                    gpu_data, (size_t)width * (size_t)height * (size_t)depth, gpu_format, required);
                if (repacked == NULL) {
                    free(gpu_owned);
                    mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                    return 0;
                }
                gpu_data = repacked;
                gpu_format = required;
            }
        }
        if (internalformat == (GLint)GL_R8 || internalformat == (GLint)GL_RG8 ||
            internalformat == (GLint)GL_R16F || internalformat == (GLint)GL_RG16F ||
            narrow_storage_format((GLenum)internalformat) != (GLenum)internalformat) {
            unsigned char *narrow = repack_narrow_color_upload(
                gpu_data, (size_t)width * (size_t)height * (size_t)depth, gpu_format, gpu_type,
                narrow_storage_format((GLenum)internalformat), &gpu_format, &gpu_type);
            if (narrow == NULL) {
                free(repacked);
                free(gpu_owned);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            free(repacked);
            free(gpu_owned);
            repacked = NULL;
            gpu_owned = narrow;
            gpu_data = narrow;
        }
        const int use_pbo = native_upload_can_use_pbo(
            ctx, pbo_source, original_format, original_type, gpu_format, gpu_type, data, gpu_data);
        native_upload_begin_source(ctx, use_pbo);
        ctx->native_dispatch->glesTexImage3D(
            target, level, gpu_internal, width, height, depth, border, gpu_format, gpu_type,
            (use_pbo ? (const void *)(uintptr_t)pbo_offset : gpu_data));
        native_upload_end(ctx);
        free(repacked);
        free(gpu_owned);
    }
    native_apply_legacy_luminance_swizzle(ctx, target, (GLenum)internalformat,
                                          previous_internalformat);
    return 1;
}

int mgl_texture_apply_image_3d_tight(struct mgl_context *ctx, GLenum target, GLint level,
                                     GLint internalformat, GLsizei width, GLsizei height,
                                     GLsizei depth, GLint border, GLenum format, GLenum type,
                                     const void *data, size_t data_size) {
    return mgl_texture_apply_image_3d_tight_impl(ctx, target, level, internalformat, width, height,
                                                 depth, border, format, type, data, data_size, 0,
                                                 0u);
}

static int mgl_texture_apply_sub_image_3d_tight_impl(struct mgl_context *ctx, GLenum target,
                                                     GLint level, GLint xoffset, GLint yoffset,
                                                     GLint zoffset, GLsizei width, GLsizei height,
                                                     GLsizei depth, GLenum format, GLenum type,
                                                     const void *data, size_t data_size,
                                                     int pbo_source, size_t pbo_offset) {
    const GLenum original_format = format, original_type = type;
    struct mgl_texture *obj;
    struct mgl_texture_level *dst;
    size_t components = 0u, dst_components = 0u, expected = 0u;
    size_t row_bytes, src_slice, dst_slice;
    GLsizei z, y;
    unsigned char *normalized = NULL, *converted = NULL;
    if (ctx == NULL)
        return 0;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (!valid_3d_target(target) || !valid_format_type(format, type, &components)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0 || depth < 0 ||
        !checked_mul((size_t)width, components, &expected) ||
        !checked_mul(expected, (size_t)height, &expected) ||
        !checked_mul(expected, (size_t)depth, &expected) || expected != data_size) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (needs_color_normalization(format, type)) {
        normalized = malloc(data_size != 0u ? data_size : 1u);
        if (normalized == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        if (data_size != 0u)
            memcpy(normalized, data, data_size);
        if (!normalize_upload_color(&normalized, &data_size, &format, &type,
                                    texture_upload_level_internalformat(ctx, target, level))) {
            free(normalized);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        normalize_tight_color(normalized, data_size, &format, &type);
        if (!valid_format_type(format, type, &components)) {
            free(normalized);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        data = normalized;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL || !obj->realized || !obj->levels[level].defined) {
        free(normalized);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    dst = &obj->levels[level];
    if (dst->compressed || !valid_format_type(dst->format, dst->type, &dst_components) ||
        !subimage_format_compatible(dst->internal_format, format)) {
        free(normalized);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (xoffset > dst->width || yoffset > dst->height || zoffset > dst->depth ||
        width > dst->width - xoffset || height > dst->height - yoffset ||
        depth > dst->depth - zoffset) {
        free(normalized);
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesTexSubImage3D == NULL) {
        free(converted);
        free(normalized);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (!subimage_prepare_shadow(dst, data, (size_t)width * (size_t)height * (size_t)depth, format,
                                 type, components, &converted, &dst_components)) {
        free(normalized);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    row_bytes = (size_t)width * dst_components;
    src_slice = row_bytes * (size_t)height;
    dst_slice = (size_t)dst->width * (size_t)dst->height * dst_components;
    for (z = 0; z < depth; ++z) {
        for (y = 0; y < height; ++y) {
            size_t dst_off = ((size_t)zoffset + (size_t)z) * dst_slice +
                             ((size_t)yoffset + (size_t)y) * (size_t)dst->width * dst_components +
                             (size_t)xoffset * dst_components;
            size_t src_off = (size_t)z * src_slice + (size_t)y * row_bytes;
            const unsigned char *patch = converted != NULL ? converted : data;
            memcpy(dst->data + dst_off, patch + src_off, row_bytes);
        }
    }
    {
        GLenum gpu_format = native_storage_external_format(dst->format);
        GLenum gpu_type = dst->type;
        const void *gpu_data = converted != NULL ? converted : data;
        size_t gpu_bytes = (size_t)width * (size_t)height * (size_t)depth * dst_components;
        unsigned char *gpu_owned = NULL;
        unsigned char *repacked = NULL;
        if (gpu_format == GL_RGB && gpu_type == GL_UNSIGNED_SHORT &&
            gles_color_storage_format(dst->internal_format) != 0u) {
            gpu_owned = rgb16_to_rgb8(gpu_data, (size_t)width * (size_t)height * (size_t)depth);
            gpu_type = GL_UNSIGNED_BYTE;
        } else if (dst->format == GL_RGBA && dst->type == GL_UNSIGNED_BYTE &&
                   dst->internal_format == GL_RGBA16 && rgba16_gpu_native(ctx)) {
            gpu_owned = depth8_to_depth16(gpu_data, gpu_bytes);
            gpu_type = GL_UNSIGNED_SHORT;
        } else if (dst->format == GL_RGBA && dst->type == GL_UNSIGNED_BYTE &&
                   dst->internal_format == GL_RGBA16F) {
            gpu_owned = rgba8_to_half_float(gpu_data, gpu_bytes);
            gpu_type = GL_HALF_FLOAT;
        } else if (dst->internal_format == GL_DEPTH_COMPONENT32 &&
                   dst->format == GL_DEPTH_COMPONENT && dst->type != GL_FLOAT) {
            gpu_owned = depth_integer_to_float(
                gpu_data, (size_t)width * (size_t)height * (size_t)depth, dst->type);
            gpu_type = GL_FLOAT;
        } else if (dst->format == GL_DEPTH_COMPONENT && dst->type == GL_UNSIGNED_BYTE) {
            gpu_owned = depth8_to_depth16(gpu_data, gpu_bytes);
            gpu_type = GL_UNSIGNED_SHORT;
        } else if (dst->format == GL_RGBA && dst->type == GL_UNSIGNED_SHORT &&
                   (dst->internal_format != GL_RGBA16 || !rgba16_gpu_native(ctx))) {
            gpu_owned = rgba16_to_rgba8(gpu_data, gpu_bytes);
            gpu_type = GL_UNSIGNED_BYTE;
        }
        if (gpu_owned == NULL && gpu_type != dst->type) {
            free(converted);
            free(normalized);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        if (gpu_owned != NULL)
            gpu_data = gpu_owned;
        if (dst->internal_format == GL_RGB16F || dst->internal_format == GL_RGB32F_ARB) {
            if (gpu_type == GL_UNSIGNED_BYTE && (gpu_format == GL_RGB || gpu_format == GL_RGBA)) {
                GLenum float_type =
                    native_storage_internalformat(ctx, dst->internal_format) == GL_RGBA16F
                        ? GL_HALF_FLOAT
                        : GL_FLOAT;
                unsigned char *expanded = repack_color_byte_float_rgba(
                    gpu_data, (size_t)width * (size_t)height * (size_t)depth, gpu_format,
                    float_type);
                if (expanded == NULL) {
                    free(gpu_owned);
                    free(converted);
                    free(normalized);
                    mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                    return 0;
                }
                free(gpu_owned);
                gpu_owned = expanded;
                gpu_data = expanded;
                gpu_format = GL_RGBA;
                gpu_type = float_type;
            }
        }
        if ((dst->internal_format == GL_RGB16F || dst->internal_format == GL_RGB32F_ARB) &&
            gpu_format == GL_RGB && (gpu_type == GL_FLOAT || gpu_type == GL_HALF_FLOAT)) {
            unsigned char *expanded = repack_rgb_float_rgba(
                gpu_data, (size_t)width * (size_t)height * (size_t)depth, gpu_type);
            if (expanded == NULL) {
                free(gpu_owned);
                free(converted);
                free(normalized);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            free(gpu_owned);
            gpu_owned = expanded;
            gpu_data = expanded;
            gpu_format = GL_RGBA;
        }
        if (gpu_type == GL_UNSIGNED_BYTE && (gpu_format == GL_RGB || gpu_format == GL_RGBA)) {
            GLenum required = gles_color_storage_format(dst->internal_format);
            if (required != 0u && required != gpu_format) {
                repacked = repack_rgb_rgba_for_gles(
                    gpu_data, (size_t)width * (size_t)height * (size_t)depth, gpu_format, required);
                if (repacked == NULL) {
                    free(converted);
                    free(normalized);
                    free(gpu_owned);
                    mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                    return 0;
                }
                gpu_data = repacked;
                gpu_format = required;
            }
        }
        if (dst->internal_format == GL_R8 || dst->internal_format == GL_RG8 ||
            dst->internal_format == GL_R16F || dst->internal_format == GL_RG16F ||
            narrow_storage_format(dst->internal_format) != dst->internal_format) {
            unsigned char *narrow = repack_narrow_color_upload(
                gpu_data, (size_t)width * (size_t)height * (size_t)depth, gpu_format, gpu_type,
                narrow_storage_format(dst->internal_format), &gpu_format, &gpu_type);
            if (narrow == NULL) {
                free(repacked);
                free(gpu_owned);
                free(converted);
                free(normalized);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            free(repacked);
            free(gpu_owned);
            repacked = NULL;
            gpu_owned = narrow;
            gpu_data = narrow;
        }
        const int use_pbo = native_upload_can_use_pbo(
            ctx, pbo_source, original_format, original_type, gpu_format, gpu_type, data, gpu_data);
        native_upload_begin_source(ctx, use_pbo);
        ctx->native_dispatch->glesTexSubImage3D(
            target, level, xoffset, yoffset, zoffset, width, height, depth, gpu_format, gpu_type,
            (use_pbo ? (const void *)(uintptr_t)pbo_offset : gpu_data));
        native_upload_end(ctx);
        free(repacked);
        free(gpu_owned);
    }
    free(converted);
    free(normalized);
    return 1;
}

int mgl_texture_apply_sub_image_3d_tight(struct mgl_context *ctx, GLenum target, GLint level,
                                         GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,
                                         GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                         const void *data, size_t data_size) {
    return mgl_texture_apply_sub_image_3d_tight_impl(ctx, target, level, xoffset, yoffset, zoffset,
                                                     width, height, depth, format, type, data,
                                                     data_size, 0, 0u);
}

void glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                  GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glTexImage3D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"depth", (uint64_t)(uintptr_t)depth},
                                       {"border", (uint64_t)(uintptr_t)border},
                                       {"format", (uint64_t)(uintptr_t)format}},
        8u);
    size_t components = 0u, data_size = 0u;
    unsigned char *data;
    int suppress;
    if (ctx == NULL)
        return;
    if (invalid_packed_upload_pair(format, type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }

    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_tex_image_3d(ctx, target, level, internalformat, width, height, depth,
                                         border, format, type, NULL, 0u))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_3d_target(target) || !valid_format_type(format, type, &components) ||
        !valid_internal(internalformat) || !format_matches_internal(format, internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        depth < 0 || border != 0 ||
        (target == GL_TEXTURE_CUBE_MAP_ARRAY &&
         (width != height || depth <= 0 || depth % 6 != 0))) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!needs_color_normalization(format, type) && format != GL_COLOR_INDEX &&
        !color_transfer_active(&ctx->pixel_transfer)) {
        const unsigned char *direct = tight_borrow_3d(ctx, pixels, width, height, depth, components,
                                                      pixel_element_size(type), &data_size);
        if (direct != NULL) {
            suppress =
                mgl_list_record_tex_image_3d(ctx, target, level, internalformat, width, height,
                                             depth, border, format, type, direct, data_size);
            if (!suppress) {
                size_t offset = 0u;
                int pbo_source =
                    texture_pbo_source_offset(ctx, direct, pixel_element_size(type), &offset);
                (void)mgl_texture_apply_image_3d_tight_impl(
                    ctx, target, level, internalformat, width, height, depth, border, format, type,
                    direct, data_size, pbo_source, offset);
            }
            return;
        }
    }
    data = tight_copy_3d(ctx, pixels, width, height, depth, components, pixel_element_size(type),
                         &data_size);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (needs_color_normalization(format, type)) {
        if (!normalize_upload_color(&data, &data_size, &format, &type, (GLenum)internalformat)) {
            free(data);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        normalize_tight_color(data, data_size, &format, &type);
    }
    if ((pixels != NULL || ctx->pixel_unpack_buffer != 0u) &&
        !transform_texture_color(ctx, &data, &data_size, &format, &type, 0)) {
        free(data);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    suppress = mgl_list_record_tex_image_3d(ctx, target, level, internalformat, width, height,
                                            depth, border, format, type, data, data_size);
    if (!suppress)
        (void)mgl_texture_apply_image_3d_tight(ctx, target, level, internalformat, width, height,
                                               depth, border, format, type, data, data_size);
    free(data);
}

void glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glTexStorage1D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"levels", (uint64_t)(uintptr_t)levels},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width}},
        4u);
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_1D) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)texture_storage_2d(ctx, obj, target, levels, internalformat, width, 1);
}

void glTextureView(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat,
                   GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glTextureView",
        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                       {"target", (uint64_t)(uintptr_t)target},
                                       {"origtexture", (uint64_t)(uintptr_t)origtexture},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"minlevel", (uint64_t)(uintptr_t)minlevel},
                                       {"numlevels", (uint64_t)(uintptr_t)numlevels},
                                       {"minlayer", (uint64_t)(uintptr_t)minlayer},
                                       {"numlayers", (uint64_t)(uintptr_t)numlayers}},
        8u);
    struct mgl_texture *view, *source, *root;
    struct mgl_texture_level *reference;
    GLuint source_level, source_layer, available_layers, available_levels, level, face;
    int array_source;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_2D && target != GL_TEXTURE_2D_ARRAY &&
        target != GL_TEXTURE_CUBE_MAP && target != GL_TEXTURE_CUBE_MAP_ARRAY) {
        mgl_error_set(&ctx->errors,
                      valid_bind_target(target) ? GL_INVALID_OPERATION : GL_INVALID_ENUM);
        return;
    }
    if (texture == 0u || origtexture == 0u || texture == origtexture) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    source = mgl_texture_find(ctx, origtexture);
    view = mgl_texture_find(ctx, texture);
    array_source = source != NULL && (source->target == GL_TEXTURE_2D_ARRAY ||
                                      source->target == GL_TEXTURE_CUBE_MAP_ARRAY);
    if (source == NULL || !source->realized ||
        (source->target != target &&
         !(array_source && (target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP ||
                             target == GL_TEXTURE_2D_ARRAY ||
                             target == GL_TEXTURE_CUBE_MAP_ARRAY)) &&
         !(source->target == GL_TEXTURE_CUBE_MAP &&
           (target == GL_TEXTURE_2D || target == GL_TEXTURE_2D_ARRAY)) &&
         !(source->target == GL_TEXTURE_2D && target == GL_TEXTURE_2D_ARRAY)) ||
        !source->immutable_format || view == NULL || view->realized || view->target != 0u) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    available_levels =
        source->view_root != NULL ? source->view_num_levels : (GLuint)source->immutable_levels;
    if (minlevel >= available_levels) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (numlevels > available_levels - minlevel)
        numlevels = available_levels - minlevel;
    if (numlevels == 0u) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    reference = source->target == GL_TEXTURE_CUBE_MAP ? &source->cube_levels[0][minlevel]
                                                      : &source->levels[minlevel];
    /* Software-decoded RGTC and BC6H cannot reinterpret signed compression endpoints through a
     * native texture view. */
    if (!mgl_texture_view_formats_compatible(reference->internal_format, internalformat) ||
        (reference->compressed && !source->native_compressed_storage &&
         ((mgl_texture_view_format_class(internalformat) == GL_VIEW_CLASS_RGTC1_RED ||
           mgl_texture_view_format_class(internalformat) == GL_VIEW_CLASS_RGTC2_RG ||
           mgl_texture_view_format_class(internalformat) == GL_VIEW_CLASS_BPTC_FLOAT) &&
          internalformat != reference->internal_format))) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    available_layers = array_source ? (GLuint)reference->depth
                       : source->target == GL_TEXTURE_CUBE_MAP ? 6u : 1u;
    if (minlayer >= available_layers || (target == GL_TEXTURE_2D && numlayers != 1u)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (numlayers > available_layers - minlayer)
        numlayers = available_layers - minlayer;
    if (numlayers == 0u) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((target == GL_TEXTURE_CUBE_MAP && numlayers != 6u) ||
        (target == GL_TEXTURE_CUBE_MAP_ARRAY && numlayers % 6u != 0u)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if ((target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY) &&
        reference->width != reference->height) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!source->native_immutable_storage || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesTextureView == NULL ||
        (!mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_EXT_texture_view") &&
         !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_OES_texture_view")) ||
        ctx->native_dispatch->glesGenTextures == NULL) {
        unsigned extension =
            ctx->native_dispatch != NULL &&
            (mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_EXT_texture_view") ||
             mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_OES_texture_view"));
        unsigned entrypoints = ctx->native_dispatch != NULL &&
                               ctx->native_dispatch->glesTextureView != NULL &&
                               ctx->native_dispatch->glesGenTextures != NULL;
        mgl_render_diag_detail(ctx, "glTextureView", "native_capability_immutable",
                               (extension << 1u) | entrypoints, source->native_immutable_storage);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesGenTextures(1, &view->native_name);
    if (view->native_name == 0u) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    ctx->native_dispatch->glesTextureView(view->native_name, target, source->native_name,
                                          reference->compressed &&
                                                  !source->native_compressed_storage
                                              ? compressed_fallback_internalformat(internalformat)
                                              : internalformat,
                                          minlevel, numlevels, minlayer, numlayers);
    if (ctx->native_dispatch->glesGetError != NULL) {
        GLenum error = ctx->native_dispatch->glesGetError();
        if (error != GL_NO_ERROR) {
            if (ctx->native_dispatch->glesDeleteTextures != NULL)
                ctx->native_dispatch->glesDeleteTextures(1, &view->native_name);
            view->native_name = 0u;
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, error);
            return;
        }
    }
    root = source->view_root != NULL ? source->view_root : source;
    source_level = (source->view_root != NULL ? source->view_min_level : 0u) + minlevel;
    source_layer = (source->view_root != NULL ? source->view_min_layer : 0u) + minlayer;
    view->view_root = root;
    view->view_min_level = source_level;
    view->view_num_levels = numlevels;
    view->view_min_layer = source_layer;
    view->view_num_layers = numlayers;
    ++root->view_refcount;
    view->target = target;
    view->realized = GL_TRUE;
    view->immutable_format = GL_TRUE;
    view->immutable_levels = source->immutable_levels;
    view->native_immutable_storage = GL_TRUE;
    view->native_compressed_storage = source->native_compressed_storage;
    for (level = 0u; level < numlevels; ++level) {
        if (target == GL_TEXTURE_CUBE_MAP) {
            for (face = 0u; face < 6u; ++face) {
                struct mgl_texture_level *dst = &view->cube_levels[face][level];
                if (root->target == GL_TEXTURE_2D_ARRAY ||
                root->target == GL_TEXTURE_CUBE_MAP_ARRAY) {
                    const struct mgl_texture_level *src = &root->levels[source_level + level];
                    size_t slice_bytes = src->data_size / (size_t)src->depth;
                    *dst = *src;
                    dst->internal_format = internalformat;
                    dst->data = src->data + (source_layer + face) * slice_bytes;
                    dst->data_size = slice_bytes;
                    dst->depth = 1;
                } else {
                    *dst = root->cube_levels[source_layer + face][source_level + level];
                    dst->internal_format = internalformat;
                }
            }
        } else {
            struct mgl_texture_level *dst = &view->levels[level];
            if (root->target == GL_TEXTURE_CUBE_MAP)
                *dst = root->cube_levels[source_layer][source_level + level];
            else
                *dst = root->levels[source_level + level];
            dst->internal_format = internalformat;
            if (root->target == GL_TEXTURE_2D_ARRAY ||
                root->target == GL_TEXTURE_CUBE_MAP_ARRAY) {
                size_t slice_bytes = dst->data_size / (size_t)dst->depth;
                dst->data += source_layer * slice_bytes;
                dst->data_size = (size_t)numlayers * slice_bytes;
                dst->depth = (GLsizei)numlayers;
            }
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                    GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glTexStorage2D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"levels", (uint64_t)(uintptr_t)levels},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height}},
        5u);
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_2D && target != GL_TEXTURE_CUBE_MAP) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)texture_storage_2d(ctx, obj, target, levels, internalformat, width, height);
}

void glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                    GLsizei height, GLsizei depth) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glTexStorage3D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"levels", (uint64_t)(uintptr_t)levels},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"depth", (uint64_t)(uintptr_t)depth}},
        6u);
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_3d_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)texture_storage_3d(ctx, obj, target, levels, internalformat, width, height, depth);
}

void glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || obj->target != GL_TEXTURE_1D) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    dsa_native_bind_texture(ctx, obj);
    (void)texture_storage_2d(ctx, obj, GL_TEXTURE_1D, levels, internalformat, width, 1);
    dsa_native_restore_texture(ctx, GL_TEXTURE_1D);
}

void glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width,
                        GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || (obj->target != GL_TEXTURE_2D && obj->target != GL_TEXTURE_CUBE_MAP)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    dsa_native_bind_texture(ctx, obj);
    (void)texture_storage_2d(ctx, obj, obj->target, levels, internalformat, width, height);
    dsa_native_restore_texture(ctx, obj->target);
}

void glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width,
                        GLsizei height, GLsizei depth) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || !valid_3d_target(obj->target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    dsa_native_bind_texture(ctx, obj);
    (void)texture_storage_3d(ctx, obj, obj->target, levels, internalformat, width, height, depth);
    dsa_native_restore_texture(ctx, obj->target);
}

void glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                     GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                     const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTexSubImage3D",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"zoffset", (uint64_t)(uintptr_t)zoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height},
                                                       {"depth", (uint64_t)(uintptr_t)depth}},
                        8u);
    size_t components = 0u, data_size = 0u;
    unsigned char *data;
    int suppress;
    if (ctx == NULL)
        return;
    if (invalid_packed_upload_pair(format, type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }

    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_tex_sub_image_3d(ctx, target, level, xoffset, yoffset, zoffset, width,
                                             height, depth, format, type, NULL, 0u))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_3d_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0 || depth < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (angle_native_sub_image(ctx, target, level, xoffset, yoffset, zoffset, width, height, depth,
                               format, type, pixels))
        return;
    if (!valid_format_type(format, type, &components)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!needs_color_normalization(format, type) && format != GL_COLOR_INDEX &&
        !color_transfer_active(&ctx->pixel_transfer)) {
        const unsigned char *direct = tight_borrow_3d(ctx, pixels, width, height, depth, components,
                                                      pixel_element_size(type), &data_size);
        if (direct != NULL) {
            suppress = mgl_list_record_tex_sub_image_3d(ctx, target, level, xoffset, yoffset,
                                                        zoffset, width, height, depth, format, type,
                                                        direct, data_size);
            if (!suppress) {
                size_t offset = 0u;
                int pbo_source =
                    texture_pbo_source_offset(ctx, direct, pixel_element_size(type), &offset);
                (void)mgl_texture_apply_sub_image_3d_tight_impl(
                    ctx, target, level, xoffset, yoffset, zoffset, width, height, depth, format,
                    type, direct, data_size, pbo_source, offset);
            }
            return;
        }
    }
    data = tight_copy_3d(ctx, pixels, width, height, depth, components, pixel_element_size(type),
                         &data_size);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (needs_color_normalization(format, type)) {
        if (!normalize_upload_color(&data, &data_size, &format, &type,
                                    texture_upload_level_internalformat(ctx, target, level))) {
            free(data);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        normalize_tight_color(data, data_size, &format, &type);
    }
    if (!transform_texture_color(ctx, &data, &data_size, &format, &type, 0)) {
        free(data);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    suppress =
        mgl_list_record_tex_sub_image_3d(ctx, target, level, xoffset, yoffset, zoffset, width,
                                         height, depth, format, type, data, data_size);
    if (!suppress)
        (void)mgl_texture_apply_sub_image_3d_tight(ctx, target, level, xoffset, yoffset, zoffset,
                                                   width, height, depth, format, type, data,
                                                   data_size);
    free(data);
}

static int compressed_format_is_rgtc(GLenum format) {
    return format == GL_COMPRESSED_RED_RGTC1 || format == GL_COMPRESSED_SIGNED_RED_RGTC1 ||
           format == GL_COMPRESSED_RG_RGTC2 || format == GL_COMPRESSED_SIGNED_RG_RGTC2;
}

static int compressed_format_is_signed_rgtc(GLenum format) {
    return format == GL_COMPRESSED_SIGNED_RED_RGTC1 || format == GL_COMPRESSED_SIGNED_RG_RGTC2;
}

static unsigned compressed_format_components(GLenum format) {
    return format == GL_COMPRESSED_RG_RGTC2 || format == GL_COMPRESSED_SIGNED_RG_RGTC2 ? 2u : 1u;
}

static size_t compressed_block_bytes(GLenum format) {
    switch (format) {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RED_RGTC1:
        case GL_COMPRESSED_SIGNED_RED_RGTC1:
            return 8u;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_RGBA_BPTC_UNORM:
        case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:
        case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT:
        case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:
        case GL_COMPRESSED_RG_RGTC2:
        case GL_COMPRESSED_SIGNED_RG_RGTC2:
            return 16u;
        default:
            return 0u;
    }
}

static int compressed_image_size(GLenum format, GLsizei width, GLsizei height, size_t *out_size);

static int compressed_format_is_bptc(GLenum format) {
    return format == GL_COMPRESSED_RGBA_BPTC_UNORM ||
           format == GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM ||
           format == GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT ||
           format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
}

static GLenum compressed_fallback_internalformat(GLenum format) {
    if (format == GL_COMPRESSED_RED_RGTC1)
        return GL_R8;
    if (format == GL_COMPRESSED_SIGNED_RED_RGTC1)
        return GL_R8_SNORM;
    if (format == GL_COMPRESSED_RG_RGTC2)
        return GL_RG8;
    if (format == GL_COMPRESSED_SIGNED_RG_RGTC2)
        return GL_RG8_SNORM;
    if (format == GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT ||
        format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT)
        return GL_RGB16F;
    if (format == GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM ||
        format == GL_COMPRESSED_SRGB_S3TC_DXT1_EXT ||
        format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT ||
        format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT ||
        format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT)
        return GL_SRGB8_ALPHA8;
    return compressed_block_bytes(format) != 0u ? GL_RGBA8 : 0u;
}

static int compressed_fallback_pixel_format(GLenum format, GLenum *external, GLenum *type) {
    if (compressed_block_bytes(format) == 0u)
        return 0;
    if (compressed_format_is_rgtc(format)) {
        *external = compressed_format_components(format) == 2u ? GL_RG : GL_RED;
        *type = compressed_format_is_signed_rgtc(format) ? GL_BYTE : GL_UNSIGNED_BYTE;
    } else if (format == GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT ||
               format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT) {
        *external = GL_RGB;
        *type = GL_HALF_FLOAT;
    } else {
        *external = GL_RGBA;
        *type = GL_UNSIGNED_BYTE;
    }
    return 1;
}

static int compressed_format_native(const struct mgl_native_dispatch *native, GLenum format) {
    if (native == NULL)
        return 0;
    if (format == GL_COMPRESSED_SRGB_S3TC_DXT1_EXT ||
        format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT ||
        format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT ||
        format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT)
        return mgl_native_dispatch_has_gles_extension(native,
                                                      "GL_EXT_texture_compression_s3tc_srgb");
    if (compressed_format_is_bptc(format))
        return mgl_native_dispatch_has_gles_extension(native, "GL_EXT_texture_compression_bptc") ||
               mgl_native_dispatch_has_gles_extension(native, "GL_ARB_texture_compression_bptc");
    if (format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT || format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
        return mgl_native_dispatch_has_gles_extension(native, "GL_EXT_texture_compression_s3tc") ||
               mgl_native_dispatch_has_gles_extension(native, "GL_S3_s3tc") ||
               mgl_native_dispatch_has_gles_extension(native, "GL_EXT_texture_compression_dxt1");
    if (format == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT)
        return mgl_native_dispatch_has_gles_extension(native, "GL_EXT_texture_compression_s3tc") ||
               mgl_native_dispatch_has_gles_extension(native, "GL_S3_s3tc") ||
               mgl_native_dispatch_has_gles_extension(native, "GL_ANGLE_texture_compression_dxt3");
    if (format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
        return mgl_native_dispatch_has_gles_extension(native, "GL_EXT_texture_compression_s3tc") ||
               mgl_native_dispatch_has_gles_extension(native, "GL_S3_s3tc") ||
               mgl_native_dispatch_has_gles_extension(native, "GL_ANGLE_texture_compression_dxt5");
    if (compressed_format_is_rgtc(format))
        return mgl_native_dispatch_has_gles_extension(native, "GL_EXT_texture_compression_rgtc");
    return 0;
}

static unsigned char *decode_bcn_image(GLenum format, GLsizei width, GLsizei height,
                                       const unsigned char *data, size_t *out_size) {
    unsigned char *out;
    size_t blocks_x, blocks_y, pixel_bytes, image_bytes;
    size_t by, bx;
    int half_float = format == GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT ||
                     format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
    if (width <= 0 || height <= 0 || data == NULL || out_size == NULL)
        return NULL;
    if (!compressed_format_is_bptc(format) && format != GL_COMPRESSED_RGB_S3TC_DXT1_EXT &&
        format != GL_COMPRESSED_RGBA_S3TC_DXT1_EXT && format != GL_COMPRESSED_RGBA_S3TC_DXT3_EXT &&
        format != GL_COMPRESSED_RGBA_S3TC_DXT5_EXT && format != GL_COMPRESSED_SRGB_S3TC_DXT1_EXT &&
        format != GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT &&
        format != GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT &&
        format != GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT)
        return NULL;
    pixel_bytes = half_float ? 6u : 4u;
    if (!checked_mul((size_t)width, (size_t)height, &image_bytes) ||
        !checked_mul(image_bytes, pixel_bytes, &image_bytes))
        return NULL;
    out = malloc(image_bytes);
    if (out == NULL)
        return NULL;
    blocks_x = ((size_t)width + 3u) / 4u;
    blocks_y = ((size_t)height + 3u) / 4u;
    for (by = 0u; by < blocks_y; ++by) {
        for (bx = 0u; bx < blocks_x; ++bx) {
            unsigned char block_pixels[4u * 4u * 6u];
            const unsigned char *block =
                data + (by * blocks_x + bx) * compressed_block_bytes(format);
            size_t py, px;
            if (format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ||
                format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ||
                format == GL_COMPRESSED_SRGB_S3TC_DXT1_EXT ||
                format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT)
                bcdec_bc1(block, block_pixels, 16);
            else if (format == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT ||
                     format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT)
                bcdec_bc2(block, block_pixels, 16);
            else if (format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT ||
                     format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT)
                bcdec_bc3(block, block_pixels, 16);
            else if (half_float)
                bcdec_bc6h_half(block, block_pixels, 12,
                                format == GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT);
            else
                bcdec_bc7(block, block_pixels, 16);
            for (py = 0u; py < 4u && by * 4u + py < (size_t)height; ++py) {
                for (px = 0u; px < 4u && bx * 4u + px < (size_t)width; ++px) {
                    unsigned char *dst =
                        out + ((by * 4u + py) * (size_t)width + bx * 4u + px) * pixel_bytes;
                    const unsigned char *src = block_pixels + (py * 4u + px) * pixel_bytes;
                    memcpy(dst, src, pixel_bytes);
                    if (format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ||
                        format == GL_COMPRESSED_SRGB_S3TC_DXT1_EXT)
                        dst[3] = 255u;
                }
            }
        }
    }
    *out_size = image_bytes;
    return out;
}

static int native_upload_bcn_image(struct mgl_context *ctx, GLenum target, GLint level,
                                   GLenum format, GLsizei width, GLsizei height,
                                   const unsigned char *data) {
    size_t decoded_size = 0u;
    unsigned char *decoded;
    GLint native_internal = GL_RGBA8;
    GLenum external_format = GL_RGBA, external_type = GL_UNSIGNED_BYTE;
    if (ctx == NULL || ctx->native_dispatch == NULL)
        return 1;
    if (compressed_format_native(ctx->native_dispatch, format) &&
        ctx->native_dispatch->glesCompressedTexImage2D != NULL) {
        size_t bytes = 0u;
        if (!compressed_image_size(format, width, height, &bytes))
            return 0;
        if (ctx->native_dispatch->glesCompressedTexImage2D == NULL)
            return 0;
        native_upload_begin(ctx);
        ctx->native_dispatch->glesCompressedTexImage2D(target, level, format, width, height, 0,
                                                       (GLsizei)bytes, data);
        native_upload_end(ctx);
        return 1;
    }
    if (ctx->native_dispatch->glesTexImage2D == NULL)
        return 1;
    decoded = decode_bcn_image(format, width, height, data, &decoded_size);
    if (decoded == NULL)
        return 0;
    if (format == GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM ||
        format == GL_COMPRESSED_SRGB_S3TC_DXT1_EXT ||
        format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT ||
        format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT ||
        format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT)
        native_internal = GL_SRGB8_ALPHA8;
    else if (format == GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT ||
             format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT) {
        native_internal = GL_RGB16F;
        external_format = GL_RGB;
        external_type = GL_HALF_FLOAT;
    }
    native_upload_begin(ctx);
    ctx->native_dispatch->glesTexImage2D(target, level, native_internal, width, height, 0,
                                         external_format, external_type, decoded);
    native_upload_end(ctx);
    free(decoded);
    return 1;
}

static int compressed_image_size_3d(GLenum format, GLsizei width, GLsizei height, GLsizei depth,
                                    size_t *out_size);

static int native_upload_bcn_image_3d(struct mgl_context *ctx, GLenum target, GLint level,
                                      GLenum format, GLsizei width, GLsizei height, GLsizei depth,
                                      const unsigned char *data) {
    size_t slice_size = 0u, decoded_slice_size = 0u, total_size = 0u, z;
    size_t pixel_bytes = (format == GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT ||
                          format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT)
                             ? 6u
                             : 4u;
    unsigned char *decoded;
    GLint internal = GL_RGBA8;
    GLenum external = GL_RGBA, type = GL_UNSIGNED_BYTE;
    if (ctx->native_dispatch == NULL)
        return 1;
    if ((target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY) &&
        compressed_format_native(ctx->native_dispatch, format) &&
        ctx->native_dispatch->glesCompressedTexImage3D != NULL) {
        size_t bytes;
        if (!compressed_image_size_3d(format, width, height, depth, &bytes))
            return 0;
        native_upload_begin(ctx);
        ctx->native_dispatch->glesCompressedTexImage3D(target, level, format, width, height, depth,
                                                       0, (GLsizei)bytes, data);
        native_upload_end(ctx);
        return 1;
    }
    if (ctx->native_dispatch->glesTexImage3D == NULL ||
        !compressed_image_size(format, width, height, &slice_size) ||
        !checked_mul((size_t)width, (size_t)height, &decoded_slice_size) ||
        !checked_mul(decoded_slice_size, pixel_bytes, &decoded_slice_size) ||
        !checked_mul(decoded_slice_size, (size_t)depth, &total_size))
        return 0;
    decoded = malloc(total_size != 0u ? total_size : 1u);
    if (decoded == NULL)
        return 0;
    for (z = 0u; z < (size_t)depth; ++z) {
        size_t size = 0u;
        unsigned char *slice =
            decode_bcn_image(format, width, height, data + z * slice_size, &size);
        if (slice == NULL && decoded_slice_size != 0u) {
            free(decoded);
            return 0;
        }
        if (size != decoded_slice_size && decoded_slice_size != 0u) {
            free(slice);
            free(decoded);
            return 0;
        }
        if (decoded_slice_size != 0u)
            memcpy(decoded + z * decoded_slice_size, slice, decoded_slice_size);
        free(slice);
    }
    if (format == GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM ||
        format == GL_COMPRESSED_SRGB_S3TC_DXT1_EXT ||
        format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT ||
        format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT ||
        format == GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT)
        internal = GL_SRGB8_ALPHA8;
    else if (pixel_bytes == 6u) {
        internal = GL_RGB16F;
        external = GL_RGB;
        type = GL_HALF_FLOAT;
    }
    native_upload_begin(ctx);
    ctx->native_dispatch->glesTexImage3D(target, level, internal, width, height, depth, 0, external,
                                         type, total_size != 0u ? decoded : NULL);
    native_upload_end(ctx);
    free(decoded);
    return 1;
}

static int compressed_image_size(GLenum format, GLsizei width, GLsizei height, size_t *out_size) {
    size_t block_bytes = compressed_block_bytes(format);
    size_t block_columns;
    size_t block_rows;
    size_t blocks;
    if (out_size == NULL || block_bytes == 0u || width < 0 || height < 0)
        return 0;
    block_columns = ((size_t)width + 3u) / 4u;
    block_rows = ((size_t)height + 3u) / 4u;
    if (!checked_mul(block_columns, block_rows, &blocks) ||
        !checked_mul(blocks, block_bytes, out_size))
        return 0;
    return 1;
}

static int compressed_image_size_3d(GLenum format, GLsizei width, GLsizei height, GLsizei depth,
                                    size_t *out_size) {
    size_t slice_size = 0u;
    if (depth < 0 || !compressed_image_size(format, width, height, &slice_size))
        return 0;
    return checked_mul(slice_size, (size_t)depth, out_size);
}

static int divide_round_nearest(int numerator, int denominator) {
    if (numerator >= 0)
        return (numerator + denominator / 2) / denominator;
    return -((-numerator + denominator / 2) / denominator);
}

static void decode_rgtc_channel(const unsigned char block[8], int signed_format,
                                signed char signed_values[16], unsigned char unsigned_values[16]) {
    uint64_t selectors = 0u;
    unsigned i;
    for (i = 0u; i < 6u; ++i)
        selectors |= (uint64_t)block[2u + i] << (8u * i);
    if (signed_format) {
        int raw0 = (int)(int8_t)block[0];
        int raw1 = (int)(int8_t)block[1];
        int v0 = raw0 == -128 ? -127 : raw0;
        int v1 = raw1 == -128 ? -127 : raw1;
        int palette[8];
        palette[0] = v0;
        palette[1] = v1;
        if (raw0 > raw1) {
            for (i = 2u; i < 8u; ++i)
                palette[i] = divide_round_nearest((int)(8u - i) * v0 + (int)(i - 1u) * v1, 7);
        } else {
            for (i = 2u; i < 6u; ++i)
                palette[i] = divide_round_nearest((int)(6u - i) * v0 + (int)(i - 1u) * v1, 5);
            palette[6] = -127;
            palette[7] = 127;
        }
        for (i = 0u; i < 16u; ++i)
            signed_values[i] = (signed char)palette[(selectors >> (3u * i)) & 7u];
    } else {
        unsigned v0 = block[0];
        unsigned v1 = block[1];
        unsigned palette[8];
        palette[0] = v0;
        palette[1] = v1;
        if (v0 > v1) {
            for (i = 2u; i < 8u; ++i)
                palette[i] = ((8u - i) * v0 + (i - 1u) * v1 + 3u) / 7u;
        } else {
            for (i = 2u; i < 6u; ++i)
                palette[i] = ((6u - i) * v0 + (i - 1u) * v1 + 2u) / 5u;
            palette[6] = 0u;
            palette[7] = 255u;
        }
        for (i = 0u; i < 16u; ++i)
            unsigned_values[i] = (unsigned char)palette[(selectors >> (3u * i)) & 7u];
    }
}

static unsigned char *decode_rgtc_image(GLenum format, GLsizei width, GLsizei height, GLsizei depth,
                                        const unsigned char *data, size_t *out_size) {
    unsigned components;
    size_t pixel_count = 0u, bytes = 0u;
    size_t blocks_x, blocks_y, block_bytes, slice_bytes;
    unsigned char *out;
    GLsizei z;
    size_t by, bx;
    int signed_format;
    if (out_size == NULL || data == NULL || !compressed_format_is_rgtc(format) || width < 0 ||
        height < 0 || depth < 0)
        return NULL;
    components = compressed_format_components(format);
    if (!checked_mul((size_t)width, (size_t)height, &pixel_count) ||
        !checked_mul(pixel_count, (size_t)depth, &pixel_count) ||
        !checked_mul(pixel_count, (size_t)components, &bytes))
        return NULL;
    out = calloc(bytes != 0u ? bytes : 1u, 1u);
    if (out == NULL)
        return NULL;
    blocks_x = ((size_t)width + 3u) / 4u;
    blocks_y = ((size_t)height + 3u) / 4u;
    block_bytes = compressed_block_bytes(format);
    if (!checked_mul(blocks_x, blocks_y, &slice_bytes) ||
        !checked_mul(slice_bytes, block_bytes, &slice_bytes)) {
        free(out);
        return NULL;
    }
    signed_format = compressed_format_is_signed_rgtc(format);
    for (z = 0; z < depth; ++z) {
        const unsigned char *slice = data + (size_t)z * slice_bytes;
        for (by = 0u; by < blocks_y; ++by) {
            for (bx = 0u; bx < blocks_x; ++bx) {
                const unsigned char *block = slice + (by * blocks_x + bx) * block_bytes;
                signed char signed_r[16], signed_g[16];
                unsigned char unsigned_r[16], unsigned_g[16];
                unsigned py, px;
                decode_rgtc_channel(block, signed_format, signed_r, unsigned_r);
                if (components == 2u)
                    decode_rgtc_channel(block + 8u, signed_format, signed_g, unsigned_g);
                for (py = 0u; py < 4u; ++py) {
                    size_t y = by * 4u + py;
                    if (y >= (size_t)height)
                        continue;
                    for (px = 0u; px < 4u; ++px) {
                        size_t x = bx * 4u + px;
                        size_t src_index = py * 4u + px;
                        size_t dst_index;
                        if (x >= (size_t)width)
                            continue;
                        dst_index =
                            (((size_t)z * (size_t)height + y) * (size_t)width + x) * components;
                        if (signed_format) {
                            out[dst_index] = (unsigned char)signed_r[src_index];
                            if (components == 2u)
                                out[dst_index + 1u] = (unsigned char)signed_g[src_index];
                        } else {
                            out[dst_index] = unsigned_r[src_index];
                            if (components == 2u)
                                out[dst_index + 1u] = unsigned_g[src_index];
                        }
                    }
                }
            }
        }
    }
    *out_size = bytes;
    return out;
}

static int native_upload_rgtc_image(struct mgl_context *ctx, GLenum target, GLint level,
                                    GLenum format, GLsizei width, GLsizei height, GLsizei depth,
                                    const unsigned char *data) {
    unsigned char *decoded;
    size_t decoded_size = 0u;
    GLenum external_format;
    GLenum external_type;
    GLint internal_format;
    if (ctx == NULL || ctx->native_dispatch == NULL)
        return 1;
    if (mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                               "GL_EXT_texture_compression_rgtc")) {
        size_t compressed_size = 0u;
        if (!compressed_image_size_3d(format, width, height, depth, &compressed_size))
            return 0;
        native_upload_begin(ctx);
        if (depth == 1 && valid_2d_image_target(target) &&
            ctx->native_dispatch->glesCompressedTexImage2D != NULL) {
            ctx->native_dispatch->glesCompressedTexImage2D(target, level, format, width, height, 0,
                                                           (GLsizei)compressed_size, data);
            native_upload_end(ctx);
            return 1;
        }
        if ((target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY) &&
            ctx->native_dispatch->glesCompressedTexImage3D != NULL) {
            ctx->native_dispatch->glesCompressedTexImage3D(
                target, level, format, width, height, depth, 0, (GLsizei)compressed_size, data);
            native_upload_end(ctx);
            return 1;
        }
        native_upload_end(ctx);
    }
    decoded = decode_rgtc_image(format, width, height, depth, data, &decoded_size);
    if (decoded == NULL)
        return 0;
    external_format = compressed_format_components(format) == 2u ? GL_RG : GL_RED;
    external_type = compressed_format_is_signed_rgtc(format) ? GL_BYTE : GL_UNSIGNED_BYTE;
    if (compressed_format_components(format) == 2u)
        internal_format = (GLint)(compressed_format_is_signed_rgtc(format) ? GL_RG8_SNORM : GL_RG8);
    else
        internal_format = (GLint)(compressed_format_is_signed_rgtc(format) ? GL_R8_SNORM : GL_R8);
    native_upload_begin(ctx);
    if (depth == 1 && valid_2d_image_target(target) && ctx->native_dispatch->glesTexImage2D != NULL)
        ctx->native_dispatch->glesTexImage2D(target, level, internal_format, width, height, 0,
                                             external_format, external_type,
                                             decoded_size != 0u ? decoded : NULL);
    else if ((target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY ||
              target == GL_TEXTURE_3D) &&
             ctx->native_dispatch->glesTexImage3D != NULL)
        ctx->native_dispatch->glesTexImage3D(target, level, internal_format, width, height, depth,
                                             0, external_format, external_type,
                                             decoded_size != 0u ? decoded : NULL);
    native_upload_end(ctx);
    free(decoded);
    return 1;
}

static unsigned char *decode_compressed_volume(GLenum format, GLsizei width, GLsizei height,
                                               GLsizei depth, const unsigned char *source,
                                               size_t *out_size) {
    size_t compressed_slice, decoded_slice, total, z;
    size_t pixel_bytes = (format == GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT ||
                          format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT)
                             ? 6u
                             : 4u;
    unsigned char *decoded;
    if (compressed_format_is_rgtc(format))
        return decode_rgtc_image(format, width, height, depth, source, out_size);
    if (!compressed_image_size(format, width, height, &compressed_slice) ||
        !checked_mul((size_t)width, (size_t)height, &decoded_slice) ||
        !checked_mul(decoded_slice, pixel_bytes, &decoded_slice) ||
        !checked_mul(decoded_slice, (size_t)depth, &total))
        return NULL;
    decoded = malloc(total != 0u ? total : 1u);
    if (decoded == NULL)
        return NULL;
    for (z = 0u; z < (size_t)depth; ++z) {
        size_t size = 0u;
        unsigned char *slice =
            decode_bcn_image(format, width, height, source + z * compressed_slice, &size);
        if (slice == NULL || size != decoded_slice) {
            free(slice);
            free(decoded);
            return NULL;
        }
        memcpy(decoded + z * decoded_slice, slice, decoded_slice);
        free(slice);
    }
    *out_size = total;
    return decoded;
}

static int native_immutable_compressed_sub_image_2d(struct mgl_context *ctx,
                                                    const struct mgl_texture *obj, GLenum target,
                                                    GLint level, GLint xoffset, GLint yoffset,
                                                    GLsizei width, GLsizei height, GLenum format,
                                                    const unsigned char *data, size_t data_size) {
    GLenum external, type;
    unsigned char *decoded;
    size_t decoded_size = 0u;
    if (ctx->native_dispatch == NULL || width == 0 || height == 0)
        return 1;
    if (obj->native_compressed_storage) {
        native_upload_begin(ctx);
        ctx->native_dispatch->glesCompressedTexSubImage2D(target, level, xoffset, yoffset, width,
                                                          height, format, (GLsizei)data_size, data);
        native_upload_end(ctx);
        return 1;
    }
    if (ctx->native_dispatch->glesTexSubImage2D == NULL ||
        !compressed_fallback_pixel_format(format, &external, &type))
        return 0;
    decoded = decode_compressed_volume(format, width, height, 1, data, &decoded_size);
    if (decoded == NULL)
        return 0;
    native_upload_begin(ctx);
    ctx->native_dispatch->glesTexSubImage2D(target, level, xoffset, yoffset, width, height,
                                            external, type, decoded);
    native_upload_end(ctx);
    free(decoded);
    return 1;
}

static int native_immutable_compressed_sub_image_3d(struct mgl_context *ctx,
                                                    const struct mgl_texture *obj, GLenum target,
                                                    GLint level, GLint xoffset, GLint yoffset,
                                                    GLint zoffset, GLsizei width, GLsizei height,
                                                    GLsizei depth, GLenum format,
                                                    const unsigned char *data, size_t data_size) {
    GLenum external, type;
    unsigned char *decoded;
    size_t decoded_size = 0u;
    if (ctx->native_dispatch == NULL || width == 0 || height == 0 || depth == 0)
        return 1;
    if (obj->native_compressed_storage) {
        native_upload_begin(ctx);
        ctx->native_dispatch->glesCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                                          width, height, depth, format,
                                                          (GLsizei)data_size, data);
        native_upload_end(ctx);
        return 1;
    }
    if (ctx->native_dispatch->glesTexSubImage3D == NULL ||
        !compressed_fallback_pixel_format(format, &external, &type))
        return 0;
    decoded = decode_compressed_volume(format, width, height, depth, data, &decoded_size);
    if (decoded == NULL)
        return 0;
    native_upload_begin(ctx);
    ctx->native_dispatch->glesTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height,
                                            depth, external, type, decoded);
    native_upload_end(ctx);
    free(decoded);
    return 1;
}

static unsigned char *compressed_copy(struct mgl_context *ctx, const void *pixels, size_t bytes) {
    const unsigned char *src;
    unsigned char *dst;
    if (bytes == 0u)
        return calloc(1u, 1u);
    if (pixels == NULL && ctx->pixel_unpack_buffer == 0u)
        return calloc(1u, bytes);
    src = pixel_source(ctx, pixels, bytes);
    if (src == NULL)
        return NULL;
    dst = malloc(bytes);
    if (dst == NULL)
        return NULL;
    memcpy(dst, src, bytes);
    return dst;
}

static const unsigned char *compressed_source(struct mgl_context *ctx, const void *pixels,
                                              size_t bytes, unsigned char **owned) {
    *owned = NULL;
    if (bytes != 0u && (pixels != NULL || ctx->pixel_unpack_buffer != 0u))
        return pixel_source(ctx, pixels, bytes);
    *owned = compressed_copy(ctx, pixels, bytes);
    return *owned;
}

static int mgl_texture_apply_compressed_image_2d(struct mgl_context *ctx, GLenum target,
                                                 GLint level, GLenum internalformat, GLsizei width,
                                                 GLsizei height, GLint border, const void *data,
                                                 size_t data_size) {
    struct mgl_texture *obj;
    struct mgl_texture_level *dst;
    GLenum previous_internalformat = 0u;
    unsigned char *owned;
    size_t expected = 0u;
    if (ctx == NULL)
        return 0;
    if (!valid_2d_image_target(target) || compressed_block_bytes(internalformat) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        border != 0 || !compressed_image_size(internalformat, width, height, &expected) ||
        expected != data_size) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    obj = mgl_bound_texture(ctx, cube_face_index(target) >= 0 ? GL_TEXTURE_CUBE_MAP : target);
    if (obj == NULL || !obj->realized || obj->immutable_format) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    owned = malloc(data_size != 0u ? data_size : 1u);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    if (data_size != 0u)
        memcpy(owned, data, data_size);
    dst = texture_level_for_target(obj, target, level);
    if (dst == NULL) {
        free(owned);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    previous_internalformat = dst->internal_format;
    free(dst->data);
    dst->data = owned;
    dst->data_size = data_size;
    dst->width = width;
    dst->height = height;
    dst->depth = 1;
    dst->internal_format = internalformat;
    dst->format = internalformat;
    dst->type = 0u;
    dst->defined = GL_TRUE;
    dst->compressed = GL_TRUE;
    dst->shadow_stale = GL_FALSE;
    if (compressed_format_is_rgtc(internalformat)) {
        if (target == GL_TEXTURE_1D && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesBindTexture != NULL)
            mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D, obj->native_name,
                                         ctx->compat.active_texture);
        if (!native_upload_rgtc_image(ctx, target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target, level,
                                      internalformat, width, height, 1,
                                      data_size != 0u ? data : (const unsigned char *)"")) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
    } else {
        if (target == GL_TEXTURE_1D && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesBindTexture != NULL)
            mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D, obj->native_name,
                                         ctx->compat.active_texture);
        if (!native_upload_bcn_image(ctx, target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target, level,
                                     internalformat, width, height,
                                     data_size != 0u ? data : (const unsigned char *)"")) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
    }
    native_apply_legacy_luminance_swizzle(ctx, target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target,
                                          internalformat, previous_internalformat);
    if (target == GL_TEXTURE_1D && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesBindTexture != NULL) {
        struct mgl_texture *bound2d = mgl_bound_texture(ctx, GL_TEXTURE_2D);
        mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D,
                                     bound2d != NULL ? bound2d->native_name : 0u,
                                     ctx->compat.active_texture);
    }
    return 1;
}

static GLenum validate_compressed_sub_image_2d_destination(struct mgl_texture *obj, GLenum target,
                                                           GLint level, GLint xoffset,
                                                           GLint yoffset, GLsizei width,
                                                           GLsizei height, GLenum format,
                                                           struct mgl_texture_level **out_level) {
    struct mgl_texture_level *dst;
    if (obj == NULL || !obj->realized)
        return GL_INVALID_OPERATION;
    dst = texture_level_for_target(obj, target, level);
    if (dst == NULL || !dst->defined)
        return GL_INVALID_OPERATION;
    if (!dst->compressed || dst->internal_format != format)
        return GL_INVALID_OPERATION;
    if (xoffset > dst->width || yoffset > dst->height || width > dst->width - xoffset ||
        height > dst->height - yoffset)
        return GL_INVALID_VALUE;
    if ((xoffset & 3) != 0 || (yoffset & 3) != 0 ||
        (((width & 3) != 0) && xoffset + width != dst->width) ||
        (((height & 3) != 0) && yoffset + height != dst->height))
        return GL_INVALID_OPERATION;
    if (out_level != NULL)
        *out_level = dst;
    return GL_NO_ERROR;
}

static int mgl_texture_apply_compressed_sub_image_2d(struct mgl_context *ctx, GLenum target,
                                                     GLint level, GLint xoffset, GLint yoffset,
                                                     GLsizei width, GLsizei height, GLenum format,
                                                     const void *data, size_t data_size) {
    struct mgl_texture *obj;
    struct mgl_texture_level *dst;
    GLenum validation;
    size_t expected = 0u;
    size_t block_bytes;
    size_t dst_columns;
    size_t src_columns;
    size_t src_rows;
    size_t xblock;
    size_t yblock;
    size_t row;
    if (ctx == NULL)
        return 0;
    block_bytes = compressed_block_bytes(format);
    if (!valid_2d_image_target(target) || block_bytes == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        width < 0 || height < 0 || !compressed_image_size(format, width, height, &expected) ||
        expected != data_size) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    obj = mgl_bound_texture(ctx, cube_face_index(target) >= 0 ? GL_TEXTURE_CUBE_MAP : target);
    validation = validate_compressed_sub_image_2d_destination(obj, target, level, xoffset, yoffset,
                                                              width, height, format, &dst);
    if (validation != GL_NO_ERROR) {
        mgl_error_set(&ctx->errors, validation);
        return 0;
    }
    dst_columns = ((size_t)dst->width + 3u) / 4u;
    src_columns = ((size_t)width + 3u) / 4u;
    src_rows = ((size_t)height + 3u) / 4u;
    xblock = (size_t)xoffset / 4u;
    yblock = (size_t)yoffset / 4u;
    for (row = 0u; row < src_rows; ++row) {
        size_t dst_offset = ((yblock + row) * dst_columns + xblock) * block_bytes;
        size_t src_offset = row * src_columns * block_bytes;
        memcpy(dst->data + dst_offset, (const unsigned char *)data + src_offset,
               src_columns * block_bytes);
    }
    if (obj->immutable_format) {
        int ok;
        if (target == GL_TEXTURE_1D && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesBindTexture != NULL)
            mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D, obj->native_name,
                                         ctx->compat.active_texture);
        ok = native_immutable_compressed_sub_image_2d(
            ctx, obj, target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target, level, xoffset, yoffset,
            width, height, format, data, data_size);
        if (target == GL_TEXTURE_1D && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesBindTexture != NULL) {
            struct mgl_texture *bound2d = mgl_bound_texture(ctx, GL_TEXTURE_2D);
            mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D,
                                         bound2d != NULL ? bound2d->native_name : 0u,
                                         ctx->compat.active_texture);
        }
        if (!ok)
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return ok;
    }
    if (compressed_format_is_rgtc(format)) {
        if (target == GL_TEXTURE_1D && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesBindTexture != NULL)
            mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D, obj->native_name,
                                         ctx->compat.active_texture);
        if (ctx->native_dispatch != NULL &&
            mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                   "GL_EXT_texture_compression_rgtc") &&
            ctx->native_dispatch->glesCompressedTexSubImage2D != NULL) {
            native_upload_begin(ctx);
            ctx->native_dispatch->glesCompressedTexSubImage2D(
                target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target, level, xoffset, yoffset, width,
                height, format, (GLsizei)data_size, data_size != 0u ? data : NULL);
            native_upload_end(ctx);
        } else if (!native_upload_rgtc_image(ctx, target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target,
                                             level, format, dst->width, dst->height, 1,
                                             dst->data_size != 0u ? dst->data
                                                                  : (const unsigned char *)"")) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
    } else if (ctx->native_dispatch != NULL) {
        if (target == GL_TEXTURE_1D && ctx->native_dispatch->glesBindTexture != NULL)
            mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D, obj->native_name,
                                         ctx->compat.active_texture);
        if (compressed_format_native(ctx->native_dispatch, format) &&
            ctx->native_dispatch->glesCompressedTexSubImage2D != NULL) {
            native_upload_begin(ctx);
            ctx->native_dispatch->glesCompressedTexSubImage2D(
                target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target, level, xoffset, yoffset, width,
                height, format, (GLsizei)data_size, data_size != 0u ? data : NULL);
            native_upload_end(ctx);
        } else if (!native_upload_bcn_image(ctx, target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target,
                                            level, format, dst->width, dst->height, dst->data)) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        if (target == GL_TEXTURE_1D && ctx->native_dispatch->glesBindTexture != NULL) {
            struct mgl_texture *bound2d = mgl_bound_texture(ctx, GL_TEXTURE_2D);
            mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D,
                                         bound2d != NULL ? bound2d->native_name : 0u,
                                         ctx->compat.active_texture);
        }
    }
    return 1;
}

static int mgl_texture_apply_compressed_image_3d(struct mgl_context *ctx, GLenum target,
                                                 GLint level, GLenum internalformat, GLsizei width,
                                                 GLsizei height, GLsizei depth, GLint border,
                                                 const void *data, size_t data_size) {
    struct mgl_texture *obj;
    struct mgl_texture_level *dst;
    GLenum previous_internalformat = 0u;
    unsigned char *owned;
    size_t expected = 0u;
    if (ctx == NULL)
        return 0;
    if (!valid_3d_target(target) || compressed_block_bytes(internalformat) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (target == GL_TEXTURE_3D && compressed_format_is_rgtc(internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (border != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        depth < 0 || !compressed_image_size_3d(internalformat, width, height, depth, &expected) ||
        expected != data_size) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL || !obj->realized || obj->immutable_format) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    owned = malloc(data_size != 0u ? data_size : 1u);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    if (data_size != 0u)
        memcpy(owned, data, data_size);
    dst = &obj->levels[level];
    previous_internalformat = dst->internal_format;
    free(dst->data);
    memset(dst, 0, sizeof(*dst));
    dst->data = owned;
    dst->data_size = data_size;
    dst->width = width;
    dst->height = height;
    dst->depth = depth;
    dst->internal_format = internalformat;
    dst->format = internalformat;
    dst->type = 0u;
    dst->defined = GL_TRUE;
    dst->compressed = GL_TRUE;
    if (!(compressed_format_is_rgtc(internalformat)
              ? native_upload_rgtc_image(ctx, target, level, internalformat, width, height, depth,
                                         data_size != 0u ? data : (const unsigned char *)"")
              : native_upload_bcn_image_3d(ctx, target, level, internalformat, width, height, depth,
                                           data_size != 0u ? data : (const unsigned char *)""))) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    native_apply_legacy_luminance_swizzle(ctx, target, internalformat, previous_internalformat);
    return 1;
}

static int mgl_texture_apply_compressed_sub_image_3d(struct mgl_context *ctx, GLenum target,
                                                     GLint level, GLint xoffset, GLint yoffset,
                                                     GLint zoffset, GLsizei width, GLsizei height,
                                                     GLsizei depth, GLenum format, const void *data,
                                                     size_t data_size) {
    struct mgl_texture *obj;
    struct mgl_texture_level *dst;
    size_t expected = 0u, block_bytes, dst_blocks_x, src_blocks_x, src_blocks_y;
    size_t dst_slice_bytes, src_slice_bytes;
    GLsizei z;
    size_t row;
    if (ctx == NULL)
        return 0;
    if (!valid_3d_target(target) || compressed_block_bytes(format) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (target == GL_TEXTURE_3D && compressed_format_is_rgtc(format)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0 || depth < 0 ||
        !compressed_image_size_3d(format, width, height, depth, &expected) ||
        expected != data_size) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL || !obj->realized || !obj->levels[level].defined) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    dst = &obj->levels[level];
    if (!dst->compressed || dst->internal_format != format) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (xoffset > dst->width || yoffset > dst->height || zoffset > dst->depth ||
        width > dst->width - xoffset || height > dst->height - yoffset ||
        depth > dst->depth - zoffset) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if ((xoffset & 3) != 0 || (yoffset & 3) != 0 ||
        (((width & 3) != 0) && xoffset + width != dst->width) ||
        (((height & 3) != 0) && yoffset + height != dst->height)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    block_bytes = compressed_block_bytes(format);
    dst_blocks_x = ((size_t)dst->width + 3u) / 4u;
    src_blocks_x = ((size_t)width + 3u) / 4u;
    src_blocks_y = ((size_t)height + 3u) / 4u;
    if (!checked_mul(dst_blocks_x, ((size_t)dst->height + 3u) / 4u, &dst_slice_bytes) ||
        !checked_mul(dst_slice_bytes, block_bytes, &dst_slice_bytes) ||
        !checked_mul(src_blocks_x, src_blocks_y, &src_slice_bytes) ||
        !checked_mul(src_slice_bytes, block_bytes, &src_slice_bytes)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    for (z = 0; z < depth; ++z) {
        const unsigned char *src_slice = (const unsigned char *)data + (size_t)z * src_slice_bytes;
        unsigned char *dst_slice = dst->data + ((size_t)zoffset + (size_t)z) * dst_slice_bytes;
        size_t xblock = (size_t)xoffset / 4u;
        size_t yblock = (size_t)yoffset / 4u;
        for (row = 0u; row < src_blocks_y; ++row) {
            size_t dst_offset = ((yblock + row) * dst_blocks_x + xblock) * block_bytes;
            size_t src_offset = row * src_blocks_x * block_bytes;
            memcpy(dst_slice + dst_offset, src_slice + src_offset, src_blocks_x * block_bytes);
        }
    }
    if (obj->immutable_format) {
        int ok = native_immutable_compressed_sub_image_3d(ctx, obj, target, level, xoffset, yoffset,
                                                          zoffset, width, height, depth, format,
                                                          data, data_size);
        if (!ok)
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return ok;
    }
    if (ctx->native_dispatch != NULL &&
        (target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY) &&
        compressed_format_native(ctx->native_dispatch, format) &&
        ctx->native_dispatch->glesCompressedTexSubImage3D != NULL) {
        native_upload_begin(ctx);
        ctx->native_dispatch->glesCompressedTexSubImage3D(
            target, level, xoffset, yoffset, zoffset, width, height, depth, format,
            (GLsizei)data_size, data_size != 0u ? data : NULL);
        native_upload_end(ctx);
    } else if (!(compressed_format_is_rgtc(format)
                     ? native_upload_rgtc_image(
                           ctx, target, level, format, dst->width, dst->height, dst->depth,
                           dst->data_size != 0u ? dst->data : (const unsigned char *)"")
                     : native_upload_bcn_image_3d(
                           ctx, target, level, format, dst->width, dst->height, dst->depth,
                           dst->data_size != 0u ? dst->data : (const unsigned char *)""))) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    return 1;
}

void glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                            GLint border, GLsizei imageSize, const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glCompressedTexImage1D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"border", (uint64_t)(uintptr_t)border},
                                       {"imageSize", (uint64_t)(uintptr_t)imageSize},
                                       {"data", (uint64_t)(uintptr_t)data}},
        7u);
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
}

void glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width,
                               GLenum format, GLsizei imageSize, const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glCompressedTexSubImage1D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"format", (uint64_t)(uintptr_t)format},
                                       {"imageSize", (uint64_t)(uintptr_t)imageSize},
                                       {"data", (uint64_t)(uintptr_t)data}},
        7u);
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
}

void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                            GLsizei height, GLint border, GLsizei imageSize, const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glCompressedTexImage2D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"border", (uint64_t)(uintptr_t)border},
                                       {"imageSize", (uint64_t)(uintptr_t)imageSize},
                                       {"data", (uint64_t)(uintptr_t)data}},
        8u);
    const unsigned char *owned;
    unsigned char *staged = NULL;
    size_t expected = 0u;
    int suppress;
    if (ctx == NULL)
        return;
    /* List compilation records texture commands for replay-time validation. Active Begin/End
     * commands are rejected before execution. */
    if (ctx->immediate.active && (ctx->compiling_list == NULL || ctx->list_replaying)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_2d_image_target(target) || compressed_block_bytes(internalformat) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        border != 0 || imageSize < 0 ||
        !compressed_image_size(internalformat, width, height, &expected) ||
        expected != (size_t)imageSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    owned = compressed_source(ctx, data, expected, &staged);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_compressed_tex_image_2d(ctx, target, level, internalformat, width,
                                                       height, border, owned, expected);
    /* GL_COMPILE_AND_EXECUTE records the
     * payload but reports the execution error without applying it. */
    if (ctx->immediate.active) {
        if (!suppress)
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        free(staged);
        return;
    }
    if (!suppress)
        (void)mgl_texture_apply_compressed_image_2d(ctx, target, level, internalformat, width,
                                                    height, border, owned, expected);
    free(staged);
}

void glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                               GLsizei width, GLsizei height, GLenum format, GLsizei imageSize,
                               const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glCompressedTexSubImage2D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"format", (uint64_t)(uintptr_t)format},
                                       {"imageSize", (uint64_t)(uintptr_t)imageSize}},
        8u);
    const unsigned char *owned;
    unsigned char *staged = NULL;
    size_t expected = 0u;
    int suppress;
    if (ctx == NULL)
        return;

    if (ctx->immediate.active && (ctx->compiling_list == NULL || ctx->list_replaying)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_2d_image_target(target) || compressed_block_bytes(format) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        width < 0 || height < 0 || imageSize < 0 ||
        !compressed_image_size(format, width, height, &expected) || expected != (size_t)imageSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    owned = compressed_source(ctx, data, expected, &staged);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_compressed_tex_sub_image_2d(ctx, target, level, xoffset, yoffset,
                                                           width, height, format, owned, expected);

    if (ctx->immediate.active) {
        if (!suppress)
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        free(staged);
        return;
    }
    if (!suppress)
        (void)mgl_texture_apply_compressed_sub_image_2d(ctx, target, level, xoffset, yoffset, width,
                                                        height, format, owned, expected);
    free(staged);
}

void glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                            GLsizei height, GLsizei depth, GLint border, GLsizei imageSize,
                            const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glCompressedTexImage3D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"depth", (uint64_t)(uintptr_t)depth},
                                       {"border", (uint64_t)(uintptr_t)border},
                                       {"imageSize", (uint64_t)(uintptr_t)imageSize}},
        8u);
    const unsigned char *owned;
    unsigned char *staged = NULL;
    size_t expected = 0u;
    int suppress;
    if (ctx == NULL)
        return;

    if (ctx->immediate.active && (ctx->compiling_list == NULL || ctx->list_replaying)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_3d_target(target) || compressed_block_bytes(internalformat) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (border != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        depth < 0 || imageSize < 0 ||
        (target == GL_TEXTURE_CUBE_MAP_ARRAY &&
         (width != height || depth <= 0 || depth % 6 != 0)) ||
        !compressed_image_size_3d(internalformat, width, height, depth, &expected) ||
        expected != (size_t)imageSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    owned = compressed_source(ctx, data, expected, &staged);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_compressed_tex_image_3d(ctx, target, level, internalformat, width,
                                                       height, depth, border, owned, expected);

    if (ctx->immediate.active) {
        if (!suppress)
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        free(staged);
        return;
    }
    if (!suppress)
        (void)mgl_texture_apply_compressed_image_3d(ctx, target, level, internalformat, width,
                                                    height, depth, border, owned, expected);
    free(staged);
}

void glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                               GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                               GLenum format, GLsizei imageSize, const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glCompressedTexSubImage3D",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"zoffset", (uint64_t)(uintptr_t)zoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height},
                                                       {"depth", (uint64_t)(uintptr_t)depth}},
                        8u);
    const unsigned char *owned;
    unsigned char *staged = NULL;
    size_t expected = 0u;
    int suppress;
    if (ctx == NULL)
        return;

    if (ctx->immediate.active && (ctx->compiling_list == NULL || ctx->list_replaying)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_3d_target(target) || compressed_block_bytes(format) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0 || depth < 0 || imageSize < 0 ||
        !compressed_image_size_3d(format, width, height, depth, &expected) ||
        expected != (size_t)imageSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    owned = compressed_source(ctx, data, expected, &staged);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress =
        mgl_list_record_compressed_tex_sub_image_3d(ctx, target, level, xoffset, yoffset, zoffset,
                                                    width, height, depth, format, owned, expected);

    if (ctx->immediate.active) {
        if (!suppress)
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        free(staged);
        return;
    }
    if (!suppress)
        (void)mgl_texture_apply_compressed_sub_image_3d(ctx, target, level, xoffset, yoffset,
                                                        zoffset, width, height, depth, format,
                                                        owned, expected);
    free(staged);
}

void glCompressedTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                   GLsizei width, GLsizei height, GLenum format, GLsizei imageSize,
                                   const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glCompressedTextureSubImage2D",
        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"format", (uint64_t)(uintptr_t)format},
                                       {"imageSize", (uint64_t)(uintptr_t)imageSize}},
        8u);
    struct mgl_texture *obj;
    const unsigned char *owned;
    unsigned char *staged = NULL;
    size_t expected = 0u;
    GLuint saved;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || obj->target != GL_TEXTURE_2D) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (compressed_block_bytes(format) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        width < 0 || height < 0 || imageSize < 0 ||
        !compressed_image_size(format, width, height, &expected) || expected != (size_t)imageSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    owned = compressed_source(ctx, data, expected, &staged);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    saved = ctx->texture_2d_binding[ctx->compat.active_texture];
    ctx->texture_2d_binding[ctx->compat.active_texture] = texture;
    dsa_native_bind_texture(ctx, obj);
    (void)mgl_texture_apply_compressed_sub_image_2d(ctx, GL_TEXTURE_2D, level, xoffset, yoffset,
                                                    width, height, format, owned, expected);
    ctx->texture_2d_binding[ctx->compat.active_texture] = saved;
    dsa_native_restore_texture(ctx, GL_TEXTURE_2D);
    free(staged);
}

void glCompressedTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                   GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                   GLenum format, GLsizei imageSize, const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glCompressedTextureSubImage3D",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"zoffset", (uint64_t)(uintptr_t)zoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height},
                                                       {"depth", (uint64_t)(uintptr_t)depth}},
                        8u);
    struct mgl_texture *obj;
    const unsigned char *owned;
    unsigned char *staged = NULL;
    size_t expected = 0u, slice_size = 0u;
    GLuint saved;
    GLsizei face;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || (obj->target != GL_TEXTURE_3D && obj->target != GL_TEXTURE_2D_ARRAY &&
                        obj->target != GL_TEXTURE_CUBE_MAP_ARRAY &&
                        obj->target != GL_TEXTURE_CUBE_MAP)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (compressed_block_bytes(format) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0 || depth < 0 || imageSize < 0 ||
        !compressed_image_size_3d(format, width, height, depth, &expected) ||
        expected != (size_t)imageSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (obj->target == GL_TEXTURE_CUBE_MAP && (zoffset > 6 || depth > 6 - zoffset)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    owned = compressed_source(ctx, data, expected, &staged);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (obj->target == GL_TEXTURE_CUBE_MAP) {
        GLenum validation = GL_NO_ERROR;
        if (!compressed_image_size(format, width, height, &slice_size)) {
            free(staged);
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        for (face = 0; face < depth; ++face) {
            validation = validate_compressed_sub_image_2d_destination(
                obj, GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)zoffset + (GLenum)face, level,
                xoffset, yoffset, width, height, format, NULL);
            if (validation != GL_NO_ERROR)
                break;
        }
        if (validation != GL_NO_ERROR) {
            free(staged);
            mgl_error_set(&ctx->errors, validation);
            return;
        }
        saved = ctx->texture_cube_binding[ctx->compat.active_texture];
        ctx->texture_cube_binding[ctx->compat.active_texture] = texture;
        dsa_native_bind_texture(ctx, obj);
        for (face = 0; face < depth; ++face) {
            (void)mgl_texture_apply_compressed_sub_image_2d(
                ctx, GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)zoffset + (GLenum)face, level,
                xoffset, yoffset, width, height, format, owned + (size_t)face * slice_size,
                slice_size);
        }
        ctx->texture_cube_binding[ctx->compat.active_texture] = saved;
        dsa_native_restore_texture(ctx, GL_TEXTURE_CUBE_MAP);
        free(staged);
        return;
    }
    {
        GLuint *binding = active_texture_binding_slot(ctx, obj->target);
        if (binding == NULL) {
            free(staged);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        saved = *binding;
        *binding = texture;
        dsa_native_bind_texture(ctx, obj);
        (void)mgl_texture_apply_compressed_sub_image_3d(ctx, obj->target, level, xoffset, yoffset,
                                                        zoffset, width, height, depth, format,
                                                        owned, expected);
        *binding = saved;
        dsa_native_restore_texture(ctx, obj->target);
    }
    free(staged);
}

static unsigned char clamp_transfer_byte(unsigned char value, GLfloat scale, GLfloat bias) {
    GLfloat out = ((GLfloat)value / 255.0f) * scale + bias;
    if (out <= 0.0f)
        return 0u;
    if (out >= 1.0f)
        return 255u;
    return (unsigned char)(out * 255.0f + 0.5f);
}

int mgl_read_color_rgba(struct mgl_context *ctx, GLint x, GLint y, GLsizei width, GLsizei height,
                        unsigned char **out_rgba, size_t *out_size) {
    struct mgl_framebuffer *read_fb;
    GLuint native_fb = 0u;
    unsigned char *rgba;
    size_t pixels;
    size_t bytes;
    GLenum native_error = GL_NO_ERROR;
    if (ctx == NULL || out_rgba == NULL || out_size == NULL || width < 0 || height < 0)
        return 0;
    if (!checked_mul((size_t)width, (size_t)height, &pixels) || !checked_mul(pixels, 4u, &bytes))
        return 0;
    rgba = calloc(bytes != 0u ? bytes : 1u, 1u);
    if (rgba == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesReadPixels == NULL) {
        free(rgba);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    mgl_texture_refresh_current_framebuffers(ctx);
    read_fb = mgl_framebuffer_find(ctx, ctx->read_framebuffer);
    if (read_fb != NULL)
        native_fb = read_fb->native_name;
    if (ctx->native_dispatch->glesGetError != NULL) {
        while (ctx->native_dispatch->glesGetError() != GL_NO_ERROR) {}
    }
    if (ctx->native_dispatch->glesBindFramebuffer != NULL)
        ctx->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER, native_fb);
    if (ctx->native_dispatch->glesBindBuffer != NULL)
        mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_PACK_BUFFER, 0u);
    if (ctx->native_dispatch->glesPixelStorei != NULL)
        ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, 1);
    if (ctx->native_dispatch->glesPixelStorei != NULL) {
        ctx->native_dispatch->glesPixelStorei(GL_PACK_ROW_LENGTH, 0);
        ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_ROWS, 0);
    }
    if (bytes != 0u) {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_COPY_READBACK);
        ctx->native_dispatch->glesReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    }
    if (ctx->native_dispatch->glesGetError != NULL)
        native_error = ctx->native_dispatch->glesGetError();
    if (ctx->native_dispatch->glesPixelStorei != NULL)
        ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, ctx->pack_alignment);
    if (ctx->native_dispatch->glesPixelStorei != NULL) {
        ctx->native_dispatch->glesPixelStorei(GL_PACK_ROW_LENGTH, ctx->pack_row_length);
        ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_PIXELS, ctx->pack_skip_pixels);
        ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_ROWS, ctx->pack_skip_rows);
    }
    if (ctx->native_dispatch->glesBindBuffer != NULL)
        mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_PACK_BUFFER, native_pack_buffer(ctx));
    if (native_error != GL_NO_ERROR) {
        free(rgba);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (ctx->pixel_transfer.red_scale != 1.0f || ctx->pixel_transfer.red_bias != 0.0f ||
        ctx->pixel_transfer.green_scale != 1.0f || ctx->pixel_transfer.green_bias != 0.0f ||
        ctx->pixel_transfer.blue_scale != 1.0f || ctx->pixel_transfer.blue_bias != 0.0f ||
        ctx->pixel_transfer.alpha_scale != 1.0f || ctx->pixel_transfer.alpha_bias != 0.0f) {
        size_t i;
        for (i = 0u; i < pixels; ++i) {
            rgba[i * 4u + 0u] = clamp_transfer_byte(
                rgba[i * 4u + 0u], ctx->pixel_transfer.red_scale, ctx->pixel_transfer.red_bias);
            rgba[i * 4u + 1u] = clamp_transfer_byte(
                rgba[i * 4u + 1u], ctx->pixel_transfer.green_scale, ctx->pixel_transfer.green_bias);
            rgba[i * 4u + 2u] = clamp_transfer_byte(
                rgba[i * 4u + 2u], ctx->pixel_transfer.blue_scale, ctx->pixel_transfer.blue_bias);
            rgba[i * 4u + 3u] = clamp_transfer_byte(
                rgba[i * 4u + 3u], ctx->pixel_transfer.alpha_scale, ctx->pixel_transfer.alpha_bias);
        }
    }
    *out_rgba = rgba;
    *out_size = bytes;
    return 1;
}

int mgl_read_color_rgba_precise(struct mgl_context *ctx, GLint x, GLint y, GLsizei width,
                                GLsizei height, GLenum type, float **out_rgba) {
    struct mgl_framebuffer *read_fb;
    GLuint native_fb = 0u;
    float *rgba;
    void *transfer;
    size_t pixels, bytes, transfer_bytes, bytes_per_pixel;
    GLenum error = GL_NO_ERROR;
    if (type == GL_FLOAT)
        bytes_per_pixel = 4u * sizeof(float);
    else if (type == GL_UNSIGNED_SHORT || type == GL_SHORT)
        bytes_per_pixel = 4u * sizeof(uint16_t);
    else if (type == GL_BYTE)
        bytes_per_pixel = 4u * sizeof(int8_t);
    else if (type == GL_UNSIGNED_INT_2_10_10_10_REV)
        bytes_per_pixel = sizeof(uint32_t);
    else
        return 0;
    if (ctx == NULL || out_rgba == NULL || width < 0 || height < 0 ||
        !checked_mul((size_t)width, (size_t)height, &pixels) ||
        !checked_mul(pixels, 4u * sizeof(float), &bytes) ||
        !checked_mul(pixels, bytes_per_pixel, &transfer_bytes))
        return 0;
    *out_rgba = NULL;
    rgba = malloc(bytes != 0u ? bytes : 1u);
    if (rgba == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    transfer = type == GL_FLOAT ? rgba : malloc(transfer_bytes != 0u ? transfer_bytes : 1u);
    if (transfer == NULL) {
        free(rgba);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesReadPixels == NULL) {
        if (transfer != rgba)
            free(transfer);
        free(rgba);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    mgl_texture_refresh_current_framebuffers(ctx);
    read_fb = mgl_framebuffer_find(ctx, ctx->read_framebuffer);
    if (read_fb != NULL)
        native_fb = read_fb->native_name;
    if (ctx->native_dispatch->glesGetError != NULL)
        while (ctx->native_dispatch->glesGetError() != GL_NO_ERROR) {}
    if (ctx->native_dispatch->glesBindFramebuffer != NULL)
        ctx->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER, native_fb);
    if (ctx->native_dispatch->glesBindBuffer != NULL)
        mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_PACK_BUFFER, 0u);
    if (ctx->native_dispatch->glesPixelStorei != NULL) {
        ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, 1);
        ctx->native_dispatch->glesPixelStorei(GL_PACK_ROW_LENGTH, 0);
        ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_ROWS, 0);
    }
    if (transfer_bytes != 0u) {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_COPY_READBACK);
        ctx->native_dispatch->glesReadPixels(x, y, width, height, GL_RGBA, type, transfer);
    }
    if (ctx->native_dispatch->glesGetError != NULL)
        error = ctx->native_dispatch->glesGetError();
    if (ctx->native_dispatch->glesPixelStorei != NULL) {
        ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, ctx->pack_alignment);
        ctx->native_dispatch->glesPixelStorei(GL_PACK_ROW_LENGTH, ctx->pack_row_length);
        ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_PIXELS, ctx->pack_skip_pixels);
        ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_ROWS, ctx->pack_skip_rows);
    }
    if (ctx->native_dispatch->glesBindBuffer != NULL)
        mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_PACK_BUFFER, native_pack_buffer(ctx));
    if (error != GL_NO_ERROR) {
        if (transfer != rgba)
            free(transfer);
        free(rgba);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (type == GL_UNSIGNED_SHORT) {
        const uint16_t *words = (const uint16_t *)transfer;
        for (size_t i = 0u; i < pixels * 4u; ++i)
            rgba[i] = (float)words[i] / 65535.0f;
    } else if (type == GL_BYTE) {
        const int8_t *words = (const int8_t *)transfer;
        for (size_t i = 0u; i < pixels * 4u; ++i)
            rgba[i] = words[i] < -127 ? -1.0f : (float)words[i] / 127.0f;
    } else if (type == GL_SHORT) {
        const int16_t *words = (const int16_t *)transfer;
        for (size_t i = 0u; i < pixels * 4u; ++i)
            rgba[i] = words[i] < -32767 ? -1.0f : (float)words[i] / 32767.0f;
    } else if (type == GL_UNSIGNED_INT_2_10_10_10_REV) {
        const uint32_t *words = (const uint32_t *)transfer;
        for (size_t i = 0u; i < pixels; ++i) {
            uint32_t word = words[i];
            rgba[i * 4u + 0u] = (float)(word & 1023u) / 1023.0f;
            rgba[i * 4u + 1u] = (float)((word >> 10u) & 1023u) / 1023.0f;
            rgba[i * 4u + 2u] = (float)((word >> 20u) & 1023u) / 1023.0f;
            rgba[i * 4u + 3u] = (float)((word >> 30u) & 3u) / 3.0f;
        }
    }
    if (transfer != rgba)
        free(transfer);
    for (size_t i = 0u; i < pixels; ++i) {
        const GLfloat scales[4] = {ctx->pixel_transfer.red_scale, ctx->pixel_transfer.green_scale,
                                   ctx->pixel_transfer.blue_scale, ctx->pixel_transfer.alpha_scale};
        const GLfloat biases[4] = {ctx->pixel_transfer.red_bias, ctx->pixel_transfer.green_bias,
                                   ctx->pixel_transfer.blue_bias, ctx->pixel_transfer.alpha_bias};
        for (size_t channel = 0u; channel < 4u; ++channel)
            rgba[i * 4u + channel] = rgba[i * 4u + channel] * scales[channel] + biases[channel];
    }
    *out_rgba = rgba;
    return 1;
}

static unsigned copy_color_format_channels(GLenum format, unsigned indices[4]) {
    indices[0] = 0u;
    indices[1] = 1u;
    indices[2] = 2u;
    indices[3] = 3u;
    switch (format) {
        case GL_RED:
        case GL_LUMINANCE:
        case GL_INTENSITY:
            return 1u;
        case GL_GREEN:
            indices[0] = 1u;
            return 1u;
        case GL_BLUE:
            indices[0] = 2u;
            return 1u;
        case GL_ALPHA:
            indices[0] = 3u;
            return 1u;
        case GL_RG:
            return 2u;
        case GL_LUMINANCE_ALPHA:
            indices[1] = 3u;
            return 2u;
        case GL_BGR:
            indices[0] = 2u;
            indices[2] = 0u;
            return 3u;
        case GL_RGB:
            return 3u;
        case GL_BGRA:
            indices[0] = 2u;
            indices[2] = 0u;
            return 4u;
        default:
            return 4u;
    }
}

static void *copy_color_rgba_to_format(const void *rgba, size_t pixels, GLenum format, GLenum type,
                                       size_t *out_size) {
    unsigned indices[4];
    unsigned components = copy_color_format_channels(format, indices);
    size_t component_size = type == GL_FLOAT           ? sizeof(float)
                            : type == GL_UNSIGNED_BYTE ? sizeof(unsigned char)
                                                       : 0u;
    size_t bytes;
    unsigned char *out;
    const unsigned char *input = rgba;
    if (component_size == 0u || !checked_mul(pixels, components * component_size, &bytes))
        return NULL;
    out = malloc(bytes != 0u ? bytes : 1u);
    if (out == NULL)
        return NULL;
    for (size_t i = 0u; i < pixels; ++i)
        for (unsigned channel = 0u; channel < components; ++channel)
            memcpy(out + (i * components + channel) * component_size,
                   input + (i * 4u + indices[channel]) * component_size, component_size);
    *out_size = bytes;
    return out;
}

static int depth_internal(GLenum internalformat) {
    return internalformat == GL_DEPTH_COMPONENT || internalformat == GL_DEPTH_COMPONENT16 ||
           internalformat == GL_DEPTH_COMPONENT24 || internalformat == GL_DEPTH_COMPONENT32 ||
           internalformat == GL_DEPTH_COMPONENT32F;
}

static GLenum depth_storage_type(GLenum internalformat) {
    if (internalformat == GL_DEPTH_COMPONENT16)
        return GL_UNSIGNED_SHORT;
    if (internalformat == GL_DEPTH_COMPONENT32 || internalformat == GL_DEPTH_COMPONENT32F)
        return GL_FLOAT;
    return GL_UNSIGNED_INT;
}

static unsigned char *depth_values_to_storage(const GLfloat *values, size_t pixels, GLenum type,
                                              size_t *out_size) {
    size_t bpp = type == GL_UNSIGNED_SHORT ? 2u : 4u;
    size_t bytes;
    unsigned char *out;
    size_t i;
    if (!checked_mul(pixels, bpp, &bytes))
        return NULL;
    out = malloc(bytes != 0u ? bytes : 1u);
    if (out == NULL)
        return NULL;
    for (i = 0u; i < pixels; ++i)
        depth_store(out + i * bpp, type, (double)values[i]);
    *out_size = bytes;
    return out;
}

static int read_attached_depth(struct mgl_context *ctx, GLint x, GLint y, GLsizei width,
                               GLsizei height, GLfloat **out) {
    struct mgl_framebuffer *fb;
    struct mgl_texture *texture;
    size_t pixels;
    GLfloat *depth;
    if (ctx == NULL || out == NULL || width < 0 || height < 0)
        return 0;
    fb = mgl_framebuffer_find(ctx, ctx->read_framebuffer);
    if (fb == NULL || fb->depth_texture == 0u)
        return 0;
    texture = mgl_texture_find(ctx, fb->depth_texture);
    if (texture == NULL || fb->depth_texture_level < 0 ||
        fb->depth_texture_level >= (GLint)MGL_MAX_TEXTURE_LEVELS)
        return 0;
    if (!checked_mul((size_t)width, (size_t)height, &pixels) || pixels > SIZE_MAX / sizeof(*depth))
        return 0;
    depth = malloc((pixels != 0u ? pixels : 1u) * sizeof(*depth));
    if (depth == NULL)
        return 0;
    if (!mgl_depth_texture_readback(ctx, texture, fb->depth_texture_level, x, y, width, height,
                                    depth)) {
        free(depth);
        return 0;
    }
    *out = depth;
    return 1;
}

static int direct_copy_state_ok(const struct mgl_context *ctx) {
    const struct mgl_pixel_transfer_state *p = &ctx->pixel_transfer;
    return !p->map_color && p->red_scale == 1.0f && p->green_scale == 1.0f &&
           p->blue_scale == 1.0f && p->alpha_scale == 1.0f && p->red_bias == 0.0f &&
           p->green_bias == 0.0f && p->blue_bias == 0.0f && p->alpha_bias == 0.0f;
}

static int direct_copy_image_2d(struct mgl_context *ctx, GLenum target, GLint level,
                                GLenum internalformat, GLint x, GLint y, GLsizei width,
                                GLsizei height) {
    struct mgl_texture *obj;
    struct mgl_texture_level *dst;
    unsigned char *shadow;
    size_t bytes;
    GLenum format = (internalformat == GL_RGB8 || internalformat == GL_RGB) ? GL_RGB : GL_RGBA;
    if (target != GL_TEXTURE_2D ||
        (internalformat != GL_RGBA8 && internalformat != GL_RGBA && internalformat != GL_RGB8 &&
         internalformat != GL_RGB) ||
        width <= 0 || height <= 0 || !direct_copy_state_ok(ctx) || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesCopyTexImage2D == NULL ||
        !checked_mul((size_t)width, (size_t)height, &bytes) ||
        !checked_mul(bytes, format == GL_RGB ? 3u : 4u, &bytes))
        return 0;
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL || !obj->realized || obj->immutable_format || obj->view_root != NULL)
        return 0;
    dst = texture_level_for_target(obj, target, level);
    if (dst == NULL)
        return 0;
    shadow = calloc(bytes, 1u);
    if (shadow == NULL)
        return 0;
    mgl_texture_refresh_current_framebuffers(ctx);
    mgl_texture_realize_bindings(ctx);
    {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_COPY_TEX);
        ctx->native_dispatch->glesCopyTexImage2D(target, level, format, x, y, width, height, 0);
    }
    free(dst->data);
    memset(dst, 0, sizeof(*dst));
    dst->data = shadow;
    dst->data_size = bytes;
    dst->width = width;
    dst->height = height;
    dst->depth = 1;
    dst->internal_format = internalformat;
    dst->format = format;
    dst->type = GL_UNSIGNED_BYTE;
    dst->defined = GL_TRUE;
    dst->shadow_stale = GL_TRUE;
    obj->used_as_color_attachment = GL_TRUE;
    return 1;
}

static int direct_copy_sub_image_2d(struct mgl_context *ctx, GLenum target, GLint level,
                                    GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width,
                                    GLsizei height) {
    struct mgl_texture *obj;
    struct mgl_texture_level *dst;
    if (target != GL_TEXTURE_2D || width <= 0 || height <= 0 || !direct_copy_state_ok(ctx) ||
        ctx->native_dispatch == NULL || ctx->native_dispatch->glesCopyTexSubImage2D == NULL)
        return 0;
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL || !obj->realized)
        return 0;
    dst = texture_level_for_target(obj, target, level);
    if (dst == NULL || !dst->defined || dst->compressed ||
        (dst->format != GL_RGBA && dst->format != GL_RGB) || dst->type != GL_UNSIGNED_BYTE ||
        (dst->internal_format != GL_RGBA8 && dst->internal_format != GL_RGBA &&
         dst->internal_format != GL_RGB8 && dst->internal_format != GL_RGB) ||
        xoffset > dst->width || yoffset > dst->height || width > dst->width - xoffset ||
        height > dst->height - yoffset)
        return 0;
    mgl_texture_refresh_current_framebuffers(ctx);
    mgl_texture_realize_bindings(ctx);
    {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_COPY_TEX);
        ctx->native_dispatch->glesCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width,
                                                    height);
    }
    dst->shadow_stale = GL_TRUE;
    obj->used_as_color_attachment = GL_TRUE;
    return 1;
}

static void copy_tex_image_2d_apply(struct mgl_context *ctx, GLenum target, GLint level,
                                    GLenum internalformat, GLint x, GLint y, GLsizei width,
                                    GLsizei height, GLint border) {
    void *rgba = NULL;
    void *tight = NULL;
    size_t rgba_size = 0u;
    GLenum copy_type = GL_UNSIGNED_BYTE;
    size_t tight_size = 0u;
    size_t pixels = 0u;
    GLenum format;
    if (target != GL_TEXTURE_1D &&
        direct_copy_image_2d(ctx, target, level, internalformat, x, y, width, height))
        return;
    if (depth_internal(internalformat)) {
        GLfloat *depth = NULL;
        unsigned char *storage;
        GLenum storage_type = depth_storage_type(internalformat);
        size_t storage_size = 0u;
        if (!checked_mul((size_t)width, (size_t)height, &pixels) ||
            !read_attached_depth(ctx, x, y, width, height, &depth)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        storage = depth_values_to_storage(depth, pixels, storage_type, &storage_size);
        free(depth);
        if (storage == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        (void)mgl_texture_apply_image_2d_tight(ctx, target, level, (GLint)internalformat, width,
                                               height, border, GL_DEPTH_COMPONENT, storage_type,
                                               storage, storage_size);
        free(storage);
        return;
    }
    if (!mgl_read_color_for_copy(ctx, x, y, width, height, &rgba, &rgba_size, &copy_type))
        return;
    (void)rgba_size;
    format = (internalformat == GL_RGB || internalformat == GL_RGB8) ? GL_RGB : GL_RGBA;
    if (!checked_mul((size_t)width, (size_t)height, &pixels)) {
        free(rgba);
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    tight = copy_color_rgba_to_format(rgba, pixels, format, copy_type, &tight_size);
    free(rgba);
    if (tight == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    (void)mgl_texture_apply_image_2d_tight(ctx, target, level, (GLint)internalformat, width, height,
                                           border, format, copy_type, tight, tight_size);
    free(tight);
}

void glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
                      GLsizei width, GLint border) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active && (ctx->compiling_list == NULL || ctx->list_replaying)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_1D || !valid_internal((GLint)internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || border != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_copy_tex_image_2d(ctx, target, level, internalformat, x, y, width, 1,
                                          border))
        return;
    copy_tex_image_2d_apply(ctx, target, level, internalformat, x, y, width, 1, border);
}

void glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
                      GLsizei width, GLsizei height, GLint border) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_API_COPY);
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glCopyTexImage2D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"x", (uint64_t)(uintptr_t)x},
                                       {"y", (uint64_t)(uintptr_t)y},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"border", (uint64_t)(uintptr_t)border}},
        8u);
    int suppress;
    if (ctx == NULL)
        return;

    if (ctx->immediate.active && (ctx->compiling_list == NULL || ctx->list_replaying)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_2d_image_target(target) || !valid_internal((GLint)internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        border != 0 || (cube_face_index(target) >= 0 && width != height)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    suppress = mgl_list_record_copy_tex_image_2d(ctx, target, level, internalformat, x, y, width,
                                                 height, border);
    if (suppress)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    {
        uint64_t started = mgl_perf_diag_now();
        copy_tex_image_2d_apply(ctx, target, level, internalformat, x, y, width, height, border);
        mgl_perf_diag_copy(ctx, started);
    }
}

static void copy_tex_sub_image_2d_apply(struct mgl_context *ctx, GLenum target, GLint level,
                                        GLint xoffset, GLint yoffset, GLint x, GLint y,
                                        GLsizei width, GLsizei height) {
    struct mgl_texture *obj;
    struct mgl_texture_level *level_state;
    void *rgba = NULL;
    void *tight = NULL;
    size_t rgba_size = 0u;
    GLenum copy_type = GL_UNSIGNED_BYTE;
    size_t tight_size = 0u;
    size_t pixels = 0u;
    GLenum format;
    if (target != GL_TEXTURE_1D &&
        direct_copy_sub_image_2d(ctx, target, level, xoffset, yoffset, x, y, width, height))
        return;
    obj = mgl_bound_texture(ctx, target);
    level_state = texture_level_for_target(obj, target, level);
    if (obj == NULL || level_state == NULL || !level_state->defined || level_state->compressed) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    format = level_state->format;
    if (format == GL_DEPTH_COMPONENT) {
        GLfloat *depth = NULL;
        unsigned char *storage;
        size_t storage_size = 0u;
        struct mgl_framebuffer *read_fb = mgl_framebuffer_find(ctx, ctx->read_framebuffer);
        if (read_fb == NULL || read_fb->depth_texture == 0u) {
            if (!mgl_depth_copy_framebuffer_to_texture(ctx, obj, target, level, xoffset, yoffset, x,
                                                       y, width, height))
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (!checked_mul((size_t)width, (size_t)height, &pixels) ||
            !read_attached_depth(ctx, x, y, width, height, &depth)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        storage = depth_values_to_storage(depth, pixels, level_state->type, &storage_size);
        free(depth);
        if (storage == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        (void)mgl_texture_apply_sub_image_2d_tight(ctx, target, level, xoffset, yoffset, width,
                                                   height, GL_DEPTH_COMPONENT, level_state->type,
                                                   storage, storage_size);
        free(storage);
        return;
    }
    if (!mgl_read_color_for_copy(ctx, x, y, width, height, &rgba, &rgba_size, &copy_type))
        return;
    (void)rgba_size;
    if (!checked_mul((size_t)width, (size_t)height, &pixels)) {
        free(rgba);
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    tight = copy_color_rgba_to_format(rgba, pixels, format, copy_type, &tight_size);
    free(rgba);
    if (tight == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    (void)mgl_texture_apply_sub_image_2d_tight(ctx, target, level, xoffset, yoffset, width, height,
                                               format, copy_type, tight, tight_size);
    free(tight);
}

void glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y,
                         GLsizei width) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active && (ctx->compiling_list == NULL || ctx->list_replaying)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_1D) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || width < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_copy_tex_sub_image_2d(ctx, target, level, xoffset, 0, x, y, width, 1))
        return;
    copy_tex_sub_image_2d_apply(ctx, target, level, xoffset, 0, x, y, width, 1);
}

void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
                         GLsizei width, GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_API_COPY);
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glCopyTexSubImage2D",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"x", (uint64_t)(uintptr_t)x},
                                                       {"y", (uint64_t)(uintptr_t)y},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height}},
                        8u);
    int suppress;
    if (ctx == NULL)
        return;

    if (ctx->immediate.active && (ctx->compiling_list == NULL || ctx->list_replaying)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_2d_image_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    suppress = mgl_list_record_copy_tex_sub_image_2d(ctx, target, level, xoffset, yoffset, x, y,
                                                     width, height);
    if (suppress)
        return;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    {
        uint64_t started = mgl_perf_diag_now();
        copy_tex_sub_image_2d_apply(ctx, target, level, xoffset, yoffset, x, y, width, height);
        mgl_perf_diag_copy(ctx, started);
    }
}

static void copy_tex_sub_image_3d_apply(struct mgl_context *ctx, GLenum target, GLint level,
                                        GLint xoffset, GLint yoffset, GLint zoffset, GLint x,
                                        GLint y, GLsizei width, GLsizei height) {
    struct mgl_texture *obj;
    struct mgl_texture_level *level_state;
    void *rgba = NULL;
    void *tight = NULL;
    size_t rgba_size = 0u;
    GLenum copy_type = GL_UNSIGNED_BYTE;
    size_t tight_size = 0u;
    size_t pixels = 0u;
    GLenum format;
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL || !obj->levels[level].defined || obj->levels[level].compressed) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    level_state = &obj->levels[level];
    if (xoffset > level_state->width || yoffset > level_state->height ||
        zoffset >= level_state->depth || width > level_state->width - xoffset ||
        height > level_state->height - yoffset) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    format = level_state->format;
    if (format == GL_DEPTH_COMPONENT) {
        GLfloat *depth = NULL;
        unsigned char *storage;
        size_t storage_size = 0u;
        if (!checked_mul((size_t)width, (size_t)height, &pixels) ||
            !read_attached_depth(ctx, x, y, width, height, &depth)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        storage = depth_values_to_storage(depth, pixels, level_state->type, &storage_size);
        free(depth);
        if (storage == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        (void)mgl_texture_apply_sub_image_3d_tight(ctx, target, level, xoffset, yoffset, zoffset,
                                                   width, height, 1, GL_DEPTH_COMPONENT,
                                                   level_state->type, storage, storage_size);
        free(storage);
        return;
    }
    if (!mgl_read_color_for_copy(ctx, x, y, width, height, &rgba, &rgba_size, &copy_type))
        return;
    (void)rgba_size;
    if (!checked_mul((size_t)width, (size_t)height, &pixels)) {
        free(rgba);
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    tight = copy_color_rgba_to_format(rgba, pixels, format, copy_type, &tight_size);
    free(rgba);
    if (tight == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    (void)mgl_texture_apply_sub_image_3d_tight(ctx, target, level, xoffset, yoffset, zoffset, width,
                                               height, 1, format, copy_type, tight, tight_size);
    free(tight);
}

void glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                         GLint x, GLint y, GLsizei width, GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glCopyTexSubImage3D",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"zoffset", (uint64_t)(uintptr_t)zoffset},
                                                       {"x", (uint64_t)(uintptr_t)x},
                                                       {"y", (uint64_t)(uintptr_t)y},
                                                       {"width", (uint64_t)(uintptr_t)width}},
                        8u);
    int suppress;
    if (ctx == NULL)
        return;

    if (ctx->immediate.active && (ctx->compiling_list == NULL || ctx->list_replaying)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_3d_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    suppress = mgl_list_record_copy_tex_sub_image_3d(ctx, target, level, xoffset, yoffset, zoffset,
                                                     x, y, width, height);
    if (suppress)
        return;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    copy_tex_sub_image_3d_apply(ctx, target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void glCompressedTextureImage2DEXT(GLuint texture, GLenum target, GLint level,
                                   GLenum internalformat, GLsizei width, GLsizei height,
                                   GLint border, GLsizei imageSize, const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glCompressedTextureImage2DEXT",
        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                       {"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"border", (uint64_t)(uintptr_t)border},
                                       {"imageSize", (uint64_t)(uintptr_t)imageSize}},
        8u);
    const unsigned char *owned;
    unsigned char *staged = NULL;
    size_t expected = 0u;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_2d_image_target(target) || compressed_block_bytes(internalformat) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        border != 0 || imageSize < 0 ||
        !compressed_image_size(internalformat, width, height, &expected) ||
        expected != (size_t)imageSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    owned = compressed_source(ctx, data, expected, &staged);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_compressed_texture_image_2d_ext(
        ctx, texture, target, level, internalformat, width, height, border, owned, expected);
    if (!suppress)
        (void)mgl_texture_apply_compressed_image_2d_tight_named_ext(
            ctx, texture, target, level, internalformat, width, height, border, owned, expected);
    free(staged);
}

void glCompressedTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                      GLint yoffset, GLsizei width, GLsizei height, GLenum format,
                                      GLsizei imageSize, const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glCompressedTextureSubImage2DEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height},
                                                       {"format", (uint64_t)(uintptr_t)format}},
                        8u);
    const unsigned char *owned;
    unsigned char *staged = NULL;
    size_t expected = 0u;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_2d_image_target(target) || compressed_block_bytes(format) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        width < 0 || height < 0 || imageSize < 0 ||
        !compressed_image_size(format, width, height, &expected) || expected != (size_t)imageSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    owned = compressed_source(ctx, data, expected, &staged);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_compressed_texture_sub_image_2d_ext(
        ctx, texture, target, level, xoffset, yoffset, width, height, format, owned, expected);
    if (!suppress)
        (void)mgl_texture_apply_compressed_sub_image_2d_tight_named_ext(
            ctx, texture, target, level, xoffset, yoffset, width, height, format, owned, expected);
    free(staged);
}

void glCompressedTextureImage3DEXT(GLuint texture, GLenum target, GLint level,
                                   GLenum internalformat, GLsizei width, GLsizei height,
                                   GLsizei depth, GLint border, GLsizei imageSize,
                                   const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glCompressedTextureImage3DEXT",
        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                       {"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"depth", (uint64_t)(uintptr_t)depth},
                                       {"border", (uint64_t)(uintptr_t)border}},
        8u);
    const unsigned char *owned;
    unsigned char *staged = NULL;
    size_t expected = 0u;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_3d_target(target) || compressed_block_bytes(internalformat) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (border != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        depth < 0 || imageSize < 0 ||
        !compressed_image_size_3d(internalformat, width, height, depth, &expected) ||
        expected != (size_t)imageSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    owned = compressed_source(ctx, data, expected, &staged);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_compressed_texture_image_3d_ext(
        ctx, texture, target, level, internalformat, width, height, depth, border, owned, expected);
    if (!suppress)
        (void)mgl_texture_apply_compressed_image_3d_tight_named_ext(ctx, texture, target, level,
                                                                    internalformat, width, height,
                                                                    depth, border, owned, expected);
    free(staged);
}

void glCompressedTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                      GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                                      GLsizei depth, GLenum format, GLsizei imageSize,
                                      const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glCompressedTextureSubImage3DEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"zoffset", (uint64_t)(uintptr_t)zoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height}},
                        8u);
    const unsigned char *owned;
    unsigned char *staged = NULL;
    size_t expected = 0u;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_3d_target(target) || compressed_block_bytes(format) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0 || depth < 0 || imageSize < 0 ||
        !compressed_image_size_3d(format, width, height, depth, &expected) ||
        expected != (size_t)imageSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    owned = compressed_source(ctx, data, expected, &staged);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_compressed_texture_sub_image_3d_ext(
        ctx, texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, owned,
        expected);
    if (!suppress)
        (void)mgl_texture_apply_compressed_sub_image_3d_tight_named_ext(
            ctx, texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format,
            owned, expected);
    free(staged);
}

void glTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat,
                         GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
                         const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glTextureImage2DEXT",
        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                       {"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"border", (uint64_t)(uintptr_t)border},
                                       {"format", (uint64_t)(uintptr_t)format}},
        8u);
    size_t components = 0u, data_size = 0u;
    const unsigned char *data;
    unsigned char *owned = NULL;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_2d_image_target(target) || !valid_format_type(format, type, &components) ||
        !valid_internal(internalformat) || !format_matches_internal(format, internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        border != 0 || (cube_face_index(target) >= 0 && width != height)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    data = tight_source_2d(ctx, pixels, width, height, components, pixel_element_size(type),
                           &data_size, &owned);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress =
        mgl_list_record_texture_image_2d_ext(ctx, texture, target, level, internalformat, width,
                                             height, border, format, type, data, data_size);
    if (!suppress)
        (void)mgl_texture_apply_image_2d_tight_named_ext(ctx, texture, target, level,
                                                         internalformat, width, height, border,
                                                         format, type, data, data_size);
    free(owned);
}

void glTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                            GLint yoffset, GLsizei width, GLsizei height, GLenum format,
                            GLenum type, const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTextureSubImage2DEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height},
                                                       {"format", (uint64_t)(uintptr_t)format}},
                        8u);
    size_t components = 0u, data_size = 0u;
    const unsigned char *data;
    unsigned char *owned = NULL;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_2d_image_target(target) || !valid_format_type(format, type, &components)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    data = tight_source_2d(ctx, pixels, width, height, components, pixel_element_size(type),
                           &data_size, &owned);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress =
        mgl_list_record_texture_sub_image_2d_ext(ctx, texture, target, level, xoffset, yoffset,
                                                 width, height, format, type, data, data_size);
    if (!suppress)
        (void)mgl_texture_apply_sub_image_2d_tight_named_ext(ctx, texture, target, level, xoffset,
                                                             yoffset, width, height, format, type,
                                                             data, data_size);
    free(owned);
}

void glTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat,
                         GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format,
                         GLenum type, const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glTextureImage3DEXT",
        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                       {"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"depth", (uint64_t)(uintptr_t)depth},
                                       {"border", (uint64_t)(uintptr_t)border}},
        8u);
    size_t components = 0u, data_size = 0u;
    const unsigned char *data;
    unsigned char *owned = NULL;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_3d_target(target) || !valid_format_type(format, type, &components) ||
        !valid_internal(internalformat) || !format_matches_internal(format, internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        depth < 0 || border != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    data = tight_source_3d(ctx, pixels, width, height, depth, components, pixel_element_size(type),
                           &data_size, &owned);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress =
        mgl_list_record_texture_image_3d_ext(ctx, texture, target, level, internalformat, width,
                                             height, depth, border, format, type, data, data_size);
    if (!suppress)
        (void)mgl_texture_apply_image_3d_tight_named_ext(ctx, texture, target, level,
                                                         internalformat, width, height, depth,
                                                         border, format, type, data, data_size);
    free(owned);
}

void glTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                            GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                            GLsizei depth, GLenum format, GLenum type, const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTextureSubImage3DEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"zoffset", (uint64_t)(uintptr_t)zoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height}},
                        8u);
    size_t components = 0u, data_size = 0u;
    const unsigned char *data;
    unsigned char *owned = NULL;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_3d_target(target) || !valid_format_type(format, type, &components)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0 || depth < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    data = tight_source_3d(ctx, pixels, width, height, depth, components, pixel_element_size(type),
                           &data_size, &owned);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_texture_sub_image_3d_ext(ctx, texture, target, level, xoffset,
                                                        yoffset, zoffset, width, height, depth,
                                                        format, type, data, data_size);
    if (!suppress)
        (void)mgl_texture_apply_sub_image_3d_tight_named_ext(ctx, texture, target, level, xoffset,
                                                             yoffset, zoffset, width, height, depth,
                                                             format, type, data, data_size);
    free(owned);
}

void glCopyTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat,
                             GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glCopyTextureImage2DEXT",
        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                       {"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"x", (uint64_t)(uintptr_t)x},
                                       {"y", (uint64_t)(uintptr_t)y},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height}},
        8u);
    struct mgl_texture *obj;
    GLenum object_target;
    GLuint *binding;
    GLuint saved;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_2d_image_target(target) || !valid_internal((GLint)internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        border != 0 || (cube_face_index(target) >= 0 && width != height)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    suppress = mgl_list_record_copy_texture_image_2d_ext(
        ctx, texture, target, level, internalformat, x, y, width, height, border);
    if (suppress)
        return;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return;
    object_target = ext_dsa_texture_object_target(target);
    binding = active_texture_binding_slot(ctx, object_target);
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    saved = *binding;
    *binding = texture;
    dsa_native_bind_texture(ctx, obj);
    copy_tex_image_2d_apply(ctx, target, level, internalformat, x, y, width, height, border);
    *binding = saved;
    dsa_native_restore_texture(ctx, object_target);
}

void glCopyTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glCopyTextureSubImage2DEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"x", (uint64_t)(uintptr_t)x},
                                                       {"y", (uint64_t)(uintptr_t)y},
                                                       {"width", (uint64_t)(uintptr_t)width}},
                        8u);
    struct mgl_texture *obj;
    GLenum object_target;
    GLuint *binding;
    GLuint saved;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_2d_image_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    suppress = mgl_list_record_copy_texture_sub_image_2d_ext(ctx, texture, target, level, xoffset,
                                                             yoffset, x, y, width, height);
    if (suppress)
        return;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return;
    object_target = ext_dsa_texture_object_target(target);
    binding = active_texture_binding_slot(ctx, object_target);
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    saved = *binding;
    *binding = texture;
    dsa_native_bind_texture(ctx, obj);
    copy_tex_sub_image_2d_apply(ctx, target, level, xoffset, yoffset, x, y, width, height);
    *binding = saved;
    dsa_native_restore_texture(ctx, object_target);
}

void glCopyTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width,
                                GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glCopyTextureSubImage3DEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"zoffset", (uint64_t)(uintptr_t)zoffset},
                                                       {"x", (uint64_t)(uintptr_t)x},
                                                       {"y", (uint64_t)(uintptr_t)y}},
                        8u);
    struct mgl_texture *obj;
    GLuint *binding;
    GLuint saved;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_3d_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    suppress = mgl_list_record_copy_texture_sub_image_3d_ext(ctx, texture, target, level, xoffset,
                                                             yoffset, zoffset, x, y, width, height);
    if (suppress)
        return;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return;
    binding = active_texture_binding_slot(ctx, target);
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    saved = *binding;
    *binding = texture;
    dsa_native_bind_texture(ctx, obj);
    copy_tex_sub_image_3d_apply(ctx, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    *binding = saved;
    dsa_native_restore_texture(ctx, target);
}

void glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x,
                             GLint y, GLsizei width, GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    GLuint saved;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || obj->target != GL_TEXTURE_2D) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    saved = ctx->texture_2d_binding[ctx->compat.active_texture];
    ctx->texture_2d_binding[ctx->compat.active_texture] = texture;
    dsa_native_bind_texture(ctx, obj);
    copy_tex_sub_image_2d_apply(ctx, GL_TEXTURE_2D, level, xoffset, yoffset, x, y, width, height);
    ctx->texture_2d_binding[ctx->compat.active_texture] = saved;
    dsa_native_restore_texture(ctx, GL_TEXTURE_2D);
}

void glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                             GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    GLuint saved;
    GLenum apply_target;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || (!valid_3d_target(obj->target) && obj->target != GL_TEXTURE_CUBE_MAP)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (obj->target == GL_TEXTURE_CUBE_MAP && zoffset >= 6) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (obj->target == GL_TEXTURE_3D) {
        saved = ctx->texture_3d_binding[ctx->compat.active_texture];
        ctx->texture_3d_binding[ctx->compat.active_texture] = texture;
        apply_target = GL_TEXTURE_3D;
    } else if (obj->target == GL_TEXTURE_2D_ARRAY) {
        saved = ctx->texture_2d_array_binding[ctx->compat.active_texture];
        ctx->texture_2d_array_binding[ctx->compat.active_texture] = texture;
        apply_target = GL_TEXTURE_2D_ARRAY;
    } else if (obj->target == GL_TEXTURE_CUBE_MAP_ARRAY) {
        saved = ctx->texture_cube_array_binding[ctx->compat.active_texture];
        ctx->texture_cube_array_binding[ctx->compat.active_texture] = texture;
        apply_target = GL_TEXTURE_CUBE_MAP_ARRAY;
    } else {
        saved = ctx->texture_cube_binding[ctx->compat.active_texture];
        ctx->texture_cube_binding[ctx->compat.active_texture] = texture;
        apply_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)zoffset;
    }
    dsa_native_bind_texture(ctx, obj);
    if (obj->target == GL_TEXTURE_CUBE_MAP)
        copy_tex_sub_image_2d_apply(ctx, apply_target, level, xoffset, yoffset, x, y, width,
                                    height);
    else
        copy_tex_sub_image_3d_apply(ctx, apply_target, level, xoffset, yoffset, zoffset, x, y,
                                    width, height);
    if (obj->target == GL_TEXTURE_3D)
        ctx->texture_3d_binding[ctx->compat.active_texture] = saved;
    else if (obj->target == GL_TEXTURE_2D_ARRAY)
        ctx->texture_2d_array_binding[ctx->compat.active_texture] = saved;
    else if (obj->target == GL_TEXTURE_CUBE_MAP_ARRAY)
        ctx->texture_cube_array_binding[ctx->compat.active_texture] = saved;
    else
        ctx->texture_cube_binding[ctx->compat.active_texture] = saved;
    dsa_native_restore_texture(ctx, obj->target);
}

static int texture_anisotropy_supported(const struct mgl_context *ctx) {
    return ctx != NULL && ctx->native_dispatch != NULL &&
           mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                  "GL_EXT_texture_filter_anisotropic");
}

static GLfloat texture_anisotropy_clamp(struct mgl_context *ctx, GLfloat value) {
    GLfloat maximum = value;
    if (ctx != NULL && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesGetFloatv != NULL) {
        ctx->native_dispatch->glesGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximum);
        if (maximum < value)
            return maximum;
    }
    return value;
}

static int texture_anisotropy_validate(struct mgl_context *ctx, GLenum pname, GLfloat value) {
    if (pname != GL_TEXTURE_MAX_ANISOTROPY_EXT)
        return 1;
    if (!texture_anisotropy_supported(ctx)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (!(value >= 1.0f)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    return 1;
}

static int validate_texture_param(GLenum pname, GLint value) {
    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
            return value == (GLint)GL_NEAREST || value == (GLint)GL_LINEAR ||
                   value == (GLint)GL_NEAREST_MIPMAP_NEAREST ||
                   value == (GLint)GL_LINEAR_MIPMAP_NEAREST ||
                   value == (GLint)GL_NEAREST_MIPMAP_LINEAR ||
                   value == (GLint)GL_LINEAR_MIPMAP_LINEAR;
        case GL_TEXTURE_MAG_FILTER:
            return value == (GLint)GL_NEAREST || value == (GLint)GL_LINEAR;
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
        case GL_TEXTURE_WRAP_R:
            return value == (GLint)GL_REPEAT || value == (GLint)GL_CLAMP_TO_EDGE ||
                   value == (GLint)GL_MIRRORED_REPEAT || value == (GLint)GL_CLAMP ||
                   value == (GLint)GL_CLAMP_TO_BORDER;
        case GL_TEXTURE_BASE_LEVEL:
        case GL_TEXTURE_MAX_LEVEL:
            return value >= 0;
        case GL_TEXTURE_MIN_LOD:
        case GL_TEXTURE_MAX_LOD:
            return 1;
        case GL_TEXTURE_LOD_BIAS:
            return 1;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
            return value >= 1;
        case GL_TEXTURE_COMPARE_MODE:
            return value == (GLint)GL_NONE || value == (GLint)GL_COMPARE_REF_TO_TEXTURE;
        case GL_TEXTURE_COMPARE_FUNC:
            return value == (GLint)GL_NEVER || value == (GLint)GL_LESS ||
                   value == (GLint)GL_EQUAL || value == (GLint)GL_LEQUAL ||
                   value == (GLint)GL_GREATER || value == (GLint)GL_NOTEQUAL ||
                   value == (GLint)GL_GEQUAL || value == (GLint)GL_ALWAYS;
        case GL_DEPTH_TEXTURE_MODE:
            return value == (GLint)GL_LUMINANCE || value == (GLint)GL_INTENSITY ||
                   value == (GLint)GL_ALPHA;
        case GL_DEPTH_STENCIL_TEXTURE_MODE:
            return value == (GLint)GL_DEPTH_COMPONENT || value == (GLint)GL_STENCIL_INDEX;
        case GL_TEXTURE_SWIZZLE_R:
        case GL_TEXTURE_SWIZZLE_G:
        case GL_TEXTURE_SWIZZLE_B:
        case GL_TEXTURE_SWIZZLE_A:
            return value == (GLint)GL_RED || value == (GLint)GL_GREEN || value == (GLint)GL_BLUE ||
                   value == (GLint)GL_ALPHA || value == (GLint)GL_ZERO || value == (GLint)GL_ONE;
        default:
            return -1;
    }
}

static GLfloat clamp_texture_component(GLfloat value) {
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

static GLfloat normalized_texture_int(GLint value) {
    if (value <= 0)
        return 0.0f;
    return (GLfloat)((double)value / (double)INT_MAX);
}

/* LOD integer queries round halfway values away from zero and saturate at GLint limits. */
static GLint texture_lod_to_int(GLfloat value) {
    if (value >= (GLfloat)INT_MAX)
        return INT_MAX;
    if (value <= (GLfloat)INT_MIN)
        return INT_MIN;
    return (GLint)roundf(value);
}

static GLint texture_float_to_int(GLfloat value) {
    double scaled = (double)clamp_texture_component(value) * (double)INT_MAX;
    if (scaled >= (double)INT_MAX)
        return INT_MAX;
    return (GLint)(scaled + 0.5);
}

static GLenum native_wrap_mode(GLint value) {
    if (value == (GLint)GL_CLAMP || value == (GLint)GL_CLAMP_TO_BORDER)
        return GL_CLAMP_TO_BORDER;
    return (GLenum)value;
}

static void apply_texture_border_color(struct mgl_texture *obj, const GLfloat *params) {
    unsigned i;
    if (obj == NULL || params == NULL)
        return;
    for (i = 0u; i < 4u; ++i)
        obj->border_color[i] = clamp_texture_component(params[i]);
    obj->border_color_type = GL_FLOAT;
}

static void apply_texture_border_color_i(struct mgl_texture *obj, const GLint *params) {
    if (obj == NULL || params == NULL)
        return;
    memcpy(obj->border_color_i, params, sizeof(obj->border_color_i));
    obj->border_color_type = GL_INT;
}

static void apply_texture_border_color_ui(struct mgl_texture *obj, const GLuint *params) {
    if (obj == NULL || params == NULL)
        return;
    memcpy(obj->border_color_ui, params, sizeof(obj->border_color_ui));
    obj->border_color_type = GL_UNSIGNED_INT;
}

static void get_texture_border_color_i(const struct mgl_texture *obj, GLint *params) {
    unsigned i;
    if (obj == NULL || params == NULL)
        return;
    if (obj->border_color_type == GL_INT) {
        memcpy(params, obj->border_color_i, sizeof(obj->border_color_i));
        return;
    }
    if (obj->border_color_type == GL_UNSIGNED_INT) {
        for (i = 0u; i < 4u; ++i)
            params[i] = (GLint)obj->border_color_ui[i];
        return;
    }
    for (i = 0u; i < 4u; ++i)
        params[i] = (GLint)obj->border_color[i];
}

static void get_texture_border_color_ui(const struct mgl_texture *obj, GLuint *params) {
    unsigned i;
    if (obj == NULL || params == NULL)
        return;
    if (obj->border_color_type == GL_UNSIGNED_INT) {
        memcpy(params, obj->border_color_ui, sizeof(obj->border_color_ui));
        return;
    }
    if (obj->border_color_type == GL_INT) {
        for (i = 0u; i < 4u; ++i)
            params[i] = (GLuint)obj->border_color_i[i];
        return;
    }
    for (i = 0u; i < 4u; ++i)
        params[i] = obj->border_color[i] <= 0.0f ? 0u : (GLuint)obj->border_color[i];
}

static void texture_border_color_from_ints(GLfloat out[4], const GLint *params) {
    unsigned i;
    if (out == NULL || params == NULL)
        return;
    for (i = 0u; i < 4u; ++i)
        out[i] = normalized_texture_int(params[i]);
}

static void apply_texture_param(struct mgl_texture *obj, GLenum pname, GLint value) {
    if (obj == NULL)
        return;
    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
            obj->min_filter = value;
            break;
        case GL_TEXTURE_MAG_FILTER:
            obj->mag_filter = value;
            break;
        case GL_TEXTURE_WRAP_S:
            obj->wrap_s = value;
            break;
        case GL_TEXTURE_WRAP_T:
            obj->wrap_t = value;
            break;
        case GL_TEXTURE_WRAP_R:
            obj->wrap_r = value;
            break;
        case GL_TEXTURE_MIN_LOD:
            obj->min_lod = (GLfloat)value;
            break;
        case GL_TEXTURE_MAX_LOD:
            obj->max_lod = (GLfloat)value;
            break;
        case GL_TEXTURE_LOD_BIAS:
            obj->lod_bias = (GLfloat)value;
            break;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
            obj->max_anisotropy = (GLfloat)value;
            break;
        case GL_TEXTURE_BASE_LEVEL:
            obj->base_level = value;
            break;
        case GL_TEXTURE_MAX_LEVEL:
            obj->max_level = value;
            break;
        case GL_TEXTURE_COMPARE_MODE:
            obj->compare_mode = value;
            break;
        case GL_TEXTURE_COMPARE_FUNC:
            obj->compare_func = value;
            break;
        case GL_DEPTH_TEXTURE_MODE:
            obj->depth_texture_mode = value;
            break;
        case GL_DEPTH_STENCIL_TEXTURE_MODE:
            obj->depth_stencil_texture_mode = value;
            break;
        case GL_TEXTURE_SWIZZLE_R:
        case GL_TEXTURE_SWIZZLE_G:
        case GL_TEXTURE_SWIZZLE_B:
        case GL_TEXTURE_SWIZZLE_A:
            obj->swizzle[pname - GL_TEXTURE_SWIZZLE_R] = value;
            break;
        default:
            break;
    }
}

static int validate_texture_float_param(GLenum pname) {
    return pname == GL_TEXTURE_MIN_LOD || pname == GL_TEXTURE_MAX_LOD ||
           pname == GL_TEXTURE_PRIORITY || pname == GL_TEXTURE_LOD_BIAS ||
           pname == GL_TEXTURE_MAX_ANISOTROPY_EXT;
}

static void apply_texture_float_param(struct mgl_texture *obj, GLenum pname, GLfloat value) {
    if (obj == NULL)
        return;
    if (pname == GL_TEXTURE_MIN_LOD)
        obj->min_lod = value;
    else if (pname == GL_TEXTURE_MAX_LOD)
        obj->max_lod = value;
    else if (pname == GL_TEXTURE_PRIORITY)
        obj->priority = clamp_texture_priority(value);
    else if (pname == GL_TEXTURE_LOD_BIAS)
        obj->lod_bias = value;
    else if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT)
        obj->max_anisotropy = value;
}

static int valid_parameter_target(GLenum target) {
    return target == GL_TEXTURE_1D || target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE ||
           target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY ||
           target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY ||
           target == GL_TEXTURE_2D_MULTISAMPLE ||
           target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
}

static int multisample_sampler_parameter(GLenum pname) {
    return pname == GL_TEXTURE_MIN_FILTER || pname == GL_TEXTURE_MAG_FILTER ||
           pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_R ||
           pname == GL_TEXTURE_MIN_LOD || pname == GL_TEXTURE_MAX_LOD ||
           pname == GL_TEXTURE_LOD_BIAS || pname == GL_TEXTURE_COMPARE_MODE ||
           pname == GL_TEXTURE_COMPARE_FUNC || pname == GL_TEXTURE_BORDER_COLOR ||
           pname == GL_TEXTURE_MAX_ANISOTROPY_EXT;
}

static int texture_is_multisample(const struct mgl_texture *obj) {
    return obj != NULL && (obj->target == GL_TEXTURE_2D_MULTISAMPLE ||
                           obj->target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY);
}

static GLuint texture_binding_for_unit(const struct mgl_context *ctx, unsigned unit,
                                       GLenum target) {
    if (ctx == NULL || unit >= mgl_programmable_texture_limit(ctx))
        return 0u;
    if (target == GL_TEXTURE_2D)
        return ctx->texture_2d_binding[unit];
    if (target == GL_TEXTURE_RECTANGLE)
        return ctx->texture_rectangle_binding[unit];
    if (target == GL_TEXTURE_3D)
        return ctx->texture_3d_binding[unit];
    if (target == GL_TEXTURE_2D_ARRAY)
        return ctx->texture_2d_array_binding[unit];
    if (target == GL_TEXTURE_CUBE_MAP)
        return ctx->texture_cube_binding[unit];
    if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
        return ctx->texture_cube_array_binding[unit];
    if (target == GL_TEXTURE_2D_MULTISAMPLE)
        return ctx->texture_2d_multisample_binding[unit];
    if (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        return ctx->texture_2d_multisample_array_binding[unit];
    return 0u;
}

static struct mgl_texture *texture_for_unit(struct mgl_context *ctx, unsigned unit, GLenum target) {
    GLuint name;
    if (ctx == NULL || unit >= mgl_programmable_texture_limit(ctx))
        return NULL;
    name = texture_binding_for_unit(ctx, unit, target);
    return name == 0u ? mgl_default_texture(ctx, target) : mgl_texture_find(ctx, name);
}

static int explicit_texunit_index(struct mgl_context *ctx, GLenum texunit, unsigned *unit) {
    if (ctx == NULL || unit == NULL)
        return 0;
    if (texunit < GL_TEXTURE0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    *unit = (unsigned)(texunit - GL_TEXTURE0);
    if (*unit >= mgl_programmable_texture_limit(ctx)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    return 1;
}

static int explicit_unit_apply_begin(struct mgl_context *ctx, unsigned unit, unsigned *saved_unit) {
    if (ctx == NULL || saved_unit == NULL || unit >= mgl_programmable_texture_limit(ctx)) {
        if (ctx != NULL)
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    *saved_unit = ctx->compat.active_texture;
    ctx->compat.active_texture = unit;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesActiveTexture != NULL)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + unit);
    return 1;
}

static void explicit_unit_apply_end(struct mgl_context *ctx, unsigned saved_unit) {
    if (ctx == NULL)
        return;
    ctx->compat.active_texture = saved_unit;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesActiveTexture != NULL)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + saved_unit);
}

int mgl_texture_apply_image_2d_tight_on_unit(struct mgl_context *ctx, unsigned unit, GLenum target,
                                             GLint level, GLint internalformat, GLsizei width,
                                             GLsizei height, GLint border, GLenum format,
                                             GLenum type, const void *data, size_t data_size) {
    unsigned saved_unit;
    int result;
    if (!explicit_unit_apply_begin(ctx, unit, &saved_unit))
        return 0;
    result = mgl_texture_apply_image_2d_tight(ctx, target, level, internalformat, width, height,
                                              border, format, type, data, data_size);
    explicit_unit_apply_end(ctx, saved_unit);
    return result;
}

int mgl_texture_apply_sub_image_2d_tight_on_unit(struct mgl_context *ctx, unsigned unit,
                                                 GLenum target, GLint level, GLint xoffset,
                                                 GLint yoffset, GLsizei width, GLsizei height,
                                                 GLenum format, GLenum type, const void *data,
                                                 size_t data_size) {
    unsigned saved_unit;
    int result;
    if (!explicit_unit_apply_begin(ctx, unit, &saved_unit))
        return 0;
    result = mgl_texture_apply_sub_image_2d_tight(ctx, target, level, xoffset, yoffset, width,
                                                  height, format, type, data, data_size);
    explicit_unit_apply_end(ctx, saved_unit);
    return result;
}

int mgl_texture_apply_image_3d_tight_on_unit(struct mgl_context *ctx, unsigned unit, GLenum target,
                                             GLint level, GLint internalformat, GLsizei width,
                                             GLsizei height, GLsizei depth, GLint border,
                                             GLenum format, GLenum type, const void *data,
                                             size_t data_size) {
    unsigned saved_unit;
    int result;
    if (!explicit_unit_apply_begin(ctx, unit, &saved_unit))
        return 0;
    result = mgl_texture_apply_image_3d_tight(ctx, target, level, internalformat, width, height,
                                              depth, border, format, type, data, data_size);
    explicit_unit_apply_end(ctx, saved_unit);
    return result;
}

int mgl_texture_apply_sub_image_3d_tight_on_unit(struct mgl_context *ctx, unsigned unit,
                                                 GLenum target, GLint level, GLint xoffset,
                                                 GLint yoffset, GLint zoffset, GLsizei width,
                                                 GLsizei height, GLsizei depth, GLenum format,
                                                 GLenum type, const void *data, size_t data_size) {
    unsigned saved_unit;
    int result;
    if (!explicit_unit_apply_begin(ctx, unit, &saved_unit))
        return 0;
    result =
        mgl_texture_apply_sub_image_3d_tight(ctx, target, level, xoffset, yoffset, zoffset, width,
                                             height, depth, format, type, data, data_size);
    explicit_unit_apply_end(ctx, saved_unit);
    return result;
}

int mgl_texture_apply_image_2d_tight_named_ext(struct mgl_context *ctx, GLuint texture,
                                               GLenum target, GLint level, GLint internalformat,
                                               GLsizei width, GLsizei height, GLint border,
                                               GLenum format, GLenum type, const void *data,
                                               size_t data_size) {
    struct mgl_texture *obj;
    GLenum object_target;
    GLuint *binding;
    GLuint saved;
    int result;
    if (ctx == NULL)
        return 0;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return 0;
    object_target = ext_dsa_texture_object_target(target);
    binding = active_texture_binding_slot(ctx, object_target);
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    saved = *binding;
    *binding = texture;
    dsa_native_bind_texture(ctx, obj);
    result = mgl_texture_apply_image_2d_tight(ctx, target, level, internalformat, width, height,
                                              border, format, type, data, data_size);
    *binding = saved;
    dsa_native_restore_texture(ctx, object_target);
    return result;
}

int mgl_texture_apply_sub_image_2d_tight_named_ext(struct mgl_context *ctx, GLuint texture,
                                                   GLenum target, GLint level, GLint xoffset,
                                                   GLint yoffset, GLsizei width, GLsizei height,
                                                   GLenum format, GLenum type, const void *data,
                                                   size_t data_size) {
    struct mgl_texture *obj;
    GLenum object_target;
    GLuint *binding;
    GLuint saved;
    int result;
    if (ctx == NULL)
        return 0;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return 0;
    object_target = ext_dsa_texture_object_target(target);
    binding = active_texture_binding_slot(ctx, object_target);
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    saved = *binding;
    *binding = texture;
    dsa_native_bind_texture(ctx, obj);
    result = mgl_texture_apply_sub_image_2d_tight(ctx, target, level, xoffset, yoffset, width,
                                                  height, format, type, data, data_size);
    *binding = saved;
    dsa_native_restore_texture(ctx, object_target);
    return result;
}

int mgl_texture_apply_image_3d_tight_named_ext(struct mgl_context *ctx, GLuint texture,
                                               GLenum target, GLint level, GLint internalformat,
                                               GLsizei width, GLsizei height, GLsizei depth,
                                               GLint border, GLenum format, GLenum type,
                                               const void *data, size_t data_size) {
    struct mgl_texture *obj;
    GLuint *binding;
    GLuint saved;
    int result;
    if (ctx == NULL)
        return 0;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return 0;
    binding = active_texture_binding_slot(ctx, target);
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    saved = *binding;
    *binding = texture;
    dsa_native_bind_texture(ctx, obj);
    result = mgl_texture_apply_image_3d_tight(ctx, target, level, internalformat, width, height,
                                              depth, border, format, type, data, data_size);
    *binding = saved;
    dsa_native_restore_texture(ctx, target);
    return result;
}

int mgl_texture_apply_sub_image_3d_tight_named_ext(struct mgl_context *ctx, GLuint texture,
                                                   GLenum target, GLint level, GLint xoffset,
                                                   GLint yoffset, GLint zoffset, GLsizei width,
                                                   GLsizei height, GLsizei depth, GLenum format,
                                                   GLenum type, const void *data,
                                                   size_t data_size) {
    struct mgl_texture *obj;
    GLuint *binding;
    GLuint saved;
    int result;
    if (ctx == NULL)
        return 0;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return 0;
    binding = active_texture_binding_slot(ctx, target);
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    saved = *binding;
    *binding = texture;
    dsa_native_bind_texture(ctx, obj);
    result =
        mgl_texture_apply_sub_image_3d_tight(ctx, target, level, xoffset, yoffset, zoffset, width,
                                             height, depth, format, type, data, data_size);
    *binding = saved;
    dsa_native_restore_texture(ctx, target);
    return result;
}

int mgl_texture_apply_compressed_image_2d_tight_named_ext(struct mgl_context *ctx, GLuint texture,
                                                          GLenum target, GLint level,
                                                          GLenum internalformat, GLsizei width,
                                                          GLsizei height, GLint border,
                                                          const void *data, size_t data_size) {
    struct mgl_texture *obj;
    GLenum object_target;
    GLuint *binding;
    GLuint saved;
    int result;
    if (ctx == NULL)
        return 0;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return 0;
    object_target = ext_dsa_texture_object_target(target);
    binding = active_texture_binding_slot(ctx, object_target);
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    saved = *binding;
    *binding = texture;
    dsa_native_bind_texture(ctx, obj);
    result = mgl_texture_apply_compressed_image_2d(ctx, target, level, internalformat, width,
                                                   height, border, data, data_size);
    *binding = saved;
    dsa_native_restore_texture(ctx, object_target);
    return result;
}

int mgl_texture_apply_compressed_sub_image_2d_tight_named_ext(struct mgl_context *ctx,
                                                              GLuint texture, GLenum target,
                                                              GLint level, GLint xoffset,
                                                              GLint yoffset, GLsizei width,
                                                              GLsizei height, GLenum format,
                                                              const void *data, size_t data_size) {
    struct mgl_texture *obj;
    GLenum object_target;
    GLuint *binding;
    GLuint saved;
    int result;
    if (ctx == NULL)
        return 0;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return 0;
    object_target = ext_dsa_texture_object_target(target);
    binding = active_texture_binding_slot(ctx, object_target);
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    saved = *binding;
    *binding = texture;
    dsa_native_bind_texture(ctx, obj);
    result = mgl_texture_apply_compressed_sub_image_2d(ctx, target, level, xoffset, yoffset, width,
                                                       height, format, data, data_size);
    *binding = saved;
    dsa_native_restore_texture(ctx, object_target);
    return result;
}

int mgl_texture_apply_compressed_image_3d_tight_named_ext(struct mgl_context *ctx, GLuint texture,
                                                          GLenum target, GLint level,
                                                          GLenum internalformat, GLsizei width,
                                                          GLsizei height, GLsizei depth,
                                                          GLint border, const void *data,
                                                          size_t data_size) {
    struct mgl_texture *obj;
    GLuint *binding;
    GLuint saved;
    int result;
    if (ctx == NULL)
        return 0;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return 0;
    binding = active_texture_binding_slot(ctx, target);
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    saved = *binding;
    *binding = texture;
    dsa_native_bind_texture(ctx, obj);
    result = mgl_texture_apply_compressed_image_3d(ctx, target, level, internalformat, width,
                                                   height, depth, border, data, data_size);
    *binding = saved;
    dsa_native_restore_texture(ctx, target);
    return result;
}

int mgl_texture_apply_compressed_sub_image_3d_tight_named_ext(
    struct mgl_context *ctx, GLuint texture, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format,
    const void *data, size_t data_size) {
    struct mgl_texture *obj;
    GLuint *binding;
    GLuint saved;
    int result;
    if (ctx == NULL)
        return 0;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return 0;
    binding = active_texture_binding_slot(ctx, target);
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    saved = *binding;
    *binding = texture;
    dsa_native_bind_texture(ctx, obj);
    result =
        mgl_texture_apply_compressed_sub_image_3d(ctx, target, level, xoffset, yoffset, zoffset,
                                                  width, height, depth, format, data, data_size);
    *binding = saved;
    dsa_native_restore_texture(ctx, target);
    return result;
}

int mgl_texture_apply_compressed_image_2d_tight_on_unit(struct mgl_context *ctx, unsigned unit,
                                                        GLenum target, GLint level,
                                                        GLenum internalformat, GLsizei width,
                                                        GLsizei height, GLint border,
                                                        const void *data, size_t data_size) {
    unsigned saved_unit;
    int result;
    if (!explicit_unit_apply_begin(ctx, unit, &saved_unit))
        return 0;
    result = mgl_texture_apply_compressed_image_2d(ctx, target, level, internalformat, width,
                                                   height, border, data, data_size);
    explicit_unit_apply_end(ctx, saved_unit);
    return result;
}

int mgl_texture_apply_compressed_sub_image_2d_tight_on_unit(struct mgl_context *ctx, unsigned unit,
                                                            GLenum target, GLint level,
                                                            GLint xoffset, GLint yoffset,
                                                            GLsizei width, GLsizei height,
                                                            GLenum format, const void *data,
                                                            size_t data_size) {
    unsigned saved_unit;
    int result;
    if (!explicit_unit_apply_begin(ctx, unit, &saved_unit))
        return 0;
    result = mgl_texture_apply_compressed_sub_image_2d(ctx, target, level, xoffset, yoffset, width,
                                                       height, format, data, data_size);
    explicit_unit_apply_end(ctx, saved_unit);
    return result;
}

int mgl_texture_apply_compressed_image_3d_tight_on_unit(struct mgl_context *ctx, unsigned unit,
                                                        GLenum target, GLint level,
                                                        GLenum internalformat, GLsizei width,
                                                        GLsizei height, GLsizei depth, GLint border,
                                                        const void *data, size_t data_size) {
    unsigned saved_unit;
    int result;
    if (!explicit_unit_apply_begin(ctx, unit, &saved_unit))
        return 0;
    result = mgl_texture_apply_compressed_image_3d(ctx, target, level, internalformat, width,
                                                   height, depth, border, data, data_size);
    explicit_unit_apply_end(ctx, saved_unit);
    return result;
}

int mgl_texture_apply_compressed_sub_image_3d_tight_on_unit(
    struct mgl_context *ctx, unsigned unit, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format,
    const void *data, size_t data_size) {
    unsigned saved_unit;
    int result;
    if (!explicit_unit_apply_begin(ctx, unit, &saved_unit))
        return 0;
    result =
        mgl_texture_apply_compressed_sub_image_3d(ctx, target, level, xoffset, yoffset, zoffset,
                                                  width, height, depth, format, data, data_size);
    explicit_unit_apply_end(ctx, saved_unit);
    return result;
}

void glBindMultiTexture(GLenum texunit, GLenum target, GLuint texture) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glBindMultiTexture",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"texture", (uint64_t)(uintptr_t)texture}},
                        3u);
    struct mgl_texture *obj = NULL;
    GLuint native_name = 0u;
    unsigned unit;
    if (ctx == NULL)
        return;
    /* List compilation defers texture binds to replay. Active Begin/End rejection preserves
     * canonical and native texture bindings. */
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_bind_multi_texture(ctx, texunit, target, texture))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_bind_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_bind_multi_texture(ctx, texunit, target, texture))
        return;
    if (texture != 0u) {
        (void)pthread_mutex_lock(&ctx->share_group->lock);
        obj = mgl_texture_find(ctx, texture);
        if (obj != NULL && obj->target != 0u && obj->target != target) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        obj = mgl_texture_ensure(ctx, texture, target, 1);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        if (obj == NULL || obj->native_name == 0u) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        native_name = obj->native_name;
    }
    if (target == GL_TEXTURE_1D) {
        ctx->texture_1d_binding[unit] = texture;
        ctx->texture_2d_prefer_1d[unit] = texture != 0u;
        ctx->texture_2d_prefer_rectangle[unit] = GL_FALSE;
    } else if (target == GL_TEXTURE_2D)
        ctx->texture_2d_binding[unit] = texture;
    else if (target == GL_TEXTURE_RECTANGLE)
        ctx->texture_rectangle_binding[unit] = texture;
    else if (target == GL_TEXTURE_3D)
        ctx->texture_3d_binding[unit] = texture;
    else if (target == GL_TEXTURE_2D_ARRAY)
        ctx->texture_2d_array_binding[unit] = texture;
    else if (target == GL_TEXTURE_BUFFER)
        ctx->texture_buffer_binding[unit] = texture;
    else if (target == GL_TEXTURE_CUBE_MAP)
        ctx->texture_cube_binding[unit] = texture;
    else if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
        ctx->texture_cube_array_binding[unit] = texture;
    else if (target == GL_TEXTURE_2D_MULTISAMPLE)
        ctx->texture_2d_multisample_binding[unit] = texture;
    else
        ctx->texture_2d_multisample_array_binding[unit] = texture;
    if (target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE) {
        ctx->texture_2d_prefer_rectangle[unit] = target == GL_TEXTURE_RECTANGLE && texture != 0u;
        ctx->texture_2d_prefer_1d[unit] = GL_FALSE;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesActiveTexture != NULL &&
        ctx->native_dispatch->glesBindTexture != NULL) {
        mgl_native_select_active_texture(ctx, texunit);
        if (target != GL_TEXTURE_BUFFER || native_texture_buffer_supported(ctx))
            mgl_texture_bind_canonical_native_unit(ctx, mgl_texture_native_target(target),
                                                   native_name, unit);
        if (target == GL_TEXTURE_RECTANGLE && obj != NULL && !obj->rectangle_native_initialized &&
            ctx->native_dispatch->glesTexParameteri != NULL) {
            ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                                    GL_LINEAR);
            ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                                    GL_CLAMP_TO_EDGE);
            ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                                    GL_CLAMP_TO_EDGE);
            obj->rectangle_native_initialized = GL_TRUE;
        }
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + ctx->compat.active_texture);
    }
}

void glBindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture) {
    glBindMultiTexture(texunit, target, texture);
}

void glMultiTexBuffer(GLenum texunit, GLenum target, GLenum internalformat, GLuint buffer) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glMultiTexBuffer",
        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                       {"target", (uint64_t)(uintptr_t)target},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"buffer", (uint64_t)(uintptr_t)buffer}},
        4u);
    struct mgl_texture *texture;
    unsigned unit, saved_unit;
    if (ctx == NULL)
        return;
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (target != GL_TEXTURE_BUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_multi_tex_buffer(ctx, texunit, target, internalformat, buffer))
        return;
    if (!explicit_unit_apply_begin(ctx, unit, &saved_unit))
        return;
    texture = mgl_bound_texture(ctx, GL_TEXTURE_BUFFER);
    if (texture == NULL)
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    else
        (void)texture_buffer_attach(ctx, texture, internalformat, buffer, 0, 0, 1);
    explicit_unit_apply_end(ctx, saved_unit);
}

void glMultiTexBufferEXT(GLenum texunit, GLenum target, GLenum internalformat, GLuint buffer) {
    glMultiTexBuffer(texunit, target, internalformat, buffer);
}

static void realize_texture_param_on_unit(struct mgl_context *ctx, unsigned unit, GLenum target,
                                          struct mgl_texture *obj, GLenum pname, GLint param) {
    if (ctx == NULL || obj == NULL || pname == GL_DEPTH_TEXTURE_MODE ||
        pname == GL_TEXTURE_LOD_BIAS || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesTexParameteri == NULL)
        return;
    if (ctx->native_dispatch->glesActiveTexture != NULL) {
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + unit);
    }
    if (ctx->native_dispatch->glesBindTexture != NULL)
        mgl_texture_bind_native_unit(ctx, mgl_texture_native_target(target), obj->native_name,
                                     unit);
    if ((pname == GL_TEXTURE_BASE_LEVEL || pname == GL_TEXTURE_MAX_LEVEL) &&
        obj->immutable_format && !obj->native_immutable_storage) {
        native_realize_immutable_level_range(ctx, obj, target);
    } else {
        if (pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_R)
            param = (GLint)native_wrap_mode(param);
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_TEX_PARAMETER);
        ctx->native_dispatch->glesTexParameteri(mgl_texture_native_target(target), pname, param);
    }
    if (target == GL_TEXTURE_1D && ctx->native_dispatch->glesBindTexture != NULL) {
        struct mgl_texture *bound2d = mgl_texture_find(ctx, ctx->texture_2d_binding[unit]);
        mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D,
                                     bound2d != NULL ? bound2d->native_name : 0u, unit);
    }
    if (ctx->native_dispatch->glesActiveTexture != NULL) {
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + ctx->compat.active_texture);
    }
}

static void realize_texture_float_param_on_unit(struct mgl_context *ctx, unsigned unit,
                                                GLenum target, struct mgl_texture *obj,
                                                GLenum pname, GLfloat param) {
    if (ctx == NULL || obj == NULL ||
        (pname == GL_TEXTURE_PRIORITY || pname == GL_TEXTURE_LOD_BIAS) ||
        ctx->native_dispatch == NULL || ctx->native_dispatch->glesTexParameterf == NULL)
        return;
    if (ctx->native_dispatch->glesActiveTexture != NULL)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + unit);
    if (ctx->native_dispatch->glesBindTexture != NULL)
        mgl_texture_bind_native_unit(ctx, mgl_texture_native_target(target), obj->native_name,
                                     unit);
    ctx->native_dispatch->glesTexParameterf(mgl_texture_native_target(target), pname, param);
    if (target == GL_TEXTURE_1D && ctx->native_dispatch->glesBindTexture != NULL) {
        struct mgl_texture *bound2d = mgl_texture_find(ctx, ctx->texture_2d_binding[unit]);
        mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D,
                                     bound2d != NULL ? bound2d->native_name : 0u, unit);
    }
    if (ctx->native_dispatch->glesActiveTexture != NULL)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + ctx->compat.active_texture);
}

static void realize_texture_border_f_on_unit(struct mgl_context *ctx, unsigned unit, GLenum target,
                                             struct mgl_texture *obj, const GLfloat *params) {
    if (ctx == NULL || obj == NULL || params == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesTexParameterfv == NULL)
        return;
    if (ctx->native_dispatch->glesActiveTexture != NULL)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + unit);
    if (ctx->native_dispatch->glesBindTexture != NULL)
        mgl_texture_bind_native_unit(ctx, mgl_texture_native_target(target), obj->native_name,
                                     unit);
    ctx->native_dispatch->glesTexParameterfv(mgl_texture_native_target(target),
                                             GL_TEXTURE_BORDER_COLOR, params);
    if (target == GL_TEXTURE_1D && ctx->native_dispatch->glesBindTexture != NULL) {
        struct mgl_texture *bound2d = mgl_texture_find(ctx, ctx->texture_2d_binding[unit]);
        mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D,
                                     bound2d != NULL ? bound2d->native_name : 0u, unit);
    }
    if (ctx->native_dispatch->glesActiveTexture != NULL)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + ctx->compat.active_texture);
}

static void realize_texture_border_i_on_unit(struct mgl_context *ctx, unsigned unit, GLenum target,
                                             struct mgl_texture *obj, const GLint *params) {
    if (ctx == NULL || obj == NULL || params == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesTexParameterIiv == NULL)
        return;
    if (ctx->native_dispatch->glesActiveTexture != NULL)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + unit);
    if (ctx->native_dispatch->glesBindTexture != NULL)
        mgl_texture_bind_native_unit(ctx, mgl_texture_native_target(target), obj->native_name,
                                     unit);
    ctx->native_dispatch->glesTexParameterIiv(mgl_texture_native_target(target),
                                              GL_TEXTURE_BORDER_COLOR, params);
    if (target == GL_TEXTURE_1D && ctx->native_dispatch->glesBindTexture != NULL) {
        struct mgl_texture *bound2d = mgl_texture_find(ctx, ctx->texture_2d_binding[unit]);
        mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D,
                                     bound2d != NULL ? bound2d->native_name : 0u, unit);
    }
    if (ctx->native_dispatch->glesActiveTexture != NULL)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + ctx->compat.active_texture);
}

static void realize_texture_border_ui_on_unit(struct mgl_context *ctx, unsigned unit, GLenum target,
                                              struct mgl_texture *obj, const GLuint *params) {
    if (ctx == NULL || obj == NULL || params == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesTexParameterIuiv == NULL)
        return;
    if (ctx->native_dispatch->glesActiveTexture != NULL)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + unit);
    if (ctx->native_dispatch->glesBindTexture != NULL)
        mgl_texture_bind_native_unit(ctx, mgl_texture_native_target(target), obj->native_name,
                                     unit);
    ctx->native_dispatch->glesTexParameterIuiv(mgl_texture_native_target(target),
                                               GL_TEXTURE_BORDER_COLOR, params);
    if (target == GL_TEXTURE_1D && ctx->native_dispatch->glesBindTexture != NULL) {
        struct mgl_texture *bound2d = mgl_texture_find(ctx, ctx->texture_2d_binding[unit]);
        mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D,
                                     bound2d != NULL ? bound2d->native_name : 0u, unit);
    }
    if (ctx->native_dispatch->glesActiveTexture != NULL)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + ctx->compat.active_texture);
}

void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_TEXTURE_STATE);
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_TEX_PARAMETER);
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTexParameteri",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"param", (uint64_t)(uintptr_t)param}},
                        3u);
    struct mgl_texture *obj;
    int result;
    if (ctx == NULL)
        return;
    /* Begin/End legality takes precedence over texture parameter validation. Display-list
     * compilation defers the error to replay. */
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_tex_parameter(ctx, target, pname, param))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!texture_anisotropy_validate(ctx, pname, (GLfloat)param))
        return;
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target == GL_TEXTURE_RECTANGLE &&
        ((pname == GL_TEXTURE_MIN_FILTER && param != GL_NEAREST && param != GL_LINEAR) ||
         ((pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T ||
           pname == GL_TEXTURE_WRAP_R) &&
          param != GL_CLAMP_TO_EDGE && param != GL_CLAMP_TO_BORDER && param != GL_CLAMP) ||
         ((pname == GL_TEXTURE_BASE_LEVEL || pname == GL_TEXTURE_MAX_LEVEL) && param != 0))) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (texture_is_multisample(obj) &&
        (multisample_sampler_parameter(pname) || (pname == GL_TEXTURE_BASE_LEVEL && param != 0))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname == GL_TEXTURE_PRIORITY) {
        if (mgl_list_record_tex_parameter(ctx, target, pname, param))
            return;
        obj->priority = normalized_texture_int(param);
        return;
    }
    result = validate_texture_param(pname, param);
    if (result < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }

    if (!result) {
        GLenum error =
            (pname >= GL_TEXTURE_SWIZZLE_R && pname <= GL_TEXTURE_SWIZZLE_A) ? GL_INVALID_OPERATION
            : (pname == GL_TEXTURE_BASE_LEVEL || pname == GL_TEXTURE_MAX_LEVEL) ? GL_INVALID_VALUE
                                                                                : GL_INVALID_ENUM;
        mgl_error_set(&ctx->errors, error);
        return;
    }
    if (mgl_list_record_tex_parameter(ctx, target, pname, param))
        return;
    apply_texture_param(obj, pname, param);
    if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT)
        obj->max_anisotropy = texture_anisotropy_clamp(ctx, (GLfloat)param);
    realize_texture_param_on_unit(ctx, ctx->compat.active_texture, target, obj, pname, param);
}

void glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTexParameterf",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname}},
                        2u);
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    /* Begin/End legality precedes texture parameter validation. Display-list compilation records
     * the command for replay-time validation. */
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_tex_parameter_f(ctx, target, pname, param))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!texture_anisotropy_validate(ctx, pname, (GLfloat)param))
        return;
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture_is_multisample(obj) && multisample_sampler_parameter(pname)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    /* GLES 3 provides no per-texture LOD-bias state. Zero is accepted; nonzero values are
     * rejected. */
    if (!validate_texture_float_param(pname)) {
        glTexParameteri(target, pname, (GLint)param);
        return;
    }
    if (mgl_list_record_tex_parameter_f(ctx, target, pname, param))
        return;
    apply_texture_float_param(obj, pname, param);
    if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT)
        obj->max_anisotropy = texture_anisotropy_clamp(ctx, param);
    realize_texture_float_param_on_unit(ctx, ctx->compat.active_texture, target, obj, pname, param);
}

void glTexParameteriv(GLenum target, GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTexParameteriv",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    struct mgl_texture *obj;
    GLfloat converted[4];
    if (ctx == NULL || params == NULL)
        return;
    /* Scalar pnames delegate to the scalar
     * entry point (one supplied element is sufficient). For a border-color
     * vector, record all four owned elements before replay-time rejection. */
    if (ctx->immediate.active && pname == GL_TEXTURE_BORDER_COLOR) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_tex_parameter_iv4(ctx, target, pname, params))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname == GL_TEXTURE_SWIZZLE_RGBA) {
        unsigned i;
        for (i = 0u; i < 4u; ++i) {
            GLint value = params[i];
            if (value != GL_RED && value != GL_GREEN && value != GL_BLUE && value != GL_ALPHA &&
                value != GL_ZERO && value != GL_ONE) {
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return;
            }
        }
        for (i = 0u; i < 4u; ++i)
            glTexParameteri(target, GL_TEXTURE_SWIZZLE_R + i, params[i]);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glTexParameteri(target, pname, params[0]);
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture_is_multisample(obj)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_tex_parameter_iv4(ctx, target, pname, params))
        return;
    texture_border_color_from_ints(converted, params);
    apply_texture_border_color(obj, converted);
    realize_texture_border_f_on_unit(ctx, ctx->compat.active_texture, target, obj,
                                     obj->border_color);
}

void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTexParameterfv",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    struct mgl_texture *obj;
    if (ctx == NULL || params == NULL)
        return;

    if (ctx->immediate.active && pname == GL_TEXTURE_BORDER_COLOR) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_tex_parameter_fv4(ctx, target, pname, params))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname == GL_TEXTURE_SWIZZLE_RGBA) {
        unsigned i;
        for (i = 0u; i < 4u; ++i) {
            GLint value = (GLint)params[i];
            if (value != GL_RED && value != GL_GREEN && value != GL_BLUE && value != GL_ALPHA &&
                value != GL_ZERO && value != GL_ONE) {
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return;
            }
        }
        for (i = 0u; i < 4u; ++i)
            glTexParameteri(target, GL_TEXTURE_SWIZZLE_R + i, (GLint)params[i]);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glTexParameterf(target, pname, params[0]);
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture_is_multisample(obj)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_tex_parameter_fv4(ctx, target, pname, params))
        return;
    apply_texture_border_color(obj, params);
    realize_texture_border_f_on_unit(ctx, ctx->compat.active_texture, target, obj,
                                     obj->border_color);
}

void glTexParameterIiv(GLenum target, GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTexParameterIiv",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    struct mgl_texture *obj;
    if (ctx == NULL || params == NULL)
        return;
    /* During list compilation, record vector border-color commands. Begin/End
     * rejection precedes parameter validation and GLES submission. */
    if (ctx->immediate.active && pname == GL_TEXTURE_BORDER_COLOR) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_tex_parameter_iiv(ctx, target, pname, params))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glTexParameteriv(target, pname, params);
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture_is_multisample(obj)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_tex_parameter_iiv(ctx, target, pname, params))
        return;
    apply_texture_border_color_i(obj, params);
    realize_texture_border_i_on_unit(ctx, ctx->compat.active_texture, target, obj, params);
}

void glTexParameterIuiv(GLenum target, GLenum pname, const GLuint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTexParameterIuiv",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    struct mgl_texture *obj;
    if (ctx == NULL || params == NULL)
        return;
    /* Integer border-color list commands are recorded for replay; active Begin/End rejection
     * preserves texture state. */
    if (ctx->immediate.active && pname == GL_TEXTURE_BORDER_COLOR) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_tex_parameter_iuiv(ctx, target, pname, params))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glTexParameteri(target, pname, (GLint)params[0]);
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture_is_multisample(obj)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_tex_parameter_iuiv(ctx, target, pname, params))
        return;
    apply_texture_border_color_ui(obj, params);
    realize_texture_border_ui_on_unit(ctx, ctx->compat.active_texture, target, obj, params);
}

void glGetTexParameteriv(GLenum target, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTexParameteriv",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    struct mgl_texture *obj;
    if (ctx == NULL || params == NULL)
        return;
    /* Begin/End queries reject before target/pname validation and preserve the output buffer. */
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
            *params = obj->min_filter;
            return;
        case GL_TEXTURE_MAG_FILTER:
            *params = obj->mag_filter;
            return;
        case GL_TEXTURE_WRAP_S:
            *params = obj->wrap_s;
            return;
        case GL_TEXTURE_WRAP_T:
            *params = obj->wrap_t;
            return;
        case GL_TEXTURE_WRAP_R:
            *params = obj->wrap_r;
            return;
        case GL_TEXTURE_MIN_LOD:
            *params = texture_lod_to_int(obj->min_lod);
            return;
        case GL_TEXTURE_MAX_LOD:
            *params = texture_lod_to_int(obj->max_lod);
            return;
        case GL_TEXTURE_LOD_BIAS:
            *params = texture_lod_to_int(obj->lod_bias);
            return;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
            if (!texture_anisotropy_supported(ctx)) {
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return;
            }
            *params = texture_lod_to_int(obj->max_anisotropy);
            return;
        case GL_TEXTURE_BASE_LEVEL:
            *params = obj->base_level;
            return;
        case GL_TEXTURE_MAX_LEVEL:
            *params = obj->max_level;
            return;
        case GL_TEXTURE_IMMUTABLE_FORMAT:
            *params = obj->immutable_format ? (GLint)GL_TRUE : (GLint)GL_FALSE;
            return;
        case GL_TEXTURE_IMMUTABLE_LEVELS:
            *params = obj->immutable_levels;
            return;
        case GL_TEXTURE_VIEW_MIN_LEVEL:
            *params = obj->view_root != NULL ? (GLint)obj->view_min_level : 0;
            return;
        case GL_TEXTURE_VIEW_NUM_LEVELS:
            *params = obj->view_root != NULL ? (GLint)obj->view_num_levels : 0;
            return;
        case GL_TEXTURE_VIEW_MIN_LAYER:
            *params = obj->view_root != NULL ? (GLint)obj->view_min_layer : 0;
            return;
        case GL_TEXTURE_VIEW_NUM_LAYERS:
            *params = obj->view_root != NULL ? (GLint)obj->view_num_layers : 0;
            return;
        case GL_TEXTURE_COMPARE_MODE:
            *params = obj->compare_mode;
            return;
        case GL_TEXTURE_COMPARE_FUNC:
            *params = obj->compare_func;
            return;
        case GL_DEPTH_TEXTURE_MODE:
            *params = obj->depth_texture_mode;
            break;
        case GL_DEPTH_STENCIL_TEXTURE_MODE:
            *params = obj->depth_stencil_texture_mode;
            break;
        case GL_IMAGE_FORMAT_COMPATIBILITY_TYPE:
            *params = GL_IMAGE_FORMAT_COMPATIBILITY_BY_SIZE;
            return;
        case GL_TEXTURE_SWIZZLE_R:
        case GL_TEXTURE_SWIZZLE_G:
        case GL_TEXTURE_SWIZZLE_B:
        case GL_TEXTURE_SWIZZLE_A:
            *params = obj->swizzle[pname - GL_TEXTURE_SWIZZLE_R];
            return;
        case GL_TEXTURE_SWIZZLE_RGBA:
            memcpy(params, obj->swizzle, 4u * sizeof(GLint));
            return;
        case GL_TEXTURE_PRIORITY:
            *params = texture_float_to_int(obj->priority);
            return;
        case GL_TEXTURE_BORDER_COLOR:
            params[0] = texture_float_to_int(obj->border_color[0]);
            params[1] = texture_float_to_int(obj->border_color[1]);
            params[2] = texture_float_to_int(obj->border_color[2]);
            params[3] = texture_float_to_int(obj->border_color[3]);
            return;
        case GL_TEXTURE_RESIDENT:
            *params = (GLint)GL_TRUE;
            return;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
    }
}

void glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTexParameterfv",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    struct mgl_texture *obj;
    GLint value = 0;
    if (ctx == NULL || params == NULL)
        return;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname == GL_TEXTURE_SWIZZLE_RGBA) {
        unsigned i;
        for (i = 0u; i < 4u; ++i)
            params[i] = (GLfloat)obj->swizzle[i];
        return;
    }
    if (pname == GL_TEXTURE_MIN_LOD) {
        *params = obj->min_lod;
        return;
    }
    if (pname == GL_TEXTURE_MAX_LOD) {
        *params = obj->max_lod;
        return;
    }
    if (pname == GL_TEXTURE_LOD_BIAS) {
        *params = obj->lod_bias;
        return;
    }
    if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT) {
        if (!texture_anisotropy_supported(ctx)) {
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
        }
        *params = obj->max_anisotropy;
        return;
    }
    if (pname == GL_TEXTURE_PRIORITY) {
        *params = obj->priority;
        return;
    }
    if (pname == GL_TEXTURE_BORDER_COLOR) {
        memcpy(params, obj->border_color, sizeof(obj->border_color));
        return;
    }
    if (pname == GL_TEXTURE_RESIDENT) {
        *params = 1.0f;
        return;
    }
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetTexParameteriv(target, pname, &value);
    if (ctx->errors.sequence == error_sequence_before)
        *params = (GLfloat)value;
}

void glGetTexParameterIiv(GLenum target, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTexParameterIiv",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    struct mgl_texture *obj;
    if (ctx == NULL || params == NULL)
        return;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glGetTexParameteriv(target, pname, params);
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    get_texture_border_color_i(obj, params);
}

void glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTexParameterIuiv",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    struct mgl_texture *obj;
    GLint value = 0;
    if (ctx == NULL || params == NULL)
        return;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname == GL_TEXTURE_SWIZZLE_RGBA) {
        GLint values[4];
        unsigned i;
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetTexParameteriv(target, pname, values);
        if (ctx->errors.sequence == error_sequence_before)
            for (i = 0u; i < 4u; ++i)
                params[i] = (GLuint)values[i];
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetTexParameteriv(target, pname, &value);
        if (ctx->errors.sequence == error_sequence_before)
            params[0] = (GLuint)value;
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    get_texture_border_color_ui(obj, params);
}

void glMultiTexParameteri(GLenum texunit, GLenum target, GLenum pname, GLint param) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glMultiTexParameteri",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"param", (uint64_t)(uintptr_t)param}},
                        4u);
    struct mgl_texture *obj;
    unsigned unit;
    int result;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {

        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_multi_tex_parameter(ctx, texunit, target, pname, param))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!texture_anisotropy_validate(ctx, pname, (GLfloat)param))
        return;
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = texture_for_unit(ctx, unit, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname == GL_TEXTURE_PRIORITY) {
        if (mgl_list_record_multi_tex_parameter(ctx, texunit, target, pname, param))
            return;
        obj->priority = normalized_texture_int(param);
        return;
    }
    result = validate_texture_param(pname, param);
    if (result < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!result) {
        GLenum error =
            (pname >= GL_TEXTURE_SWIZZLE_R && pname <= GL_TEXTURE_SWIZZLE_A) ? GL_INVALID_OPERATION
            : (pname == GL_TEXTURE_BASE_LEVEL || pname == GL_TEXTURE_MAX_LEVEL) ? GL_INVALID_VALUE
                                                                                : GL_INVALID_ENUM;
        mgl_error_set(&ctx->errors, error);
        return;
    }
    if (mgl_list_record_multi_tex_parameter(ctx, texunit, target, pname, param))
        return;
    apply_texture_param(obj, pname, param);
    if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT)
        obj->max_anisotropy = texture_anisotropy_clamp(ctx, (GLfloat)param);
    realize_texture_param_on_unit(ctx, unit, target, obj, pname, param);
}

void glMultiTexParameteriv(GLenum texunit, GLenum target, GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glMultiTexParameteriv",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    unsigned unit;
    GLfloat converted[4];
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {

        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            (pname == GL_TEXTURE_BORDER_COLOR
                 ? mgl_list_record_multi_tex_parameter_iv4(ctx, texunit, target, pname, params)
                 : mgl_list_record_multi_tex_parameter(ctx, texunit, target, pname, params[0])))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glMultiTexParameteri(texunit, target, pname, params[0]);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = texture_for_unit(ctx, unit, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_multi_tex_parameter_iv4(ctx, texunit, target, pname, params))
        return;
    texture_border_color_from_ints(converted, params);
    apply_texture_border_color(obj, converted);
}

void glMultiTexParameterf(GLenum texunit, GLenum target, GLenum pname, GLfloat param) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glMultiTexParameterf",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname}},
                        3u);
    struct mgl_texture *obj;
    unsigned unit;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {

        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_multi_tex_parameter_f(ctx, texunit, target, pname, param))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!texture_anisotropy_validate(ctx, pname, (GLfloat)param))
        return;
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = texture_for_unit(ctx, unit, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }

    if (!validate_texture_float_param(pname)) {
        glMultiTexParameteri(texunit, target, pname, (GLint)param);
        return;
    }
    if (mgl_list_record_multi_tex_parameter_f(ctx, texunit, target, pname, param))
        return;
    apply_texture_float_param(obj, pname, param);
    if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT)
        obj->max_anisotropy = texture_anisotropy_clamp(ctx, param);
    realize_texture_float_param_on_unit(ctx, unit, target, obj, pname, param);
}

void glMultiTexParameterfv(GLenum texunit, GLenum target, GLenum pname, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glMultiTexParameterfv",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    unsigned unit;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {

        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            (pname == GL_TEXTURE_BORDER_COLOR
                 ? mgl_list_record_multi_tex_parameter_fv4(ctx, texunit, target, pname, params)
                 : mgl_list_record_multi_tex_parameter_f(ctx, texunit, target, pname, params[0])))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glMultiTexParameterf(texunit, target, pname, params[0]);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = texture_for_unit(ctx, unit, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_multi_tex_parameter_fv4(ctx, texunit, target, pname, params))
        return;
    apply_texture_border_color(obj, params);
}

void glMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glMultiTexParameterIivEXT",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    unsigned unit;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {

        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_multi_tex_parameter_iiv(ctx, texunit, target, pname, params))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glMultiTexParameteriv(texunit, target, pname, params);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = texture_for_unit(ctx, unit, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture_is_multisample(obj)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_multi_tex_parameter_iiv(ctx, texunit, target, pname, params))
        return;
    apply_texture_border_color_i(obj, params);
    realize_texture_border_i_on_unit(ctx, unit, target, obj, params);
}

void glMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname, const GLuint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glMultiTexParameterIuivEXT",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    unsigned unit;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {

        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_multi_tex_parameter_iuiv(ctx, texunit, target, pname, params))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glMultiTexParameteri(texunit, target, pname, (GLint)params[0]);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = texture_for_unit(ctx, unit, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture_is_multisample(obj)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_multi_tex_parameter_iuiv(ctx, texunit, target, pname, params))
        return;
    apply_texture_border_color_ui(obj, params);
    realize_texture_border_ui_on_unit(ctx, unit, target, obj, params);
}

void glGetMultiTexParameteriv(GLenum texunit, GLenum target, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetMultiTexParameteriv",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    unsigned unit;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = texture_for_unit(ctx, unit, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
            *params = obj->min_filter;
            return;
        case GL_TEXTURE_MAG_FILTER:
            *params = obj->mag_filter;
            return;
        case GL_TEXTURE_WRAP_S:
            *params = obj->wrap_s;
            return;
        case GL_TEXTURE_WRAP_T:
            *params = obj->wrap_t;
            return;
        case GL_TEXTURE_WRAP_R:
            *params = obj->wrap_r;
            return;
        case GL_TEXTURE_MIN_LOD:
            *params = (GLint)obj->min_lod;
            return;
        case GL_TEXTURE_MAX_LOD:
            *params = (GLint)obj->max_lod;
            return;
        case GL_TEXTURE_LOD_BIAS:
            *params = texture_lod_to_int(obj->lod_bias);
            return;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
            if (!texture_anisotropy_supported(ctx)) {
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return;
            }
            *params = texture_lod_to_int(obj->max_anisotropy);
            return;
        case GL_TEXTURE_BASE_LEVEL:
            *params = obj->base_level;
            return;
        case GL_TEXTURE_MAX_LEVEL:
            *params = obj->max_level;
            return;
        case GL_TEXTURE_COMPARE_MODE:
            *params = obj->compare_mode;
            return;
        case GL_TEXTURE_COMPARE_FUNC:
            *params = obj->compare_func;
            return;
        case GL_DEPTH_TEXTURE_MODE:
            *params = obj->depth_texture_mode;
            break;
        case GL_DEPTH_STENCIL_TEXTURE_MODE:
            *params = obj->depth_stencil_texture_mode;
            break;
        case GL_IMAGE_FORMAT_COMPATIBILITY_TYPE:
            *params = GL_IMAGE_FORMAT_COMPATIBILITY_BY_SIZE;
            return;
        case GL_TEXTURE_SWIZZLE_R:
        case GL_TEXTURE_SWIZZLE_G:
        case GL_TEXTURE_SWIZZLE_B:
        case GL_TEXTURE_SWIZZLE_A:
            *params = obj->swizzle[pname - GL_TEXTURE_SWIZZLE_R];
            return;
        case GL_TEXTURE_PRIORITY:
            *params = texture_float_to_int(obj->priority);
            return;
        case GL_TEXTURE_BORDER_COLOR:
            params[0] = texture_float_to_int(obj->border_color[0]);
            params[1] = texture_float_to_int(obj->border_color[1]);
            params[2] = texture_float_to_int(obj->border_color[2]);
            params[3] = texture_float_to_int(obj->border_color[3]);
            return;
        case GL_TEXTURE_RESIDENT:
            *params = (GLint)GL_TRUE;
            return;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
    }
}

void glGetMultiTexParameterfv(GLenum texunit, GLenum target, GLenum pname, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetMultiTexParameterfv",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    unsigned unit;
    GLint value = 0;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = texture_for_unit(ctx, unit, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname == GL_TEXTURE_MIN_LOD) {
        *params = obj->min_lod;
        return;
    }
    if (pname == GL_TEXTURE_MAX_LOD) {
        *params = obj->max_lod;
        return;
    }
    if (pname == GL_TEXTURE_LOD_BIAS) {
        *params = obj->lod_bias;
        return;
    }
    if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT) {
        if (!texture_anisotropy_supported(ctx)) {
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
        }
        *params = obj->max_anisotropy;
        return;
    }
    if (pname == GL_TEXTURE_PRIORITY) {
        *params = obj->priority;
        return;
    }
    if (pname == GL_TEXTURE_BORDER_COLOR) {
        memcpy(params, obj->border_color, sizeof(obj->border_color));
        return;
    }
    if (pname == GL_TEXTURE_RESIDENT) {
        *params = 1.0f;
        return;
    }
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetMultiTexParameteriv(texunit, target, pname, &value);
    if (ctx->errors.sequence == error_sequence_before)
        *params = (GLfloat)value;
}

void glGetMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetMultiTexParameterIivEXT",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    unsigned unit;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glGetMultiTexParameteriv(texunit, target, pname, params);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = texture_for_unit(ctx, unit, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    get_texture_border_color_i(obj, params);
}

void glGetMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname, GLuint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetMultiTexParameterIuivEXT",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    unsigned unit;
    GLint value = 0;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetMultiTexParameteriv(texunit, target, pname, &value);
        if (ctx->errors.sequence == error_sequence_before)
            params[0] = (GLuint)value;
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = texture_for_unit(ctx, unit, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    get_texture_border_color_ui(obj, params);
}

void glMultiTexParameteriEXT(GLenum texunit, GLenum target, GLenum pname, GLint param) {
    glMultiTexParameteri(texunit, target, pname, param);
}

void glMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname, const GLint *params) {
    glMultiTexParameteriv(texunit, target, pname, params);
}

void glMultiTexParameterfEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat param) {
    glMultiTexParameterf(texunit, target, pname, param);
}

void glMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname, const GLfloat *params) {
    glMultiTexParameterfv(texunit, target, pname, params);
}

void glGetMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname, GLint *params) {
    glGetMultiTexParameteriv(texunit, target, pname, params);
}

void glGetMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat *params) {
    glGetMultiTexParameterfv(texunit, target, pname, params);
}

static unsigned char *pixel_destination(struct mgl_context *ctx, void *pixels, size_t bytes,
                                        struct mgl_buffer **out_buffer, size_t *out_offset) {
    if (out_buffer != NULL)
        *out_buffer = NULL;
    if (out_offset != NULL)
        *out_offset = 0u;
    if (ctx->pixel_pack_buffer != 0u) {
        struct mgl_buffer *buffer = mgl_buffer_find(ctx, ctx->pixel_pack_buffer);
        size_t offset = (size_t)(uintptr_t)pixels;
        if (buffer == NULL || buffer->mapped || offset > buffer->size ||
            bytes > buffer->size - offset)
            return NULL;
        if (out_buffer != NULL)
            *out_buffer = buffer;
        if (out_offset != NULL)
            *out_offset = offset;
        return mgl_buffer_shadow_ensure(ctx, buffer) ? buffer->shadow + offset : NULL;
    }
    return (unsigned char *)pixels;
}

static double depth_load(const unsigned char *src, GLenum type) {
    if (type == GL_UNSIGNED_BYTE)
        return (double)src[0] / 255.0;
    if (type == GL_UNSIGNED_SHORT) {
        uint16_t value;
        memcpy(&value, src, sizeof(value));
        return (double)value / 65535.0;
    }
    if (type == GL_UNSIGNED_INT) {
        uint32_t value;
        memcpy(&value, src, sizeof(value));
        return (double)value / 4294967295.0;
    }
    if (type == GL_FLOAT) {
        GLfloat value;
        memcpy(&value, src, sizeof(value));
        if (value < 0.0f)
            value = 0.0f;
        else if (value > 1.0f)
            value = 1.0f;
        return (double)value;
    }
    return 0.0;
}

static void depth_store(unsigned char *dst, GLenum type, double value) {
    if (value < 0.0)
        value = 0.0;
    else if (value > 1.0)
        value = 1.0;
    if (type == GL_UNSIGNED_BYTE) {
        dst[0] = (unsigned char)(value * 255.0 + 0.5);
    } else if (type == GL_UNSIGNED_SHORT) {
        uint16_t out = (uint16_t)(value * 65535.0 + 0.5);
        memcpy(dst, &out, sizeof(out));
    } else if (type == GL_UNSIGNED_INT) {
        uint32_t out = (uint32_t)(value * 4294967295.0 + 0.5);
        memcpy(dst, &out, sizeof(out));
    } else {
        GLfloat out = (GLfloat)value;
        memcpy(dst, &out, sizeof(out));
    }
}

static int texture_readback_format_compatible(GLenum source, GLenum destination) {
    if (source == GL_DEPTH_COMPONENT)
        return destination == GL_DEPTH_COMPONENT;
    if (source == GL_STENCIL_INDEX)
        return destination == GL_STENCIL_INDEX;
    if (source == GL_DEPTH_STENCIL)
        return destination == GL_DEPTH_STENCIL || destination == GL_DEPTH_COMPONENT ||
               destination == GL_STENCIL_INDEX;
    return destination != GL_DEPTH_COMPONENT && destination != GL_DEPTH_STENCIL &&
           destination != GL_STENCIL_INDEX;
}

static void texture_readback_noncolor_pixel(const unsigned char *src, GLenum source_format,
                                            GLenum source_type, unsigned char *dst, GLenum format,
                                            GLenum type) {
    double depth = 0.0;
    uint32_t stencil = 0u;
    if (source_format == GL_STENCIL_INDEX) {
        stencil = src[0];
    } else if (source_format == GL_DEPTH_STENCIL) {
        if (source_type == GL_UNSIGNED_INT_24_8) {
            uint32_t packed;
            memcpy(&packed, src, sizeof(packed));
            depth = (double)(packed >> 8u) / 16777215.0;
            stencil = packed & 255u;
        } else if (source_type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV) {
            float f;
            uint32_t packed;
            memcpy(&f, src, sizeof(f));
            memcpy(&packed, src + 4u, sizeof(packed));
            depth = (double)f;
            stencil = packed & 255u;
        }
    } else if (source_format == GL_DEPTH_COMPONENT) {
        depth = depth_load(src, source_type);
    }
    if (format == GL_STENCIL_INDEX) {
        dst[0] = (unsigned char)stencil;
    } else if (format == GL_DEPTH_COMPONENT) {
        depth_store(dst, type, depth);
    } else if (type == GL_UNSIGNED_INT_24_8) {
        uint32_t word = ((uint32_t)floor(fmin(1.0, fmax(0.0, depth)) * 16777215.0 + 0.5) << 8u) |
                        (stencil & 255u);
        memcpy(dst, &word, sizeof(word));
    } else if (type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV) {
        float f = (float)depth;
        uint32_t word = stencil & 255u;
        memcpy(dst, &f, sizeof(f));
        memcpy(dst + 4u, &word, sizeof(word));
    }
}

static float readback_half_to_float(uint16_t h);
static uint32_t compressed_readback_ufloat(double value, unsigned mantissa_bits);
static uint32_t compressed_readback_rgb9e5(const double rgba[4]);
static int compressed_readback_packed_layout(GLenum type, unsigned *channels, unsigned widths[4],
                                             unsigned *bytes, int *reverse);

static int texture_color_integer_format(GLenum format) {
    return format == GL_RED_INTEGER || format == GL_GREEN_INTEGER || format == GL_BLUE_INTEGER ||
           format == GL_RG_INTEGER || format == GL_RGB_INTEGER || format == GL_BGR_INTEGER ||
           format == GL_RGBA_INTEGER || format == GL_BGRA_INTEGER;
}

static unsigned texture_color_channels(GLenum format) {
    switch (format) {
        case GL_RED:
        case GL_RED_INTEGER:
        case GL_GREEN:
        case GL_GREEN_INTEGER:
        case GL_BLUE:
        case GL_BLUE_INTEGER:
        case GL_ALPHA:
        case GL_LUMINANCE:
        case GL_INTENSITY:
        case GL_COLOR_INDEX:
            return 1u;
        case GL_RG:
        case GL_RG_INTEGER:
        case GL_LUMINANCE_ALPHA:
            return 2u;
        case GL_RGB:
        case GL_RGB_INTEGER:
        case GL_BGR:
        case GL_BGR_INTEGER:
            return 3u;
        case GL_RGBA:
        case GL_RGBA_INTEGER:
        case GL_BGRA:
        case GL_BGRA_INTEGER:
            return 4u;
        default:
            return 0u;
    }
}

static double texture_color_load_scalar(const unsigned char *src, GLenum type, int integer) {
    switch (type) {
        case GL_UNSIGNED_BYTE:
            return integer ? (double)src[0] : (double)src[0] / 255.0;
        case GL_BYTE: {
            int8_t v;
            memcpy(&v, src, sizeof(v));
            return integer ? (double)v : fmax(-1.0, (double)v / 127.0);
        }
        case GL_UNSIGNED_SHORT: {
            uint16_t v;
            memcpy(&v, src, sizeof(v));
            return integer ? (double)v : (double)v / 65535.0;
        }
        case GL_SHORT: {
            int16_t v;
            memcpy(&v, src, sizeof(v));
            return integer ? (double)v : fmax(-1.0, (double)v / 32767.0);
        }
        case GL_UNSIGNED_INT: {
            uint32_t v;
            memcpy(&v, src, sizeof(v));
            return integer ? (double)v : (double)v / 4294967295.0;
        }
        case GL_INT: {
            int32_t v;
            memcpy(&v, src, sizeof(v));
            return integer ? (double)v : fmax(-1.0, (double)v / 2147483647.0);
        }
        case GL_HALF_FLOAT: {
            uint16_t h;
            memcpy(&h, src, sizeof(h));
            return (double)readback_half_to_float(h);
        }
        case GL_FLOAT: {
            float v;
            memcpy(&v, src, sizeof(v));
            return (double)v;
        }
        default:
            return 0.0;
    }
}

static double texture_color_load_ufloat(uint32_t v, unsigned mantissa_bits) {
    unsigned exponent = v >> mantissa_bits;
    unsigned fraction = v & ((1u << mantissa_bits) - 1u);
    if (exponent == 31u)
        return fraction ? NAN : INFINITY;
    if (exponent == 0u)
        return ldexp((double)fraction, -14 - (int)mantissa_bits);
    return ldexp((double)((1u << mantissa_bits) + fraction),
                 (int)exponent - 15 - (int)mantissa_bits);
}

static void texture_color_store_scalar(unsigned char *dst, GLenum type, double v, int integer) {
    if (type == GL_FLOAT) {
        float f = (float)v;
        memcpy(dst, &f, sizeof(f));
        return;
    }
    if (type == GL_HALF_FLOAT) {
        uint16_t h = float_to_half_bits((float)v);
        memcpy(dst, &h, sizeof(h));
        return;
    }
    if (type == GL_UNSIGNED_BYTE) {
        dst[0] = (uint8_t)(integer ? fmin(255.0, fmax(0.0, v))
                                   : floor(fmin(1.0, fmax(0.0, v)) * 255.0 + 0.5));
    } else if (type == GL_BYTE) {
        int8_t out = (int8_t)(integer ? fmin(127.0, fmax(-128.0, v))
                                      : round(fmin(1.0, fmax(-1.0, v)) * 127.0));
        memcpy(dst, &out, sizeof(out));
    } else if (type == GL_UNSIGNED_SHORT) {
        uint16_t out = (uint16_t)(integer ? fmin(65535.0, fmax(0.0, v))
                                          : floor(fmin(1.0, fmax(0.0, v)) * 65535.0 + 0.5));
        memcpy(dst, &out, sizeof(out));
    } else if (type == GL_SHORT) {
        int16_t out = (int16_t)(integer ? fmin(32767.0, fmax(-32768.0, v))
                                        : round(fmin(1.0, fmax(-1.0, v)) * 32767.0));
        memcpy(dst, &out, sizeof(out));
    } else if (type == GL_UNSIGNED_INT) {
        uint32_t out = (uint32_t)(integer ? fmin(4294967295.0, fmax(0.0, v))
                                          : floor(fmin(1.0, fmax(0.0, v)) * 4294967295.0 + 0.5));
        memcpy(dst, &out, sizeof(out));
    } else if (type == GL_INT) {
        int32_t out = (int32_t)(integer ? fmin(2147483647.0, fmax(-2147483648.0, v))
                                        : round(fmin(1.0, fmax(-1.0, v)) * 2147483647.0));
        memcpy(dst, &out, sizeof(out));
    }
}

static void convert_texture_color_texel(const unsigned char *src, GLenum src_format,
                                        GLenum src_type, unsigned char *dst, GLenum dst_format,
                                        GLenum dst_type) {
    double rgba[4] = {0.0, 0.0, 0.0, 1.0}, output[4] = {0.0, 0.0, 0.0, 1.0};
    unsigned src_channels = texture_color_channels(src_format);
    unsigned dst_channels = texture_color_channels(dst_format);
    unsigned packed_channels = 0u, packed_bytes = 0u, widths[4] = {0u};
    unsigned i, offset;
    int reverse = 0, integer = texture_color_integer_format(src_format);
    uint32_t packed;
    size_t scalar_bytes =
        src_type == GL_BYTE || src_type == GL_UNSIGNED_BYTE                                  ? 1u
        : src_type == GL_SHORT || src_type == GL_UNSIGNED_SHORT || src_type == GL_HALF_FLOAT ? 2u
                                                                                             : 4u;
    if (compressed_readback_packed_layout(src_type, &packed_channels, widths, &packed_bytes,
                                          &reverse)) {
        packed = 0u;
        memcpy(&packed, src, packed_bytes);
        offset = reverse ? 0u : packed_bytes * 8u;
        for (i = 0u; i < packed_channels; ++i) {
            uint32_t mask = (1u << widths[i]) - 1u;
            unsigned shift;
            if (reverse) {
                shift = offset;
                offset += widths[i];
            } else {
                offset -= widths[i];
                shift = offset;
            }
            rgba[i] = integer ? (double)((packed >> shift) & mask)
                              : (double)((packed >> shift) & mask) / (double)mask;
        }
    } else if (src_type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
        memcpy(&packed, src, sizeof(packed));
        rgba[0] = texture_color_load_ufloat(packed & 0x7ffu, 6u);
        rgba[1] = texture_color_load_ufloat((packed >> 11u) & 0x7ffu, 6u);
        rgba[2] = texture_color_load_ufloat((packed >> 22u) & 0x3ffu, 5u);
    } else if (src_type == GL_UNSIGNED_INT_5_9_9_9_REV) {
        memcpy(&packed, src, sizeof(packed));
        for (i = 0u; i < 3u; ++i)
            rgba[i] = ldexp((double)((packed >> (i * 9u)) & 0x1ffu), (int)(packed >> 27u) - 24);
    } else {
        for (i = 0u; i < src_channels; ++i)
            rgba[i] = texture_color_load_scalar(src + (size_t)i * scalar_bytes, src_type, integer);
    }
    if (src_format == GL_BGR || src_format == GL_BGRA || src_format == GL_BGR_INTEGER ||
        src_format == GL_BGRA_INTEGER) {
        double tmp = rgba[0];
        rgba[0] = rgba[2];
        rgba[2] = tmp;
    }
    if (src_format == GL_ALPHA) {
        rgba[3] = rgba[0];
        rgba[0] = 0.0;
    } else if (src_format == GL_INTENSITY)
        rgba[1] = rgba[2] = rgba[3] = rgba[0];
    else if (src_format == GL_LUMINANCE || src_format == GL_LUMINANCE_ALPHA) {
        if (src_format == GL_LUMINANCE_ALPHA)
            rgba[3] = rgba[1];
        rgba[1] = rgba[2] = rgba[0];
    }
    for (i = 0u; i < dst_channels; ++i) {
        if ((dst_format == GL_BGR || dst_format == GL_BGRA || dst_format == GL_BGR_INTEGER ||
             dst_format == GL_BGRA_INTEGER) &&
            (i == 0u || i == 2u))
            output[i] = rgba[2u - i];
        else if (dst_format == GL_GREEN || dst_format == GL_GREEN_INTEGER)
            output[i] = rgba[1];
        else if (dst_format == GL_BLUE || dst_format == GL_BLUE_INTEGER)
            output[i] = rgba[2];
        else if (dst_format == GL_ALPHA)
            output[i] = rgba[3];
        else if (dst_format == GL_LUMINANCE_ALPHA && i == 1u)
            output[i] = rgba[3];
        else
            output[i] = rgba[i];
    }
    if (dst_type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
        packed = compressed_readback_ufloat(output[0], 6u) |
                 compressed_readback_ufloat(output[1], 6u) << 11u |
                 compressed_readback_ufloat(output[2], 5u) << 22u;
        memcpy(dst, &packed, sizeof(packed));
    } else if (dst_type == GL_UNSIGNED_INT_5_9_9_9_REV) {
        packed = compressed_readback_rgb9e5(output);
        memcpy(dst, &packed, sizeof(packed));
    } else if (compressed_readback_packed_layout(dst_type, &packed_channels, widths, &packed_bytes,
                                                 &reverse)) {
        packed = 0u;
        offset = reverse ? 0u : packed_bytes * 8u;
        for (i = 0u; i < packed_channels; ++i) {
            uint32_t max_value = (1u << widths[i]) - 1u;
            double v = output[i];
            uint32_t component =
                (uint32_t)(integer ? fmin((double)max_value, fmax(0.0, v))
                                   : floor(fmin(1.0, fmax(0.0, v)) * max_value + 0.5));
            if (reverse) {
                packed |= component << offset;
                offset += widths[i];
            } else {
                offset -= widths[i];
                packed |= component << offset;
            }
        }
        memcpy(dst, &packed, packed_bytes);
    } else {
        size_t out_bytes =
            dst_type == GL_BYTE || dst_type == GL_UNSIGNED_BYTE ? 1u
            : dst_type == GL_SHORT || dst_type == GL_UNSIGNED_SHORT || dst_type == GL_HALF_FLOAT
                ? 2u
                : 4u;
        for (i = 0u; i < dst_channels; ++i)
            texture_color_store_scalar(dst + (size_t)i * out_bytes, dst_type, output[i], integer);
    }
}

static size_t texture_readback_swap_element_size(GLenum type) {
    switch (type) {
        case GL_UNSIGNED_SHORT:
        case GL_SHORT:
        case GL_HALF_FLOAT:
        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            return 2u;
        case GL_UNSIGNED_INT:
        case GL_INT:
        case GL_FLOAT:
        case GL_UNSIGNED_INT_24_8:
        case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
        case GL_UNSIGNED_INT_5_9_9_9_REV:
            return 4u;
        default:
            return 1u;
    }
}

static float readback_half_to_float(uint16_t h) {
    uint32_t bits;
    unsigned exponent = (h >> 10u) & 31u;
    unsigned fraction = h & 1023u;
    float value;
    if (exponent == 0u) {
        if (fraction == 0u)
            bits = 0u;
        else {
            int shift = 0;
            while (!(fraction & 1024u)) {
                fraction <<= 1u;
                ++shift;
            }
            bits = (uint32_t)(113 - shift) << 23u | (fraction & 1023u) << 13u;
        }
    } else if (exponent == 31u)
        bits = 0x7f800000u | fraction << 13u;
    else
        bits = (exponent + 112u) << 23u | fraction << 13u;
    bits |= ((uint32_t)h & 0x8000u) << 16u;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t compressed_readback_round_even(double value) {
    double integral = floor(value);
    double fraction = value - integral;
    if (fraction > 0.5 || (fraction == 0.5 && fmod(integral, 2.0) != 0.0))
        integral += 1.0;
    return (uint32_t)integral;
}

static uint32_t compressed_readback_ufloat(double value, unsigned mantissa_bits) {
    double maximum = ldexp(2.0 - ldexp(1.0, -(int)mantissa_bits), 15);
    int exponent;
    uint32_t mantissa;
    if (isnan(value))
        return (31u << mantissa_bits) | 1u;
    if (value <= 0.0)
        return 0u;
    if (isinf(value))
        return 31u << mantissa_bits;
    if (value >= maximum)
        return (30u << mantissa_bits) | ((1u << mantissa_bits) - 1u);
    if (value < ldexp(1.0, -14))
        return compressed_readback_round_even(ldexp(value, (int)mantissa_bits + 14));
    (void)frexp(value, &exponent);
    --exponent;
    mantissa = compressed_readback_round_even((ldexp(value, -exponent) - 1.0) *
                                              (double)(1u << mantissa_bits));
    if (mantissa == (1u << mantissa_bits)) {
        mantissa = 0u;
        ++exponent;
    }
    return ((uint32_t)(exponent + 15) << mantissa_bits) | mantissa;
}

static uint32_t compressed_readback_rgb9e5(const double rgba[4]) {
    double rgb[3], maximum, scale;
    int exponent, i;
    uint32_t components[3];
    for (i = 0; i < 3; ++i) {
        rgb[i] = isnan(rgba[i]) || rgba[i] <= 0.0 ? 0.0 : fmin(rgba[i], 65408.0);
    }
    maximum = fmax(rgb[0], fmax(rgb[1], rgb[2]));
    if (maximum == 0.0)
        return 0u;
    (void)frexp(maximum, &exponent);
    exponent = exponent + 15;
    if (exponent < 0)
        exponent = 0;
    if (exponent > 31)
        exponent = 31;
    scale = ldexp(1.0, exponent - 24);
    if (floor(maximum / scale + 0.5) >= 512.0 && exponent < 31) {
        ++exponent;
        scale *= 2.0;
    }
    for (i = 0; i < 3; ++i)
        components[i] = (uint32_t)floor(rgb[i] / scale + 0.5);
    return ((uint32_t)exponent << 27u) | (components[2] << 18u) | (components[1] << 9u) |
           components[0];
}

static int compressed_readback_packed_layout(GLenum type, unsigned *channels, unsigned widths[4],
                                             unsigned *bytes, int *reverse) {
    *reverse = 0;
    switch (type) {
        case GL_UNSIGNED_BYTE_3_3_2:
            *channels = 3u;
            *bytes = 1u;
            widths[0] = 3u;
            widths[1] = 3u;
            widths[2] = 2u;
            break;
        case GL_UNSIGNED_BYTE_2_3_3_REV:
            *channels = 3u;
            *bytes = 1u;
            *reverse = 1;
            widths[0] = 3u;
            widths[1] = 3u;
            widths[2] = 2u;
            break;
        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
            *channels = 3u;
            *bytes = 2u;
            *reverse = type == GL_UNSIGNED_SHORT_5_6_5_REV;
            widths[0] = 5u;
            widths[1] = 6u;
            widths[2] = 5u;
            break;
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
            *channels = 4u;
            *bytes = 2u;
            *reverse = type == GL_UNSIGNED_SHORT_4_4_4_4_REV;
            widths[0] = widths[1] = widths[2] = widths[3] = 4u;
            break;
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            *channels = 4u;
            *bytes = 2u;
            *reverse = type == GL_UNSIGNED_SHORT_1_5_5_5_REV;
            widths[0] = widths[1] = widths[2] = 5u;
            widths[3] = 1u;
            break;
        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
            *channels = 4u;
            *bytes = 4u;
            *reverse = type == GL_UNSIGNED_INT_8_8_8_8_REV;
            widths[0] = widths[1] = widths[2] = widths[3] = 8u;
            break;
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            *channels = 4u;
            *bytes = 4u;
            *reverse = type == GL_UNSIGNED_INT_2_10_10_10_REV;
            widths[0] = widths[1] = widths[2] = 10u;
            widths[3] = 2u;
            break;
        default:
            return 0;
    }
    return 1;
}

static int compressed_readback_supported(GLenum format, GLenum type) {
    unsigned channels, packed_channels, bytes, widths[4] = {0u};
    int reverse;
    if (format == GL_RED || format == GL_GREEN || format == GL_BLUE || format == GL_LUMINANCE ||
        format == GL_ALPHA)
        channels = 1u;
    else if (format == GL_RG || format == GL_LUMINANCE_ALPHA)
        channels = 2u;
    else if (format == GL_RGB || format == GL_BGR)
        channels = 3u;
    else if (format == GL_RGBA || format == GL_BGRA)
        channels = 4u;
    else
        return 0;
    if (type == GL_UNSIGNED_BYTE || type == GL_BYTE || type == GL_UNSIGNED_SHORT ||
        type == GL_SHORT || type == GL_UNSIGNED_INT || type == GL_INT || type == GL_HALF_FLOAT ||
        type == GL_FLOAT)
        return 1;
    if (format == GL_RGB &&
        (type == GL_UNSIGNED_INT_10F_11F_11F_REV || type == GL_UNSIGNED_INT_5_9_9_9_REV))
        return 1;
    return compressed_readback_packed_layout(type, &packed_channels, widths, &bytes, &reverse) &&
           channels == packed_channels && (channels != 3u || format == GL_RGB);
}

static int texture_readback_format_type(GLenum format, GLenum type, size_t *bytes) {
    unsigned channels, packed_channels, packed_bytes, widths[4] = {0u};
    int reverse;
    size_t scalar_size = 0u;
    unsigned scalar_channels = 0u;
    int integer = texture_color_integer_format(format);
    if (valid_format_type(format, type, bytes))
        return 1;
    if (format == GL_GREEN || format == GL_BLUE || format == GL_GREEN_INTEGER ||
        format == GL_BLUE_INTEGER)
        scalar_channels = 1u;
    else if (format == GL_BGR || format == GL_BGR_INTEGER)
        scalar_channels = 3u;
    else if (format == GL_BGRA || format == GL_BGRA_INTEGER)
        scalar_channels = 4u;
    if (type == GL_BYTE || type == GL_UNSIGNED_BYTE)
        scalar_size = 1u;
    else if (type == GL_SHORT || type == GL_UNSIGNED_SHORT || type == GL_HALF_FLOAT)
        scalar_size = 2u;
    else if (type == GL_INT || type == GL_UNSIGNED_INT || type == GL_FLOAT)
        scalar_size = 4u;
    if (scalar_channels != 0u && scalar_size != 0u &&
        (!integer || (type != GL_HALF_FLOAT && type != GL_FLOAT))) {
        if (bytes != NULL)
            *bytes = scalar_channels * scalar_size;
        return 1;
    }
    if (format == GL_RGB)
        channels = 3u;
    else if (format == GL_RGBA || format == GL_BGRA)
        channels = 4u;
    else
        return 0;
    if (format == GL_RGB &&
        (type == GL_UNSIGNED_INT_10F_11F_11F_REV || type == GL_UNSIGNED_INT_5_9_9_9_REV)) {
        *bytes = 4u;
        return 1;
    }
    if (!compressed_readback_packed_layout(type, &packed_channels, widths, &packed_bytes,
                                           &reverse) ||
        channels != packed_channels)
        return 0;
    *bytes = (size_t)packed_bytes;
    return 1;
}

static GLenum texture_readback_format_type_error(GLenum format, GLenum type) {
    unsigned channels, bytes, widths[4] = {0u};
    int reverse;
    switch (format) {
        case GL_RED:
        case GL_GREEN:
        case GL_BLUE:
        case GL_RG:
        case GL_RGB:
        case GL_RGBA:
        case GL_BGR:
        case GL_BGRA:
        case GL_ALPHA:
        case GL_LUMINANCE:
        case GL_LUMINANCE_ALPHA:
        case GL_RED_INTEGER:
        case GL_GREEN_INTEGER:
        case GL_BLUE_INTEGER:
        case GL_RG_INTEGER:
        case GL_RGB_INTEGER:
        case GL_BGR_INTEGER:
        case GL_RGBA_INTEGER:
        case GL_BGRA_INTEGER:
        case GL_DEPTH_COMPONENT:
        case GL_DEPTH_STENCIL:
        case GL_STENCIL_INDEX:
            break;
        default:
            return GL_INVALID_ENUM;
    }
    if (compressed_readback_packed_layout(type, &channels, widths, &bytes, &reverse) ||
        type == GL_UNSIGNED_INT_10F_11F_11F_REV || type == GL_UNSIGNED_INT_5_9_9_9_REV)
        return GL_INVALID_OPERATION;
    return GL_INVALID_ENUM;
}

static int compressed_readback_pixel(const unsigned char *src, GLenum compressed_format,
                                     GLenum format, GLenum type, unsigned char *dst) {
    double rgba[4] = {0.0, 0.0, 0.0, 1.0};
    double output[4];
    unsigned channels, i, packed_channels, packed_bytes, widths[4] = {0u};
    int reverse;
    int signed_rgtc = compressed_format_is_signed_rgtc(compressed_format);
    if (format == GL_RED || format == GL_GREEN || format == GL_BLUE || format == GL_LUMINANCE ||
        format == GL_ALPHA)
        channels = 1u;
    else if (format == GL_RG || format == GL_LUMINANCE_ALPHA)
        channels = 2u;
    else if (format == GL_RGB || format == GL_BGR)
        channels = 3u;
    else if (format == GL_RGBA || format == GL_BGRA)
        channels = 4u;
    else
        return 0;
    if (compressed_format_is_rgtc(compressed_format)) {
        unsigned count = compressed_format_components(compressed_format);
        for (i = 0u; i < count; ++i)
            rgba[i] =
                signed_rgtc ? fmax(-1.0, (double)(int8_t)src[i] / 127.0) : (double)src[i] / 255.0;
    } else if (compressed_format_is_bptc(compressed_format) &&
               (compressed_format == GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT ||
                compressed_format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT)) {
        for (i = 0u; i < 3u; ++i) {
            uint16_t half;
            memcpy(&half, src + i * sizeof(half), sizeof(half));
            rgba[i] = (double)readback_half_to_float(half);
        }
    } else {
        for (i = 0u; i < 4u; ++i)
            rgba[i] = (double)src[i] / 255.0;
    }
    for (i = 0u; i < channels; ++i) {
        if ((format == GL_BGRA || format == GL_BGR) && (i == 0u || i == 2u))
            output[i] = rgba[2u - i];
        else if (format == GL_GREEN)
            output[i] = rgba[1u];
        else if (format == GL_BLUE)
            output[i] = rgba[2u];
        else if (format == GL_ALPHA)
            output[i] = rgba[3u];
        else if (format == GL_LUMINANCE_ALPHA && i == 1u)
            output[i] = rgba[3u];
        else
            output[i] = rgba[i];
    }
    if (format == GL_RGB && type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
        uint32_t packed = compressed_readback_ufloat(output[0], 6u) |
                          (compressed_readback_ufloat(output[1], 6u) << 11u) |
                          (compressed_readback_ufloat(output[2], 5u) << 22u);
        memcpy(dst, &packed, sizeof(packed));
        return 1;
    }
    if (format == GL_RGB && type == GL_UNSIGNED_INT_5_9_9_9_REV) {
        uint32_t packed = compressed_readback_rgb9e5(output);
        memcpy(dst, &packed, sizeof(packed));
        return 1;
    }
    if (compressed_readback_packed_layout(type, &packed_channels, widths, &packed_bytes,
                                          &reverse)) {
        uint32_t packed = 0u;
        unsigned offset = reverse ? 0u : packed_bytes * 8u;
        if (packed_channels != channels)
            return 0;
        for (i = 0u; i < channels; ++i) {
            uint32_t maximum = (1u << widths[i]) - 1u;
            double value = output[i];
            uint32_t component;
            if (!(value > 0.0))
                value = 0.0;
            if (value > 1.0)
                value = 1.0;
            component = (uint32_t)floor(value * (double)maximum + 0.5);
            if (reverse) {
                packed |= component << offset;
                offset += widths[i];
            } else {
                offset -= widths[i];
                packed |= component << offset;
            }
        }
        memcpy(dst, &packed, packed_bytes);
        return 1;
    }
    for (i = 0u; i < channels; ++i) {
        double value = output[i];
        if (type == GL_FLOAT) {
            float v = (float)value;
            memcpy(dst + 4u * i, &v, sizeof(v));
        } else if (type == GL_HALF_FLOAT) {
            uint16_t v = float_to_half_bits((float)value);
            memcpy(dst + 2u * i, &v, sizeof(v));
        } else if (type == GL_UNSIGNED_BYTE)
            dst[i] = (unsigned char)(fmin(1.0, fmax(0.0, value)) * 255.0 + 0.5);
        else if (type == GL_BYTE)
            dst[i] = (unsigned char)(int8_t)lround(fmin(1.0, fmax(-1.0, value)) * 127.0);
        else if (type == GL_UNSIGNED_SHORT) {
            uint16_t v = (uint16_t)(fmin(1.0, fmax(0.0, value)) * 65535.0 + 0.5);
            memcpy(dst + 2u * i, &v, sizeof(v));
        } else if (type == GL_SHORT) {
            int16_t v = (int16_t)lround(fmin(1.0, fmax(-1.0, value)) * 32767.0);
            memcpy(dst + 2u * i, &v, sizeof(v));
        } else if (type == GL_UNSIGNED_INT) {
            uint32_t v = (uint32_t)(fmin(1.0, fmax(0.0, value)) * 4294967295.0 + 0.5);
            memcpy(dst + 4u * i, &v, sizeof(v));
        } else if (type == GL_INT) {
            int32_t v = (int32_t)llround(fmin(1.0, fmax(-1.0, value)) * 2147483647.0);
            memcpy(dst + 4u * i, &v, sizeof(v));
        } else
            return 0;
    }
    return 1;
}

static int compressed_readback_region(struct mgl_context *ctx, const struct mgl_texture_level *src,
                                      GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,
                                      GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                      size_t pixel_bytes, unsigned char *dst, size_t row_stride,
                                      size_t image_stride) {
    GLenum storage_format = src->internal_format;
    GLenum decoded_format, decoded_type;
    size_t size = 0u, decoded_bpp, decoded_expected, slice_bytes, stored_bytes;
    unsigned char *decoded;
    GLsizei z, y, x;
    if (!compressed_fallback_pixel_format(storage_format, &decoded_format, &decoded_type) ||
        !compressed_readback_supported(format, type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (width == 0 || height == 0 || depth == 0)
        return 1;
    if (!valid_format_type(decoded_format, decoded_type, &decoded_bpp) ||
        !checked_mul((size_t)src->width, (size_t)src->height, &decoded_expected) ||
        !checked_mul(decoded_expected, (size_t)depth, &decoded_expected) ||
        !checked_mul(decoded_expected, decoded_bpp, &decoded_expected) ||
        !compressed_image_size(storage_format, src->width, src->height, &slice_bytes) ||
        !checked_mul(slice_bytes, (size_t)src->depth, &stored_bytes) || src->data == NULL ||
        src->data_size < stored_bytes || zoffset < 0 || zoffset > src->depth ||
        depth > src->depth - zoffset) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    decoded = decode_compressed_volume(storage_format, src->width, src->height, depth,
                                       src->data + (size_t)zoffset * slice_bytes, &size);
    if (decoded == NULL || size != decoded_expected) {
        free(decoded);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    for (z = 0; z < depth; ++z)
        for (y = 0; y < height; ++y)
            for (x = 0; x < width; ++x) {
                const unsigned char *from =
                    decoded + (((size_t)z * (size_t)src->height + (size_t)(y + yoffset)) *
                                   (size_t)src->width +
                               (size_t)(x + xoffset)) *
                                  decoded_bpp;
                unsigned char *to = dst + (size_t)z * image_stride + (size_t)y * row_stride +
                                    (size_t)x * pixel_bytes;
                if (!compressed_readback_pixel(from, storage_format, format, type, to)) {
                    free(decoded);
                    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                    return 0;
                }
                if (ctx->pack_swap_bytes) {
                    size_t element = type == GL_UNSIGNED_SHORT || type == GL_SHORT ||
                                             type == GL_HALF_FLOAT ||
                                             type == GL_UNSIGNED_SHORT_5_6_5 ||
                                             type == GL_UNSIGNED_SHORT_5_6_5_REV ||
                                             type == GL_UNSIGNED_SHORT_4_4_4_4 ||
                                             type == GL_UNSIGNED_SHORT_4_4_4_4_REV ||
                                             type == GL_UNSIGNED_SHORT_5_5_5_1 ||
                                             type == GL_UNSIGNED_SHORT_1_5_5_5_REV
                                         ? 2u
                                     : type == GL_UNSIGNED_INT || type == GL_INT ||
                                             type == GL_FLOAT || type == GL_UNSIGNED_INT_8_8_8_8 ||
                                             type == GL_UNSIGNED_INT_8_8_8_8_REV ||
                                             type == GL_UNSIGNED_INT_10_10_10_2 ||
                                             type == GL_UNSIGNED_INT_2_10_10_10_REV ||
                                             type == GL_UNSIGNED_INT_10F_11F_11F_REV ||
                                             type == GL_UNSIGNED_INT_5_9_9_9_REV
                                         ? 4u
                                         : 1u;
                    if (element > 1u)
                        swap_elements(to, pixel_bytes, element);
                }
            }
    free(decoded);
    return 1;
}

static int native_color_readback(struct mgl_context *ctx, GLsizei width, GLsizei height,
                                 size_t count, unsigned char *rgba) {
    GLint native_format = 0, native_type = 0;
    size_t components, element_size, sample_bytes, i, j;
    unsigned char *samples;
    GLenum err;
    if (ctx->native_dispatch->glesGetIntegerv == NULL)
        return 0;
    ctx->native_dispatch->glesGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &native_format);
    ctx->native_dispatch->glesGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &native_type);
    if (ctx->native_dispatch->glesGetError() != GL_NO_ERROR)
        return 0;
    switch ((GLenum)native_format) {
        case GL_RGBA:
        case GL_BGRA:
            components = 4u;
            break;
        case GL_RGB:
            components = 3u;
            break;
        case GL_RG:
            components = 2u;
            break;
        case GL_RED:
            components = 1u;
            break;
        default:
            return 0;
    }
    switch ((GLenum)native_type) {
        case GL_UNSIGNED_BYTE:
            element_size = 1u;
            break;
        case GL_HALF_FLOAT:
            element_size = 2u;
            break;
        case GL_FLOAT:
            element_size = 4u;
            break;
        default:
            return 0;
    }
    if (count > SIZE_MAX / components / element_size)
        return 0;
    sample_bytes = count * components * element_size;
    samples = malloc(sample_bytes ? sample_bytes : 1u);
    if (samples == NULL)
        return 0;
    if (count != 0u)
        ctx->native_dispatch->glesReadPixels(0, 0, width, height, (GLenum)native_format,
                                             (GLenum)native_type, samples);
    err = ctx->native_dispatch->glesGetError();
    if (err == GL_NO_ERROR) {
        for (i = 0; i < count; ++i) {
            for (j = 0; j < 4u; ++j) {
                size_t component = (native_format == GL_BGRA && (j == 0u || j == 2u)) ? 2u - j : j;
                float v;
                if (component >= components)
                    v = j == 3u ? 1.0f : 0.0f;
                else if (native_type == GL_UNSIGNED_BYTE)
                    v = samples[i * components + component] / 255.0f;
                else if (native_type == GL_HALF_FLOAT) {
                    uint16_t h;
                    memcpy(&h, samples + (i * components + component) * 2u, 2u);
                    v = readback_half_to_float(h);
                } else
                    memcpy(&v, samples + (i * components + component) * 4u, 4u);
                if (!(v >= 0.0f))
                    v = 0.0f;
                if (v > 1.0f)
                    v = 1.0f;
                rgba[i * 4u + j] = (unsigned char)(v * 255.0f + 0.5f);
            }
        }
    }
    free(samples);
    return err == GL_NO_ERROR;
}

static int read_rendered_color_texture(struct mgl_context *ctx, const struct mgl_texture *obj,
                                       GLint level, GLsizei width, GLsizei height,
                                       unsigned char **out_pixels) {
    GLuint temp_fbo = 0u;
    struct mgl_framebuffer *read_fb;
    GLuint saved_read = 0u;
    int use_bound_read_fbo = 0;
    unsigned char *rgba;
    size_t count, bytes;
    GLenum err = GL_NO_ERROR;
    int ok = 0;
    if (ctx == NULL || obj == NULL || out_pixels == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesGenFramebuffers == NULL ||
        ctx->native_dispatch->glesDeleteFramebuffers == NULL ||
        ctx->native_dispatch->glesBindFramebuffer == NULL ||
        ctx->native_dispatch->glesFramebufferTexture2D == NULL ||
        ctx->native_dispatch->glesCheckFramebufferStatus == NULL ||
        ctx->native_dispatch->glesReadPixels == NULL ||
        ctx->native_dispatch->glesBindBuffer == NULL ||
        ctx->native_dispatch->glesPixelStorei == NULL ||
        ctx->native_dispatch->glesGetError == NULL ||
        !checked_mul((size_t)width, (size_t)height, &count) || !checked_mul(count, 4u, &bytes)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    rgba = malloc(bytes != 0u ? bytes : 1u);
    if (rgba == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    read_fb = mgl_framebuffer_find(ctx, ctx->read_framebuffer);
    if (read_fb != NULL) {
        saved_read = read_fb->native_name;
        /* When the texture is already attached to the READ framebuffer, reuse that attachment and
         * its GL_READ_BUFFER selection. */
        use_bound_read_fbo = read_fb->color_texture == obj->name &&
                             read_fb->color_texture_level == level &&
                             read_fb->color_texture_target == GL_TEXTURE_2D &&
                             read_fb->read_buffer == GL_COLOR_ATTACHMENT0;
    }
    if (use_bound_read_fbo) {

        (void)mgl_texture_refresh_framebuffer_attachments(ctx, read_fb);
        ctx->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER, saved_read);
    } else {
        ctx->native_dispatch->glesGenFramebuffers(1, &temp_fbo);
        if (temp_fbo != 0u) {
            ctx->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER, temp_fbo);
            ctx->native_dispatch->glesFramebufferTexture2D(
                GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, obj->native_name, level);
            if (ctx->native_dispatch->glesReadBuffer != NULL)
                ctx->native_dispatch->glesReadBuffer(GL_COLOR_ATTACHMENT0);
        }
    }
    if (use_bound_read_fbo || temp_fbo != 0u) {
        if (ctx->native_dispatch->glesCheckFramebufferStatus(GL_READ_FRAMEBUFFER) ==
            GL_FRAMEBUFFER_COMPLETE) {
            mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_PACK_BUFFER, 0u);
            ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, 1);
            if (bytes != 0u)
                ctx->native_dispatch->glesReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                                                     rgba);
            err = ctx->native_dispatch->glesGetError();
            ok = err == GL_NO_ERROR;
            if (!ok && err == GL_INVALID_OPERATION)
                ok = native_color_readback(ctx, width, height, count, rgba);
        }
    }
    /* Restore the read framebuffer and pack state on every exit; GL_READ_BUFFER belongs to its
     * framebuffer. */
    ctx->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER, saved_read);
    mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_PACK_BUFFER, native_pack_buffer(ctx));
    ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, ctx->pack_alignment);
    if (temp_fbo != 0u)
        ctx->native_dispatch->glesDeleteFramebuffers(1, &temp_fbo);
    if (!ok) {
        free(rgba);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    *out_pixels = rgba;
    return 1;
}

void glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTexImage",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"format", (uint64_t)(uintptr_t)format},
                                                       {"type", (uint64_t)(uintptr_t)type},
                                                       {"pixels", (uint64_t)(uintptr_t)pixels}},
                        5u);
    struct mgl_texture *obj;
    struct mgl_texture_level *src_level;
    struct mgl_buffer *pack_buffer = NULL;
    size_t src_components = 0u, dst_components = 0u;
    struct mgl_pixel_layout pack_layout;
    size_t row_stride, slice_stride, destination_size;
    size_t pack_offset = 0u;
    unsigned char *dst;
    GLsizei depth, z, y;
    if (ctx == NULL)
        return;
    /* Begin/End legality precedes validation and
     * must not read pixels or touch a pack-buffer destination. */
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_1D && target != GL_TEXTURE_2D && target != GL_TEXTURE_RECTANGLE &&
        target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY &&
        target != GL_TEXTURE_CUBE_MAP_ARRAY && cube_face_index(target) < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!texture_readback_format_type(format, type, &dst_components)) {
        mgl_error_set(&ctx->errors, texture_readback_format_type_error(format, type));
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    obj = mgl_bound_texture(ctx, cube_face_index(target) >= 0 ? GL_TEXTURE_CUBE_MAP : target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    src_level = texture_level_for_target(obj, target, level);
    if (src_level == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!src_level->defined) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!src_level->compressed && !texture_readback_format_type(format, type, NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!src_level->compressed &&
        (!texture_readback_format_compatible(src_level->format, format) ||
         (src_level->format != GL_DEPTH_COMPONENT && src_level->format != GL_DEPTH_STENCIL &&
          src_level->format != GL_STENCIL_INDEX &&
          texture_color_integer_format(src_level->format) !=
              texture_color_integer_format(format)))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    depth = src_level->depth > 0 ? src_level->depth : 1;
    if ((target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY ||
         target == GL_TEXTURE_CUBE_MAP_ARRAY)
            ? !mgl_pixel_layout_3d(src_level->width, src_level->height, depth, dst_components,
                                   ctx->pack_alignment, ctx->pack_row_length,
                                   ctx->pack_image_height, ctx->pack_skip_pixels,
                                   ctx->pack_skip_rows, ctx->pack_skip_images, &pack_layout)
            : !mgl_pixel_layout_2d(src_level->width, src_level->height, dst_components,
                                   ctx->pack_alignment, ctx->pack_row_length, ctx->pack_skip_pixels,
                                   ctx->pack_skip_rows, &pack_layout)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    row_stride = pack_layout.row_stride;
    slice_stride = pack_layout.image_stride;
    destination_size = pack_layout.span_bytes;
    if (destination_size != 0u && pixels == NULL && ctx->pixel_pack_buffer == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    dst = pixel_destination(ctx, pixels, destination_size, &pack_buffer, &pack_offset);
    if (destination_size != 0u && dst == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (destination_size != 0u)
        dst += pack_layout.start_offset;
    uint64_t perf_started = mgl_perf_diag_now();

    if (src_level->compressed) {
        if (!compressed_readback_region(ctx, src_level, 0, 0, 0, src_level->width,
                                        src_level->height, depth, format, type, dst_components, dst,
                                        row_stride, slice_stride))
            return;
    } else if (src_level->format == GL_DEPTH_STENCIL || src_level->format == GL_STENCIL_INDEX) {
        size_t src_bpp = 0u;
        if (!valid_format_type(src_level->format, src_level->type, &src_bpp)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        for (z = 0; z < depth; ++z) {
            for (y = 0; y < src_level->height; ++y) {
                size_t x;
                const unsigned char *src_row =
                    src_level->data + ((size_t)z * (size_t)src_level->height + (size_t)y) *
                                          (size_t)src_level->width * src_bpp;
                unsigned char *dst_row = dst + (size_t)z * slice_stride + (size_t)y * row_stride;
                for (x = 0u; x < (size_t)src_level->width; ++x) {
                    unsigned char *pixel = dst_row + x * dst_components;
                    texture_readback_noncolor_pixel(src_row + x * src_bpp, src_level->format,
                                                    src_level->type, pixel, format, type);
                    if (ctx->pack_swap_bytes)
                        swap_elements(pixel, dst_components,
                                      texture_readback_swap_element_size(type));
                }
            }
        }
    } else if (src_level->format == GL_DEPTH_COMPONENT) {
        size_t src_bpp = 0u, dst_bpp = 0u;
        GLfloat *native_depth = NULL;
        int native_ok = 0;
        if (format != GL_DEPTH_COMPONENT ||
            !valid_format_type(src_level->format, src_level->type, &src_bpp) ||
            !valid_format_type(format, type, &dst_bpp)) {
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
        }
        if (target == GL_TEXTURE_2D && depth == 1 && src_level->width > 0 &&
            src_level->height > 0) {
            size_t pixel_count;
            if (checked_mul((size_t)src_level->width, (size_t)src_level->height, &pixel_count)) {
                native_depth = malloc(pixel_count * sizeof(*native_depth));
                native_ok = native_depth != NULL &&
                            mgl_depth_texture_readback(ctx, obj, level, 0, 0, src_level->width,
                                                       src_level->height, native_depth);
            }
        }
        for (z = 0; z < depth; ++z) {
            for (y = 0; y < src_level->height; ++y) {
                size_t x;
                size_t src_row_base = (((size_t)z * (size_t)src_level->height + (size_t)y) *
                                       (size_t)src_level->width) *
                                      src_bpp;
                unsigned char *dst_row = dst + (size_t)z * slice_stride + (size_t)y * row_stride;
                for (x = 0u; x < (size_t)src_level->width; ++x) {
                    double value;
                    if (native_ok)
                        value = (double)native_depth[(size_t)y * (size_t)src_level->width + x];
                    else
                        value = depth_load(src_level->data + src_row_base + x * src_bpp,
                                           src_level->type);
                    depth_store(dst_row + x * dst_bpp, type, value);
                    if (ctx->pack_swap_bytes && dst_bpp > 1u)
                        swap_elements(dst_row + x * dst_bpp, dst_bpp, dst_bpp);
                }
            }
        }
        free(native_depth);
    } else {
        unsigned char *gpu_rgba = NULL;
        const unsigned char *source_data = src_level->data;
        GLenum source_format = src_level->format, source_type = src_level->type;
        if (format == GL_DEPTH_COMPONENT ||
            !valid_format_type(src_level->format, src_level->type, &src_components)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (target == GL_TEXTURE_2D && obj->used_as_color_attachment &&
            !(obj->color_attachment_incomplete && !obj->color_attachment_complete) &&
            ctx->native_dispatch != NULL && ctx->native_dispatch->glesReadPixels != NULL &&
            src_level->width > 0 && src_level->height > 0) {
            if (!read_rendered_color_texture(ctx, obj, level, src_level->width, src_level->height,
                                             &gpu_rgba))
                return;
            source_data = gpu_rgba;
            source_format = GL_RGBA;
            source_type = GL_UNSIGNED_BYTE;
            src_components = 4u;
        }
        if (src_level->internal_format == GL_INTENSITY8 && source_format == GL_RED)
            source_format = GL_INTENSITY;
        for (z = 0; z < depth; ++z) {
            for (y = 0; y < src_level->height; ++y) {
                size_t x;
                size_t src_row_base = (((size_t)z * (size_t)src_level->height + (size_t)y) *
                                       (size_t)src_level->width) *
                                      src_components;
                unsigned char *dst_row = dst + (size_t)z * slice_stride + (size_t)y * row_stride;
                for (x = 0u; x < (size_t)src_level->width; ++x) {
                    convert_texture_color_texel(source_data + src_row_base + x * src_components,
                                                source_format, source_type,
                                                dst_row + x * dst_components, format, type);
                    if (ctx->pack_swap_bytes)
                        swap_elements(dst_row + x * dst_components, dst_components,
                                      texture_readback_swap_element_size(type));
                }
            }
        }
        free(gpu_rgba);
    }
    if (pack_buffer != NULL && destination_size != 0u)
        (void)mgl_buffer_write_bytes(ctx, pack_buffer, pack_offset, destination_size,
                                     dst - pack_layout.start_offset, 0);
    mgl_perf_diag_extra(ctx, MGL_PERF_GET_TEX_IMAGE, perf_started);
}

static int generate_mip_level_images(struct mgl_context *ctx, const struct mgl_texture *obj,
                                     GLenum upload_target, const struct mgl_texture_level *src,
                                     struct mgl_texture_level *dst, GLint dst_index) {
    GLsizei dw = src->width > 1 ? src->width / 2 : src->width;
    GLsizei dh = src->height > 1 ? src->height / 2 : src->height;
    GLsizei dd = upload_target == GL_TEXTURE_3D && src->depth > 1 ? src->depth / 2
                 : (upload_target == GL_TEXTURE_2D_ARRAY ||
                  upload_target == GL_TEXTURE_CUBE_MAP_ARRAY) ? src->depth
                                                                  : 1;
    size_t bpp = 0u, texels = 0u, bytes = 0u;
    unsigned char *data;
    GLsizei z, y, x;
    if (!valid_format_type(src->format, src->type, &bpp) || src->compressed)
        return 0;
    if (!checked_mul((size_t)dw, (size_t)dh, &texels) ||
        !checked_mul(texels, (size_t)dd, &texels) || !checked_mul(texels, bpp, &bytes))
        return 0;
    data = bytes != 0u ? malloc(bytes) : calloc(1u, 1u);
    if (data == NULL)
        return 0;
    for (z = 0; z < dd; ++z) {
        for (y = 0; y < dh; ++y) {
            for (x = 0; x < dw; ++x) {
                GLsizei sx0 = x * 2, sy0 = y * 2;
                GLsizei sz0 = upload_target == GL_TEXTURE_3D ? z * 2 : z;
                GLsizei sx_count = src->width > 1 ? 2 : 1;
                GLsizei sy_count = src->height > 1 ? 2 : 1;
                GLsizei sz_count = (upload_target == GL_TEXTURE_3D && src->depth > 1) ? 2 : 1;
                size_t dst_off =
                    (((size_t)z * (size_t)dh + (size_t)y) * (size_t)dw + (size_t)x) * bpp;
                if (src->format == GL_DEPTH_COMPONENT) {
                    double sum = 0.0;
                    unsigned count = 0u;
                    GLsizei oz, oy, ox;
                    for (oz = 0; oz < sz_count; ++oz) {
                        for (oy = 0; oy < sy_count; ++oy) {
                            for (ox = 0; ox < sx_count; ++ox) {
                                GLsizei sx = sx0 + ox, sy = sy0 + oy, sz = sz0 + oz;
                                size_t src_off;
                                if (sx >= src->width || sy >= src->height || sz >= src->depth)
                                    continue;
                                src_off = (((size_t)sz * (size_t)src->height + (size_t)sy) *
                                               (size_t)src->width +
                                           (size_t)sx) *
                                          bpp;
                                sum += depth_load(src->data + src_off, src->type);
                                ++count;
                            }
                        }
                    }
                    depth_store(data + dst_off, src->type, count != 0u ? sum / (double)count : 0.0);
                } else {
                    size_t c;
                    for (c = 0u; c < bpp; ++c) {
                        unsigned sum = 0u, count = 0u;
                        GLsizei oz, oy, ox;
                        for (oz = 0; oz < sz_count; ++oz) {
                            for (oy = 0; oy < sy_count; ++oy) {
                                for (ox = 0; ox < sx_count; ++ox) {
                                    GLsizei sx = sx0 + ox, sy = sy0 + oy, sz = sz0 + oz;
                                    size_t src_off;
                                    if (sx >= src->width || sy >= src->height || sz >= src->depth)
                                        continue;
                                    src_off = (((size_t)sz * (size_t)src->height + (size_t)sy) *
                                                   (size_t)src->width +
                                               (size_t)sx) *
                                              bpp;
                                    sum += src->data[src_off + c];
                                    ++count;
                                }
                            }
                        }
                        data[dst_off + c] =
                            (unsigned char)(count != 0u ? (sum + count / 2u) / count : 0u);
                    }
                }
            }
        }
    }
    free(dst->data);
    memset(dst, 0, sizeof(*dst));
    dst->width = dw;
    dst->height = dh;
    dst->depth = dd;
    dst->internal_format = src->internal_format;
    dst->format = src->format;
    dst->type = src->type;
    dst->data_size = bytes;
    dst->data = data;
    dst->defined = GL_TRUE;
    dst->compressed = GL_FALSE;
    if (ctx->native_dispatch != NULL &&
        (ctx->native_dispatch->glesGenerateMipmap == NULL ||
         (obj != NULL && obj->immutable_format && !obj->native_immutable_storage))) {
        native_upload_begin(ctx);
        if ((upload_target == GL_TEXTURE_3D || upload_target == GL_TEXTURE_2D_ARRAY ||
             upload_target == GL_TEXTURE_CUBE_MAP_ARRAY) &&
            obj != NULL && obj->immutable_format && obj->native_immutable_storage &&
            ctx->native_dispatch->glesTexSubImage3D != NULL) {
            ctx->native_dispatch->glesTexSubImage3D(upload_target, dst_index, 0, 0, 0, dw, dh, dd,
                                                    native_storage_external_format(dst->format),
                                                    dst->type, bytes != 0u ? data : NULL);
        } else if ((upload_target == GL_TEXTURE_3D || upload_target == GL_TEXTURE_2D_ARRAY ||
             upload_target == GL_TEXTURE_CUBE_MAP_ARRAY) &&
                   ctx->native_dispatch->glesTexImage3D != NULL) {
            ctx->native_dispatch->glesTexImage3D(
                upload_target, dst_index,
                (GLint)native_storage_internalformat(ctx, dst->internal_format), dw, dh, dd, 0,
                native_storage_external_format(dst->format), dst->type, bytes != 0u ? data : NULL);
        } else if (obj != NULL && obj->immutable_format && obj->native_immutable_storage &&
                   ctx->native_dispatch->glesTexSubImage2D != NULL) {
            ctx->native_dispatch->glesTexSubImage2D(
                upload_target == GL_TEXTURE_1D ? GL_TEXTURE_2D : upload_target, dst_index, 0, 0, dw,
                dh, native_storage_external_format(dst->format), dst->type,
                bytes != 0u ? data : NULL);
        } else if (ctx->native_dispatch->glesTexImage2D != NULL) {
            ctx->native_dispatch->glesTexImage2D(
                upload_target == GL_TEXTURE_1D ? GL_TEXTURE_2D : upload_target, dst_index,
                (GLint)native_storage_internalformat(ctx, dst->internal_format), dw, dh, 0,
                native_storage_external_format(dst->format), dst->type, bytes != 0u ? data : NULL);
        }
        native_upload_end(ctx);
    }
    return 1;
}

static int cube_base_complete(const struct mgl_texture *obj, GLint level) {
    const struct mgl_texture_level *base;
    unsigned face;
    if (obj == NULL || level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS)
        return 0;
    base = &obj->cube_levels[0][level];
    if (!base->defined || base->compressed || base->width != base->height)
        return 0;
    for (face = 1u; face < 6u; ++face) {
        const struct mgl_texture_level *lv = &obj->cube_levels[face][level];
        if (!lv->defined || lv->compressed || lv->width != base->width ||
            lv->height != base->height || lv->internal_format != base->internal_format ||
            lv->format != base->format || lv->type != base->type)
            return 0;
    }
    return 1;
}

static int copy_image_complete(const struct mgl_texture *obj) {
    const struct mgl_texture_level *base;
    GLint last;
    GLsizei w, h, d;
    if (obj->immutable_format)
        return 1;
    if (obj->base_level < 0 || obj->base_level >= (GLint)MGL_MAX_TEXTURE_LEVELS)
        return 0;
    base = &obj->levels[obj->base_level];
    if (!base->defined)
        return 0;
    if (obj->min_filter == GL_NEAREST || obj->min_filter == GL_LINEAR)
        return 1;
    w = base->width;
    h = base->height;
    d = base->depth;
    last = obj->base_level;
    while ((w > 1 || h > 1 || (obj->target == GL_TEXTURE_3D && d > 1)) && last < obj->max_level) {
        const struct mgl_texture_level *next;
        if (++last >= (GLint)MGL_MAX_TEXTURE_LEVELS)
            return 0;
        if (w > 1)
            w /= 2;
        if (h > 1)
            h /= 2;
        if (obj->target == GL_TEXTURE_3D && d > 1)
            d /= 2;
        next = &obj->levels[last];
        if (!next->defined || next->width != w || next->height != h || next->depth != d ||
            next->internal_format != base->internal_format)
            return 0;
    }
    return 1;
}

static int copy_image_native_supported(const struct mgl_native_dispatch *native) {
    const char *version;
    if (native == NULL || native->glesCopyImageSubData == NULL)
        return 0;
    if (mgl_native_dispatch_has_gles_extension(native, "GL_EXT_copy_image"))
        return 1;
    if (native->glesGetString == NULL)
        return 0;
    version = (const char *)native->glesGetString(GL_VERSION);
    return version != NULL && strncmp(version, "OpenGL ES ", 10) == 0 &&
           (version[10] > '3' || (version[10] == '3' && version[11] == '.' && version[12] >= '2'));
}

static int copy_image_target(GLenum target) {
    return target == GL_TEXTURE_2D || target == GL_TEXTURE_3D ||
           target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP_ARRAY;
}

static int copy_image_range(GLint x, GLsizei extent, GLsizei size) {
    return x >= 0 && extent >= 0 && x <= size && extent <= size - x;
}

static GLuint *active_texture_binding_slot(struct mgl_context *ctx, GLenum target);

static int copy_compressed_image_shadow(struct mgl_context *ctx, const struct mgl_texture_level *sl,
                                        struct mgl_texture *dst, const struct mgl_texture_level *dl,
                                        GLint src_x, GLint src_y, GLint src_z, GLint dst_level,
                                        GLint dst_x, GLint dst_y, GLint dst_z, GLsizei width,
                                        GLsizei height, GLsizei depth) {
    size_t block_bytes = compressed_block_bytes(sl->internal_format);
    size_t src_blocks_x = ((size_t)sl->width + 3u) / 4u;
    size_t src_blocks_y = ((size_t)sl->height + 3u) / 4u;
    size_t region_blocks_x = ((size_t)width + 3u) / 4u;
    size_t region_blocks_y = ((size_t)height + 3u) / 4u;
    size_t src_slice, region_row, region_slice, bytes, z, y;
    unsigned char *copy;
    GLuint *binding, saved;
    int ok;
    if (sl->data == NULL || block_bytes == 0u || (src_x & 3) || (src_y & 3) || (dst_x & 3) ||
        (dst_y & 3) ||
        ((width & 3) && (src_x + width != sl->width || dst_x + width != dl->width)) ||
        ((height & 3) && (src_y + height != sl->height || dst_y + height != dl->height))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (!checked_mul(src_blocks_x, src_blocks_y, &src_slice) ||
        !checked_mul(src_slice, block_bytes, &src_slice) ||
        !checked_mul(region_blocks_x, block_bytes, &region_row) ||
        !checked_mul(region_row, region_blocks_y, &region_slice) ||
        !checked_mul(region_slice, (size_t)depth, &bytes)) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    if (bytes == 0u)
        return 1;
    copy = malloc(bytes);
    if (copy == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    for (z = 0u; z < (size_t)depth; ++z) {
        for (y = 0u; y < region_blocks_y; ++y) {
            size_t src_offset =
                ((z + (size_t)src_z) * src_slice) +
                ((y + (size_t)src_y / 4u) * src_blocks_x + (size_t)src_x / 4u) * block_bytes;
            memcpy(copy + z * region_slice + y * region_row, sl->data + src_offset, region_row);
        }
    }
    binding = active_texture_binding_slot(ctx, dst->target);
    if (binding == NULL) {
        free(copy);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    saved = *binding;
    *binding = dst->name;
    dsa_native_bind_texture(ctx, dst);
    if (dst->target == GL_TEXTURE_2D) {
        ok = mgl_texture_apply_compressed_sub_image_2d(ctx, GL_TEXTURE_2D, dst_level, dst_x, dst_y,
                                                       width, height, sl->internal_format, copy,
                                                       region_slice);
    } else {
        ok = mgl_texture_apply_compressed_sub_image_3d(ctx, dst->target, dst_level, dst_x, dst_y,
                                                       dst_z, width, height, depth,
                                                       sl->internal_format, copy, bytes);
    }
    *binding = saved;
    dsa_native_restore_texture(ctx, dst->target);
    free(copy);
    return ok;
}

void glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY,
                        GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX,
                        GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight,
                        GLsizei srcDepth) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glCopyImageSubData",
        (const struct mgl_error_arg[]){{"srcName", (uint64_t)(uintptr_t)srcName},
                                       {"srcTarget", (uint64_t)(uintptr_t)srcTarget},
                                       {"srcLevel", (uint64_t)(uintptr_t)srcLevel},
                                       {"srcX", (uint64_t)(uintptr_t)srcX},
                                       {"srcY", (uint64_t)(uintptr_t)srcY},
                                       {"srcZ", (uint64_t)(uintptr_t)srcZ},
                                       {"dstName", (uint64_t)(uintptr_t)dstName},
                                       {"dstTarget", (uint64_t)(uintptr_t)dstTarget}},
        8u);
    struct mgl_texture *src, *dst;
    struct mgl_texture_level *sl, *dl;
    unsigned char *snapshot = NULL;
    size_t pixel_bytes, slice_bytes, row_bytes, row, layer, offset;
    int can_copy_shadow;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!copy_image_target(srcTarget) || !copy_image_target(dstTarget)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (srcLevel < 0 || srcLevel >= (GLint)MGL_MAX_TEXTURE_LEVELS || dstLevel < 0 ||
        dstLevel >= (GLint)MGL_MAX_TEXTURE_LEVELS || srcWidth < 0 || srcHeight < 0 ||
        srcDepth < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    src = mgl_texture_find(ctx, srcName);
    dst = mgl_texture_find(ctx, dstName);
    if (src == NULL || dst == NULL || !src->realized || !dst->realized ||
        src->target != srcTarget || dst->target != dstTarget) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    sl = &src->levels[srcLevel];
    dl = &dst->levels[dstLevel];
    if (!sl->defined || !dl->defined || !copy_image_complete(src) || !copy_image_complete(dst) ||
        sl->internal_format != dl->internal_format || sl->compressed != dl->compressed ||
        sl->samples != 0 || dl->samples != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!copy_image_range(srcX, srcWidth, sl->width) ||
        !copy_image_range(srcY, srcHeight, sl->height) ||
        !copy_image_range(srcZ, srcDepth, sl->depth) ||
        !copy_image_range(dstX, srcWidth, dl->width) ||
        !copy_image_range(dstY, srcHeight, dl->height) ||
        !copy_image_range(dstZ, srcDepth, dl->depth) ||
        (srcTarget == GL_TEXTURE_2D && (srcZ != 0 || srcDepth != 1)) ||
        (dstTarget == GL_TEXTURE_2D && (dstZ != 0 || srcDepth != 1))) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (sl->compressed) {
        (void)copy_compressed_image_shadow(ctx, sl, dst, dl, srcX, srcY, srcZ, dstLevel, dstX, dstY,
                                           dstZ, srcWidth, srcHeight, srcDepth);
        return;
    }
    if (!copy_image_native_supported(ctx->native_dispatch)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->angle_direct_texture_passthrough && srcTarget == GL_TEXTURE_2D &&
        dstTarget == GL_TEXTURE_2D && angle_native_texture_shadow_readable(dl) &&
        ctx->native_dispatch->glesReadPixels != NULL &&
        ctx->native_dispatch->glesGenFramebuffers != NULL &&
        ctx->native_dispatch->glesDeleteFramebuffers != NULL &&
        ctx->native_dispatch->glesBindFramebuffer != NULL &&
        ctx->native_dispatch->glesFramebufferTexture2D != NULL &&
        ctx->native_dispatch->glesCheckFramebufferStatus != NULL &&
        ctx->native_dispatch->glesBindBuffer != NULL &&
        ctx->native_dispatch->glesPixelStorei != NULL &&
        ctx->native_dispatch->glesGetError != NULL) {
        ctx->native_dispatch->glesCopyImageSubData(
            src->native_name, srcTarget, srcLevel, srcX, srcY, srcZ, dst->native_name, dstTarget,
            dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
        mgl_perf_diag_angle_texture(ctx, 0, 1);
        if (srcWidth > 0 && srcHeight > 0 && srcDepth > 0) {
            dl->shadow_stale = GL_TRUE;
            dst->used_as_color_attachment = GL_TRUE;
        }
        return;
    }
    can_copy_shadow =
        !src->used_as_color_attachment && sl->data != NULL && dl->data != NULL &&
        sl->format == dl->format && sl->type == dl->type && sl->width > 0 && sl->height > 0 &&
        sl->depth > 0 && dl->width > 0 && dl->height > 0 && dl->depth > 0 &&
        sl->data_size % ((size_t)sl->width * (size_t)sl->height * (size_t)sl->depth) == 0u;
    if (can_copy_shadow) {
        pixel_bytes = sl->data_size / ((size_t)sl->width * (size_t)sl->height * (size_t)sl->depth);
        can_copy_shadow =
            pixel_bytes > 0 && pixel_bytes <= 16u &&
            dl->data_size % ((size_t)dl->width * (size_t)dl->height * (size_t)dl->depth) == 0u &&
            dl->data_size / ((size_t)dl->width * (size_t)dl->height * (size_t)dl->depth) ==
                pixel_bytes;
        if (can_copy_shadow && sl->data == dl->data && srcWidth > 0 && srcHeight > 0 &&
            srcDepth > 0) {
            row_bytes = (size_t)srcWidth * pixel_bytes;
            slice_bytes = row_bytes * (size_t)srcHeight;
            if (srcHeight != 0 && slice_bytes / (size_t)srcHeight != row_bytes) {
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return;
            }
            if ((size_t)srcDepth > SIZE_MAX / slice_bytes) {
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return;
            }
            snapshot = malloc(slice_bytes * (size_t)srcDepth);
            if (snapshot == NULL) {
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return;
            }
            for (layer = 0; layer < (size_t)srcDepth; ++layer) {
                for (row = 0; row < (size_t)srcHeight; ++row) {
                    offset = (((layer + (size_t)srcZ) * (size_t)sl->height + row + (size_t)srcY) *
                                  (size_t)sl->width +
                              (size_t)srcX) *
                             pixel_bytes;
                    memcpy(snapshot + layer * slice_bytes + row * row_bytes, sl->data + offset,
                           row_bytes);
                }
            }
        }
    }
    ctx->native_dispatch->glesCopyImageSubData(src->native_name, srcTarget, srcLevel, srcX, srcY,
                                               srcZ, dst->native_name, dstTarget, dstLevel, dstX,
                                               dstY, dstZ, srcWidth, srcHeight, srcDepth);
    if (can_copy_shadow && snapshot != NULL) {
        for (layer = 0; layer < (size_t)srcDepth; ++layer) {
            for (row = 0; row < (size_t)srcHeight; ++row) {
                offset = (((layer + (size_t)dstZ) * (size_t)dl->height + row + (size_t)dstY) *
                              (size_t)dl->width +
                          (size_t)dstX) *
                         pixel_bytes;
                memcpy(dl->data + offset, snapshot + layer * slice_bytes + row * row_bytes,
                       row_bytes);
            }
        }
    } else if (can_copy_shadow && sl->data != dl->data && srcWidth > 0 && srcHeight > 0 &&
               srcDepth > 0) {
        row_bytes = (size_t)srcWidth * pixel_bytes;
        for (layer = 0; layer < (size_t)srcDepth; ++layer) {
            for (row = 0; row < (size_t)srcHeight; ++row) {
                size_t src_offset =
                    (((layer + (size_t)srcZ) * (size_t)sl->height + row + (size_t)srcY) *
                         (size_t)sl->width +
                     (size_t)srcX) *
                    pixel_bytes;
                offset = (((layer + (size_t)dstZ) * (size_t)dl->height + row + (size_t)dstY) *
                              (size_t)dl->width +
                          (size_t)dstX) *
                         pixel_bytes;
                memcpy(dl->data + offset, sl->data + src_offset, row_bytes);
            }
        }
    } else if (srcWidth != 0 && srcHeight != 0 && srcDepth != 0) {
        dst->used_as_color_attachment = GL_TRUE;
    }
    free(snapshot);
}

void glGenerateMipmap(GLenum target) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_GENERATE_MIPMAP);
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGenerateMipmap",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target}},
                        1u);
    struct mgl_texture *obj;
    GLint level, max_level;
    if (ctx == NULL)
        return;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_1D && target != GL_TEXTURE_2D && target != GL_TEXTURE_3D &&
        target != GL_TEXTURE_2D_ARRAY && target != GL_TEXTURE_CUBE_MAP &&
        target != GL_TEXTURE_CUBE_MAP_ARRAY) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL || !obj->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    level = obj->immutable_format ? immutable_effective_base_level(obj) : obj->base_level;
    if (target == GL_TEXTURE_2D && level >= 0 && level < (GLint)MGL_MAX_TEXTURE_LEVELS &&
        obj->levels[level].shadow_stale &&
        !materialize_stale_color_shadow(ctx, obj, level, &obj->levels[level])) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target == GL_TEXTURE_1D && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesBindTexture != NULL)
        mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D, obj->native_name,
                                     ctx->compat.active_texture);
    if (target == GL_TEXTURE_CUBE_MAP) {
        unsigned face;
        if (!cube_base_complete(obj, level)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        max_level = obj->immutable_format ? immutable_effective_max_level(obj) : obj->max_level;
        if (max_level >= (GLint)MGL_MAX_TEXTURE_LEVELS)
            max_level = (GLint)MGL_MAX_TEXTURE_LEVELS - 1;
        while (level < max_level && obj->cube_levels[0][level].width > 1) {
            for (face = 0u; face < 6u; ++face) {
                GLenum face_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
                if (!generate_mip_level_images(ctx, obj, face_target,
                                               &obj->cube_levels[face][level],
                                               &obj->cube_levels[face][level + 1], level + 1)) {
                    mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                    return;
                }
            }
            ++level;
        }
    } else {
        if (!obj->levels[level].defined || obj->levels[level].compressed) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        max_level = obj->immutable_format ? immutable_effective_max_level(obj) : obj->max_level;
        if (max_level >= (GLint)MGL_MAX_TEXTURE_LEVELS)
            max_level = (GLint)MGL_MAX_TEXTURE_LEVELS - 1;
        while (level < max_level) {
            const struct mgl_texture_level *src = &obj->levels[level];
            if (src->width <= 1 && src->height <= 1 && (target != GL_TEXTURE_3D || src->depth <= 1))
                break;
            if (!generate_mip_level_images(ctx, obj, target, src, &obj->levels[level + 1],
                                           level + 1)) {
                if (target == GL_TEXTURE_1D && ctx->native_dispatch != NULL &&
                    ctx->native_dispatch->glesBindTexture != NULL) {
                    struct mgl_texture *bound2d = mgl_bound_texture(ctx, GL_TEXTURE_2D);
                    mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D,
                                                 bound2d != NULL ? bound2d->native_name : 0u,
                                                 ctx->compat.active_texture);
                }
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return;
            }
            ++level;
        }
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGenerateMipmap != NULL &&
        (!obj->immutable_format || obj->native_immutable_storage)) {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_GENERATE_MIPMAP);
        ctx->native_dispatch->glesGenerateMipmap(target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target);
    }
    if (target == GL_TEXTURE_1D && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesBindTexture != NULL) {
        struct mgl_texture *bound2d = mgl_bound_texture(ctx, GL_TEXTURE_2D);
        mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D,
                                     bound2d != NULL ? bound2d->native_name : 0u,
                                     ctx->compat.active_texture);
    }
}

void glGetnCompressedTexImage(GLenum target, GLint level, GLsizei bufSize, void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    const struct mgl_texture_level *src_level;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (target != GL_TEXTURE_2D && target != GL_TEXTURE_3D &&
        target != GL_TEXTURE_2D_ARRAY && target != GL_TEXTURE_CUBE_MAP_ARRAY &&
        cube_face_index(target) < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    obj = mgl_bound_texture(ctx, cube_face_index(target) >= 0 ? GL_TEXTURE_CUBE_MAP : target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    src_level = texture_level_for_target(obj, target, level);
    if (!src_level->defined || !src_level->compressed) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (src_level->data_size > (size_t)bufSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    glGetCompressedTexImage(target, level, pixels);
}

void glGetCompressedTexImage(GLenum target, GLint level, void *img) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetCompressedTexImage",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"img", (uint64_t)(uintptr_t)img}},
                        3u);
    struct mgl_texture *obj;
    struct mgl_texture_level *src_level;
    struct mgl_buffer *pack_buffer = NULL;
    size_t pack_offset = 0u;
    unsigned char *dst;
    if (ctx == NULL)
        return;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_2D && target != GL_TEXTURE_3D &&
        target != GL_TEXTURE_2D_ARRAY && target != GL_TEXTURE_CUBE_MAP_ARRAY &&
        cube_face_index(target) < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    obj = mgl_bound_texture(ctx, cube_face_index(target) >= 0 ? GL_TEXTURE_CUBE_MAP : target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    src_level = texture_level_for_target(obj, target, level);
    if (!src_level->defined || !src_level->compressed) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (src_level->data_size != 0u && img == NULL && ctx->pixel_pack_buffer == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    dst = pixel_destination(ctx, img, src_level->data_size, &pack_buffer, &pack_offset);
    if (src_level->data_size != 0u && dst == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (src_level->data_size != 0u)
        memcpy(dst, src_level->data, src_level->data_size);
    if (pack_buffer != NULL && src_level->data_size != 0u)
        (void)mgl_buffer_write_bytes(ctx, pack_buffer, pack_offset, src_level->data_size, dst, 0);
}

static GLuint *active_texture_binding_slot(struct mgl_context *ctx, GLenum target) {
    unsigned unit;
    if (ctx == NULL)
        return NULL;
    unit = ctx->compat.active_texture;
    if (target == GL_TEXTURE_1D)
        return &ctx->texture_1d_binding[unit];
    if (target == GL_TEXTURE_2D)
        return &ctx->texture_2d_binding[unit];
    if (target == GL_TEXTURE_RECTANGLE)
        return &ctx->texture_rectangle_binding[unit];
    if (target == GL_TEXTURE_3D)
        return &ctx->texture_3d_binding[unit];
    if (target == GL_TEXTURE_2D_ARRAY)
        return &ctx->texture_2d_array_binding[unit];
    if (target == GL_TEXTURE_BUFFER)
        return &ctx->texture_buffer_binding[unit];
    if (target == GL_TEXTURE_CUBE_MAP)
        return &ctx->texture_cube_binding[unit];
    if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
        return &ctx->texture_cube_array_binding[unit];
    if (target == GL_TEXTURE_2D_MULTISAMPLE)
        return &ctx->texture_2d_multisample_binding[unit];
    if (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        return &ctx->texture_2d_multisample_array_binding[unit];
    return NULL;
}

static int texture_image_pack_span(const struct mgl_context *ctx, GLenum target,
                                   const struct mgl_texture_level *level, size_t bytes_per_pixel,
                                   size_t *out_span) {
    struct mgl_pixel_layout layout;
    GLsizei depth;
    if (ctx == NULL || level == NULL || out_span == NULL)
        return 0;
    depth = level->depth > 0 ? level->depth : 1;
    if (target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY ||
        target == GL_TEXTURE_CUBE_MAP_ARRAY) {
        if (!mgl_pixel_layout_3d(level->width, level->height, depth, bytes_per_pixel,
                                 ctx->pack_alignment, ctx->pack_row_length, ctx->pack_image_height,
                                 ctx->pack_skip_pixels, ctx->pack_skip_rows, ctx->pack_skip_images,
                                 &layout))
            return 0;
    } else {
        if (!mgl_pixel_layout_2d(level->width, level->height, bytes_per_pixel, ctx->pack_alignment,
                                 ctx->pack_row_length, ctx->pack_skip_pixels, ctx->pack_skip_rows,
                                 &layout))
            return 0;
    }
    *out_span = layout.span_bytes;
    return 1;
}

void glGetnTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize,
                    void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    struct mgl_texture_level *src;
    size_t pixel_bytes = 0u, required = 0u;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (bufSize < 0 || level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (target != GL_TEXTURE_1D && target != GL_TEXTURE_2D && target != GL_TEXTURE_RECTANGLE &&
        target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY &&
        target != GL_TEXTURE_CUBE_MAP_ARRAY && cube_face_index(target) < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!texture_readback_format_type(format, type, &pixel_bytes)) {
        mgl_error_set(&ctx->errors, texture_readback_format_type_error(format, type));
        return;
    }
    obj = mgl_bound_texture(ctx, cube_face_index(target) >= 0 ? GL_TEXTURE_CUBE_MAP : target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    src = texture_level_for_target(obj, target, level);
    if (src == NULL || !src->defined ||
        !texture_image_pack_span(ctx, target, src, pixel_bytes, &required)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((size_t)bufSize < required) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    glGetTexImage(target, level, format, type, pixels);
}

static int get_cube_texture_image(struct mgl_context *ctx, struct mgl_texture *obj, GLint level,
                                  GLenum format, GLenum type, GLsizei buf_size, void *pixels) {
    const struct mgl_texture_level *first;
    struct mgl_pixel_layout layout;
    struct mgl_buffer *pack_buffer = NULL;
    size_t pack_offset = 0u, dst_bpp = 0u, src_bpp = 0u;
    unsigned char *base, *dst;
    unsigned face;
    GLsizei y;
    if (ctx == NULL || obj == NULL)
        return 0;
    first = &obj->cube_levels[0][level];
    if (!first->defined) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    for (face = 1u; face < 6u; ++face) {
        const struct mgl_texture_level *candidate = &obj->cube_levels[face][level];
        if (!candidate->defined || candidate->compressed != first->compressed ||
            candidate->width != first->width || candidate->height != first->height ||
            candidate->internal_format != first->internal_format ||
            candidate->format != first->format || candidate->type != first->type) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
    }
    if (!texture_readback_format_type(format, type, &dst_bpp)) {
        mgl_error_set(&ctx->errors, texture_readback_format_type_error(format, type));
        return 0;
    }
    if (!first->compressed && !texture_readback_format_type(format, type, NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (!first->compressed &&
        (!texture_readback_format_compatible(first->format, format) ||
         (first->format != GL_DEPTH_COMPONENT && first->format != GL_DEPTH_STENCIL &&
          first->format != GL_STENCIL_INDEX &&
          texture_color_integer_format(first->format) != texture_color_integer_format(format)))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (!mgl_pixel_layout_3d(first->width, first->height, 6, dst_bpp, ctx->pack_alignment,
                             ctx->pack_row_length, ctx->pack_image_height, ctx->pack_skip_pixels,
                             ctx->pack_skip_rows, ctx->pack_skip_images, &layout)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if ((size_t)buf_size < layout.span_bytes) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (layout.span_bytes != 0u && pixels == NULL && ctx->pixel_pack_buffer == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    base = pixel_destination(ctx, pixels, layout.span_bytes, &pack_buffer, &pack_offset);
    if (layout.span_bytes != 0u && base == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    dst = layout.span_bytes != 0u ? base + layout.start_offset : base;

    if (first->compressed) {
        for (face = 0u; face < 6u; ++face) {
            const struct mgl_texture_level *src = &obj->cube_levels[face][level];
            if (!compressed_readback_region(ctx, src, 0, 0, 0, src->width, src->height, 1, format,
                                            type, dst_bpp, dst + (size_t)face * layout.image_stride,
                                            layout.row_stride, layout.image_stride))
                return 0;
        }
    } else if (first->format == GL_DEPTH_STENCIL || first->format == GL_STENCIL_INDEX) {
        if (!valid_format_type(first->format, first->type, &src_bpp)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        for (face = 0u; face < 6u; ++face) {
            const struct mgl_texture_level *src = &obj->cube_levels[face][level];
            for (y = 0; y < src->height; ++y) {
                size_t x;
                const unsigned char *src_row = src->data + (size_t)y * (size_t)src->width * src_bpp;
                unsigned char *dst_row =
                    dst + (size_t)face * layout.image_stride + (size_t)y * layout.row_stride;
                for (x = 0u; x < (size_t)src->width; ++x) {
                    unsigned char *pixel = dst_row + x * dst_bpp;
                    texture_readback_noncolor_pixel(src_row + x * src_bpp, first->format,
                                                    first->type, pixel, format, type);
                    if (ctx->pack_swap_bytes)
                        swap_elements(pixel, dst_bpp, texture_readback_swap_element_size(type));
                }
            }
        }
    } else if (first->format == GL_DEPTH_COMPONENT) {
        if (format != GL_DEPTH_COMPONENT ||
            !valid_format_type(first->format, first->type, &src_bpp)) {
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return 0;
        }
        for (face = 0u; face < 6u; ++face) {
            const struct mgl_texture_level *src = &obj->cube_levels[face][level];
            for (y = 0; y < src->height; ++y) {
                size_t x;
                const unsigned char *src_row = src->data + (size_t)y * (size_t)src->width * src_bpp;
                unsigned char *dst_row =
                    dst + (size_t)face * layout.image_stride + (size_t)y * layout.row_stride;
                for (x = 0u; x < (size_t)src->width; ++x) {
                    depth_store(dst_row + x * dst_bpp, type,
                                depth_load(src_row + x * src_bpp, src->type));
                    if (ctx->pack_swap_bytes && dst_bpp > 1u)
                        swap_elements(dst_row + x * dst_bpp, dst_bpp, dst_bpp);
                }
            }
        }
    } else {
        if (format == GL_DEPTH_COMPONENT ||
            !valid_format_type(first->format, first->type, &src_bpp)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        for (face = 0u; face < 6u; ++face) {
            const struct mgl_texture_level *src = &obj->cube_levels[face][level];
            for (y = 0; y < src->height; ++y) {
                size_t x;
                const unsigned char *src_row = src->data + (size_t)y * (size_t)src->width * src_bpp;
                unsigned char *dst_row =
                    dst + (size_t)face * layout.image_stride + (size_t)y * layout.row_stride;
                for (x = 0u; x < (size_t)src->width; ++x) {
                    unsigned char *pixel = dst_row + x * dst_bpp;
                    convert_texture_color_texel(src_row + x * src_bpp, src->format, src->type,
                                                pixel, format, type);
                    if (ctx->pack_swap_bytes)
                        swap_elements(pixel, dst_bpp, texture_readback_swap_element_size(type));
                }
            }
        }
    }
    if (pack_buffer != NULL && layout.span_bytes != 0u)
        (void)mgl_buffer_write_bytes(ctx, pack_buffer, pack_offset, layout.span_bytes, base, 0);
    return 1;
}

void glGetTextureImage(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize,
                       void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTextureImage",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"format", (uint64_t)(uintptr_t)format},
                                                       {"type", (uint64_t)(uintptr_t)type},
                                                       {"bufSize", (uint64_t)(uintptr_t)bufSize},
                                                       {"pixels", (uint64_t)(uintptr_t)pixels}},
                        6u);
    struct mgl_texture *obj;
    struct mgl_texture_level *src;
    GLuint *binding_slot;
    GLuint saved;
    size_t dst_bpp = 0u, required = 0u;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (bufSize < 0 || level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (obj->target != GL_TEXTURE_2D && obj->target != GL_TEXTURE_RECTANGLE &&
        obj->target != GL_TEXTURE_3D && obj->target != GL_TEXTURE_2D_ARRAY &&
        obj->target != GL_TEXTURE_CUBE_MAP_ARRAY && obj->target != GL_TEXTURE_CUBE_MAP) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (obj->target == GL_TEXTURE_CUBE_MAP) {
        (void)get_cube_texture_image(ctx, obj, level, format, type, bufSize, pixels);
        return;
    }
    if (!texture_readback_format_type(format, type, &dst_bpp)) {
        mgl_error_set(&ctx->errors, texture_readback_format_type_error(format, type));
        return;
    }
    src = &obj->levels[level];
    if (!src->defined) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!texture_image_pack_span(ctx, obj->target, src, dst_bpp, &required)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((size_t)bufSize < required) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    binding_slot = active_texture_binding_slot(ctx, obj->target);
    if (binding_slot == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    saved = *binding_slot;
    *binding_slot = texture;
    glGetTexImage(obj->target, level, format, type, pixels);
    *binding_slot = saved;
    dsa_native_restore_texture(ctx, obj->target);
}

static int get_compressed_cube_texture_image(struct mgl_context *ctx, struct mgl_texture *obj,
                                             GLint level, GLsizei buf_size, void *pixels) {
    struct mgl_buffer *pack_buffer = NULL;
    size_t pack_offset = 0u, total = 0u, cursor = 0u;
    unsigned char *dst;
    unsigned face;
    GLenum internal_format = 0u;
    if (ctx == NULL || obj == NULL)
        return 0;
    for (face = 0u; face < 6u; ++face) {
        const struct mgl_texture_level *src = &obj->cube_levels[face][level];
        if (!src->defined || !src->compressed) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        if (face == 0u)
            internal_format = src->internal_format;
        else if (src->internal_format != internal_format) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        if (src->data_size > SIZE_MAX - total) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        total += src->data_size;
    }
    if ((size_t)buf_size < total) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (total != 0u && pixels == NULL && ctx->pixel_pack_buffer == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    dst = pixel_destination(ctx, pixels, total, &pack_buffer, &pack_offset);
    if (total != 0u && dst == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    for (face = 0u; face < 6u; ++face) {
        const struct mgl_texture_level *src = &obj->cube_levels[face][level];
        if (src->data_size != 0u)
            memcpy(dst + cursor, src->data, src->data_size);
        cursor += src->data_size;
    }
    if (pack_buffer != NULL && total != 0u)
        (void)mgl_buffer_write_bytes(ctx, pack_buffer, pack_offset, total, dst, 0);
    return 1;
}

void glGetCompressedTextureImage(GLuint texture, GLint level, GLsizei bufSize, void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    struct mgl_texture_level *src;
    GLuint *binding_slot;
    GLuint saved;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (bufSize < 0 || level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (obj->target != GL_TEXTURE_2D && obj->target != GL_TEXTURE_RECTANGLE &&
        obj->target != GL_TEXTURE_3D && obj->target != GL_TEXTURE_2D_ARRAY &&
        obj->target != GL_TEXTURE_CUBE_MAP_ARRAY && obj->target != GL_TEXTURE_CUBE_MAP) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (obj->target == GL_TEXTURE_CUBE_MAP) {
        (void)get_compressed_cube_texture_image(ctx, obj, level, bufSize, pixels);
        return;
    }
    src = &obj->levels[level];
    if (!src->defined || !src->compressed) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((size_t)bufSize < src->data_size) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    binding_slot = active_texture_binding_slot(ctx, obj->target);
    if (binding_slot == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    saved = *binding_slot;
    *binding_slot = texture;
    glGetCompressedTexImage(obj->target, level, pixels);
    *binding_slot = saved;
    dsa_native_restore_texture(ctx, obj->target);
}

static int ext_dsa_query_bind(struct mgl_context *ctx, GLuint texture, GLenum target,
                              GLenum *object_target, GLuint **binding, GLuint *saved) {
    struct mgl_texture *obj;
    GLenum resolved_target;
    GLuint *slot;
    if (ctx == NULL || object_target == NULL || binding == NULL || saved == NULL)
        return 0;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return 0;
    resolved_target = ext_dsa_texture_object_target(target);
    slot = active_texture_binding_slot(ctx, resolved_target);
    if (slot == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    *object_target = resolved_target;
    *binding = slot;
    *saved = *slot;
    *slot = texture;
    return 1;
}

static void ext_dsa_query_unbind(struct mgl_context *ctx, GLenum object_target, GLuint *binding,
                                 GLuint saved, int restore_native) {
    if (binding == NULL)
        return;
    *binding = saved;
    if (restore_native)
        dsa_native_restore_texture(ctx, object_target);
}

static int texture_subimage_region(struct mgl_context *ctx, struct mgl_texture *obj, GLint level,
                                   GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,
                                   GLsizei height, GLsizei depth) {
    const struct mgl_texture_level *first;
    GLsizei image_depth;
    GLsizei image_height;
    int is_cube = obj->target == GL_TEXTURE_CUBE_MAP;
    int is_1d = obj->target == GL_TEXTURE_1D;
    int is_single = is_1d || obj->target == GL_TEXTURE_2D || obj->target == GL_TEXTURE_RECTANGLE;
    GLint face;
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0 || depth < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    first = is_cube ? &obj->cube_levels[zoffset >= 0 && zoffset < 6 ? zoffset : 0][level]
                    : &obj->levels[level];
    if (!first->defined) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    image_height = is_1d ? 1 : first->height;
    image_depth = is_cube ? 6 : is_single ? 1 : first->depth;
    if ((is_1d && (yoffset != 0 || height != 1)) || (is_single && (zoffset != 0 || depth != 1)) ||
        xoffset > first->width || width > first->width - xoffset || yoffset > image_height ||
        height > image_height - yoffset || zoffset > image_depth || depth > image_depth - zoffset) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (is_cube) {
        for (face = zoffset; face < zoffset + depth; ++face) {
            const struct mgl_texture_level *current = &obj->cube_levels[face][level];
            if (!current->defined || current->width != first->width ||
                current->height != first->height || current->compressed != first->compressed ||
                current->internal_format != first->internal_format) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return 0;
            }
        }
    }
    return 1;
}

void glGetTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                          GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                          GLsizei bufSize, void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    const struct mgl_texture_level *first;
    struct mgl_pixel_layout layout;
    struct mgl_buffer *pack_buffer = NULL;
    size_t bpp = 0u, source_bpp = 0u, pack_offset = 0u;
    unsigned char *destination, *native_image = NULL;
    GLfloat *native_depth = NULL;
    const unsigned char *source_image = NULL;
    GLenum source_format, source_type;
    GLsizei slice, row;
    int direct_copy, is_cube;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || obj->target == GL_TEXTURE_BUFFER ||
        obj->target == GL_TEXTURE_2D_MULTISAMPLE ||
        obj->target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!texture_readback_format_type(format, type, &bpp)) {
        mgl_error_set(&ctx->errors, texture_readback_format_type_error(format, type));
        return;
    }
    if (!texture_subimage_region(ctx, obj, level, xoffset, yoffset, zoffset, width, height, depth))
        return;
    is_cube = obj->target == GL_TEXTURE_CUBE_MAP;
    first = is_cube ? &obj->cube_levels[zoffset >= 0 && zoffset < 6 ? zoffset : 0][level]
                    : &obj->levels[level];
    if (!first->compressed && !texture_readback_format_type(format, type, NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!first->compressed &&
        (!texture_readback_format_compatible(first->format, format) ||
         (first->format != GL_DEPTH_COMPONENT && first->format != GL_DEPTH_STENCIL &&
          first->format != GL_STENCIL_INDEX &&
          texture_color_integer_format(first->format) != texture_color_integer_format(format)))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!first->compressed && !valid_format_type(first->format, first->type, &source_bpp)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (first->compressed ? !compressed_readback_supported(format, type)
                          : !texture_readback_format_compatible(first->format, format)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!(obj->target == GL_TEXTURE_3D || obj->target == GL_TEXTURE_2D_ARRAY ||
          obj->target == GL_TEXTURE_CUBE_MAP_ARRAY || is_cube
              ? mgl_pixel_layout_3d(width, height, depth, bpp, ctx->pack_alignment,
                                    ctx->pack_row_length, ctx->pack_image_height,
                                    ctx->pack_skip_pixels, ctx->pack_skip_rows,
                                    ctx->pack_skip_images, &layout)
              : mgl_pixel_layout_2d(width, height, bpp, ctx->pack_alignment, ctx->pack_row_length,
                                    ctx->pack_skip_pixels, ctx->pack_skip_rows, &layout)) ||
        layout.span_bytes > (size_t)bufSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (width == 0 || height == 0 || depth == 0)
        return;
    if (pixels == NULL && ctx->pixel_pack_buffer == 0u)
        return;
    destination = pixel_destination(ctx, pixels, layout.span_bytes, &pack_buffer, &pack_offset);
    if (destination == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (first->compressed) {
        for (slice = 0; slice < depth; ++slice) {
            const struct mgl_texture_level *src =
                is_cube ? &obj->cube_levels[zoffset + slice][level] : first;
            if (!compressed_readback_region(
                    ctx, src, xoffset, yoffset, is_cube ? 0 : zoffset + slice, width, height, 1,
                    format, type, bpp,
                    destination + layout.start_offset + (size_t)slice * layout.image_stride,
                    layout.row_stride, layout.image_stride))
                return;
        }
        if (pack_buffer != NULL)
            (void)mgl_buffer_write_bytes(ctx, pack_buffer, pack_offset, layout.span_bytes,
                                         destination, 0);
        return;
    }
    source_format = first->format;
    source_type = first->type;
    if (first->format == GL_DEPTH_COMPONENT && obj->target == GL_TEXTURE_2D) {
        size_t pixels_count;
        if (checked_mul((size_t)width, (size_t)height, &pixels_count) &&
            pixels_count <= SIZE_MAX / sizeof(*native_depth)) {
            native_depth = malloc(pixels_count * sizeof(*native_depth));
            if (native_depth != NULL &&
                !mgl_depth_texture_readback(ctx, obj, level, xoffset, yoffset, width, height,
                                            native_depth)) {
                free(native_depth);
                native_depth = NULL;
            }
        }
    }
    if (!is_cube && obj->target == GL_TEXTURE_2D && first->format != GL_DEPTH_COMPONENT &&
        obj->used_as_color_attachment &&
        !(obj->color_attachment_incomplete && !obj->color_attachment_complete) &&
        ctx->native_dispatch != NULL && ctx->native_dispatch->glesReadPixels != NULL) {
        if (!read_rendered_color_texture(ctx, obj, level, first->width, first->height,
                                         &native_image))
            return;
        source_image = native_image;
        source_format = GL_RGBA;
        source_type = GL_UNSIGNED_BYTE;
        source_bpp = 4u;
    }
    if (first->internal_format == GL_INTENSITY8 && source_format == GL_RED)
        source_format = GL_INTENSITY;
    direct_copy = first->format != GL_DEPTH_COMPONENT && source_format == format &&
                  source_type == type && !ctx->pack_swap_bytes;
    for (slice = 0; slice < depth; ++slice) {
        const struct mgl_texture_level *src =
            is_cube ? &obj->cube_levels[zoffset + slice][level] : first;
        size_t available;
        if (source_image != NULL || native_depth != NULL)
            continue;
        if (!checked_mul((size_t)src->width, (size_t)src->height, &available) ||
            !checked_mul(available,
                         (size_t)(is_cube          ? 1
                                  : src->depth > 0 ? src->depth
                                                   : 1),
                         &available) ||
            !checked_mul(available, source_bpp, &available) || src->data == NULL ||
            src->data_size < available) {
            free(native_image);
            free(native_depth);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
    }
    for (slice = 0; slice < depth; ++slice) {
        const struct mgl_texture_level *src =
            is_cube ? &obj->cube_levels[zoffset + slice][level] : first;
        const unsigned char *data = source_image != NULL ? source_image : src->data;
        size_t source_slice = is_cube ? 0u
                                      : (size_t)(zoffset + slice) * (size_t)src->height *
                                            (size_t)src->width * source_bpp;
        for (row = 0; row < height; ++row) {
            const unsigned char *from =
                data != NULL
                    ? data + source_slice +
                          ((size_t)(yoffset + row) * (size_t)src->width + (size_t)xoffset) *
                              source_bpp
                    : NULL;
            unsigned char *to = destination + layout.start_offset +
                                (size_t)slice * layout.image_stride +
                                (size_t)row * layout.row_stride;
            GLsizei col;
            if (direct_copy) {
                memcpy(to, from, (size_t)width * bpp);
            } else if (source_format == GL_DEPTH_STENCIL || source_format == GL_STENCIL_INDEX) {
                for (col = 0; col < width; ++col) {
                    unsigned char *pixel = to + (size_t)col * bpp;
                    texture_readback_noncolor_pixel(from + (size_t)col * source_bpp, source_format,
                                                    source_type, pixel, format, type);
                    if (ctx->pack_swap_bytes)
                        swap_elements(pixel, bpp, texture_readback_swap_element_size(type));
                }
            } else if (source_format == GL_DEPTH_COMPONENT) {
                for (col = 0; col < width; ++col) {
                    depth_store(
                        to + (size_t)col * bpp, type,
                        native_depth != NULL
                            ? (double)native_depth[(size_t)row * (size_t)width + (size_t)col]
                            : depth_load(from + (size_t)col * source_bpp, source_type));
                    if (ctx->pack_swap_bytes && bpp > 1u)
                        swap_elements(to + (size_t)col * bpp, bpp, bpp);
                }
            } else {
                for (col = 0; col < width; ++col) {
                    unsigned char *pixel = to + (size_t)col * bpp;
                    convert_texture_color_texel(from + (size_t)col * source_bpp, source_format,
                                                source_type, pixel, format, type);
                    if (ctx->pack_swap_bytes)
                        swap_elements(pixel, bpp, texture_readback_swap_element_size(type));
                }
            }
        }
    }
    free(native_image);
    free(native_depth);
    if (pack_buffer != NULL)
        (void)mgl_buffer_write_bytes(ctx, pack_buffer, pack_offset, layout.span_bytes, destination,
                                     0);
}

void glGetCompressedTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                    GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                    GLsizei bufSize, void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    const struct mgl_texture_level *first;
    struct mgl_buffer *pack_buffer = NULL;
    size_t block_bytes, src_blocks, row_blocks, rows, slice_bytes, total, pack_offset = 0u;
    unsigned char *destination;
    GLsizei slice;
    size_t row;
    int is_cube;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || obj->target == GL_TEXTURE_BUFFER ||
        obj->target == GL_TEXTURE_2D_MULTISAMPLE ||
        obj->target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!texture_subimage_region(ctx, obj, level, xoffset, yoffset, zoffset, width, height, depth))
        return;
    is_cube = obj->target == GL_TEXTURE_CUBE_MAP;
    first = is_cube ? &obj->cube_levels[zoffset >= 0 && zoffset < 6 ? zoffset : 0][level]
                    : &obj->levels[level];
    block_bytes = compressed_block_bytes(first->internal_format);
    if (!first->compressed || block_bytes == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((xoffset % 4) != 0 || (yoffset % 4) != 0 ||
        ((width % 4) != 0 && xoffset + width != first->width) ||
        ((height % 4) != 0 && yoffset + height != first->height)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    src_blocks = ((size_t)first->width + 3u) / 4u;
    row_blocks = ((size_t)width + 3u) / 4u;
    rows = ((size_t)height + 3u) / 4u;
    if (!checked_mul(row_blocks, block_bytes, &slice_bytes) ||
        !checked_mul(slice_bytes, rows, &slice_bytes) ||
        !checked_mul(slice_bytes, (size_t)depth, &total) || total > (size_t)bufSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (width == 0 || height == 0 || depth == 0)
        return;
    if (pixels == NULL && ctx->pixel_pack_buffer == 0u)
        return;
    for (slice = 0; slice < depth; ++slice) {
        const struct mgl_texture_level *src =
            is_cube ? &obj->cube_levels[zoffset + slice][level] : first;
        size_t source_bytes, base, row_end;
        if (!checked_mul(src_blocks, ((size_t)src->height + 3u) / 4u, &source_bytes) ||
            !checked_mul(source_bytes, block_bytes, &source_bytes) ||
            !checked_mul((size_t)(is_cube ? 0 : zoffset + slice), source_bytes, &base) ||
            !checked_mul(((size_t)yoffset / 4u + rows - 1u) * src_blocks + (size_t)xoffset / 4u +
                             row_blocks,
                         block_bytes, &row_end) ||
            base > src->data_size || row_end > src->data_size - base || src->data == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
    }
    destination = pixel_destination(ctx, pixels, total, &pack_buffer, &pack_offset);
    if (destination == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    for (slice = 0; slice < depth; ++slice) {
        const struct mgl_texture_level *src =
            is_cube ? &obj->cube_levels[zoffset + slice][level] : first;
        size_t src_slice_bytes, source_offset;
        if (!checked_mul(src_blocks, ((size_t)src->height + 3u) / 4u, &src_slice_bytes) ||
            !checked_mul(src_slice_bytes, block_bytes, &src_slice_bytes))
            return;
        source_offset = is_cube ? 0u : ((size_t)zoffset + (size_t)slice) * src_slice_bytes;
        for (row = 0u; row < rows; ++row) {
            size_t offset =
                source_offset +
                (((size_t)yoffset / 4u + row) * src_blocks + (size_t)xoffset / 4u) * block_bytes;
            memcpy(destination + (size_t)slice * slice_bytes + row * row_blocks * block_bytes,
                   src->data + offset, row_blocks * block_bytes);
        }
    }
    if (pack_buffer != NULL)
        (void)mgl_buffer_write_bytes(ctx, pack_buffer, pack_offset, total, destination, 0);
}

void glGetTextureImageEXT(GLuint texture, GLenum target, GLint level, GLenum format, GLenum type,
                          void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTextureImageEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"format", (uint64_t)(uintptr_t)format},
                                                       {"type", (uint64_t)(uintptr_t)type},
                                                       {"pixels", (uint64_t)(uintptr_t)pixels}},
                        6u);
    GLenum object_target;
    GLuint *binding;
    GLuint saved;
    size_t components;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_2d_image_target(target) && !valid_3d_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!texture_readback_format_type(format, type, &components)) {
        mgl_error_set(&ctx->errors, texture_readback_format_type_error(format, type));
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_query_bind(ctx, texture, target, &object_target, &binding, &saved))
        return;
    glGetTexImage(target, level, format, type, pixels);
    ext_dsa_query_unbind(ctx, object_target, binding, saved, 1);
}

void glGetCompressedTextureImageEXT(GLuint texture, GLenum target, GLint level, void *img) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetCompressedTextureImageEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"img", (uint64_t)(uintptr_t)img}},
                        4u);
    GLenum object_target;
    GLuint *binding;
    GLuint saved;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_2d_image_target(target) && !valid_3d_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_query_bind(ctx, texture, target, &object_target, &binding, &saved))
        return;
    glGetCompressedTexImage(target, level, img);
    ext_dsa_query_unbind(ctx, object_target, binding, saved, 1);
}

void glGetTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTextureParameterivEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    GLenum object_target;
    GLuint *binding;
    GLuint saved;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!ext_dsa_query_bind(ctx, texture, target, &object_target, &binding, &saved))
        return;
    glGetTexParameteriv(target, pname, params);
    ext_dsa_query_unbind(ctx, object_target, binding, saved, 0);
}

void glGetTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTextureParameterfvEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    GLenum object_target;
    GLuint *binding;
    GLuint saved;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!ext_dsa_query_bind(ctx, texture, target, &object_target, &binding, &saved))
        return;
    glGetTexParameterfv(target, pname, params);
    ext_dsa_query_unbind(ctx, object_target, binding, saved, 0);
}

void glGetTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTextureParameterIivEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glGetTextureParameterivEXT(texture, target, pname, params);
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return;
    get_texture_border_color_i(obj, params);
}

void glGetTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname, GLuint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTextureParameterIuivEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    GLint value = 0;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetTextureParameterivEXT(texture, target, pname, &value);
        if (ctx->errors.sequence == error_sequence_before)
            params[0] = (GLuint)value;
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return;
    get_texture_border_color_ui(obj, params);
}

static int texture_level_component_pname(GLenum pname) {
    switch (pname) {
        case GL_TEXTURE_RED_SIZE:
        case GL_TEXTURE_GREEN_SIZE:
        case GL_TEXTURE_BLUE_SIZE:
        case GL_TEXTURE_ALPHA_SIZE:
        case GL_TEXTURE_DEPTH_SIZE:
        case GL_TEXTURE_STENCIL_SIZE:
        case GL_TEXTURE_SHARED_SIZE:
        case GL_TEXTURE_RED_TYPE:
        case GL_TEXTURE_GREEN_TYPE:
        case GL_TEXTURE_BLUE_TYPE:
        case GL_TEXTURE_ALPHA_TYPE:
        case GL_TEXTURE_DEPTH_TYPE:
            return 1;
        default:
            return 0;
    }
}

void glGetTextureLevelParameterivEXT(GLuint texture, GLenum target, GLint level, GLenum pname,
                                     GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTextureLevelParameterivEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        5u);
    GLenum object_target;
    GLuint *binding;
    GLuint saved;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_level_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if ((target == GL_TEXTURE_BUFFER || target == GL_TEXTURE_RECTANGLE ||
         target == GL_TEXTURE_2D_MULTISAMPLE ||
         target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) && level != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_query_bind(ctx, texture, target, &object_target, &binding, &saved))
        return;
    glGetTexLevelParameteriv(target, level, pname, params);
    ext_dsa_query_unbind(ctx, object_target, binding, saved, 0);
}

void glGetTextureLevelParameterfvEXT(GLuint texture, GLenum target, GLint level, GLenum pname,
                                     GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTextureLevelParameterfvEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        5u);
    GLenum object_target;
    GLuint *binding;
    GLuint saved;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_level_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if ((target == GL_TEXTURE_BUFFER || target == GL_TEXTURE_RECTANGLE ||
         target == GL_TEXTURE_2D_MULTISAMPLE ||
         target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) && level != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!ext_dsa_query_bind(ctx, texture, target, &object_target, &binding, &saved))
        return;
    glGetTexLevelParameterfv(target, level, pname, params);
    ext_dsa_query_unbind(ctx, object_target, binding, saved, 0);
}

void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTexLevelParameteriv",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    struct mgl_texture_level *src_level;
    if (ctx == NULL || params == NULL)
        return;
    /* Queries are illegal inside Begin/End,
     * independently of target, level and pname validation. */
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!(valid_level_target(target) || target == GL_PROXY_TEXTURE_2D ||
          target == GL_PROXY_TEXTURE_1D)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if ((target == GL_TEXTURE_BUFFER || target == GL_TEXTURE_RECTANGLE ||
         target == GL_TEXTURE_2D_MULTISAMPLE ||
         target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) && level != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    obj = (target == GL_PROXY_TEXTURE_2D || target == GL_PROXY_TEXTURE_1D)
              ? NULL
              : mgl_bound_texture(ctx, cube_face_index(target) >= 0 ? GL_TEXTURE_CUBE_MAP : target);
    if (target == GL_TEXTURE_BUFFER) {
        struct mgl_buffer *buffer;
        size_t texel_size;
        size_t width;
        if (obj == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        buffer = obj->buffer_texture_buffer;
        texel_size = texture_buffer_texel_size(obj->buffer_texture_internal_format);
        width = (buffer != NULL && texel_size != 0u && obj->buffer_texture_size > 0)
                    ? (size_t)obj->buffer_texture_size / texel_size
                    : 0u;
        if (width > MGL_MAX_TEXTURE_BUFFER_SIZE)
            width = MGL_MAX_TEXTURE_BUFFER_SIZE;
        if (texture_level_component_pname(pname)) {
            *params = mgl_texture_level_component_value(
                buffer != NULL ? obj->buffer_texture_internal_format : GL_NONE, pname);
            return;
        }
        switch (pname) {
            case GL_TEXTURE_WIDTH:
                *params = (GLint)width;
                return;
            case GL_TEXTURE_HEIGHT:
                *params = buffer != NULL ? 1 : 0;
                return;
            case GL_TEXTURE_DEPTH:
                *params = buffer != NULL ? 1 : 0;
                return;
            case GL_TEXTURE_INTERNAL_FORMAT:
                *params = (GLint)obj->buffer_texture_internal_format;
                return;
            case GL_TEXTURE_BUFFER_DATA_STORE_BINDING:
                *params = buffer != NULL ? (GLint)buffer->name : 0;
                return;
            case GL_TEXTURE_BUFFER_OFFSET:
                *params = buffer != NULL ? (GLint)obj->buffer_texture_offset : 0;
                return;
            case GL_TEXTURE_BUFFER_SIZE:
                *params = buffer != NULL ? (GLint)obj->buffer_texture_size : 0;
                return;
            case GL_TEXTURE_BORDER:
            case GL_TEXTURE_SAMPLES:
                *params = 0;
                return;
            case GL_TEXTURE_FIXED_SAMPLE_LOCATIONS:
                *params = GL_TRUE;
                return;
            case GL_TEXTURE_COMPRESSED:
                *params = 0;
                return;
            case GL_TEXTURE_COMPRESSED_IMAGE_SIZE:
                *params = 0;
                return;
            default:
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return;
        }
    }
    if (target == GL_PROXY_TEXTURE_1D) {
        src_level = &ctx->proxy_texture_1d_levels[level];
    } else if (target == GL_PROXY_TEXTURE_2D) {
        src_level = &ctx->proxy_texture_2d_levels[level];
    } else {
        if (obj == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        src_level = texture_level_for_target(obj, target, level);
    }
    if (src_level == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname == GL_TEXTURE_COMPRESSED_IMAGE_SIZE &&
        (target == GL_PROXY_TEXTURE_1D || target == GL_PROXY_TEXTURE_2D ||
         !src_level->compressed)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname == GL_TEXTURE_STENCIL_SIZE || pname == GL_TEXTURE_SHARED_SIZE ||
        pname == GL_TEXTURE_RED_TYPE || pname == GL_TEXTURE_GREEN_TYPE ||
        pname == GL_TEXTURE_BLUE_TYPE || pname == GL_TEXTURE_ALPHA_TYPE ||
        pname == GL_TEXTURE_DEPTH_TYPE) {
        *params = mgl_texture_level_component_value(
            src_level->defined ? src_level->internal_format : GL_NONE, pname);
        return;
    }
    if (pname == GL_TEXTURE_RED_SIZE || pname == GL_TEXTURE_GREEN_SIZE ||
        pname == GL_TEXTURE_BLUE_SIZE || pname == GL_TEXTURE_ALPHA_SIZE ||
        pname == GL_TEXTURE_DEPTH_SIZE) {
        GLenum internal = src_level->defined ? src_level->internal_format : 0u;
        GLint bits = mgl_texture_level_component_value(internal, pname);
        if (pname == GL_TEXTURE_DEPTH_SIZE) {
            if (internal == GL_DEPTH_COMPONENT16)
                bits = 16;
            else if (internal == GL_DEPTH_COMPONENT24)
                bits = 24;
            else if (internal == GL_DEPTH_COMPONENT32 || internal == GL_DEPTH_COMPONENT32F ||
                     internal == GL_DEPTH32F_STENCIL8)
                bits = 32;
            else if (internal == GL_DEPTH_COMPONENT)
                bits = 16;
        } else if (internal == GL_INTENSITY8) {
            bits = pname == GL_TEXTURE_RED_SIZE || pname == GL_TEXTURE_GREEN_SIZE ||
                           pname == GL_TEXTURE_BLUE_SIZE || pname == GL_TEXTURE_ALPHA_SIZE
                       ? 8
                       : 0;
        } else if (internal == GL_RGBA16) {
            bits = rgba16_gpu_native(ctx) ? 16 : 8;
        } else if (internal == GL_RGBA16F || internal == GL_RGB16F || internal == GL_ALPHA16F_ARB ||
                   internal == GL_LUMINANCE16F_ARB || internal == GL_LUMINANCE_ALPHA16F_ARB) {
            bits = internal == GL_ALPHA16F_ARB ? (pname == GL_TEXTURE_ALPHA_SIZE ? 16 : 0)
                   : internal == GL_LUMINANCE_ALPHA16F_ARB
                       ? (pname == GL_TEXTURE_ALPHA_SIZE || pname == GL_TEXTURE_RED_SIZE ||
                                  pname == GL_TEXTURE_GREEN_SIZE || pname == GL_TEXTURE_BLUE_SIZE
                              ? 16
                              : 0)
                   : internal == GL_LUMINANCE16F_ARB ? (pname == GL_TEXTURE_ALPHA_SIZE ? 0 : 16)
                   : internal == GL_RGB16F && pname == GL_TEXTURE_ALPHA_SIZE ? 0
                                                                             : 16;
        } else if (internal == GL_RGB32F_ARB || internal == GL_ALPHA32F_ARB ||
                   internal == GL_LUMINANCE32F_ARB || internal == GL_LUMINANCE_ALPHA32F_ARB) {
            bits = internal == GL_ALPHA32F_ARB ? (pname == GL_TEXTURE_ALPHA_SIZE ? 32 : 0)
                   : internal == GL_LUMINANCE_ALPHA32F_ARB ? 32
                   : internal == GL_LUMINANCE32F_ARB ? (pname == GL_TEXTURE_ALPHA_SIZE ? 0 : 32)
                                                     : (pname == GL_TEXTURE_ALPHA_SIZE ? 0 : 32);
        } else if (internal == GL_RGB5) {
            if (pname == GL_TEXTURE_RED_SIZE || pname == GL_TEXTURE_BLUE_SIZE)
                bits = 5;
            else if (pname == GL_TEXTURE_GREEN_SIZE)
                bits = 6;
        } else if (internal == GL_RGBA || internal == GL_RGBA8 || internal == GL_SRGB8_ALPHA8 ||
                   internal == GL_RGB || internal == GL_RGB8 || internal == GL_SRGB8) {
            if (pname != GL_TEXTURE_ALPHA_SIZE || internal == GL_RGBA || internal == GL_RGBA8 ||
                internal == GL_SRGB8_ALPHA8)
                bits = 8;
        }
        *params = bits;
        return;
    }
    switch (pname) {
        case GL_TEXTURE_WIDTH:
            *params = src_level->defined ? src_level->width : 0;
            return;
        case GL_TEXTURE_HEIGHT:
            *params = src_level->defined ? src_level->height : 0;
            return;
        case GL_TEXTURE_DEPTH:
            *params = src_level->defined ? src_level->depth : 0;
            return;
        case GL_TEXTURE_SAMPLES:
            *params = src_level->defined ? src_level->samples : 0;
            return;
        case GL_TEXTURE_FIXED_SAMPLE_LOCATIONS:
            *params = (src_level->defined && src_level->fixed_sample_locations) ? 1 : 0;
            return;
        case GL_TEXTURE_INTERNAL_FORMAT:
            *params = src_level->defined ? (GLint)src_level->internal_format : 0;
            return;
        case GL_TEXTURE_COMPRESSED:
            *params = (src_level->defined && src_level->compressed) ? 1 : 0;
            return;
        case GL_TEXTURE_COMPRESSED_IMAGE_SIZE:
            *params =
                (src_level->defined && src_level->compressed) ? (GLint)src_level->data_size : 0;
            return;
        case GL_TEXTURE_BORDER:
            *params = 0;
            return;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
    }
}

void glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params) {
    GLint value = 0;
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || params == NULL)
        return;

    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetTexLevelParameteriv(target, level, pname, &value);
    if (ctx->errors.sequence == error_sequence_before)
        *params = (GLfloat)value;
}

void glMultiTexImage2D(GLenum texunit, GLenum target, GLint level, GLint internalformat,
                       GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
                       const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glMultiTexImage2D",
        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                       {"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"border", (uint64_t)(uintptr_t)border},
                                       {"format", (uint64_t)(uintptr_t)format}},
        8u);
    size_t components = 0u, data_size = 0u;
    const unsigned char *data;
    unsigned char *owned = NULL;
    unsigned unit;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {

        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_multi_tex_image_2d(ctx, texunit, target, level, internalformat, width,
                                               height, border, format, type, NULL, 0u))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_2d_image_target(target) || !valid_format_type(format, type, &components) ||
        !valid_internal(internalformat) || !format_matches_internal(format, internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        border != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    data = tight_source_2d(ctx, pixels, width, height, components, pixel_element_size(type),
                           &data_size, &owned);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress =
        mgl_list_record_multi_tex_image_2d(ctx, texunit, target, level, internalformat, width,
                                           height, border, format, type, data, data_size);
    if (!suppress)
        (void)mgl_texture_apply_image_2d_tight_on_unit(ctx, unit, target, level, internalformat,
                                                       width, height, border, format, type, data,
                                                       data_size);
    free(owned);
}

void glMultiTexSubImage2D(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset,
                          GLsizei width, GLsizei height, GLenum format, GLenum type,
                          const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glMultiTexSubImage2D",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height},
                                                       {"format", (uint64_t)(uintptr_t)format}},
                        8u);
    size_t components = 0u, data_size = 0u;
    const unsigned char *data;
    unsigned char *owned = NULL;
    unsigned unit;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {

        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_multi_tex_sub_image_2d(ctx, texunit, target, level, xoffset, yoffset,
                                                   width, height, format, type, NULL, 0u))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_2d_image_target(target) || !valid_format_type(format, type, &components)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    data = tight_source_2d(ctx, pixels, width, height, components, pixel_element_size(type),
                           &data_size, &owned);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_multi_tex_sub_image_2d(ctx, texunit, target, level, xoffset, yoffset,
                                                      width, height, format, type, data, data_size);
    if (!suppress)
        (void)mgl_texture_apply_sub_image_2d_tight_on_unit(ctx, unit, target, level, xoffset,
                                                           yoffset, width, height, format, type,
                                                           data, data_size);
    free(owned);
}

void glMultiTexImage3D(GLenum texunit, GLenum target, GLint level, GLint internalformat,
                       GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format,
                       GLenum type, const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glMultiTexImage3D",
        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                       {"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"depth", (uint64_t)(uintptr_t)depth},
                                       {"border", (uint64_t)(uintptr_t)border}},
        8u);
    size_t components = 0u, data_size = 0u;
    const unsigned char *data;
    unsigned char *owned = NULL;
    unsigned unit;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {

        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_multi_tex_image_3d(ctx, texunit, target, level, internalformat, width,
                                               height, depth, border, format, type, NULL, 0u))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_3d_target(target) || !valid_format_type(format, type, &components) ||
        !valid_internal(internalformat) || !format_matches_internal(format, internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        depth < 0 || border != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    data = tight_source_3d(ctx, pixels, width, height, depth, components, pixel_element_size(type),
                           &data_size, &owned);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress =
        mgl_list_record_multi_tex_image_3d(ctx, texunit, target, level, internalformat, width,
                                           height, depth, border, format, type, data, data_size);
    if (!suppress)
        (void)mgl_texture_apply_image_3d_tight_on_unit(ctx, unit, target, level, internalformat,
                                                       width, height, depth, border, format, type,
                                                       data, data_size);
    free(owned);
}

void glMultiTexSubImage3D(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset,
                          GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                          GLenum format, GLenum type, const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glMultiTexSubImage3D",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"zoffset", (uint64_t)(uintptr_t)zoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height}},
                        8u);
    size_t components = 0u, data_size = 0u;
    const unsigned char *data;
    unsigned char *owned = NULL;
    unsigned unit;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {

        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_multi_tex_sub_image_3d(ctx, texunit, target, level, xoffset, yoffset,
                                                   zoffset, width, height, depth, format, type,
                                                   NULL, 0u))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_3d_target(target) || !valid_format_type(format, type, &components)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0 || depth < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    data = tight_source_3d(ctx, pixels, width, height, depth, components, pixel_element_size(type),
                           &data_size, &owned);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_multi_tex_sub_image_3d(ctx, texunit, target, level, xoffset, yoffset,
                                                      zoffset, width, height, depth, format, type,
                                                      data, data_size);
    if (!suppress)
        (void)mgl_texture_apply_sub_image_3d_tight_on_unit(ctx, unit, target, level, xoffset,
                                                           yoffset, zoffset, width, height, depth,
                                                           format, type, data, data_size);
    free(owned);
}

void glCompressedMultiTexImage2D(GLenum texunit, GLenum target, GLint level, GLenum internalformat,
                                 GLsizei width, GLsizei height, GLint border, GLsizei imageSize,
                                 const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glCompressedMultiTexImage2D",
        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                       {"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"border", (uint64_t)(uintptr_t)border},
                                       {"imageSize", (uint64_t)(uintptr_t)imageSize}},
        8u);
    const unsigned char *owned;
    unsigned char *staged = NULL;
    size_t expected = 0u;
    unsigned unit;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_target(target) || compressed_block_bytes(internalformat) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        border != 0 || imageSize < 0 ||
        !compressed_image_size(internalformat, width, height, &expected) ||
        expected != (size_t)imageSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    owned = compressed_source(ctx, data, expected, &staged);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_compressed_multi_tex_image_2d(
        ctx, texunit, target, level, internalformat, width, height, border, owned, expected);
    if (!suppress)
        (void)mgl_texture_apply_compressed_image_2d_tight_on_unit(
            ctx, unit, target, level, internalformat, width, height, border, owned, expected);
    free(staged);
}

void glCompressedMultiTexSubImage2D(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                                    GLint yoffset, GLsizei width, GLsizei height, GLenum format,
                                    GLsizei imageSize, const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glCompressedMultiTexSubImage2D",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height},
                                                       {"format", (uint64_t)(uintptr_t)format}},
                        8u);
    const unsigned char *owned;
    unsigned char *staged = NULL;
    size_t expected = 0u;
    unsigned unit;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_target(target) || compressed_block_bytes(format) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        width < 0 || height < 0 || imageSize < 0 ||
        !compressed_image_size(format, width, height, &expected) || expected != (size_t)imageSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    owned = compressed_source(ctx, data, expected, &staged);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_compressed_multi_tex_sub_image_2d(
        ctx, texunit, target, level, xoffset, yoffset, width, height, format, owned, expected);
    if (!suppress)
        (void)mgl_texture_apply_compressed_sub_image_2d_tight_on_unit(
            ctx, unit, target, level, xoffset, yoffset, width, height, format, owned, expected);
    free(staged);
}

void glCompressedMultiTexImage3D(GLenum texunit, GLenum target, GLint level, GLenum internalformat,
                                 GLsizei width, GLsizei height, GLsizei depth, GLint border,
                                 GLsizei imageSize, const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glCompressedMultiTexImage3D",
        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                       {"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height},
                                       {"depth", (uint64_t)(uintptr_t)depth},
                                       {"border", (uint64_t)(uintptr_t)border}},
        8u);
    const unsigned char *owned;
    unsigned char *staged = NULL;
    size_t expected = 0u;
    unsigned unit;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_3d_target(target) || compressed_block_bytes(internalformat) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (border != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        depth < 0 || imageSize < 0 ||
        !compressed_image_size_3d(internalformat, width, height, depth, &expected) ||
        expected != (size_t)imageSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    owned = compressed_source(ctx, data, expected, &staged);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_compressed_multi_tex_image_3d(
        ctx, texunit, target, level, internalformat, width, height, depth, border, owned, expected);
    if (!suppress)
        (void)mgl_texture_apply_compressed_image_3d_tight_on_unit(ctx, unit, target, level,
                                                                  internalformat, width, height,
                                                                  depth, border, owned, expected);
    free(staged);
}

void glCompressedMultiTexSubImage3D(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                                    GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                                    GLsizei depth, GLenum format, GLsizei imageSize,
                                    const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glCompressedMultiTexSubImage3D",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"zoffset", (uint64_t)(uintptr_t)zoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height}},
                        8u);
    const unsigned char *owned;
    unsigned char *staged = NULL;
    size_t expected = 0u;
    unsigned unit;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_3d_target(target) || compressed_block_bytes(format) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0 || depth < 0 || imageSize < 0 ||
        !compressed_image_size_3d(format, width, height, depth, &expected) ||
        expected != (size_t)imageSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    owned = compressed_source(ctx, data, expected, &staged);
    if (owned == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_compressed_multi_tex_sub_image_3d(
        ctx, texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, owned,
        expected);
    if (!suppress)
        (void)mgl_texture_apply_compressed_sub_image_3d_tight_on_unit(
            ctx, unit, target, level, xoffset, yoffset, zoffset, width, height, depth, format,
            owned, expected);
    free(staged);
}

void glCopyMultiTexImage2D(GLenum texunit, GLenum target, GLint level, GLenum internalformat,
                           GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glCopyMultiTexImage2D",
        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                       {"target", (uint64_t)(uintptr_t)target},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"internalformat", (uint64_t)(uintptr_t)internalformat},
                                       {"x", (uint64_t)(uintptr_t)x},
                                       {"y", (uint64_t)(uintptr_t)y},
                                       {"width", (uint64_t)(uintptr_t)width},
                                       {"height", (uint64_t)(uintptr_t)height}},
        8u);
    unsigned unit, saved_unit;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {

        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_copy_multi_tex_image_2d(ctx, texunit, target, level, internalformat, x,
                                                    y, width, height, border))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_2d_image_target(target) || !valid_internal((GLint)internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 ||
        border != 0 || (cube_face_index(target) >= 0 && width != height)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    suppress = mgl_list_record_copy_multi_tex_image_2d(ctx, texunit, target, level, internalformat,
                                                       x, y, width, height, border);
    if (suppress)
        return;
    if (!explicit_unit_apply_begin(ctx, unit, &saved_unit))
        return;
    copy_tex_image_2d_apply(ctx, target, level, internalformat, x, y, width, height, border);
    explicit_unit_apply_end(ctx, saved_unit);
}

void glCopyMultiTexSubImage2D(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                              GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glCopyMultiTexSubImage2D",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"x", (uint64_t)(uintptr_t)x},
                                                       {"y", (uint64_t)(uintptr_t)y},
                                                       {"width", (uint64_t)(uintptr_t)width}},
                        8u);
    unsigned unit, saved_unit;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {

        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_copy_multi_tex_sub_image_2d(ctx, texunit, target, level, xoffset,
                                                        yoffset, x, y, width, height))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_2d_image_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    suppress = mgl_list_record_copy_multi_tex_sub_image_2d(ctx, texunit, target, level, xoffset,
                                                           yoffset, x, y, width, height);
    if (suppress)
        return;
    if (!explicit_unit_apply_begin(ctx, unit, &saved_unit))
        return;
    copy_tex_sub_image_2d_apply(ctx, target, level, xoffset, yoffset, x, y, width, height);
    explicit_unit_apply_end(ctx, saved_unit);
}

void glCopyMultiTexSubImage3D(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                              GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width,
                              GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glCopyMultiTexSubImage3D",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"zoffset", (uint64_t)(uintptr_t)zoffset},
                                                       {"x", (uint64_t)(uintptr_t)x},
                                                       {"y", (uint64_t)(uintptr_t)y}},
                        8u);
    unsigned unit, saved_unit;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {

        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_copy_multi_tex_sub_image_3d(ctx, texunit, target, level, xoffset,
                                                        yoffset, zoffset, x, y, width, height))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!valid_3d_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    suppress = mgl_list_record_copy_multi_tex_sub_image_3d(ctx, texunit, target, level, xoffset,
                                                           yoffset, zoffset, x, y, width, height);
    if (suppress)
        return;
    if (!explicit_unit_apply_begin(ctx, unit, &saved_unit))
        return;
    copy_tex_sub_image_3d_apply(ctx, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    explicit_unit_apply_end(ctx, saved_unit);
}

void glCopyMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat,
                              GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    glCopyMultiTexImage2D(texunit, target, level, internalformat, x, y, width, height, border);
}

void glCopyMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                                 GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    glCopyMultiTexSubImage2D(texunit, target, level, xoffset, yoffset, x, y, width, height);
}

void glCopyMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                                 GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width,
                                 GLsizei height) {
    glCopyMultiTexSubImage3D(texunit, target, level, xoffset, yoffset, zoffset, x, y, width,
                             height);
}

void glMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint internalformat,
                          GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
                          const void *pixels) {
    glMultiTexImage2D(texunit, target, level, internalformat, width, height, border, format, type,
                      pixels);
}

void glMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                             GLint yoffset, GLsizei width, GLsizei height, GLenum format,
                             GLenum type, const void *pixels) {
    glMultiTexSubImage2D(texunit, target, level, xoffset, yoffset, width, height, format, type,
                         pixels);
}

void glMultiTexImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint internalformat,
                          GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format,
                          GLenum type, const void *pixels) {
    glMultiTexImage3D(texunit, target, level, internalformat, width, height, depth, border, format,
                      type, pixels);
}

void glMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                             GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                             GLsizei depth, GLenum format, GLenum type, const void *pixels) {
    glMultiTexSubImage3D(texunit, target, level, xoffset, yoffset, zoffset, width, height, depth,
                         format, type, pixels);
}

void glCompressedMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level,
                                    GLenum internalformat, GLsizei width, GLsizei height,
                                    GLint border, GLsizei imageSize, const void *data) {
    glCompressedMultiTexImage2D(texunit, target, level, internalformat, width, height, border,
                                imageSize, data);
}

void glCompressedMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                                       GLint yoffset, GLsizei width, GLsizei height, GLenum format,
                                       GLsizei imageSize, const void *data) {
    glCompressedMultiTexSubImage2D(texunit, target, level, xoffset, yoffset, width, height, format,
                                   imageSize, data);
}

void glCompressedMultiTexImage3DEXT(GLenum texunit, GLenum target, GLint level,
                                    GLenum internalformat, GLsizei width, GLsizei height,
                                    GLsizei depth, GLint border, GLsizei imageSize,
                                    const void *data) {
    glCompressedMultiTexImage3D(texunit, target, level, internalformat, width, height, depth,
                                border, imageSize, data);
}

void glCompressedMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                                       GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                                       GLsizei depth, GLenum format, GLsizei imageSize,
                                       const void *data) {
    glCompressedMultiTexSubImage3D(texunit, target, level, xoffset, yoffset, zoffset, width, height,
                                   depth, format, imageSize, data);
}

static int multitex_query_begin(struct mgl_context *ctx, GLenum texunit, unsigned *saved_unit) {
    unsigned unit;
    if (ctx == NULL || saved_unit == NULL)
        return 0;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return 0;
    *saved_unit = ctx->compat.active_texture;
    ctx->compat.active_texture = unit;
    return 1;
}

static void multitex_query_end(struct mgl_context *ctx, unsigned saved_unit) {
    if (ctx != NULL)
        ctx->compat.active_texture = saved_unit;
}

void glGetMultiTexImage(GLenum texunit, GLenum target, GLint level, GLenum format, GLenum type,
                        void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned saved_unit;
    if (ctx == NULL)
        return;
    if (!multitex_query_begin(ctx, texunit, &saved_unit))
        return;
    glGetTexImage(target, level, format, type, pixels);
    multitex_query_end(ctx, saved_unit);
}

void glGetMultiTexLevelParameteriv(GLenum texunit, GLenum target, GLint level, GLenum pname,
                                   GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned saved_unit;
    if (ctx == NULL || params == NULL)
        return;
    if (!multitex_query_begin(ctx, texunit, &saved_unit))
        return;
    glGetTexLevelParameteriv(target, level, pname, params);
    multitex_query_end(ctx, saved_unit);
}

void glGetMultiTexLevelParameterfv(GLenum texunit, GLenum target, GLint level, GLenum pname,
                                   GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned saved_unit;
    if (ctx == NULL || params == NULL)
        return;
    if (!multitex_query_begin(ctx, texunit, &saved_unit))
        return;
    glGetTexLevelParameterfv(target, level, pname, params);
    multitex_query_end(ctx, saved_unit);
}

void glGetCompressedMultiTexImage(GLenum texunit, GLenum target, GLint level, void *img) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned saved_unit;
    if (ctx == NULL)
        return;
    if (!multitex_query_begin(ctx, texunit, &saved_unit))
        return;
    glGetCompressedTexImage(target, level, img);
    multitex_query_end(ctx, saved_unit);
}

void glGetMultiTexImageEXT(GLenum texunit, GLenum target, GLint level, GLenum format, GLenum type,
                           void *pixels) {
    glGetMultiTexImage(texunit, target, level, format, type, pixels);
}

void glGetMultiTexLevelParameterivEXT(GLenum texunit, GLenum target, GLint level, GLenum pname,
                                      GLint *params) {
    glGetMultiTexLevelParameteriv(texunit, target, level, pname, params);
}

void glGetMultiTexLevelParameterfvEXT(GLenum texunit, GLenum target, GLint level, GLenum pname,
                                      GLfloat *params) {
    glGetMultiTexLevelParameterfv(texunit, target, level, pname, params);
}

void glGetCompressedMultiTexImageEXT(GLenum texunit, GLenum target, GLint level, void *img) {
    glGetCompressedMultiTexImage(texunit, target, level, img);
}

static int texture_storage_multisample(struct mgl_context *ctx, struct mgl_texture *obj,
                                       GLenum target, GLsizei samples, GLenum internalformat,
                                       GLsizei width, GLsizei height, GLsizei depth,
                                       GLboolean fixedsamplelocations) {
    struct mgl_texture_level *level;
    int is_array = target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
    if (ctx == NULL || obj == NULL)
        return 0;
    if (target != GL_TEXTURE_2D_MULTISAMPLE && target != GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (!mgl_renderable_storage_format(ctx, internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (samples <= 0 || width <= 0 || height <= 0 || (is_array && depth <= 0)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (obj->name == 0u || obj->target != target || obj->immutable_format) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (ctx->native_dispatch == NULL ||
        (!is_array && ctx->native_dispatch->glesTexStorage2DMultisample == NULL) ||
        (is_array && ctx->native_dispatch->glesTexStorage3DMultisample == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    level = &obj->levels[0];
    if (level->defined && !mgl_texture_recreate_native(ctx, obj)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    clear_texture_image_storage(obj);
    level = &obj->levels[0];
    level->defined = GL_TRUE;
    level->width = width;
    level->height = height;
    level->depth = is_array ? depth : 1;
    level->samples = samples;
    level->internal_format = internalformat;
    level->fixed_sample_locations = fixedsamplelocations ? GL_TRUE : GL_FALSE;
    obj->immutable_format = GL_TRUE;
    obj->immutable_levels = 1;
    obj->native_immutable_storage = GL_TRUE;
    if (ctx->native_dispatch->glesBindTexture != NULL)
        mgl_texture_bind_native_unit(ctx, target, obj->native_name, ctx->compat.active_texture);
    if (is_array)
        ctx->native_dispatch->glesTexStorage3DMultisample(
            target, samples, native_storage_internalformat(ctx, internalformat), width, height,
            depth, fixedsamplelocations ? GL_TRUE : GL_FALSE);
    else
        ctx->native_dispatch->glesTexStorage2DMultisample(
            target, samples, native_storage_internalformat(ctx, internalformat), width, height,
            fixedsamplelocations ? GL_TRUE : GL_FALSE);
    return 1;
}

void glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                             GLsizei height, GLboolean fixedsamplelocations) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTexImage2DMultisample",
                        (const struct mgl_error_arg[]){
                            {"target", (uint64_t)(uintptr_t)target},
                            {"samples", (uint64_t)(uintptr_t)samples},
                            {"internalformat", (uint64_t)(uintptr_t)internalformat},
                            {"width", (uint64_t)(uintptr_t)width},
                            {"height", (uint64_t)(uintptr_t)height},
                            {"fixedsamplelocations", (uint64_t)(uintptr_t)fixedsamplelocations}},
                        6u);
    struct mgl_texture *obj;
    struct mgl_texture_level *level;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_2D_MULTISAMPLE || !mgl_renderable_storage_format(ctx, internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (samples <= 0 || width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL || !obj->realized || obj->immutable_format) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    level = &obj->levels[0];
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesTexStorage2DMultisample == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (level->defined && !mgl_texture_recreate_native(ctx, obj)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    free(level->data);
    memset(level, 0, sizeof(*level));
    level->defined = GL_TRUE;
    level->width = width;
    level->height = height;
    level->depth = 1;
    level->samples = samples;
    level->internal_format = internalformat;
    level->fixed_sample_locations = fixedsamplelocations ? GL_TRUE : GL_FALSE;
    if (width != 0 && height != 0)
        ctx->native_dispatch->glesTexStorage2DMultisample(
            target, samples, native_storage_internalformat(ctx, internalformat), width, height,
            fixedsamplelocations);
}

void glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                             GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTexImage3DMultisample",
                        (const struct mgl_error_arg[]){
                            {"target", (uint64_t)(uintptr_t)target},
                            {"samples", (uint64_t)(uintptr_t)samples},
                            {"internalformat", (uint64_t)(uintptr_t)internalformat},
                            {"width", (uint64_t)(uintptr_t)width},
                            {"height", (uint64_t)(uintptr_t)height},
                            {"depth", (uint64_t)(uintptr_t)depth},
                            {"fixedsamplelocations", (uint64_t)(uintptr_t)fixedsamplelocations}},
                        7u);
    struct mgl_texture *obj;
    struct mgl_texture_level *level;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_2D_MULTISAMPLE_ARRAY || !mgl_renderable_storage_format(ctx, internalformat)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (samples <= 0 || width < 0 || height < 0 || depth < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL || !obj->realized || obj->immutable_format) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    level = &obj->levels[0];
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesTexStorage3DMultisample == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (level->defined && !mgl_texture_recreate_native(ctx, obj)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    free(level->data);
    memset(level, 0, sizeof(*level));
    level->defined = GL_TRUE;
    level->width = width;
    level->height = height;
    level->depth = depth;
    level->samples = samples;
    level->internal_format = internalformat;
    level->fixed_sample_locations = fixedsamplelocations ? GL_TRUE : GL_FALSE;
    if (width != 0 && height != 0 && depth != 0)
        ctx->native_dispatch->glesTexStorage3DMultisample(
            target, samples, native_storage_internalformat(ctx, internalformat), width, height,
            depth, fixedsamplelocations);
}

void glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                               GLsizei height, GLboolean fixedsamplelocations) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTexStorage2DMultisample",
                        (const struct mgl_error_arg[]){
                            {"target", (uint64_t)(uintptr_t)target},
                            {"samples", (uint64_t)(uintptr_t)samples},
                            {"internalformat", (uint64_t)(uintptr_t)internalformat},
                            {"width", (uint64_t)(uintptr_t)width},
                            {"height", (uint64_t)(uintptr_t)height},
                            {"fixedsamplelocations", (uint64_t)(uintptr_t)fixedsamplelocations}},
                        6u);
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_2D_MULTISAMPLE) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)texture_storage_multisample(ctx, obj, target, samples, internalformat, width, height, 1,
                                      fixedsamplelocations);
}

void glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                               GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTexStorage3DMultisample",
                        (const struct mgl_error_arg[]){
                            {"target", (uint64_t)(uintptr_t)target},
                            {"samples", (uint64_t)(uintptr_t)samples},
                            {"internalformat", (uint64_t)(uintptr_t)internalformat},
                            {"width", (uint64_t)(uintptr_t)width},
                            {"height", (uint64_t)(uintptr_t)height},
                            {"depth", (uint64_t)(uintptr_t)depth},
                            {"fixedsamplelocations", (uint64_t)(uintptr_t)fixedsamplelocations}},
                        7u);
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_texture(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)texture_storage_multisample(ctx, obj, target, samples, internalformat, width, height,
                                      depth, fixedsamplelocations);
}

void glTextureStorage2DMultisample(GLuint texture, GLsizei samples, GLenum internalformat,
                                   GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || obj->target != GL_TEXTURE_2D_MULTISAMPLE) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    dsa_native_bind_texture(ctx, obj);
    (void)texture_storage_multisample(ctx, obj, obj->target, samples, internalformat, width, height,
                                      1, fixedsamplelocations);
    dsa_native_restore_texture(ctx, obj->target);
}

void glTextureStorage3DMultisample(GLuint texture, GLsizei samples, GLenum internalformat,
                                   GLsizei width, GLsizei height, GLsizei depth,
                                   GLboolean fixedsamplelocations) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || obj->target != GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    dsa_native_bind_texture(ctx, obj);
    (void)texture_storage_multisample(ctx, obj, obj->target, samples, internalformat, width, height,
                                      depth, fixedsamplelocations);
    dsa_native_restore_texture(ctx, obj->target);
}

void glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glFramebufferTexture",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"attachment", (uint64_t)(uintptr_t)attachment},
                                       {"texture", (uint64_t)(uintptr_t)texture},
                                       {"level", (uint64_t)(uintptr_t)level}},
        4u);
    struct mgl_framebuffer *fb;
    struct mgl_texture *tex = NULL;
    GLenum tex_target = 0u;
    GLuint native_texture = 0u;
    GLint layer = -1;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER &&
        target != GL_READ_FRAMEBUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!((attachment >= GL_COLOR_ATTACHMENT0 &&
           attachment < GL_COLOR_ATTACHMENT0 + MGL_MAX_DRAW_BUFFERS) ||
          attachment == GL_DEPTH_ATTACHMENT || attachment == GL_STENCIL_ATTACHMENT ||
          attachment == GL_DEPTH_STENCIL_ATTACHMENT)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    fb = mgl_bound_framebuffer(ctx, target);
    if (fb == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture != 0u) {
        tex = mgl_texture_find(ctx, texture);
        if (tex == NULL || !tex->realized || tex->target == 0u) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        tex_target = tex->target;
        if (tex_target == GL_TEXTURE_BUFFER) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if ((tex_target == GL_TEXTURE_2D_MULTISAMPLE ||
             tex_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) &&
            level != 0) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        native_texture = tex->native_name;
    }
    if (tex_target != GL_TEXTURE_1D && tex_target != GL_TEXTURE_2D &&
        tex_target != GL_TEXTURE_2D_MULTISAMPLE && texture != 0u) {
        if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesFramebufferTexture == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        layer = -2;
    } else if (ctx->native_dispatch == NULL ||
               (ctx->native_dispatch->glesFramebufferTexture2D == NULL &&
                ctx->native_dispatch->glesFramebufferTexture == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (attachment >= GL_COLOR_ATTACHMENT0 &&
        attachment < GL_COLOR_ATTACHMENT0 + MGL_MAX_DRAW_BUFFERS) {
        unsigned idx = (unsigned)(attachment - GL_COLOR_ATTACHMENT0);
        if (idx == 0u) {
            fb->color_texture = texture;
            fb->color_texture_level = level;
            fb->color_texture_layer = layer;
            fb->color_texture_target = tex_target;
            fb->color_renderbuffer = 0u;
        } else {
            fb->extra_color_textures[idx] = texture;
            fb->extra_color_levels[idx] = level;
            fb->extra_color_layers[idx] = layer;
            fb->extra_color_targets[idx] = tex_target;
            fb->extra_color_renderbuffers[idx] = 0u;
        }
        if (tex != NULL)
            tex->used_as_color_attachment = GL_TRUE;
    } else {
        if (attachment == GL_DEPTH_ATTACHMENT || attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
            fb->depth_texture = texture;
            fb->depth_texture_level = level;
            fb->depth_texture_layer = layer;
            fb->depth_texture_target = tex_target;
            fb->depth_renderbuffer = 0u;
        }
        if (attachment == GL_STENCIL_ATTACHMENT || attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
            fb->stencil_texture = texture;
            fb->stencil_texture_level = level;
            fb->stencil_texture_layer = layer;
            fb->stencil_texture_target = tex_target;
            fb->stencil_renderbuffer = 0u;
        }
    }
    if (layer == -2 || ctx->native_dispatch->glesFramebufferTexture2D == NULL)
        ctx->native_dispatch->glesFramebufferTexture(target, attachment, native_texture, level);
    else
        ctx->native_dispatch->glesFramebufferTexture2D(
            target, attachment,
            tex_target == GL_TEXTURE_2D_MULTISAMPLE ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D,
            native_texture, level);
}

void glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                            GLint level) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glFramebufferTexture1D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"attachment", (uint64_t)(uintptr_t)attachment},
                                       {"textarget", (uint64_t)(uintptr_t)textarget},
                                       {"texture", (uint64_t)(uintptr_t)texture},
                                       {"level", (uint64_t)(uintptr_t)level}},
        5u);
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (textarget != GL_TEXTURE_1D) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                            GLint level) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glFramebufferTexture2D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"attachment", (uint64_t)(uintptr_t)attachment},
                                       {"textarget", (uint64_t)(uintptr_t)textarget},
                                       {"texture", (uint64_t)(uintptr_t)texture},
                                       {"level", (uint64_t)(uintptr_t)level}},
        5u);
    struct mgl_framebuffer *fb;
    struct mgl_texture *tex = NULL;
    GLuint native_texture = 0u;
    GLenum expected_target;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER &&
        target != GL_READ_FRAMEBUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (textarget != GL_TEXTURE_1D && textarget != GL_TEXTURE_2D &&
        textarget != GL_TEXTURE_RECTANGLE && textarget != GL_TEXTURE_2D_MULTISAMPLE &&
        cube_face_index(textarget) < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 ||
        ((textarget == GL_TEXTURE_RECTANGLE || textarget == GL_TEXTURE_2D_MULTISAMPLE) &&
         level != 0)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!((attachment >= GL_COLOR_ATTACHMENT0 &&
           attachment < GL_COLOR_ATTACHMENT0 + MGL_MAX_DRAW_BUFFERS) ||
          attachment == GL_DEPTH_ATTACHMENT || attachment == GL_STENCIL_ATTACHMENT ||
          attachment == GL_DEPTH_STENCIL_ATTACHMENT)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    fb = mgl_bound_framebuffer(ctx, target);
    if (fb == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    expected_target = cube_face_index(textarget) >= 0 ? GL_TEXTURE_CUBE_MAP : textarget;
    if (texture != 0u) {
        tex = mgl_texture_find(ctx, texture);
        if (tex == NULL || !tex->realized || tex->target != expected_target) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        native_texture = tex->native_name;
    }
    if (attachment == GL_COLOR_ATTACHMENT0) {
        fb->color_texture = texture;
        fb->color_texture_level = level;
        fb->color_texture_layer = -1;
        fb->color_texture_target = texture != 0u ? textarget : 0u;
        fb->color_renderbuffer = 0u;
    } else if (attachment > GL_COLOR_ATTACHMENT0 &&
               attachment < GL_COLOR_ATTACHMENT0 + MGL_MAX_DRAW_BUFFERS) {
        unsigned idx = (unsigned)(attachment - GL_COLOR_ATTACHMENT0);
        fb->extra_color_textures[idx] = texture;
        fb->extra_color_levels[idx] = level;
        fb->extra_color_layers[idx] = -1;
        fb->extra_color_targets[idx] = texture != 0u ? textarget : 0u;
        fb->extra_color_renderbuffers[idx] = 0u;
    } else if (attachment == GL_DEPTH_ATTACHMENT) {
        fb->depth_texture = texture;
        fb->depth_texture_level = level;
        fb->depth_texture_layer = -1;
        fb->depth_texture_target = texture != 0u ? textarget : 0u;
        fb->depth_renderbuffer = 0u;
    } else if (attachment == GL_STENCIL_ATTACHMENT) {
        fb->stencil_texture = texture;
        fb->stencil_texture_level = level;
        fb->stencil_texture_layer = -1;
        fb->stencil_texture_target = texture != 0u ? textarget : 0u;
        fb->stencil_renderbuffer = 0u;
    } else {
        fb->depth_texture = texture;
        fb->depth_texture_level = level;
        fb->depth_texture_layer = -1;
        fb->depth_texture_target = texture != 0u ? textarget : 0u;
        fb->stencil_texture = texture;
        fb->stencil_texture_level = level;
        fb->stencil_texture_layer = -1;
        fb->stencil_texture_target = texture != 0u ? textarget : 0u;
        fb->depth_renderbuffer = 0u;
        fb->stencil_renderbuffer = 0u;
    }
    if (tex != NULL && attachment >= GL_COLOR_ATTACHMENT0 &&
        attachment < GL_COLOR_ATTACHMENT0 + MGL_MAX_DRAW_BUFFERS)
        tex->used_as_color_attachment = GL_TRUE;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesFramebufferTexture2D != NULL)
        ctx->native_dispatch->glesFramebufferTexture2D(
            target, attachment, mgl_texture_native_target(textarget), native_texture, level);
}

void glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                            GLint level, GLint zoffset) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glFramebufferTexture3D",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"attachment", (uint64_t)(uintptr_t)attachment},
                                       {"textarget", (uint64_t)(uintptr_t)textarget},
                                       {"texture", (uint64_t)(uintptr_t)texture},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"zoffset", (uint64_t)(uintptr_t)zoffset}},
        6u);
    struct mgl_framebuffer *fb;
    struct mgl_texture *tex = NULL;
    GLuint native_texture = 0u;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER &&
        target != GL_READ_FRAMEBUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!((attachment >= GL_COLOR_ATTACHMENT0 &&
           attachment < GL_COLOR_ATTACHMENT0 + MGL_MAX_DRAW_BUFFERS) ||
          attachment == GL_DEPTH_ATTACHMENT || attachment == GL_STENCIL_ATTACHMENT ||
          attachment == GL_DEPTH_STENCIL_ATTACHMENT)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    fb = mgl_bound_framebuffer(ctx, target);
    if (fb == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    /* A detach updates canonical attachment state even when the native has
     * no layered-attachment callback. A new 3D attachment still requires it. */
    if (texture != 0u && (ctx->native_dispatch == NULL ||
                          ctx->native_dispatch->glesFramebufferTextureLayer == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture != 0u) {
        if (textarget != GL_TEXTURE_3D) {
            mgl_error_set(&ctx->errors,
                          valid_level_target(textarget) ? GL_INVALID_OPERATION : GL_INVALID_ENUM);
            return;
        }
        tex = mgl_texture_find(ctx, texture);
        if (tex == NULL || !tex->realized) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        if (tex->target != GL_TEXTURE_3D) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || zoffset < 0) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        native_texture = tex->native_name;
    } else {

        level = 0;
        zoffset = 0;
    }
    if (attachment == GL_COLOR_ATTACHMENT0) {
        fb->color_texture = texture;
        fb->color_texture_level = level;
        fb->color_texture_layer = texture != 0u ? zoffset : -1;
        fb->color_texture_target = texture != 0u ? GL_TEXTURE_3D : 0u;
        fb->color_renderbuffer = 0u;
    } else if (attachment > GL_COLOR_ATTACHMENT0 &&
               attachment < GL_COLOR_ATTACHMENT0 + MGL_MAX_DRAW_BUFFERS) {
        unsigned idx = (unsigned)(attachment - GL_COLOR_ATTACHMENT0);
        fb->extra_color_textures[idx] = texture;
        fb->extra_color_levels[idx] = level;
        fb->extra_color_layers[idx] = texture != 0u ? zoffset : -1;
        fb->extra_color_targets[idx] = texture != 0u ? GL_TEXTURE_3D : 0u;
        fb->extra_color_renderbuffers[idx] = 0u;
    } else if (attachment == GL_DEPTH_ATTACHMENT) {
        fb->depth_texture = texture;
        fb->depth_texture_level = level;
        fb->depth_texture_layer = texture != 0u ? zoffset : -1;
        fb->depth_texture_target = texture != 0u ? GL_TEXTURE_3D : 0u;
        fb->depth_renderbuffer = 0u;
    } else if (attachment == GL_STENCIL_ATTACHMENT) {
        fb->stencil_texture = texture;
        fb->stencil_texture_level = level;
        fb->stencil_texture_layer = texture != 0u ? zoffset : -1;
        fb->stencil_texture_target = texture != 0u ? GL_TEXTURE_3D : 0u;
        fb->stencil_renderbuffer = 0u;
    } else {
        fb->depth_texture = texture;
        fb->depth_texture_level = level;
        fb->depth_texture_layer = texture != 0u ? zoffset : -1;
        fb->depth_texture_target = texture != 0u ? GL_TEXTURE_3D : 0u;
        fb->stencil_texture = texture;
        fb->stencil_texture_level = level;
        fb->stencil_texture_layer = texture != 0u ? zoffset : -1;
        fb->stencil_texture_target = texture != 0u ? GL_TEXTURE_3D : 0u;
        fb->depth_renderbuffer = 0u;
        fb->stencil_renderbuffer = 0u;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesFramebufferTextureLayer != NULL)
        ctx->native_dispatch->glesFramebufferTextureLayer(target, attachment, native_texture, level,
                                                          zoffset);
}

void glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level,
                               GLint layer) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glFramebufferTextureLayer",
        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                       {"attachment", (uint64_t)(uintptr_t)attachment},
                                       {"texture", (uint64_t)(uintptr_t)texture},
                                       {"level", (uint64_t)(uintptr_t)level},
                                       {"layer", (uint64_t)(uintptr_t)layer}},
        5u);
    struct mgl_framebuffer *fb;
    struct mgl_texture *tex = NULL;
    GLuint native_texture = 0u;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER &&
        target != GL_READ_FRAMEBUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!((attachment >= GL_COLOR_ATTACHMENT0 &&
           attachment < GL_COLOR_ATTACHMENT0 + MGL_MAX_DRAW_BUFFERS) ||
          attachment == GL_DEPTH_ATTACHMENT || attachment == GL_STENCIL_ATTACHMENT ||
          attachment == GL_DEPTH_STENCIL_ATTACHMENT)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || layer < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    fb = mgl_bound_framebuffer(ctx, target);
    if (fb == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture != 0u) {
        tex = mgl_texture_find(ctx, texture);
        if (tex == NULL || !tex->realized ||
            (tex->target != GL_TEXTURE_3D && tex->target != GL_TEXTURE_2D_ARRAY &&
             tex->target != GL_TEXTURE_CUBE_MAP_ARRAY &&
             tex->target != GL_TEXTURE_2D_MULTISAMPLE_ARRAY)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (level >= (GLint)MGL_MAX_TEXTURE_LEVELS ||
            (tex->levels[level].defined && layer >= tex->levels[level].depth)) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        if (ctx->native_dispatch == NULL ||
            ctx->native_dispatch->glesFramebufferTextureLayer == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        native_texture = tex->native_name;
    }
    if (attachment == GL_COLOR_ATTACHMENT0) {
        fb->color_texture = texture;
        fb->color_texture_level = level;
        fb->color_texture_layer = texture != 0u ? layer : -1;
        fb->color_texture_target = texture != 0u ? tex->target : 0u;
        fb->color_renderbuffer = 0u;
    } else if (attachment > GL_COLOR_ATTACHMENT0 &&
               attachment < GL_COLOR_ATTACHMENT0 + MGL_MAX_DRAW_BUFFERS) {
        unsigned idx = (unsigned)(attachment - GL_COLOR_ATTACHMENT0);
        fb->extra_color_textures[idx] = texture;
        fb->extra_color_levels[idx] = level;
        fb->extra_color_layers[idx] = texture != 0u ? layer : -1;
        fb->extra_color_targets[idx] = texture != 0u ? tex->target : 0u;
        fb->extra_color_renderbuffers[idx] = 0u;
    } else if (attachment == GL_DEPTH_ATTACHMENT) {
        fb->depth_texture = texture;
        fb->depth_texture_level = level;
        fb->depth_texture_layer = texture != 0u ? layer : -1;
        fb->depth_texture_target = texture != 0u ? tex->target : 0u;
        fb->depth_renderbuffer = 0u;
    } else if (attachment == GL_STENCIL_ATTACHMENT) {
        fb->stencil_texture = texture;
        fb->stencil_texture_level = level;
        fb->stencil_texture_layer = texture != 0u ? layer : -1;
        fb->stencil_texture_target = texture != 0u ? tex->target : 0u;
        fb->stencil_renderbuffer = 0u;
    } else {
        fb->depth_texture = texture;
        fb->depth_texture_level = level;
        fb->depth_texture_layer = texture != 0u ? layer : -1;
        fb->depth_texture_target = texture != 0u ? tex->target : 0u;
        fb->stencil_texture = texture;
        fb->stencil_texture_level = level;
        fb->stencil_texture_layer = texture != 0u ? layer : -1;
        fb->stencil_texture_target = texture != 0u ? tex->target : 0u;
        fb->depth_renderbuffer = 0u;
        fb->stencil_renderbuffer = 0u;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesFramebufferTextureLayer != NULL)
        ctx->native_dispatch->glesFramebufferTextureLayer(target, attachment, native_texture, level,
                                                          layer);
}

static int named_framebuffer_attach_begin(struct mgl_context *ctx, GLuint framebuffer,
                                          GLuint *previous) {
    struct mgl_framebuffer *fb = mgl_framebuffer_find(ctx, framebuffer);
    if (fb == NULL || !fb->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesBindFramebuffer == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    *previous = ctx->draw_framebuffer;
    ctx->native_dispatch->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb->native_name);
    ctx->draw_framebuffer = framebuffer;
    return 1;
}

static void named_framebuffer_attach_end(struct mgl_context *ctx, GLuint previous) {
    struct mgl_framebuffer *fb = mgl_framebuffer_find(ctx, previous);
    ctx->draw_framebuffer = previous;
    ctx->native_dispatch->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                              fb != NULL ? fb->native_name : 0u);
}

void glNamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint previous;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!named_framebuffer_attach_begin(ctx, framebuffer, &previous))
        return;
    glFramebufferTexture(GL_DRAW_FRAMEBUFFER, attachment, texture, level);
    named_framebuffer_attach_end(ctx, previous);
}

void glNamedFramebufferTextureLayer(GLuint framebuffer, GLenum attachment, GLuint texture,
                                    GLint level, GLint layer) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint previous;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!named_framebuffer_attach_begin(ctx, framebuffer, &previous))
        return;
    glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, attachment, texture, level, layer);
    named_framebuffer_attach_end(ctx, previous);
}

static struct mgl_texture *dsa_texture_checked(struct mgl_context *ctx, GLuint texture) {
    struct mgl_texture *obj;
    if (ctx == NULL || texture == 0u)
        return NULL;
    obj = mgl_texture_find(ctx, texture);
    if (obj == NULL || !obj->realized || obj->target == 0u)
        return NULL;
    return obj;
}

static GLuint native_texture_for_name(struct mgl_context *ctx, GLuint name) {
    struct mgl_texture *obj = mgl_texture_find(ctx, name);
    return obj != NULL && obj->realized ? obj->native_name : 0u;
}

static void dsa_native_bind_texture(struct mgl_context *ctx, const struct mgl_texture *obj) {
    if (ctx == NULL || obj == NULL || ctx->native_dispatch == NULL)
        return;
    if (ctx->native_dispatch->glesActiveTexture != NULL)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + ctx->compat.active_texture);
    if (ctx->native_dispatch->glesBindTexture != NULL &&
        (obj->target != GL_TEXTURE_BUFFER || native_texture_buffer_supported(ctx)))
        mgl_texture_bind_native_unit(ctx, mgl_texture_native_target(obj->target), obj->native_name,
                                     ctx->compat.active_texture);
}

static void dsa_native_restore_texture(struct mgl_context *ctx, GLenum target) {
    GLuint name = 0u;
    if (ctx == NULL || ctx->native_dispatch == NULL)
        return;
    if (target == GL_TEXTURE_1D || target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE) {
        unsigned unit = ctx->compat.active_texture;
        if (ctx->texture_2d_prefer_1d[unit] && ctx->texture_1d_binding[unit] != 0u)
            name = ctx->texture_1d_binding[unit];
        else if (ctx->texture_2d_prefer_rectangle[unit] &&
                 ctx->texture_rectangle_binding[unit] != 0u)
            name = ctx->texture_rectangle_binding[unit];
        else
            name = ctx->texture_2d_binding[unit];
    } else if (target == GL_TEXTURE_3D)
        name = ctx->texture_3d_binding[ctx->compat.active_texture];
    else if (target == GL_TEXTURE_2D_ARRAY)
        name = ctx->texture_2d_array_binding[ctx->compat.active_texture];
    else if (target == GL_TEXTURE_CUBE_MAP)
        name = ctx->texture_cube_binding[ctx->compat.active_texture];
    else if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
        name = ctx->texture_cube_array_binding[ctx->compat.active_texture];
    else if (target == GL_TEXTURE_BUFFER)
        name = ctx->texture_buffer_binding[ctx->compat.active_texture];
    else if (target == GL_TEXTURE_2D_MULTISAMPLE)
        name = ctx->texture_2d_multisample_binding[ctx->compat.active_texture];
    else if (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        name = ctx->texture_2d_multisample_array_binding[ctx->compat.active_texture];
    if (ctx->native_dispatch->glesActiveTexture != NULL)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + ctx->compat.active_texture);
    if (ctx->native_dispatch->glesBindTexture != NULL &&
        (target != GL_TEXTURE_BUFFER || native_texture_buffer_supported(ctx)))
        mgl_texture_bind_native_unit(ctx, mgl_texture_native_target(target),
                                     native_texture_for_name(ctx, name),
                                     ctx->compat.active_texture);
}

void glCreateTextures(GLenum target, GLsizei n, GLuint *textures) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glCreateTextures",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"n", (uint64_t)(uintptr_t)n},
                                                       {"textures", (uint64_t)(uintptr_t)textures}},
                        3u);
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_create_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0 || textures == NULL)
        return;
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGenTextures(n, textures);
    if (ctx->errors.sequence != error_sequence_before)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < n; ++i) {
        struct mgl_texture *obj = mgl_texture_ensure(ctx, textures[i], target, 1);
        if (obj == NULL) {
            textures[i] = 0u;
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glBindTextureUnit(GLuint unit, GLuint texture) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj = NULL;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (unit >= mgl_programmable_texture_limit(ctx)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (texture != 0u) {
        obj = dsa_texture_checked(ctx, texture);
        if (obj == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (!valid_bind_target(obj->target)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
    }
    if (obj == NULL) {
        ctx->texture_1d_binding[unit] = 0u;
        ctx->texture_2d_binding[unit] = 0u;
        ctx->texture_rectangle_binding[unit] = 0u;
        ctx->texture_2d_prefer_rectangle[unit] = GL_FALSE;
        ctx->texture_2d_prefer_1d[unit] = GL_FALSE;
        ctx->texture_3d_binding[unit] = 0u;
        ctx->texture_2d_array_binding[unit] = 0u;
        ctx->texture_cube_binding[unit] = 0u;
        ctx->texture_cube_array_binding[unit] = 0u;
        ctx->texture_2d_multisample_binding[unit] = 0u;
        ctx->texture_2d_multisample_array_binding[unit] = 0u;
        ctx->texture_buffer_binding[unit] = 0u;
    } else if (obj->target == GL_TEXTURE_1D) {
        ctx->texture_1d_binding[unit] = texture;
        ctx->texture_2d_prefer_1d[unit] = GL_TRUE;
        ctx->texture_2d_prefer_rectangle[unit] = GL_FALSE;
    } else if (obj->target == GL_TEXTURE_2D) {
        ctx->texture_2d_binding[unit] = texture;
        ctx->texture_2d_prefer_1d[unit] = GL_FALSE;
        ctx->texture_2d_prefer_rectangle[unit] = GL_FALSE;
    } else if (obj->target == GL_TEXTURE_RECTANGLE) {
        ctx->texture_rectangle_binding[unit] = texture;
        ctx->texture_2d_prefer_rectangle[unit] = GL_TRUE;
        ctx->texture_2d_prefer_1d[unit] = GL_FALSE;
    } else if (obj->target == GL_TEXTURE_3D)
        ctx->texture_3d_binding[unit] = texture;
    else if (obj->target == GL_TEXTURE_2D_ARRAY)
        ctx->texture_2d_array_binding[unit] = texture;
    else if (obj->target == GL_TEXTURE_BUFFER)
        ctx->texture_buffer_binding[unit] = texture;
    else if (obj->target == GL_TEXTURE_CUBE_MAP)
        ctx->texture_cube_binding[unit] = texture;
    else if (obj->target == GL_TEXTURE_CUBE_MAP_ARRAY)
        ctx->texture_cube_array_binding[unit] = texture;
    else if (obj->target == GL_TEXTURE_2D_MULTISAMPLE)
        ctx->texture_2d_multisample_binding[unit] = texture;
    else
        ctx->texture_2d_multisample_array_binding[unit] = texture;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesActiveTexture != NULL &&
        ctx->native_dispatch->glesBindTexture != NULL) {
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + unit);
        if (obj != NULL) {
            if (obj->target != GL_TEXTURE_BUFFER || native_texture_buffer_supported(ctx))
                mgl_texture_bind_native_unit(ctx, mgl_texture_native_target(obj->target),
                                             obj->native_name, unit);
            if (obj->target != GL_TEXTURE_BUFFER && obj->target != GL_TEXTURE_2D_MULTISAMPLE &&
                obj->target != GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
                mgl_sampler_realize_unit(ctx, unit, obj);
        } else {
            mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D, 0u, unit);
            if (ctx->native_dispatch->glesTexImage3D != NULL)
                mgl_texture_bind_native_unit(ctx, GL_TEXTURE_3D, 0u, unit);
            if (ctx->native_dispatch->glesTexImage3D != NULL)
                mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D_ARRAY, 0u, unit);
            mgl_texture_bind_native_unit(ctx, GL_TEXTURE_CUBE_MAP, 0u, unit);
            mgl_texture_bind_native_unit(ctx, GL_TEXTURE_CUBE_MAP_ARRAY, 0u, unit);
            if (native_texture_buffer_supported(ctx))
                mgl_texture_bind_native_unit(ctx, GL_TEXTURE_BUFFER, 0u, unit);
            if (ctx->native_dispatch->glesTexStorage2DMultisample != NULL)
                mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D_MULTISAMPLE, 0u, unit);
            if (ctx->native_dispatch->glesTexStorage3DMultisample != NULL)
                mgl_texture_bind_native_unit(ctx, GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 0u, unit);
        }
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + ctx->compat.active_texture);
    }
}

void glBindTextures(GLuint first, GLsizei count, const GLuint *textures) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint limit;
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    limit = mgl_programmable_texture_limit(ctx);
    if (first > limit || (GLuint)count > limit - first) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    for (i = 0; i < count; ++i)
        glBindTextureUnit(first + (GLuint)i, textures != NULL ? textures[i] : 0u);
}

static void ext_dsa_texture_parameter_apply_i(struct mgl_context *ctx, struct mgl_texture *obj,
                                              GLenum pname, GLint param) {
    GLint native_param;
    if (ctx == NULL || obj == NULL)
        return;
    if (pname == GL_TEXTURE_PRIORITY) {
        obj->priority = normalized_texture_int(param);
        return;
    }
    apply_texture_param(obj, pname, param);
    if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT)
        obj->max_anisotropy = texture_anisotropy_clamp(ctx, (GLfloat)param);
    if (pname == GL_DEPTH_TEXTURE_MODE || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesTexParameteri == NULL)
        return;
    native_param = param;
    if (pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_R)
        native_param = (GLint)native_wrap_mode(param);
    dsa_native_bind_texture(ctx, obj);
    if ((pname == GL_TEXTURE_BASE_LEVEL || pname == GL_TEXTURE_MAX_LEVEL) &&
        obj->immutable_format && !obj->native_immutable_storage)
        native_realize_immutable_level_range(ctx, obj, obj->target);
    else
        ctx->native_dispatch->glesTexParameteri(
            obj->target == GL_TEXTURE_1D ? GL_TEXTURE_2D : obj->target, pname, native_param);
    dsa_native_restore_texture(ctx, obj->target);
}

static void ext_dsa_texture_parameter_apply_f(struct mgl_context *ctx, struct mgl_texture *obj,
                                              GLenum pname, GLfloat param) {
    if (ctx == NULL || obj == NULL)
        return;
    apply_texture_float_param(obj, pname, param);
    if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT)
        obj->max_anisotropy = texture_anisotropy_clamp(ctx, param);
    if (pname == GL_TEXTURE_PRIORITY || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesTexParameterf == NULL)
        return;
    dsa_native_bind_texture(ctx, obj);
    ctx->native_dispatch->glesTexParameterf(obj->target, pname, param);
    dsa_native_restore_texture(ctx, obj->target);
}

static void ext_dsa_texture_border_apply_f(struct mgl_context *ctx, struct mgl_texture *obj,
                                           const GLfloat *params) {
    if (ctx == NULL || obj == NULL || params == NULL)
        return;
    apply_texture_border_color(obj, params);
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesTexParameterfv == NULL)
        return;
    dsa_native_bind_texture(ctx, obj);
    ctx->native_dispatch->glesTexParameterfv(mgl_texture_native_target(obj->target),
                                             GL_TEXTURE_BORDER_COLOR, obj->border_color);
    dsa_native_restore_texture(ctx, obj->target);
}

static void ext_dsa_texture_border_apply_i(struct mgl_context *ctx, struct mgl_texture *obj,
                                           const GLint *params) {
    if (ctx == NULL || obj == NULL || params == NULL)
        return;
    apply_texture_border_color_i(obj, params);
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesTexParameterIiv == NULL)
        return;
    dsa_native_bind_texture(ctx, obj);
    ctx->native_dispatch->glesTexParameterIiv(mgl_texture_native_target(obj->target),
                                              GL_TEXTURE_BORDER_COLOR, params);
    dsa_native_restore_texture(ctx, obj->target);
}

static void ext_dsa_texture_border_apply_ui(struct mgl_context *ctx, struct mgl_texture *obj,
                                            const GLuint *params) {
    if (ctx == NULL || obj == NULL || params == NULL)
        return;
    apply_texture_border_color_ui(obj, params);
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesTexParameterIuiv == NULL)
        return;
    dsa_native_bind_texture(ctx, obj);
    ctx->native_dispatch->glesTexParameterIuiv(mgl_texture_native_target(obj->target),
                                               GL_TEXTURE_BORDER_COLOR, params);
    dsa_native_restore_texture(ctx, obj->target);
}

void glTextureParameteriEXT(GLuint texture, GLenum target, GLenum pname, GLint param) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTextureParameteriEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"param", (uint64_t)(uintptr_t)param}},
                        4u);
    struct mgl_texture *obj;
    int result;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!texture_anisotropy_validate(ctx, pname, (GLfloat)param))
        return;
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    if ((target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) &&
        (multisample_sampler_parameter(pname) || (pname == GL_TEXTURE_BASE_LEVEL && param != 0))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_PRIORITY) {
        result = validate_texture_param(pname, param);
        if (result < 0) {
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
        }
        if (!result) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
    }
    if (mgl_list_record_texture_parameter_ext(ctx, texture, target, pname, param))
        return;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return;
    ext_dsa_texture_parameter_apply_i(ctx, obj, pname, param);
}

void glTextureParameterfEXT(GLuint texture, GLenum target, GLenum pname, GLfloat param) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTextureParameterfEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname}},
                        3u);
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!texture_anisotropy_validate(ctx, pname, (GLfloat)param))
        return;
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    if ((target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) &&
        multisample_sampler_parameter(pname)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!validate_texture_float_param(pname)) {
        glTextureParameteriEXT(texture, target, pname, (GLint)param);
        return;
    }
    if (mgl_list_record_texture_parameter_ext_f(ctx, texture, target, pname, param))
        return;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return;
    ext_dsa_texture_parameter_apply_f(ctx, obj, pname, param);
}

void glTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTextureParameterivEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    GLfloat converted[4];
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glTextureParameteriEXT(texture, target, pname, params[0]);
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    if (target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_texture_parameter_ext_iv4(ctx, texture, target, pname, params))
        return;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return;
    texture_border_color_from_ints(converted, params);
    ext_dsa_texture_border_apply_f(ctx, obj, converted);
}

void glTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTextureParameterfvEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glTextureParameterfEXT(texture, target, pname, params[0]);
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    if (target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_texture_parameter_ext_fv4(ctx, texture, target, pname, params))
        return;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return;
    ext_dsa_texture_border_apply_f(ctx, obj, params);
}

void glTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTextureParameterIivEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glTextureParameterivEXT(texture, target, pname, params);
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    if (target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_texture_parameter_ext_iiv(ctx, texture, target, pname, params))
        return;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return;
    ext_dsa_texture_border_apply_i(ctx, obj, params);
}

void glTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname, const GLuint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTextureParameterIuivEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glTextureParameteriEXT(texture, target, pname, (GLint)params[0]);
        return;
    }
    if (!valid_parameter_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!ext_dsa_existing_texture_target_ok(ctx, texture, target))
        return;
    if (target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_texture_parameter_ext_iuiv(ctx, texture, target, pname, params))
        return;
    obj = ext_dsa_texture_resolve(ctx, texture, target);
    if (obj == NULL)
        return;
    ext_dsa_texture_border_apply_ui(ctx, obj, params);
}

void glTextureParameteri(GLuint texture, GLenum pname, GLint param) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTextureParameteri",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"param", (uint64_t)(uintptr_t)param}},
                        3u);
    struct mgl_texture *obj;
    int result;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!texture_anisotropy_validate(ctx, pname, (GLfloat)param))
        return;
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture_is_multisample(obj) &&
        (multisample_sampler_parameter(pname) || (pname == GL_TEXTURE_BASE_LEVEL && param != 0))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname == GL_TEXTURE_PRIORITY) {
        obj->priority = normalized_texture_int(param);
        return;
    }
    result = validate_texture_param(pname, param);
    if (result < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!result) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    apply_texture_param(obj, pname, param);
    if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT)
        obj->max_anisotropy = texture_anisotropy_clamp(ctx, (GLfloat)param);
    if (pname != GL_DEPTH_TEXTURE_MODE && pname != GL_TEXTURE_LOD_BIAS &&
        ctx->native_dispatch != NULL && ctx->native_dispatch->glesTexParameteri != NULL) {
        GLint native_param = param;
        if (pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_R)
            native_param = (GLint)native_wrap_mode(param);
        dsa_native_bind_texture(ctx, obj);
        if ((pname == GL_TEXTURE_BASE_LEVEL || pname == GL_TEXTURE_MAX_LEVEL) &&
            obj->immutable_format && !obj->native_immutable_storage)
            native_realize_immutable_level_range(ctx, obj, obj->target);
        else
            ctx->native_dispatch->glesTexParameteri(obj->target, pname, native_param);
        dsa_native_restore_texture(ctx, obj->target);
    }
}

void glTextureParameterf(GLuint texture, GLenum pname, GLfloat param) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!texture_anisotropy_validate(ctx, pname, (GLfloat)param))
        return;
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture_is_multisample(obj) && multisample_sampler_parameter(pname)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }

    if (!validate_texture_float_param(pname)) {
        glTextureParameteri(texture, pname, (GLint)param);
        return;
    }
    apply_texture_float_param(obj, pname, param);
    if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT)
        obj->max_anisotropy = texture_anisotropy_clamp(ctx, param);
    if (pname != GL_TEXTURE_PRIORITY && pname != GL_TEXTURE_LOD_BIAS &&
        ctx->native_dispatch != NULL && ctx->native_dispatch->glesTexParameterf != NULL) {
        dsa_native_bind_texture(ctx, obj);
        ctx->native_dispatch->glesTexParameterf(obj->target, pname, param);
        dsa_native_restore_texture(ctx, obj->target);
    }
}

void glTextureParameteriv(GLuint texture, GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    GLfloat converted[4];
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    if (pname == GL_TEXTURE_SWIZZLE_RGBA) {
        unsigned i;
        for (i = 0u; i < 4u; ++i) {
            if (!(params[i] == GL_RED || params[i] == GL_GREEN || params[i] == GL_BLUE ||
                  params[i] == GL_ALPHA || params[i] == GL_ZERO || params[i] == GL_ONE)) {
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return;
            }
        }
        obj = dsa_texture_checked(ctx, texture);
        if (obj == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        for (i = 0u; i < 4u; ++i)
            glTextureParameteri(texture, GL_TEXTURE_SWIZZLE_R + i, params[i]);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glTextureParameteri(texture, pname, params[0]);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture_is_multisample(obj)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    texture_border_color_from_ints(converted, params);
    ext_dsa_texture_border_apply_f(ctx, obj, converted);
}

void glTextureParameterfv(GLuint texture, GLenum pname, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    if (pname == GL_TEXTURE_SWIZZLE_RGBA) {
        GLint values[4];
        unsigned i;
        for (i = 0u; i < 4u; ++i)
            values[i] = (GLint)params[i];
        glTextureParameteriv(texture, pname, values);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glTextureParameterf(texture, pname, params[0]);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture_is_multisample(obj)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ext_dsa_texture_border_apply_f(ctx, obj, params);
}

void glTextureParameterIiv(GLuint texture, GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glTextureParameteriv(texture, pname, params);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture_is_multisample(obj)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ext_dsa_texture_border_apply_i(ctx, obj, params);
}

void glTextureParameterIuiv(GLuint texture, GLenum pname, const GLuint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    if (pname == GL_TEXTURE_SWIZZLE_RGBA) {
        GLint values[4];
        unsigned i;
        for (i = 0u; i < 4u; ++i)
            values[i] = (GLint)params[i];
        glTextureParameteriv(texture, pname, values);
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glTextureParameteri(texture, pname, (GLint)params[0]);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture_is_multisample(obj)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ext_dsa_texture_border_apply_ui(ctx, obj, params);
}

void glGetTextureParameteriv(GLuint texture, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTextureParameteriv",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
            *params = obj->min_filter;
            break;
        case GL_TEXTURE_MAG_FILTER:
            *params = obj->mag_filter;
            break;
        case GL_TEXTURE_WRAP_S:
            *params = obj->wrap_s;
            break;
        case GL_TEXTURE_WRAP_T:
            *params = obj->wrap_t;
            break;
        case GL_TEXTURE_WRAP_R:
            *params = obj->wrap_r;
            break;
        case GL_TEXTURE_MIN_LOD:
            *params = texture_lod_to_int(obj->min_lod);
            break;
        case GL_TEXTURE_MAX_LOD:
            *params = texture_lod_to_int(obj->max_lod);
            break;
        case GL_TEXTURE_LOD_BIAS:
            *params = texture_lod_to_int(obj->lod_bias);
            break;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
            if (!texture_anisotropy_supported(ctx)) {
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                break;
            }
            *params = texture_lod_to_int(obj->max_anisotropy);
            break;
        case GL_TEXTURE_BASE_LEVEL:
            *params = obj->base_level;
            break;
        case GL_TEXTURE_MAX_LEVEL:
            *params = obj->max_level;
            break;
        case GL_TEXTURE_IMMUTABLE_FORMAT:
            *params = obj->immutable_format ? (GLint)GL_TRUE : (GLint)GL_FALSE;
            break;
        case GL_TEXTURE_IMMUTABLE_LEVELS:
            *params = obj->immutable_levels;
            break;
        case GL_TEXTURE_VIEW_MIN_LEVEL:
            *params = obj->view_root != NULL ? (GLint)obj->view_min_level : 0;
            break;
        case GL_TEXTURE_VIEW_NUM_LEVELS:
            *params = obj->view_root != NULL ? (GLint)obj->view_num_levels : 0;
            break;
        case GL_TEXTURE_VIEW_MIN_LAYER:
            *params = obj->view_root != NULL ? (GLint)obj->view_min_layer : 0;
            break;
        case GL_TEXTURE_VIEW_NUM_LAYERS:
            *params = obj->view_root != NULL ? (GLint)obj->view_num_layers : 0;
            break;
        case GL_TEXTURE_COMPARE_MODE:
            *params = obj->compare_mode;
            break;
        case GL_TEXTURE_COMPARE_FUNC:
            *params = obj->compare_func;
            break;
        case GL_DEPTH_TEXTURE_MODE:
            *params = obj->depth_texture_mode;
            break;
        case GL_DEPTH_STENCIL_TEXTURE_MODE:
            *params = obj->depth_stencil_texture_mode;
            break;
        case GL_IMAGE_FORMAT_COMPATIBILITY_TYPE:
            *params = GL_IMAGE_FORMAT_COMPATIBILITY_BY_SIZE;
            break;
        case GL_TEXTURE_SWIZZLE_R:
        case GL_TEXTURE_SWIZZLE_G:
        case GL_TEXTURE_SWIZZLE_B:
        case GL_TEXTURE_SWIZZLE_A:
            *params = obj->swizzle[pname - GL_TEXTURE_SWIZZLE_R];
            break;
        case GL_TEXTURE_SWIZZLE_RGBA:
            memcpy(params, obj->swizzle, 4u * sizeof(GLint));
            break;
        case GL_TEXTURE_PRIORITY:
            *params = texture_float_to_int(obj->priority);
            break;
        case GL_TEXTURE_BORDER_COLOR:
            params[0] = texture_float_to_int(obj->border_color[0]);
            params[1] = texture_float_to_int(obj->border_color[1]);
            params[2] = texture_float_to_int(obj->border_color[2]);
            params[3] = texture_float_to_int(obj->border_color[3]);
            break;
        case GL_TEXTURE_RESIDENT:
            *params = (GLint)GL_TRUE;
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
}

void glGetTextureParameterfv(GLuint texture, GLenum pname, GLfloat *params) {
    GLint v = 0;
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname == GL_TEXTURE_SWIZZLE_RGBA) {
        unsigned i;
        for (i = 0u; i < 4u; ++i)
            params[i] = (GLfloat)obj->swizzle[i];
        return;
    }
    if (pname == GL_TEXTURE_MIN_LOD) {
        *params = obj->min_lod;
        return;
    }
    if (pname == GL_TEXTURE_MAX_LOD) {
        *params = obj->max_lod;
        return;
    }
    if (pname == GL_TEXTURE_LOD_BIAS) {
        *params = obj->lod_bias;
        return;
    }
    if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT) {
        if (!texture_anisotropy_supported(ctx)) {
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
        }
        *params = obj->max_anisotropy;
        return;
    }
    if (pname == GL_TEXTURE_PRIORITY) {
        *params = obj->priority;
        return;
    }
    if (pname == GL_TEXTURE_BORDER_COLOR) {
        memcpy(params, obj->border_color, sizeof(obj->border_color));
        return;
    }
    if (pname == GL_TEXTURE_RESIDENT) {
        *params = 1.0f;
        return;
    }
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetTextureParameteriv(texture, pname, &v);
    if (ctx->errors.sequence == error_sequence_before)
        *params = (GLfloat)v;
}

void glGetTextureParameterIiv(GLuint texture, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glGetTextureParameteriv(texture, pname, params);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    get_texture_border_color_i(obj, params);
}

void glGetTextureParameterIuiv(GLuint texture, GLenum pname, GLuint *params) {
    GLint value = 0;
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    if (pname == GL_TEXTURE_SWIZZLE_RGBA) {
        GLint values[4];
        unsigned i;
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetTextureParameteriv(texture, pname, values);
        if (ctx->errors.sequence == error_sequence_before)
            for (i = 0u; i < 4u; ++i)
                params[i] = (GLuint)values[i];
        return;
    }
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetTextureParameteriv(texture, pname, &value);
        if (ctx->errors.sequence == error_sequence_before)
            params[0] = (GLuint)value;
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    get_texture_border_color_ui(obj, params);
}

static int dsa_texture_1d_begin(struct mgl_context *ctx, GLuint texture, GLuint *saved) {
    struct mgl_texture *obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || obj->target != GL_TEXTURE_1D) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    *saved = ctx->texture_1d_binding[ctx->compat.active_texture];
    ctx->texture_1d_binding[ctx->compat.active_texture] = texture;
    dsa_native_bind_texture(ctx, obj);
    return 1;
}

static void dsa_texture_1d_end(struct mgl_context *ctx, GLuint saved) {
    ctx->texture_1d_binding[ctx->compat.active_texture] = saved;
    dsa_native_restore_texture(ctx, GL_TEXTURE_1D);
}

void glTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format,
                         GLenum type, const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint saved;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!dsa_texture_1d_begin(ctx, texture, &saved))
        return;
    glTexSubImage1D(GL_TEXTURE_1D, level, xoffset, width, format, type, pixels);
    dsa_texture_1d_end(ctx, saved);
}

void glCompressedTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width,
                                   GLenum format, GLsizei imageSize, const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint saved;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!dsa_texture_1d_begin(ctx, texture, &saved))
        return;
    glCompressedTexSubImage1D(GL_TEXTURE_1D, level, xoffset, width, format, imageSize, data);
    dsa_texture_1d_end(ctx, saved);
}

void glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x, GLint y,
                             GLsizei width) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint saved;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!dsa_texture_1d_begin(ctx, texture, &saved))
        return;
    glCopyTexSubImage1D(GL_TEXTURE_1D, level, xoffset, x, y, width);
    dsa_texture_1d_end(ctx, saved);
}

void glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                         GLsizei height, GLenum format, GLenum type, const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTextureSubImage2D",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height},
                                                       {"format", (uint64_t)(uintptr_t)format},
                                                       {"type", (uint64_t)(uintptr_t)type}},
                        8u);
    struct mgl_texture *obj;
    size_t components = 0u, data_size = 0u;
    const unsigned char *data;
    unsigned char *owned = NULL;
    GLuint saved;
    if (ctx == NULL)
        return;
    if (invalid_packed_upload_pair(format, type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || obj->target != GL_TEXTURE_2D) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_format_type(format, type, &components)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    data = tight_source_2d(ctx, pixels, width, height, components, pixel_element_size(type),
                           &data_size, &owned);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    saved = ctx->texture_2d_binding[ctx->compat.active_texture];
    ctx->texture_2d_binding[ctx->compat.active_texture] = texture;
    dsa_native_bind_texture(ctx, obj);
    (void)mgl_texture_apply_sub_image_2d_tight(ctx, obj->target, level, xoffset, yoffset, width,
                                               height, format, type, data, data_size);
    ctx->texture_2d_binding[ctx->compat.active_texture] = saved;
    dsa_native_restore_texture(ctx, obj->target);
    free(owned);
}

void glTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                         GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                         const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTextureSubImage3D",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"xoffset", (uint64_t)(uintptr_t)xoffset},
                                                       {"yoffset", (uint64_t)(uintptr_t)yoffset},
                                                       {"zoffset", (uint64_t)(uintptr_t)zoffset},
                                                       {"width", (uint64_t)(uintptr_t)width},
                                                       {"height", (uint64_t)(uintptr_t)height},
                                                       {"depth", (uint64_t)(uintptr_t)depth}},
                        8u);
    struct mgl_texture *obj;
    size_t components = 0u, data_size = 0u, slice_size = 0u;
    const unsigned char *data;
    unsigned char *owned = NULL;
    GLuint saved;
    GLsizei face;
    if (ctx == NULL)
        return;
    if (invalid_packed_upload_pair(format, type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || (obj->target != GL_TEXTURE_3D && obj->target != GL_TEXTURE_2D_ARRAY &&
                        obj->target != GL_TEXTURE_CUBE_MAP_ARRAY &&
                        obj->target != GL_TEXTURE_CUBE_MAP)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_format_type(format, type, &components)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0 || depth < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (obj->target == GL_TEXTURE_CUBE_MAP && (zoffset > 6 || depth > 6 - zoffset)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    data = tight_source_3d(ctx, pixels, width, height, depth, components, pixel_element_size(type),
                           &data_size, &owned);
    if (data == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (obj->target == GL_TEXTURE_CUBE_MAP) {
        GLenum validation = GL_NO_ERROR;
        if (!checked_mul((size_t)width, components, &slice_size) ||
            !checked_mul(slice_size, (size_t)height, &slice_size)) {
            free(owned);
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        for (face = 0; face < depth; ++face) {
            validation = validate_sub_image_2d_destination(
                obj, GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)zoffset + (GLenum)face, level,
                xoffset, yoffset, width, height, format, NULL);
            if (validation != GL_NO_ERROR)
                break;
        }
        if (validation != GL_NO_ERROR) {
            free(owned);
            mgl_error_set(&ctx->errors, validation);
            return;
        }
        saved = ctx->texture_cube_binding[ctx->compat.active_texture];
        ctx->texture_cube_binding[ctx->compat.active_texture] = texture;
        dsa_native_bind_texture(ctx, obj);
        for (face = 0; face < depth; ++face) {
            (void)mgl_texture_apply_sub_image_2d_tight(
                ctx, GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)zoffset + (GLenum)face, level,
                xoffset, yoffset, width, height, format, type, data + (size_t)face * slice_size,
                slice_size);
        }
        ctx->texture_cube_binding[ctx->compat.active_texture] = saved;
        dsa_native_restore_texture(ctx, GL_TEXTURE_CUBE_MAP);
        free(owned);
        return;
    }
    GLuint *binding = active_texture_binding_slot(ctx, obj->target);
    if (binding == NULL) {
        free(owned);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    saved = *binding;
    *binding = texture;
    dsa_native_bind_texture(ctx, obj);
    (void)mgl_texture_apply_sub_image_3d_tight(ctx, obj->target, level, xoffset, yoffset, zoffset,
                                               width, height, depth, format, type, data, data_size);
    *binding = saved;
    dsa_native_restore_texture(ctx, obj->target);
    free(owned);
}

void glGenerateTextureMipmap(GLuint texture) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    GLuint saved;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL || (obj->target != GL_TEXTURE_2D && obj->target != GL_TEXTURE_3D &&
                        obj->target != GL_TEXTURE_2D_ARRAY &&
                        obj->target != GL_TEXTURE_CUBE_MAP_ARRAY &&
                        obj->target != GL_TEXTURE_CUBE_MAP)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (obj->target == GL_TEXTURE_2D) {
        saved = ctx->texture_2d_binding[ctx->compat.active_texture];
        ctx->texture_2d_binding[ctx->compat.active_texture] = texture;
    } else if (obj->target == GL_TEXTURE_3D) {
        saved = ctx->texture_3d_binding[ctx->compat.active_texture];
        ctx->texture_3d_binding[ctx->compat.active_texture] = texture;
    } else if (obj->target == GL_TEXTURE_2D_ARRAY) {
        saved = ctx->texture_2d_array_binding[ctx->compat.active_texture];
        ctx->texture_2d_array_binding[ctx->compat.active_texture] = texture;
    } else if (obj->target == GL_TEXTURE_CUBE_MAP_ARRAY) {
        saved = ctx->texture_cube_array_binding[ctx->compat.active_texture];
        ctx->texture_cube_array_binding[ctx->compat.active_texture] = texture;
    } else {
        saved = ctx->texture_cube_binding[ctx->compat.active_texture];
        ctx->texture_cube_binding[ctx->compat.active_texture] = texture;
    }
    dsa_native_bind_texture(ctx, obj);
    glGenerateMipmap(obj->target);
    if (obj->target == GL_TEXTURE_2D)
        ctx->texture_2d_binding[ctx->compat.active_texture] = saved;
    else if (obj->target == GL_TEXTURE_3D)
        ctx->texture_3d_binding[ctx->compat.active_texture] = saved;
    else if (obj->target == GL_TEXTURE_2D_ARRAY)
        ctx->texture_2d_array_binding[ctx->compat.active_texture] = saved;
    else if (obj->target == GL_TEXTURE_CUBE_MAP_ARRAY)
        ctx->texture_cube_array_binding[ctx->compat.active_texture] = saved;
    else
        ctx->texture_cube_binding[ctx->compat.active_texture] = saved;
    dsa_native_restore_texture(ctx, obj->target);
}

void glGenerateTextureMipmapEXT(GLuint texture, GLenum target) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGenerateTextureMipmapEXT",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"target", (uint64_t)(uintptr_t)target}},
                        2u);
    struct mgl_texture *obj;
    GLuint *binding;
    GLuint saved;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TEXTURE_2D && target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY &&
        target != GL_TEXTURE_CUBE_MAP && target != GL_TEXTURE_CUBE_MAP_ARRAY) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (texture == 0u) {
        obj = mgl_default_texture(ctx, target);
    } else {
        (void)pthread_mutex_lock(&ctx->share_group->lock);
        obj = mgl_texture_find(ctx, texture);
        if (obj != NULL && obj->target != 0u && obj->target != target) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        obj = mgl_texture_ensure(ctx, texture, target, 1);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        if (obj == NULL || !obj->realized) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
    }
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    binding = active_texture_binding_slot(ctx, target);
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    saved = *binding;
    *binding = texture;
    dsa_native_bind_texture(ctx, obj);
    glGenerateMipmap(target);
    *binding = saved;
    dsa_native_restore_texture(ctx, target);
}

void glGenerateMultiTexMipmap(GLenum texunit, GLenum target) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned unit, saved_unit;
    if (ctx == NULL)
        return;
    if (!explicit_texunit_index(ctx, texunit, &unit))
        return;
    if (!explicit_unit_apply_begin(ctx, unit, &saved_unit))
        return;
    glGenerateMipmap(target);
    explicit_unit_apply_end(ctx, saved_unit);
}

void glGenerateMultiTexMipmapEXT(GLenum texunit, GLenum target) {
    glGenerateMultiTexMipmap(texunit, target);
}

void glGetTextureLevelParameteriv(GLuint texture, GLint level, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTextureLevelParameteriv",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texture *obj;
    struct mgl_texture_level *lv;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    obj = dsa_texture_checked(ctx, texture);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (obj->target == GL_TEXTURE_BUFFER) {
        struct mgl_buffer *buffer;
        size_t texel_size, width;
        if (level != 0) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        buffer = obj->buffer_texture_buffer;
        texel_size = texture_buffer_texel_size(obj->buffer_texture_internal_format);
        width = (buffer != NULL && texel_size != 0u && obj->buffer_texture_size > 0)
                    ? (size_t)obj->buffer_texture_size / texel_size
                    : 0u;
        if (width > MGL_MAX_TEXTURE_BUFFER_SIZE)
            width = MGL_MAX_TEXTURE_BUFFER_SIZE;
        if (texture_level_component_pname(pname)) {
            *params = mgl_texture_level_component_value(
                buffer != NULL ? obj->buffer_texture_internal_format : GL_NONE, pname);
            return;
        }
        switch (pname) {
            case GL_TEXTURE_WIDTH:
                *params = (GLint)width;
                return;
            case GL_TEXTURE_HEIGHT:
                *params = buffer != NULL ? 1 : 0;
                return;
            case GL_TEXTURE_DEPTH:
                *params = buffer != NULL ? 1 : 0;
                return;
            case GL_TEXTURE_INTERNAL_FORMAT:
                *params = (GLint)obj->buffer_texture_internal_format;
                return;
            case GL_TEXTURE_BUFFER_DATA_STORE_BINDING:
                *params = buffer != NULL ? (GLint)buffer->name : 0;
                return;
            case GL_TEXTURE_BUFFER_OFFSET:
                *params = buffer != NULL ? (GLint)obj->buffer_texture_offset : 0;
                return;
            case GL_TEXTURE_BUFFER_SIZE:
                *params = buffer != NULL ? (GLint)obj->buffer_texture_size : 0;
                return;
            case GL_TEXTURE_BORDER:
            case GL_TEXTURE_SAMPLES:
                *params = 0;
                return;
            case GL_TEXTURE_FIXED_SAMPLE_LOCATIONS:
                *params = GL_TRUE;
                return;
            case GL_TEXTURE_COMPRESSED:
                *params = 0;
                return;
            case GL_TEXTURE_COMPRESSED_IMAGE_SIZE:
                *params = 0;
                return;
            default:
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return;
        }
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if ((obj->target == GL_TEXTURE_RECTANGLE || obj->target == GL_TEXTURE_2D_MULTISAMPLE ||
         obj->target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) && level != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    lv = obj->target == GL_TEXTURE_CUBE_MAP ? &obj->cube_levels[0][level] : &obj->levels[level];
    if (texture_level_component_pname(pname)) {
        GLenum internal = lv->defined ? lv->internal_format : GL_NONE;
        *params = mgl_texture_level_component_value(internal, pname);
        if (internal == GL_RGBA16 && !rgba16_gpu_native(ctx) &&
            (pname == GL_TEXTURE_RED_SIZE || pname == GL_TEXTURE_GREEN_SIZE ||
             pname == GL_TEXTURE_BLUE_SIZE || pname == GL_TEXTURE_ALPHA_SIZE))
            *params = 8;
        return;
    }
    switch (pname) {
        case GL_TEXTURE_SAMPLES:
            *params = lv->defined ? lv->samples : 0;
            break;
        case GL_TEXTURE_FIXED_SAMPLE_LOCATIONS:
            *params = lv->defined && lv->fixed_sample_locations ? GL_TRUE : GL_FALSE;
            break;
        case GL_TEXTURE_WIDTH:
            *params = lv->defined ? lv->width : 0;
            break;
        case GL_TEXTURE_HEIGHT:
            *params = lv->defined ? lv->height : 0;
            break;
        case GL_TEXTURE_DEPTH:
            *params = lv->defined ? lv->depth : 0;
            break;
        case GL_TEXTURE_INTERNAL_FORMAT:
            *params = lv->defined ? (GLint)lv->internal_format : 0;
            break;
        case GL_TEXTURE_COMPRESSED:
            *params = lv->defined && lv->compressed ? GL_TRUE : GL_FALSE;
            break;
        case GL_TEXTURE_COMPRESSED_IMAGE_SIZE:
            *params = lv->defined && lv->compressed ? (GLint)lv->data_size : 0;
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
}

void glGetTextureLevelParameterfv(GLuint texture, GLint level, GLenum pname, GLfloat *params) {
    GLint value = 0;
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetTextureLevelParameteriv(texture, level, pname, &value);
    if (ctx->errors.sequence == error_sequence_before)
        *params = (GLfloat)value;
}

void glFramebufferTexture1DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                               GLint level) {
    glFramebufferTexture1D(target, attachment, textarget, texture, level);
}

void glFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                               GLint level) {
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void glFramebufferTexture3DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                               GLint level, GLint zoffset) {
    glFramebufferTexture3D(target, attachment, textarget, texture, level, zoffset);
}

void glFramebufferTextureARB(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    glFramebufferTexture(target, attachment, texture, level);
}

void glFramebufferTextureLayerARB(GLenum target, GLenum attachment, GLuint texture, GLint level,
                                  GLint layer) {
    glFramebufferTextureLayer(target, attachment, texture, level, layer);
}

void glFramebufferTextureFaceARB(GLenum target, GLenum attachment, GLuint texture, GLint level,
                                 GLenum face) {
    glFramebufferTexture2D(target, attachment, face, texture, level);
}

void glGenerateMipmapEXT(GLenum target) {
    glGenerateMipmap(target);
}

void glCopyTexSubImage3DEXT(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                            GLint x, GLint y, GLsizei width, GLsizei height) {
    glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void glTexImage3DEXT(GLenum target, GLint level, GLint internalformat, GLsizei width,
                     GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type,
                     const void *pixels) {
    glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);
}

void glTexSubImage3DEXT(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                        GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                        const void *pixels) {
    glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type,
                    pixels);
}

static int clear_texel_rgba8(const struct mgl_texture_level *level, GLenum format, GLenum type,
                             const void *data, unsigned char out[8], size_t *stride) {
    unsigned char rgba[4] = {0u, 0u, 0u, 0u};
    const unsigned char *src = data;
    unsigned channels;
    unsigned i;
    if (format != GL_RGBA && format != GL_RGB && format != GL_BGR && format != GL_BGRA)
        return 0;
    if (type != GL_UNSIGNED_BYTE && type != GL_FLOAT &&
        !(format == GL_BGRA && type == GL_UNSIGNED_INT_8_8_8_8_REV))
        return 0;
    if (level->type != GL_UNSIGNED_BYTE || (level->format != GL_RGB && level->format != GL_RGBA))
        return 0;
    channels = (format == GL_RGBA || format == GL_BGRA) ? 4u : 3u;
    if (src != NULL) {
        if (type == GL_FLOAT) {
            for (i = 0u; i < channels; ++i) {
                GLfloat value;
                memcpy(&value, src + (size_t)i * sizeof(value), sizeof(value));
                if (!(value > 0.0f))
                    rgba[i] = 0u;
                else if (value >= 1.0f)
                    rgba[i] = 255u;
                else
                    rgba[i] = (unsigned char)lrintf(value * 255.0f);
            }
        } else {
            memcpy(rgba, src, channels);
        }
        if (channels == 3u)
            rgba[3] = 255u;
        if (format == GL_BGR || format == GL_BGRA) {
            unsigned char blue = rgba[0];
            rgba[0] = rgba[2];
            rgba[2] = blue;
        }
    }
    *stride = level->format == GL_RGB ? 3u : 4u;
    for (i = 0u; i < *stride; ++i)
        out[i] = rgba[i];
    return 1;
}

static int clear_texel_pattern(const struct mgl_texture_level *level, GLenum format, GLenum type,
                               const void *data, unsigned char out[16], size_t *stride) {
    if (clear_texel_rgba8(level, format, type, data, out, stride))
        return 1;
    if (format != level->format || type != level->type)
        return 0;
    if (!valid_format_type(format, type, stride) || *stride > 16u)
        return 0;
    if (data != NULL)
        memcpy(out, data, *stride);
    else
        memset(out, 0, *stride);
    return 1;
}

static int clear_texel_region(struct mgl_context *ctx, GLuint texture, GLint level, GLint xoffset,
                              GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                              GLsizei depth, GLenum format, GLenum type, const void *data,
                              int whole_level) {
    struct mgl_texture *obj;
    struct mgl_texture_level *state;
    GLenum target;
    unsigned char texel[16];
    unsigned char *pixels;
    size_t stride, count, size, i;
    int first_face, last_face, face;
    GLuint *binding;
    GLuint saved;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0 || depth < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    obj = mgl_texture_find(ctx, texture);
    if (texture == 0u || obj == NULL || !obj->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    target = obj->target;
    if (target != GL_TEXTURE_1D && target != GL_TEXTURE_2D && target != GL_TEXTURE_3D &&
        target != GL_TEXTURE_2D_ARRAY && target != GL_TEXTURE_CUBE_MAP &&
        target != GL_TEXTURE_CUBE_MAP_ARRAY) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    state = target == GL_TEXTURE_CUBE_MAP ? &obj->cube_levels[0][level] : &obj->levels[level];
    if (!state->defined || state->compressed) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (whole_level) {
        xoffset = yoffset = zoffset = 0;
        width = state->width;
        height = state->height;
        depth = target == GL_TEXTURE_CUBE_MAP ? 6 : state->depth;
    }
    if ((target == GL_TEXTURE_1D && (yoffset != 0 || zoffset != 0 || height != 1 || depth != 1)) ||
        (target == GL_TEXTURE_2D && (zoffset != 0 || depth != 1)) ||
        (target == GL_TEXTURE_CUBE_MAP && (zoffset > 6 || depth > 6 - zoffset)) ||
        xoffset > state->width || width > state->width - xoffset || yoffset > state->height ||
        height > state->height - yoffset ||
        (target != GL_TEXTURE_CUBE_MAP &&
         (zoffset > state->depth || depth > state->depth - zoffset))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (target == GL_TEXTURE_CUBE_MAP) {
        for (face = zoffset; face < zoffset + depth; ++face) {
            const struct mgl_texture_level *other = &obj->cube_levels[face][level];
            if (!other->defined || other->compressed || other->width != state->width ||
                other->height != state->height || other->format != state->format ||
                other->type != state->type) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return 0;
            }
        }
    }
    if (!clear_texel_pattern(state, format, type, data, texel, &stride)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if ((size_t)width > SIZE_MAX / (size_t)(height > 0 ? height : 1) ||
        (size_t)width * (size_t)height > SIZE_MAX / stride) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    count = (size_t)width * (size_t)height;
    if (target != GL_TEXTURE_CUBE_MAP && target != GL_TEXTURE_1D && target != GL_TEXTURE_2D) {
        if (depth > 0 && count > SIZE_MAX / (size_t)depth) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        count *= (size_t)depth;
    }
    if (count > SIZE_MAX / stride) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    size = count * stride;
    pixels = malloc(size != 0u ? size : 1u);
    if (pixels == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    for (i = 0u; i < count; ++i)
        memcpy(pixels + i * stride, texel, stride);
    binding = active_texture_binding_slot(ctx, target);
    if (binding == NULL) {
        free(pixels);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    saved = *binding;
    *binding = texture;
    dsa_native_bind_texture(ctx, obj);
    if (target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY ||
        target == GL_TEXTURE_CUBE_MAP_ARRAY) {
        (void)mgl_texture_apply_sub_image_3d_tight(ctx, target, level, xoffset, yoffset, zoffset,
                                                   width, height, depth, state->format, state->type,
                                                   pixels, size);
    } else if (target == GL_TEXTURE_CUBE_MAP) {
        first_face = zoffset;
        last_face = zoffset + depth;
        for (face = first_face; face < last_face; ++face) {
            (void)mgl_texture_apply_sub_image_2d_tight(
                ctx, GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)face, level, xoffset, yoffset, width,
                height, state->format, state->type, pixels, size);
        }
    } else {
        (void)mgl_texture_apply_sub_image_2d_tight(ctx, target, level, xoffset, yoffset, width,
                                                   height, state->format, state->type, pixels,
                                                   size);
    }
    *binding = saved;
    dsa_native_restore_texture(ctx, target);
    free(pixels);
    return 1;
}

void glClearTexImage(GLuint texture, GLint level, GLenum format, GLenum type, const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx != NULL)
        (void)clear_texel_region(ctx, texture, level, 0, 0, 0, 0, 0, 0, format, type, data, 1);
}

void glClearTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                        GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                        const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx != NULL)
        (void)clear_texel_region(ctx, texture, level, xoffset, yoffset, zoffset, width, height,
                                 depth, format, type, data, 0);
}

void glInvalidateTexImage(GLuint texture, GLint level) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = mgl_texture_find(ctx, texture);
    if (obj == NULL || !obj->realized || obj->target == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
    }
}

void glInvalidateTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                             GLint zoffset, GLsizei width, GLsizei height, GLsizei depth) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_texture *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = mgl_texture_find(ctx, texture);
    if (obj == NULL || !obj->realized || obj->target == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        zoffset < 0 || width < 0 || height < 0 || depth < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
    }
}

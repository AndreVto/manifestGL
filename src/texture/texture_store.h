#ifndef MGL_TEXTURE_STORE_H
#define MGL_TEXTURE_STORE_H

#include "core/context.h"
#include "core/render_diag.h"

struct mgl_texture *mgl_texture_find(struct mgl_context *ctx, GLuint name);
struct mgl_texture *mgl_texture_ensure(struct mgl_context *ctx, GLuint name, GLenum target,
                                       int realize_native);
void mgl_texture_remove(struct mgl_context *ctx, GLuint name);
void mgl_texture_release_object(struct mgl_texture *obj);
struct mgl_texture *mgl_default_texture(struct mgl_context *ctx, GLenum target);
void mgl_texture_release_context_defaults(struct mgl_context *ctx);
struct mgl_texture *mgl_bound_texture(struct mgl_context *ctx, GLenum target);
struct mgl_texture *mgl_bound_texture_2d(struct mgl_context *ctx);

static inline GLenum mgl_texture_native_target(GLenum target) {
    return target == GL_TEXTURE_RECTANGLE || target == GL_TEXTURE_1D ? GL_TEXTURE_2D : target;
}

/* Temporary native texture binds invalidate subsequent draw-state realization. */
static inline void mgl_texture_bind_native(struct mgl_context *ctx, GLenum target, GLuint texture) {
    mgl_texture_mark_all_bindings_dirty(ctx);
    {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_BIND_TEXTURE);
        mgl_render_diag_native_check(ctx, "before_glesBindTexture", (uint64_t)target,
                                     (uint64_t)texture);
        ctx->native_dispatch->glesBindTexture(target, texture);
        mgl_render_diag_native_check(ctx, "glesBindTexture", (uint64_t)target, (uint64_t)texture);
    }
}

static inline void mgl_texture_bind_native_unit(struct mgl_context *ctx, GLenum target,
                                                GLuint texture, unsigned unit) {
    unsigned index = mgl_native_texture_target_index(target);
    unsigned char bit = index < MGL_NATIVE_TEXTURE_TARGET_COUNT ? (unsigned char)(1u << index) : 0u;
    mgl_texture_mark_unit_dirty(ctx, unit);
    if (unit < mgl_programmable_texture_limit(ctx) && bit != 0u &&
        (ctx->native_texture_known[unit] & bit) != 0u &&
        ctx->native_texture_names[unit][index] == texture)
        return;
    {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_BIND_TEXTURE);
        mgl_render_diag_native_check(ctx, "before_glesBindTexture", (uint64_t)target,
                                     (uint64_t)texture);
        ctx->native_dispatch->glesBindTexture(target, texture);
        mgl_render_diag_native_check(ctx, "glesBindTexture", (uint64_t)target, (uint64_t)texture);
    }
    if (unit < mgl_programmable_texture_limit(ctx) && bit != 0u) {
        ctx->native_texture_names[unit][index] = texture;
        ctx->native_texture_known[unit] |= bit;
    } else {
        mgl_native_texture_invalidate_target(ctx, unit, target);
    }
}

static inline void mgl_texture_bind_canonical_native_unit(struct mgl_context *ctx, GLenum target,
                                                          GLuint texture, unsigned unit) {
    int clean = !ctx->texture_bindings_dirty && ctx->native_dispatch->glesBindSampler != NULL;
    unsigned index = mgl_native_texture_target_index(target);
    if (clean && unit < mgl_programmable_texture_limit(ctx) &&
        index < MGL_NATIVE_TEXTURE_TARGET_COUNT &&
        (ctx->native_texture_known[unit] & (1u << index)) != 0u &&
        ctx->native_texture_names[unit][index] == texture)
        return;
    mgl_texture_bind_native_unit(ctx, target, texture, unit);
    if (clean) {
        unsigned i;
        ctx->texture_bindings_dirty_units[unit] = 0u;
        for (i = 0u; i < mgl_programmable_texture_limit(ctx); ++i)
            if (ctx->texture_bindings_dirty_units[i])
                break;
        if (i == mgl_programmable_texture_limit(ctx))
            ctx->texture_bindings_dirty = GL_FALSE;
    }
}

void mgl_texture_realize_bindings(struct mgl_context *ctx);
void mgl_texture_realize_fixed_bindings(struct mgl_context *ctx);
int mgl_texture_buffer_native_format_supported(GLenum internalformat);
GLenum mgl_texture_native_storage_internalformat(const struct mgl_context *ctx,
                                                 GLenum internalformat);
int mgl_texture_refresh_framebuffer_attachments(struct mgl_context *ctx,
                                                struct mgl_framebuffer *fb);
void mgl_texture_refresh_current_framebuffers(struct mgl_context *ctx);
int mgl_texture_recreate_native(struct mgl_context *ctx, struct mgl_texture *obj);
int mgl_read_color_rgba(struct mgl_context *ctx, GLint x, GLint y, GLsizei width, GLsizei height,
                        unsigned char **out_rgba, size_t *out_size);
int mgl_read_color_rgba_precise(struct mgl_context *ctx, GLint x, GLint y, GLsizei width,
                                GLsizei height, GLenum type, float **out_rgba);
int mgl_read_color_for_copy(struct mgl_context *ctx, GLint x, GLint y, GLsizei width,
                            GLsizei height, void **out_rgba, size_t *out_size, GLenum *out_type);

int mgl_texture_apply_image_2d_tight(struct mgl_context *ctx, GLenum target, GLint level,
                                     GLint internalformat, GLsizei width, GLsizei height,
                                     GLint border, GLenum format, GLenum type, const void *data,
                                     size_t data_size);
int mgl_texture_apply_sub_image_2d_tight(struct mgl_context *ctx, GLenum target, GLint level,
                                         GLint xoffset, GLint yoffset, GLsizei width,
                                         GLsizei height, GLenum format, GLenum type,
                                         const void *data, size_t data_size);
int mgl_texture_apply_image_3d_tight(struct mgl_context *ctx, GLenum target, GLint level,
                                     GLint internalformat, GLsizei width, GLsizei height,
                                     GLsizei depth, GLint border, GLenum format, GLenum type,
                                     const void *data, size_t data_size);
int mgl_texture_apply_sub_image_3d_tight(struct mgl_context *ctx, GLenum target, GLint level,
                                         GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,
                                         GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                         const void *data, size_t data_size);
int mgl_texture_apply_image_2d_tight_on_unit(struct mgl_context *ctx, unsigned unit, GLenum target,
                                             GLint level, GLint internalformat, GLsizei width,
                                             GLsizei height, GLint border, GLenum format,
                                             GLenum type, const void *data, size_t data_size);
int mgl_texture_apply_sub_image_2d_tight_on_unit(struct mgl_context *ctx, unsigned unit,
                                                 GLenum target, GLint level, GLint xoffset,
                                                 GLint yoffset, GLsizei width, GLsizei height,
                                                 GLenum format, GLenum type, const void *data,
                                                 size_t data_size);
int mgl_texture_apply_image_3d_tight_on_unit(struct mgl_context *ctx, unsigned unit, GLenum target,
                                             GLint level, GLint internalformat, GLsizei width,
                                             GLsizei height, GLsizei depth, GLint border,
                                             GLenum format, GLenum type, const void *data,
                                             size_t data_size);
int mgl_texture_apply_sub_image_3d_tight_on_unit(struct mgl_context *ctx, unsigned unit,
                                                 GLenum target, GLint level, GLint xoffset,
                                                 GLint yoffset, GLint zoffset, GLsizei width,
                                                 GLsizei height, GLsizei depth, GLenum format,
                                                 GLenum type, const void *data, size_t data_size);
int mgl_texture_apply_image_2d_tight_named_ext(struct mgl_context *ctx, GLuint texture,
                                               GLenum target, GLint level, GLint internalformat,
                                               GLsizei width, GLsizei height, GLint border,
                                               GLenum format, GLenum type, const void *data,
                                               size_t data_size);
int mgl_texture_apply_sub_image_2d_tight_named_ext(struct mgl_context *ctx, GLuint texture,
                                                   GLenum target, GLint level, GLint xoffset,
                                                   GLint yoffset, GLsizei width, GLsizei height,
                                                   GLenum format, GLenum type, const void *data,
                                                   size_t data_size);
int mgl_texture_apply_image_3d_tight_named_ext(struct mgl_context *ctx, GLuint texture,
                                               GLenum target, GLint level, GLint internalformat,
                                               GLsizei width, GLsizei height, GLsizei depth,
                                               GLint border, GLenum format, GLenum type,
                                               const void *data, size_t data_size);
int mgl_texture_apply_sub_image_3d_tight_named_ext(struct mgl_context *ctx, GLuint texture,
                                                   GLenum target, GLint level, GLint xoffset,
                                                   GLint yoffset, GLint zoffset, GLsizei width,
                                                   GLsizei height, GLsizei depth, GLenum format,
                                                   GLenum type, const void *data, size_t data_size);
int mgl_texture_apply_compressed_image_2d_tight_named_ext(struct mgl_context *ctx, GLuint texture,
                                                          GLenum target, GLint level,
                                                          GLenum internalformat, GLsizei width,
                                                          GLsizei height, GLint border,
                                                          const void *data, size_t data_size);
int mgl_texture_apply_compressed_sub_image_2d_tight_named_ext(struct mgl_context *ctx,
                                                              GLuint texture, GLenum target,
                                                              GLint level, GLint xoffset,
                                                              GLint yoffset, GLsizei width,
                                                              GLsizei height, GLenum format,
                                                              const void *data, size_t data_size);
int mgl_texture_apply_compressed_image_3d_tight_named_ext(
    struct mgl_context *ctx, GLuint texture, GLenum target, GLint level, GLenum internalformat,
    GLsizei width, GLsizei height, GLsizei depth, GLint border, const void *data, size_t data_size);
int mgl_texture_apply_compressed_sub_image_3d_tight_named_ext(
    struct mgl_context *ctx, GLuint texture, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format,
    const void *data, size_t data_size);
int mgl_texture_apply_compressed_image_2d_tight_on_unit(struct mgl_context *ctx, unsigned unit,
                                                        GLenum target, GLint level,
                                                        GLenum internalformat, GLsizei width,
                                                        GLsizei height, GLint border,
                                                        const void *data, size_t data_size);
int mgl_texture_apply_compressed_sub_image_2d_tight_on_unit(struct mgl_context *ctx, unsigned unit,
                                                            GLenum target, GLint level,
                                                            GLint xoffset, GLint yoffset,
                                                            GLsizei width, GLsizei height,
                                                            GLenum format, const void *data,
                                                            size_t data_size);
int mgl_texture_apply_compressed_image_3d_tight_on_unit(struct mgl_context *ctx, unsigned unit,
                                                        GLenum target, GLint level,
                                                        GLenum internalformat, GLsizei width,
                                                        GLsizei height, GLsizei depth, GLint border,
                                                        const void *data, size_t data_size);
int mgl_texture_apply_compressed_sub_image_3d_tight_on_unit(
    struct mgl_context *ctx, unsigned unit, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format,
    const void *data, size_t data_size);

#endif

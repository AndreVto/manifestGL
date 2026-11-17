#include "texture/texture_store.h"
#include "core/render_diag.h"
#include "objects/object_store.h"
#include "objects/sampler.h"

#include <stdlib.h>

static void initialize_texture_state(struct mgl_texture *obj, GLuint name, GLenum target,
                                     GLboolean realized) {
    if (obj == NULL)
        return;
    obj->name = name;
    obj->native_name = 0u;
    obj->realized = realized;
    obj->target = target;
    obj->min_filter = GL_NEAREST_MIPMAP_LINEAR;
    obj->mag_filter = GL_LINEAR;
    obj->wrap_s = GL_REPEAT;
    obj->wrap_t = GL_REPEAT;
    obj->wrap_r = GL_REPEAT;
    if (target == GL_TEXTURE_RECTANGLE) {
        obj->min_filter = GL_LINEAR;
        obj->wrap_s = GL_CLAMP_TO_EDGE;
        obj->wrap_t = GL_CLAMP_TO_EDGE;
        obj->wrap_r = GL_CLAMP_TO_EDGE;
        obj->max_level = 0;
    }
    obj->min_lod = -1000.0f;
    obj->max_lod = 1000.0f;
    obj->lod_bias = 0.0f;
    obj->max_anisotropy = 1.0f;
    obj->priority = 1.0f;
    obj->base_level = 0;
    obj->max_level = target == GL_TEXTURE_RECTANGLE ? 0 : 1000;
    obj->compare_mode = (GLint)GL_NONE;
    obj->compare_func = (GLint)GL_LEQUAL;
    obj->depth_texture_mode = (GLint)GL_LUMINANCE;
    obj->depth_stencil_texture_mode = (GLint)GL_DEPTH_COMPONENT;
    obj->swizzle[0] = GL_RED;
    obj->swizzle[1] = GL_GREEN;
    obj->swizzle[2] = GL_BLUE;
    obj->swizzle[3] = GL_ALPHA;
    obj->border_color[0] = 0.0f;
    obj->border_color[1] = 0.0f;
    obj->border_color[2] = 0.0f;
    obj->border_color[3] = 0.0f;
    obj->border_color_i[0] = 0;
    obj->border_color_i[1] = 0;
    obj->border_color_i[2] = 0;
    obj->border_color_i[3] = 0;
    obj->border_color_ui[0] = 0u;
    obj->border_color_ui[1] = 0u;
    obj->border_color_ui[2] = 0u;
    obj->border_color_ui[3] = 0u;
    obj->border_color_type = GL_FLOAT;
    obj->buffer_texture_internal_format = GL_R8;
    obj->buffer_texture_offset = 0;
    obj->buffer_texture_size = 0;
    obj->immutable_format = GL_FALSE;
    obj->immutable_levels = 0;
    obj->native_immutable_storage = GL_FALSE;
    obj->native_compressed_storage = GL_FALSE;
}

static GLenum normalize_default_target(GLenum target) {
    if (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
        return GL_TEXTURE_CUBE_MAP;
    return target;
}

int mgl_texture_buffer_native_format_supported(GLenum internalformat) {
    /* GL_EXT_texture_buffer on GLES lacks normalized 16-bit R/RG/RGBA formats required by
     * desktop OpenGL 3.3. Their canonical formats are retained until a conversion realization is
     * available. */
    return internalformat != GL_R16 && internalformat != GL_RG16 && internalformat != GL_RGBA16;
}

struct mgl_texture *mgl_default_texture(struct mgl_context *ctx, GLenum target) {
    struct mgl_texture *obj = NULL;
    GLenum normalized;
    if (ctx == NULL)
        return NULL;
    normalized = normalize_default_target(target);
    if (normalized == GL_TEXTURE_1D)
        obj = &ctx->default_texture_1d;
    else if (normalized == GL_TEXTURE_2D)
        obj = &ctx->default_texture_2d;
    else if (normalized == GL_TEXTURE_RECTANGLE)
        obj = &ctx->default_texture_rectangle;
    else if (normalized == GL_TEXTURE_3D)
        obj = &ctx->default_texture_3d;
    else if (normalized == GL_TEXTURE_2D_ARRAY)
        obj = &ctx->default_texture_2d_array;
    else if (normalized == GL_TEXTURE_BUFFER)
        obj = &ctx->default_texture_buffer;
    else if (normalized == GL_TEXTURE_CUBE_MAP)
        obj = &ctx->default_texture_cube;
    else if (normalized == GL_TEXTURE_CUBE_MAP_ARRAY)
        obj = &ctx->default_texture_cube_array;
    if (obj == NULL)
        return NULL;
    if (obj->target == 0u)
        initialize_texture_state(obj, 0u, normalized, GL_TRUE);
    return obj;
}

static void release_texture_level_storage(struct mgl_texture *obj) {
    unsigned level;
    if (obj == NULL || obj->view_root != NULL)
        return;
    for (level = 0u; level < MGL_MAX_TEXTURE_LEVELS; ++level) {
        unsigned face;
        free(obj->levels[level].data);
        obj->levels[level].data = NULL;
        for (face = 0u; face < 6u; ++face) {
            free(obj->cube_levels[face][level].data);
            obj->cube_levels[face][level].data = NULL;
        }
    }
}

void mgl_texture_release_object(struct mgl_texture *obj) {
    struct mgl_texture *root;
    if (obj == NULL)
        return;
    root = obj->view_root;
    if (root == NULL && obj->view_refcount != 0u) {
        obj->deleted_name = GL_TRUE;
        return;
    }
    release_texture_level_storage(obj);
    free(obj);
    if (root != NULL && --root->view_refcount == 0u && root->deleted_name)
        mgl_texture_release_object(root);
}

void mgl_texture_release_context_defaults(struct mgl_context *ctx) {
    if (ctx == NULL)
        return;
    release_texture_level_storage(&ctx->default_texture_1d);
    release_texture_level_storage(&ctx->default_texture_2d);
    release_texture_level_storage(&ctx->default_texture_rectangle);
    release_texture_level_storage(&ctx->default_texture_3d);
    release_texture_level_storage(&ctx->default_texture_2d_array);
    release_texture_level_storage(&ctx->default_texture_cube);
    release_texture_level_storage(&ctx->default_texture_cube_array);
    if (ctx->default_texture_buffer.buffer_texture_buffer != NULL && ctx->share_group != NULL) {
        (void)pthread_mutex_lock(&ctx->share_group->lock);
        mgl_buffer_release_texture(ctx, ctx->default_texture_buffer.buffer_texture_buffer);
        ctx->default_texture_buffer.buffer_texture_buffer = NULL;
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
    }
}

static void advance_atomic_name(atomic_uint *counter, GLuint name) {
    unsigned current = atomic_load_explicit(counter, memory_order_relaxed);
    while (current <= name &&
           !atomic_compare_exchange_weak_explicit(counter, &current, name + 1u,
                                                  memory_order_relaxed, memory_order_relaxed)) {}
}

static unsigned texture_bucket(GLuint name) {
    return (unsigned)(((uint32_t)name * UINT32_C(2654435761)) >> 22u);
}

struct mgl_texture *mgl_texture_find(struct mgl_context *ctx, GLuint name) {
    struct mgl_texture *it;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return NULL;
    for (it = ctx->share_group->texture_index[texture_bucket(name)]; it != NULL;
         it = it->hash_next) {
        if (it->name == name)
            return it;
    }
    return NULL;
}

static int realize(struct mgl_context *ctx, struct mgl_texture *obj) {
    if (obj->native_name != 0u)
        return 1;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGenTextures == NULL)
        return 0;
    ctx->native_dispatch->glesGenTextures(1, &obj->native_name);
    if (obj->native_name == 0u)
        return 0;
    obj->realized = GL_TRUE;
    return 1;
}

struct mgl_texture *mgl_texture_ensure(struct mgl_context *ctx, GLuint name, GLenum target,
                                       int realize_native) {
    struct mgl_texture *obj;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return NULL;
    obj = mgl_texture_find(ctx, name);
    if (obj == NULL) {
        obj = calloc(1, sizeof(*obj));
        if (obj == NULL)
            return NULL;
        initialize_texture_state(obj, name, target, GL_FALSE);
        obj->next = ctx->share_group->textures;
        ctx->share_group->textures = obj;
        obj->hash_next = ctx->share_group->texture_index[texture_bucket(name)];
        ctx->share_group->texture_index[texture_bucket(name)] = obj;
        advance_atomic_name(&ctx->share_group->next_texture_name, name);
    } else if (obj->target != 0u && target != 0u && obj->target != target) {
        return NULL;
    }
    if (obj->target == 0u) {
        obj->target = target;
        if (target == GL_TEXTURE_RECTANGLE) {
            obj->min_filter = GL_LINEAR;
            obj->wrap_s = GL_CLAMP_TO_EDGE;
            obj->wrap_t = GL_CLAMP_TO_EDGE;
            obj->wrap_r = GL_CLAMP_TO_EDGE;
            obj->max_level = 0;
        }
    }
    if (realize_native && !realize(ctx, obj))
        return NULL;
    return obj;
}

void mgl_texture_remove(struct mgl_context *ctx, GLuint name) {
    struct mgl_texture **link;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return;
    link = &ctx->share_group->textures;
    while (*link != NULL) {
        if ((*link)->name == name) {
            struct mgl_texture *victim = *link;
            struct mgl_texture **bucket_link =
                &ctx->share_group->texture_index[texture_bucket(name)];
            while (*bucket_link != NULL && *bucket_link != victim)
                bucket_link = &(*bucket_link)->hash_next;
            if (*bucket_link == victim)
                *bucket_link = victim->hash_next;
            *link = victim->next;
            if (victim->native_name != 0u && ctx->native_dispatch != NULL &&
                ctx->native_dispatch->glesDeleteTextures != NULL)
                ctx->native_dispatch->glesDeleteTextures(1, &victim->native_name);
            if (victim->buffer_texture_buffer != NULL)
                mgl_buffer_release_texture(ctx, victim->buffer_texture_buffer);
            mgl_texture_release_object(victim);
            return;
        }
        link = &(*link)->next;
    }
}

struct mgl_texture *mgl_bound_texture(struct mgl_context *ctx, GLenum target) {
    GLuint unit;
    GLuint name;
    if (ctx == NULL)
        return NULL;
    unit = ctx->compat.active_texture;
    if (unit >= mgl_programmable_texture_limit(ctx))
        return NULL;
    if (target == GL_TEXTURE_1D)
        name = ctx->texture_1d_binding[unit];
    else if (target == GL_TEXTURE_2D)
        name = ctx->texture_2d_binding[unit];
    else if (target == GL_TEXTURE_RECTANGLE)
        name = ctx->texture_rectangle_binding[unit];
    else if (target == GL_TEXTURE_3D)
        name = ctx->texture_3d_binding[unit];
    else if (target == GL_TEXTURE_2D_ARRAY)
        name = ctx->texture_2d_array_binding[unit];
    else if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
        name = ctx->texture_cube_array_binding[unit];
    else if (target == GL_TEXTURE_BUFFER)
        name = ctx->texture_buffer_binding[unit];
    else if (target == GL_TEXTURE_CUBE_MAP ||
             (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z))
        name = ctx->texture_cube_binding[unit];
    else if (target == GL_TEXTURE_2D_MULTISAMPLE)
        name = ctx->texture_2d_multisample_binding[unit];
    else if (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        name = ctx->texture_2d_multisample_array_binding[unit];
    else
        return NULL;
    if (name == 0u)
        return mgl_default_texture(ctx, target);
    return mgl_texture_find(ctx, name);
}

struct mgl_texture *mgl_bound_texture_2d(struct mgl_context *ctx) {
    return mgl_bound_texture(ctx, GL_TEXTURE_2D);
}

static void realize_one_texture_binding(struct mgl_context *ctx, unsigned unit,
                                        unsigned target_index, GLenum target, GLuint name,
                                        unsigned *active_unit) {
    unsigned char bit = (unsigned char)(1u << target_index);
    if ((ctx->native_texture_known[unit] & bit) != 0u &&
        ctx->native_texture_names[unit][target_index] == name)
        return;
    if (*active_unit != unit) {
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + unit);
        *active_unit = unit;
    }
    mgl_render_diag_native_check(ctx, "before_glesBindTexture", (uint64_t)target, (uint64_t)name);
    ctx->native_dispatch->glesBindTexture(target, name);
    mgl_render_diag_native_check(ctx, "glesBindTexture", (uint64_t)target, (uint64_t)name);
    ctx->native_texture_names[unit][target_index] = name;
    ctx->native_texture_known[unit] |= bit;
}

static GLuint realized_texture_name(const struct mgl_texture *texture) {
    return texture != NULL && texture->realized ? texture->native_name : 0u;
}

void mgl_texture_realize_bindings(struct mgl_context *ctx) {
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_TEXTURE_REALIZE);
    unsigned unit, active_unit = UINT32_MAX;
    unsigned dirty_count = 0u;
    uint64_t perf_started;
    int has_texture_buffer;
    if (ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesActiveTexture == NULL ||
        ctx->native_dispatch->glesBindTexture == NULL)
        return;
    if (ctx->fixed_texture_binding_override &&
        (ctx->current_program != 0u || ctx->compat.arb_vertex_program_enabled ||
         ctx->compat.arb_fragment_program_enabled)) {
        mgl_texture_mark_all_bindings_dirty(ctx);
        ctx->fixed_texture_binding_override = GL_FALSE;
    }
    if (!ctx->texture_bindings_dirty)
        return;
    for (unit = 0u; unit < mgl_programmable_texture_limit(ctx); ++unit)
        dirty_count += ctx->texture_bindings_dirty_units[unit] != 0u;
    if (dirty_count == 0u) {
        memset(ctx->texture_bindings_dirty_units, 1, mgl_programmable_texture_limit(ctx));
        dirty_count = mgl_programmable_texture_limit(ctx);
    }
    perf_started = mgl_perf_diag_now();
    mgl_perf_diag_texture_rebind(ctx, dirty_count);
    if (!ctx->texture_buffer_extension_checked) {
        ctx->texture_buffer_extension_supported =
            ctx->native_dispatch != NULL && ctx->native_dispatch->glesTexBufferEXT != NULL
                ? GL_TRUE
                : GL_FALSE;
        ctx->texture_buffer_extension_checked = GL_TRUE;
    }
    has_texture_buffer = ctx->texture_buffer_extension_supported;
    for (unit = 0u; unit < mgl_programmable_texture_limit(ctx); ++unit) {
        struct mgl_texture *texture, *tex3d, *array, *buffer, *cube, *cube_array;
        if (ctx->texture_bindings_dirty_units[unit] == 0u)
            continue;
        /* Rectangle textures share the native GLES 2D binding slot while retaining independent
         * desktop target bindings. */
        if (ctx->texture_2d_prefer_1d[unit] && ctx->texture_1d_binding[unit] != 0u)
            texture = mgl_texture_find(ctx, ctx->texture_1d_binding[unit]);
        else if (ctx->texture_2d_prefer_rectangle[unit] &&
                 ctx->texture_rectangle_binding[unit] != 0u)
            texture = mgl_texture_find(ctx, ctx->texture_rectangle_binding[unit]);
        else
            texture = ctx->texture_2d_binding[unit] == 0u
                          ? mgl_default_texture(ctx, GL_TEXTURE_2D)
                          : mgl_texture_find(ctx, ctx->texture_2d_binding[unit]);
        tex3d = ctx->texture_3d_binding[unit] == 0u
                    ? mgl_default_texture(ctx, GL_TEXTURE_3D)
                    : mgl_texture_find(ctx, ctx->texture_3d_binding[unit]);
        array = ctx->texture_2d_array_binding[unit] == 0u
                    ? mgl_default_texture(ctx, GL_TEXTURE_2D_ARRAY)
                    : mgl_texture_find(ctx, ctx->texture_2d_array_binding[unit]);
        buffer = ctx->texture_buffer_binding[unit] == 0u
                     ? mgl_default_texture(ctx, GL_TEXTURE_BUFFER)
                     : mgl_texture_find(ctx, ctx->texture_buffer_binding[unit]);
        cube = ctx->texture_cube_binding[unit] == 0u
                   ? mgl_default_texture(ctx, GL_TEXTURE_CUBE_MAP)
                   : mgl_texture_find(ctx, ctx->texture_cube_binding[unit]);
        cube_array = ctx->texture_cube_array_binding[unit] == 0u
                         ? mgl_default_texture(ctx, GL_TEXTURE_CUBE_MAP_ARRAY)
                         : mgl_texture_find(ctx, ctx->texture_cube_array_binding[unit]);
        realize_one_texture_binding(ctx, unit, 0u, GL_TEXTURE_2D, realized_texture_name(texture),
                                    &active_unit);
        if (ctx->native_dispatch->glesTexImage3D != NULL) {
            realize_one_texture_binding(ctx, unit, 1u, GL_TEXTURE_3D, realized_texture_name(tex3d),
                                        &active_unit);
            realize_one_texture_binding(ctx, unit, 2u, GL_TEXTURE_2D_ARRAY,
                                        realized_texture_name(array), &active_unit);
        }
        realize_one_texture_binding(ctx, unit, 3u, GL_TEXTURE_CUBE_MAP, realized_texture_name(cube),
                                    &active_unit);
        realize_one_texture_binding(ctx, unit, 7u, GL_TEXTURE_CUBE_MAP_ARRAY,
                                    realized_texture_name(cube_array), &active_unit);
        if (has_texture_buffer)
            realize_one_texture_binding(ctx, unit, 4u, GL_TEXTURE_BUFFER,
                                        realized_texture_name(buffer), &active_unit);
        if (ctx->native_dispatch->glesTexStorage2DMultisample != NULL) {
            struct mgl_texture *ms =
                mgl_texture_find(ctx, ctx->texture_2d_multisample_binding[unit]);
            realize_one_texture_binding(ctx, unit, 5u, GL_TEXTURE_2D_MULTISAMPLE,
                                        realized_texture_name(ms), &active_unit);
        }
        if (ctx->native_dispatch->glesTexStorage3DMultisample != NULL) {
            struct mgl_texture *msa =
                mgl_texture_find(ctx, ctx->texture_2d_multisample_array_binding[unit]);
            realize_one_texture_binding(ctx, unit, 6u, GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                        realized_texture_name(msa), &active_unit);
        }
        if (ctx->native_dispatch->glesBindSampler == NULL) {

            if (active_unit != unit) {
                mgl_native_select_active_texture(ctx, GL_TEXTURE0 + unit);
                active_unit = unit;
            }
            mgl_sampler_realize_unit(ctx, unit, texture);
            if (ctx->native_dispatch->glesTexImage3D != NULL) {
                mgl_sampler_realize_unit(ctx, unit, tex3d);
                mgl_sampler_realize_unit(ctx, unit, array);
            }
            mgl_sampler_realize_unit(ctx, unit, cube);
            mgl_sampler_realize_unit(ctx, unit, cube_array);
        }
    }
    if (active_unit != ctx->compat.active_texture)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + ctx->compat.active_texture);
    ctx->texture_bindings_dirty = GL_FALSE;
    memset(ctx->texture_bindings_dirty_units, 0, mgl_programmable_texture_limit(ctx));
    mgl_perf_diag_extra(ctx, MGL_PERF_TEXTURE_REALIZE, perf_started);
}

void mgl_texture_realize_fixed_bindings(struct mgl_context *ctx) {
    unsigned unit, active_unit = UINT32_MAX;
    if (ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesActiveTexture == NULL ||
        ctx->native_dispatch->glesBindTexture == NULL)
        return;
    for (unit = 0u; unit < MGL_TEXTURE_UNITS && unit < mgl_programmable_texture_limit(ctx);
         ++unit) {
        struct mgl_texture *texture;
        GLuint interface_name, native_name;
        GLenum target;
        unsigned native_index;
        GLenum native_target;
        target = mgl_compat_selected_texture_target(&ctx->compat, unit);
        if (target == 0u)
            continue;
        if (target == GL_TEXTURE_1D)
            interface_name = ctx->texture_1d_binding[unit];
        else if (target == GL_TEXTURE_RECTANGLE)
            interface_name = ctx->texture_rectangle_binding[unit];
        else if (target == GL_TEXTURE_3D)
            interface_name = ctx->texture_3d_binding[unit];
        else if (target == GL_TEXTURE_CUBE_MAP)
            interface_name = ctx->texture_cube_binding[unit];
        else
            interface_name = ctx->texture_2d_binding[unit];
        native_target =
            target == GL_TEXTURE_3D || target == GL_TEXTURE_CUBE_MAP ? target : GL_TEXTURE_2D;
        native_index = target == GL_TEXTURE_3D ? 1u : target == GL_TEXTURE_CUBE_MAP ? 3u : 0u;
        texture = interface_name == 0u ? mgl_default_texture(ctx, target)
                                       : mgl_texture_find(ctx, interface_name);
        native_name = realized_texture_name(texture);
        if ((ctx->native_texture_known[unit] & (1u << native_index)) != 0u &&
            ctx->native_texture_names[unit][native_index] == native_name)
            continue;
        realize_one_texture_binding(ctx, unit, native_index, native_target, native_name,
                                    &active_unit);
        if (ctx->native_dispatch->glesBindSampler == NULL)
            mgl_sampler_realize_unit(ctx, unit, texture);
        ctx->fixed_texture_binding_override = GL_TRUE;
    }
    if (active_unit != UINT32_MAX && active_unit != ctx->compat.active_texture)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0 + ctx->compat.active_texture);
}

static GLuint framebuffer_native_name(struct mgl_context *ctx, GLuint name) {
    struct mgl_framebuffer *fb = mgl_framebuffer_find(ctx, name);
    return fb != NULL && fb->realized ? fb->native_name : 0u;
}

static int framebuffer_references_texture(const struct mgl_framebuffer *fb, GLuint texture) {
    unsigned i;
    if (fb == NULL)
        return 0;
    if (fb->color_texture == texture || fb->depth_texture == texture ||
        fb->stencil_texture == texture)
        return 1;
    for (i = 1u; i < MGL_MAX_DRAW_BUFFERS; ++i)
        if (fb->extra_color_textures[i] == texture)
            return 1;
    return 0;
}

static int can_refresh_framebuffer_texture(struct mgl_context *ctx,
                                           const struct mgl_framebuffer *fb, GLuint texture) {
    if (!framebuffer_references_texture(fb, texture))
        return 1;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesBindFramebuffer == NULL)
        return 0;
    {
        unsigned i;
        for (i = 1u; i < MGL_MAX_DRAW_BUFFERS; ++i) {
            if (fb->extra_color_textures[i] == texture &&
                ((fb->extra_color_layers[i] >= 0 &&
                  ctx->native_dispatch->glesFramebufferTextureLayer == NULL) ||
                 (fb->extra_color_layers[i] == -2 &&
                  ctx->native_dispatch->glesFramebufferTexture == NULL) ||
                 (fb->extra_color_layers[i] == -1 &&
                  ctx->native_dispatch->glesFramebufferTexture2D == NULL)))
                return 0;
        }
    }
    if ((fb->color_texture == texture && fb->color_texture_layer >= 0) ||
        (fb->depth_texture == texture && fb->depth_texture_layer >= 0) ||
        (fb->stencil_texture == texture && fb->stencil_texture_layer >= 0)) {
        if (ctx->native_dispatch->glesFramebufferTextureLayer == NULL)
            return 0;
    }
    if ((fb->color_texture == texture && fb->color_texture_layer == -1) ||
        (fb->depth_texture == texture && fb->depth_texture_layer == -1) ||
        (fb->stencil_texture == texture && fb->stencil_texture_layer == -1)) {
        if (ctx->native_dispatch->glesFramebufferTexture2D == NULL)
            return 0;
    }
    if ((fb->color_texture == texture && fb->color_texture_layer == -2) ||
        (fb->depth_texture == texture && fb->depth_texture_layer == -2) ||
        (fb->stencil_texture == texture && fb->stencil_texture_layer == -2)) {
        if (ctx->native_dispatch->glesFramebufferTexture == NULL)
            return 0;
    }
    return 1;
}

static void refresh_one_framebuffer_attachment(struct mgl_context *ctx, GLenum attachment,
                                               GLuint texture, GLint level, GLint layer,
                                               GLenum textarget, GLuint native_texture) {
    if (texture == 0u)
        return;
    if (layer == -2)
        ctx->native_dispatch->glesFramebufferTexture(GL_FRAMEBUFFER, attachment, native_texture,
                                                     level);
    else if (layer >= 0)
        ctx->native_dispatch->glesFramebufferTextureLayer(GL_FRAMEBUFFER, attachment,
                                                          native_texture, level, layer);
    else
        ctx->native_dispatch->glesFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                                                       mgl_texture_native_target(textarget),
                                                       native_texture, level);
    mgl_render_diag_native_check(ctx, "glesFramebufferAttachment", (uint64_t)attachment,
                                 (uint64_t)native_texture);
}

static void refresh_framebuffers_for_texture(struct mgl_context *ctx,
                                             const struct mgl_texture *obj) {
    struct mgl_framebuffer *fb;
    if (ctx == NULL || obj == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesBindFramebuffer == NULL)
        return;
    for (fb = ctx->framebuffers; fb != NULL; fb = fb->next) {
        unsigned i;
        if (!fb->realized || !framebuffer_references_texture(fb, obj->name))
            continue;
        ctx->native_dispatch->glesBindFramebuffer(GL_FRAMEBUFFER, fb->native_name);
        if (fb->color_texture == obj->name)
            refresh_one_framebuffer_attachment(ctx, GL_COLOR_ATTACHMENT0, fb->color_texture,
                                               fb->color_texture_level, fb->color_texture_layer,
                                               fb->color_texture_target, obj->native_name);
        for (i = 1u; i < MGL_MAX_DRAW_BUFFERS; ++i)
            if (fb->extra_color_textures[i] == obj->name)
                refresh_one_framebuffer_attachment(
                    ctx, GL_COLOR_ATTACHMENT0 + i, fb->extra_color_textures[i],
                    fb->extra_color_levels[i], fb->extra_color_layers[i],
                    fb->extra_color_targets[i], obj->native_name);
        if (fb->depth_texture == obj->name)
            refresh_one_framebuffer_attachment(ctx, GL_DEPTH_ATTACHMENT, fb->depth_texture,
                                               fb->depth_texture_level, fb->depth_texture_layer,
                                               fb->depth_texture_target, obj->native_name);
        if (fb->stencil_texture == obj->name)
            refresh_one_framebuffer_attachment(ctx, GL_STENCIL_ATTACHMENT, fb->stencil_texture,
                                               fb->stencil_texture_level, fb->stencil_texture_layer,
                                               fb->stencil_texture_target, obj->native_name);
    }
    ctx->native_dispatch->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                              framebuffer_native_name(ctx, ctx->draw_framebuffer));
    ctx->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER,
                                              framebuffer_native_name(ctx, ctx->read_framebuffer));
}

int mgl_texture_refresh_framebuffer_attachments(struct mgl_context *ctx,
                                                struct mgl_framebuffer *fb) {
    struct mgl_texture *texture;
    unsigned i;
    if (ctx == NULL || fb == NULL || !fb->realized || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesBindFramebuffer == NULL)
        return 0;
    ctx->native_dispatch->glesBindFramebuffer(GL_FRAMEBUFFER, fb->native_name);
    if (fb->color_texture != 0u) {
        texture = mgl_texture_find(ctx, fb->color_texture);
        if (texture != NULL)
            refresh_one_framebuffer_attachment(ctx, GL_COLOR_ATTACHMENT0, fb->color_texture,
                                               fb->color_texture_level, fb->color_texture_layer,
                                               fb->color_texture_target, texture->native_name);
    }
    for (i = 1u; i < MGL_MAX_DRAW_BUFFERS; ++i) {
        if (fb->extra_color_textures[i] == 0u)
            continue;
        texture = mgl_texture_find(ctx, fb->extra_color_textures[i]);
        if (texture != NULL)
            refresh_one_framebuffer_attachment(ctx, GL_COLOR_ATTACHMENT0 + i,
                                               fb->extra_color_textures[i],
                                               fb->extra_color_levels[i], fb->extra_color_layers[i],
                                               fb->extra_color_targets[i], texture->native_name);
    }
    if (fb->depth_texture != 0u) {
        texture = mgl_texture_find(ctx, fb->depth_texture);
        if (texture != NULL)
            refresh_one_framebuffer_attachment(ctx, GL_DEPTH_ATTACHMENT, fb->depth_texture,
                                               fb->depth_texture_level, fb->depth_texture_layer,
                                               fb->depth_texture_target, texture->native_name);
    }
    if (fb->stencil_texture != 0u) {
        texture = mgl_texture_find(ctx, fb->stencil_texture);
        if (texture != NULL)
            refresh_one_framebuffer_attachment(ctx, GL_STENCIL_ATTACHMENT, fb->stencil_texture,
                                               fb->stencil_texture_level, fb->stencil_texture_layer,
                                               fb->stencil_texture_target, texture->native_name);
    }
    ctx->native_dispatch->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                              framebuffer_native_name(ctx, ctx->draw_framebuffer));
    ctx->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER,
                                              framebuffer_native_name(ctx, ctx->read_framebuffer));
    return 1;
}

void mgl_texture_refresh_current_framebuffers(struct mgl_context *ctx) {
    struct mgl_framebuffer *draw;
    struct mgl_framebuffer *read;
    if (ctx == NULL)
        return;
    draw = mgl_framebuffer_find(ctx, ctx->draw_framebuffer);
    read = mgl_framebuffer_find(ctx, ctx->read_framebuffer);
    if (draw != NULL)
        (void)mgl_texture_refresh_framebuffer_attachments(ctx, draw);
    if (read != NULL && read != draw)
        (void)mgl_texture_refresh_framebuffer_attachments(ctx, read);
}

int mgl_texture_recreate_native(struct mgl_context *ctx, struct mgl_texture *obj) {
    struct mgl_framebuffer *fb;
    GLuint old_name;
    GLuint new_name = 0u;
    if (ctx == NULL || obj == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesGenTextures == NULL ||
        ctx->native_dispatch->glesDeleteTextures == NULL ||
        ctx->native_dispatch->glesActiveTexture == NULL ||
        ctx->native_dispatch->glesBindTexture == NULL)
        return 0;
    mgl_texture_mark_all_bindings_dirty(ctx);
    for (fb = ctx->framebuffers; fb != NULL; fb = fb->next) {
        if (!can_refresh_framebuffer_texture(ctx, fb, obj->name))
            return 0;
    }
    ctx->native_dispatch->glesGenTextures(1, &new_name);
    if (new_name == 0u)
        return 0;
    old_name = obj->native_name;
    obj->native_name = new_name;
    obj->rectangle_native_initialized = GL_FALSE;
    obj->realized = GL_TRUE;
    mgl_texture_realize_bindings(ctx);
    refresh_framebuffers_for_texture(ctx, obj);
    if (old_name != 0u)
        ctx->native_dispatch->glesDeleteTextures(1, &old_name);
    return 1;
}

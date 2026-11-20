#include "mgl/gl.h"

#include "core/context.h"
#include "objects/sampler.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Check Begin/End legality before resolving sampler names or parameters. */
static int reject_during_begin_end(struct mgl_context *ctx) {
    if (ctx == NULL || !ctx->immediate.active)
        return 0;
    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    return 1;
}

static GLuint next_sampler_name(struct mgl_share_group *group) {
    GLuint name;
    do {
        name =
            (GLuint)atomic_fetch_add_explicit(&group->next_sampler_name, 1u, memory_order_relaxed);
    } while (name == 0u);
    return name;
}

static GLenum native_wrap(GLint wrap) {
    if (wrap == (GLint)GL_CLAMP || wrap == (GLint)GL_CLAMP_TO_BORDER)
        return GL_CLAMP_TO_BORDER;
    return (GLenum)wrap;
}

struct mgl_sampler *mgl_sampler_find(struct mgl_context *ctx, GLuint name) {
    struct mgl_sampler *it;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return NULL;
    for (it = ctx->share_group->samplers; it != NULL; it = it->next) {
        if (it->name == name)
            return it;
    }
    return NULL;
}

static void native_initialize_sampler(struct mgl_context *ctx, struct mgl_sampler *s) {
    if (ctx == NULL || s == NULL || s->native_name == 0u || ctx->native_dispatch == NULL)
        return;
    if (ctx->native_dispatch->glesSamplerParameteri != NULL) {
        ctx->native_dispatch->glesSamplerParameteri(s->native_name, GL_TEXTURE_MIN_FILTER,
                                                    s->min_filter);
        ctx->native_dispatch->glesSamplerParameteri(s->native_name, GL_TEXTURE_MAG_FILTER,
                                                    s->mag_filter);
        ctx->native_dispatch->glesSamplerParameteri(s->native_name, GL_TEXTURE_WRAP_S,
                                                    (GLint)native_wrap(s->wrap_s));
        ctx->native_dispatch->glesSamplerParameteri(s->native_name, GL_TEXTURE_WRAP_T,
                                                    (GLint)native_wrap(s->wrap_t));
        ctx->native_dispatch->glesSamplerParameteri(s->native_name, GL_TEXTURE_WRAP_R,
                                                    (GLint)native_wrap(s->wrap_r));
        ctx->native_dispatch->glesSamplerParameteri(s->native_name, GL_TEXTURE_COMPARE_MODE,
                                                    s->compare_mode);
        ctx->native_dispatch->glesSamplerParameteri(s->native_name, GL_TEXTURE_COMPARE_FUNC,
                                                    s->compare_func);
    }
    if (ctx->native_dispatch->glesSamplerParameterf != NULL) {
        if (mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                   "GL_EXT_texture_filter_anisotropic"))
            ctx->native_dispatch->glesSamplerParameterf(
                s->native_name, GL_TEXTURE_MAX_ANISOTROPY_EXT, s->max_anisotropy);
        ctx->native_dispatch->glesSamplerParameterf(s->native_name, GL_TEXTURE_MIN_LOD, s->min_lod);
        ctx->native_dispatch->glesSamplerParameterf(s->native_name, GL_TEXTURE_MAX_LOD, s->max_lod);
    }
    if (s->border_color_type == GL_INT && ctx->native_dispatch->glesSamplerParameterIiv != NULL)
        ctx->native_dispatch->glesSamplerParameterIiv(s->native_name, GL_TEXTURE_BORDER_COLOR,
                                                      s->border_color_i);
    else if (s->border_color_type == GL_UNSIGNED_INT &&
             ctx->native_dispatch->glesSamplerParameterIuiv != NULL)
        ctx->native_dispatch->glesSamplerParameterIuiv(s->native_name, GL_TEXTURE_BORDER_COLOR,
                                                       s->border_color_ui);
    else if (s->border_color_type == GL_FLOAT &&
             ctx->native_dispatch->glesSamplerParameterfv != NULL)
        ctx->native_dispatch->glesSamplerParameterfv(s->native_name, GL_TEXTURE_BORDER_COLOR,
                                                     s->border_color);
}

struct mgl_sampler *mgl_sampler_create(struct mgl_context *ctx, GLuint name) {
    struct mgl_sampler *s;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return NULL;
    s = mgl_sampler_find(ctx, name);
    if (s != NULL)
        return s;
    s = calloc(1, sizeof(*s));
    if (s == NULL)
        return NULL;
    s->name = name;
    s->min_filter = (GLint)GL_NEAREST_MIPMAP_LINEAR;
    s->mag_filter = (GLint)GL_LINEAR;
    s->wrap_s = (GLint)GL_REPEAT;
    s->wrap_t = (GLint)GL_REPEAT;
    s->wrap_r = (GLint)GL_REPEAT;
    s->min_lod = -1000.0f;
    s->max_lod = 1000.0f;
    s->max_anisotropy = 1.0f;
    s->compare_mode = (GLint)GL_NONE;
    s->compare_func = (GLint)GL_LEQUAL;
    s->border_color_type = GL_FLOAT;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGenSamplers != NULL) {
        ctx->native_dispatch->glesGenSamplers(1, &s->native_name);
        if (s->native_name != 0u) {
            s->realized = GL_TRUE;
            native_initialize_sampler(ctx, s);
        }
    }
    s->next = ctx->share_group->samplers;
    ctx->share_group->samplers = s;
    return s;
}

void mgl_sampler_remove(struct mgl_context *ctx, GLuint name) {
    struct mgl_sampler **link;
    unsigned unit;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return;
    link = &ctx->share_group->samplers;
    while (*link != NULL) {
        if ((*link)->name == name) {
            struct mgl_sampler *victim = *link;
            *link = victim->next;
            for (unit = 0u; unit < mgl_programmable_texture_limit(ctx); ++unit) {
                if (ctx->sampler_binding[unit] == name) {
                    ctx->sampler_binding[unit] = 0u;
                    if (ctx->native_dispatch != NULL &&
                        ctx->native_dispatch->glesBindSampler != NULL)
                        ctx->native_dispatch->glesBindSampler(unit, 0u);
                }
            }
            if (victim->native_name != 0u && ctx->native_dispatch != NULL &&
                ctx->native_dispatch->glesDeleteSamplers != NULL)
                ctx->native_dispatch->glesDeleteSamplers(1, &victim->native_name);
            free(victim);
            return;
        }
        link = &(*link)->next;
    }
}

void mgl_sampler_realize_unit(struct mgl_context *ctx, GLuint unit,
                              const struct mgl_texture *texture) {
    struct mgl_sampler *s;
    GLenum target;
    GLint min_filter;
    GLint mag_filter;
    GLint wrap_s;
    GLint wrap_t;
    GLint wrap_r;
    GLint compare_mode;
    GLint compare_func;
    GLfloat min_lod;
    GLfloat max_lod;
    GLfloat max_anisotropy;
    if (ctx == NULL || ctx->native_dispatch == NULL || unit >= mgl_programmable_texture_limit(ctx))
        return;
    s = mgl_sampler_find(ctx, ctx->sampler_binding[unit]);
    if (ctx->native_dispatch->glesBindSampler != NULL) {
        ctx->native_dispatch->glesBindSampler(unit, s != NULL ? s->native_name : 0u);
        return;
    }
    /* The GLES2 sampler fallback applies parameters to the bound texture target; multisample
     * textures have no sampler state. */
    if (ctx->native_dispatch->glesTexParameteri == NULL || texture == NULL)
        return;
    target = texture->target == GL_TEXTURE_1D || texture->target == GL_TEXTURE_RECTANGLE
                 ? GL_TEXTURE_2D
                 : texture->target;
    if (target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY ||
        target == 0u)
        return;
    min_filter = s != NULL ? s->min_filter : texture->min_filter;
    mag_filter = s != NULL ? s->mag_filter : texture->mag_filter;
    wrap_s = s != NULL ? s->wrap_s : texture->wrap_s;
    wrap_t = s != NULL ? s->wrap_t : texture->wrap_t;
    wrap_r = s != NULL ? s->wrap_r : texture->wrap_r;
    compare_mode = s != NULL ? s->compare_mode : texture->compare_mode;
    compare_func = s != NULL ? s->compare_func : texture->compare_func;
    min_lod = s != NULL ? s->min_lod : texture->min_lod;
    max_lod = s != NULL ? s->max_lod : texture->max_lod;
    max_anisotropy = s != NULL ? s->max_anisotropy : texture->max_anisotropy;

    ctx->native_dispatch->glesTexParameteri(target, GL_TEXTURE_MIN_FILTER, min_filter);
    ctx->native_dispatch->glesTexParameteri(target, GL_TEXTURE_MAG_FILTER, mag_filter);
    ctx->native_dispatch->glesTexParameteri(target, GL_TEXTURE_WRAP_S, (GLint)native_wrap(wrap_s));
    ctx->native_dispatch->glesTexParameteri(target, GL_TEXTURE_WRAP_T, (GLint)native_wrap(wrap_t));

    /* These sampler parameters are part of GLES3-class texture state. Keep the
     * GLES2 fallback to the parameters that GLES2 can represent. */
    if (ctx->native_dispatch->glesTexImage3D != NULL) {
        ctx->native_dispatch->glesTexParameteri(target, GL_TEXTURE_WRAP_R,
                                                (GLint)native_wrap(wrap_r));
        ctx->native_dispatch->glesTexParameteri(target, GL_TEXTURE_COMPARE_MODE, compare_mode);
        ctx->native_dispatch->glesTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, compare_func);
        if (ctx->native_dispatch->glesTexParameterf != NULL) {
            ctx->native_dispatch->glesTexParameterf(target, GL_TEXTURE_MIN_LOD, min_lod);
            ctx->native_dispatch->glesTexParameterf(target, GL_TEXTURE_MAX_LOD, max_lod);
            if (mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                       "GL_EXT_texture_filter_anisotropic"))
                ctx->native_dispatch->glesTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                                        max_anisotropy);
        }
    }
    if (s != NULL && s->border_color_type == GL_INT &&
        ctx->native_dispatch->glesTexParameterIiv != NULL)
        ctx->native_dispatch->glesTexParameterIiv(target, GL_TEXTURE_BORDER_COLOR,
                                                  s->border_color_i);
    else if (s != NULL && s->border_color_type == GL_UNSIGNED_INT &&
             ctx->native_dispatch->glesTexParameterIuiv != NULL)
        ctx->native_dispatch->glesTexParameterIuiv(target, GL_TEXTURE_BORDER_COLOR,
                                                   s->border_color_ui);
    else if (ctx->native_dispatch->glesTexParameterfv != NULL)
        ctx->native_dispatch->glesTexParameterfv(
            target, GL_TEXTURE_BORDER_COLOR, s != NULL ? s->border_color : texture->border_color);
}

static int valid_min_filter(GLint v) {
    return v == (GLint)GL_NEAREST || v == (GLint)GL_LINEAR ||
           v == (GLint)GL_NEAREST_MIPMAP_NEAREST || v == (GLint)GL_LINEAR_MIPMAP_NEAREST ||
           v == (GLint)GL_NEAREST_MIPMAP_LINEAR || v == (GLint)GL_LINEAR_MIPMAP_LINEAR;
}

static int valid_mag_filter(GLint v) {
    return v == (GLint)GL_NEAREST || v == (GLint)GL_LINEAR;
}

static int valid_wrap(GLint v) {
    return v == (GLint)GL_REPEAT || v == (GLint)GL_MIRRORED_REPEAT ||
           v == (GLint)GL_CLAMP_TO_EDGE || v == (GLint)GL_CLAMP_TO_BORDER || v == (GLint)GL_CLAMP;
}

static int valid_compare_func(GLint v) {
    return v == (GLint)GL_NEVER || v == (GLint)GL_LESS || v == (GLint)GL_EQUAL ||
           v == (GLint)GL_LEQUAL || v == (GLint)GL_GREATER || v == (GLint)GL_NOTEQUAL ||
           v == (GLint)GL_GEQUAL || v == (GLint)GL_ALWAYS;
}

static int sampler_anisotropy_validate(struct mgl_context *ctx, GLenum pname, GLfloat value) {
    if (pname != GL_TEXTURE_MAX_ANISOTROPY_EXT)
        return 1;
    if (ctx->native_dispatch == NULL ||
        !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                "GL_EXT_texture_filter_anisotropic")) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (!(value >= 1.0f)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    return 1;
}

static int sampler_set_float(struct mgl_context *ctx, struct mgl_sampler *s, GLenum pname,
                             const GLfloat *params) {
    GLint iv;
    if (params == NULL)
        return 0;
    iv = (GLint)params[0];
    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
            if (!valid_min_filter(iv))
                return 0;
            s->min_filter = iv;
            break;
        case GL_TEXTURE_MAG_FILTER:
            if (!valid_mag_filter(iv))
                return 0;
            s->mag_filter = iv;
            break;
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
        case GL_TEXTURE_WRAP_R:
            if (!valid_wrap(iv))
                return 0;
            if (pname == GL_TEXTURE_WRAP_S)
                s->wrap_s = iv;
            else if (pname == GL_TEXTURE_WRAP_T)
                s->wrap_t = iv;
            else
                s->wrap_r = iv;
            break;
        case GL_TEXTURE_MIN_LOD:
            s->min_lod = params[0];
            break;
        case GL_TEXTURE_MAX_LOD:
            s->max_lod = params[0];
            break;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT: {
            GLfloat maximum = params[0];
            if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetFloatv != NULL) {
                ctx->native_dispatch->glesGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximum);
                if (maximum > params[0])
                    maximum = params[0];
            }
            s->max_anisotropy = maximum;
            break;
        }
        case GL_TEXTURE_COMPARE_MODE:
            if (iv != (GLint)GL_NONE && iv != (GLint)GL_COMPARE_REF_TO_TEXTURE)
                return 0;
            s->compare_mode = iv;
            break;
        case GL_TEXTURE_COMPARE_FUNC:
            if (!valid_compare_func(iv))
                return 0;
            s->compare_func = iv;
            break;
        case GL_TEXTURE_BORDER_COLOR:
            memcpy(s->border_color, params, sizeof(s->border_color));
            s->border_color_type = GL_FLOAT;
            break;
        default:
            return 0;
    }
    (void)ctx;
    return 1;
}

static int sampler_get_float(const struct mgl_sampler *s, GLenum pname, GLfloat *params) {
    if (s == NULL || params == NULL)
        return 0;
    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
            params[0] = (GLfloat)s->min_filter;
            break;
        case GL_TEXTURE_MAG_FILTER:
            params[0] = (GLfloat)s->mag_filter;
            break;
        case GL_TEXTURE_WRAP_S:
            params[0] = (GLfloat)s->wrap_s;
            break;
        case GL_TEXTURE_WRAP_T:
            params[0] = (GLfloat)s->wrap_t;
            break;
        case GL_TEXTURE_WRAP_R:
            params[0] = (GLfloat)s->wrap_r;
            break;
        case GL_TEXTURE_MIN_LOD:
            params[0] = s->min_lod;
            break;
        case GL_TEXTURE_MAX_LOD:
            params[0] = s->max_lod;
            break;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
            params[0] = s->max_anisotropy;
            break;
        case GL_TEXTURE_COMPARE_MODE:
            params[0] = (GLfloat)s->compare_mode;
            break;
        case GL_TEXTURE_COMPARE_FUNC:
            params[0] = (GLfloat)s->compare_func;
            break;
        case GL_TEXTURE_BORDER_COLOR:
            memcpy(params, s->border_color, sizeof(s->border_color));
            break;
        default:
            return 0;
    }
    return 1;
}

static struct mgl_sampler *require_sampler(struct mgl_context *ctx, GLuint sampler) {
    struct mgl_sampler *s = mgl_sampler_find(ctx, sampler);
    if (s == NULL)
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    return s;
}

/* Native GLES sampler objects use independent sampler bindings. The GLES2 fallback stores
 * sampling parameters on texture objects. */
static void mark_sampler_fallback_dirty(struct mgl_context *ctx) {
    if (ctx != NULL &&
        (ctx->native_dispatch == NULL || ctx->native_dispatch->glesBindSampler == NULL))
        mgl_texture_mark_all_bindings_dirty(ctx);
}

void glGenSamplers(GLsizei count, GLuint *samplers) {
    struct mgl_context *ctx = mgl_context_current();
    mark_sampler_fallback_dirty(ctx);
    GLsizei i;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (count == 0 || samplers == NULL)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < count; ++i) {
        GLuint name = next_sampler_name(ctx->share_group);
        struct mgl_sampler *s = mgl_sampler_create(ctx, name);
        samplers[i] = s != NULL ? name : 0u;
        if (s == NULL)
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glCreateSamplers(GLsizei count, GLuint *samplers) {
    glGenSamplers(count, samplers);
}

void glDeleteSamplers(GLsizei count, const GLuint *samplers) {
    struct mgl_context *ctx = mgl_context_current();
    mark_sampler_fallback_dirty(ctx);
    GLsizei i;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (count == 0 || samplers == NULL)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < count; ++i)
        if (samplers[i] != 0u)
            mgl_sampler_remove(ctx, samplers[i]);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

GLboolean glIsSampler(GLuint sampler) {
    struct mgl_context *ctx = mgl_context_current();
    GLboolean out;
    if (ctx == NULL)
        return GL_FALSE;
    if (reject_during_begin_end(ctx))
        return GL_FALSE;
    if (sampler == 0u)
        return GL_FALSE;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    out = mgl_sampler_find(ctx, sampler) != NULL ? GL_TRUE : GL_FALSE;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return out;
}

void glBindSampler(GLuint unit, GLuint sampler) {
    struct mgl_context *ctx = mgl_context_current();
    mark_sampler_fallback_dirty(ctx);
    struct mgl_sampler *s = NULL;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (unit >= mgl_programmable_texture_limit(ctx)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (sampler != 0u) {
        s = mgl_sampler_find(ctx, sampler);
        if (s == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
    }
    ctx->sampler_binding[unit] = sampler;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindSampler != NULL)
        ctx->native_dispatch->glesBindSampler(unit, s != NULL ? s->native_name : 0u);
}

void glBindSamplers(GLuint first, GLsizei count, const GLuint *samplers) {
    struct mgl_context *ctx = mgl_context_current();
    mark_sampler_fallback_dirty(ctx);
    GLsizei i;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (first > mgl_programmable_texture_limit(ctx) ||
        (GLuint)count > mgl_programmable_texture_limit(ctx) - first) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    for (i = 0; i < count; ++i)
        glBindSampler(first + (GLuint)i, samplers != NULL ? samplers[i] : 0u);
}

void glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param) {
    struct mgl_context *ctx = mgl_context_current();
    mark_sampler_fallback_dirty(ctx);
    struct mgl_sampler *s;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (!sampler_anisotropy_validate(ctx, pname, (GLfloat)param))
        return;
    s = require_sampler(ctx, sampler);
    if (s == NULL)
        return;
    if (pname == GL_TEXTURE_BORDER_COLOR || !sampler_set_float(ctx, s, pname, &param)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (s->native_name != 0u && ctx->native_dispatch != NULL) {
        if ((pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T ||
             pname == GL_TEXTURE_WRAP_R) &&
            ctx->native_dispatch->glesSamplerParameteri != NULL)
            ctx->native_dispatch->glesSamplerParameteri(s->native_name, pname,
                                                        (GLint)native_wrap((GLint)param));
        else if (ctx->native_dispatch->glesSamplerParameterf != NULL)
            ctx->native_dispatch->glesSamplerParameterf(s->native_name, pname, param);
    }
}

void glSamplerParameteri(GLuint sampler, GLenum pname, GLint param) {
    struct mgl_context *ctx = mgl_context_current();
    mark_sampler_fallback_dirty(ctx);
    struct mgl_sampler *s;
    GLfloat f = (GLfloat)param;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (!sampler_anisotropy_validate(ctx, pname, (GLfloat)param))
        return;
    s = require_sampler(ctx, sampler);
    if (s == NULL)
        return;
    if (pname == GL_TEXTURE_BORDER_COLOR || !sampler_set_float(ctx, s, pname, &f)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (s->native_name != 0u && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesSamplerParameteri != NULL)
        ctx->native_dispatch->glesSamplerParameteri(
            s->native_name, pname,
            (pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_R)
                ? (GLint)native_wrap(param)
                : param);
}

void glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mark_sampler_fallback_dirty(ctx);
    struct mgl_sampler *s;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (params == NULL)
        return;
    if (!sampler_anisotropy_validate(ctx, pname, (GLfloat)params[0]))
        return;
    s = require_sampler(ctx, sampler);
    if (s == NULL)
        return;
    if (!sampler_set_float(ctx, s, pname, params)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_R) {
        if (s->native_name != 0u && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesSamplerParameteri != NULL)
            ctx->native_dispatch->glesSamplerParameteri(s->native_name, pname,
                                                        (GLint)native_wrap((GLint)params[0]));
    } else if (s->native_name != 0u && ctx->native_dispatch != NULL &&
               ctx->native_dispatch->glesSamplerParameterfv != NULL)
        ctx->native_dispatch->glesSamplerParameterfv(s->native_name, pname, params);
    else if (pname != GL_TEXTURE_BORDER_COLOR)
        glSamplerParameterf(sampler, pname, params[0]);
}

void glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mark_sampler_fallback_dirty(ctx);
    GLfloat f[4];
    unsigned i, n = pname == GL_TEXTURE_BORDER_COLOR ? 4u : 1u;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (params == NULL)
        return;
    if (!sampler_anisotropy_validate(ctx, pname, (GLfloat)params[0]))
        return;
    for (i = 0u; i < n; ++i)
        f[i] = pname == GL_TEXTURE_BORDER_COLOR ? (GLfloat)params[i] / 2147483647.0f
                                                : (GLfloat)params[i];
    glSamplerParameterfv(sampler, pname, f);
}

void glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mark_sampler_fallback_dirty(ctx);
    struct mgl_sampler *s;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (params == NULL)
        return;
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glSamplerParameteriv(sampler, pname, params);
        return;
    }
    s = require_sampler(ctx, sampler);
    if (s == NULL)
        return;
    memcpy(s->border_color_i, params, sizeof(s->border_color_i));
    s->border_color_type = GL_INT;
    if (s->native_name != 0u && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesSamplerParameterIiv != NULL)
        ctx->native_dispatch->glesSamplerParameterIiv(s->native_name, pname, params);
}

void glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mark_sampler_fallback_dirty(ctx);
    struct mgl_sampler *s;
    GLint value;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (params == NULL)
        return;
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        value = (GLint)params[0];
        glSamplerParameteriv(sampler, pname, &value);
        return;
    }
    s = require_sampler(ctx, sampler);
    if (s == NULL)
        return;
    memcpy(s->border_color_ui, params, sizeof(s->border_color_ui));
    s->border_color_type = GL_UNSIGNED_INT;
    if (s->native_name != 0u && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesSamplerParameterIuiv != NULL)
        ctx->native_dispatch->glesSamplerParameterIuiv(s->native_name, pname, params);
}

void glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_sampler *s;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (params == NULL)
        return;
    if (!sampler_anisotropy_validate(ctx, pname, 1.0f))
        return;
    s = require_sampler(ctx, sampler);
    if (s == NULL)
        return;
    if (!sampler_get_float(s, pname, params))
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
}

static GLint sampler_lod_to_int(GLfloat value) {
    if (value >= (GLfloat)INT_MAX)
        return INT_MAX;
    if (value <= (GLfloat)INT_MIN)
        return INT_MIN;
    return (GLint)roundf(value);
}

void glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_sampler *s;
    GLfloat f[4];
    unsigned i;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (params == NULL)
        return;
    if (!sampler_anisotropy_validate(ctx, pname, 1.0f))
        return;
    s = require_sampler(ctx, sampler);
    if (s == NULL)
        return;
    if (!sampler_get_float(s, pname, f)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (pname == GL_TEXTURE_BORDER_COLOR) {
        for (i = 0u; i < 4u; ++i) {
            GLfloat v = f[i] < -1.0f ? -1.0f : (f[i] > 1.0f ? 1.0f : f[i]);
            params[i] = (GLint)(v * 2147483647.0f);
        }
    } else if (pname == GL_TEXTURE_MIN_LOD || pname == GL_TEXTURE_MAX_LOD) {
        params[0] = sampler_lod_to_int(f[0]);
    } else
        params[0] = (GLint)f[0];
}

void glGetSamplerParameterIiv(GLuint sampler, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_sampler *s;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (params == NULL)
        return;
    s = require_sampler(ctx, sampler);
    if (s == NULL)
        return;
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        glGetSamplerParameteriv(sampler, pname, params);
        return;
    }
    if (s->border_color_type == GL_INT)
        memcpy(params, s->border_color_i, sizeof(s->border_color_i));
    else if (s->border_color_type == GL_UNSIGNED_INT) {
        unsigned i;
        for (i = 0u; i < 4u; ++i)
            params[i] = (GLint)s->border_color_ui[i];
    } else {
        unsigned i;
        for (i = 0u; i < 4u; ++i)
            params[i] = (GLint)s->border_color[i];
    }
}

void glGetSamplerParameterIuiv(GLuint sampler, GLenum pname, GLuint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_sampler *s;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (params == NULL)
        return;
    s = require_sampler(ctx, sampler);
    if (s == NULL)
        return;
    if (pname != GL_TEXTURE_BORDER_COLOR) {
        GLint value = 0;
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetSamplerParameteriv(sampler, pname, &value);
        if (ctx->errors.sequence == error_sequence_before)
            params[0] = (GLuint)value;
        return;
    }
    if (s->border_color_type == GL_UNSIGNED_INT)
        memcpy(params, s->border_color_ui, sizeof(s->border_color_ui));
    else if (s->border_color_type == GL_INT) {
        unsigned i;
        for (i = 0u; i < 4u; ++i)
            params[i] = (GLuint)s->border_color_i[i];
    } else {
        unsigned i;
        for (i = 0u; i < 4u; ++i)
            params[i] = s->border_color[i] <= 0.0f ? 0u : (GLuint)s->border_color[i];
    }
}

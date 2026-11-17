#include "compat/fixed_function.h"
#include "core/render_diag.h"
#include "mgl/gl.h"

#include "core/context.h"
#include "shader/shader_store.h"
#include "texture/texture_store.h"
#include "shader/translator.h"
#include "list/display_list.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

static void mat_identity(GLfloat m[16]) {
    memset(m, 0, 16u * sizeof(*m));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat_mul(GLfloat out[16], const GLfloat a[16], const GLfloat b[16]) {
    GLfloat tmp[16];
    unsigned col;
    unsigned row;
    for (col = 0u; col < 4u; ++col) {
        for (row = 0u; row < 4u; ++row) {
            tmp[col * 4u + row] =
                a[0u * 4u + row] * b[col * 4u + 0u] + a[1u * 4u + row] * b[col * 4u + 1u] +
                a[2u * 4u + row] * b[col * 4u + 2u] + a[3u * 4u + row] * b[col * 4u + 3u];
        }
    }
    memcpy(out, tmp, sizeof(tmp));
}

static int mat_inverse4(const GLfloat m[16], GLfloat out[16]) {
    GLfloat a[4][8];
    unsigned r, c, k;
    for (r = 0u; r < 4u; ++r) {
        for (c = 0u; c < 4u; ++c)
            a[r][c] = m[c * 4u + r];
        for (c = 0u; c < 4u; ++c)
            a[r][4u + c] = r == c ? 1.0f : 0.0f;
    }
    for (c = 0u; c < 4u; ++c) {
        unsigned pivot = c;
        GLfloat best = fabsf(a[pivot][c]);
        for (r = c + 1u; r < 4u; ++r) {
            GLfloat v = fabsf(a[r][c]);
            if (v > best) {
                best = v;
                pivot = r;
            }
        }
        if (best < 1.0e-20f)
            return 0;
        if (pivot != c) {
            for (k = 0u; k < 8u; ++k) {
                GLfloat t = a[c][k];
                a[c][k] = a[pivot][k];
                a[pivot][k] = t;
            }
        }
        {
            GLfloat inv = 1.0f / a[c][c];
            for (k = 0u; k < 8u; ++k)
                a[c][k] *= inv;
        }
        for (r = 0u; r < 4u; ++r)
            if (r != c) {
                GLfloat f = a[r][c];
                for (k = 0u; k < 8u; ++k)
                    a[r][k] -= f * a[c][k];
            }
    }
    for (r = 0u; r < 4u; ++r)
        for (c = 0u; c < 4u; ++c)
            out[c * 4u + r] = a[r][4u + c];
    return 1;
}

static void transform_plane_inverse_transpose(const GLfloat matrix[16], const GLfloat in[4],
                                              GLfloat out[4]) {
    GLfloat inv[16];
    unsigned r;
    if (!mat_inverse4(matrix, inv)) {
        memcpy(out, in, 4u * sizeof(*out));
        return;
    }
    for (r = 0u; r < 4u; ++r)
        out[r] = inv[r * 4u + 0u] * in[0] + inv[r * 4u + 1u] * in[1] + inv[r * 4u + 2u] * in[2] +
                 inv[r * 4u + 3u] * in[3];
}

static GLfloat *current_matrix(struct mgl_compat_state *s) {
    if (s->matrix_mode == GL_COLOR)
        return s->color_matrix[s->color_matrix_top];
    if (s->matrix_mode == GL_MODELVIEW)
        return s->modelview[s->modelview_top];
    if (s->matrix_mode == GL_PROJECTION)
        return s->projection[s->projection_top];
    return s->texture[s->active_texture][s->texture_top[s->active_texture]];
}

static unsigned *current_top(struct mgl_compat_state *s) {
    if (s->matrix_mode == GL_COLOR)
        return &s->color_matrix_top;
    if (s->matrix_mode == GL_MODELVIEW)
        return &s->modelview_top;
    if (s->matrix_mode == GL_PROJECTION)
        return &s->projection_top;
    return &s->texture_top[s->active_texture];
}

static unsigned current_capacity(const struct mgl_compat_state *s) {
    if (s->matrix_mode == GL_COLOR)
        return 4u;
    if (s->matrix_mode == GL_MODELVIEW)
        return MGL_MODELVIEW_STACK_DEPTH;
    if (s->matrix_mode == GL_PROJECTION)
        return MGL_PROJECTION_STACK_DEPTH;
    return MGL_TEXTURE_STACK_DEPTH;
}

static GLfloat *matrix_at(struct mgl_compat_state *s, unsigned top) {
    if (s->matrix_mode == GL_COLOR)
        return s->color_matrix[top];
    if (s->matrix_mode == GL_MODELVIEW)
        return s->modelview[top];
    if (s->matrix_mode == GL_PROJECTION)
        return s->projection[top];
    return s->texture[s->active_texture][top];
}

struct mgl_matrix_selection {
    GLenum mode;
    unsigned texture_unit;
};

static int resolve_matrix_selection(const struct mgl_compat_state *s, GLenum matrix_mode,
                                    struct mgl_matrix_selection *selection) {
    if (selection == NULL)
        return 0;
    if (matrix_mode == GL_COLOR || matrix_mode == GL_MODELVIEW || matrix_mode == GL_PROJECTION) {
        selection->mode = matrix_mode;
        selection->texture_unit = 0u;
        return 1;
    }
    if (matrix_mode == GL_TEXTURE) {
        if (s->active_texture >= MGL_TEXTURE_UNITS)
            return 0;
        selection->mode = GL_TEXTURE;
        selection->texture_unit = s->active_texture;
        return 1;
    }
    if (matrix_mode >= GL_TEXTURE0 && matrix_mode < GL_TEXTURE0 + MGL_TEXTURE_UNITS) {
        selection->mode = GL_TEXTURE;
        selection->texture_unit = (unsigned)(matrix_mode - GL_TEXTURE0);
        return 1;
    }
    return 0;
}

static GLfloat *selected_matrix(struct mgl_compat_state *s,
                                const struct mgl_matrix_selection *selection) {
    if (selection->mode == GL_COLOR)
        return s->color_matrix[s->color_matrix_top];
    if (selection->mode == GL_MODELVIEW)
        return s->modelview[s->modelview_top];
    if (selection->mode == GL_PROJECTION)
        return s->projection[s->projection_top];
    return s->texture[selection->texture_unit][s->texture_top[selection->texture_unit]];
}

static unsigned *selected_top(struct mgl_compat_state *s,
                              const struct mgl_matrix_selection *selection) {
    if (selection->mode == GL_COLOR)
        return &s->color_matrix_top;
    if (selection->mode == GL_MODELVIEW)
        return &s->modelview_top;
    if (selection->mode == GL_PROJECTION)
        return &s->projection_top;
    return &s->texture_top[selection->texture_unit];
}

static unsigned selected_capacity(const struct mgl_matrix_selection *selection) {
    if (selection->mode == GL_COLOR)
        return 4u;
    if (selection->mode == GL_MODELVIEW)
        return MGL_MODELVIEW_STACK_DEPTH;
    if (selection->mode == GL_PROJECTION)
        return MGL_PROJECTION_STACK_DEPTH;
    return MGL_TEXTURE_STACK_DEPTH;
}

static GLfloat *selected_matrix_at(struct mgl_compat_state *s,
                                   const struct mgl_matrix_selection *selection, unsigned top) {
    if (selection->mode == GL_COLOR)
        return s->color_matrix[top];
    if (selection->mode == GL_MODELVIEW)
        return s->modelview[top];
    if (selection->mode == GL_PROJECTION)
        return s->projection[top];
    return s->texture[selection->texture_unit][top];
}

static void transpose_matrix(GLfloat out[16], const GLfloat in[16]) {
    unsigned row;
    unsigned col;
    for (col = 0u; col < 4u; ++col)
        for (row = 0u; row < 4u; ++row)
            out[col * 4u + row] = in[row * 4u + col];
}

void mgl_compat_state_init(struct mgl_compat_state *s) {
    unsigned unit;
    memset(s, 0, sizeof(*s));
    s->matrix_mode = GL_MODELVIEW;
    s->fog_mode = GL_EXP;
    s->fog_density = 1.0f;
    s->fog_start = 0.0f;
    s->fog_end = 1.0f;
    s->fog_index = 0.0f;
    s->fog_coord_source = GL_FRAGMENT_DEPTH;
    s->color_material_face = GL_FRONT_AND_BACK;
    s->color_material_mode = GL_AMBIENT_AND_DIFFUSE;
    s->shade_model = GL_SMOOTH;
    s->light_model_ambient[0] = 0.2f;
    s->light_model_ambient[1] = 0.2f;
    s->light_model_ambient[2] = 0.2f;
    s->light_model_ambient[3] = 1.0f;
    s->material_front.ambient[0] = s->material_front.ambient[1] = s->material_front.ambient[2] =
        0.2f;
    s->material_front.ambient[3] = 1.0f;
    s->material_front.diffuse[0] = s->material_front.diffuse[1] = s->material_front.diffuse[2] =
        0.8f;
    s->material_front.diffuse[3] = 1.0f;
    s->material_back = s->material_front;
    for (unit = 0u; unit < MGL_MAX_LIGHTS; ++unit) {
        struct mgl_light_state *l = &s->lights[unit];
        l->position[2] = 1.0f;
        l->spot_direction[2] = -1.0f;
        l->spot_cutoff = 180.0f;
        l->constant_attenuation = 1.0f;
        if (unit == 0u) {
            l->diffuse[0] = l->diffuse[1] = l->diffuse[2] = l->diffuse[3] = 1.0f;
            l->specular[0] = l->specular[1] = l->specular[2] = l->specular[3] = 1.0f;
        }
    }
    mat_identity(s->color_matrix[0]);
    mat_identity(s->modelview[0]);
    mat_identity(s->projection[0]);
    for (unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit) {
        unsigned coord;
        mat_identity(s->texture[unit][0]);
        s->texture_env_mode[unit] = GL_MODULATE;
        {
            struct mgl_texenv_combine_state *env = &s->texture_env_combine[unit];
            unsigned arg;
            env->rgb_mode = GL_MODULATE;
            env->alpha_mode = GL_MODULATE;
            env->rgb_scale = env->alpha_scale = 1.0f;
            env->rgb_source[0] = env->alpha_source[0] = GL_TEXTURE;
            env->rgb_source[1] = env->alpha_source[1] = GL_PREVIOUS;
            env->rgb_source[2] = env->alpha_source[2] = GL_CONSTANT;
            for (arg = 0u; arg < 3u; ++arg) {
                env->rgb_operand[arg] = GL_SRC_COLOR;
                env->alpha_operand[arg] = GL_SRC_ALPHA;
            }
        }
        for (coord = 0u; coord < MGL_TEXGEN_COORDS; ++coord) {
            struct mgl_texgen_state *tg = &s->texgen[unit][coord];
            tg->mode = GL_EYE_LINEAR;
            tg->object_plane[coord] = 1.0f;
            tg->eye_plane[coord] = 1.0f;
        }
    }
}

const GLfloat *mgl_compat_modelview(const struct mgl_compat_state *s) {
    return s->modelview[s->modelview_top];
}

const GLfloat *mgl_compat_projection(const struct mgl_compat_state *s) {
    return s->projection[s->projection_top];
}

const GLfloat *mgl_compat_texture_matrix(const struct mgl_compat_state *s, unsigned unit) {
    return unit < MGL_TEXTURE_UNITS ? s->texture[unit][s->texture_top[unit]] : NULL;
}

void mgl_compat_mvp(const struct mgl_compat_state *s, GLfloat out[16]) {
    mat_mul(out, mgl_compat_projection(s), mgl_compat_modelview(s));
}

void mgl_compat_normal_matrix(const struct mgl_compat_state *s, GLfloat out[9]) {
    const GLfloat *m = mgl_compat_modelview(s);
    GLfloat a00 = m[0], a01 = m[4], a02 = m[8];
    GLfloat a10 = m[1], a11 = m[5], a12 = m[9];
    GLfloat a20 = m[2], a21 = m[6], a22 = m[10];
    GLfloat c00 = a11 * a22 - a12 * a21;
    GLfloat c01 = a12 * a20 - a10 * a22;
    GLfloat c02 = a10 * a21 - a11 * a20;
    GLfloat det = a00 * c00 + a01 * c01 + a02 * c02;
    if (fabsf(det) < 1.0e-20f) {
        memset(out, 0, 9u * sizeof(*out));
        out[0] = out[4] = out[8] = 1.0f;
        return;
    }
    {
        GLfloat inv = 1.0f / det;

        out[0] = c00 * inv;
        out[1] = (a02 * a21 - a01 * a22) * inv;
        out[2] = (a01 * a12 - a02 * a11) * inv;
        out[3] = c01 * inv;
        out[4] = (a00 * a22 - a02 * a20) * inv;
        out[5] = (a02 * a10 - a00 * a12) * inv;
        out[6] = c02 * inv;
        out[7] = (a01 * a20 - a00 * a21) * inv;
        out[8] = (a00 * a11 - a01 * a10) * inv;
    }
}

static int valid_matrix_mode(GLenum mode) {
    return mode == GL_COLOR || mode == GL_MODELVIEW || mode == GL_PROJECTION || mode == GL_TEXTURE;
}

void glActiveTexture(GLenum texture) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_TEXTURE_STATE);
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_ACTIVE_TEXTURE);
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glActiveTexture",
                        (const struct mgl_error_arg[]){{"texture", (uint64_t)(uintptr_t)texture}},
                        1u);
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        /* GL_COMPILE defers an illegal command within Begin/End to replay.
         * COMPILE_AND_EXECUTE both records it and reports the immediate error. */
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_active_texture(ctx, texture))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture < GL_TEXTURE0 || texture >= GL_TEXTURE0 + mgl_programmable_texture_limit(ctx)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_active_texture(ctx, texture))
        return;
    ctx->compat.active_texture = texture - GL_TEXTURE0;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesActiveTexture != NULL) {
        mgl_native_select_active_texture(ctx, texture);
    }
}

static int mgl_fog_reject_inside_begin(struct mgl_context *ctx) {
    if (ctx != NULL && ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 1;
    }
    return 0;
}

static GLfloat mgl_signed_int_to_color(GLint value) {
    const double scale = 2.0 * (double)INT_MAX + 1.0;
    return (GLfloat)((2.0 * (double)value + 1.0) / scale);
}

static int valid_texture_env_mode(GLenum mode) {
    return mode == GL_MODULATE || mode == GL_REPLACE || mode == GL_DECAL || mode == GL_ADD ||
           mode == GL_BLEND || mode == GL_COMBINE;
}

/* Invalid TexEnv arguments must not partially update canonical state. */
static int mgl_texenv_set(struct mgl_context *ctx, unsigned unit, GLenum pname,
                          const GLfloat *values) {
    struct mgl_texenv_combine_state *env = &ctx->compat.texture_env_combine[unit];
    GLenum value = (GLenum)(GLint)values[0];
    GLenum *slot = NULL;
    unsigned arg;
    if (pname == GL_TEXTURE_LOD_BIAS) {
        ctx->compat.texture_lod_bias[unit] = values[0];
        return 1;
    }
    if (pname == GL_TEXTURE_ENV_MODE) {
        if (!valid_texture_env_mode(value))
            goto invalid;
        if (ctx->compat.texture_env_mode[unit] != value)
            ctx->compat.fixed_variant_key_valid = GL_FALSE;
        ctx->compat.texture_env_mode[unit] = value;
        return 1;
    }
    if (pname == GL_TEXTURE_ENV_COLOR) {
        for (arg = 0; arg < 4; ++arg)
            env->color[arg] = fminf(1.0f, fmaxf(0.0f, values[arg]));
        return 1;
    }
    if (pname == GL_RGB_SCALE || pname == GL_ALPHA_SCALE) {
        if (values[0] != 1.0f && values[0] != 2.0f && values[0] != 4.0f)
            goto invalid;
        if (pname == GL_RGB_SCALE)
            env->rgb_scale = values[0];
        else
            env->alpha_scale = values[0];
        return 1;
    }
    if (pname == GL_COMBINE_RGB) {
        if (value != GL_REPLACE && value != GL_MODULATE && value != GL_ADD &&
            value != GL_ADD_SIGNED && value != GL_INTERPOLATE && value != GL_SUBTRACT &&
            value != GL_DOT3_RGB && value != GL_DOT3_RGBA)
            goto invalid;
        env->rgb_mode = value;
        return 1;
    }
    if (pname == GL_COMBINE_ALPHA) {
        if (value != GL_REPLACE && value != GL_MODULATE && value != GL_ADD &&
            value != GL_ADD_SIGNED && value != GL_INTERPOLATE && value != GL_SUBTRACT)
            goto invalid;
        env->alpha_mode = value;
        return 1;
    }
    if (pname >= GL_SOURCE0_RGB && pname <= GL_SOURCE2_RGB)
        slot = &env->rgb_source[pname - GL_SOURCE0_RGB];
    else if (pname >= GL_SOURCE0_ALPHA && pname <= GL_SOURCE2_ALPHA)
        slot = &env->alpha_source[pname - GL_SOURCE0_ALPHA];
    else if (pname >= GL_OPERAND0_RGB && pname <= GL_OPERAND2_RGB) {
        if (value != GL_SRC_COLOR && value != GL_ONE_MINUS_SRC_COLOR && value != GL_SRC_ALPHA &&
            value != GL_ONE_MINUS_SRC_ALPHA)
            goto invalid;
        env->rgb_operand[pname - GL_OPERAND0_RGB] = value;
        return 1;
    } else if (pname >= GL_OPERAND0_ALPHA && pname <= GL_OPERAND2_ALPHA) {
        if (value != GL_SRC_ALPHA && value != GL_ONE_MINUS_SRC_ALPHA)
            goto invalid;
        env->alpha_operand[pname - GL_OPERAND0_ALPHA] = value;
        return 1;
    }
    if (slot != NULL && (value == GL_TEXTURE || value == GL_CONSTANT || value == GL_PRIMARY_COLOR ||
                         value == GL_PREVIOUS ||
                         (value >= GL_TEXTURE0 && value < GL_TEXTURE0 + MGL_TEXTURE_UNITS))) {
        *slot = value;
        return 1;
    }
invalid:
    mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
    return 0;
}

static int mgl_texenv_get(const struct mgl_context *ctx, unsigned unit, GLenum pname,
                          GLfloat *values) {
    const struct mgl_texenv_combine_state *env = &ctx->compat.texture_env_combine[unit];
    if (pname == GL_TEXTURE_LOD_BIAS)
        values[0] = ctx->compat.texture_lod_bias[unit];
    else if (pname == GL_TEXTURE_ENV_MODE)
        values[0] = (GLfloat)ctx->compat.texture_env_mode[unit];
    else if (pname == GL_TEXTURE_ENV_COLOR)
        memcpy(values, env->color, sizeof(env->color));
    else if (pname == GL_RGB_SCALE)
        values[0] = env->rgb_scale;
    else if (pname == GL_ALPHA_SCALE)
        values[0] = env->alpha_scale;
    else if (pname == GL_COMBINE_RGB)
        values[0] = (GLfloat)env->rgb_mode;
    else if (pname == GL_COMBINE_ALPHA)
        values[0] = (GLfloat)env->alpha_mode;
    else if (pname >= GL_SOURCE0_RGB && pname <= GL_SOURCE2_RGB)
        values[0] = (GLfloat)env->rgb_source[pname - GL_SOURCE0_RGB];
    else if (pname >= GL_SOURCE0_ALPHA && pname <= GL_SOURCE2_ALPHA)
        values[0] = (GLfloat)env->alpha_source[pname - GL_SOURCE0_ALPHA];
    else if (pname >= GL_OPERAND0_RGB && pname <= GL_OPERAND2_RGB)
        values[0] = (GLfloat)env->rgb_operand[pname - GL_OPERAND0_RGB];
    else if (pname >= GL_OPERAND0_ALPHA && pname <= GL_OPERAND2_ALPHA)
        values[0] = (GLfloat)env->alpha_operand[pname - GL_OPERAND0_ALPHA];
    else
        return 0;
    return 1;
}

void glFogf(GLenum pname, GLfloat param) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glFogf",
                        (const struct mgl_error_arg[]){{"pname", (uint64_t)(uintptr_t)pname}}, 1u);
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_fog(ctx, pname, &param, 1u))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    switch (pname) {
        case GL_FOG_MODE:
            if (param != (GLfloat)GL_LINEAR && param != (GLfloat)GL_EXP &&
                param != (GLfloat)GL_EXP2) {
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return;
            }
            if (mgl_list_record_fog(ctx, pname, &param, 1u))
                return;
            ctx->compat.fog_mode = (GLenum)(GLint)param;
            return;
        case GL_FOG_DENSITY:
            if (param < 0.0f) {
                mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
                return;
            }
            if (mgl_list_record_fog(ctx, pname, &param, 1u))
                return;
            ctx->compat.fog_density = param;
            return;
        case GL_FOG_START:
            if (mgl_list_record_fog(ctx, pname, &param, 1u))
                return;
            ctx->compat.fog_start = param;
            return;
        case GL_FOG_END:
            if (mgl_list_record_fog(ctx, pname, &param, 1u))
                return;
            ctx->compat.fog_end = param;
            return;
        case GL_FOG_INDEX:
            if (mgl_list_record_fog(ctx, pname, &param, 1u))
                return;
            ctx->compat.fog_index = param;
            return;
        case GL_FOG_COORD_SRC: {
            GLenum source;
            if (param != (GLfloat)GL_FRAGMENT_DEPTH && param != (GLfloat)GL_FOG_COORD) {
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return;
            }
            source = (GLenum)(GLint)param;
            if (mgl_list_record_fog(ctx, pname, &param, 1u))
                return;
            ctx->compat.fog_coord_source = source;
            return;
        }
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
    }
}

void glFogi(GLenum pname, GLint param) {
    if (pname == GL_FOG_MODE)
        glFogf(pname, (GLfloat)param);
    else
        glFogf(pname, (GLfloat)param);
}

void glFogfv(GLenum pname, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_fog(ctx, pname, params, pname == GL_FOG_COLOR ? 4u : 1u))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pname == GL_FOG_COLOR) {
        unsigned i;
        if (mgl_list_record_fog(ctx, pname, params, 4u))
            return;
        for (i = 0u; i < 4u; ++i) {
            GLfloat value = params[i];
            ctx->compat.fog_color[i] = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        }
        return;
    }
    glFogf(pname, params[0]);
}

void glFogiv(GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || params == NULL)
        return;
    if (pname == GL_FOG_COLOR) {
        GLfloat values[4];
        unsigned i;
        for (i = 0u; i < 4u; ++i)
            values[i] = mgl_signed_int_to_color(params[i]);
        glFogfv(pname, values);
        return;
    }
    glFogi(pname, params[0]);
}

void glFogCoordf(GLfloat coord) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (mgl_list_record_fog_coord(ctx, coord))
        return;
    ctx->compat_current[MGL_COMPAT_SEM_FOG][0] = coord;
}

void glFogCoordfv(const GLfloat *coord) {
    if (coord != NULL)
        glFogCoordf(coord[0]);
}

void glFogCoordd(GLdouble coord) {
    glFogCoordf((GLfloat)coord);
}

void glFogCoorddv(const GLdouble *coord) {
    if (coord != NULL)
        glFogCoordd(coord[0]);
}

static int mgl_ext_texunit_index(struct mgl_context *ctx, GLenum texunit, unsigned *unit) {
    if (ctx == NULL || unit == NULL)
        return 0;
    if (texunit < GL_TEXTURE0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    *unit = (unsigned)(texunit - GL_TEXTURE0);
    if (*unit >= MGL_TEXTURE_UNITS) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    return 1;
}

void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || params == NULL)
        return;
    mgl_error_trace_api(&ctx->errors, "glTexEnvfv", NULL, 0u);
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_tex_env(ctx, target, pname, params))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target == GL_TEXTURE_FILTER_CONTROL) {
        if (pname != GL_TEXTURE_LOD_BIAS) {
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
        }
    } else if (target != GL_TEXTURE_ENV) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    } else if (pname == GL_TEXTURE_LOD_BIAS) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (ctx->compat.active_texture >= MGL_TEXTURE_UNITS) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_tex_env(ctx, target, pname, params))
        return;
    (void)mgl_texenv_set(ctx, ctx->compat.active_texture, pname, params);
}

void glTexEnvi(GLenum target, GLenum pname, GLint param) {
    GLfloat value = (GLfloat)param;
    if (pname == GL_TEXTURE_ENV_COLOR) {
        struct mgl_context *ctx = mgl_context_current();
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_COMPAT_STATE);
        if (ctx != NULL)
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    glTexEnvfv(target, pname, &value);
}

void glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
    if (pname == GL_TEXTURE_ENV_COLOR) {
        struct mgl_context *ctx = mgl_context_current();
        if (ctx != NULL)
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    glTexEnvfv(target, pname, &param);
}

void glTexEnviv(GLenum target, GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat data[4];
    unsigned i;
    if (ctx == NULL || params == NULL)
        return;
    if (pname == GL_TEXTURE_ENV_COLOR) {
        for (i = 0; i < 4; ++i)
            data[i] = mgl_signed_int_to_color(params[i]);
        glTexEnvfv(target, pname, data);
    } else {
        data[0] = (GLfloat)params[0];
        glTexEnvfv(target, pname, data);
    }
}

void glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || params == NULL)
        return;
    if (mgl_fog_reject_inside_begin(ctx))
        return;
    if (ctx->compat.active_texture >= MGL_TEXTURE_UNITS) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!((target == GL_TEXTURE_FILTER_CONTROL && pname == GL_TEXTURE_LOD_BIAS) ||
          (target == GL_TEXTURE_ENV && pname != GL_TEXTURE_LOD_BIAS)) ||
        !mgl_texenv_get(ctx, ctx->compat.active_texture, pname, params))
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
}

void glGetTexEnviv(GLenum target, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat data[4];
    unsigned i;
    if (ctx == NULL || params == NULL)
        return;
    if (mgl_fog_reject_inside_begin(ctx))
        return;
    if (ctx->compat.active_texture >= MGL_TEXTURE_UNITS) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!((target == GL_TEXTURE_FILTER_CONTROL && pname == GL_TEXTURE_LOD_BIAS) ||
          (target == GL_TEXTURE_ENV && pname != GL_TEXTURE_LOD_BIAS)) ||
        !mgl_texenv_get(ctx, ctx->compat.active_texture, pname, data)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (pname == GL_TEXTURE_ENV_COLOR)
        for (i = 0; i < 4; ++i)
            params[i] = (GLint)(data[i] * (GLfloat)INT_MAX);
    else
        params[0] = (GLint)data[0];
}

void glMultiTexEnvfvEXT(GLenum texunit, GLenum target, GLenum pname, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned unit;
    if (ctx == NULL || params == NULL)
        return;
    if (mgl_fog_reject_inside_begin(ctx))
        return;
    if (!mgl_ext_texunit_index(ctx, texunit, &unit))
        return;
    if (target == GL_TEXTURE_FILTER_CONTROL) {
        if (pname != GL_TEXTURE_LOD_BIAS) {
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
        }
    } else if (target != GL_TEXTURE_ENV) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    } else if (pname == GL_TEXTURE_LOD_BIAS) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_multi_tex_env_ext(ctx, texunit, target, pname, params))
        return;
    (void)mgl_texenv_set(ctx, unit, pname, params);
}

void glMultiTexEnviEXT(GLenum texunit, GLenum target, GLenum pname, GLint param) {
    GLfloat value = (GLfloat)param;
    if (pname == GL_TEXTURE_ENV_COLOR) {
        struct mgl_context *ctx = mgl_context_current();
        if (ctx != NULL)
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    glMultiTexEnvfvEXT(texunit, target, pname, &value);
}

void glMultiTexEnvfEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat param) {
    if (pname == GL_TEXTURE_ENV_COLOR) {
        struct mgl_context *ctx = mgl_context_current();
        if (ctx != NULL)
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    glMultiTexEnvfvEXT(texunit, target, pname, &param);
}

void glMultiTexEnvivEXT(GLenum texunit, GLenum target, GLenum pname, const GLint *params) {
    GLfloat data[4];
    unsigned i;
    if (params == NULL)
        return;
    if (pname == GL_TEXTURE_ENV_COLOR) {
        for (i = 0; i < 4; ++i)
            data[i] = mgl_signed_int_to_color(params[i]);
    } else
        data[0] = (GLfloat)params[0];
    glMultiTexEnvfvEXT(texunit, target, pname, data);
}

void glGetMultiTexEnvfvEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned unit;
    if (ctx == NULL || params == NULL)
        return;
    if (mgl_fog_reject_inside_begin(ctx))
        return;
    if (!mgl_ext_texunit_index(ctx, texunit, &unit))
        return;
    if (target != GL_TEXTURE_ENV || !mgl_texenv_get(ctx, unit, pname, params))
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
}

void glGetMultiTexEnvivEXT(GLenum texunit, GLenum target, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned unit, i;
    GLfloat data[4];
    if (ctx == NULL || params == NULL)
        return;
    if (mgl_fog_reject_inside_begin(ctx))
        return;
    if (!mgl_ext_texunit_index(ctx, texunit, &unit))
        return;
    if (target != GL_TEXTURE_ENV || !mgl_texenv_get(ctx, unit, pname, data)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (pname == GL_TEXTURE_ENV_COLOR)
        for (i = 0; i < 4; ++i)
            params[i] = (GLint)(data[i] * (GLfloat)INT_MAX);
    else
        params[0] = (GLint)data[0];
}

static int matrix_reject_inside_begin(struct mgl_context *ctx) {
    if (ctx != NULL && ctx->compat.matrix_mode == GL_TEXTURE &&
        ctx->compat.active_texture >= MGL_TEXTURE_UNITS) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 1;
    }
    if (ctx != NULL && ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 1;
    }
    return 0;
}

void glMatrixMode(GLenum mode) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_MATRIX);
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glMatrixMode",
                        (const struct mgl_error_arg[]){{"mode", (uint64_t)(uintptr_t)mode}}, 1u);
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_matrix_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_matrix_mode(ctx, mode))
        return;
    ctx->compat.matrix_mode = mode;
}

void glLoadIdentity(void) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_MATRIX);
    if (ctx == NULL)
        return;
    if (matrix_reject_inside_begin(ctx))
        return;
    if (mgl_list_record_load_identity(ctx))
        return;
    mat_identity(current_matrix(&ctx->compat));
    if (ctx->compat.matrix_mode == GL_TEXTURE)
        ctx->compat.fixed_variant_key_valid = GL_FALSE;
}

void glLoadMatrixf(const GLfloat *m) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || matrix_reject_inside_begin(ctx))
        return;
    if (m == NULL)
        return;
    if (mgl_list_record_load_matrixf(ctx, m))
        return;
    memcpy(current_matrix(&ctx->compat), m, 16u * sizeof(*m));
    if (ctx->compat.matrix_mode == GL_TEXTURE)
        ctx->compat.fixed_variant_key_valid = GL_FALSE;
}

void glLoadMatrixd(const GLdouble *m) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned i;
    GLfloat tmp[16];
    if (ctx == NULL || matrix_reject_inside_begin(ctx))
        return;
    if (m == NULL)
        return;
    for (i = 0u; i < 16u; ++i)
        tmp[i] = (GLfloat)m[i];
    glLoadMatrixf(tmp);
}

void glMultMatrixf(const GLfloat *m) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_MATRIX);
    if (ctx == NULL || matrix_reject_inside_begin(ctx))
        return;
    if (m == NULL)
        return;
    if (mgl_list_record_mult_matrixf(ctx, m))
        return;
    mat_mul(current_matrix(&ctx->compat), current_matrix(&ctx->compat), m);
    if (ctx->compat.matrix_mode == GL_TEXTURE)
        ctx->compat.fixed_variant_key_valid = GL_FALSE;
}

void glMultMatrixd(const GLdouble *m) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned i;
    GLfloat tmp[16];
    if (ctx == NULL || matrix_reject_inside_begin(ctx))
        return;
    if (m == NULL)
        return;
    for (i = 0u; i < 16u; ++i)
        tmp[i] = (GLfloat)m[i];
    glMultMatrixf(tmp);
}

void glLoadTransposeMatrixf(const GLfloat *m) {
    GLfloat tmp[16];
    if (m == NULL)
        return;
    transpose_matrix(tmp, m);
    glLoadMatrixf(tmp);
}

void glLoadTransposeMatrixd(const GLdouble *m) {
    GLfloat src[16];
    GLfloat tmp[16];
    unsigned i;
    if (m == NULL)
        return;
    for (i = 0u; i < 16u; ++i)
        src[i] = (GLfloat)m[i];
    transpose_matrix(tmp, src);
    glLoadMatrixf(tmp);
}

void glMultTransposeMatrixf(const GLfloat *m) {
    GLfloat tmp[16];
    if (m == NULL)
        return;
    transpose_matrix(tmp, m);
    glMultMatrixf(tmp);
}

void glMultTransposeMatrixd(const GLdouble *m) {
    GLfloat src[16];
    GLfloat tmp[16];
    unsigned i;
    if (m == NULL)
        return;
    for (i = 0u; i < 16u; ++i)
        src[i] = (GLfloat)m[i];
    transpose_matrix(tmp, src);
    glMultMatrixf(tmp);
}

static int matrix_ext_selection(struct mgl_context *ctx, GLenum matrix_mode,
                                struct mgl_matrix_selection *selection) {
    if (matrix_reject_inside_begin(ctx))
        return 0;
    if (!resolve_matrix_selection(&ctx->compat, matrix_mode, selection)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    return 1;
}

void glMatrixLoadfEXT(GLenum matrixMode, const GLfloat *m) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_matrix_selection selection;
    if (ctx == NULL || m == NULL)
        return;
    if (!matrix_ext_selection(ctx, matrixMode, &selection))
        return;
    if (mgl_list_record_matrix_load_ext(ctx, matrixMode, m))
        return;
    memcpy(selected_matrix(&ctx->compat, &selection), m, 16u * sizeof(*m));
    if (selection.mode == GL_TEXTURE)
        ctx->compat.fixed_variant_key_valid = GL_FALSE;
}

void glMatrixLoaddEXT(GLenum matrixMode, const GLdouble *m) {
    GLfloat tmp[16];
    unsigned i;
    if (m == NULL)
        return;
    for (i = 0u; i < 16u; ++i)
        tmp[i] = (GLfloat)m[i];
    glMatrixLoadfEXT(matrixMode, tmp);
}

void glMatrixMultfEXT(GLenum matrixMode, const GLfloat *m) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_matrix_selection selection;
    GLfloat *dst;
    if (ctx == NULL || m == NULL)
        return;
    if (!matrix_ext_selection(ctx, matrixMode, &selection))
        return;
    if (mgl_list_record_matrix_mult_ext(ctx, matrixMode, m))
        return;
    dst = selected_matrix(&ctx->compat, &selection);
    mat_mul(dst, dst, m);
    if (selection.mode == GL_TEXTURE)
        ctx->compat.fixed_variant_key_valid = GL_FALSE;
}

void glMatrixMultdEXT(GLenum matrixMode, const GLdouble *m) {
    GLfloat tmp[16];
    unsigned i;
    if (m == NULL)
        return;
    for (i = 0u; i < 16u; ++i)
        tmp[i] = (GLfloat)m[i];
    glMatrixMultfEXT(matrixMode, tmp);
}

void glMatrixLoadIdentityEXT(GLenum matrixMode) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_matrix_selection selection;
    if (ctx == NULL)
        return;
    if (!matrix_ext_selection(ctx, matrixMode, &selection))
        return;
    if (mgl_list_record_matrix_load_identity_ext(ctx, matrixMode))
        return;
    mat_identity(selected_matrix(&ctx->compat, &selection));
    if (selection.mode == GL_TEXTURE)
        ctx->compat.fixed_variant_key_valid = GL_FALSE;
}

void glMatrixPushEXT(GLenum matrixMode) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_matrix_selection selection;
    unsigned *top;
    unsigned next;
    if (ctx == NULL)
        return;
    if (!matrix_ext_selection(ctx, matrixMode, &selection))
        return;
    if (mgl_list_record_matrix_push_ext(ctx, matrixMode))
        return;
    top = selected_top(&ctx->compat, &selection);
    next = *top + 1u;
    if (next >= selected_capacity(&selection)) {
        mgl_error_set(&ctx->errors, GL_STACK_OVERFLOW);
        return;
    }
    memcpy(selected_matrix_at(&ctx->compat, &selection, next),
           selected_matrix_at(&ctx->compat, &selection, *top), 16u * sizeof(GLfloat));
    *top = next;
    if (selection.mode == GL_TEXTURE)
        ctx->compat.fixed_variant_key_valid = GL_FALSE;
}

void glMatrixPopEXT(GLenum matrixMode) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_matrix_selection selection;
    unsigned *top;
    if (ctx == NULL)
        return;
    if (!matrix_ext_selection(ctx, matrixMode, &selection))
        return;
    if (mgl_list_record_matrix_pop_ext(ctx, matrixMode))
        return;
    top = selected_top(&ctx->compat, &selection);
    if (*top == 0u) {
        mgl_error_set(&ctx->errors, GL_STACK_UNDERFLOW);
        return;
    }
    --*top;
    if (selection.mode == GL_TEXTURE)
        ctx->compat.fixed_variant_key_valid = GL_FALSE;
}

void glMatrixLoadTransposefEXT(GLenum matrixMode, const GLfloat *m) {
    GLfloat tmp[16];
    if (m == NULL)
        return;
    transpose_matrix(tmp, m);
    glMatrixLoadfEXT(matrixMode, tmp);
}

void glMatrixLoadTransposedEXT(GLenum matrixMode, const GLdouble *m) {
    GLfloat src[16];
    GLfloat tmp[16];
    unsigned i;
    if (m == NULL)
        return;
    for (i = 0u; i < 16u; ++i)
        src[i] = (GLfloat)m[i];
    transpose_matrix(tmp, src);
    glMatrixLoadfEXT(matrixMode, tmp);
}

void glMatrixMultTransposefEXT(GLenum matrixMode, const GLfloat *m) {
    GLfloat tmp[16];
    if (m == NULL)
        return;
    transpose_matrix(tmp, m);
    glMatrixMultfEXT(matrixMode, tmp);
}

void glMatrixMultTransposedEXT(GLenum matrixMode, const GLdouble *m) {
    GLfloat src[16];
    GLfloat tmp[16];
    unsigned i;
    if (m == NULL)
        return;
    for (i = 0u; i < 16u; ++i)
        src[i] = (GLfloat)m[i];
    transpose_matrix(tmp, src);
    glMatrixMultfEXT(matrixMode, tmp);
}

void glMatrixTranslatefEXT(GLenum matrixMode, GLfloat x, GLfloat y, GLfloat z) {
    GLfloat m[16];
    mat_identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
    glMatrixMultfEXT(matrixMode, m);
}

void glMatrixTranslatedEXT(GLenum matrixMode, GLdouble x, GLdouble y, GLdouble z) {
    glMatrixTranslatefEXT(matrixMode, (GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void glMatrixScalefEXT(GLenum matrixMode, GLfloat x, GLfloat y, GLfloat z) {
    GLfloat m[16];
    mat_identity(m);
    m[0] = x;
    m[5] = y;
    m[10] = z;
    glMatrixMultfEXT(matrixMode, m);
}

void glMatrixScaledEXT(GLenum matrixMode, GLdouble x, GLdouble y, GLdouble z) {
    glMatrixScalefEXT(matrixMode, (GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void glMatrixRotatefEXT(GLenum matrixMode, GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat len;
    if (ctx == NULL || matrix_reject_inside_begin(ctx))
        return;
    len = sqrtf(x * x + y * y + z * z);
    GLfloat m[16];
    GLfloat c, s, t;
    const GLfloat pi = 3.14159265358979323846f;
    if (len == 0.0f)
        return;
    x /= len;
    y /= len;
    z /= len;
    c = cosf(angle * pi / 180.0f);
    s = sinf(angle * pi / 180.0f);
    t = 1.0f - c;
    mat_identity(m);
    m[0] = t * x * x + c;
    m[4] = t * x * y - s * z;
    m[8] = t * x * z + s * y;
    m[1] = t * x * y + s * z;
    m[5] = t * y * y + c;
    m[9] = t * y * z - s * x;
    m[2] = t * x * z - s * y;
    m[6] = t * y * z + s * x;
    m[10] = t * z * z + c;
    glMatrixMultfEXT(matrixMode, m);
}

void glMatrixRotatedEXT(GLenum matrixMode, GLdouble angle, GLdouble x, GLdouble y, GLdouble z) {
    glMatrixRotatefEXT(matrixMode, (GLfloat)angle, (GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void glMatrixOrthoEXT(GLenum matrixMode, GLdouble left, GLdouble right, GLdouble bottom,
                      GLdouble top, GLdouble near_val, GLdouble far_val) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat m[16];
    if (ctx == NULL || matrix_reject_inside_begin(ctx))
        return;
    if (right == left || top == bottom || far_val == near_val) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    mat_identity(m);
    m[0] = (GLfloat)(2.0 / (right - left));
    m[5] = (GLfloat)(2.0 / (top - bottom));
    m[10] = (GLfloat)(-2.0 / (far_val - near_val));
    m[12] = (GLfloat)(-(right + left) / (right - left));
    m[13] = (GLfloat)(-(top + bottom) / (top - bottom));
    m[14] = (GLfloat)(-(far_val + near_val) / (far_val - near_val));
    glMatrixMultfEXT(matrixMode, m);
}

void glMatrixFrustumEXT(GLenum matrixMode, GLdouble left, GLdouble right, GLdouble bottom,
                        GLdouble top, GLdouble near_val, GLdouble far_val) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat m[16];
    if (ctx == NULL || matrix_reject_inside_begin(ctx))
        return;
    if (right == left || top == bottom || far_val == near_val || near_val <= 0.0 ||
        far_val <= 0.0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    memset(m, 0, sizeof(m));
    m[0] = (GLfloat)((2.0 * near_val) / (right - left));
    m[5] = (GLfloat)((2.0 * near_val) / (top - bottom));
    m[8] = (GLfloat)((right + left) / (right - left));
    m[9] = (GLfloat)((top + bottom) / (top - bottom));
    m[10] = (GLfloat)(-(far_val + near_val) / (far_val - near_val));
    m[11] = -1.0f;
    m[14] = (GLfloat)(-(2.0 * far_val * near_val) / (far_val - near_val));
    glMatrixMultfEXT(matrixMode, m);
}

void glPushMatrix(void) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned *top;
    unsigned next;
    if (ctx == NULL)
        return;
    if (matrix_reject_inside_begin(ctx))
        return;
    if (mgl_list_record_push_matrix(ctx))
        return;
    top = current_top(&ctx->compat);
    next = *top + 1u;
    if (next >= current_capacity(&ctx->compat)) {
        mgl_error_set(&ctx->errors, GL_STACK_OVERFLOW);
        return;
    }
    memcpy(matrix_at(&ctx->compat, next), matrix_at(&ctx->compat, *top), 16u * sizeof(GLfloat));
    *top = next;
    if (ctx->compat.matrix_mode == GL_TEXTURE)
        ctx->compat.fixed_variant_key_valid = GL_FALSE;
}

void glPopMatrix(void) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned *top;
    if (ctx == NULL)
        return;
    if (matrix_reject_inside_begin(ctx))
        return;
    if (mgl_list_record_pop_matrix(ctx))
        return;
    top = current_top(&ctx->compat);
    if (*top == 0u) {
        mgl_error_set(&ctx->errors, GL_STACK_UNDERFLOW);
        return;
    }
    --*top;
    if (ctx->compat.matrix_mode == GL_TEXTURE)
        ctx->compat.fixed_variant_key_valid = GL_FALSE;
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    GLfloat m[16];
    mat_identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
    glMultMatrixf(m);
}

void glTranslated(GLdouble x, GLdouble y, GLdouble z) {
    glTranslatef((GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z) {
    GLfloat m[16];
    mat_identity(m);
    m[0] = x;
    m[5] = y;
    m[10] = z;
    glMultMatrixf(m);
}

void glScaled(GLdouble x, GLdouble y, GLdouble z) {
    glScalef((GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat len, magnitude;
    GLfloat m[16];
    GLfloat c, s, t;
    const GLfloat pi = 3.14159265358979323846f;
    if (ctx == NULL || matrix_reject_inside_begin(ctx))
        return;

    magnitude = fmaxf(fmaxf(fabsf(x), fabsf(y)), fabsf(z));
    if (magnitude == 0.0f)
        return;
    if (isfinite(magnitude)) {
        x /= magnitude;
        y /= magnitude;
        z /= magnitude;
    }
    len = sqrtf(x * x + y * y + z * z);
    if (len == 0.0f)
        return;
    x /= len;
    y /= len;
    z /= len;
    c = cosf(angle * pi / 180.0f);
    s = sinf(angle * pi / 180.0f);
    t = 1.0f - c;
    mat_identity(m);
    m[0] = t * x * x + c;
    m[4] = t * x * y - s * z;
    m[8] = t * x * z + s * y;
    m[1] = t * x * y + s * z;
    m[5] = t * y * y + c;
    m[9] = t * y * z - s * x;
    m[2] = t * x * z - s * y;
    m[6] = t * y * z + s * x;
    m[10] = t * z * z + c;
    glMultMatrixf(m);
}

void glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z) {

    GLdouble magnitude = fmax(fmax(fabs(x), fabs(y)), fabs(z));
    if (magnitude > 0.0 && isfinite(magnitude)) {
        x /= magnitude;
        y /= magnitude;
        z /= magnitude;
    }
    glRotatef((GLfloat)angle, (GLfloat)x, (GLfloat)y, (GLfloat)z);
}

/* Begin/End errors precede projection value errors. */
void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val,
             GLdouble far_val) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat m[16];
    if (ctx == NULL || matrix_reject_inside_begin(ctx))
        return;
    if (right == left || top == bottom || far_val == near_val) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    mat_identity(m);
    m[0] = (GLfloat)(2.0 / (right - left));
    m[5] = (GLfloat)(2.0 / (top - bottom));
    m[10] = (GLfloat)(-2.0 / (far_val - near_val));
    m[12] = (GLfloat)(-(right + left) / (right - left));
    m[13] = (GLfloat)(-(top + bottom) / (top - bottom));
    m[14] = (GLfloat)(-(far_val + near_val) / (far_val - near_val));
    glMultMatrixf(m);
}

void glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near_val,
              GLfloat far_val) {
    glOrtho((GLdouble)left, (GLdouble)right, (GLdouble)bottom, (GLdouble)top, (GLdouble)near_val,
            (GLdouble)far_val);
}

void glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val,
               GLdouble far_val) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat m[16];
    if (ctx == NULL || matrix_reject_inside_begin(ctx))
        return;
    if (right == left || top == bottom || far_val == near_val || near_val <= 0.0 ||
        far_val <= 0.0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    memset(m, 0, sizeof(m));
    m[0] = (GLfloat)((2.0 * near_val) / (right - left));
    m[5] = (GLfloat)((2.0 * near_val) / (top - bottom));
    m[8] = (GLfloat)((right + left) / (right - left));
    m[9] = (GLfloat)((top + bottom) / (top - bottom));
    m[10] = (GLfloat)(-(far_val + near_val) / (far_val - near_val));
    m[11] = -1.0f;
    m[14] = (GLfloat)(-(2.0 * far_val * near_val) / (far_val - near_val));
    glMultMatrixf(m);
}

void glFrustumf(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near_val,
                GLfloat far_val) {
    glFrustum((GLdouble)left, (GLdouble)right, (GLdouble)bottom, (GLdouble)top, (GLdouble)near_val,
              (GLdouble)far_val);
}

void glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    glColor4f(r, g, b, 1.0f);
}

void glColor3fv(const GLfloat *v) {
    if (v != NULL)
        glColor3f(v[0], v[1], v[2]);
}

void glColor3ubv(const GLubyte *v) {
    if (v != NULL)
        glColor4f((GLfloat)v[0] / 255.0f, (GLfloat)v[1] / 255.0f, (GLfloat)v[2] / 255.0f, 1.0f);
}

void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (mgl_list_record_color4f(ctx, r, g, b, a))
        return;
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][0] = r;
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][1] = g;
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][2] = b;
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][3] = a;
}

void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) {
    glColor4f((GLfloat)r / 255.0f, (GLfloat)g / 255.0f, (GLfloat)b / 255.0f, (GLfloat)a / 255.0f);
}

void glColor4ubv(const GLubyte *v) {
    if (v != NULL)
        glColor4ub(v[0], v[1], v[2], v[3]);
}

void glColor4fv(const GLfloat *v) {
    if (v != NULL)
        glColor4f(v[0], v[1], v[2], v[3]);
}

void glSecondaryColor3f(GLfloat r, GLfloat g, GLfloat b) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (mgl_list_record_secondary3f(ctx, r, g, b))
        return;
    ctx->compat_current[MGL_COMPAT_SEM_SECONDARY][0] = r;
    ctx->compat_current[MGL_COMPAT_SEM_SECONDARY][1] = g;
    ctx->compat_current[MGL_COMPAT_SEM_SECONDARY][2] = b;
    ctx->compat_current[MGL_COMPAT_SEM_SECONDARY][3] = 1.0f;
}

void glSecondaryColor3fv(const GLfloat *v) {
    if (v != NULL)
        glSecondaryColor3f(v[0], v[1], v[2]);
}

void glNormal3f(GLfloat x, GLfloat y, GLfloat z) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (mgl_list_record_normal3f(ctx, x, y, z))
        return;
    ctx->compat_current[MGL_COMPAT_SEM_NORMAL][0] = x;
    ctx->compat_current[MGL_COMPAT_SEM_NORMAL][1] = y;
    ctx->compat_current[MGL_COMPAT_SEM_NORMAL][2] = z;
    ctx->compat_current[MGL_COMPAT_SEM_NORMAL][3] = 1.0f;
}

void glNormal3fv(const GLfloat *v) {
    if (v != NULL)
        glNormal3f(v[0], v[1], v[2]);
}

static void texcoord4(unsigned unit, GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned semantic;
    if (ctx == NULL || unit >= MGL_TEXTURE_UNITS)
        return;
    if (mgl_list_record_texcoord4f(ctx, unit, s, t, r, q))
        return;
    semantic = MGL_COMPAT_SEM_TEXCOORD0 + unit;
    ctx->compat_current[semantic][0] = s;
    ctx->compat_current[semantic][1] = t;
    ctx->compat_current[semantic][2] = r;
    ctx->compat_current[semantic][3] = q;
}

void glTexCoord1f(GLfloat s) {
    texcoord4(0u, s, 0.0f, 0.0f, 1.0f);
}

void glTexCoord1fv(const GLfloat *v) {
    if (v != NULL)
        glTexCoord1f(v[0]);
}

void glTexCoord2f(GLfloat s, GLfloat t) {
    texcoord4(0u, s, t, 0.0f, 1.0f);
}

void glTexCoord2fv(const GLfloat *v) {
    if (v != NULL)
        glTexCoord2f(v[0], v[1]);
}

void glTexCoord2sv(const GLshort *v) {
    if (v != NULL)
        glTexCoord2f((GLfloat)v[0], (GLfloat)v[1]);
}

void glTexCoord3f(GLfloat s, GLfloat t, GLfloat r) {
    texcoord4(0u, s, t, r, 1.0f);
}

void glTexCoord3fv(const GLfloat *v) {
    if (v != NULL)
        glTexCoord3f(v[0], v[1], v[2]);
}

void glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    texcoord4(0u, s, t, r, q);
}

void glTexCoord4fv(const GLfloat *v) {
    if (v != NULL)
        glTexCoord4f(v[0], v[1], v[2], v[3]);
}

void glTexCoord4sv(const GLshort *v) {
    if (v != NULL)
        glTexCoord4f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

void glTexCoord1d(GLdouble s) {
    glTexCoord1f((GLfloat)s);
}

void glTexCoord1dv(const GLdouble *v) {
    if (v != NULL)
        glTexCoord1f((GLfloat)v[0]);
}

void glTexCoord1i(GLint s) {
    glTexCoord1f((GLfloat)s);
}

void glTexCoord1iv(const GLint *v) {
    if (v != NULL)
        glTexCoord1f((GLfloat)v[0]);
}

void glTexCoord1s(GLshort s) {
    glTexCoord1f((GLfloat)s);
}

void glTexCoord1sv(const GLshort *v) {
    if (v != NULL)
        glTexCoord1f((GLfloat)v[0]);
}

void glTexCoord2d(GLdouble s, GLdouble t) {
    glTexCoord2f((GLfloat)s, (GLfloat)t);
}

void glTexCoord2dv(const GLdouble *v) {
    if (v != NULL)
        glTexCoord2f((GLfloat)v[0], (GLfloat)v[1]);
}

void glTexCoord2i(GLint s, GLint t) {
    glTexCoord2f((GLfloat)s, (GLfloat)t);
}

void glTexCoord2iv(const GLint *v) {
    if (v != NULL)
        glTexCoord2f((GLfloat)v[0], (GLfloat)v[1]);
}

void glTexCoord2s(GLshort s, GLshort t) {
    glTexCoord2f((GLfloat)s, (GLfloat)t);
}

void glTexCoord3d(GLdouble s, GLdouble t, GLdouble r) {
    glTexCoord3f((GLfloat)s, (GLfloat)t, (GLfloat)r);
}

void glTexCoord3dv(const GLdouble *v) {
    if (v != NULL)
        glTexCoord3f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2]);
}

void glTexCoord3i(GLint s, GLint t, GLint r) {
    glTexCoord3f((GLfloat)s, (GLfloat)t, (GLfloat)r);
}

void glTexCoord3iv(const GLint *v) {
    if (v != NULL)
        glTexCoord3f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2]);
}

void glTexCoord3s(GLshort s, GLshort t, GLshort r) {
    glTexCoord3f((GLfloat)s, (GLfloat)t, (GLfloat)r);
}

void glTexCoord3sv(const GLshort *v) {
    if (v != NULL)
        glTexCoord3f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2]);
}

void glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q) {
    glTexCoord4f((GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q);
}

void glTexCoord4dv(const GLdouble *v) {
    if (v != NULL)
        glTexCoord4f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

void glTexCoord4i(GLint s, GLint t, GLint r, GLint q) {
    glTexCoord4f((GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q);
}

void glTexCoord4iv(const GLint *v) {
    if (v != NULL)
        glTexCoord4f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

void glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q) {
    glTexCoord4f((GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q);
}

void glMultiTexCoord1f(GLenum texture, GLfloat s) {
    glMultiTexCoord4f(texture, s, 0.0f, 0.0f, 1.0f);
}

void glMultiTexCoord1fv(GLenum texture, const GLfloat *v) {
    if (v != NULL)
        glMultiTexCoord1f(texture, v[0]);
}

void glMultiTexCoord2f(GLenum texture, GLfloat s, GLfloat t) {
    glMultiTexCoord4f(texture, s, t, 0.0f, 1.0f);
}

void glMultiTexCoord2fv(GLenum texture, const GLfloat *v) {
    if (v != NULL)
        glMultiTexCoord2f(texture, v[0], v[1]);
}

void glMultiTexCoord3f(GLenum texture, GLfloat s, GLfloat t, GLfloat r) {
    glMultiTexCoord4f(texture, s, t, r, 1.0f);
}

void glMultiTexCoord3fv(GLenum texture, const GLfloat *v) {
    if (v != NULL)
        glMultiTexCoord3f(texture, v[0], v[1], v[2]);
}

void glMultiTexCoord4f(GLenum texture, GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    /* Compatibility texture-coordinate current attributes use the selector low three bits to
     * address eight units. */
    texcoord4((unsigned)(texture & (MGL_TEXTURE_UNITS - 1u)), s, t, r, q);
}

void glMultiTexCoord4fv(GLenum texture, const GLfloat *v) {
    if (v != NULL)
        glMultiTexCoord4f(texture, v[0], v[1], v[2], v[3]);
}

/* Uniform-cache keys include each contributing fixed-function state value. */
struct mgl_compat_uniform_cache {
    struct mgl_compat_state compat;
    GLboolean alpha_test;
    GLenum alpha_func;
    GLfloat alpha_ref;
    GLboolean program_point_size_enabled;
    GLfloat point_size;
    GLboolean point_sprite_enabled;
    GLenum point_sprite_coord_origin;
    GLfloat effective_lod_bias[8];
    GLboolean lod_bias_valid;
    GLfloat rect_scale[8][2];
    GLboolean rect_scale_valid;
};

void mgl_compat_realize_program_uniforms(struct mgl_context *ctx, struct mgl_program *program) {
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_DRAW_UNIFORMS);
    struct mgl_compat_uniform_cache *cache;
    int first, matrices_dirty, alpha_dirty, point_dirty, texenv_dirty;
    int texgen_dirty, implicit_texgen_dirty, clip_dirty, fog_dirty, lights_dirty, material_dirty;
    int texture_matrices_used = 0;
    int texture_matrices_dirty = 0;
    unsigned texture_unit;
    const struct mgl_compat_state *old;
    const struct mgl_compat_state *now;
    GLfloat mvp[16];
    GLfloat normal[9];
    if (ctx == NULL || program == NULL || ctx->native_dispatch == NULL)
        return;
    for (texture_unit = 0u; texture_unit < MGL_TEXTURE_UNITS; ++texture_unit)
        if (program->compat_uniform_texture_matrix[texture_unit] >= 0) {
            texture_matrices_used = 1;
            break;
        }

    if (!texture_matrices_used && program->compat_uniform_model_view_projection < 0 &&
        program->compat_uniform_model_view < 0 && program->compat_uniform_projection < 0 &&
        program->compat_uniform_normal_matrix < 0 && program->compat_uniform_sampler0 < 0 &&
        program->compat_uniform_ff_texture_enabled < 0 &&
        program->compat_uniform_alpha_enabled < 0 && program->compat_uniform_alpha_func < 0 &&
        program->compat_uniform_alpha_ref < 0 && program->compat_uniform_point_coord_flip < 0 &&
        program->compat_uniform_point_size < 0 && program->compat_uniform_program_point_size < 0 &&
        program->compat_uniform_texture_env_mode0 < 0 && program->compat_env_enabled < 0 &&
        program->compat_uniform_texgen_enabled < 0 && program->compat_uniform_texgen_mode < 0 &&
        program->compat_uniform_texgen_object_plane < 0 &&
        program->compat_uniform_texgen_eye_plane < 0 && program->compat_uniform_clip_enabled < 0 &&
        program->compat_uniform_clip_plane < 0 && program->compat_uniform_fog_enabled < 0 &&
        program->compat_uniform_fog_mode < 0 && program->compat_uniform_fog_density < 0 &&
        program->compat_uniform_fog_start < 0 && program->compat_uniform_fog_end < 0 &&
        program->compat_uniform_fog_color < 0 && program->compat_uniform_fog_coord_explicit < 0 &&
        program->compat_uniform_normalize_enabled < 0 &&
        program->compat_uniform_lighting_enabled < 0 &&
        program->compat_uniform_implicit_texgen_enabled < 0 &&
        program->compat_uniform_implicit_texgen_mode < 0 &&
        program->compat_uniform_implicit_texgen_object_plane < 0 &&
        program->compat_uniform_implicit_texgen_eye_plane < 0 &&
        program->compat_uniform_light_enabled < 0 && program->compat_uniform_light_ambient < 0 &&
        program->compat_uniform_light_diffuse < 0 && program->compat_uniform_light_specular < 0 &&
        program->compat_uniform_light_position < 0 &&
        program->compat_uniform_light_spot_direction < 0 &&
        program->compat_uniform_light_spot_exponent < 0 &&
        program->compat_uniform_light_spot_cutoff < 0 &&
        program->compat_uniform_light_constant_attenuation < 0 &&
        program->compat_uniform_light_linear_attenuation < 0 &&
        program->compat_uniform_light_quadratic_attenuation < 0 &&
        program->compat_uniform_light_model_ambient < 0 &&
        program->compat_uniform_light_model_local_viewer < 0 &&
        program->compat_uniform_light_model_two_side < 0 &&
        program->compat_uniform_material_ambient < 0 &&
        program->compat_uniform_material_diffuse < 0 &&
        program->compat_uniform_material_specular < 0 &&
        program->compat_uniform_material_emission < 0 &&
        program->compat_uniform_material_shininess < 0 &&
        program->compat_uniform_material_back_ambient < 0 &&
        program->compat_uniform_material_back_diffuse < 0 &&
        program->compat_uniform_material_back_specular < 0 &&
        program->compat_uniform_material_back_emission < 0 &&
        program->compat_uniform_material_back_shininess < 0 &&
        program->compat_uniform_color_material_enabled < 0 &&
        program->compat_uniform_color_material_mode < 0 &&
        program->compat_uniform_color_material_face < 0)
        return;
    cache = program->compat_uniform_cache;
    if (cache == NULL) {
        cache = calloc(1u, sizeof(*cache));
        if (cache != NULL)
            program->compat_uniform_cache = cache;
    }
    /* Allocation failure skips the uniform-cache optimization. */
    first = cache == NULL || !program->compat_uniform_cache_valid;
    old = cache != NULL ? &cache->compat : NULL;
    now = &ctx->compat;
#define COMPAT_CHANGED(field) (first || memcmp(&old->field, &now->field, sizeof(now->field)) != 0)
    matrices_dirty =
        first ||
        memcmp(mgl_compat_modelview(old), mgl_compat_modelview(now), 16u * sizeof(GLfloat)) != 0 ||
        memcmp(mgl_compat_projection(old), mgl_compat_projection(now), 16u * sizeof(GLfloat)) != 0;
    if (texture_matrices_used)
        for (texture_unit = 0u; texture_unit < MGL_TEXTURE_UNITS; ++texture_unit)
            if (program->compat_uniform_texture_matrix[texture_unit] >= 0 &&
                (first || memcmp(mgl_compat_texture_matrix(old, texture_unit),
                                 mgl_compat_texture_matrix(now, texture_unit),
                                 16u * sizeof(GLfloat)) != 0)) {
                texture_matrices_dirty = 1;
                break;
            }
    alpha_dirty = first || cache->alpha_test != ctx->canonical_state.alpha_test ||
                  cache->alpha_func != ctx->canonical_state.alpha_func ||
                  cache->alpha_ref != ctx->canonical_state.alpha_ref;
    point_dirty =
        first ||
        cache->program_point_size_enabled != ctx->canonical_state.program_point_size_enabled ||
        cache->point_size != ctx->canonical_state.point_size ||
        cache->point_sprite_enabled != ctx->canonical_state.point_sprite_enabled ||
        cache->point_sprite_coord_origin != ctx->canonical_state.point_sprite_coord_origin;
    texenv_dirty = COMPAT_CHANGED(texture_env_mode) || COMPAT_CHANGED(texture_1d_enabled) ||
                   COMPAT_CHANGED(texture_2d_enabled) || COMPAT_CHANGED(texture_3d_enabled) ||
                   COMPAT_CHANGED(texture_cube_enabled) ||
                   COMPAT_CHANGED(texture_rectangle_enabled) || COMPAT_CHANGED(texture_env_combine);
    texgen_dirty = COMPAT_CHANGED(texgen[0]);
    implicit_texgen_dirty = (program->compat_uniform_implicit_texgen_enabled >= 0 ||
                             program->compat_uniform_implicit_texgen_mode >= 0 ||
                             program->compat_uniform_implicit_texgen_object_plane >= 0 ||
                             program->compat_uniform_implicit_texgen_eye_plane >= 0) &&
                            COMPAT_CHANGED(texgen);
    clip_dirty = COMPAT_CHANGED(clip_plane_enabled) || COMPAT_CHANGED(clip_plane);
    fog_dirty = COMPAT_CHANGED(fog_enabled) || COMPAT_CHANGED(fog_mode) ||
                COMPAT_CHANGED(fog_density) || COMPAT_CHANGED(fog_start) ||
                COMPAT_CHANGED(fog_end) || COMPAT_CHANGED(fog_color) ||
                COMPAT_CHANGED(fog_coord_source);
    lights_dirty =
        (program->compat_uniform_lighting_enabled >= 0 ||
         program->compat_uniform_light_enabled >= 0 || program->compat_uniform_light_ambient >= 0 ||
         program->compat_uniform_light_diffuse >= 0 ||
         program->compat_uniform_light_specular >= 0 ||
         program->compat_uniform_light_position >= 0 ||
         program->compat_uniform_light_spot_direction >= 0 ||
         program->compat_uniform_light_spot_exponent >= 0 ||
         program->compat_uniform_light_spot_cutoff >= 0 ||
         program->compat_uniform_light_constant_attenuation >= 0 ||
         program->compat_uniform_light_linear_attenuation >= 0 ||
         program->compat_uniform_light_quadratic_attenuation >= 0 ||
         program->compat_uniform_normalize_enabled >= 0 ||
         program->compat_uniform_light_model_ambient >= 0 ||
         program->compat_uniform_light_model_local_viewer >= 0 ||
         program->compat_uniform_light_model_two_side >= 0) &&
        (COMPAT_CHANGED(lighting_enabled) || COMPAT_CHANGED(lights) ||
         COMPAT_CHANGED(light_model_ambient) || COMPAT_CHANGED(light_model_local_viewer) ||
         COMPAT_CHANGED(light_model_two_side) || COMPAT_CHANGED(normalize_enabled) ||
         COMPAT_CHANGED(rescale_normal_enabled));
    material_dirty = COMPAT_CHANGED(material_front) || COMPAT_CHANGED(material_back) ||
                     COMPAT_CHANGED(color_material_enabled) ||
                     COMPAT_CHANGED(color_material_face) || COMPAT_CHANGED(color_material_mode);
#undef COMPAT_CHANGED
    if (program->compat_env_lod_bias >= 0 && ctx->native_dispatch->glesUniform1fv != NULL) {
        GLfloat biases[8];
        for (texture_unit = 0u; texture_unit < 8u; ++texture_unit) {
            struct mgl_texture *tex = mgl_texture_find(ctx, ctx->texture_2d_binding[texture_unit]);
            if (tex == NULL)
                tex = mgl_default_texture(ctx, GL_TEXTURE_2D);
            biases[texture_unit] =
                now->texture_lod_bias[texture_unit] + (tex != NULL ? tex->lod_bias : 0.0f);
        }
        if (cache == NULL || !cache->lod_bias_valid ||
            memcmp(cache->effective_lod_bias, biases, sizeof(biases)) != 0) {
            (ctx->native_dispatch->glesUniform1fv(program->compat_env_lod_bias, 8, biases),
             mgl_render_diag_native_check(ctx, "Uniform1fv:compat_env_lod_bias",
                                          (uint64_t)(program->compat_env_lod_bias),
                                          (uint64_t)program->native_name));
            if (cache != NULL) {
                memcpy(cache->effective_lod_bias, biases, sizeof(biases));
                cache->lod_bias_valid = GL_TRUE;
            }
        }
    }
    if (ctx->native_dispatch->glesUniform2f != NULL) {
        GLfloat scale[8][2];
        unsigned unit;
        for (unit = 0u; unit < 8u; ++unit) {
            const struct mgl_texture *tex = NULL;
            scale[unit][0] = 1.0f;
            scale[unit][1] = 1.0f;
            if (mgl_compat_selected_texture_target(now, unit) == GL_TEXTURE_RECTANGLE) {
                tex = mgl_texture_find(ctx, ctx->texture_rectangle_binding[unit]);
                if (tex == NULL)
                    tex = mgl_default_texture(ctx, GL_TEXTURE_RECTANGLE);
                if (tex != NULL && tex->levels[0].width > 0 && tex->levels[0].height > 0) {
                    scale[unit][0] = 1.0f / (GLfloat)tex->levels[0].width;
                    scale[unit][1] = 1.0f / (GLfloat)tex->levels[0].height;
                }
            }
            if (program->compat_uniform_rect_scale[unit] >= 0 &&
                (cache == NULL || !cache->rect_scale_valid ||
                 memcmp(cache->rect_scale[unit], scale[unit], sizeof(scale[unit])) != 0)) {
                ctx->native_dispatch->glesUniform2f(program->compat_uniform_rect_scale[unit],
                                                    scale[unit][0], scale[unit][1]);
                mgl_render_diag_native_check(ctx, "Uniform2f:compat_uniform_rect_scale",
                                             (uint64_t)program->compat_uniform_rect_scale[unit],
                                             (uint64_t)program->native_name);
            }
        }
        if (cache != NULL) {
            memcpy(cache->rect_scale, scale, sizeof(scale));
            cache->rect_scale_valid = GL_TRUE;
        }
    }
    if (!(matrices_dirty || texture_matrices_dirty || alpha_dirty || point_dirty || texenv_dirty ||
          texgen_dirty || implicit_texgen_dirty || clip_dirty || fog_dirty || lights_dirty ||
          material_dirty))
        return;
    if (matrices_dirty) {
        if (program->compat_uniform_model_view_projection >= 0 &&
            ctx->native_dispatch->glesUniformMatrix4fv != NULL) {
            mgl_compat_mvp(&ctx->compat, mvp);
            (ctx->native_dispatch->glesUniformMatrix4fv(
                 program->compat_uniform_model_view_projection, 1, GL_FALSE, mvp),
             mgl_render_diag_native_check(ctx,
                                          "UniformMatrix4fv:compat_uniform_model_view_projection",
                                          (uint64_t)(program->compat_uniform_model_view_projection),
                                          (uint64_t)program->native_name));
        }
        if (program->compat_uniform_model_view >= 0 &&
            ctx->native_dispatch->glesUniformMatrix4fv != NULL)
            (ctx->native_dispatch->glesUniformMatrix4fv(program->compat_uniform_model_view, 1,
                                                        GL_FALSE,
                                                        mgl_compat_modelview(&ctx->compat)),
             mgl_render_diag_native_check(ctx, "UniformMatrix4fv:compat_uniform_model_view",
                                          (uint64_t)(program->compat_uniform_model_view),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_projection >= 0 &&
            ctx->native_dispatch->glesUniformMatrix4fv != NULL)
            (ctx->native_dispatch->glesUniformMatrix4fv(program->compat_uniform_projection, 1,
                                                        GL_FALSE,
                                                        mgl_compat_projection(&ctx->compat)),
             mgl_render_diag_native_check(ctx, "UniformMatrix4fv:compat_uniform_projection",
                                          (uint64_t)(program->compat_uniform_projection),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_normal_matrix >= 0 &&
            ctx->native_dispatch->glesUniformMatrix3fv != NULL) {
            mgl_compat_normal_matrix(&ctx->compat, normal);
            (ctx->native_dispatch->glesUniformMatrix3fv(program->compat_uniform_normal_matrix, 1,
                                                        GL_FALSE, normal),
             mgl_render_diag_native_check(ctx, "UniformMatrix3fv:compat_uniform_normal_matrix",
                                          (uint64_t)(program->compat_uniform_normal_matrix),
                                          (uint64_t)program->native_name));
        }
    }
    if (texture_matrices_dirty && ctx->native_dispatch->glesUniformMatrix4fv != NULL)
        for (texture_unit = 0u; texture_unit < MGL_TEXTURE_UNITS; ++texture_unit)
            if (program->compat_uniform_texture_matrix[texture_unit] >= 0 &&
                (first ||
                 memcmp(mgl_compat_texture_matrix(old, texture_unit),
                        mgl_compat_texture_matrix(now, texture_unit), 16u * sizeof(GLfloat)) != 0))
                (ctx->native_dispatch->glesUniformMatrix4fv(
                     program->compat_uniform_texture_matrix[texture_unit], 1, GL_FALSE,
                     mgl_compat_texture_matrix(now, texture_unit)),
                 mgl_render_diag_native_check(
                     ctx, "UniformMatrix4fv:compat_uniform_texture_matrix[texture_unit]",
                     (uint64_t)(program->compat_uniform_texture_matrix[texture_unit]),
                     (uint64_t)program->native_name));
    if (first && program->compat_uniform_sampler0 >= 0 &&
        ctx->native_dispatch->glesUniform1i != NULL)
        (ctx->native_dispatch->glesUniform1i(program->compat_uniform_sampler0, 0),
         mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_sampler0",
                                      (uint64_t)(program->compat_uniform_sampler0),
                                      (uint64_t)program->native_name));
    if (alpha_dirty) {
        if (program->compat_uniform_alpha_enabled >= 0 &&
            ctx->native_dispatch->glesUniform1i != NULL)
            (ctx->native_dispatch->glesUniform1i(program->compat_uniform_alpha_enabled,
                                                 ctx->canonical_state.alpha_test ? 1 : 0),
             mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_alpha_enabled",
                                          (uint64_t)(program->compat_uniform_alpha_enabled),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_alpha_func >= 0 && ctx->native_dispatch->glesUniform1i != NULL)
            (ctx->native_dispatch->glesUniform1i(program->compat_uniform_alpha_func,
                                                 (GLint)ctx->canonical_state.alpha_func),
             mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_alpha_func",
                                          (uint64_t)(program->compat_uniform_alpha_func),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_alpha_ref >= 0 && ctx->native_dispatch->glesUniform1f != NULL)
            (ctx->native_dispatch->glesUniform1f(program->compat_uniform_alpha_ref,
                                                 ctx->canonical_state.alpha_ref),
             mgl_render_diag_native_check(ctx, "Uniform1f:compat_uniform_alpha_ref",
                                          (uint64_t)(program->compat_uniform_alpha_ref),
                                          (uint64_t)program->native_name));
    }
    if (point_dirty) {
        if (program->compat_uniform_program_point_size >= 0 &&
            ctx->native_dispatch->glesUniform1i != NULL)
            (ctx->native_dispatch->glesUniform1i(
                 program->compat_uniform_program_point_size,
                 ctx->canonical_state.program_point_size_enabled ? 1 : 0),
             mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_program_point_size",
                                          (uint64_t)(program->compat_uniform_program_point_size),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_point_size >= 0 && ctx->native_dispatch->glesUniform1f != NULL)
            (ctx->native_dispatch->glesUniform1f(program->compat_uniform_point_size,
                                                 ctx->canonical_state.point_size),
             mgl_render_diag_native_check(ctx, "Uniform1f:compat_uniform_point_size",
                                          (uint64_t)(program->compat_uniform_point_size),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_point_coord_flip >= 0 &&
            ctx->native_dispatch->glesUniform1i != NULL)
            (ctx->native_dispatch->glesUniform1i(
                 program->compat_uniform_point_coord_flip,
                 ctx->canonical_state.point_sprite_enabled &&
                         ctx->canonical_state.point_sprite_coord_origin == GL_LOWER_LEFT
                     ? 1
                     : 0),
             mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_point_coord_flip",
                                          (uint64_t)(program->compat_uniform_point_coord_flip),
                                          (uint64_t)program->native_name));
    }
    if (texenv_dirty && program->compat_uniform_ff_texture_enabled >= 0 &&
        ctx->native_dispatch->glesUniform1i != NULL)
        (ctx->native_dispatch->glesUniform1i(
             program->compat_uniform_ff_texture_enabled,
             mgl_compat_selected_texture_target(&ctx->compat, 0u) != 0u ? 1 : 0),
         mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_ff_texture_enabled",
                                      (uint64_t)(program->compat_uniform_ff_texture_enabled),
                                      (uint64_t)program->native_name));
    if (texenv_dirty && program->compat_uniform_texture_env_mode0 >= 0 &&
        ctx->native_dispatch->glesUniform1i != NULL)
        (ctx->native_dispatch->glesUniform1i(program->compat_uniform_texture_env_mode0,
                                             (GLint)ctx->compat.texture_env_mode[0]),
         mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_texture_env_mode0",
                                      (uint64_t)(program->compat_uniform_texture_env_mode0),
                                      (uint64_t)program->native_name));
    if (texenv_dirty && program->compat_env_enabled >= 0 &&
        ctx->native_dispatch->glesUniform1iv != NULL &&
        ctx->native_dispatch->glesUniform4fv != NULL &&
        ctx->native_dispatch->glesUniform1fv != NULL) {
        GLint enabled[8], mode[8], rgb_mode[8], alpha_mode[8];
        GLint rgb_src[24], alpha_src[24], rgb_op[24], alpha_op[24];
        GLfloat color[32], rgb_scale[8], alpha_scale[8];
        unsigned unit, arg;
        for (unit = 0; unit < 8; ++unit) {
            const struct mgl_texenv_combine_state *e = &now->texture_env_combine[unit];
            enabled[unit] = mgl_compat_selected_texture_target(now, unit) != 0u ? 1 : 0;
            mode[unit] = (GLint)now->texture_env_mode[unit];
            rgb_mode[unit] = (GLint)e->rgb_mode;
            alpha_mode[unit] = (GLint)e->alpha_mode;
            rgb_scale[unit] = e->rgb_scale;
            alpha_scale[unit] = e->alpha_scale;
            memcpy(&color[unit * 4], e->color, sizeof(e->color));
            for (arg = 0; arg < 3; ++arg) {
                unsigned index = unit * 3 + arg;
                rgb_src[index] = (GLint)e->rgb_source[arg];
                alpha_src[index] = (GLint)e->alpha_source[arg];
                rgb_op[index] = (GLint)e->rgb_operand[arg];
                alpha_op[index] = (GLint)e->alpha_operand[arg];
            }
        }
#define MGL_ENV_INT(field, value, count)                                                           \
    if (program->field >= 0)                                                                       \
    (ctx->native_dispatch->glesUniform1iv(program->field, (count), (value)),                       \
     mgl_render_diag_native_check(ctx, "Uniform1iv:" #field, (uint64_t)(program->field),           \
                                  (uint64_t)program->native_name))
        MGL_ENV_INT(compat_env_enabled, enabled, 8);
        MGL_ENV_INT(compat_env_mode, mode, 8);
        MGL_ENV_INT(compat_env_rgb_mode, rgb_mode, 8);
        MGL_ENV_INT(compat_env_alpha_mode, alpha_mode, 8);
        MGL_ENV_INT(compat_env_rgb_source, rgb_src, 24);
        MGL_ENV_INT(compat_env_alpha_source, alpha_src, 24);
        MGL_ENV_INT(compat_env_rgb_operand, rgb_op, 24);
        MGL_ENV_INT(compat_env_alpha_operand, alpha_op, 24);
#undef MGL_ENV_INT
        if (program->compat_env_color >= 0)
            (ctx->native_dispatch->glesUniform4fv(program->compat_env_color, 8, color),
             mgl_render_diag_native_check(ctx, "Uniform4fv:compat_env_color",
                                          (uint64_t)(program->compat_env_color),
                                          (uint64_t)program->native_name));
        if (program->compat_env_rgb_scale >= 0)
            (ctx->native_dispatch->glesUniform1fv(program->compat_env_rgb_scale, 8, rgb_scale),
             mgl_render_diag_native_check(ctx, "Uniform1fv:compat_env_rgb_scale",
                                          (uint64_t)(program->compat_env_rgb_scale),
                                          (uint64_t)program->native_name));
        if (program->compat_env_alpha_scale >= 0)
            (ctx->native_dispatch->glesUniform1fv(program->compat_env_alpha_scale, 8, alpha_scale),
             mgl_render_diag_native_check(ctx, "Uniform1fv:compat_env_alpha_scale",
                                          (uint64_t)(program->compat_env_alpha_scale),
                                          (uint64_t)program->native_name));
    }
    if (first && ctx->native_dispatch->glesUniform1i != NULL) {
        for (texture_unit = 0; texture_unit < 8; ++texture_unit)
            if (program->compat_env_sampler[texture_unit] >= 0)
                (ctx->native_dispatch->glesUniform1i(program->compat_env_sampler[texture_unit],
                                                     (GLint)texture_unit),
                 mgl_render_diag_native_check(ctx, "Uniform1i:compat_env_sampler[texture_unit]",
                                              (uint64_t)(program->compat_env_sampler[texture_unit]),
                                              (uint64_t)program->native_name));
    }

    if (implicit_texgen_dirty) {
        GLint enabled[MGL_TEXTURE_UNITS * MGL_TEXGEN_COORDS];
        GLint modes[MGL_TEXTURE_UNITS * MGL_TEXGEN_COORDS];
        GLfloat object_planes[MGL_TEXTURE_UNITS * MGL_TEXGEN_COORDS * 4u];
        GLfloat eye_planes[MGL_TEXTURE_UNITS * MGL_TEXGEN_COORDS * 4u];
        unsigned unit, coord;
        for (unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit) {
            for (coord = 0u; coord < MGL_TEXGEN_COORDS; ++coord) {
                unsigned index = unit * MGL_TEXGEN_COORDS + coord;
                const struct mgl_texgen_state *tg = &ctx->compat.texgen[unit][coord];
                enabled[index] = tg->enabled ? 1 : 0;
                modes[index] = (GLint)tg->mode;
                memcpy(object_planes + index * 4u, tg->object_plane, 4u * sizeof(GLfloat));
                memcpy(eye_planes + index * 4u, tg->eye_plane, 4u * sizeof(GLfloat));
            }
        }
        if (program->compat_uniform_implicit_texgen_enabled >= 0 &&
            ctx->native_dispatch->glesUniform1iv != NULL)
            (ctx->native_dispatch->glesUniform1iv(program->compat_uniform_implicit_texgen_enabled,
                                                  MGL_TEXTURE_UNITS * MGL_TEXGEN_COORDS, enabled),
             mgl_render_diag_native_check(
                 ctx, "Uniform1iv:compat_uniform_implicit_texgen_enabled",
                 (uint64_t)(program->compat_uniform_implicit_texgen_enabled),
                 (uint64_t)program->native_name));
        if (program->compat_uniform_implicit_texgen_mode >= 0 &&
            ctx->native_dispatch->glesUniform1iv != NULL)
            (ctx->native_dispatch->glesUniform1iv(program->compat_uniform_implicit_texgen_mode,
                                                  MGL_TEXTURE_UNITS * MGL_TEXGEN_COORDS, modes),
             mgl_render_diag_native_check(ctx, "Uniform1iv:compat_uniform_implicit_texgen_mode",
                                          (uint64_t)(program->compat_uniform_implicit_texgen_mode),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_implicit_texgen_object_plane >= 0 &&
            ctx->native_dispatch->glesUniform4fv != NULL)
            (ctx->native_dispatch->glesUniform4fv(
                 program->compat_uniform_implicit_texgen_object_plane,
                 MGL_TEXTURE_UNITS * MGL_TEXGEN_COORDS, object_planes),
             mgl_render_diag_native_check(
                 ctx, "Uniform4fv:compat_uniform_implicit_texgen_object_plane",
                 (uint64_t)(program->compat_uniform_implicit_texgen_object_plane),
                 (uint64_t)program->native_name));
        if (program->compat_uniform_implicit_texgen_eye_plane >= 0 &&
            ctx->native_dispatch->glesUniform4fv != NULL)
            (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_implicit_texgen_eye_plane,
                                                  MGL_TEXTURE_UNITS * MGL_TEXGEN_COORDS,
                                                  eye_planes),
             mgl_render_diag_native_check(
                 ctx, "Uniform4fv:compat_uniform_implicit_texgen_eye_plane",
                 (uint64_t)(program->compat_uniform_implicit_texgen_eye_plane),
                 (uint64_t)program->native_name));
    }
    if (texgen_dirty && program->compat_uniform_texgen_enabled >= 0) {
        GLint enabled[MGL_TEXGEN_COORDS];
        GLint modes[MGL_TEXGEN_COORDS];
        GLfloat object_plane[MGL_TEXGEN_COORDS * 4u];
        GLfloat eye_plane[MGL_TEXGEN_COORDS * 4u];
        unsigned i;
        for (i = 0u; i < MGL_TEXGEN_COORDS; ++i) {
            enabled[i] = ctx->compat.texgen[0][i].enabled ? 1 : 0;
            modes[i] = (GLint)ctx->compat.texgen[0][i].mode;
            memcpy(object_plane + i * 4u, ctx->compat.texgen[0][i].object_plane,
                   4u * sizeof(GLfloat));
            memcpy(eye_plane + i * 4u, ctx->compat.texgen[0][i].eye_plane, 4u * sizeof(GLfloat));
        }
        if (ctx->native_dispatch->glesUniform1iv != NULL) {
            (ctx->native_dispatch->glesUniform1iv(program->compat_uniform_texgen_enabled,
                                                  MGL_TEXGEN_COORDS, enabled),
             mgl_render_diag_native_check(ctx, "Uniform1iv:compat_uniform_texgen_enabled",
                                          (uint64_t)(program->compat_uniform_texgen_enabled),
                                          (uint64_t)program->native_name));
            if (program->compat_uniform_texgen_mode >= 0)
                (ctx->native_dispatch->glesUniform1iv(program->compat_uniform_texgen_mode,
                                                      MGL_TEXGEN_COORDS, modes),
                 mgl_render_diag_native_check(ctx, "Uniform1iv:compat_uniform_texgen_mode",
                                              (uint64_t)(program->compat_uniform_texgen_mode),
                                              (uint64_t)program->native_name));
        }
        if (ctx->native_dispatch->glesUniform4fv != NULL) {
            if (program->compat_uniform_texgen_object_plane >= 0)
                (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_texgen_object_plane,
                                                      MGL_TEXGEN_COORDS, object_plane),
                 mgl_render_diag_native_check(
                     ctx, "Uniform4fv:compat_uniform_texgen_object_plane",
                     (uint64_t)(program->compat_uniform_texgen_object_plane),
                     (uint64_t)program->native_name));
            if (program->compat_uniform_texgen_eye_plane >= 0)
                (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_texgen_eye_plane,
                                                      MGL_TEXGEN_COORDS, eye_plane),
                 mgl_render_diag_native_check(ctx, "Uniform4fv:compat_uniform_texgen_eye_plane",
                                              (uint64_t)(program->compat_uniform_texgen_eye_plane),
                                              (uint64_t)program->native_name));
        }
    }
    if (clip_dirty && program->compat_uniform_clip_enabled >= 0) {
        GLint enabled[MGL_MAX_CLIP_PLANES];
        GLfloat planes[MGL_MAX_CLIP_PLANES * 4u];
        unsigned i;
        for (i = 0u; i < MGL_MAX_CLIP_PLANES; ++i) {
            enabled[i] = ctx->compat.clip_plane_enabled[i] ? 1 : 0;
            memcpy(planes + i * 4u, ctx->compat.clip_plane[i], 4u * sizeof(GLfloat));
        }
        if (ctx->native_dispatch->glesUniform1iv != NULL)
            (ctx->native_dispatch->glesUniform1iv(program->compat_uniform_clip_enabled,
                                                  MGL_MAX_CLIP_PLANES, enabled),
             mgl_render_diag_native_check(ctx, "Uniform1iv:compat_uniform_clip_enabled",
                                          (uint64_t)(program->compat_uniform_clip_enabled),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_clip_plane >= 0 && ctx->native_dispatch->glesUniform4fv != NULL)
            (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_clip_plane,
                                                  MGL_MAX_CLIP_PLANES, planes),
             mgl_render_diag_native_check(ctx, "Uniform4fv:compat_uniform_clip_plane",
                                          (uint64_t)(program->compat_uniform_clip_plane),
                                          (uint64_t)program->native_name));
    }
    if (fog_dirty) {
        if (program->compat_uniform_fog_enabled >= 0 && ctx->native_dispatch->glesUniform1i != NULL)
            (ctx->native_dispatch->glesUniform1i(program->compat_uniform_fog_enabled,
                                                 ctx->compat.fog_enabled ? 1 : 0),
             mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_fog_enabled",
                                          (uint64_t)(program->compat_uniform_fog_enabled),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_fog_mode >= 0 && ctx->native_dispatch->glesUniform1i != NULL)
            (ctx->native_dispatch->glesUniform1i(program->compat_uniform_fog_mode,
                                                 (GLint)ctx->compat.fog_mode),
             mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_fog_mode",
                                          (uint64_t)(program->compat_uniform_fog_mode),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_fog_density >= 0 && ctx->native_dispatch->glesUniform1f != NULL)
            (ctx->native_dispatch->glesUniform1f(program->compat_uniform_fog_density,
                                                 ctx->compat.fog_density),
             mgl_render_diag_native_check(ctx, "Uniform1f:compat_uniform_fog_density",
                                          (uint64_t)(program->compat_uniform_fog_density),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_fog_start >= 0 && ctx->native_dispatch->glesUniform1f != NULL)
            (ctx->native_dispatch->glesUniform1f(program->compat_uniform_fog_start,
                                                 ctx->compat.fog_start),
             mgl_render_diag_native_check(ctx, "Uniform1f:compat_uniform_fog_start",
                                          (uint64_t)(program->compat_uniform_fog_start),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_fog_end >= 0 && ctx->native_dispatch->glesUniform1f != NULL)
            (ctx->native_dispatch->glesUniform1f(program->compat_uniform_fog_end,
                                                 ctx->compat.fog_end),
             mgl_render_diag_native_check(ctx, "Uniform1f:compat_uniform_fog_end",
                                          (uint64_t)(program->compat_uniform_fog_end),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_fog_color >= 0 && ctx->native_dispatch->glesUniform4f != NULL)
            (ctx->native_dispatch->glesUniform4f(
                 program->compat_uniform_fog_color, ctx->compat.fog_color[0],
                 ctx->compat.fog_color[1], ctx->compat.fog_color[2], ctx->compat.fog_color[3]),
             mgl_render_diag_native_check(ctx, "Uniform4f:compat_uniform_fog_color",
                                          (uint64_t)(program->compat_uniform_fog_color),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_fog_coord_explicit >= 0 &&
            ctx->native_dispatch->glesUniform1i != NULL)
            (ctx->native_dispatch->glesUniform1i(program->compat_uniform_fog_coord_explicit,
                                                 ctx->compat.fog_coord_source == GL_FOG_COORD ? 1
                                                                                              : 0),
             mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_fog_coord_explicit",
                                          (uint64_t)(program->compat_uniform_fog_coord_explicit),
                                          (uint64_t)program->native_name));
    }
    if (lights_dirty) {
        if (program->compat_uniform_normalize_enabled >= 0 &&
            ctx->native_dispatch->glesUniform1i != NULL)
            (ctx->native_dispatch->glesUniform1i(
                 program->compat_uniform_normalize_enabled,
                 (ctx->compat.normalize_enabled || ctx->compat.rescale_normal_enabled) ? 1 : 0),
             mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_normalize_enabled",
                                          (uint64_t)(program->compat_uniform_normalize_enabled),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_light_enabled >= 0) {
            GLint enabled[MGL_MAX_LIGHTS];
            GLfloat ambient[MGL_MAX_LIGHTS * 4u], diffuse[MGL_MAX_LIGHTS * 4u],
                specular[MGL_MAX_LIGHTS * 4u], position[MGL_MAX_LIGHTS * 4u];
            GLfloat spot_dir[MGL_MAX_LIGHTS * 3u], spot_exp[MGL_MAX_LIGHTS],
                spot_cutoff[MGL_MAX_LIGHTS];
            GLfloat att_const[MGL_MAX_LIGHTS], att_linear[MGL_MAX_LIGHTS], att_quad[MGL_MAX_LIGHTS];
            unsigned i, j;
            for (i = 0u; i < MGL_MAX_LIGHTS; ++i) {
                const struct mgl_light_state *l = &ctx->compat.lights[i];
                enabled[i] = l->enabled ? 1 : 0;
                for (j = 0u; j < 4u; ++j) {
                    ambient[i * 4u + j] = l->ambient[j];
                    diffuse[i * 4u + j] = l->diffuse[j];
                    specular[i * 4u + j] = l->specular[j];
                    position[i * 4u + j] = l->position[j];
                }
                for (j = 0u; j < 3u; ++j)
                    spot_dir[i * 3u + j] = l->spot_direction[j];
                spot_exp[i] = l->spot_exponent;
                spot_cutoff[i] = l->spot_cutoff;
                att_const[i] = l->constant_attenuation;
                att_linear[i] = l->linear_attenuation;
                att_quad[i] = l->quadratic_attenuation;
            }
            if (ctx->native_dispatch->glesUniform1iv != NULL)
                (ctx->native_dispatch->glesUniform1iv(program->compat_uniform_light_enabled,
                                                      MGL_MAX_LIGHTS, enabled),
                 mgl_render_diag_native_check(ctx, "Uniform1iv:compat_uniform_light_enabled",
                                              (uint64_t)(program->compat_uniform_light_enabled),
                                              (uint64_t)program->native_name));
            if (program->compat_uniform_light_ambient >= 0 &&
                ctx->native_dispatch->glesUniform4fv != NULL)
                (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_light_ambient,
                                                      MGL_MAX_LIGHTS, ambient),
                 mgl_render_diag_native_check(ctx, "Uniform4fv:compat_uniform_light_ambient",
                                              (uint64_t)(program->compat_uniform_light_ambient),
                                              (uint64_t)program->native_name));
            if (program->compat_uniform_light_diffuse >= 0 &&
                ctx->native_dispatch->glesUniform4fv != NULL)
                (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_light_diffuse,
                                                      MGL_MAX_LIGHTS, diffuse),
                 mgl_render_diag_native_check(ctx, "Uniform4fv:compat_uniform_light_diffuse",
                                              (uint64_t)(program->compat_uniform_light_diffuse),
                                              (uint64_t)program->native_name));
            if (program->compat_uniform_light_specular >= 0 &&
                ctx->native_dispatch->glesUniform4fv != NULL)
                (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_light_specular,
                                                      MGL_MAX_LIGHTS, specular),
                 mgl_render_diag_native_check(ctx, "Uniform4fv:compat_uniform_light_specular",
                                              (uint64_t)(program->compat_uniform_light_specular),
                                              (uint64_t)program->native_name));
            if (program->compat_uniform_light_position >= 0 &&
                ctx->native_dispatch->glesUniform4fv != NULL)
                (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_light_position,
                                                      MGL_MAX_LIGHTS, position),
                 mgl_render_diag_native_check(ctx, "Uniform4fv:compat_uniform_light_position",
                                              (uint64_t)(program->compat_uniform_light_position),
                                              (uint64_t)program->native_name));
            if (program->compat_uniform_light_spot_direction >= 0 &&
                ctx->native_dispatch->glesUniform3fv != NULL)
                (ctx->native_dispatch->glesUniform3fv(program->compat_uniform_light_spot_direction,
                                                      MGL_MAX_LIGHTS, spot_dir),
                 mgl_render_diag_native_check(
                     ctx, "Uniform3fv:compat_uniform_light_spot_direction",
                     (uint64_t)(program->compat_uniform_light_spot_direction),
                     (uint64_t)program->native_name));
            if (program->compat_uniform_light_spot_exponent >= 0 &&
                ctx->native_dispatch->glesUniform1fv != NULL)
                (ctx->native_dispatch->glesUniform1fv(program->compat_uniform_light_spot_exponent,
                                                      MGL_MAX_LIGHTS, spot_exp),
                 mgl_render_diag_native_check(
                     ctx, "Uniform1fv:compat_uniform_light_spot_exponent",
                     (uint64_t)(program->compat_uniform_light_spot_exponent),
                     (uint64_t)program->native_name));
            if (program->compat_uniform_light_spot_cutoff >= 0 &&
                ctx->native_dispatch->glesUniform1fv != NULL)
                (ctx->native_dispatch->glesUniform1fv(program->compat_uniform_light_spot_cutoff,
                                                      MGL_MAX_LIGHTS, spot_cutoff),
                 mgl_render_diag_native_check(ctx, "Uniform1fv:compat_uniform_light_spot_cutoff",
                                              (uint64_t)(program->compat_uniform_light_spot_cutoff),
                                              (uint64_t)program->native_name));
            if (program->compat_uniform_light_constant_attenuation >= 0 &&
                ctx->native_dispatch->glesUniform1fv != NULL)
                (ctx->native_dispatch->glesUniform1fv(
                     program->compat_uniform_light_constant_attenuation, MGL_MAX_LIGHTS, att_const),
                 mgl_render_diag_native_check(
                     ctx, "Uniform1fv:compat_uniform_light_constant_attenuation",
                     (uint64_t)(program->compat_uniform_light_constant_attenuation),
                     (uint64_t)program->native_name));
            if (program->compat_uniform_light_linear_attenuation >= 0 &&
                ctx->native_dispatch->glesUniform1fv != NULL)
                (ctx->native_dispatch->glesUniform1fv(
                     program->compat_uniform_light_linear_attenuation, MGL_MAX_LIGHTS, att_linear),
                 mgl_render_diag_native_check(
                     ctx, "Uniform1fv:compat_uniform_light_linear_attenuation",
                     (uint64_t)(program->compat_uniform_light_linear_attenuation),
                     (uint64_t)program->native_name));
            if (program->compat_uniform_light_quadratic_attenuation >= 0 &&
                ctx->native_dispatch->glesUniform1fv != NULL)
                (ctx->native_dispatch->glesUniform1fv(
                     program->compat_uniform_light_quadratic_attenuation, MGL_MAX_LIGHTS, att_quad),
                 mgl_render_diag_native_check(
                     ctx, "Uniform1fv:compat_uniform_light_quadratic_attenuation",
                     (uint64_t)(program->compat_uniform_light_quadratic_attenuation),
                     (uint64_t)program->native_name));
        }
        if (lights_dirty && program->compat_uniform_lighting_enabled >= 0 &&
            ctx->native_dispatch->glesUniform1i != NULL)
            (ctx->native_dispatch->glesUniform1i(program->compat_uniform_lighting_enabled,
                                                 ctx->compat.lighting_enabled ? 1 : 0),
             mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_lighting_enabled",
                                          (uint64_t)(program->compat_uniform_lighting_enabled),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_light_model_ambient >= 0 &&
            ctx->native_dispatch->glesUniform4fv != NULL)
            (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_light_model_ambient, 1,
                                                  ctx->compat.light_model_ambient),
             mgl_render_diag_native_check(ctx, "Uniform4fv:compat_uniform_light_model_ambient",
                                          (uint64_t)(program->compat_uniform_light_model_ambient),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_light_model_local_viewer >= 0 &&
            ctx->native_dispatch->glesUniform1i != NULL)
            (ctx->native_dispatch->glesUniform1i(program->compat_uniform_light_model_local_viewer,
                                                 ctx->compat.light_model_local_viewer ? 1 : 0),
             mgl_render_diag_native_check(
                 ctx, "Uniform1i:compat_uniform_light_model_local_viewer",
                 (uint64_t)(program->compat_uniform_light_model_local_viewer),
                 (uint64_t)program->native_name));
        if (program->compat_uniform_light_model_two_side >= 0 &&
            ctx->native_dispatch->glesUniform1i != NULL)
            (ctx->native_dispatch->glesUniform1i(program->compat_uniform_light_model_two_side,
                                                 ctx->compat.light_model_two_side ? 1 : 0),
             mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_light_model_two_side",
                                          (uint64_t)(program->compat_uniform_light_model_two_side),
                                          (uint64_t)program->native_name));
    }
    if (material_dirty) {
        if (program->compat_uniform_material_ambient >= 0 &&
            ctx->native_dispatch->glesUniform4fv != NULL)
            (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_material_ambient, 1,
                                                  ctx->compat.material_front.ambient),
             mgl_render_diag_native_check(ctx, "Uniform4fv:compat_uniform_material_ambient",
                                          (uint64_t)(program->compat_uniform_material_ambient),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_material_diffuse >= 0 &&
            ctx->native_dispatch->glesUniform4fv != NULL)
            (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_material_diffuse, 1,
                                                  ctx->compat.material_front.diffuse),
             mgl_render_diag_native_check(ctx, "Uniform4fv:compat_uniform_material_diffuse",
                                          (uint64_t)(program->compat_uniform_material_diffuse),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_material_specular >= 0 &&
            ctx->native_dispatch->glesUniform4fv != NULL)
            (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_material_specular, 1,
                                                  ctx->compat.material_front.specular),
             mgl_render_diag_native_check(ctx, "Uniform4fv:compat_uniform_material_specular",
                                          (uint64_t)(program->compat_uniform_material_specular),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_material_emission >= 0 &&
            ctx->native_dispatch->glesUniform4fv != NULL)
            (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_material_emission, 1,
                                                  ctx->compat.material_front.emission),
             mgl_render_diag_native_check(ctx, "Uniform4fv:compat_uniform_material_emission",
                                          (uint64_t)(program->compat_uniform_material_emission),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_material_shininess >= 0 &&
            ctx->native_dispatch->glesUniform1f != NULL)
            (ctx->native_dispatch->glesUniform1f(program->compat_uniform_material_shininess,
                                                 ctx->compat.material_front.shininess),
             mgl_render_diag_native_check(ctx, "Uniform1f:compat_uniform_material_shininess",
                                          (uint64_t)(program->compat_uniform_material_shininess),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_material_back_ambient >= 0 &&
            ctx->native_dispatch->glesUniform4fv != NULL)
            (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_material_back_ambient, 1,
                                                  ctx->compat.material_back.ambient),
             mgl_render_diag_native_check(ctx, "Uniform4fv:compat_uniform_material_back_ambient",
                                          (uint64_t)(program->compat_uniform_material_back_ambient),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_material_back_diffuse >= 0 &&
            ctx->native_dispatch->glesUniform4fv != NULL)
            (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_material_back_diffuse, 1,
                                                  ctx->compat.material_back.diffuse),
             mgl_render_diag_native_check(ctx, "Uniform4fv:compat_uniform_material_back_diffuse",
                                          (uint64_t)(program->compat_uniform_material_back_diffuse),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_material_back_specular >= 0 &&
            ctx->native_dispatch->glesUniform4fv != NULL)
            (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_material_back_specular, 1,
                                                  ctx->compat.material_back.specular),
             mgl_render_diag_native_check(
                 ctx, "Uniform4fv:compat_uniform_material_back_specular",
                 (uint64_t)(program->compat_uniform_material_back_specular),
                 (uint64_t)program->native_name));
        if (program->compat_uniform_material_back_emission >= 0 &&
            ctx->native_dispatch->glesUniform4fv != NULL)
            (ctx->native_dispatch->glesUniform4fv(program->compat_uniform_material_back_emission, 1,
                                                  ctx->compat.material_back.emission),
             mgl_render_diag_native_check(
                 ctx, "Uniform4fv:compat_uniform_material_back_emission",
                 (uint64_t)(program->compat_uniform_material_back_emission),
                 (uint64_t)program->native_name));
        if (program->compat_uniform_material_back_shininess >= 0 &&
            ctx->native_dispatch->glesUniform1f != NULL)
            (ctx->native_dispatch->glesUniform1f(program->compat_uniform_material_back_shininess,
                                                 ctx->compat.material_back.shininess),
             mgl_render_diag_native_check(
                 ctx, "Uniform1f:compat_uniform_material_back_shininess",
                 (uint64_t)(program->compat_uniform_material_back_shininess),
                 (uint64_t)program->native_name));
        if (program->compat_uniform_color_material_enabled >= 0 &&
            ctx->native_dispatch->glesUniform1i != NULL)
            (ctx->native_dispatch->glesUniform1i(program->compat_uniform_color_material_enabled,
                                                 ctx->compat.color_material_enabled ? 1 : 0),
             mgl_render_diag_native_check(
                 ctx, "Uniform1i:compat_uniform_color_material_enabled",
                 (uint64_t)(program->compat_uniform_color_material_enabled),
                 (uint64_t)program->native_name));
        if (program->compat_uniform_color_material_mode >= 0 &&
            ctx->native_dispatch->glesUniform1i != NULL)
            (ctx->native_dispatch->glesUniform1i(program->compat_uniform_color_material_mode,
                                                 (GLint)ctx->compat.color_material_mode),
             mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_color_material_mode",
                                          (uint64_t)(program->compat_uniform_color_material_mode),
                                          (uint64_t)program->native_name));
        if (program->compat_uniform_color_material_face >= 0 &&
            ctx->native_dispatch->glesUniform1i != NULL)
            (ctx->native_dispatch->glesUniform1i(program->compat_uniform_color_material_face,
                                                 (GLint)ctx->compat.color_material_face),
             mgl_render_diag_native_check(ctx, "Uniform1i:compat_uniform_color_material_face",
                                          (uint64_t)(program->compat_uniform_color_material_face),
                                          (uint64_t)program->native_name));
    }
    if (cache != NULL) {
        memcpy(&cache->compat, now, sizeof(*now));
        cache->alpha_test = ctx->canonical_state.alpha_test;
        cache->alpha_func = ctx->canonical_state.alpha_func;
        cache->alpha_ref = ctx->canonical_state.alpha_ref;
        cache->program_point_size_enabled = ctx->canonical_state.program_point_size_enabled;
        cache->point_size = ctx->canonical_state.point_size;
        cache->point_sprite_enabled = ctx->canonical_state.point_sprite_enabled;
        cache->point_sprite_coord_origin = ctx->canonical_state.point_sprite_coord_origin;
        program->compat_uniform_cache_valid = GL_TRUE;
    }
}

static GLuint compile_fixed_stage(struct mgl_context *ctx, GLenum type, const char *source) {
    GLuint shader;
    GLint status = GL_FALSE;
    const GLchar *src = source;
    if (ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesCreateShader == NULL ||
        ctx->native_dispatch->glesShaderSource == NULL ||
        ctx->native_dispatch->glesCompileShader == NULL ||
        ctx->native_dispatch->glesGetShaderiv == NULL)
        return 0u;
    shader = ctx->native_dispatch->glesCreateShader(type);
    if (shader == 0u)
        return 0u;
    ctx->native_dispatch->glesShaderSource(shader, 1, &src, NULL);
    mgl_perf_diag_program_cache(ctx, MGL_PERF_FIXED_SHADER_COMPILE);
    ctx->native_dispatch->glesCompileShader(shader);
    ctx->native_dispatch->glesGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        if (ctx->native_dispatch->glesDeleteShader != NULL)
            ctx->native_dispatch->glesDeleteShader(shader);
        return 0u;
    }
    return shader;
}

#define MGL_FIXED_COMPLEX_CACHE_LIMIT 32u

static void fixed_variant_dispose(struct mgl_context *ctx, struct mgl_fixed_program_variant *slot) {
    if (slot->program_native != 0u && ctx->native_dispatch->glesDeleteProgram != NULL)
        ctx->native_dispatch->glesDeleteProgram(slot->program_native);
    if (slot->vertex_shader_native != 0u && ctx->native_dispatch->glesDeleteShader != NULL)
        ctx->native_dispatch->glesDeleteShader(slot->vertex_shader_native);
    if (slot->fragment_shader_native != 0u && ctx->native_dispatch->glesDeleteShader != NULL)
        ctx->native_dispatch->glesDeleteShader(slot->fragment_shader_native);
    free(slot->program.compat_uniform_cache);
    memset(slot, 0, sizeof(*slot));
}

static struct mgl_fixed_program_variant *fixed_complex_take(struct mgl_context *ctx,
                                                            unsigned signature) {
    struct mgl_fixed_program_variant **link = &ctx->fixed_complex_programs;
    while (*link != NULL) {
        struct mgl_fixed_program_variant *slot = *link;
        if (slot->texture_signature == signature) {
            *link = slot->next;
            slot->next = NULL;
            --ctx->fixed_complex_program_count;
            return slot;
        }
        link = &slot->next;
    }
    return NULL;
}

static struct mgl_fixed_program_variant *fixed_complex_allocate(struct mgl_context *ctx) {
    struct mgl_fixed_program_variant *slot;
    struct mgl_fixed_program_variant **link;
    if (ctx->fixed_complex_program_count < MGL_FIXED_COMPLEX_CACHE_LIMIT)
        return calloc(1u, sizeof(struct mgl_fixed_program_variant));
    link = &ctx->fixed_complex_programs;
    while ((*link)->next != NULL)
        link = &(*link)->next;
    slot = *link;
    *link = NULL;
    --ctx->fixed_complex_program_count;
    mgl_perf_diag_program_cache(ctx, MGL_PERF_FIXED_CACHE_EVICTION);
    fixed_variant_dispose(ctx, slot);
    return slot;
}

static void fixed_save_active(struct mgl_context *ctx) {
    struct mgl_fixed_program_variant *slot;
    if (!ctx->fixed_program_ready)
        return;
    if (ctx->fixed_program_key == 4u) {
        slot = fixed_complex_allocate(ctx);
        if (slot != NULL) {
            slot->next = ctx->fixed_complex_programs;
            ctx->fixed_complex_programs = slot;
            ++ctx->fixed_complex_program_count;
        }
    } else {
        slot = &ctx->fixed_program_variants[ctx->fixed_program_key];
    }
    if (slot != NULL) {
        slot->program = ctx->fixed_program;
        slot->program_native = ctx->fixed_program_native;
        slot->vertex_shader_native = ctx->fixed_vertex_shader_native;
        slot->fragment_shader_native = ctx->fixed_fragment_shader_native;
        slot->texture_signature = ctx->fixed_program_texture_signature;
        slot->ready = GL_TRUE;
    } else {
        struct mgl_fixed_program_variant discarded = {0};
        discarded.program = ctx->fixed_program;
        discarded.program_native = ctx->fixed_program_native;
        discarded.vertex_shader_native = ctx->fixed_vertex_shader_native;
        discarded.fragment_shader_native = ctx->fixed_fragment_shader_native;
        fixed_variant_dispose(ctx, &discarded);
    }
    memset(&ctx->fixed_program, 0, sizeof(ctx->fixed_program));
    ctx->fixed_program_native = 0u;
    ctx->fixed_vertex_shader_native = 0u;
    ctx->fixed_fragment_shader_native = 0u;
    ctx->fixed_program_ready = GL_FALSE;
}

static struct mgl_program *fixed_restore(struct mgl_context *ctx,
                                         struct mgl_fixed_program_variant *slot, unsigned key) {
    ctx->fixed_program = slot->program;
    ctx->fixed_program_native = slot->program_native;
    ctx->fixed_vertex_shader_native = slot->vertex_shader_native;
    ctx->fixed_fragment_shader_native = slot->fragment_shader_native;
    ctx->fixed_program_key = key;
    ctx->fixed_program_texture_signature = slot->texture_signature;
    ctx->fixed_program_ready = GL_TRUE;
    if (key == 4u)
        free(slot);
    else
        memset(slot, 0, sizeof(*slot));
    return &ctx->fixed_program;
}

struct mgl_program *mgl_compat_fixed_program(struct mgl_context *ctx) {
    static const char vertex_untextured[] =
        "#version 100\n"
        "uniform float mgl_u_point_size;\n"
        "attribute vec4 mgl_a_position;\n"
        "attribute vec4 mgl_a_color;\n"
        "attribute float mgl_a_fog;\n"
        "uniform mat4 mgl_u_model_view_projection;\n"
        "uniform int mgl_u_fog_coord_explicit;\n"
        "uniform mat4 mgl_u_model_view;\n"
        "varying vec4 mgl_ff_color;\n"
        "varying vec4 mgl_ff_eye_position;\n"
        "varying float mgl_ff_fog_coord;\n"
        "void main(){ vec4 eye=mgl_u_model_view*mgl_a_position; gl_Position = "
        "mgl_u_model_view_projection * mgl_a_position;gl_PointSize=mgl_u_point_size; mgl_ff_color "
        "= mgl_a_color; mgl_ff_eye_position=eye; "
        "mgl_ff_fog_coord=mgl_u_fog_coord_explicit!=0?mgl_a_fog:abs(eye.z); }\n";
    static const char vertex_lit_untextured[] =
        "#version 100\n"
        "uniform float mgl_u_point_size;\n"
        "attribute vec4 mgl_a_position; attribute vec4 mgl_a_normal; attribute vec4 mgl_a_color; "
        "attribute float mgl_a_fog;\n"
        "uniform mat4 mgl_u_model_view_projection; uniform mat4 mgl_u_model_view; uniform mat3 "
        "mgl_u_normal_matrix; uniform int mgl_u_fog_coord_explicit;\n"
        "uniform int mgl_u_normalize_enabled; uniform int mgl_u_light_enabled[8];\n"
        "uniform vec4 mgl_u_light_ambient[8]; uniform vec4 mgl_u_light_diffuse[8]; uniform vec4 "
        "mgl_u_light_specular[8]; uniform vec4 mgl_u_light_position[8];\n"
        "uniform vec3 mgl_u_light_spot_direction[8]; uniform float mgl_u_light_spot_exponent[8]; "
        "uniform float mgl_u_light_spot_cutoff[8];\n"
        "uniform float mgl_u_light_constant_attenuation[8]; uniform float "
        "mgl_u_light_linear_attenuation[8]; uniform float mgl_u_light_quadratic_attenuation[8];\n"
        "uniform vec4 mgl_u_light_model_ambient; uniform int mgl_u_light_model_local_viewer;\n"
        "uniform vec4 mgl_u_material_ambient; uniform vec4 mgl_u_material_diffuse; uniform vec4 "
        "mgl_u_material_specular; uniform vec4 mgl_u_material_emission; uniform float "
        "mgl_u_material_shininess;\n"
        "uniform int mgl_u_color_material_enabled; uniform int mgl_u_color_material_mode;\n"
        "varying vec4 mgl_ff_color; varying vec4 mgl_ff_eye_position; varying float "
        "mgl_ff_fog_coord;\n"
        "vec4 mgl_lit(vec4 eye, vec3 n){ vec3 N=mgl_u_normal_matrix*n; "
        "if(mgl_u_normalize_enabled!=0) N=normalize(N);"
        "vec4 ma=mgl_u_material_ambient, md=mgl_u_material_diffuse, ms=mgl_u_material_specular, "
        "me=mgl_u_material_emission;"
        "if(mgl_u_color_material_enabled!=0){ if(mgl_u_color_material_mode==4608) ma=mgl_a_color; "
        "else if(mgl_u_color_material_mode==4609) md=mgl_a_color; else "
        "if(mgl_u_color_material_mode==4610) ms=mgl_a_color; else "
        "if(mgl_u_color_material_mode==5632) me=mgl_a_color; else {ma=mgl_a_color;md=mgl_a_color;} "
        "}"
        "vec3 V=mgl_u_light_model_local_viewer!=0?normalize(-eye.xyz):vec3(0.0,0.0,1.0); vec4 "
        "c=me+mgl_u_light_model_ambient*ma;"
        "for(int i=0;i<8;++i){ if(mgl_u_light_enabled[i]!=0){ vec3 L; float att=1.0; "
        "if(mgl_u_light_position[i].w==0.0) L=normalize(mgl_u_light_position[i].xyz); else {vec3 "
        "d=mgl_u_light_position[i].xyz-eye.xyz; float dist=length(d); "
        "L=dist>0.0?d/dist:vec3(0.0,0.0,1.0); "
        "att=1.0/"
        "max(mgl_u_light_constant_attenuation[i]+mgl_u_light_linear_attenuation[i]*dist+mgl_u_"
        "light_quadratic_attenuation[i]*dist*dist,0.000001); "
        "if(mgl_u_light_spot_cutoff[i]!=180.0){float "
        "sd=dot(normalize(mgl_u_light_spot_direction[i]),-L); float "
        "cc=cos(radians(mgl_u_light_spot_cutoff[i])); if(sd<cc) att=0.0; else "
        "att*=pow(max(sd,0.0),mgl_u_light_spot_exponent[i]);}} float nd=max(dot(N,L),0.0); float "
        "sp=0.0; if(nd>0.0){vec3 H=normalize(L+V); float nh=max(dot(N,H),0.0); "
        "sp=mgl_u_material_shininess==0.0?1.0:pow(nh,mgl_u_material_shininess);} "
        "c+=att*(mgl_u_light_ambient[i]*ma+nd*mgl_u_light_diffuse[i]*md+sp*mgl_u_light_specular[i]*"
        "ms); }} c.a=md.a; return clamp(c,0.0,1.0);}\n"
        "void main(){vec4 "
        "eye=mgl_u_model_view*mgl_a_position;gl_Position=mgl_u_model_view_projection*mgl_a_"
        "position;gl_PointSize=mgl_u_point_size;mgl_ff_color=mgl_lit(eye,mgl_a_normal.xyz);mgl_ff_"
        "eye_position=eye;mgl_ff_fog_coord=mgl_u_fog_coord_explicit!=0?mgl_a_fog:abs(eye.z);}\n";
    static const char vertex_lit_textured[] =
        "#version 100\n"
        "uniform float mgl_u_point_size;\n"
        "attribute vec4 mgl_a_position; attribute vec4 mgl_a_normal; attribute vec4 mgl_a_color; "
        "attribute vec4 mgl_a_texcoord0; attribute float mgl_a_fog;\n"
        "uniform mat4 mgl_u_model_view_projection; uniform mat4 mgl_u_model_view; uniform mat3 "
        "mgl_u_normal_matrix; uniform int mgl_u_fog_coord_explicit;\n"
        "uniform int mgl_u_normalize_enabled; uniform int mgl_u_light_enabled[8];\n"
        "uniform vec4 mgl_u_light_ambient[8]; uniform vec4 mgl_u_light_diffuse[8]; uniform vec4 "
        "mgl_u_light_specular[8]; uniform vec4 mgl_u_light_position[8];\n"
        "uniform vec3 mgl_u_light_spot_direction[8]; uniform float mgl_u_light_spot_exponent[8]; "
        "uniform float mgl_u_light_spot_cutoff[8];\n"
        "uniform float mgl_u_light_constant_attenuation[8]; uniform float "
        "mgl_u_light_linear_attenuation[8]; uniform float mgl_u_light_quadratic_attenuation[8];\n"
        "uniform vec4 mgl_u_light_model_ambient; uniform int mgl_u_light_model_local_viewer;\n"
        "uniform vec4 mgl_u_material_ambient; uniform vec4 mgl_u_material_diffuse; uniform vec4 "
        "mgl_u_material_specular; uniform vec4 mgl_u_material_emission; uniform float "
        "mgl_u_material_shininess;\n"
        "uniform int mgl_u_color_material_enabled; uniform int mgl_u_color_material_mode;\n"
        "uniform int mgl_u_texgen_enabled[4]; uniform int mgl_u_texgen_mode[4]; uniform vec4 "
        "mgl_u_texgen_object_plane[4]; uniform vec4 mgl_u_texgen_eye_plane[4];\n"
        "varying vec4 mgl_ff_color; varying vec2 mgl_ff_texcoord0; varying vec4 "
        "mgl_ff_eye_position; varying float mgl_ff_fog_coord;\n"
        "vec4 mgl_texgen(vec4 obj,vec4 eye,vec3 normal,vec4 cur){vec4 tc=cur;vec3 "
        "N=normalize(mgl_u_normal_matrix*normal);vec3 U=normalize(eye.xyz);vec3 "
        "R=reflect(U,N);float mm=2.0*sqrt(max(R.x*R.x+R.y*R.y+(R.z+1.0)*(R.z+1.0),0.000001));float "
        "ss=R.x/mm+0.5;float st=R.y/mm+0.5;if(mgl_u_texgen_enabled[0]!=0){int "
        "m=mgl_u_texgen_mode[0];tc.x=m==9217?dot(obj,mgl_u_texgen_object_plane[0]):m==9216?dot(eye,"
        "mgl_u_texgen_eye_plane[0]):m==9218?ss:m==34065?N.x:R.x;}if(mgl_u_texgen_enabled[1]!=0){"
        "int "
        "m=mgl_u_texgen_mode[1];tc.y=m==9217?dot(obj,mgl_u_texgen_object_plane[1]):m==9216?dot(eye,"
        "mgl_u_texgen_eye_plane[1]):m==9218?st:m==34065?N.y:R.y;}if(mgl_u_texgen_enabled[2]!=0){"
        "int "
        "m=mgl_u_texgen_mode[2];tc.z=m==9217?dot(obj,mgl_u_texgen_object_plane[2]):m==9216?dot(eye,"
        "mgl_u_texgen_eye_plane[2]):m==34065?N.z:R.z;}if(mgl_u_texgen_enabled[3]!=0){int "
        "m=mgl_u_texgen_mode[3];tc.w=m==9217?dot(obj,mgl_u_texgen_object_plane[3]):dot(eye,mgl_u_"
        "texgen_eye_plane[3]);}return tc;}\n"
        "vec4 mgl_lit(vec4 eye, vec3 n){ vec3 N=mgl_u_normal_matrix*n; "
        "if(mgl_u_normalize_enabled!=0) N=normalize(N);"
        "vec4 ma=mgl_u_material_ambient, md=mgl_u_material_diffuse, ms=mgl_u_material_specular, "
        "me=mgl_u_material_emission;"
        "if(mgl_u_color_material_enabled!=0){ if(mgl_u_color_material_mode==4608) ma=mgl_a_color; "
        "else if(mgl_u_color_material_mode==4609) md=mgl_a_color; else "
        "if(mgl_u_color_material_mode==4610) ms=mgl_a_color; else "
        "if(mgl_u_color_material_mode==5632) me=mgl_a_color; else {ma=mgl_a_color;md=mgl_a_color;} "
        "}"
        "vec3 V=mgl_u_light_model_local_viewer!=0?normalize(-eye.xyz):vec3(0.0,0.0,1.0); vec4 "
        "c=me+mgl_u_light_model_ambient*ma;"
        "for(int i=0;i<8;++i){ if(mgl_u_light_enabled[i]!=0){ vec3 L; float att=1.0; "
        "if(mgl_u_light_position[i].w==0.0) L=normalize(mgl_u_light_position[i].xyz); else {vec3 "
        "d=mgl_u_light_position[i].xyz-eye.xyz; float dist=length(d); "
        "L=dist>0.0?d/dist:vec3(0.0,0.0,1.0); "
        "att=1.0/"
        "max(mgl_u_light_constant_attenuation[i]+mgl_u_light_linear_attenuation[i]*dist+mgl_u_"
        "light_quadratic_attenuation[i]*dist*dist,0.000001); "
        "if(mgl_u_light_spot_cutoff[i]!=180.0){float "
        "sd=dot(normalize(mgl_u_light_spot_direction[i]),-L); float "
        "cc=cos(radians(mgl_u_light_spot_cutoff[i])); if(sd<cc) att=0.0; else "
        "att*=pow(max(sd,0.0),mgl_u_light_spot_exponent[i]);}} float nd=max(dot(N,L),0.0); float "
        "sp=0.0; if(nd>0.0){vec3 H=normalize(L+V); float nh=max(dot(N,H),0.0); "
        "sp=mgl_u_material_shininess==0.0?1.0:pow(nh,mgl_u_material_shininess);} "
        "c+=att*(mgl_u_light_ambient[i]*ma+nd*mgl_u_light_diffuse[i]*md+sp*mgl_u_light_specular[i]*"
        "ms); }} c.a=md.a; return clamp(c,0.0,1.0);}\n"
        "void main(){vec4 eye=mgl_u_model_view*mgl_a_position;vec4 "
        "tc=mgl_texgen(mgl_a_position,eye,mgl_a_normal.xyz,mgl_a_texcoord0);gl_Position=mgl_u_"
        "model_view_projection*mgl_a_position;gl_PointSize=mgl_u_point_size;mgl_ff_color=mgl_lit("
        "eye,mgl_a_normal.xyz);mgl_ff_texcoord0=tc.w!=0.0?tc.xy/"
        "tc.w:tc.xy;mgl_ff_eye_position=eye;mgl_ff_fog_coord=mgl_u_fog_coord_explicit!=0?mgl_a_fog:"
        "abs(eye.z);}\n";
    static const char fragment_untextured[] =
        "#version 100\n"
        "precision mediump float;\n"
        "varying vec4 mgl_ff_color;\n"
        "varying vec4 mgl_ff_eye_position;\n"
        "varying float mgl_ff_fog_coord;\n"
        "uniform int mgl_u_alpha_test_enabled;\n"
        "uniform int mgl_u_alpha_test_func;\n"
        "uniform float mgl_u_alpha_test_ref;\n"
        "uniform int mgl_u_fog_enabled;\n"
        "uniform int mgl_u_fog_mode;\n"
        "uniform float mgl_u_fog_density;\n"
        "uniform float mgl_u_fog_start;\n"
        "uniform float mgl_u_fog_end;\n"
        "uniform vec4 mgl_u_fog_color;\n"
        "uniform int mgl_u_clip_enabled[6]; uniform vec4 mgl_u_clip_plane[6];\n"
        "bool "
        "mgl_clip_reject(){if(mgl_u_clip_enabled[0]!=0&&dot(mgl_ff_eye_position,mgl_u_clip_plane[0]"
        ")<0.0)return "
        "true;if(mgl_u_clip_enabled[1]!=0&&dot(mgl_ff_eye_position,mgl_u_clip_plane[1])<0.0)return "
        "true;if(mgl_u_clip_enabled[2]!=0&&dot(mgl_ff_eye_position,mgl_u_clip_plane[2])<0.0)return "
        "true;if(mgl_u_clip_enabled[3]!=0&&dot(mgl_ff_eye_position,mgl_u_clip_plane[3])<0.0)return "
        "true;if(mgl_u_clip_enabled[4]!=0&&dot(mgl_ff_eye_position,mgl_u_clip_plane[4])<0.0)return "
        "true;if(mgl_u_clip_enabled[5]!=0&&dot(mgl_ff_eye_position,mgl_u_clip_plane[5])<0.0)return "
        "true;return false;}\n"
        "bool mgl_alpha_test_pass(float a){ if(mgl_u_alpha_test_func==512) return false; "
        "if(mgl_u_alpha_test_func==513) return a<mgl_u_alpha_test_ref; "
        "if(mgl_u_alpha_test_func==514) return a==mgl_u_alpha_test_ref; "
        "if(mgl_u_alpha_test_func==515) return a<=mgl_u_alpha_test_ref; "
        "if(mgl_u_alpha_test_func==516) return a>mgl_u_alpha_test_ref; "
        "if(mgl_u_alpha_test_func==517) return a!=mgl_u_alpha_test_ref; "
        "if(mgl_u_alpha_test_func==518) return a>=mgl_u_alpha_test_ref; return true; }\n"
        "vec4 mgl_apply_fog(vec4 c){ if(mgl_u_fog_enabled==0) return c; float f; "
        "if(mgl_u_fog_mode==2048) f=exp(-mgl_u_fog_density*mgl_ff_fog_coord); else "
        "if(mgl_u_fog_mode==2049){ float d=mgl_u_fog_density*mgl_ff_fog_coord; f=exp(-(d*d)); } "
        "else f=(mgl_u_fog_end-mgl_ff_fog_coord)/max(mgl_u_fog_end-mgl_u_fog_start,0.000001); "
        "f=clamp(f,0.0,1.0); return vec4(mix(mgl_u_fog_color.rgb,c.rgb,f),c.a); }\n"
        "void main(){ if(mgl_clip_reject()) discard; gl_FragColor = mgl_apply_fog(mgl_ff_color); "
        "if(mgl_u_alpha_test_enabled!=0 && !mgl_alpha_test_pass(gl_FragColor.a)) discard; }\n";
    static const char vertex_textured[] =
        "#version 100\n"
        "uniform float mgl_u_point_size;\n"
        "attribute vec4 mgl_a_position;\n"
        "attribute vec4 mgl_a_normal;\n"
        "attribute vec4 mgl_a_color;\n"
        "attribute vec4 mgl_a_texcoord0;\n"
        "attribute float mgl_a_fog;\n"
        "uniform mat4 mgl_u_model_view_projection;\n"
        "uniform mat4 mgl_u_model_view; uniform mat3 mgl_u_normal_matrix;\n"
        "uniform int mgl_u_fog_coord_explicit;\n"
        "uniform int mgl_u_texgen_enabled[4]; uniform int mgl_u_texgen_mode[4]; uniform vec4 "
        "mgl_u_texgen_object_plane[4]; uniform vec4 mgl_u_texgen_eye_plane[4];\n"
        "varying vec4 mgl_ff_color;\n"
        "varying vec2 mgl_ff_texcoord0; varying vec4 mgl_ff_eye_position;\n"
        "varying float mgl_ff_fog_coord;\n"
        "vec4 mgl_texgen(vec4 obj,vec4 eye,vec3 normal,vec4 cur){vec4 tc=cur;vec3 "
        "N=normalize(mgl_u_normal_matrix*normal);vec3 U=normalize(eye.xyz);vec3 "
        "R=reflect(U,N);float mm=2.0*sqrt(max(R.x*R.x+R.y*R.y+(R.z+1.0)*(R.z+1.0),0.000001));float "
        "ss=R.x/mm+0.5;float st=R.y/mm+0.5;if(mgl_u_texgen_enabled[0]!=0){int "
        "m=mgl_u_texgen_mode[0];tc.x=m==9217?dot(obj,mgl_u_texgen_object_plane[0]):m==9216?dot(eye,"
        "mgl_u_texgen_eye_plane[0]):m==9218?ss:m==34065?N.x:R.x;}if(mgl_u_texgen_enabled[1]!=0){"
        "int "
        "m=mgl_u_texgen_mode[1];tc.y=m==9217?dot(obj,mgl_u_texgen_object_plane[1]):m==9216?dot(eye,"
        "mgl_u_texgen_eye_plane[1]):m==9218?st:m==34065?N.y:R.y;}if(mgl_u_texgen_enabled[2]!=0){"
        "int "
        "m=mgl_u_texgen_mode[2];tc.z=m==9217?dot(obj,mgl_u_texgen_object_plane[2]):m==9216?dot(eye,"
        "mgl_u_texgen_eye_plane[2]):m==34065?N.z:R.z;}if(mgl_u_texgen_enabled[3]!=0){int "
        "m=mgl_u_texgen_mode[3];tc.w=m==9217?dot(obj,mgl_u_texgen_object_plane[3]):dot(eye,mgl_u_"
        "texgen_eye_plane[3]);}return tc;}\n"
        "void main(){ vec4 eye=mgl_u_model_view*mgl_a_position; vec4 "
        "tc=mgl_texgen(mgl_a_position,eye,mgl_a_normal.xyz,mgl_a_texcoord0); gl_Position = "
        "mgl_u_model_view_projection * mgl_a_position;gl_PointSize=mgl_u_point_size; mgl_ff_color "
        "= mgl_a_color; mgl_ff_texcoord0 = tc.w!=0.0?tc.xy/tc.w:tc.xy; mgl_ff_eye_position=eye; "
        "mgl_ff_fog_coord=mgl_u_fog_coord_explicit!=0?mgl_a_fog:abs(eye.z); }\n";
    static const char fragment_textured[] =
        "#version 100\n"
        "precision mediump float;\n"
        "varying vec4 mgl_ff_color;\n"
        "varying vec2 mgl_ff_texcoord0;\n"
        "varying vec4 mgl_ff_eye_position;\n"
        "varying float mgl_ff_fog_coord;\n"
        "uniform sampler2D mgl_u_texture0;\n"
        "uniform vec2 mgl_u_rect_scale0;\n"
        "uniform int mgl_u_texture_env_mode0;\n"
        "uniform int mgl_u_alpha_test_enabled;\n"
        "uniform int mgl_u_alpha_test_func;\n"
        "uniform float mgl_u_alpha_test_ref;\n"
        "uniform int mgl_u_fog_enabled;\n"
        "uniform int mgl_u_fog_mode;\n"
        "uniform float mgl_u_fog_density;\n"
        "uniform float mgl_u_fog_start;\n"
        "uniform float mgl_u_fog_end;\n"
        "uniform vec4 mgl_u_fog_color;\n"
        "uniform int mgl_u_clip_enabled[6]; uniform vec4 mgl_u_clip_plane[6];\n"
        "bool "
        "mgl_clip_reject(){if(mgl_u_clip_enabled[0]!=0&&dot(mgl_ff_eye_position,mgl_u_clip_plane[0]"
        ")<0.0)return "
        "true;if(mgl_u_clip_enabled[1]!=0&&dot(mgl_ff_eye_position,mgl_u_clip_plane[1])<0.0)return "
        "true;if(mgl_u_clip_enabled[2]!=0&&dot(mgl_ff_eye_position,mgl_u_clip_plane[2])<0.0)return "
        "true;if(mgl_u_clip_enabled[3]!=0&&dot(mgl_ff_eye_position,mgl_u_clip_plane[3])<0.0)return "
        "true;if(mgl_u_clip_enabled[4]!=0&&dot(mgl_ff_eye_position,mgl_u_clip_plane[4])<0.0)return "
        "true;if(mgl_u_clip_enabled[5]!=0&&dot(mgl_ff_eye_position,mgl_u_clip_plane[5])<0.0)return "
        "true;return false;}\n"
        "bool mgl_alpha_test_pass(float a){ if(mgl_u_alpha_test_func==512) return false; "
        "if(mgl_u_alpha_test_func==513) return a<mgl_u_alpha_test_ref; "
        "if(mgl_u_alpha_test_func==514) return a==mgl_u_alpha_test_ref; "
        "if(mgl_u_alpha_test_func==515) return a<=mgl_u_alpha_test_ref; "
        "if(mgl_u_alpha_test_func==516) return a>mgl_u_alpha_test_ref; "
        "if(mgl_u_alpha_test_func==517) return a!=mgl_u_alpha_test_ref; "
        "if(mgl_u_alpha_test_func==518) return a>=mgl_u_alpha_test_ref; return true; }\n"
        "vec4 mgl_apply_fog(vec4 c){ if(mgl_u_fog_enabled==0) return c; float f; "
        "if(mgl_u_fog_mode==2048) f=exp(-mgl_u_fog_density*mgl_ff_fog_coord); else "
        "if(mgl_u_fog_mode==2049){ float d=mgl_u_fog_density*mgl_ff_fog_coord; f=exp(-(d*d)); } "
        "else f=(mgl_u_fog_end-mgl_ff_fog_coord)/max(mgl_u_fog_end-mgl_u_fog_start,0.000001); "
        "f=clamp(f,0.0,1.0); return vec4(mix(mgl_u_fog_color.rgb,c.rgb,f),c.a); }\n"
        "vec4 mgl_apply_texenv(vec4 p, vec4 t){ if(mgl_u_texture_env_mode0==7681) return t; "
        "if(mgl_u_texture_env_mode0==8449) return vec4(mix(p.rgb,t.rgb,t.a),p.a); "
        "if(mgl_u_texture_env_mode0==260) return vec4(clamp(p.rgb+t.rgb,0.0,1.0),p.a*t.a); return "
        "p*t; }\n"
        "void main(){ if(mgl_clip_reject()) discard; vec4 t=texture2D(mgl_u_texture0, "
        "mgl_ff_texcoord0*mgl_u_rect_scale0); gl_FragColor = "
        "mgl_apply_fog(mgl_apply_texenv(mgl_ff_color,t)); if(mgl_u_alpha_test_enabled!=0 && "
        "!mgl_alpha_test_pass(gl_FragColor.a)) discard; }\n";
    const char *vertex_source;
    const char *fragment_source;
    struct mgl_program *program;
    GLint status = GL_FALSE;
    unsigned i;
    unsigned key;
    unsigned texture_signature;
    struct mgl_fixed_program_variant *restore_slot = NULL;
    if (ctx == NULL || ctx->native_dispatch == NULL)
        return NULL;
    if (ctx->compat.fixed_variant_key_valid) {
        mgl_perf_diag_program_cache(ctx, MGL_PERF_FIXED_KEY_HIT);
        key = ctx->compat.fixed_variant_key;
    } else {
        mgl_perf_diag_program_cache(ctx, MGL_PERF_FIXED_KEY_REBUILD);
        key = (mgl_compat_selected_texture_target(&ctx->compat, 0u) != 0u ? 1u : 0u) |
              (ctx->compat.lighting_enabled ? 2u : 0u);
        for (i = 0u; i < MGL_TEXTURE_UNITS; ++i) {
            const GLfloat *texmatrix = mgl_compat_texture_matrix(&ctx->compat, i);
            unsigned coord, elem;
            GLenum target = mgl_compat_selected_texture_target(&ctx->compat, i);
            if (target == 0u)
                continue;
            if (target == GL_TEXTURE_1D || target == GL_TEXTURE_3D ||
                target == GL_TEXTURE_CUBE_MAP || i > 0u ||
                ctx->compat.texture_env_mode[i] == GL_COMBINE ||
                ctx->compat.texture_env_mode[i] == GL_BLEND)
                key |= 4u;
            for (coord = 0u; coord < MGL_TEXGEN_COORDS; ++coord)
                if (ctx->compat.texgen[i][coord].enabled)
                    key |= 4u;
            for (elem = 0u; elem < 16u; ++elem)
                if (texmatrix[elem] != (elem % 5u == 0u ? 1.0f : 0.0f))
                    key |= 4u;
        }
        if (key & 4u)
            key = 4u;
        ctx->compat.fixed_variant_key = key;
        ctx->compat.fixed_variant_key_valid = GL_TRUE;
    }
    texture_signature = mgl_compat_texture_signature(&ctx->compat);
    if (ctx->fixed_program_ready && ctx->fixed_program_key == key &&
        (key != 4u || ctx->fixed_program_texture_signature == texture_signature)) {
        mgl_perf_diag_program_cache(ctx, MGL_PERF_FIXED_PROGRAM_HIT);
        mgl_perf_diag_program_cache(ctx, MGL_PERF_FIXED_CACHE_HIT);
        return &ctx->fixed_program;
    }
    if (key == 4u)
        restore_slot = fixed_complex_take(ctx, texture_signature);
    else if (ctx->fixed_program_variants[key].ready)
        restore_slot = &ctx->fixed_program_variants[key];
    fixed_save_active(ctx);
    if (restore_slot != NULL) {
        mgl_perf_diag_program_cache(ctx, MGL_PERF_FIXED_PROGRAM_RESTORE);
        mgl_perf_diag_program_cache(ctx, MGL_PERF_FIXED_CACHE_HIT);
        return fixed_restore(ctx, restore_slot, key);
    }
    mgl_perf_diag_program_cache(ctx, MGL_PERF_FIXED_CACHE_MISS);
    mgl_perf_diag_program_cache(ctx, MGL_PERF_FIXED_PROGRAM_BUILD);
    if (ctx->fixed_program_failed)
        ctx->fixed_program_failed = GL_FALSE;
    if (ctx->native_dispatch->glesCreateProgram == NULL ||
        ctx->native_dispatch->glesAttachShader == NULL ||
        ctx->native_dispatch->glesLinkProgram == NULL ||
        ctx->native_dispatch->glesGetProgramiv == NULL ||
        ctx->native_dispatch->glesGetAttribLocation == NULL ||
        ctx->native_dispatch->glesGetUniformLocation == NULL) {
        ctx->fixed_program_failed = GL_TRUE;
        return NULL;
    }
    if ((key & 2u) != 0u)
        vertex_source = (key & 1u) != 0u ? vertex_lit_textured : vertex_lit_untextured;
    else
        vertex_source = (key & 1u) != 0u ? vertex_textured : vertex_untextured;
    fragment_source = (key & 1u) != 0u ? fragment_textured : fragment_untextured;

    if ((key & 4u) != 0u) {
        mgl_perf_diag_program_cache(ctx, MGL_PERF_FIXED_SHADER_COMPILE);
        ctx->fixed_vertex_shader_native =
            mgl_compile_full_fixed_function_stage(ctx, GL_VERTEX_SHADER);
        mgl_perf_diag_program_cache(ctx, MGL_PERF_FIXED_SHADER_COMPILE);
        ctx->fixed_fragment_shader_native =
            mgl_compile_full_fixed_function_stage(ctx, GL_FRAGMENT_SHADER);
    } else {
        ctx->fixed_vertex_shader_native = compile_fixed_stage(ctx, GL_VERTEX_SHADER, vertex_source);
        ctx->fixed_fragment_shader_native =
            compile_fixed_stage(ctx, GL_FRAGMENT_SHADER, fragment_source);
    }
    if (ctx->fixed_vertex_shader_native == 0u || ctx->fixed_fragment_shader_native == 0u)
        goto fail;
    ctx->fixed_program_native = ctx->native_dispatch->glesCreateProgram();
    if (ctx->fixed_program_native == 0u)
        goto fail;
    ctx->native_dispatch->glesAttachShader(ctx->fixed_program_native,
                                           ctx->fixed_vertex_shader_native);
    ctx->native_dispatch->glesAttachShader(ctx->fixed_program_native,
                                           ctx->fixed_fragment_shader_native);
    mgl_perf_diag_program_cache(ctx, MGL_PERF_FIXED_PROGRAM_LINK);
    ctx->native_dispatch->glesLinkProgram(ctx->fixed_program_native);
    ctx->native_dispatch->glesGetProgramiv(ctx->fixed_program_native, GL_LINK_STATUS, &status);
    if (!status)
        goto fail;

    program = &ctx->fixed_program;
    memset(program, 0, sizeof(*program));
    program->native_name = ctx->fixed_program_native;
    program->linked = GL_TRUE;
    for (i = 0u; i < MGL_COMPAT_SEMANTIC_COUNT; ++i)
        program->compat_attrib[i] = -1;
    for (i = 0u; i < MGL_COMPAT_SEM_COUNT; ++i)
        program->compat_attrib[i] = ctx->native_dispatch->glesGetAttribLocation(
            program->native_name, mgl_compat_attribute_names[i]);
    program->compat_uniform_model_view_projection = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_model_view_projection");
    program->compat_uniform_model_view =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_model_view");
    program->compat_uniform_projection = -1;
    program->compat_uniform_normal_matrix =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_normal_matrix");
    for (i = 0u; i < MGL_TEXTURE_UNITS; ++i) {
        program->compat_uniform_texture_matrix[i] = -1;
        if ((key & 4u) != 0u) {
            char uniform_name[48];
            (void)snprintf(uniform_name, sizeof(uniform_name), "mgl_u_texture_matrix[%u]", i);
            program->compat_uniform_texture_matrix[i] =
                ctx->native_dispatch->glesGetUniformLocation(program->native_name, uniform_name);
        }
    }
    program->compat_uniform_sampler0 =
        (key & 1u) != 0u
            ? ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_texture0")
            : -1;
    for (i = 0u; i < 8u; ++i) {
        program->compat_uniform_rect_scale[i] = -1;
        if ((key & 4u) != 0u) {
            char name[40];
            (void)snprintf(name, sizeof(name), "mgl_u_rect_scale[%u]", i);
            program->compat_uniform_rect_scale[i] =
                ctx->native_dispatch->glesGetUniformLocation(program->native_name, name);
        } else if (i == 0u && (key & 1u) != 0u) {
            program->compat_uniform_rect_scale[i] = ctx->native_dispatch->glesGetUniformLocation(
                program->native_name, "mgl_u_rect_scale0");
        }
    }
    program->compat_uniform_ff_texture_enabled = -1;
    program->compat_uniform_alpha_enabled = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_alpha_test_enabled");
    program->compat_uniform_alpha_func =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_alpha_test_func");
    program->compat_uniform_alpha_ref =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_alpha_test_ref");
    program->compat_uniform_point_coord_flip = -1;
    program->compat_uniform_point_size =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_point_size");
    program->compat_uniform_program_point_size = -1;
    program->compat_uniform_texture_env_mode0 =
        (key & 1u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(program->native_name,
                                                                        "mgl_u_texture_env_mode0")
                         : -1;
    program->compat_env_enabled = -1;
    program->compat_env_mode = -1;
    program->compat_env_color = -1;
    program->compat_env_rgb_mode = -1;
    program->compat_env_alpha_mode = -1;
    program->compat_env_rgb_source = -1;
    program->compat_env_alpha_source = -1;
    program->compat_env_rgb_operand = -1;
    program->compat_env_alpha_operand = -1;
    program->compat_env_rgb_scale = -1;
    program->compat_env_alpha_scale = -1;
    program->compat_env_lod_bias = -1;
    for (i = 0u; i < 8u; ++i)
        program->compat_env_sampler[i] = -1;
    if ((key & 4u) != 0u) {
#define MGL_LOOKUP_ENV(field, name)                                                                \
    program->field = ctx->native_dispatch->glesGetUniformLocation(program->native_name, name)
        MGL_LOOKUP_ENV(compat_env_enabled, "mgl_u_env_enabled[0]");
        MGL_LOOKUP_ENV(compat_env_mode, "mgl_u_env_mode[0]");
        MGL_LOOKUP_ENV(compat_env_color, "mgl_u_env_color[0]");
        MGL_LOOKUP_ENV(compat_env_rgb_mode, "mgl_u_env_rgb_mode[0]");
        MGL_LOOKUP_ENV(compat_env_alpha_mode, "mgl_u_env_alpha_mode[0]");
        MGL_LOOKUP_ENV(compat_env_rgb_source, "mgl_u_env_rgb_source[0]");
        MGL_LOOKUP_ENV(compat_env_alpha_source, "mgl_u_env_alpha_source[0]");
        MGL_LOOKUP_ENV(compat_env_rgb_operand, "mgl_u_env_rgb_operand[0]");
        MGL_LOOKUP_ENV(compat_env_alpha_operand, "mgl_u_env_alpha_operand[0]");
        MGL_LOOKUP_ENV(compat_env_rgb_scale, "mgl_u_env_rgb_scale[0]");
        MGL_LOOKUP_ENV(compat_env_alpha_scale, "mgl_u_env_alpha_scale[0]");
        MGL_LOOKUP_ENV(compat_env_lod_bias, "mgl_u_env_lod_bias[0]");
#undef MGL_LOOKUP_ENV
        for (i = 0u; i < 8u; ++i) {
            char uniform_name[32];
            (void)snprintf(uniform_name, sizeof(uniform_name), "mgl_u_texture%u", i);
            program->compat_env_sampler[i] =
                ctx->native_dispatch->glesGetUniformLocation(program->native_name, uniform_name);
        }
    }
    program->compat_uniform_texgen_enabled =
        (key & 1u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(program->native_name,
                                                                        "mgl_u_texgen_enabled[0]")
                         : -1;
    program->compat_uniform_texgen_mode =
        (key & 1u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(program->native_name,
                                                                        "mgl_u_texgen_mode[0]")
                         : -1;
    program->compat_uniform_texgen_object_plane =
        (key & 1u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(
                               program->native_name, "mgl_u_texgen_object_plane[0]")
                         : -1;
    program->compat_uniform_texgen_eye_plane =
        (key & 1u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(program->native_name,
                                                                        "mgl_u_texgen_eye_plane[0]")
                         : -1;
    program->compat_uniform_clip_enabled =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_clip_enabled[0]");
    program->compat_uniform_clip_plane =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_clip_plane[0]");
    program->compat_uniform_fog_enabled =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_fog_enabled");
    program->compat_uniform_fog_mode =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_fog_mode");
    program->compat_uniform_fog_density =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_fog_density");
    program->compat_uniform_fog_start =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_fog_start");
    program->compat_uniform_fog_end =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_fog_end");
    program->compat_uniform_fog_color =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_fog_color");
    program->compat_uniform_fog_coord_explicit = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_fog_coord_explicit");
    program->compat_uniform_normalize_enabled = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_normalize_enabled");
    program->compat_uniform_lighting_enabled =
        (key & 4u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(program->native_name,
                                                                        "mgl_u_lighting_enabled")
                         : -1;
    program->compat_uniform_implicit_texgen_enabled =
        (key & 4u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(
                               program->native_name, "mgl_u_implicit_texgen_enabled[0]")
                         : -1;
    program->compat_uniform_implicit_texgen_mode =
        (key & 4u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(
                               program->native_name, "mgl_u_implicit_texgen_mode[0]")
                         : -1;
    program->compat_uniform_implicit_texgen_object_plane =
        (key & 4u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(
                               program->native_name, "mgl_u_implicit_texgen_object_plane[0]")
                         : -1;
    program->compat_uniform_implicit_texgen_eye_plane =
        (key & 4u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(
                               program->native_name, "mgl_u_implicit_texgen_eye_plane[0]")
                         : -1;
    program->compat_uniform_light_enabled = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_enabled[0]");
    program->compat_uniform_light_ambient = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_ambient[0]");
    program->compat_uniform_light_diffuse = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_diffuse[0]");
    program->compat_uniform_light_specular = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_specular[0]");
    program->compat_uniform_light_position = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_position[0]");
    program->compat_uniform_light_spot_direction = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_spot_direction[0]");
    program->compat_uniform_light_spot_exponent = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_spot_exponent[0]");
    program->compat_uniform_light_spot_cutoff = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_spot_cutoff[0]");
    program->compat_uniform_light_constant_attenuation =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name,
                                                     "mgl_u_light_constant_attenuation[0]");
    program->compat_uniform_light_linear_attenuation = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_linear_attenuation[0]");
    program->compat_uniform_light_quadratic_attenuation =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name,
                                                     "mgl_u_light_quadratic_attenuation[0]");
    program->compat_uniform_light_model_ambient = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_model_ambient");
    program->compat_uniform_light_model_local_viewer = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_model_local_viewer");
    program->compat_uniform_light_model_two_side =
        (key & 4u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(
                               program->native_name, "mgl_u_light_model_two_side")
                         : -1;
    program->compat_uniform_material_ambient = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_ambient");
    program->compat_uniform_material_diffuse = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_diffuse");
    program->compat_uniform_material_specular = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_specular");
    program->compat_uniform_material_emission = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_emission");
    program->compat_uniform_material_shininess = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_shininess");
    program->compat_uniform_material_back_ambient =
        (key & 4u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(
                               program->native_name, "mgl_u_material_back_ambient")
                         : -1;
    program->compat_uniform_material_back_diffuse =
        (key & 4u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(
                               program->native_name, "mgl_u_material_back_diffuse")
                         : -1;
    program->compat_uniform_material_back_specular =
        (key & 4u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(
                               program->native_name, "mgl_u_material_back_specular")
                         : -1;
    program->compat_uniform_material_back_emission =
        (key & 4u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(
                               program->native_name, "mgl_u_material_back_emission")
                         : -1;
    program->compat_uniform_material_back_shininess =
        (key & 4u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(
                               program->native_name, "mgl_u_material_back_shininess")
                         : -1;
    program->compat_uniform_color_material_enabled = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_color_material_enabled");
    program->compat_uniform_color_material_mode = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_color_material_mode");
    program->compat_uniform_color_material_face =
        (key & 4u) != 0u ? ctx->native_dispatch->glesGetUniformLocation(program->native_name,
                                                                        "mgl_u_color_material_face")
                         : -1;
    ctx->fixed_program_key = key;
    ctx->fixed_program_texture_signature = texture_signature;
    ctx->fixed_program_ready = GL_TRUE;
    return program;

fail:
    mgl_compat_destroy_fixed_program(ctx);
    ctx->fixed_program_failed = GL_TRUE;
    return NULL;
}

void mgl_compat_destroy_fixed_program(struct mgl_context *ctx) {
    unsigned key;
    if (ctx == NULL || ctx->native_dispatch == NULL)
        return;
    if (ctx->fixed_program_native != 0u && ctx->native_dispatch->glesDeleteProgram != NULL)
        ctx->native_dispatch->glesDeleteProgram(ctx->fixed_program_native);
    if (ctx->fixed_vertex_shader_native != 0u && ctx->native_dispatch->glesDeleteShader != NULL)
        ctx->native_dispatch->glesDeleteShader(ctx->fixed_vertex_shader_native);
    if (ctx->fixed_fragment_shader_native != 0u && ctx->native_dispatch->glesDeleteShader != NULL)
        ctx->native_dispatch->glesDeleteShader(ctx->fixed_fragment_shader_native);
    ctx->fixed_program_native = 0u;
    ctx->fixed_vertex_shader_native = 0u;
    ctx->fixed_fragment_shader_native = 0u;
    ctx->fixed_program_ready = GL_FALSE;
    ctx->fixed_program_key = 0u;
    ctx->fixed_program_texture_signature = 0u;
    free(ctx->fixed_program.compat_uniform_cache);
    memset(&ctx->fixed_program, 0, sizeof(ctx->fixed_program));
    for (key = 0u; key < 5u; ++key)
        fixed_variant_dispose(ctx, &ctx->fixed_program_variants[key]);
    while (ctx->fixed_complex_programs != NULL) {
        struct mgl_fixed_program_variant *slot = ctx->fixed_complex_programs;
        ctx->fixed_complex_programs = slot->next;
        fixed_variant_dispose(ctx, slot);
        free(slot);
    }
    ctx->fixed_complex_program_count = 0u;
    ctx->draw_native_program_valid = GL_FALSE;
}

static int mgl_texgen_coord_index(GLenum coord) {
    if (coord < GL_S || coord > GL_Q)
        return -1;
    return (int)(coord - GL_S);
}

static int mgl_texgen_mode_valid(unsigned coord, GLenum mode) {
    if (mode == GL_OBJECT_LINEAR || mode == GL_EYE_LINEAR)
        return 1;
    if ((coord == 0u || coord == 1u) && mode == GL_SPHERE_MAP)
        return 1;
    if (coord < 3u && (mode == GL_NORMAL_MAP || mode == GL_REFLECTION_MAP))
        return 1;
    return 0;
}

void glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glTexGenfv",
                        (const struct mgl_error_arg[]){{"coord", (uint64_t)(uintptr_t)coord},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    struct mgl_texgen_state *tg;
    int index;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_texgen(ctx, coord, pname, params,
                                   pname == GL_TEXTURE_GEN_MODE ? 1u : 4u))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    index = mgl_texgen_coord_index(coord);
    if (index < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (ctx->compat.active_texture >= MGL_TEXTURE_UNITS) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    tg = &ctx->compat.texgen[ctx->compat.active_texture][(unsigned)index];
    if (pname == GL_TEXTURE_GEN_MODE) {
        GLenum mode = (GLenum)(GLint)params[0];
        if (!mgl_texgen_mode_valid((unsigned)index, mode)) {
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
        }
        if (mgl_list_record_texgen(ctx, coord, pname, params, 1u))
            return;
        tg->mode = mode;
    } else if (pname == GL_OBJECT_PLANE) {
        if (mgl_list_record_texgen(ctx, coord, pname, params, 4u))
            return;
        memcpy(tg->object_plane, params, 4u * sizeof(GLfloat));
    } else if (pname == GL_EYE_PLANE) {
        if (mgl_list_record_texgen(ctx, coord, pname, params, 4u))
            return;
        transform_plane_inverse_transpose(mgl_compat_modelview(&ctx->compat), params,
                                          tg->eye_plane);
    } else {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
    }
}

void glTexGenf(GLenum coord, GLenum pname, GLfloat param) {
    GLfloat values[4] = {param, 0.0f, 0.0f, 0.0f};
    glTexGenfv(coord, pname, values);
}

void glTexGeni(GLenum coord, GLenum pname, GLint param) {
    GLfloat values[4] = {(GLfloat)param, 0.0f, 0.0f, 0.0f};
    glTexGenfv(coord, pname, values);
}

void glTexGend(GLenum coord, GLenum pname, GLdouble param) {
    GLfloat values[4] = {(GLfloat)param, 0.0f, 0.0f, 0.0f};
    glTexGenfv(coord, pname, values);
}

void glTexGeniv(GLenum coord, GLenum pname, const GLint *params) {
    GLfloat v[4];
    unsigned i, count = pname == GL_TEXTURE_GEN_MODE ? 1u : 4u;
    if (params == NULL)
        return;
    for (i = 0u; i < count; ++i)
        v[i] = (GLfloat)params[i];
    glTexGenfv(coord, pname, v);
}

void glTexGendv(GLenum coord, GLenum pname, const GLdouble *params) {
    GLfloat v[4];
    unsigned i, count = pname == GL_TEXTURE_GEN_MODE ? 1u : 4u;
    if (params == NULL)
        return;
    for (i = 0u; i < count; ++i)
        v[i] = (GLfloat)params[i];
    glTexGenfv(coord, pname, v);
}

void glGetTexGenfv(GLenum coord, GLenum pname, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetTexGenfv",
                        (const struct mgl_error_arg[]){{"coord", (uint64_t)(uintptr_t)coord},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    struct mgl_texgen_state *tg;
    int index;
    if (ctx == NULL || params == NULL)
        return;
    if (mgl_fog_reject_inside_begin(ctx))
        return;
    index = mgl_texgen_coord_index(coord);
    if (index < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (ctx->compat.active_texture >= MGL_TEXTURE_UNITS) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    tg = &ctx->compat.texgen[ctx->compat.active_texture][(unsigned)index];
    if (pname == GL_TEXTURE_GEN_MODE)
        params[0] = (GLfloat)tg->mode;
    else if (pname == GL_OBJECT_PLANE)
        memcpy(params, tg->object_plane, 4u * sizeof(GLfloat));
    else if (pname == GL_EYE_PLANE)
        memcpy(params, tg->eye_plane, 4u * sizeof(GLfloat));
    else
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
}

void glGetTexGeniv(GLenum coord, GLenum pname, GLint *params) {
    GLfloat v[4] = {0};
    unsigned i, count = pname == GL_TEXTURE_GEN_MODE ? 1u : 4u;
    if (params == NULL)
        return;
    {
        struct mgl_context *ctx = mgl_context_current();
        if (ctx == NULL || mgl_fog_reject_inside_begin(ctx))
            return;
    }
    glGetTexGenfv(coord, pname, v);
    for (i = 0u; i < count; ++i)
        params[i] = (GLint)v[i];
}

void glGetTexGendv(GLenum coord, GLenum pname, GLdouble *params) {
    GLfloat v[4] = {0};
    unsigned i, count = pname == GL_TEXTURE_GEN_MODE ? 1u : 4u;
    if (params == NULL)
        return;
    {
        struct mgl_context *ctx = mgl_context_current();
        if (ctx == NULL || mgl_fog_reject_inside_begin(ctx))
            return;
    }
    glGetTexGenfv(coord, pname, v);
    for (i = 0u; i < count; ++i)
        params[i] = (GLdouble)v[i];
}

static void mgl_multi_texgenfv_apply(struct mgl_context *ctx, unsigned unit, GLenum texunit,
                                     GLenum coord, GLenum pname, const GLfloat *params) {
    struct mgl_texgen_state *tg;
    unsigned count;
    int index;
    if (ctx == NULL || params == NULL)
        return;
    index = mgl_texgen_coord_index(coord);
    if (index < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    tg = &ctx->compat.texgen[unit][(unsigned)index];
    if (pname == GL_TEXTURE_GEN_MODE) {
        GLenum mode = (GLenum)(GLint)params[0];
        if (!mgl_texgen_mode_valid((unsigned)index, mode)) {
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
        }
        count = 1u;
        if (mgl_list_record_multi_texgen_ext(ctx, texunit, coord, pname, params, count))
            return;
        tg->mode = mode;
    } else if (pname == GL_OBJECT_PLANE) {
        count = 4u;
        if (mgl_list_record_multi_texgen_ext(ctx, texunit, coord, pname, params, count))
            return;
        memcpy(tg->object_plane, params, 4u * sizeof(GLfloat));
    } else if (pname == GL_EYE_PLANE) {
        count = 4u;
        if (mgl_list_record_multi_texgen_ext(ctx, texunit, coord, pname, params, count))
            return;
        transform_plane_inverse_transpose(mgl_compat_modelview(&ctx->compat), params,
                                          tg->eye_plane);
    } else {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
    }
}

void glMultiTexGenfvEXT(GLenum texunit, GLenum coord, GLenum pname, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned unit;
    if (ctx == NULL || params == NULL)
        return;
    if (mgl_fog_reject_inside_begin(ctx))
        return;
    if (!mgl_ext_texunit_index(ctx, texunit, &unit))
        return;
    mgl_multi_texgenfv_apply(ctx, unit, texunit, coord, pname, params);
}

void glMultiTexGenfEXT(GLenum texunit, GLenum coord, GLenum pname, GLfloat param) {
    GLfloat values[4] = {param, 0.0f, 0.0f, 0.0f};
    glMultiTexGenfvEXT(texunit, coord, pname, values);
}

void glMultiTexGeniEXT(GLenum texunit, GLenum coord, GLenum pname, GLint param) {
    GLfloat values[4] = {(GLfloat)param, 0.0f, 0.0f, 0.0f};
    glMultiTexGenfvEXT(texunit, coord, pname, values);
}

void glMultiTexGendEXT(GLenum texunit, GLenum coord, GLenum pname, GLdouble param) {
    GLfloat values[4] = {(GLfloat)param, 0.0f, 0.0f, 0.0f};
    glMultiTexGenfvEXT(texunit, coord, pname, values);
}

void glMultiTexGenivEXT(GLenum texunit, GLenum coord, GLenum pname, const GLint *params) {
    GLfloat values[4];
    unsigned i, count = pname == GL_TEXTURE_GEN_MODE ? 1u : 4u;
    if (params == NULL)
        return;
    for (i = 0u; i < count; ++i)
        values[i] = (GLfloat)params[i];
    glMultiTexGenfvEXT(texunit, coord, pname, values);
}

void glMultiTexGendvEXT(GLenum texunit, GLenum coord, GLenum pname, const GLdouble *params) {
    GLfloat values[4];
    unsigned i, count = pname == GL_TEXTURE_GEN_MODE ? 1u : 4u;
    if (params == NULL)
        return;
    for (i = 0u; i < count; ++i)
        values[i] = (GLfloat)params[i];
    glMultiTexGenfvEXT(texunit, coord, pname, values);
}

void glGetMultiTexGenfvEXT(GLenum texunit, GLenum coord, GLenum pname, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetMultiTexGenfvEXT",
                        (const struct mgl_error_arg[]){{"texunit", (uint64_t)(uintptr_t)texunit},
                                                       {"coord", (uint64_t)(uintptr_t)coord},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        4u);
    struct mgl_texgen_state *tg;
    unsigned unit;
    int index;
    if (ctx == NULL || params == NULL)
        return;
    if (!mgl_ext_texunit_index(ctx, texunit, &unit))
        return;
    index = mgl_texgen_coord_index(coord);
    if (index < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    tg = &ctx->compat.texgen[unit][(unsigned)index];
    if (pname == GL_TEXTURE_GEN_MODE)
        params[0] = (GLfloat)tg->mode;
    else if (pname == GL_OBJECT_PLANE)
        memcpy(params, tg->object_plane, 4u * sizeof(GLfloat));
    else if (pname == GL_EYE_PLANE)
        memcpy(params, tg->eye_plane, 4u * sizeof(GLfloat));
    else
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
}

void glGetMultiTexGenivEXT(GLenum texunit, GLenum coord, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat values[4] = {0};
    unsigned i, count = pname == GL_TEXTURE_GEN_MODE ? 1u : 4u;
    if (ctx == NULL || params == NULL)
        return;
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetMultiTexGenfvEXT(texunit, coord, pname, values);
    if (ctx->errors.sequence != error_sequence_before)
        return;
    for (i = 0u; i < count; ++i)
        params[i] = (GLint)values[i];
}

void glGetMultiTexGendvEXT(GLenum texunit, GLenum coord, GLenum pname, GLdouble *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat values[4] = {0};
    unsigned i, count = pname == GL_TEXTURE_GEN_MODE ? 1u : 4u;
    if (ctx == NULL || params == NULL)
        return;
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetMultiTexGenfvEXT(texunit, coord, pname, values);
    if (ctx->errors.sequence != error_sequence_before)
        return;
    for (i = 0u; i < count; ++i)
        params[i] = (GLdouble)values[i];
}

void glClipPlane(GLenum plane, const GLdouble *equation) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glClipPlane",
                        (const struct mgl_error_arg[]){{"plane", (uint64_t)(uintptr_t)plane},
                                                       {"equation", (uint64_t)(uintptr_t)equation}},
                        2u);
    GLfloat eq[4], transformed[4];
    unsigned i, index;
    if (ctx == NULL || equation == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying) {
            for (i = 0u; i < 4u; ++i)
                eq[i] = (GLfloat)equation[i];
            if (mgl_list_record_clip_plane(ctx, plane, eq))
                return;
        }
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (plane < GL_CLIP_PLANE0 || plane >= GL_CLIP_PLANE0 + MGL_MAX_CLIP_PLANES) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    index = plane - GL_CLIP_PLANE0;
    for (i = 0u; i < 4u; ++i)
        eq[i] = (GLfloat)equation[i];
    if (mgl_list_record_clip_plane(ctx, plane, eq))
        return;
    transform_plane_inverse_transpose(mgl_compat_modelview(&ctx->compat), eq, transformed);
    memcpy(ctx->compat.clip_plane[index], transformed, sizeof(transformed));
}

void glClipPlanef(GLenum plane, const GLfloat *equation) {
    GLdouble converted[4];
    unsigned i;
    if (equation == NULL)
        return;
    for (i = 0u; i < 4u; ++i)
        converted[i] = (GLdouble)equation[i];
    glClipPlane(plane, converted);
}

void glGetClipPlane(GLenum plane, GLdouble *equation) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetClipPlane",
                        (const struct mgl_error_arg[]){{"plane", (uint64_t)(uintptr_t)plane},
                                                       {"equation", (uint64_t)(uintptr_t)equation}},
                        2u);
    unsigned i, index;
    if (ctx == NULL || equation == NULL)
        return;
    if (mgl_fog_reject_inside_begin(ctx))
        return;
    if (plane < GL_CLIP_PLANE0 || plane >= GL_CLIP_PLANE0 + MGL_MAX_CLIP_PLANES) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    index = plane - GL_CLIP_PLANE0;
    for (i = 0u; i < 4u; ++i)
        equation[i] = (GLdouble)ctx->compat.clip_plane[index][i];
}

void glGetClipPlanef(GLenum plane, GLfloat *equation) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetClipPlanef",
                        (const struct mgl_error_arg[]){{"plane", (uint64_t)(uintptr_t)plane},
                                                       {"equation", (uint64_t)(uintptr_t)equation}},
                        2u);
    unsigned i, index;
    if (ctx == NULL || equation == NULL)
        return;
    if (mgl_fog_reject_inside_begin(ctx))
        return;
    if (plane < GL_CLIP_PLANE0 || plane >= GL_CLIP_PLANE0 + MGL_MAX_CLIP_PLANES) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    index = plane - GL_CLIP_PLANE0;
    for (i = 0u; i < 4u; ++i)
        equation[i] = ctx->compat.clip_plane[index][i];
}

static int mgl_light_index(GLenum light) {
    if (light < GL_LIGHT0 || light >= GL_LIGHT0 + MGL_MAX_LIGHTS)
        return -1;
    return (int)(light - GL_LIGHT0);
}

static void mgl_transform_vec4(const GLfloat m[16], const GLfloat in[4], GLfloat out[4]) {
    unsigned r;
    for (r = 0u; r < 4u; ++r)
        out[r] = m[0u * 4u + r] * in[0] + m[1u * 4u + r] * in[1] + m[2u * 4u + r] * in[2] +
                 m[3u * 4u + r] * in[3];
}

static int mgl_light_scalar_pname(GLenum pname) {
    return pname == GL_SPOT_EXPONENT || pname == GL_SPOT_CUTOFF ||
           pname == GL_CONSTANT_ATTENUATION || pname == GL_LINEAR_ATTENUATION ||
           pname == GL_QUADRATIC_ATTENUATION;
}

static int mgl_light_param_count(GLenum pname, unsigned *count) {
    if (count == NULL)
        return 0;
    switch (pname) {
        case GL_AMBIENT:
        case GL_DIFFUSE:
        case GL_SPECULAR:
        case GL_POSITION:
            *count = 4u;
            return 1;
        case GL_SPOT_DIRECTION:
            *count = 3u;
            return 1;
        case GL_SPOT_EXPONENT:
        case GL_SPOT_CUTOFF:
        case GL_CONSTANT_ATTENUATION:
        case GL_LINEAR_ATTENUATION:
        case GL_QUADRATIC_ATTENUATION:
            *count = 1u;
            return 1;
        default:
            return 0;
    }
}

static int mgl_light_params_valid(struct mgl_context *ctx, GLenum pname, const GLfloat *params) {
    switch (pname) {
        case GL_SPOT_EXPONENT:
            if (params[0] < 0.0f || params[0] > 128.0f) {
                mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
                return 0;
            }
            break;
        case GL_SPOT_CUTOFF:
            if (params[0] != 180.0f && (params[0] < 0.0f || params[0] > 90.0f)) {
                mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
                return 0;
            }
            break;
        case GL_CONSTANT_ATTENUATION:
        case GL_LINEAR_ATTENUATION:
        case GL_QUADRATIC_ATTENUATION:
            if (params[0] < 0.0f) {
                mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
                return 0;
            }
            break;
        default:
            break;
    }
    return 1;
}

static int mgl_lighting_reject_inside_begin(struct mgl_context *ctx) {
    if (ctx != NULL && ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 1;
    }
    return 0;
}

/* A list compiled inside Begin/End records the command but defers its error
 * to replay. Immediate execution, including COMPILE_AND_EXECUTE, rejects it. */
static int mgl_light_record_illegal_begin(struct mgl_context *ctx, GLenum light, GLenum pname,
                                          const GLfloat *params, unsigned count) {
    if (!ctx->immediate.active)
        return 0;
    if (ctx->compiling_list != NULL && !ctx->list_replaying &&
        mgl_list_record_light(ctx, light, pname, params, count))
        return 1;
    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    return 1;
}

static int mgl_light_color_pname(GLenum pname) {
    return pname == GL_AMBIENT || pname == GL_DIFFUSE || pname == GL_SPECULAR;
}

static int mgl_material_color_pname(GLenum pname) {
    return pname == GL_AMBIENT || pname == GL_DIFFUSE || pname == GL_SPECULAR ||
           pname == GL_EMISSION || pname == GL_AMBIENT_AND_DIFFUSE;
}

static GLint mgl_color_to_signed_int(GLfloat value) {
    const double scale = 2.0 * (double)INT_MAX + 1.0;
    const double converted = (scale * (double)value - 1.0) / 2.0;
    if (converted <= (double)INT_MIN)
        return INT_MIN;
    if (converted >= (double)INT_MAX)
        return INT_MAX;
    return (GLint)converted;
}

static GLint mgl_float_to_nearest_int(GLfloat value) {
    const double converted = round((double)value);
    if (converted <= (double)INT_MIN)
        return INT_MIN;
    if (converted >= (double)INT_MAX)
        return INT_MAX;
    return (GLint)converted;
}

void glLightfv(GLenum light, GLenum pname, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glLightfv",
                        (const struct mgl_error_arg[]){{"light", (uint64_t)(uintptr_t)light},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    struct mgl_light_state *l;
    unsigned count;
    int idx;
    if (ctx == NULL || params == NULL)
        return;
    count = 1u;
    (void)mgl_light_param_count(pname, &count);
    if (mgl_light_record_illegal_begin(ctx, light, pname, params, count))
        return;
    idx = mgl_light_index(light);
    if (idx < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!mgl_light_param_count(pname, &count)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!mgl_light_params_valid(ctx, pname, params))
        return;
    l = &ctx->compat.lights[idx];
    if (mgl_list_record_light(ctx, light, pname, params, count))
        return;
    switch (pname) {
        case GL_AMBIENT:
            memcpy(l->ambient, params, 4u * sizeof(GLfloat));
            break;
        case GL_DIFFUSE:
            memcpy(l->diffuse, params, 4u * sizeof(GLfloat));
            break;
        case GL_SPECULAR:
            memcpy(l->specular, params, 4u * sizeof(GLfloat));
            break;
        case GL_POSITION:
            mgl_transform_vec4(mgl_compat_modelview(&ctx->compat), params, l->position);
            break;
        case GL_SPOT_DIRECTION: {
            GLfloat normal[9];
            mgl_compat_normal_matrix(&ctx->compat, normal);
            l->spot_direction[0] =
                normal[0] * params[0] + normal[3] * params[1] + normal[6] * params[2];
            l->spot_direction[1] =
                normal[1] * params[0] + normal[4] * params[1] + normal[7] * params[2];
            l->spot_direction[2] =
                normal[2] * params[0] + normal[5] * params[1] + normal[8] * params[2];
            break;
        }
        case GL_SPOT_EXPONENT:
            l->spot_exponent = params[0];
            break;
        case GL_SPOT_CUTOFF:
            l->spot_cutoff = params[0];
            break;
        case GL_CONSTANT_ATTENUATION:
            l->constant_attenuation = params[0];
            break;
        case GL_LINEAR_ATTENUATION:
            l->linear_attenuation = params[0];
            break;
        case GL_QUADRATIC_ATTENUATION:
            l->quadratic_attenuation = params[0];
            break;
        default:
            return;
    }
}

void glLightf(GLenum light, GLenum pname, GLfloat param) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glLightf",
                        (const struct mgl_error_arg[]){{"light", (uint64_t)(uintptr_t)light},
                                                       {"pname", (uint64_t)(uintptr_t)pname}},
                        2u);
    if (ctx == NULL)
        return;
    if (mgl_light_record_illegal_begin(ctx, light, pname, &param, 1u))
        return;
    if (!mgl_light_scalar_pname(pname)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    glLightfv(light, pname, &param);
}

void glLighti(GLenum light, GLenum pname, GLint param) {
    glLightf(light, pname, (GLfloat)param);
}

void glLightiv(GLenum light, GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glLightiv",
                        (const struct mgl_error_arg[]){{"light", (uint64_t)(uintptr_t)light},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    GLfloat v[4];
    unsigned i, n;
    if (ctx == NULL || params == NULL)
        return;
    n = 1u;
    (void)mgl_light_param_count(pname, &n);
    for (i = 0u; i < n; ++i)
        v[i] =
            mgl_light_color_pname(pname) ? mgl_signed_int_to_color(params[i]) : (GLfloat)params[i];
    if (mgl_light_record_illegal_begin(ctx, light, pname, v, n))
        return;
    if (!mgl_light_param_count(pname, &n)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    glLightfv(light, pname, v);
}

void glGetLightfv(GLenum light, GLenum pname, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetLightfv",
                        (const struct mgl_error_arg[]){{"light", (uint64_t)(uintptr_t)light},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    struct mgl_light_state *l;
    int idx;
    if (ctx == NULL || params == NULL)
        return;
    if (mgl_lighting_reject_inside_begin(ctx))
        return;
    idx = mgl_light_index(light);
    if (idx < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    l = &ctx->compat.lights[idx];
    switch (pname) {
        case GL_AMBIENT:
            memcpy(params, l->ambient, 4u * sizeof(GLfloat));
            break;
        case GL_DIFFUSE:
            memcpy(params, l->diffuse, 4u * sizeof(GLfloat));
            break;
        case GL_SPECULAR:
            memcpy(params, l->specular, 4u * sizeof(GLfloat));
            break;
        case GL_POSITION:
            memcpy(params, l->position, 4u * sizeof(GLfloat));
            break;
        case GL_SPOT_DIRECTION:
            memcpy(params, l->spot_direction, 3u * sizeof(GLfloat));
            break;
        case GL_SPOT_EXPONENT:
            params[0] = l->spot_exponent;
            break;
        case GL_SPOT_CUTOFF:
            params[0] = l->spot_cutoff;
            break;
        case GL_CONSTANT_ATTENUATION:
            params[0] = l->constant_attenuation;
            break;
        case GL_LINEAR_ATTENUATION:
            params[0] = l->linear_attenuation;
            break;
        case GL_QUADRATIC_ATTENUATION:
            params[0] = l->quadratic_attenuation;
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
}

void glGetLightiv(GLenum light, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetLightiv",
                        (const struct mgl_error_arg[]){{"light", (uint64_t)(uintptr_t)light},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    GLfloat v[4] = {0};
    unsigned i, n;
    if (ctx == NULL || params == NULL)
        return;
    if (mgl_lighting_reject_inside_begin(ctx))
        return;
    if (mgl_light_index(light) < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!mgl_light_param_count(pname, &n)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    glGetLightfv(light, pname, v);
    for (i = 0u; i < n; ++i)
        params[i] = mgl_light_color_pname(pname) ? mgl_color_to_signed_int(v[i])
                                                 : mgl_float_to_nearest_int(v[i]);
}

void glLightModelfv(GLenum pname, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glLightModelfv",
                        (const struct mgl_error_arg[]){{"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        2u);
    unsigned count;
    if (ctx == NULL || params == NULL)
        return;
    count = pname == GL_LIGHT_MODEL_AMBIENT ? 4u : 1u;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_light_model(ctx, pname, params, count))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_light_model(ctx, pname, params, count))
        return;
    switch (pname) {
        case GL_LIGHT_MODEL_AMBIENT:
            memcpy(ctx->compat.light_model_ambient, params, 4u * sizeof(GLfloat));
            break;
        case GL_LIGHT_MODEL_LOCAL_VIEWER:
            ctx->compat.light_model_local_viewer = params[0] != 0.0f;
            break;
        case GL_LIGHT_MODEL_TWO_SIDE:
            ctx->compat.light_model_two_side = params[0] != 0.0f;
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
}

void glLightModelf(GLenum pname, GLfloat param) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glLightModelf",
                        (const struct mgl_error_arg[]){{"pname", (uint64_t)(uintptr_t)pname}}, 1u);
    if (ctx == NULL)
        return;
    if (pname == GL_LIGHT_MODEL_AMBIENT) {
        mgl_error_set(&ctx->errors, ctx->immediate.active ? GL_INVALID_OPERATION : GL_INVALID_ENUM);
        return;
    }
    glLightModelfv(pname, &param);
}

void glLightModeli(GLenum pname, GLint param) {
    glLightModelf(pname, (GLfloat)param);
}

void glLightModeliv(GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat values[4];
    unsigned i, count = pname == GL_LIGHT_MODEL_AMBIENT ? 4u : 1u;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        /* Record copied values during GL_COMPILE; replay validates legality. */
        if (ctx->compiling_list != NULL && !ctx->list_replaying) {
            for (i = 0u; i < count; ++i)
                values[i] = (GLfloat)params[i];
            if (mgl_list_record_light_model(ctx, pname, values, count))
                return;
        }
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    for (i = 0u; i < count; ++i)
        values[i] = (GLfloat)params[i];
    glLightModelfv(pname, values);
}

static struct mgl_material_state *mgl_material_for_face(struct mgl_context *ctx, GLenum face) {
    if (face == GL_FRONT)
        return &ctx->compat.material_front;
    if (face == GL_BACK)
        return &ctx->compat.material_back;
    return NULL;
}

static void mgl_set_material_one(struct mgl_material_state *m, GLenum pname, const GLfloat *p) {
    switch (pname) {
        case GL_AMBIENT:
            memcpy(m->ambient, p, 4u * sizeof(GLfloat));
            break;
        case GL_DIFFUSE:
            memcpy(m->diffuse, p, 4u * sizeof(GLfloat));
            break;
        case GL_SPECULAR:
            memcpy(m->specular, p, 4u * sizeof(GLfloat));
            break;
        case GL_EMISSION:
            memcpy(m->emission, p, 4u * sizeof(GLfloat));
            break;
        case GL_AMBIENT_AND_DIFFUSE:
            memcpy(m->ambient, p, 4u * sizeof(GLfloat));
            memcpy(m->diffuse, p, 4u * sizeof(GLfloat));
            break;
        case GL_SHININESS:
            m->shininess = p[0];
            break;
        default:
            break;
    }
}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glMaterialfv",
                        (const struct mgl_error_arg[]){{"face", (uint64_t)(uintptr_t)face},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    unsigned count;
    if (ctx == NULL || params == NULL)
        return;
    if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (pname != GL_AMBIENT && pname != GL_DIFFUSE && pname != GL_SPECULAR &&
        pname != GL_EMISSION && pname != GL_AMBIENT_AND_DIFFUSE && pname != GL_SHININESS) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (pname == GL_SHININESS && (params[0] < 0.0f || params[0] > 128.0f)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    count = pname == GL_SHININESS ? 1u : 4u;
    if (mgl_list_record_material(ctx, face, pname, params, count))
        return;
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
        mgl_set_material_one(&ctx->compat.material_front, pname, params);
    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
        mgl_set_material_one(&ctx->compat.material_back, pname, params);
}

void glMaterialf(GLenum face, GLenum pname, GLfloat param) {
    if (pname != GL_SHININESS) {
        struct mgl_context *ctx = mgl_context_current();
        mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glMaterialf",
                            (const struct mgl_error_arg[]){{"face", (uint64_t)(uintptr_t)face},
                                                           {"pname", (uint64_t)(uintptr_t)pname}},
                            2u);
        if (ctx)
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    glMaterialfv(face, pname, &param);
}

void glMateriali(GLenum face, GLenum pname, GLint param) {
    glMaterialf(face, pname, (GLfloat)param);
}

void glMaterialiv(GLenum face, GLenum pname, const GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glMaterialiv",
                        (const struct mgl_error_arg[]){{"face", (uint64_t)(uintptr_t)face},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    GLfloat v[4];
    unsigned i, n;
    if (ctx == NULL || params == NULL)
        return;
    if (pname == GL_SHININESS)
        n = 1u;
    else if (mgl_material_color_pname(pname))
        n = 4u;
    else {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    for (i = 0; i < n; ++i)
        v[i] = mgl_material_color_pname(pname) ? mgl_signed_int_to_color(params[i])
                                               : (GLfloat)params[i];
    glMaterialfv(face, pname, v);
}

void glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetMaterialfv",
                        (const struct mgl_error_arg[]){{"face", (uint64_t)(uintptr_t)face},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    struct mgl_material_state *m;
    if (ctx == NULL || params == NULL)
        return;
    if (mgl_lighting_reject_inside_begin(ctx))
        return;
    m = mgl_material_for_face(ctx, face);
    if (m == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    switch (pname) {
        case GL_AMBIENT:
            memcpy(params, m->ambient, 4u * sizeof(GLfloat));
            break;
        case GL_DIFFUSE:
            memcpy(params, m->diffuse, 4u * sizeof(GLfloat));
            break;
        case GL_SPECULAR:
            memcpy(params, m->specular, 4u * sizeof(GLfloat));
            break;
        case GL_EMISSION:
            memcpy(params, m->emission, 4u * sizeof(GLfloat));
            break;
        case GL_SHININESS:
            params[0] = m->shininess;
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
}

void glGetMaterialiv(GLenum face, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetMaterialiv",
                        (const struct mgl_error_arg[]){{"face", (uint64_t)(uintptr_t)face},
                                                       {"pname", (uint64_t)(uintptr_t)pname},
                                                       {"params", (uint64_t)(uintptr_t)params}},
                        3u);
    GLfloat v[4] = {0};
    unsigned i, n;
    if (ctx == NULL || params == NULL)
        return;
    if (mgl_lighting_reject_inside_begin(ctx))
        return;
    if (face != GL_FRONT && face != GL_BACK) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (pname == GL_SHININESS)
        n = 1u;
    else if (pname == GL_AMBIENT || pname == GL_DIFFUSE || pname == GL_SPECULAR ||
             pname == GL_EMISSION)
        n = 4u;
    else {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    glGetMaterialfv(face, pname, v);
    for (i = 0; i < n; ++i)
        params[i] = mgl_material_color_pname(pname) ? mgl_color_to_signed_int(v[i])
                                                    : mgl_float_to_nearest_int(v[i]);
}

void glColorMaterial(GLenum face, GLenum mode) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glColorMaterial",
                        (const struct mgl_error_arg[]){{"face", (uint64_t)(uintptr_t)face},
                                                       {"mode", (uint64_t)(uintptr_t)mode}},
                        2u);
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_color_material(ctx, face, mode))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mode != GL_EMISSION && mode != GL_AMBIENT && mode != GL_DIFFUSE && mode != GL_SPECULAR &&
        mode != GL_AMBIENT_AND_DIFFUSE) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_color_material(ctx, face, mode))
        return;
    ctx->compat.color_material_face = face;
    ctx->compat.color_material_mode = mode;
}

void glShadeModel(GLenum mode) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glShadeModel",
                        (const struct mgl_error_arg[]){{"mode", (uint64_t)(uintptr_t)mode}}, 1u);
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_shade_model(ctx, mode))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mode != GL_FLAT && mode != GL_SMOOTH) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_shade_model(ctx, mode))
        return;
    ctx->compat.shade_model = mode;
}

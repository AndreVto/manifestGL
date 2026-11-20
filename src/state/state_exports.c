#include "core/render_diag.h"
#include "mgl/gl.h"

#include "core/context.h"
#include "core/capabilities.h"
#include "core/version.h"
#include "compat/evaluator.h"
#include "shader/translator.h"
#include "list/display_list.h"
#include "texture/texture_store.h"
#include "objects/object_store.h"

#include <string.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

GLenum glGetGraphicsResetStatus(void) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesGetGraphicsResetStatus == NULL)
        return GL_NO_ERROR;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_NO_ERROR;
    }
    return ctx->native_dispatch->glesGetGraphicsResetStatus();
}

static const struct mgl_indexed_buffer_binding *
current_tf_indexed_binding(const struct mgl_context *ctx, GLuint index) {
    const struct mgl_transform_feedback *tf;
    if (ctx == NULL || index >= MGL_MAX_INDEXED_BUFFER_BINDINGS)
        return NULL;
    if (ctx->transform_feedback_binding != 0u) {
        for (tf = ctx->transform_feedbacks; tf != NULL; tf = tf->next)
            if (tf->name == ctx->transform_feedback_binding)
                return &tf->buffer_bindings[index];
    }
    return &ctx->transform_feedback_buffer_bindings[index];
}

static int valid_depth_func(GLenum value) {
    return value >= GL_NEVER && value <= GL_ALWAYS;
}

static int reject_inside_begin(struct mgl_context *ctx) {
    if (ctx != NULL && ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 1;
    }
    return 0;
}

static int valid_cull_mode(GLenum value) {
    return value == GL_FRONT || value == GL_BACK || value == GL_FRONT_AND_BACK;
}

static int valid_front_face(GLenum value) {
    return value == GL_CW || value == GL_CCW;
}

static int blend_factor_is_src1(GLenum value) {
    return value == GL_SRC1_COLOR || value == GL_ONE_MINUS_SRC1_COLOR || value == GL_SRC1_ALPHA ||
           value == GL_ONE_MINUS_SRC1_ALPHA;
}

static int valid_blend_factor(GLenum value) {
    switch (value) {
        case GL_ZERO:
        case GL_ONE:
        case GL_SRC_COLOR:
        case GL_ONE_MINUS_SRC_COLOR:
        case GL_SRC_ALPHA:
        case GL_ONE_MINUS_SRC_ALPHA:
        case GL_DST_ALPHA:
        case GL_ONE_MINUS_DST_ALPHA:
        case GL_DST_COLOR:
        case GL_ONE_MINUS_DST_COLOR:
        case GL_SRC_ALPHA_SATURATE:
        case GL_CONSTANT_COLOR:
        case GL_ONE_MINUS_CONSTANT_COLOR:
        case GL_CONSTANT_ALPHA:
        case GL_ONE_MINUS_CONSTANT_ALPHA:
        case GL_SRC1_COLOR:
        case GL_ONE_MINUS_SRC1_COLOR:
        case GL_SRC1_ALPHA:
        case GL_ONE_MINUS_SRC1_ALPHA:
            return 1;
        default:
            return 0;
    }
}

static int valid_blend_equation(GLenum value) {
    return value == GL_FUNC_ADD || value == GL_FUNC_SUBTRACT || value == GL_FUNC_REVERSE_SUBTRACT ||
           value == GL_MIN || value == GL_MAX;
}

static int advanced_blend_equation(GLenum mode) {
    switch (mode) {
        case GL_MULTIPLY_KHR:
        case GL_SCREEN_KHR:
        case GL_OVERLAY_KHR:
        case GL_DARKEN_KHR:
        case GL_LIGHTEN_KHR:
        case GL_COLORDODGE_KHR:
        case GL_COLORBURN_KHR:
        case GL_HARDLIGHT_KHR:
        case GL_SOFTLIGHT_KHR:
        case GL_DIFFERENCE_KHR:
        case GL_EXCLUSION_KHR:
        case GL_HSL_HUE_KHR:
        case GL_HSL_SATURATION_KHR:
        case GL_HSL_COLOR_KHR:
        case GL_HSL_LUMINOSITY_KHR:
            return 1;
        default:
            return 0;
    }
}

static void lower_blend_equation(struct mgl_context *ctx, GLuint index, GLenum rgb, GLenum alpha,
                                 int indexed) {
    if (ctx->native_dispatch == NULL)
        return;
    if (advanced_blend_equation(rgb)) {
        if (indexed) {
            if (ctx->native_dispatch->glesBlendEquationi != NULL)
                ctx->native_dispatch->glesBlendEquationi(index, rgb);
        } else if (ctx->native_dispatch->glesBlendEquation != NULL) {
            ctx->native_dispatch->glesBlendEquation(rgb);
        }
    } else if (indexed) {
        if (ctx->native_dispatch->glesBlendEquationSeparatei != NULL)
            ctx->native_dispatch->glesBlendEquationSeparatei(index, rgb, alpha);
        else if (index == 0u && ctx->native_dispatch->glesBlendEquationSeparate != NULL)
            ctx->native_dispatch->glesBlendEquationSeparate(rgb, alpha);
    } else if (ctx->native_dispatch->glesBlendEquationSeparate != NULL) {
        ctx->native_dispatch->glesBlendEquationSeparate(rgb, alpha);
    } else if (rgb == alpha && ctx->native_dispatch->glesBlendEquation != NULL) {
        ctx->native_dispatch->glesBlendEquation(rgb);
    }
}

static int blend_func_uses_src1(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    return blend_factor_is_src1(srcRGB) || blend_factor_is_src1(dstRGB) ||
           blend_factor_is_src1(srcAlpha) || blend_factor_is_src1(dstAlpha);
}

static int native_can_realize_src1(const struct mgl_context *ctx) {
    return ctx != NULL && ctx->native_dispatch != NULL &&
           mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                  "GL_EXT_blend_func_extended");
}

static void lower_blend_func(struct mgl_context *ctx, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
                             GLenum dstAlpha) {
    if (ctx == NULL || ctx->native_dispatch == NULL)
        return;
    if (blend_func_uses_src1(srcRGB, dstRGB, srcAlpha, dstAlpha) && !native_can_realize_src1(ctx))
        return;
    if (ctx->native_dispatch->glesBlendFuncSeparate != NULL)
        ctx->native_dispatch->glesBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
    else if (srcRGB == srcAlpha && dstRGB == dstAlpha &&
             ctx->native_dispatch->glesBlendFunc != NULL)
        ctx->native_dispatch->glesBlendFunc(srcRGB, dstRGB);
}

static void lower_blend_func_indexed(struct mgl_context *ctx, GLuint index, GLenum srcRGB,
                                     GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    if (ctx == NULL || ctx->native_dispatch == NULL)
        return;
    if (blend_func_uses_src1(srcRGB, dstRGB, srcAlpha, dstAlpha) && !native_can_realize_src1(ctx))
        return;
    if (ctx->native_dispatch->glesBlendFuncSeparatei != NULL)
        ctx->native_dispatch->glesBlendFuncSeparatei(index, srcRGB, dstRGB, srcAlpha, dstAlpha);
    else if (index == 0u && ctx->native_dispatch->glesBlendFuncSeparate != NULL)
        ctx->native_dispatch->glesBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

static int valid_stencil_face(GLenum face) {
    return face == GL_FRONT || face == GL_BACK || face == GL_FRONT_AND_BACK;
}

static int valid_stencil_op(GLenum op) {
    return op == GL_KEEP || op == GL_ZERO || op == GL_REPLACE || op == GL_INCR || op == GL_DECR ||
           op == GL_INVERT || op == GL_INCR_WRAP || op == GL_DECR_WRAP;
}

static void set_stencil_func(struct mgl_stencil_face_state *face, GLenum func, GLint ref,
                             GLuint mask) {
    face->func = func;
    face->ref = ref;
    face->value_mask = mask;
}

static void set_stencil_op(struct mgl_stencil_face_state *face, GLenum sfail, GLenum dpfail,
                           GLenum dppass) {
    face->sfail = sfail;
    face->dpfail = dpfail;
    face->dppass = dppass;
}

static const struct mgl_stencil_face_state *stencil_native_back(const struct mgl_context *ctx) {
    return ctx->canonical_state.stencil_two_side_test ? &ctx->canonical_state.stencil_back_ext
                                                      : &ctx->canonical_state.stencil_back;
}

static const struct mgl_stencil_face_state *stencil_query_front(const struct mgl_context *ctx) {
    return ctx->canonical_state.active_stencil_face == GL_BACK
               ? &ctx->canonical_state.stencil_back_ext
               : &ctx->canonical_state.stencil_front;
}

static void stencil_native_sync_back(struct mgl_context *ctx) {
    const struct mgl_stencil_face_state *s;
    if (ctx->native_dispatch == NULL)
        return;
    s = stencil_native_back(ctx);
    if (ctx->native_dispatch->glesStencilFuncSeparate != NULL)
        ctx->native_dispatch->glesStencilFuncSeparate(GL_BACK, s->func, s->ref, s->value_mask);
    if (ctx->native_dispatch->glesStencilMaskSeparate != NULL)
        ctx->native_dispatch->glesStencilMaskSeparate(GL_BACK, s->write_mask);
    if (ctx->native_dispatch->glesStencilOpSeparate != NULL)
        ctx->native_dispatch->glesStencilOpSeparate(GL_BACK, s->sfail, s->dpfail, s->dppass);
}

static GLboolean *mgl_compat_enable_ptr(struct mgl_context *ctx, GLenum cap) {
    if (ctx == NULL)
        return NULL;
    if (cap == GL_FOG)
        return &ctx->compat.fog_enabled;
    if (cap == GL_LIGHTING)
        return &ctx->compat.lighting_enabled;
    if (cap == GL_COLOR_MATERIAL)
        return &ctx->compat.color_material_enabled;
    if (cap == GL_COLOR_SUM)
        return &ctx->compat.color_sum_enabled;
    if (cap == GL_NORMALIZE)
        return &ctx->compat.normalize_enabled;
    if (cap == GL_RESCALE_NORMAL)
        return &ctx->compat.rescale_normal_enabled;
    if (cap == GL_VERTEX_PROGRAM_ARB)
        return &ctx->compat.arb_vertex_program_enabled;
    if (cap == GL_FRAGMENT_PROGRAM_ARB)
        return &ctx->compat.arb_fragment_program_enabled;
    {
        GLboolean *eval = mgl_evaluator_enable_ptr(ctx, cap);
        if (eval != NULL)
            return eval;
    }
    if (ctx->compat.active_texture >= MGL_TEXTURE_UNITS)
        return NULL;
    if (cap == GL_TEXTURE_1D)
        return &ctx->compat.texture_1d_enabled[ctx->compat.active_texture];
    if (cap == GL_TEXTURE_3D)
        return &ctx->compat.texture_3d_enabled[ctx->compat.active_texture];
    if (cap == GL_TEXTURE_CUBE_MAP)
        return &ctx->compat.texture_cube_enabled[ctx->compat.active_texture];
    if (cap == GL_TEXTURE_RECTANGLE)
        return &ctx->compat.texture_rectangle_enabled[ctx->compat.active_texture];
    if (cap == GL_TEXTURE_GEN_S)
        return &ctx->compat.texgen[ctx->compat.active_texture][0].enabled;
    if (cap == GL_TEXTURE_GEN_T)
        return &ctx->compat.texgen[ctx->compat.active_texture][1].enabled;
    if (cap == GL_TEXTURE_GEN_R)
        return &ctx->compat.texgen[ctx->compat.active_texture][2].enabled;
    if (cap == GL_TEXTURE_GEN_Q)
        return &ctx->compat.texgen[ctx->compat.active_texture][3].enabled;
    if (cap >= GL_CLIP_PLANE0 && cap < GL_CLIP_PLANE0 + MGL_MAX_CLIP_PLANES)
        return &ctx->compat.clip_plane_enabled[cap - GL_CLIP_PLANE0];
    if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + MGL_MAX_LIGHTS)
        return &ctx->compat.lights[cap - GL_LIGHT0].enabled;
    return NULL;
}

static GLboolean *mgl_indexed_texture_enable_ptr(struct mgl_context *ctx, GLenum cap,
                                                 GLuint index) {
    if (ctx == NULL || index >= MGL_TEXTURE_UNITS)
        return NULL;
    if (cap == GL_TEXTURE_1D)
        return &ctx->compat.texture_1d_enabled[index];
    if (cap == GL_TEXTURE_2D)
        return &ctx->compat.texture_2d_enabled[index];
    if (cap == GL_TEXTURE_3D)
        return &ctx->compat.texture_3d_enabled[index];
    if (cap == GL_TEXTURE_CUBE_MAP)
        return &ctx->compat.texture_cube_enabled[index];
    if (cap == GL_TEXTURE_RECTANGLE)
        return &ctx->compat.texture_rectangle_enabled[index];
    if (cap == GL_TEXTURE_GEN_S)
        return &ctx->compat.texgen[index][0].enabled;
    if (cap == GL_TEXTURE_GEN_T)
        return &ctx->compat.texgen[index][1].enabled;
    if (cap == GL_TEXTURE_GEN_R)
        return &ctx->compat.texgen[index][2].enabled;
    if (cap == GL_TEXTURE_GEN_Q)
        return &ctx->compat.texgen[index][3].enabled;
    return NULL;
}

static void mgl_sync_scalar_draw_buffer_zero(struct mgl_raster_state *state) {
    const struct mgl_draw_buffer_state *db = &state->draw_buffers[0];
    state->blend = db->blend;
    memcpy(state->color_mask, db->color_mask, sizeof(state->color_mask));
    state->blend_src_rgb = db->blend_src_rgb;
    state->blend_dst_rgb = db->blend_dst_rgb;
    state->blend_src_alpha = db->blend_src_alpha;
    state->blend_dst_alpha = db->blend_dst_alpha;
    state->blend_equation_rgb = db->blend_equation_rgb;
    state->blend_equation_alpha = db->blend_equation_alpha;
}

static int pixel_map_index(GLenum map) {
    switch (map) {
        case GL_PIXEL_MAP_I_TO_I:
            return 0;
        case GL_PIXEL_MAP_S_TO_S:
            return 1;
        case GL_PIXEL_MAP_I_TO_R:
            return 2;
        case GL_PIXEL_MAP_I_TO_G:
            return 3;
        case GL_PIXEL_MAP_I_TO_B:
            return 4;
        case GL_PIXEL_MAP_I_TO_A:
            return 5;
        case GL_PIXEL_MAP_R_TO_R:
            return 6;
        case GL_PIXEL_MAP_G_TO_G:
            return 7;
        case GL_PIXEL_MAP_B_TO_B:
            return 8;
        case GL_PIXEL_MAP_A_TO_A:
            return 9;
        default:
            return -1;
    }
}

static int pixel_map_needs_power_of_two(GLenum map) {
    return map == GL_PIXEL_MAP_I_TO_I || map == GL_PIXEL_MAP_S_TO_S || map == GL_PIXEL_MAP_I_TO_R ||
           map == GL_PIXEL_MAP_I_TO_G || map == GL_PIXEL_MAP_I_TO_B || map == GL_PIXEL_MAP_I_TO_A;
}

static int is_power_of_two_size(GLsizei size) {
    unsigned value;
    if (size <= 0)
        return 0;
    value = (unsigned)size;
    return (value & (value - 1u)) == 0u;
}

int mgl_pixel_map_set_canonical(struct mgl_context *ctx, GLenum map, GLsizei mapsize,
                                const GLdouble *values) {
    int index = pixel_map_index(map);
    struct mgl_pixel_map_table *table;
    GLsizei i;
    if (ctx == NULL)
        return 0;
    if (index < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (mapsize <= 0 || mapsize > (GLsizei)MGL_PIXEL_MAP_MAX_SIZE || values == NULL ||
        (pixel_map_needs_power_of_two(map) && !is_power_of_two_size(mapsize))) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    table = &ctx->pixel_transfer.maps[index];
    table->size = mapsize;
    for (i = 0; i < mapsize; ++i)
        table->values[i] = values[i];
    return 1;
}

static int pixel_map_is_integer(GLenum map) {
    return map == GL_PIXEL_MAP_I_TO_I || map == GL_PIXEL_MAP_S_TO_S;
}

static GLdouble clamp01d(GLdouble value) {
    if (value < 0.0)
        return 0.0;
    if (value > 1.0)
        return 1.0;
    return value;
}

GLboolean glIsEnabled(GLenum cap) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_IS_ENABLED);
    GLboolean *slot;
    if (ctx == NULL)
        return GL_FALSE;
    if (reject_inside_begin(ctx))
        return GL_FALSE;
    /* Client-array enables belong to the currently bound VAO; texture
     * coordinates are selected by the *client* active texture unit. */
    switch (cap) {
        case GL_VERTEX_ARRAY:
            return ctx->current_vao->compat[MGL_COMPAT_SEM_POSITION].enabled;
        case GL_NORMAL_ARRAY:
            return ctx->current_vao->compat[MGL_COMPAT_SEM_NORMAL].enabled;
        case GL_COLOR_ARRAY:
            return ctx->current_vao->compat[MGL_COMPAT_SEM_COLOR].enabled;
        case GL_SECONDARY_COLOR_ARRAY:
            return ctx->current_vao->compat[MGL_COMPAT_SEM_SECONDARY].enabled;
        case GL_FOG_COORD_ARRAY:
            return ctx->current_vao->compat[MGL_COMPAT_SEM_FOG].enabled;
        case GL_EDGE_FLAG_ARRAY:
            return ctx->current_vao->edge_flag.enabled;
        case GL_INDEX_ARRAY:
            return ctx->current_vao->color_index.enabled;
        case GL_TEXTURE_COORD_ARRAY:
            return ctx->current_vao->compat[MGL_COMPAT_SEM_TEXCOORD0 + ctx->client_active_texture]
                .enabled;
        default:
            break;
    }
    if (cap == GL_TEXTURE_2D)
        return ctx->compat.active_texture < MGL_TEXTURE_UNITS
                   ? ctx->compat.texture_2d_enabled[ctx->compat.active_texture]
                   : GL_FALSE;
    if (cap == GL_DEBUG_OUTPUT)
        return ctx->debug.output_enabled;
    if (cap == GL_DEBUG_OUTPUT_SYNCHRONOUS)
        return ctx->debug.synchronous;
    slot = mgl_compat_enable_ptr(ctx, cap);
    if (slot != NULL)
        return *slot;
    if (cap == GL_SAMPLE_SHADING &&
        (ctx->native_dispatch == NULL || ctx->native_dispatch->glesMinSampleShading == NULL ||
         !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_OES_sample_shading"))) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return GL_FALSE;
    }
    if (cap == GL_DEPTH_BOUNDS_TEST_EXT &&
        (ctx->native_dispatch == NULL || !mgl_native_dispatch_has_gles_extension(
                                             ctx->native_dispatch, "GL_EXT_depth_bounds_test"))) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return GL_FALSE;
    }
    slot = mgl_state_cap_ptr(&ctx->canonical_state, cap);
    if (slot == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return GL_FALSE;
    }
    return *slot;
}

void glEnable(GLenum cap) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_ENABLE_STATE);
    GLboolean *slot;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_enable(ctx, cap, GL_TRUE))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (cap == GL_DEBUG_OUTPUT) {
        ctx->debug.output_enabled = GL_TRUE;
        return;
    }
    if (cap == GL_DEBUG_OUTPUT_SYNCHRONOUS) {
        ctx->debug.synchronous = GL_TRUE;
        return;
    }
    if (cap == GL_TEXTURE_2D) {
        if (ctx->compat.active_texture >= MGL_TEXTURE_UNITS) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (mgl_list_record_enable(ctx, cap, GL_TRUE))
            return;
        if (!ctx->compat.texture_2d_enabled[ctx->compat.active_texture])
            ctx->compat.fixed_variant_key_valid = GL_FALSE;
        ctx->compat.texture_2d_enabled[ctx->compat.active_texture] = GL_TRUE;
        return;
    }
    slot = mgl_compat_enable_ptr(ctx, cap);
    if (slot != NULL) {
        if (mgl_list_record_enable(ctx, cap, GL_TRUE))
            return;
        if (!*slot)
            ctx->compat.fixed_variant_key_valid = GL_FALSE;
        *slot = GL_TRUE;
        return;
    }
    if (cap == GL_SAMPLE_SHADING &&
        (ctx->native_dispatch == NULL || ctx->native_dispatch->glesMinSampleShading == NULL ||
         !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_OES_sample_shading"))) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (cap == GL_DEPTH_BOUNDS_TEST_EXT &&
        (ctx->native_dispatch == NULL || !mgl_native_dispatch_has_gles_extension(
                                             ctx->native_dispatch, "GL_EXT_depth_bounds_test"))) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    slot = mgl_state_cap_ptr(&ctx->canonical_state, cap);
    if (slot == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_enable(ctx, cap, GL_TRUE))
        return;
    if (*slot == GL_TRUE && cap != GL_BLEND)
        return;
    if (*slot == GL_TRUE && cap == GL_BLEND) {
        unsigned i;
        for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i)
            if (!ctx->canonical_state.draw_buffers[i].blend)
                break;
        if (i == MGL_MAX_DRAW_BUFFERS)
            return;
    }
    *slot = GL_TRUE;
    if (cap == GL_STENCIL_TEST_TWO_SIDE_EXT)
        stencil_native_sync_back(ctx);
    if (cap == GL_BLEND) {
        unsigned i;
        for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i)
            ctx->canonical_state.draw_buffers[i].blend = GL_TRUE;
    }
    if (cap != GL_MULTISAMPLE && cap != GL_TEXTURE_CUBE_MAP_SEAMLESS && cap != GL_STENCIL_TEST_TWO_SIDE_EXT &&
        cap != GL_ALPHA_TEST && cap != GL_POINT_SPRITE && cap != GL_PROGRAM_POINT_SIZE &&
        cap != GL_LINE_SMOOTH && cap != GL_POLYGON_OFFSET_POINT && cap != GL_POLYGON_OFFSET_LINE &&
        cap != GL_LINE_STIPPLE && cap != GL_POLYGON_STIPPLE &&
        (cap != GL_COLOR_LOGIC_OP ||
         (ctx->native_dispatch != NULL && ctx->native_dispatch->glesLogicOpANGLE != NULL &&
          mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_ANGLE_logic_op"))) &&
        (cap != GL_FRAMEBUFFER_SRGB || mgl_native_dispatch_has_gles_extension(
                                           ctx->native_dispatch, "GL_EXT_sRGB_write_control")) &&
        ctx->native_dispatch != NULL && ctx->native_dispatch->glesEnable != NULL)
        ctx->native_dispatch->glesEnable(cap);
}

void glDisable(GLenum cap) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_ENABLE_STATE);
    GLboolean *slot;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_enable(ctx, cap, GL_FALSE))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (cap == GL_DEBUG_OUTPUT) {
        ctx->debug.output_enabled = GL_FALSE;
        return;
    }
    if (cap == GL_DEBUG_OUTPUT_SYNCHRONOUS) {
        ctx->debug.synchronous = GL_FALSE;
        return;
    }
    if (cap == GL_TEXTURE_2D) {
        if (ctx->compat.active_texture >= MGL_TEXTURE_UNITS) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (mgl_list_record_enable(ctx, cap, GL_FALSE))
            return;
        if (ctx->compat.texture_2d_enabled[ctx->compat.active_texture])
            ctx->compat.fixed_variant_key_valid = GL_FALSE;
        ctx->compat.texture_2d_enabled[ctx->compat.active_texture] = GL_FALSE;
        return;
    }
    slot = mgl_compat_enable_ptr(ctx, cap);
    if (slot != NULL) {
        if (mgl_list_record_enable(ctx, cap, GL_FALSE))
            return;
        if (*slot)
            ctx->compat.fixed_variant_key_valid = GL_FALSE;
        *slot = GL_FALSE;
        return;
    }
    if (cap == GL_SAMPLE_SHADING &&
        (ctx->native_dispatch == NULL || ctx->native_dispatch->glesMinSampleShading == NULL ||
         !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_OES_sample_shading"))) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (cap == GL_DEPTH_BOUNDS_TEST_EXT &&
        (ctx->native_dispatch == NULL || !mgl_native_dispatch_has_gles_extension(
                                             ctx->native_dispatch, "GL_EXT_depth_bounds_test"))) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    slot = mgl_state_cap_ptr(&ctx->canonical_state, cap);
    if (slot == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_enable(ctx, cap, GL_FALSE))
        return;
    if (*slot == GL_FALSE && cap != GL_BLEND)
        return;
    if (*slot == GL_FALSE && cap == GL_BLEND) {
        unsigned i;
        for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i)
            if (ctx->canonical_state.draw_buffers[i].blend)
                break;
        if (i == MGL_MAX_DRAW_BUFFERS)
            return;
    }
    *slot = GL_FALSE;
    if (cap == GL_STENCIL_TEST_TWO_SIDE_EXT)
        stencil_native_sync_back(ctx);
    if (cap == GL_BLEND) {
        unsigned i;
        for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i)
            ctx->canonical_state.draw_buffers[i].blend = GL_FALSE;
    }
    if (cap != GL_MULTISAMPLE && cap != GL_TEXTURE_CUBE_MAP_SEAMLESS && cap != GL_STENCIL_TEST_TWO_SIDE_EXT &&
        cap != GL_ALPHA_TEST && cap != GL_POINT_SPRITE && cap != GL_PROGRAM_POINT_SIZE &&
        cap != GL_LINE_SMOOTH && cap != GL_POLYGON_OFFSET_POINT && cap != GL_POLYGON_OFFSET_LINE &&
        cap != GL_LINE_STIPPLE && cap != GL_POLYGON_STIPPLE &&
        (cap != GL_COLOR_LOGIC_OP ||
         (ctx->native_dispatch != NULL && ctx->native_dispatch->glesLogicOpANGLE != NULL &&
          mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_ANGLE_logic_op"))) &&
        (cap != GL_FRAMEBUFFER_SRGB || mgl_native_dispatch_has_gles_extension(
                                           ctx->native_dispatch, "GL_EXT_sRGB_write_control")) &&
        ctx->native_dispatch != NULL && ctx->native_dispatch->glesDisable != NULL)
        ctx->native_dispatch->glesDisable(cap);
}

static GLfloat clamp01f(GLfloat value) {
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

static void mgl_set_depth_range(GLfloat nearVal, GLfloat farVal, GLboolean list_recordable) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    /* The ES-compatible float entry point executes during list compilation;
       the desktop double entry point is compiled into the list. */
    if (ctx->compiling_list != NULL && !ctx->list_replaying && !list_recordable) {
        if (ctx->immediate.active)
            return;
    } else if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_depth_range(ctx, nearVal, farVal))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    nearVal = clamp01f(nearVal);
    farVal = clamp01f(farVal);
    if (list_recordable && mgl_list_record_depth_range(ctx, nearVal, farVal))
        return;
    ctx->canonical_state.depth_range[0] = nearVal;
    ctx->canonical_state.depth_range[1] = farVal;
    ctx->canonical_state.indexed_depth_ranges[0][0] = nearVal;
    ctx->canonical_state.indexed_depth_ranges[0][1] = farVal;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesDepthRangef != NULL)
        ctx->native_dispatch->glesDepthRangef(nearVal, farVal);
}

void glDepthRangef(GLfloat nearVal, GLfloat farVal) {
    mgl_set_depth_range(nearVal, farVal, GL_FALSE);
}

void glDepthRange(GLdouble nearVal, GLdouble farVal) {
    mgl_set_depth_range((GLfloat)nearVal, (GLfloat)farVal, GL_TRUE);
}

void glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_blend_color(ctx, red, green, blue, alpha))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    red = clamp01f(red);
    green = clamp01f(green);
    blue = clamp01f(blue);
    alpha = clamp01f(alpha);
    if (mgl_list_record_blend_color(ctx, red, green, blue, alpha))
        return;
    ctx->canonical_state.blend_color[0] = red;
    ctx->canonical_state.blend_color[1] = green;
    ctx->canonical_state.blend_color[2] = blue;
    ctx->canonical_state.blend_color[3] = alpha;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBlendColor != NULL)
        ctx->native_dispatch->glesBlendColor(red, green, blue, alpha);
}

void glClampColor(GLenum target, GLenum clamp) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (target != GL_CLAMP_VERTEX_COLOR && target != GL_CLAMP_FRAGMENT_COLOR &&
        target != GL_CLAMP_READ_COLOR) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (clamp != GL_TRUE && clamp != GL_FALSE && clamp != GL_FIXED_ONLY) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (target == GL_CLAMP_VERTEX_COLOR)
        ctx->canonical_state.clamp_vertex_color = clamp;
    else if (target == GL_CLAMP_FRAGMENT_COLOR)
        ctx->canonical_state.clamp_fragment_color = clamp;
    else
        ctx->canonical_state.clamp_read_color = clamp;
}

void glClampColorARB(GLenum target, GLenum clamp) {
    glClampColor(target, clamp);
}

void glPointSize(GLfloat size) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (!(size > 0.0f)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    ctx->canonical_state.point_size = size;
}

void glLineWidth(GLfloat width) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_line_width(ctx, width))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!(width > 0.0f)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_line_width(ctx, width))
        return;
    ctx->canonical_state.line_width = width;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesLineWidth != NULL)
        ctx->native_dispatch->glesLineWidth(width);
}

void glClipControl(GLenum origin, GLenum depth) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (origin != GL_LOWER_LEFT && origin != GL_UPPER_LEFT) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (depth != GL_NEGATIVE_ONE_TO_ONE && depth != GL_ZERO_TO_ONE) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesClipControlEXT == NULL ||
        !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_EXT_clip_control")) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesClipControlEXT(origin, depth);
    ctx->canonical_state.clip_origin = origin;
    ctx->canonical_state.clip_depth_mode = depth;
}

void glDepthBoundsEXT(GLclampd zmin, GLclampd zmax) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (!(zmin >= 0.0 && zmin <= 1.0 && zmax >= 0.0 && zmax <= 1.0 && zmin <= zmax)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesDepthBoundsEXT == NULL ||
        !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_EXT_depth_bounds_test")) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesDepthBoundsEXT(zmin, zmax);
    ctx->canonical_state.depth_bounds[0] = zmin;
    ctx->canonical_state.depth_bounds[1] = zmax;
}

void glPatchParameteri(GLenum pname, GLint value) {
    struct mgl_context *ctx = mgl_context_current();
    GLint limit = 0;
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (pname != GL_PATCH_VERTICES) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (value <= 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesPatchParameteri == NULL ||
        ctx->native_dispatch->glesGetIntegerv == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesGetIntegerv(GL_MAX_PATCH_VERTICES, &limit);
    if (value > limit) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    ctx->native_dispatch->glesPatchParameteri(pname, value);
    ctx->canonical_state.patch_vertices = value;
}

void glPatchParameterfv(GLenum pname, const GLfloat *values) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat *dst;
    size_t count;
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (pname == GL_PATCH_DEFAULT_INNER_LEVEL) {
        dst = ctx->canonical_state.patch_inner_levels;
        count = 2u;
    } else if (pname == GL_PATCH_DEFAULT_OUTER_LEVEL) {
        dst = ctx->canonical_state.patch_outer_levels;
        count = 4u;
    } else {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesPatchParameterfv == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesPatchParameterfv(pname, values);
    memcpy(dst, values, count * sizeof(GLfloat));
}

void glMinSampleShading(GLfloat value) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesMinSampleShading == NULL ||
        !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_OES_sample_shading")) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (value < 0.0f)
        value = 0.0f;
    if (value > 1.0f)
        value = 1.0f;
    ctx->native_dispatch->glesMinSampleShading(value);
    ctx->canonical_state.min_sample_shading = value;
}

void glMinSampleShadingARB(GLfloat value) {
    glMinSampleShading(value);
}

void glLogicOp(GLenum opcode) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (opcode < GL_CLEAR || opcode > GL_SET) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    ctx->canonical_state.logic_op_mode = opcode;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesLogicOpANGLE != NULL &&
        mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_ANGLE_logic_op"))
        ctx->native_dispatch->glesLogicOpANGLE(opcode);
}

void glPointParameterfv(GLenum pname, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (pname != GL_POINT_SPRITE_COORD_ORIGIN && pname != GL_POINT_SIZE_MIN &&
        pname != GL_POINT_SIZE_MAX && pname != GL_POINT_FADE_THRESHOLD_SIZE &&
        pname != GL_POINT_DISTANCE_ATTENUATION) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (params == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    switch (pname) {
        case GL_POINT_SPRITE_COORD_ORIGIN:
            if (params[0] != (GLfloat)GL_LOWER_LEFT && params[0] != (GLfloat)GL_UPPER_LEFT) {
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return;
            }
            ctx->canonical_state.point_sprite_coord_origin = (GLenum)params[0];
            break;
        case GL_POINT_SIZE_MIN:
        case GL_POINT_SIZE_MAX:
        case GL_POINT_FADE_THRESHOLD_SIZE:
            if (!(params[0] >= 0.0f)) {
                mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
                return;
            }
            if (pname == GL_POINT_SIZE_MIN)
                ctx->canonical_state.point_size_min = params[0];
            else if (pname == GL_POINT_SIZE_MAX)
                ctx->canonical_state.point_size_max = params[0];
            else
                ctx->canonical_state.point_fade_threshold_size = params[0];
            break;
        case GL_POINT_DISTANCE_ATTENUATION:
            memcpy(ctx->canonical_state.point_distance_attenuation, params, 3u * sizeof(GLfloat));
            break;
        default:
            break;
    }
}

void glPointParameterf(GLenum pname, GLfloat param) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (pname == GL_POINT_DISTANCE_ATTENUATION) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    glPointParameterfv(pname, &param);
}

void glPointParameteri(GLenum pname, GLint param) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat value = (GLfloat)param;
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (pname == GL_POINT_DISTANCE_ATTENUATION) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (pname == GL_POINT_SPRITE_COORD_ORIGIN && param != (GLint)GL_LOWER_LEFT &&
        param != (GLint)GL_UPPER_LEFT) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    glPointParameterfv(pname, &value);
}

void glPointParameteriv(GLenum pname, const GLint *params) {
    GLfloat converted[3];
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (params == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (pname == GL_POINT_DISTANCE_ATTENUATION) {
        for (unsigned i = 0u; i < 3u; ++i)
            converted[i] = (GLfloat)params[i];
        glPointParameterfv(pname, converted);
    } else {
        glPointParameteri(pname, params[0]);
    }
}

void glPointParameterfARB(GLenum pname, GLfloat param) {
    glPointParameterf(pname, param);
}

void glPointParameterfvARB(GLenum pname, const GLfloat *params) {
    glPointParameterfv(pname, params);
}

void glPointParameterfEXT(GLenum pname, GLfloat param) {
    glPointParameterf(pname, param);
}

void glPointParameterfvEXT(GLenum pname, const GLfloat *params) {
    glPointParameterfv(pname, params);
}

void glPolygonMode(GLenum face, GLenum mode) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mode != GL_POINT && mode != GL_LINE && mode != GL_FILL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesPolygonModeNV != NULL &&
        mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_NV_polygon_mode")) {
        if (face != GL_FRONT_AND_BACK) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        ctx->native_dispatch->glesPolygonModeNV(face, mode);
    } else if (mode != GL_FILL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (face != GL_BACK)
        ctx->canonical_state.polygon_mode[0] = mode;
    if (face != GL_FRONT)
        ctx->canonical_state.polygon_mode[1] = mode;
}

void glPolygonOffset(GLfloat factor, GLfloat units) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_polygon_offset(ctx, factor, units))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_polygon_offset(ctx, factor, units))
        return;
    ctx->canonical_state.polygon_offset_factor = factor;
    ctx->canonical_state.polygon_offset_units = units;
    ctx->canonical_state.polygon_offset_clamp = 0.0f;
    if (ctx->native_dispatch != NULL) {
        if (ctx->native_dispatch->glesPolygonOffsetClamp != NULL)
            ctx->native_dispatch->glesPolygonOffsetClamp(factor, units, 0.0f);
        else if (ctx->native_dispatch->glesPolygonOffset != NULL)
            ctx->native_dispatch->glesPolygonOffset(factor, units);
    }
}

void glPolygonOffsetClamp(GLfloat factor, GLfloat units, GLfloat clamp) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesPolygonOffsetClamp == NULL ||
        (!mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                 "GL_EXT_polygon_offset_clamp") &&
         !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                 "GL_ARB_polygon_offset_clamp"))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesPolygonOffsetClamp(factor, units, clamp);
    ctx->canonical_state.polygon_offset_factor = factor;
    ctx->canonical_state.polygon_offset_units = units;
    ctx->canonical_state.polygon_offset_clamp = clamp;
}

void glPolygonOffsetClampEXT(GLfloat factor, GLfloat units, GLfloat clamp) {
    glPolygonOffsetClamp(factor, units, clamp);
}

void glSampleMaski(GLuint maskNumber, GLbitfield mask) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (maskNumber != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesSampleMaski == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesSampleMaski(maskNumber, mask);
    ctx->canonical_state.sample_mask_value = mask;
}

void glSampleCoverage(GLfloat value, GLboolean invert) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_sample_coverage(ctx, value, invert))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    value = clamp01f(value);
    invert = invert ? GL_TRUE : GL_FALSE;
    if (mgl_list_record_sample_coverage(ctx, value, invert))
        return;
    ctx->canonical_state.sample_coverage_value = value;
    ctx->canonical_state.sample_coverage_invert = invert;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesSampleCoverage != NULL)
        ctx->native_dispatch->glesSampleCoverage(value, invert);
}

static GLenum *mgl_hint_state_ptr(struct mgl_raster_state *state, GLenum target) {
    if (state == NULL)
        return NULL;
    switch (target) {
        case GL_PERSPECTIVE_CORRECTION_HINT:
            return &state->perspective_correction_hint;
        case GL_POINT_SMOOTH_HINT:
            return &state->point_smooth_hint;
        case GL_LINE_SMOOTH_HINT:
            return &state->line_smooth_hint;
        case GL_POLYGON_SMOOTH_HINT:
            return &state->polygon_smooth_hint;
        case GL_FOG_HINT:
            return &state->fog_hint;
        case GL_GENERATE_MIPMAP_HINT:
            return &state->generate_mipmap_hint;
        case GL_TEXTURE_COMPRESSION_HINT:
            return &state->texture_compression_hint;
        case GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
            return &state->fragment_shader_derivative_hint;
        default:
            return NULL;
    }
}

void glHint(GLenum target, GLenum mode) {
    struct mgl_context *ctx = mgl_context_current();
    GLenum *slot;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_hint(ctx, target, mode))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    slot = mgl_hint_state_ptr(&ctx->canonical_state, target);
    if (slot == NULL || (mode != GL_DONT_CARE && mode != GL_FASTEST && mode != GL_NICEST)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_hint(ctx, target, mode))
        return;
    *slot = mode;
}

void glAlphaFunc(GLenum func, GLclampf ref) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_alpha_func(ctx, func, ref))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_depth_func(func)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (ref < 0.0f)
        ref = 0.0f;
    if (ref > 1.0f)
        ref = 1.0f;
    if (mgl_list_record_alpha_func(ctx, func, ref))
        return;
    ctx->canonical_state.alpha_func = func;
    ctx->canonical_state.alpha_ref = ref;
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_ENABLE_STATE);
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_scissor(ctx, x, y, width, height))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_scissor(ctx, x, y, width, height))
        return;
    ctx->canonical_state.scissor[0] = x;
    ctx->canonical_state.scissor[1] = y;
    ctx->canonical_state.scissor[2] = width;
    ctx->canonical_state.scissor[3] = height;
    ctx->canonical_state.indexed_scissors[0][0] = x;
    ctx->canonical_state.indexed_scissors[0][1] = y;
    ctx->canonical_state.indexed_scissors[0][2] = width;
    ctx->canonical_state.indexed_scissors[0][3] = height;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesScissor != NULL)
        ctx->native_dispatch->glesScissor(x, y, width, height);
}

static GLuint viewport_array_limit(const struct mgl_context *ctx) {
    GLint limit = 0;
    if (ctx->native_dispatch == NULL ||
        !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_OES_viewport_array") ||
        ctx->native_dispatch->glesViewportArrayvOES == NULL ||
        ctx->native_dispatch->glesScissorArrayvOES == NULL ||
        ctx->native_dispatch->glesDepthRangeArrayfvOES == NULL ||
        ctx->native_dispatch->glesGetIntegerv == NULL)
        return 1u;
    ctx->native_dispatch->glesGetIntegerv(GL_MAX_VIEWPORTS, &limit);
    if (limit < 1)
        return 1u;
    return limit < 16 ? (GLuint)limit : 16u;
}

static int validate_viewport_span(struct mgl_context *ctx, GLuint first, GLsizei count) {
    GLuint limit;
    if (reject_inside_begin(ctx))
        return 0;
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    limit = viewport_array_limit(ctx);
    if (first > limit || (GLuint)count > limit - first) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    return 1;
}

static GLint viewport_round_to_int(GLfloat value) {
    if (value <= (GLfloat)INT_MIN)
        return INT_MIN;
    if (value >= (GLfloat)INT_MAX)
        return INT_MAX;
    return (GLint)lrintf(value);
}

void glViewportArrayv(GLuint first, GLsizei count, const GLfloat *v) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL || !validate_viewport_span(ctx, first, count))
        return;
    if (count == 0)
        return;
    if (v == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < count; ++i) {
        const GLfloat *p = &v[4 * i];
        if (!(p[2] >= 0.0f) || !(p[3] >= 0.0f)) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesViewportArrayvOES == NULL ||
        !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_OES_viewport_array")) {
        if (first != 0u || count != 1 || !isfinite(v[0]) || !isfinite(v[1]) || !isfinite(v[2]) ||
            !isfinite(v[3])) {
            mgl_render_diag_detail(ctx, "glViewportArrayv", "native_viewport_array_unavailable",
                                   first, (uint64_t)count);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesViewport != NULL)
            ctx->native_dispatch->glesViewport(viewport_round_to_int(v[0]),
                                               viewport_round_to_int(v[1]),
                                               viewport_round_to_int(v[2]),
                                               viewport_round_to_int(v[3]));
    } else
        ctx->native_dispatch->glesViewportArrayvOES(first, count, v);
    for (i = 0; i < count; ++i)
        memcpy(ctx->canonical_state.indexed_viewports[first + (GLuint)i], &v[4 * i],
               4 * sizeof(GLfloat));
    if (first == 0u) {
        for (i = 0; i < 4; ++i)
            ctx->canonical_state.viewport[i] = viewport_round_to_int(v[i]);
    }
}

void glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h) {
    GLfloat v[4] = {x, y, w, h};
    glViewportArrayv(index, 1, v);
}

void glViewportIndexedfv(GLuint index, const GLfloat *v) {
    glViewportArrayv(index, 1, v);
}

void glScissorArrayv(GLuint first, GLsizei count, const GLint *v) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL || !validate_viewport_span(ctx, first, count))
        return;
    if (count == 0)
        return;
    if (v == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < count; ++i) {
        if (v[4 * i + 2] < 0 || v[4 * i + 3] < 0) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesScissorArrayvOES != NULL &&
        mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_OES_viewport_array"))
        ctx->native_dispatch->glesScissorArrayvOES(first, count, v);
    else if (first == 0u && count == 1) {
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesScissor != NULL)
            ctx->native_dispatch->glesScissor(v[0], v[1], v[2], v[3]);
    } else {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    for (i = 0; i < count; ++i)
        memcpy(ctx->canonical_state.indexed_scissors[first + (GLuint)i], &v[4 * i],
               4 * sizeof(GLint));
    if (first == 0u)
        memcpy(ctx->canonical_state.scissor, v, 4 * sizeof(GLint));
}

void glScissorIndexed(GLuint index, GLint x, GLint y, GLsizei w, GLsizei h) {
    GLint v[4] = {x, y, w, h};
    glScissorArrayv(index, 1, v);
}

void glScissorIndexedv(GLuint index, const GLint *v) {
    glScissorArrayv(index, 1, v);
}

void glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble *v) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat values[32];
    GLsizei i;
    if (ctx == NULL || !validate_viewport_span(ctx, first, count))
        return;
    if (count == 0)
        return;
    if (v == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < 2 * count; ++i)
        values[i] = clamp01f((GLfloat)v[i]);
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesDepthRangeArrayfvOES != NULL &&
        mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_OES_viewport_array"))
        ctx->native_dispatch->glesDepthRangeArrayfvOES(first, count, values);
    else if (first == 0u && count == 1) {
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesDepthRangef != NULL)
            ctx->native_dispatch->glesDepthRangef(values[0], values[1]);
    } else {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    for (i = 0; i < count; ++i) {
        ctx->canonical_state.indexed_depth_ranges[first + (GLuint)i][0] = values[2 * i];
        ctx->canonical_state.indexed_depth_ranges[first + (GLuint)i][1] = values[2 * i + 1];
    }
    if (first == 0u) {
        ctx->canonical_state.depth_range[0] = values[0];
        ctx->canonical_state.depth_range[1] = values[1];
    }
}

void glDepthRangeIndexed(GLuint index, GLdouble n, GLdouble f) {
    GLdouble v[2] = {n, f};
    glDepthRangeArrayv(index, 1, v);
}

static int mgl_indexed_query_component_count(GLenum target);

void glGetDoublei_v(GLenum target, GLuint index, GLdouble *data) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || reject_inside_begin(ctx) || data == NULL)
        return;
    if (target == GL_VIEWPORT || target == GL_SCISSOR_BOX || target == GL_DEPTH_RANGE) {
        int count = target == GL_DEPTH_RANGE ? 2 : 4;
        if (index >= viewport_array_limit(ctx)) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        if (target == GL_DEPTH_RANGE) {
            data[0] = ctx->canonical_state.indexed_depth_ranges[index][0];
            data[1] = ctx->canonical_state.indexed_depth_ranges[index][1];
        } else if (target == GL_SCISSOR_BOX) {
            for (int i = 0; i < count; ++i)
                data[i] = ctx->canonical_state.indexed_scissors[index][i];
        } else {
            for (int i = 0; i < count; ++i)
                data[i] = ctx->canonical_state.indexed_viewports[index][i];
        }
        return;
    }
    {
        GLint64 values[4] = {0};
        uint64_t sequence = ctx->errors.sequence;
        glGetInteger64i_v(target, index, values);
        if (ctx->errors.sequence != sequence)
            return;
        for (int i = 0; i < mgl_indexed_query_component_count(target); ++i)
            data[i] = (GLdouble)values[i];
    }
}

void glGetFloati_v(GLenum target, GLuint index, GLfloat *data) {
    struct mgl_context *ctx = mgl_context_current();
    GLdouble values[4] = {0};
    uint64_t sequence;
    if (ctx == NULL || data == NULL)
        return;
    sequence = ctx->errors.sequence;
    glGetDoublei_v(target, index, values);
    if (ctx->errors.sequence != sequence)
        return;
    for (int i = 0; i < mgl_indexed_query_component_count(target); ++i)
        data[i] = (GLfloat)values[i];
}

void glDepthFunc(GLenum func) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_depth_func(ctx, func))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_depth_func(func)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_depth_func(ctx, func))
        return;
    ctx->canonical_state.depth_func = func;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesDepthFunc != NULL)
        ctx->native_dispatch->glesDepthFunc(func);
}

void glDepthMask(GLboolean flag) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_depth_mask(ctx, flag))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    flag = flag ? GL_TRUE : GL_FALSE;
    if (mgl_list_record_depth_mask(ctx, flag))
        return;
    ctx->canonical_state.depth_mask = flag;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesDepthMask != NULL)
        ctx->native_dispatch->glesDepthMask(ctx->canonical_state.depth_mask);
}

void glActiveStencilFaceEXT(GLenum face) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (face != GL_FRONT && face != GL_BACK) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    ctx->canonical_state.active_stencil_face = face;
}

void glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_stencil_func(ctx, face, func, ref, mask))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_stencil_face(face) || !valid_depth_func(func)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_stencil_func(ctx, face, func, ref, mask))
        return;
    if (face != GL_BACK)
        set_stencil_func(&ctx->canonical_state.stencil_front, func, ref, mask);
    if (face != GL_FRONT)
        set_stencil_func(&ctx->canonical_state.stencil_back, func, ref, mask);
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesStencilFuncSeparate != NULL) {
        if (!ctx->canonical_state.stencil_two_side_test)
            ctx->native_dispatch->glesStencilFuncSeparate(face, func, ref, mask);
        else if (face != GL_BACK)
            ctx->native_dispatch->glesStencilFuncSeparate(GL_FRONT, func, ref, mask);
    }
}

void glStencilFunc(GLenum func, GLint ref, GLuint mask) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_stencil_func(ctx, GL_NONE, func, ref, mask))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_depth_func(func)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_stencil_func(ctx, GL_NONE, func, ref, mask))
        return;
    if (ctx->canonical_state.active_stencil_face == GL_BACK) {
        set_stencil_func(&ctx->canonical_state.stencil_back_ext, func, ref, mask);
        if (ctx->canonical_state.stencil_two_side_test && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesStencilFuncSeparate != NULL)
            ctx->native_dispatch->glesStencilFuncSeparate(GL_BACK, func, ref, mask);
    } else {
        set_stencil_func(&ctx->canonical_state.stencil_front, func, ref, mask);
        set_stencil_func(&ctx->canonical_state.stencil_back, func, ref, mask);
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesStencilFuncSeparate != NULL) {
            ctx->native_dispatch->glesStencilFuncSeparate(
                ctx->canonical_state.stencil_two_side_test ? GL_FRONT : GL_FRONT_AND_BACK, func,
                ref, mask);
        }
    }
}

void glStencilMaskSeparate(GLenum face, GLuint mask) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_stencil_mask(ctx, face, mask))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_stencil_face(face)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_stencil_mask(ctx, face, mask))
        return;
    if (face != GL_BACK)
        ctx->canonical_state.stencil_front.write_mask = mask;
    if (face != GL_FRONT)
        ctx->canonical_state.stencil_back.write_mask = mask;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesStencilMaskSeparate != NULL) {
        if (!ctx->canonical_state.stencil_two_side_test)
            ctx->native_dispatch->glesStencilMaskSeparate(face, mask);
        else if (face != GL_BACK)
            ctx->native_dispatch->glesStencilMaskSeparate(GL_FRONT, mask);
    }
}

void glStencilMask(GLuint mask) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_stencil_mask(ctx, GL_NONE, mask))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_stencil_mask(ctx, GL_NONE, mask))
        return;
    if (ctx->canonical_state.active_stencil_face == GL_BACK) {
        ctx->canonical_state.stencil_back_ext.write_mask = mask;
        if (ctx->canonical_state.stencil_two_side_test && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesStencilMaskSeparate != NULL)
            ctx->native_dispatch->glesStencilMaskSeparate(GL_BACK, mask);
    } else {
        ctx->canonical_state.stencil_front.write_mask = mask;
        ctx->canonical_state.stencil_back.write_mask = mask;
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesStencilMaskSeparate != NULL) {
            ctx->native_dispatch->glesStencilMaskSeparate(
                ctx->canonical_state.stencil_two_side_test ? GL_FRONT : GL_FRONT_AND_BACK, mask);
        }
    }
}

void glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_stencil_op(ctx, face, sfail, dpfail, dppass))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_stencil_face(face) || !valid_stencil_op(sfail) || !valid_stencil_op(dpfail) ||
        !valid_stencil_op(dppass)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_stencil_op(ctx, face, sfail, dpfail, dppass))
        return;
    if (face != GL_BACK)
        set_stencil_op(&ctx->canonical_state.stencil_front, sfail, dpfail, dppass);
    if (face != GL_FRONT)
        set_stencil_op(&ctx->canonical_state.stencil_back, sfail, dpfail, dppass);
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesStencilOpSeparate != NULL) {
        if (!ctx->canonical_state.stencil_two_side_test)
            ctx->native_dispatch->glesStencilOpSeparate(face, sfail, dpfail, dppass);
        else if (face != GL_BACK)
            ctx->native_dispatch->glesStencilOpSeparate(GL_FRONT, sfail, dpfail, dppass);
    }
}

void glStencilFuncSeparateATI(GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_depth_func(frontfunc) || !valid_depth_func(backfunc)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    glStencilFuncSeparate(GL_FRONT, frontfunc, ref, mask);
    glStencilFuncSeparate(GL_BACK, backfunc, ref, mask);
}

void glStencilOpSeparateATI(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    glStencilOpSeparate(face, sfail, dpfail, dppass);
}

void glStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_stencil_op(ctx, GL_NONE, sfail, dpfail, dppass))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_stencil_op(sfail) || !valid_stencil_op(dpfail) || !valid_stencil_op(dppass)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_stencil_op(ctx, GL_NONE, sfail, dpfail, dppass))
        return;
    if (ctx->canonical_state.active_stencil_face == GL_BACK) {
        set_stencil_op(&ctx->canonical_state.stencil_back_ext, sfail, dpfail, dppass);
        if (ctx->canonical_state.stencil_two_side_test && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesStencilOpSeparate != NULL)
            ctx->native_dispatch->glesStencilOpSeparate(GL_BACK, sfail, dpfail, dppass);
    } else {
        set_stencil_op(&ctx->canonical_state.stencil_front, sfail, dpfail, dppass);
        set_stencil_op(&ctx->canonical_state.stencil_back, sfail, dpfail, dppass);
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesStencilOpSeparate != NULL) {
            ctx->native_dispatch->glesStencilOpSeparate(
                ctx->canonical_state.stencil_two_side_test ? GL_FRONT : GL_FRONT_AND_BACK, sfail,
                dpfail, dppass);
        }
    }
}

void glIndexMask(GLuint mask) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (reject_inside_begin(ctx))
        return;
    if (mgl_list_record_index_mask(ctx, mask))
        return;
    ctx->canonical_state.index_writemask = mask;
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_color_mask(ctx, red, green, blue, alpha))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_color_mask(ctx, red, green, blue, alpha))
        return;
    ctx->canonical_state.color_mask[0] = red ? GL_TRUE : GL_FALSE;
    ctx->canonical_state.color_mask[1] = green ? GL_TRUE : GL_FALSE;
    ctx->canonical_state.color_mask[2] = blue ? GL_TRUE : GL_FALSE;
    ctx->canonical_state.color_mask[3] = alpha ? GL_TRUE : GL_FALSE;
    {
        unsigned i;
        for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i)
            memcpy(ctx->canonical_state.draw_buffers[i].color_mask, ctx->canonical_state.color_mask,
                   sizeof(ctx->canonical_state.color_mask));
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesColorMask != NULL) {
        ctx->native_dispatch->glesColorMask(
            ctx->canonical_state.color_mask[0], ctx->canonical_state.color_mask[1],
            ctx->canonical_state.color_mask[2], ctx->canonical_state.color_mask[3]);
    }
}

void glCullFace(GLenum mode) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_cull_face(ctx, mode))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_cull_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_cull_face(ctx, mode))
        return;
    ctx->canonical_state.cull_face_mode = mode;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesCullFace != NULL)
        ctx->native_dispatch->glesCullFace(mode);
}

void glFrontFace(GLenum mode) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_front_face(ctx, mode))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_front_face(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_front_face(ctx, mode))
        return;
    ctx->canonical_state.front_face = mode;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesFrontFace != NULL)
        ctx->native_dispatch->glesFrontFace(mode);
}

void glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_blend_func_separate(ctx, srcRGB, dstRGB, srcAlpha, dstAlpha))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_blend_factor(srcRGB) || !valid_blend_factor(dstRGB) ||
        !valid_blend_factor(srcAlpha) || !valid_blend_factor(dstAlpha)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_blend_func_separate(ctx, srcRGB, dstRGB, srcAlpha, dstAlpha))
        return;
    ctx->canonical_state.blend_src_rgb = srcRGB;
    ctx->canonical_state.blend_dst_rgb = dstRGB;
    ctx->canonical_state.blend_src_alpha = srcAlpha;
    ctx->canonical_state.blend_dst_alpha = dstAlpha;
    {
        unsigned i;
        for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i) {
            ctx->canonical_state.draw_buffers[i].blend_src_rgb = srcRGB;
            ctx->canonical_state.draw_buffers[i].blend_dst_rgb = dstRGB;
            ctx->canonical_state.draw_buffers[i].blend_src_alpha = srcAlpha;
            ctx->canonical_state.draw_buffers[i].blend_dst_alpha = dstAlpha;
        }
    }
    lower_blend_func(ctx, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void glBlendFunc(GLenum sfactor, GLenum dfactor) {
    glBlendFuncSeparate(sfactor, dfactor, sfactor, dfactor);
}

void glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_blend_equation_separate(ctx, modeRGB, modeAlpha))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_blend_equation(modeRGB) || !valid_blend_equation(modeAlpha)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_blend_equation_separate(ctx, modeRGB, modeAlpha))
        return;
    ctx->canonical_state.blend_equation_rgb = modeRGB;
    ctx->canonical_state.blend_equation_alpha = modeAlpha;
    {
        unsigned i;
        for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i) {
            ctx->canonical_state.draw_buffers[i].blend_equation_rgb = modeRGB;
            ctx->canonical_state.draw_buffers[i].blend_equation_alpha = modeAlpha;
        }
    }
    lower_blend_equation(ctx, 0u, modeRGB, modeAlpha, 0);
}

void glBlendEquation(GLenum mode) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned i;
    if (!advanced_blend_equation(mode)) {
        glBlendEquationSeparate(mode, mode);
        return;
    }
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_blend_equation_separate(ctx, mode, mode))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_blend_equation_separate(ctx, mode, mode))
        return;
    ctx->canonical_state.blend_equation_rgb = mode;
    ctx->canonical_state.blend_equation_alpha = mode;
    for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i) {
        ctx->canonical_state.draw_buffers[i].blend_equation_rgb = mode;
        ctx->canonical_state.draw_buffers[i].blend_equation_alpha = mode;
    }
    lower_blend_equation(ctx, 0u, mode, mode, 0);
}

static int mgl_valid_draw_buffer_index(struct mgl_context *ctx, GLuint index) {
    if (index >= MGL_MAX_DRAW_BUFFERS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    return 1;
}

static int mgl_set_enable_indexed(struct mgl_context *ctx, GLenum cap, GLuint index,
                                  GLboolean enabled, GLboolean ext_semantics) {
    GLboolean *slot;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_enable_indexed(ctx, cap, index, enabled, ext_semantics))
            return 0;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (!ext_semantics && cap != GL_BLEND) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (cap == GL_BLEND) {
        if (!mgl_valid_draw_buffer_index(ctx, index))
            return 0;
        if (mgl_list_record_enable_indexed(ctx, cap, index, enabled, ext_semantics))
            return 0;
        if (ctx->canonical_state.draw_buffers[index].blend == enabled)
            return 1;
        ctx->canonical_state.draw_buffers[index].blend = enabled;
        if (index == 0u)
            mgl_sync_scalar_draw_buffer_zero(&ctx->canonical_state);
        if (ctx->native_dispatch != NULL) {
            if (enabled && ctx->native_dispatch->glesEnablei != NULL)
                ctx->native_dispatch->glesEnablei(GL_BLEND, index);
            else if (!enabled && ctx->native_dispatch->glesDisablei != NULL)
                ctx->native_dispatch->glesDisablei(GL_BLEND, index);
            else if (index == 0u) {
                if (enabled && ctx->native_dispatch->glesEnable != NULL)
                    ctx->native_dispatch->glesEnable(GL_BLEND);
                if (!enabled && ctx->native_dispatch->glesDisable != NULL)
                    ctx->native_dispatch->glesDisable(GL_BLEND);
            }
        }
        return 1;
    }
    slot = mgl_indexed_texture_enable_ptr(ctx, cap, index);
    if (slot != NULL) {
        if (mgl_list_record_enable_indexed(ctx, cap, index, enabled, GL_TRUE))
            return 0;
        *slot = enabled;
        return 1;
    }
    if (cap == GL_TEXTURE_2D || cap == GL_TEXTURE_GEN_S || cap == GL_TEXTURE_GEN_T ||
        cap == GL_TEXTURE_GEN_R || cap == GL_TEXTURE_GEN_Q) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
    return 0;
}

void glEnablei(GLenum cap, GLuint index) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx != NULL)
        (void)mgl_set_enable_indexed(ctx, cap, index, GL_TRUE, GL_FALSE);
}

void glDisablei(GLenum cap, GLuint index) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx != NULL)
        (void)mgl_set_enable_indexed(ctx, cap, index, GL_FALSE, GL_FALSE);
}

void glEnableIndexed(GLenum cap, GLuint index) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx != NULL)
        (void)mgl_set_enable_indexed(ctx, cap, index, GL_TRUE, GL_TRUE);
}

void glDisableIndexed(GLenum cap, GLuint index) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx != NULL)
        (void)mgl_set_enable_indexed(ctx, cap, index, GL_FALSE, GL_TRUE);
}

void glEnableIndexedEXT(GLenum cap, GLuint index) {
    glEnableIndexed(cap, index);
}

void glDisableIndexedEXT(GLenum cap, GLuint index) {
    glDisableIndexed(cap, index);
}

GLboolean glIsEnabledi(GLenum cap, GLuint index) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return GL_FALSE;
    if (reject_inside_begin(ctx))
        return GL_FALSE;
    if (cap == GL_BLEND) {
        if (!mgl_valid_draw_buffer_index(ctx, index))
            return GL_FALSE;
        return ctx->canonical_state.draw_buffers[index].blend;
    }
    mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
    return GL_FALSE;
}

GLboolean glIsEnabledIndexed(GLenum cap, GLuint index) {
    struct mgl_context *ctx = mgl_context_current();
    GLboolean *slot;
    if (ctx == NULL)
        return GL_FALSE;
    if (reject_inside_begin(ctx))
        return GL_FALSE;
    if (cap == GL_BLEND) {
        if (!mgl_valid_draw_buffer_index(ctx, index))
            return GL_FALSE;
        return ctx->canonical_state.draw_buffers[index].blend;
    }
    slot = mgl_indexed_texture_enable_ptr(ctx, cap, index);
    if (slot != NULL)
        return *slot;
    if (cap == GL_TEXTURE_2D || cap == GL_TEXTURE_GEN_S || cap == GL_TEXTURE_GEN_T ||
        cap == GL_TEXTURE_GEN_R || cap == GL_TEXTURE_GEN_Q) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
    return GL_FALSE;
}

GLboolean glIsEnabledIndexedEXT(GLenum cap, GLuint index) {
    return glIsEnabledIndexed(cap, index);
}

void glColorMaski(GLuint index, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_draw_buffer_state *db;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_color_mask_indexed(ctx, index, red, green, blue, alpha))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!mgl_valid_draw_buffer_index(ctx, index))
        return;
    red = red ? GL_TRUE : GL_FALSE;
    green = green ? GL_TRUE : GL_FALSE;
    blue = blue ? GL_TRUE : GL_FALSE;
    alpha = alpha ? GL_TRUE : GL_FALSE;
    if (mgl_list_record_color_mask_indexed(ctx, index, red, green, blue, alpha))
        return;
    db = &ctx->canonical_state.draw_buffers[index];
    db->color_mask[0] = red;
    db->color_mask[1] = green;
    db->color_mask[2] = blue;
    db->color_mask[3] = alpha;
    if (index == 0u)
        mgl_sync_scalar_draw_buffer_zero(&ctx->canonical_state);
    if (ctx->native_dispatch != NULL) {
        if (ctx->native_dispatch->glesColorMaski != NULL)
            ctx->native_dispatch->glesColorMaski(index, red, green, blue, alpha);
        else if (index == 0u && ctx->native_dispatch->glesColorMask != NULL)
            ctx->native_dispatch->glesColorMask(red, green, blue, alpha);
    }
}

void glColorMaskIndexedEXT(GLuint index, GLboolean red, GLboolean green, GLboolean blue,
                           GLboolean alpha) {
    glColorMaski(index, red, green, blue, alpha);
}

void glBlendFuncSeparatei(GLuint index, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
                          GLenum dstAlpha) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_draw_buffer_state *db;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_blend_func_indexed(ctx, index, srcRGB, dstRGB, srcAlpha, dstAlpha))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!mgl_valid_draw_buffer_index(ctx, index))
        return;
    if (!valid_blend_factor(srcRGB) || !valid_blend_factor(dstRGB) ||
        !valid_blend_factor(srcAlpha) || !valid_blend_factor(dstAlpha)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_blend_func_indexed(ctx, index, srcRGB, dstRGB, srcAlpha, dstAlpha))
        return;
    db = &ctx->canonical_state.draw_buffers[index];
    db->blend_src_rgb = srcRGB;
    db->blend_dst_rgb = dstRGB;
    db->blend_src_alpha = srcAlpha;
    db->blend_dst_alpha = dstAlpha;
    if (index == 0u)
        mgl_sync_scalar_draw_buffer_zero(&ctx->canonical_state);
    lower_blend_func_indexed(ctx, index, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void glBlendFunci(GLuint index, GLenum src, GLenum dst) {
    glBlendFuncSeparatei(index, src, dst, src, dst);
}

void glBlendFuncSeparateiARB(GLuint index, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
                             GLenum dstAlpha) {
    glBlendFuncSeparatei(index, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void glBlendFunciARB(GLuint index, GLenum src, GLenum dst) {
    glBlendFunci(index, src, dst);
}

void glBlendFuncSeparateiEXT(GLuint index, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
                             GLenum dstAlpha) {
    glBlendFuncSeparatei(index, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void glBlendFunciEXT(GLuint index, GLenum src, GLenum dst) {
    glBlendFunci(index, src, dst);
}

void glBlendEquationSeparatei(GLuint index, GLenum modeRGB, GLenum modeAlpha) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_draw_buffer_state *db;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_blend_equation_indexed(ctx, index, modeRGB, modeAlpha))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!mgl_valid_draw_buffer_index(ctx, index))
        return;
    if (!valid_blend_equation(modeRGB) || !valid_blend_equation(modeAlpha)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_blend_equation_indexed(ctx, index, modeRGB, modeAlpha))
        return;
    db = &ctx->canonical_state.draw_buffers[index];
    db->blend_equation_rgb = modeRGB;
    db->blend_equation_alpha = modeAlpha;
    if (index == 0u)
        mgl_sync_scalar_draw_buffer_zero(&ctx->canonical_state);
    lower_blend_equation(ctx, index, modeRGB, modeAlpha, 1);
}

void glBlendEquationi(GLuint index, GLenum mode) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_draw_buffer_state *db;
    if (!advanced_blend_equation(mode)) {
        glBlendEquationSeparatei(index, mode, mode);
        return;
    }
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_blend_equation_indexed(ctx, index, mode, mode))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!mgl_valid_draw_buffer_index(ctx, index))
        return;
    if (mgl_list_record_blend_equation_indexed(ctx, index, mode, mode))
        return;
    db = &ctx->canonical_state.draw_buffers[index];
    db->blend_equation_rgb = mode;
    db->blend_equation_alpha = mode;
    if (index == 0u)
        mgl_sync_scalar_draw_buffer_zero(&ctx->canonical_state);
    lower_blend_equation(ctx, index, mode, mode, 1);
}

void glBlendEquationSeparateiARB(GLuint index, GLenum modeRGB, GLenum modeAlpha) {
    glBlendEquationSeparatei(index, modeRGB, modeAlpha);
}

void glBlendEquationiARB(GLuint index, GLenum mode) {
    glBlendEquationi(index, mode);
}

void glBlendEquationSeparateiEXT(GLuint index, GLenum modeRGB, GLenum modeAlpha) {
    glBlendEquationSeparatei(index, modeRGB, modeAlpha);
}

void glBlendEquationiEXT(GLuint index, GLenum mode) {
    glBlendEquationi(index, mode);
}

int mgl_query_component_count(GLenum pname) {
    switch (pname) {
        case GL_PRIMITIVE_BOUNDING_BOX:
            return 8;
        case GL_MODELVIEW_MATRIX:
        case GL_PROJECTION_MATRIX:
        case GL_TEXTURE_MATRIX:
        case GL_COLOR_MATRIX:
            return 16;
        case GL_COLOR_CLEAR_VALUE:
        case GL_ACCUM_CLEAR_VALUE:
        case GL_BLEND_COLOR:
        case GL_COLOR_WRITEMASK:
        case GL_FOG_COLOR:
        case GL_LIGHT_MODEL_AMBIENT:
        case GL_CURRENT_COLOR:
        case GL_CURRENT_SECONDARY_COLOR:
        case GL_CURRENT_TEXTURE_COORDS:
        case GL_CURRENT_RASTER_COLOR:
        case GL_CURRENT_RASTER_SECONDARY_COLOR:
        case GL_CURRENT_RASTER_POSITION:
        case GL_CURRENT_RASTER_TEXTURE_COORDS:
        case GL_VIEWPORT:
        case GL_SCISSOR_BOX:
        case GL_MAP2_GRID_DOMAIN:
        case GL_PATCH_DEFAULT_OUTER_LEVEL:
            return 4;
        case GL_CURRENT_NORMAL:
        case GL_POINT_DISTANCE_ATTENUATION:
            return 3;
        case GL_DEPTH_RANGE:
        case GL_DEPTH_BOUNDS_EXT:
        case GL_POINT_SIZE_RANGE:
        case GL_ALIASED_POINT_SIZE_RANGE:
        case GL_MAX_VIEWPORT_DIMS:
        case GL_ALIASED_LINE_WIDTH_RANGE:
        case GL_MULTISAMPLE_LINE_WIDTH_RANGE:
        case GL_POLYGON_MODE:
        case GL_PATCH_DEFAULT_INNER_LEVEL:
        case GL_MAP1_GRID_DOMAIN:
        case GL_MAP2_GRID_SEGMENTS:
            return 2;
        case GL_COMPRESSED_TEXTURE_FORMATS:
            return (int)mgl_capability_compressed_texture_format_count();
        default:
            return 1;
    }
}

static int mgl_query_component_count_for_context(const struct mgl_context *ctx, GLenum pname) {
    GLint count = 0;
    if (pname != GL_SHADER_BINARY_FORMATS && pname != GL_PROGRAM_BINARY_FORMATS)
        return mgl_query_component_count(pname);
    if (ctx != NULL && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesGetIntegerv != NULL)
        ctx->native_dispatch->glesGetIntegerv(pname == GL_SHADER_BINARY_FORMATS
                                                  ? GL_NUM_SHADER_BINARY_FORMATS
                                                  : GL_NUM_PROGRAM_BINARY_FORMATS,
                                              &count);
    if (count < 0)
        count = 0;
    return pname == GL_SHADER_BINARY_FORMATS ? count + 1 : count;
}

static int mgl_indexed_query_component_count(GLenum target) {
    return target == GL_COLOR_WRITEMASK || target == GL_VIEWPORT || target == GL_SCISSOR_BOX ? 4
           : target == GL_DEPTH_RANGE                                                        ? 2
                                                                                             : 1;
}

void glGetBooleani_v(GLenum target, GLuint index, GLboolean *data) {
    struct mgl_context *ctx = mgl_context_current();
    const struct mgl_draw_buffer_state *db;
    GLint image_value = 0;
    if (ctx == NULL || data == NULL)
        return;
    if (reject_inside_begin(ctx))
        return;
    if (target == GL_VERTEX_BINDING_BUFFER || target == GL_VERTEX_BINDING_STRIDE ||
        target == GL_VERTEX_BINDING_DIVISOR || target == GL_VERTEX_BINDING_OFFSET) {
        GLint64 value = 0;
        uint64_t sequence = ctx->errors.sequence;
        glGetInteger64i_v(target, index, &value);
        if (ctx->errors.sequence == sequence)
            *data = value != 0 ? GL_TRUE : GL_FALSE;
        return;
    }
    if (target == GL_IMAGE_BINDING_NAME || target == GL_IMAGE_BINDING_LEVEL ||
        target == GL_IMAGE_BINDING_LAYERED || target == GL_IMAGE_BINDING_LAYER ||
        target == GL_IMAGE_BINDING_ACCESS || target == GL_IMAGE_BINDING_FORMAT) {
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetIntegeri_v(target, index, &image_value);
        if (ctx->errors.sequence == error_sequence_before)
            data[0] = image_value != 0 ? GL_TRUE : GL_FALSE;
        return;
    }
    if (target == GL_VIEWPORT || target == GL_SCISSOR_BOX || target == GL_DEPTH_RANGE) {
        GLdouble values[4] = {0};
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetDoublei_v(target, index, values);
        if (ctx->errors.sequence != error_sequence_before)
            return;
        for (int i = 0; i < mgl_indexed_query_component_count(target); ++i)
            data[i] = lrint(values[i]) != 0 ? GL_TRUE : GL_FALSE;
        return;
    }
    if (target == GL_BLEND || target == GL_COLOR_WRITEMASK) {
        if (index >= MGL_MAX_DRAW_BUFFERS) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        db = &ctx->canonical_state.draw_buffers[index];
        if (target == GL_BLEND)
            data[0] = db->blend;
        else
            memcpy(data, db->color_mask, sizeof(db->color_mask));
        return;
    }
    {
        GLint64 value = 0;
        uint64_t sequence = ctx->errors.sequence;
        glGetInteger64i_v(target, index, &value);
        if (ctx->errors.sequence == sequence)
            data[0] = value != 0 ? GL_TRUE : GL_FALSE;
    }
}

void glGetBooleanIndexedv(GLenum target, GLuint index, GLboolean *data) {
    glGetBooleani_v(target, index, data);
}

void glGetBooleanIndexedvEXT(GLenum target, GLuint index, GLboolean *data) {
    glGetBooleani_v(target, index, data);
}

void glGetBooleanv(GLenum pname, GLboolean *data) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_GET_STATE);
    GLfloat local[16] = {0};
    GLfloat *values = local;
    int count;
    int i;
    if (ctx == NULL || data == NULL)
        return;
    if (reject_inside_begin(ctx))
        return;
    if (pname == GL_PRIMITIVE_BOUNDING_BOX) {
        GLfloat bounds[8];
        if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetFloatv == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        ctx->native_dispatch->glesGetFloatv(pname, bounds);
        for (i = 0; i < 8; ++i)
            data[i] = bounds[i] != 0.0f ? GL_TRUE : GL_FALSE;
        return;
    }
    switch (pname) {
        case GL_DEPTH_WRITEMASK:
            data[0] = ctx->canonical_state.depth_mask;
            return;
        case GL_COLOR_WRITEMASK:
            for (i = 0; i < 4; ++i)
                data[i] = ctx->canonical_state.color_mask[i];
            return;
        case GL_MAP_COLOR:
            data[0] = ctx->pixel_transfer.map_color;
            return;
        case GL_MAP_STENCIL:
            data[0] = ctx->pixel_transfer.map_stencil;
            return;
        case GL_SAMPLE_COVERAGE_INVERT:
            data[0] = ctx->canonical_state.sample_coverage_invert;
            return;
        case GL_SAMPLE_MASK:
            data[0] = ctx->canonical_state.sample_mask;
            return;
        case GL_EDGE_FLAG:
            data[0] = ctx->current_edge_flag;
            return;
        case GL_CURRENT_RASTER_POSITION_VALID:
            data[0] = ctx->compat_raster.valid;
            return;
        case GL_FOG_COORD_ARRAY:
            data[0] = ctx->current_vao->compat[MGL_COMPAT_SEM_FOG].enabled;
            return;
        case GL_EDGE_FLAG_ARRAY:
            data[0] = ctx->current_vao->edge_flag.enabled;
            return;
        case GL_INDEX_ARRAY:
            data[0] = ctx->current_vao->color_index.enabled;
            return;
        case GL_TEXTURE_2D:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_CUBE_MAP:
        case GL_COLOR_SUM:
        case GL_POLYGON_STIPPLE:
        case GL_LINE_SMOOTH:
        case GL_LINE_STIPPLE:
        case GL_FRAMEBUFFER_SRGB:
        case GL_TEXTURE_CUBE_MAP_SEAMLESS:
        case GL_FOG:
        case GL_LIGHTING:
        case GL_COLOR_MATERIAL:
        case GL_NORMALIZE:
        case GL_RESCALE_NORMAL:
        case GL_LIGHT0:
        case GL_LIGHT1:
        case GL_LIGHT2:
        case GL_LIGHT3:
        case GL_LIGHT4:
        case GL_LIGHT5:
        case GL_LIGHT6:
        case GL_LIGHT7:
        case GL_TEXTURE_GEN_S:
        case GL_TEXTURE_GEN_T:
        case GL_TEXTURE_GEN_R:
        case GL_TEXTURE_GEN_Q:
        case GL_CLIP_PLANE0:
        case GL_CLIP_PLANE1:
        case GL_CLIP_PLANE2:
        case GL_CLIP_PLANE3:
        case GL_CLIP_PLANE4:
        case GL_CLIP_PLANE5:
        case GL_AUTO_NORMAL:
        case GL_MAP1_COLOR_4:
        case GL_MAP1_INDEX:
        case GL_MAP1_NORMAL:
        case GL_MAP1_TEXTURE_COORD_1:
        case GL_MAP1_TEXTURE_COORD_2:
        case GL_MAP1_TEXTURE_COORD_3:
        case GL_MAP1_TEXTURE_COORD_4:
        case GL_MAP1_VERTEX_3:
        case GL_MAP1_VERTEX_4:
        case GL_MAP2_COLOR_4:
        case GL_MAP2_INDEX:
        case GL_MAP2_NORMAL:
        case GL_MAP2_TEXTURE_COORD_1:
        case GL_MAP2_TEXTURE_COORD_2:
        case GL_MAP2_TEXTURE_COORD_3:
        case GL_MAP2_TEXTURE_COORD_4:
        case GL_MAP2_VERTEX_3:
        case GL_MAP2_VERTEX_4:
        case GL_COLOR_LOGIC_OP:
        case GL_ALPHA_TEST:
        case GL_POINT_SPRITE:
        case GL_PROGRAM_POINT_SIZE:
        case GL_DEPTH_TEST:
        case GL_DEPTH_BOUNDS_TEST_EXT:
        case GL_STENCIL_TEST:
        case GL_BLEND:
        case GL_CULL_FACE:
        case GL_SCISSOR_TEST:
        case GL_DITHER:
        case GL_POLYGON_OFFSET_FILL:
        case GL_POLYGON_OFFSET_POINT:
        case GL_POLYGON_OFFSET_LINE:
        case GL_TEXTURE_RECTANGLE:
        case GL_SAMPLE_ALPHA_TO_COVERAGE:
        case GL_SAMPLE_COVERAGE:
            data[0] = glIsEnabled(pname);
            return;
        default:
            break;
    }
    count = mgl_query_component_count_for_context(ctx, pname);
    if (count > (int)(sizeof(local) / sizeof(local[0]))) {
        values = malloc((size_t)count * sizeof(*values));
        if (values == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
    }
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetFloatv(pname, values);
    if (ctx->errors.sequence == error_sequence_before)
        for (i = 0; i < count; ++i)
            data[i] = values[i] != 0.0f ? GL_TRUE : GL_FALSE;
    if (values != local)
        free(values);
}

static GLint gles_limit_or_default(const struct mgl_context *ctx, GLenum pname, GLint fallback) {
    GLint value = 0;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegerv != NULL) {
        ctx->native_dispatch->glesGetIntegerv(pname, &value);
        if (value > 0)
            return value;
    }
    return fallback;
}

void glGetMultisamplefv(GLenum pname, GLuint index, GLfloat *val) {
    struct mgl_context *ctx = mgl_context_current();
    GLint samples = 0;
    if (ctx == NULL || reject_inside_begin(ctx))
        return;
    if (pname != GL_SAMPLE_POSITION) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetIntegerv == NULL ||
        ctx->native_dispatch->glesGetMultisamplefv == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesGetIntegerv(GL_SAMPLES, &samples);
    if (samples <= 0 || index >= (GLuint)samples) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    ctx->native_dispatch->glesGetMultisamplefv(pname, index, val);
}

static int mgl_scalar_64bit_state(GLenum pname) {
    switch (pname) {
        case GL_TIMESTAMP:
        case GL_MAX_SERVER_WAIT_TIMEOUT:
        case GL_MAX_SHADER_STORAGE_BLOCK_SIZE:
        case GL_MAX_UNIFORM_BLOCK_SIZE:
        case GL_MAX_ELEMENT_INDEX:
        case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
        case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
        case GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS:
        case GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS:
        case GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS:
        case GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS:
            return 1;
        default:
            return 0;
    }
}

void glGetInteger64v(GLenum pname, GLint64 *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLint local[16] = {0};
    GLint *values = local;
    GLint count = 1, i;
    if (ctx == NULL || params == NULL || reject_inside_begin(ctx))
        return;
    if (pname == GL_PRIMITIVE_BOUNDING_BOX) {
        if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetInteger64v == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        ctx->native_dispatch->glesGetInteger64v(pname, params);
        return;
    }
    if (mgl_scalar_64bit_state(pname)) {
        if (pname == GL_TIMESTAMP && (ctx->native_dispatch == NULL ||
                                      !mgl_native_dispatch_has_gles_extension(
                                          ctx->native_dispatch, "GL_EXT_disjoint_timer_query"))) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetInteger64v == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        ctx->native_dispatch->glesGetInteger64v(pname, params);
        return;
    }
    count = mgl_query_component_count_for_context(ctx, pname);
    if (count > (GLint)(sizeof(local) / sizeof(local[0]))) {
        values = malloc((size_t)count * sizeof(*values));
        if (values == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
    }
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetIntegerv(pname, values);
    if (ctx->errors.sequence == error_sequence_before)
        for (i = 0; i < count; ++i)
            params[i] = (GLint64)values[i];
    if (values != local)
        free(values);
}

static GLint mgl_get_float_as_integer(GLfloat value, int normalized) {
    double rounded = normalized ? (double)value * (double)INT_MAX : (double)value;
    if (rounded >= (double)INT_MAX)
        return INT_MAX;
    if (rounded <= (double)INT_MIN)
        return INT_MIN;
    return (GLint)nearbyint(rounded);
}

void glGetIntegerv(GLenum pname, GLint *data) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_GET_STATE);
    if (ctx == NULL || data == NULL)
        return;
    if (reject_inside_begin(ctx))
        return;
    switch (pname) {
        case GL_VERTEX_ARRAY:
        case GL_NORMAL_ARRAY:
        case GL_COLOR_ARRAY:
        case GL_SECONDARY_COLOR_ARRAY:
        case GL_TEXTURE_COORD_ARRAY:
        case GL_ALPHA_TEST:
        case GL_AUTO_NORMAL:
        case GL_BLEND:
        case GL_CLIP_PLANE0:
        case GL_CLIP_PLANE1:
        case GL_CLIP_PLANE2:
        case GL_CLIP_PLANE3:
        case GL_CLIP_PLANE4:
        case GL_CLIP_PLANE5:
        case GL_COLOR_MATERIAL:
        case GL_CULL_FACE:
        case GL_DEPTH_BOUNDS_TEST_EXT:
        case GL_DEPTH_TEST:
        case GL_DITHER:
        case GL_FOG:
        case GL_LIGHT0:
        case GL_LIGHT1:
        case GL_LIGHT2:
        case GL_LIGHT3:
        case GL_LIGHT4:
        case GL_LIGHT5:
        case GL_LIGHT6:
        case GL_LIGHT7:
        case GL_LIGHTING:
        case GL_MAP1_COLOR_4:
        case GL_MAP1_INDEX:
        case GL_MAP1_NORMAL:
        case GL_MAP1_TEXTURE_COORD_1:
        case GL_MAP1_TEXTURE_COORD_2:
        case GL_MAP1_TEXTURE_COORD_3:
        case GL_MAP1_TEXTURE_COORD_4:
        case GL_MAP1_VERTEX_3:
        case GL_MAP1_VERTEX_4:
        case GL_MAP2_COLOR_4:
        case GL_MAP2_INDEX:
        case GL_MAP2_NORMAL:
        case GL_MAP2_TEXTURE_COORD_1:
        case GL_MAP2_TEXTURE_COORD_2:
        case GL_MAP2_TEXTURE_COORD_3:
        case GL_MAP2_TEXTURE_COORD_4:
        case GL_MAP2_VERTEX_3:
        case GL_MAP2_VERTEX_4:
        case GL_NORMALIZE:
        case GL_POLYGON_OFFSET_FILL:
        case GL_RESCALE_NORMAL:
        case GL_SAMPLE_ALPHA_TO_COVERAGE:
        case GL_SAMPLE_COVERAGE:
        case GL_SCISSOR_TEST:
        case GL_STENCIL_TEST:
        case GL_TEXTURE_2D:
        case GL_TEXTURE_GEN_Q:
        case GL_TEXTURE_GEN_R:
        case GL_TEXTURE_GEN_S:
        case GL_TEXTURE_GEN_T:
        case GL_MULTISAMPLE:
        case GL_DEBUG_OUTPUT:
        case GL_DEBUG_OUTPUT_SYNCHRONOUS:
        case GL_PRIMITIVE_RESTART:
        case GL_STENCIL_TEST_TWO_SIDE_EXT:
            *data = glIsEnabled(pname) ? 1 : 0;
            return;
        case GL_ALPHA_BIAS:
        case GL_ALPHA_SCALE:
        case GL_ALPHA_TEST_REF:
        case GL_BLEND_COLOR:
        case GL_BLUE_BIAS:
        case GL_BLUE_SCALE:
        case GL_COLOR_CLEAR_VALUE:
        case GL_COLOR_MATRIX:
        case GL_CURRENT_COLOR:
        case GL_CURRENT_FOG_COORD:
        case GL_CURRENT_INDEX:
        case GL_CURRENT_NORMAL:
        case GL_CURRENT_RASTER_COLOR:
        case GL_CURRENT_RASTER_POSITION:
        case GL_CURRENT_RASTER_SECONDARY_COLOR:
        case GL_CURRENT_RASTER_TEXTURE_COORDS:
        case GL_CURRENT_SECONDARY_COLOR:
        case GL_CURRENT_TEXTURE_COORDS:
        case GL_DEPTH_BIAS:
        case GL_DEPTH_BOUNDS_EXT:
        case GL_DEPTH_CLEAR_VALUE:
        case GL_DEPTH_RANGE:
        case GL_DEPTH_SCALE:
        case GL_FOG_COLOR:
        case GL_FOG_DENSITY:
        case GL_FOG_END:
        case GL_FOG_INDEX:
        case GL_FOG_START:
        case GL_GREEN_BIAS:
        case GL_GREEN_SCALE:
        case GL_LIGHT_MODEL_AMBIENT:
        case GL_LINE_WIDTH:
        case GL_MAP1_GRID_DOMAIN:
        case GL_MAP2_GRID_DOMAIN:
        case GL_MODELVIEW_MATRIX:
        case GL_POLYGON_OFFSET_FACTOR:
        case GL_POLYGON_OFFSET_UNITS:
        case GL_PROJECTION_MATRIX:
        case GL_RED_BIAS:
        case GL_RED_SCALE:
        case GL_SAMPLE_COVERAGE_VALUE:
        case GL_TEXTURE_MATRIX:
        case GL_ZOOM_X:
        case GL_ZOOM_Y: {
            GLfloat values[16] = {0};
            const int count = mgl_query_component_count(pname);
            const int normalized =
                pname == GL_ALPHA_TEST_REF || pname == GL_BLEND_COLOR ||
                pname == GL_COLOR_CLEAR_VALUE || pname == GL_CURRENT_COLOR ||
                pname == GL_CURRENT_NORMAL || pname == GL_CURRENT_RASTER_COLOR ||
                pname == GL_CURRENT_RASTER_SECONDARY_COLOR ||
                pname == GL_CURRENT_SECONDARY_COLOR || pname == GL_DEPTH_CLEAR_VALUE ||
                pname == GL_DEPTH_RANGE || pname == GL_FOG_COLOR ||
                pname == GL_LIGHT_MODEL_AMBIENT;
            uint64_t sequence = ctx->errors.sequence;
            glGetFloatv(pname, values);
            if (ctx->errors.sequence != sequence)
                return;
            for (int i = 0; i < count; ++i)
                data[i] = mgl_get_float_as_integer(values[i], normalized);
            return;
        }
        case GL_SAMPLE_COVERAGE_INVERT:
            *data = ctx->canonical_state.sample_coverage_invert ? 1 : 0;
            return;
        case GL_SAMPLE_MASK:
            *data = ctx->canonical_state.sample_mask ? 1 : 0;
            return;
        case GL_CURRENT_RASTER_POSITION_VALID:
            *data = ctx->compat_raster.valid ? 1 : 0;
            return;
        case GL_PRIMITIVE_BOUNDING_BOX:
            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetIntegerv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetIntegerv(pname, data);
            return;
        case GL_SHADER_COMPILER:
            *data = GL_TRUE;
            return;
        case GL_RED_BITS:
        case GL_GREEN_BITS:
        case GL_BLUE_BITS:
        case GL_ALPHA_BITS:
        case GL_DEPTH_BITS:
        case GL_STENCIL_BITS:
        case GL_SUBPIXEL_BITS:
        case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
        case GL_IMPLEMENTATION_COLOR_READ_TYPE:
            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetIntegerv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetIntegerv(pname, data);
            return;
        case GL_FEEDBACK_BUFFER_SIZE:
            *data = ctx->selection.feedback_size;
            return;
        case GL_FEEDBACK_BUFFER_TYPE:
            *data = (GLint)ctx->selection.feedback_type;
            return;
        case GL_MAX_VIEWPORTS:
            *data = (GLint)viewport_array_limit(ctx);
            return;
        case GL_MAX_VERTEX_STREAMS:
            *data = 1;
            if (ctx->native_dispatch != NULL &&
                mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                       "GL_ARB_transform_feedback3") &&
                ctx->native_dispatch->glesGetIntegerv != NULL) {
                GLint native_streams = 1;
                ctx->native_dispatch->glesGetIntegerv(GL_MAX_VERTEX_STREAMS, &native_streams);
                if (native_streams > 1)
                    *data = native_streams;
            }
            return;
        case GL_VIEWPORT_SUBPIXEL_BITS:
            if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegerv != NULL &&
                mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                       "GL_OES_viewport_array"))
                ctx->native_dispatch->glesGetIntegerv(GL_VIEWPORT_SUBPIXEL_BITS, data);
            else
                *data = 0;
            return;
        case GL_TIMESTAMP:
        case GL_GPU_DISJOINT_EXT:
            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetIntegerv == NULL ||
                !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                        "GL_EXT_disjoint_timer_query")) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetIntegerv(pname, data);
            return;
        case GL_MAX_ELEMENT_INDEX:
        case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
        case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
        case GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS:
        case GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS:
        case GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS:
        case GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS: {
            GLint64 value = 0;
            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetInteger64v == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetInteger64v(pname, &value);
            *data = value > INT_MAX ? INT_MAX : value < INT_MIN ? INT_MIN : (GLint)value;
            return;
        }
        case GL_MAX_SHADER_STORAGE_BLOCK_SIZE: {
            GLint64 size = 0;
            if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetInteger64v != NULL) {
                ctx->native_dispatch->glesGetInteger64v(pname, &size);
                *data = size > INT_MAX ? INT_MAX : size < 0 ? 0 : (GLint)size;
            } else {
                *data = 0;
            }
            return;
        }
        case GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS: {
            GLint native_limit = gles_limit_or_default(ctx, pname, 8);
            *data = native_limit < (GLint)MGL_MAX_INDEXED_BUFFER_BINDINGS
                        ? native_limit : (GLint)MGL_MAX_INDEXED_BUFFER_BINDINGS;
            return;
        }
        case GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS: {
            GLint native_limit = gles_limit_or_default(ctx, pname, 8);
            *data = native_limit < (GLint)MGL_MAX_INDEXED_BUFFER_BINDINGS
                        ? native_limit : (GLint)MGL_MAX_INDEXED_BUFFER_BINDINGS;
            return;
        }
        case GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS:
        case GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS:
        case GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS:
        case GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS:
        case GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_TESS_GEN_LEVEL:
        case GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS:
        case GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS:
        case GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS:
        case GL_MAX_TESS_PATCH_COMPONENTS:
        case GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS:
        case GL_MAX_COMBINED_IMAGE_UNIFORMS:
        case GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS:
        case GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS:
        case GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS:
        case GL_MAX_COMPUTE_UNIFORM_BLOCKS:
        case GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS:
        case GL_MAX_FRAGMENT_IMAGE_UNIFORMS:
        case GL_MAX_VERTEX_IMAGE_UNIFORMS:
        case GL_MAX_COMPUTE_IMAGE_UNIFORMS:
        case GL_MAX_GEOMETRY_IMAGE_UNIFORMS:
        case GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS:
        case GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS:
        case GL_MAX_COMBINED_ATOMIC_COUNTERS:
        case GL_MAX_VERTEX_ATOMIC_COUNTERS:
        case GL_MAX_FRAGMENT_ATOMIC_COUNTERS:
        case GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS:
        case GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS:
        case GL_MAX_COMPUTE_ATOMIC_COUNTERS:
        case GL_MAX_COMPUTE_UNIFORM_COMPONENTS:
        case GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS:
        case GL_MAX_COMPUTE_SHARED_MEMORY_SIZE:
            if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegerv != NULL)
                ctx->native_dispatch->glesGetIntegerv(pname, data);
            else
                *data = 0;
            return;
        case GL_MAX_VIEWPORT_DIMS:

            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetIntegerv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetIntegerv(GL_MAX_VIEWPORT_DIMS, data);
            return;
        case GL_VIEWPORT:
            data[0] = ctx->canonical_state.viewport[0];
            data[1] = ctx->canonical_state.viewport[1];
            data[2] = ctx->canonical_state.viewport[2];
            data[3] = ctx->canonical_state.viewport[3];
            return;
        case GL_SCISSOR_BOX:
            data[0] = ctx->canonical_state.scissor[0];
            data[1] = ctx->canonical_state.scissor[1];
            data[2] = ctx->canonical_state.scissor[2];
            data[3] = ctx->canonical_state.scissor[3];
            return;
        case GL_ACCUM_RED_BITS:
        case GL_ACCUM_GREEN_BITS:
        case GL_ACCUM_BLUE_BITS:
        case GL_ACCUM_ALPHA_BITS:
            *data = 0;
            return;
        case GL_ACCUM_CLEAR_VALUE:
            for (unsigned i = 0u; i < 4u; ++i)
                data[i] = (GLint)lroundf(ctx->canonical_state.clear_accum[i]);
            return;
        case GL_EDGE_FLAG:
            *data = (GLint)ctx->current_edge_flag;
            return;
        case GL_LINE_STIPPLE_PATTERN:
            *data = (GLint)ctx->canonical_state.line_stipple_pattern;
            return;
        case GL_LINE_STIPPLE_REPEAT:
            *data = ctx->canonical_state.line_stipple_factor;
            return;
        case GL_ALPHA_TEST_FUNC:
            *data = (GLint)ctx->canonical_state.alpha_func;
            return;
        case GL_FOG_MODE:
            *data = (GLint)ctx->compat.fog_mode;
            return;
        case GL_FOG_COORD_SRC:
            *data = (GLint)ctx->compat.fog_coord_source;
            return;
        case GL_FOG_COORD_ARRAY:
            *data = ctx->current_vao->compat[MGL_COMPAT_SEM_FOG].enabled ? 1 : 0;
            return;
        case GL_FOG_COORD_ARRAY_TYPE:
            *data = (GLint)ctx->current_vao->compat[MGL_COMPAT_SEM_FOG].type;
            return;
        case GL_FOG_COORD_ARRAY_STRIDE:
            *data = ctx->current_vao->compat[MGL_COMPAT_SEM_FOG].stride;
            return;
        case GL_FOG_COORD_ARRAY_BUFFER_BINDING:
            *data =
                (GLint)(ctx->current_vao->compat[MGL_COMPAT_SEM_FOG].data.source == MGL_DATA_BUFFER
                            ? ctx->current_vao->compat[MGL_COMPAT_SEM_FOG].data.buffer_name
                            : 0u);
            return;
        case GL_CLIP_ORIGIN:
            *data = (GLint)ctx->canonical_state.clip_origin;
            return;
        case GL_CLIP_DEPTH_MODE:
            *data = (GLint)ctx->canonical_state.clip_depth_mode;
            return;
        case GL_POLYGON_MODE:
            data[0] = (GLint)ctx->canonical_state.polygon_mode[0];
            data[1] = (GLint)ctx->canonical_state.polygon_mode[1];
            return;
        case GL_POLYGON_OFFSET_CLAMP:
            *data = (GLint)lroundf(ctx->canonical_state.polygon_offset_clamp);
            return;
        case GL_MIN_SAMPLE_SHADING_VALUE:
            *data = (GLint)lroundf(ctx->canonical_state.min_sample_shading);
            return;
        case GL_PATCH_DEFAULT_INNER_LEVEL:
            for (int i = 0; i < 2; ++i)
                data[i] = (GLint)lroundf(ctx->canonical_state.patch_inner_levels[i]);
            return;
        case GL_PATCH_DEFAULT_OUTER_LEVEL:
            for (int i = 0; i < 4; ++i)
                data[i] = (GLint)lroundf(ctx->canonical_state.patch_outer_levels[i]);
            return;
        case GL_PATCH_VERTICES:
            *data = ctx->canonical_state.patch_vertices;
            return;
        case GL_SAMPLE_SHADING:
            *data = ctx->canonical_state.sample_shading;
            return;
        case GL_COLOR_LOGIC_OP:
        case GL_POLYGON_STIPPLE:
        case GL_LINE_SMOOTH:
        case GL_LINE_STIPPLE:
        case GL_FRAMEBUFFER_SRGB:
        case GL_TEXTURE_CUBE_MAP_SEAMLESS:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_CUBE_MAP:
        case GL_COLOR_SUM:
        case GL_TEXTURE_RECTANGLE:
        case GL_POLYGON_OFFSET_POINT:
        case GL_POLYGON_OFFSET_LINE:
            *data = glIsEnabled(pname) ? 1 : 0;
            return;
        case GL_MAX_PATCH_VERTICES:
            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetIntegerv == NULL ||
                ctx->native_dispatch->glesPatchParameteri == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return;
            }
            ctx->native_dispatch->glesGetIntegerv(GL_MAX_PATCH_VERTICES, data);
            return;
        case GL_DEPTH_WRITEMASK:
            *data = ctx->canonical_state.depth_mask ? 1 : 0;
            return;
        case GL_COLOR_WRITEMASK:
            for (int i = 0; i < 4; ++i)
                data[i] = ctx->canonical_state.color_mask[i] ? 1 : 0;
            return;
        case GL_DEPTH_FUNC:
            *data = (GLint)ctx->canonical_state.depth_func;
            return;
        case GL_CULL_FACE_MODE:
            *data = (GLint)ctx->canonical_state.cull_face_mode;
            return;
        case GL_FRONT_FACE:
            *data = (GLint)ctx->canonical_state.front_face;
            return;
        case GL_PERSPECTIVE_CORRECTION_HINT:
            *data = (GLint)ctx->canonical_state.perspective_correction_hint;
            return;
        case GL_POINT_SMOOTH_HINT:
            *data = (GLint)ctx->canonical_state.point_smooth_hint;
            return;
        case GL_POINT_SIZE_RANGE:
        case GL_ALIASED_POINT_SIZE_RANGE:
            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetIntegerv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetIntegerv(GL_ALIASED_POINT_SIZE_RANGE, data);
            return;
        case GL_POINT_SIZE_GRANULARITY:

            *data = 1;
            return;
        case GL_POINT_SIZE_MIN:
            *data = (GLint)ctx->canonical_state.point_size_min;
            return;
        case GL_POINT_SIZE_MAX:
            *data = (GLint)ctx->canonical_state.point_size_max;
            return;
        case GL_POINT_FADE_THRESHOLD_SIZE:
            *data = (GLint)ctx->canonical_state.point_fade_threshold_size;
            return;
        case GL_POINT_DISTANCE_ATTENUATION:
            for (int k = 0; k < 3; ++k)
                data[k] = (GLint)ctx->canonical_state.point_distance_attenuation[k];
            return;
        case GL_LOGIC_OP_MODE:
            *data = (GLint)ctx->canonical_state.logic_op_mode;
            return;
        case GL_POINT_SIZE:
            *data = (GLint)ctx->canonical_state.point_size;
            return;
        case GL_POINT_SPRITE:
            *data = (GLint)ctx->canonical_state.point_sprite_enabled;
            return;
        case GL_PROGRAM_POINT_SIZE:
            *data = (GLint)ctx->canonical_state.program_point_size_enabled;
            return;
        case GL_POINT_SPRITE_COORD_ORIGIN:
            *data = (GLint)ctx->canonical_state.point_sprite_coord_origin;
            return;
        case GL_LINE_SMOOTH_HINT:
            *data = (GLint)ctx->canonical_state.line_smooth_hint;
            return;
        case GL_POLYGON_SMOOTH_HINT:
            *data = (GLint)ctx->canonical_state.polygon_smooth_hint;
            return;
        case GL_FOG_HINT:
            *data = (GLint)ctx->canonical_state.fog_hint;
            return;
        case GL_GENERATE_MIPMAP_HINT:
            *data = (GLint)ctx->canonical_state.generate_mipmap_hint;
            return;
        case GL_TEXTURE_COMPRESSION_HINT:
            *data = (GLint)ctx->canonical_state.texture_compression_hint;
            return;
        case GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
            *data = (GLint)ctx->canonical_state.fragment_shader_derivative_hint;
            return;
        case GL_PRIMITIVE_RESTART_INDEX:
            *data = (GLint)ctx->primitive_restart_index;
            return;
        case GL_PROVOKING_VERTEX:
            *data = (GLint)ctx->provoking_vertex_mode;
            return;
        case GL_MAX_VERTEX_ATTRIB_BINDINGS:
            *data = (GLint)mgl_context_vertex_binding_limit(ctx);
            return;
        case GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET:
            *data = 2047;
            return;
        case GL_MAX_VERTEX_ATTRIB_STRIDE:
            *data = 2048;
            return;
        case GL_MAJOR_VERSION:
            *data = MGL_OPENGL_VERSION_MAJOR;
            return;
        case GL_MINOR_VERSION:
            *data = MGL_OPENGL_VERSION_MINOR;
            return;
        case GL_NUM_EXTENSIONS:
            *data = (GLint)mgl_capability_extension_count(ctx);
            return;
        case GL_NUM_SHADING_LANGUAGE_VERSIONS:
            *data = 1;
            return;
        case GL_NUM_SPIR_V_EXTENSIONS:
            *data = 0;
            return;
        case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
            *data = (GLint)mgl_capability_compressed_texture_format_count();
            return;
        case GL_COMPRESSED_TEXTURE_FORMATS: {
            GLuint i;
            for (i = 0u; i < mgl_capability_compressed_texture_format_count(); ++i)
                data[i] = (GLint)mgl_capability_compressed_texture_format(i);
            return;
        }
        case GL_CONTEXT_FLAGS:
            *data = (GLint)ctx->context_flags;
            return;
        case GL_CONTEXT_PROFILE_MASK:
            *data = (GLint)ctx->profile_mask;
            return;
        case GL_DEBUG_LOGGED_MESSAGES:
            *data = (GLint)ctx->debug.queue_count;
            return;
        case GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
            *data = ctx->debug.queue_count
                        ? (GLint)(ctx->debug.queue[ctx->debug.queue_head].length + 1)
                        : 0;
            return;
        case GL_DEBUG_GROUP_STACK_DEPTH:
            *data = (GLint)ctx->debug.group_depth;
            return;
        case GL_MAX_DEBUG_MESSAGE_LENGTH:
            *data = (GLint)MGL_DEBUG_TEXT_CAPACITY;
            return;
        case GL_MAX_DEBUG_LOGGED_MESSAGES:
            *data = (GLint)MGL_DEBUG_QUEUE_CAPACITY;
            return;
        case GL_MAX_DEBUG_GROUP_STACK_DEPTH:
            *data = (GLint)MGL_DEBUG_GROUP_MAX_DEPTH;
            return;
        case GL_MAX_LABEL_LENGTH:
            *data = 256;
            return;
        case GL_BLEND_SRC_RGB:
            *data = (GLint)ctx->canonical_state.blend_src_rgb;
            return;
        case GL_BLEND_DST_RGB:
            *data = (GLint)ctx->canonical_state.blend_dst_rgb;
            return;
        case GL_BLEND_SRC_ALPHA:
            *data = (GLint)ctx->canonical_state.blend_src_alpha;
            return;
        case GL_BLEND_DST_ALPHA:
            *data = (GLint)ctx->canonical_state.blend_dst_alpha;
            return;
        case GL_BLEND_EQUATION_RGB:
            *data = (GLint)ctx->canonical_state.blend_equation_rgb;
            return;
        case GL_BLEND_EQUATION_ALPHA:
            *data = (GLint)ctx->canonical_state.blend_equation_alpha;
            return;
        case GL_MAX_TEXTURE_UNITS:
        case GL_MAX_TEXTURE_COORDS:
            *data = (GLint)MGL_TEXTURE_UNITS;
            return;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
            *data = (GLint)mgl_programmable_texture_limit(ctx);
            return;
        case GL_MAX_TEXTURE_IMAGE_UNITS:
            *data = ctx->fragment_texture_image_units > 0 ? ctx->fragment_texture_image_units
                                                          : (GLint)MGL_TEXTURE_UNITS;
            return;
        case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
            *data = ctx->vertex_texture_image_units > 0 ? ctx->vertex_texture_image_units
                                                        : (GLint)MGL_TEXTURE_UNITS;
            return;
        case GL_MAX_RECTANGLE_TEXTURE_SIZE:
        case GL_MAX_TEXTURE_SIZE:
            *data = gles_limit_or_default(ctx, GL_MAX_TEXTURE_SIZE, 2048);
            return;
        case GL_MAX_CUBE_MAP_TEXTURE_SIZE:

            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetIntegerv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, data);
            return;
        case GL_MAX_RENDERBUFFER_SIZE:
        case GL_MAX_UNIFORM_BLOCK_SIZE:
            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetIntegerv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetIntegerv(pname, data);
            return;
        case GL_MAX_ARRAY_TEXTURE_LAYERS:
            /* The native GLES 3.0 layer limit supplies the 2D-array texture limit when
             * available. */
            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetIntegerv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, data);
            return;
        case GL_MAX_3D_TEXTURE_SIZE:
            *data = gles_limit_or_default(ctx, GL_MAX_3D_TEXTURE_SIZE, 256);
            return;
        case GL_MAX_VERTEX_ATTRIBS: {
            GLint limit = gles_limit_or_default(ctx, GL_MAX_VERTEX_ATTRIBS, 16);
            *data = limit < (GLint)MGL_MAX_VERTEX_ATTRIBS ? limit : (GLint)MGL_MAX_VERTEX_ATTRIBS;
            return;
        }
        case GL_SAMPLE_BUFFERS:

            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetIntegerv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetIntegerv(GL_SAMPLE_BUFFERS, data);
            return;
        case GL_SAMPLES:
            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetIntegerv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return;
            }
            ctx->native_dispatch->glesGetIntegerv(GL_SAMPLES, data);
            return;
        case GL_MAX_SAMPLE_MASK_WORDS:
            *data = 1;
            return;
        case GL_MAX_SAMPLES:
            *data = gles_limit_or_default(ctx, GL_MAX_SAMPLES, 4);
            return;

        case GL_MIN_PROGRAM_TEXEL_OFFSET:
        case GL_MAX_PROGRAM_TEXEL_OFFSET:
        case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
        case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
        case GL_MAX_VERTEX_OUTPUT_COMPONENTS:
        case GL_MAX_UNIFORM_LOCATIONS:
        case GL_MAX_FRAMEBUFFER_WIDTH:
        case GL_MAX_FRAMEBUFFER_HEIGHT:
        case GL_MAX_FRAMEBUFFER_SAMPLES:
        case GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE:
        case GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES:
        case GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET:
        case GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET:
        case GL_MAX_COLOR_TEXTURE_SAMPLES:
        case GL_MAX_DEPTH_TEXTURE_SAMPLES:
        case GL_MAX_INTEGER_SAMPLES:
        case GL_MAX_GEOMETRY_UNIFORM_COMPONENTS:
        case GL_MAX_GEOMETRY_INPUT_COMPONENTS:
        case GL_MAX_GEOMETRY_OUTPUT_COMPONENTS:
        case GL_MAX_GEOMETRY_OUTPUT_VERTICES:
        case GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS:
        case GL_MAX_GEOMETRY_SHADER_INVOCATIONS:
        case GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_GEOMETRY_ATOMIC_COUNTERS:
        case GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS:
        case GL_MAX_FRAMEBUFFER_LAYERS:
        case GL_MIN_FRAGMENT_INTERPOLATION_OFFSET:
        case GL_MAX_FRAGMENT_INTERPOLATION_OFFSET:
        case GL_ALIASED_LINE_WIDTH_RANGE:
        case GL_MULTISAMPLE_LINE_WIDTH_RANGE:
        case GL_MULTISAMPLE_LINE_WIDTH_GRANULARITY:
        case GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS:
        case GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS:
        case GL_MAX_TESS_CONTROL_INPUT_COMPONENTS:
        case GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS:
        case GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS:
        case GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS:
        case GL_MAX_ELEMENTS_VERTICES:
        case GL_MAX_ELEMENTS_INDICES:
        case GL_MAX_VERTEX_UNIFORM_VECTORS:
        case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
        case GL_MAX_VARYING_VECTORS:
        case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetIntegerv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetIntegerv(pname, data);
            return;
        case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
            *data = 4 * gles_limit_or_default(ctx, GL_MAX_VERTEX_UNIFORM_VECTORS, 256);
            return;
        case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
            *data = 4 * gles_limit_or_default(ctx, GL_MAX_FRAGMENT_UNIFORM_VECTORS, 224);
            return;
        case GL_MAX_VARYING_FLOATS:
        case GL_MAX_FRAGMENT_INPUT_COMPONENTS:
            *data = 4 * gles_limit_or_default(ctx, GL_MAX_VARYING_VECTORS, 16);
            return;
        case GL_MAX_GEOMETRY_UNIFORM_BLOCKS:
        case GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS:
            *data = gles_limit_or_default(ctx, pname, 0);
            return;
        case GL_MAX_VERTEX_UNIFORM_BLOCKS:
        case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:
        case GL_MAX_COMBINED_UNIFORM_BLOCKS:
            *data = gles_limit_or_default(ctx, pname, 12);
            return;
        case GL_MAX_UNIFORM_BUFFER_BINDINGS: {
            GLint native_limit = gles_limit_or_default(ctx, pname, 36);
            *data = native_limit < (GLint)MGL_MAX_INDEXED_BUFFER_BINDINGS
                        ? native_limit : (GLint)MGL_MAX_INDEXED_BUFFER_BINDINGS;
            return;
        }
        case GL_MIN_MAP_BUFFER_ALIGNMENT:
            /* mGL maps its CPU shadow, whose alignment is not 64-byte guaranteed. */
            *data = 1;
            return;
        case GL_MAX_TEXTURE_LOD_BIAS: {
            GLfloat actual = 0.0f;
            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetFloatv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &actual);
            *data = (GLint)lroundf(actual);
            return;
        }
        case GL_MAX_TEXTURE_MAX_ANISOTROPY:
            if (ctx->native_dispatch != NULL &&
                mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                       "GL_EXT_texture_filter_anisotropic") &&
                ctx->native_dispatch->glesGetIntegerv != NULL) {
                ctx->native_dispatch->glesGetIntegerv(pname, data);
                return;
            }
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
        case GL_MAX_DRAW_BUFFERS: {
            GLint limit = gles_limit_or_default(ctx, GL_MAX_DRAW_BUFFERS, MGL_MAX_DRAW_BUFFERS);
            *data = limit < (GLint)MGL_MAX_DRAW_BUFFERS ? limit : (GLint)MGL_MAX_DRAW_BUFFERS;
            return;
        }
        case GL_MAX_DUAL_SOURCE_DRAW_BUFFERS:
            *data = (GLint)MGL_MAX_DUAL_SOURCE_DRAW_BUFFERS;
            return;
        case GL_MAX_COLOR_ATTACHMENTS: {
            GLint limit =
                gles_limit_or_default(ctx, GL_MAX_COLOR_ATTACHMENTS, MGL_MAX_DRAW_BUFFERS);
            *data = limit < (GLint)MGL_MAX_DRAW_BUFFERS ? limit : (GLint)MGL_MAX_DRAW_BUFFERS;
            return;
        }
        case GL_MAX_IMAGE_UNITS:
            *data = (GLint)mgl_context_image_unit_limit(ctx);
            return;
        case GL_DRAW_BUFFER: {
            struct mgl_framebuffer *fb = ctx->draw_framebuffer != 0u
                                             ? mgl_framebuffer_find(ctx, ctx->draw_framebuffer)
                                             : NULL;
            *data =
                (GLint)(ctx->draw_framebuffer == 0u ? ctx->default_draw_buffers[0]
                                                    : (fb != NULL ? fb->draw_buffers[0] : GL_NONE));
            return;
        }
        case GL_READ_BUFFER: {
            struct mgl_framebuffer *fb = ctx->read_framebuffer != 0u
                                             ? mgl_framebuffer_find(ctx, ctx->read_framebuffer)
                                             : NULL;
            *data = (GLint)(ctx->read_framebuffer == 0u ? ctx->default_read_buffer
                                                        : (fb != NULL ? fb->read_buffer : GL_NONE));
            return;
        }
        case GL_ARRAY_BUFFER_BINDING:
            *data = (GLint)ctx->array_buffer;
            return;
        case GL_ELEMENT_ARRAY_BUFFER_BINDING:
            *data = (GLint)ctx->current_vao->element_array_buffer;
            return;
        case GL_PIXEL_PACK_BUFFER_BINDING:
            *data = (GLint)ctx->pixel_pack_buffer;
            return;
        case GL_PIXEL_UNPACK_BUFFER_BINDING:
            *data = (GLint)ctx->pixel_unpack_buffer;
            return;
        case GL_COPY_READ_BUFFER_BINDING:
            *data = (GLint)ctx->copy_read_buffer;
            return;
        case GL_COPY_WRITE_BUFFER_BINDING:
            *data = (GLint)ctx->copy_write_buffer;
            return;
        case GL_UNIFORM_BUFFER_BINDING:
            *data = (GLint)ctx->uniform_buffer;
            return;
        case GL_ATOMIC_COUNTER_BUFFER_BINDING:
            *data = (GLint)ctx->atomic_counter_buffer;
            return;
        case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
            *data = (GLint)ctx->transform_feedback_buffer;
            return;
        case GL_DRAW_INDIRECT_BUFFER_BINDING:
            *data = (GLint)ctx->draw_indirect_buffer;
            return;
        case GL_PARAMETER_BUFFER_BINDING:
            *data = (GLint)ctx->parameter_buffer;
            return;
        case GL_QUERY_BUFFER_BINDING:
            *data = (GLint)ctx->query_buffer;
            return;
        case GL_DISPATCH_INDIRECT_BUFFER_BINDING:
            *data = (GLint)ctx->dispatch_indirect_buffer;
            return;
        case GL_SHADER_STORAGE_BUFFER_BINDING:
            *data = (GLint)ctx->shader_storage_buffer;
            return;
        case GL_TEXTURE_BUFFER:
            *data = (GLint)ctx->texture_buffer;
            return;
        case GL_TRANSFORM_FEEDBACK_BINDING:
            *data = (GLint)ctx->transform_feedback_binding;
            return;
        case GL_TRANSFORM_FEEDBACK_ACTIVE:
            *data = ctx->transform_feedback_active ? GL_TRUE : GL_FALSE;
            return;
        case GL_TRANSFORM_FEEDBACK_PAUSED:
            *data = ctx->transform_feedback_paused ? GL_TRUE : GL_FALSE;
            return;
        case GL_VERTEX_ARRAY_BINDING:
            *data = (GLint)ctx->current_vao->name;
            return;
        case GL_DRAW_FRAMEBUFFER_BINDING:
            *data = (GLint)ctx->draw_framebuffer;
            return;
        case GL_READ_FRAMEBUFFER_BINDING:
            *data = (GLint)ctx->read_framebuffer;
            return;
        case GL_RENDERBUFFER_BINDING:
            *data = (GLint)ctx->renderbuffer;
            return;
        case GL_CURRENT_PROGRAM:
            *data = (GLint)ctx->current_program;
            return;
        case GL_PROGRAM_PIPELINE_BINDING:
            *data = (GLint)ctx->current_pipeline;
            return;
        case GL_ACTIVE_TEXTURE:
            *data = (GLint)(GL_TEXTURE0 + ctx->compat.active_texture);
            return;
        case GL_CLIENT_ACTIVE_TEXTURE:
            *data = (GLint)(GL_TEXTURE0 + ctx->client_active_texture);
            return;
        case GL_TEXTURE_BINDING_1D:
            *data = (GLint)ctx->texture_1d_binding[ctx->compat.active_texture];
            return;
        case GL_TEXTURE_BINDING_2D:
            *data = (GLint)ctx->texture_2d_binding[ctx->compat.active_texture];
            return;
        case GL_TEXTURE_BINDING_RECTANGLE:
            *data = (GLint)ctx->texture_rectangle_binding[ctx->compat.active_texture];
            return;
        case GL_TEXTURE_BINDING_3D:
            *data = (GLint)ctx->texture_3d_binding[ctx->compat.active_texture];
            return;
        case GL_TEXTURE_BINDING_2D_ARRAY:
            *data = (GLint)ctx->texture_2d_array_binding[ctx->compat.active_texture];
            return;
        case GL_TEXTURE_BINDING_BUFFER:
            *data = (GLint)ctx->texture_buffer_binding[ctx->compat.active_texture];
            return;
        case GL_MAX_TEXTURE_BUFFER_SIZE:
            *data = (GLint)MGL_MAX_TEXTURE_BUFFER_SIZE;
            return;
        case GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT:
            *data = 1;
            if (ctx->native_dispatch != NULL &&
                ctx->native_dispatch->glesTexBufferRangeEXT != NULL &&
                ctx->native_dispatch->glesGetIntegerv != NULL) {
                GLint native_alignment = 0;
                ctx->native_dispatch->glesGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT,
                                                      &native_alignment);
                if (native_alignment > 0)
                    *data = native_alignment;
            }
            return;
        case GL_TEXTURE_BINDING_CUBE_MAP:
            *data = (GLint)ctx->texture_cube_binding[ctx->compat.active_texture];
            return;
        case GL_TEXTURE_BINDING_CUBE_MAP_ARRAY:
            *data = (GLint)ctx->texture_cube_array_binding[ctx->compat.active_texture];
            return;
        case GL_TEXTURE_BINDING_2D_MULTISAMPLE:
            *data = (GLint)ctx->texture_2d_multisample_binding[ctx->compat.active_texture];
            return;
        case GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY:
            *data = (GLint)ctx->texture_2d_multisample_array_binding[ctx->compat.active_texture];
            return;
        case GL_SAMPLER_BINDING:
            *data = (GLint)ctx->sampler_binding[ctx->compat.active_texture];
            return;
        case GL_UNPACK_SWAP_BYTES:
            *data = ctx->unpack_swap_bytes ? 1 : 0;
            return;
        case GL_UNPACK_LSB_FIRST:
            *data = ctx->unpack_lsb_first ? 1 : 0;
            return;
        case GL_UNPACK_ALIGNMENT:
            *data = ctx->unpack_alignment;
            return;
        case GL_UNPACK_ROW_LENGTH:
            *data = ctx->unpack_row_length;
            return;
        case GL_UNPACK_SKIP_ROWS:
            *data = ctx->unpack_skip_rows;
            return;
        case GL_UNPACK_SKIP_PIXELS:
            *data = ctx->unpack_skip_pixels;
            return;
        case GL_UNPACK_IMAGE_HEIGHT:
            *data = ctx->unpack_image_height;
            return;
        case GL_UNPACK_SKIP_IMAGES:
            *data = ctx->unpack_skip_images;
            return;
        case GL_PACK_SWAP_BYTES:
            *data = ctx->pack_swap_bytes ? 1 : 0;
            return;
        case GL_PACK_LSB_FIRST:
            *data = ctx->pack_lsb_first ? 1 : 0;
            return;
        case GL_PACK_ALIGNMENT:
            *data = ctx->pack_alignment;
            return;
        case GL_PACK_ROW_LENGTH:
            *data = ctx->pack_row_length;
            return;
        case GL_PACK_SKIP_ROWS:
            *data = ctx->pack_skip_rows;
            return;
        case GL_PACK_SKIP_PIXELS:
            *data = ctx->pack_skip_pixels;
            return;
        case GL_PACK_IMAGE_HEIGHT:
            *data = ctx->pack_image_height;
            return;
        case GL_PACK_SKIP_IMAGES:
            *data = ctx->pack_skip_images;
            return;
        case GL_COLOR_MATRIX_STACK_DEPTH:
            *data = (GLint)(ctx->compat.color_matrix_top + 1u);
            return;
        case GL_MATRIX_MODE:
            *data = (GLint)ctx->compat.matrix_mode;
            return;
        case GL_SHADE_MODEL:
            *data = (GLint)ctx->compat.shade_model;
            return;
        case GL_COLOR_MATERIAL_FACE:
            *data = (GLint)ctx->compat.color_material_face;
            return;
        case GL_COLOR_MATERIAL_PARAMETER:
            *data = (GLint)ctx->compat.color_material_mode;
            return;
        case GL_LIGHT_MODEL_LOCAL_VIEWER:
            *data = ctx->compat.light_model_local_viewer ? 1 : 0;
            return;
        case GL_LIGHT_MODEL_TWO_SIDE:
            *data = ctx->compat.light_model_two_side ? 1 : 0;
            return;
        case GL_INDEX_WRITEMASK:
            *data = (GLint)ctx->canonical_state.index_writemask;
            return;
        case GL_CLAMP_VERTEX_COLOR:
            *data = (GLint)ctx->canonical_state.clamp_vertex_color;
            return;
        case GL_CLAMP_FRAGMENT_COLOR:
            *data = (GLint)ctx->canonical_state.clamp_fragment_color;
            return;
        case GL_CLAMP_READ_COLOR:
            *data = (GLint)ctx->canonical_state.clamp_read_color;
            return;
        case GL_ACTIVE_STENCIL_FACE_EXT:
            *data = (GLint)ctx->canonical_state.active_stencil_face;
            return;
        case GL_STENCIL_CLEAR_VALUE:
            *data = ctx->canonical_state.clear_stencil;
            return;
        case GL_STENCIL_FUNC:
            *data = (GLint)stencil_query_front(ctx)->func;
            return;
        case GL_STENCIL_REF:
            *data = stencil_query_front(ctx)->ref;
            return;
        case GL_STENCIL_VALUE_MASK:
            *data = (GLint)stencil_query_front(ctx)->value_mask;
            return;
        case GL_STENCIL_WRITEMASK:
            *data = (GLint)stencil_query_front(ctx)->write_mask;
            return;
        case GL_STENCIL_FAIL:
            *data = (GLint)stencil_query_front(ctx)->sfail;
            return;
        case GL_STENCIL_PASS_DEPTH_FAIL:
            *data = (GLint)stencil_query_front(ctx)->dpfail;
            return;
        case GL_STENCIL_PASS_DEPTH_PASS:
            *data = (GLint)stencil_query_front(ctx)->dppass;
            return;
        case GL_STENCIL_BACK_FUNC:
            *data = (GLint)ctx->canonical_state.stencil_back.func;
            return;
        case GL_STENCIL_BACK_REF:
            *data = ctx->canonical_state.stencil_back.ref;
            return;
        case GL_STENCIL_BACK_VALUE_MASK:
            *data = (GLint)ctx->canonical_state.stencil_back.value_mask;
            return;
        case GL_STENCIL_BACK_WRITEMASK:
            *data = (GLint)ctx->canonical_state.stencil_back.write_mask;
            return;
        case GL_STENCIL_BACK_FAIL:
            *data = (GLint)ctx->canonical_state.stencil_back.sfail;
            return;
        case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
            *data = (GLint)ctx->canonical_state.stencil_back.dpfail;
            return;
        case GL_STENCIL_BACK_PASS_DEPTH_PASS:
            *data = (GLint)ctx->canonical_state.stencil_back.dppass;
            return;
        case GL_MAP_COLOR:
            *data = ctx->pixel_transfer.map_color ? 1 : 0;
            return;
        case GL_MAP_STENCIL:
            *data = ctx->pixel_transfer.map_stencil ? 1 : 0;
            return;
        case GL_INDEX_SHIFT:
            *data = ctx->pixel_transfer.index_shift;
            return;
        case GL_INDEX_OFFSET:
            *data = ctx->pixel_transfer.index_offset;
            return;
        case GL_PIXEL_MAP_I_TO_I_SIZE:
            *data = ctx->pixel_transfer.maps[0].size;
            return;
        case GL_PIXEL_MAP_S_TO_S_SIZE:
            *data = ctx->pixel_transfer.maps[1].size;
            return;
        case GL_PIXEL_MAP_I_TO_R_SIZE:
            *data = ctx->pixel_transfer.maps[2].size;
            return;
        case GL_PIXEL_MAP_I_TO_G_SIZE:
            *data = ctx->pixel_transfer.maps[3].size;
            return;
        case GL_PIXEL_MAP_I_TO_B_SIZE:
            *data = ctx->pixel_transfer.maps[4].size;
            return;
        case GL_PIXEL_MAP_I_TO_A_SIZE:
            *data = ctx->pixel_transfer.maps[5].size;
            return;
        case GL_PIXEL_MAP_R_TO_R_SIZE:
            *data = ctx->pixel_transfer.maps[6].size;
            return;
        case GL_PIXEL_MAP_G_TO_G_SIZE:
            *data = ctx->pixel_transfer.maps[7].size;
            return;
        case GL_PIXEL_MAP_B_TO_B_SIZE:
            *data = ctx->pixel_transfer.maps[8].size;
            return;
        case GL_PIXEL_MAP_A_TO_A_SIZE:
            *data = ctx->pixel_transfer.maps[9].size;
            return;
        case GL_MAX_PIXEL_MAP_TABLE:
            *data = (GLint)MGL_PIXEL_MAP_MAX_SIZE;
            return;
        case GL_LIST_BASE:
            *data = (GLint)ctx->list_base;
            return;
        case GL_ATTRIB_STACK_DEPTH:
            *data = (GLint)ctx->attrib_stack_depth;
            return;
        case GL_CLIENT_ATTRIB_STACK_DEPTH:
            *data = (GLint)ctx->client_attrib_stack_depth;
            return;
        case GL_EDGE_FLAG_ARRAY:
            *data = ctx->current_vao->edge_flag.enabled ? 1 : 0;
            return;
        case GL_INDEX_ARRAY:
            *data = ctx->current_vao->color_index.enabled ? 1 : 0;
            return;
        case GL_INDEX_ARRAY_TYPE:
            *data = (GLint)ctx->current_vao->color_index.type;
            return;
        case GL_INDEX_ARRAY_STRIDE:
            *data = ctx->current_vao->color_index.stride;
            return;
        case GL_INDEX_ARRAY_BUFFER_BINDING:
            *data = (GLint)(ctx->current_vao->color_index.data.source == MGL_DATA_BUFFER
                                ? ctx->current_vao->color_index.data.buffer_name
                                : 0u);
            return;
        case GL_EDGE_FLAG_ARRAY_STRIDE:
            *data = ctx->current_vao->edge_flag.stride;
            return;
        case GL_EDGE_FLAG_ARRAY_BUFFER_BINDING:
            *data = (GLint)(ctx->current_vao->edge_flag.data.source == MGL_DATA_BUFFER
                                ? ctx->current_vao->edge_flag.data.buffer_name
                                : 0u);
            return;
        case GL_ARRAY_ELEMENT_LOCK_FIRST_EXT:
            *data = ctx->array_lock_first;
            return;
        case GL_ARRAY_ELEMENT_LOCK_COUNT_EXT:
            *data = ctx->array_lock_count;
            return;
        case GL_MAX_ATTRIB_STACK_DEPTH:
            *data = (GLint)MGL_ATTRIB_STACK_MAX_DEPTH;
            return;
        case GL_MAX_CLIENT_ATTRIB_STACK_DEPTH:
            *data = (GLint)MGL_ATTRIB_STACK_MAX_DEPTH;
            return;
        case GL_RENDER_MODE:
            *data =
                (GLint)(ctx->selection.render_mode != 0u ? ctx->selection.render_mode : GL_RENDER);
            return;
        case GL_NAME_STACK_DEPTH:
            *data = (GLint)ctx->selection.name_depth;
            return;
        case GL_MAX_NAME_STACK_DEPTH:
            *data = (GLint)MGL_NAME_STACK_MAX_DEPTH;
            return;
        case GL_MAX_CLIP_PLANES:
            *data = (GLint)MGL_MAX_CLIP_PLANES;
            return;
        case GL_MAX_LIGHTS:
            *data = (GLint)MGL_MAX_LIGHTS;
            return;
        case GL_MAX_EVAL_ORDER:
            *data = (GLint)MGL_MAX_EVAL_ORDER;
            return;
        case GL_PROGRAM_ERROR_POSITION_ARB:
            *data = ctx->arb_program_error_position;
            return;
        case GL_NUM_SHADER_BINARY_FORMATS:
        case GL_SHADER_BINARY_FORMATS:
            if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegerv != NULL) {
                GLint native_count = 0;
                ctx->native_dispatch->glesGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &native_count);
                if (native_count < 0)
                    native_count = 0;
                if (pname == GL_NUM_SHADER_BINARY_FORMATS) {
                    *data = native_count + 1;
                    return;
                }
                if (native_count > 0)
                    ctx->native_dispatch->glesGetIntegerv(GL_SHADER_BINARY_FORMATS, data);
                data[native_count] = (GLint)GL_SHADER_BINARY_FORMAT_SPIR_V;
                return;
            }
            if (pname == GL_NUM_SHADER_BINARY_FORMATS)
                *data = 1;
            else
                *data = (GLint)GL_SHADER_BINARY_FORMAT_SPIR_V;
            return;
        case GL_NUM_PROGRAM_BINARY_FORMATS:
        case GL_PROGRAM_BINARY_FORMATS:
            if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegerv != NULL) {
                ctx->native_dispatch->glesGetIntegerv(pname, data);
                return;
            }
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        case GL_MAX_PROGRAM_ENV_PARAMETERS_ARB:
            *data = (GLint)MGL_ARB_MAX_PROGRAM_PARAMETERS;
            return;
        case GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB:
            *data = (GLint)MGL_ARB_MAX_PROGRAM_PARAMETERS;
            return;
        case GL_MAP1_GRID_SEGMENTS:
            *data = ctx->evaluator.grid1_segments;
            return;
        case GL_MAP2_GRID_SEGMENTS:
            data[0] = ctx->evaluator.grid2_u_segments;
            data[1] = ctx->evaluator.grid2_v_segments;
            return;
        default:
            if (pname >= GL_DRAW_BUFFER0 && pname < GL_DRAW_BUFFER0 + MGL_MAX_DRAW_BUFFERS) {
                const GLuint draw_index = (GLuint)(pname - GL_DRAW_BUFFER0);
                struct mgl_framebuffer *fb = ctx->draw_framebuffer != 0u
                                                 ? mgl_framebuffer_find(ctx, ctx->draw_framebuffer)
                                                 : NULL;
                *data = (GLint)(ctx->draw_framebuffer == 0u
                                    ? ctx->default_draw_buffers[draw_index]
                                    : (fb != NULL ? fb->draw_buffers[draw_index] : GL_NONE));
                return;
            }
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
    }
}

void glGetFloatv(GLenum pname, GLfloat *data) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_GET_STATE);
    GLint local[16] = {0};
    GLint *values = local;
    int count;
    int i;
    if (ctx == NULL || data == NULL)
        return;
    if (reject_inside_begin(ctx))
        return;
    if (mgl_scalar_64bit_state(pname)) {
        GLint64 value = 0;
        uint64_t sequence = ctx->errors.sequence;
        glGetInteger64v(pname, &value);
        if (ctx->errors.sequence == sequence)
            *data = (GLfloat)value;
        return;
    }
    if (pname == GL_MAX_TEXTURE_MAX_ANISOTROPY && ctx->native_dispatch != NULL &&
        mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                               "GL_EXT_texture_filter_anisotropic") &&
        ctx->native_dispatch->glesGetFloatv != NULL) {
        ctx->native_dispatch->glesGetFloatv(pname, data);
        return;
    }
    switch (pname) {
        case GL_PRIMITIVE_BOUNDING_BOX:
            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetFloatv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetFloatv(pname, data);
            return;
        case GL_INDEX_CLEAR_VALUE:
            *data = ctx->canonical_state.clear_index;
            return;
        case GL_COLOR_MATRIX:
            memcpy(data, ctx->compat.color_matrix[ctx->compat.color_matrix_top],
                   16u * sizeof(GLfloat));
            return;
        case GL_COLOR_CLEAR_VALUE:
            for (i = 0; i < 4; ++i)
                data[i] = ctx->canonical_state.clear_color[i];
            return;
        case GL_ACCUM_CLEAR_VALUE:
            memcpy(data, ctx->canonical_state.clear_accum,
                   sizeof(ctx->canonical_state.clear_accum));
            return;
        case GL_DEPTH_CLEAR_VALUE:
            *data = ctx->canonical_state.clear_depth;
            return;
        case GL_DEPTH_RANGE:
            data[0] = ctx->canonical_state.depth_range[0];
            data[1] = ctx->canonical_state.depth_range[1];
            return;
        case GL_VIEWPORT:
            memcpy(data, ctx->canonical_state.indexed_viewports[0],
                   sizeof(ctx->canonical_state.indexed_viewports[0]));
            return;
        case GL_DEPTH_BOUNDS_EXT:
            data[0] = (GLfloat)ctx->canonical_state.depth_bounds[0];
            data[1] = (GLfloat)ctx->canonical_state.depth_bounds[1];
            return;
        case GL_BLEND_COLOR:
            for (i = 0; i < 4; ++i)
                data[i] = ctx->canonical_state.blend_color[i];
            return;
        case GL_LINE_WIDTH:
            *data = ctx->canonical_state.line_width;
            return;
        case GL_LINE_STIPPLE_PATTERN:
            *data = (GLfloat)ctx->canonical_state.line_stipple_pattern;
            return;
        case GL_LINE_STIPPLE_REPEAT:
            *data = (GLfloat)ctx->canonical_state.line_stipple_factor;
            return;
        case GL_EDGE_FLAG:
            *data = ctx->current_edge_flag ? 1.0f : 0.0f;
            return;
        case GL_POINT_SIZE_RANGE:
        case GL_ALIASED_POINT_SIZE_RANGE:
        case GL_MAX_VIEWPORT_DIMS:
            count = 2;
            break;
        case GL_MIN_FRAGMENT_INTERPOLATION_OFFSET:
        case GL_MAX_FRAGMENT_INTERPOLATION_OFFSET:
        case GL_ALIASED_LINE_WIDTH_RANGE:
        case GL_MULTISAMPLE_LINE_WIDTH_RANGE:
        case GL_MULTISAMPLE_LINE_WIDTH_GRANULARITY:
            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetFloatv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetFloatv(pname, data);
            return;
        case GL_MAX_TEXTURE_LOD_BIAS:
            if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetFloatv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetFloatv(GL_MAX_TEXTURE_LOD_BIAS, data);
            return;
        case GL_POINT_SIZE_GRANULARITY:
            *data = 1.0f;
            return;
        case GL_POINT_SIZE_MIN:
            *data = ctx->canonical_state.point_size_min;
            return;
        case GL_POINT_SIZE_MAX:
            *data = ctx->canonical_state.point_size_max;
            return;
        case GL_POINT_FADE_THRESHOLD_SIZE:
            *data = ctx->canonical_state.point_fade_threshold_size;
            return;
        case GL_POINT_DISTANCE_ATTENUATION:
            memcpy(data, ctx->canonical_state.point_distance_attenuation, 3u * sizeof(GLfloat));
            return;
        case GL_LOGIC_OP_MODE:
            *data = (GLfloat)ctx->canonical_state.logic_op_mode;
            return;
        case GL_POINT_SIZE:
            *data = ctx->canonical_state.point_size;
            return;
        case GL_POLYGON_MODE:
            data[0] = (GLfloat)ctx->canonical_state.polygon_mode[0];
            data[1] = (GLfloat)ctx->canonical_state.polygon_mode[1];
            return;
        case GL_POLYGON_OFFSET_FACTOR:
            *data = ctx->canonical_state.polygon_offset_factor;
            return;
        case GL_POLYGON_OFFSET_UNITS:
            *data = ctx->canonical_state.polygon_offset_units;
            return;
        case GL_POLYGON_OFFSET_CLAMP:
            *data = ctx->canonical_state.polygon_offset_clamp;
            return;
        case GL_MIN_SAMPLE_SHADING_VALUE:
            *data = ctx->canonical_state.min_sample_shading;
            return;
        case GL_PATCH_DEFAULT_INNER_LEVEL:
            memcpy(data, ctx->canonical_state.patch_inner_levels,
                   sizeof(ctx->canonical_state.patch_inner_levels));
            return;
        case GL_PATCH_DEFAULT_OUTER_LEVEL:
            memcpy(data, ctx->canonical_state.patch_outer_levels,
                   sizeof(ctx->canonical_state.patch_outer_levels));
            return;
        case GL_SAMPLE_COVERAGE_VALUE:
            *data = ctx->canonical_state.sample_coverage_value;
            return;
        case GL_ALPHA_TEST_REF:
            *data = ctx->canonical_state.alpha_ref;
            return;
        case GL_FOG_DENSITY:
            *data = ctx->compat.fog_density;
            return;
        case GL_FOG_START:
            *data = ctx->compat.fog_start;
            return;
        case GL_FOG_END:
            *data = ctx->compat.fog_end;
            return;
        case GL_FOG_INDEX:
            *data = ctx->compat.fog_index;
            return;
        case GL_FOG_COLOR:
            for (i = 0; i < 4; ++i)
                data[i] = ctx->compat.fog_color[i];
            return;
        case GL_LIGHT_MODEL_AMBIENT:
            for (i = 0; i < 4; ++i)
                data[i] = ctx->compat.light_model_ambient[i];
            return;
        case GL_RED_SCALE:
            *data = ctx->pixel_transfer.red_scale;
            return;
        case GL_RED_BIAS:
            *data = ctx->pixel_transfer.red_bias;
            return;
        case GL_GREEN_SCALE:
            *data = ctx->pixel_transfer.green_scale;
            return;
        case GL_GREEN_BIAS:
            *data = ctx->pixel_transfer.green_bias;
            return;
        case GL_BLUE_SCALE:
            *data = ctx->pixel_transfer.blue_scale;
            return;
        case GL_BLUE_BIAS:
            *data = ctx->pixel_transfer.blue_bias;
            return;
        case GL_ALPHA_SCALE:
            *data = ctx->pixel_transfer.alpha_scale;
            return;
        case GL_ALPHA_BIAS:
            *data = ctx->pixel_transfer.alpha_bias;
            return;
        case GL_DEPTH_SCALE:
            *data = ctx->pixel_transfer.depth_scale;
            return;
        case GL_DEPTH_BIAS:
            *data = ctx->pixel_transfer.depth_bias;
            return;
        case GL_MODELVIEW_MATRIX:
            for (i = 0; i < 16; ++i)
                data[i] = mgl_compat_modelview(&ctx->compat)[i];
            return;
        case GL_PROJECTION_MATRIX:
            for (i = 0; i < 16; ++i)
                data[i] = mgl_compat_projection(&ctx->compat)[i];
            return;
        case GL_TEXTURE_MATRIX: {
            const GLfloat *m = mgl_compat_texture_matrix(&ctx->compat, ctx->compat.active_texture);
            if (m == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            for (i = 0; i < 16; ++i)
                data[i] = m[i];
            return;
        }
        case GL_CURRENT_COLOR:
            for (i = 0; i < 4; ++i)
                data[i] = ctx->compat_current[MGL_COMPAT_SEM_COLOR][i];
            return;
        case GL_CURRENT_SECONDARY_COLOR:
            for (i = 0; i < 4; ++i)
                data[i] = ctx->compat_current[MGL_COMPAT_SEM_SECONDARY][i];
            return;
        case GL_CURRENT_NORMAL:
            for (i = 0; i < 3; ++i)
                data[i] = ctx->compat_current[MGL_COMPAT_SEM_NORMAL][i];
            return;
        case GL_CURRENT_FOG_COORD:
            *data = ctx->compat_current[MGL_COMPAT_SEM_FOG][0];
            return;
        case GL_CURRENT_INDEX:
            *data = ctx->evaluator.current_index;
            return;
        case GL_CURRENT_TEXTURE_COORDS: {
            unsigned sem = MGL_COMPAT_SEM_TEXCOORD0 + ctx->compat.active_texture;
            if (ctx->compat.active_texture >= MGL_TEXTURE_UNITS) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            for (i = 0; i < 4; ++i)
                data[i] = ctx->compat_current[sem][i];
            return;
        }
        case GL_CURRENT_RASTER_COLOR:
            for (i = 0; i < 4; ++i)
                data[i] = ctx->compat_raster.color[i];
            return;
        case GL_CURRENT_RASTER_SECONDARY_COLOR:
            for (i = 0; i < 4; ++i)
                data[i] = ctx->compat_raster.secondary[i];
            return;
        case GL_CURRENT_RASTER_POSITION:
            for (i = 0; i < 4; ++i)
                data[i] = ctx->compat_raster.position[i];
            return;
        case GL_CURRENT_RASTER_TEXTURE_COORDS:
            for (i = 0; i < 4; ++i)
                data[i] = ctx->compat_raster.texcoord[i];
            return;
        case GL_ZOOM_X:
            *data = ctx->pixel_transfer.pixel_zoom_x;
            return;
        case GL_ZOOM_Y:
            *data = ctx->pixel_transfer.pixel_zoom_y;
            return;
        case GL_MAP1_GRID_DOMAIN:
            data[0] = (GLfloat)ctx->evaluator.grid1_u1;
            data[1] = (GLfloat)ctx->evaluator.grid1_u2;
            return;
        case GL_MAP2_GRID_DOMAIN:
            data[0] = (GLfloat)ctx->evaluator.grid2_u1;
            data[1] = (GLfloat)ctx->evaluator.grid2_u2;
            data[2] = (GLfloat)ctx->evaluator.grid2_v1;
            data[3] = (GLfloat)ctx->evaluator.grid2_v2;
            return;
        default:
            break;
    }
    count = mgl_query_component_count_for_context(ctx, pname);
    if (count > (int)(sizeof(local) / sizeof(local[0]))) {
        values = malloc((size_t)count * sizeof(*values));
        if (values == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
    }
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetIntegerv(pname, values);
    if (ctx->errors.sequence == error_sequence_before)
        for (i = 0; i < count; ++i)
            data[i] = (GLfloat)values[i];
    if (values != local)
        free(values);
}

void glGetDoublev(GLenum pname, GLdouble *data) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat local[16] = {0.0f};
    GLfloat *values = local;
    uint64_t sequence;
    int count;
    int i;
    if (ctx == NULL || data == NULL || reject_inside_begin(ctx))
        return;
    if (mgl_scalar_64bit_state(pname)) {
        GLint64 value = 0;
        sequence = ctx->errors.sequence;
        glGetInteger64v(pname, &value);
        if (ctx->errors.sequence == sequence)
            *data = (GLdouble)value;
        return;
    }
    if (pname == GL_DEPTH_BOUNDS_EXT) {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_GET_STATE);
        data[0] = ctx->canonical_state.depth_bounds[0];
        data[1] = ctx->canonical_state.depth_bounds[1];
        return;
    }
    count = mgl_query_component_count_for_context(ctx, pname);
    if (count > (int)(sizeof(local) / sizeof(local[0]))) {
        values = malloc((size_t)count * sizeof(*values));
        if (values == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
    }
    sequence = ctx->errors.sequence;
    glGetFloatv(pname, values);
    if (ctx->errors.sequence == sequence)
        for (i = 0; i < count; ++i)
            data[i] = (GLdouble)values[i];
    if (values != local)
        free(values);
}

static void mgl_native_dispatch_restore_cap(struct mgl_context *ctx, GLenum cap,
                                            GLboolean enabled) {
    if (ctx->native_dispatch == NULL || cap == GL_ALPHA_TEST || cap == GL_FOG ||
        cap == GL_TEXTURE_2D || cap == GL_TEXTURE_3D || cap == GL_TEXTURE_CUBE_MAP ||
        cap == GL_COLOR_SUM || cap == GL_LINE_SMOOTH || cap == GL_LINE_STIPPLE ||
        cap == GL_POLYGON_STIPPLE)
        return;
    if (cap == GL_COLOR_LOGIC_OP &&
        (ctx->native_dispatch->glesLogicOpANGLE == NULL ||
         !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_ANGLE_logic_op")))
        return;
    if (cap == GL_FRAMEBUFFER_SRGB &&
        !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_EXT_sRGB_write_control"))
        return;
    if (enabled) {
        if (ctx->native_dispatch->glesEnable != NULL)
            ctx->native_dispatch->glesEnable(cap);
    } else if (ctx->native_dispatch->glesDisable != NULL) {
        ctx->native_dispatch->glesDisable(cap);
    }
}

static void mgl_realize_restored_server_state(struct mgl_context *ctx, GLbitfield mask) {
    if (ctx->native_dispatch == NULL)
        return;
    if ((mask & GL_TEXTURE_BIT) != 0u)
        mgl_texture_realize_bindings(ctx);
    if ((mask & (GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT |
                 GL_POLYGON_BIT | GL_SCISSOR_BIT | GL_MULTISAMPLE_BIT)) != 0u) {
        mgl_native_dispatch_restore_cap(ctx, GL_DEPTH_TEST, ctx->canonical_state.depth_test);
        mgl_native_dispatch_restore_cap(ctx, GL_STENCIL_TEST, ctx->canonical_state.stencil_test);
        mgl_native_dispatch_restore_cap(ctx, GL_BLEND, ctx->canonical_state.blend);
        mgl_native_dispatch_restore_cap(ctx, GL_COLOR_LOGIC_OP,
                                        ctx->canonical_state.color_logic_op);
        mgl_native_dispatch_restore_cap(ctx, GL_CULL_FACE, ctx->canonical_state.cull_face);
        mgl_native_dispatch_restore_cap(ctx, GL_SCISSOR_TEST, ctx->canonical_state.scissor_test);
        mgl_native_dispatch_restore_cap(ctx, GL_DITHER, ctx->canonical_state.dither);
        mgl_native_dispatch_restore_cap(ctx, GL_POLYGON_OFFSET_FILL,
                                        ctx->canonical_state.polygon_offset_fill);
        mgl_native_dispatch_restore_cap(ctx, GL_SAMPLE_ALPHA_TO_COVERAGE,
                                        ctx->canonical_state.sample_alpha_to_coverage);
        mgl_native_dispatch_restore_cap(ctx, GL_SAMPLE_COVERAGE,
                                        ctx->canonical_state.sample_coverage);
        if (ctx->native_dispatch->glesMinSampleShading != NULL)
            mgl_native_dispatch_restore_cap(ctx, GL_SAMPLE_SHADING,
                                            ctx->canonical_state.sample_shading);
        mgl_native_dispatch_restore_cap(ctx, GL_FRAMEBUFFER_SRGB,
                                        ctx->canonical_state.framebuffer_srgb);
    }
    if ((mask & GL_VIEWPORT_BIT) != 0u && ctx->native_dispatch->glesViewportArrayvOES != NULL &&
        ctx->native_dispatch->glesDepthRangeArrayfvOES != NULL &&
        mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_OES_viewport_array")) {
        for (GLuint index = 0; index < viewport_array_limit(ctx); ++index) {
            GLfloat depth[2] = {(GLfloat)ctx->canonical_state.indexed_depth_ranges[index][0],
                                (GLfloat)ctx->canonical_state.indexed_depth_ranges[index][1]};
            ctx->native_dispatch->glesViewportArrayvOES(
                index, 1, ctx->canonical_state.indexed_viewports[index]);
            ctx->native_dispatch->glesDepthRangeArrayfvOES(index, 1, depth);
        }
    } else if ((mask & GL_VIEWPORT_BIT) != 0u) {
        if (ctx->native_dispatch->glesViewport != NULL)
            ctx->native_dispatch->glesViewport(
                ctx->canonical_state.viewport[0], ctx->canonical_state.viewport[1],
                ctx->canonical_state.viewport[2], ctx->canonical_state.viewport[3]);
        if (ctx->native_dispatch->glesDepthRangef != NULL)
            ctx->native_dispatch->glesDepthRangef(ctx->canonical_state.depth_range[0],
                                                  ctx->canonical_state.depth_range[1]);
    }
    if ((mask & GL_SCISSOR_BIT) != 0u) {
        if (ctx->native_dispatch->glesScissorArrayvOES != NULL &&
            mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_OES_viewport_array")) {
            ctx->native_dispatch->glesScissorArrayvOES(
                0, (GLsizei)viewport_array_limit(ctx),
                &ctx->canonical_state.indexed_scissors[0][0]);
        } else if (ctx->native_dispatch->glesScissor != NULL)
            ctx->native_dispatch->glesScissor(
                ctx->canonical_state.scissor[0], ctx->canonical_state.scissor[1],
                ctx->canonical_state.scissor[2], ctx->canonical_state.scissor[3]);
    }
    if ((mask & GL_DEPTH_BUFFER_BIT) != 0u) {
        if (ctx->native_dispatch->glesClearDepthf != NULL)
            ctx->native_dispatch->glesClearDepthf(ctx->canonical_state.clear_depth);
        if (ctx->native_dispatch->glesDepthFunc != NULL)
            ctx->native_dispatch->glesDepthFunc(ctx->canonical_state.depth_func);
        if (ctx->native_dispatch->glesDepthMask != NULL)
            ctx->native_dispatch->glesDepthMask(ctx->canonical_state.depth_mask);
    }
    if ((mask & (GL_STENCIL_BUFFER_BIT | GL_ENABLE_BIT)) != 0u) {
        if (ctx->native_dispatch->glesClearStencil != NULL)
            ctx->native_dispatch->glesClearStencil(ctx->canonical_state.clear_stencil);
        if (ctx->native_dispatch->glesStencilFuncSeparate != NULL) {
            ctx->native_dispatch->glesStencilFuncSeparate(
                GL_FRONT, ctx->canonical_state.stencil_front.func,
                ctx->canonical_state.stencil_front.ref,
                ctx->canonical_state.stencil_front.value_mask);
            ctx->native_dispatch->glesStencilFuncSeparate(
                GL_BACK,
                (ctx->canonical_state.stencil_two_side_test ? ctx->canonical_state.stencil_back_ext
                                                            : ctx->canonical_state.stencil_back)
                    .func,
                (ctx->canonical_state.stencil_two_side_test ? ctx->canonical_state.stencil_back_ext
                                                            : ctx->canonical_state.stencil_back)
                    .ref,
                (ctx->canonical_state.stencil_two_side_test ? ctx->canonical_state.stencil_back_ext
                                                            : ctx->canonical_state.stencil_back)
                    .value_mask);
        }
        if (ctx->native_dispatch->glesStencilMaskSeparate != NULL) {
            ctx->native_dispatch->glesStencilMaskSeparate(
                GL_FRONT, ctx->canonical_state.stencil_front.write_mask);
            ctx->native_dispatch->glesStencilMaskSeparate(
                GL_BACK,
                (ctx->canonical_state.stencil_two_side_test ? ctx->canonical_state.stencil_back_ext
                                                            : ctx->canonical_state.stencil_back)
                    .write_mask);
        }
        if (ctx->native_dispatch->glesStencilOpSeparate != NULL) {
            ctx->native_dispatch->glesStencilOpSeparate(GL_FRONT,
                                                        ctx->canonical_state.stencil_front.sfail,
                                                        ctx->canonical_state.stencil_front.dpfail,
                                                        ctx->canonical_state.stencil_front.dppass);
            ctx->native_dispatch->glesStencilOpSeparate(
                GL_BACK,
                (ctx->canonical_state.stencil_two_side_test ? ctx->canonical_state.stencil_back_ext
                                                            : ctx->canonical_state.stencil_back)
                    .sfail,
                (ctx->canonical_state.stencil_two_side_test ? ctx->canonical_state.stencil_back_ext
                                                            : ctx->canonical_state.stencil_back)
                    .dpfail,
                (ctx->canonical_state.stencil_two_side_test ? ctx->canonical_state.stencil_back_ext
                                                            : ctx->canonical_state.stencil_back)
                    .dppass);
        }
    }
    if ((mask & (GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT)) != 0u &&
        ctx->native_dispatch->glesLogicOpANGLE != NULL &&
        mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_ANGLE_logic_op"))
        ctx->native_dispatch->glesLogicOpANGLE(ctx->canonical_state.logic_op_mode);
    if ((mask & GL_COLOR_BUFFER_BIT) != 0u) {
        if (ctx->native_dispatch->glesClearColor != NULL)
            ctx->native_dispatch->glesClearColor(
                ctx->canonical_state.clear_color[0], ctx->canonical_state.clear_color[1],
                ctx->canonical_state.clear_color[2], ctx->canonical_state.clear_color[3]);
        if (ctx->native_dispatch->glesColorMask != NULL)
            ctx->native_dispatch->glesColorMask(
                ctx->canonical_state.color_mask[0], ctx->canonical_state.color_mask[1],
                ctx->canonical_state.color_mask[2], ctx->canonical_state.color_mask[3]);
        lower_blend_func(ctx, ctx->canonical_state.blend_src_rgb,
                         ctx->canonical_state.blend_dst_rgb, ctx->canonical_state.blend_src_alpha,
                         ctx->canonical_state.blend_dst_alpha);
        if (ctx->native_dispatch->glesBlendColor != NULL)
            ctx->native_dispatch->glesBlendColor(
                ctx->canonical_state.blend_color[0], ctx->canonical_state.blend_color[1],
                ctx->canonical_state.blend_color[2], ctx->canonical_state.blend_color[3]);
        lower_blend_equation(ctx, 0u, ctx->canonical_state.blend_equation_rgb,
                             ctx->canonical_state.blend_equation_alpha, 0);
        {
            unsigned i;
            for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i) {
                const struct mgl_draw_buffer_state *db = &ctx->canonical_state.draw_buffers[i];
                if (ctx->native_dispatch->glesColorMaski != NULL)
                    ctx->native_dispatch->glesColorMaski(i, db->color_mask[0], db->color_mask[1],
                                                         db->color_mask[2], db->color_mask[3]);
                lower_blend_func_indexed(ctx, i, db->blend_src_rgb, db->blend_dst_rgb,
                                         db->blend_src_alpha, db->blend_dst_alpha);
                lower_blend_equation(ctx, i, db->blend_equation_rgb, db->blend_equation_alpha, 1);
                if (db->blend) {
                    if (ctx->native_dispatch->glesEnablei != NULL)
                        ctx->native_dispatch->glesEnablei(GL_BLEND, i);
                } else {
                    if (ctx->native_dispatch->glesDisablei != NULL)
                        ctx->native_dispatch->glesDisablei(GL_BLEND, i);
                }
            }
        }
    }
    if ((mask & GL_POLYGON_BIT) != 0u) {
        if (ctx->native_dispatch->glesCullFace != NULL)
            ctx->native_dispatch->glesCullFace(ctx->canonical_state.cull_face_mode);
        if (ctx->native_dispatch->glesPolygonModeNV != NULL &&
            mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_NV_polygon_mode") &&
            ctx->canonical_state.polygon_mode[0] == ctx->canonical_state.polygon_mode[1])
            ctx->native_dispatch->glesPolygonModeNV(GL_FRONT_AND_BACK,
                                                    ctx->canonical_state.polygon_mode[0]);
        if (ctx->native_dispatch->glesFrontFace != NULL)
            ctx->native_dispatch->glesFrontFace(ctx->canonical_state.front_face);
        if (ctx->native_dispatch->glesPolygonOffsetClamp != NULL)
            ctx->native_dispatch->glesPolygonOffsetClamp(ctx->canonical_state.polygon_offset_factor,
                                                         ctx->canonical_state.polygon_offset_units,
                                                         ctx->canonical_state.polygon_offset_clamp);
        else if (ctx->native_dispatch->glesPolygonOffset != NULL)
            ctx->native_dispatch->glesPolygonOffset(ctx->canonical_state.polygon_offset_factor,
                                                    ctx->canonical_state.polygon_offset_units);
    }
    if ((mask & GL_LINE_BIT) != 0u && ctx->native_dispatch->glesLineWidth != NULL)
        ctx->native_dispatch->glesLineWidth(ctx->canonical_state.line_width);
    if ((mask & GL_MULTISAMPLE_BIT) != 0u) {
        if (ctx->native_dispatch->glesSampleCoverage != NULL)
            ctx->native_dispatch->glesSampleCoverage(ctx->canonical_state.sample_coverage_value,
                                                     ctx->canonical_state.sample_coverage_invert);
        if (ctx->native_dispatch->glesMinSampleShading != NULL)
            ctx->native_dispatch->glesMinSampleShading(ctx->canonical_state.min_sample_shading);
    }
}

static void mgl_capture_attrib_state(const struct mgl_context *ctx,
                                     struct mgl_attrib_snapshot *snapshot, GLbitfield mask) {
    snapshot->mask = mask;
    snapshot->canonical_state = ctx->canonical_state;
    snapshot->compat = ctx->compat;
    snapshot->pixel_transfer = ctx->pixel_transfer;
    snapshot->compat_raster = ctx->compat_raster;
    snapshot->list_base = ctx->list_base;
    memcpy(snapshot->compat_current, ctx->compat_current, sizeof(snapshot->compat_current));
    snapshot->current_edge_flag = ctx->current_edge_flag;
    memcpy(snapshot->texture_1d_binding, ctx->texture_1d_binding,
           sizeof(snapshot->texture_1d_binding));
    memcpy(snapshot->texture_2d_binding, ctx->texture_2d_binding,
           sizeof(snapshot->texture_2d_binding));
    memcpy(snapshot->texture_rectangle_binding, ctx->texture_rectangle_binding,
           sizeof(snapshot->texture_rectangle_binding));
    memcpy(snapshot->texture_2d_prefer_rectangle, ctx->texture_2d_prefer_rectangle,
           sizeof(snapshot->texture_2d_prefer_rectangle));
    memcpy(snapshot->texture_2d_prefer_1d, ctx->texture_2d_prefer_1d,
           sizeof(snapshot->texture_2d_prefer_1d));
    memcpy(snapshot->texture_3d_binding, ctx->texture_3d_binding,
           sizeof(snapshot->texture_3d_binding));
    memcpy(snapshot->texture_2d_array_binding, ctx->texture_2d_array_binding,
           sizeof(snapshot->texture_2d_array_binding));
    memcpy(snapshot->texture_buffer_binding, ctx->texture_buffer_binding,
           sizeof(snapshot->texture_buffer_binding));
    memcpy(snapshot->texture_cube_binding, ctx->texture_cube_binding,
           sizeof(snapshot->texture_cube_binding));
    memcpy(snapshot->texture_cube_array_binding, ctx->texture_cube_array_binding,
           sizeof(snapshot->texture_cube_array_binding));
    memcpy(snapshot->texture_2d_multisample_binding, ctx->texture_2d_multisample_binding,
           sizeof(snapshot->texture_2d_multisample_binding));
    memcpy(snapshot->texture_2d_multisample_array_binding,
           ctx->texture_2d_multisample_array_binding,
           sizeof(snapshot->texture_2d_multisample_array_binding));
}

void glPushAttrib(GLbitfield mask) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_attrib_snapshot *snapshot;
    const GLbitfield allowed = GL_ALL_ATTRIB_BITS | GL_MULTISAMPLE_BIT;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_push_attrib(ctx, mask))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((mask & ~allowed) != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_push_attrib(ctx, mask))
        return;
    if (ctx->attrib_stack_depth >= MGL_ATTRIB_STACK_MAX_DEPTH) {
        mgl_error_set(&ctx->errors, GL_STACK_OVERFLOW);
        return;
    }
    snapshot = &ctx->attrib_stack[ctx->attrib_stack_depth++];
    mgl_capture_attrib_state(ctx, snapshot, mask);
}

static void mgl_apply_attrib_state(struct mgl_context *ctx,
                                   const struct mgl_attrib_snapshot *snapshot, GLbitfield mask,
                                   int realize) {
    unsigned i;
    if ((mask & GL_ACCUM_BUFFER_BIT) != 0u)
        memcpy(ctx->canonical_state.clear_accum, snapshot->canonical_state.clear_accum,
               sizeof(ctx->canonical_state.clear_accum));
    if ((mask & GL_CURRENT_BIT) != 0u) {
        memcpy(ctx->compat_current, snapshot->compat_current, sizeof(ctx->compat_current));
        ctx->current_edge_flag = snapshot->current_edge_flag;
        ctx->compat_raster = snapshot->compat_raster;
    }
    if ((mask & GL_LIST_BIT) != 0u)
        ctx->list_base = snapshot->list_base;
    if ((mask & GL_PIXEL_MODE_BIT) != 0u)
        ctx->pixel_transfer = snapshot->pixel_transfer;
    if ((mask & GL_VIEWPORT_BIT) != 0u) {
        memcpy(ctx->canonical_state.viewport, snapshot->canonical_state.viewport,
               sizeof(ctx->canonical_state.viewport));
        memcpy(ctx->canonical_state.depth_range, snapshot->canonical_state.depth_range,
               sizeof(ctx->canonical_state.depth_range));
        memcpy(ctx->canonical_state.indexed_viewports, snapshot->canonical_state.indexed_viewports,
               sizeof(ctx->canonical_state.indexed_viewports));
        memcpy(ctx->canonical_state.indexed_depth_ranges,
               snapshot->canonical_state.indexed_depth_ranges,
               sizeof(ctx->canonical_state.indexed_depth_ranges));
    }
    if ((mask & GL_SCISSOR_BIT) != 0u) {
        memcpy(ctx->canonical_state.scissor, snapshot->canonical_state.scissor,
               sizeof(ctx->canonical_state.scissor));
        memcpy(ctx->canonical_state.indexed_scissors, snapshot->canonical_state.indexed_scissors,
               sizeof(ctx->canonical_state.indexed_scissors));
        ctx->canonical_state.scissor_test = snapshot->canonical_state.scissor_test;
    }
    if ((mask & GL_DEPTH_BUFFER_BIT) != 0u) {
        ctx->canonical_state.clear_depth = snapshot->canonical_state.clear_depth;
        ctx->canonical_state.depth_test = snapshot->canonical_state.depth_test;
        ctx->canonical_state.depth_func = snapshot->canonical_state.depth_func;
        ctx->canonical_state.depth_mask = snapshot->canonical_state.depth_mask;
    }
    if ((mask & GL_STENCIL_BUFFER_BIT) != 0u) {
        ctx->canonical_state.clear_stencil = snapshot->canonical_state.clear_stencil;
        ctx->canonical_state.stencil_test = snapshot->canonical_state.stencil_test;
        ctx->canonical_state.stencil_front = snapshot->canonical_state.stencil_front;
        ctx->canonical_state.stencil_back = snapshot->canonical_state.stencil_back;
        ctx->canonical_state.stencil_back_ext = snapshot->canonical_state.stencil_back_ext;
        ctx->canonical_state.active_stencil_face = snapshot->canonical_state.active_stencil_face;
    }
    if ((mask & GL_COLOR_BUFFER_BIT) != 0u) {
        ctx->canonical_state.clamp_fragment_color = snapshot->canonical_state.clamp_fragment_color;
        ctx->canonical_state.clamp_read_color = snapshot->canonical_state.clamp_read_color;
        ctx->canonical_state.clear_index = snapshot->canonical_state.clear_index;
        ctx->canonical_state.index_writemask = snapshot->canonical_state.index_writemask;
        memcpy(ctx->canonical_state.clear_color, snapshot->canonical_state.clear_color,
               sizeof(ctx->canonical_state.clear_color));
        memcpy(ctx->canonical_state.color_mask, snapshot->canonical_state.color_mask,
               sizeof(ctx->canonical_state.color_mask));
        ctx->canonical_state.alpha_test = snapshot->canonical_state.alpha_test;
        ctx->canonical_state.alpha_func = snapshot->canonical_state.alpha_func;
        ctx->canonical_state.alpha_ref = snapshot->canonical_state.alpha_ref;
        ctx->canonical_state.blend = snapshot->canonical_state.blend;
        ctx->canonical_state.color_logic_op = snapshot->canonical_state.color_logic_op;
        ctx->canonical_state.logic_op_mode = snapshot->canonical_state.logic_op_mode;
        ctx->canonical_state.blend_src_rgb = snapshot->canonical_state.blend_src_rgb;
        ctx->canonical_state.blend_dst_rgb = snapshot->canonical_state.blend_dst_rgb;
        ctx->canonical_state.blend_src_alpha = snapshot->canonical_state.blend_src_alpha;
        ctx->canonical_state.blend_dst_alpha = snapshot->canonical_state.blend_dst_alpha;
        ctx->canonical_state.blend_equation_rgb = snapshot->canonical_state.blend_equation_rgb;
        ctx->canonical_state.blend_equation_alpha = snapshot->canonical_state.blend_equation_alpha;
        memcpy(ctx->canonical_state.blend_color, snapshot->canonical_state.blend_color,
               sizeof(ctx->canonical_state.blend_color));
        memcpy(ctx->canonical_state.draw_buffers, snapshot->canonical_state.draw_buffers,
               sizeof(ctx->canonical_state.draw_buffers));
        ctx->canonical_state.dither = snapshot->canonical_state.dither;
        ctx->canonical_state.multisample = snapshot->canonical_state.multisample;
        ctx->canonical_state.framebuffer_srgb = snapshot->canonical_state.framebuffer_srgb;
    }
    if ((mask & GL_POLYGON_BIT) != 0u) {
        ctx->canonical_state.cull_face = snapshot->canonical_state.cull_face;
        ctx->canonical_state.cull_face_mode = snapshot->canonical_state.cull_face_mode;
        ctx->canonical_state.front_face = snapshot->canonical_state.front_face;
        memcpy(ctx->canonical_state.polygon_mode, snapshot->canonical_state.polygon_mode,
               sizeof(ctx->canonical_state.polygon_mode));
        ctx->canonical_state.polygon_offset_fill = snapshot->canonical_state.polygon_offset_fill;
        ctx->canonical_state.polygon_offset_point = snapshot->canonical_state.polygon_offset_point;
        ctx->canonical_state.polygon_offset_line = snapshot->canonical_state.polygon_offset_line;
        ctx->canonical_state.polygon_offset_factor =
            snapshot->canonical_state.polygon_offset_factor;
        ctx->canonical_state.polygon_offset_units = snapshot->canonical_state.polygon_offset_units;
    }
    if ((mask & GL_POLYGON_STIPPLE_BIT) != 0u) {
        ctx->canonical_state.polygon_stipple = snapshot->canonical_state.polygon_stipple;
        memcpy(ctx->canonical_state.polygon_stipple_mask,
               snapshot->canonical_state.polygon_stipple_mask,
               sizeof(ctx->canonical_state.polygon_stipple_mask));
    }
    if ((mask & GL_LINE_BIT) != 0u) {
        ctx->canonical_state.line_width = snapshot->canonical_state.line_width;
        ctx->canonical_state.line_smooth = snapshot->canonical_state.line_smooth;
        ctx->canonical_state.line_stipple = snapshot->canonical_state.line_stipple;
        ctx->canonical_state.line_stipple_factor = snapshot->canonical_state.line_stipple_factor;
        ctx->canonical_state.line_stipple_pattern = snapshot->canonical_state.line_stipple_pattern;
    }
    if ((mask & GL_POINT_BIT) != 0u) {
        ctx->canonical_state.point_size = snapshot->canonical_state.point_size;
        ctx->canonical_state.point_size_min = snapshot->canonical_state.point_size_min;
        ctx->canonical_state.point_size_max = snapshot->canonical_state.point_size_max;
        ctx->canonical_state.point_fade_threshold_size =
            snapshot->canonical_state.point_fade_threshold_size;
        memcpy(ctx->canonical_state.point_distance_attenuation,
               snapshot->canonical_state.point_distance_attenuation,
               sizeof(ctx->canonical_state.point_distance_attenuation));
        ctx->canonical_state.program_point_size_enabled =
            snapshot->canonical_state.program_point_size_enabled;
        ctx->canonical_state.point_sprite_coord_origin =
            snapshot->canonical_state.point_sprite_coord_origin;
        ctx->canonical_state.point_sprite_enabled = snapshot->canonical_state.point_sprite_enabled;
        ctx->canonical_state.program_point_size_enabled =
            snapshot->canonical_state.program_point_size_enabled;
    }
    if ((mask & GL_MULTISAMPLE_BIT) != 0u) {
        ctx->canonical_state.sample_coverage_value =
            snapshot->canonical_state.sample_coverage_value;
        ctx->canonical_state.sample_coverage_invert =
            snapshot->canonical_state.sample_coverage_invert;
    }
    if ((mask & GL_HINT_BIT) != 0u) {
        ctx->canonical_state.perspective_correction_hint =
            snapshot->canonical_state.perspective_correction_hint;
        ctx->canonical_state.point_smooth_hint = snapshot->canonical_state.point_smooth_hint;
        ctx->canonical_state.line_smooth_hint = snapshot->canonical_state.line_smooth_hint;
        ctx->canonical_state.polygon_smooth_hint = snapshot->canonical_state.polygon_smooth_hint;
        ctx->canonical_state.fog_hint = snapshot->canonical_state.fog_hint;
        ctx->canonical_state.generate_mipmap_hint = snapshot->canonical_state.generate_mipmap_hint;
        ctx->canonical_state.texture_compression_hint =
            snapshot->canonical_state.texture_compression_hint;
        ctx->canonical_state.fragment_shader_derivative_hint =
            snapshot->canonical_state.fragment_shader_derivative_hint;
    }
    if ((mask & GL_LIGHTING_BIT) != 0u) {
        ctx->canonical_state.clamp_vertex_color = snapshot->canonical_state.clamp_vertex_color;
        ctx->compat.lighting_enabled = snapshot->compat.lighting_enabled;
        ctx->compat.color_material_enabled = snapshot->compat.color_material_enabled;
        ctx->compat.color_sum_enabled = snapshot->compat.color_sum_enabled;
        ctx->compat.normalize_enabled = snapshot->compat.normalize_enabled;
        ctx->compat.rescale_normal_enabled = snapshot->compat.rescale_normal_enabled;
        ctx->compat.arb_vertex_program_enabled = snapshot->compat.arb_vertex_program_enabled;
        ctx->compat.arb_fragment_program_enabled = snapshot->compat.arb_fragment_program_enabled;
        ctx->compat.color_material_face = snapshot->compat.color_material_face;
        ctx->compat.color_material_mode = snapshot->compat.color_material_mode;
        ctx->compat.shade_model = snapshot->compat.shade_model;
        memcpy(ctx->compat.light_model_ambient, snapshot->compat.light_model_ambient,
               sizeof(ctx->compat.light_model_ambient));
        ctx->compat.light_model_local_viewer = snapshot->compat.light_model_local_viewer;
        ctx->compat.light_model_two_side = snapshot->compat.light_model_two_side;
        memcpy(ctx->compat.lights, snapshot->compat.lights, sizeof(ctx->compat.lights));
        ctx->compat.material_front = snapshot->compat.material_front;
        ctx->compat.material_back = snapshot->compat.material_back;
    }
    if ((mask & GL_FOG_BIT) != 0u) {
        ctx->compat.fog_enabled = snapshot->compat.fog_enabled;
        ctx->compat.fog_mode = snapshot->compat.fog_mode;
        ctx->compat.fog_density = snapshot->compat.fog_density;
        ctx->compat.fog_start = snapshot->compat.fog_start;
        ctx->compat.fog_end = snapshot->compat.fog_end;
        ctx->compat.fog_index = snapshot->compat.fog_index;
        ctx->compat.fog_coord_source = snapshot->compat.fog_coord_source;
        memcpy(ctx->compat.fog_color, snapshot->compat.fog_color, sizeof(ctx->compat.fog_color));
    }
    if ((mask & GL_TEXTURE_BIT) != 0u) {
        ctx->compat.active_texture = snapshot->compat.active_texture;
        for (i = 0u; i < MGL_TEXTURE_UNITS; ++i) {
            ctx->compat.texture_1d_enabled[i] = snapshot->compat.texture_1d_enabled[i];
            ctx->compat.texture_2d_enabled[i] = snapshot->compat.texture_2d_enabled[i];
            ctx->compat.texture_3d_enabled[i] = snapshot->compat.texture_3d_enabled[i];
            ctx->compat.texture_cube_enabled[i] = snapshot->compat.texture_cube_enabled[i];
            ctx->compat.texture_rectangle_enabled[i] =
                snapshot->compat.texture_rectangle_enabled[i];
            ctx->compat.texture_env_mode[i] = snapshot->compat.texture_env_mode[i];
            ctx->compat.texture_lod_bias[i] = snapshot->compat.texture_lod_bias[i];
            ctx->compat.texture_env_combine[i] = snapshot->compat.texture_env_combine[i];
            memcpy(ctx->compat.texgen[i], snapshot->compat.texgen[i],
                   sizeof(ctx->compat.texgen[i]));
        }
        memcpy(ctx->texture_1d_binding, snapshot->texture_1d_binding,
               sizeof(ctx->texture_1d_binding));
        memcpy(ctx->texture_2d_binding, snapshot->texture_2d_binding,
               sizeof(ctx->texture_2d_binding));
        memcpy(ctx->texture_rectangle_binding, snapshot->texture_rectangle_binding,
               sizeof(ctx->texture_rectangle_binding));
        memcpy(ctx->texture_2d_prefer_rectangle, snapshot->texture_2d_prefer_rectangle,
               sizeof(ctx->texture_2d_prefer_rectangle));
        memcpy(ctx->texture_2d_prefer_1d, snapshot->texture_2d_prefer_1d,
               sizeof(ctx->texture_2d_prefer_1d));
        memcpy(ctx->texture_3d_binding, snapshot->texture_3d_binding,
               sizeof(ctx->texture_3d_binding));
        memcpy(ctx->texture_2d_array_binding, snapshot->texture_2d_array_binding,
               sizeof(ctx->texture_2d_array_binding));
        memcpy(ctx->texture_buffer_binding, snapshot->texture_buffer_binding,
               sizeof(ctx->texture_buffer_binding));
        memcpy(ctx->texture_cube_binding, snapshot->texture_cube_binding,
               sizeof(ctx->texture_cube_binding));
        memcpy(ctx->texture_cube_array_binding, snapshot->texture_cube_array_binding,
               sizeof(ctx->texture_cube_array_binding));
        memcpy(ctx->texture_2d_multisample_binding, snapshot->texture_2d_multisample_binding,
               sizeof(ctx->texture_2d_multisample_binding));
        memcpy(ctx->texture_2d_multisample_array_binding,
               snapshot->texture_2d_multisample_array_binding,
               sizeof(ctx->texture_2d_multisample_array_binding));
    }
    if ((mask & GL_TRANSFORM_BIT) != 0u) {
        ctx->compat.matrix_mode = snapshot->compat.matrix_mode;
        memcpy(ctx->compat.clip_plane, snapshot->compat.clip_plane, sizeof(ctx->compat.clip_plane));
    }
    if ((mask & GL_MULTISAMPLE_BIT) != 0u) {
        ctx->canonical_state.multisample = snapshot->canonical_state.multisample;
        ctx->canonical_state.sample_alpha_to_coverage =
            snapshot->canonical_state.sample_alpha_to_coverage;
        ctx->canonical_state.sample_coverage = snapshot->canonical_state.sample_coverage;
    }
    if ((mask & GL_ENABLE_BIT) != 0u) {
        ctx->canonical_state.point_sprite_enabled = snapshot->canonical_state.point_sprite_enabled;
        ctx->canonical_state.program_point_size_enabled =
            snapshot->canonical_state.program_point_size_enabled;
        ctx->canonical_state.alpha_test = snapshot->canonical_state.alpha_test;
        ctx->canonical_state.depth_test = snapshot->canonical_state.depth_test;
        ctx->canonical_state.stencil_test = snapshot->canonical_state.stencil_test;
        ctx->canonical_state.stencil_two_side_test =
            snapshot->canonical_state.stencil_two_side_test;
        ctx->canonical_state.blend = snapshot->canonical_state.blend;
        ctx->canonical_state.color_logic_op = snapshot->canonical_state.color_logic_op;
        for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i)
            ctx->canonical_state.draw_buffers[i].blend =
                snapshot->canonical_state.draw_buffers[i].blend;
        ctx->canonical_state.cull_face = snapshot->canonical_state.cull_face;
        ctx->canonical_state.scissor_test = snapshot->canonical_state.scissor_test;
        ctx->canonical_state.dither = snapshot->canonical_state.dither;
        ctx->canonical_state.multisample = snapshot->canonical_state.multisample;
        ctx->canonical_state.line_smooth = snapshot->canonical_state.line_smooth;
        ctx->canonical_state.line_stipple = snapshot->canonical_state.line_stipple;
        ctx->canonical_state.polygon_stipple = snapshot->canonical_state.polygon_stipple;
        ctx->canonical_state.framebuffer_srgb = snapshot->canonical_state.framebuffer_srgb;
        ctx->canonical_state.polygon_offset_fill = snapshot->canonical_state.polygon_offset_fill;
        ctx->canonical_state.polygon_offset_point = snapshot->canonical_state.polygon_offset_point;
        ctx->canonical_state.polygon_offset_line = snapshot->canonical_state.polygon_offset_line;
        ctx->canonical_state.polygon_offset_factor =
            snapshot->canonical_state.polygon_offset_factor;
        ctx->canonical_state.polygon_offset_units = snapshot->canonical_state.polygon_offset_units;
        ctx->canonical_state.sample_alpha_to_coverage =
            snapshot->canonical_state.sample_alpha_to_coverage;
        ctx->canonical_state.sample_coverage = snapshot->canonical_state.sample_coverage;
        ctx->compat.fog_enabled = snapshot->compat.fog_enabled;
        ctx->compat.lighting_enabled = snapshot->compat.lighting_enabled;
        ctx->compat.color_material_enabled = snapshot->compat.color_material_enabled;
        ctx->compat.color_sum_enabled = snapshot->compat.color_sum_enabled;
        ctx->compat.normalize_enabled = snapshot->compat.normalize_enabled;
        ctx->compat.rescale_normal_enabled = snapshot->compat.rescale_normal_enabled;
        ctx->compat.arb_vertex_program_enabled = snapshot->compat.arb_vertex_program_enabled;
        ctx->compat.arb_fragment_program_enabled = snapshot->compat.arb_fragment_program_enabled;
        for (i = 0u; i < MGL_MAX_LIGHTS; ++i)
            ctx->compat.lights[i].enabled = snapshot->compat.lights[i].enabled;
        for (i = 0u; i < MGL_MAX_CLIP_PLANES; ++i)
            ctx->compat.clip_plane_enabled[i] = snapshot->compat.clip_plane_enabled[i];
        for (i = 0u; i < MGL_TEXTURE_UNITS; ++i) {
            unsigned coord;
            ctx->compat.texture_1d_enabled[i] = snapshot->compat.texture_1d_enabled[i];
            ctx->compat.texture_2d_enabled[i] = snapshot->compat.texture_2d_enabled[i];
            ctx->compat.texture_3d_enabled[i] = snapshot->compat.texture_3d_enabled[i];
            ctx->compat.texture_cube_enabled[i] = snapshot->compat.texture_cube_enabled[i];
            ctx->compat.texture_rectangle_enabled[i] =
                snapshot->compat.texture_rectangle_enabled[i];
            for (coord = 0u; coord < MGL_TEXGEN_COORDS; ++coord)
                ctx->compat.texgen[i][coord].enabled = snapshot->compat.texgen[i][coord].enabled;
        }
    }
    if ((mask & (GL_TEXTURE_BIT | GL_ENABLE_BIT | GL_LIGHTING_BIT)) != 0u)
        ctx->compat.fixed_variant_key_valid = GL_FALSE;
    if (realize)
        mgl_realize_restored_server_state(ctx, mask);
}

void glPopAttrib(void) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_attrib_snapshot *snapshot;
    GLbitfield mask;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying && mgl_list_record_pop_attrib(ctx))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_pop_attrib(ctx))
        return;
    if (ctx->attrib_stack_depth == 0u) {
        mgl_error_set(&ctx->errors, GL_STACK_UNDERFLOW);
        return;
    }
    snapshot = &ctx->attrib_stack[--ctx->attrib_stack_depth];
    mask = snapshot->mask;

    mgl_apply_attrib_state(ctx, snapshot, mask, 1);
}

int mgl_copy_server_context_state(struct mgl_context *dst, const struct mgl_context *src,
                                  GLbitfield mask) {
    struct mgl_attrib_snapshot *snapshot;
    if (dst == NULL || src == NULL)
        return 0;
    snapshot = malloc(sizeof(*snapshot));
    if (snapshot == NULL)
        return 0;
    mgl_capture_attrib_state(src, snapshot, mask);
    mgl_apply_attrib_state(dst, snapshot, mask, 0);
    dst->glx_pending_copy_mask |= mask;
    free(snapshot);
    return 1;
}

void mgl_realize_copied_server_state(struct mgl_context *ctx) {
    GLbitfield mask;
    if (ctx == NULL)
        return;
    mask = ctx->glx_pending_copy_mask;
    ctx->glx_pending_copy_mask = 0u;
    if (mask != 0u)
        mgl_realize_restored_server_state(ctx, mask);
}

void glPushClientAttrib(GLbitfield mask) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_client_attrib_snapshot *snapshot;
    const GLbitfield allowed = GL_CLIENT_PIXEL_STORE_BIT | GL_CLIENT_VERTEX_ARRAY_BIT;
    if (ctx == NULL)
        return;
    if (reject_inside_begin(ctx))
        return;
    if (mask != GL_CLIENT_ALL_ATTRIB_BITS && (mask & ~allowed) != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (ctx->client_attrib_stack_depth >= MGL_ATTRIB_STACK_MAX_DEPTH) {
        mgl_error_set(&ctx->errors, GL_STACK_OVERFLOW);
        return;
    }
    if (mask == GL_CLIENT_ALL_ATTRIB_BITS)
        mask = allowed;
    snapshot = &ctx->client_attrib_stack[ctx->client_attrib_stack_depth++];
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->mask = mask;
    snapshot->array_buffer = ctx->array_buffer;
    snapshot->pixel_pack_buffer = ctx->pixel_pack_buffer;
    snapshot->pixel_unpack_buffer = ctx->pixel_unpack_buffer;
    snapshot->client_active_texture = ctx->client_active_texture;
    snapshot->array_lock_first = ctx->array_lock_first;
    snapshot->array_lock_count = ctx->array_lock_count;
    snapshot->pack_alignment = ctx->pack_alignment;
    snapshot->unpack_alignment = ctx->unpack_alignment;
    snapshot->pack_row_length = ctx->pack_row_length;
    snapshot->pack_skip_rows = ctx->pack_skip_rows;
    snapshot->pack_skip_pixels = ctx->pack_skip_pixels;
    snapshot->pack_image_height = ctx->pack_image_height;
    snapshot->pack_skip_images = ctx->pack_skip_images;
    snapshot->pack_swap_bytes = ctx->pack_swap_bytes;
    snapshot->pack_lsb_first = ctx->pack_lsb_first;
    snapshot->unpack_row_length = ctx->unpack_row_length;
    snapshot->unpack_skip_rows = ctx->unpack_skip_rows;
    snapshot->unpack_skip_pixels = ctx->unpack_skip_pixels;
    snapshot->unpack_image_height = ctx->unpack_image_height;
    snapshot->unpack_skip_images = ctx->unpack_skip_images;
    snapshot->unpack_swap_bytes = ctx->unpack_swap_bytes;
    snapshot->unpack_lsb_first = ctx->unpack_lsb_first;
    snapshot->element_array_buffer = ctx->current_vao->element_array_buffer;
    memcpy(snapshot->compat, ctx->current_vao->compat, sizeof(snapshot->compat));
    snapshot->edge_flag = ctx->current_vao->edge_flag;
    snapshot->color_index = ctx->current_vao->color_index;
    memcpy(snapshot->generic, ctx->current_vao->generic, sizeof(snapshot->generic));
}

void glPopClientAttrib(void) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_client_attrib_snapshot *snapshot;
    if (ctx == NULL)
        return;
    if (reject_inside_begin(ctx))
        return;
    if (ctx->client_attrib_stack_depth == 0u) {
        mgl_error_set(&ctx->errors, GL_STACK_UNDERFLOW);
        return;
    }
    snapshot = &ctx->client_attrib_stack[--ctx->client_attrib_stack_depth];
    if ((snapshot->mask & GL_CLIENT_VERTEX_ARRAY_BIT) != 0u) {
        ctx->array_buffer = snapshot->array_buffer;
        ctx->client_active_texture = snapshot->client_active_texture;
        ctx->array_lock_first = snapshot->array_lock_first;
        ctx->array_lock_count = snapshot->array_lock_count;
        mgl_restore_element_array_binding(ctx, snapshot->element_array_buffer);
        memcpy(ctx->current_vao->compat, snapshot->compat, sizeof(snapshot->compat));
        ctx->current_vao->edge_flag = snapshot->edge_flag;
        ctx->current_vao->color_index = snapshot->color_index;
        memcpy(ctx->current_vao->generic, snapshot->generic, sizeof(snapshot->generic));
    }
    if ((snapshot->mask & GL_CLIENT_PIXEL_STORE_BIT) != 0u) {
        ctx->pixel_pack_buffer = snapshot->pixel_pack_buffer;
        ctx->pixel_unpack_buffer = snapshot->pixel_unpack_buffer;
        ctx->pack_alignment = snapshot->pack_alignment;
        ctx->unpack_alignment = snapshot->unpack_alignment;
        ctx->pack_row_length = snapshot->pack_row_length;
        ctx->pack_skip_rows = snapshot->pack_skip_rows;
        ctx->pack_skip_pixels = snapshot->pack_skip_pixels;
        ctx->pack_image_height = snapshot->pack_image_height;
        ctx->pack_skip_images = snapshot->pack_skip_images;
        ctx->pack_swap_bytes = snapshot->pack_swap_bytes;
        ctx->pack_lsb_first = snapshot->pack_lsb_first;
        ctx->unpack_row_length = snapshot->unpack_row_length;
        ctx->unpack_skip_rows = snapshot->unpack_skip_rows;
        ctx->unpack_skip_pixels = snapshot->unpack_skip_pixels;
        ctx->unpack_image_height = snapshot->unpack_image_height;
        ctx->unpack_skip_images = snapshot->unpack_skip_images;
        ctx->unpack_swap_bytes = snapshot->unpack_swap_bytes;
        ctx->unpack_lsb_first = snapshot->unpack_lsb_first;
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesPixelStorei != NULL) {
            ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, ctx->pack_alignment);
            ctx->native_dispatch->glesPixelStorei(GL_UNPACK_ALIGNMENT, ctx->unpack_alignment);
        }
    }
}

static int valid_client_attrib_mask(GLbitfield mask, GLbitfield *normalized) {
    const GLbitfield allowed = GL_CLIENT_PIXEL_STORE_BIT | GL_CLIENT_VERTEX_ARRAY_BIT;
    if (mask == GL_CLIENT_ALL_ATTRIB_BITS) {
        if (normalized != NULL)
            *normalized = allowed;
        return 1;
    }
    if ((mask & ~allowed) != 0u)
        return 0;
    if (normalized != NULL)
        *normalized = mask;
    return 1;
}

void glClientAttribDefaultEXT(GLbitfield mask) {
    struct mgl_context *ctx = mgl_context_current();
    GLbitfield normalized;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_client_attrib_mask(mask, &normalized)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }

    if ((normalized & GL_CLIENT_PIXEL_STORE_BIT) != 0u) {
        ctx->pixel_pack_buffer = 0u;
        ctx->pixel_unpack_buffer = 0u;
        ctx->pack_alignment = 4;
        ctx->unpack_alignment = 4;
        ctx->pack_row_length = 0;
        ctx->pack_skip_rows = 0;
        ctx->pack_skip_pixels = 0;
        ctx->pack_image_height = 0;
        ctx->pack_skip_images = 0;
        ctx->pack_swap_bytes = GL_FALSE;
        ctx->pack_lsb_first = GL_FALSE;
        ctx->unpack_row_length = 0;
        ctx->unpack_skip_rows = 0;
        ctx->unpack_skip_pixels = 0;
        ctx->unpack_image_height = 0;
        ctx->unpack_skip_images = 0;
        ctx->unpack_swap_bytes = GL_FALSE;
        ctx->unpack_lsb_first = GL_FALSE;
        if (ctx->native_dispatch != NULL) {
            if (ctx->native_dispatch->glesBindBuffer != NULL) {
                mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_PACK_BUFFER, 0u);
                mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_UNPACK_BUFFER, 0u);
            }
            if (ctx->native_dispatch->glesPixelStorei != NULL) {
                ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, 4);
                ctx->native_dispatch->glesPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            }
        }
    }

    if ((normalized & GL_CLIENT_VERTEX_ARRAY_BIT) != 0u) {
        struct mgl_vertex_array defaults;
        memset(&defaults, 0, sizeof(defaults));
        mgl_vertex_array_init(&defaults);
        ctx->array_buffer = 0u;
        ctx->client_active_texture = 0u;
        ctx->array_lock_first = 0;
        ctx->array_lock_count = 0;
        ctx->current_vao->element_array_buffer = 0u;
        memcpy(ctx->current_vao->compat, defaults.compat, sizeof(defaults.compat));
        ctx->current_vao->edge_flag = defaults.edge_flag;
        ctx->current_vao->color_index = defaults.color_index;
        memcpy(ctx->current_vao->generic, defaults.generic, sizeof(defaults.generic));
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindBuffer != NULL) {
            mgl_native_dispatch_bind_buffer(ctx, GL_ARRAY_BUFFER, 0u);
            mgl_native_dispatch_bind_buffer(ctx, GL_ELEMENT_ARRAY_BUFFER, 0u);
        }
    }
}

void glPushClientAttribDefaultEXT(GLbitfield mask) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned depth;
    if (ctx == NULL)
        return;
    depth = ctx->client_attrib_stack_depth;
    glPushClientAttrib(mask);
    if (ctx->client_attrib_stack_depth == depth + 1u)
        glClientAttribDefaultEXT(mask);
}

void glPixelMapfv(GLenum map, GLsizei mapsize, const GLfloat *values) {
    struct mgl_context *ctx = mgl_context_current();
    GLdouble canonical[MGL_PIXEL_MAP_MAX_SIZE];
    GLsizei i;
    int integer;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active && (ctx->compiling_list == NULL || ctx->list_replaying)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pixel_map_index(map) < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mapsize <= 0 || mapsize > (GLsizei)MGL_PIXEL_MAP_MAX_SIZE || values == NULL ||
        (pixel_map_needs_power_of_two(map) && !is_power_of_two_size(mapsize))) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    integer = pixel_map_is_integer(map);
    for (i = 0; i < mapsize; ++i)
        canonical[i] = integer ? floor((GLdouble)values[i] + 0.5) : clamp01d((GLdouble)values[i]);
    if (mgl_list_record_pixel_map(ctx, map, mapsize, canonical))
        return;
    if (reject_inside_begin(ctx))
        return;
    (void)mgl_pixel_map_set_canonical(ctx, map, mapsize, canonical);
}

void glPixelMapuiv(GLenum map, GLsizei mapsize, const GLuint *values) {
    struct mgl_context *ctx = mgl_context_current();
    GLdouble canonical[MGL_PIXEL_MAP_MAX_SIZE];
    GLsizei i;
    int integer;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active && (ctx->compiling_list == NULL || ctx->list_replaying)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pixel_map_index(map) < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mapsize <= 0 || mapsize > (GLsizei)MGL_PIXEL_MAP_MAX_SIZE || values == NULL ||
        (pixel_map_needs_power_of_two(map) && !is_power_of_two_size(mapsize))) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    integer = pixel_map_is_integer(map);
    for (i = 0; i < mapsize; ++i)
        canonical[i] = integer ? (GLdouble)values[i] : (GLdouble)values[i] / (GLdouble)UINT_MAX;
    if (mgl_list_record_pixel_map(ctx, map, mapsize, canonical))
        return;
    if (reject_inside_begin(ctx))
        return;
    (void)mgl_pixel_map_set_canonical(ctx, map, mapsize, canonical);
}

void glPixelMapusv(GLenum map, GLsizei mapsize, const GLushort *values) {
    struct mgl_context *ctx = mgl_context_current();
    GLdouble canonical[MGL_PIXEL_MAP_MAX_SIZE];
    GLsizei i;
    int integer;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active && (ctx->compiling_list == NULL || ctx->list_replaying)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (pixel_map_index(map) < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mapsize <= 0 || mapsize > (GLsizei)MGL_PIXEL_MAP_MAX_SIZE || values == NULL ||
        (pixel_map_needs_power_of_two(map) && !is_power_of_two_size(mapsize))) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    integer = pixel_map_is_integer(map);
    for (i = 0; i < mapsize; ++i)
        canonical[i] = integer ? (GLdouble)values[i] : (GLdouble)values[i] / (GLdouble)USHRT_MAX;
    if (mgl_list_record_pixel_map(ctx, map, mapsize, canonical))
        return;
    if (reject_inside_begin(ctx))
        return;
    (void)mgl_pixel_map_set_canonical(ctx, map, mapsize, canonical);
}

void glGetPixelMapfv(GLenum map, GLfloat *values) {
    struct mgl_context *ctx = mgl_context_current();
    int index;
    GLsizei i;
    if (ctx == NULL)
        return;
    if (reject_inside_begin(ctx))
        return;
    if (values == NULL)
        return;
    index = pixel_map_index(map);
    if (index < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    for (i = 0; i < ctx->pixel_transfer.maps[index].size; ++i)
        values[i] = (GLfloat)ctx->pixel_transfer.maps[index].values[i];
}

void glGetPixelMapuiv(GLenum map, GLuint *values) {
    struct mgl_context *ctx = mgl_context_current();
    int index, integer;
    GLsizei i;
    if (ctx == NULL)
        return;
    if (reject_inside_begin(ctx))
        return;
    if (values == NULL)
        return;
    index = pixel_map_index(map);
    if (index < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    integer = pixel_map_is_integer(map);
    for (i = 0; i < ctx->pixel_transfer.maps[index].size; ++i) {
        GLdouble value = ctx->pixel_transfer.maps[index].values[i];
        values[i] = integer ? (GLuint)(value < 0.0 ? 0.0 : value)
                            : (GLuint)(clamp01d(value) * (GLdouble)UINT_MAX + 0.5);
    }
}

void glGetPixelMapusv(GLenum map, GLushort *values) {
    struct mgl_context *ctx = mgl_context_current();
    int index, integer;
    GLsizei i;
    if (ctx == NULL)
        return;
    if (reject_inside_begin(ctx))
        return;
    if (values == NULL)
        return;
    index = pixel_map_index(map);
    if (index < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    integer = pixel_map_is_integer(map);
    for (i = 0; i < ctx->pixel_transfer.maps[index].size; ++i) {
        GLdouble value = ctx->pixel_transfer.maps[index].values[i];
        values[i] = integer ? (GLushort)(value < 0.0 ? 0.0 : value)
                            : (GLushort)(clamp01d(value) * (GLdouble)USHRT_MAX + 0.5);
    }
}

static int getn_pixel_map_size(struct mgl_context *ctx, GLenum map, GLsizei bufSize,
                               size_t element_size, const void *values, size_t *bytes) {
    int index;
    if (reject_inside_begin(ctx))
        return 0;
    index = pixel_map_index(map);
    if (index < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    *bytes = (size_t)ctx->pixel_transfer.maps[index].size * element_size;
    if (ctx->pixel_pack_buffer != 0u) {
        struct mgl_buffer *buffer = mgl_buffer_find(ctx, ctx->pixel_pack_buffer);
        size_t offset = (size_t)(uintptr_t)values;
        if (buffer == NULL || buffer->mapped || offset > buffer->size ||
            *bytes > buffer->size - offset) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
    } else if (*bytes > (size_t)bufSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    return 1;
}

void glGetnPixelMapfv(GLenum map, GLsizei bufSize, GLfloat *values) {
    struct mgl_context *ctx = mgl_context_current();
    size_t bytes;
    if (ctx == NULL || !getn_pixel_map_size(ctx, map, bufSize, sizeof(*values), values, &bytes))
        return;
    if (ctx->pixel_pack_buffer != 0u) {
        GLfloat data[MGL_PIXEL_MAP_MAX_SIZE];
        if (!mgl_buffer_shadow_ensure(ctx, mgl_buffer_find(ctx, ctx->pixel_pack_buffer)))
            return;
        glGetPixelMapfv(map, data);
        glBufferSubData(GL_PIXEL_PACK_BUFFER, (GLintptr)(uintptr_t)values, (GLsizeiptr)bytes, data);
    } else
        glGetPixelMapfv(map, values);
}

void glGetnPixelMapuiv(GLenum map, GLsizei bufSize, GLuint *values) {
    struct mgl_context *ctx = mgl_context_current();
    size_t bytes;
    if (ctx == NULL || !getn_pixel_map_size(ctx, map, bufSize, sizeof(*values), values, &bytes))
        return;
    if (ctx->pixel_pack_buffer != 0u) {
        GLuint data[MGL_PIXEL_MAP_MAX_SIZE];
        if (!mgl_buffer_shadow_ensure(ctx, mgl_buffer_find(ctx, ctx->pixel_pack_buffer)))
            return;
        glGetPixelMapuiv(map, data);
        glBufferSubData(GL_PIXEL_PACK_BUFFER, (GLintptr)(uintptr_t)values, (GLsizeiptr)bytes, data);
    } else
        glGetPixelMapuiv(map, values);
}

void glGetnPixelMapusv(GLenum map, GLsizei bufSize, GLushort *values) {
    struct mgl_context *ctx = mgl_context_current();
    size_t bytes;
    if (ctx == NULL || !getn_pixel_map_size(ctx, map, bufSize, sizeof(*values), values, &bytes))
        return;
    if (ctx->pixel_pack_buffer != 0u) {
        GLushort data[MGL_PIXEL_MAP_MAX_SIZE];
        if (!mgl_buffer_shadow_ensure(ctx, mgl_buffer_find(ctx, ctx->pixel_pack_buffer)))
            return;
        glGetPixelMapusv(map, data);
        glBufferSubData(GL_PIXEL_PACK_BUFFER, (GLintptr)(uintptr_t)values, (GLsizeiptr)bytes, data);
    } else
        glGetPixelMapusv(map, values);
}

void glPixelTransferf(GLenum pname, GLfloat param) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_pixel_transfer(ctx, pname, param))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    switch (pname) {
        case GL_MAP_COLOR:
        case GL_MAP_STENCIL:
        case GL_INDEX_SHIFT:
        case GL_INDEX_OFFSET:
        case GL_RED_SCALE:
        case GL_RED_BIAS:
        case GL_GREEN_SCALE:
        case GL_GREEN_BIAS:
        case GL_BLUE_SCALE:
        case GL_BLUE_BIAS:
        case GL_ALPHA_SCALE:
        case GL_ALPHA_BIAS:
        case GL_DEPTH_SCALE:
        case GL_DEPTH_BIAS:
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
    }
    if (mgl_list_record_pixel_transfer(ctx, pname, param))
        return;
    switch (pname) {
        case GL_MAP_COLOR:
            ctx->pixel_transfer.map_color = param != 0.0f ? GL_TRUE : GL_FALSE;
            break;
        case GL_MAP_STENCIL:
            ctx->pixel_transfer.map_stencil = param != 0.0f ? GL_TRUE : GL_FALSE;
            break;
        case GL_INDEX_SHIFT:
            ctx->pixel_transfer.index_shift = (GLint)param;
            break;
        case GL_INDEX_OFFSET:
            ctx->pixel_transfer.index_offset = (GLint)param;
            break;
        case GL_RED_SCALE:
            ctx->pixel_transfer.red_scale = param;
            break;
        case GL_RED_BIAS:
            ctx->pixel_transfer.red_bias = param;
            break;
        case GL_GREEN_SCALE:
            ctx->pixel_transfer.green_scale = param;
            break;
        case GL_GREEN_BIAS:
            ctx->pixel_transfer.green_bias = param;
            break;
        case GL_BLUE_SCALE:
            ctx->pixel_transfer.blue_scale = param;
            break;
        case GL_BLUE_BIAS:
            ctx->pixel_transfer.blue_bias = param;
            break;
        case GL_ALPHA_SCALE:
            ctx->pixel_transfer.alpha_scale = param;
            break;
        case GL_ALPHA_BIAS:
            ctx->pixel_transfer.alpha_bias = param;
            break;
        case GL_DEPTH_SCALE:
            ctx->pixel_transfer.depth_scale = param;
            break;
        case GL_DEPTH_BIAS:
            ctx->pixel_transfer.depth_bias = param;
            break;
        default:
            break;
    }
}

void glPixelTransferi(GLenum pname, GLint param) {
    glPixelTransferf(pname, (GLfloat)param);
}

static GLsizeiptr indexed_binding_effective_size(struct mgl_context *ctx,
                                                 const struct mgl_indexed_buffer_binding *slot) {
    struct mgl_buffer *obj;
    if (ctx == NULL || slot == NULL || slot->buffer == 0u)
        return 0;
    if (slot->ranged)
        return slot->size;
    obj = mgl_buffer_find(ctx, slot->buffer);
    return obj != NULL ? (GLsizeiptr)obj->size : 0;
}

static GLuint indexed_query_limit(struct mgl_context *ctx, GLenum target) {
    GLenum pname;
    GLint native_limit;
    if (target == GL_UNIFORM_BUFFER_BINDING || target == GL_UNIFORM_BUFFER_START ||
        target == GL_UNIFORM_BUFFER_SIZE)
        pname = GL_MAX_UNIFORM_BUFFER_BINDINGS;
    else if (target == GL_SHADER_STORAGE_BUFFER_BINDING ||
             target == GL_SHADER_STORAGE_BUFFER_START || target == GL_SHADER_STORAGE_BUFFER_SIZE)
        pname = GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS;
    else if (target == GL_ATOMIC_COUNTER_BUFFER_BINDING ||
             target == GL_ATOMIC_COUNTER_BUFFER_START || target == GL_ATOMIC_COUNTER_BUFFER_SIZE)
        pname = GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS;
    else if (target == GL_TRANSFORM_FEEDBACK_BUFFER_BINDING ||
             target == GL_TRANSFORM_FEEDBACK_BUFFER_START ||
             target == GL_TRANSFORM_FEEDBACK_BUFFER_SIZE)
        pname = GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS;
    else
        return MGL_MAX_INDEXED_BUFFER_BINDINGS;
    native_limit = gles_limit_or_default(ctx, pname, (GLint)MGL_MAX_INDEXED_BUFFER_BINDINGS);
    if (native_limit > 0 && native_limit < (GLint)MGL_MAX_INDEXED_BUFFER_BINDINGS)
        return (GLuint)native_limit;
    return MGL_MAX_INDEXED_BUFFER_BINDINGS;
}

void glGetIntegeri_v(GLenum target, GLuint index, GLint *data) {
    struct mgl_context *ctx = mgl_context_current();
    const struct mgl_indexed_buffer_binding *slot = NULL;
    const struct mgl_image_unit_binding *image = NULL;
    if (ctx == NULL || data == NULL)
        return;
    if (reject_inside_begin(ctx))
        return;
    if (target == GL_MAX_COMPUTE_WORK_GROUP_COUNT || target == GL_MAX_COMPUTE_WORK_GROUP_SIZE) {
        if (index >= 3u) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegeri_v != NULL)
            ctx->native_dispatch->glesGetIntegeri_v(target, index, data);
        else
            *data = 0;
        return;
    }
    if (target == GL_VIEWPORT || target == GL_SCISSOR_BOX || target == GL_DEPTH_RANGE) {
        GLdouble values[4] = {0};
        int count = target == GL_DEPTH_RANGE ? 2 : 4;
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetDoublei_v(target, index, values);
        if (ctx->errors.sequence != error_sequence_before)
            return;
        for (int i = 0; i < count; ++i)
            data[i] = (GLint)lrint(values[i]);
        return;
    }
    if (target == GL_SAMPLE_MASK_VALUE) {
        if (index != 0u) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        *data = (GLint)ctx->canonical_state.sample_mask_value;
        return;
    }

    if (target == GL_VERTEX_BINDING_BUFFER || target == GL_VERTEX_BINDING_STRIDE ||
        target == GL_VERTEX_BINDING_DIVISOR || target == GL_VERTEX_BINDING_OFFSET) {
        const struct mgl_vertex_buffer_slot *binding;
        if (index >= mgl_context_vertex_binding_limit(ctx)) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        binding = &ctx->current_vao->vertex_buffers[index];
        if (target == GL_VERTEX_BINDING_BUFFER)
            *data = (GLint)binding->data.buffer_name;
        else if (target == GL_VERTEX_BINDING_STRIDE)
            *data = binding->stride;
        else if (target == GL_VERTEX_BINDING_DIVISOR)
            *data = (GLint)binding->divisor;
        else
            *data = (GLint)binding->data.offset;
        return;
    }
    if (target == GL_IMAGE_BINDING_NAME || target == GL_IMAGE_BINDING_LEVEL ||
        target == GL_IMAGE_BINDING_LAYERED || target == GL_IMAGE_BINDING_LAYER ||
        target == GL_IMAGE_BINDING_ACCESS || target == GL_IMAGE_BINDING_FORMAT) {
        if (index >= mgl_context_image_unit_limit(ctx)) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        image = &ctx->image_units[index];
        if (target == GL_IMAGE_BINDING_NAME)
            *data = (GLint)image->texture;
        else if (target == GL_IMAGE_BINDING_LEVEL)
            *data = image->level;
        else if (target == GL_IMAGE_BINDING_LAYERED)
            *data = image->layered ? 1 : 0;
        else if (target == GL_IMAGE_BINDING_LAYER)
            *data = image->layer;
        else if (target == GL_IMAGE_BINDING_ACCESS)
            *data = (GLint)image->access;
        else
            *data = (GLint)image->format;
        return;
    }
    if (target == GL_BLEND || target == GL_COLOR_WRITEMASK || target == GL_BLEND_SRC_RGB ||
        target == GL_BLEND_DST_RGB || target == GL_BLEND_SRC_ALPHA ||
        target == GL_BLEND_DST_ALPHA || target == GL_BLEND_EQUATION_RGB ||
        target == GL_BLEND_EQUATION_ALPHA) {
        const struct mgl_draw_buffer_state *db;
        if (index >= MGL_MAX_DRAW_BUFFERS) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        db = &ctx->canonical_state.draw_buffers[index];
        if (target == GL_BLEND) {
            data[0] = db->blend ? 1 : 0;
            return;
        }
        if (target == GL_COLOR_WRITEMASK) {
            data[0] = db->color_mask[0];
            data[1] = db->color_mask[1];
            data[2] = db->color_mask[2];
            data[3] = db->color_mask[3];
            return;
        }
        if (target == GL_BLEND_SRC_RGB) {
            data[0] = (GLint)db->blend_src_rgb;
            return;
        }
        if (target == GL_BLEND_DST_RGB) {
            data[0] = (GLint)db->blend_dst_rgb;
            return;
        }
        if (target == GL_BLEND_SRC_ALPHA) {
            data[0] = (GLint)db->blend_src_alpha;
            return;
        }
        if (target == GL_BLEND_DST_ALPHA) {
            data[0] = (GLint)db->blend_dst_alpha;
            return;
        }
        if (target == GL_BLEND_EQUATION_RGB) {
            data[0] = (GLint)db->blend_equation_rgb;
            return;
        }
        data[0] = (GLint)db->blend_equation_alpha;
        return;
    }
    if (index >= indexed_query_limit(ctx, target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    switch (target) {
        case GL_UNIFORM_BUFFER_BINDING:
            slot = &ctx->uniform_buffer_bindings[index];
            *data = (GLint)slot->buffer;
            return;
        case GL_ATOMIC_COUNTER_BUFFER_BINDING:
            slot = &ctx->atomic_counter_buffer_bindings[index];
            *data = (GLint)slot->buffer;
            return;
        case GL_ATOMIC_COUNTER_BUFFER_START:
            slot = &ctx->atomic_counter_buffer_bindings[index];
            *data = (GLint)slot->offset;
            return;
        case GL_ATOMIC_COUNTER_BUFFER_SIZE:
            slot = &ctx->atomic_counter_buffer_bindings[index];
            *data = (GLint)indexed_binding_effective_size(ctx, slot);
            return;
        case GL_SHADER_STORAGE_BUFFER_BINDING:
            slot = &ctx->shader_storage_buffer_bindings[index];
            *data = (GLint)slot->buffer;
            return;
        case GL_SHADER_STORAGE_BUFFER_START:
            slot = &ctx->shader_storage_buffer_bindings[index];
            *data = (GLint)slot->offset;
            return;
        case GL_SHADER_STORAGE_BUFFER_SIZE:
            slot = &ctx->shader_storage_buffer_bindings[index];
            *data = (GLint)indexed_binding_effective_size(ctx, slot);
            return;
        case GL_UNIFORM_BUFFER_START:
            slot = &ctx->uniform_buffer_bindings[index];
            *data = (GLint)slot->offset;
            return;
        case GL_UNIFORM_BUFFER_SIZE:
            slot = &ctx->uniform_buffer_bindings[index];
            *data = (GLint)indexed_binding_effective_size(ctx, slot);
            return;
        case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
            slot = current_tf_indexed_binding(ctx, index);
            *data = (GLint)slot->buffer;
            return;
        case GL_TRANSFORM_FEEDBACK_BUFFER_START:
            slot = current_tf_indexed_binding(ctx, index);
            *data = (GLint)slot->offset;
            return;
        case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
            slot = current_tf_indexed_binding(ctx, index);
            *data = (GLint)indexed_binding_effective_size(ctx, slot);
            return;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
    }
}

void glGetIntegerIndexedv(GLenum target, GLuint index, GLint *data) {
    glGetIntegeri_v(target, index, data);
}

void glGetIntegerIndexedvEXT(GLenum target, GLuint index, GLint *data) {
    glGetIntegeri_v(target, index, data);
}

void glGetFloatIndexedv(GLenum target, GLuint index, GLfloat *data) {
    glGetFloati_v(target, index, data);
}

void glGetFloatIndexedvEXT(GLenum target, GLuint index, GLfloat *data) {
    glGetFloatIndexedv(target, index, data);
}

void glGetDoubleIndexedv(GLenum target, GLuint index, GLdouble *data) {
    glGetDoublei_v(target, index, data);
}

void glGetDoubleIndexedvEXT(GLenum target, GLuint index, GLdouble *data) {
    glGetDoubleIndexedv(target, index, data);
}

void glGetInteger64i_v(GLenum target, GLuint index, GLint64 *data) {
    struct mgl_context *ctx = mgl_context_current();
    const struct mgl_indexed_buffer_binding *slot = NULL;
    GLint image_value = 0;
    GLint indexed_values[4] = {0, 0, 0, 0};
    int i;
    if (ctx == NULL || data == NULL)
        return;
    if (reject_inside_begin(ctx))
        return;
    if (target == GL_MAX_COMPUTE_WORK_GROUP_COUNT || target == GL_MAX_COMPUTE_WORK_GROUP_SIZE) {
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetIntegeri_v(target, index, &image_value);
        if (ctx->errors.sequence == error_sequence_before)
            *data = (GLint64)image_value;
        return;
    }
    if (target == GL_VIEWPORT || target == GL_SCISSOR_BOX || target == GL_DEPTH_RANGE) {
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetIntegeri_v(target, index, indexed_values);
        if (ctx->errors.sequence == error_sequence_before)
            for (i = 0; i < mgl_indexed_query_component_count(target); ++i)
                data[i] = indexed_values[i];
        return;
    }
    if (target == GL_SAMPLE_MASK_VALUE) {
        if (index != 0u) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        *data = (GLint64)(GLint)ctx->canonical_state.sample_mask_value;
        return;
    }
    if (target == GL_VERTEX_BINDING_OFFSET) {
        if (index >= mgl_context_vertex_binding_limit(ctx)) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        *data = (GLint64)ctx->current_vao->vertex_buffers[index].data.offset;
        return;
    }
    if (target == GL_VERTEX_BINDING_BUFFER || target == GL_VERTEX_BINDING_STRIDE ||
        target == GL_VERTEX_BINDING_DIVISOR) {
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetIntegeri_v(target, index, &image_value);
        if (ctx->errors.sequence == error_sequence_before)
            *data = (GLint64)image_value;
        return;
    }
    if (target == GL_IMAGE_BINDING_NAME || target == GL_IMAGE_BINDING_LEVEL ||
        target == GL_IMAGE_BINDING_LAYERED || target == GL_IMAGE_BINDING_LAYER ||
        target == GL_IMAGE_BINDING_ACCESS || target == GL_IMAGE_BINDING_FORMAT) {
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetIntegeri_v(target, index, &image_value);
        if (ctx->errors.sequence == error_sequence_before)
            *data = (GLint64)image_value;
        return;
    }
    if (target == GL_BLEND || target == GL_COLOR_WRITEMASK || target == GL_BLEND_SRC_RGB ||
        target == GL_BLEND_DST_RGB || target == GL_BLEND_SRC_ALPHA ||
        target == GL_BLEND_DST_ALPHA || target == GL_BLEND_EQUATION_RGB ||
        target == GL_BLEND_EQUATION_ALPHA) {
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetIntegeri_v(target, index, indexed_values);
        if (ctx->errors.sequence == error_sequence_before)
            for (i = 0; i < mgl_indexed_query_component_count(target); ++i)
                data[i] = (GLint64)indexed_values[i];
        return;
    }
    if (index >= indexed_query_limit(ctx, target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    switch (target) {
        case GL_UNIFORM_BUFFER_BINDING:
            slot = &ctx->uniform_buffer_bindings[index];
            *data = (GLint64)slot->buffer;
            return;
        case GL_ATOMIC_COUNTER_BUFFER_BINDING:
            slot = &ctx->atomic_counter_buffer_bindings[index];
            *data = (GLint64)slot->buffer;
            return;
        case GL_ATOMIC_COUNTER_BUFFER_START:
            slot = &ctx->atomic_counter_buffer_bindings[index];
            *data = (GLint64)slot->offset;
            return;
        case GL_ATOMIC_COUNTER_BUFFER_SIZE:
            slot = &ctx->atomic_counter_buffer_bindings[index];
            *data = (GLint64)indexed_binding_effective_size(ctx, slot);
            return;
        case GL_SHADER_STORAGE_BUFFER_BINDING:
            slot = &ctx->shader_storage_buffer_bindings[index];
            *data = (GLint64)slot->buffer;
            return;
        case GL_SHADER_STORAGE_BUFFER_START:
            slot = &ctx->shader_storage_buffer_bindings[index];
            *data = (GLint64)slot->offset;
            return;
        case GL_SHADER_STORAGE_BUFFER_SIZE:
            slot = &ctx->shader_storage_buffer_bindings[index];
            *data = (GLint64)indexed_binding_effective_size(ctx, slot);
            return;
        case GL_UNIFORM_BUFFER_START:
            slot = &ctx->uniform_buffer_bindings[index];
            *data = (GLint64)slot->offset;
            return;
        case GL_UNIFORM_BUFFER_SIZE:
            slot = &ctx->uniform_buffer_bindings[index];
            *data = (GLint64)indexed_binding_effective_size(ctx, slot);
            return;
        case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
            slot = current_tf_indexed_binding(ctx, index);
            *data = (GLint64)slot->buffer;
            return;
        case GL_TRANSFORM_FEEDBACK_BUFFER_START:
            slot = current_tf_indexed_binding(ctx, index);
            *data = (GLint64)slot->offset;
            return;
        case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
            slot = current_tf_indexed_binding(ctx, index);
            *data = (GLint64)indexed_binding_effective_size(ctx, slot);
            return;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
    }
}

void glActiveTextureARB(GLenum texture) {
    glActiveTexture(texture);
}

void glClientActiveTextureARB(GLenum texture) {
    glClientActiveTexture(texture);
}

void glBlendColorEXT(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    glBlendColor(red, green, blue, alpha);
}

void glBlendEquationEXT(GLenum mode) {
    glBlendEquation(mode);
}

void glBlendEquationSeparateEXT(GLenum modeRGB, GLenum modeAlpha) {
    glBlendEquationSeparate(modeRGB, modeAlpha);
}

void glBlendFuncSeparateEXT(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void glSampleCoverageARB(GLfloat value, GLboolean invert) {
    glSampleCoverage(value, invert);
}

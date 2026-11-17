#ifndef MGL_FIXED_FUNCTION_H
#define MGL_FIXED_FUNCTION_H

#include "mgl/types.h"

#define MGL_MODELVIEW_STACK_DEPTH 32u
#define MGL_PROJECTION_STACK_DEPTH 2u
#define MGL_TEXTURE_STACK_DEPTH 2u
#define MGL_TEXTURE_UNITS 8u
#define MGL_PROGRAMMABLE_TEXTURE_CAPACITY 256u
#define MGL_MAX_LIGHTS 8u
#define MGL_MAX_CLIP_PLANES 6u
#define MGL_TEXGEN_COORDS 4u

struct mgl_light_state {
    GLboolean enabled;
    GLfloat ambient[4];
    GLfloat diffuse[4];
    GLfloat specular[4];
    GLfloat position[4];
    GLfloat spot_direction[3];
    GLfloat spot_exponent;
    GLfloat spot_cutoff;
    GLfloat constant_attenuation;
    GLfloat linear_attenuation;
    GLfloat quadratic_attenuation;
};

struct mgl_texgen_state {
    GLboolean enabled;
    GLenum mode;
    GLfloat object_plane[4];
    GLfloat eye_plane[4];
};

struct mgl_material_state {
    GLfloat ambient[4];
    GLfloat diffuse[4];
    GLfloat specular[4];
    GLfloat emission[4];
    GLfloat shininess;
};

struct mgl_matrix_stack {
    GLfloat *storage;
    unsigned depth;
    unsigned top;
};

/* Source and operand arrays follow the OpenGL 1.3 argument order 0, 1, 2. */
struct mgl_texenv_combine_state {
    GLenum rgb_mode;
    GLenum alpha_mode;
    GLenum rgb_source[3];
    GLenum alpha_source[3];
    GLenum rgb_operand[3];
    GLenum alpha_operand[3];
    GLfloat rgb_scale;
    GLfloat alpha_scale;
    GLfloat color[4];
};

struct mgl_compat_state {
    unsigned fixed_variant_key;
    GLboolean fixed_variant_key_valid;
    GLenum matrix_mode;
    GLuint active_texture;
    GLfloat color_matrix[4][16];
    unsigned color_matrix_top;
    GLfloat modelview[MGL_MODELVIEW_STACK_DEPTH][16];
    GLfloat projection[MGL_PROJECTION_STACK_DEPTH][16];
    GLfloat texture[MGL_TEXTURE_UNITS][MGL_TEXTURE_STACK_DEPTH][16];
    unsigned modelview_top;
    unsigned projection_top;
    unsigned texture_top[MGL_TEXTURE_UNITS];
    GLboolean texture_1d_enabled[MGL_TEXTURE_UNITS];
    GLboolean texture_2d_enabled[MGL_TEXTURE_UNITS];
    GLboolean texture_3d_enabled[MGL_TEXTURE_UNITS];
    GLboolean texture_cube_enabled[MGL_TEXTURE_UNITS];
    GLboolean texture_rectangle_enabled[MGL_TEXTURE_UNITS];
    GLenum texture_env_mode[MGL_TEXTURE_UNITS];
    GLfloat texture_lod_bias[MGL_TEXTURE_UNITS];
    struct mgl_texenv_combine_state texture_env_combine[MGL_TEXTURE_UNITS];
    struct mgl_texgen_state texgen[MGL_TEXTURE_UNITS][MGL_TEXGEN_COORDS];
    GLboolean clip_plane_enabled[MGL_MAX_CLIP_PLANES];
    GLfloat clip_plane[MGL_MAX_CLIP_PLANES][4];
    GLboolean fog_enabled;
    GLenum fog_mode;
    GLfloat fog_density;
    GLfloat fog_start;
    GLfloat fog_end;
    GLfloat fog_index;
    GLfloat fog_color[4];
    GLenum fog_coord_source;
    GLboolean lighting_enabled;
    GLboolean color_material_enabled;
    GLboolean color_sum_enabled;
    GLboolean normalize_enabled;
    GLboolean rescale_normal_enabled;
    GLboolean arb_vertex_program_enabled;
    GLboolean arb_fragment_program_enabled;
    GLenum color_material_face;
    GLenum color_material_mode;
    GLenum shade_model;
    GLfloat light_model_ambient[4];
    GLboolean light_model_local_viewer;
    GLboolean light_model_two_side;
    struct mgl_light_state lights[MGL_MAX_LIGHTS];
    struct mgl_material_state material_front;
    struct mgl_material_state material_back;
};

static inline GLenum mgl_compat_selected_texture_target(const struct mgl_compat_state *state,
                                                        unsigned unit) {
    if (unit >= MGL_TEXTURE_UNITS)
        return 0u;
    if (state->texture_cube_enabled[unit])
        return GL_TEXTURE_CUBE_MAP;
    if (state->texture_3d_enabled[unit])
        return GL_TEXTURE_3D;
    if (state->texture_rectangle_enabled[unit])
        return GL_TEXTURE_RECTANGLE;
    if (state->texture_2d_enabled[unit])
        return GL_TEXTURE_2D;
    if (state->texture_1d_enabled[unit])
        return GL_TEXTURE_1D;
    return 0u;
}

static inline unsigned mgl_compat_texture_signature(const struct mgl_compat_state *state) {
    unsigned unit, signature = 0u;
    for (unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit) {
        GLenum target = mgl_compat_selected_texture_target(state, unit);
        unsigned kind = target == GL_TEXTURE_1D         ? 1u
                        : target == GL_TEXTURE_3D       ? 2u
                        : target == GL_TEXTURE_CUBE_MAP ? 3u
                                                        : 0u;
        signature |= kind << (unit * 2u);
    }
    return signature;
}

struct mgl_context;
struct mgl_program;

void mgl_compat_state_init(struct mgl_compat_state *state);
const GLfloat *mgl_compat_modelview(const struct mgl_compat_state *state);
const GLfloat *mgl_compat_projection(const struct mgl_compat_state *state);
const GLfloat *mgl_compat_texture_matrix(const struct mgl_compat_state *state, unsigned unit);
void mgl_compat_mvp(const struct mgl_compat_state *state, GLfloat out[16]);
void mgl_compat_normal_matrix(const struct mgl_compat_state *state, GLfloat out[9]);
void mgl_compat_realize_program_uniforms(struct mgl_context *ctx, struct mgl_program *program);
struct mgl_program *mgl_compat_fixed_program(struct mgl_context *ctx);
void mgl_compat_destroy_fixed_program(struct mgl_context *ctx);

#endif

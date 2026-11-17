#ifndef MGL_STATE_H
#define MGL_STATE_H

#include "mgl/types.h"

#define MGL_PIXEL_MAP_MAX_SIZE 256u
#define MGL_PIXEL_MAP_COUNT 10u

struct mgl_context;

struct mgl_pixel_map_table {
    GLsizei size;
    GLdouble values[MGL_PIXEL_MAP_MAX_SIZE];
};

struct mgl_pixel_transfer_state {
    GLboolean map_color;
    GLboolean map_stencil;
    GLint index_shift;
    GLint index_offset;
    GLfloat red_scale, red_bias;
    GLfloat green_scale, green_bias;
    GLfloat blue_scale, blue_bias;
    GLfloat alpha_scale, alpha_bias;
    GLfloat depth_scale, depth_bias;
    GLfloat pixel_zoom_x, pixel_zoom_y;
    struct mgl_pixel_map_table maps[MGL_PIXEL_MAP_COUNT];
};

struct mgl_draw_buffer_state {
    GLboolean blend;
    GLboolean color_mask[4];
    GLenum blend_src_rgb;
    GLenum blend_dst_rgb;
    GLenum blend_src_alpha;
    GLenum blend_dst_alpha;
    GLenum blend_equation_rgb;
    GLenum blend_equation_alpha;
    GLfloat blend_color[4];
    GLfloat line_width;
    GLfloat polygon_offset_factor;
    GLfloat polygon_offset_units;
    GLfloat sample_coverage_value;
    GLboolean sample_coverage_invert;
};

struct mgl_stencil_face_state {
    GLenum func;
    GLint ref;
    GLuint value_mask;
    GLuint write_mask;
    GLenum sfail;
    GLenum dpfail;
    GLenum dppass;
};

struct mgl_raster_state {
    GLint viewport[4];
    GLint scissor[4];
    GLfloat indexed_viewports[16][4];
    GLint indexed_scissors[16][4];
    GLdouble indexed_depth_ranges[16][2];
    GLfloat clear_color[4];
    GLfloat clear_accum[4];
    GLfloat clear_index;
    GLuint index_writemask;
    GLfloat clear_depth;
    GLfloat depth_range[2];
    GLenum clamp_vertex_color;
    GLenum clamp_fragment_color;
    GLenum clamp_read_color;
    GLenum clip_origin;
    GLenum clip_depth_mode;
    GLclampd depth_bounds[2];
    GLboolean depth_bounds_test;
    GLint patch_vertices;
    GLfloat patch_inner_levels[2];
    GLfloat patch_outer_levels[4];
    GLfloat min_sample_shading;
    GLboolean sample_shading;
    GLfloat polygon_offset_clamp;
    GLint clear_stencil;
    GLboolean alpha_test;
    GLenum alpha_func;
    GLfloat alpha_ref;
    GLboolean depth_test;
    GLboolean stencil_test;
    GLboolean stencil_two_side_test;
    GLenum active_stencil_face;
    struct mgl_stencil_face_state stencil_front;
    struct mgl_stencil_face_state stencil_back;
    struct mgl_stencil_face_state stencil_back_ext;
    GLboolean blend;
    GLboolean cull_face;
    GLboolean scissor_test;
    GLboolean dither;
    GLboolean multisample;
    GLboolean line_smooth;
    GLboolean line_stipple;
    GLboolean polygon_stipple;
    GLint line_stipple_factor;
    GLushort line_stipple_pattern;
    GLubyte polygon_stipple_mask[128];
    GLboolean framebuffer_srgb;
    GLboolean texture_cube_map_seamless;
    GLboolean polygon_offset_fill;
    GLboolean polygon_offset_point;
    GLboolean polygon_offset_line;
    GLboolean sample_alpha_to_coverage;
    GLboolean sample_coverage;
    GLboolean sample_mask;
    GLbitfield sample_mask_value;
    GLboolean primitive_restart;
    GLenum depth_func;
    GLboolean depth_mask;
    GLboolean color_mask[4];
    GLenum cull_face_mode;
    GLenum front_face;
    GLenum polygon_mode[2];
    GLenum blend_src_rgb;
    GLenum blend_dst_rgb;
    GLenum blend_src_alpha;
    GLenum blend_dst_alpha;
    GLenum blend_equation_rgb;
    GLenum blend_equation_alpha;
    GLfloat blend_color[4];
    GLfloat line_width;
    GLfloat point_size;
    GLfloat point_size_min;
    GLfloat point_size_max;
    GLfloat point_fade_threshold_size;
    GLfloat point_distance_attenuation[3];
    GLenum logic_op_mode;
    GLboolean color_logic_op;
    GLfloat polygon_offset_factor;
    GLfloat polygon_offset_units;
    GLfloat sample_coverage_value;
    GLboolean sample_coverage_invert;
    GLenum perspective_correction_hint;
    GLenum point_smooth_hint;
    GLenum point_sprite_coord_origin;
    GLboolean point_sprite_enabled;
    GLboolean program_point_size_enabled;
    GLenum line_smooth_hint;
    GLenum polygon_smooth_hint;
    GLenum fog_hint;
    GLenum generate_mipmap_hint;
    GLenum texture_compression_hint;
    GLenum fragment_shader_derivative_hint;
    struct mgl_draw_buffer_state draw_buffers[MGL_MAX_DRAW_BUFFERS];
};

void mgl_state_init(struct mgl_raster_state *state, GLsizei width, GLsizei height);
void mgl_pixel_transfer_init(struct mgl_pixel_transfer_state *state);
int mgl_pixel_map_set_canonical(struct mgl_context *ctx, GLenum map, GLsizei mapsize,
                                const GLdouble *values);
GLboolean *mgl_state_cap_ptr(struct mgl_raster_state *state, GLenum cap);
int mgl_query_component_count(GLenum pname);

int mgl_copy_server_context_state(struct mgl_context *dst, const struct mgl_context *src,
                                  GLbitfield mask);
void mgl_realize_copied_server_state(struct mgl_context *ctx);

#endif

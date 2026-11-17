#include "state/state.h"
#include <string.h>

void mgl_state_init(struct mgl_raster_state *s, GLsizei width, GLsizei height) {
    memset(s, 0, sizeof(*s));
    for (unsigned i = 0; i < 16u; ++i) {
        s->indexed_viewports[i][2] = (GLfloat)width;
        s->indexed_viewports[i][3] = (GLfloat)height;
        s->indexed_scissors[i][2] = width;
        s->indexed_scissors[i][3] = height;
        s->indexed_depth_ranges[i][1] = 1.0;
    }
    s->viewport[2] = width;
    s->viewport[3] = height;
    s->scissor[2] = width;
    s->scissor[3] = height;
    s->clear_index = 0.0f;
    s->index_writemask = UINT32_MAX;
    s->clear_depth = 1.0f;
    s->depth_range[0] = 0.0f;
    s->depth_range[1] = 1.0f;
    s->clamp_vertex_color = GL_TRUE;
    s->clamp_fragment_color = GL_FIXED_ONLY;
    s->clamp_read_color = GL_FIXED_ONLY;
    s->clip_origin = GL_LOWER_LEFT;
    s->clip_depth_mode = GL_NEGATIVE_ONE_TO_ONE;
    s->depth_bounds[0] = 0.0;
    s->depth_bounds[1] = 1.0;
    s->patch_vertices = 3;
    s->patch_inner_levels[0] = s->patch_inner_levels[1] = 1.0f;
    for (unsigned i = 0; i < 4u; ++i)
        s->patch_outer_levels[i] = 1.0f;
    s->min_sample_shading = 0.0f;
    s->alpha_func = GL_ALWAYS;
    s->alpha_ref = 0.0f;
    s->depth_func = GL_LESS;
    s->active_stencil_face = GL_FRONT;
    s->stencil_front.func = GL_ALWAYS;
    s->stencil_back.func = GL_ALWAYS;
    s->stencil_back_ext.func = GL_ALWAYS;
    s->stencil_front.value_mask = s->stencil_back.value_mask = s->stencil_back_ext.value_mask =
        UINT32_MAX;
    s->stencil_front.write_mask = s->stencil_back.write_mask = s->stencil_back_ext.write_mask =
        UINT32_MAX;
    s->stencil_front.sfail = s->stencil_front.dpfail = s->stencil_front.dppass = GL_KEEP;
    s->stencil_back.sfail = s->stencil_back.dpfail = s->stencil_back.dppass = GL_KEEP;
    s->stencil_back_ext.sfail = s->stencil_back_ext.dpfail = s->stencil_back_ext.dppass = GL_KEEP;
    s->depth_mask = GL_TRUE;
    s->color_mask[0] = GL_TRUE;
    s->color_mask[1] = GL_TRUE;
    s->color_mask[2] = GL_TRUE;
    s->color_mask[3] = GL_TRUE;
    s->cull_face_mode = GL_BACK;
    s->front_face = GL_CCW;
    s->polygon_mode[0] = GL_FILL;
    s->polygon_mode[1] = GL_FILL;
    s->blend_src_rgb = GL_ONE;
    s->blend_dst_rgb = GL_ZERO;
    s->blend_src_alpha = GL_ONE;
    s->blend_dst_alpha = GL_ZERO;
    s->blend_equation_rgb = GL_FUNC_ADD;
    s->blend_equation_alpha = GL_FUNC_ADD;
    s->line_width = 1.0f;
    s->line_stipple_factor = 1;
    s->line_stipple_pattern = 0xffffu;
    memset(s->polygon_stipple_mask, 0xff, sizeof(s->polygon_stipple_mask));
    s->point_size = 1.0f;
    s->point_size_min = 0.0f;
    s->point_size_max = 1.0f;
    s->point_fade_threshold_size = 1.0f;
    s->point_distance_attenuation[0] = 1.0f;
    s->logic_op_mode = GL_COPY;
    s->sample_mask_value = UINT32_MAX;
    s->sample_coverage_value = 1.0f;
    s->sample_coverage_invert = GL_FALSE;
    s->perspective_correction_hint = GL_DONT_CARE;
    s->point_smooth_hint = GL_DONT_CARE;
    s->point_sprite_coord_origin = GL_UPPER_LEFT;
    s->line_smooth_hint = GL_DONT_CARE;
    s->polygon_smooth_hint = GL_DONT_CARE;
    s->fog_hint = GL_DONT_CARE;
    s->generate_mipmap_hint = GL_DONT_CARE;
    s->texture_compression_hint = GL_DONT_CARE;
    s->fragment_shader_derivative_hint = GL_DONT_CARE;
    {
        unsigned i;
        for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i) {
            s->draw_buffers[i].blend = GL_FALSE;
            s->draw_buffers[i].color_mask[0] = GL_TRUE;
            s->draw_buffers[i].color_mask[1] = GL_TRUE;
            s->draw_buffers[i].color_mask[2] = GL_TRUE;
            s->draw_buffers[i].color_mask[3] = GL_TRUE;
            s->draw_buffers[i].blend_src_rgb = GL_ONE;
            s->draw_buffers[i].blend_dst_rgb = GL_ZERO;
            s->draw_buffers[i].blend_src_alpha = GL_ONE;
            s->draw_buffers[i].blend_dst_alpha = GL_ZERO;
            s->draw_buffers[i].blend_equation_rgb = GL_FUNC_ADD;
            s->draw_buffers[i].blend_equation_alpha = GL_FUNC_ADD;
        }
    }
    s->dither = GL_TRUE;
    s->multisample = GL_TRUE;
}

GLboolean *mgl_state_cap_ptr(struct mgl_raster_state *s, GLenum cap) {
    switch (cap) {
        case GL_POINT_SPRITE:
            return &s->point_sprite_enabled;
        case GL_PROGRAM_POINT_SIZE:
            return &s->program_point_size_enabled;
        case GL_ALPHA_TEST:
            return &s->alpha_test;
        case GL_DEPTH_TEST:
            return &s->depth_test;
        case GL_DEPTH_BOUNDS_TEST_EXT:
            return &s->depth_bounds_test;
        case GL_STENCIL_TEST:
            return &s->stencil_test;
        case GL_STENCIL_TEST_TWO_SIDE_EXT:
            return &s->stencil_two_side_test;
        case GL_BLEND:
            return &s->blend;
        case GL_COLOR_LOGIC_OP:
            return &s->color_logic_op;
        case GL_CULL_FACE:
            return &s->cull_face;
        case GL_SCISSOR_TEST:
            return &s->scissor_test;
        case GL_DITHER:
            return &s->dither;
        case GL_LINE_SMOOTH:
            return &s->line_smooth;
        case GL_LINE_STIPPLE:
            return &s->line_stipple;
        case GL_POLYGON_STIPPLE:
            return &s->polygon_stipple;
        case GL_FRAMEBUFFER_SRGB:
            return &s->framebuffer_srgb;
        case GL_TEXTURE_CUBE_MAP_SEAMLESS:
            return &s->texture_cube_map_seamless;
        case GL_POLYGON_OFFSET_FILL:
            return &s->polygon_offset_fill;
        case GL_POLYGON_OFFSET_POINT:
            return &s->polygon_offset_point;
        case GL_POLYGON_OFFSET_LINE:
            return &s->polygon_offset_line;
        case GL_MULTISAMPLE:
            return &s->multisample;
        case GL_SAMPLE_ALPHA_TO_COVERAGE:
            return &s->sample_alpha_to_coverage;
        case GL_SAMPLE_COVERAGE:
            return &s->sample_coverage;
        case GL_SAMPLE_MASK:
            return &s->sample_mask;
        case GL_SAMPLE_SHADING:
            return &s->sample_shading;
        case GL_PRIMITIVE_RESTART:
            return &s->primitive_restart;
        default:
            return NULL;
    }
}

void mgl_pixel_transfer_init(struct mgl_pixel_transfer_state *s) {
    memset(s, 0, sizeof(*s));
    s->red_scale = 1.0f;
    s->green_scale = 1.0f;
    s->blue_scale = 1.0f;
    s->alpha_scale = 1.0f;
    s->depth_scale = 1.0f;
    s->pixel_zoom_x = 1.0f;
    s->pixel_zoom_y = 1.0f;
    {
        unsigned i;
        for (i = 0u; i < MGL_PIXEL_MAP_COUNT; ++i) {
            s->maps[i].size = 1;
            s->maps[i].values[0] = 0.0;
        }
    }
}

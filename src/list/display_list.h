#ifndef MGL_DISPLAY_LIST_H
#define MGL_DISPLAY_LIST_H

#include "mgl/types.h"

struct mgl_context;
struct mgl_share_group;

struct mgl_display_list;
struct mgl_draw_snapshot;

int mgl_list_record_active_texture(struct mgl_context *ctx, GLenum texture);
int mgl_list_record_matrix_mode(struct mgl_context *ctx, GLenum mode);
int mgl_list_record_load_identity(struct mgl_context *ctx);
int mgl_list_record_load_matrixf(struct mgl_context *ctx, const GLfloat *matrix);
int mgl_list_record_mult_matrixf(struct mgl_context *ctx, const GLfloat *matrix);
int mgl_list_record_push_matrix(struct mgl_context *ctx);
int mgl_list_record_pop_matrix(struct mgl_context *ctx);
int mgl_list_record_matrix_load_ext(struct mgl_context *ctx, GLenum matrix_mode,
                                    const GLfloat *matrix);
int mgl_list_record_matrix_mult_ext(struct mgl_context *ctx, GLenum matrix_mode,
                                    const GLfloat *matrix);
int mgl_list_record_matrix_load_identity_ext(struct mgl_context *ctx, GLenum matrix_mode);
int mgl_list_record_matrix_push_ext(struct mgl_context *ctx, GLenum matrix_mode);
int mgl_list_record_matrix_pop_ext(struct mgl_context *ctx, GLenum matrix_mode);
int mgl_list_record_color4f(struct mgl_context *ctx, GLfloat r, GLfloat g, GLfloat b, GLfloat a);
int mgl_list_record_secondary3f(struct mgl_context *ctx, GLfloat r, GLfloat g, GLfloat b);
int mgl_list_record_normal3f(struct mgl_context *ctx, GLfloat x, GLfloat y, GLfloat z);
int mgl_list_record_texcoord4f(struct mgl_context *ctx, unsigned unit, GLfloat s, GLfloat t,
                               GLfloat r, GLfloat q);
int mgl_list_record_fog_coord(struct mgl_context *ctx, GLfloat coord);
int mgl_list_record_enable(struct mgl_context *ctx, GLenum cap, GLboolean enabled);
int mgl_list_record_push_attrib(struct mgl_context *ctx, GLbitfield mask);
int mgl_list_record_pop_attrib(struct mgl_context *ctx);
int mgl_list_record_pixel_transfer(struct mgl_context *ctx, GLenum pname, GLfloat param);
int mgl_list_record_pixel_map(struct mgl_context *ctx, GLenum map, GLsizei mapsize,
                              const GLdouble *values);
int mgl_list_record_alpha_func(struct mgl_context *ctx, GLenum func, GLfloat ref);
int mgl_list_record_clear_index(struct mgl_context *ctx, GLfloat index);
int mgl_list_record_clear_color(struct mgl_context *ctx, GLfloat red, GLfloat green, GLfloat blue,
                                GLfloat alpha);
int mgl_list_record_clear_depth(struct mgl_context *ctx, GLfloat depth);
int mgl_list_record_clear_stencil(struct mgl_context *ctx, GLint stencil);
int mgl_list_record_clear(struct mgl_context *ctx, GLbitfield mask);
int mgl_list_record_viewport(struct mgl_context *ctx, GLint x, GLint y, GLsizei width,
                             GLsizei height);
int mgl_list_record_scissor(struct mgl_context *ctx, GLint x, GLint y, GLsizei width,
                            GLsizei height);
int mgl_list_record_depth_func(struct mgl_context *ctx, GLenum func);
int mgl_list_record_depth_mask(struct mgl_context *ctx, GLboolean flag);
int mgl_list_record_index_mask(struct mgl_context *ctx, GLuint mask);
int mgl_list_record_color_mask(struct mgl_context *ctx, GLboolean red, GLboolean green,
                               GLboolean blue, GLboolean alpha);
int mgl_list_record_cull_face(struct mgl_context *ctx, GLenum mode);
int mgl_list_record_front_face(struct mgl_context *ctx, GLenum mode);
int mgl_list_record_fog(struct mgl_context *ctx, GLenum pname, const GLfloat *params,
                        unsigned count);
int mgl_list_record_light(struct mgl_context *ctx, GLenum light, GLenum pname,
                          const GLfloat *params, unsigned count);
int mgl_list_record_light_model(struct mgl_context *ctx, GLenum pname, const GLfloat *params,
                                unsigned count);
int mgl_list_record_material(struct mgl_context *ctx, GLenum face, GLenum pname,
                             const GLfloat *params, unsigned count);
int mgl_list_record_color_material(struct mgl_context *ctx, GLenum face, GLenum mode);
int mgl_list_record_shade_model(struct mgl_context *ctx, GLenum mode);
int mgl_list_record_tex_env(struct mgl_context *ctx, GLenum target, GLenum pname,
                            const GLfloat *params);
int mgl_list_record_multi_tex_env_ext(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                      GLenum pname, const GLfloat *params);
int mgl_list_record_texgen(struct mgl_context *ctx, GLenum coord, GLenum pname,
                           const GLfloat *params, unsigned count);
int mgl_list_record_multi_texgen_ext(struct mgl_context *ctx, GLenum texunit, GLenum coord,
                                     GLenum pname, const GLfloat *params, unsigned count);
int mgl_list_record_program_uniform_f_ext(struct mgl_context *ctx, GLuint program, GLint location,
                                          unsigned components, GLsizei count, const GLfloat *values,
                                          GLboolean scalar);
int mgl_list_record_program_uniform_i_ext(struct mgl_context *ctx, GLuint program, GLint location,
                                          unsigned components, GLsizei count, const GLint *values,
                                          GLboolean scalar);
int mgl_list_record_program_uniform_matrix_ext(struct mgl_context *ctx, GLuint program,
                                               GLint location, unsigned dimension, GLsizei count,
                                               GLboolean transpose, const GLfloat *values);
int mgl_list_record_arb_program_parameters(struct mgl_context *ctx, GLenum target, GLuint index,
                                           GLsizei count, const GLfloat *values, GLboolean local);
int mgl_list_record_arb_program_string(struct mgl_context *ctx, GLenum target, GLenum format,
                                       GLsizei len, const void *string);
int mgl_list_record_clip_plane(struct mgl_context *ctx, GLenum plane, const GLfloat *equation);
int mgl_list_record_stencil_func(struct mgl_context *ctx, GLenum face, GLenum func, GLint ref,
                                 GLuint mask);
int mgl_list_record_stencil_mask(struct mgl_context *ctx, GLenum face, GLuint mask);
int mgl_list_record_stencil_op(struct mgl_context *ctx, GLenum face, GLenum sfail, GLenum dpfail,
                               GLenum dppass);
int mgl_list_record_blend_func_separate(struct mgl_context *ctx, GLenum src_rgb, GLenum dst_rgb,
                                        GLenum src_alpha, GLenum dst_alpha);
int mgl_list_record_blend_equation_separate(struct mgl_context *ctx, GLenum rgb, GLenum alpha);
int mgl_list_record_depth_range(struct mgl_context *ctx, GLfloat near_value, GLfloat far_value);
int mgl_list_record_blend_color(struct mgl_context *ctx, GLfloat red, GLfloat green, GLfloat blue,
                                GLfloat alpha);
int mgl_list_record_line_width(struct mgl_context *ctx, GLfloat width);
int mgl_list_record_polygon_offset(struct mgl_context *ctx, GLfloat factor, GLfloat units);
int mgl_list_record_sample_coverage(struct mgl_context *ctx, GLfloat value, GLboolean invert);
int mgl_list_record_hint(struct mgl_context *ctx, GLenum target, GLenum mode);
int mgl_list_record_set_fence_nv(struct mgl_context *ctx, GLuint name, GLenum condition);
int mgl_list_record_enable_indexed(struct mgl_context *ctx, GLenum cap, GLuint index,
                                   GLboolean enabled, GLboolean ext_semantics);
int mgl_list_record_color_mask_indexed(struct mgl_context *ctx, GLuint index, GLboolean red,
                                       GLboolean green, GLboolean blue, GLboolean alpha);
int mgl_list_record_blend_func_indexed(struct mgl_context *ctx, GLuint index, GLenum src_rgb,
                                       GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha);
int mgl_list_record_blend_equation_indexed(struct mgl_context *ctx, GLuint index, GLenum rgb,
                                           GLenum alpha);
int mgl_list_record_clear_buffer_iv(struct mgl_context *ctx, GLenum buffer, GLint drawbuffer,
                                    const GLint *value, unsigned count);
int mgl_list_record_clear_buffer_uiv(struct mgl_context *ctx, GLenum buffer, GLint drawbuffer,
                                     const GLuint *value, unsigned count);
int mgl_list_record_clear_buffer_fv(struct mgl_context *ctx, GLenum buffer, GLint drawbuffer,
                                    const GLfloat *value, unsigned count);
int mgl_list_record_clear_buffer_fi(struct mgl_context *ctx, GLenum buffer, GLint drawbuffer,
                                    GLfloat depth, GLint stencil);
int mgl_list_record_draw_buffer(struct mgl_context *ctx, GLenum buffer);
int mgl_list_record_draw_buffers(struct mgl_context *ctx, GLsizei count, const GLenum *buffers);
int mgl_list_record_read_buffer(struct mgl_context *ctx, GLenum buffer);
int mgl_list_record_map1(struct mgl_context *ctx, GLenum target, GLdouble u1, GLdouble u2,
                         GLint order, unsigned components, const GLdouble *coeff);
int mgl_list_record_map2(struct mgl_context *ctx, GLenum target, GLdouble u1, GLdouble u2,
                         GLdouble v1, GLdouble v2, GLint uorder, GLint vorder, unsigned components,
                         const GLdouble *coeff);
int mgl_list_record_eval_coord1(struct mgl_context *ctx, GLdouble u);
int mgl_list_record_eval_coord2(struct mgl_context *ctx, GLdouble u, GLdouble v);
int mgl_list_record_map_grid1(struct mgl_context *ctx, GLint un, GLdouble u1, GLdouble u2);
int mgl_list_record_map_grid2(struct mgl_context *ctx, GLint un, GLdouble u1, GLdouble u2, GLint vn,
                              GLdouble v1, GLdouble v2);
int mgl_list_record_eval_point1(struct mgl_context *ctx, GLint i);
int mgl_list_record_eval_point2(struct mgl_context *ctx, GLint i, GLint j);
int mgl_list_record_eval_mesh1(struct mgl_context *ctx, GLenum mode, GLint i1, GLint i2);
int mgl_list_record_eval_mesh2(struct mgl_context *ctx, GLenum mode, GLint i1, GLint i2, GLint j1,
                               GLint j2);
int mgl_list_record_raster_pos4d(struct mgl_context *ctx, GLdouble x, GLdouble y, GLdouble z,
                                 GLdouble w);
int mgl_list_record_window_pos3f(struct mgl_context *ctx, GLfloat x, GLfloat y, GLfloat z);
int mgl_list_record_pixel_zoom(struct mgl_context *ctx, GLfloat xfactor, GLfloat yfactor);
int mgl_list_record_draw_pixels(struct mgl_context *ctx, GLsizei width, GLsizei height,
                                GLenum format, GLenum type, const void *data, size_t data_size);
int mgl_list_record_bitmap(struct mgl_context *ctx, GLsizei width, GLsizei height, GLfloat xorig,
                           GLfloat yorig, GLfloat xmove, GLfloat ymove, const void *data,
                           size_t data_size);
int mgl_list_record_copy_pixels(struct mgl_context *ctx, GLint x, GLint y, GLsizei width,
                                GLsizei height, GLenum type);
int mgl_list_record_begin(struct mgl_context *ctx, GLenum mode);
int mgl_list_record_vertex4f(struct mgl_context *ctx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
int mgl_list_record_end(struct mgl_context *ctx);
int mgl_list_record_bind_texture(struct mgl_context *ctx, GLenum target, GLuint texture);
int mgl_list_record_bind_multi_texture(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                       GLuint texture);
int mgl_list_record_multi_tex_buffer(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                     GLenum internalformat, GLuint buffer);
int mgl_list_record_texture_buffer_ext(struct mgl_context *ctx, GLuint texture, GLenum target,
                                       GLenum internalformat, GLuint buffer);
int mgl_list_record_prioritize_textures(struct mgl_context *ctx, GLsizei n, const GLuint *textures,
                                        const GLfloat *priorities);
int mgl_list_record_tex_parameter(struct mgl_context *ctx, GLenum target, GLenum pname,
                                  GLint param);
int mgl_list_record_tex_parameter_f(struct mgl_context *ctx, GLenum target, GLenum pname,
                                    GLfloat param);
int mgl_list_record_tex_parameter_iv4(struct mgl_context *ctx, GLenum target, GLenum pname,
                                      const GLint *params);
int mgl_list_record_tex_parameter_fv4(struct mgl_context *ctx, GLenum target, GLenum pname,
                                      const GLfloat *params);
int mgl_list_record_tex_parameter_iiv(struct mgl_context *ctx, GLenum target, GLenum pname,
                                      const GLint *params);
int mgl_list_record_tex_parameter_iuiv(struct mgl_context *ctx, GLenum target, GLenum pname,
                                       const GLuint *params);
int mgl_list_record_multi_tex_parameter(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                        GLenum pname, GLint param);
int mgl_list_record_multi_tex_parameter_f(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                          GLenum pname, GLfloat param);
int mgl_list_record_multi_tex_parameter_iv4(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                            GLenum pname, const GLint *params);
int mgl_list_record_multi_tex_parameter_fv4(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                            GLenum pname, const GLfloat *params);
int mgl_list_record_multi_tex_parameter_iiv(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                            GLenum pname, const GLint *params);
int mgl_list_record_multi_tex_parameter_iuiv(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                             GLenum pname, const GLuint *params);
int mgl_list_record_texture_parameter_ext(struct mgl_context *ctx, GLuint texture, GLenum target,
                                          GLenum pname, GLint param);
int mgl_list_record_texture_parameter_ext_f(struct mgl_context *ctx, GLuint texture, GLenum target,
                                            GLenum pname, GLfloat param);
int mgl_list_record_texture_parameter_ext_iv4(struct mgl_context *ctx, GLuint texture,
                                              GLenum target, GLenum pname, const GLint *params);
int mgl_list_record_texture_parameter_ext_fv4(struct mgl_context *ctx, GLuint texture,
                                              GLenum target, GLenum pname, const GLfloat *params);
int mgl_list_record_texture_parameter_ext_iiv(struct mgl_context *ctx, GLuint texture,
                                              GLenum target, GLenum pname, const GLint *params);
int mgl_list_record_texture_parameter_ext_iuiv(struct mgl_context *ctx, GLuint texture,
                                               GLenum target, GLenum pname, const GLuint *params);
int mgl_list_record_multi_tex_image_2d(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                       GLint level, GLint internalformat, GLsizei width,
                                       GLsizei height, GLint border, GLenum format, GLenum type,
                                       const void *data, size_t data_size);
int mgl_list_record_multi_tex_sub_image_2d(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                           GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                                           GLsizei height, GLenum format, GLenum type,
                                           const void *data, size_t data_size);
int mgl_list_record_multi_tex_image_3d(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                       GLint level, GLint internalformat, GLsizei width,
                                       GLsizei height, GLsizei depth, GLint border, GLenum format,
                                       GLenum type, const void *data, size_t data_size);
int mgl_list_record_multi_tex_sub_image_3d(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                           GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                           GLsizei width, GLsizei height, GLsizei depth,
                                           GLenum format, GLenum type, const void *data,
                                           size_t data_size);
int mgl_list_record_compressed_multi_tex_image_2d(struct mgl_context *ctx, GLenum texunit,
                                                  GLenum target, GLint level, GLenum internalformat,
                                                  GLsizei width, GLsizei height, GLint border,
                                                  const void *data, size_t data_size);
int mgl_list_record_compressed_multi_tex_sub_image_2d(struct mgl_context *ctx, GLenum texunit,
                                                      GLenum target, GLint level, GLint xoffset,
                                                      GLint yoffset, GLsizei width, GLsizei height,
                                                      GLenum format, const void *data,
                                                      size_t data_size);
int mgl_list_record_compressed_multi_tex_image_3d(struct mgl_context *ctx, GLenum texunit,
                                                  GLenum target, GLint level, GLenum internalformat,
                                                  GLsizei width, GLsizei height, GLsizei depth,
                                                  GLint border, const void *data, size_t data_size);
int mgl_list_record_compressed_multi_tex_sub_image_3d(struct mgl_context *ctx, GLenum texunit,
                                                      GLenum target, GLint level, GLint xoffset,
                                                      GLint yoffset, GLint zoffset, GLsizei width,
                                                      GLsizei height, GLsizei depth, GLenum format,
                                                      const void *data, size_t data_size);
int mgl_list_record_tex_image_2d(struct mgl_context *ctx, GLenum target, GLint level,
                                 GLint internalformat, GLsizei width, GLsizei height, GLint border,
                                 GLenum format, GLenum type, const void *data, size_t data_size);
int mgl_list_record_tex_sub_image_2d(struct mgl_context *ctx, GLenum target, GLint level,
                                     GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                     GLenum format, GLenum type, const void *data,
                                     size_t data_size);
int mgl_list_record_tex_image_3d(struct mgl_context *ctx, GLenum target, GLint level,
                                 GLint internalformat, GLsizei width, GLsizei height, GLsizei depth,
                                 GLint border, GLenum format, GLenum type, const void *data,
                                 size_t data_size);
int mgl_list_record_tex_sub_image_3d(struct mgl_context *ctx, GLenum target, GLint level,
                                     GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,
                                     GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                     const void *data, size_t data_size);
int mgl_list_record_compressed_tex_image_2d(struct mgl_context *ctx, GLenum target, GLint level,
                                            GLenum internalformat, GLsizei width, GLsizei height,
                                            GLint border, const void *data, size_t data_size);
int mgl_list_record_compressed_tex_image_3d(struct mgl_context *ctx, GLenum target, GLint level,
                                            GLenum internalformat, GLsizei width, GLsizei height,
                                            GLsizei depth, GLint border, const void *data,
                                            size_t data_size);
int mgl_list_record_compressed_tex_sub_image_2d(struct mgl_context *ctx, GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset, GLsizei width,
                                                GLsizei height, GLenum format, const void *data,
                                                size_t data_size);
int mgl_list_record_compressed_tex_sub_image_3d(struct mgl_context *ctx, GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset, GLint zoffset,
                                                GLsizei width, GLsizei height, GLsizei depth,
                                                GLenum format, const void *data, size_t data_size);
int mgl_list_record_copy_tex_image_2d(struct mgl_context *ctx, GLenum target, GLint level,
                                      GLenum internalformat, GLint x, GLint y, GLsizei width,
                                      GLsizei height, GLint border);
int mgl_list_record_copy_tex_sub_image_2d(struct mgl_context *ctx, GLenum target, GLint level,
                                          GLint xoffset, GLint yoffset, GLint x, GLint y,
                                          GLsizei width, GLsizei height);
int mgl_list_record_copy_tex_sub_image_3d(struct mgl_context *ctx, GLenum target, GLint level,
                                          GLint xoffset, GLint yoffset, GLint zoffset, GLint x,
                                          GLint y, GLsizei width, GLsizei height);
int mgl_list_record_compressed_texture_image_2d_ext(struct mgl_context *ctx, GLuint texture,
                                                    GLenum target, GLint level,
                                                    GLenum internalformat, GLsizei width,
                                                    GLsizei height, GLint border, const void *data,
                                                    size_t data_size);
int mgl_list_record_compressed_texture_sub_image_2d_ext(struct mgl_context *ctx, GLuint texture,
                                                        GLenum target, GLint level, GLint xoffset,
                                                        GLint yoffset, GLsizei width,
                                                        GLsizei height, GLenum format,
                                                        const void *data, size_t data_size);
int mgl_list_record_compressed_texture_image_3d_ext(struct mgl_context *ctx, GLuint texture,
                                                    GLenum target, GLint level,
                                                    GLenum internalformat, GLsizei width,
                                                    GLsizei height, GLsizei depth, GLint border,
                                                    const void *data, size_t data_size);
int mgl_list_record_compressed_texture_sub_image_3d_ext(struct mgl_context *ctx, GLuint texture,
                                                        GLenum target, GLint level, GLint xoffset,
                                                        GLint yoffset, GLint zoffset, GLsizei width,
                                                        GLsizei height, GLsizei depth,
                                                        GLenum format, const void *data,
                                                        size_t data_size);
int mgl_list_record_texture_image_2d_ext(struct mgl_context *ctx, GLuint texture, GLenum target,
                                         GLint level, GLint internalformat, GLsizei width,
                                         GLsizei height, GLint border, GLenum format, GLenum type,
                                         const void *data, size_t data_size);
int mgl_list_record_texture_sub_image_2d_ext(struct mgl_context *ctx, GLuint texture, GLenum target,
                                             GLint level, GLint xoffset, GLint yoffset,
                                             GLsizei width, GLsizei height, GLenum format,
                                             GLenum type, const void *data, size_t data_size);
int mgl_list_record_texture_image_3d_ext(struct mgl_context *ctx, GLuint texture, GLenum target,
                                         GLint level, GLint internalformat, GLsizei width,
                                         GLsizei height, GLsizei depth, GLint border, GLenum format,
                                         GLenum type, const void *data, size_t data_size);
int mgl_list_record_texture_sub_image_3d_ext(struct mgl_context *ctx, GLuint texture, GLenum target,
                                             GLint level, GLint xoffset, GLint yoffset,
                                             GLint zoffset, GLsizei width, GLsizei height,
                                             GLsizei depth, GLenum format, GLenum type,
                                             const void *data, size_t data_size);
int mgl_list_record_copy_texture_image_2d_ext(struct mgl_context *ctx, GLuint texture,
                                              GLenum target, GLint level, GLenum internalformat,
                                              GLint x, GLint y, GLsizei width, GLsizei height,
                                              GLint border);
int mgl_list_record_copy_texture_sub_image_2d_ext(struct mgl_context *ctx, GLuint texture,
                                                  GLenum target, GLint level, GLint xoffset,
                                                  GLint yoffset, GLint x, GLint y, GLsizei width,
                                                  GLsizei height);
int mgl_list_record_copy_texture_sub_image_3d_ext(struct mgl_context *ctx, GLuint texture,
                                                  GLenum target, GLint level, GLint xoffset,
                                                  GLint yoffset, GLint zoffset, GLint x, GLint y,
                                                  GLsizei width, GLsizei height);
int mgl_list_record_copy_multi_tex_image_2d(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                            GLint level, GLenum internalformat, GLint x, GLint y,
                                            GLsizei width, GLsizei height, GLint border);
int mgl_list_record_copy_multi_tex_sub_image_2d(struct mgl_context *ctx, GLenum texunit,
                                                GLenum target, GLint level, GLint xoffset,
                                                GLint yoffset, GLint x, GLint y, GLsizei width,
                                                GLsizei height);
int mgl_list_record_copy_multi_tex_sub_image_3d(struct mgl_context *ctx, GLenum texunit,
                                                GLenum target, GLint level, GLint xoffset,
                                                GLint yoffset, GLint zoffset, GLint x, GLint y,
                                                GLsizei width, GLsizei height);
int mgl_list_record_init_names(struct mgl_context *ctx);
int mgl_list_record_push_name(struct mgl_context *ctx, GLuint name);
int mgl_list_record_pop_name(struct mgl_context *ctx);
int mgl_list_record_load_name(struct mgl_context *ctx, GLuint name);
int mgl_list_record_draw_snapshot(struct mgl_context *ctx, struct mgl_draw_snapshot *snapshot);
void mgl_display_list_abort_compile(struct mgl_context *ctx);
void mgl_display_list_free_all(struct mgl_share_group *group);

#endif

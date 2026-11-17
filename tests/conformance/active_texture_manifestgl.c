#include "mgl/gl.h"
#include "core/context.h"
#include "compat/fixed_function.h"
#include "objects/object_store.h"
#include "shader/translator.h"
#include "conformance/active_texture_trace.h"
#include "conformance/bind_texture_trace.h"

#include "conformance/tex_parameter_trace.h"
#include "conformance/tex_parameter_variants_trace.h"

#include "conformance/tex_parameter_integer_trace.h"

#include "conformance/tex_parameter_getters_trace.h"

#include "conformance/tex_level_query_trace.h"
#include "conformance/tex_image_readback_trace.h"
#include "conformance/tex_upload_trace.h"
#include "conformance/tex_compressed_copy_trace.h"
#include "conformance/tex_storage_mipmap_readback_trace.h"
#include "conformance/tex_buffer_attachment_trace.h"
#include "conformance/sampler_lifecycle_trace.h"
#include "conformance/buffer_object_trace.h"
#include "conformance/core_buffer_semantics_trace.h"
#include "conformance/buffer_map_flush_trace.h"
#include "conformance/framebuffer_renderbuffer_trace.h"
#include "conformance/enable_state_trace.h"
#include "conformance/query_object_trace.h"
#include "conformance/sync_object_trace.h"
#include "conformance/transform_feedback_trace.h"
#include "conformance/shader_program_trace.h"
#include "conformance/vertex_attrib_state_trace.h"
#include "conformance/vertex_current_batch_trace.h"
#include "conformance/object_lifecycle_batch_trace.h"
#include "conformance/matrix_family_batch_trace.h"
#include "conformance/scalar_uniform_batch_trace.h"
#include "conformance/immediate_current_batch_trace.h"
#include "conformance/extension_multitex_batch_trace.h"
#include "conformance/fixed_point_batch_trace.h"
#include "conformance/texture_dsa_parameter_batch_trace.h"
#include "conformance/indexed_family_batch_trace.h"
#include "conformance/arb_program_compat_batch_trace.h"
#include "conformance/client_array_state_trace.h"
#include "conformance/draw_call_trace.h"
#include "conformance/modern_state_trace.h"
#include "conformance/indexed_state_getters_trace.h"
#include "conformance/legacy_state_trace.h"
#include "conformance/fixed_function_state_trace.h"
#include "conformance/fog_state_trace.h"
#include "conformance/light_state_trace.h"
#include "conformance/evaluator_state_trace.h"
#include "conformance/selection_state_trace.h"
#include "conformance/raster_operation_trace.h"
#include "conformance/dsa_texture_state_trace.h"
#include "conformance/depth_stencil_state_trace.h"
#include "conformance/dsa_texture_transfer_trace.h"

#include "conformance/object_list_batch_trace.h"
#include "conformance/framebuffer_family_batch_trace.h"
#include "conformance/texture_transfer_bx_trace.h"
#include "conformance/debug_and_compat_by_trace.h"
#include "conformance/indexed_output_vao_bz_trace.h"
#include "conformance/core_texture_dsa_ca_trace.h"
#include "conformance/multitex_alias_cb_trace.h"
#include "conformance/vao_ownership_cc_trace.h"
#include "conformance/compat_remainder_cd_trace.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static unsigned capability_enable_calls, capability_disable_calls;
static unsigned capability_enable_indexed_calls, capability_disable_indexed_calls;

static void mock_cap_enable(GLenum cap) {
    (void)cap;
    ++capability_enable_calls;
}

static void mock_cap_disable(GLenum cap) {
    (void)cap;
    ++capability_disable_calls;
}

static void mock_cap_enablei(GLenum cap, GLuint index) {
    (void)cap;
    (void)index;
    ++capability_enable_indexed_calls;
}

static void mock_cap_disablei(GLenum cap, GLuint index) {
    (void)cap;
    (void)index;
    ++capability_disable_indexed_calls;
}

static unsigned ds_clear_calls, ds_scissor_calls, ds_stencil_calls, ds_depth_range_calls;

static void mock_ds_clear(GLbitfield mask) {
    (void)mask;
    ++ds_clear_calls;
}

static void mock_ds_scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    ++ds_scissor_calls;
}

static void mock_ds_stencil(GLenum face, GLenum func, GLint ref, GLuint mask) {
    (void)face;
    (void)func;
    (void)ref;
    (void)mask;
    ++ds_stencil_calls;
}

static void mock_ds_depth_range(GLfloat near_value, GLfloat far_value) {
    (void)near_value;
    (void)far_value;
    ++ds_depth_range_calls;
}

static unsigned image2_calls, image3_calls, sub2_calls, sub3_calls, pixel_store_calls;

static unsigned storage2_calls, storage3_calls, mipmap_calls;

static void mock_storage2(GLenum target, GLsizei levels, GLenum internal, GLsizei w, GLsizei h) {
    (void)target;
    (void)levels;
    (void)internal;
    (void)w;
    (void)h;
    ++storage2_calls;
}

static unsigned ca_multisample2_calls, ca_multisample3_calls;

static void ca_storage2_ms(GLenum target, GLsizei samples, GLenum format, GLsizei width,
                           GLsizei height, GLboolean fixed) {
    (void)target;
    (void)samples;
    (void)format;
    (void)width;
    (void)height;
    (void)fixed;
    ++ca_multisample2_calls;
}

static void ca_storage3_ms(GLenum target, GLsizei samples, GLenum format, GLsizei width,
                           GLsizei height, GLsizei depth, GLboolean fixed) {
    (void)target;
    (void)samples;
    (void)format;
    (void)width;
    (void)height;
    (void)depth;
    (void)fixed;
    ++ca_multisample3_calls;
}

static void mock_storage3(GLenum target, GLsizei levels, GLenum internal, GLsizei w, GLsizei h,
                          GLsizei d) {
    (void)target;
    (void)levels;
    (void)internal;
    (void)w;
    (void)h;
    (void)d;
    ++storage3_calls;
}

static void mock_mipmap(GLenum target) {
    (void)target;
    ++mipmap_calls;
}

static void mock_upload_image_2d(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h,
                                 GLint border, GLenum format, GLenum type, const void *data) {
    (void)target;
    (void)level;
    (void)internal;
    (void)w;
    (void)h;
    (void)border;
    (void)format;
    (void)type;
    (void)data;
    ++image2_calls;
}

static void mock_upload_sub_2d(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h,
                               GLenum format, GLenum type, const void *data) {
    (void)target;
    (void)level;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)format;
    (void)type;
    (void)data;
    ++sub2_calls;
}

static void mock_pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
    ++pixel_store_calls;
}

static void mock_upload_image_3d(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h,
                                 GLsizei d, GLint border, GLenum format, GLenum type,
                                 const void *data) {
    (void)target;
    (void)level;
    (void)internal;
    (void)w;
    (void)h;
    (void)d;
    (void)border;
    (void)format;
    (void)type;
    (void)data;
    ++image3_calls;
}

static void mock_upload_sub_3d(GLenum target, GLint level, GLint x, GLint y, GLint z, GLsizei w,
                               GLsizei h, GLsizei d, GLenum format, GLenum type, const void *data) {
    (void)target;
    (void)level;
    (void)x;
    (void)y;
    (void)z;
    (void)w;
    (void)h;
    (void)d;
    (void)format;
    (void)type;
    (void)data;
    ++sub3_calls;
}

static unsigned mock_indexed_mask_calls, mock_indexed_blend_func_calls, mock_indexed_blend_eq_calls;

static void mock_indexed_mask(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    (void)index;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
    ++mock_indexed_mask_calls;
}

static void mock_indexed_blend_func(GLuint index, GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha,
                                    GLenum dst_alpha) {
    (void)index;
    (void)src_rgb;
    (void)dst_rgb;
    (void)src_alpha;
    (void)dst_alpha;
    ++mock_indexed_blend_func_calls;
}

static void mock_indexed_blend_eq(GLuint index, GLenum rgb, GLenum alpha) {
    (void)index;
    (void)rgb;
    (void)alpha;
    ++mock_indexed_blend_eq_calls;
}

static unsigned uniform_invalid_native_calls;

static void mock_invalid_uniform(GLint location, GLfloat value) {
    (void)location;
    (void)value;
    ++uniform_invalid_native_calls;
}

static void mock_invalid_program_uniform(GLuint program, GLint location, GLfloat value) {
    (void)program;
    (void)location;
    (void)value;
    ++uniform_invalid_native_calls;
}

static unsigned mock_image_bind_calls, mock_barrier_calls, mock_region_barrier_calls;

static void mock_bind_image(GLuint unit, GLuint texture, GLint level, GLboolean layered,
                            GLint layer, GLenum access, GLenum format) {
    (void)unit;
    (void)texture;
    (void)level;
    (void)layered;
    (void)layer;
    (void)access;
    (void)format;
    ++mock_image_bind_calls;
}

static void mock_barrier(GLbitfield bits) {
    (void)bits;
    ++mock_barrier_calls;
}

static void mock_region_barrier(GLbitfield bits) {
    (void)bits;
    ++mock_region_barrier_calls;
}

static unsigned calls;
static unsigned binds;
static unsigned tex_parameter_calls;
static unsigned tex_float_calls;

static unsigned tex_signed_calls;
static unsigned tex_unsigned_calls;

static void mock_tex_parameter_iiv(GLenum target, GLenum pname, const GLint *params) {
    (void)target;
    (void)pname;
    (void)params;
    ++tex_signed_calls;
}

static void mock_tex_parameter_iuiv(GLenum target, GLenum pname, const GLuint *params) {
    (void)target;
    (void)pname;
    (void)params;
    ++tex_unsigned_calls;
}

static GLuint mock_buffer_name = 2000u;

static void mock_gen_buffers(GLsizei count, GLuint *out) {
    GLsizei i;
    for (i = 0; i < count; ++i)
        out[i] = ++mock_buffer_name;
}

static void mock_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static unsigned indexed_base_calls, indexed_range_calls, named_buffer_data_calls,
    named_buffer_subdata_calls;

static void mock_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)target;
    (void)size;
    (void)data;
    (void)usage;
    ++named_buffer_data_calls;
}

static void mock_buffer_subdata(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    (void)target;
    (void)offset;
    (void)size;
    (void)data;
    ++named_buffer_subdata_calls;
}

static void mock_buffer_base(GLenum target, GLuint index, GLuint buffer) {
    (void)target;
    (void)index;
    (void)buffer;
    ++indexed_base_calls;
}

static void mock_buffer_range(GLenum target, GLuint index, GLuint buffer, GLintptr off,
                              GLsizeiptr size) {
    (void)target;
    (void)index;
    (void)buffer;
    (void)off;
    (void)size;
    ++indexed_range_calls;
}

static const GLubyte *mock_extensions(GLenum name) {
    return name == GL_EXTENSIONS ? (const GLubyte *)"GL_EXT_texture_buffer" : (const GLubyte *)"";
}

static void mock_get_integer(GLenum name, GLint *value) {
    *value = name == GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT ? 256 : name == GL_MAX_IMAGE_UNITS ? 8 : 0;
}

static unsigned buffer_full_calls, buffer_range_calls;

static void mock_tex_buffer(GLenum target, GLenum format, GLuint buffer) {
    (void)target;
    (void)format;
    (void)buffer;
    ++buffer_full_calls;
}

static void mock_tex_buffer_range(GLenum target, GLenum format, GLuint buffer, GLintptr off,
                                  GLsizeiptr size) {
    (void)target;
    (void)format;
    (void)buffer;
    (void)off;
    (void)size;
    ++buffer_range_calls;
}

static GLuint next_native_sampler = 4000u;
static unsigned sampler_gen_calls, sampler_delete_calls, sampler_bind_calls, sampler_param_calls;

static void mock_sampler_gen(GLsizei n, GLuint *out) {
    GLsizei i;
    ++sampler_gen_calls;
    for (i = 0; i < n; ++i)
        out[i] = ++next_native_sampler;
}

static void mock_sampler_delete(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
    ++sampler_delete_calls;
}

static void mock_sampler_bind(GLuint unit, GLuint name) {
    (void)unit;
    (void)name;
    ++sampler_bind_calls;
}

static void mock_sampler_param(GLuint sampler, GLenum pname, GLint value) {
    (void)sampler;
    (void)pname;
    (void)value;
    ++sampler_param_calls;
}

static void mock_sampler_paramf(GLuint sampler, GLenum pname, GLfloat value) {
    (void)sampler;
    (void)pname;
    (void)value;
    ++sampler_param_calls;
}

static GLuint generated = 1000u;

static void mock_gen_textures(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++generated;
}

static void mock_bind_texture(GLenum target, GLuint name) {
    (void)target;
    (void)name;
    ++binds;
}

static void mock_tex_parameter(GLenum target, GLenum pname, GLint param) {
    (void)target;
    (void)pname;
    (void)param;
    ++tex_parameter_calls;
}

static void mock_tex_parameterf(GLenum target, GLenum pname, GLfloat param) {
    (void)target;
    (void)pname;
    (void)param;
    ++tex_float_calls;
}

static GLenum last_unit;

static void mock_active_texture(GLenum texture) {
    ++calls;
    last_unit = texture;
}

static GLuint mock_native_object_name = 8000u;
static unsigned mock_rb_storage_calls, mock_fb_attachment_calls, mock_blit_calls;

static void mock_gen_objects(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++mock_native_object_name;
}

static void mock_bind_framebuffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void mock_bind_renderbuffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static GLenum mock_check_framebuffer(GLenum target) {
    (void)target;
    return GL_FRAMEBUFFER_COMPLETE;
}

static void mock_renderbuffer_storage(GLenum target, GLenum format, GLsizei w, GLsizei h) {
    (void)target;
    (void)format;
    (void)w;
    (void)h;
    ++mock_rb_storage_calls;
}

static void mock_renderbuffer_storage_multisample(GLenum target, GLsizei samples, GLenum format,
                                                  GLsizei w, GLsizei h) {
    (void)target;
    (void)samples;
    (void)format;
    (void)w;
    (void)h;
    ++mock_rb_storage_calls;
}

static void mock_framebuffer_renderbuffer(GLenum target, GLenum attachment, GLenum rbtarget,
                                          GLuint name) {
    (void)target;
    (void)attachment;
    (void)rbtarget;
    (void)name;
    ++mock_fb_attachment_calls;
}

static void mock_blit_framebuffer(GLint a, GLint b, GLint c, GLint d, GLint e, GLint f, GLint g,
                                  GLint h, GLbitfield mask, GLenum filter) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    (void)g;
    (void)h;
    (void)mask;
    (void)filter;
    ++mock_blit_calls;
}

static unsigned mock_clear_calls;

static void mock_clear_iv(GLenum target, GLint drawbuffer, const GLint *value) {
    (void)target;
    (void)drawbuffer;
    (void)value;
    ++mock_clear_calls;
}

static void mock_clear_uiv(GLenum target, GLint drawbuffer, const GLuint *value) {
    (void)target;
    (void)drawbuffer;
    (void)value;
    ++mock_clear_calls;
}

static void mock_clear_fv(GLenum target, GLint drawbuffer, const GLfloat *value) {
    (void)target;
    (void)drawbuffer;
    (void)value;
    ++mock_clear_calls;
}

static void mock_clear_fi(GLenum target, GLint drawbuffer, GLfloat depth, GLint stencil) {
    (void)target;
    (void)drawbuffer;
    (void)depth;
    (void)stencil;
    ++mock_clear_calls;
}

static GLuint mock_next_shader_program = 9000u;
static unsigned mock_shader_attach_calls, mock_shader_detach_calls;

static GLuint mock_create_shader(GLenum kind) {
    (void)kind;
    return ++mock_next_shader_program;
}

static GLuint mock_create_program(void) {
    return ++mock_next_shader_program;
}

static void mock_attach_shader(GLuint prog, GLuint shader) {
    (void)prog;
    (void)shader;
    ++mock_shader_attach_calls;
}

static void mock_detach_shader(GLuint prog, GLuint shader) {
    (void)prog;
    (void)shader;
    ++mock_shader_detach_calls;
}

static void mock_delete_shader(GLuint shader) {
    (void)shader;
}

static void mock_delete_program(GLuint prog) {
    (void)prog;
}

#include "../conformance/program_uniform_matrix_trace.h"
#include "../conformance/program_reflection_trace.h"
#include "../conformance/uniform_matrix_trace.h"

static unsigned reflection_native_calls;

static void mock_get_active_uniform(GLuint program, GLuint index, GLsizei bufsize, GLsizei *length,
                                    GLint *size, GLenum *type, GLchar *name) {
    const char *uniform_name = "u23";
    size_t n = strlen(uniform_name);
    (void)program;
    (void)index;
    ++reflection_native_calls;
    if (size != NULL)
        *size = 1;
    if (type != NULL)
        *type = 0x8B65u;
    if (length != NULL)
        *length = bufsize > 0 ? (GLsizei)n : 0;
    if (name != NULL && bufsize > 0) {
        if (n >= (size_t)bufsize)
            n = (size_t)bufsize - 1u;
        memcpy(name, uniform_name, n);
        name[n] = '\0';
        if (length != NULL)
            *length = (GLsizei)n;
    }
}

static GLuint matrix_host_program = 990u;
static struct mgl_context *matrix_host_context;
static GLfloat matrix_host_values[10][16];
static unsigned matrix_native_calls;

static void matrix_native_23(GLuint program, GLint location, GLsizei count, GLboolean transpose,
                             const GLfloat *values) {
    (void)program;
    (void)location;
    if (count != 1 || transpose != GL_FALSE)
        return;
    ++matrix_native_calls;
    memcpy(matrix_host_values[0], values, 6u * sizeof(GLfloat));
}

static void matrix_native_43(GLuint program, GLint location, GLsizei count, GLboolean transpose,
                             const GLfloat *values) {
    (void)program;
    (void)location;
    if (count != 2 || transpose != GL_FALSE)
        return;
    ++matrix_native_calls;
    memcpy(matrix_host_values[1], values, 12u * sizeof(GLfloat));
    memcpy(matrix_host_values[2], values + 12, 12u * sizeof(GLfloat));
}

static void matrix_native_44(GLuint program, GLint location, GLsizei count, GLboolean transpose,
                             const GLfloat *values) {
    (void)program;
    (void)location;
    if (count != 1 || transpose != GL_FALSE)
        return;
    ++matrix_native_calls;
    memcpy(matrix_host_values[3], values, 16u * sizeof(GLfloat));
}

static void matrix_native_22(GLuint program, GLint location, GLsizei count, GLboolean transpose,
                             const GLfloat *values) {
    (void)program;
    (void)location;
    if (count != 1 || transpose != GL_FALSE)
        return;
    ++matrix_native_calls;
    memcpy(matrix_host_values[4], values, 4u * sizeof(GLfloat));
}

static void matrix_native_33(GLuint program, GLint location, GLsizei count, GLboolean transpose,
                             const GLfloat *values) {
    (void)program;
    (void)location;
    if (count != 1 || transpose != GL_FALSE)
        return;
    ++matrix_native_calls;
    memcpy(matrix_host_values[5], values, 9u * sizeof(GLfloat));
}

static void matrix_native_32(GLuint program, GLint location, GLsizei count, GLboolean transpose,
                             const GLfloat *values) {
    (void)program;
    (void)location;
    if (count != 1 || transpose != GL_FALSE)
        return;
    ++matrix_native_calls;
    memcpy(matrix_host_values[6], values, 6u * sizeof(GLfloat));
}

static void matrix_native_24(GLuint program, GLint location, GLsizei count, GLboolean transpose,
                             const GLfloat *values) {
    (void)program;
    (void)location;
    if (count != 1 || transpose != GL_FALSE)
        return;
    ++matrix_native_calls;
    memcpy(matrix_host_values[7], values, 8u * sizeof(GLfloat));
}

static void matrix_native_42(GLuint program, GLint location, GLsizei count, GLboolean transpose,
                             const GLfloat *values) {
    (void)program;
    (void)location;
    if (count != 1 || transpose != GL_FALSE)
        return;
    ++matrix_native_calls;
    memcpy(matrix_host_values[8], values, 8u * sizeof(GLfloat));
}

static void matrix_native_34(GLuint program, GLint location, GLsizei count, GLboolean transpose,
                             const GLfloat *values) {
    (void)program;
    (void)location;
    if (count != 1 || transpose != GL_FALSE)
        return;
    ++matrix_native_calls;
    memcpy(matrix_host_values[9], values, 12u * sizeof(GLfloat));
}

static void mock_use_program(GLuint program) {
    (void)program;
}

static unsigned rejected_draw_native_calls;

static void mock_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    (void)mode;
    (void)first;
    (void)count;
    ++rejected_draw_native_calls;
}

static void mock_draw_elements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    (void)mode;
    (void)count;
    (void)type;
    (void)indices;
    ++rejected_draw_native_calls;
}

static void mock_uniform_matrix_2(GLint location, GLsizei count, GLboolean transpose,
                                  const GLfloat *value) {
    matrix_native_22(0u, location, count, transpose, value);
}

static void mock_uniform_matrix_3(GLint location, GLsizei count, GLboolean transpose,
                                  const GLfloat *value) {
    matrix_native_33(0u, location, count, transpose, value);
}

static void mock_uniform_matrix_4(GLint location, GLsizei count, GLboolean transpose,
                                  const GLfloat *value) {
    matrix_native_44(0u, location, count, transpose, value);
}

static void mock_uniform_matrix_2x3(GLint location, GLsizei count, GLboolean transpose,
                                    const GLfloat *value) {
    matrix_native_23(0u, location, count, transpose, value);
}

static void mock_uniform_matrix_3x2(GLint location, GLsizei count, GLboolean transpose,
                                    const GLfloat *value) {
    matrix_native_32(0u, location, count, transpose, value);
}

static void mock_uniform_matrix_2x4(GLint location, GLsizei count, GLboolean transpose,
                                    const GLfloat *value) {
    matrix_native_24(0u, location, count, transpose, value);
}

static void mock_uniform_matrix_4x2(GLint location, GLsizei count, GLboolean transpose,
                                    const GLfloat *value) {
    matrix_native_42(0u, location, count, transpose, value);
}

static void mock_uniform_matrix_3x4(GLint location, GLsizei count, GLboolean transpose,
                                    const GLfloat *value) {
    matrix_native_34(0u, location, count, transpose, value);
}

static void mock_uniform_matrix_4x3(GLint location, GLsizei count, GLboolean transpose,
                                    const GLfloat *value) {
    matrix_native_43(0u, location, count, transpose, value);
}

static GLuint matrix_trace_prepare(void) {
    struct mgl_program *p = calloc(1u, sizeof(*p));
    if (p == NULL)
        return 0u;
    p->name = matrix_host_program;
    p->native_name = 999u;
    p->linked = GL_TRUE;
    p->next = matrix_host_context->share_group->programs;
    matrix_host_context->share_group->programs = p;
    return matrix_host_program;
}

static GLint matrix_trace_location(int index) {
    return 100 + index;
}

static void matrix_trace_fetch(int index, GLfloat *values) {
    memcpy(values, matrix_host_values[index], 16u * sizeof(GLfloat));
}

static void matrix_trace_finish(GLuint program) {
    struct mgl_program *p = matrix_host_context->share_group->programs;
    if (program == matrix_host_program && p != NULL) {
        matrix_host_context->share_group->programs = p->next;
        free(p);
    }
}

static void by_test_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                   GLsizei length, const GLchar *message, const void *userParam) {
    (void)source;
    (void)type;
    (void)id;
    (void)severity;
    (void)length;
    (void)message;
    (void)userParam;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesEnable = mock_cap_enable;
    native.glesDisable = mock_cap_disable;
    native.glesEnablei = mock_cap_enablei;
    native.glesDisablei = mock_cap_disablei;
    native.glesGetString = mock_extensions;
    native.glesGetIntegerv = mock_get_integer;
    native.glesColorMaski = mock_indexed_mask;
    native.glesBlendFuncSeparatei = mock_indexed_blend_func;
    native.glesBlendEquationSeparatei = mock_indexed_blend_eq;
    native.glesBindImageTexture = mock_bind_image;
    native.glesMemoryBarrier = mock_barrier;
    native.glesMemoryBarrierByRegion = mock_region_barrier;
    native.glesGenBuffers = mock_gen_buffers;
    native.glesBindBuffer = mock_bind_buffer;
    native.glesBufferData = mock_buffer_data;
    native.glesBufferSubData = mock_buffer_subdata;
    native.glesBindBufferBase = mock_buffer_base;
    native.glesBindBufferRange = mock_buffer_range;
    native.glesTexBufferEXT = mock_tex_buffer;
    native.glesTexBufferRangeEXT = mock_tex_buffer_range;
    native.glesGenSamplers = mock_sampler_gen;
    native.glesDeleteSamplers = mock_sampler_delete;
    native.glesBindSampler = mock_sampler_bind;
    native.glesSamplerParameteri = mock_sampler_param;
    native.glesSamplerParameterf = mock_sampler_paramf;
    native.glesActiveTexture = mock_active_texture;
    native.glesTexImage2D = mock_upload_image_2d;
    native.glesTexSubImage2D = mock_upload_sub_2d;
    native.glesTexImage3D = mock_upload_image_3d;
    native.glesTexSubImage3D = mock_upload_sub_3d;
    native.glesTexStorage2D = mock_storage2;
    native.glesTexStorage3D = mock_storage3;
    native.glesTexStorage2DMultisample = ca_storage2_ms;
    native.glesTexStorage3DMultisample = ca_storage3_ms;
    native.glesGenerateMipmap = mock_mipmap;
    native.glesPixelStorei = mock_pixel_store;
    native.glesGenTextures = mock_gen_textures;
    native.glesBindTexture = mock_bind_texture;
    native.glesTexParameteri = mock_tex_parameter;
    native.glesTexParameterf = mock_tex_parameterf;
    native.glesTexParameterIiv = mock_tex_parameter_iiv;
    native.glesTexParameterIuiv = mock_tex_parameter_iuiv;
    native.glesGenFramebuffers = mock_gen_objects;
    native.glesBindFramebuffer = mock_bind_framebuffer;
    native.glesCheckFramebufferStatus = mock_check_framebuffer;
    native.glesFramebufferRenderbuffer = mock_framebuffer_renderbuffer;
    native.glesBlitFramebuffer = mock_blit_framebuffer;
    native.glesClear = mock_ds_clear;
    native.glesScissor = mock_ds_scissor;
    native.glesStencilFuncSeparate = mock_ds_stencil;
    native.glesDepthRangef = mock_ds_depth_range;
    native.glesClearBufferiv = mock_clear_iv;
    native.glesClearBufferuiv = mock_clear_uiv;
    native.glesClearBufferfv = mock_clear_fv;
    native.glesClearBufferfi = mock_clear_fi;
    native.glesGenRenderbuffers = mock_gen_objects;
    native.glesBindRenderbuffer = mock_bind_renderbuffer;
    native.glesRenderbufferStorage = mock_renderbuffer_storage;
    native.glesRenderbufferStorageMultisample = mock_renderbuffer_storage_multisample;
    native.glesUseProgram = mock_use_program;
    native.glesUniform1f = mock_invalid_uniform;
    native.glesProgramUniform1f = mock_invalid_program_uniform;
    native.glesDrawArrays = mock_draw_arrays;
    native.glesDrawElements = mock_draw_elements;
    native.glesProgramUniformMatrix2fv = matrix_native_22;
    native.glesProgramUniformMatrix3fv = matrix_native_33;
    native.glesProgramUniformMatrix3x2fv = matrix_native_32;
    native.glesProgramUniformMatrix2x4fv = matrix_native_24;
    native.glesProgramUniformMatrix4x2fv = matrix_native_42;
    native.glesProgramUniformMatrix3x4fv = matrix_native_34;
    native.glesProgramUniformMatrix2x3fv = matrix_native_23;
    native.glesProgramUniformMatrix4x3fv = matrix_native_43;
    native.glesProgramUniformMatrix4fv = matrix_native_44;
    native.glesUniformMatrix2fv = mock_uniform_matrix_2;
    native.glesUniformMatrix3fv = mock_uniform_matrix_3;
    native.glesUniformMatrix4fv = mock_uniform_matrix_4;
    native.glesUniformMatrix2x3fv = mock_uniform_matrix_2x3;
    native.glesUniformMatrix3x2fv = mock_uniform_matrix_3x2;
    native.glesUniformMatrix2x4fv = mock_uniform_matrix_2x4;
    native.glesUniformMatrix4x2fv = mock_uniform_matrix_4x2;
    native.glesUniformMatrix3x4fv = mock_uniform_matrix_3x4;
    native.glesUniformMatrix4x3fv = mock_uniform_matrix_4x3;
    native.glesCreateShader = mock_create_shader;
    native.glesCreateProgram = mock_create_program;
    native.glesGetActiveUniform = mock_get_active_uniform;
    native.glesGetActiveAttrib = mock_get_active_uniform;
    native.glesAttachShader = mock_attach_shader;
    native.glesDetachShader = mock_detach_shader;
    native.glesDeleteShader = mock_delete_shader;
    native.glesDeleteProgram = mock_delete_program;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 2;
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 64, 64);

    ctx.unpack_alignment = 4;
    ctx.pack_alignment = 4;
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.next_query_name = 1u;
    mgl_context_set_current_for_test(&ctx);
    matrix_host_context = &ctx;

    active_texture_trace();

    if (last_unit != GL_TEXTURE0 + ctx.compat.active_texture || calls == 0u) {
        fputs("active texture conformance: native selector desynchronized\n", stderr);
        return 3;
    }

    {
        unsigned before = calls;
        unsigned selected = ctx.compat.active_texture;
        glBegin(GL_POINTS);
        glActiveTexture(GL_TEXTURE2);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || calls != before ||
            ctx.compat.active_texture != selected) {
            fputs("active texture conformance: illegal Begin/End reached GLES or changed state\n",
                  stderr);
            return 4;
        }
        glActiveTexture(0u);
        if (glGetError() != GL_INVALID_ENUM || calls != before ||
            ctx.compat.active_texture != selected) {
            fputs("active texture conformance: invalid enum reached GLES or changed state\n",
                  stderr);
            return 5;
        }
    }
    bind_texture_trace();
    /* A rejected bind cannot allocate a native
     * texture or issue a GLES binding, even when the texture name is new. */
    {
        unsigned before_binds = binds;
        GLuint before_generated = generated;
        GLint bound = -1;
        glActiveTexture(GL_TEXTURE1);
        glBegin(GL_POINTS);
        glBindTexture(GL_TEXTURE_2D, 9001u);
        glEnd();
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        if (glGetError() != GL_INVALID_OPERATION || bound != 0 || binds != before_binds ||
            generated != before_generated) {
            fputs("bind texture conformance: illegal Begin/End mutated object or native state\n",
                  stderr);
            return 6;
        }
    }
    tex_parameter_trace();
    /* An illegal parameter update must reach neither
     * the texture object nor the GLES callback, even with an active binding. */
    {
        unsigned before = tex_parameter_calls;
        GLint wrap = -1;
        glActiveTexture(GL_TEXTURE1);
        glBegin(GL_POINTS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glEnd();
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap);
        if (glGetError() != GL_INVALID_OPERATION || wrap != GL_CLAMP_TO_EDGE ||
            tex_parameter_calls != before) {
            fputs("texture parameter conformance: illegal Begin/End changed texture or native "
                  "state\n",
                  stderr);
            return 7;
        }
    }
    tex_parameter_variants_trace();
    /* Rejected scalar and vector updates preserve texture parameters and issue no GLES calls. */
    {
        const GLfloat changed_border[4] = {0.75f, 0.75f, 0.75f, 0.75f};
        const GLint changed_integer_border[4] = {0, 0, 0, 0};
        GLfloat lod = -1.0f;
        GLfloat border[4] = {0};
        unsigned before_float = tex_float_calls;
        unsigned before_int = tex_parameter_calls;
        glBegin(GL_POINTS);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 12.0f);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, changed_border);
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, changed_integer_border);
        glEnd();
        glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, &lod);
        glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        if (glGetError() != GL_INVALID_OPERATION || lod != 5.5f || border[0] != 0.125f ||
            border[1] != 0.25f || border[2] != 0.375f || border[3] != 0.5f ||
            tex_float_calls != before_float || tex_parameter_calls != before_int) {
            fputs("texture parameter variants conformance: illegal Begin/End mutated interface or "
                  "GLES state\n",
                  stderr);
            return 8;
        }
    }
    tex_parameter_integer_trace();
    /* Illegal integer setters must not affect
     * observable border state or call the GLES integer-border functions. */
    {
        const GLint invalid_signed[4] = {101, 102, 103, 104};
        const GLuint invalid_unsigned[4] = {201u, 202u, 203u, 204u};
        GLint border[4] = {0, 0, 0, 0};
        unsigned previous_signed = tex_signed_calls;
        unsigned previous_unsigned = tex_unsigned_calls;
        glBegin(GL_POINTS);
        glTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, invalid_signed);
        glTexParameterIuiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, invalid_unsigned);
        glEnd();
        glGetTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        if (glGetError() != GL_INVALID_OPERATION || border[0] != 11 || border[1] != 13 ||
            border[2] != 17 || border[3] != 19 || tex_signed_calls != previous_signed ||
            tex_unsigned_calls != previous_unsigned) {
            fputs("integer texture parameter conformance: illegal Begin/End mutated state or GLES "
                  "native\n",
                  stderr);
            return 9;
        }
    }
    tex_parameter_getters_trace();
    tex_level_query_trace();
    tex_image_readback_trace();
    tex_upload_trace();
    tex_compressed_copy_trace();
    tex_storage_mipmap_readback_trace();
    tex_buffer_attachment_trace();
    sampler_lifecycle_trace();
    buffer_object_trace();
    core_buffer_semantics_trace();
    buffer_map_flush_trace();
    {
        unsigned old_storage = mock_rb_storage_calls;
        unsigned old_attach = mock_fb_attachment_calls;
        unsigned old_blit = mock_blit_calls;
        unsigned old_clear = mock_clear_calls;
        framebuffer_trace_trace();
        /* Only the initial storage, attachment, and later legal storage reach GLES. */
        if (mock_rb_storage_calls != old_storage + 2u ||
            mock_fb_attachment_calls != old_attach + 1u || mock_blit_calls != old_blit ||
            mock_clear_calls != old_clear) {
            fputs("framebuffer conformance: rejected operation reached GLES\n", stderr);
            return 90;
        }
    }
    enable_state_trace();
    query_object_trace();
    sync_object_trace();
    transform_feedback_trace_trace();
    {
        unsigned attaches = mock_shader_attach_calls;
        unsigned detaches = mock_shader_detach_calls;
        shader_program_trace_trace();
        if (mock_shader_attach_calls != attaches + 1u ||
            mock_shader_detach_calls != detaches + 1u) {
            fputs("shader conformance: invalid Begin/End operation reached GLES\n", stderr);
            return 97;
        }
    }
    if (!matrix_trace_trace())
        return 98;
    {
        unsigned before = reflection_native_calls;
        if (!reflection_trace())
            return 99;
        if (reflection_native_calls != before + 2u) {
            fputs("reflection conformance: rejected lookup reached GLES native\n", stderr);
            return 99;
        }
    }
    if (matrix_native_calls != 10u) {
        fputs("matrix conformance: unexpected native uploads\n", stderr);
        return 98;
    }
    if (!uniform_matrix_trace_trace())
        return 100;
    if (matrix_native_calls != 20u) {
        fputs("core matrix conformance: wrong GLES upload count\n", stderr);
        return 100;
    }
    vertex_attrib_state_trace();
    vertex_current_batch_trace();
    client_array_state_trace();
    draw_call_trace_trace();
    {
        unsigned image_before = mock_image_bind_calls;
        unsigned barrier_before = mock_barrier_calls;
        unsigned region_before = mock_region_barrier_calls;
        modern_state_trace();
        {
            unsigned mask_before = mock_indexed_mask_calls;
            unsigned func_before = mock_indexed_blend_func_calls;
            unsigned eq_before = mock_indexed_blend_eq_calls;
            indexed_state_getters_trace();
            legacy_state_trace();
            fixed_function_state_trace();
            fog_state_trace();
            light_state_trace();
            evaluator_state_trace();
            selection_state_trace();
            raster_operation_trace();
            dsa_texture_state_trace();
            /* Rejected DSA state calls must not emit GLES work or mutate texture state. */
            {
                GLuint named = 0;
                GLint v = 0, sentinel = -515;
                GLfloat priority = .2f;
                GLint border[4] = {1, 2, 3, 4};
                unsigned b0, t0, f0, i0, u0, a0;
                glCreateTextures(GL_TEXTURE_2D, 1, &named);
                glTextureParameteri(named, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                if (glGetError() != GL_NO_ERROR)
                    return 116;
                b0 = binds;
                t0 = tex_parameter_calls;
                f0 = tex_float_calls;
                i0 = tex_signed_calls;
                u0 = tex_unsigned_calls;
                a0 = calls;
                glBegin(GL_POINTS);
                glCreateTextures((GLenum)0xDEAD, 1, &named);
                glBindTextureUnit(0u, named);
                glTextureParameteri(named, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTextureParameterf(named, GL_TEXTURE_MIN_LOD, 22.f);
                glTextureParameterIiv(named, GL_TEXTURE_BORDER_COLOR, border);
                glTextureParameterIuiv(named, GL_TEXTURE_BORDER_COLOR, (const GLuint *)border);
                glPrioritizeTextures(1, &named, &priority);
                glGetTextureParameteriv(named, GL_TEXTURE_WRAP_S, &sentinel);
                glEnd();
                if (glGetError() != GL_INVALID_OPERATION || sentinel != -515 || binds != b0 ||
                    tex_parameter_calls != t0 || tex_float_calls != f0 || tex_signed_calls != i0 ||
                    tex_unsigned_calls != u0 || calls != a0) {
                    fputs("DSA texture conformance: Begin/End touched native or query output\n",
                          stderr);
                    return 117;
                }
                glGetTextureParameteriv(named, GL_TEXTURE_WRAP_S, &v);
                if (glGetError() != GL_NO_ERROR || v != (GLint)GL_CLAMP_TO_EDGE ||
                    !glIsTexture(named)) {
                    fputs("DSA texture conformance: illegal call changed live texture\n", stderr);
                    return 118;
                }
                glDeleteTextures(1, &named);
            }
            depth_stencil_state_trace();
            dsa_texture_transfer_trace();
            object_lifecycle_batch_trace();
            matrix_family_batch_trace();
            scalar_uniform_batch_trace();
            immediate_current_batch_trace();
            extension_multitex_batch_trace();
            fixed_point_batch_trace();
            texture_dsa_parameter_batch_trace();
            /* Rejected EXT parameter and upload commands preserve the named texture and issue no
             * GLES calls. */
            {
                GLuint named = 0;
                GLint wrap = -1, width = -1, sentinel = -515;
                const GLint border[4] = {1, 2, 3, 4};
                const GLubyte block[8] = {0};
                unsigned b0, t0, f0, i0, u0;
                glCreateTextures(GL_TEXTURE_2D, 1, &named);
                glTextureParameteriEXT(named, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                if (glGetError() != GL_NO_ERROR)
                    return 141;
                b0 = binds;
                t0 = tex_parameter_calls;
                f0 = tex_float_calls;
                i0 = tex_signed_calls;
                u0 = tex_unsigned_calls;
                glBegin(GL_POINTS);
                glTextureParameteriEXT(named, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTextureParameterfEXT(named, GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 3.0f);
                glTextureParameterIivEXT(named, GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
                glGetTextureParameterivEXT(named, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &sentinel);
                glCompressedTextureImage2DEXT(named, GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4,
                                              4, 0, 8, block);
                glEnd();
                if (glGetError() != GL_INVALID_OPERATION || sentinel != -515 || binds != b0 ||
                    tex_parameter_calls != t0 || tex_float_calls != f0 || tex_signed_calls != i0 ||
                    tex_unsigned_calls != u0) {
                    fputs("BS: rejected EXT texture command modified output or GLES callbacks\n",
                          stderr);
                    return 142;
                }
                glGetTextureParameteriv(named, GL_TEXTURE_WRAP_S, &wrap);
                glGetTextureLevelParameteriv(named, 0, GL_TEXTURE_WIDTH, &width);
                if (glGetError() != GL_NO_ERROR || wrap != GL_CLAMP_TO_EDGE || width != 0) {
                    fputs("BS: rejected EXT texture command modified texture state\n", stderr);
                    return 143;
                }
                glDeleteTextures(1, &named);
            }

            if (uniform_invalid_native_calls != 0u) {
                fputs("uniform conformance: rejected negative locations reached GLES\n", stderr);
                return 140;
            }
            {
                GLint box[4] = {0};
                unsigned clear_before, scissor_before, stencil_before, range_before;
                glScissor(11, 12, 24, 25);
                if (glGetError() != GL_NO_ERROR)
                    return 112;
                clear_before = ds_clear_calls;
                scissor_before = ds_scissor_calls;
                stencil_before = ds_stencil_calls;
                range_before = ds_depth_range_calls;
                glBegin(GL_POINTS);
                glScissor(0, 0, -1, -1);
                glStencilFuncSeparate(0xDEAD, 0xDEAD, 0, 0);
                glDepthRange(0., 1.);
                glClear(0xDEAD0000u);
                glEnd();
                if (glGetError() != GL_INVALID_OPERATION || clear_before != ds_clear_calls ||
                    scissor_before != ds_scissor_calls || stencil_before != ds_stencil_calls ||
                    range_before != ds_depth_range_calls) {
                    fputs("depth/stencil conformance: illegal state operation reached GLES\n",
                          stderr);
                    return 113;
                }
                glGetIntegerv(GL_SCISSOR_BOX, box);
                if (glGetError() != GL_NO_ERROR || box[0] != 11 || box[1] != 12 || box[2] != 24 ||
                    box[3] != 25) {
                    fputs("depth/stencil conformance: illegal call modified scissor state\n",
                          stderr);
                    return 114;
                }
            }
            if (mock_indexed_mask_calls != mask_before + 1u ||
                mock_indexed_blend_func_calls != func_before + 1u ||
                mock_indexed_blend_eq_calls != eq_before + 1u) {
                fputs("indexed state conformance: rejected call reached GLES\n", stderr);
                return 105;
            }
        }
        if (mock_image_bind_calls != image_before + 1u ||
            mock_barrier_calls != barrier_before + 1u ||
            mock_region_barrier_calls != region_before + 1u || ctx.primitive_restart_index != 17u ||
            ctx.provoking_vertex_mode != GL_FIRST_VERTEX_CONVENTION ||
            ctx.image_units[0].texture != 0u || ctx.image_units[0].format != GL_RGBA8) {
            fputs("modern state conformance: rejected call reached GLES or mutated state\n",
                  stderr);
            return 104;
        }
    }
    {
        static const GLubyte indices[] = {0};
        glBegin(GL_POINTS);
        glDrawArrays(GL_POINTS, 0, 1);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || rejected_draw_native_calls != 0u) {
            fputs("draw conformance: illegal array draw reached native or failed validation\n",
                  stderr);
            return 71;
        }
        glBegin(GL_POINTS);
        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_BYTE, indices);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || rejected_draw_native_calls != 0u) {
            fputs("draw conformance: illegal indexed draw reached native or failed validation\n",
                  stderr);
            return 72;
        }
    }

    /* Current generic attributes may be updated inside Begin/End; client-array configuration is
     * rejected. */
    {
        const struct mgl_vertex_binding attrib_before = ctx.current_vao->generic[2];
        const struct mgl_vertex_binding vertex_before =
            ctx.current_vao->compat[MGL_COMPAT_SEM_POSITION];
        const struct mgl_vertex_binding color_before =
            ctx.current_vao->compat[MGL_COMPAT_SEM_COLOR];
        GLfloat output[4] = {-5.0f, -5.0f, -5.0f, -5.0f};
        void *pointer_output = (void *)(uintptr_t)0x7f00;
        glBegin(GL_POINTS);
        glVertexAttribPointer(2u, 1, GL_FLOAT, GL_FALSE, 0, (const void *)(uintptr_t)0x9876);
        glVertexAttribIPointer(2u, 1, GL_INT, 0, (const void *)(uintptr_t)0x8765);
        glEnableVertexAttribArray(2u);
        glVertexAttribDivisor(2u, 77u);
        glVertexPointer(2, GL_FLOAT, 0, (const void *)(uintptr_t)0x7654);
        glColorPointer(3, GL_FLOAT, 0, (const void *)(uintptr_t)0x6543);
        glInterleavedArrays(GL_T2F_V3F, 0, (const void *)(uintptr_t)0x5432);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION ||
            memcmp(&attrib_before, &ctx.current_vao->generic[2], sizeof(attrib_before)) != 0 ||
            memcmp(&vertex_before, &ctx.current_vao->compat[MGL_COMPAT_SEM_POSITION],
                   sizeof(vertex_before)) != 0 ||
            memcmp(&color_before, &ctx.current_vao->compat[MGL_COMPAT_SEM_COLOR],
                   sizeof(color_before)) != 0) {
            fputs("vertex conformance: rejected array configuration changed VAO state\n", stderr);
            return 101;
        }
        glBegin(GL_POINTS);
        glGetVertexAttribfv(2u, GL_CURRENT_VERTEX_ATTRIB, output);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || output[0] != -5.0f || output[3] != -5.0f) {
            fputs("vertex conformance: rejected getter overwrote caller output\n", stderr);
            return 102;
        }
        glBegin(GL_POINTS);
        glGetVertexAttribPointerv(2u, GL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer_output);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || pointer_output != (void *)(uintptr_t)0x7f00) {
            fputs("vertex conformance: rejected pointer getter overwrote caller output\n", stderr);
            return 103;
        }
    }
    {
        unsigned enabled = capability_enable_calls;
        unsigned disabled = capability_disable_calls;
        unsigned indexed_enabled = capability_enable_indexed_calls;
        unsigned indexed_disabled = capability_disable_indexed_calls;
        glBegin(GL_POINTS);
        glEnable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
        glEnablei(GL_BLEND, 1u);
        glDisablei(GL_BLEND, 1u);
        glEnableIndexedEXT(GL_BLEND, 2u);
        glDisableIndexedEXT(GL_BLEND, 2u);
        (void)glIsEnabled(GL_BLEND);
        (void)glIsEnabledi(GL_BLEND, 1u);
        (void)glIsEnabledIndexedEXT(GL_BLEND, 2u);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || glIsEnabled(GL_BLEND) ||
            glIsEnabled(GL_SCISSOR_TEST) || glIsEnabledi(GL_BLEND, 1u) ||
            glIsEnabledIndexedEXT(GL_BLEND, 2u) || glGetError() != GL_NO_ERROR ||
            capability_enable_calls != enabled || capability_disable_calls != disabled ||
            capability_enable_indexed_calls != indexed_enabled ||
            capability_disable_indexed_calls != indexed_disabled) {
            fputs("capability conformance: rejected state commands reached GLES or changed "
                  "interface state\n",
                  stderr);
            return 91;
        }
    }
    {
        GLuint buffer = 0;
        GLuint selected[1];
        GLubyte initial[4] = {10, 20, 30, 40};
        GLubyte changed[4] = {90, 91, 92, 93};
        GLubyte retained[4] = {0, 0, 0, 0};
        GLint size = -111;
        unsigned base, range, data, subdata;
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_UNIFORM_BUFFER, buffer);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(initial), initial, GL_STATIC_DRAW);
        if (glGetError() != GL_NO_ERROR)
            return 23;
        selected[0] = buffer;
        base = indexed_base_calls;
        range = indexed_range_calls;
        data = named_buffer_data_calls;
        subdata = named_buffer_subdata_calls;
        glBegin(GL_POINTS);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, buffer);
        glBindBufferRange(GL_UNIFORM_BUFFER, 0, buffer, 0, 4);
        glBindBuffersBase(GL_UNIFORM_BUFFER, 0, 1, selected);
        glBindBuffersRange(GL_UNIFORM_BUFFER, 0, 1, selected, (const GLintptr[]){0},
                           (const GLsizeiptr[]){4});
        glNamedBufferData(buffer, sizeof(changed), changed, GL_STATIC_DRAW);
        glNamedBufferSubData(buffer, 0, sizeof(changed), changed);
        glGetNamedBufferSubData(buffer, 0, sizeof(retained), retained);
        glGetNamedBufferParameteriv(buffer, GL_BUFFER_SIZE, &size);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || indexed_base_calls != base ||
            indexed_range_calls != range || named_buffer_data_calls != data ||
            named_buffer_subdata_calls != subdata || size != -111 || retained[0] != 0 ||
            retained[1] != 0) {
            fputs("buffer conformance: illegal indexed/named commands modified native or output\n",
                  stderr);
            return 24;
        }
        glGetNamedBufferSubData(buffer, 0, sizeof(retained), retained);
        if (glGetError() != GL_NO_ERROR || memcmp(retained, initial, sizeof(initial)) != 0) {
            fputs("buffer conformance: illegal named commands mutated object data\n", stderr);
            return 25;
        }
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glDeleteBuffers(1, &buffer);
        if (glGetError() != GL_NO_ERROR)
            return 26;
    }
    /* Illegal sampler commands must not allocate,
     * delete, rebind, or change GLES sampler parameters or interface state. */
    {
        GLuint sampler = 0u;
        unsigned before_gen, before_delete, before_bind, before_param;
        GLfloat min_lod = -1.0f;
        GLint wrap = -1;
        glGenSamplers(1, &sampler);
        glBindSampler(0u, sampler);
        glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, 2.25f);
        if (sampler == 0u || glGetError() != GL_NO_ERROR)
            return 23;
        before_gen = sampler_gen_calls;
        before_delete = sampler_delete_calls;
        before_bind = sampler_bind_calls;
        before_param = sampler_param_calls;
        glBegin(GL_POINTS);
        glGenSamplers(1, &sampler);
        glDeleteSamplers(1, &sampler);
        glBindSampler(0u, 0u);
        glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, 15.0f);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glEnd();
        glGetSamplerParameterfv(sampler, GL_TEXTURE_MIN_LOD, &min_lod);
        glGetSamplerParameteriv(sampler, GL_TEXTURE_WRAP_S, &wrap);
        if (glGetError() != GL_INVALID_OPERATION || !glIsSampler(sampler) || min_lod != 2.25f ||
            wrap != (GLint)GL_REPEAT || ctx.sampler_binding[0] != sampler ||
            sampler_gen_calls != before_gen || sampler_delete_calls != before_delete ||
            sampler_bind_calls != before_bind || sampler_param_calls != before_param) {
            fputs("sampler conformance: rejected Begin/End changed object, parameters or GLES "
                  "native\n",
                  stderr);
            return 23;
        }
        glBindSampler(0u, 0u);
        glDeleteSamplers(1, &sampler);
        if (glGetError() != GL_NO_ERROR)
            return 24;
    }
    /* Validate native callbacks and retained state independently of optional native conformance
     * tests. */
    {
        static const GLubyte compressed[8] = {18, 71, 2, 0, 0, 0, 0, 0};
        static const GLubyte pixels[64] = {1, 2, 3, 255};
        GLubyte guard[8] = {171, 172, 173, 174, 175, 176, 177, 178};
        unsigned before_s2, before_s3, before_m;
        GLint w = -1;
        glBindTexture(GL_TEXTURE_2D, 9751u);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8, compressed);
        if (glGetError() != GL_NO_ERROR)
            return 16;
        glBegin(GL_POINTS);
        glGetCompressedTexImage(GL_TEXTURE_2D, 0, guard);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || guard[0] != 171 || guard[7] != 178) {
            fputs("storage/mipmap conformance: compressed readback wrote during Begin/End\n",
                  stderr);
            return 16;
        }
        glBindTexture(GL_TEXTURE_2D, 9752u);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        if (glGetError() != GL_NO_ERROR)
            return 17;
        before_m = mipmap_calls;
        glBegin(GL_POINTS);
        glGenerateMipmap(GL_TEXTURE_2D);
        glEnd();
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &w);
        if (glGetError() != GL_INVALID_OPERATION || mipmap_calls != before_m || w != 0) {
            fputs("storage/mipmap conformance: illegal mipmap generation mutated state or GLES\n",
                  stderr);
            return 17;
        }
        glBindTexture(GL_TEXTURE_2D, 9753u);
        glBindTexture(GL_TEXTURE_3D, 9754u);
        if (glGetError() != GL_NO_ERROR)
            return 18;
        before_s2 = storage2_calls;
        before_s3 = storage3_calls;
        glBegin(GL_POINTS);
        glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 4, 4);
        glTexStorage3D(GL_TEXTURE_3D, 3, GL_RGBA8, 4, 4, 2);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || storage2_calls != before_s2 ||
            storage3_calls != before_s3) {
            fputs("storage/mipmap conformance: illegal immutable allocation reached GLES\n",
                  stderr);
            return 18;
        }
        w = -1;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
        if (glGetError() != GL_NO_ERROR || w != 0)
            return 19;
        w = -1;
        glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_WIDTH, &w);
        if (glGetError() != GL_NO_ERROR || w != 0) {
            fputs("storage/mipmap conformance: illegal immutable allocation modified levels\n",
                  stderr);
            return 19;
        }
    }

    {
        const GLubyte sample[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        unsigned before_i2 = image2_calls, before_i3 = image3_calls;
        unsigned before_s2 = sub2_calls, before_s3 = sub3_calls;
        unsigned before_pixel = pixel_store_calls;
        GLint unpack_before = ctx.unpack_alignment, pack_before = ctx.pack_alignment;
        glBegin(GL_POINTS);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
        glPixelStoref(GL_PACK_ALIGNMENT, 8.0f);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, sample);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, sample);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, sample);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, sample);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || image2_calls != before_i2 ||
            image3_calls != before_i3 || sub2_calls != before_s2 || sub3_calls != before_s3 ||
            pixel_store_calls != before_pixel || ctx.unpack_alignment != unpack_before ||
            ctx.pack_alignment != pack_before) {
            fputs("texture upload conformance: illegal Begin/End reached GLES or changed state\n",
                  stderr);
            return 14;
        }
    }
    {
        GLubyte guard[8] = {71, 72, 73, 74, 75, 76, 77, 78};
        glBegin(GL_POINTS);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, guard);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || guard[0] != 71 || guard[1] != 72 ||
            guard[7] != 78) {
            fputs("texture image conformance: Begin/End readback must not touch output\n", stderr);
            return 13;
        }
    }
    {
        GLint lod = -31415;
        GLfloat float_lod = -31415.0f;
        GLuint border[4] = {31415u, 31415u, 31415u, 31415u};
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 2.5f);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, &lod);
        if (lod != 3 || glGetError() != GL_NO_ERROR) {
            fputs("texture getter conformance: positive half LOD was not rounded\n", stderr);
            return 10;
        }
        glBegin(GL_POINTS);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, &lod);
        glGetTexParameterfv(0u, 0u, &float_lod);
        glGetTexParameterIuiv(0u, GL_TEXTURE_BORDER_COLOR, border);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || lod != 3 || float_lod != -31415.0f ||
            border[0] != 31415u || border[1] != 31415u || border[2] != 31415u ||
            border[3] != 31415u) {
            fputs("texture getter conformance: illegal Begin/End query changed outputs or error\n",
                  stderr);
            return 11;
        }
    }
    {
        GLint width = -31415;
        GLfloat height = -31415.0f;
        glBegin(GL_POINTS);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameterfv(0u, -1, 0u, &height);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || width != -31415 || height != -31415.0f) {
            fputs("texture level conformance: illegal Begin/End query modified output or error\n",
                  stderr);
            return 12;
        }
    }
    /* Rejected compressed uploads and copies preserve texture state and
     * perform no uncompressed GLES upload through the RGTC fallback. */
    {
        static const GLubyte block[16] = {30, 60, 0, 0, 0, 0, 0, 0, 90, 100, 0, 0, 0, 0, 0, 0};
        unsigned before_2d, before_3d, before_sub_2d, before_sub_3d;
        GLint width_2d = -1, width_array = -1;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 9501);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 9502);
        (void)glGetError();
        before_2d = image2_calls;
        before_3d = image3_calls;
        before_sub_2d = sub2_calls;
        before_sub_3d = sub3_calls;
        glBegin(GL_POINTS);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 8, 4, 0, 16, block);
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_COMPRESSED_RED_RGTC1, 8, block);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 8, 4, 2, 0, 32,
                               block);
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 4, 4, 1, GL_COMPRESSED_RED_RGTC1,
                                  8, block);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 1, 1, 0);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 1, 1);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || image2_calls != before_2d ||
            image3_calls != before_3d || sub2_calls != before_sub_2d ||
            sub3_calls != before_sub_3d) {
            fputs("compressed/copy conformance: Begin/End touched GLES or returned wrong error\n",
                  stderr);
            return 14;
        }
        glBindTexture(GL_TEXTURE_2D, 9501);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width_2d);
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_WIDTH, &width_array);
        if (glGetError() != GL_NO_ERROR || width_2d != 4 || width_array != 4) {
            fputs("compressed/copy conformance: rejected uploads modified compressed levels\n",
                  stderr);
            return 15;
        }
    }
    /* Host callback and ownership checks run without a native GLX provider. */
    {
        GLuint buffer = 0, texture = 0;
        unsigned full_before, range_before, bind_before;
        GLint data_store = -1, width = -1;
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_TEXTURE_BUFFER, buffer);
        glBufferData(GL_TEXTURE_BUFFER, 1024, NULL, GL_STATIC_DRAW);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_BUFFER, texture);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, buffer);
        if (glGetError() != GL_NO_ERROR)
            return 19;
        full_before = buffer_full_calls;
        range_before = buffer_range_calls;
        bind_before = binds;
        glBegin(GL_POINTS);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, 0);
        glTexBufferRange(GL_TEXTURE_BUFFER, GL_R32UI, buffer, 0, 256);
        glTextureBuffer(texture, GL_R32UI, 0);
        glTextureBufferRange(texture, GL_R32UI, buffer, 0, 256);
        glDeleteTextures(1, &texture);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || !glIsTexture(texture) ||
            glGetError() != GL_NO_ERROR || buffer_full_calls != full_before ||
            buffer_range_calls != range_before || binds != bind_before) {
            fputs(
                "texture-buffer conformance: illegal operation mutated native or deleted texture\n",
                stderr);
            return 20;
        }
        glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_DATA_STORE_BINDING,
                                 &data_store);
        glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &width);
        if (glGetError() != GL_NO_ERROR || data_store != (GLint)buffer || width != 256) {
            fputs("texture-buffer conformance: Begin/End changed attached buffer state\n", stderr);
            return 21;
        }
        glDeleteTextures(1, &texture);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);
        glDeleteBuffers(1, &buffer);
        if (glGetError() != GL_NO_ERROR)
            return 22;
    }
    /* Fixed-function Begin/End rejection must leave independent state intact. */
    {
        GLdouble plane[4] = {1., 0., 0., 0.};
        GLint mode = 0, shade = 0, texgen = 0;
        GLfloat query = -7.f;
        glActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
        glShadeModel(GL_SMOOTH);
        glClipPlane(GL_CLIP_PLANE0, plane);
        if (glGetError() != GL_NO_ERROR)
            return 23;
        glBegin(GL_POINTS);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glGetTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &query);
        glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
        glShadeModel(GL_FLAT);
        glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);
        glClipPlane(GL_CLIP_PLANE0, plane);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || query != -7.f) {
            fputs("fixed-function conformance: illegal operation or getter was accepted\n", stderr);
            return 24;
        }
        glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &mode);
        glGetTexGeniv(GL_S, GL_TEXTURE_GEN_MODE, &texgen);
        glGetIntegerv(GL_SHADE_MODEL, &shade);
        if (glGetError() != GL_NO_ERROR || mode != (GLint)GL_REPLACE ||
            texgen != (GLint)GL_OBJECT_LINEAR || shade != (GLint)GL_SMOOTH) {
            fputs("fixed-function conformance: illegal Begin/End changed retained state\n", stderr);
            return 25;
        }
    }
    /* Named texture commands rejected inside Begin/End must never bind a
     * GLES texture or access/write the caller's output storage. */
    {
        GLuint texture = 0;
        GLint width = -73;
        GLfloat fwidth = -73.0f;
        GLubyte pixels[16] = {0};
        const GLubyte source[16] = {1, 2, 3, 255, 4, 5, 6, 255, 7, 8, 9, 255, 10, 11, 12, 255};
        unsigned before_binds, before_storage, before_sub, before_mipmap;
        memset(pixels, 0xa5, sizeof(pixels));
        glCreateTextures(GL_TEXTURE_2D, 1, &texture);
        glTextureStorage2D(texture, 1, GL_RGBA8, 2, 2);
        if (glGetError() != GL_NO_ERROR)
            return 36;
        before_binds = binds;
        before_storage = storage2_calls;
        before_sub = sub2_calls;
        before_mipmap = mipmap_calls;
        glBegin(GL_POINTS);
        glTextureStorage2D(texture, 1, GL_RGBA8, 2, 2);
        glTextureSubImage2D(texture, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, source);
        glCompressedTextureSubImage2D(texture, 0, 0, 0, 4, 4, GL_COMPRESSED_RED_RGTC1, 8, source);
        glGenerateTextureMipmap(texture);
        glCopyTextureSubImage2D(texture, 0, 0, 0, 0, 0, 1, 1);
        glGetTextureImage(texture, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLsizei)sizeof(pixels), pixels);
        glGetCompressedTextureImage(texture, 0, (GLsizei)sizeof(pixels), pixels);
        glGetTextureLevelParameteriv(texture, 0, GL_TEXTURE_WIDTH, &width);
        glGetTextureLevelParameterfv(texture, 0, GL_TEXTURE_WIDTH, &fwidth);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || binds != before_binds ||
            storage2_calls != before_storage || sub2_calls != before_sub ||
            mipmap_calls != before_mipmap || width != -73 || fwidth != -73.0f ||
            pixels[0] != 0xa5 || pixels[15] != 0xa5) {
            fputs("named texture conformance: illegal operation touched GLES, texture state or "
                  "getter output\n",
                  stderr);
            return 37;
        }
        glGetTextureLevelParameteriv(texture, 0, GL_TEXTURE_WIDTH, &width);
        if (glGetError() != GL_NO_ERROR || width != 2)
            return 38;
        glDeleteTextures(1, &texture);
        if (glGetError() != GL_NO_ERROR)
            return 39;
    }
    indexed_family_batch_trace();
    arb_program_compat_batch_trace();
    /* Verify ARB object ownership and native dispatch behavior on rejected operations. */
    {
        GLhandleARB shader = glCreateShaderObjectARB(GL_VERTEX_SHADER);
        GLhandleARB program = glCreateProgramObjectARB();
        GLint attached = -23, output = -23;
        unsigned attach_before, detach_before;
        if (shader == 0u || program == 0u || glGetError() != GL_NO_ERROR)
            return 73;
        glAttachObjectARB(program, shader);
        if (glGetError() != GL_NO_ERROR)
            return 74;
        attach_before = mock_shader_attach_calls;
        detach_before = mock_shader_detach_calls;
        glBegin(GL_POINTS);
        glDetachObjectARB(program, shader);
        glAttachObjectARB(program, shader);
        glDeleteObjectARB(shader);
        glGetObjectParameterivARB(shader, GL_OBJECT_TYPE_ARB, &output);
        glGetAttachedObjectsARB(program, 1, &attached, NULL);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || output != -23 || attached != -23 ||
            mock_shader_attach_calls != attach_before ||
            mock_shader_detach_calls != detach_before || !glIsShader(shader) ||
            glGetError() != GL_NO_ERROR) {
            fputs("ARB object conformance: illegal Begin/End changed objects, outputs or GLES "
                  "dispatch\n",
                  stderr);
            return 75;
        }
        glGetObjectParameterivARB(program, GL_OBJECT_ATTACHED_OBJECTS_ARB, &attached);
        if (attached != 1 || glGetError() != GL_NO_ERROR)
            return 76;
        glDetachObjectARB(program, shader);
        if (glGetError() != GL_NO_ERROR || mock_shader_detach_calls != detach_before + 1u)
            return 77;
        glDeleteObjectARB(shader);
        glDeleteObjectARB(program);
        if (glGetError() != GL_NO_ERROR)
            return 78;
    }
    object_list_batch_trace();
    fbf_trace();
    texture_transfer_bx_trace();
    by_trace();
    indexed_output_vao_bz_trace();
    core_texture_dsa_ca_trace();
    multitex_alias_cb_trace();
    vao_ownership_cc_trace();
    compat_remainder_cd_trace();
    /* Named multitexture binds within Begin/End preserve texture-unit ownership and issue no
     * GLES binds. */
    {
        GLuint obj = 0;
        GLint actual = -1;
        unsigned binds_before, active_before;
        glGenTextures(1, &obj);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, obj);
        glActiveTexture(GL_TEXTURE0);
        if (glGetError() != GL_NO_ERROR)
            return 196;
        binds_before = binds;
        active_before = calls;
        glBegin(GL_POINTS);
        glBindMultiTexture(GL_TEXTURE1, GL_TEXTURE_2D, 0);
        glBindMultiTexture(GL_TEXTURE1, 0xDEADu, 0);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || binds != binds_before ||
            calls != active_before) {
            fputs("multitexture bind: illegal primitive changed GLES state\n", stderr);
            return 197;
        }
        glActiveTexture(GL_TEXTURE1);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &actual);
        if (actual != (GLint)obj || glGetError() != GL_NO_ERROR) {
            fputs("multitexture bind: illegal primitive changed texture ownership\n", stderr);
            return 198;
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glDeleteTextures(1, &obj);
        if (glGetError() != GL_NO_ERROR)
            return 199;
    }
    /* Named VAO client-memory pointers require an array buffer. Valid VBO offsets remain legal. */
    {
        GLuint vao = 0, buffer = 0;
        void *v = (void *)(uintptr_t)0xbad;
        GLboolean enabled = GL_TRUE;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        if (glGetError() != GL_NO_ERROR)
            return 185;
        glVertexPointer(3, GL_FLOAT, 0, (const void *)(uintptr_t)0x1230);
        if (glGetError() != GL_INVALID_OPERATION)
            return 186;
        glGetPointerv(GL_VERTEX_ARRAY_POINTER, &v);
        if (v != NULL || glGetError() != GL_NO_ERROR)
            return 187;
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 0, (const void *)(uintptr_t)0x2340);
        if (glGetError() != GL_INVALID_OPERATION)
            return 188;
        v = (void *)(uintptr_t)0xbad;
        glGetVertexAttribPointerv(4, GL_VERTEX_ATTRIB_ARRAY_POINTER, &v);
        if (v != NULL || glGetError() != GL_NO_ERROR)
            return 189;
        glVertexAttribIPointer(5, 3, GL_INT, 0, (const void *)(uintptr_t)0x3450);
        if (glGetError() != GL_INVALID_OPERATION)
            return 190;
        glInterleavedArrays(GL_V3F, 0, (const void *)(uintptr_t)0x4560);
        if (glGetError() != GL_INVALID_OPERATION)
            return 191;
        enabled = glIsEnabled(GL_VERTEX_ARRAY);
        if (enabled != GL_FALSE || glGetError() != GL_NO_ERROR)
            return 192;
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);
        glVertexPointer(3, GL_FLOAT, 0, (const void *)(uintptr_t)0x20);
        if (glGetError() != GL_NO_ERROR)
            return 193;
        v = NULL;
        glGetPointerv(GL_VERTEX_ARRAY_POINTER, &v);
        if (v != (void *)(uintptr_t)0x20 || glGetError() != GL_NO_ERROR)
            return 194;
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &buffer);
        glDeleteVertexArrays(1, &vao);
        if (glGetError() != GL_NO_ERROR)
            return 195;
    }
    /* Invalid compatibility-alias calls preserve the selected texture unit and issue no GLES
     * work. */
    {
        GLuint texture[2] = {0, 0};
        GLint wrap0 = -1, wrap1 = -1, active = -1, rejected = -919;
        const GLubyte pixel[4] = {1, 2, 3, 4};
        unsigned p0, im0, sub0, bind0, active0;
        glGenTextures(2, texture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture[1]);
        glActiveTexture(GL_TEXTURE0);
        glMultiTexParameteri(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glMultiTexImage2D(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, pixel);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap0);
        glGetMultiTexParameteriv(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap1);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        if (glGetError() != GL_NO_ERROR || wrap0 != GL_REPEAT || wrap1 != GL_CLAMP_TO_EDGE ||
            active != GL_TEXTURE0) {
            fputs("multitexture aliases: valid operations changed wrong unit\n", stderr);
            return 183;
        }
        p0 = tex_parameter_calls;
        im0 = image2_calls;
        sub0 = sub2_calls;
        bind0 = binds;
        active0 = calls;
        glBegin(GL_POINTS);
        glMultiTexParameteri(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glMultiTexImage2D(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA,
                          GL_UNSIGNED_BYTE, pixel);
        glMultiTexSubImage2D(GL_TEXTURE1, GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                             pixel);
        glGetMultiTexParameteriv(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &rejected);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || rejected != -919 || tex_parameter_calls != p0 ||
            image2_calls != im0 || sub2_calls != sub0 || binds != bind0 || calls != active0) {
            fputs("multitexture aliases: rejected operation modified GLES or output\n", stderr);
            return 184;
        }
        glGetMultiTexParameteriv(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap1);
        glGetMultiTexLevelParameteriv(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &active);
        if (glGetError() != GL_NO_ERROR || wrap1 != GL_CLAMP_TO_EDGE || active != 1) {
            fputs("multitexture aliases: rejected operation modified texture storage\n", stderr);
            return 185;
        }
        glDeleteTextures(2, texture);
        if (glGetError() != GL_NO_ERROR)
            return 186;
    }
    if (ca_multisample2_calls != 3u || ca_multisample3_calls != 3u) {
        fputs("core texture DSA: rejected multisample storage dispatched to GLES or legal storage "
              "missing\n",
              stderr);
        return 1;
    }
    /* Rejected debug and client-state calls preserve labels, callbacks, message groups, outputs,
     * and client texture selection. */
    {
        GLuint texture = 0;
        GLchar output[16];
        GLsizei length = -31;
        void *pointer = (void *)(uintptr_t)0x1234u;
        GLint unit = -1;
        unsigned depth_before, queue_before;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glObjectLabel(GL_TEXTURE, texture, -1, "stable");
        glDebugMessageCallback(by_test_debug_callback, &ctx);
        glClientActiveTexture(GL_TEXTURE3);
        if (glGetError() != GL_NO_ERROR)
            return 168;
        depth_before = ctx.debug.group_depth;
        queue_before = ctx.debug.queue_count;
        memset(output, '!', sizeof(output));
        glBegin(GL_POINTS);
        glObjectLabel(GL_TEXTURE, texture, -1, "corrupt");
        glGetObjectLabel(GL_TEXTURE, texture, (GLsizei)sizeof(output), &length, output);
        glGetPointerv(GL_VERTEX_ARRAY_POINTER, &pointer);
        glDebugMessageCallback(NULL, NULL);
        glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 932,
                             GL_DEBUG_SEVERITY_NOTIFICATION, -1, "illegal");
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 933, -1, "illegal");
        glClientAttribDefaultEXT(GL_CLIENT_VERTEX_ARRAY_BIT);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || length != -31 || output[0] != '!' ||
            pointer != (void *)(uintptr_t)0x1234u || ctx.debug.callback != by_test_debug_callback ||
            ctx.debug.callback_user != &ctx || ctx.debug.group_depth != depth_before ||
            ctx.debug.queue_count != queue_before) {
            fputs("debug/client-state conformance: illegal operation changed owned state or "
                  "outputs\n",
                  stderr);
            return 169;
        }
        memset(output, 0, sizeof(output));
        length = -1;
        glGetObjectLabel(GL_TEXTURE, texture, (GLsizei)sizeof(output), &length, output);
        glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &unit);
        if (glGetError() != GL_NO_ERROR || length != 6 || strcmp(output, "stable") != 0 ||
            unit != (GLint)GL_TEXTURE3) {
            fputs("debug/client-state conformance: rejected operation changed state\n", stderr);
            return 170;
        }
        glDebugMessageCallback(NULL, NULL);
        glDeleteTextures(1, &texture);
        if (glGetError() != GL_NO_ERROR)
            return 171;
    }
    {
        GLuint texture = 0;
        GLubyte initial[4 * 4 * 4] = {0};
        GLubyte update[4 * 4 * 4] = {0};
        GLint level_width = -5;
        GLfloat widthf = -5.f;
        GLint before_width = -1;
        unsigned before_image, before_sub, before_binds, before_mipmap;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, initial);
        if (glGetError() != GL_NO_ERROR)
            return 164;
        before_image = image2_calls;
        before_sub = sub2_calls;
        before_binds = binds;
        before_mipmap = mipmap_calls;
        glBegin(GL_POINTS);
        glTextureImage2DEXT(texture, GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                            update);
        glTextureSubImage2DEXT(texture, GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                               update);
        glTextureImage3DEXT(texture, 0xDEADu, -1, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                            update);
        glTextureSubImage3DEXT(texture, 0xDEADu, -1, 0, 0, 0, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                               update);
        glCopyTextureImage2DEXT(texture, GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 2, 2, 0);
        glCopyTextureSubImage2DEXT(texture, GL_TEXTURE_2D, 0, 0, 0, 0, 0, 2, 2);
        glCopyTextureSubImage3DEXT(texture, 0xDEADu, -1, 0, 0, 0, 0, 0, 2, 2);
        glGenerateTextureMipmapEXT(texture, GL_TEXTURE_2D);
        glGetTextureImageEXT(texture, GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, update);
        glGetCompressedTextureImageEXT(texture, GL_TEXTURE_2D, 0, update);
        glGetTextureLevelParameterivEXT(texture, GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &level_width);
        glGetTextureLevelParameterfvEXT(texture, GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &widthf);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || level_width != -5 || widthf != -5.f ||
            image2_calls != before_image || sub2_calls != before_sub || binds != before_binds ||
            mipmap_calls != before_mipmap) {
            fputs("EXT transfer batch: rejected Begin/End call reached GLES or getter output\n",
                  stderr);
            return 165;
        }
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &before_width);
        if (glGetError() != GL_NO_ERROR || before_width != 4) {
            fputs("EXT transfer batch: rejection changed texture dimensions\n", stderr);
            return 166;
        }
        glDeleteTextures(1, &texture);
        if (glGetError() != GL_NO_ERROR)
            return 167;
    }
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

#ifndef MGL_OBJECTS_H
#define MGL_OBJECTS_H

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include "mgl/types.h"

#define MGL_MAX_VERTEX_ATTRIBS 32u
#define MGL_COMPAT_SEMANTIC_COUNT 13u
#define MGL_MAX_ATTACHED_SHADERS 8u
#define MGL_MAX_INDEXED_BUFFER_BINDINGS 256u

#define MGL_ARB_MAX_PROGRAM_PARAMETERS 96u
#define MGL_ARB_MAX_PROGRAM_INSTRUCTIONS 1024u
#define MGL_ARB_MAX_PROGRAM_TEMPORARIES 32u
#define MGL_ARB_MAX_PROGRAM_TEX_INSTRUCTIONS 256u
#define MGL_ARB_MAX_PROGRAM_ATTRIBS 32u

struct mgl_arb_program {
    GLuint name;
    GLenum target;
    char *source;
    char *translated_source;
    GLsizei source_length;
    GLboolean valid;
    GLuint serial;
    uint64_t local_revision; /* ARB parameter writes invalidate uniform uploads without recompiling the source. */
    GLuint generic_mask;
    unsigned max_env_index;
    unsigned max_local_index;
    GLuint instruction_count;
    GLuint temporary_count;
    GLuint parameter_count;
    GLuint attrib_count;
    GLuint address_count;
    GLuint texture_count;
    GLuint texture_indirections;
    GLfloat local[MGL_ARB_MAX_PROGRAM_PARAMETERS][4];
    struct mgl_arb_program *next;
};

struct mgl_buffer {
    GLuint name;
    GLuint native_name;
    GLboolean realized;
    GLenum usage;
    GLenum access;
    size_t size;
    unsigned char *shadow;
    GLboolean mapped;
    GLboolean immutable;
    GLboolean native_immutable;
    GLbitfield storage_flags;
    void *native_mapping;
    size_t map_offset;
    size_t map_length;
    GLbitfield map_access;
    unsigned char zero_length_map_byte;
    GLboolean delete_pending;
    unsigned texture_refcount;
    struct mgl_buffer *next;
    /* The hash indexes live names; the list also owns deleted buffer objects still retained by
     * textures. */
    struct mgl_buffer *hash_next;
};

#define MGL_MAX_TEXTURE_LEVELS 16u

struct mgl_texture_level {
    GLsizei width;
    GLsizei height;
    GLsizei depth;
    GLsizei samples;
    GLboolean fixed_sample_locations;
    GLenum internal_format;
    GLenum format;
    GLenum type;
    size_t data_size;
    unsigned char *data;
    GLboolean defined;
    GLboolean compressed;
    GLboolean shadow_stale;
};

struct mgl_texture {
    GLuint name;
    GLuint native_name;
    GLboolean realized;
    /* CPU upload shadows do not include later framebuffer rendering. */
    GLboolean used_as_color_attachment;
    GLboolean color_attachment_complete;
    GLboolean color_attachment_incomplete;
    GLenum target;
    GLint min_filter;
    GLint mag_filter;
    GLint wrap_s;
    GLint wrap_t;
    GLint wrap_r;
    GLfloat min_lod;
    GLfloat max_lod;
    GLfloat lod_bias;
    GLfloat max_anisotropy;
    GLfloat priority;
    GLint base_level;
    GLint max_level;
    GLint compare_mode;
    GLint compare_func;
    GLint depth_texture_mode;
    GLint depth_stencil_texture_mode;
    GLint swizzle[4];
    GLfloat border_color[4];
    GLint border_color_i[4];
    GLuint border_color_ui[4];
    GLenum border_color_type;
    struct mgl_buffer *buffer_texture_buffer;
    GLenum buffer_texture_internal_format;
    GLintptr buffer_texture_offset;
    GLsizeiptr buffer_texture_size;
    GLboolean immutable_format;
    GLint immutable_levels;
    GLboolean native_immutable_storage;
    GLboolean native_compressed_storage;
    GLboolean rectangle_native_initialized;
    struct mgl_texture *view_root;
    GLuint view_min_level;
    GLuint view_num_levels;
    GLuint view_min_layer;
    GLuint view_num_layers;
    unsigned view_refcount;
    GLboolean deleted_name;
    struct mgl_texture_level levels[MGL_MAX_TEXTURE_LEVELS];
    struct mgl_texture_level cube_levels[6][MGL_MAX_TEXTURE_LEVELS];
    struct mgl_texture *next;
    struct mgl_texture *hash_next;
};

struct mgl_indexed_buffer_binding {
    GLuint buffer;
    GLintptr offset;
    GLsizeiptr size;
    GLboolean ranged;
};

struct mgl_transform_feedback {
    GLuint name;
    GLuint native_name;
    GLboolean realized;
    GLboolean active;
    GLboolean paused;
    GLboolean ever_ended;
    struct mgl_indexed_buffer_binding buffer_bindings[MGL_MAX_INDEXED_BUFFER_BINDINGS];
    struct mgl_transform_feedback *next;
};

struct mgl_sync {
    GLsync native_sync;
    GLenum condition;
    GLbitfield flags;
    GLboolean fallback_signaled;
    struct mgl_sync *next;
};

struct mgl_sampler {
    GLuint name;
    GLuint native_name;
    GLboolean realized;
    GLint min_filter;
    GLint mag_filter;
    GLint wrap_s;
    GLint wrap_t;
    GLint wrap_r;
    GLfloat min_lod;
    GLfloat max_lod;
    GLfloat max_anisotropy;
    GLint compare_mode;
    GLint compare_func;
    GLfloat border_color[4];
    GLint border_color_i[4];
    GLuint border_color_ui[4];
    GLenum border_color_type;
    struct mgl_sampler *next;
};

struct mgl_renderbuffer {
    GLuint name;
    GLuint native_name;
    GLboolean realized;
    GLenum internal_format;
    GLsizei width;
    GLsizei height;
    GLsizei samples;
    struct mgl_renderbuffer *next;
};

enum mgl_data_source { MGL_DATA_CLIENT, MGL_DATA_BUFFER };

struct mgl_data_ref {
    enum mgl_data_source source;
    const void *client_ptr;
    GLuint buffer_name;
    size_t offset;
};

struct mgl_vertex_binding {
    GLboolean enabled;
    GLint size;
    GLenum type;
    GLboolean normalized;
    GLboolean integer;
    GLsizei stride;
    GLsizei attrib_array_stride;
    GLuint divisor;
    GLboolean separate_stride;
    struct mgl_data_ref data;
};

struct mgl_vertex_buffer_slot {
    struct mgl_data_ref data;
    GLsizei stride;
    GLuint divisor;
};

struct mgl_vertex_array {
    GLuint name;
    GLuint native_name;
    GLboolean realized;
    GLuint element_array_buffer;
    struct mgl_vertex_binding generic[MGL_MAX_VERTEX_ATTRIBS];
    struct mgl_vertex_buffer_slot vertex_buffers[MGL_MAX_VERTEX_ATTRIBS];
    GLuint vertex_attrib_binding[MGL_MAX_VERTEX_ATTRIBS];
    GLuint vertex_attrib_relative_offset[MGL_MAX_VERTEX_ATTRIBS];
    struct mgl_vertex_binding compat[MGL_COMPAT_SEMANTIC_COUNT];
    /* Edge-flag arrays are per-VAO client state. */
    struct mgl_vertex_binding edge_flag;
    /* Color-index arrays are separate from element-index buffers. */
    struct mgl_vertex_binding color_index;
    struct mgl_vertex_array *next;
};

struct mgl_framebuffer {
    GLuint name;
    GLuint native_name;
    GLuint routed_draw_native_name;
    GLboolean realized;
    GLenum draw_buffers[MGL_MAX_DRAW_BUFFERS];
    GLenum read_buffer;
    GLint default_width;
    GLint default_height;
    GLint default_layers;
    GLint default_samples;
    GLboolean default_fixed_sample_locations;
    GLuint color_renderbuffer;
    GLuint depth_renderbuffer;
    GLuint stencil_renderbuffer;
    GLuint color_texture;
    GLint color_texture_level;
    GLint color_texture_layer;
    GLenum color_texture_target;
    /* Additional GLES color attachments use the same native FBO. Index 0
     * remains in the established fields above for compatibility. */
    GLuint extra_color_renderbuffers[MGL_MAX_DRAW_BUFFERS];
    GLuint extra_color_textures[MGL_MAX_DRAW_BUFFERS];
    GLint extra_color_levels[MGL_MAX_DRAW_BUFFERS];
    GLint extra_color_layers[MGL_MAX_DRAW_BUFFERS];
    GLenum extra_color_targets[MGL_MAX_DRAW_BUFFERS];
    GLuint depth_texture;
    GLint depth_texture_level;
    GLint depth_texture_layer;
    GLenum depth_texture_target;
    GLuint stencil_texture;
    GLint stencil_texture_level;
    GLint stencil_texture_layer;
    GLenum stencil_texture_target;
    struct mgl_framebuffer *next;
};

struct mgl_spirv_module {
    atomic_uint references;
    size_t word_count;
    uint32_t words[];
};

struct mgl_shader {
    GLuint name;
    GLuint native_name;
    GLenum type;
    GLboolean delete_pending;
    GLboolean compiled;
    unsigned attach_count;
    char *source;
    char *translated_source;
    char *info_log;
    struct mgl_spirv_module *spirv_module;
    GLboolean spirv_specialized;
    struct mgl_shader *next;
};

struct mgl_frag_output_binding {
    char *name;
    GLuint color_number;
    GLuint index;
    GLuint linked_color_number;
    GLuint linked_index;
    GLboolean linked_explicit;
};

struct mgl_buffer_sampler_uniform {
    GLint location;
    GLenum type;
};

struct mgl_compat_uniform_cache;

struct mgl_program {
    GLuint name;
    GLuint native_name;
    GLboolean delete_pending;
    GLboolean linked;
    GLboolean linked_compute;
    GLboolean validated;
    GLboolean binary_retrievable_hint;
    GLboolean separable;
    GLboolean linked_separable;
    unsigned use_count;
    unsigned attached_count;
    struct mgl_shader *attached[MGL_MAX_ATTACHED_SHADERS];
    struct mgl_frag_output_binding *frag_output_bindings;
    size_t frag_output_binding_count;
    size_t frag_output_binding_capacity;
    struct mgl_buffer_sampler_uniform *buffer_sampler_uniforms;
    size_t buffer_sampler_uniform_count;
    GLboolean buffer_sampler_reflection_failed;
    GLint compat_attrib[MGL_COMPAT_SEMANTIC_COUNT];
    uint32_t active_generic_mask;
    GLenum active_generic_type[MGL_MAX_VERTEX_ATTRIBS];
    /* Last applied compatibility uniform values are stored per linked GLES program, including
     * programs shared by contexts. */
    struct mgl_compat_uniform_cache *compat_uniform_cache;
    GLboolean compat_uniform_cache_valid;
    GLint compat_uniform_model_view_projection;
    GLint compat_uniform_model_view;
    GLint compat_uniform_projection;
    GLint compat_uniform_normal_matrix;
    GLint compat_uniform_texture_matrix[8];
    GLint compat_uniform_sampler0;
    GLint compat_uniform_rect_scale[8];
    GLint compat_uniform_ff_texture_enabled;
    GLint compat_uniform_alpha_enabled;
    GLint compat_uniform_alpha_func;
    GLint compat_uniform_alpha_ref;
    GLint compat_uniform_point_coord_flip;
    GLint compat_uniform_point_size;
    GLint compat_uniform_program_point_size;
    GLint compat_uniform_texture_env_mode0;
    /* The generated implicit fragment stage uses these eight-unit texenv
     * locations. Other programs leave the locations at -1. */
    GLint compat_env_enabled;
    GLint compat_env_mode;
    GLint compat_env_color;
    GLint compat_env_rgb_mode;
    GLint compat_env_alpha_mode;
    GLint compat_env_rgb_source;
    GLint compat_env_alpha_source;
    GLint compat_env_rgb_operand;
    GLint compat_env_alpha_operand;
    GLint compat_env_rgb_scale;
    GLint compat_env_alpha_scale;
    GLint compat_env_lod_bias;
    GLint compat_env_sampler[8];
    GLint compat_uniform_texgen_enabled;
    GLint compat_uniform_texgen_mode;
    GLint compat_uniform_texgen_object_plane;
    GLint compat_uniform_texgen_eye_plane;
    GLint compat_uniform_clip_enabled;
    GLint compat_uniform_clip_plane;
    GLint compat_uniform_fog_enabled;
    GLint compat_uniform_fog_mode;
    GLint compat_uniform_fog_density;
    GLint compat_uniform_fog_start;
    GLint compat_uniform_fog_end;
    GLint compat_uniform_fog_color;
    GLint compat_uniform_fog_coord_explicit;
    GLint compat_uniform_normalize_enabled;
    GLint compat_uniform_lighting_enabled;
    GLint compat_uniform_implicit_texgen_enabled;
    GLint compat_uniform_implicit_texgen_mode;
    GLint compat_uniform_implicit_texgen_object_plane;
    GLint compat_uniform_implicit_texgen_eye_plane;
    GLint compat_uniform_light_enabled;
    GLint compat_uniform_light_ambient;
    GLint compat_uniform_light_diffuse;
    GLint compat_uniform_light_specular;
    GLint compat_uniform_light_position;
    GLint compat_uniform_light_spot_direction;
    GLint compat_uniform_light_spot_exponent;
    GLint compat_uniform_light_spot_cutoff;
    GLint compat_uniform_light_constant_attenuation;
    GLint compat_uniform_light_linear_attenuation;
    GLint compat_uniform_light_quadratic_attenuation;
    GLint compat_uniform_light_model_ambient;
    GLint compat_uniform_light_model_local_viewer;
    GLint compat_uniform_light_model_two_side;
    GLint compat_uniform_material_ambient;
    GLint compat_uniform_material_diffuse;
    GLint compat_uniform_material_specular;
    GLint compat_uniform_material_emission;
    GLint compat_uniform_material_shininess;
    GLint compat_uniform_material_back_ambient;
    GLint compat_uniform_material_back_diffuse;
    GLint compat_uniform_material_back_specular;
    GLint compat_uniform_material_back_emission;
    GLint compat_uniform_material_back_shininess;
    GLint compat_uniform_color_material_enabled;
    GLint compat_uniform_color_material_mode;
    GLint compat_uniform_color_material_face;
    GLsizei transform_feedback_varying_count;
    char **transform_feedback_varyings;
    GLenum transform_feedback_buffer_mode;
    char *info_log;
    struct mgl_program *next;
};

#endif

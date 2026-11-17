#ifndef MGL_CONTEXT_H
#define MGL_CONTEXT_H

#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include "native/dispatch.h"
#include "compat/fixed_function.h"
#include "compat/evaluator.h"
#include "compat/immediate.h"
#include "compat/raster.h"
#include "compat/selection.h"
#include "core/error.h"
#include "core/render_diag.h"
#include "debug/debug.h"
#include "objects/objects.h"
#include "state/state.h"

struct mgl_display_list;
struct mgl_query;
struct mgl_nv_fence;
struct mgl_arb_pair_cache;

#define MGL_ATTRIB_STACK_MAX_DEPTH 16u
#define MGL_IMAGE_UNIT_CAPACITY 32u
#define MGL_NATIVE_TEXTURE_TARGET_COUNT 8u

struct mgl_image_unit_binding {
    GLuint texture;
    GLint level;
    GLboolean layered;
    GLint layer;
    GLenum access;
    GLenum format;
};

struct mgl_attrib_snapshot {
    GLbitfield mask;
    struct mgl_raster_state canonical_state;
    struct mgl_compat_state compat;
    struct mgl_evaluator_state evaluator;
    struct mgl_pixel_transfer_state pixel_transfer;
    struct mgl_compat_raster_state compat_raster;
    GLfloat compat_current[MGL_COMPAT_SEMANTIC_COUNT][4];
    GLboolean current_edge_flag;
    GLuint list_base;
    GLuint texture_1d_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_2d_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_rectangle_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLboolean texture_2d_prefer_rectangle[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLboolean texture_2d_prefer_1d[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_3d_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_2d_array_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_buffer_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_cube_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_cube_array_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_2d_multisample_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_2d_multisample_array_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint sampler_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
};

struct mgl_client_attrib_snapshot {
    GLbitfield mask;
    GLuint array_buffer;
    GLuint pixel_pack_buffer;
    GLuint pixel_unpack_buffer;
    GLuint client_active_texture;
    GLint array_lock_first;
    GLsizei array_lock_count;
    GLint pack_alignment;
    GLint unpack_alignment;
    GLint pack_row_length;
    GLint pack_skip_rows;
    GLint pack_skip_pixels;
    GLint pack_image_height;
    GLint pack_skip_images;
    GLboolean pack_swap_bytes;
    GLboolean pack_lsb_first;
    GLint unpack_row_length;
    GLint unpack_skip_rows;
    GLint unpack_skip_pixels;
    GLint unpack_image_height;
    GLint unpack_skip_images;
    GLboolean unpack_swap_bytes;
    GLboolean unpack_lsb_first;
    GLuint element_array_buffer;
    struct mgl_vertex_binding compat[MGL_COMPAT_SEMANTIC_COUNT];
    struct mgl_vertex_binding edge_flag;
    struct mgl_vertex_binding color_index;
    struct mgl_vertex_binding generic[MGL_MAX_VERTEX_ATTRIBS];
};

struct mgl_share_group {
    atomic_uint refcount;
    pthread_mutex_t lock;
    atomic_uint next_buffer_name;
    atomic_uint buffer_revision;
    atomic_uint next_renderbuffer_name;
    atomic_uint next_texture_name;
    atomic_uint next_sampler_name;
    atomic_uint next_shader_program_name;
    atomic_uint_fast64_t program_link_revision;
    atomic_uint next_list_name;
    atomic_uint next_arb_program_name;
    struct mgl_buffer *buffers;
    struct mgl_buffer *buffer_index[1024];
    struct mgl_renderbuffer *renderbuffers;
    struct mgl_texture *textures;
    struct mgl_texture *texture_index[1024];
    struct mgl_sampler *samplers;
    struct mgl_sync *syncs;
    struct mgl_shader *shaders;
    struct mgl_program *programs;
    struct mgl_display_list *display_lists;
    struct mgl_arb_program *arb_programs;
    struct mgl_object_label *object_labels;
    /* Incremented under the share-group lock when ARB programs are created,
     * deleted, or modified; contexts use the revision to cache the active pair. */
    uint64_t arb_programs_revision;
};

struct mgl_depth_copy_cache {
    GLuint source_texture;
    GLuint source_framebuffer;
    GLuint destination_framebuffer;
    GLuint vertex_shader;
    GLuint fragment_shader;
    GLuint program;
    GLsizei width;
    GLsizei height;
    GLenum source_format;
};

struct mgl_pipeline {
    GLuint name;
    GLboolean realized;
    GLboolean validated;
    struct mgl_program *stages[6];
    struct mgl_program *active;
    struct mgl_pipeline *next;
};

struct mgl_context {
    struct mgl_share_group *share_group;
    struct mgl_error_state errors;
    struct mgl_debug_state debug;
    struct mgl_native_dispatch *native_dispatch;
    void (*native_draw_elements_base_vertex)(GLenum, GLsizei, GLenum, const void *, GLint);
    GLbitfield context_flags;
    GLbitfield profile_mask;
    GLbitfield glx_pending_copy_mask;
    struct mgl_depth_copy_cache depth_copy;

    MGL_EGLDisplay egl_display;
    MGL_EGLConfig egl_config;
    MGL_EGLContext egl_context;
    GLboolean native_gles32_verified;
    GLboolean angle_client_arrays;
    GLboolean angle_direct_texture_passthrough;
    GLboolean angle_lazy_buffer_shadows;
    MGL_EGLSurface egl_draw;
    MGL_EGLSurface egl_read;
    GLboolean egl_draw_owned;
    GLboolean egl_read_owned;
    GLsizei egl_draw_width;
    GLsizei egl_draw_height;
    GLsizei egl_read_width;
    GLsizei egl_read_height;

    struct mgl_raster_state canonical_state;
    struct mgl_compat_state compat;
    struct mgl_evaluator_state evaluator;
    struct mgl_pixel_transfer_state pixel_transfer;
    struct mgl_immediate_state immediate;
    struct mgl_compat_raster_state compat_raster;
    struct mgl_selection_state selection;
    struct mgl_display_list *compiling_list;
    GLenum list_compile_mode;
    GLboolean list_compile_failed;
    GLboolean list_replaying;
    unsigned list_replay_depth;
    GLuint list_base;

    struct mgl_attrib_snapshot attrib_stack[MGL_ATTRIB_STACK_MAX_DEPTH];
    unsigned attrib_stack_depth;
    struct mgl_client_attrib_snapshot client_attrib_stack[MGL_ATTRIB_STACK_MAX_DEPTH];
    unsigned client_attrib_stack_depth;

    GLuint array_buffer;
    GLuint pixel_pack_buffer;
    GLuint pixel_unpack_buffer;
    GLuint copy_read_buffer;
    GLuint copy_write_buffer;
    GLuint uniform_buffer;
    GLuint atomic_counter_buffer;
    GLuint transform_feedback_buffer;
    GLuint draw_indirect_buffer;
    GLuint parameter_buffer;
    GLuint query_buffer;
    GLuint dispatch_indirect_buffer;
    GLuint shader_storage_buffer;
    GLuint texture_buffer;
    struct mgl_indexed_buffer_binding uniform_buffer_bindings[MGL_MAX_INDEXED_BUFFER_BINDINGS];
    struct mgl_indexed_buffer_binding atomic_counter_buffer_bindings[MGL_MAX_INDEXED_BUFFER_BINDINGS];
    struct mgl_indexed_buffer_binding
        shader_storage_buffer_bindings[MGL_MAX_INDEXED_BUFFER_BINDINGS];
    struct mgl_indexed_buffer_binding
        transform_feedback_buffer_bindings[MGL_MAX_INDEXED_BUFFER_BINDINGS];

    struct mgl_vertex_array default_vao;
    struct mgl_vertex_array *vaos;
    struct mgl_vertex_array *current_vao;
    GLuint next_vao_name;

    struct mgl_framebuffer *framebuffers;
    GLuint next_framebuffer_name;
    GLuint draw_framebuffer;
    GLuint read_framebuffer;
    /* GLES FBO representing desktop default framebuffer zero for Android AHB WSI. */
    GLuint native_default_framebuffer;
    GLenum default_draw_buffers[MGL_MAX_DRAW_BUFFERS];
    GLenum default_read_buffer;
    GLuint renderbuffer;
    GLuint current_program;
    GLboolean current_program_executable;
    GLuint current_pipeline;
    struct mgl_pipeline *pipelines;
    GLuint use_program_fast_name;
    uint64_t use_program_fast_revision;
    uint64_t use_program_fast_hits;

    GLuint texture_1d_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_2d_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_rectangle_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLboolean texture_2d_prefer_rectangle[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLboolean texture_2d_prefer_1d[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_3d_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_2d_array_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_buffer_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_cube_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_cube_array_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_2d_multisample_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint texture_2d_multisample_array_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLuint sampler_binding[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    struct mgl_image_unit_binding image_units[MGL_IMAGE_UNIT_CAPACITY];
    /* Name-zero textures are context-local, with one object per target shared
     * across that context's texture units. They are excluded from the
     * share-group texture-name table. */
    struct mgl_texture default_texture_1d;
    struct mgl_texture_level proxy_texture_1d_levels[MGL_MAX_TEXTURE_LEVELS];
    struct mgl_texture_level proxy_texture_2d_levels[MGL_MAX_TEXTURE_LEVELS];
    struct mgl_texture default_texture_2d;
    struct mgl_texture default_texture_rectangle;
    struct mgl_texture default_texture_3d;
    struct mgl_texture default_texture_2d_array;
    struct mgl_texture default_texture_buffer;
    struct mgl_texture default_texture_cube;
    struct mgl_texture default_texture_cube_array;
    GLint unpack_alignment;
    GLint pack_alignment;
    GLint unpack_row_length;
    GLint unpack_skip_rows;
    GLint unpack_skip_pixels;
    GLint unpack_image_height;
    GLint unpack_skip_images;
    GLboolean unpack_swap_bytes;
    GLboolean unpack_lsb_first;
    GLint pack_row_length;
    GLint pack_skip_rows;
    GLint pack_skip_pixels;
    GLint pack_image_height;
    GLint pack_skip_images;
    GLboolean pack_swap_bytes;
    GLboolean pack_lsb_first;

    GLuint arb_vertex_program_binding;
    GLuint arb_fragment_program_binding;
    struct mgl_arb_program arb_default_vertex_program;
    struct mgl_arb_program arb_default_fragment_program;
    GLfloat arb_vertex_env[MGL_ARB_MAX_PROGRAM_PARAMETERS][4];
    GLfloat arb_fragment_env[MGL_ARB_MAX_PROGRAM_PARAMETERS][4];
    GLfloat arb_default_vertex_local[MGL_ARB_MAX_PROGRAM_PARAMETERS][4];
    GLfloat arb_default_fragment_local[MGL_ARB_MAX_PROGRAM_PARAMETERS][4];
    uint64_t arb_vertex_env_revision, arb_fragment_env_revision;
    uint64_t arb_default_vertex_local_revision, arb_default_fragment_local_revision;
    GLint arb_program_error_position;
    char arb_program_error_string[256];

    struct mgl_arb_pair_cache *arb_pair_cache;
    unsigned arb_pair_cache_count;
    struct mgl_program arb_linked_program;
    GLuint arb_native_program;
    GLuint arb_vertex_shader_native;
    GLuint arb_fragment_shader_native;
    GLuint arb_cached_vertex_name;
    GLuint arb_cached_fragment_name;
    GLuint arb_cached_vertex_serial;
    GLuint arb_cached_fragment_serial;
    GLint arb_uniform_vp_env;
    GLint arb_uniform_vp_local;
    GLint arb_uniform_fp_env;
    GLint arb_uniform_fp_local;
    GLint arb_uniform_textures[MGL_TEXTURE_UNITS];
    unsigned arb_vp_env_count;
    unsigned arb_vp_local_count;
    unsigned arb_fp_env_count;
    unsigned arb_fp_local_count;
    uint64_t arb_uploaded_vp_env_revision, arb_uploaded_vp_local_revision;
    uint64_t arb_uploaded_fp_env_revision, arb_uploaded_fp_local_revision;
    unsigned arb_uniform_uploaded_mask;
    GLboolean arb_sampler_uniforms_uploaded;
    GLboolean arb_native_ready;
    GLboolean arb_native_failed;

    struct mgl_fixed_program_variant {
        struct mgl_program program;
        GLuint program_native;
        GLuint vertex_shader_native;
        GLuint fragment_shader_native;
        GLboolean ready;
        unsigned texture_signature;
        struct mgl_fixed_program_variant *next;
    } fixed_program_variants[5];
    struct mgl_fixed_program_variant *fixed_complex_programs;
    unsigned fixed_complex_program_count;
    struct mgl_program fixed_program;
    GLuint fixed_program_native;
    GLuint fixed_vertex_shader_native;
    GLuint fixed_fragment_shader_native;
    GLboolean fixed_program_ready;
    GLboolean fixed_program_failed;
    unsigned fixed_program_key;
    unsigned fixed_program_texture_signature;

    GLuint client_active_texture;
    GLint array_lock_first;
    GLsizei array_lock_count;
    GLfloat generic_current[MGL_MAX_VERTEX_ATTRIBS][4];
    GLint generic_current_i[MGL_MAX_VERTEX_ATTRIBS][4];
    GLuint generic_current_ui[MGL_MAX_VERTEX_ATTRIBS][4];
    GLenum generic_current_type[MGL_MAX_VERTEX_ATTRIBS];
    GLfloat compat_current[MGL_COMPAT_SEMANTIC_COUNT][4];
    GLboolean current_edge_flag;
    GLuint scratch_array_buffer_native;
    GLuint scratch_element_buffer_native;
    unsigned char *scratch_cpu_storage;
    size_t scratch_cpu_capacity;
    GLint native_max_vertex_attribs;
    GLint native_max_vertex_bindings;

    const struct mgl_vertex_array *draw_cached_vao;
    uint32_t draw_enabled_mask;
    GLboolean draw_enabled_mask_valid;
    /* Native GLES program and attribute-pointer cache entries have separate identities from
     * canonical GL bindings. */
    GLuint draw_native_program;
    GLboolean draw_native_program_valid;
    uint32_t draw_native_pointer_valid;
    /* Generic current-attribute values are GLES context state. Cache their four components and
     * input type; a change between float, signed integer, and unsigned integer invalidates the
     * cache entry. */
    uint32_t draw_constant_valid;

    struct {
        unsigned char kind; /* 1: float, 2: signed integer, 3: unsigned integer */
        uint32_t bits[4];
    } draw_constant[MGL_MAX_VERTEX_ATTRIBS];

    struct {
        GLuint buffer;
        GLint size;
        GLenum type;
        GLboolean normalized;
        GLboolean integer;
        GLsizei stride;
        size_t offset;
        GLuint divisor;
    } draw_native_pointer[MGL_MAX_VERTEX_ATTRIBS];

    /* Texture and sampler mutations that change native GLES bindings invalidate realization. */
    GLboolean texture_bindings_dirty;
    GLboolean fixed_texture_binding_override;
    unsigned programmable_texture_units;
    GLint fragment_texture_image_units;
    GLint vertex_texture_image_units;
    unsigned char texture_bindings_dirty_units[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    GLboolean texture_buffer_extension_checked;
    GLboolean texture_buffer_extension_supported;
    /* Native GLES binding cache; every native mutation site maintains its validity. */
    GLuint native_texture_names[MGL_PROGRAMMABLE_TEXTURE_CAPACITY][MGL_NATIVE_TEXTURE_TARGET_COUNT];
    unsigned char native_texture_known[MGL_PROGRAMMABLE_TEXTURE_CAPACITY];
    /* Native GLES texture-unit selector, tracked separately from canonical GL state. */
    GLenum native_active_texture;
    GLboolean native_active_texture_known;
    GLuint native_buffer_bindings[10];
    uint32_t native_buffer_binding_known;
    GLuint buffer_name_cache_canonical[10];
    GLuint buffer_name_cache_native[10];
    uint32_t buffer_name_cache_valid;
    unsigned buffer_name_cache_revision;

    struct mgl_query *queries;
    struct mgl_nv_fence *nv_fences;
    GLuint next_nv_fence_name;
    struct mgl_query *active_occlusion_query;
    struct mgl_transform_feedback *transform_feedbacks;
    GLuint next_transform_feedback_name;
    GLuint transform_feedback_binding;
    GLboolean transform_feedback_active;
    GLboolean transform_feedback_paused;
    GLuint next_query_name;

    GLboolean conditional_render_active;
    GLboolean conditional_render_skip;
    GLuint conditional_render_query;
    GLenum conditional_render_mode;
    GLuint primitive_restart_index;
    GLenum provoking_vertex_mode;
    uint64_t arb_resolved_revision;
    unsigned arb_cached_stage_mask;
};

static inline unsigned mgl_programmable_texture_limit(const struct mgl_context *ctx) {
    return ctx != NULL && ctx->programmable_texture_units != 0u ? ctx->programmable_texture_units
                                                                : MGL_TEXTURE_UNITS;
}

static inline unsigned mgl_native_texture_target_index(GLenum target) {
    switch (target) {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_RECTANGLE:
            return 0u;
        case GL_TEXTURE_3D:
            return 1u;
        case GL_TEXTURE_2D_ARRAY:
            return 2u;
        case GL_TEXTURE_CUBE_MAP:
            return 3u;
        case GL_TEXTURE_CUBE_MAP_ARRAY:
            return 7u;
        case GL_TEXTURE_BUFFER:
            return 4u;
        case GL_TEXTURE_2D_MULTISAMPLE:
            return 5u;
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
            return 6u;
        default:
            return MGL_NATIVE_TEXTURE_TARGET_COUNT;
    }
}

static inline void mgl_native_texture_invalidate_target(struct mgl_context *ctx, unsigned unit,
                                                        GLenum target) {
    unsigned index = mgl_native_texture_target_index(target);
    if (unit >= mgl_programmable_texture_limit(ctx)) {
        unsigned i;
        for (i = 0u; i < mgl_programmable_texture_limit(ctx); ++i)
            ctx->native_texture_known[i] = 0u;
    } else if (index >= MGL_NATIVE_TEXTURE_TARGET_COUNT) {
        ctx->native_texture_known[unit] = 0u;
    } else {
        ctx->native_texture_known[unit] &= (unsigned char)~(1u << index);
    }
}

/* Shared GLES texture selector for application and internal operations.
 * Context binding invalidates the cached native unit. */
static inline void mgl_native_select_active_texture(struct mgl_context *ctx, GLenum texture) {
    if (ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesActiveTexture == NULL)
        return;
    if (ctx->native_active_texture_known && ctx->native_active_texture == texture) {
        mgl_perf_diag_active_texture(ctx, 1);
        return;
    }
    mgl_perf_diag_active_texture(ctx, 0);
    {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_ACTIVE_TEXTURE);
        mgl_render_diag_native_check(ctx, "before_glesActiveTexture", (uint64_t)texture, 0u);
        ctx->native_dispatch->glesActiveTexture(texture);
        mgl_render_diag_native_check(ctx, "glesActiveTexture", (uint64_t)texture, 0u);
    }
    ctx->native_active_texture = texture;
    ctx->native_active_texture_known = GL_TRUE;
}

static inline unsigned mgl_native_dispatch_buffer_binding_index(GLenum target) {
    switch (target) {
        case GL_ARRAY_BUFFER:
            return 0u;
        case GL_PIXEL_PACK_BUFFER:
            return 1u;
        case GL_PIXEL_UNPACK_BUFFER:
            return 2u;
        case GL_COPY_READ_BUFFER:
            return 3u;
        case GL_COPY_WRITE_BUFFER:
            return 4u;
        case GL_UNIFORM_BUFFER:
            return 5u;
        case GL_TRANSFORM_FEEDBACK_BUFFER:
            return 6u;
        case GL_DRAW_INDIRECT_BUFFER:
            return 7u;
        case GL_DISPATCH_INDIRECT_BUFFER:
            return 8u;
        case GL_SHADER_STORAGE_BUFFER:
            return 9u;
        default:
            return 10u;
    }
}

static inline void mgl_native_dispatch_buffer_binding_invalidate(struct mgl_context *ctx,
                                                                 GLenum target) {
    unsigned index = mgl_native_dispatch_buffer_binding_index(target);
    if (ctx != NULL && index < 10u)
        ctx->native_buffer_binding_known &= ~(UINT32_C(1) << index);
}

static inline void mgl_native_dispatch_bind_buffer(struct mgl_context *ctx, GLenum target,
                                                   GLuint name) {
    unsigned index;
    uint32_t bit;
    if (ctx == NULL || ctx->native_dispatch == NULL || ctx->native_dispatch->glesBindBuffer == NULL)
        return;
    index = mgl_native_dispatch_buffer_binding_index(target);
    bit = index < 10u ? UINT32_C(1) << index : 0u;
    if (bit != 0u && (ctx->native_buffer_binding_known & bit) != 0u &&
        ctx->native_buffer_bindings[index] == name) {
        return;
    }
    ctx->native_dispatch->glesBindBuffer(target, name);
    if (bit != 0u) {
        ctx->native_buffer_bindings[index] = name;
        ctx->native_buffer_binding_known |= bit;
    }
}

static inline void mgl_native_dispatch_bind_buffer_base(struct mgl_context *ctx, GLenum target,
                                                        GLuint index, GLuint name) {
    if (ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesBindBufferBase == NULL)
        return;
    ctx->native_dispatch->glesBindBufferBase(target, index, name);
    mgl_native_dispatch_buffer_binding_invalidate(ctx, target);
}

static inline void mgl_native_dispatch_bind_buffer_range(struct mgl_context *ctx, GLenum target,
                                                         GLuint index, GLuint name, GLintptr offset,
                                                         GLsizeiptr size) {
    if (ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesBindBufferRange == NULL)
        return;
    ctx->native_dispatch->glesBindBufferRange(target, index, name, offset, size);
    mgl_native_dispatch_buffer_binding_invalidate(ctx, target);
}

static inline void mgl_texture_mark_all_bindings_dirty(struct mgl_context *ctx) {
    unsigned i;
    for (i = 0u; i < mgl_programmable_texture_limit(ctx); ++i)
        ctx->native_texture_known[i] = 0u;
    ctx->texture_bindings_dirty = GL_TRUE;
    memset(ctx->texture_bindings_dirty_units, 1, mgl_programmable_texture_limit(ctx));
}

static inline void mgl_texture_mark_unit_dirty(struct mgl_context *ctx, unsigned unit) {
    if (unit < mgl_programmable_texture_limit(ctx)) {
        ctx->texture_bindings_dirty = GL_TRUE;
        ctx->texture_bindings_dirty_units[unit] = 1u;
    }
}

/* Shared native program selector; accounts for internal program changes
 * before subsequent draw submissions. */
static inline void mgl_draw_use_native_program(struct mgl_context *ctx, GLuint name) {
    if (ctx == NULL || ctx->native_dispatch == NULL || ctx->native_dispatch->glesUseProgram == NULL)
        return;
    if (!ctx->draw_native_program_valid || ctx->draw_native_program != name) {
        mgl_render_diag_native_check(ctx, "before_glesUseProgram", (uint64_t)name, 0u);
        ctx->native_dispatch->glesUseProgram(name);
        mgl_render_diag_native_check(ctx, "glesUseProgram", (uint64_t)name, 0u);
        ctx->draw_native_program = name;
        ctx->draw_native_program_valid = GL_TRUE;
    }
}

struct mgl_share_group *mgl_share_group_create(void);
void mgl_share_group_retain(struct mgl_share_group *group);
void mgl_share_group_release(struct mgl_share_group *group);

void mgl_pipeline_cleanup(struct mgl_context *ctx, int native_available);
struct mgl_context *mgl_context_create_offscreen(struct mgl_native_dispatch *native,
                                                 struct mgl_context *share, int width, int height);
void mgl_context_destroy(struct mgl_context *ctx);
int mgl_context_make_current(struct mgl_context *ctx);
int mgl_context_configure_pbuffers(struct mgl_context *ctx, GLsizei draw_width, GLsizei draw_height,
                                   GLsizei read_width, GLsizei read_height);
int mgl_context_use_external_surface(struct mgl_context *ctx, MGL_EGLSurface surface, GLsizei width,
                                     GLsizei height);
void mgl_context_set_creation_flags(struct mgl_context *ctx, GLbitfield flags);
void mgl_context_set_profile_mask(struct mgl_context *ctx, GLbitfield profile);
struct mgl_context *mgl_context_current(void);
void mgl_context_set_current_for_test(struct mgl_context *ctx);
void mgl_context_init_image_units(struct mgl_context *ctx);
GLuint mgl_context_image_unit_limit(struct mgl_context *ctx);
GLuint mgl_context_vertex_binding_limit(struct mgl_context *ctx);

#endif

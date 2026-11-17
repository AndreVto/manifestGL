#include "core/capabilities.h"
#include "core/context.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

static const GLenum mgl_compressed_texture_formats[] = {GL_COMPRESSED_RED_RGTC1,
                                                        GL_COMPRESSED_SIGNED_RED_RGTC1,
                                                        GL_COMPRESSED_RG_RGTC2,
                                                        GL_COMPRESSED_SIGNED_RG_RGTC2,
                                                        GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                                                        GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
                                                        GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
                                                        GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
                                                        GL_COMPRESSED_SRGB_S3TC_DXT1_EXT,
                                                        GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT,
                                                        GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT,
                                                        GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,
                                                        GL_COMPRESSED_RGBA_BPTC_UNORM,
                                                        GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM,
                                                        GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT,
                                                        GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT};

/* Extension advertisement is independent of exported diagnostic entry points. */
static const char *const mgl_extensions[] = {
    "GL_ARB_ES2_compatibility",
    "GL_ARB_ES3_1_compatibility",
    "GL_ARB_ES3_2_compatibility",
    "GL_ARB_ES3_compatibility",
    "GL_ARB_base_instance",
    "GL_ARB_blend_func_extended",
    "GL_ARB_buffer_storage",
    "GL_ARB_clear_buffer_object",
    "GL_ARB_clear_texture",
    "GL_ARB_clip_control",
    "GL_ARB_color_buffer_float",
    "GL_ARB_compute_shader",
    "GL_ARB_copy_buffer",
    "GL_ARB_copy_image",
    "GL_ARB_debug_output",
    "GL_ARB_depth_buffer_float",
    "GL_ARB_depth_texture",
    "GL_ARB_direct_state_access",
    "GL_ARB_draw_buffers",
    "GL_ARB_draw_buffers_blend",
    "GL_ARB_draw_elements_base_vertex",
    "GL_ARB_draw_indirect",
    "GL_ARB_draw_instanced",
    "GL_ARB_explicit_attrib_location",
    "GL_ARB_fragment_program",
    "GL_ARB_fragment_shader",
    "GL_ARB_framebuffer_no_attachments",
    "GL_ARB_framebuffer_object",
    "GL_ARB_framebuffer_sRGB",
    "GL_ARB_geometry_shader4",
    "GL_ARB_get_program_binary",
    "GL_ARB_get_texture_sub_image",
    "GL_ARB_half_float_vertex",
    "GL_ARB_indirect_parameters",
    "GL_ARB_instanced_arrays",
    "GL_ARB_internalformat_query",
    "GL_ARB_internalformat_query2",
    "GL_ARB_invalidate_subdata",
    "GL_ARB_map_buffer_range",
    "GL_ARB_multi_bind",
    "GL_ARB_multi_draw_indirect",
    "GL_ARB_multisample",
    "GL_ARB_multitexture",
    "GL_ARB_occlusion_query",
    "GL_ARB_occlusion_query2",
    "GL_ARB_pixel_buffer_object",
    "GL_ARB_point_parameters",
    "GL_ARB_point_sprite",
    "GL_ARB_polygon_offset_clamp",
    "GL_ARB_program_interface_query",
    "GL_ARB_provoking_vertex",
    "GL_ARB_query_buffer_object",
    "GL_ARB_robustness",
    "GL_ARB_sample_shading",
    "GL_ARB_sampler_objects",
    "GL_ARB_separate_shader_objects",
    "GL_ARB_shader_atomic_counters",
    "GL_ARB_shader_image_load_store",
    "GL_ARB_shader_objects",
    "GL_ARB_shader_storage_buffer_object",
    "GL_ARB_shading_language_100",
    "GL_ARB_sync",
    "GL_ARB_tessellation_shader",
    "GL_ARB_texture_barrier",
    "GL_ARB_texture_border_clamp",
    "GL_ARB_texture_buffer_object",
    "GL_ARB_texture_buffer_object_rgb32",
    "GL_ARB_texture_buffer_range",
    "GL_ARB_texture_compression",
    "GL_ARB_texture_compression_bptc",
    "GL_ARB_texture_compression_rgtc",
    "GL_ARB_texture_cube_map",
    "GL_ARB_texture_cube_map_array",
    "GL_ARB_texture_env_add",
    "GL_ARB_texture_env_combine",
    "GL_ARB_texture_env_crossbar",
    "GL_ARB_texture_env_dot3",
    "GL_ARB_texture_filter_anisotropic",
    "GL_ARB_texture_float",
    "GL_ARB_texture_gather",
    "GL_ARB_texture_mirrored_repeat",
    "GL_ARB_texture_multisample",
    "GL_ARB_texture_non_power_of_two",
    "GL_ARB_texture_rectangle",
    "GL_ARB_texture_rg",
    "GL_ARB_texture_rgb10_a2ui",
    "GL_ARB_texture_stencil8",
    "GL_ARB_texture_storage",
    "GL_ARB_texture_storage_multisample",
    "GL_ARB_texture_swizzle",
    "GL_ARB_texture_view",
    "GL_ARB_timer_query",
    "GL_ARB_transform_feedback2",
    "GL_ARB_transform_feedback3",
    "GL_ARB_transform_feedback_instanced",
    "GL_ARB_transpose_matrix",
    "GL_ARB_uniform_buffer_object",
    "GL_ARB_vertex_array_object",
    "GL_ARB_vertex_attrib_binding",
    "GL_ARB_vertex_buffer_object",
    "GL_ARB_vertex_program",
    "GL_ARB_vertex_shader",
    "GL_ARB_vertex_type_2_10_10_10_rev",
    "GL_ARB_viewport_array",
    "GL_ARB_window_pos",
    "GL_ATI_separate_stencil",
    "GL_EXT_bgra",
    "GL_EXT_blend_color",
    "GL_EXT_blend_equation_separate",
    "GL_EXT_blend_func_separate",
    "GL_EXT_blend_minmax",
    "GL_EXT_blend_subtract",
    "GL_EXT_compiled_vertex_array",
    "GL_EXT_depth_bounds_test",
    "GL_EXT_draw_buffers2",
    "GL_EXT_draw_range_elements",
    "GL_EXT_fog_coord",
    "GL_EXT_framebuffer_blit",
    "GL_EXT_framebuffer_multisample",
    "GL_EXT_framebuffer_object",
    "GL_EXT_framebuffer_sRGB",
    "GL_EXT_gpu_program_parameters",
    "GL_EXT_gpu_shader4",
    "GL_EXT_packed_depth_stencil",
    "GL_EXT_packed_float",
    "GL_EXT_packed_pixels",
    "GL_EXT_point_parameters",
    "GL_EXT_polygon_offset_clamp",
    "GL_EXT_provoking_vertex",
    "GL_EXT_secondary_color",
    "GL_EXT_stencil_two_side",
    "GL_EXT_stencil_wrap",
    "GL_EXT_texture3D",
    "GL_EXT_texture_array",
    "GL_EXT_texture_compression_dxt1",
    "GL_EXT_texture_compression_rgtc",
    "GL_EXT_texture_compression_s3tc",
    "GL_EXT_texture_edge_clamp",
    "GL_EXT_texture_env_add",
    "GL_EXT_texture_env_combine",
    "GL_EXT_texture_env_crossbar",
    "GL_EXT_texture_env_dot3",
    "GL_EXT_texture_filter_anisotropic",
    "GL_EXT_texture_integer",
    "GL_EXT_texture_lod_bias",
    "GL_EXT_texture_rectangle",
    "GL_EXT_texture_sRGB",
    "GL_EXT_texture_shared_exponent",
    "GL_EXT_texture_snorm",
    "GL_KHR_blend_equation_advanced",
    "GL_KHR_debug",
    "GL_NV_fence",
    "GL_NV_half_float",
    "GL_NV_texture_barrier"
};

enum {
    MGL_EXT_ANISOTROPY = 1u << 0,
    MGL_EXT_TIMER_QUERY = 1u << 1,
    MGL_EXT_NV_FENCE = 1u << 2,
    MGL_EXT_TEXTURE_VIEW = 1u << 3,
    MGL_EXT_ADVANCED_BLEND = 1u << 4,
    MGL_EXT_TEXTURE_BARRIER = 1u << 5,
    MGL_EXT_CLIP_CONTROL = 1u << 6,
    MGL_EXT_DEPTH_BOUNDS = 1u << 7,
    MGL_EXT_POLYGON_OFFSET_CLAMP = 1u << 8,
    MGL_EXT_TEXTURE_BUFFER = 1u << 9,
    MGL_EXT_TEXTURE_BUFFER_RANGE = 1u << 10,
    MGL_EXT_DUAL_SOURCE_BLEND = 1u << 11,
    MGL_EXT_FRAMEBUFFER_SRGB = 1u << 12,
    MGL_EXT_FRAMEBUFFER_NO_ATTACHMENTS = 1u << 13,
    MGL_EXT_ES32_COMPATIBILITY = 1u << 14
};

static int has_native_extension(const struct mgl_native_dispatch *native, const char *name) {
    return mgl_native_dispatch_has_gles_extension(native, name);
}

static unsigned available_extensions(const struct mgl_context *ctx) {
    const struct mgl_native_dispatch *native = ctx != NULL ? ctx->native_dispatch : NULL;
    unsigned flags = 0u;
    if (native == NULL)
        return 0u;
    if (has_native_extension(native, "GL_EXT_texture_filter_anisotropic"))
        flags |= MGL_EXT_ANISOTROPY;
    if (native->glesFramebufferParameteri != NULL)
        flags |= MGL_EXT_FRAMEBUFFER_NO_ATTACHMENTS;
    if (native->glesPrimitiveBoundingBox != NULL)
        flags |= MGL_EXT_ES32_COMPATIBILITY;
    if (native->glesGenQueries != NULL && native->glesDeleteQueries != NULL &&
        native->glesBeginQuery != NULL && native->glesEndQuery != NULL &&
        native->glesGetQueryObjectuiv != NULL && native->glesGetQueryObjectui64vEXT != NULL &&
        native->glesQueryCounterEXT != NULL && native->glesGetInteger64v != NULL &&
        has_native_extension(native, "GL_EXT_disjoint_timer_query"))
        flags |= MGL_EXT_TIMER_QUERY;
    if (native->glesFenceSync != NULL && native->glesClientWaitSync != NULL &&
        native->glesDeleteSync != NULL)
        flags |= MGL_EXT_NV_FENCE;
    if (native->glesTextureView != NULL && native->glesGenTextures != NULL &&
        native->glesTexStorage2D != NULL &&
        (has_native_extension(native, "GL_EXT_texture_view") ||
         has_native_extension(native, "GL_OES_texture_view")))
        flags |= MGL_EXT_TEXTURE_VIEW;
    if (native->glesBlendBarrier != NULL && native->glesBlendEquation != NULL)
        flags |= MGL_EXT_ADVANCED_BLEND;
    if (native->glesTextureBarrier != NULL &&
        (has_native_extension(native, "GL_NV_texture_barrier") ||
         has_native_extension(native, "GL_EXT_texture_barrier") ||
         has_native_extension(native, "GL_ARB_texture_barrier")))
        flags |= MGL_EXT_TEXTURE_BARRIER;
    if (native->glesClipControlEXT != NULL && has_native_extension(native, "GL_EXT_clip_control"))
        flags |= MGL_EXT_CLIP_CONTROL;
    if (native->glesDepthBoundsEXT != NULL &&
        has_native_extension(native, "GL_EXT_depth_bounds_test"))
        flags |= MGL_EXT_DEPTH_BOUNDS;
    if (native->glesPolygonOffsetClamp != NULL &&
        (has_native_extension(native, "GL_EXT_polygon_offset_clamp") ||
         has_native_extension(native, "GL_ARB_polygon_offset_clamp")))
        flags |= MGL_EXT_POLYGON_OFFSET_CLAMP;
    if (native->glesTexBufferEXT != NULL) {
        flags |= MGL_EXT_TEXTURE_BUFFER;
        if (native->glesTexBufferRangeEXT != NULL)
            flags |= MGL_EXT_TEXTURE_BUFFER_RANGE;
    }
    if (has_native_extension(native, "GL_EXT_blend_func_extended") &&
        native->glesBlendFuncSeparate != NULL && native->glesBlendFuncSeparatei != NULL)
        flags |= MGL_EXT_DUAL_SOURCE_BLEND;
    if (has_native_extension(native, "GL_EXT_sRGB_write_control") &&
        native->glesEnable != NULL && native->glesDisable != NULL)
        flags |= MGL_EXT_FRAMEBUFFER_SRGB;
    return flags;
}

static int extension_visible(size_t index, unsigned flags) {
    const char *name = mgl_extensions[index];
    static const struct {
        const char *name;
        unsigned flag;
    } conditional[] = {
        {"GL_ARB_texture_filter_anisotropic", MGL_EXT_ANISOTROPY},
        {"GL_EXT_texture_filter_anisotropic", MGL_EXT_ANISOTROPY},
        {"GL_ARB_timer_query", MGL_EXT_TIMER_QUERY},
        {"GL_NV_fence", MGL_EXT_NV_FENCE},
        {"GL_ARB_texture_view", MGL_EXT_TEXTURE_VIEW},
        {"GL_KHR_blend_equation_advanced", MGL_EXT_ADVANCED_BLEND},
        {"GL_ARB_texture_barrier", MGL_EXT_TEXTURE_BARRIER},
        {"GL_NV_texture_barrier", MGL_EXT_TEXTURE_BARRIER},
        {"GL_ARB_clip_control", MGL_EXT_CLIP_CONTROL},
        {"GL_EXT_depth_bounds_test", MGL_EXT_DEPTH_BOUNDS},
        {"GL_ARB_polygon_offset_clamp", MGL_EXT_POLYGON_OFFSET_CLAMP},
        {"GL_EXT_polygon_offset_clamp", MGL_EXT_POLYGON_OFFSET_CLAMP},
        {"GL_ARB_texture_buffer_object", MGL_EXT_TEXTURE_BUFFER},
        {"GL_ARB_texture_buffer_object_rgb32", MGL_EXT_TEXTURE_BUFFER},
        {"GL_ARB_texture_buffer_range", MGL_EXT_TEXTURE_BUFFER_RANGE},
        {"GL_ARB_blend_func_extended", MGL_EXT_DUAL_SOURCE_BLEND},
        {"GL_ARB_framebuffer_no_attachments", MGL_EXT_FRAMEBUFFER_NO_ATTACHMENTS},
        {"GL_ARB_ES3_2_compatibility", MGL_EXT_ES32_COMPATIBILITY},
        {"GL_ARB_framebuffer_sRGB", MGL_EXT_FRAMEBUFFER_SRGB},
        {"GL_EXT_framebuffer_sRGB", MGL_EXT_FRAMEBUFFER_SRGB}
    };
    size_t i;
    for (i = 0u; i < sizeof(conditional) / sizeof(conditional[0]); ++i)
        if (strcmp(name, conditional[i].name) == 0)
            return (flags & conditional[i].flag) != 0u;
    return 1;
}

struct extension_string_cache {
    struct extension_string_cache *next;
    unsigned flags;
    GLubyte text[];
};

static pthread_mutex_t extension_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static struct extension_string_cache *extension_cache;
static const GLubyte empty_extension_string[] = "";

static const GLubyte *extension_string_for_flags(unsigned flags) {
    struct extension_string_cache *entry;
    size_t used = 0u, i;
    GLubyte *out;
    const GLubyte *result;
    (void)pthread_mutex_lock(&extension_cache_lock);
    for (entry = extension_cache; entry != NULL; entry = entry->next) {
        if (entry->flags == flags) {
            result = entry->text;
            (void)pthread_mutex_unlock(&extension_cache_lock);
            return result;
        }
    }
    for (i = 0u; i < sizeof(mgl_extensions) / sizeof(mgl_extensions[0]); ++i) {
        if (!extension_visible(i, flags))
            continue;
        used += strlen(mgl_extensions[i]) + (used != 0u ? 1u : 0u);
    }
    entry = malloc(sizeof(*entry) + used + 1u);
    if (entry == NULL) {
        (void)pthread_mutex_unlock(&extension_cache_lock);
        return empty_extension_string;
    }
    entry->flags = flags;
    out = entry->text;
    used = 0u;
    for (i = 0u; i < sizeof(mgl_extensions) / sizeof(mgl_extensions[0]); ++i) {
        size_t length;
        if (!extension_visible(i, flags))
            continue;
        if (used != 0u)
            out[used++] = ' ';
        length = strlen(mgl_extensions[i]);
        memcpy(out + used, mgl_extensions[i], length);
        used += length;
    }
    out[used] = '\0';
    /* Stable glGetString pointers remain valid as other contexts query different capabilities. */
    entry->next = extension_cache;
    extension_cache = entry;
    result = entry->text;
    (void)pthread_mutex_unlock(&extension_cache_lock);
    return result;
}

GLuint mgl_capability_extension_count(const struct mgl_context *ctx) {
    unsigned flags = available_extensions(ctx);
    GLuint count = 0u;
    size_t i;
    for (i = 0u; i < sizeof(mgl_extensions) / sizeof(mgl_extensions[0]); ++i)
        if (extension_visible(i, flags))
            ++count;
    return count;
}

const GLubyte *mgl_capability_extension(const struct mgl_context *ctx, GLuint index) {
    unsigned flags = available_extensions(ctx);
    size_t i;
    for (i = 0u; i < sizeof(mgl_extensions) / sizeof(mgl_extensions[0]); ++i) {
        if (!extension_visible(i, flags))
            continue;
        if (index == 0u)
            return (const GLubyte *)mgl_extensions[i];
        --index;
    }
    return NULL;
}

const GLubyte *mgl_capability_extension_string(const struct mgl_context *ctx) {
    return extension_string_for_flags(available_extensions(ctx));
}

GLuint mgl_capability_compressed_texture_format_count(void) {
    return (GLuint)(sizeof(mgl_compressed_texture_formats) /
                    sizeof(mgl_compressed_texture_formats[0]));
}

GLenum mgl_capability_compressed_texture_format(GLuint index) {
    if (index >= mgl_capability_compressed_texture_format_count())
        return 0u;
    return mgl_compressed_texture_formats[index];
}

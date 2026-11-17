#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "core/capabilities.h"
#include "native/dispatch.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static const char *native_extensions = "";

static const GLubyte *fake_gpu_identity(GLenum name) {
    if (name == GL_VENDOR)
        return (const GLubyte *)"Example Vendor";
    if (name == GL_RENDERER)
        return (const GLubyte *)"Example GPU";
    if (name == GL_EXTENSIONS)
        return (const GLubyte *)native_extensions;
    return NULL;
}

static GLenum last_limit_query;

static void fake_limit_query(GLenum pname, GLint *result) {
    last_limit_query = pname;
    *result = (GLint)317;
}

static int has_extension_token(const char *list, const char *name) {
    size_t n = strlen(name);
    const char *p = list;
    while ((p = strstr(p, name)) != NULL) {
        if ((p == list || p[-1] == ' ') && (p[n] == '\0' || p[n] == ' '))
            return 1;
        p += n;
    }
    return 0;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "capability reporting failure %d: %s\n", n, message);
    return n;
}

static GLsync fake_fence(GLenum condition, GLbitfield flags) {
    (void)condition;
    (void)flags;
    return (GLsync)(uintptr_t)1u;
}

static void fake_delete_sync(GLsync handle) { (void)handle; }
static GLenum fake_wait_sync(GLsync handle, GLbitfield flags, GLuint64 timeout) {
    (void)handle;
    (void)flags;
    (void)timeout;
    return GL_ALREADY_SIGNALED;
}
static void fake_texture_view(GLuint texture, GLenum target, GLuint source, GLenum format,
                              GLuint base_level, GLuint levels, GLuint base_layer,
                              GLuint layers) {
    (void)texture;
    (void)target;
    (void)source;
    (void)format;
    (void)base_level;
    (void)levels;
    (void)base_layer;
    (void)layers;
}
static void fake_gen_textures(GLsizei n, GLuint *textures) {
    (void)n;
    (void)textures;
}
static void fake_tex_storage2d(GLenum target, GLsizei levels, GLenum format, GLsizei width,
                               GLsizei height) {
    (void)target;
    (void)levels;
    (void)format;
    (void)width;
    (void)height;
}
static void fake_blend_barrier(void) {}
static void fake_blend_equation(GLenum mode) { (void)mode; }
static void fake_enable(GLenum cap) { (void)cap; }
static void fake_disable(GLenum cap) { (void)cap; }
static void fake_blend_func_separate(GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha) {
    (void)src_rgb;
    (void)dst_rgb;
    (void)src_alpha;
    (void)dst_alpha;
}
static void fake_blend_func_separatei(GLuint index, GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha) {
    (void)index;
    (void)src_rgb;
    (void)dst_rgb;
    (void)src_alpha;
    (void)dst_alpha;
}
static void fake_texture_barrier(void) {}
static void fake_clip_control(GLenum origin, GLenum depth) {
    (void)origin;
    (void)depth;
}
static void fake_depth_bounds(GLclampd lower, GLclampd upper) {
    (void)lower;
    (void)upper;
}
static void fake_polygon_offset_clamp(GLfloat factor, GLfloat units, GLfloat clamp) {
    (void)factor;
    (void)units;
    (void)clamp;
}
static void fake_framebuffer_parameter(GLenum target, GLenum pname, GLint param) {
    (void)target;
    (void)pname;
    (void)param;
}
static void fake_primitive_bounding_box(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
                                        GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW) {
    (void)minX;
    (void)minY;
    (void)minZ;
    (void)minW;
    (void)maxX;
    (void)maxY;
    (void)maxZ;
    (void)maxW;
}
static void fake_tex_buffer(GLenum target, GLenum format, GLuint buffer) {
    (void)target;
    (void)format;
    (void)buffer;
}
static void fake_tex_buffer_range(GLenum target, GLenum format, GLuint buffer, GLintptr offset,
                                  GLsizeiptr size) {
    (void)target;
    (void)format;
    (void)buffer;
    (void)offset;
    (void)size;
}
static void fake_gen_queries(GLsizei n, GLuint *out) { (void)n; (void)out; }
static void fake_delete_queries(GLsizei n, const GLuint *names) { (void)n; (void)names; }
static void fake_begin_query(GLenum target, GLuint name) { (void)target; (void)name; }
static void fake_end_query(GLenum target) { (void)target; }
static void fake_get_query(GLuint name, GLenum pname, GLuint *value) {
    (void)name;
    (void)pname;
    (void)value;
}
static void fake_query_counter(GLuint name, GLenum target) { (void)name; (void)target; }
static void fake_get_query64(GLuint name, GLenum pname, GLuint64 *value) {
    (void)name;
    (void)pname;
    (void)value;
}
static void fake_get_integer64(GLenum pname, GLint64 *value) {
    (void)pname;
    (void)value;
}

int main(void) {
    struct mgl_native_dispatch native;
    struct mgl_context ctx;
    const GLubyte *legacy;
    GLint scratch = 0, count = 0, flags = -1, profile = 0;
    GLint compressed_count = 0;
    GLint compressed_formats[64] = {0};
    GLint i;

    memset(&native, 0, sizeof(native));
    native.glesGetIntegerv = fake_limit_query;
    native.glesGetString = fake_gpu_identity;
    memset(&ctx, 0, sizeof(ctx));
    ctx.native_dispatch = &native;
    ctx.profile_mask = GL_CONTEXT_COMPATIBILITY_PROFILE_BIT;
    mgl_error_init(&ctx.errors);
    mgl_context_set_current_for_test(&ctx);

    if (strcmp((const char *)glGetString(GL_VENDOR), "Example Vendor") != 0)
        return fail(18, "GPU vendor pass-through");
    if (strcmp((const char *)glGetString(GL_RENDERER), "Example GPU") != 0)
        return fail(19, "GPU renderer pass-through");

    {
        GLint array_layers = 0;
        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &array_layers);
        if (array_layers != 317 || last_limit_query != GL_MAX_ARRAY_TEXTURE_LAYERS ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(16, "GL_MAX_ARRAY_TEXTURE_LAYERS must expose real GLES limit");
        ctx.native_dispatch = NULL;
        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &array_layers);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
            return fail(17, "missing GLES native must be reported, not fabricated");
        ctx.native_dispatch = &native;
    }

    {
        const GLenum limits[] = {GL_MAX_RENDERBUFFER_SIZE, GL_MAX_UNIFORM_BLOCK_SIZE};
        size_t j;
        for (j = 0u; j < sizeof(limits) / sizeof(limits[0]); ++j) {
            GLint limit = -1;
            glGetIntegerv(limits[j], &limit);
            if (limit != 317 || last_limit_query != limits[j] ||
                mgl_error_take(&ctx.errors) != GL_NO_ERROR)
                return fail(20, "desktop framebuffer/UBO limit forwarding");
        }
        ctx.native_dispatch = NULL;
        glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &scratch);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
            return fail(21, "framebuffer limit requires a GLES native");
        glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &scratch);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
            return fail(22, "uniform block size requires a GLES native");
        ctx.native_dispatch = &native;
    }

    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    if (flags != 0 || profile != (GLint)GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)
        return fail(3, "context flags/profile integer queries");

    ctx.profile_mask = GL_CONTEXT_CORE_PROFILE_BIT;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    if (profile != (GLint)GL_CONTEXT_CORE_PROFILE_BIT)
        return fail(23, "core context profile query");
    if (glGetString(GL_EXTENSIONS) != NULL || mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(24, "core profile rejects legacy extension query");
    if (glGetStringi(GL_EXTENSIONS, 0u) == NULL || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(25, "core profile exposes indexed extensions");
    ctx.profile_mask = GL_CONTEXT_COMPATIBILITY_PROFILE_BIT;

    legacy = glGetString(GL_EXTENSIONS);
    if (legacy == NULL || ((const char *)legacy)[0] == '\0')
        return fail(4, "legacy extension string");
    if (((const char *)legacy)[strlen((const char *)legacy) - 1u] == ' ')
        return fail(5, "extension string trailing separator");
    {
        size_t counted_tokens = 0u;
        const char *scan = (const char *)legacy;
        const GLubyte *previous = NULL;
        while (*scan != '\0') {
            while (*scan == ' ')
                ++scan;
            if (*scan == '\0')
                break;
            ++counted_tokens;
            while (*scan != '\0' && *scan != ' ')
                ++scan;
        }
        if (counted_tokens != (size_t)count)
            return fail(14, "legacy extension list truncated or count inconsistent");
        for (i = 0; i < count; ++i) {
            const GLubyte *ext = glGetStringi(GL_EXTENSIONS, (GLuint)i);
            if (ext == NULL || !has_extension_token((const char *)legacy, (const char *)ext))
                return fail(6, "indexed/legacy extension consistency");
            if (previous != NULL && strcmp((const char *)previous, (const char *)ext) >= 0)
                return fail(15, "extensions must be sorted without duplicates");
            previous = ext;
        }
    }
    {
        const char *required[] = {
            "GL_ARB_ES2_compatibility",
            "GL_ARB_ES3_compatibility",
            "GL_ARB_ES3_1_compatibility",
            "GL_ARB_base_instance",
            "GL_ARB_buffer_storage",
            "GL_ARB_clear_buffer_object",
            "GL_ARB_clear_texture",
            "GL_ARB_color_buffer_float",
            "GL_ARB_debug_output",
            "GL_ARB_depth_buffer_float",
            "GL_ARB_compute_shader",
            "GL_ARB_copy_image",
            "GL_ARB_copy_buffer",
            "GL_ARB_direct_state_access",
            "GL_ARB_draw_buffers_blend",
            "GL_ARB_draw_indirect",
            "GL_ARB_point_parameters",
            "GL_ARB_get_texture_sub_image",
            "GL_ATI_separate_stencil",
            "GL_ARB_internalformat_query",
            "GL_ARB_internalformat_query2",
            "GL_ARB_invalidate_subdata",
            "GL_ARB_multi_bind",
            "GL_ARB_multi_draw_indirect",
            "GL_ARB_occlusion_query2",
            "GL_ARB_program_interface_query",
            "GL_ARB_robustness",
            "GL_ARB_sample_shading",
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shader_image_load_store",
            "GL_ARB_shader_storage_buffer_object",
            "GL_ARB_tessellation_shader",
            "GL_ARB_texture_border_clamp",
            "GL_ARB_texture_multisample",
            "GL_ARB_texture_rectangle",
            "GL_ARB_texture_storage_multisample",
            "GL_ARB_texture_swizzle",
            "GL_ARB_transform_feedback2",
            "GL_ARB_transform_feedback3",
            "GL_ARB_vertex_attrib_binding",
            "GL_ARB_viewport_array",
            "GL_ARB_window_pos",
            "GL_EXT_gpu_program_parameters",
            "GL_EXT_gpu_shader4",
            "GL_EXT_packed_float",
            "GL_EXT_provoking_vertex",
            "GL_EXT_texture_snorm",
            "GL_EXT_texture_shared_exponent",
            "GL_EXT_draw_buffers2",
            "GL_EXT_secondary_color",
            "GL_EXT_texture_array",
            "GL_EXT_texture_compression_dxt1",
            "GL_EXT_texture_integer",
            "GL_NV_half_float"
        };
        size_t j;
        for (j = 0u; j < sizeof(required) / sizeof(required[0]); ++j)
            if (!has_extension_token((const char *)legacy, required[j]))
                return fail(26, "supported extension missing from inventory");
        if (has_extension_token((const char *)legacy, "GL_ARB_gpu_shader_fp64") ||
            has_extension_token((const char *)legacy, "GL_ARB_shader_subroutine") ||
            has_extension_token((const char *)legacy, "GL_APPLE_fence"))
            return fail(33, "unsupported extension families must remain absent");
        if (has_extension_token((const char *)legacy, "GL_NV_fence") ||
            has_extension_token((const char *)legacy, "GL_ARB_timer_query") ||
            has_extension_token((const char *)legacy, "GL_EXT_texture_filter_anisotropic") ||
            has_extension_token((const char *)legacy, "GL_ARB_texture_filter_anisotropic") ||
            has_extension_token((const char *)legacy, "GL_ARB_ES3_2_compatibility") ||
            has_extension_token((const char *)legacy, "GL_ARB_framebuffer_no_attachments"))
            return fail(27, "native-dependent extensions advertised without support");
        {
            const char *conditional[] = {
                "GL_ARB_blend_func_extended",
                "GL_ARB_ES3_2_compatibility",
                "GL_ARB_framebuffer_no_attachments",
                "GL_ARB_texture_buffer_object_rgb32",
                "GL_EXT_polygon_offset_clamp",
                "GL_ARB_clip_control",
                "GL_ARB_framebuffer_sRGB",
                "GL_ARB_polygon_offset_clamp",
                "GL_ARB_texture_barrier",
                "GL_ARB_texture_buffer_object",
                "GL_ARB_texture_buffer_range",
                "GL_ARB_texture_view",
                "GL_EXT_depth_bounds_test",
                "GL_EXT_framebuffer_sRGB",
                "GL_KHR_blend_equation_advanced",
                "GL_NV_texture_barrier"
            };
            const char *exts = (const char *)glGetString(GL_EXTENSIONS);
            const GLubyte *baseline_string = glGetString(GL_EXTENSIONS);
            size_t k;
            for (k = 0u; k < sizeof(conditional) / sizeof(conditional[0]); ++k)
                if (has_extension_token(exts, conditional[k]))
                    return fail(34, "backend-dependent extension visible without provider");
            native.glesFramebufferParameteri = fake_framebuffer_parameter;
            native.glesPrimitiveBoundingBox = fake_primitive_bounding_box;
            native.glesTextureView = fake_texture_view;
            native.glesGenTextures = fake_gen_textures;
            native.glesTexStorage2D = fake_tex_storage2d;
            native.glesBlendBarrier = fake_blend_barrier;
            native.glesBlendEquation = fake_blend_equation;
            native.glesTextureBarrier = fake_texture_barrier;
            native.glesClipControlEXT = fake_clip_control;
            native.glesDepthBoundsEXT = fake_depth_bounds;
            native.glesPolygonOffsetClamp = fake_polygon_offset_clamp;
            native.glesTexBufferEXT = fake_tex_buffer;
            native.glesEnable = fake_enable;
            native.glesDisable = fake_disable;
            native.glesBlendFuncSeparate = fake_blend_func_separate;
            native.glesBlendFuncSeparatei = fake_blend_func_separatei;
            native.glesTexBufferRangeEXT = fake_tex_buffer_range;
            for (k = 0u; k < sizeof(conditional) / sizeof(conditional[0]); ++k) {
                if (strcmp(conditional[k], "GL_KHR_blend_equation_advanced") == 0 ||
                    strcmp(conditional[k], "GL_ARB_texture_buffer_object") == 0 ||
                    strcmp(conditional[k], "GL_ARB_texture_buffer_range") == 0 ||
                    strcmp(conditional[k], "GL_ARB_texture_buffer_object_rgb32") == 0 ||
                    strcmp(conditional[k], "GL_ARB_ES3_2_compatibility") == 0 ||
                    strcmp(conditional[k], "GL_ARB_framebuffer_no_attachments") == 0)
                    continue;
                if (has_extension_token((const char *)glGetString(GL_EXTENSIONS), conditional[k]))
                    return fail(35, "backend-dependent extension visible with pointer only");
            }
            if (!has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                     "GL_ARB_ES3_2_compatibility") ||
                !has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                     "GL_ARB_framebuffer_no_attachments") ||
                !has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                     "GL_ARB_texture_buffer_object_rgb32"))
                return fail(46, "core GLES feature dispatch must be advertised");
            if (!has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                     "GL_KHR_blend_equation_advanced"))
                return fail(42, "GLES 3.2 advanced blending requires no extension string");
            if (!has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                     "GL_ARB_texture_buffer_object") ||
                !has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                     "GL_ARB_texture_buffer_range"))
                return fail(43, "core texture-buffer entry points require no extension string");
            native_extensions = "GL_EXT_blend_func_extended GL_EXT_sRGB_write_control "
                                "GL_EXT_texture_view GL_KHR_blend_equation_advanced "
                                "GL_EXT_texture_barrier GL_EXT_clip_control "
                                "GL_EXT_depth_bounds_test GL_EXT_polygon_offset_clamp "
                                "GL_EXT_texture_buffer";
            native.glesTexBufferRangeEXT = NULL;
            native.glesTextureView = NULL;
            if (!has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                     "GL_ARB_blend_func_extended") ||
                !has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                     "GL_EXT_framebuffer_sRGB") ||
                !has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                     "GL_ARB_framebuffer_sRGB"))
                return fail(44, "conditional blending and framebuffer sRGB omitted");
            if (has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                    "GL_ARB_texture_view") ||
                has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                    "GL_ARB_texture_buffer_range"))
                return fail(36, "native entry points are required for advertised features");
            native.glesTextureView = fake_texture_view;
            native.glesTexBufferRangeEXT = fake_tex_buffer_range;
            for (k = 0u; k < sizeof(conditional) / sizeof(conditional[0]); ++k)
                if (!has_extension_token((const char *)glGetString(GL_EXTENSIONS), conditional[k]))
                    return fail(37, "supported native feature omitted from extension list");
            if (strcmp((const char *)baseline_string,
                       (const char *)glGetString(GL_EXTENSIONS)) == 0 ||
                has_extension_token((const char *)baseline_string, "GL_ARB_texture_view"))
                return fail(38, "extension strings must stay stable across provider changes");
            if (mgl_capability_extension_count(&ctx) <= (GLuint)count)
                return fail(39, "backend-specific capabilities must change indexed count");
            {
                GLuint index, advertised = mgl_capability_extension_count(&ctx);
                const GLubyte *previous = NULL;
                const char *current = (const char *)glGetString(GL_EXTENSIONS);
                for (index = 0u; index < advertised; ++index) {
                    const GLubyte *name = glGetStringi(GL_EXTENSIONS, index);
                    if (name == NULL ||
                        !has_extension_token(current, (const char *)name) ||
                        (previous != NULL && strcmp((const char *)previous,
                                                    (const char *)name) >= 0))
                        return fail(40, "conditional indexed enumeration mismatch");
                    previous = name;
                }
            }
            native.glesFramebufferParameteri = NULL;
            native.glesPrimitiveBoundingBox = NULL;
            native.glesTextureView = NULL;
            native.glesGenTextures = NULL;
            native.glesTexStorage2D = NULL;
            native.glesBlendBarrier = NULL;
            native.glesBlendEquation = NULL;
            native.glesTextureBarrier = NULL;
            native.glesClipControlEXT = NULL;
            native.glesDepthBoundsEXT = NULL;
            native.glesPolygonOffsetClamp = NULL;
            native.glesTexBufferEXT = NULL;
            native.glesEnable = NULL;
            native.glesDisable = NULL;
            native.glesBlendFuncSeparate = NULL;
            native.glesBlendFuncSeparatei = NULL;
            native.glesTexBufferRangeEXT = NULL;
            native_extensions = "GL_OES_texture_view GL_NV_texture_barrier "
                                "GL_ARB_polygon_offset_clamp";
            native.glesTextureView = fake_texture_view;
            native.glesGenTextures = fake_gen_textures;
            native.glesTexStorage2D = fake_tex_storage2d;
            native.glesTextureBarrier = fake_texture_barrier;
            native.glesPolygonOffsetClamp = fake_polygon_offset_clamp;
            if (has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                    "GL_ARB_ES3_2_compatibility") ||
                has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                    "GL_ARB_framebuffer_no_attachments"))
                return fail(47, "GLES core extensions require their backing entry points");
            if (!has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                     "GL_ARB_texture_view") ||
                !has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                     "GL_NV_texture_barrier") ||
                !has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                     "GL_ARB_polygon_offset_clamp"))
                return fail(41, "supported alternative native extensions not recognized");
            native.glesTextureView = NULL;
            native.glesGenTextures = NULL;
            native.glesTexStorage2D = NULL;
            native.glesTextureBarrier = NULL;
            native.glesPolygonOffsetClamp = NULL;
            native_extensions = "GL_EXT_blend_func_extended GL_EXT_sRGB_write_control";
            if (has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                    "GL_ARB_blend_func_extended") ||
                has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                    "GL_EXT_framebuffer_sRGB"))
                return fail(45, "extension strings without native dispatch must not advertise");
            native_extensions = "";
        }
        native.glesFenceSync = fake_fence;
        native.glesClientWaitSync = fake_wait_sync;
        native.glesDeleteSync = fake_delete_sync;
        native.glesGenQueries = fake_gen_queries;
        native.glesDeleteQueries = fake_delete_queries;
        native.glesBeginQuery = fake_begin_query;
        native.glesEndQuery = fake_end_query;
        native.glesGetQueryObjectuiv = fake_get_query;
        native.glesQueryCounterEXT = fake_query_counter;
        native.glesGetQueryObjectui64vEXT = fake_get_query64;
        native.glesGetInteger64v = fake_get_integer64;
        native_extensions = "GL_EXT_texture_filter_anisotropic GL_EXT_disjoint_timer_query";
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);
        if (count != (GLint)mgl_capability_extension_count(&ctx) ||
            !has_extension_token((const char *)glGetString(GL_EXTENSIONS), "GL_NV_fence") ||
            !has_extension_token((const char *)glGetString(GL_EXTENSIONS), "GL_ARB_timer_query") ||
            !has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                 "GL_EXT_texture_filter_anisotropic") ||
            !has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                 "GL_ARB_texture_filter_anisotropic"))
            return fail(28, "available native features must appear in both extension APIs");
        {
            const GLubyte *previous = NULL;
            for (i = 0; i < count; ++i) {
                const GLubyte *ext = glGetStringi(GL_EXTENSIONS, (GLuint)i);
                if (ext == NULL ||
                    !has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                         (const char *)ext) ||
                    (previous != NULL && strcmp((const char *)previous, (const char *)ext) >= 0))
                    return fail(29, "conditional extensions must preserve sorted enumeration");
                previous = ext;
            }
        }
        native.glesClientWaitSync = NULL;
        if (has_extension_token((const char *)glGetString(GL_EXTENSIONS), "GL_NV_fence"))
            return fail(30, "NV fence requires complete native sync dispatch");
        native.glesClientWaitSync = fake_wait_sync;
        native.glesQueryCounterEXT = NULL;
        if (has_extension_token((const char *)glGetString(GL_EXTENSIONS), "GL_ARB_timer_query"))
            return fail(31, "timer query requires native timestamp dispatch");
        native_extensions = "";
        if (has_extension_token((const char *)glGetString(GL_EXTENSIONS), "GL_ARB_timer_query") ||
            has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                "GL_EXT_texture_filter_anisotropic") ||
            has_extension_token((const char *)glGetString(GL_EXTENSIONS),
                                "GL_ARB_texture_filter_anisotropic") ||
            !has_extension_token((const char *)glGetString(GL_EXTENSIONS), "GL_NV_fence"))
            return fail(32, "native extension filtering and GLES core sync support");
        native.glesFenceSync = NULL;
        native.glesClientWaitSync = NULL;
        native.glesDeleteSync = NULL;
        native.glesQueryCounterEXT = NULL;
    }

    glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &compressed_count);
    if (compressed_count <= 0 ||
        (size_t)compressed_count > sizeof(compressed_formats) / sizeof(compressed_formats[0]))
        return fail(12, "compressed format inventory bounds");
    glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, compressed_formats);
    {
        const GLint required[] = {(GLint)GL_COMPRESSED_RED_RGTC1,
                                  (GLint)GL_COMPRESSED_SIGNED_RED_RGTC1,
                                  (GLint)GL_COMPRESSED_RG_RGTC2,
                                  (GLint)GL_COMPRESSED_SIGNED_RG_RGTC2,
                                  (GLint)GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                                  (GLint)GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
                                  (GLint)GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
                                  (GLint)GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
                                  (GLint)GL_COMPRESSED_SRGB_S3TC_DXT1_EXT,
                                  (GLint)GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT,
                                  (GLint)GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT,
                                  (GLint)GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT};
        size_t r;
        for (r = 0u; r < sizeof(required) / sizeof(required[0]); ++r) {
            int found = 0;
            for (i = 0; i < compressed_count; ++i) {
                if (compressed_formats[i] == required[r]) {
                    found = 1;
                    break;
                }
            }
            if (!found)
                return fail(13, "required compressed format missing");
        }
    }

    if (glGetStringi(GL_EXTENSIONS, (GLuint)count) != NULL ||
        mgl_error_take(&ctx.errors) != GL_INVALID_VALUE) {
        return fail(9, "indexed extension bounds");
    }
    if (glGetStringi(GL_VERSION, 0u) != NULL || mgl_error_take(&ctx.errors) != GL_INVALID_ENUM) {
        return fail(10, "indexed string enum validation");
    }
    if (glXGetProcAddress((const GLubyte *)"glGetStringi") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexCoord1dARB") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexCoord3ivARB") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexCoord4sARB") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCopyTexSubImage3DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glProgramStringARB") == NULL)
        return fail(11, "proc-address coverage");

    mgl_context_set_current_for_test(NULL);
    puts("capability reporting semantics passed");
    return 0;
}

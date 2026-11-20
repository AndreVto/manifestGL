#ifndef MGL_RENDER_DIAG_H
#define MGL_RENDER_DIAG_H

#include "mgl/types.h"
#include <stddef.h>
#include <stdint.h>

struct mgl_context;

/* Diagnostics must not consume errors owned by the application. */
void mgl_render_diag_shader(struct mgl_context *ctx, const char *stage, GLuint name,
                            GLuint native_name, GLenum type, GLboolean compiled, const char *log);
void mgl_render_diag_shader_source(struct mgl_context *ctx, GLuint name, const char *kind,
                                   const char *source);
void mgl_render_diag_program(struct mgl_context *ctx, GLuint name, GLuint native_name,
                             GLboolean linked, const char *log);
void mgl_render_diag_framebuffer(struct mgl_context *ctx, const char *where, GLenum target,
                                 GLuint name, GLenum status);
void mgl_render_diag_gl_error(struct mgl_context *ctx, const char *origin, GLenum error);
void mgl_render_diag_detail(struct mgl_context *ctx, const char *operation, const char *reason,
                            uint64_t arg0, uint64_t arg1);
void mgl_render_diag_skip(struct mgl_context *ctx, const char *operation, const char *reason,
                          uint64_t arg0, uint64_t arg1);

uint64_t mgl_perf_diag_now(void);
/* Draw route: 0 = GPU-resident vertices and indices, 1 = packed staging,
 * 2 = GPU-resident vertices with client indices uploaded to a scratch EBO. */
void mgl_perf_diag_draw(struct mgl_context *ctx, int fallback, uint64_t started,
                        size_t scratch_bytes);
void mgl_perf_diag_direct_upload(struct mgl_context *ctx);
void mgl_perf_diag_arb_uniform(struct mgl_context *ctx, unsigned set, unsigned reused);
void mgl_perf_diag_active_texture(struct mgl_context *ctx, int skipped);
void mgl_perf_diag_attrib(struct mgl_context *ctx, uint32_t pointers_set, uint32_t pointers_reused,
                          uint32_t constants_set, uint32_t constants_reused, uint32_t divisors_set,
                          uint32_t divisors_reused);
void mgl_perf_diag_shader(struct mgl_context *ctx, int link, uint64_t started);
void mgl_perf_diag_texture(struct mgl_context *ctx, uint64_t started, size_t bytes);
void mgl_perf_diag_angle_texture(struct mgl_context *ctx, int pbo, int copy);
void mgl_perf_diag_texture_rebind(struct mgl_context *ctx, uint32_t units);
void mgl_perf_diag_copy(struct mgl_context *ctx, uint64_t started);
/* Mode: 0 = linked GLSL, 1 = ARB assembly, 2 = fixed-function. */
void mgl_perf_diag_program_mode(struct mgl_context *ctx, unsigned mode);

enum mgl_perf_program_cache_event {
    MGL_PERF_ARB_CACHE_HIT,
    MGL_PERF_ARB_CACHE_RESOLVE,
    MGL_PERF_ARB_CACHE_RELINK,
    MGL_PERF_ARB_PAIR_RESTORE,
    MGL_PERF_ARB_PAIR_EVICT,
    MGL_PERF_FIXED_KEY_HIT,
    MGL_PERF_FIXED_KEY_REBUILD,
    MGL_PERF_FIXED_PROGRAM_HIT,
    MGL_PERF_FIXED_PROGRAM_RESTORE,
    MGL_PERF_FIXED_PROGRAM_BUILD,
    MGL_PERF_FIXED_CACHE_HIT,
    MGL_PERF_FIXED_CACHE_MISS,
    MGL_PERF_FIXED_CACHE_EVICTION,
    MGL_PERF_FIXED_SHADER_COMPILE,
    MGL_PERF_FIXED_PROGRAM_LINK,
    MGL_PERF_PROGRAM_CACHE_COUNT
};

void mgl_perf_diag_program_cache(struct mgl_context *ctx, enum mgl_perf_program_cache_event event);

enum mgl_perf_extra {
    MGL_PERF_TEXTURE_REALIZE,
    MGL_PERF_READ_PIXELS,
    MGL_PERF_GET_TEX_IMAGE,
    MGL_PERF_CLEAR,
    MGL_PERF_FLUSH,
    MGL_PERF_FINISH,
    MGL_PERF_EXTRA_COUNT
};

void mgl_perf_diag_extra(struct mgl_context *ctx, enum mgl_perf_extra kind, uint64_t started);
void mgl_perf_diag_present(struct mgl_context *ctx, uint64_t started, uint64_t submitted,
                           uint64_t ended);

enum mgl_deep_kind {
    MGL_DEEP_GLX_GEOMETRY,
    MGL_DEEP_GLX_DRAWABLE_INFO,
    MGL_DEEP_AHB_PRESENT,
    MGL_DEEP_AHB_SUBMIT,
    MGL_DEEP_AHB_ACQUIRE,
    MGL_DEEP_GLES_GET_ERROR,
    MGL_DEEP_GL_GET_ERROR,
    MGL_DEEP_GL_GET_STATE,
    MGL_DEEP_GL_IS_ENABLED,
    MGL_DEEP_GL_LIST_REPLAY,
    MGL_DEEP_GL_IMMEDIATE_END,
    MGL_DEEP_GL_COMPAT_STATE,
    MGL_DEEP_GL_MATRIX,
    MGL_DEEP_GL_TEXTURE_STATE,
    MGL_DEEP_GL_ACTIVE_TEXTURE,
    MGL_DEEP_GL_BIND_TEXTURE,
    MGL_DEEP_GL_TEX_PARAMETER,
    MGL_DEEP_GLES_ACTIVE_TEXTURE,
    MGL_DEEP_GLES_BIND_TEXTURE,
    MGL_DEEP_GLES_TEX_PARAMETER,
    MGL_DEEP_GL_FRAMEBUFFER,
    MGL_DEEP_GL_BUFFER_STATE,
    MGL_DEEP_GL_BUFFER_DATA,
    MGL_DEEP_GLES_BUFFER_DATA,
    MGL_DEEP_GL_PROGRAM_STATE,
    MGL_DEEP_GL_ENABLE_STATE,
    MGL_DEEP_DRAW_PREPARE,
    MGL_DEEP_DRAW_PROGRAM_LOOKUP,
    MGL_DEEP_DRAW_ARB_LOOKUP,
    MGL_DEEP_DRAW_FIXED_LOOKUP,
    MGL_DEEP_DRAW_GLSL_LOOKUP,
    MGL_DEEP_DRAW_PACK,
    MGL_DEEP_DRAW_UPLOAD,
    MGL_DEEP_DRAW_NATIVE_ATTRIB,
    MGL_DEEP_DRAW_ELEMENTS_LOCK_WAIT,
    MGL_DEEP_DRAW_NATIVE_CHECK,
    MGL_DEEP_GLES_INDEX_UPLOAD,
    MGL_DEEP_DRAW_UNIFORMS,
    MGL_DEEP_DRAW_ARB_UNIFORMS,
    MGL_DEEP_GLES_DRAW,
    MGL_DEEP_GLES_DRAW_SUBMIT,
    MGL_DEEP_TEXTURE_REALIZE,
    MGL_DEEP_GLES_FRAMEBUFFER_CHECK,
    MGL_DEEP_GL_API_COPY,
    MGL_DEEP_GLES_COPY_TEX,
    MGL_DEEP_GLES_COPY_READBACK,
    MGL_DEEP_GLES_CLEAR,
    MGL_DEEP_GL_TEX_IMAGE_2D,
    MGL_DEEP_GL_TEX_SUB_IMAGE_2D,
    MGL_DEEP_TEX_UNPACK,
    MGL_DEEP_TEX_COLOR_TRANSFORM,
    MGL_DEEP_TEX_IMAGE_APPLY_2D,
    MGL_DEEP_TEX_SUB_IMAGE_APPLY_2D,
    MGL_DEEP_TEX_SHADOW_IMAGE_2D,
    MGL_DEEP_TEX_SHADOW_SUB_IMAGE_2D,
    MGL_DEEP_GLES_TEX_IMAGE_2D,
    MGL_DEEP_GLES_TEX_SUB_IMAGE_2D,
    MGL_DEEP_TEX_UPLOAD_BEGIN,
    MGL_DEEP_TEX_UPLOAD_END,
    MGL_DEEP_GL_GENERATE_MIPMAP,
    MGL_DEEP_GLES_GENERATE_MIPMAP,
    MGL_DEEP_COUNT
};

struct mgl_deep_scope {
    struct mgl_context *ctx;
    struct mgl_deep_scope *parent;
    uint64_t started;
    uint64_t child_ns;
    enum mgl_deep_kind kind;
};

void mgl_deep_scope_begin(struct mgl_deep_scope *scope, struct mgl_context *ctx,
                          enum mgl_deep_kind kind);
void mgl_deep_scope_end(struct mgl_deep_scope *scope);
void mgl_deep_record(struct mgl_context *ctx, enum mgl_deep_kind kind, uint64_t started);
#if defined(__GNUC__) || defined(__clang__)
#define MGL_DEEP_JOIN2(a, b) a##b
#define MGL_DEEP_JOIN(a, b) MGL_DEEP_JOIN2(a, b)
#define MGL_DEEP_SCOPE(ctx, kind)                                                                  \
    struct mgl_deep_scope MGL_DEEP_JOIN(mgl_deep_scope_, __LINE__)                                 \
        __attribute__((cleanup(mgl_deep_scope_end))) = {0};                                        \
    mgl_deep_scope_begin(&MGL_DEEP_JOIN(mgl_deep_scope_, __LINE__), (ctx), (kind))
#else
#error "Deep performance instrumentation requires compiler scope cleanup"
#endif

#endif

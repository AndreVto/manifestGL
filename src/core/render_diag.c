#define _POSIX_C_SOURCE 200809L
#include "core/render_diag.h"
#include "core/context.h"
#include "texture/texture_store.h"
#include "shader/shader_store.h"
#include "objects/sampler.h"
#include "objects/object_store.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

static void report(struct mgl_context *ctx, const char *kind, const char *format, ...) {
    va_list args;
    const char *severity = strcmp(kind, "detail") == 0 || strcmp(kind, "lowering_skip") == 0
                               ? "warning" : "error";
    flockfile(stderr);
    (void)fprintf(stderr, "mGL: %s: pid=%ld thread=%lx ctx=%p event=%s ", severity,
                  (long)getpid(), (unsigned long)pthread_self(), (void *)ctx, kind);
    va_start(args, format);
    (void)vfprintf(stderr, format, args);
    va_end(args);
    (void)fputc('\n', stderr);
    funlockfile(stderr);
}

void mgl_render_diag_shader(struct mgl_context *ctx, const char *stage, GLuint name,
                            GLuint native_name, GLenum type, GLboolean compiled, const char *log) {
    if (compiled)
        return;
    report(ctx, "shader_compile", "stage=%s shader=%u native=%u type=0x%x status=%s log=%.1536s",
           stage, name, native_name, (unsigned)type, compiled ? "PASS" : "FAIL",
           log != NULL ? log : "");
}

void mgl_render_diag_shader_source(struct mgl_context *ctx, GLuint name, const char *kind,
                                   const char *source) {
    size_t bytes = 0u;
    unsigned line = 1u;
    const size_t limit = 96u * 1024u;
    const char *cursor = source != NULL ? source : "";
    report(ctx, "shader_source_begin", "shader=%u kind=%s limit=%zu", name, kind, limit);
    while (*cursor != '\0' && bytes < limit && line <= 512u) {
        const char *end = strchr(cursor, '\n');
        const size_t len = end != NULL ? (size_t)(end - cursor) : strlen(cursor);
        size_t offset = 0u;
        if (len == 0u)
            report(ctx, "shader_source", "shader=%u kind=%s line=%u text=", name, kind, line);
        while (offset < len && bytes < limit) {
            char chunk[513];
            size_t take = len - offset;
            size_t i;
            if (take > 512u)
                take = 512u;
            if (take > limit - bytes)
                take = limit - bytes;
            for (i = 0u; i < take; ++i) {
                unsigned char c = (unsigned char)cursor[offset + i];
                chunk[i] = (c < 32u || c == 127u) ? ' ' : (char)c;
            }
            chunk[take] = '\0';
            report(ctx, "shader_source", "shader=%u kind=%s line=%u offset=%zu text=%s", name, kind,
                   line, offset, chunk);
            offset += take;
            bytes += take;
        }
        if (end == NULL) {
            cursor += offset;
            break;
        }
        cursor = end + 1;
        ++line;
        ++bytes;
    }
    report(ctx, "shader_source_end", "shader=%u kind=%s lines=%u bytes=%zu truncated=%d", name,
           kind, line, bytes, *cursor != '\0' && (bytes >= limit || line > 512u));
}

void mgl_render_diag_program(struct mgl_context *ctx, GLuint name, GLuint native_name,
                             GLboolean linked, const char *log) {
    if (linked)
        return;
    report(ctx, "program_link", "program=%u native=%u status=%s log=%.1536s", name, native_name,
           linked ? "PASS" : "FAIL", log != NULL ? log : "");
}

static atomic_uint framebuffer_error_reports = ATOMIC_VAR_INIT(0u);
static atomic_uint gl_error_reports[2][7];

static unsigned gl_error_kind(GLenum error) {
    switch (error) {
        case GL_INVALID_ENUM:
            return 0u;
        case GL_INVALID_VALUE:
            return 1u;
        case GL_INVALID_OPERATION:
            return 2u;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return 3u;
        case GL_OUT_OF_MEMORY:
            return 4u;
        case GL_STACK_OVERFLOW:
            return 5u;
        default:
            return 6u;
    }
}

void mgl_render_diag_framebuffer(struct mgl_context *ctx, const char *where, GLenum target,
                                 GLuint name, GLenum status) {
    unsigned count;
    if (status == GL_FRAMEBUFFER_COMPLETE)
        return;
    count = atomic_fetch_add_explicit(&framebuffer_error_reports, 1u, memory_order_relaxed);
    if (count >= 32u)
        return;
    report(ctx, "framebuffer", "where=%s target=0x%x framebuffer=%u status=0x%x (%s)", where,
           (unsigned)target, name, (unsigned)status,
           status == GL_FRAMEBUFFER_COMPLETE ? "COMPLETE" : "INCOMPLETE_OR_ERROR");
}

void mgl_render_diag_gl_error(struct mgl_context *ctx, const char *origin, GLenum error) {
    atomic_uint *counter;
    unsigned count;
    if (error == GL_NO_ERROR)
        return;
    counter = &gl_error_reports[origin != NULL && strcmp(origin, "gles") == 0 ? 1u : 0u]
                               [gl_error_kind(error)];
    count = atomic_fetch_add_explicit(counter, 1u, memory_order_relaxed);
    if (count >= 32u)
        return;
    report(
        ctx, "glGetError",
        "origin=%s error=%s(0x%x) caused_by=%s program=%u draw_fbo=%u read_fbo=%u active_unit=%u",
        origin, mgl_error_name(error), (unsigned)error,
        ctx != NULL && strcmp(origin, "interface") == 0 && ctx->errors.pending_api != NULL
            ? ctx->errors.pending_api : "native",
        ctx != NULL ? ctx->current_program : 0u, ctx != NULL ? ctx->draw_framebuffer : 0u,
        ctx != NULL ? ctx->read_framebuffer : 0u, ctx != NULL ? ctx->compat.active_texture : 0u);
}

static atomic_uint native_check_reports[256];
static atomic_int native_check_enabled = ATOMIC_VAR_INIT(-1);

void mgl_render_diag_native_check(struct mgl_context *ctx, const char *operation, uint64_t arg0,
                                  uint64_t arg1) {
    GLenum error;
    unsigned hash = 2166136261u, count, i;
    const char *cursor;
    int active = atomic_load_explicit(&native_check_enabled, memory_order_relaxed);
    if (active < 0) {
        const char *option = getenv("MGL_BACKEND_ERROR_CHECK");
        active = option != NULL && option[0] == '1' && option[1] == '\0';
        atomic_store_explicit(&native_check_enabled, active, memory_order_relaxed);
    }
    if (!active || ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesGetError == NULL || ctx->errors.native_pending_count >= 16u)
        return;
    error = ctx->native_dispatch->glesGetError();
    if (error == GL_NO_ERROR)
        return;
    mgl_error_native_push(&ctx->errors, error);
    for (cursor = operation; cursor != NULL && *cursor != '\0'; ++cursor)
        hash = (hash ^ (unsigned char)*cursor) * 16777619u;
    hash ^= (unsigned)error;
    count =
        atomic_fetch_add_explicit(&native_check_reports[hash % 256u], 1u, memory_order_relaxed) +
        1u;
    if (count > 4u && (count & (count - 1u)) != 0u)
        return;
    flockfile(stderr);
    (void)fprintf(stderr,
                  "mGL: error: pid=%ld thread=%lx ctx=%p event=gles_reject "
                  "operation=%s error=%s(0x%x) arg0=0x%llx arg1=0x%llx api=%s "
                  "program=%u draw_fbo=%u read_fbo=%u active_unit=%u queued=%u",
                  (long)getpid(), (unsigned long)pthread_self(), (void *)ctx,
                  operation != NULL ? operation : "unknown", mgl_error_name(error), (unsigned)error,
                  (unsigned long long)arg0, (unsigned long long)arg1,
                  ctx->errors.diagnostic_api != NULL ? ctx->errors.diagnostic_api : "unknown",
                  ctx->current_program, ctx->draw_framebuffer, ctx->read_framebuffer,
                  ctx->compat.active_texture, ctx->errors.native_pending_count);
    for (i = 0u; i < ctx->errors.diagnostic_arg_count; ++i)
        (void)fprintf(stderr, " %s=0x%llx", ctx->errors.diagnostic_args[i].name,
                      (unsigned long long)ctx->errors.diagnostic_args[i].value);
    if (operation != NULL && strcmp(operation, "glesBindTexture") == 0 &&
        (arg0 == GL_TEXTURE_2D_MULTISAMPLE || arg0 == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)) {
        unsigned unit =
            ctx->native_active_texture_known && ctx->native_active_texture >= GL_TEXTURE0
                ? (unsigned)(ctx->native_active_texture - GL_TEXTURE0)
                : ctx->compat.active_texture;
        GLuint interface = unit < mgl_programmable_texture_limit(ctx)
                               ? (arg0 == GL_TEXTURE_2D_MULTISAMPLE
                                      ? ctx->texture_2d_multisample_binding[unit]
                                      : ctx->texture_2d_multisample_array_binding[unit])
                               : 0u;
        struct mgl_texture *texture = mgl_texture_find(ctx, interface);
        (void)fprintf(
            stderr,
            " native_unit=%u interface_texture=%u interface_target=0x%x interface_native=%u", unit,
            interface, texture != NULL ? (unsigned)texture->target : 0u,
            texture != NULL ? texture->native_name : 0u);
    }
    (void)fputc('\n', stderr);
    funlockfile(stderr);
}

static atomic_uint detail_reports[256];

void mgl_render_diag_detail(struct mgl_context *ctx, const char *operation, const char *reason,
                            uint64_t arg0, uint64_t arg1) {
    unsigned hash = 2166136261u, count;
    const char *cursor;
    for (cursor = operation; cursor != NULL && *cursor != '\0'; ++cursor)
        hash = (hash ^ (unsigned char)*cursor) * 16777619u;
    for (cursor = reason; cursor != NULL && *cursor != '\0'; ++cursor)
        hash = (hash ^ (unsigned char)*cursor) * 16777619u;
    count = atomic_fetch_add_explicit(&detail_reports[hash % 256u], 1u, memory_order_relaxed) + 1u;
    if (count > 4u && (count & (count - 1u)) != 0u)
        return;
    report(ctx, "detail", "operation=%s reason=%s a=0x%llx b=0x%llx", operation, reason,
           (unsigned long long)arg0, (unsigned long long)arg1);
}

static atomic_uint skip_reports[256];

void mgl_render_diag_skip(struct mgl_context *ctx, const char *operation, const char *reason,
                          uint64_t arg0, uint64_t arg1) {
    unsigned hash = 2166136261u, count;
    const char *cursor;
    for (cursor = operation; cursor != NULL && *cursor != '\0'; ++cursor)
        hash = (hash ^ (unsigned char)*cursor) * 16777619u;
    for (cursor = reason; cursor != NULL && *cursor != '\0'; ++cursor)
        hash = (hash ^ (unsigned char)*cursor) * 16777619u;
    count = atomic_fetch_add_explicit(&skip_reports[hash % 256u], 1u, memory_order_relaxed) + 1u;
    if (count > 4u && (count & (count - 1u)) != 0u)
        return;
    report(ctx, "lowering_skip",
           "operation=%s reason=%s arg0=0x%llx arg1=0x%llx api=%s "
           "program=%u draw_fbo=%u read_fbo=%u",
           operation, reason, (unsigned long long)arg0, (unsigned long long)arg1,
           ctx != NULL && ctx->errors.diagnostic_api != NULL ? ctx->errors.diagnostic_api
                                                             : "unknown",
           ctx != NULL ? ctx->current_program : 0u, ctx != NULL ? ctx->draw_framebuffer : 0u,
           ctx != NULL ? ctx->read_framebuffer : 0u);
}

struct mgl_perf_stats {
    struct mgl_context *ctx;
    uint64_t first_ns;
    uint64_t program_use_fast_seen;
    uint64_t draws_native;
    uint64_t draws_scratch;
    uint64_t draws_mixed;
    uint64_t draws_direct_upload;
    uint64_t arb_uniform_set, arb_uniform_reuse;
    uint64_t attrib_pointers_set, attrib_pointers_reused;
    uint64_t attrib_constants_set, attrib_constants_reused;
    uint64_t attrib_divisors_set, attrib_divisors_reused;
    uint64_t active_texture_set, active_texture_skipped;
    uint64_t draw_mixed_ns;
    uint64_t draws_angle_client;
    uint64_t mixed_index_bytes;
    uint64_t draw_native_ns;
    uint64_t draw_scratch_ns;
    uint64_t scratch_bytes;
    uint64_t shader_compile_count;
    uint64_t shader_compile_ns;
    uint64_t program_link_count;
    uint64_t program_link_ns;
    uint64_t texture_upload_count;
    uint64_t angle_texture_direct;
    uint64_t angle_texture_pbo_direct;
    uint64_t angle_image_copy_direct;
    uint64_t texture_upload_ns;
    uint64_t texture_bytes;
    uint64_t copy_count;
    uint64_t copy_ns;
    uint64_t program_modes[3];
    uint64_t program_cache_events[MGL_PERF_PROGRAM_CACHE_COUNT];
    uint64_t texture_rebinds;
    uint64_t texture_units_rebound;
    uint64_t extra_ns[MGL_PERF_EXTRA_COUNT];
    uint64_t extra_count[MGL_PERF_EXTRA_COUNT];
    uint64_t presents;
    uint64_t reports;
    uint64_t submit_ns;
    uint64_t pace_ns;
};

struct mgl_deep_bucket {
    uint64_t calls;
    uint64_t inclusive_ns;
    uint64_t exclusive_ns;
    uint64_t max_ns;
};

struct mgl_deep_stats {
    struct mgl_context *ctx;
    struct mgl_deep_scope *active;
    struct mgl_deep_bucket bucket[MGL_DEEP_COUNT];
};

static _Thread_local struct mgl_deep_stats deep_thread;

static const char *const deep_names[MGL_DEEP_COUNT] = {
    "glx_geometry",
    "glx_drawable_info",
    "ahb_present",
    "ahb_submit",
    "ahb_acquire",
    "gles_get_error",
    "gl_get_error",
    "gl_get_state",
    "gl_is_enabled",
    "gl_list_replay",
    "gl_immediate_end",
    "gl_compat_state",
    "gl_matrix",
    "gl_texture_state",
    "gl_active_texture",
    "gl_bind_texture",
    "gl_tex_parameter",
    "gles_active_texture",
    "gles_bind_texture",
    "gles_tex_parameter",
    "gl_framebuffer",
    "gl_buffer_state",
    "gl_buffer_data",
    "gles_buffer_data",
    "gl_program_state",
    "gl_enable_state",
    "draw_prepare",
    "draw_program_lookup",
    "draw_arb_lookup",
    "draw_fixed_lookup",
    "draw_glsl_lookup",
    "draw_pack",
    "draw_upload",
    "draw_native_attrib",
    "draw_elements_lock_wait",
    "draw_native_check",
    "gles_index_upload",
    "draw_uniforms",
    "draw_arb_uniforms",
    "gles_draw",
    "gles_draw_submit",
    "texture_realize",
    "gles_framebuffer_check",
    "gl_interface_copy",
    "gles_copy_tex",
    "gles_copy_readback",
    "gles_clear",
    "gl_tex_image_2d",
    "gl_tex_sub_image_2d",
    "tex_unpack",
    "tex_color_transform",
    "tex_image_apply_2d",
    "tex_sub_image_apply_2d",
    "tex_shadow_image_2d",
    "tex_shadow_sub_image_2d",
    "gles_tex_image_2d",
    "gles_tex_sub_image_2d",
    "tex_upload_begin",
    "tex_upload_end",
    "gl_generate_mipmap",
    "gles_generate_mipmap",
};

static struct mgl_deep_stats *deep_stats(struct mgl_context *ctx) {
    if (deep_thread.ctx != ctx) {
        memset(&deep_thread, 0, sizeof(deep_thread));
        deep_thread.ctx = ctx;
    }
    return &deep_thread;
}

void mgl_deep_scope_begin(struct mgl_deep_scope *scope, struct mgl_context *ctx,
                          enum mgl_deep_kind kind) {
    uint64_t now = mgl_perf_diag_now();
    memset(scope, 0, sizeof(*scope));
    if (now == 0u || ctx == NULL || (unsigned)kind >= (unsigned)MGL_DEEP_COUNT)
        return;
    scope->ctx = ctx;
    scope->kind = kind;
    scope->started = now;
    scope->parent = deep_stats(ctx)->active;
    deep_stats(ctx)->active = scope;
}

void mgl_deep_scope_end(struct mgl_deep_scope *scope) {
    struct mgl_deep_stats *stats;
    struct mgl_deep_bucket *bucket;
    uint64_t ended, duration;
    if (scope == NULL || scope->started == 0u)
        return;
    ended = mgl_perf_diag_now();
    stats = deep_stats(scope->ctx);
    duration = ended >= scope->started ? ended - scope->started : 0u;
    bucket = &stats->bucket[scope->kind];
    ++bucket->calls;
    bucket->inclusive_ns += duration;
    bucket->exclusive_ns += duration > scope->child_ns ? duration - scope->child_ns : 0u;
    if (duration > bucket->max_ns)
        bucket->max_ns = duration;
    if (scope->parent != NULL)
        scope->parent->child_ns += duration;
    stats->active = scope->parent;
}

void mgl_deep_record(struct mgl_context *ctx, enum mgl_deep_kind kind, uint64_t started) {
    struct mgl_deep_stats *stats;
    struct mgl_deep_bucket *bucket;
    uint64_t ended, duration;
    if (started == 0u || ctx == NULL || (unsigned)kind >= (unsigned)MGL_DEEP_COUNT)
        return;
    ended = mgl_perf_diag_now();
    duration = ended >= started ? ended - started : 0u;
    stats = deep_stats(ctx);
    bucket = &stats->bucket[kind];
    ++bucket->calls;
    bucket->inclusive_ns += duration;
    bucket->exclusive_ns += duration;
    if (duration > bucket->max_ns)
        bucket->max_ns = duration;
    if (stats->active != NULL)
        stats->active->child_ns += duration;
}

static void deep_report(struct mgl_context *ctx, uint64_t wall_ns, uint64_t frames) {
    struct mgl_deep_stats *stats = deep_stats(ctx);
    uint64_t covered_ns = 0u;
    unsigned indices[MGL_DEEP_COUNT];
    unsigned i, j;
    if (stats->active != NULL)
        return;
    for (i = 0u; i < (unsigned)MGL_DEEP_COUNT; ++i) {
        covered_ns += stats->bucket[i].exclusive_ns;
        indices[i] = i;
    }
    for (i = 1u; i < (unsigned)MGL_DEEP_COUNT; ++i) {
        unsigned key = indices[i];
        j = i;
        while (j > 0u &&
               stats->bucket[indices[j - 1u]].exclusive_ns < stats->bucket[key].exclusive_ns) {
            indices[j] = indices[j - 1u];
            --j;
        }
        indices[j] = key;
    }
    fprintf(stderr,
            "MGL_DEEP_DIAG pid=%ld ctx=%p frames=%" PRIu64
            " wall_ms=%.2f scoped_self_ms=%.2f not_scoped_ms=%.2f\n",
            (long)getpid(), (void *)ctx, frames, (double)wall_ns / 1000000.0,
            (double)covered_ns / 1000000.0,
            (double)(wall_ns > covered_ns ? wall_ns - covered_ns : 0u) / 1000000.0);
    for (i = 0u; i < (unsigned)MGL_DEEP_COUNT; ++i) {
        const struct mgl_deep_bucket *b = &stats->bucket[indices[i]];
        if (b->calls == 0u)
            continue;
        fprintf(stderr,
                "MGL_DEEP_DIAG pid=%ld kind=%s calls=%" PRIu64
                " total_ms=%.3f self_ms=%.3f max_call_ms=%.3f\n",
                (long)getpid(), deep_names[indices[i]], b->calls,
                (double)b->inclusive_ns / 1000000.0, (double)b->exclusive_ns / 1000000.0,
                (double)b->max_ns / 1000000.0);
    }
    memset(stats->bucket, 0, sizeof(stats->bucket));
}

static atomic_int perf_enabled = ATOMIC_VAR_INIT(-1);
static _Thread_local struct mgl_perf_stats perf_thread;

static int perf_is_enabled(void) {
    int active = atomic_load_explicit(&perf_enabled, memory_order_relaxed);
    if (active < 0) {
        const char *option = getenv("MGL_PERF_DIAG");
        active = (option != NULL && option[0] == '1' && option[1] == '\0') ? 1 : 0;
        atomic_store_explicit(&perf_enabled, active, memory_order_relaxed);
    }
    return active;
}

uint64_t mgl_perf_diag_now(void) {
    struct timespec ts;
    if (!perf_is_enabled() || clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0u;
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

static struct mgl_perf_stats *perf_stats(struct mgl_context *ctx) {
    if (perf_thread.ctx != ctx) {
        memset(&perf_thread, 0, sizeof(perf_thread));
        perf_thread.ctx = ctx;
    }
    return &perf_thread;
}

void mgl_perf_diag_draw(struct mgl_context *ctx, int fallback, uint64_t started,
                        size_t scratch_bytes) {
    uint64_t ended;
    struct mgl_perf_stats *s;
    if (started == 0u || ctx == NULL)
        return;
    ended = mgl_perf_diag_now();
    s = perf_stats(ctx);
    if (fallback == 2 || fallback == 3) {
        ++s->draws_mixed;
        if (fallback == 3)
            ++s->draws_angle_client;
        s->draw_mixed_ns += ended >= started ? ended - started : 0u;
        s->mixed_index_bytes += (uint64_t)scratch_bytes;
    } else if (fallback) {
        ++s->draws_scratch;
        s->draw_scratch_ns += ended >= started ? ended - started : 0u;
        s->scratch_bytes += (uint64_t)scratch_bytes;
    } else {
        ++s->draws_native;
        s->draw_native_ns += ended >= started ? ended - started : 0u;
    }
}

void mgl_perf_diag_attrib(struct mgl_context *ctx, uint32_t pointers_set, uint32_t pointers_reused,
                          uint32_t constants_set, uint32_t constants_reused, uint32_t divisors_set,
                          uint32_t divisors_reused) {
    struct mgl_perf_stats *s;
    if (ctx == NULL || !perf_is_enabled())
        return;
    s = perf_stats(ctx);
    s->attrib_pointers_set += pointers_set;
    s->attrib_pointers_reused += pointers_reused;
    s->attrib_constants_set += constants_set;
    s->attrib_constants_reused += constants_reused;
    s->attrib_divisors_set += divisors_set;
    s->attrib_divisors_reused += divisors_reused;
}

void mgl_perf_diag_active_texture(struct mgl_context *ctx, int skipped) {
    if (ctx == NULL || !perf_is_enabled())
        return;
    struct mgl_perf_stats *s = perf_stats(ctx);
    if (skipped)
        ++s->active_texture_skipped;
    else
        ++s->active_texture_set;
}

void mgl_perf_diag_arb_uniform(struct mgl_context *ctx, unsigned set, unsigned reused) {
    if (ctx == NULL || !perf_is_enabled())
        return;
    struct mgl_perf_stats *s = perf_stats(ctx);
    s->arb_uniform_set += set;
    s->arb_uniform_reuse += reused;
}

void mgl_perf_diag_direct_upload(struct mgl_context *ctx) {
    if (ctx != NULL && perf_is_enabled())
        ++perf_stats(ctx)->draws_direct_upload;
}

void mgl_perf_diag_shader(struct mgl_context *ctx, int link, uint64_t started) {
    uint64_t ended;
    struct mgl_perf_stats *s;
    if (started == 0u || ctx == NULL)
        return;
    ended = mgl_perf_diag_now();
    s = perf_stats(ctx);
    if (link) {
        ++s->program_link_count;
        s->program_link_ns += ended >= started ? ended - started : 0u;
    } else {
        ++s->shader_compile_count;
        s->shader_compile_ns += ended >= started ? ended - started : 0u;
    }
}

void mgl_perf_diag_texture(struct mgl_context *ctx, uint64_t started, size_t bytes) {
    uint64_t ended;
    struct mgl_perf_stats *s;
    if (started == 0u || ctx == NULL)
        return;
    ended = mgl_perf_diag_now();
    s = perf_stats(ctx);
    ++s->texture_upload_count;
    s->texture_upload_ns += ended >= started ? ended - started : 0u;
    s->texture_bytes += (uint64_t)bytes;
}

void mgl_perf_diag_angle_texture(struct mgl_context *ctx, int pbo, int copy) {
    struct mgl_perf_stats *s;
    if (!perf_is_enabled() || ctx == NULL)
        return;
    s = perf_stats(ctx);
    if (copy)
        ++s->angle_image_copy_direct;
    else if (pbo)
        ++s->angle_texture_pbo_direct;
    else
        ++s->angle_texture_direct;
}

void mgl_perf_diag_texture_rebind(struct mgl_context *ctx, uint32_t units) {
    struct mgl_perf_stats *s;
    if (!perf_is_enabled() || ctx == NULL)
        return;
    s = perf_stats(ctx);
    ++s->texture_rebinds;
    s->texture_units_rebound += units;
}

void mgl_perf_diag_program_mode(struct mgl_context *ctx, unsigned mode) {
    if (ctx == NULL || mode >= 3u || !perf_is_enabled())
        return;
    ++perf_stats(ctx)->program_modes[mode];
}

void mgl_perf_diag_program_cache(struct mgl_context *ctx, enum mgl_perf_program_cache_event event) {
    if (ctx == NULL || (unsigned)event >= (unsigned)MGL_PERF_PROGRAM_CACHE_COUNT ||
        !perf_is_enabled())
        return;
    ++perf_stats(ctx)->program_cache_events[event];
}

void mgl_perf_diag_copy(struct mgl_context *ctx, uint64_t started) {
    uint64_t ended;
    struct mgl_perf_stats *s;
    if (started == 0u || ctx == NULL)
        return;
    ended = mgl_perf_diag_now();
    s = perf_stats(ctx);
    ++s->copy_count;
    s->copy_ns += ended >= started ? ended - started : 0u;
}

void mgl_perf_diag_extra(struct mgl_context *ctx, enum mgl_perf_extra kind, uint64_t started) {
    uint64_t ended;
    struct mgl_perf_stats *s;
    if (started == 0u || ctx == NULL || (unsigned)kind >= (unsigned)MGL_PERF_EXTRA_COUNT)
        return;
    ended = mgl_perf_diag_now();
    s = perf_stats(ctx);
    ++s->extra_count[kind];
    if (ended >= started)
        s->extra_ns[kind] += ended - started;
}

void mgl_perf_diag_present(struct mgl_context *ctx, uint64_t started, uint64_t submitted,
                           uint64_t ended) {
    struct mgl_perf_stats *s;
    double seconds;
    if (started == 0u || ctx == NULL || submitted < started || ended < submitted)
        return;
    s = perf_stats(ctx);
    if (s->first_ns == 0u)
        s->first_ns = ended;
    ++s->presents;
    s->submit_ns += submitted - started;
    s->pace_ns += ended - submitted;
    if (s->reports == 0u) {
        if (s->presents != 1u)
            return;
    } else if (s->presents < 30u)
        return;
    seconds = ended > s->first_ns ? (double)(ended - s->first_ns) / 1000000000.0 : 0.0;
    (void)fprintf(
        stderr,
        "MGL_PERF_DIAG pid=%ld ctx=%p frames=%" PRIu64 " wall_ms=%.2f fps=%.2f "
        "draw_native=%" PRIu64 " native_cpu_ms=%.2f draw_mixed=%" PRIu64 " mixed_cpu_ms=%.2f "
        "draw_scratch=%" PRIu64 " scratch_cpu_ms=%.2f draw_direct_upload=%" PRIu64
        " draw_angle_client=%" PRIu64 " "
        "mixed_index_mib=%.2f arb_uniform_set=%" PRIu64 " arb_uniform_reuse=%" PRIu64 " "
        "attrib_pointer_set=%" PRIu64 " attrib_pointer_reuse=%" PRIu64 " "
        "attrib_constant_set=%" PRIu64 " attrib_constant_reuse=%" PRIu64 " "
        "attrib_divisor_set=%" PRIu64 " attrib_divisor_reuse=%" PRIu64 " "
        "active_texture_set=%" PRIu64 " active_texture_skip=%" PRIu64 " "
        "scratch_mib=%.2f shader_compiles=%" PRIu64 " shader_compile_ms=%.2f "
        "program_links=%" PRIu64 " program_link_ms=%.2f program_use_fast=%" PRIu64
        " tex_uploads=%" PRIu64 " tex_cpu_ms=%.2f "
        "tex_mib=%.2f angle_tex_direct=%" PRIu64 " angle_tex_pbo=%" PRIu64
        " angle_image_copy=%" PRIu64 " copy_ops=%" PRIu64
        " copy_cpu_ms=%.2f texture_rebinds=%" PRIu64 " texture_units_rebound=%" PRIu64
        " submit_cpu_ms=%.2f pace_ms=%.2f"
        " rebind_cpu_ms=%.2f readpixels_calls=%" PRIu64 " readpixels_cpu_ms=%.2f"
        " getteximage_calls=%" PRIu64 " getteximage_cpu_ms=%.2f"
        " clear_calls=%" PRIu64 " clear_cpu_ms=%.2f"
        " flush_calls=%" PRIu64 " flush_cpu_ms=%.2f"
        " finish_calls=%" PRIu64 " finish_cpu_ms=%.2f"
        " lookup_glsl=%" PRIu64 " lookup_arb=%" PRIu64 " lookup_fixed=%" PRIu64
        " arb_cache_hit=%" PRIu64 " arb_cache_resolve=%" PRIu64 " arb_cache_relink=%" PRIu64
        " arb_pair_restore=%" PRIu64 " arb_pair_evict=%" PRIu64 " fixed_key_hit=%" PRIu64
        " fixed_key_rebuild=%" PRIu64 " fixed_program_hit=%" PRIu64
        " fixed_program_restore=%" PRIu64 " fixed_program_build=%" PRIu64 "\n",
        (long)getpid(), (void *)ctx, s->presents, seconds * 1000.0,
        seconds > 0.0 ? (double)(s->reports == 0u ? s->presents - 1u : s->presents) / seconds : 0.0,
        s->draws_native, (double)s->draw_native_ns / 1000000.0, s->draws_mixed,
        (double)s->draw_mixed_ns / 1000000.0, s->draws_scratch,
        (double)s->draw_scratch_ns / 1000000.0, s->draws_direct_upload, s->draws_angle_client,
        (double)s->mixed_index_bytes / 1048576.0, s->arb_uniform_set, s->arb_uniform_reuse,
        s->attrib_pointers_set, s->attrib_pointers_reused, s->attrib_constants_set,
        s->attrib_constants_reused, s->attrib_divisors_set, s->attrib_divisors_reused,
        s->active_texture_set, s->active_texture_skipped, (double)s->scratch_bytes / 1048576.0,
        s->shader_compile_count, (double)s->shader_compile_ns / 1000000.0, s->program_link_count,
        (double)s->program_link_ns / 1000000.0,
        ctx->use_program_fast_hits - s->program_use_fast_seen, s->texture_upload_count,
        (double)s->texture_upload_ns / 1000000.0, (double)s->texture_bytes / 1048576.0,
        s->angle_texture_direct, s->angle_texture_pbo_direct, s->angle_image_copy_direct,
        s->copy_count, (double)s->copy_ns / 1000000.0, s->texture_rebinds, s->texture_units_rebound,
        (double)s->submit_ns / 1000000.0, (double)s->pace_ns / 1000000.0,
        (double)s->extra_ns[MGL_PERF_TEXTURE_REALIZE] / 1000000.0,
        s->extra_count[MGL_PERF_READ_PIXELS], (double)s->extra_ns[MGL_PERF_READ_PIXELS] / 1000000.0,
        s->extra_count[MGL_PERF_GET_TEX_IMAGE],
        (double)s->extra_ns[MGL_PERF_GET_TEX_IMAGE] / 1000000.0, s->extra_count[MGL_PERF_CLEAR],
        (double)s->extra_ns[MGL_PERF_CLEAR] / 1000000.0, s->extra_count[MGL_PERF_FLUSH],
        (double)s->extra_ns[MGL_PERF_FLUSH] / 1000000.0, s->extra_count[MGL_PERF_FINISH],
        (double)s->extra_ns[MGL_PERF_FINISH] / 1000000.0, s->program_modes[0], s->program_modes[1],
        s->program_modes[2], s->program_cache_events[MGL_PERF_ARB_CACHE_HIT],
        s->program_cache_events[MGL_PERF_ARB_CACHE_RESOLVE],
        s->program_cache_events[MGL_PERF_ARB_CACHE_RELINK],
        s->program_cache_events[MGL_PERF_ARB_PAIR_RESTORE],
        s->program_cache_events[MGL_PERF_ARB_PAIR_EVICT],
        s->program_cache_events[MGL_PERF_FIXED_KEY_HIT],
        s->program_cache_events[MGL_PERF_FIXED_KEY_REBUILD],
        s->program_cache_events[MGL_PERF_FIXED_PROGRAM_HIT],
        s->program_cache_events[MGL_PERF_FIXED_PROGRAM_RESTORE],
        s->program_cache_events[MGL_PERF_FIXED_PROGRAM_BUILD]);
    (void)fprintf(stderr,
                  "MGL_FIXED_CACHE pid=%ld ctx=%p hits=%" PRIu64 " misses=%" PRIu64
                  " evictions=%" PRIu64 " shader_compiles=%" PRIu64 " program_links=%" PRIu64 "\n",
                  (long)getpid(), (void *)ctx, s->program_cache_events[MGL_PERF_FIXED_CACHE_HIT],
                  s->program_cache_events[MGL_PERF_FIXED_CACHE_MISS],
                  s->program_cache_events[MGL_PERF_FIXED_CACHE_EVICTION],
                  s->program_cache_events[MGL_PERF_FIXED_SHADER_COMPILE],
                  s->program_cache_events[MGL_PERF_FIXED_PROGRAM_LINK]);
    deep_report(ctx, ended > s->first_ns ? ended - s->first_ns : 0u, s->presents);
    {
        uint64_t reports = s->reports + 1u;
        memset(s, 0, sizeof(*s));
        s->reports = reports;
        s->program_use_fast_seen = ctx->use_program_fast_hits;
    }
    s->ctx = ctx;
    s->first_ns = ended;
}

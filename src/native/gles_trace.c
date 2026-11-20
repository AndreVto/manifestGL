#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "native/gles_trace.h"
#include "core/context.h"
#include "core/error.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MGL_GLES_TRACE_HISTORY 24u
#define MGL_GLES_TRACE_MAX_ARGS 16u
#define MGL_GLES_TRACE_MAX_ERROR_POLLS 32u

struct mgl_trace_call {
    const char *name;
    uint64_t sequence;
    uint64_t args[MGL_GLES_TRACE_MAX_ARGS];
    unsigned arg_count;
    void *caller;
};

struct mgl_trace_thread {
    struct mgl_context *ctx;
    struct mgl_trace_call calls[MGL_GLES_TRACE_HISTORY];
    unsigned next;
    unsigned size;
    unsigned reports;
    uint64_t sequence;
};

static _Thread_local struct mgl_trace_thread trace;

GLenum mgl_gles_trace_get_error(struct mgl_context *ctx,
                                const struct mgl_native_dispatch *raw) {
    GLenum error = ctx != NULL ? mgl_error_native_take(&ctx->errors) : GL_NO_ERROR;
    if (error != GL_NO_ERROR)
        return error;
    return raw != NULL && raw->glesGetError != NULL ? raw->glesGetError() : GL_NO_ERROR;
}

int mgl_gles_trace_enabled(void) {
    const char *value = getenv("MGL_GLES_TRACE");
    return value != NULL && (strcmp(value, "1") == 0 || strcmp(value, "all") == 0);
}

uint64_t mgl_gles_trace_float(GLfloat value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return (uint64_t)bits;
}

uint64_t mgl_gles_trace_double(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int trace_report_allowed(void) {
    const char *value = getenv("MGL_GLES_TRACE");
    unsigned count = ++trace.reports;
    if (count == 1u)
        return 1;
    if (value == NULL || strcmp(value, "all") != 0)
        return 0;
    return count <= 8u || (count & (count - 1u)) == 0u;
}

static void trace_report(struct mgl_context *ctx, const char *phase, const char *name,
                         GLenum error) {
    unsigned index;
    unsigned i;
    if (!trace_report_allowed())
        return;
    flockfile(stderr);
    (void)fprintf(stderr,
                  "mGL: error: pid=%ld thread=%lx ctx=%p event=gles_origin phase=%s "
                  "native=%s error=%s(0x%x) sequence=%llu queued=%u\n",
                  (long)getpid(), (unsigned long)pthread_self(), (void *)ctx, phase,
                  name, mgl_error_name(error), (unsigned)error,
                  (unsigned long long)trace.sequence,
                  ctx->errors.native_pending_count);
    if (trace.reports == 1u) {
        for (i = 0u; i < trace.size; ++i) {
            const struct mgl_trace_call *call;
            index = (trace.next + MGL_GLES_TRACE_HISTORY - trace.size + i) %
                    MGL_GLES_TRACE_HISTORY;
            call = &trace.calls[index];
            Dl_info info;
            int resolved = dladdr(call->caller, &info);
            (void)fprintf(stderr,
                          "mGL: trace: event=gles_history sequence=%llu native=%s "
                          "caller=%p module=%s offset=0x%llx",
                          (unsigned long long)call->sequence, call->name, call->caller,
                          resolved && info.dli_fname != NULL ? info.dli_fname : "unknown",
                          resolved && info.dli_fbase != NULL
                              ? (unsigned long long)((uintptr_t)call->caller -
                                                     (uintptr_t)info.dli_fbase)
                              : 0ull);
            for (unsigned arg = 0u; arg < call->arg_count; ++arg)
                (void)fprintf(stderr, " a%u=0x%llx", arg,
                              (unsigned long long)call->args[arg]);
            (void)fputc('\n', stderr);
        }
    }
    funlockfile(stderr);
}

static void trace_check(struct mgl_context *ctx, const struct mgl_native_dispatch *raw,
                        const char *phase, const char *name) {
    const unsigned capacity = (unsigned)(sizeof(ctx->errors.native_pending) /
                                         sizeof(ctx->errors.native_pending[0]));
    if (ctx == NULL || raw == NULL || raw->glesGetError == NULL)
        return;
    for (unsigned poll = 0u; poll < MGL_GLES_TRACE_MAX_ERROR_POLLS &&
                             ctx->errors.native_pending_count < capacity; ++poll) {
        GLenum error = raw->glesGetError();
        if (error == GL_NO_ERROR)
            break;
        unsigned i;
        int queued = 0;
        for (i = 0u; i < ctx->errors.native_pending_count; ++i) {
            if (ctx->errors.native_pending[i] == error) {
                queued = 1;
                break;
            }
        }
        if (!queued)
            mgl_error_native_push(&ctx->errors, error);
        trace_report(ctx, phase, name, error);
    }
}

void mgl_gles_trace_before(struct mgl_context *ctx, const struct mgl_native_dispatch *raw,
                           const char *name, const uint64_t *args, unsigned arg_count, void *caller) {
    struct mgl_trace_call *call;
    if (ctx == NULL || raw == NULL)
        return;
    if (trace.ctx != ctx) {
        memset(&trace, 0, sizeof(trace));
        trace.ctx = ctx;
    }
    trace_check(ctx, raw, "inherited_before_call", name);
    call = &trace.calls[trace.next];
    call->name = name;
    call->sequence = ++trace.sequence;
    if (arg_count > MGL_GLES_TRACE_MAX_ARGS)
        arg_count = MGL_GLES_TRACE_MAX_ARGS;
    call->arg_count = args != NULL ? arg_count : 0u;
    if (call->arg_count != 0u)
        memcpy(call->args, args, (size_t)call->arg_count * sizeof(call->args[0]));
    call->caller = caller;
    trace.next = (trace.next + 1u) % MGL_GLES_TRACE_HISTORY;
    if (trace.size < MGL_GLES_TRACE_HISTORY)
        ++trace.size;
}

void mgl_gles_trace_after(struct mgl_context *ctx, const struct mgl_native_dispatch *raw,
                          const char *name) {
    if (ctx != NULL && raw != NULL)
        trace_check(ctx, raw, "generated_by_call", name);
}

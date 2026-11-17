#define _POSIX_C_SOURCE 200809L
#define MGL_ERROR_IMPLEMENTATION 1
#include "core/error.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static atomic_uint reject_counts[2048];

const char *mgl_error_name(GLenum error) {
    switch (error) {
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
        case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
        default: return "GL_UNKNOWN_ERROR";
    }
}

static int reject_report(const char *file, unsigned line, GLenum error) {
    uintptr_t key = (uintptr_t)file;
    unsigned count;
    key ^= (uintptr_t)line * UINT32_C(2654435761);
    key ^= (uintptr_t)error * UINT32_C(2246822519);
    key ^= key >> 11u;
    count = atomic_fetch_add_explicit(&reject_counts[key % 2048u], 1u, memory_order_relaxed) + 1u;
    return count <= 4u || (count & (count - 1u)) == 0u;
}

void mgl_error_init(struct mgl_error_state *state) {
    if (state != NULL) {
        state->pending = GL_NO_ERROR;
        state->sequence = 0u;
        state->native_pending_count = 0u;
        state->suppress = GL_FALSE;
        state->diagnostic_context = NULL;
        state->diagnostic_api = NULL;
        state->pending_api = NULL;
        state->diagnostic_arg_count = 0u;
        memset(state->diagnostic_args, 0, sizeof(state->diagnostic_args));
    }
}

void mgl_error_trace_api(struct mgl_error_state *state, const char *api,
                         const struct mgl_error_arg *args, unsigned count) {
    if (state == NULL)
        return;
    state->diagnostic_api = api;
    if (count > MGL_ERROR_TRACE_MAX_ARGS)
        count = MGL_ERROR_TRACE_MAX_ARGS;
    if (args == NULL)
        count = 0u;
    state->diagnostic_arg_count = count;
    if (count != 0u)
        memcpy(state->diagnostic_args, args, (size_t)count * sizeof(*args));
}

void mgl_error_set_at(struct mgl_error_state *state, GLenum error, const char *file, unsigned line,
                      const char *caller) {
    unsigned i, arg_count;
    GLenum pending;
    const char *api;
    if (state == NULL || state->suppress || error == GL_NO_ERROR)
        return;
    ++state->sequence;
    pending = state->pending;
    api = state->diagnostic_api != NULL ? state->diagnostic_api : caller;
    arg_count = state->diagnostic_arg_count;
    if (caller != NULL && caller[0] == 'g' && caller[1] == 'l' &&
        caller[2] >= 'A' && caller[2] <= 'Z' &&
        (api == NULL || strcmp(caller, api) != 0)) {
        api = caller;
        arg_count = 0u;
    }
    if (pending == GL_NO_ERROR) {
        state->pending = error;
        state->pending_api = api;
    }
    if (!reject_report(file, line, error))
        return;
    flockfile(stderr);
    (void)fprintf(stderr,
                  "mGL: error: pid=%ld thread=%lx ctx=%p event=interface_error_set "
                  "error=%s(0x%x) api=%s setter=%s at=%s:%u pending=0x%x",
                  (long)getpid(), (unsigned long)pthread_self(), state->diagnostic_context,
                  mgl_error_name(error), (unsigned)error,
                  api != NULL ? api : "<untracked>",
                  caller != NULL ? caller : "<unknown>", file != NULL ? file : "<unknown>", line,
                  (unsigned)pending);
    for (i = 0; i < arg_count; ++i) {
        (void)fprintf(stderr, " %s=0x%llx", state->diagnostic_args[i].name,
                      (unsigned long long)state->diagnostic_args[i].value);
    }
    (void)fputc('\n', stderr);
    funlockfile(stderr);
}

void mgl_error_set(struct mgl_error_state *state, GLenum error) {
    mgl_error_set_at(state, error, NULL, 0u, NULL);
}

GLenum mgl_error_take(struct mgl_error_state *state) {
    GLenum out;
    if (state == NULL)
        return GL_NO_ERROR;
    out = state->pending;
    state->pending = GL_NO_ERROR;
    return out;
}

GLenum mgl_error_peek(const struct mgl_error_state *state) {
    return state != NULL ? state->pending : GL_NO_ERROR;
}

void mgl_error_native_push(struct mgl_error_state *state, GLenum error) {
    if (state == NULL || error == GL_NO_ERROR ||
        state->native_pending_count >=
            sizeof(state->native_pending) / sizeof(state->native_pending[0]))
        return;
    state->native_pending[state->native_pending_count++] = error;
}

GLenum mgl_error_native_take(struct mgl_error_state *state) {
    GLenum error;
    if (state == NULL || state->native_pending_count == 0u)
        return GL_NO_ERROR;
    error = state->native_pending[0];
    --state->native_pending_count;
    if (state->native_pending_count != 0u)
        memmove(state->native_pending, state->native_pending + 1u,
                (size_t)state->native_pending_count * sizeof(state->native_pending[0]));
    return error;
}

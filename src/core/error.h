#ifndef MGL_ERROR_H
#define MGL_ERROR_H

#include <stddef.h>
#include <stdint.h>
#include "mgl/types.h"

#define MGL_ERROR_TRACE_MAX_ARGS 8u

struct mgl_error_arg {
    const char *name;
    uint64_t value;
};

struct mgl_error_state {
    GLenum pending;
    uint64_t sequence;
    GLenum native_pending[16];
    unsigned native_pending_count;
    GLboolean suppress;

    const void *diagnostic_context;
    const char *diagnostic_api;
    const char *pending_api;
    struct mgl_error_arg diagnostic_args[MGL_ERROR_TRACE_MAX_ARGS];
    unsigned diagnostic_arg_count;
};

const char *mgl_error_name(GLenum error);
void mgl_error_init(struct mgl_error_state *state);
void mgl_error_set(struct mgl_error_state *state, GLenum error);
void mgl_error_set_at(struct mgl_error_state *state, GLenum error, const char *file, unsigned line,
                      const char *caller);
void mgl_error_trace_api(struct mgl_error_state *state, const char *api,
                         const struct mgl_error_arg *args, unsigned count);
GLenum mgl_error_take(struct mgl_error_state *state);
void mgl_error_native_push(struct mgl_error_state *state, GLenum error);
GLenum mgl_error_native_take(struct mgl_error_state *state);
GLenum mgl_error_peek(const struct mgl_error_state *state);

/* mgl_error_set_at records the rejecting call site; mgl_error_set remains available for direct
 * callers. */
#ifndef MGL_ERROR_IMPLEMENTATION
#define mgl_error_set(state, error)                                                                \
    mgl_error_set_at((state), (error), __FILE__, (unsigned)__LINE__, __func__)
#endif

#endif

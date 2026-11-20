#ifndef MGL_ERROR_H
#define MGL_ERROR_H

#include <stddef.h>
#include <stdint.h>
#include "mgl/types.h"

struct mgl_error_state {
    GLenum pending;
    uint64_t sequence;
    GLenum native_pending[16];
    unsigned native_pending_count;
    GLboolean suppress;

    const void *diagnostic_context;
};

const char *mgl_error_name(GLenum error);
void mgl_error_init(struct mgl_error_state *state);
void mgl_error_set(struct mgl_error_state *state, GLenum error);
void mgl_error_set_at(struct mgl_error_state *state, GLenum error, const char *file, unsigned line,
                      const char *caller);
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

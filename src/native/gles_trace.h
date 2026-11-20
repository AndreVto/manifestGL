#ifndef MGL_GLES_TRACE_H
#define MGL_GLES_TRACE_H

#include "native/dispatch.h"
#include <stdint.h>

struct mgl_context;

int mgl_gles_trace_enabled(void);
GLenum mgl_gles_trace_get_error(struct mgl_context *ctx,
                                const struct mgl_native_dispatch *raw);
uint64_t mgl_gles_trace_float(GLfloat value);
uint64_t mgl_gles_trace_double(double value);
void mgl_gles_trace_before(struct mgl_context *ctx, const struct mgl_native_dispatch *raw,
                           const char *name, const uint64_t *args, unsigned arg_count, void *caller);
void mgl_gles_trace_after(struct mgl_context *ctx, const struct mgl_native_dispatch *raw,
                          const char *name);

#endif

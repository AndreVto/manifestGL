#ifndef MGL_DRAW_SNAPSHOT_H
#define MGL_DRAW_SNAPSHOT_H

#include "mgl/types.h"

struct mgl_context;
struct mgl_draw_snapshot;

struct mgl_draw_snapshot *mgl_draw_snapshot_capture_arrays(struct mgl_context *ctx, GLenum mode,
                                                           GLint first, GLsizei count);
struct mgl_draw_snapshot *mgl_draw_snapshot_capture_elements(struct mgl_context *ctx, GLenum mode,
                                                             GLsizei count, GLenum type,
                                                             const void *indices);
void mgl_draw_snapshot_replay(struct mgl_context *ctx, const struct mgl_draw_snapshot *snapshot);
void mgl_draw_snapshot_destroy(struct mgl_draw_snapshot *snapshot);

#endif

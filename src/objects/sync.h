#ifndef MGL_SYNC_H
#define MGL_SYNC_H

#include "core/context.h"

struct mgl_sync *mgl_sync_find(struct mgl_context *ctx, GLsync handle);
void mgl_sync_free_all(struct mgl_context *ctx);

#endif

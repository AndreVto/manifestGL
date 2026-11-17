#ifndef MGL_NV_FENCE_H
#define MGL_NV_FENCE_H

struct mgl_context;
void mgl_nv_fence_free_all(struct mgl_context *ctx, int native_current);

#endif

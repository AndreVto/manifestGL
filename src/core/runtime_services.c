#include "core/runtime_services.h"

#include "abi/proc_address.h"
#include "core/context.h"
#include "core/runtime.h"
#include "mgl/export.h"
#include "presentation/android_direct.h"

static int bridge_context_complete_fence(struct mgl_context *ctx) {
    GLsync native_sync = NULL;
    if (ctx == NULL || ctx != mgl_context_current() || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesFinish == NULL)
        return 0;

    /* EGL fence completion is synchronous: insert a GLES fence when available, then glFinish before
     * returning. */
    if (ctx->native_dispatch->glesFenceSync != NULL &&
        ctx->native_dispatch->glesDeleteSync != NULL) {
        native_sync = ctx->native_dispatch->glesFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0u);
        if (native_sync == NULL)
            return 0;
    }
    ctx->native_dispatch->glesFinish();
    if (native_sync != NULL)
        ctx->native_dispatch->glesDeleteSync(native_sync);
    return 1;
}

static void bridge_context_set_no_error(struct mgl_context *ctx, int enabled) {
    if (ctx != NULL) {
        ctx->errors.suppress = enabled ? GL_TRUE : GL_FALSE;
        if (enabled)
            ctx->errors.pending = GL_NO_ERROR;
    }
}

static const struct mgl_runtime_services runtime_services = {
    .runtime_native_dispatch = mgl_runtime_native_dispatch,
    .context_create_offscreen = mgl_context_create_offscreen,
    .context_destroy = mgl_context_destroy,
    .context_make_current = mgl_context_make_current,
    .context_configure_pbuffers = mgl_context_configure_pbuffers,
    .context_set_no_error = bridge_context_set_no_error,
    .context_set_creation_flags = mgl_context_set_creation_flags,
    .context_current = mgl_context_current,
    .context_complete_fence = bridge_context_complete_fence,
    .proc_address = mgl_proc_address,
    .android_present_create = mgl_android_present_create,
    .android_present_unref = mgl_android_present_unref,
    .android_present_configure = mgl_android_present_configure,
    .android_present_swap = mgl_android_present_swap,
    .android_present_set_swap_interval = mgl_android_present_set_swap_interval,
    .android_effective_swap_interval = mgl_android_effective_swap_interval,
    .context_set_profile_mask = mgl_context_set_profile_mask,
};

MGL_EXPORT const struct mgl_runtime_services *mgl_runtime_services_get(void) {
    return &runtime_services;
}

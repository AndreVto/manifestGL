#include "mgl/gl.h"

#include "core/context.h"
#include "objects/sync.h"

#include <stdlib.h>

struct mgl_sync *mgl_sync_find(struct mgl_context *ctx, GLsync handle) {
    struct mgl_sync *it;
    if (ctx == NULL || ctx->share_group == NULL || handle == NULL)
        return NULL;
    for (it = ctx->share_group->syncs; it != NULL; it = it->next) {
        if ((GLsync)it == handle)
            return it;
    }
    return NULL;
}

static void mgl_sync_remove(struct mgl_context *ctx, struct mgl_sync *sync) {
    struct mgl_sync **link;
    if (ctx == NULL || ctx->share_group == NULL || sync == NULL)
        return;
    link = &ctx->share_group->syncs;
    while (*link != NULL) {
        if (*link == sync) {
            *link = sync->next;
            if (sync->native_sync != NULL && ctx->native_dispatch != NULL &&
                ctx->native_dispatch->glesDeleteSync != NULL)
                ctx->native_dispatch->glesDeleteSync(sync->native_sync);
            free(sync);
            return;
        }
        link = &(*link)->next;
    }
}

void mgl_sync_free_all(struct mgl_context *ctx) {
    struct mgl_sync *sync;
    if (ctx == NULL || ctx->share_group == NULL)
        return;
    sync = ctx->share_group->syncs;
    while (sync != NULL) {
        struct mgl_sync *next = sync->next;
        if (sync->native_sync != NULL && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesDeleteSync != NULL)
            ctx->native_dispatch->glesDeleteSync(sync->native_sync);
        free(sync);
        sync = next;
    }
    ctx->share_group->syncs = NULL;
}

/* Check Begin/End legality before inspecting sync handles or parameters. */
static int reject_during_begin_end(struct mgl_context *ctx) {
    if (ctx == NULL || !ctx->immediate.active)
        return 0;
    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    return 1;
}

static struct mgl_sync *require_sync(struct mgl_context *ctx, GLsync handle) {
    struct mgl_sync *sync = mgl_sync_find(ctx, handle);
    if (sync == NULL)
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
    return sync;
}

GLsync glFenceSync(GLenum condition, GLbitfield flags) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_sync *sync;
    if (ctx == NULL)
        return NULL;
    if (reject_during_begin_end(ctx))
        return NULL;
    if (condition != GL_SYNC_GPU_COMMANDS_COMPLETE) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return NULL;
    }
    if (flags != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return NULL;
    }
    sync = calloc(1, sizeof(*sync));
    if (sync == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return NULL;
    }
    sync->condition = condition;
    sync->flags = flags;
    sync->fallback_signaled = GL_TRUE;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesFenceSync != NULL) {
        sync->native_sync = ctx->native_dispatch->glesFenceSync(condition, flags);
        if (sync->native_sync == NULL) {
            free(sync);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return NULL;
        }
        sync->fallback_signaled = GL_FALSE;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    sync->next = ctx->share_group->syncs;
    ctx->share_group->syncs = sync;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return (GLsync)sync;
}

GLboolean glIsSync(GLsync handle) {
    struct mgl_context *ctx = mgl_context_current();
    GLboolean out;
    if (ctx == NULL)
        return GL_FALSE;
    if (reject_during_begin_end(ctx))
        return GL_FALSE;
    if (handle == NULL)
        return GL_FALSE;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    out = mgl_sync_find(ctx, handle) != NULL ? GL_TRUE : GL_FALSE;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return out;
}

void glDeleteSync(GLsync handle) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_sync *sync;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (handle == NULL)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    sync = mgl_sync_find(ctx, handle);
    if (sync == NULL)
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
    else
        mgl_sync_remove(ctx, sync);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

GLenum glClientWaitSync(GLsync handle, GLbitfield flags, GLuint64 timeout) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_sync *sync;
    const GLbitfield allowed = GL_SYNC_FLUSH_COMMANDS_BIT;
    if (ctx == NULL)
        return GL_WAIT_FAILED;
    if (reject_during_begin_end(ctx))
        return GL_WAIT_FAILED;
    sync = require_sync(ctx, handle);
    if (sync == NULL)
        return GL_WAIT_FAILED;
    if ((flags & ~allowed) != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return GL_WAIT_FAILED;
    }
    if (sync->native_sync != NULL && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesClientWaitSync != NULL)
        return ctx->native_dispatch->glesClientWaitSync(sync->native_sync, flags, timeout);
    if ((flags & GL_SYNC_FLUSH_COMMANDS_BIT) != 0u && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesFlush != NULL)
        ctx->native_dispatch->glesFlush();
    (void)timeout;
    return GL_ALREADY_SIGNALED;
}

void glWaitSync(GLsync handle, GLbitfield flags, GLuint64 timeout) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_sync *sync;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    sync = require_sync(ctx, handle);
    if (sync == NULL)
        return;
    if (flags != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (timeout != GL_TIMEOUT_IGNORED) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (sync->native_sync != NULL && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesWaitSync != NULL)
        ctx->native_dispatch->glesWaitSync(sync->native_sync, flags, timeout);
}

void glGetSynciv(GLsync handle, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_sync *sync;
    GLint value;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    sync = require_sync(ctx, handle);
    if (sync == NULL)
        return;
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    switch (pname) {
        case GL_OBJECT_TYPE:
            value = (GLint)GL_SYNC_FENCE;
            break;
        case GL_SYNC_CONDITION:
            value = (GLint)sync->condition;
            break;
        case GL_SYNC_FLAGS:
            value = (GLint)sync->flags;
            break;
        case GL_SYNC_STATUS:
            if (sync->native_sync != NULL && ctx->native_dispatch != NULL &&
                ctx->native_dispatch->glesGetSynciv != NULL) {
                GLint native_value = (GLint)GL_UNSIGNALED;
                GLsizei native_length = 0;
                ctx->native_dispatch->glesGetSynciv(sync->native_sync, GL_SYNC_STATUS, 1,
                                                    &native_length, &native_value);
                value = native_value;
            } else
                value = sync->fallback_signaled ? (GLint)GL_SIGNALED : (GLint)GL_UNSIGNALED;
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
    }
    if (length != NULL)
        *length = bufSize > 0 ? 1 : 0;
    if (bufSize > 0 && values != NULL)
        values[0] = value;
}

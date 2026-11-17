#include "abi/unimplemented_exports.h"
#include "core/context.h"
#include "list/display_list.h"
#include "objects/nv_fence.h"

#include <limits.h>
#include <stdlib.h>

struct mgl_nv_fence {
    GLuint name;
    GLboolean realized;
    GLboolean signaled;
    GLsync sync;
    struct mgl_nv_fence *next;
};

static struct mgl_nv_fence *find_fence(struct mgl_context *ctx, GLuint name) {
    struct mgl_nv_fence *fence;
    for (fence = ctx->nv_fences; fence != NULL; fence = fence->next)
        if (fence->name == name)
            return fence;
    return NULL;
}

static int fence_begin_end(struct mgl_context *ctx) {
    if (!ctx->immediate.active)
        return 0;
    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    return 1;
}

static struct mgl_nv_fence *require_fence(struct mgl_context *ctx, GLuint name) {
    struct mgl_nv_fence *fence = find_fence(ctx, name);
    if (fence == NULL || !fence->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    return fence;
}

static void delete_native_fence(struct mgl_context *ctx, struct mgl_nv_fence *fence) {
    if (fence->sync != NULL && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesDeleteSync != NULL)
        ctx->native_dispatch->glesDeleteSync(fence->sync);
    fence->sync = NULL;
}

void mgl_nv_fence_free_all(struct mgl_context *ctx, int native_current) {
    struct mgl_nv_fence *fence = ctx->nv_fences;
    while (fence != NULL) {
        struct mgl_nv_fence *next = fence->next;
        if (native_current)
            delete_native_fence(ctx, fence);
        free(fence);
        fence = next;
    }
    ctx->nv_fences = NULL;
}

void glGenFencesNV(GLsizei n, GLuint *fences) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL || fence_begin_end(ctx))
        return;
    if (n < 0 || (n > 0 && fences == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < n; ++i) {
        struct mgl_nv_fence *fence;
        GLuint name = ctx->next_nv_fence_name;
        if (name == 0u)
            name = 1u;
        while (find_fence(ctx, name) != NULL) {
            ++name;
            if (name == 0u)
                name = 1u;
            if (name == ctx->next_nv_fence_name) {
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return;
            }
        }
        fence = calloc(1, sizeof(*fence));
        if (fence == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        fence->name = name;
        fence->next = ctx->nv_fences;
        ctx->nv_fences = fence;
        ctx->next_nv_fence_name = name + 1u;
        fences[i] = name;
    }
}

void glDeleteFencesNV(GLsizei n, const GLuint *fences) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL || fence_begin_end(ctx))
        return;
    if (n < 0 || (n > 0 && fences == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < n; ++i) {
        struct mgl_nv_fence **link = &ctx->nv_fences;
        while (*link != NULL && (*link)->name != fences[i])
            link = &(*link)->next;
        if (*link != NULL) {
            struct mgl_nv_fence *fence = *link;
            *link = fence->next;
            delete_native_fence(ctx, fence);
            free(fence);
        }
    }
}

void glSetFenceNV(GLuint name, GLenum condition) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_nv_fence *fence;
    GLsync sync;
    if (ctx == NULL || fence_begin_end(ctx))
        return;
    if (condition != GL_ALL_COMPLETED_NV) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (name == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_set_fence_nv(ctx, name, condition))
        return;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesFenceSync == NULL ||
        ctx->native_dispatch->glesClientWaitSync == NULL ||
        ctx->native_dispatch->glesDeleteSync == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    sync = ctx->native_dispatch->glesFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0u);
    if (sync == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    fence = find_fence(ctx, name);
    if (fence == NULL) {
        fence = calloc(1, sizeof(*fence));
        if (fence == NULL) {
            ctx->native_dispatch->glesDeleteSync(sync);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        fence->name = name;
        fence->next = ctx->nv_fences;
        ctx->nv_fences = fence;
    } else
        delete_native_fence(ctx, fence);
    fence->sync = sync;
    fence->signaled = GL_FALSE;
    fence->realized = GL_TRUE;
}

static GLboolean test_fence(struct mgl_context *ctx, struct mgl_nv_fence *fence, int finish,
                            int *failed) {
    GLenum result;
    if (failed != NULL)
        *failed = 0;
    if (fence->signaled)
        return GL_TRUE;
    result = ctx->native_dispatch->glesClientWaitSync(
        fence->sync, GL_SYNC_FLUSH_COMMANDS_BIT, finish ? GL_TIMEOUT_IGNORED : 0u);
    if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED) {
        fence->signaled = GL_TRUE;
        return GL_TRUE;
    }
    if (result == GL_WAIT_FAILED || (finish && result == GL_TIMEOUT_EXPIRED)) {
        if (failed != NULL)
            *failed = 1;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    }
    return GL_FALSE;
}

GLboolean glTestFenceNV(GLuint name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_nv_fence *fence;
    if (ctx == NULL || fence_begin_end(ctx))
        return GL_FALSE;
    fence = require_fence(ctx, name);
    return fence == NULL ? GL_TRUE : test_fence(ctx, fence, 0, NULL);
}

void glFinishFenceNV(GLuint name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_nv_fence *fence;
    if (ctx == NULL || fence_begin_end(ctx))
        return;
    fence = require_fence(ctx, name);
    if (fence != NULL)
        (void)test_fence(ctx, fence, 1, NULL);
}

GLboolean glIsFenceNV(GLuint name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_nv_fence *fence;
    if (ctx == NULL || fence_begin_end(ctx))
        return GL_FALSE;
    fence = find_fence(ctx, name);
    return fence != NULL && fence->realized ? GL_TRUE : GL_FALSE;
}

void glGetFenceivNV(GLuint name, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_nv_fence *fence;
    if (ctx == NULL || fence_begin_end(ctx))
        return;
    fence = require_fence(ctx, name);
    if (fence == NULL)
        return;
    if (pname != GL_FENCE_STATUS_NV && pname != GL_FENCE_CONDITION_NV) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (params == NULL)
        return;
    if (pname == GL_FENCE_CONDITION_NV)
        *params = (GLint)GL_ALL_COMPLETED_NV;
    else {
        int failed = 0;
        GLboolean result = test_fence(ctx, fence, 0, &failed);
        if (!failed)
            *params = (GLint)result;
    }
}

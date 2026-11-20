#define _POSIX_C_SOURCE 200809L
#include "core/context.h"
#include "core/error.h"
#include "native/gles_trace.h"
#include "mgl/gl.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLenum next_error;
static unsigned get_error_calls;

static GLenum repeated_native_error(void) {
    ++get_error_calls;
    return GL_INVALID_OPERATION;
}

struct worker_args {
    struct mgl_context *ctx;
    struct mgl_native_dispatch *native;
    GLenum consumed;
};

static void *consume_on_worker(void *data) {
    struct worker_args *args = data;
    mgl_context_set_current_for_test(args->ctx);
    args->consumed = mgl_gles_trace_get_error(args->ctx, args->native);
    mgl_context_set_current_for_test(NULL);
    return NULL;
}

static GLenum native_get_error(void) {
    GLenum result = next_error;
    next_error = GL_NO_ERROR;
    ++get_error_calls;
    return result;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    if (setenv("MGL_GLES_TRACE", "all", 1) != 0 || !mgl_gles_trace_enabled())
        return 1;
    mgl_error_init(&ctx.errors);
    ctx.native_dispatch = &native;
    native.glesGetError = native_get_error;
    mgl_context_set_current_for_test(&ctx);
    glBindBuffer(GL_ARRAY_BUFFER, 0u);
    mgl_gles_trace_before(&ctx, &native, "glesDrawElements",
                          (const uint64_t[]){4u, 8u}, 2u, NULL);
    if (get_error_calls != 1u || ctx.errors.native_pending_count != 0u)
        return 2;
    next_error = GL_INVALID_ENUM;
    mgl_gles_trace_after(&ctx, &native, "glesDrawElements");
    if (get_error_calls != 3u || ctx.errors.native_pending_count != 1u)
        return 6;
    mgl_error_set(&ctx.errors, GL_INVALID_VALUE);
    if (glGetError() != GL_INVALID_VALUE || get_error_calls != 3u)
        return 3;
    if (glGetError() != GL_INVALID_ENUM || get_error_calls != 3u)
        return 4;
    if (glGetError() != GL_NO_ERROR || get_error_calls != 4u)
        return 5;
    next_error = GL_INVALID_OPERATION;
    mgl_gles_trace_before(&ctx, &native, "glesBindTexture",
                          (const uint64_t[]){GL_TEXTURE_2D, 3u}, 2u, NULL);
    if (ctx.errors.native_pending_count != 1u || ctx.errors.native_pending[0] != GL_INVALID_OPERATION)
        return 7;
    next_error = GL_INVALID_OPERATION;
    mgl_gles_trace_after(&ctx, &native, "glesBindTexture");
    if (ctx.errors.native_pending_count != 1u)
        return 8;
    if (glGetError() != GL_INVALID_OPERATION || glGetError() != GL_NO_ERROR)
        return 9;
    mgl_gles_trace_before(&ctx, &native, "glesReadPixels", NULL, 0u, NULL);
    next_error = GL_INVALID_OPERATION;
    mgl_gles_trace_after(&ctx, &native, "glesReadPixels");
    if (mgl_gles_trace_get_error(&ctx, &native) != GL_INVALID_OPERATION ||
        glGetError() != GL_NO_ERROR)
        return 10;
    mgl_gles_trace_before(&ctx, &native, "glesReadPixels", NULL, 0u, NULL);
    mgl_gles_trace_after(&ctx, &native, "glesReadPixels");
    if (mgl_gles_trace_get_error(&ctx, &native) != GL_NO_ERROR)
        return 11;
    mgl_error_native_push(&ctx.errors, GL_INVALID_ENUM);
    next_error = GL_INVALID_VALUE;
    if (mgl_gles_trace_get_error(&ctx, &native) != GL_INVALID_ENUM ||
        next_error != GL_INVALID_VALUE ||
        mgl_gles_trace_get_error(&ctx, &native) != GL_INVALID_VALUE ||
        glGetError() != GL_NO_ERROR)
        return 12;
    mgl_gles_trace_before(&ctx, &native, "glesTexImage2D", NULL, 0u, NULL);
    next_error = GL_OUT_OF_MEMORY;
    mgl_gles_trace_after(&ctx, &native, "glesTexImage2D");
    if (glGetError() != GL_OUT_OF_MEMORY ||
        mgl_gles_trace_get_error(&ctx, &native) != GL_NO_ERROR)
        return 13;
    {
        unsigned before = get_error_calls;
        native.glesGetError = repeated_native_error;
        mgl_gles_trace_after(&ctx, &native, "glesRepeatError");
        if (get_error_calls - before != 32u || ctx.errors.native_pending_count != 1u ||
            ctx.errors.native_pending[0] != GL_INVALID_OPERATION)
            return 14;
        native.glesGetError = native_get_error;
        if (mgl_gles_trace_get_error(&ctx, &native) != GL_INVALID_OPERATION ||
            glGetError() != GL_NO_ERROR)
            return 15;
    }
    {
        struct mgl_context other;
        memset(&other, 0, sizeof(other));
        mgl_error_init(&other.errors);
        other.native_dispatch = &native;
        mgl_gles_trace_before(&ctx, &native, "glesCtxOne", NULL, 0u, NULL);
        next_error = GL_INVALID_ENUM;
        mgl_gles_trace_after(&ctx, &native, "glesCtxOne");
        mgl_context_set_current_for_test(&other);
        mgl_gles_trace_before(&other, &native, "glesCtxTwo", NULL, 0u, NULL);
        next_error = GL_INVALID_VALUE;
        mgl_gles_trace_after(&other, &native, "glesCtxTwo");
        if (glGetError() != GL_INVALID_VALUE || glGetError() != GL_NO_ERROR)
            return 16;
        mgl_context_set_current_for_test(&ctx);
        if (glGetError() != GL_INVALID_ENUM || glGetError() != GL_NO_ERROR)
            return 17;
    }
    {
        struct worker_args worker = {&ctx, &native, GL_NO_ERROR};
        pthread_t thread;
        mgl_gles_trace_before(&ctx, &native, "glesThreadTransfer", NULL, 0u, NULL);
        next_error = GL_OUT_OF_MEMORY;
        mgl_gles_trace_after(&ctx, &native, "glesThreadTransfer");
        mgl_context_set_current_for_test(NULL);
        if (pthread_create(&thread, NULL, consume_on_worker, &worker) != 0)
            return 18;
        if (pthread_join(thread, NULL) != 0 || worker.consumed != GL_OUT_OF_MEMORY)
            return 19;
        mgl_context_set_current_for_test(&ctx);
        if (glGetError() != GL_NO_ERROR)
            return 20;
    }
    mgl_context_set_current_for_test(NULL);
    puts("mGL native error tracing test: PASS");
    return 0;
}

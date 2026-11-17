#define _POSIX_C_SOURCE 200809L
#include "core/context.h"
#include "core/error.h"
#include "core/render_diag.h"
#include "mgl/gl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLenum next_error;
static unsigned get_error_calls;

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
    if (unsetenv("MGL_BACKEND_ERROR_CHECK") != 0)
        return 1;
    mgl_error_init(&ctx.errors);
    ctx.native_dispatch = &native;
    native.glesGetError = native_get_error;
    mgl_context_set_current_for_test(&ctx);
    glBindBuffer(GL_ARRAY_BUFFER, 0u);
    next_error = GL_INVALID_ENUM;
    mgl_render_diag_native_check(&ctx, "glesDrawElements", 4u, 8u);
    if (get_error_calls != 0u || ctx.errors.native_pending_count != 0u)
        return 2;
    mgl_error_set(&ctx.errors, GL_INVALID_VALUE);
    if (glGetError() != GL_INVALID_VALUE || get_error_calls != 0u)
        return 3;
    if (glGetError() != GL_INVALID_ENUM || get_error_calls != 1u)
        return 4;
    if (glGetError() != GL_NO_ERROR || get_error_calls != 2u)
        return 5;
    mgl_context_set_current_for_test(NULL);
    puts("mGL deferred native error polling test: PASS");
    return 0;
}

#define _POSIX_C_SOURCE 200809L
#include "core/context.h"
#include "core/error.h"
#include "core/render_diag.h"
#include "native/gles_trace.h"
#include "mgl/gl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static GLenum native_error;
static GLint last_viewport[4];
static unsigned viewport_calls;

static void fake_viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    last_viewport[0] = x;
    last_viewport[1] = y;
    last_viewport[2] = width;
    last_viewport[3] = height;
    ++viewport_calls;
}

static GLenum fake_get_error(void) {
    GLenum error = native_error;
    native_error = GL_NO_ERROR;
    return error;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    FILE *log;
    int saved_fd;
    char output[8192];
    size_t length;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    mgl_error_init(&ctx.errors);
    ctx.errors.diagnostic_context = &ctx;
    ctx.native_dispatch = &native;
    native.glesGetError = fake_get_error;
    native.glesViewport = fake_viewport;
    if (unsetenv("MGL_GLES_TRACE") != 0)
        return 21;
    mgl_context_set_current_for_test(&ctx);
    log = tmpfile();
    if (log == NULL)
        return 2;
    saved_fd = dup(fileno(stderr));
    if (saved_fd < 0 || dup2(fileno(log), fileno(stderr)) < 0)
        return 3;
    mgl_error_set_at(&ctx.errors, GL_INVALID_VALUE, "texture.c", 17u, "texture_upload");
    mgl_error_set_at(&ctx.errors, GL_OUT_OF_MEMORY, "texture.c", 19u, "texture_upload");
    if (setenv("MGL_GLES_TRACE", "all", 1) != 0)
        return 22;
    if (mgl_error_peek(&ctx.errors) != GL_INVALID_VALUE)
        return 4;
    mgl_gles_trace_before(&ctx, &native, "glesBindTexture",
                          (const uint64_t[]){GL_TEXTURE_2D, 3u}, 2u, NULL);
    native_error = GL_INVALID_OPERATION;
    mgl_gles_trace_after(&ctx, &native, "glesBindTexture");
    if (native_error != GL_NO_ERROR || ctx.errors.native_pending_count != 1u)
        return 5;
    if (glGetError() != GL_INVALID_VALUE)
        return 6;
    if (glGetError() != GL_INVALID_OPERATION)
        return 7;
    if (glGetError() != GL_NO_ERROR)
        return 8;
    native_error = GL_INVALID_ENUM;
    if (glGetError() != GL_INVALID_ENUM)
        return 17;
    mgl_gles_trace_before(&ctx, &native, "glesDrawBuffers",
                          (const uint64_t[]){4u, GL_COLOR_ATTACHMENT0}, 2u, NULL);
    native_error = GL_INVALID_VALUE;
    mgl_gles_trace_after(&ctx, &native, "glesDrawBuffers");
    if (glGetError() != GL_INVALID_VALUE)
        return 18;
    mgl_gles_trace_before(&ctx, &native, "glesUseProgram",
                          (const uint64_t[]){2u, 0u}, 2u, NULL);
    native_error = GL_INVALID_OPERATION;
    mgl_gles_trace_after(&ctx, &native, "glesUseProgram");
    if (native_error != GL_NO_ERROR || glGetError() != GL_INVALID_OPERATION)
        return 20;
    mgl_render_diag_skip(&ctx, "glDrawBuffers", "nonidentity_attachment", 1u, GL_COLOR_ATTACHMENT0);
    glPointSize(-1.f);
    if (glGetError() != GL_INVALID_VALUE)
        return 25;
    {
        const GLfloat fractional_viewport[] = {2.25f, 3.75f, 10.25f, 11.75f};
        GLfloat result[4] = {0};
        glViewportArrayv(0u, 1, fractional_viewport);
        glGetFloatv(GL_VIEWPORT, result);
        if (glGetError() != GL_NO_ERROR || viewport_calls != 1u || last_viewport[0] != 2 ||
            last_viewport[1] != 4 || last_viewport[2] != 10 || last_viewport[3] != 12 ||
            memcmp(result, fractional_viewport, sizeof(result)) != 0)
            return 23;
        glViewportArrayv(1u, 1, fractional_viewport);
        if (glGetError() != GL_INVALID_VALUE || viewport_calls != 1u)
            return 26;
    }
    fflush(stderr);
    if (dup2(saved_fd, fileno(stderr)) < 0)
        return 9;
    close(saved_fd);
    rewind(log);
    length = fread(output, 1u, sizeof(output) - 1u, log);
    output[length] = '\0';
    fclose(log);
    if (strstr(output, "event=interface_error_set error=GL_INVALID_VALUE(0x501)") == NULL)
        return 10;
    if (strstr(output, "event=interface_error_set error=GL_OUT_OF_MEMORY(0x505)") == NULL)
        return 11;
    if (strstr(output, "pending=0x501") == NULL)
        return 12;
    if (strstr(output, "setter=texture_upload at=texture.c:17") == NULL ||
        strstr(output, "setter=texture_upload at=texture.c:19") == NULL)
        return 13;
    if (strstr(output, "event=gles_origin phase=generated_by_call native=glesBindTexture error=GL_INVALID_OPERATION(0x502)") == NULL)
        return 14;
    if (strstr(output, "mGL: warning:") == NULL ||
        strstr(output, "event=lowering_skip operation=glDrawBuffers") == NULL)
        return 15;
    if (strstr(output, "mGL: error:") == NULL ||
        strstr(output, "setter=validate_viewport_span at=") == NULL ||
        strstr(output, "setter=glPointSize at=") == NULL ||
        strstr(output, "event=glGetError origin=interface error=GL_INVALID_VALUE(0x501)") == NULL ||
        strstr(output, "caused_by=") != NULL ||
        strstr(output, " api=") != NULL)
        return 24;
    mgl_context_set_current_for_test(NULL);
    return 0;
}

#define _POSIX_C_SOURCE 200809L
#include "core/context.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned context_calls;
static unsigned requested_true;
static unsigned queried_client_state;
static int advertise_egl_extension;
static int advertise_gl_extension;
static int enabled_client_state;

static MGL_EGLDisplay fake_get_display(MGL_EGLNativeDisplayType native) {
    (void)native;
    return (MGL_EGLDisplay)(uintptr_t)1u;
}

static MGL_EGLBoolean fake_initialize(MGL_EGLDisplay display, MGL_EGLint *major,
                                      MGL_EGLint *minor) {
    (void)display;
    if (major != NULL)
        *major = 1;
    if (minor != NULL)
        *minor = 5;
    return MGL_EGL_TRUE;
}

static const char *fake_query_string(MGL_EGLDisplay display, MGL_EGLint name) {
    (void)display;
    if (name != MGL_EGL_EXTENSIONS)
        return NULL;
    if (advertise_egl_extension == 1)
        return "EGL_KHR_surfaceless_context EGL_ANGLE_create_context_client_arrays "
               "EGL_KHR_create_context";
    if (advertise_egl_extension == 2)
        return "EGL_ANGLE_create_context_client_arrays_extra";
    return "EGL_KHR_surfaceless_context EGL_KHR_create_context";
}

static MGL_EGLBoolean fake_bind_api(MGL_EGLint api) {
    return api == MGL_EGL_OPENGL_ES_API ? MGL_EGL_TRUE : MGL_EGL_FALSE;
}

static MGL_EGLBoolean fake_choose_config(MGL_EGLDisplay display, const MGL_EGLint *attrs,
                                         MGL_EGLConfig *config, MGL_EGLint count,
                                         MGL_EGLint *returned) {
    (void)display;
    (void)attrs;
    if (count > 0)
        config[0] = (MGL_EGLConfig)(uintptr_t)2u;
    if (returned != NULL)
        *returned = 1;
    return MGL_EGL_TRUE;
}

static MGL_EGLSurface fake_pbuffer(MGL_EGLDisplay display, MGL_EGLConfig config,
                                   const MGL_EGLint *attrs) {
    (void)display;
    (void)config;
    (void)attrs;
    return malloc(1u);
}

static MGL_EGLContext fake_create_context(MGL_EGLDisplay display, MGL_EGLConfig config,
                                          MGL_EGLContext share, const MGL_EGLint *attrs) {
    const MGL_EGLint *p;
    int found_client_arrays = 0;
    (void)display;
    (void)config;
    (void)share;
    ++context_calls;
    for (p = attrs; *p != MGL_EGL_NONE; p += 2) {
        if (p[0] == MGL_EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE) {
            if (found_client_arrays || !advertise_egl_extension || advertise_egl_extension == 2 ||
                p[1] != (MGL_EGLint)MGL_EGL_TRUE)
                return MGL_EGL_NO_CONTEXT;
            ++requested_true;
            found_client_arrays = 1;
        }
    }
    if (advertise_egl_extension == 1 && !found_client_arrays)
        return MGL_EGL_NO_CONTEXT;
    return malloc(1u);
}

static MGL_EGLBoolean fake_make_current(MGL_EGLDisplay display, MGL_EGLSurface draw,
                                        MGL_EGLSurface read, MGL_EGLContext context) {
    (void)display;
    (void)draw;
    (void)read;
    (void)context;
    return MGL_EGL_TRUE;
}

static const GLubyte *fake_get_string_i(GLenum name, GLuint index) {
    if (name == GL_EXTENSIONS && index == 0u && advertise_gl_extension)
        return (const GLubyte *)"GL_ANGLE_client_arrays";
    return NULL;
}

static void fake_get_integer(GLenum name, GLint *value) {
    if (name == GL_MAJOR_VERSION)
        *value = 3;
    else if (name == GL_MINOR_VERSION)
        *value = 2;
    else if (name == GL_NUM_EXTENSIONS)
        *value = advertise_gl_extension ? 1 : 0;
    else if (name == 0x93AAu) {
        ++queried_client_state;
        *value = enabled_client_state;
    } else if (name == GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS || name == GL_MAX_TEXTURE_IMAGE_UNITS ||
               name == GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS)
        *value = 16;
    else
        *value = 0;
}

static MGL_EGLBoolean fake_destroy_context(MGL_EGLDisplay display, MGL_EGLContext context) {
    (void)display;
    free(context);
    return MGL_EGL_TRUE;
}

static MGL_EGLBoolean fake_destroy_surface(MGL_EGLDisplay display, MGL_EGLSurface surface) {
    (void)display;
    free(surface);
    return MGL_EGL_TRUE;
}

static void init_native(struct mgl_native_dispatch *native) {
    memset(native, 0, sizeof(*native));
    native->initialized = 1;
    native->available = 1;
    native->eglGetDisplay = fake_get_display;
    native->eglInitialize = fake_initialize;
    native->eglQueryString = fake_query_string;
    native->eglBindAPI = fake_bind_api;
    native->eglChooseConfig = fake_choose_config;
    native->eglCreatePbufferSurface = fake_pbuffer;
    native->eglCreateContext = fake_create_context;
    native->eglMakeCurrent = fake_make_current;
    native->eglDestroyContext = fake_destroy_context;
    native->eglDestroySurface = fake_destroy_surface;
    native->glesGetStringi = fake_get_string_i;
    native->glesGetIntegerv = fake_get_integer;
}

static int verify_case(struct mgl_native_dispatch *native, int egl_extension, int gl_extension,
                       int gl_state, int expected_enabled, unsigned expected_state_queries,
                       unsigned expected_requests) {
    struct mgl_context *ctx;
    advertise_egl_extension = egl_extension;
    advertise_gl_extension = gl_extension;
    enabled_client_state = gl_state;
    context_calls = 0u;
    requested_true = 0u;
    queried_client_state = 0u;
    ctx = mgl_context_create_offscreen(native, NULL, 16, 16);
    if (ctx == NULL)
        return 1;
    if (!mgl_context_make_current(ctx)) {
        mgl_context_destroy(ctx);
        return 1;
    }
    if ((ctx->angle_client_arrays != GL_FALSE) != expected_enabled ||
        requested_true != expected_requests || context_calls != 1u ||
        queried_client_state != expected_state_queries) {
        fprintf(stderr, "ANGLE client arrays: enabled=%d requests=%u contexts=%u queries=%u\n",
                (int)ctx->angle_client_arrays, requested_true, context_calls, queried_client_state);
        mgl_context_make_current(NULL);
        mgl_context_destroy(ctx);
        return 1;
    }
    mgl_context_make_current(NULL);
    mgl_context_destroy(ctx);
    return 0;
}

int main(void) {
    struct mgl_native_dispatch native;
    init_native(&native);
    if (verify_case(&native, 1, 1, 1, 1, 1u, 1u))
        return 1;
    if (verify_case(&native, 1, 1, 0, 0, 1u, 1u))
        return 2;
    if (verify_case(&native, 0, 1, 1, 1, 1u, 0u))
        return 3;
    if (verify_case(&native, 2, 1, 1, 1, 1u, 0u))
        return 4;
    if (verify_case(&native, 1, 0, 1, 0, 0u, 1u))
        return 5;
    if (setenv("MGL_ANGLE_CLIENT_ARRAYS", "0", 1) != 0)
        return 6;
    if (verify_case(&native, 1, 1, 1, 0, 0u, 1u))
        return 7;
    unsetenv("MGL_ANGLE_CLIENT_ARRAYS");
    return 0;
}

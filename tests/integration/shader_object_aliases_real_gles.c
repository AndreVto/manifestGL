#include "native/dispatch.h"
#include "core/context.h"
#include "conformance/shader_object_alias_cases.h"
#include <stdio.h>
#include <string.h>

static MGL_EGLDisplay (*platform_display)(MGL_EGLint, void *, const MGL_EGLint *);

static MGL_EGLDisplay surfaceless_display(MGL_EGLNativeDisplayType unused) {
    (void)unused;
    return platform_display(0x31ddu, NULL, NULL);
}

int main(void) {
    struct mgl_native_dispatch native;
    struct mgl_context *ctx;
    memset(&native, 0, sizeof(native));
    if (!mgl_native_dispatch_init(&native))
        return 2;
    platform_display = (void *)native.eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (platform_display == NULL)
        return 3;
    native.eglGetDisplay = surfaceless_display;
    ctx = mgl_context_create_offscreen(&native, NULL, 64, 64);
    if (ctx == NULL || !mgl_context_make_current(ctx))
        return 4;
    shader_object_alias_trace();
    mgl_context_make_current(NULL);
    mgl_context_destroy(ctx);
    mgl_native_dispatch_shutdown(&native);
    return 0;
}

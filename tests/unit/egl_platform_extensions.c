#include "mgl/egl.h"

#include <X11/Xlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "egl_platform_extensions: CHECK failed %s:%d: %s (egl=%#x)\n",         \
                    __FILE__, __LINE__, #x, eglGetError());                                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int has_extension(const char *list, const char *name) {
    size_t n = strlen(name);
    const char *p = list;
    while (p != NULL && *p != '\0') {
        const char *hit = strstr(p, name);
        if (hit == NULL)
            return 0;
        if ((hit == list || hit[-1] == ' ') && (hit[n] == '\0' || hit[n] == ' '))
            return 1;
        p = hit + n;
    }
    return 0;
}

int main(void) {
    const char *exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    Display *x;
    EGLDisplay d_core, d_ext, d_default, surfaceless;
    EGLConfig config;
    EGLSurface s_core, s_ext;
    EGLint count = 0, visual = 0, visual_type = 0;
    Window window;
    int screen;
    EGLAttrib core_attrs[3];
    EGLint ext_attrs[3];
    EGLAttrib core_window_attrs[] = {EGL_RENDER_BUFFER, EGL_SINGLE_BUFFER, EGL_NONE};
    EGLint ext_window_attrs[] = {EGL_RENDER_BUFFER, EGL_SINGLE_BUFFER, EGL_NONE};
    EGLAttrib bad_core_attrs[] = {EGL_PLATFORM_X11_SCREEN_KHR, 9999, EGL_NONE};
    EGLint bad_ext_attrs[] = {EGL_PLATFORM_X11_SCREEN_EXT, 9999, EGL_NONE};

    CHECK(exts != NULL);
    CHECK(has_extension(exts, "EGL_EXT_client_extensions"));
    CHECK(has_extension(exts, "EGL_EXT_platform_base"));
    CHECK(has_extension(exts, "EGL_EXT_platform_x11"));
    CHECK(has_extension(exts, "EGL_KHR_platform_x11"));
    CHECK(has_extension(exts, "EGL_MESA_platform_surfaceless"));
    CHECK(eglGetProcAddress("eglGetPlatformDisplayEXT") != NULL);
    CHECK(eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT") != NULL);
    CHECK(eglGetProcAddress("eglCreatePlatformPixmapSurfaceEXT") != NULL);

    CHECK(eglGetPlatformDisplay(0xdead, NULL, NULL) == EGL_NO_DISPLAY);
    CHECK(eglGetError() == EGL_BAD_PARAMETER);
    CHECK(eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, (void *)(uintptr_t)1u, NULL) ==
          EGL_NO_DISPLAY);
    CHECK(eglGetError() == EGL_BAD_PARAMETER);

    surfaceless =
        eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    CHECK(surfaceless != EGL_NO_DISPLAY);
    CHECK(eglInitialize(surfaceless, NULL, NULL));
    CHECK(eglGetConfigs(surfaceless, &config, 1, &count) && count > 0);
    CHECK(eglGetConfigAttrib(surfaceless, config, EGL_NATIVE_VISUAL_TYPE, &visual_type));
    CHECK(visual_type == EGL_NONE);
    window = (Window)1;
    CHECK(eglCreatePlatformWindowSurfaceEXT(surfaceless, config, &window, NULL) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_NATIVE_WINDOW);
    CHECK(eglTerminate(surfaceless));

    x = XOpenDisplay(NULL);
    CHECK(x != NULL);
    screen = DefaultScreen(x);
    core_attrs[0] = EGL_PLATFORM_X11_SCREEN_KHR;
    core_attrs[1] = (EGLAttrib)screen;
    core_attrs[2] = EGL_NONE;
    ext_attrs[0] = EGL_PLATFORM_X11_SCREEN_EXT;
    ext_attrs[1] = screen;
    ext_attrs[2] = EGL_NONE;

    d_core = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, x, core_attrs);
    CHECK(d_core != EGL_NO_DISPLAY);
    d_ext = eglGetPlatformDisplayEXT(EGL_PLATFORM_X11_EXT, x, ext_attrs);
    CHECK(d_ext == d_core);
    CHECK(eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, x, bad_core_attrs) == EGL_NO_DISPLAY);
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
    CHECK(eglGetPlatformDisplayEXT(EGL_PLATFORM_X11_EXT, x, bad_ext_attrs) == EGL_NO_DISPLAY);
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);

    CHECK(eglInitialize(d_core, NULL, NULL));
    CHECK(eglGetConfigs(d_core, &config, 1, &count) && count > 0);
    CHECK(eglGetConfigAttrib(d_core, config, EGL_NATIVE_VISUAL_ID, &visual));
    CHECK(visual == (EGLint)XVisualIDFromVisual(DefaultVisual(x, screen)));
    CHECK(eglGetConfigAttrib(d_core, config, EGL_NATIVE_VISUAL_TYPE, &visual_type));
    CHECK(visual_type == DefaultVisual(x, screen)->class);

    window = XCreateSimpleWindow(x, RootWindow(x, screen), 0, 0, 32, 24, 0, 0, 0);
    CHECK(window != None);
    XMapWindow(x, window);
    XSync(x, False);
    s_core = eglCreatePlatformWindowSurface(d_core, config, &window, core_window_attrs);
    CHECK(s_core != EGL_NO_SURFACE);
    CHECK(eglQuerySurface(d_core, s_core, EGL_RENDER_BUFFER, &visual_type));
    CHECK(visual_type == EGL_SINGLE_BUFFER);
    CHECK(eglCreatePlatformWindowSurfaceEXT(d_ext, config, &window, ext_window_attrs) ==
          EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_ALLOC);
    CHECK(eglDestroySurface(d_core, s_core));
    s_ext = eglCreatePlatformWindowSurfaceEXT(d_ext, config, &window, ext_window_attrs);
    CHECK(s_ext != EGL_NO_SURFACE);
    CHECK(eglQuerySurface(d_ext, s_ext, EGL_RENDER_BUFFER, &visual_type));
    CHECK(visual_type == EGL_SINGLE_BUFFER);
    CHECK(eglDestroySurface(d_ext, s_ext));
    CHECK(eglTerminate(d_core));
    XDestroyWindow(x, window);
    XCloseDisplay(x);

    /* EGL_DEFAULT_DISPLAY selects the internally opened X11 connection. */
    d_default = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, EGL_DEFAULT_DISPLAY, NULL);
    CHECK(d_default != EGL_NO_DISPLAY);
    CHECK(eglInitialize(d_default, NULL, NULL));
    CHECK(eglTerminate(d_default));

    puts("egl_platform_extensions: PASS");
    return 0;
}

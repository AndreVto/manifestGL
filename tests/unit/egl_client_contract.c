#include "mgl/egl.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "egl_client_contract: CHECK failed %s:%d: %s (egl=%#x)\n", __FILE__,   \
                    __LINE__, #x, eglGetError());                                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
#define CHECK_HELPER(x)                                                                            \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "egl_client_contract: CHECK failed %s:%d: %s (egl=%#x)\n", __FILE__,   \
                    __LINE__, #x, eglGetError());                                                  \
            return 0;                                                                              \
        }                                                                                          \
    } while (0)

struct fresh_thread_make_current_args {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int result;
};

static void *fresh_thread_make_current(void *opaque) {
    struct fresh_thread_make_current_args *args = opaque;

    args->result = 0;
    if (eglQueryAPI() != EGL_NONE)
        return NULL;
    if (!eglBindAPI(EGL_OPENGL_API))
        return NULL;
    if (!eglMakeCurrent(args->display, args->surface, args->surface, args->context))
        return NULL;
    if (eglGetCurrentContext() != args->context)
        return NULL;
    if (!eglSwapInterval(args->display, 2))
        return NULL;
    if (!eglSwapBuffers(args->display, args->surface))
        return NULL;
    if (!eglMakeCurrent(args->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
        return NULL;
    args->result = 1;
    return NULL;
}

struct fresh_thread_swap_args {
    EGLDisplay display;
    EGLSurface surface;
    int result;
};

static void *fresh_thread_swap_recovery(void *opaque) {
    struct fresh_thread_swap_args *args = opaque;
    EGLContext fallback;

    args->result = 0;
    if (eglQueryAPI() != EGL_NONE)
        return NULL;
    if (eglSwapBuffers(args->display, args->surface))
        return NULL;
    if (eglGetError() != EGL_BAD_SURFACE)
        return NULL;
    if (!eglBindAPI(EGL_OPENGL_API))
        return NULL;
    fallback = eglCreateContext(args->display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, NULL);
    if (fallback == EGL_NO_CONTEXT)
        return NULL;
    if (!eglMakeCurrent(args->display, args->surface, args->surface, fallback)) {
        (void)eglDestroyContext(args->display, fallback);
        return NULL;
    }
    /* The cached drawable interval is restored when a temporary context becomes current. */
    if (!eglSwapInterval(args->display, 3)) {
        (void)eglMakeCurrent(args->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        (void)eglDestroyContext(args->display, fallback);
        return NULL;
    }
    if (!eglSwapBuffers(args->display, args->surface)) {
        (void)eglMakeCurrent(args->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        (void)eglDestroyContext(args->display, fallback);
        return NULL;
    }
    if (!eglMakeCurrent(args->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
        (void)eglDestroyContext(args->display, fallback);
        return NULL;
    }
    if (!eglDestroyContext(args->display, fallback))
        return NULL;
    args->result = 1;
    return NULL;
}

static int has_extension(const char *list, const char *name) {
    size_t n = strlen(name);
    const char *p = list;
    while (p && *p) {
        const char *hit = strstr(p, name);
        if (!hit)
            return 0;
        if ((hit == list || hit[-1] == ' ') && (hit[n] == '\0' || hit[n] == ' '))
            return 1;
        p = hit + n;
    }
    return 0;
}

static int check_core_proc_table(void) {
    static const char *const names[] = {"eglBindAPI",
                                        "eglBindTexImage",
                                        "eglChooseConfig",
                                        "eglClientWaitSync",
                                        "eglCopyBuffers",
                                        "eglCreateContext",
                                        "eglCreateImage",
                                        "eglCreatePbufferFromClientBuffer",
                                        "eglCreatePbufferSurface",
                                        "eglCreatePixmapSurface",
                                        "eglCreatePlatformPixmapSurface",
                                        "eglCreatePlatformWindowSurface",
                                        "eglCreateSync",
                                        "eglCreateWindowSurface",
                                        "eglDestroyContext",
                                        "eglDestroyImage",
                                        "eglDestroySurface",
                                        "eglDestroySync",
                                        "eglGetConfigAttrib",
                                        "eglGetConfigs",
                                        "eglGetCurrentContext",
                                        "eglGetCurrentDisplay",
                                        "eglGetCurrentSurface",
                                        "eglGetDisplay",
                                        "eglGetError",
                                        "eglGetPlatformDisplay",
                                        "eglGetProcAddress",
                                        "eglGetSyncAttrib",
                                        "eglInitialize",
                                        "eglMakeCurrent",
                                        "eglQueryAPI",
                                        "eglQueryContext",
                                        "eglQueryString",
                                        "eglQuerySurface",
                                        "eglReleaseTexImage",
                                        "eglReleaseThread",
                                        "eglSurfaceAttrib",
                                        "eglSwapBuffers",
                                        "eglSwapInterval",
                                        "eglTerminate",
                                        "eglWaitClient",
                                        "eglWaitGL",
                                        "eglWaitNative",
                                        "eglWaitSync"};
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (eglGetProcAddress(names[i]) == NULL) {
            fprintf(stderr, "egl_client_contract: missing proc %s\n", names[i]);
            return 0;
        }
    }
    return 1;
}

static int probe_context_versions(EGLDisplay d, EGLConfig config) {
    static const int versions[] = {46, 45, 44, 43, 42, 41, 40, 33, 32, 31,
                                   30, 21, 20, 15, 14, 13, 12, 11, 10};
    EGLContext compat = EGL_NO_CONTEXT, core = EGL_NO_CONTEXT;
    int requested_core_version = 0;
    size_t i;

    CHECK_HELPER(eglBindAPI(EGL_OPENGL_API));
    for (i = 0; i < sizeof(versions) / sizeof(versions[0]) &&
                (compat == EGL_NO_CONTEXT || core == EGL_NO_CONTEXT);
         ++i) {
        EGLint attrs[] = {EGL_CONTEXT_MAJOR_VERSION,
                          versions[i] / 10,
                          EGL_CONTEXT_MINOR_VERSION,
                          versions[i] % 10,
                          EGL_CONTEXT_OPENGL_PROFILE_MASK,
                          EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
                          EGL_NONE};
        if (compat == EGL_NO_CONTEXT && versions[i] >= 30) {
            compat = eglCreateContext(d, config, EGL_NO_CONTEXT, attrs);
            if (compat == EGL_NO_CONTEXT)
                (void)eglGetError();
        }
        if (core == EGL_NO_CONTEXT) {
            attrs[5] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
            core = eglCreateContext(d, config, EGL_NO_CONTEXT, attrs);
            if (core == EGL_NO_CONTEXT)
                (void)eglGetError();
            else
                requested_core_version = versions[i];
        }
    }
    CHECK_HELPER(compat != EGL_NO_CONTEXT);
    CHECK_HELPER(core != EGL_NO_CONTEXT);
    {
        EGLint profile = -1;
        EGLint major = -1, minor = -1;
        CHECK_HELPER(eglQueryContext(d, core, EGL_CONTEXT_OPENGL_PROFILE_MASK, &profile));
        CHECK_HELPER(eglQueryContext(d, core, EGL_CONTEXT_MAJOR_VERSION, &major));
        CHECK_HELPER(eglQueryContext(d, core, EGL_CONTEXT_MINOR_VERSION, &minor));
        CHECK_HELPER(profile == EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT &&
                     major * 10 + minor >= requested_core_version);
    }
    CHECK_HELPER(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, compat));
    CHECK_HELPER(eglGetCurrentContext() == compat);
    CHECK_HELPER(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    CHECK_HELPER(eglDestroyContext(d, compat));
    if (core != EGL_NO_CONTEXT)
        CHECK_HELPER(eglDestroyContext(d, core));
    return 1;
}

static int check_config_contract(EGLDisplay d, EGLConfig *out) {
    static const EGLint attrs[] = {EGL_RENDERABLE_TYPE,
                                   EGL_NATIVE_VISUAL_ID,
                                   EGL_SURFACE_TYPE,
                                   EGL_CONFIG_ID,
                                   EGL_NATIVE_RENDERABLE,
                                   EGL_COLOR_BUFFER_TYPE,
                                   EGL_RED_SIZE,
                                   EGL_GREEN_SIZE,
                                   EGL_BLUE_SIZE,
                                   EGL_ALPHA_SIZE,
                                   EGL_DEPTH_SIZE,
                                   EGL_STENCIL_SIZE,
                                   EGL_BUFFER_SIZE,
                                   EGL_TRANSPARENT_TYPE,
                                   EGL_TRANSPARENT_RED_VALUE,
                                   EGL_TRANSPARENT_GREEN_VALUE,
                                   EGL_TRANSPARENT_BLUE_VALUE,
                                   EGL_SAMPLE_BUFFERS,
                                   EGL_SAMPLES,
                                   EGL_MAX_PBUFFER_WIDTH,
                                   EGL_MAX_PBUFFER_HEIGHT,
                                   EGL_MAX_PBUFFER_PIXELS};
    EGLConfig configs[8];
    EGLint count = 0, render = 0, value = 0;
    size_t i;

    CHECK_HELPER(eglGetConfigs(d, NULL, 0, &count) && count > 0);
    if (count > (EGLint)(sizeof(configs) / sizeof(configs[0])))
        count = (EGLint)(sizeof(configs) / sizeof(configs[0]));
    CHECK_HELPER(eglGetConfigs(d, configs, count, &count) && count > 0);
    CHECK_HELPER(eglGetConfigAttrib(d, configs[0], EGL_RENDERABLE_TYPE, &render));
    CHECK_HELPER((render & EGL_OPENGL_BIT) != 0);
    for (i = 0; i < sizeof(attrs) / sizeof(attrs[0]); ++i)
        CHECK_HELPER(eglGetConfigAttrib(d, configs[0], attrs[i], &value));
    *out = configs[0];
    return 1;
}

int main(void) {
    static const EGLint pb_attrs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 12, EGL_NONE};
    const char *exts;
    EGLDisplay surf, xdpy;
    EGLConfig config;
    EGLSurface pb, win_surface, test_pbuffer;
    EGLint major = 0, minor = 0, width = 0, height = 0, visual_id = 0;
    Display *x;
    XVisualInfo template_vi, *vis = NULL;
    XSetWindowAttributes swa;
    Window window;
    int nvis = 0, screen;

    CHECK(XInitThreads() != 0);

    exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    CHECK(exts != NULL);
    CHECK(has_extension(exts, "EGL_KHR_client_get_all_proc_addresses"));
    CHECK(has_extension(exts, "EGL_EXT_platform_base"));
    CHECK(has_extension(exts, "EGL_KHR_platform_x11"));
    CHECK(has_extension(exts, "EGL_MESA_platform_surfaceless"));
    CHECK(check_core_proc_table());
    CHECK(eglGetProcAddress("eglGetPlatformDisplayEXT") != NULL);
    CHECK(eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT") != NULL);
    CHECK(eglGetProcAddress("eglCreatePlatformPixmapSurfaceEXT") != NULL);
    CHECK(dlsym(RTLD_DEFAULT, "wine_android_register_egl_window") != NULL);

    surf = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    CHECK(surf != EGL_NO_DISPLAY);
    CHECK(eglInitialize(surf, &major, &minor));
    CHECK(major > 1 || (major == 1 && minor >= 5));
    exts = eglQueryString(surf, EGL_EXTENSIONS);
    CHECK(exts != NULL);
    CHECK(has_extension(exts, "EGL_KHR_create_context"));
    CHECK(has_extension(exts, "EGL_KHR_create_context_no_error"));
    CHECK(has_extension(exts, "EGL_KHR_no_config_context"));
    CHECK(check_config_contract(surf, &config));
    CHECK(probe_context_versions(surf, config));

    pb = eglCreatePbufferSurface(surf, config, pb_attrs);
    CHECK(pb != EGL_NO_SURFACE);
    CHECK(eglQuerySurface(surf, pb, EGL_WIDTH, &width) && width == 16);
    CHECK(eglQuerySurface(surf, pb, EGL_HEIGHT, &height) && height == 12);
    CHECK(eglDestroySurface(surf, pb));
    CHECK(eglTerminate(surf));

    x = XOpenDisplay(NULL);
    CHECK(x != NULL);
    screen = DefaultScreen(x);
    xdpy = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, x, NULL);
    CHECK(xdpy != EGL_NO_DISPLAY);
    CHECK(eglInitialize(xdpy, NULL, NULL));
    CHECK(check_config_contract(xdpy, &config));
    CHECK(eglGetConfigAttrib(xdpy, config, EGL_NATIVE_VISUAL_ID, &visual_id));

    memset(&template_vi, 0, sizeof(template_vi));
    template_vi.visualid = (VisualID)visual_id;
    vis = XGetVisualInfo(x, VisualIDMask, &template_vi, &nvis);
    CHECK(vis != NULL && nvis > 0);
    swa.colormap = XCreateColormap(x, RootWindow(x, screen), vis[0].visual, AllocNone);
    swa.event_mask = StructureNotifyMask;
    window = XCreateWindow(x, RootWindow(x, screen), 0, 0, 32, 24, 0, vis[0].depth, InputOutput,
                           vis[0].visual, CWColormap | CWEventMask, &swa);
    CHECK(window != None);
    XMapWindow(x, window);
    XSync(x, False);

    win_surface = eglCreateWindowSurface(xdpy, config, (EGLNativeWindowType)window, NULL);
    CHECK(win_surface != EGL_NO_SURFACE);
    CHECK(eglBindAPI(EGL_OPENGL_API));
    test_pbuffer = eglCreatePbufferSurface(xdpy, config, pb_attrs);
    CHECK(test_pbuffer != EGL_NO_SURFACE);
    {
        EGLContext unregistered = eglCreateContext(xdpy, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, NULL);
        CHECK(unregistered != EGL_NO_CONTEXT);
        CHECK(!eglMakeCurrent(xdpy, win_surface, win_surface, unregistered));
        CHECK(eglGetError() == EGL_BAD_MATCH);
        CHECK(!eglSwapBuffers(xdpy, win_surface));
        CHECK(eglGetError() == EGL_BAD_SURFACE);
        CHECK(eglDestroyContext(xdpy, unregistered));
    }
    {
        EGLContext c = eglCreateContext(xdpy, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, NULL);
        CHECK(c != EGL_NO_CONTEXT);
        CHECK(eglMakeCurrent(xdpy, test_pbuffer, test_pbuffer, c));
        CHECK(eglSwapInterval(xdpy, 1));
        CHECK(eglSwapBuffers(xdpy, test_pbuffer));
        CHECK(eglMakeCurrent(xdpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

        /* A context must remain usable when installed on a fresh thread. */
        {
            struct fresh_thread_make_current_args args = {xdpy, test_pbuffer, c, 0};
            pthread_t thread;
            CHECK(pthread_create(&thread, NULL, fresh_thread_make_current, &args) == 0);
            CHECK(pthread_join(thread, NULL) == 0);
            CHECK(args.result);
        }

        /* Retry a no-current-context swap through the temporary-context fallback. */
        CHECK(!eglSwapBuffers(xdpy, test_pbuffer));
        CHECK(eglGetError() == EGL_BAD_SURFACE);
        CHECK(eglGetCurrentContext() == EGL_NO_CONTEXT);
        {
            EGLContext fallback = eglCreateContext(xdpy, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, NULL);
            CHECK(fallback != EGL_NO_CONTEXT);
            CHECK(eglMakeCurrent(xdpy, test_pbuffer, test_pbuffer, fallback));
            CHECK(eglSwapBuffers(xdpy, test_pbuffer));
            CHECK(eglMakeCurrent(xdpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
            CHECK(eglDestroyContext(xdpy, fallback));
        }
        CHECK(eglDestroyContext(xdpy, c));
    }
    {
        struct fresh_thread_swap_args args = {xdpy, test_pbuffer, 0};
        pthread_t thread;
        CHECK(pthread_create(&thread, NULL, fresh_thread_swap_recovery, &args) == 0);
        CHECK(pthread_join(thread, NULL) == 0);
        CHECK(args.result);
    }
    CHECK(eglDestroySurface(xdpy, test_pbuffer));
    CHECK(eglDestroySurface(xdpy, win_surface));
    CHECK(eglTerminate(xdpy));
    XDestroyWindow(x, window);
    XFreeColormap(x, swa.colormap);
    XFree(vis);
    XCloseDisplay(x);

    puts("egl_client_contract: PASS");
    return 0;
}

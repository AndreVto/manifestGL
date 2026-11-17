#define _POSIX_C_SOURCE 200809L
#include "mgl/egl.h"
#include "mgl/platform.h"

#include <X11/Xlib.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "egl_direct_present: CHECK failed %s:%d: %s (egl=%#x)\n", __FILE__,    \
                    __LINE__, #x, eglGetError());                                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

struct direct_recovery_args {
    EGLDisplay display;
    EGLSurface surface;
    int result;
};

static void *direct_recovery_thread(void *opaque) {
    struct direct_recovery_args *args = opaque;
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
    if (!eglMakeCurrent(args->display, args->surface, args->surface, fallback))
        goto destroy;
    if (!eglSwapInterval(args->display, 2))
        goto unbind;
    if (!eglSwapBuffers(args->display, args->surface))
        goto unbind;
    if (!eglMakeCurrent(args->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
        goto destroy;
    if (!eglDestroyContext(args->display, fallback))
        return NULL;
    args->result = 1;
    return NULL;

unbind:
    (void)eglMakeCurrent(args->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
destroy:
    (void)eglDestroyContext(args->display, fallback);
    return NULL;
}

int main(void) {
    const char *bridge_path = getenv("WINE_ANDROID_DIRECT_PRESENT_LIB");
    const char *native_path = getenv("MGL_EGL_LIBRARY");
    void *bridge_lib;
    void *native_lib;
    void (*set_window)(uintptr_t);
    unsigned (*acquire_count)(void);
    unsigned (*retire_count)(void);
    uint32_t (*last_drawable)(void);
    uint32_t (*last_whole)(void);
    uint64_t (*last_hwnd)(void);
    uint64_t (*last_owner)(void);
    void (*set_fail_swap)(int, int);
    unsigned (*create_pbuffer_count)(void);
    unsigned (*create_window_count)(void);
    unsigned (*swap_count)(void);
    unsigned (*make_current_count)(void);
    unsigned (*swap_interval_count)(void);
    int (*last_swap_interval)(void);
    void *(*last_native_window)(void);
    Display *x;
    EGLDisplay d;
    EGLConfig config;
    EGLContext context;
    EGLSurface surface;
    EGLSurface pbuffer;
    EGLSurface unregistered_surface;
    Window unregistered_window;
    EGLint count = 0;
    Window window;
    unsigned base_pbuffers;
    unsigned base_windows;
    unsigned base_swaps;

    CHECK(bridge_path != NULL && bridge_path[0] != '\0');
    CHECK(native_path != NULL && native_path[0] != '\0');
    bridge_lib = dlopen(bridge_path, RTLD_NOW | RTLD_LOCAL);
    native_lib = dlopen(native_path, RTLD_NOW | RTLD_LOCAL);
    CHECK(bridge_lib != NULL && native_lib != NULL);

    *(void **)(&set_window) = dlsym(bridge_lib, "mgl_test_bridge_set_window");
    *(void **)(&acquire_count) = dlsym(bridge_lib, "mgl_test_bridge_acquire_count");
    *(void **)(&retire_count) = dlsym(bridge_lib, "mgl_test_bridge_retire_count");
    *(void **)(&last_drawable) = dlsym(bridge_lib, "mgl_test_bridge_last_drawable");
    *(void **)(&last_whole) = dlsym(bridge_lib, "mgl_test_bridge_last_whole");
    *(void **)(&last_hwnd) = dlsym(bridge_lib, "mgl_test_bridge_last_hwnd");
    *(void **)(&last_owner) = dlsym(bridge_lib, "mgl_test_bridge_last_owner");
    *(void **)(&set_fail_swap) = dlsym(native_lib, "mgl_test_native_egl_set_fail_swap");
    *(void **)(&create_pbuffer_count) =
        dlsym(native_lib, "mgl_test_native_egl_create_pbuffer_count");
    *(void **)(&create_window_count) = dlsym(native_lib, "mgl_test_native_egl_create_window_count");
    *(void **)(&swap_count) = dlsym(native_lib, "mgl_test_native_egl_swap_count");
    *(void **)(&make_current_count) = dlsym(native_lib, "mgl_test_native_egl_make_current_count");
    *(void **)(&swap_interval_count) = dlsym(native_lib, "mgl_test_native_egl_swap_interval_count");
    *(void **)(&last_swap_interval) = dlsym(native_lib, "mgl_test_native_egl_last_swap_interval");
    *(void **)(&last_native_window) = dlsym(native_lib, "mgl_test_native_egl_last_native_window");
    CHECK(set_window && acquire_count && retire_count && last_drawable && last_whole && last_hwnd &&
          last_owner);
    CHECK(set_fail_swap && create_pbuffer_count && create_window_count && swap_count &&
          make_current_count && swap_interval_count && last_swap_interval && last_native_window);

    x = XOpenDisplay(NULL);
    CHECK(x != NULL);
    d = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, x, NULL);
    CHECK(d != EGL_NO_DISPLAY && eglInitialize(d, NULL, NULL));
    CHECK(eglGetConfigs(d, &config, 1, &count) && count > 0);

    window = XCreateSimpleWindow(x, DefaultRootWindow(x), 0, 0, 80, 60, 0, 0, 0);
    XMapWindow(x, window);
    XSync(x, False);
    surface = eglCreateWindowSurface(d, config, (EGLNativeWindowType)(uintptr_t)window, NULL);
    CHECK(surface != EGL_NO_SURFACE);
    pbuffer =
        eglCreatePbufferSurface(d, config, (const EGLint[]){EGL_WIDTH, 4, EGL_HEIGHT, 4, EGL_NONE});
    CHECK(pbuffer != EGL_NO_SURFACE);
    CHECK(!wine_android_register_egl_window(d, pbuffer, UINT64_C(0x2222), 9u));
    CHECK(wine_android_register_egl_window(d, surface, UINT64_C(0xABCDEF), 77u));
    CHECK(!wine_android_register_egl_window(d, surface, UINT64_C(0xABCDEF), 77u));

    CHECK(eglBindAPI(EGL_OPENGL_API));
    context = eglCreateContext(d, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, NULL);
    CHECK(context != EGL_NO_CONTEXT);

    /* Unregistered native windows fail presentation selection. */
    unregistered_window = XCreateSimpleWindow(x, DefaultRootWindow(x), 0, 0, 80, 60, 0, 0, 0);
    CHECK(unregistered_window != None);
    XMapWindow(x, unregistered_window);
    XSync(x, False);
    unregistered_surface = eglCreateWindowSurface(
        d, config, (EGLNativeWindowType)(uintptr_t)unregistered_window, NULL);
    CHECK(unregistered_surface != EGL_NO_SURFACE);
    CHECK(setenv("WINE_ANDROID_DIRECT_PRESENT", "1", 1) == 0);
    CHECK(!eglMakeCurrent(d, unregistered_surface, unregistered_surface, context));
    CHECK(eglGetError() == EGL_BAD_MATCH);
    CHECK(eglDestroySurface(d, unregistered_surface));
    XDestroyWindow(x, unregistered_window);
    base_pbuffers = create_pbuffer_count();
    base_windows = create_window_count();
    base_swaps = swap_count();

    CHECK(eglMakeCurrent(d, surface, surface, context));
    CHECK(create_window_count() == base_windows + 1u);
    CHECK(create_pbuffer_count() == base_pbuffers);
    CHECK(last_native_window() == (void *)(uintptr_t)0x1000u);
    CHECK(acquire_count() == 1u);
    CHECK(last_drawable() == (uint32_t)window);
    CHECK(last_whole() == 77u && last_hwnd() == UINT64_C(0xABCDEF));
    CHECK((last_owner() >> 48) == UINT64_C(0x474c));

    CHECK(eglSwapBuffers(d, surface));
    CHECK(swap_count() == base_swaps + 1u);
    {
        unsigned before_bind = make_current_count();
        unsigned before_windows = create_window_count();
        EGLint resized_width = 0;
        XResizeWindow(x, window, 120, 60);
        XSync(x, False);
        CHECK(eglSwapBuffers(d, surface));
        CHECK(make_current_count() >= before_bind + 2u);
        CHECK(create_window_count() == before_windows + 1u);
        CHECK(eglQuerySurface(d, surface, EGL_WIDTH, &resized_width) && resized_width == 120);
        before_bind = make_current_count();
        CHECK(eglSwapBuffers(d, surface));
        CHECK(make_current_count() == before_bind);
        XResizeWindow(x, window, 80, 60);
        XSync(x, False);
        CHECK(eglSwapBuffers(d, surface));
        CHECK(make_current_count() >= before_bind + 2u);
        CHECK(create_window_count() == before_windows + 2u);
    }
    base_windows = create_window_count() - 1u;
    CHECK(eglSwapInterval(d, 1));
    CHECK(swap_interval_count() >= 1u && last_swap_interval() == 3);
    base_swaps = swap_count();

    set_window((uintptr_t)0u);
    CHECK(eglSwapBuffers(d, surface));
    CHECK(create_pbuffer_count() == base_pbuffers + 1u);
    /* Losing the SurfaceView switches to the recovery pbuffer and drops that
     * generation's frame; there is no native window swap for the dropped frame. */
    CHECK(swap_count() == base_swaps);
    CHECK(swap_interval_count() >= 2u && last_swap_interval() == 3);

    set_window((uintptr_t)0x2000u);
    {
        unsigned before_intervals = swap_interval_count();
        CHECK(eglSwapBuffers(d, surface));
        CHECK(create_window_count() == base_windows + 2u);
        CHECK(swap_count() == base_swaps);
        CHECK(swap_interval_count() == before_intervals + 1u && last_swap_interval() == 3);
    }
    CHECK(eglSwapBuffers(d, surface));
    CHECK(swap_count() == base_swaps + 1u);

    set_fail_swap(1, EGL_BAD_NATIVE_WINDOW);
    CHECK(eglSwapBuffers(d, surface));
    CHECK(create_window_count() == base_windows + 3u);
    CHECK(swap_count() == base_swaps + 2u);
    set_fail_swap(0, EGL_SUCCESS);
    CHECK(eglSwapBuffers(d, surface));
    CHECK(swap_count() == base_swaps + 3u);

    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    {
        struct direct_recovery_args args = {d, surface, 0};
        pthread_t thread;
        unsigned before_swaps = swap_count();
        unsigned before_intervals = swap_interval_count();

        /* No-current swap recovery must restore the registered surface and swap interval. */
        CHECK(pthread_create(&thread, NULL, direct_recovery_thread, &args) == 0);
        CHECK(pthread_join(thread, NULL) == 0);
        CHECK(args.result);
        CHECK(swap_interval_count() == before_intervals + 2u);
        CHECK(last_swap_interval() == 3);
        CHECK(swap_count() == before_swaps + 1u);
    }
    CHECK(eglDestroyContext(d, context));
    CHECK(eglDestroySurface(d, pbuffer));
    CHECK(eglDestroySurface(d, surface));
    CHECK(retire_count() == 1u);
    CHECK(eglTerminate(d));
    XDestroyWindow(x, window);
    XCloseDisplay(x);
    dlclose(native_lib);
    dlclose(bridge_lib);
    puts("egl_direct_present: PASS");
    return 0;
}

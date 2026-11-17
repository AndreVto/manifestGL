#define _POSIX_C_SOURCE 200809L
#include "mgl/glx.h"
#include "abi/proc_address.h"
#include "glx/drawable.h"

#include <X11/Xlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

static int fail(int n, const char *message) {
    fprintf(stderr, "GLX swap-control failure %d: %s\n", n, message);
    return n;
}

static int query_interval(Display *dpy, GLXDrawable drawable, unsigned int expected, int code) {
    unsigned int value = 0xffffffffu;
    glXQueryDrawable(dpy, drawable, GLX_SWAP_INTERVAL_EXT, &value);
    if (value != expected)
        return fail(code, "canonical drawable interval");
    return 0;
}

int main(void) {
    const char *native_path = getenv("MGL_EGL_LIBRARY");
    void *native_lib;
    unsigned (*swap_interval_count)(void);
    int (*last_swap_interval)(void);
    Display *dpy;
    GLXContext ctx;
    GLXPbuffer first;
    GLXPbuffer second;
    unsigned before;
    int rc;
    const int pbattrs[] = {GLX_PBUFFER_WIDTH, 4, GLX_PBUFFER_HEIGHT, 4, 0};

    if (native_path == NULL || native_path[0] == '\0')
        return fail(1, "native path");
    native_lib = dlopen(native_path, RTLD_NOW | RTLD_LOCAL);
    if (native_lib == NULL)
        return fail(2, "native dlopen");
    *(void **)(&swap_interval_count) = dlsym(native_lib, "mgl_test_native_egl_swap_interval_count");
    *(void **)(&last_swap_interval) = dlsym(native_lib, "mgl_test_native_egl_last_swap_interval");
    if (swap_interval_count == NULL || last_swap_interval == NULL)
        return fail(3, "swap interval counters");

    dpy = XOpenDisplay(NULL);
    if (dpy == NULL)
        return fail(4, "XOpenDisplay");
    first = glXCreatePbuffer(dpy, NULL, pbattrs);
    second = glXCreatePbuffer(dpy, NULL, pbattrs);
    ctx = glXCreateContext(dpy, NULL, NULL, True);
    if (first == 0 || second == 0 || ctx == NULL)
        return fail(5, "GLX object creation");

    /* Direct/native registration must not overwrite a swap interval the
     * application explicitly chose before registration. */
    if (!mgl_glx_drawable_set_initial_swap_interval(dpy, first, 0))
        return fail(28, "default interval setter could not find drawable");
    if ((rc = query_interval(dpy, first, 0u, 29)) != 0)
        return rc;

    before = swap_interval_count();
    glXSwapIntervalEXT(dpy, first, 2);
    if (swap_interval_count() != before)
        return fail(6, "non-current interval reached EGL");
    if ((rc = query_interval(dpy, first, 2u, 7)) != 0)
        return rc;
    if (!mgl_glx_drawable_set_initial_swap_interval(dpy, first, 0))
        return fail(30, "default interval setter failed after explicit request");
    if ((rc = query_interval(dpy, first, 2u, 31)) != 0)
        return rc;

    if (!glXMakeCurrent(dpy, first, ctx))
        return fail(8, "make first current");
    if (swap_interval_count() != before + 1u || last_swap_interval() != 2)
        return fail(9, "stored first interval not realized on bind");

    before = swap_interval_count();
    glXSwapIntervalEXT(dpy, second, 3);
    if (swap_interval_count() != before)
        return fail(10, "second interval changed current EGL surface");
    if ((rc = query_interval(dpy, second, 3u, 11)) != 0)
        return rc;

    if (!glXMakeCurrent(dpy, second, ctx))
        return fail(12, "make second current");
    if (swap_interval_count() != before + 1u || last_swap_interval() != 3)
        return fail(13, "stored second interval not realized on bind");

    before = swap_interval_count();
    glXSwapIntervalEXT(dpy, first, 4);
    if (swap_interval_count() != before)
        return fail(14, "inactive first interval reached EGL");
    if (!glXMakeCurrent(dpy, first, ctx))
        return fail(15, "rebind first");
    if (swap_interval_count() != before + 1u || last_swap_interval() != 4)
        return fail(16, "updated first interval not realized on rebind");

    before = swap_interval_count();
    glXSwapIntervalEXT(dpy, first, 5);
    if (swap_interval_count() != before + 1u || last_swap_interval() != 5)
        return fail(17, "current interval not realized immediately");
    if ((rc = query_interval(dpy, first, 5u, 18)) != 0)
        return rc;

    before = swap_interval_count();
    glXSwapIntervalEXT(dpy, (GLXDrawable)0x7ffffff0u, 6);
    if (swap_interval_count() != before)
        return fail(19, "invalid drawable reached EGL");
    glXSwapIntervalEXT(dpy, first, -1);
    if (swap_interval_count() != before)
        return fail(20, "negative interval reached EGL");
    if ((rc = query_interval(dpy, first, 5u, 21)) != 0)
        return rc;

    if (glXSwapIntervalMESA(6u) != 0)
        return fail(22, "MESA current interval result");
    if (last_swap_interval() != 6 || glXGetSwapIntervalMESA() != 6)
        return fail(23, "MESA interval state");
    before = swap_interval_count();
    if (glXSwapIntervalSGI(0) == 0 || swap_interval_count() != before)
        return fail(24, "SGI zero interval validation");
    if (glXSwapIntervalSGI(7) != 0 || last_swap_interval() != 7)
        return fail(25, "SGI interval realization");

    if (mgl_proc_address("glXSwapIntervalEXT") == NULL ||
        mgl_proc_address("glXSwapIntervalMESA") == NULL ||
        mgl_proc_address("glXGetSwapIntervalMESA") == NULL ||
        mgl_proc_address("glXSwapIntervalSGI") == NULL)
        return fail(26, "proc addresses");

    if (!glXMakeCurrent(dpy, 0, NULL))
        return fail(27, "unbind");
    glXDestroyContext(dpy, ctx);
    glXDestroyPbuffer(dpy, second);
    glXDestroyPbuffer(dpy, first);
    XCloseDisplay(dpy);
    dlclose(native_lib);
    puts("GLX per-drawable swap-control semantics passed");
    return 0;
}

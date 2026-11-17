#define _POSIX_C_SOURCE 200809L
#include "mgl/glx.h"
#include "abi/proc_address.h"
#include "glx/drawable.h"

#include <X11/Xlib.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int fail(int n, const char *message) {
    fprintf(stderr, "GLX wait-sync failure %d: %s\n", n, message);
    return n;
}

static int wait_for_geometry(Display *dpy, GLXDrawable drawable, unsigned wanted_width,
                             unsigned wanted_height) {
    struct timespec delay = {0, 1000000L};
    unsigned width, height;
    int i;
    for (i = 0; i < 1500; ++i) {
        if (mgl_glx_drawable_watch_poll(dpy, drawable) != 1)
            return 0;
        if (mgl_glx_drawable_info(dpy, drawable, NULL, NULL, &width, &height, NULL) &&
            width == wanted_width && height == wanted_height)
            return 1;
        (void)nanosleep(&delay, NULL);
    }
    return 0;
}

int main(void) {
    const char *native_path = getenv("MGL_EGL_LIBRARY");
    void *native_lib;
    unsigned (*finish_count)(void);
    Display *dpy1;
    Display *dpy2;
    GLXContext ctx;
    GLXPbuffer pbuffer;
    GLXWindow glxwin;
    GLXFBConfig *configs;
    int config_count;
    unsigned int cached_width = 0u, cached_height = 0u;
    Window win;
    XEvent event;
    Window root;
    int x, y;
    unsigned width, height, border, depth;
    unsigned before;
    const int pbattrs[] = {GLX_PBUFFER_WIDTH, 4, GLX_PBUFFER_HEIGHT, 4, 0};

    if (native_path == NULL || native_path[0] == '\0')
        return fail(1, "native path");
    native_lib = dlopen(native_path, RTLD_NOW | RTLD_LOCAL);
    if (native_lib == NULL)
        return fail(2, "native dlopen");
    *(void **)(&finish_count) = dlsym(native_lib, "mgl_test_native_egl_finish_count");
    if (finish_count == NULL)
        return fail(3, "finish counter symbol");

    dpy1 = XOpenDisplay(NULL);
    dpy2 = XOpenDisplay(NULL);
    if (dpy1 == NULL || dpy2 == NULL)
        return fail(4, "XOpenDisplay");

    pbuffer = glXCreatePbuffer(dpy1, NULL, pbattrs);
    ctx = glXCreateContext(dpy1, NULL, NULL, True);
    if (pbuffer == 0 || ctx == NULL)
        return fail(5, "GLX context/pbuffer creation");
    if (!glXMakeCurrent(dpy1, pbuffer, ctx))
        return fail(6, "glXMakeCurrent");

    before = finish_count();
    glXWaitGL();
    if (finish_count() != before + 1u)
        return fail(7, "glXWaitGL did not finish GLES");

    win = XCreateSimpleWindow(dpy1, DefaultRootWindow(dpy1), 0, 0, 11u, 13u, 0u, 0u, 0u);
    XSync(dpy1, False);
    configs = glXGetFBConfigs(dpy1, DefaultScreen(dpy1), &config_count);
    if (configs == NULL || config_count < 1)
        return fail(13, "GLX configs");
    glxwin = glXCreateWindow(dpy1, configs[0], win, NULL);
    free(configs);
    if (glxwin == 0)
        return fail(13, "GLX window record");
    if (!mgl_glx_drawable_info(dpy1, glxwin, NULL, NULL, &cached_width, &cached_height, NULL) ||
        cached_width != 11u || cached_height != 13u)
        return fail(14, "initial cached geometry");
    if (!XGetGeometry(dpy2, win, &root, &x, &y, &width, &height, &border, &depth) || width != 11u ||
        height != 13u)
        return fail(8, "initial X geometry");

    XSelectInput(dpy1, win, StructureNotifyMask);
    XSync(dpy1, False);
    if (!mgl_glx_drawable_watch(dpy1, glxwin))
        return fail(21, "geometry watcher registration");
    XResizeWindow(dpy1, win, 37u, 29u);
    glXWaitX();
    if (!XGetGeometry(dpy2, win, &root, &x, &y, &width, &height, &border, &depth) || width != 37u ||
        height != 29u)
        return fail(9, "glXWaitX did not complete X requests");
    if (!wait_for_geometry(dpy1, glxwin, 37u, 29u))
        return fail(15, "resize not delivered without query");
    if (!XCheckTypedWindowEvent(dpy1, win, ConfigureNotify, &event) ||
        event.xconfigure.width != 37 || event.xconfigure.height != 29)
        return fail(22, "geometry watcher changed application events");
    XResizeWindow(dpy1, win, 41u, 31u);
    XSync(dpy1, False);
    if (!wait_for_geometry(dpy1, glxwin, 41u, 31u))
        return fail(23, "second resize while context remains bound");
    if (glXMakeCurrent(dpy1, glxwin, ctx))
        return fail(19, "unregistered window accepted as presentation target");
    if (glXGetCurrentDrawable() != pbuffer)
        return fail(20, "rejected window changed current drawable");
    glXQueryDrawable(dpy1, glxwin, GLX_WIDTH, &cached_width);
    if (cached_width != 41u)
        return fail(16, "explicit geometry refresh");
    if (!mgl_glx_drawable_info(dpy1, glxwin, NULL, NULL, &cached_width, &cached_height, NULL) ||
        cached_width != 41u || cached_height != 31u)
        return fail(17, "refreshed cached geometry");
    before = finish_count();
    glXSwapBuffers(dpy1, glxwin);
    if (finish_count() != before)
        return fail(18, "unregistered swap reached X11 presentation");

    if (!glXMakeCurrent(dpy1, 0, NULL))
        return fail(10, "unbind");
    before = finish_count();
    glXWaitGL();
    glXWaitX();
    if (finish_count() != before)
        return fail(11, "wait without current context");

    if (mgl_proc_address("glXWaitGL") == NULL || mgl_proc_address("glXWaitX") == NULL)
        return fail(12, "proc addresses");

    glXDestroyContext(dpy1, ctx);
    glXDestroyPbuffer(dpy1, pbuffer);
    XDestroyWindow(dpy1, win);
    XSync(dpy1, False);
    {
        struct timespec delay = {0, 1000000L};
        int i, observed = 0;
        for (i = 0; i < 1500; ++i) {
            if (mgl_glx_drawable_watch_poll(dpy1, glxwin) == -1) {
                observed = 1;
                break;
            }
            (void)nanosleep(&delay, NULL);
        }
        if (!observed)
            return fail(24, "window destruction was not observed");
        if (mgl_glx_drawable_info(dpy1, glxwin, NULL, NULL, NULL, NULL, NULL))
            return fail(25, "destroyed window still reports valid geometry");
    }
    glXDestroyWindow(dpy1, glxwin);
    XCloseDisplay(dpy2);
    XCloseDisplay(dpy1);
    dlclose(native_lib);
    puts("GLX wait synchronization semantics passed");
    return 0;
}

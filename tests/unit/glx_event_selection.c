#include "mgl/glx.h"
#include "abi/proc_address.h"
#include "glx/drawable.h"

#include <stdint.h>
#include <stdio.h>

static int fail(int n, const char *message) {
    fprintf(stderr, "GLX event selection failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    Display *dpy = NULL;
    GLXDrawable win;
    GLXDrawable pbuffer;
    GLXDrawable pixmap;
    unsigned long mask = ~0ul;

    win = mgl_glx_drawable_create(dpy, MGL_GLX_DRAWABLE_WINDOW, 0, 16, 16);
    pbuffer = mgl_glx_drawable_create(dpy, MGL_GLX_DRAWABLE_PBUFFER, 0, 8, 8);
    pixmap = mgl_glx_drawable_create(dpy, MGL_GLX_DRAWABLE_PIXMAP, 0, 4, 4);
    if (win == 0 || pbuffer == 0 || pixmap == 0)
        return fail(1, "drawable creation");

    glXGetSelectedEvent(dpy, win, &mask);
    if (mask != 0ul)
        return fail(2, "initial window mask");
    glXSelectEvent(dpy, win, GLX_PBUFFER_CLOBBER_MASK);
    glXGetSelectedEvent(dpy, win, &mask);
    if (mask != GLX_PBUFFER_CLOBBER_MASK)
        return fail(3, "window mask set");
    glXSelectEvent(dpy, win, 0ul);
    glXGetSelectedEvent(dpy, win, &mask);
    if (mask != 0ul)
        return fail(4, "window mask replace");

    glXSelectEvent(dpy, pbuffer, GLX_PBUFFER_CLOBBER_MASK);
    mask = 0ul;
    glXGetSelectedEvent(dpy, pbuffer, &mask);
    if (mask != GLX_PBUFFER_CLOBBER_MASK)
        return fail(5, "pbuffer mask");

    mask = 77ul;
    glXSelectEvent(dpy, pixmap, GLX_PBUFFER_CLOBBER_MASK);
    glXGetSelectedEvent(dpy, pixmap, &mask);
    if (mask != 77ul)
        return fail(6, "pixmap treated as selectable");

    if (mgl_proc_address("glXSelectEvent") == NULL ||
        mgl_proc_address("glXGetSelectedEvent") == NULL)
        return fail(7, "proc addresses");

    mgl_glx_drawable_destroy(dpy, win);
    mgl_glx_drawable_destroy(dpy, pbuffer);
    mgl_glx_drawable_destroy(dpy, pixmap);
    puts("GLX event selection semantics passed");
    return 0;
}

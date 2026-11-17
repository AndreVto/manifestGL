#include "mgl/glx.h"
#include "abi/proc_address.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>

static int fail(int n, const char *message) {
    fprintf(stderr, "GLX legacy pixmap failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    int screen;
    Pixmap xpix;
    GLXPixmap glxpix;
    XVisualInfo templ;
    XVisualInfo *vis;
    int count = 0;
    unsigned int value = 0u;
    GLXFBConfig *configs = NULL;
    int config_count = 0;
    int ivalue = 0;

    if (dpy == NULL)
        return fail(1, "XOpenDisplay");
    screen = DefaultScreen(dpy);
    xpix =
        XCreatePixmap(dpy, RootWindow(dpy, screen), 23u, 17u, (unsigned)DefaultDepth(dpy, screen));
    if (xpix == 0) {
        XCloseDisplay(dpy);
        return fail(2, "XCreatePixmap");
    }

    templ.screen = screen;
    templ.visualid = XVisualIDFromVisual(DefaultVisual(dpy, screen));
    vis = XGetVisualInfo(dpy, VisualScreenMask | VisualIDMask, &templ, &count);
    if (vis == NULL || count < 1) {
        XFreePixmap(dpy, xpix);
        XCloseDisplay(dpy);
        return fail(3, "XGetVisualInfo");
    }

    glxpix = glXCreateGLXPixmap(dpy, vis, xpix);
    if (glxpix == 0)
        return fail(4, "glXCreateGLXPixmap");
    glXQueryDrawable(dpy, glxpix, GLX_WIDTH, &value);
    if (value != 23u)
        return fail(5, "width");
    glXQueryDrawable(dpy, glxpix, GLX_HEIGHT, &value);
    if (value != 17u)
        return fail(6, "height");

    if (glXCreateGLXPixmap(dpy, NULL, xpix) != 0)
        return fail(7, "null visual");
    if (glXCreateGLXPixmap(NULL, vis, xpix) != 0)
        return fail(8, "null display");
    if (glXCreateGLXPixmap(dpy, vis, 0) != 0)
        return fail(9, "zero pixmap");
    if (mgl_proc_address("glXCreateGLXPixmap") == NULL ||
        mgl_proc_address("glXDestroyGLXPixmap") == NULL)
        return fail(10, "proc addresses");

    configs = glXGetFBConfigs(dpy, screen, &config_count);
    if (configs == NULL || config_count <= 0)
        return fail(11, "GetFBConfigs visual query");
    if (glXGetFBConfigAttrib(dpy, configs[0], GLX_VISUAL_ID, &ivalue) != 0 ||
        ivalue != (int)XVisualIDFromVisual(DefaultVisual(dpy, screen)))
        return fail(12, "FBConfig visual id");
    if (glXGetFBConfigAttrib(dpy, configs[0], GLX_X_VISUAL_TYPE, &ivalue) != 0)
        return fail(13, "FBConfig visual type query");
    switch (DefaultVisual(dpy, screen)->class) {
        case TrueColor:
            if (ivalue != GLX_TRUE_COLOR)
                return fail(14, "TrueColor mapping");
            break;
        case DirectColor:
            if (ivalue != GLX_DIRECT_COLOR)
                return fail(15, "DirectColor mapping");
            break;
        case PseudoColor:
            if (ivalue != GLX_PSEUDO_COLOR)
                return fail(16, "PseudoColor mapping");
            break;
        case StaticColor:
            if (ivalue != GLX_STATIC_COLOR)
                return fail(17, "StaticColor mapping");
            break;
        case GrayScale:
            if (ivalue != GLX_GRAY_SCALE)
                return fail(18, "GrayScale mapping");
            break;
        case StaticGray:
            if (ivalue != GLX_STATIC_GRAY)
                return fail(19, "StaticGray mapping");
            break;
        default:
            return fail(20, "unknown X visual class");
    }
    free(configs);

    config_count = -1;
    configs = glXChooseFBConfig(dpy, ScreenCount(dpy), NULL, &config_count);
    if (configs != NULL || config_count != 0)
        return fail(21, "invalid screen chooser");

    glXDestroyGLXPixmap(dpy, glxpix);
    XFree(vis);
    XFreePixmap(dpy, xpix);
    XCloseDisplay(dpy);
    puts("GLX legacy pixmap semantics passed");
    return 0;
}

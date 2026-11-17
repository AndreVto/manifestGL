#include "mgl/glx.h"
#include "mgl/gl.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>

static int fail(int code, const char *message) {
    fprintf(stderr, "GLX init probe failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    Display *display = XOpenDisplay(NULL);
    XVisualInfo *visual;
    GLXContext context;
    GLXWindow glx_window;
    GLXFBConfig *configs = NULL;
    int config_count = 0;
    XSetWindowAttributes attributes = {0};
    Window root, window;
    int attribs[] = {GLX_RGBA, GLX_DOUBLEBUFFER, None};

    if (display == NULL)
        return fail(1, "XOpenDisplay");
    visual = glXChooseVisual(display, DefaultScreen(display), attribs);
    if (visual == NULL)
        return fail(2, "glXChooseVisual");
    context = glXCreateContext(display, visual, NULL, True);
    if (context == NULL)
        return fail(3, "glXCreateContext");

    root = RootWindow(display, visual->screen);
    attributes.override_redirect = True;
    attributes.border_pixel = 0;
    attributes.colormap = XCreateColormap(display, root, visual->visual, AllocNone);
    window =
        XCreateWindow(display, root, -1, -1, 1, 1, 0, visual->depth, InputOutput, visual->visual,
                      CWOverrideRedirect | CWBorderPixel | CWColormap, &attributes);
    if (window == 0)
        return fail(4, "XCreateWindow");
    XMapWindow(display, window);

    if (!glXMakeCurrent(display, window, context))
        return fail(5, "glXMakeCurrent on raw X11 window");
    if (glXGetCurrentContext() != context || glXGetCurrentDrawable() != window ||
        glGetString(GL_VERSION) == NULL)
        return fail(6, "GL context current and queryable");

    configs = glXGetFBConfigs(display, visual->screen, &config_count);
    if (configs == NULL || config_count < 1)
        return fail(7, "glXGetFBConfigs");
    glx_window = glXCreateWindow(display, configs[0], window, NULL);
    free(configs);
    if (glx_window == 0)
        return fail(7, "glXCreateWindow");
    if (glXMakeCurrent(display, glx_window, context))
        return fail(8, "managed GLXWindow bound without presentation registration");
    if (glXGetCurrentDrawable() != window)
        return fail(9, "failed bind discarded previous context");
    if (!glXMakeCurrent(display, None, NULL))
        return fail(10, "glXMakeCurrent release");

    glXDestroyWindow(display, glx_window);
    glXDestroyContext(display, context);
    XDestroyWindow(display, window);
    XFreeColormap(display, attributes.colormap);
    XFree(visual);
    XCloseDisplay(display);
    puts("GLX raw-window initialization probe passed");
    return 0;
}

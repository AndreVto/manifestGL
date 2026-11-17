#include "mgl/glx.h"
#include "conformance/shader_object_alias_cases.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    Display *display = XOpenDisplay(NULL);
    XVisualInfo *visual;
    GLXContext ctx;
    Window window;
    XSetWindowAttributes attr;
    int visual_attrs[] = {GLX_RGBA, None};
    if (display == NULL)
        return 2;
    visual = glXChooseVisual(display, DefaultScreen(display), visual_attrs);
    if (visual == NULL)
        return 3;
    attr.colormap = XCreateColormap(display, RootWindow(display, DefaultScreen(display)),
                                    visual->visual, AllocNone);
    attr.border_pixel = 0;
    window = XCreateWindow(display, RootWindow(display, DefaultScreen(display)), 0, 0, 64, 64, 0,
                           visual->depth, InputOutput, visual->visual, CWColormap | CWBorderPixel,
                           &attr);
    ctx = glXCreateContext(display, visual, NULL, True);
    if (ctx == NULL || !glXMakeCurrent(display, window, ctx))
        return 4;
    if (strstr((const char *)glGetString(GL_VERSION), "ManifestGL") != NULL)
        return 5;
    shader_object_alias_trace();
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, ctx);
    XDestroyWindow(display, window);
    XFreeColormap(display, attr.colormap);
    XFree(visual);
    XCloseDisplay(display);
    return 0;
}

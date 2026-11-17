#include "mgl/gl.h"
#include "mgl/glx.h"
#include "buffer_storage_cases.h"
#include <string.h>

int main(void) {
    Display *display = XOpenDisplay(NULL);
    XVisualInfo *visual;
    GLXContext context;
    Colormap colormap;
    XSetWindowAttributes attributes;
    Window window;
    int attrs[] = {GLX_RGBA, None};
    if (!display)
        return 2;
    visual = glXChooseVisual(display, DefaultScreen(display), attrs);
    if (!visual)
        return 3;
    colormap =
        XCreateColormap(display, RootWindow(display, visual->screen), visual->visual, AllocNone);
    attributes.colormap = colormap;
    attributes.border_pixel = 0;
    window =
        XCreateWindow(display, RootWindow(display, visual->screen), 0, 0, 64, 64, 0, visual->depth,
                      InputOutput, visual->visual, CWColormap | CWBorderPixel, &attributes);
    context = glXCreateContext(display, visual, NULL, True);
    if (!context || !glXMakeCurrent(display, window, context))
        return 4;
    if (strstr((const char *)glGetString(GL_VERSION), "ManifestGL"))
        return 5;
    buffer_storage_trace();
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, context);
    XDestroyWindow(display, window);
    XFreeColormap(display, colormap);
    XFree(visual);
    XCloseDisplay(display);
    return 0;
}

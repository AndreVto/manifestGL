#include "mgl/glx.h"
#include "mgl/gl.h"
#include "clear_texture_cases.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    Display *display = XOpenDisplay(NULL);
    XVisualInfo *visual;
    GLXContext context;
    Colormap colormap;
    XSetWindowAttributes attributes;
    Window window;
    int screen;
    int visual_attributes[] = {GLX_RGBA, None};
    const GLubyte *vendor;

    if (!display)
        return 2;
    screen = DefaultScreen(display);
    visual = glXChooseVisual(display, screen, visual_attributes);
    if (!visual)
        return 3;
    colormap = XCreateColormap(display, RootWindow(display, screen), visual->visual, AllocNone);
    attributes.colormap = colormap;
    attributes.border_pixel = 0;
    window = XCreateWindow(display, RootWindow(display, screen), 0, 0, 64, 64, 0, visual->depth,
                           InputOutput, visual->visual, CWColormap | CWBorderPixel, &attributes);
    context = glXCreateContext(display, visual, NULL, True);
    if (!context || !glXMakeCurrent(display, window, context))
        return 4;
    vendor = glGetString(GL_VENDOR);
    if (!vendor || strstr((const char *)glGetString(GL_VERSION), "ManifestGL"))
        return 5;
    fprintf(stderr, "Mesa reference: %s\n", vendor);

    clear_texture_trace();

    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, context);
    XDestroyWindow(display, window);
    XFreeColormap(display, colormap);
    XFree(visual);
    XCloseDisplay(display);
    return 0;
}

#include "mgl/gl.h"
#include "mgl/glx.h"
#include "buffer_clear_transfer_cases.h"
#include <string.h>

int main(void) {
    Display *d = XOpenDisplay(NULL);
    XVisualInfo *v;
    Colormap colormap;
    XSetWindowAttributes a;
    Window w;
    GLXContext c;
    int attrs[] = {GLX_RGBA, None};
    if (!d)
        return 2;
    v = glXChooseVisual(d, DefaultScreen(d), attrs);
    if (!v)
        return 3;
    colormap = XCreateColormap(d, RootWindow(d, v->screen), v->visual, AllocNone);
    a.colormap = colormap;
    a.border_pixel = 0;
    w = XCreateWindow(d, RootWindow(d, v->screen), 0, 0, 64, 64, 0, v->depth, InputOutput,
                      v->visual, CWColormap | CWBorderPixel, &a);
    c = glXCreateContext(d, v, NULL, True);
    if (!c || !glXMakeCurrent(d, w, c))
        return 4;
    if (strstr((const char *)glGetString(GL_VERSION), "ManifestGL"))
        return 5;
    buffer_clear_transfer_trace();
    glXMakeCurrent(d, None, NULL);
    glXDestroyContext(d, c);
    XDestroyWindow(d, w);
    XFreeColormap(d, colormap);
    XFree(v);
    XCloseDisplay(d);
    return 0;
}

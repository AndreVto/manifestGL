#include "mgl/glx.h"
#include "mgl/gl.h"
#include "glx/drawable.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

static int received_error;
static int received_minor;

struct concurrent_bind {
    Display *dpy;
    GLXContext context;
    Window window;
    int result;
};

static void *concurrent_bind_attempt(void *opaque) {
    struct concurrent_bind *probe = opaque;
    probe->result = glXMakeCurrent(probe->dpy, probe->window, probe->context);
    return NULL;
}

static int error_handler(Display *dpy, XErrorEvent *event) {
    (void)dpy;
    received_error = event->error_code;
    received_minor = event->minor_code;
    return 0;
}

static int fail(int code, const char *description) {
    fprintf(stderr, "GLX drawable consistency failure %d: %s\n", code, description);
    return code;
}

int main(void) {
    Display *dpy;
    pthread_t thread;
    struct concurrent_bind probe;
    if (!XInitThreads())
        return fail(30, "XInitThreads");
    dpy = XOpenDisplay(NULL);
    GLXFBConfig *configs;
    GLXContext ctx;
    GLXWindow managed, different_format;
    GLXPbuffer different_screen, incompatible_pbuffer;
    GLXPixmap glxp;
    XVisualInfo *visual;
    XSetWindowAttributes attrs = {0};
    Window win, second_win;
    Pixmap pixmap;
    XImage *image;
    int count, opcode, event_base, error_base;
    int (*previous)(Display *, XErrorEvent *);
    int screen;
    if (dpy == NULL)
        return fail(1, "XOpenDisplay");
    screen = DefaultScreen(dpy);
    if (!XQueryExtension(dpy, "GLX", &opcode, &event_base, &error_base))
        return fail(2, "GLX extension");
    configs = glXGetFBConfigs(dpy, screen, &count);
    if (configs == NULL || count <= 0)
        return fail(3, "FBConfig enumeration");
    visual = glXGetVisualFromFBConfig(dpy, configs[0]);
    if (visual == NULL)
        return fail(4, "FBConfig visual");
    attrs.colormap = XCreateColormap(dpy, RootWindow(dpy, screen), visual->visual, AllocNone);
    attrs.background_pixel = WhitePixel(dpy, screen);
    win = XCreateWindow(dpy, RootWindow(dpy, screen), 0, 0, 9, 7, 0, visual->depth, InputOutput,
                        visual->visual, CWColormap | CWBackPixel, &attrs);
    if (win == 0)
        return fail(5, "window creation");
    second_win = XCreateWindow(dpy, RootWindow(dpy, screen), 0, 0, 9, 7, 0, visual->depth,
                               InputOutput, visual->visual, CWColormap | CWBackPixel, &attrs);
    if (second_win == 0)
        return fail(5, "second window creation");
    XMapWindow(dpy, win);
    XSync(dpy, False);
    ctx = glXCreateNewContext(dpy, configs[0], GLX_RGBA_TYPE, NULL, True);
    if (ctx == NULL)
        return fail(6, "context creation");
    previous = XSetErrorHandler(error_handler);

    pixmap = XCreatePixmap(dpy, win, 9, 7, (unsigned)visual->depth);
    XSync(dpy, False);
    if (mgl_glx_drawable_info(dpy, pixmap, NULL, NULL, NULL, NULL, NULL))
        return fail(7, "bare X Pixmap recognized as GLX Window");
    if (glXMakeCurrent(dpy, pixmap, ctx))
        return fail(8, "bound bare X Pixmap");
    if (received_error != error_base + 2 || received_minor != 5)
        return fail(9, "GLXBadDrawable on raw Pixmap");
    received_error = 0;

    if (!glXMakeCurrent(dpy, win, ctx))
        return fail(10, "raw window binding");
    probe.dpy = dpy;
    probe.context = ctx;
    probe.window = win;
    probe.result = True;
    if (pthread_create(&thread, NULL, concurrent_bind_attempt, &probe) != 0 ||
        pthread_join(thread, NULL) != 0)
        return fail(31, "concurrent bind thread");
    if (probe.result || received_error != BadAccess || received_minor != 5 ||
        glXGetCurrentContext() != ctx)
        return fail(32, "current context accepted on another thread");
    received_error = 0;
    glPixelStorei(GL_PACK_ALIGNMENT, 8);
    glPixelStorei(GL_PACK_ROW_LENGTH, 17);
    glPixelStorei(GL_PACK_SKIP_ROWS, 2);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 3);
    glXSwapBuffers(dpy, win);
    {
        GLint value;
        glGetIntegerv(GL_PACK_ALIGNMENT, &value);
        if (value != 8)
            return fail(33, "pack alignment preserved");
        glGetIntegerv(GL_PACK_ROW_LENGTH, &value);
        if (value != 17)
            return fail(34, "pack row length preserved");
        glGetIntegerv(GL_PACK_SKIP_ROWS, &value);
        if (value != 2)
            return fail(35, "pack skip rows preserved");
        glGetIntegerv(GL_PACK_SKIP_PIXELS, &value);
        if (value != 3)
            return fail(36, "pack skip pixels preserved");
    }
    XSync(dpy, False);
    image = XGetImage(dpy, win, 0, 0, 1, 1, AllPlanes, ZPixmap);
    if (image == NULL)
        return fail(11, "raw window swap image");
    if (XGetPixel(image, 0, 0) != BlackPixel(dpy, screen))
        return fail(12, "raw window readback presentation");
    XDestroyImage(image);

    if (glXCreateWindow(dpy, NULL, win, NULL) != 0 || received_error != error_base + 9)
        return fail(13, "invalid FBConfig creation");
    received_error = 0;
    if (glXCreateWindow(dpy, configs[4], win, NULL) != 0 || received_error != BadMatch)
        return fail(14, "pbuffer-only config for window");
    received_error = 0;
    managed = glXCreateWindow(dpy, configs[0], win, NULL);
    different_format = glXCreateWindow(dpy, configs[1], second_win, NULL);
    if (managed == 0 || different_format == 0)
        return fail(15, "valid managed drawable creation");
    received_error = 0;
    if (glXCreateWindow(dpy, configs[0], win, NULL) != 0 || received_error != BadAlloc)
        return fail(29, "duplicate GLXWindow association");
    received_error = 0;
    if (glXMakeCurrent(dpy, different_format, ctx))
        return fail(16, "incompatible framebuffer binding");
    if (received_error != BadMatch || glXGetCurrentDrawable() != win)
        return fail(17, "BadMatch and preservation of prior binding");
    received_error = 0;
    if (glXMakeCurrent(dpy, managed, ctx))
        return fail(18, "unregistered managed window was bound");
    if (glXGetCurrentDrawable() != win)
        return fail(19, "failed managed binding changed current");

    incompatible_pbuffer = glXCreatePbuffer(dpy, configs[4], NULL);
    if (incompatible_pbuffer == 0)
        return fail(20, "pbuffer-only FBConfig");
    if (glXMakeCurrent(dpy, incompatible_pbuffer, ctx))
        return fail(21, "incompatible pbuffer was bound");
    if (received_error != BadMatch)
        return fail(22, "pbuffer BadMatch");
    received_error = 0;
    different_screen = (GLXPbuffer)mgl_glx_drawable_create(dpy, MGL_GLX_DRAWABLE_PBUFFER, 0, 3, 3);
    if (!mgl_glx_drawable_set_format(dpy, different_screen, screen + 1, 1, 0))
        return fail(23, "screen metadata");
    if (glXMakeCurrent(dpy, different_screen, ctx))
        return fail(24, "other screen pbuffer binding");
    if (received_error != BadMatch)
        return fail(25, "other screen BadMatch");
    received_error = 0;

    glxp = glXCreateGLXPixmap(dpy, visual, pixmap);
    if (glxp == 0)
        return fail(26, "legacy GLXPixmap creation");
    if (!glXMakeCurrent(dpy, glxp, ctx))
        return fail(27, "valid GLXPixmap bind");
    if (!glXMakeCurrent(dpy, 0, NULL))
        return fail(28, "unbind");
    glXDestroyGLXPixmap(dpy, glxp);
    mgl_glx_drawable_destroy(dpy, different_screen);
    glXDestroyPbuffer(dpy, incompatible_pbuffer);
    glXDestroyWindow(dpy, different_format);
    glXDestroyWindow(dpy, managed);
    glXDestroyContext(dpy, ctx);
    XFreePixmap(dpy, pixmap);
    XDestroyWindow(dpy, win);
    XDestroyWindow(dpy, second_win);
    XFreeColormap(dpy, attrs.colormap);
    XFree(visual);
    free(configs);
    XSetErrorHandler(previous);
    XCloseDisplay(dpy);
    puts("GLX drawable classification, compatibility and raw-window presentation passed");
    return 0;
}

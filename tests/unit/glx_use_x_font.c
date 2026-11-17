#define _POSIX_C_SOURCE 200809L
#include "mgl/gl.h"
#include "mgl/glx.h"
#include "abi/proc_address.h"

#include <X11/Xlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

static int fail(int n, const char *message) {
    fprintf(stderr, "GLX UseXFont failure %d: %s\n", n, message);
    return n;
}

static int close_enough(GLfloat actual, GLfloat expected) {
    GLfloat delta = actual - expected;
    return delta > -0.001f && delta < 0.001f;
}

int main(void) {
    const char *native_path = getenv("MGL_EGL_LIBRARY");
    void *native_lib;
    unsigned (*flush_count)(void);
    Display *dpy;
    XFontStruct *font;
    GLXContext ctx;
    GLXPbuffer pbuffer;
    GLfloat before_pos[4], after_pos[4];
    GLint unpack_alignment = 0;
    unsigned before_flush;
    int space_advance;
    const int pbattrs[] = {GLX_PBUFFER_WIDTH, 16, GLX_PBUFFER_HEIGHT, 16, 0};

    if (native_path == NULL || native_path[0] == '\0')
        return fail(1, "native path");
    native_lib = dlopen(native_path, RTLD_NOW | RTLD_LOCAL);
    if (native_lib == NULL)
        return fail(2, "native dlopen");
    *(void **)(&flush_count) = dlsym(native_lib, "mgl_test_native_egl_flush_count");
    if (flush_count == NULL)
        return fail(3, "flush counter symbol");

    dpy = XOpenDisplay(NULL);
    if (dpy == NULL)
        return fail(4, "XOpenDisplay");
    font = XLoadQueryFont(dpy, "fixed");
    if (font == NULL)
        return fail(5, "fixed X font");
    space_advance = XTextWidth(font, " ", 1);
    if (space_advance <= 0)
        return fail(6, "space advance metric");

    pbuffer = glXCreatePbuffer(dpy, NULL, pbattrs);
    ctx = glXCreateContext(dpy, NULL, NULL, True);
    if (pbuffer == 0 || ctx == NULL)
        return fail(7, "GLX object creation");
    if (!glXMakeCurrent(dpy, pbuffer, ctx))
        return fail(8, "glXMakeCurrent");

    glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
    before_flush = flush_count();
    glXUseXFont(font->fid, 32, 2, 100);
    if (flush_count() != before_flush + 1u)
        return fail(9, "implicit glFlush");
    if (!glIsList(100u) || !glIsList(101u))
        return fail(10, "font display lists not created");
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
    if (unpack_alignment != 8)
        return fail(11, "pixel unpack state changed");

    glRasterPos2f(0.0f, 0.0f);
    glGetFloatv(GL_CURRENT_RASTER_POSITION, before_pos);
    glCallList(100u);
    glGetFloatv(GL_CURRENT_RASTER_POSITION, after_pos);
    if (!close_enough(after_pos[0] - before_pos[0], (GLfloat)space_advance) ||
        !close_enough(after_pos[1] - before_pos[1], 0.0f))
        return fail(12, "space glyph raster advance");

    glRasterPos2f(0.0f, 0.0f);
    glGetFloatv(GL_CURRENT_RASTER_POSITION, before_pos);
    glXUseXFont(font->fid, 0x10000, 1, 200);
    if (!glIsList(200u))
        return fail(13, "undefined glyph empty list");
    glCallList(200u);
    glGetFloatv(GL_CURRENT_RASTER_POSITION, after_pos);
    if (!close_enough(after_pos[0] - before_pos[0], 0.0f) ||
        !close_enough(after_pos[1] - before_pos[1], 0.0f))
        return fail(14, "undefined glyph list was not empty");

    glNewList(300u, GL_COMPILE);
    before_flush = flush_count();
    glXUseXFont(font->fid, 32, 1, 400);
    if (flush_count() != before_flush)
        return fail(15, "list-construction call flushed");
    glEndList();
    if (!glIsList(300u) || glIsList(400u))
        return fail(16, "list-construction isolation");

    if (!glXMakeCurrent(dpy, 0, NULL))
        return fail(17, "unbind");
    before_flush = flush_count();
    glXUseXFont(font->fid, 32, 1, 500);
    if (flush_count() != before_flush)
        return fail(18, "no-context call flushed");
    if (!glXMakeCurrent(dpy, pbuffer, ctx))
        return fail(19, "rebind");
    if (glIsList(500u))
        return fail(20, "no-context call created a list");

    if (mgl_proc_address("glXUseXFont") == NULL)
        return fail(21, "proc address");

    if (!glXMakeCurrent(dpy, 0, NULL))
        return fail(22, "final unbind");
    glXDestroyContext(dpy, ctx);
    glXDestroyPbuffer(dpy, pbuffer);
    XFreeFont(dpy, font);
    XCloseDisplay(dpy);
    dlclose(native_lib);
    puts("GLX UseXFont display-list semantics passed");
    return 0;
}

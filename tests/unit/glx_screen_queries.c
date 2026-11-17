/* Validate public GLX screen-index behavior against
 * a live X display, distinct from the synthetic-display handle-lifetime tests. */
#include "mgl/glx.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(int code, const char *why) {
    fprintf(stderr, "GLX screen query failure %d: %s\n", code, why);
    return code;
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    const char *vendor, *extensions;
    GLXFBConfig *configs;
    XVisualInfo *visual;
    int first, invalid, count, attribute_value;
    if (dpy == NULL)
        return fail(1, "open X display");
    first = DefaultScreen(dpy);
    invalid = ScreenCount(dpy);

    vendor = glXQueryServerString(dpy, first, GLX_VENDOR);
    extensions = glXQueryExtensionsString(dpy, first);
    if (vendor == NULL || *vendor == '\0' || extensions == NULL ||
        strstr(extensions, "GLX_ARB_create_context") == NULL)
        return fail(2, "server strings on a valid screen");
    if (glXQueryServerString(dpy, first, -1) != NULL)
        return fail(3, "unknown server-string selector");
    if (glXQueryServerString(dpy, -1, GLX_VENDOR) != NULL ||
        glXQueryServerString(dpy, invalid, GLX_VENDOR) != NULL)
        return fail(4, "out-of-range server-string screen");
    if (glXQueryExtensionsString(dpy, -1) != NULL || glXQueryExtensionsString(dpy, invalid) != NULL)
        return fail(5, "out-of-range extension-string screen");
    if (glXQueryServerString(NULL, first, GLX_VENDOR) != NULL ||
        glXQueryExtensionsString(NULL, first) != NULL)
        return fail(6, "missing X display on server queries");

    count = -1;
    configs = glXGetFBConfigs(dpy, first, &count);
    if (configs == NULL || count <= 0)
        return fail(7, "valid FBConfig enumeration");
    free(configs);
    count = -1;
    configs = glXGetFBConfigs(dpy, -1, &count);
    if (configs != NULL || count != 0)
        return fail(8, "negative FBConfig screen");
    count = -1;
    configs = glXGetFBConfigs(dpy, invalid, &count);
    if (configs != NULL || count != 0)
        return fail(9, "high FBConfig screen");

    count = -1;
    configs = glXChooseFBConfig(dpy, -1, NULL, &count);
    if (configs != NULL || count != 0)
        return fail(10, "negative chooser screen");
    count = -1;
    configs = glXChooseFBConfig(dpy, invalid, NULL, &count);
    if (configs != NULL || count != 0)
        return fail(11, "high chooser screen");

    visual = glXChooseVisual(dpy, first, NULL);
    if (visual == NULL)
        return fail(12, "choose visual on valid screen");
    XFree(visual);
    if (glXChooseVisual(dpy, -1, NULL) != NULL || glXChooseVisual(dpy, invalid, NULL) != NULL)
        return fail(13, "out-of-range visual screen");

    /* The singleton FBConfig can be queried in a
     * synthetic no-display test mode. Its screen must still be validated
     * before any subsequent real-display Xlib visual lookup. */
    count = -1;
    configs = glXGetFBConfigs(NULL, invalid, &count);
    if (configs == NULL || count <= 0)
        return fail(14, "synthetic config for invalid screen");
    attribute_value = 987;
    if (glXGetFBConfigAttrib(dpy, configs[0], GLX_VISUAL_ID, &attribute_value) !=
            GLX_BAD_ATTRIBUTE ||
        attribute_value != 987)
        return fail(15, "invalid config screen must preserve output and avoid Xlib");
    if (glXGetVisualFromFBConfig(dpy, configs[0]) != NULL)
        return fail(16, "invalid config screen must not access Xlib visual");
    free(configs);

    XCloseDisplay(dpy);
    puts("GLX real-display screen validation passed");
    return 0;
}

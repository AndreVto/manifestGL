#include "mgl/glx.h"
#include "mgl/gl.h"
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>

static int seen_code, seen_major, seen_minor, seen_count;

static int on_x_error(Display *dpy, XErrorEvent *error) {
    (void)dpy;
    seen_code = error->error_code;
    seen_major = error->request_code;
    seen_minor = error->minor_code;
    ++seen_count;
    return 0;
}

static int check(int ok, const char *message) {
    if (!ok)
        fprintf(stderr, "GLX error delivery: %s\n", message);
    return ok;
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    int base = 0, event = 0, opcode = 0, expected_base = 0, expected_event = 0;
    int count = 0, error = 0, config_id = 0;
    int found_pbuffer_only = 0, found_multisample = 0;
    unsigned int queried = 0xdeadbeefu;
    GLXFBConfig *configs;
    const int invalid_profile[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                   3,
                                   GLX_CONTEXT_MINOR_VERSION_ARB,
                                   3,
                                   GLX_CONTEXT_PROFILE_MASK_ARB,
                                   3,
                                   0};
    const int invalid_attribute[] = {0x7ff0, 1, 0};
    if (!check(dpy != NULL, "X display"))
        return 1;
    if (!check(XQueryExtension(dpy, "GLX", &opcode, &expected_event, &expected_base),
               "server GLX extension"))
        return 2;
    if (!check(glXQueryExtension(dpy, &base, &event) && base == expected_base &&
                   event == expected_event,
               "GLX extension error and event bases"))
        return 3;
    configs = glXGetFBConfigs(dpy, DefaultScreen(dpy), &count);
    if (!check(configs != NULL && count > 0, "configuration enumeration"))
        return 4;
    for (int i = 0; i < count; ++i) {
        if (glXGetFBConfigAttrib(dpy, configs[i], GLX_DRAWABLE_TYPE, &error) == Success &&
            error == GLX_PBUFFER_BIT)
            found_pbuffer_only = 1;
        if (glXGetFBConfigAttrib(dpy, configs[i], GLX_SAMPLES, &error) == Success && error > 0)
            found_multisample = 1;
    }
    if (!check(found_pbuffer_only, "pbuffer-only configuration"))
        return 5;
    if (!check(found_multisample, "multisample configuration"))
        return 6;
    XSetErrorHandler(on_x_error);
    if (!check(glXCreateContextAttribsARB(dpy, configs[0], NULL, True, invalid_profile) == NULL,
               "invalid profile rejection"))
        return 7;
    if (!check(seen_count == 1 && seen_code == base + 13 && seen_major == opcode &&
                   seen_minor == 34,
               "profile error code"))
        return 8;
    if (!check(glXCreateContextAttribsARB(dpy, configs[0], NULL, True, invalid_attribute) == NULL,
               "unknown attribute rejection"))
        return 9;
    if (!check(seen_count == 2 && seen_code == BadValue && seen_major == opcode && seen_minor == 34,
               "attribute error code"))
        return 10;
    glXCopyContext(dpy, NULL, NULL, GL_COLOR_BUFFER_BIT);
    if (!check(seen_count == 3 && seen_code == base && seen_major == opcode && seen_minor == 10,
               "context-copy error code"))
        return 11;
    glXQueryDrawable(dpy, (GLXDrawable)0x7fffdeadul, GLX_WIDTH, &queried);
    if (!check(seen_count == 4 && seen_code == base + 2 && seen_minor == 29 &&
                   queried == 0xdeadbeefu,
               "invalid drawable query error"))
        return 12;
    {
        GLXPbuffer pbuffer = glXCreatePbuffer(dpy, configs[0], NULL);
        if (!check(pbuffer != 0, "query test pbuffer"))
            return 13;
        if (!check(glXGetFBConfigAttrib(dpy, configs[0], GLX_FBCONFIG_ID, &config_id) ==
                       Success,
                   "FBConfig ID attribute"))
            return 14;
        glXQueryDrawable(dpy, pbuffer, GLX_FBCONFIG_ID, &queried);
        if (!check(queried == (unsigned int)config_id, "FBConfig ID query"))
            return 14;
        queried = 0xdeadbeefu;
        glXQueryDrawable(dpy, pbuffer, 0x7fff, &queried);
        if (!check(seen_count == 5 && seen_code == BadValue && seen_minor == 29 &&
                       queried == 0xdeadbeefu,
                   "invalid attribute query error"))
            return 15;
        glXSelectEvent(dpy, pbuffer, ~0ul);
        if (!check(seen_count == 6 && seen_code == BadValue && seen_minor == 30,
                   "invalid event mask"))
            return 18;
        glXDestroyPbuffer(dpy, pbuffer);
    }
    {
        unsigned long selected = 0xdeadbeeful;
        glXGetSelectedEvent(dpy, (GLXDrawable)0x7fffdeadul, &selected);
        if (!check(seen_count == 7 && seen_code == base + 2 && seen_minor == 29 &&
                       selected == 0xdeadbeeful,
                   "invalid selected-event query"))
            return 16;
        glXSelectEvent(dpy, (GLXDrawable)0x7fffdeadul, GLX_PBUFFER_CLOBBER_MASK);
        if (!check(seen_count == 8 && seen_code == base + 2 && seen_minor == 30,
                   "invalid event-selection drawable"))
            return 17;
    }
    free(configs);
    XCloseDisplay(dpy);
    return 0;
}

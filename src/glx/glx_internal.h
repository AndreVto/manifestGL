#ifndef MGL_GLX_INTERNAL_H
#define MGL_GLX_INTERNAL_H

#include "mgl/glx.h"
#include "core/context.h"

struct mgl_android_present;

struct mgl_glx_context {
    struct mgl_context *core;
    Display *display;
    int screen;
    int fbconfig_id;
    int render_type;
    GLXDrawable draw;
    GLXDrawable read;
    struct mgl_android_present *android_present;
    unsigned current_count;
    int destroy_pending;

    struct mgl_glx_context *registry_next;
};

struct mgl_glx_fbconfig {
    int screen;
    int id;
    int drawable_type;
    int x_renderable;
    int red, green, blue, alpha, depth, stencil;
    int double_buffer;
    int sample_buffers, samples;
};

#endif

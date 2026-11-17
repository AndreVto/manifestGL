#ifndef MGL_GLX_DRAWABLE_H
#define MGL_GLX_DRAWABLE_H

#include "mgl/glx.h"

#include <pthread.h>

enum mgl_glx_drawable_kind {
    MGL_GLX_DRAWABLE_WINDOW,
    MGL_GLX_DRAWABLE_PBUFFER,
    MGL_GLX_DRAWABLE_PIXMAP
};

struct mgl_glx_drawable_record {
    GLXDrawable id;
    Display *display;
    enum mgl_glx_drawable_kind kind;
    Drawable native_drawable;
    int screen;
    int fbconfig_id;
    VisualID visual_id;
    unsigned width;
    unsigned height;
    int preserved_contents;
    int largest_pbuffer;
    int swap_interval;
    int swap_interval_explicit;
    unsigned long event_mask;
    Display *watch_display;
    int watch_destroyed;
    struct mgl_glx_drawable_record *next;
};

GLXDrawable mgl_glx_drawable_create(Display *dpy, enum mgl_glx_drawable_kind kind,
                                    Drawable native_drawable, unsigned width, unsigned height);
void mgl_glx_drawable_destroy(Display *dpy, GLXDrawable drawable);
int mgl_glx_window_has_config(Display *dpy, Window window);
int mgl_glx_native_window_info(Display *dpy, Window window, int *screen, VisualID *visual_id,
                               unsigned *width, unsigned *height);
int mgl_glx_native_pixmap_info(Display *dpy, Pixmap pixmap, int *screen, unsigned *width,
                               unsigned *height, unsigned *depth);
int mgl_glx_drawable_set_format(Display *dpy, GLXDrawable drawable, int screen, int fbconfig_id,
                                VisualID visual_id);
int mgl_glx_drawable_get_format(Display *dpy, GLXDrawable drawable, int *screen, int *fbconfig_id,
                                VisualID *visual_id);

int mgl_glx_drawable_refresh(Display *dpy, GLXDrawable drawable);
int mgl_glx_drawable_watch(Display *dpy, GLXDrawable drawable);
int mgl_glx_drawable_watch_poll(Display *dpy, GLXDrawable drawable);
int mgl_glx_drawable_info(Display *dpy, GLXDrawable drawable, enum mgl_glx_drawable_kind *kind,
                          Drawable *native_drawable, unsigned *width, unsigned *height,
                          int *swap_interval);
int mgl_glx_drawable_set_pbuffer_attributes(Display *dpy, GLXDrawable drawable,
                                            int preserved_contents, int largest_pbuffer);
int mgl_glx_drawable_get_pbuffer_attributes(Display *dpy, GLXDrawable drawable,
                                            int *preserved_contents, int *largest_pbuffer);
int mgl_glx_drawable_set_swap_interval(Display *dpy, GLXDrawable drawable, int interval);
int mgl_glx_drawable_set_initial_swap_interval(Display *dpy, GLXDrawable drawable, int interval);
int mgl_glx_drawable_set_event_mask(Display *dpy, GLXDrawable drawable, unsigned long event_mask);
int mgl_glx_drawable_get_event_mask(Display *dpy, GLXDrawable drawable, unsigned long *event_mask);
int mgl_glx_drawable_native_identity(Display *dpy, GLXDrawable drawable,
                                     enum mgl_glx_drawable_kind *kind, Drawable *native_drawable);

#endif

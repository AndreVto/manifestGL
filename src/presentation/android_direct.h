#ifndef MGL_PRESENTATION_ANDROID_DIRECT_H
#define MGL_PRESENTATION_ANDROID_DIRECT_H

#include "native/egl_abi.h"
#include "mgl/glx.h"

#include <stdint.h>

struct mgl_context;
struct mgl_android_present;

int mgl_android_direct_enabled(void);
struct mgl_android_present *mgl_android_present_create(Display *dpy, Drawable native_drawable,
                                                       uint64_t hwnd, uint32_t whole_window);
int mgl_android_present_register(Display *dpy, GLXDrawable drawable, Drawable native_drawable,
                                 uint64_t hwnd, uint32_t whole_window);
void mgl_android_present_unregister(Display *dpy, GLXDrawable drawable);
struct mgl_android_present *mgl_android_present_ref(Display *dpy, GLXDrawable drawable);
void mgl_android_present_unref(struct mgl_android_present *present);

/* prepare_surface may replace the underlying EGLSurface. The caller must not
 * have present->surface current while a replacement is required. */
MGL_EGLSurface mgl_android_present_prepare_surface(struct mgl_android_present *present,
                                                   struct mgl_context *ctx, int width, int height);
int mgl_android_present_configure(struct mgl_android_present *present, struct mgl_context *ctx,
                                  int width, int height);
int mgl_android_present_swap(struct mgl_android_present *present, struct mgl_context *ctx,
                             int width, int height);
int mgl_android_present_set_swap_interval(struct mgl_android_present *present,
                                          struct mgl_context *ctx, int requested);
int mgl_android_present_changed(struct mgl_android_present *present);
int mgl_android_present_is_window(struct mgl_android_present *present);
void mgl_android_present_invalidate(struct mgl_android_present *present);
void mgl_android_presented(struct mgl_android_present *present);
int mgl_android_effective_swap_interval(int requested);

#endif

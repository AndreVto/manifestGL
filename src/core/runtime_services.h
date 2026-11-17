#ifndef MGL_RUNTIME_SERVICES_H
#define MGL_RUNTIME_SERVICES_H

#include <X11/Xlib.h>
#include <stdint.h>

#include "native/egl_abi.h"

struct mgl_native_dispatch;
struct mgl_context;
struct mgl_android_present;

struct mgl_runtime_services {
    struct mgl_native_dispatch *(*runtime_native_dispatch)(void);
    struct mgl_context *(*context_create_offscreen)(struct mgl_native_dispatch *,
                                                    struct mgl_context *, int, int);
    void (*context_destroy)(struct mgl_context *);
    int (*context_make_current)(struct mgl_context *);
    int (*context_configure_pbuffers)(struct mgl_context *, int, int, int, int);
    void (*context_set_no_error)(struct mgl_context *, int);
    void (*context_set_creation_flags)(struct mgl_context *, unsigned int);
    struct mgl_context *(*context_current)(void);
    int (*context_complete_fence)(struct mgl_context *);
    void *(*proc_address)(const char *);
    struct mgl_android_present *(*android_present_create)(Display *, Drawable, uint64_t, uint32_t);
    void (*android_present_unref)(struct mgl_android_present *);
    int (*android_present_configure)(struct mgl_android_present *, struct mgl_context *, int, int);
    int (*android_present_swap)(struct mgl_android_present *, struct mgl_context *, int, int);
    int (*android_present_set_swap_interval)(struct mgl_android_present *, struct mgl_context *,
                                             int);
    int (*android_effective_swap_interval)(int);
    void (*context_set_profile_mask)(struct mgl_context *, unsigned int);
};

const struct mgl_runtime_services *mgl_runtime_services_get(void);

#endif

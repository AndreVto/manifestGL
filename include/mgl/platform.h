#ifndef MGL_PLATFORM_H
#define MGL_PLATFORM_H

#include <stdint.h>
#include "mgl/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Registers an X11 drawable; native Android windows remain owned by the host bridge. */
MGL_EXPORT int wine_android_register_glx_window(void *display, unsigned long drawable,
                                                uint64_t hwnd, uint32_t whole_window);

/* Registers a public mGL EGL X11 window surface; native window handles stay private. */
MGL_EXPORT int wine_android_register_egl_window(void *display, void *surface, uint64_t hwnd,
                                                uint32_t whole_window);

#ifdef __cplusplus
}
#endif

#endif

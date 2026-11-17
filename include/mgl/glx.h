#ifndef MGL_GLX_H
#define MGL_GLX_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "mgl/export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef XID GLXDrawable;
typedef XID GLXWindow;
typedef XID GLXPbuffer;
typedef XID GLXPixmap;

typedef struct mgl_glx_context *GLXContext;
typedef struct mgl_glx_fbconfig *GLXFBConfig;
typedef void (*__GLXextFuncPtr)(void);

#define GLX_VENDOR 0x1
#define GLX_VERSION 0x2
#define GLX_EXTENSIONS 0x3
#define GLX_BAD_SCREEN 1
#define GLX_BAD_ATTRIBUTE 2
#define GLX_NO_EXTENSION 3
#define GLX_BAD_VISUAL 4
#define GLX_BAD_VALUE 6
#define GLX_USE_GL 1
#define GLX_BAD_CONTEXT 5

#define GLX_RGBA 4
#define GLX_DOUBLEBUFFER 5
#define GLX_BUFFER_SIZE 2
#define GLX_LEVEL 3
#define GLX_STEREO 6
#define GLX_AUX_BUFFERS 7
#define GLX_RED_SIZE 8
#define GLX_GREEN_SIZE 9
#define GLX_BLUE_SIZE 10
#define GLX_ALPHA_SIZE 11
#define GLX_DEPTH_SIZE 12
#define GLX_STENCIL_SIZE 13
#define GLX_ACCUM_RED_SIZE 14
#define GLX_ACCUM_GREEN_SIZE 15
#define GLX_ACCUM_BLUE_SIZE 16
#define GLX_ACCUM_ALPHA_SIZE 17

#define GLX_CONFIG_CAVEAT 0x20
#define GLX_X_VISUAL_TYPE 0x22
#define GLX_TRANSPARENT_TYPE 0x23
#define GLX_TRANSPARENT_INDEX_VALUE 0x24
#define GLX_TRANSPARENT_RED_VALUE 0x25
#define GLX_TRANSPARENT_GREEN_VALUE 0x26
#define GLX_TRANSPARENT_BLUE_VALUE 0x27
#define GLX_TRANSPARENT_ALPHA_VALUE 0x28
#define GLX_NONE 0x8000
#define GLX_SLOW_CONFIG 0x8001
#define GLX_TRUE_COLOR 0x8002
#define GLX_DIRECT_COLOR 0x8003
#define GLX_PSEUDO_COLOR 0x8004
#define GLX_STATIC_COLOR 0x8005
#define GLX_GRAY_SCALE 0x8006
#define GLX_STATIC_GRAY 0x8007
#define GLX_TRANSPARENT_RGB 0x8008
#define GLX_TRANSPARENT_INDEX 0x8009
#define GLX_VISUAL_ID 0x800B
#define GLX_NON_CONFORMANT_CONFIG 0x800D

#define GLX_WINDOW_BIT 0x00000001
#define GLX_PIXMAP_BIT 0x00000002
#define GLX_PBUFFER_BIT 0x00000004
#define GLX_RGBA_BIT 0x00000001
#define GLX_COLOR_INDEX_BIT 0x00000002
#define GLX_DONT_CARE 0xFFFFFFFFu
#define GLX_DRAWABLE_TYPE 0x8010
#define GLX_RENDER_TYPE 0x8011
#define GLX_X_RENDERABLE 0x8012
#define GLX_FBCONFIG_ID 0x8013
#define GLX_SCREEN 0x800C
#define GLX_RGBA_TYPE 0x8014
#define GLX_MAX_PBUFFER_WIDTH 0x8016
#define GLX_MAX_PBUFFER_HEIGHT 0x8017
#define GLX_MAX_PBUFFER_PIXELS 0x8018
#define GLX_SAMPLE_BUFFERS 100000
#define GLX_SAMPLES 100001
#define GLX_SAMPLE_BUFFERS_ARB GLX_SAMPLE_BUFFERS
#define GLX_SAMPLES_ARB GLX_SAMPLES
#define GLX_WIDTH 0x801D
#define GLX_HEIGHT 0x801E
#define GLX_PRESERVED_CONTENTS 0x801B
#define GLX_PBUFFER_HEIGHT 0x8040
#define GLX_PBUFFER_WIDTH 0x8041
#define GLX_LARGEST_PBUFFER 0x801C
#define GLX_SWAP_INTERVAL_EXT 0x20F1
#define GLX_PBUFFER_CLOBBER_MASK 0x08000000ul

#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#define GLX_CONTEXT_FLAGS_ARB 0x2094
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define GLX_CONTEXT_DEBUG_BIT_ARB 0x00000001
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002

MGL_EXPORT Bool glXQueryExtension(Display *dpy, int *error_base, int *event_base);
MGL_EXPORT Bool glXQueryVersion(Display *dpy, int *major, int *minor);
MGL_EXPORT const char *glXGetClientString(Display *dpy, int name);
MGL_EXPORT const char *glXQueryServerString(Display *dpy, int screen, int name);
MGL_EXPORT const char *glXQueryExtensionsString(Display *dpy, int screen);
MGL_EXPORT __GLXextFuncPtr glXGetProcAddress(const unsigned char *name);
MGL_EXPORT __GLXextFuncPtr glXGetProcAddressARB(const unsigned char *name);
MGL_EXPORT XVisualInfo *glXChooseVisual(Display *dpy, int screen, int *attrib_list);
MGL_EXPORT int glXGetConfig(Display *dpy, XVisualInfo *vis, int attrib, int *value);
MGL_EXPORT GLXContext glXCreateContext(Display *dpy, XVisualInfo *vis, GLXContext share,
                                       Bool direct);
MGL_EXPORT GLXContext glXCreateNewContext(Display *dpy, GLXFBConfig config, int render_type,
                                          GLXContext share, Bool direct);
MGL_EXPORT GLXContext glXCreateContextAttribsARB(Display *dpy, GLXFBConfig config, GLXContext share,
                                                 Bool direct, const int *attrib_list);
MGL_EXPORT GLXContext glXCreateContextAttribs(Display *dpy, GLXFBConfig config, GLXContext share,
                                              Bool direct, const int *attrib_list);
MGL_EXPORT void glXCopyContext(Display *dpy, GLXContext src, GLXContext dst, unsigned long mask);
MGL_EXPORT void glXDestroyContext(Display *dpy, GLXContext ctx);
MGL_EXPORT Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx);
MGL_EXPORT Bool glXMakeContextCurrent(Display *dpy, GLXDrawable draw, GLXDrawable read,
                                      GLXContext ctx);
MGL_EXPORT GLXContext glXGetCurrentContext(void);
MGL_EXPORT GLXDrawable glXGetCurrentDrawable(void);
MGL_EXPORT GLXDrawable glXGetCurrentReadDrawable(void);
MGL_EXPORT Display *glXGetCurrentDisplay(void);
MGL_EXPORT void glXSwapBuffers(Display *dpy, GLXDrawable drawable);
MGL_EXPORT void glXWaitGL(void);
MGL_EXPORT void glXWaitX(void);
MGL_EXPORT void glXUseXFont(Font font, int first, int count, int list_base);
MGL_EXPORT Bool glXIsDirect(Display *dpy, GLXContext ctx);
MGL_EXPORT GLXFBConfig *glXChooseFBConfig(Display *dpy, int screen, const int *attrib_list,
                                          int *nelements);
MGL_EXPORT GLXFBConfig *glXChooseFBConfigSGIX(Display *dpy, int screen, const int *attrib_list,
                                              int *nelements);
MGL_EXPORT GLXFBConfig *glXGetFBConfigs(Display *dpy, int screen, int *nelements);
MGL_EXPORT int glXGetFBConfigAttrib(Display *dpy, GLXFBConfig config, int attribute, int *value);
MGL_EXPORT int glXQueryContext(Display *dpy, GLXContext ctx, int attribute, int *value);
MGL_EXPORT XVisualInfo *glXGetVisualFromFBConfig(Display *dpy, GLXFBConfig config);

MGL_EXPORT GLXWindow glXCreateWindow(Display *dpy, GLXFBConfig config, Window win,
                                     const int *attrib_list);
MGL_EXPORT void glXDestroyWindow(Display *dpy, GLXWindow window);
MGL_EXPORT GLXPbuffer glXCreatePbuffer(Display *dpy, GLXFBConfig config, const int *attrib_list);
MGL_EXPORT void glXDestroyPbuffer(Display *dpy, GLXPbuffer pbuffer);
MGL_EXPORT GLXPixmap glXCreatePixmap(Display *dpy, GLXFBConfig config, Pixmap pixmap,
                                     const int *attrib_list);
MGL_EXPORT void glXDestroyPixmap(Display *dpy, GLXPixmap pixmap);
MGL_EXPORT GLXPixmap glXCreateGLXPixmap(Display *dpy, XVisualInfo *vis, Pixmap pixmap);
MGL_EXPORT void glXDestroyGLXPixmap(Display *dpy, GLXPixmap pixmap);
MGL_EXPORT void glXQueryDrawable(Display *dpy, GLXDrawable draw, int attribute,
                                 unsigned int *value);
MGL_EXPORT void glXSelectEvent(Display *dpy, GLXDrawable draw, unsigned long event_mask);
MGL_EXPORT void glXGetSelectedEvent(Display *dpy, GLXDrawable draw, unsigned long *event_mask);

MGL_EXPORT void glXSwapIntervalEXT(Display *dpy, GLXDrawable drawable, int interval);
MGL_EXPORT int glXSwapIntervalMESA(unsigned int interval);
MGL_EXPORT int glXGetSwapIntervalMESA(void);
MGL_EXPORT int glXSwapIntervalSGI(int interval);

#ifdef __cplusplus
}
#endif

#endif

#ifndef MGL_EGL_ABI_H
#define MGL_EGL_ABI_H

#include <stdint.h>

typedef void *MGL_EGLDisplay;
typedef void *MGL_EGLConfig;
typedef void *MGL_EGLContext;
typedef void *MGL_EGLSurface;
typedef void *MGL_EGLClientBuffer;
typedef void *MGL_EGLNativeDisplayType;
typedef void *MGL_EGLNativeWindowType;
typedef int32_t MGL_EGLint;
typedef uint32_t MGL_EGLBoolean;

typedef void (*mgl_egl_proc_t)(void);

#define MGL_EGL_FALSE 0u
#define MGL_EGL_TRUE 1u
#define MGL_EGL_DEFAULT_DISPLAY ((MGL_EGLNativeDisplayType)0)
#define MGL_EGL_NO_DISPLAY ((MGL_EGLDisplay)0)
#define MGL_EGL_NO_CONTEXT ((MGL_EGLContext)0)
#define MGL_EGL_NO_SURFACE ((MGL_EGLSurface)0)

#define MGL_EGL_NONE 0x3038
#define MGL_EGL_SURFACE_TYPE 0x3033
#define MGL_EGL_PBUFFER_BIT 0x0001
#define MGL_EGL_WINDOW_BIT 0x0004
#define MGL_EGL_RED_SIZE 0x3024
#define MGL_EGL_GREEN_SIZE 0x3023
#define MGL_EGL_BLUE_SIZE 0x3022
#define MGL_EGL_ALPHA_SIZE 0x3021
#define MGL_EGL_DEPTH_SIZE 0x3025
#define MGL_EGL_STENCIL_SIZE 0x3026
#define MGL_EGL_RENDERABLE_TYPE 0x3040
#define MGL_EGL_OPENGL_ES2_BIT 0x0004
#define MGL_EGL_OPENGL_ES3_BIT 0x0040
#define MGL_EGL_CONTEXT_CLIENT_VERSION 0x3098
#define MGL_EGL_CONTEXT_MINOR_VERSION 0x30FB
#define MGL_EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE 0x3452
#define MGL_EGL_WIDTH 0x3057
#define MGL_EGL_HEIGHT 0x3056
#define MGL_EGL_OPENGL_ES_API 0x30A0
#define MGL_EGL_VENDOR 0x3053
#define MGL_EGL_VERSION 0x3054
#define MGL_EGL_EXTENSIONS 0x3055
#define MGL_EGL_BAD_NATIVE_WINDOW 0x300B
#define MGL_EGL_BAD_SURFACE 0x300D

#endif

#include "mgl/egl.h"

#include <X11/Xlib.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "CHECK failed %s:%d: %s (egl=%#x)\n", __FILE__, __LINE__, #x,          \
                    eglGetError());                                                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int query_eq(EGLDisplay d, EGLSurface s, EGLint attr, EGLint expected) {
    EGLint value = 0x6a6a6a6a;
    if (!eglQuerySurface(d, s, attr, &value))
        return 0;
    return value == expected;
}

static int query_unchanged(EGLDisplay d, EGLSurface s, EGLint attr) {
    EGLint value = 0x13572468;
    if (!eglQuerySurface(d, s, attr, &value))
        return 0;
    return value == 0x13572468;
}

int main(void) {
    static const EGLint pbuffer_attrs[] = {EGL_WIDTH,           8,        EGL_HEIGHT, 4,
                                           EGL_LARGEST_PBUFFER, EGL_TRUE, EGL_NONE};
    static const EGLint srgb_window_attrs[] = {EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_SRGB, EGL_NONE};
    static const EGLint invalid_window_colorspace_attrs[] = {EGL_GL_COLORSPACE, 0x7fffffff,
                                                             EGL_NONE};
    static const EGLint linear_window_attrs[] = {EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_LINEAR,
                                                 EGL_RENDER_BUFFER, EGL_SINGLE_BUFFER, EGL_NONE};
    static const EGLint bad_texture_attrs[] = {
        EGL_WIDTH, 8, EGL_HEIGHT, 4, EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE, EGL_NONE};
    static const EGLint linear_pbuffer_attrs[] = {
        EGL_WIDTH, 6, EGL_HEIGHT, 5, EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_LINEAR, EGL_NONE};
    static const EGLint srgb_pbuffer_attrs[] = {
        EGL_WIDTH, 6, EGL_HEIGHT, 5, EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_SRGB, EGL_NONE};
    static const EGLint invalid_colorspace_attrs[] = {EGL_WIDTH,         6,          EGL_HEIGHT, 5,
                                                      EGL_GL_COLORSPACE, 0x7fffffff, EGL_NONE};
    static const EGLint negative_width_attrs[] = {EGL_WIDTH, -1, EGL_HEIGHT, 4, EGL_NONE};
    static const EGLint ignored_config_filters[] = {EGL_RENDERABLE_TYPE,
                                                    EGL_OPENGL_BIT,
                                                    EGL_SURFACE_TYPE,
                                                    EGL_PBUFFER_BIT,
                                                    EGL_MAX_PBUFFER_WIDTH,
                                                    1,
                                                    EGL_MAX_PBUFFER_HEIGHT,
                                                    1,
                                                    EGL_MAX_PBUFFER_PIXELS,
                                                    1,
                                                    EGL_NATIVE_VISUAL_ID,
                                                    0x7ffffffe,
                                                    EGL_NONE};
    EGLint config_id_selector[] = {EGL_CONFIG_ID, 0, EGL_BIND_TO_TEXTURE_RGB, EGL_TRUE,
                                   EGL_NONE};
    EGLint config_id_bad_attribute[] = {EGL_CONFIG_ID, 0, 0x7ffffffe, 0, EGL_NONE};
    static const EGLint pbuffer_native_visual_filter[] = {
        EGL_RENDERABLE_TYPE,    EGL_OPENGL_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_NATIVE_VISUAL_TYPE, 0x7ffffffe,     EGL_NONE};
    static const EGLint transparent_none_filter[] = {EGL_RENDERABLE_TYPE,
                                                     EGL_OPENGL_BIT,
                                                     EGL_SURFACE_TYPE,
                                                     EGL_PBUFFER_BIT,
                                                     EGL_TRANSPARENT_TYPE,
                                                     EGL_NONE,
                                                     EGL_TRANSPARENT_RED_VALUE,
                                                     0x7ffffffe,
                                                     EGL_TRANSPARENT_GREEN_VALUE,
                                                     0x7ffffffe,
                                                     EGL_TRANSPARENT_BLUE_VALUE,
                                                     0x7ffffffe,
                                                     EGL_NONE};
    static const EGLint bad_match_native_pixmap_dont_care[] = {EGL_MATCH_NATIVE_PIXMAP,
                                                               EGL_DONT_CARE, EGL_NONE};
    static const EGLint bad_level_dont_care[] = {EGL_LEVEL, EGL_DONT_CARE, EGL_NONE};
    static const EGLint bad_boolean_filter[] = {EGL_NATIVE_RENDERABLE, 2, EGL_NONE};
    static const EGLint bad_color_type_filter[] = {EGL_COLOR_BUFFER_TYPE, 0x7ffffffe, EGL_NONE};
    static const EGLint bad_renderable_mask_filter[] = {EGL_RENDERABLE_TYPE, 0x0010, EGL_NONE};
    static const EGLint bad_surface_mask_filter[] = {EGL_SURFACE_TYPE, 0x0008, EGL_NONE};
    static const EGLint bad_config_id_filter[] = {EGL_CONFIG_ID, 0, EGL_NONE};
    static const EGLint bad_size_filter[] = {EGL_RED_SIZE, -2, EGL_NONE};
    EGLDisplay d;
    EGLConfig config;
    EGLSurface pbuffer;
    EGLSurface window_surface;
    EGLContext context;
    EGLint count = 0;
    EGLint value = 0, config_id = 0;
    Display *xdpy;
    Window window;
    Pixmap pixmap;
    int screen;

    d = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL);
    CHECK(d != EGL_NO_DISPLAY);
    CHECK(eglInitialize(d, NULL, NULL));
    CHECK(eglGetConfigs(d, &config, 1, &count) && count > 0);
    CHECK(eglGetConfigAttrib(d, config, EGL_CONFIG_ID, &config_id) && config_id > 0);
    config_id_selector[1] = config_id;
    config_id_bad_attribute[1] = config_id;

    count = -1;
    CHECK(eglChooseConfig(d, ignored_config_filters, &config, 1, &count) && count > 0);
    count = -1;
    CHECK(eglChooseConfig(d, config_id_selector, &config, 1, &count) && count == 1);
    count = -1;
    CHECK(!eglChooseConfig(d, config_id_bad_attribute, &config, 1, &count));
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
    count = -1;
    CHECK(eglChooseConfig(d, pbuffer_native_visual_filter, &config, 1, &count) && count > 0);
    count = -1;
    CHECK(eglChooseConfig(d, transparent_none_filter, &config, 1, &count) && count > 0);
    CHECK(!eglChooseConfig(d, bad_match_native_pixmap_dont_care, &config, 1, &count));
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
    CHECK(!eglChooseConfig(d, bad_level_dont_care, &config, 1, &count));
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
    CHECK(!eglChooseConfig(d, bad_boolean_filter, &config, 1, &count));
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
    CHECK(!eglChooseConfig(d, bad_color_type_filter, &config, 1, &count));
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
    CHECK(!eglChooseConfig(d, bad_renderable_mask_filter, &config, 1, &count));
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
    CHECK(!eglChooseConfig(d, bad_surface_mask_filter, &config, 1, &count));
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
    CHECK(!eglChooseConfig(d, bad_config_id_filter, &config, 1, &count));
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
    CHECK(!eglChooseConfig(d, bad_size_filter, &config, 1, &count));
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);

    CHECK(eglCreatePbufferSurface(d, config, negative_width_attrs) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_PARAMETER);
    CHECK(eglCreatePbufferSurface(d, config, bad_texture_attrs) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);

    CHECK(eglCreatePbufferSurface(d, config, srgb_pbuffer_attrs) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_MATCH);
    CHECK(eglCreatePbufferSurface(d, config, invalid_colorspace_attrs) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
    {
        EGLSurface linear = eglCreatePbufferSurface(d, config, linear_pbuffer_attrs);
        CHECK(linear != EGL_NO_SURFACE);
        CHECK(query_eq(d, linear, EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_LINEAR));
        CHECK(eglDestroySurface(d, linear));
    }
    CHECK(eglGetConfigs(d, &config, 1, &count) && count > 0);
    pbuffer = eglCreatePbufferSurface(d, config, pbuffer_attrs);
    CHECK(pbuffer != EGL_NO_SURFACE);
    CHECK(query_eq(d, pbuffer, EGL_CONFIG_ID, config_id));
    CHECK(query_eq(d, pbuffer, EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_LINEAR));
    CHECK(query_eq(d, pbuffer, EGL_WIDTH, 8));
    CHECK(query_eq(d, pbuffer, EGL_HEIGHT, 4));
    CHECK(query_eq(d, pbuffer, EGL_LARGEST_PBUFFER, EGL_TRUE));
    CHECK(query_eq(d, pbuffer, EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE));
    CHECK(query_eq(d, pbuffer, EGL_TEXTURE_TARGET, EGL_NO_TEXTURE));
    CHECK(query_eq(d, pbuffer, EGL_MIPMAP_TEXTURE, EGL_FALSE));
    CHECK(query_eq(d, pbuffer, EGL_MIPMAP_LEVEL, 0));
    CHECK(query_eq(d, pbuffer, EGL_MULTISAMPLE_RESOLVE, EGL_MULTISAMPLE_RESOLVE_DEFAULT));
    CHECK(query_eq(d, pbuffer, EGL_RENDER_BUFFER, EGL_BACK_BUFFER));
    CHECK(query_eq(d, pbuffer, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED));
    CHECK(query_eq(d, pbuffer, EGL_HORIZONTAL_RESOLUTION, EGL_UNKNOWN));
    CHECK(query_eq(d, pbuffer, EGL_VERTICAL_RESOLUTION, EGL_UNKNOWN));
    CHECK(query_eq(d, pbuffer, EGL_PIXEL_ASPECT_RATIO, EGL_UNKNOWN));

    /* Pbuffer texture-binding requests are rejected by the OpenGL-only EGL interface. */
    CHECK(!eglBindTexImage(d, pbuffer, EGL_BACK_BUFFER));
    CHECK(eglGetError() == EGL_BAD_SURFACE);
    CHECK(!eglReleaseTexImage(d, pbuffer, EGL_BACK_BUFFER));
    CHECK(eglGetError() == EGL_BAD_SURFACE);
    CHECK(!eglBindTexImage(EGL_NO_DISPLAY, pbuffer, EGL_BACK_BUFFER));
    CHECK(eglGetError() == EGL_BAD_DISPLAY);
    CHECK(!eglBindTexImage(d, (EGLSurface)(uintptr_t)1, EGL_BACK_BUFFER));
    CHECK(eglGetError() == EGL_BAD_SURFACE);
    CHECK(!eglReleaseTexImage(EGL_NO_DISPLAY, pbuffer, EGL_BACK_BUFFER));
    CHECK(eglGetError() == EGL_BAD_DISPLAY);
    CHECK(!eglReleaseTexImage(d, (EGLSurface)(uintptr_t)1, EGL_BACK_BUFFER));
    CHECK(eglGetError() == EGL_BAD_SURFACE);

    CHECK(eglCreatePbufferFromClientBuffer(EGL_NO_DISPLAY, EGL_OPENVG_IMAGE, NULL, config, NULL) ==
          EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_DISPLAY);
    CHECK(eglCreatePbufferFromClientBuffer(d, EGL_OPENVG_IMAGE, NULL, NULL, NULL) ==
          EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_CONFIG);
    CHECK(eglCreatePbufferFromClientBuffer(d, 0x7ffffffeu, NULL, config, NULL) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_PARAMETER);
    CHECK(eglCreatePbufferFromClientBuffer(d, EGL_OPENVG_IMAGE, NULL, config, NULL) ==
          EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_ACCESS);

    /* eglCopyBuffers first requires the surface to be the current draw
     * surface. Once that is true, this no-pixmap implementation must report
     * EGL_BAD_NATIVE_PIXMAP. */
    CHECK(!eglCopyBuffers(EGL_NO_DISPLAY, pbuffer, (EGLNativePixmapType)0));
    CHECK(eglGetError() == EGL_BAD_DISPLAY);
    CHECK(!eglCopyBuffers(d, pbuffer, (EGLNativePixmapType)0));
    CHECK(eglGetError() == EGL_BAD_SURFACE);
    CHECK(eglBindAPI(EGL_OPENGL_API));
    context = eglCreateContext(d, config, EGL_NO_CONTEXT, NULL);
    CHECK(context != EGL_NO_CONTEXT);
    CHECK(eglMakeCurrent(d, pbuffer, pbuffer, context));
    CHECK(!eglCopyBuffers(d, pbuffer, (EGLNativePixmapType)0));
    CHECK(eglGetError() == EGL_BAD_NATIVE_PIXMAP);
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    CHECK(eglDestroyContext(d, context));

    CHECK(!eglSurfaceAttrib(d, pbuffer, EGL_MIPMAP_LEVEL, 2));
    CHECK(eglGetError() == EGL_BAD_PARAMETER);
    CHECK(query_eq(d, pbuffer, EGL_MIPMAP_LEVEL, 0));
    CHECK(eglSurfaceAttrib(d, pbuffer, EGL_MULTISAMPLE_RESOLVE, EGL_MULTISAMPLE_RESOLVE_DEFAULT));
    CHECK(!eglSurfaceAttrib(d, pbuffer, EGL_MULTISAMPLE_RESOLVE, EGL_MULTISAMPLE_RESOLVE_BOX));
    CHECK(eglGetError() == EGL_BAD_MATCH);
    CHECK(query_eq(d, pbuffer, EGL_MULTISAMPLE_RESOLVE, EGL_MULTISAMPLE_RESOLVE_DEFAULT));
    CHECK(eglSurfaceAttrib(d, pbuffer, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED));
    CHECK(!eglSurfaceAttrib(d, pbuffer, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED));
    CHECK(eglGetError() == EGL_BAD_MATCH);
    CHECK(query_eq(d, pbuffer, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED));
    CHECK(!eglSurfaceAttrib(d, pbuffer, EGL_WIDTH, 1));
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
    CHECK(eglDestroySurface(d, pbuffer));
    CHECK(eglTerminate(d));

    xdpy = XOpenDisplay(NULL);
    CHECK(xdpy != NULL);
    screen = DefaultScreen(xdpy);
    window = XCreateSimpleWindow(xdpy, RootWindow(xdpy, screen), 0, 0, 16, 12, 0, 0, 0);
    CHECK(window != None);
    XMapWindow(xdpy, window);
    pixmap = XCreatePixmap(xdpy, window, 4, 4, (unsigned int)DefaultDepth(xdpy, screen));
    CHECK(pixmap != None);
    XSync(xdpy, False);

    d = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, xdpy, NULL);
    CHECK(d != EGL_NO_DISPLAY);
    CHECK(eglInitialize(d, NULL, NULL));
    CHECK(eglGetConfigs(d, &config, 1, &count) && count > 0);
    count = -1;
    CHECK(eglChooseConfig(d, ignored_config_filters, &config, 1, &count) && count > 0);
    count = -1;
    CHECK(eglChooseConfig(d, pbuffer_native_visual_filter, &config, 1, &count) && count > 0);
    {
        EGLint match_native_pixmap_filter[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                                               EGL_MATCH_NATIVE_PIXMAP, (EGLint)(uintptr_t)pixmap,
                                               EGL_NONE};
        count = -1;
        CHECK(eglChooseConfig(d, match_native_pixmap_filter, &config, 1, &count) && count == 0);
    }
    CHECK(eglCreateWindowSurface(d, config, (EGLNativeWindowType)(uintptr_t)window,
                                 srgb_window_attrs) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_MATCH);
    CHECK(eglCreateWindowSurface(d, config, (EGLNativeWindowType)(uintptr_t)window,
                                 invalid_window_colorspace_attrs) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
    window_surface = eglCreateWindowSurface(d, config, (EGLNativeWindowType)(uintptr_t)window,
                                            linear_window_attrs);
    CHECK(window_surface != EGL_NO_SURFACE);
    CHECK(eglGetConfigAttrib(d, config, EGL_CONFIG_ID, &config_id) && config_id > 0);
    CHECK(query_eq(d, window_surface, EGL_CONFIG_ID, config_id));
    CHECK(query_eq(d, window_surface, EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_LINEAR));
    CHECK(query_eq(d, window_surface, EGL_RENDER_BUFFER, EGL_SINGLE_BUFFER));
    CHECK(query_eq(d, window_surface, EGL_MULTISAMPLE_RESOLVE, EGL_MULTISAMPLE_RESOLVE_DEFAULT));
    CHECK(query_eq(d, window_surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED));
    CHECK(query_unchanged(d, window_surface, EGL_LARGEST_PBUFFER));
    CHECK(query_unchanged(d, window_surface, EGL_TEXTURE_FORMAT));
    CHECK(query_unchanged(d, window_surface, EGL_TEXTURE_TARGET));
    CHECK(query_unchanged(d, window_surface, EGL_MIPMAP_TEXTURE));
    CHECK(query_unchanged(d, window_surface, EGL_MIPMAP_LEVEL));
    CHECK(eglBindAPI(EGL_OPENGL_API));
    context = eglCreateContext(d, config, EGL_NO_CONTEXT, NULL);
    CHECK(context != EGL_NO_CONTEXT);
    CHECK(!eglMakeCurrent(d, window_surface, window_surface, context));
    CHECK(eglGetError() == EGL_BAD_MATCH);
    CHECK(eglDestroyContext(d, context));
    CHECK(!eglSurfaceAttrib(d, window_surface, EGL_MIPMAP_LEVEL, 1));
    CHECK(eglGetError() == EGL_BAD_PARAMETER);
    CHECK(eglDestroySurface(d, window_surface));
    CHECK(eglTerminate(d));
    XFreePixmap(xdpy, pixmap);
    XDestroyWindow(xdpy, window);
    XCloseDisplay(xdpy);
    return 0;
}

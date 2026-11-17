#include "mgl/egl.h"
#include "mgl/gl.h"
#include "mgl/platform.h"

#include "core/runtime_services.h"
#include "core/version.h"

#include <X11/Xlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MGL_EGL_DISPLAY_MAGIC 0x4d454744u
#define MGL_EGL_SURFACE_MAGIC 0x4d454753u
#define MGL_EGL_CONTEXT_MAGIC 0x4d454743u
#define MGL_EGL_SYNC_MAGIC 0x4d454759u

enum mgl_egl_surface_kind { MGL_EGL_SURFACE_PBUFFER, MGL_EGL_SURFACE_WINDOW };

struct mgl_egl_display;

struct mgl_egl_config {
    struct mgl_egl_display *display;
};

struct mgl_egl_surface {
    uint32_t magic;
    struct mgl_egl_display *display;
    struct mgl_egl_config *config;
    enum mgl_egl_surface_kind kind;
    EGLNativeWindowType native_window;
    EGLint width, height;
    EGLint texture_format, texture_target;
    EGLBoolean mipmap_texture;
    EGLBoolean largest_pbuffer;
    EGLint mipmap_level;
    EGLint multisample_resolve;
    EGLint render_buffer;
    EGLint gl_colorspace;
    EGLint swap_behavior;
    EGLint swap_interval;
    struct mgl_android_present *android_present;
    unsigned refs;
    unsigned current_count;
    int destroy_pending;
    struct mgl_egl_surface *next;
};

struct mgl_egl_context {
    uint32_t magic;
    struct mgl_egl_display *display;
    struct mgl_context *core;
    EGLint major, minor;
    EGLint flags, profile_mask;
    EGLint config_id;
    EGLint render_buffer;
    EGLBoolean no_error;
    EGLBoolean ever_current;
    unsigned refs;
    unsigned current_count;
    int destroy_pending;
    struct mgl_egl_context *next;
};

struct mgl_egl_sync {
    uint32_t magic;
    struct mgl_egl_display *display;
    EGLenum type;
    EGLAttrib status;
    EGLAttrib condition;
    struct mgl_egl_sync *next;
};

struct mgl_egl_display {
    uint32_t magic;
    EGLenum platform;
    void *native_display;
    void *native_display_key;
    int native_screen;
    int owns_native_display;
    int initialized;
    struct mgl_egl_config config;
    struct mgl_egl_display *next;
};

struct mgl_egl_tls {
    EGLint error;
    EGLenum api;
    struct mgl_egl_display *display;
    struct mgl_egl_context *context;
    struct mgl_egl_surface *draw;
    struct mgl_egl_surface *read;
};

static pthread_mutex_t egl_lock = PTHREAD_MUTEX_INITIALIZER;
static struct mgl_egl_display *displays;
static struct mgl_egl_surface *surfaces;
static struct mgl_egl_context *contexts;
static struct mgl_egl_sync *syncs;
static _Thread_local struct mgl_egl_tls tls = {EGL_SUCCESS, EGL_NONE, NULL, NULL, NULL, NULL};

MGL_EXPORT const unsigned int mgl_public_egl_interface = 1u;

static const char client_extensions[] =
    "EGL_EXT_client_extensions EGL_EXT_platform_base EGL_EXT_platform_x11 "
    "EGL_KHR_client_get_all_proc_addresses EGL_KHR_platform_x11 "
    "EGL_MESA_platform_surfaceless";
static const char display_extensions[] = "EGL_KHR_create_context EGL_KHR_create_context_no_error "
                                         "EGL_KHR_no_config_context EGL_KHR_surfaceless_context";

static const struct mgl_runtime_services *bridge(void) {
    return mgl_runtime_services_get();
}

static void set_error(EGLint error) {
    if (tls.error == EGL_SUCCESS)
        tls.error = error;
}

static struct mgl_egl_display *find_display_locked(EGLDisplay handle) {
    struct mgl_egl_display *d;
    for (d = displays; d != NULL; d = d->next)
        if ((EGLDisplay)d == handle && d->magic == MGL_EGL_DISPLAY_MAGIC)
            return d;
    return NULL;
}

static struct mgl_egl_surface *find_surface_locked(EGLSurface handle) {
    struct mgl_egl_surface *s;
    for (s = surfaces; s != NULL; s = s->next)
        if ((EGLSurface)s == handle && s->magic == MGL_EGL_SURFACE_MAGIC && !s->destroy_pending)
            return s;
    return NULL;
}

static struct mgl_egl_context *find_context_locked(EGLContext handle) {
    struct mgl_egl_context *c;
    for (c = contexts; c != NULL; c = c->next)
        if ((EGLContext)c == handle && c->magic == MGL_EGL_CONTEXT_MAGIC && !c->destroy_pending)
            return c;
    return NULL;
}

static struct mgl_egl_sync *find_sync_locked(EGLSync handle) {
    struct mgl_egl_sync *sync;
    for (sync = syncs; sync != NULL; sync = sync->next) {
        if ((EGLSync)sync == handle && sync->magic == MGL_EGL_SYNC_MAGIC)
            return sync;
    }
    return NULL;
}

static void unlink_sync_locked(struct mgl_egl_sync *sync) {
    struct mgl_egl_sync **link = &syncs;
    if (sync == NULL)
        return;
    while (*link != NULL && *link != sync)
        link = &(*link)->next;
    if (*link == sync)
        *link = sync->next;
    sync->magic = 0u;
    free(sync);
}

static void unlink_surface_locked(struct mgl_egl_surface *s) {
    struct mgl_egl_surface **p = &surfaces;
    while (*p != NULL && *p != s)
        p = &(*p)->next;
    if (*p == s)
        *p = s->next;
    if (s->android_present != NULL)
        bridge()->android_present_unref(s->android_present);
    s->magic = 0u;
    free(s);
}

static void surface_retain_locked(struct mgl_egl_surface *s) {
    if (s != NULL)
        ++s->refs;
}

static void surface_release_locked(struct mgl_egl_surface *s) {
    if (s != NULL && --s->refs == 0u)
        unlink_surface_locked(s);
}

static void unlink_context_locked(struct mgl_egl_context *c) {
    struct mgl_egl_context **p = &contexts;
    while (*p != NULL && *p != c)
        p = &(*p)->next;
    if (*p == c)
        *p = c->next;
    c->magic = 0u;
    bridge()->context_destroy(c->core);
    free(c);
}

static void context_retain_locked(struct mgl_egl_context *c) {
    if (c != NULL)
        ++c->refs;
}

static void context_release_locked(struct mgl_egl_context *c) {
    if (c != NULL && --c->refs == 0u)
        unlink_context_locked(c);
}

static int display_ready_locked(struct mgl_egl_display *d) {
    if (d == NULL) {
        set_error(EGL_BAD_DISPLAY);
        return 0;
    }
    if (!d->initialized) {
        set_error(EGL_NOT_INITIALIZED);
        return 0;
    }
    return 1;
}

MGL_EXPORT EGLint eglGetError(void) {
    EGLint e = tls.error;
    tls.error = EGL_SUCCESS;
    return e;
}

static EGLDisplay get_platform_display_common(EGLenum platform, void *native_display,
                                              int has_screen, EGLint requested_screen) {
    struct mgl_egl_display *d;
    void *native_key = native_display;
    Display *x11 = NULL;
    int owns_x11 = 0;
    int screen = 0;

    if (platform != EGL_PLATFORM_SURFACELESS_MESA && platform != EGL_PLATFORM_X11_KHR) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_NO_DISPLAY;
    }
    if (platform == EGL_PLATFORM_SURFACELESS_MESA) {
        if (native_display != EGL_DEFAULT_DISPLAY || has_screen) {
            set_error(has_screen ? EGL_BAD_ATTRIBUTE : EGL_BAD_PARAMETER);
            return EGL_NO_DISPLAY;
        }
    } else {
        x11 = (Display *)native_display;
        if (x11 == NULL) {
            x11 = XOpenDisplay(NULL);
            if (x11 == NULL)
                return EGL_NO_DISPLAY;
            owns_x11 = 1;
        }
        screen = has_screen ? requested_screen : DefaultScreen(x11);
        if (screen < 0 || screen >= ScreenCount(x11)) {
            if (owns_x11)
                XCloseDisplay(x11);
            set_error(EGL_BAD_ATTRIBUTE);
            return EGL_NO_DISPLAY;
        }
        native_display = x11;
    }

    pthread_mutex_lock(&egl_lock);
    for (d = displays; d != NULL; d = d->next) {
        if (d->platform == platform && d->native_display_key == native_key &&
            d->native_screen == screen) {
            pthread_mutex_unlock(&egl_lock);
            if (owns_x11)
                XCloseDisplay(x11);
            return (EGLDisplay)d;
        }
    }
    d = calloc(1, sizeof(*d));
    if (d == NULL) {
        pthread_mutex_unlock(&egl_lock);
        if (owns_x11)
            XCloseDisplay(x11);
        set_error(EGL_BAD_ALLOC);
        return EGL_NO_DISPLAY;
    }
    d->magic = MGL_EGL_DISPLAY_MAGIC;
    d->platform = platform;
    d->native_display = native_display;
    d->native_display_key = native_key;
    d->native_screen = screen;
    d->owns_native_display = owns_x11;
    d->config.display = d;
    d->next = displays;
    displays = d;
    pthread_mutex_unlock(&egl_lock);
    return (EGLDisplay)d;
}

MGL_EXPORT EGLDisplay eglGetPlatformDisplay(EGLenum platform, void *native_display,
                                            const EGLAttrib *attribs) {
    int has_screen = 0;
    EGLint screen = 0;
    const EGLAttrib *p;
    if (attribs != NULL) {
        for (p = attribs; *p != EGL_NONE; p += 2) {
            if (platform == EGL_PLATFORM_X11_KHR && p[0] == EGL_PLATFORM_X11_SCREEN_KHR) {
                has_screen = 1;
                screen = (EGLint)p[1];
            } else {
                set_error(EGL_BAD_ATTRIBUTE);
                return EGL_NO_DISPLAY;
            }
        }
    }
    return get_platform_display_common(platform, native_display, has_screen, screen);
}

MGL_EXPORT EGLDisplay eglGetPlatformDisplayEXT(EGLenum platform, void *native_display,
                                               const EGLint *attribs) {
    int has_screen = 0;
    EGLint screen = 0;
    const EGLint *p;
    if (attribs != NULL) {
        for (p = attribs; *p != EGL_NONE; p += 2) {
            if (platform == EGL_PLATFORM_X11_KHR && p[0] == EGL_PLATFORM_X11_SCREEN_EXT) {
                has_screen = 1;
                screen = p[1];
            } else {
                set_error(EGL_BAD_ATTRIBUTE);
                return EGL_NO_DISPLAY;
            }
        }
    }
    return get_platform_display_common(platform, native_display, has_screen, screen);
}

MGL_EXPORT EGLDisplay eglGetDisplay(EGLNativeDisplayType native_display) {
    if (native_display != EGL_DEFAULT_DISPLAY)
        return eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, native_display, NULL);
    return eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL);
}

MGL_EXPORT EGLBoolean eglInitialize(EGLDisplay display, EGLint *major, EGLint *minor) {
    struct mgl_egl_display *d;
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (d == NULL) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_DISPLAY);
        return EGL_FALSE;
    }
    d->initialized = 1;
    pthread_mutex_unlock(&egl_lock);
    if (major != NULL)
        *major = 1;
    if (minor != NULL)
        *minor = 5;
    return EGL_TRUE;
}

MGL_EXPORT EGLBoolean eglTerminate(EGLDisplay display) {
    struct mgl_egl_display *d;
    struct mgl_egl_context *c, *next_c;
    struct mgl_egl_surface *s, *next_s;
    struct mgl_egl_sync *sync, *next_sync;

    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (d == NULL) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_DISPLAY);
        return EGL_FALSE;
    }

    /* EGL termination invalidates handles immediately; current resources remain alive until
     * released. */
    for (c = contexts; c != NULL; c = next_c) {
        next_c = c->next;
        if (c->display == d && !c->destroy_pending) {
            c->destroy_pending = 1;
            context_release_locked(c);
        }
    }
    for (s = surfaces; s != NULL; s = next_s) {
        next_s = s->next;
        if (s->display == d && !s->destroy_pending) {
            s->destroy_pending = 1;
            surface_release_locked(s);
        }
    }
    for (sync = syncs; sync != NULL; sync = next_sync) {
        next_sync = sync->next;
        if (sync->display == d)
            unlink_sync_locked(sync);
    }
    d->initialized = 0;
    pthread_mutex_unlock(&egl_lock);
    return EGL_TRUE;
}

MGL_EXPORT const char *eglQueryString(EGLDisplay display, EGLint name) {
    struct mgl_egl_display *d;
    if (display == EGL_NO_DISPLAY) {
        if (name == EGL_EXTENSIONS)
            return client_extensions;
        if (name == EGL_VERSION)
            return "1.5 ManifestGL";
        set_error(EGL_BAD_DISPLAY);
        return NULL;
    }
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return NULL;
    }
    pthread_mutex_unlock(&egl_lock);
    switch (name) {
        case EGL_VENDOR:
            return "ManifestGL";
        case EGL_VERSION:
            return "1.5 ManifestGL";
        case EGL_EXTENSIONS:
            return display_extensions;
        case EGL_CLIENT_APIS:
            return "OpenGL";
        default:
            set_error(EGL_BAD_PARAMETER);
            return NULL;
    }
}

MGL_EXPORT EGLBoolean eglGetConfigs(EGLDisplay display, EGLConfig *configs, EGLint config_size,
                                    EGLint *num_config) {
    struct mgl_egl_display *d;
    if (num_config == NULL || config_size < 0) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    if (configs != NULL && config_size > 0)
        configs[0] = (EGLConfig)&d->config;
    *num_config = 1;
    pthread_mutex_unlock(&egl_lock);
    return EGL_TRUE;
}

static EGLint config_value(struct mgl_egl_display *d, EGLint attribute, int *known) {
    *known = 1;
    switch (attribute) {
        case EGL_BUFFER_SIZE:
            return 32;
        case EGL_RED_SIZE:
        case EGL_GREEN_SIZE:
        case EGL_BLUE_SIZE:
        case EGL_ALPHA_SIZE:
            return 8;
        case EGL_DEPTH_SIZE:
            return 24;
        case EGL_STENCIL_SIZE:
            return 8;
        case EGL_CONFIG_ID:
            return 1;
        case EGL_NATIVE_RENDERABLE:
            return EGL_FALSE;
        case EGL_COLOR_BUFFER_TYPE:
            return EGL_RGB_BUFFER;
        case EGL_RENDERABLE_TYPE:
            return EGL_OPENGL_BIT;
        case EGL_CONFORMANT:
            return 0;
        case EGL_SURFACE_TYPE:
            return d->platform == EGL_PLATFORM_X11_KHR ? (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)
                                                       : EGL_PBUFFER_BIT;
        case EGL_NATIVE_VISUAL_ID:
            if (d->platform == EGL_PLATFORM_X11_KHR)
                return (EGLint)XVisualIDFromVisual(
                    DefaultVisual((Display *)d->native_display, d->native_screen));
            return 0;
        case EGL_NATIVE_VISUAL_TYPE:
            if (d->platform == EGL_PLATFORM_X11_KHR)
                return DefaultVisual((Display *)d->native_display, d->native_screen)->class;
            return EGL_NONE;
        case EGL_TRANSPARENT_TYPE:
            return EGL_NONE;
        case EGL_TRANSPARENT_RED_VALUE:
        case EGL_TRANSPARENT_GREEN_VALUE:
        case EGL_TRANSPARENT_BLUE_VALUE:
            return 0;
        case EGL_SAMPLE_BUFFERS:
        case EGL_SAMPLES:
            return 0;
        case EGL_LEVEL:
            return 0;
        case EGL_CONFIG_CAVEAT:
            return EGL_NONE;
        case EGL_LUMINANCE_SIZE:
        case EGL_ALPHA_MASK_SIZE:
            return 0;
        case EGL_BIND_TO_TEXTURE_RGB:
        case EGL_BIND_TO_TEXTURE_RGBA:
            return EGL_FALSE;
        case EGL_MIN_SWAP_INTERVAL:
            return 0;
        case EGL_MAX_SWAP_INTERVAL:
            return 4;
        case EGL_MAX_PBUFFER_WIDTH:
        case EGL_MAX_PBUFFER_HEIGHT:
            return 16384;
        case EGL_MAX_PBUFFER_PIXELS:
            return 16384 * 16384;
        default:
            *known = 0;
            return 0;
    }
}

MGL_EXPORT EGLBoolean eglGetConfigAttrib(EGLDisplay display, EGLConfig config, EGLint attribute,
                                         EGLint *value) {
    struct mgl_egl_display *d;
    int known;
    if (value == NULL) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    if (config != (EGLConfig)&d->config) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_CONFIG);
        return EGL_FALSE;
    }
    *value = config_value(d, attribute, &known);
    pthread_mutex_unlock(&egl_lock);
    if (!known) {
        set_error(EGL_BAD_ATTRIBUTE);
        return EGL_FALSE;
    }
    return EGL_TRUE;
}

static int choose_config_value_valid(EGLint attribute, EGLint value) {
    const EGLint renderable_bits = EGL_OPENGL_ES_BIT | EGL_OPENVG_BIT | EGL_OPENGL_ES2_BIT |
                                   EGL_OPENGL_BIT | EGL_OPENGL_ES3_BIT;
    const EGLint surface_bits = EGL_PBUFFER_BIT | EGL_PIXMAP_BIT | EGL_WINDOW_BIT |
                                EGL_VG_COLORSPACE_LINEAR_BIT | EGL_VG_ALPHA_FORMAT_PRE_BIT |
                                EGL_MULTISAMPLE_RESOLVE_BOX_BIT | EGL_SWAP_BEHAVIOR_PRESERVED_BIT;

    if (value == EGL_DONT_CARE)
        return attribute != EGL_LEVEL && attribute != EGL_MATCH_NATIVE_PIXMAP;

    switch (attribute) {
        case EGL_BIND_TO_TEXTURE_RGB:
        case EGL_BIND_TO_TEXTURE_RGBA:
        case EGL_NATIVE_RENDERABLE:
            return value == EGL_FALSE || value == EGL_TRUE;
        case EGL_COLOR_BUFFER_TYPE:
            return value == EGL_RGB_BUFFER || value == EGL_LUMINANCE_BUFFER;
        case EGL_CONFIG_CAVEAT:
            return value == EGL_NONE || value == EGL_SLOW_CONFIG ||
                   value == EGL_NON_CONFORMANT_CONFIG;
        case EGL_TRANSPARENT_TYPE:
            return value == EGL_NONE || value == EGL_TRANSPARENT_RGB;
        case EGL_RENDERABLE_TYPE:
        case EGL_CONFORMANT:
            return value >= 0 && (value & ~renderable_bits) == 0;
        case EGL_SURFACE_TYPE:
            return value >= 0 && (value & ~surface_bits) == 0;
        case EGL_CONFIG_ID:
            return value >= 1;
        case EGL_BUFFER_SIZE:
        case EGL_RED_SIZE:
        case EGL_GREEN_SIZE:
        case EGL_BLUE_SIZE:
        case EGL_LUMINANCE_SIZE:
        case EGL_ALPHA_SIZE:
        case EGL_ALPHA_MASK_SIZE:
        case EGL_DEPTH_SIZE:
        case EGL_SAMPLE_BUFFERS:
        case EGL_SAMPLES:
        case EGL_STENCIL_SIZE:
        case EGL_MIN_SWAP_INTERVAL:
        case EGL_MAX_SWAP_INTERVAL:
            return value >= 0;
        default:
            return 1;
    }
}

MGL_EXPORT EGLBoolean eglChooseConfig(EGLDisplay display, const EGLint *attribs, EGLConfig *configs,
                                      EGLint config_size, EGLint *num_config) {
    struct mgl_egl_display *d;
    EGLint value;
    EGLint requested_config_id = EGL_DONT_CARE;
    EGLint requested_surface_type = EGL_DONT_CARE;
    EGLint requested_transparent_type = EGL_DONT_CARE;
    int config_id_specified = 0;
    int renderable_type_specified = 0;
    int surface_type_specified = 0;
    int transparent_type_specified = 0;
    const EGLint *p;

    if (num_config == NULL || config_size < 0) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }

    /* Validate every attribute name before applying the EGL_CONFIG_ID selection rule. */
    if (attribs != NULL)
        for (p = attribs; *p != EGL_NONE; p += 2) {
            EGLint attr = p[0];
            int known = attr == EGL_MATCH_NATIVE_PIXMAP;
            if (!known)
                (void)config_value(d, attr, &known);
            if (!known) {
                pthread_mutex_unlock(&egl_lock);
                set_error(EGL_BAD_ATTRIBUTE);
                return EGL_FALSE;
            }
            if (!choose_config_value_valid(attr, p[1])) {
                pthread_mutex_unlock(&egl_lock);
                set_error(EGL_BAD_ATTRIBUTE);
                return EGL_FALSE;
            }
            if (attr == EGL_CONFIG_ID) {
                config_id_specified = 1;
                requested_config_id = p[1];
            } else if (attr == EGL_RENDERABLE_TYPE)
                renderable_type_specified = 1;
            else if (attr == EGL_SURFACE_TYPE) {
                surface_type_specified = 1;
                requested_surface_type = p[1];
            } else if (attr == EGL_TRANSPARENT_TYPE) {
                transparent_type_specified = 1;
                requested_transparent_type = p[1];
            }
        }

    /* After attribute validation, a concrete EGL_CONFIG_ID selects by ID alone. */
    if (config_id_specified && requested_config_id != EGL_DONT_CARE) {
        *num_config = requested_config_id == config_value(d, EGL_CONFIG_ID, &(int){0}) ? 1 : 0;
        if (*num_config != 0 && configs != NULL && config_size > 0)
            configs[0] = (EGLConfig)&d->config;
        pthread_mutex_unlock(&egl_lock);
        return EGL_TRUE;
    }

    if (attribs != NULL)
        for (p = attribs; *p != EGL_NONE; p += 2) {
            EGLint attr = p[0], want = p[1];
            int known, reject = 0;

            if (attr == EGL_MATCH_NATIVE_PIXMAP) {
                if (want == EGL_NONE)
                    continue;
                *num_config = 0;
                pthread_mutex_unlock(&egl_lock);
                return EGL_TRUE;
            }

            value = config_value(d, attr, &known);
            (void)known;

            if (attr == EGL_MAX_PBUFFER_WIDTH || attr == EGL_MAX_PBUFFER_HEIGHT ||
                attr == EGL_MAX_PBUFFER_PIXELS || attr == EGL_NATIVE_VISUAL_ID)
                continue;

            if (attr == EGL_NATIVE_VISUAL_TYPE &&
                ((surface_type_specified && requested_surface_type != EGL_DONT_CARE &&
                  (requested_surface_type & EGL_WINDOW_BIT) == 0) ||
                 d->platform != EGL_PLATFORM_X11_KHR))
                continue;

            if ((attr == EGL_TRANSPARENT_RED_VALUE || attr == EGL_TRANSPARENT_GREEN_VALUE ||
                 attr == EGL_TRANSPARENT_BLUE_VALUE) &&
                transparent_type_specified && requested_transparent_type == EGL_NONE)
                continue;

            if (want != EGL_DONT_CARE) {
                if (attr == EGL_SURFACE_TYPE || attr == EGL_RENDERABLE_TYPE ||
                    attr == EGL_CONFORMANT)
                    reject = (value & want) != want;
                else if (attr == EGL_RED_SIZE || attr == EGL_GREEN_SIZE || attr == EGL_BLUE_SIZE ||
                         attr == EGL_ALPHA_SIZE || attr == EGL_DEPTH_SIZE ||
                         attr == EGL_STENCIL_SIZE || attr == EGL_BUFFER_SIZE)
                    reject = value < want;
                else
                    reject = value != want;
            }
            if (reject) {
                *num_config = 0;
                pthread_mutex_unlock(&egl_lock);
                return EGL_TRUE;
            }
        }

    /* EGL's default ES1 renderability and window-surface selectors constrain the single config. */
    if (!renderable_type_specified && (config_value(d, EGL_RENDERABLE_TYPE, &(int){0}) &
                                       EGL_OPENGL_ES_BIT) != EGL_OPENGL_ES_BIT) {
        *num_config = 0;
        pthread_mutex_unlock(&egl_lock);
        return EGL_TRUE;
    }
    if (!surface_type_specified &&
        (config_value(d, EGL_SURFACE_TYPE, &(int){0}) & EGL_WINDOW_BIT) != EGL_WINDOW_BIT) {
        *num_config = 0;
        pthread_mutex_unlock(&egl_lock);
        return EGL_TRUE;
    }
    if (configs != NULL && config_size > 0)
        configs[0] = (EGLConfig)&d->config;
    *num_config = 1;
    pthread_mutex_unlock(&egl_lock);
    return EGL_TRUE;
}

static struct mgl_egl_surface *alloc_surface_locked(struct mgl_egl_display *d,
                                                    enum mgl_egl_surface_kind kind) {
    struct mgl_egl_surface *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return NULL;
    s->magic = MGL_EGL_SURFACE_MAGIC;
    s->display = d;
    s->config = &d->config;
    s->kind = kind;
    s->refs = 1u;
    s->texture_format = EGL_NO_TEXTURE;
    s->texture_target = EGL_NO_TEXTURE;
    s->swap_interval = 1;
    s->mipmap_level = 0;
    s->multisample_resolve = EGL_MULTISAMPLE_RESOLVE_DEFAULT;
    s->render_buffer = EGL_BACK_BUFFER;
    s->gl_colorspace = EGL_GL_COLORSPACE_LINEAR;
    s->swap_behavior = EGL_BUFFER_DESTROYED;
    s->next = surfaces;
    surfaces = s;
    return s;
}

MGL_EXPORT EGLSurface eglCreatePbufferSurface(EGLDisplay display, EGLConfig config,
                                              const EGLint *attribs) {
    struct mgl_egl_display *d;
    struct mgl_egl_surface *s;
    const EGLint *p;
    EGLint width = 0, height = 0, texture_format = EGL_NO_TEXTURE, texture_target = EGL_NO_TEXTURE;
    EGLBoolean mipmap = EGL_FALSE, largest = EGL_FALSE;
    int texture_attribute_specified = 0;
    if (attribs != NULL)
        for (p = attribs; *p != EGL_NONE; p += 2)
            switch (p[0]) {
                case EGL_WIDTH:
                    width = p[1];
                    break;
                case EGL_HEIGHT:
                    height = p[1];
                    break;
                case EGL_GL_COLORSPACE:
                    if (p[1] == EGL_GL_COLORSPACE_SRGB) {
                        set_error(EGL_BAD_MATCH);
                        return EGL_NO_SURFACE;
                    }
                    if (p[1] != EGL_GL_COLORSPACE_LINEAR) {
                        set_error(EGL_BAD_ATTRIBUTE);
                        return EGL_NO_SURFACE;
                    }
                    break;
                case EGL_LARGEST_PBUFFER:
                    if (p[1] != EGL_FALSE && p[1] != EGL_TRUE) {
                        set_error(EGL_BAD_ATTRIBUTE);
                        return EGL_NO_SURFACE;
                    }
                    largest = (EGLBoolean)p[1];
                    break;
                case EGL_TEXTURE_FORMAT:
                    texture_attribute_specified = 1;
                    if (p[1] != EGL_NO_TEXTURE && p[1] != EGL_TEXTURE_RGB &&
                        p[1] != EGL_TEXTURE_RGBA) {
                        set_error(EGL_BAD_ATTRIBUTE);
                        return EGL_NO_SURFACE;
                    }
                    texture_format = p[1];
                    break;
                case EGL_TEXTURE_TARGET:
                    texture_attribute_specified = 1;
                    if (p[1] != EGL_NO_TEXTURE && p[1] != EGL_TEXTURE_2D) {
                        set_error(EGL_BAD_ATTRIBUTE);
                        return EGL_NO_SURFACE;
                    }
                    texture_target = p[1];
                    break;
                case EGL_MIPMAP_TEXTURE:
                    texture_attribute_specified = 1;
                    if (p[1] != EGL_FALSE && p[1] != EGL_TRUE) {
                        set_error(EGL_BAD_ATTRIBUTE);
                        return EGL_NO_SURFACE;
                    }
                    mipmap = (EGLBoolean)p[1];
                    break;
                default:
                    set_error(EGL_BAD_ATTRIBUTE);
                    return EGL_NO_SURFACE;
            }
    if (width < 0 || height < 0) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_NO_SURFACE;
    }
    if (width == 0 || height == 0) {
        set_error(EGL_BAD_ATTRIBUTE);
        return EGL_NO_SURFACE;
    }
    if ((texture_format == EGL_NO_TEXTURE) != (texture_target == EGL_NO_TEXTURE)) {
        set_error(EGL_BAD_MATCH);
        return EGL_NO_SURFACE;
    }
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_NO_SURFACE;
    }
    if (config != (EGLConfig)&d->config) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_CONFIG);
        return EGL_NO_SURFACE;
    }
    /* The single config advertises OpenGL renderability. EGL 1.5 rejects pbuffer texture
     * attributes without OpenGL ES renderability. */
    if (texture_attribute_specified) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_ATTRIBUTE);
        return EGL_NO_SURFACE;
    }
    s = alloc_surface_locked(d, MGL_EGL_SURFACE_PBUFFER);
    if (s != NULL) {
        s->width = width;
        s->height = height;
        s->texture_format = texture_format;
        s->texture_target = texture_target;
        s->mipmap_texture = mipmap;
        s->largest_pbuffer = largest;
    }
    pthread_mutex_unlock(&egl_lock);
    if (s == NULL) {
        set_error(EGL_BAD_ALLOC);
        return EGL_NO_SURFACE;
    }
    return (EGLSurface)s;
}

static int native_window_surface_exists_locked(struct mgl_egl_display *d,
                                               EGLNativeWindowType window) {
    const struct mgl_egl_surface *s;
    for (s = surfaces; s != NULL; s = s->next) {
        if (s->display == d && s->kind == MGL_EGL_SURFACE_WINDOW && s->native_window == window)
            return 1;
    }
    return 0;
}

MGL_EXPORT EGLSurface eglCreateWindowSurface(EGLDisplay display, EGLConfig config,
                                             EGLNativeWindowType window, const EGLint *attribs) {
    struct mgl_egl_display *d;
    struct mgl_egl_surface *s;
    XWindowAttributes wa;
    const EGLint *p;
    EGLint render_buffer = EGL_BACK_BUFFER;
    if (attribs != NULL)
        for (p = attribs; *p != EGL_NONE; p += 2) {
            if (p[0] == EGL_GL_COLORSPACE) {
                if (p[1] == EGL_GL_COLORSPACE_SRGB) {
                    set_error(EGL_BAD_MATCH);
                    return EGL_NO_SURFACE;
                }
                if (p[1] != EGL_GL_COLORSPACE_LINEAR) {
                    set_error(EGL_BAD_ATTRIBUTE);
                    return EGL_NO_SURFACE;
                }
                continue;
            }
            if (p[0] != EGL_RENDER_BUFFER ||
                (p[1] != EGL_BACK_BUFFER && p[1] != EGL_SINGLE_BUFFER)) {
                set_error(EGL_BAD_ATTRIBUTE);
                return EGL_NO_SURFACE;
            }
            render_buffer = p[1];
        }
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_NO_SURFACE;
    }
    if (d->platform != EGL_PLATFORM_X11_KHR || window == NULL) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_NATIVE_WINDOW);
        return EGL_NO_SURFACE;
    }
    if (config != (EGLConfig)&d->config) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_CONFIG);
        return EGL_NO_SURFACE;
    }
    if (!XGetWindowAttributes((Display *)d->native_display, (Window)(uintptr_t)window, &wa)) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_NATIVE_WINDOW);
        return EGL_NO_SURFACE;
    }
    if (native_window_surface_exists_locked(d, window)) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_ALLOC);
        return EGL_NO_SURFACE;
    }
    s = alloc_surface_locked(d, MGL_EGL_SURFACE_WINDOW);
    if (s != NULL) {
        s->native_window = window;
        s->width = wa.width;
        s->height = wa.height;
        s->render_buffer = render_buffer;
    }
    pthread_mutex_unlock(&egl_lock);
    if (s == NULL) {
        set_error(EGL_BAD_ALLOC);
        return EGL_NO_SURFACE;
    }
    return (EGLSurface)s;
}

static EGLSurface create_platform_window_surface_common(EGLDisplay display, EGLConfig config,
                                                        void *native_window,
                                                        const EGLint *attribs) {
    struct mgl_egl_display *d;
    Window window;
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_NO_SURFACE;
    }
    if (d->platform != EGL_PLATFORM_X11_KHR || native_window == NULL) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_NATIVE_WINDOW);
        return EGL_NO_SURFACE;
    }
    window = *(const Window *)native_window;
    pthread_mutex_unlock(&egl_lock);
    if (window == None) {
        set_error(EGL_BAD_NATIVE_WINDOW);
        return EGL_NO_SURFACE;
    }
    return eglCreateWindowSurface(display, config, (EGLNativeWindowType)(uintptr_t)window, attribs);
}

MGL_EXPORT EGLSurface eglCreatePlatformWindowSurface(EGLDisplay d, EGLConfig c, void *w,
                                                     const EGLAttrib *a) {
    EGLint attrs[3] = {EGL_NONE, EGL_NONE, EGL_NONE};
    EGLint render_buffer = EGL_BACK_BUFFER;
    int render_buffer_specified = 0;
    const EGLAttrib *p;
    if (a != NULL)
        for (p = a; *p != EGL_NONE; p += 2) {
            if (p[0] != EGL_RENDER_BUFFER ||
                (p[1] != EGL_BACK_BUFFER && p[1] != EGL_SINGLE_BUFFER)) {
                set_error(EGL_BAD_ATTRIBUTE);
                return EGL_NO_SURFACE;
            }
            render_buffer = (EGLint)p[1];
            render_buffer_specified = 1;
        }
    if (render_buffer_specified) {
        attrs[0] = EGL_RENDER_BUFFER;
        attrs[1] = render_buffer;
        attrs[2] = EGL_NONE;
    }
    return create_platform_window_surface_common(d, c, w, attrs);
}

MGL_EXPORT EGLSurface eglCreatePlatformWindowSurfaceEXT(EGLDisplay d, EGLConfig c, void *w,
                                                        const EGLint *a) {
    return create_platform_window_surface_common(d, c, w, a);
}

MGL_EXPORT int wine_android_register_egl_window(void *display_handle, void *surface_handle,
                                                uint64_t hwnd, uint32_t whole_window) {
    struct mgl_egl_display *d;
    struct mgl_egl_surface *s;
    struct mgl_android_present *present;
    if (display_handle == NULL || surface_handle == NULL || hwnd == 0u)
        return 0;
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked((EGLDisplay)display_handle);
    s = find_surface_locked((EGLSurface)surface_handle);
    if (d == NULL || !d->initialized || s == NULL || s->display != d ||
        d->platform != EGL_PLATFORM_X11_KHR || s->kind != MGL_EGL_SURFACE_WINDOW ||
        s->native_window == NULL || s->android_present != NULL) {
        pthread_mutex_unlock(&egl_lock);
        return 0;
    }
    present = bridge()->android_present_create(
        (Display *)d->native_display, (Drawable)(uintptr_t)s->native_window, hwnd, whole_window);
    if (present == NULL) {
        pthread_mutex_unlock(&egl_lock);
        return 0;
    }
    s->android_present = present;
    s->swap_interval = 0;
    pthread_mutex_unlock(&egl_lock);
    return 1;
}

MGL_EXPORT EGLBoolean eglDestroySurface(EGLDisplay display, EGLSurface surface) {
    struct mgl_egl_display *d;
    struct mgl_egl_surface *s;
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    s = find_surface_locked(surface);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    if (s == NULL || s->display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_SURFACE);
        return EGL_FALSE;
    }
    s->destroy_pending = 1;
    surface_release_locked(s);
    pthread_mutex_unlock(&egl_lock);
    return EGL_TRUE;
}

static int refresh_window_surface_locked(struct mgl_egl_surface *s) {
    XWindowAttributes wa;
    if (s == NULL || s->kind != MGL_EGL_SURFACE_WINDOW)
        return 1;
    if (!XGetWindowAttributes((Display *)s->display->native_display,
                              (Window)(uintptr_t)s->native_window, &wa))
        return 0;
    if (wa.width <= 0 || wa.height <= 0)
        return 0;
    s->width = wa.width;
    s->height = wa.height;
    return 1;
}

MGL_EXPORT EGLBoolean eglQuerySurface(EGLDisplay display, EGLSurface surface, EGLint attr,
                                      EGLint *value) {
    struct mgl_egl_display *d;
    struct mgl_egl_surface *s;
    if (value == NULL) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    s = find_surface_locked(surface);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    if (s == NULL || s->display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_SURFACE);
        return EGL_FALSE;
    }
    if ((attr == EGL_WIDTH || attr == EGL_HEIGHT) && !refresh_window_surface_locked(s)) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_NATIVE_WINDOW);
        return EGL_FALSE;
    }
    switch (attr) {
        case EGL_CONFIG_ID:
            *value = 1;
            break;
        case EGL_GL_COLORSPACE:
            *value = s->gl_colorspace;
            break;
        case EGL_WIDTH:
            *value = s->width;
            break;
        case EGL_HEIGHT:
            *value = s->height;
            break;
        case EGL_HORIZONTAL_RESOLUTION:
        case EGL_VERTICAL_RESOLUTION:
        case EGL_PIXEL_ASPECT_RATIO:
            *value = EGL_UNKNOWN;
            break;
        case EGL_RENDER_BUFFER:
            *value = s->render_buffer;
            break;
        case EGL_MULTISAMPLE_RESOLVE:
            *value = s->multisample_resolve;
            break;
        case EGL_SWAP_BEHAVIOR:
            *value = s->swap_behavior;
            break;
        case EGL_LARGEST_PBUFFER:
            if (s->kind == MGL_EGL_SURFACE_PBUFFER)
                *value = (EGLint)s->largest_pbuffer;
            break;
        case EGL_TEXTURE_FORMAT:
            if (s->kind == MGL_EGL_SURFACE_PBUFFER)
                *value = s->texture_format;
            break;
        case EGL_TEXTURE_TARGET:
            if (s->kind == MGL_EGL_SURFACE_PBUFFER)
                *value = s->texture_target;
            break;
        case EGL_MIPMAP_TEXTURE:
            if (s->kind == MGL_EGL_SURFACE_PBUFFER)
                *value = (EGLint)s->mipmap_texture;
            break;
        case EGL_MIPMAP_LEVEL:
            if (s->kind == MGL_EGL_SURFACE_PBUFFER)
                *value = s->mipmap_level;
            break;
        default:
            pthread_mutex_unlock(&egl_lock);
            set_error(EGL_BAD_ATTRIBUTE);
            return EGL_FALSE;
    }
    pthread_mutex_unlock(&egl_lock);
    return EGL_TRUE;
}

MGL_EXPORT EGLBoolean eglBindAPI(EGLenum api) {
    if (api != EGL_OPENGL_API) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    tls.api = api;
    return EGL_TRUE;
}

MGL_EXPORT EGLenum eglQueryAPI(void) {
    return tls.api;
}

MGL_EXPORT EGLContext eglCreateContext(EGLDisplay display, EGLConfig config,
                                       EGLContext share_handle, const EGLint *attribs) {
    struct mgl_egl_display *d;
    struct mgl_egl_context *share = NULL, *c;
    const EGLint *p;
    EGLint major = 1, minor = 0, flags = 0, profile = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
    EGLint reset_strategy = EGL_NO_RESET_NOTIFICATION_KHR;
    EGLBoolean no_error = EGL_FALSE;
    int reset_strategy_specified = 0;
    const EGLint known_flags = EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR |
                               EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR |
                               EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR;
    const EGLint known_profiles =
        EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR | EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR;

    if (tls.api != EGL_OPENGL_API) {
        set_error(EGL_BAD_MATCH);
        return EGL_NO_CONTEXT;
    }
    if (attribs != NULL) {
        for (p = attribs; *p != EGL_NONE; p += 2) {
            switch (p[0]) {
                case EGL_CONTEXT_MAJOR_VERSION:
                    major = p[1];
                    break;
                case EGL_CONTEXT_MINOR_VERSION:
                    minor = p[1];
                    break;
                case EGL_CONTEXT_FLAGS_KHR:
                    flags = p[1];
                    break;
                case EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR:
                    profile = p[1];
                    break;
                case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR:
                    if (p[1] != EGL_NO_RESET_NOTIFICATION_KHR &&
                        p[1] != EGL_LOSE_CONTEXT_ON_RESET_KHR) {
                        set_error(EGL_BAD_ATTRIBUTE);
                        return EGL_NO_CONTEXT;
                    }
                    reset_strategy = p[1];
                    reset_strategy_specified = 1;
                    break;
                case EGL_CONTEXT_OPENGL_NO_ERROR_KHR:
                    if (p[1] != EGL_FALSE && p[1] != EGL_TRUE) {
                        set_error(EGL_BAD_ATTRIBUTE);
                        return EGL_NO_CONTEXT;
                    }
                    no_error = (EGLBoolean)p[1];
                    break;
                default:
                    set_error(EGL_BAD_ATTRIBUTE);
                    return EGL_NO_CONTEXT;
            }
        }
    }
    if ((flags & ~known_flags) != 0 ||
        ((major > 3 || (major == 3 && minor >= 2)) && (profile & ~known_profiles) != 0)) {
        set_error(EGL_BAD_ATTRIBUTE);
        return EGL_NO_CONTEXT;
    }
    if (!mgl_opengl_version_supported(major, minor)) {
        set_error(EGL_BAD_MATCH);
        return EGL_NO_CONTEXT;
    }
    /* The debug-context flag is supported through KHR_debug. Requests for forward-compatible or
     * robust-access contexts are rejected. */
    if ((flags & (EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR |
                  EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR)) != 0) {
        set_error(EGL_BAD_MATCH);
        return EGL_NO_CONTEXT;
    }
    if (no_error == EGL_TRUE && (flags & EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR) != 0) {
        set_error(EGL_BAD_MATCH);
        return EGL_NO_CONTEXT;
    }
    /* Reset-notification requests require GL_ARB_robustness support. Valid KHR reset-strategy
     * values are rejected with EGL_BAD_MATCH. */
    if (reset_strategy_specified) {
        (void)reset_strategy;
        set_error(EGL_BAD_MATCH);
        return EGL_NO_CONTEXT;
    }
    if (major > 3 || (major == 3 && minor >= 2)) {
        if (profile != EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR &&
            profile != EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR) {
            set_error(EGL_BAD_MATCH);
            return EGL_NO_CONTEXT;
        }
    } else if (major == 3 && minor == 1) {
        profile = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
    } else {
        profile = 0;
    }

    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_NO_CONTEXT;
    }
    if (config != EGL_NO_CONFIG_KHR && config != (EGLConfig)&d->config) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_CONFIG);
        return EGL_NO_CONTEXT;
    }
    if (share_handle != EGL_NO_CONTEXT) {
        share = find_context_locked(share_handle);
        if (share == NULL || share->display != d) {
            pthread_mutex_unlock(&egl_lock);
            set_error(EGL_BAD_CONTEXT);
            return EGL_NO_CONTEXT;
        }
        if (share->no_error != no_error) {
            pthread_mutex_unlock(&egl_lock);
            set_error(EGL_BAD_MATCH);
            return EGL_NO_CONTEXT;
        }
    }
    pthread_mutex_unlock(&egl_lock);

    c = calloc(1, sizeof(*c));
    if (c == NULL) {
        set_error(EGL_BAD_ALLOC);
        return EGL_NO_CONTEXT;
    }
    c->core = bridge()->context_create_offscreen(bridge()->runtime_native_dispatch(),
                                                 share ? share->core : NULL, 1, 1);
    if (c->core == NULL) {
        free(c);
        set_error(EGL_BAD_ALLOC);
        return EGL_NO_CONTEXT;
    }
    c->magic = MGL_EGL_CONTEXT_MAGIC;
    c->display = d;
    c->major = major;
    c->minor = minor;
    c->flags = flags;
    c->profile_mask = profile;
    bridge()->context_set_profile_mask(c->core, profile == EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR
                                                    ? GL_CONTEXT_CORE_PROFILE_BIT
                                                    : GL_CONTEXT_COMPATIBILITY_PROFILE_BIT);
    c->config_id = config == EGL_NO_CONFIG_KHR ? 0 : 1;
    c->render_buffer = EGL_NONE;
    c->no_error = no_error;
    c->refs = 1u;
    bridge()->context_set_no_error(c->core, no_error == EGL_TRUE);
    bridge()->context_set_creation_flags(
        c->core,
        ((flags & EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR) != 0 ? GL_CONTEXT_FLAG_DEBUG_BIT : 0u) |
            (no_error == EGL_TRUE ? GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR : 0u));
    pthread_mutex_lock(&egl_lock);
    c->next = contexts;
    contexts = c;
    pthread_mutex_unlock(&egl_lock);
    return (EGLContext)c;
}

MGL_EXPORT EGLBoolean eglDestroyContext(EGLDisplay display, EGLContext context) {
    struct mgl_egl_display *d;
    struct mgl_egl_context *c;
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    c = find_context_locked(context);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    if (c == NULL || c->display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_CONTEXT);
        return EGL_FALSE;
    }
    c->destroy_pending = 1;
    context_release_locked(c);
    pthread_mutex_unlock(&egl_lock);
    return EGL_TRUE;
}

static void release_tls_bindings_locked(void) {
    struct mgl_egl_context *oldc = tls.context;
    struct mgl_egl_surface *oldd = tls.draw, *oldr = tls.read;
    tls.display = NULL;
    tls.context = NULL;
    tls.draw = NULL;
    tls.read = NULL;
    if (oldc != NULL) {
        if (oldc->current_count > 0u)
            --oldc->current_count;
        if (oldc->current_count == 0u)
            oldc->render_buffer = EGL_NONE;
        context_release_locked(oldc);
    }
    if (oldd != NULL) {
        if (oldd->current_count > 0u)
            --oldd->current_count;
        surface_release_locked(oldd);
    }
    if (oldr != NULL && oldr != oldd) {
        if (oldr->current_count > 0u)
            --oldr->current_count;
        surface_release_locked(oldr);
    }
}

static EGLBoolean release_current_opengl_binding(void) {
    if (tls.context == NULL)
        return EGL_TRUE;
    if (!bridge()->context_make_current(NULL)) {
        set_error(EGL_BAD_ACCESS);
        return EGL_FALSE;
    }
    pthread_mutex_lock(&egl_lock);
    release_tls_bindings_locked();
    pthread_mutex_unlock(&egl_lock);
    return EGL_TRUE;
}

static int surface_current_elsewhere_locked(struct mgl_egl_surface *s) {
    if (s == NULL || s->current_count == 0u)
        return 0;
    return tls.draw != s && tls.read != s;
}

MGL_EXPORT EGLBoolean eglMakeCurrent(EGLDisplay display, EGLSurface draw_handle,
                                     EGLSurface read_handle, EGLContext context_handle) {
    struct mgl_egl_display *d;
    struct mgl_egl_surface *draw = NULL, *read = NULL;
    struct mgl_egl_context *c = NULL;
    int dw = 1, dh = 1, rw = 1, rh = 1;
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (d == NULL) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_DISPLAY);
        return EGL_FALSE;
    }
    if (context_handle == EGL_NO_CONTEXT) {
        if (draw_handle != EGL_NO_SURFACE || read_handle != EGL_NO_SURFACE) {
            pthread_mutex_unlock(&egl_lock);
            set_error(EGL_BAD_MATCH);
            return EGL_FALSE;
        }
        pthread_mutex_unlock(&egl_lock);
        /* EGL_NONE has no current rendering API to release; a hidden OpenGL binding survives until
         * reselected or eglReleaseThread. */
        if (tls.api != EGL_OPENGL_API)
            return EGL_TRUE;
        return release_current_opengl_binding();
    }
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    c = find_context_locked(context_handle);
    if (c == NULL || c->display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_CONTEXT);
        return EGL_FALSE;
    }
    if (c->current_count > 0u && tls.context != c) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_ACCESS);
        return EGL_FALSE;
    }
    if ((draw_handle == EGL_NO_SURFACE) != (read_handle == EGL_NO_SURFACE)) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_MATCH);
        return EGL_FALSE;
    }
    if (draw_handle != EGL_NO_SURFACE) {
        draw = find_surface_locked(draw_handle);
        read = find_surface_locked(read_handle);
        if (draw == NULL || read == NULL || draw->display != d || read->display != d) {
            pthread_mutex_unlock(&egl_lock);
            set_error(EGL_BAD_SURFACE);
            return EGL_FALSE;
        }
        if (surface_current_elsewhere_locked(draw) ||
            (read != draw && surface_current_elsewhere_locked(read))) {
            pthread_mutex_unlock(&egl_lock);
            set_error(EGL_BAD_ACCESS);
            return EGL_FALSE;
        }
        dw = draw->width;
        dh = draw->height;
        rw = read->width;
        rh = read->height;
    }
    pthread_mutex_unlock(&egl_lock);
    if (draw != NULL && draw->kind == MGL_EGL_SURFACE_WINDOW &&
        (draw != read || draw->android_present == NULL)) {
        set_error(EGL_BAD_MATCH);
        return EGL_FALSE;
    }
    if (draw != NULL && draw == read && draw->android_present != NULL) {
        if (!bridge()->android_present_configure(draw->android_present, c->core, dw, dh)) {
            set_error(EGL_BAD_ACCESS);
            return EGL_FALSE;
        }
    } else if (!bridge()->context_configure_pbuffers(c->core, dw, dh, rw, rh)) {
        set_error(EGL_BAD_ACCESS);
        return EGL_FALSE;
    }
    if (!bridge()->context_make_current(c->core)) {
        set_error(EGL_BAD_ACCESS);
        return EGL_FALSE;
    }
    if (draw != NULL && draw == read && draw->android_present != NULL)
        (void)bridge()->android_present_set_swap_interval(draw->android_present, c->core,
                                                          (int)draw->swap_interval);
    if (!c->ever_current) {
        const GLsizei initial_width = draw != NULL ? dw : 0;
        const GLsizei initial_height = draw != NULL ? dh : 0;
        glViewport(0, 0, initial_width, initial_height);
        glScissor(0, 0, initial_width, initial_height);
    }
    pthread_mutex_lock(&egl_lock);
    release_tls_bindings_locked();
    tls.display = d;
    tls.context = c;
    tls.draw = draw;
    tls.read = read;
    context_retain_locked(c);
    ++c->current_count;
    c->ever_current = EGL_TRUE;
    /* The context reports EGL_BACK_BUFFER with a bound draw surface and EGL_NONE without one.
     * The renderer uses a back-buffer presentation path. */
    c->render_buffer = draw != NULL ? EGL_BACK_BUFFER : EGL_NONE;
    if (draw != NULL) {
        surface_retain_locked(draw);
        ++draw->current_count;
    }
    if (read != NULL && read != draw) {
        surface_retain_locked(read);
        ++read->current_count;
    }
    pthread_mutex_unlock(&egl_lock);
    return EGL_TRUE;
}

MGL_EXPORT EGLContext eglGetCurrentContext(void) {
    return tls.api == EGL_OPENGL_API ? (EGLContext)tls.context : EGL_NO_CONTEXT;
}

MGL_EXPORT EGLDisplay eglGetCurrentDisplay(void) {
    return tls.api == EGL_OPENGL_API ? (EGLDisplay)tls.display : EGL_NO_DISPLAY;
}

MGL_EXPORT EGLSurface eglGetCurrentSurface(EGLint which) {
    if (which != EGL_DRAW && which != EGL_READ) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_NO_SURFACE;
    }
    if (tls.api != EGL_OPENGL_API || tls.context == NULL)
        return EGL_NO_SURFACE;
    return which == EGL_DRAW ? (EGLSurface)tls.draw : (EGLSurface)tls.read;
}

MGL_EXPORT EGLBoolean eglQueryContext(EGLDisplay display, EGLContext context, EGLint attr,
                                      EGLint *value) {
    struct mgl_egl_display *d;
    struct mgl_egl_context *c;
    EGLint result;
    if (value == NULL) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    c = find_context_locked(context);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    if (c == NULL || c->display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_CONTEXT);
        return EGL_FALSE;
    }
    switch (attr) {
        case EGL_CONFIG_ID:
            result = c->config_id;
            break;
        case EGL_CONTEXT_MAJOR_VERSION:
            result = c->major;
            break;
        case EGL_CONTEXT_MINOR_VERSION:
            result = c->minor;
            break;
        case EGL_CONTEXT_FLAGS_KHR:
            result = c->flags;
            break;
        case EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR:
            result = c->profile_mask;
            break;
        case EGL_CONTEXT_CLIENT_TYPE:
            result = EGL_OPENGL_API;
            break;
        case EGL_RENDER_BUFFER:
            result = c->render_buffer;
            break;
        default:
            pthread_mutex_unlock(&egl_lock);
            set_error(EGL_BAD_ATTRIBUTE);
            return EGL_FALSE;
    }
    *value = result;
    pthread_mutex_unlock(&egl_lock);
    return EGL_TRUE;
}

MGL_EXPORT EGLBoolean eglSwapInterval(EGLDisplay display, EGLint interval) {
    struct mgl_egl_display *d;
    struct mgl_egl_context *c;
    struct mgl_android_present *present = NULL;
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    if (tls.api != EGL_OPENGL_API || tls.context == NULL || tls.display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_CONTEXT);
        return EGL_FALSE;
    }
    if (tls.draw == NULL) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_SURFACE);
        return EGL_FALSE;
    }
    if (interval < 0)
        interval = 0;
    if (interval > 4)
        interval = 4;
    tls.draw->swap_interval = interval;
    c = tls.context;
    if (tls.read == tls.draw)
        present = tls.draw->android_present;
    pthread_mutex_unlock(&egl_lock);
    if (present != NULL)
        (void)bridge()->android_present_set_swap_interval(present, c->core, interval);
    return EGL_TRUE;
}

MGL_EXPORT EGLBoolean eglSwapBuffers(EGLDisplay display, EGLSurface surface_handle) {
    struct mgl_egl_display *d;
    struct mgl_egl_surface *s;
    struct mgl_egl_context *c = tls.context;
    int draw_w, draw_h;

    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    s = find_surface_locked(surface_handle);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    if (s == NULL || s->display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_SURFACE);
        return EGL_FALSE;
    }
    if (tls.api != EGL_OPENGL_API || c == NULL || tls.display != d || tls.draw != s) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_SURFACE);
        return EGL_FALSE;
    }
    if (s->kind == MGL_EGL_SURFACE_PBUFFER) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_TRUE;
    }
    if (s->android_present == NULL || tls.read != s) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_SURFACE);
        return EGL_FALSE;
    }
    if (!refresh_window_surface_locked(s)) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_NATIVE_WINDOW);
        return EGL_FALSE;
    }
    draw_w = s->width;
    draw_h = s->height;
    if (s->android_present != NULL && tls.read == s) {
        struct mgl_android_present *present = s->android_present;
        pthread_mutex_unlock(&egl_lock);
        if (!bridge()->android_present_swap(present, c->core, draw_w, draw_h)) {
            set_error(EGL_BAD_SURFACE);
            return EGL_FALSE;
        }
        return EGL_TRUE;
    }
    pthread_mutex_unlock(&egl_lock);

    set_error(EGL_BAD_SURFACE);
    return EGL_FALSE;
}

MGL_EXPORT EGLBoolean eglReleaseThread(void) {
    if (tls.context != NULL && !release_current_opengl_binding())
        return EGL_FALSE;
    tls.api = EGL_NONE;
    tls.error = EGL_SUCCESS;
    return EGL_TRUE;
}

static int current_surface_invalid_locked(void) {
    return (tls.draw != NULL && tls.draw->destroy_pending) ||
           (tls.read != NULL && tls.read->destroy_pending);
}

MGL_EXPORT EGLBoolean eglWaitClient(void) {
    if (tls.api != EGL_OPENGL_API)
        return EGL_TRUE;
    pthread_mutex_lock(&egl_lock);
    if (tls.context == NULL) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_TRUE;
    }
    if (current_surface_invalid_locked()) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_CURRENT_SURFACE);
        return EGL_FALSE;
    }
    pthread_mutex_unlock(&egl_lock);

    glFinish();
    return EGL_TRUE;
}

MGL_EXPORT EGLBoolean eglWaitGL(void) {

    return EGL_TRUE;
}

MGL_EXPORT EGLBoolean eglWaitNative(EGLint engine) {
    if (engine != EGL_CORE_NATIVE_ENGINE) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    if (tls.api != EGL_OPENGL_API)
        return EGL_TRUE;
    pthread_mutex_lock(&egl_lock);
    if (tls.context == NULL) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_TRUE;
    }
    if (current_surface_invalid_locked()) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_CURRENT_SURFACE);
        return EGL_FALSE;
    }
    pthread_mutex_unlock(&egl_lock);
    /* The single config reports EGL_NATIVE_RENDERABLE as EGL_FALSE. eglWaitNative succeeds
     * without issuing native-rendering operations. */
    return EGL_TRUE;
}

MGL_EXPORT EGLBoolean eglBindTexImage(EGLDisplay display, EGLSurface surface, EGLint buffer) {
    struct mgl_egl_display *d;
    struct mgl_egl_surface *s;
    (void)buffer;
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    s = find_surface_locked(surface);
    if (s == NULL || s->display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_SURFACE);
        return EGL_FALSE;
    }
    pthread_mutex_unlock(&egl_lock);
    /* The desktop OpenGL interface rejects EGL pbuffer render-to-texture requests. */
    set_error(EGL_BAD_SURFACE);
    return EGL_FALSE;
}

MGL_EXPORT EGLBoolean eglCopyBuffers(EGLDisplay display, EGLSurface surface,
                                     EGLNativePixmapType target) {
    struct mgl_egl_display *d;
    struct mgl_egl_surface *s;
    (void)target;
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    s = find_surface_locked(surface);
    if (s == NULL || s->display != d || tls.api != EGL_OPENGL_API || tls.context == NULL ||
        tls.display != d || tls.draw != s) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_SURFACE);
        return EGL_FALSE;
    }
    pthread_mutex_unlock(&egl_lock);
    /* eglCopyBuffers reports EGL_BAD_NATIVE_PIXMAP when native pixmap copying is unavailable. */
    set_error(EGL_BAD_NATIVE_PIXMAP);
    return EGL_FALSE;
}

MGL_EXPORT EGLBoolean eglDestroyImage(EGLDisplay d, EGLImage i) {
    (void)d;
    (void)i;
    set_error(EGL_BAD_MATCH);
    return EGL_FALSE;
}

MGL_EXPORT EGLBoolean eglDestroySync(EGLDisplay display, EGLSync handle) {
    struct mgl_egl_display *d;
    struct mgl_egl_sync *sync;
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    sync = find_sync_locked(handle);
    if (sync == NULL || sync->display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    unlink_sync_locked(sync);
    pthread_mutex_unlock(&egl_lock);
    return EGL_TRUE;
}

MGL_EXPORT EGLBoolean eglGetSyncAttrib(EGLDisplay display, EGLSync handle, EGLint attribute,
                                       EGLAttrib *value) {
    struct mgl_egl_display *d;
    struct mgl_egl_sync *sync;
    EGLAttrib out;
    if (value == NULL) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    sync = find_sync_locked(handle);
    if (sync == NULL || sync->display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    switch (attribute) {
        case EGL_SYNC_TYPE:
            out = (EGLAttrib)sync->type;
            break;
        case EGL_SYNC_STATUS:
            out = sync->status;
            break;
        case EGL_SYNC_CONDITION:
            out = sync->condition;
            break;
        default:
            pthread_mutex_unlock(&egl_lock);
            set_error(EGL_BAD_ATTRIBUTE);
            return EGL_FALSE;
    }
    pthread_mutex_unlock(&egl_lock);
    *value = out;
    return EGL_TRUE;
}

MGL_EXPORT EGLBoolean eglReleaseTexImage(EGLDisplay display, EGLSurface surface, EGLint buffer) {
    struct mgl_egl_display *d;
    struct mgl_egl_surface *s;
    (void)buffer;
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    s = find_surface_locked(surface);
    if (s == NULL || s->display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_SURFACE);
        return EGL_FALSE;
    }
    pthread_mutex_unlock(&egl_lock);

    set_error(EGL_BAD_SURFACE);
    return EGL_FALSE;
}

MGL_EXPORT EGLBoolean eglSurfaceAttrib(EGLDisplay display, EGLSurface surface, EGLint attr,
                                       EGLint value) {
    struct mgl_egl_display *d;
    struct mgl_egl_surface *s;
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    s = find_surface_locked(surface);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    if (s == NULL || s->display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_SURFACE);
        return EGL_FALSE;
    }
    switch (attr) {
        case EGL_MIPMAP_LEVEL:
            /* EGL_MIPMAP_LEVEL is rejected with EGL_BAD_PARAMETER for this OpenGL-only surface. */
            pthread_mutex_unlock(&egl_lock);
            set_error(EGL_BAD_PARAMETER);
            return EGL_FALSE;
        case EGL_MULTISAMPLE_RESOLVE:
            if (value == EGL_MULTISAMPLE_RESOLVE_BOX) {
                pthread_mutex_unlock(&egl_lock);
                set_error(EGL_BAD_MATCH);
                return EGL_FALSE;
            }
            if (value != EGL_MULTISAMPLE_RESOLVE_DEFAULT) {
                pthread_mutex_unlock(&egl_lock);
                set_error(EGL_BAD_ATTRIBUTE);
                return EGL_FALSE;
            }
            s->multisample_resolve = value;
            break;
        case EGL_SWAP_BEHAVIOR:
            if (value == EGL_BUFFER_PRESERVED) {
                pthread_mutex_unlock(&egl_lock);
                set_error(EGL_BAD_MATCH);
                return EGL_FALSE;
            }
            if (value != EGL_BUFFER_DESTROYED) {
                pthread_mutex_unlock(&egl_lock);
                set_error(EGL_BAD_ATTRIBUTE);
                return EGL_FALSE;
            }
            s->swap_behavior = value;
            break;
        default:
            pthread_mutex_unlock(&egl_lock);
            set_error(EGL_BAD_ATTRIBUTE);
            return EGL_FALSE;
    }
    pthread_mutex_unlock(&egl_lock);
    return EGL_TRUE;
}

MGL_EXPORT EGLBoolean eglWaitSync(EGLDisplay display, EGLSync handle, EGLint flags) {
    struct mgl_egl_display *d;
    struct mgl_egl_sync *sync;
    if (flags != 0) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    sync = find_sync_locked(handle);
    if (sync == NULL || sync->display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    if (tls.api != EGL_OPENGL_API || tls.context == NULL) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_MATCH);
        return EGL_FALSE;
    }
    pthread_mutex_unlock(&egl_lock);
    /* The current implementation completes fence syncs before eglCreateSync
     * returns. A server wait on an already-signaled fence is therefore a no-op. */
    return EGL_TRUE;
}

MGL_EXPORT EGLint eglClientWaitSync(EGLDisplay display, EGLSync handle, EGLint flags,
                                    EGLTime timeout) {
    struct mgl_egl_display *d;
    struct mgl_egl_sync *sync;
    (void)timeout;
    if ((flags & ~(EGLint)EGL_SYNC_FLUSH_COMMANDS_BIT) != 0) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_FALSE;
    }
    sync = find_sync_locked(handle);
    if (sync == NULL || sync->display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    /* Synchronous fence lowering guarantees this status before the handle is
     * published. EGL_SYNC_FLUSH_COMMANDS_BIT only flushes unsignaled syncs. */
    if (sync->status == EGL_SIGNALED) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_CONDITION_SATISFIED;
    }
    pthread_mutex_unlock(&egl_lock);
    return timeout == 0u ? EGL_TIMEOUT_EXPIRED : EGL_CONDITION_SATISFIED;
}

MGL_EXPORT EGLImage eglCreateImage(EGLDisplay d, EGLContext c, EGLenum t, EGLClientBuffer b,
                                   const EGLAttrib *a) {
    (void)d;
    (void)c;
    (void)t;
    (void)b;
    (void)a;
    set_error(EGL_BAD_MATCH);
    return EGL_NO_IMAGE;
}

MGL_EXPORT EGLSurface eglCreatePbufferFromClientBuffer(EGLDisplay display, EGLenum buftype,
                                                       EGLClientBuffer buffer, EGLConfig config,
                                                       const EGLint *attribs) {
    struct mgl_egl_display *d;
    (void)buffer;
    (void)attribs;
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_NO_SURFACE;
    }
    if (config != (EGLConfig)&d->config) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_CONFIG);
        return EGL_NO_SURFACE;
    }
    pthread_mutex_unlock(&egl_lock);
    if (buftype != EGL_OPENVG_IMAGE) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_NO_SURFACE;
    }
    /* The public interface has no OpenVG context for client-buffer pbuffers. */
    set_error(EGL_BAD_ACCESS);
    return EGL_NO_SURFACE;
}

MGL_EXPORT EGLSurface eglCreatePixmapSurface(EGLDisplay display, EGLConfig config,
                                             EGLNativePixmapType pixmap, const EGLint *attribs) {
    struct mgl_egl_display *d;
    if (attribs != NULL && attribs[0] != EGL_NONE) {
        set_error(EGL_BAD_ATTRIBUTE);
        return EGL_NO_SURFACE;
    }
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_NO_SURFACE;
    }
    if (config != (EGLConfig)&d->config) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_CONFIG);
        return EGL_NO_SURFACE;
    }
    if (d->platform == EGL_PLATFORM_SURFACELESS_MESA || pixmap == NULL) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_NATIVE_PIXMAP);
        return EGL_NO_SURFACE;
    }
    pthread_mutex_unlock(&egl_lock);
    /* The single public config lacks EGL_PIXMAP_BIT, so pixmap surfaces cannot be created. */
    set_error(EGL_BAD_MATCH);
    return EGL_NO_SURFACE;
}

static EGLSurface create_platform_pixmap_surface_common(EGLDisplay display, EGLConfig config,
                                                        void *native_pixmap,
                                                        const EGLint *attribs) {
    struct mgl_egl_display *d;
    Pixmap pixmap;
    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_NO_SURFACE;
    }
    if (d->platform != EGL_PLATFORM_X11_KHR || native_pixmap == NULL) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_NATIVE_PIXMAP);
        return EGL_NO_SURFACE;
    }
    pixmap = *(const Pixmap *)native_pixmap;
    pthread_mutex_unlock(&egl_lock);
    if (pixmap == None) {
        set_error(EGL_BAD_NATIVE_PIXMAP);
        return EGL_NO_SURFACE;
    }
    return eglCreatePixmapSurface(display, config, (EGLNativePixmapType)(uintptr_t)pixmap, attribs);
}

MGL_EXPORT EGLSurface eglCreatePlatformPixmapSurface(EGLDisplay d, EGLConfig c, void *p,
                                                     const EGLAttrib *a) {
    if (a != NULL && a[0] != EGL_NONE) {
        set_error(EGL_BAD_ATTRIBUTE);
        return EGL_NO_SURFACE;
    }
    return create_platform_pixmap_surface_common(d, c, p, NULL);
}

MGL_EXPORT EGLSurface eglCreatePlatformPixmapSurfaceEXT(EGLDisplay d, EGLConfig c, void *p,
                                                        const EGLint *a) {
    if (a != NULL && a[0] != EGL_NONE) {
        set_error(EGL_BAD_ATTRIBUTE);
        return EGL_NO_SURFACE;
    }
    return create_platform_pixmap_surface_common(d, c, p, a);
}

MGL_EXPORT EGLSync eglCreateSync(EGLDisplay display, EGLenum type, const EGLAttrib *attribs) {
    struct mgl_egl_display *d;
    struct mgl_egl_context *c;
    struct mgl_egl_sync *sync;

    pthread_mutex_lock(&egl_lock);
    d = find_display_locked(display);
    if (!display_ready_locked(d)) {
        pthread_mutex_unlock(&egl_lock);
        return EGL_NO_SYNC;
    }
    if (type != EGL_SYNC_FENCE) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_PARAMETER);
        return EGL_NO_SYNC;
    }
    if (attribs != NULL && attribs[0] != EGL_NONE) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_ATTRIBUTE);
        return EGL_NO_SYNC;
    }
    c = tls.context;
    if (tls.api != EGL_OPENGL_API || c == NULL || tls.display != d) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_MATCH);
        return EGL_NO_SYNC;
    }

    sync = calloc(1, sizeof(*sync));
    if (sync == NULL) {
        pthread_mutex_unlock(&egl_lock);
        set_error(EGL_BAD_ALLOC);
        return EGL_NO_SYNC;
    }
    sync->magic = MGL_EGL_SYNC_MAGIC;
    sync->display = d;
    sync->type = EGL_SYNC_FENCE;
    sync->status = EGL_UNSIGNALED;
    sync->condition = EGL_SYNC_PRIOR_COMMANDS_COMPLETE;
    pthread_mutex_unlock(&egl_lock);

    /* Fence creation completes the native work synchronously before returning
     * a handle, transitioning the sync from unsignaled to signaled. */
    if (!bridge()->context_complete_fence(c->core)) {
        sync->magic = 0u;
        free(sync);
        set_error(EGL_BAD_ALLOC);
        return EGL_NO_SYNC;
    }
    sync->status = EGL_SIGNALED;

    pthread_mutex_lock(&egl_lock);
    if (!d->initialized || tls.context != c || tls.display != d) {
        pthread_mutex_unlock(&egl_lock);
        sync->magic = 0u;
        free(sync);
        set_error(EGL_BAD_DISPLAY);
        return EGL_NO_SYNC;
    }
    sync->next = syncs;
    syncs = sync;
    pthread_mutex_unlock(&egl_lock);
    return (EGLSync)sync;
}

struct egl_proc_entry {
    const char *name;
    __eglMustCastToProperFunctionPointerType proc;
};

#define EPROC(n)                                                                                   \
    { #n, (__eglMustCastToProperFunctionPointerType)n }
static const struct egl_proc_entry egl_procs[] = {EPROC(eglBindAPI),
                                                  EPROC(eglBindTexImage),
                                                  EPROC(eglChooseConfig),
                                                  EPROC(eglClientWaitSync),
                                                  EPROC(eglCopyBuffers),
                                                  EPROC(eglCreateContext),
                                                  EPROC(eglCreateImage),
                                                  EPROC(eglCreatePbufferFromClientBuffer),
                                                  EPROC(eglCreatePbufferSurface),
                                                  EPROC(eglCreatePixmapSurface),
                                                  EPROC(eglCreatePlatformPixmapSurface),
                                                  EPROC(eglCreatePlatformPixmapSurfaceEXT),
                                                  EPROC(eglCreatePlatformWindowSurface),
                                                  EPROC(eglCreatePlatformWindowSurfaceEXT),
                                                  EPROC(eglCreateSync),
                                                  EPROC(eglCreateWindowSurface),
                                                  EPROC(eglDestroyContext),
                                                  EPROC(eglDestroyImage),
                                                  EPROC(eglDestroySurface),
                                                  EPROC(eglDestroySync),
                                                  EPROC(eglGetConfigAttrib),
                                                  EPROC(eglGetConfigs),
                                                  EPROC(eglGetCurrentContext),
                                                  EPROC(eglGetCurrentDisplay),
                                                  EPROC(eglGetCurrentSurface),
                                                  EPROC(eglGetDisplay),
                                                  EPROC(eglGetError),
                                                  EPROC(eglGetPlatformDisplay),
                                                  EPROC(eglGetPlatformDisplayEXT),
                                                  EPROC(eglGetProcAddress),
                                                  EPROC(eglGetSyncAttrib),
                                                  EPROC(eglInitialize),
                                                  EPROC(eglMakeCurrent),
                                                  EPROC(eglQueryAPI),
                                                  EPROC(eglQueryContext),
                                                  EPROC(eglQueryString),
                                                  EPROC(eglQuerySurface),
                                                  EPROC(eglReleaseTexImage),
                                                  EPROC(eglReleaseThread),
                                                  EPROC(eglSurfaceAttrib),
                                                  EPROC(eglSwapBuffers),
                                                  EPROC(eglSwapInterval),
                                                  EPROC(eglTerminate),
                                                  EPROC(eglWaitClient),
                                                  EPROC(eglWaitGL),
                                                  EPROC(eglWaitNative),
                                                  EPROC(eglWaitSync)};
#undef EPROC

MGL_EXPORT __eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *name) {
    size_t i;
    void *p;

    union {
        void *obj;
        __eglMustCastToProperFunctionPointerType fn;
    } cast;

    if (name == NULL)
        return NULL;
    for (i = 0; i < sizeof(egl_procs) / sizeof(egl_procs[0]); ++i) {
        if (strcmp(name, egl_procs[i].name) == 0)
            return egl_procs[i].proc;
    }
    p = bridge()->proc_address(name);
    if (p == NULL)
        return NULL;
    cast.obj = p;
    return cast.fn;
}

#include "mgl/glx.h"
#include "mgl/gl.h"
#include "mgl/platform.h"

#include "abi/proc_address.h"
#include "core/runtime.h"
#include "core/render_diag.h"
#include "core/version.h"
#include "glx/drawable.h"
#include "glx/glx_internal.h"
#include "list/display_list.h"
#include "presentation/android_direct.h"

#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <limits.h>
#include <X11/Xproto.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static _Thread_local Display *tls_display;
static _Thread_local GLXContext tls_glx_context;
static _Thread_local GLXDrawable tls_drawable;
static _Thread_local GLXDrawable tls_read_drawable;
static struct mgl_glx_fbconfig default_fbconfig = {
    0, 1, GLX_WINDOW_BIT | GLX_PBUFFER_BIT | GLX_PIXMAP_BIT, 1, 8, 8, 8, 8, 24, 8, 1, 0, 0};
#define MGL_GLX_SCREEN_CACHE 32
#define MGL_GLX_CONFIG_COUNT 8
static struct mgl_glx_fbconfig glx_configs[MGL_GLX_SCREEN_CACHE][MGL_GLX_CONFIG_COUNT];
static pthread_mutex_t glx_config_lock = PTHREAD_MUTEX_INITIALIZER;

static void init_fbconfigs(int screen) {
    static const struct mgl_glx_fbconfig defs[MGL_GLX_CONFIG_COUNT] = {
        {0, 1, 7, 1, 8, 8, 8, 8, 24, 8, 1, 0, 0},
        {0, 2, 7, 1, 8, 8, 8, 8, 24, 8, 0, 0, 0},
        {0, 3, 7, 1, 8, 8, 8, 0, 24, 8, 1, 0, 0},
        {0, 4, 7, 1, 8, 8, 8, 0, 24, 8, 0, 0, 0},
        {0, 5, GLX_PBUFFER_BIT, 0, 5, 6, 5, 0, 16, 0, 1, 0, 0},
        {0, 6, GLX_PBUFFER_BIT, 0, 5, 6, 5, 0, 16, 0, 0, 0, 0},
        {0, 7, 7, 1, 8, 8, 8, 8, 24, 8, 1, 1, 4},
        {0, 8, 7, 1, 8, 8, 8, 8, 24, 8, 1, 1, 2}};
    unsigned i;
    if (screen < 0 || screen >= MGL_GLX_SCREEN_CACHE)
        return;
    (void)pthread_mutex_lock(&glx_config_lock);
    if (glx_configs[screen][0].id == 0) {
        for (i = 0; i < MGL_GLX_CONFIG_COUNT; ++i) {
            glx_configs[screen][i] = defs[i];
            glx_configs[screen][i].screen = screen;
        }
    }
    (void)pthread_mutex_unlock(&glx_config_lock);
}

static int glx_config_valid(GLXFBConfig config) {
    unsigned i, j;
    if (config == &default_fbconfig)
        return 1;
    for (i = 0; i < MGL_GLX_SCREEN_CACHE; ++i)
        for (j = 0; j < MGL_GLX_CONFIG_COUNT; ++j)
            if (config == &glx_configs[i][j] && config->id != 0)
                return 1;
    return 0;
}

static void glx_send_error(Display *dpy, int core_error, int code, unsigned long resource,
                           unsigned short minor) {
    xError error;
    int opcode, event_base, error_base;
    if (dpy == NULL || !XQueryExtension(dpy, "GLX", &opcode, &event_base, &error_base))
        return;
    memset(&error, 0, sizeof(error));
    error.type = X_Error;
    error.errorCode = (CARD8)(core_error ? code : error_base + code);
    error.resourceID = (CARD32)resource;
    error.minorCode = (CARD16)minor;
    error.majorCode = (CARD8)opcode;
    LockDisplay(dpy);
    error.sequenceNumber = (CARD16)dpy->request;
    (void)_XError(dpy, &error);
    UnlockDisplay(dpy);
}

static pthread_mutex_t glx_context_lifetime_lock = PTHREAD_MUTEX_INITIALIZER;
/* Validate borrowed GLX handles by identity before
 * dereferencing them; protected by the existing context-lifetime mutex. */
static GLXContext glx_live_contexts;

static GLXContext find_live_context_locked(GLXContext handle) {
    GLXContext it;
    for (it = glx_live_contexts; it != NULL; it = it->registry_next) {
        if (it == handle)
            return it;
    }
    return NULL;
}

static const char client_vendor[] = "ManifestGL";
static const char client_version[] = "1.4 ManifestGL " MGL_PROJECT_VERSION_STRING;
static const char client_extensions[] =
    "GLX_ARB_create_context GLX_ARB_create_context_profile GLX_EXT_swap_control "
    "GLX_MESA_swap_control GLX_SGI_swap_control";

Bool glXQueryExtension(Display *dpy, int *error_base, int *event_base) {
    int opcode = 0, events = 0, errors = 0;
    if (dpy == NULL || !XQueryExtension(dpy, "GLX", &opcode, &events, &errors))
        return False;
    if (error_base != NULL)
        *error_base = errors;
    if (event_base != NULL)
        *event_base = events;
    return True;
}

Bool glXQueryVersion(Display *dpy, int *major, int *minor) {
    (void)dpy;
    if (major != NULL)
        *major = 1;
    if (minor != NULL)
        *minor = 4;
    return True;
}

static const char *query_string(int name) {
    switch (name) {
        case GLX_VENDOR:
            return client_vendor;
        case GLX_VERSION:
            return client_version;
        case GLX_EXTENSIONS:
            return client_extensions;
        default:
            return NULL;
    }
}

/* Server/screen-scoped GLX queries must validate
 * the X display's screen range before reading or publishing a result. */
static int glx_valid_screen(Display *dpy, int screen) {
    return dpy != NULL && screen >= 0 && screen < ScreenCount(dpy);
}

const char *glXGetClientString(Display *dpy, int name) {
    (void)dpy;
    return query_string(name);
}

const char *glXQueryServerString(Display *dpy, int screen, int name) {
    if (!glx_valid_screen(dpy, screen))
        return NULL;
    return query_string(name);
}

const char *glXQueryExtensionsString(Display *dpy, int screen) {
    if (!glx_valid_screen(dpy, screen))
        return NULL;
    return client_extensions;
}

__GLXextFuncPtr glXGetProcAddress(const unsigned char *name) {
    union {
        void *ptr;
        __GLXextFuncPtr fn;
    } cast;

    cast.ptr = mgl_proc_address((const char *)name);
    return cast.fn;
}

__GLXextFuncPtr glXGetProcAddressARB(const unsigned char *name) {
    return glXGetProcAddress(name);
}

static int legacy_visual_match(Display *dpy, GLXFBConfig config, const int *list) {
    const int *p;
    int value;
    for (p = list; p != NULL && *p != 0;) {
        int attr = *p++;
        int requested = 1;
        switch (attr) {
            case GLX_USE_GL:
            case GLX_RGBA:
            case GLX_DOUBLEBUFFER:
            case GLX_STEREO:
                break;
            case GLX_BUFFER_SIZE:
            case GLX_LEVEL:
            case GLX_AUX_BUFFERS:
            case GLX_RED_SIZE:
            case GLX_GREEN_SIZE:
            case GLX_BLUE_SIZE:
            case GLX_ALPHA_SIZE:
            case GLX_DEPTH_SIZE:
            case GLX_STENCIL_SIZE:
            case GLX_ACCUM_RED_SIZE:
            case GLX_ACCUM_GREEN_SIZE:
            case GLX_ACCUM_BLUE_SIZE:
            case GLX_ACCUM_ALPHA_SIZE:
            case GLX_SAMPLE_BUFFERS:
            case GLX_SAMPLES:
                requested = *p++;
                break;
            default:
                return 0;
        }
        if (attr == GLX_RGBA || attr == GLX_USE_GL)
            continue;
        if (glXGetFBConfigAttrib(dpy, config, attr, &value) != 0)
            return 0;
        if (attr == GLX_LEVEL || attr == GLX_DOUBLEBUFFER || attr == GLX_STEREO) {
            if (value != requested)
                return 0;
        } else if (value < requested)
            return 0;
    }
    return 1;
}

XVisualInfo *glXChooseVisual(Display *dpy, int screen, int *attrib_list) {
    GLXFBConfig *configs;
    XVisualInfo *out = NULL;
    int count = 0, i;
    if (!glx_valid_screen(dpy, screen))
        return NULL;
    configs = glXGetFBConfigs(dpy, screen, &count);
    if (configs == NULL)
        return NULL;
    for (i = 0; i < count; ++i) {
        if (configs[i]->x_renderable && (configs[i]->drawable_type & GLX_WINDOW_BIT) &&
            legacy_visual_match(dpy, configs[i], attrib_list)) {
            out = glXGetVisualFromFBConfig(dpy, configs[i]);
            if (out != NULL)
                break;
        }
    }
    free(configs);
    return out;
}

int glXGetConfig(Display *dpy, XVisualInfo *vis, int attrib, int *value) {
    GLXFBConfig *configs;
    int n, i, result = GLX_BAD_VISUAL;
    if (value == NULL)
        return GLX_BAD_VALUE;
    if (vis == NULL)
        return GLX_BAD_VISUAL;
    if (dpy == NULL || !glx_valid_screen(dpy, vis->screen))
        return GLX_BAD_SCREEN;
    configs = glXGetFBConfigs(dpy, vis->screen, &n);
    if (configs == NULL)
        return GLX_NO_EXTENSION;
    for (i = 0; i < n; ++i) {
        int visual_id = 0;
        if (glXGetFBConfigAttrib(dpy, configs[i], GLX_VISUAL_ID, &visual_id) == 0 &&
            visual_id != 0 && (VisualID)visual_id == vis->visualid) {
            if (attrib == GLX_USE_GL) {
                *value = True;
                result = Success;
            } else if (attrib == GLX_RGBA) {
                *value = True;
                result = Success;
            } else
                result = glXGetFBConfigAttrib(dpy, configs[i], attrib, value);
            break;
        }
    }
    free(configs);
    return result;
}

/* Keep the share parent alive through native
 * context creation. A foreign, stale, or delete-pending handle must not be
 * dereferenced, and GLX contexts cannot share across Displays. */
static GLXContext create_context(Display *dpy, GLXContext share, int screen, int fbconfig_id,
                                 int render_type, GLbitfield profile_mask) {
    struct mgl_glx_context *ctx = calloc(1, sizeof(*ctx));
    struct mgl_context *share_core = NULL;
    GLXContext parent;
    if (ctx == NULL)
        return NULL;
    ctx->display = dpy;
    ctx->screen = screen;
    ctx->fbconfig_id = fbconfig_id;
    ctx->render_type = render_type;
    (void)pthread_mutex_lock(&glx_context_lifetime_lock);
    if (share != NULL) {
        parent = find_live_context_locked(share);
        if (parent == NULL || parent->destroy_pending || parent->display != dpy ||
            parent->screen != screen) {
            (void)pthread_mutex_unlock(&glx_context_lifetime_lock);
            free(ctx);
            return NULL;
        }
        share_core = parent->core;
    }
    ctx->core = mgl_context_create_offscreen(mgl_runtime_native_dispatch(), share_core, 1, 1);
    if (ctx->core == NULL) {
        (void)pthread_mutex_unlock(&glx_context_lifetime_lock);
        free(ctx);
        return NULL;
    }
    mgl_context_set_profile_mask(ctx->core, profile_mask);
    ctx->registry_next = glx_live_contexts;
    glx_live_contexts = ctx;
    (void)pthread_mutex_unlock(&glx_context_lifetime_lock);
    return ctx;
}

GLXContext glXCreateContext(Display *dpy, XVisualInfo *vis, GLXContext share, Bool direct) {
    GLXFBConfig *configs;
    GLXContext ctx = NULL;
    int count, i;
    (void)direct;
    if (dpy == NULL)
        return create_context(NULL, share, vis != NULL ? vis->screen : 0, 1, GLX_RGBA_TYPE,
                              GL_CONTEXT_COMPATIBILITY_PROFILE_BIT);
    if (vis == NULL)
        return create_context(dpy, share, DefaultScreen(dpy), 1, GLX_RGBA_TYPE,
                              GL_CONTEXT_COMPATIBILITY_PROFILE_BIT);
    if (!glx_valid_screen(dpy, vis->screen))
        return NULL;
    configs = glXGetFBConfigs(dpy, vis->screen, &count);
    if (configs == NULL)
        return NULL;
    for (i = 0; i < count; ++i) {
        int visual_id;
        if (configs[i]->x_renderable && (configs[i]->drawable_type & GLX_WINDOW_BIT) &&
            glXGetFBConfigAttrib(dpy, configs[i], GLX_VISUAL_ID, &visual_id) == Success &&
            (VisualID)visual_id == vis->visualid) {
            ctx = create_context(dpy, share, vis->screen, configs[i]->id, GLX_RGBA_TYPE,
                                 GL_CONTEXT_COMPATIBILITY_PROFILE_BIT);
            break;
        }
    }
    free(configs);
    return ctx;
}

GLXContext glXCreateNewContext(Display *dpy, GLXFBConfig config, int render_type, GLXContext share,
                               Bool direct) {
    (void)direct;
    if (render_type != GLX_RGBA_TYPE || (config != NULL && !glx_config_valid(config)))
        return NULL;
    return create_context(dpy, share, config != NULL ? config->screen : 0,
                          config != NULL ? config->id : 1, render_type,
                          GL_CONTEXT_COMPATIBILITY_PROFILE_BIT);
}

GLXContext glXCreateContextAttribsARB(Display *dpy, GLXFBConfig config, GLXContext share,
                                      Bool direct, const int *attrib_list) {
    const int *p = attrib_list;
    int major = 1, minor = 0, flags = 0;
    int profile = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
    int render_type = GLX_RGBA_TYPE;
    int profile_required;
    GLbitfield selected_profile;
    GLXContext ctx;
    (void)direct;
    if (dpy != NULL && !glx_config_valid(config)) {
        glx_send_error(dpy, 0, 9, 0, 34);
        return NULL;
    }
    while (p != NULL && p[0] != 0) {
        switch (p[0]) {
            case GLX_CONTEXT_MAJOR_VERSION_ARB:
                major = p[1];
                break;
            case GLX_CONTEXT_MINOR_VERSION_ARB:
                minor = p[1];
                break;
            case GLX_CONTEXT_FLAGS_ARB:
                flags = p[1];
                break;
            case GLX_CONTEXT_PROFILE_MASK_ARB:
                profile = p[1];
                break;
            case GLX_RENDER_TYPE:
                render_type = p[1];
                break;
            default:
                glx_send_error(dpy, 1, BadValue, (unsigned long)p[0], 34);
                return NULL;
        }
        p += 2;
    }
    if (render_type != GLX_RGBA_TYPE || (flags & ~GLX_CONTEXT_DEBUG_BIT_ARB) != 0) {
        glx_send_error(dpy, 1, BadValue,
                       (unsigned long)(render_type != GLX_RGBA_TYPE ? render_type : flags), 34);
        return NULL;
    }
    if (!mgl_opengl_version_supported(major, minor)) {
        glx_send_error(dpy, 1, BadMatch, 0, 34);
        return NULL;
    }
    profile_required = major > 3 || (major == 3 && minor >= 2);
    if (profile_required && profile != GLX_CONTEXT_CORE_PROFILE_BIT_ARB &&
        profile != GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB) {
        glx_send_error(dpy, 0, 13, (unsigned long)profile, 34);
        return NULL;
    }
    selected_profile = (major == 3 && minor == 1) ||
                               (profile_required && profile == GLX_CONTEXT_CORE_PROFILE_BIT_ARB)
                           ? GL_CONTEXT_CORE_PROFILE_BIT
                           : GL_CONTEXT_COMPATIBILITY_PROFILE_BIT;
    ctx = create_context(dpy, share, config != NULL ? config->screen : 0,
                         config != NULL ? config->id : 1, GLX_RGBA_TYPE, selected_profile);
    if (ctx == NULL) {
        glx_send_error(dpy, 1, BadMatch, 0, 34);
        return NULL;
    }
    if ((flags & GLX_CONTEXT_DEBUG_BIT_ARB) != 0)
        mgl_context_set_creation_flags(ctx->core, GL_CONTEXT_FLAG_DEBUG_BIT);
    return ctx;
}

GLXContext glXCreateContextAttribs(Display *dpy, GLXFBConfig config, GLXContext share, Bool direct,
                                   const int *attrib_list) {
    return glXCreateContextAttribsARB(dpy, config, share, direct, attrib_list);
}

void glXCopyContext(Display *dpy, GLXContext src, GLXContext dst, unsigned long mask) {
    GLXContext source, target;
    int error = 0, core_error = 1;
    (void)pthread_mutex_lock(&glx_context_lifetime_lock);
    source = find_live_context_locked(src);
    target = find_live_context_locked(dst);
    if (source == NULL || target == NULL || source->destroy_pending || target->destroy_pending ||
        source->display != dpy || target->display != dpy) {
        error = 0;
        core_error = 0;
    } else if (source->screen != target->screen) {
        error = BadMatch;
    } else if (target->current_count != 0u) {
        error = BadAccess;
    } else {
        if (tls_glx_context == source && mgl_context_current() == source->core)
            glFlush();
        if (!mgl_copy_server_context_state(target->core, source->core,
                                           (GLbitfield)mask &
                                               (GL_ALL_ATTRIB_BITS | GL_MULTISAMPLE_BIT)))
            error = BadAlloc;
    }
    (void)pthread_mutex_unlock(&glx_context_lifetime_lock);
    if (error != 0 || !core_error)
        glx_send_error(dpy, core_error, error, 0, 10);
}

static void destroy_glx_context_storage(GLXContext ctx) {
    GLXContext *link;
    if (ctx == NULL)
        return;
    for (link = &glx_live_contexts; *link != NULL; link = &(*link)->registry_next) {
        if (*link == ctx) {
            *link = ctx->registry_next;
            break;
        }
    }
    mgl_context_destroy(ctx->core);
    mgl_android_present_unref(ctx->android_present);
    ctx->android_present = NULL;
    free(ctx);
}

static void release_current_context_locked(GLXContext ctx) {
    if (ctx == NULL)
        return;
    if (ctx->current_count > 0u)
        --ctx->current_count;
    if (ctx->destroy_pending && ctx->current_count == 0u)
        destroy_glx_context_storage(ctx);
}

/* Validate borrowed handle identity under the
 * lifetime lock before touching context memory or scheduling destruction. */
void glXDestroyContext(Display *dpy, GLXContext ctx) {
    GLXContext live;
    if (ctx == NULL)
        return;
    (void)pthread_mutex_lock(&glx_context_lifetime_lock);
    live = find_live_context_locked(ctx);
    if (live != NULL && live->display == dpy && !live->destroy_pending) {
        live->destroy_pending = 1;
        if (live->current_count == 0u)
            destroy_glx_context_storage(live);
    }
    (void)pthread_mutex_unlock(&glx_context_lifetime_lock);
}

static const struct mgl_glx_fbconfig *find_fbconfig(int screen, int id) {
    int i;
    if (screen < 0 || screen >= MGL_GLX_SCREEN_CACHE)
        return NULL;
    init_fbconfigs(screen);
    for (i = 0; i < MGL_GLX_CONFIG_COUNT; ++i)
        if (glx_configs[screen][i].id == id)
            return &glx_configs[screen][i];
    return NULL;
}

static int compatible_fbconfigs(const struct mgl_glx_fbconfig *context,
                                const struct mgl_glx_fbconfig *drawable) {
    return context != NULL && drawable != NULL && context->red == drawable->red &&
           context->green == drawable->green && context->blue == drawable->blue &&
           context->alpha == drawable->alpha && context->depth == drawable->depth &&
           context->stencil == drawable->stencil &&
           context->double_buffer == drawable->double_buffer &&
           context->sample_buffers == drawable->sample_buffers &&
           context->samples == drawable->samples;
}

static int drawable_compatible(Display *dpy, GLXDrawable drawable, GLXContext ctx) {
    const struct mgl_glx_fbconfig *context_config, *drawable_config;
    int screen = -1, fbconfig_id = 0;
    VisualID visual_id = 0;
    enum mgl_glx_drawable_kind kind = MGL_GLX_DRAWABLE_WINDOW;
    if (drawable == 0)
        return 1;
    if (!mgl_glx_drawable_get_format(dpy, drawable, &screen, &fbconfig_id, &visual_id) ||
        !mgl_glx_drawable_info(dpy, drawable, &kind, NULL, NULL, NULL, NULL))
        return 0;
    if (screen >= 0 && screen != ctx->screen)
        return -1;
    if (screen < 0)
        return 1;
    context_config = find_fbconfig(ctx->screen, ctx->fbconfig_id);
    if (context_config == NULL)
        return -1;
    if (fbconfig_id != 0) {
        drawable_config = find_fbconfig(screen, fbconfig_id);
        if (!compatible_fbconfigs(context_config, drawable_config))
            return -1;
        if (kind == MGL_GLX_DRAWABLE_WINDOW && !(drawable_config->drawable_type & GLX_WINDOW_BIT))
            return -1;
        if (kind == MGL_GLX_DRAWABLE_PIXMAP && !(drawable_config->drawable_type & GLX_PIXMAP_BIT))
            return -1;
        if (kind == MGL_GLX_DRAWABLE_PBUFFER && !(drawable_config->drawable_type & GLX_PBUFFER_BIT))
            return -1;
    }
    if (kind == MGL_GLX_DRAWABLE_WINDOW && visual_id != 0) {
        if (!(context_config->drawable_type & GLX_WINDOW_BIT) || !context_config->x_renderable ||
            (VisualID)XVisualIDFromVisual(DefaultVisual(dpy, screen)) != visual_id)
            return -1;
    }
    return 1;
}

static int drawable_dimensions(Display *dpy, GLXDrawable drawable, unsigned *w, unsigned *h) {
    if (drawable == 0) {
        *w = 1u;
        *h = 1u;
        return 1;
    }
    return mgl_glx_drawable_info(dpy, drawable, NULL, NULL, w, h, NULL);
}

static void apply_current_swap_interval(Display *dpy, GLXDrawable drawable, GLXContext glx) {
    struct mgl_context *core;
    int interval;
    if (glx == NULL || drawable == 0 || glx->core == NULL)
        return;
    if (!mgl_glx_drawable_info(dpy, drawable, NULL, NULL, NULL, NULL, &interval))
        return;
    core = glx->core;
    if (glx->android_present != NULL && glx->draw == drawable && glx->read == drawable) {
        (void)mgl_android_present_set_swap_interval(glx->android_present, core, interval);
    } else if (core->native_dispatch != NULL && core->native_dispatch->eglSwapInterval != NULL) {
        (void)core->native_dispatch->eglSwapInterval(core->egl_display, interval);
    }
}

static Bool make_context_current(Display *dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx,
                                 unsigned short minor) {
    unsigned draw_w, draw_h, read_w, read_h;
    struct mgl_android_present *next_present = NULL;
    struct mgl_android_present *old_present;
    GLXContext previous;
    Bool result = False;
    int draw_compatible, read_compatible;
    int error = 0, core_error = 1;
    unsigned long error_resource = 0;

    (void)pthread_mutex_lock(&glx_context_lifetime_lock);
    previous = tls_glx_context;

    if (ctx == NULL) {
        if (draw != 0 || read != 0) {
            error = BadMatch;
            goto done;
        }
        if (!mgl_context_make_current(NULL))
            goto done;
        tls_display = NULL;
        tls_glx_context = NULL;
        tls_drawable = 0;
        tls_read_drawable = 0;
        release_current_context_locked(previous);
        result = True;
        goto done;
    }
    /* Reject bad handles/displays before any
     * drawable or core/native operation; preserve the existing binding. */
    if (find_live_context_locked(ctx) == NULL || ctx->destroy_pending || ctx->display != dpy)
        goto done;
    if (ctx->current_count != 0u && previous != ctx) {
        error = BadAccess;
        goto done;
    }
    if (!!draw != !!read) {
        error = BadMatch;
        goto done;
    }
    draw_compatible = drawable_compatible(dpy, draw, ctx);
    read_compatible = read == draw ? draw_compatible : drawable_compatible(dpy, read, ctx);
    if (draw_compatible <= 0 || read_compatible <= 0) {
        if (draw_compatible < 0 || read_compatible < 0)
            error = BadMatch;
        else {
            core_error = 0;
            error = 2;
        }
        error_resource = (unsigned long)(draw_compatible <= 0 ? draw : read);
        goto done;
    }
    (void)mgl_glx_drawable_refresh(dpy, draw);
    if (read != draw)
        (void)mgl_glx_drawable_refresh(dpy, read);
    if (!drawable_dimensions(dpy, draw, &draw_w, &draw_h) ||
        !drawable_dimensions(dpy, read, &read_w, &read_h))
        goto done;

    /* Direct presentation uses one window for drawing and reading; split draw/read bindings use
     * pbuffers. */
    if (draw != 0 && draw == read)
        next_present = mgl_android_present_ref(dpy, draw);
    if (next_present == NULL) {
        enum mgl_glx_drawable_kind draw_kind = MGL_GLX_DRAWABLE_PBUFFER;
        enum mgl_glx_drawable_kind read_kind = MGL_GLX_DRAWABLE_PBUFFER;
        if ((draw != 0 && (!mgl_glx_drawable_info(dpy, draw, &draw_kind, NULL, NULL, NULL, NULL) ||
                           (draw_kind == MGL_GLX_DRAWABLE_WINDOW &&
                            mgl_glx_drawable_native_identity(dpy, draw, NULL, NULL)))) ||
            (read != 0 && (!mgl_glx_drawable_info(dpy, read, &read_kind, NULL, NULL, NULL, NULL) ||
                           (read_kind == MGL_GLX_DRAWABLE_WINDOW &&
                            mgl_glx_drawable_native_identity(dpy, read, NULL, NULL)))))
            goto done;
    }
    if (next_present != NULL) {
        if (!mgl_android_present_configure(next_present, ctx->core, (int)draw_w, (int)draw_h)) {
            mgl_android_present_unref(next_present);
            goto done;
        }
    } else if (!mgl_context_configure_pbuffers(ctx->core, (GLsizei)draw_w, (GLsizei)draw_h,
                                               (GLsizei)read_w, (GLsizei)read_h)) {
        goto done;
    }
    if (next_present == NULL)
        ctx->core->native_default_framebuffer = 0u;
    if (!mgl_context_make_current(ctx->core)) {
        mgl_android_present_unref(next_present);
        goto done;
    }

    old_present = ctx->android_present;
    ctx->android_present = next_present;
    ctx->draw = draw;
    ctx->read = read;
    if (previous != ctx)
        ++ctx->current_count;
    tls_display = dpy;
    tls_glx_context = ctx;
    tls_drawable = draw;
    tls_read_drawable = read;
    apply_current_swap_interval(dpy, draw, ctx);
    mgl_android_present_unref(old_present);
    if (previous != ctx)
        release_current_context_locked(previous);
    result = True;

done:
    (void)pthread_mutex_unlock(&glx_context_lifetime_lock);
    if (error != 0)
        glx_send_error(dpy, core_error, error, error_resource, minor);
    return result;
}

Bool glXMakeContextCurrent(Display *dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx) {
    return make_context_current(dpy, draw, read, ctx, 26);
}

Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx) {
    return make_context_current(dpy, drawable, drawable, ctx, 5);
}

GLXContext glXGetCurrentContext(void) {
    return tls_glx_context;
}

GLXDrawable glXGetCurrentDrawable(void) {
    return tls_drawable;
}

GLXDrawable glXGetCurrentReadDrawable(void) {
    return tls_read_drawable;
}

Display *glXGetCurrentDisplay(void) {
    return tls_display;
}

static unsigned long ximage_component(unsigned char value, unsigned long bits) {
    unsigned shift = 0u;
    unsigned long max_value;
    if (bits == 0u)
        return 0u;
    while ((bits & 1u) == 0u) {
        bits >>= 1u;
        ++shift;
    }
    max_value = bits;
    return (unsigned long)(((uint64_t)value * max_value + 127u) / 255u) << shift;
}

static void present_raw_window(Display *dpy, GLXDrawable drawable, struct mgl_context *core) {
    const struct mgl_glx_fbconfig *config;
    XVisualInfo templ, *visual = NULL;
    XImage *image = NULL;
    GC gc;
    unsigned width, height, x, y;
    VisualID visual_id;
    int screen, count = 0;
    unsigned char *pixels;
    size_t pixel_count;
    GLuint saved_read_fbo, saved_pbo;
    GLint saved_alignment, saved_row_length, saved_skip_rows, saved_skip_pixels;
    if (!mgl_glx_native_window_info(dpy, (Window)drawable, &screen, &visual_id, &width, &height))
        return;
    config = find_fbconfig(tls_glx_context->screen, tls_glx_context->fbconfig_id);
    if (config == NULL || !config->double_buffer || width == 0u || height == 0u ||
        width > (unsigned)INT_MAX || height > (unsigned)INT_MAX ||
        (size_t)width > SIZE_MAX / (size_t)height / 4u)
        return;
    pixel_count = (size_t)width * (size_t)height;
    pixels = malloc(pixel_count * 4u);
    if (pixels == NULL)
        return;
    memset(&templ, 0, sizeof(templ));
    templ.visualid = visual_id;
    templ.screen = screen;
    visual = XGetVisualInfo(dpy, VisualScreenMask | VisualIDMask, &templ, &count);
    if (visual == NULL || count == 0 ||
        (visual->class != TrueColor && visual->class != DirectColor))
        goto done;
    image = XCreateImage(dpy, visual->visual, (unsigned)visual->depth, ZPixmap, 0, NULL, width,
                         height, 32, 0);
    if (image == NULL || image->bytes_per_line <= 0 ||
        (size_t)image->bytes_per_line > SIZE_MAX / (size_t)height)
        goto done;
    image->data = calloc((size_t)image->bytes_per_line, (size_t)height);
    if (image->data == NULL)
        goto done;

    saved_read_fbo = core->read_framebuffer;
    saved_pbo = core->pixel_pack_buffer;
    saved_alignment = core->pack_alignment;
    saved_row_length = core->pack_row_length;
    saved_skip_rows = core->pack_skip_rows;
    saved_skip_pixels = core->pack_skip_pixels;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0u);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0u);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glReadPixels(0, 0, (GLsizei)width, (GLsizei)height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_PACK_ALIGNMENT, saved_alignment);
    glPixelStorei(GL_PACK_ROW_LENGTH, saved_row_length);
    glPixelStorei(GL_PACK_SKIP_ROWS, saved_skip_rows);
    glPixelStorei(GL_PACK_SKIP_PIXELS, saved_skip_pixels);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, saved_pbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, saved_read_fbo);

    for (y = 0u; y < height; ++y) {
        const unsigned char *row = pixels + (size_t)(height - 1u - y) * width * 4u;
        for (x = 0u; x < width; ++x) {
            const unsigned char *pixel = row + (size_t)x * 4u;
            unsigned long packed = ximage_component(pixel[0], image->red_mask) |
                                   ximage_component(pixel[1], image->green_mask) |
                                   ximage_component(pixel[2], image->blue_mask);
            XPutPixel(image, (int)x, (int)y, packed);
        }
    }
    gc = XCreateGC(dpy, (Window)drawable, 0, NULL);
    if (gc != NULL) {
        XPutImage(dpy, (Window)drawable, gc, image, 0, 0, 0, 0, width, height);
        XFreeGC(dpy, gc);
        XFlush(dpy);
    }
done:
    if (image != NULL)
        XDestroyImage(image);
    if (visual != NULL)
        XFree(visual);
    free(pixels);
}

void glXSwapBuffers(Display *dpy, GLXDrawable drawable) {
    struct mgl_context *core = mgl_context_current();
    GLXContext glx = tls_glx_context;
    if (dpy == NULL)
        return;
    if (drawable == 0 || !mgl_glx_drawable_info(dpy, drawable, NULL, NULL, NULL, NULL, NULL)) {
        glx_send_error(dpy, 0, 2, (unsigned long)drawable, 11);
        return;
    }
    if (core == NULL)
        return;

    if (glx != NULL && glx->android_present != NULL && glx->draw == drawable &&
        glx->read == drawable) {
        unsigned width = 1u, height = 1u;
        {
            uint64_t deep_started = mgl_perf_diag_now();
            int observed = mgl_glx_drawable_watch_poll(dpy, drawable);
            int valid = observed >= 0 &&
                        (observed > 0 || mgl_glx_drawable_refresh(dpy, drawable)) &&
                        drawable_dimensions(dpy, drawable, &width, &height);
            mgl_deep_record(core, MGL_DEEP_GLX_GEOMETRY, deep_started);
            if (!valid)
                return;
        }
        (void)mgl_android_present_swap(glx->android_present, core, (int)width, (int)height);
        return;
    }
    if (glx != NULL && glx->android_present == NULL && tls_display == dpy &&
        glx->draw == drawable && glx->read == drawable &&
        !mgl_glx_drawable_native_identity(dpy, drawable, NULL, NULL)) {
        glFlush();
        present_raw_window(dpy, drawable, core);
    }
}

void glXWaitGL(void) {
    if (tls_glx_context == NULL || mgl_context_current() == NULL)
        return;
    glFinish();
}

void glXWaitX(void) {
    if (tls_glx_context == NULL || tls_display == NULL || mgl_context_current() == NULL)
        return;
    XSync(tls_display, False);
}

static XCharStruct *font_glyph_metrics(XFontStruct *font, unsigned int codepoint) {
    unsigned int rows, columns, byte1, byte2, index;
    if (font == NULL)
        return NULL;
    rows = (unsigned int)font->max_byte1 - (unsigned int)font->min_byte1 + 1u;
    columns = (unsigned int)font->max_char_or_byte2 - (unsigned int)font->min_char_or_byte2 + 1u;
    if (rows == 1u) {
        if (codepoint < (unsigned int)font->min_char_or_byte2 ||
            codepoint > (unsigned int)font->max_char_or_byte2)
            return NULL;
        index = codepoint - (unsigned int)font->min_char_or_byte2;
    } else {
        if (codepoint > 0xffffu)
            return NULL;
        byte1 = codepoint >> 8;
        byte2 = codepoint & 0xffu;
        if (byte1 < (unsigned int)font->min_byte1 || byte1 > (unsigned int)font->max_byte1 ||
            byte2 < (unsigned int)font->min_char_or_byte2 ||
            byte2 > (unsigned int)font->max_char_or_byte2)
            return NULL;
        index = (byte1 - (unsigned int)font->min_byte1) * columns +
                (byte2 - (unsigned int)font->min_char_or_byte2);
    }
    return font->per_char != NULL ? &font->per_char[index] : &font->min_bounds;
}

static int capture_font_bitmap(Display *dpy, Drawable parent, GC gc, unsigned int codepoint,
                               unsigned width, unsigned height, int draw_x, int draw_y,
                               unsigned char *bitmap, size_t row_bytes) {
    Pixmap pixmap;
    XImage *image;
    XChar2b glyph;
    unsigned x, y;
    if (width == 0u || height == 0u)
        return 1;
    pixmap = XCreatePixmap(dpy, parent, (unsigned)(row_bytes * 8u), height, 1u);
    if (pixmap == 0)
        return 0;
    XSetForeground(dpy, gc, 0ul);
    XFillRectangle(dpy, pixmap, gc, 0, 0, (unsigned)(row_bytes * 8u), height);
    XSetForeground(dpy, gc, 1ul);
    glyph.byte1 = (unsigned char)((codepoint >> 8) & 0xffu);
    glyph.byte2 = (unsigned char)(codepoint & 0xffu);
    XDrawString16(dpy, pixmap, gc, draw_x, draw_y, &glyph, 1);
    image = XGetImage(dpy, pixmap, 0, 0, (unsigned)(row_bytes * 8u), height, 1ul, XYPixmap);
    if (image == NULL) {
        XFreePixmap(dpy, pixmap);
        return 0;
    }
    for (y = 0u; y < height; ++y) {
        for (x = 0u; x < width; ++x) {
            if (XGetPixel(image, (int)x, (int)y) != 0ul)
                bitmap[(size_t)(height - y - 1u) * row_bytes + x / 8u] |=
                    (unsigned char)(0x80u >> (x & 7u));
        }
    }
    XDestroyImage(image);
    XFreePixmap(dpy, pixmap);
    return 1;
}

void glXUseXFont(Font font, int first, int count, int list_base) {
    struct mgl_context *core = mgl_context_current();
    XFontStruct *font_info;
    XGCValues values;
    Drawable parent;
    Pixmap scratch;
    GC gc;
    int i;

    if (tls_glx_context == NULL || tls_display == NULL || core == NULL)
        return;
    /* Display-list compilation leaves glXUseXFont unrecorded. GLXBadContextState protocol
     * delivery is unavailable. */
    if (core->compiling_list != NULL)
        return;

    glFlush();
    if (count <= 0)
        return;
    font_info = XQueryFont(tls_display, font);
    if (font_info == NULL)
        return;

    parent = RootWindow(tls_display, DefaultScreen(tls_display));
    scratch = XCreatePixmap(tls_display, parent, 1u, 1u, 1u);
    if (scratch == 0) {
        XFreeFontInfo(NULL, font_info, 1);
        return;
    }
    memset(&values, 0, sizeof(values));
    values.foreground = 1ul;
    values.background = 0ul;
    values.font = font;
    gc = XCreateGC(tls_display, scratch, GCForeground | GCBackground | GCFont, &values);
    XFreePixmap(tls_display, scratch);
    if (gc == NULL) {
        XFreeFontInfo(NULL, font_info, 1);
        return;
    }

    for (i = 0; i < count; ++i) {
        const long long code_value = (long long)first + (long long)i;
        const long long list_value = (long long)list_base + (long long)i;
        XCharStruct *metrics = code_value >= 0 && code_value <= 0xffffffffll
                                   ? font_glyph_metrics(font_info, (unsigned int)code_value)
                                   : NULL;
        GLuint list_name = (GLuint)list_value;
        unsigned char *bitmap = NULL;
        size_t row_bytes = 0u, bitmap_bytes = 0u;
        int width = 0, height = 0;
        GLfloat xorig = 0.0f, yorig = 0.0f, xmove = 0.0f;
        int captured = 1;

        if (metrics != NULL) {
            width = metrics->rbearing - metrics->lbearing;
            height = metrics->ascent + metrics->descent;
            if (width < 0)
                width = 0;
            if (height < 0)
                height = 0;
            xorig = (GLfloat)(-metrics->lbearing);
            yorig = (GLfloat)metrics->descent;
            xmove = (GLfloat)metrics->width;
            row_bytes = ((size_t)width + 7u) / 8u;
            if (height != 0 && row_bytes > SIZE_MAX / (size_t)height)
                captured = 0;
            else
                bitmap_bytes = row_bytes * (size_t)height;
            if (captured && bitmap_bytes != 0u) {
                bitmap = calloc(bitmap_bytes, 1u);
                if (bitmap == NULL)
                    captured = 0;
            }
            if (captured &&
                !capture_font_bitmap(tls_display, parent, gc, (unsigned int)code_value,
                                     (unsigned)width, (unsigned)height, -metrics->lbearing,
                                     metrics->ascent, bitmap, row_bytes))
                captured = 0;
        }

        if (metrics != NULL && !captured) {
            free(bitmap);
            break;
        }

        glNewList(list_name, GL_COMPILE);
        if (core->compiling_list != NULL) {
            if (metrics != NULL)
                (void)mgl_list_record_bitmap(core, (GLsizei)width, (GLsizei)height, xorig, yorig,
                                             xmove, 0.0f, bitmap, bitmap_bytes);
            glEndList();
        }
        free(bitmap);
    }

    XFreeGC(tls_display, gc);
    XFreeFontInfo(NULL, font_info, 1);
}

Bool glXIsDirect(Display *dpy, GLXContext ctx) {
    GLXContext live;
    Bool result = False;
    if (ctx == NULL)
        return False;
    (void)pthread_mutex_lock(&glx_context_lifetime_lock);
    live = find_live_context_locked(ctx);
    if (live != NULL && !live->destroy_pending && live->display == dpy)
        result = True;
    (void)pthread_mutex_unlock(&glx_context_lifetime_lock);
    return result;
}

static int fbconfig_attribute_valid(int attribute) {
    switch (attribute) {
        case GLX_FBCONFIG_ID:
        case GLX_BUFFER_SIZE:
        case GLX_LEVEL:
        case GLX_DOUBLEBUFFER:
        case GLX_STEREO:
        case GLX_AUX_BUFFERS:
        case GLX_RED_SIZE:
        case GLX_GREEN_SIZE:
        case GLX_BLUE_SIZE:
        case GLX_ALPHA_SIZE:
        case GLX_DEPTH_SIZE:
        case GLX_STENCIL_SIZE:
        case GLX_ACCUM_RED_SIZE:
        case GLX_ACCUM_GREEN_SIZE:
        case GLX_ACCUM_BLUE_SIZE:
        case GLX_ACCUM_ALPHA_SIZE:
        case GLX_SAMPLE_BUFFERS:
        case GLX_SAMPLES:
        case GLX_RENDER_TYPE:
        case GLX_DRAWABLE_TYPE:
        case GLX_X_RENDERABLE:
        case GLX_X_VISUAL_TYPE:
        case GLX_CONFIG_CAVEAT:
        case GLX_TRANSPARENT_TYPE:
        case GLX_TRANSPARENT_INDEX_VALUE:
        case GLX_TRANSPARENT_RED_VALUE:
        case GLX_TRANSPARENT_GREEN_VALUE:
        case GLX_TRANSPARENT_BLUE_VALUE:
        case GLX_TRANSPARENT_ALPHA_VALUE:
        case GLX_MAX_PBUFFER_WIDTH:
        case GLX_MAX_PBUFFER_HEIGHT:
        case GLX_MAX_PBUFFER_PIXELS:
        case GLX_VISUAL_ID:
            return 1;
        default:
            return 0;
    }
}

static int fbconfig_minimum_match(int actual, int requested) {
    return requested == (int)GLX_DONT_CARE || actual >= requested;
}

static int fbconfig_exact_match(int actual, int requested) {
    return requested == (int)GLX_DONT_CARE || actual == requested;
}

static int fbconfig_mask_match(int actual, int requested) {
    return requested == (int)GLX_DONT_CARE || (actual & requested) == requested;
}

static int fbconfig_matches(Display *dpy, GLXFBConfig config, const int *attrib_list) {
    const int *p;
    int requested_id = -1;
    int requested_drawable_type = GLX_WINDOW_BIT;
    int requested_transparent_type = GLX_NONE;
    int level_explicit = 0;
    int stereo_explicit = 0;
    int render_type_explicit = 0;
    int drawable_type_explicit = 0;
    int transparent_type_explicit = 0;

    for (p = attrib_list; p != NULL && p[0] != 0; p += 2) {
        if (!fbconfig_attribute_valid(p[0]))
            return -1;
        if (p[0] == GLX_FBCONFIG_ID)
            requested_id = p[1];
        else if (p[0] == GLX_LEVEL)
            level_explicit = 1;
        else if (p[0] == GLX_STEREO)
            stereo_explicit = 1;
        else if (p[0] == GLX_RENDER_TYPE)
            render_type_explicit = 1;
        else if (p[0] == GLX_DRAWABLE_TYPE) {
            requested_drawable_type = p[1];
            drawable_type_explicit = 1;
        } else if (p[0] == GLX_TRANSPARENT_TYPE) {
            requested_transparent_type = p[1];
            transparent_type_explicit = 1;
        }
    }

    if (requested_id != -1)
        return requested_id == config->id;

    {
        int actual;
        if (!level_explicit && (glXGetFBConfigAttrib(dpy, config, GLX_LEVEL, &actual) != 0 ||
                                !fbconfig_exact_match(actual, 0)))
            return 0;
        if (!stereo_explicit && (glXGetFBConfigAttrib(dpy, config, GLX_STEREO, &actual) != 0 ||
                                 !fbconfig_exact_match(actual, False)))
            return 0;
        if (!render_type_explicit &&
            (glXGetFBConfigAttrib(dpy, config, GLX_RENDER_TYPE, &actual) != 0 ||
             !fbconfig_mask_match(actual, GLX_RGBA_BIT)))
            return 0;
        if (!drawable_type_explicit &&
            (glXGetFBConfigAttrib(dpy, config, GLX_DRAWABLE_TYPE, &actual) != 0 ||
             !fbconfig_mask_match(actual, GLX_WINDOW_BIT)))
            return 0;
        if (!transparent_type_explicit &&
            (glXGetFBConfigAttrib(dpy, config, GLX_TRANSPARENT_TYPE, &actual) != 0 ||
             !fbconfig_exact_match(actual, GLX_NONE)))
            return 0;
    }

    for (p = attrib_list; p != NULL && p[0] != 0; p += 2) {
        int actual = 0;
        int requested = p[1];
        int rc;

        if (p[0] == GLX_FBCONFIG_ID || p[0] == GLX_MAX_PBUFFER_WIDTH ||
            p[0] == GLX_MAX_PBUFFER_HEIGHT || p[0] == GLX_MAX_PBUFFER_PIXELS ||
            p[0] == GLX_VISUAL_ID)
            continue;
        if (p[0] == GLX_X_VISUAL_TYPE && drawable_type_explicit &&
            (requested_drawable_type & GLX_WINDOW_BIT) == 0)
            continue;
        if ((p[0] == GLX_TRANSPARENT_INDEX_VALUE || p[0] == GLX_TRANSPARENT_RED_VALUE ||
             p[0] == GLX_TRANSPARENT_GREEN_VALUE || p[0] == GLX_TRANSPARENT_BLUE_VALUE ||
             p[0] == GLX_TRANSPARENT_ALPHA_VALUE) &&
            transparent_type_explicit && requested_transparent_type == GLX_NONE)
            continue;
        if (requested == (int)GLX_DONT_CARE && p[0] == GLX_LEVEL)
            return 0;

        rc = glXGetFBConfigAttrib(dpy, config, p[0], &actual);
        if (rc != 0)
            return -1;
        switch (p[0]) {
            case GLX_LEVEL:
            case GLX_DOUBLEBUFFER:
            case GLX_STEREO:
            case GLX_X_RENDERABLE:
            case GLX_X_VISUAL_TYPE:
            case GLX_CONFIG_CAVEAT:
            case GLX_TRANSPARENT_TYPE:
            case GLX_TRANSPARENT_INDEX_VALUE:
            case GLX_TRANSPARENT_RED_VALUE:
            case GLX_TRANSPARENT_GREEN_VALUE:
            case GLX_TRANSPARENT_BLUE_VALUE:
            case GLX_TRANSPARENT_ALPHA_VALUE:
                if (!fbconfig_exact_match(actual, requested))
                    return 0;
                break;
            case GLX_RENDER_TYPE:
            case GLX_DRAWABLE_TYPE:
                if (!fbconfig_mask_match(actual, requested))
                    return 0;
                break;
            default:
                if (!fbconfig_minimum_match(actual, requested))
                    return 0;
                break;
        }
    }
    return 1;
}

GLXFBConfig *glXChooseFBConfig(Display *dpy, int screen, const int *attrib_list, int *nelements) {
    GLXFBConfig *all, *out;
    int n, i, selected = 0;
    if (nelements != NULL)
        *nelements = 0;
    all = glXGetFBConfigs(dpy, screen, &n);
    if (all == NULL)
        return NULL;
    out = malloc((size_t)n * sizeof(*out));
    if (out == NULL) {
        free(all);
        return NULL;
    }
    for (i = 0; i < n; ++i) {
        int match = fbconfig_matches(dpy, all[i], attrib_list);
        if (match < 0) {
            free(all);
            free(out);
            return NULL;
        }
        if (match != 0)
            out[selected++] = all[i];
    }
    free(all);
    if (selected == 0) {
        free(out);
        return NULL;
    }
    if (nelements != NULL)
        *nelements = selected;
    return out;
}

GLXFBConfig *glXChooseFBConfigSGIX(Display *dpy, int screen, const int *attrib_list,
                                   int *nelements) {
    return glXChooseFBConfig(dpy, screen, attrib_list, nelements);
}

GLXFBConfig *glXGetFBConfigs(Display *dpy, int screen, int *nelements) {
    GLXFBConfig *out;
    int n, i;
    if (nelements != NULL)
        *nelements = 0;
    if (screen < 0 ||
        (dpy != NULL && (!glx_valid_screen(dpy, screen) || screen >= MGL_GLX_SCREEN_CACHE)))
        return NULL;
    n = dpy != NULL ? MGL_GLX_CONFIG_COUNT : 1;
    out = malloc((size_t)n * sizeof(*out));
    if (out == NULL)
        return NULL;
    if (dpy != NULL) {
        init_fbconfigs(screen);
        for (i = 0; i < n; ++i)
            out[i] = &glx_configs[screen][i];
    } else {
        default_fbconfig.screen = screen;
        out[0] = &default_fbconfig;
    }
    if (nelements != NULL)
        *nelements = n;
    return out;
}

static int glx_visual_type(const Visual *visual) {
    if (visual == NULL)
        return GLX_NONE;
    switch (visual->class) {
        case TrueColor:
            return GLX_TRUE_COLOR;
        case DirectColor:
            return GLX_DIRECT_COLOR;
        case PseudoColor:
            return GLX_PSEUDO_COLOR;
        case StaticColor:
            return GLX_STATIC_COLOR;
        case GrayScale:
            return GLX_GRAY_SCALE;
        case StaticGray:
            return GLX_STATIC_GRAY;
        default:
            return GLX_NONE;
    }
}

int glXGetFBConfigAttrib(Display *dpy, GLXFBConfig config, int attribute, int *value) {
    int result, screen;
    Visual *visual = NULL;
    if (value == NULL || !glx_config_valid(config))
        return GLX_BAD_ATTRIBUTE;
    screen = config->screen;
    if (dpy != NULL) {
        if (!glx_valid_screen(dpy, screen))
            return GLX_BAD_ATTRIBUTE;
        visual = DefaultVisual(dpy, screen);
    }
    switch (attribute) {
        case GLX_FBCONFIG_ID:
            result = config->id;
            break;
        case GLX_BUFFER_SIZE:
            result = config->red + config->green + config->blue + config->alpha;
            break;
        case GLX_LEVEL:
            result = 0;
            break;
        case GLX_DOUBLEBUFFER:
            result = config->double_buffer;
            break;
        case GLX_STEREO:
            result = False;
            break;
        case GLX_AUX_BUFFERS:
            result = 0;
            break;
        case GLX_RED_SIZE:
            result = config->red;
            break;
        case GLX_GREEN_SIZE:
            result = config->green;
            break;
        case GLX_BLUE_SIZE:
            result = config->blue;
            break;
        case GLX_ALPHA_SIZE:
            result = config->alpha;
            break;
        case GLX_DEPTH_SIZE:
            result = config->depth;
            break;
        case GLX_STENCIL_SIZE:
            result = config->stencil;
            break;
        case GLX_ACCUM_RED_SIZE:
        case GLX_ACCUM_GREEN_SIZE:
        case GLX_ACCUM_BLUE_SIZE:
        case GLX_ACCUM_ALPHA_SIZE:
            result = 0;
            break;
        case GLX_SAMPLE_BUFFERS:
            result = config->sample_buffers;
            break;
        case GLX_SAMPLES:
            result = config->samples;
            break;
        case GLX_RENDER_TYPE:
            result = GLX_RGBA_BIT;
            break;
        case GLX_DRAWABLE_TYPE:
            result = config->drawable_type;
            break;
        case GLX_X_RENDERABLE:
            result = config->x_renderable;
            break;
        case GLX_X_VISUAL_TYPE:
            result = config->x_renderable ? glx_visual_type(visual) : GLX_NONE;
            break;
        case GLX_CONFIG_CAVEAT:
            result = dpy == NULL ? GLX_NON_CONFORMANT_CONFIG : GLX_NONE;
            break;
        case GLX_TRANSPARENT_TYPE:
            result = GLX_NONE;
            break;
        case GLX_TRANSPARENT_INDEX_VALUE:
        case GLX_TRANSPARENT_RED_VALUE:
        case GLX_TRANSPARENT_GREEN_VALUE:
        case GLX_TRANSPARENT_BLUE_VALUE:
        case GLX_TRANSPARENT_ALPHA_VALUE:
            result = 0;
            break;
        case GLX_MAX_PBUFFER_WIDTH:
        case GLX_MAX_PBUFFER_HEIGHT:
            result = 16384;
            break;
        case GLX_MAX_PBUFFER_PIXELS:
            result = 16384 * 16384;
            break;
        case GLX_VISUAL_ID:
            result = config->x_renderable && visual != NULL ? (int)XVisualIDFromVisual(visual) : 0;
            break;
        default:
            return GLX_BAD_ATTRIBUTE;
    }
    *value = result;
    return Success;
}

int glXQueryContext(Display *dpy, GLXContext ctx, int attribute, int *value) {
    GLXContext live;
    int result;
    (void)pthread_mutex_lock(&glx_context_lifetime_lock);
    live = find_live_context_locked(ctx);
    if (live == NULL || live->display != dpy || live->destroy_pending) {
        (void)pthread_mutex_unlock(&glx_context_lifetime_lock);
        return GLX_BAD_CONTEXT;
    }
    switch (attribute) {
        case GLX_FBCONFIG_ID:
            result = live->fbconfig_id;
            break;
        case GLX_RENDER_TYPE:
            result = live->render_type;
            break;
        case GLX_SCREEN:
            result = live->screen;
            break;
        default:
            (void)pthread_mutex_unlock(&glx_context_lifetime_lock);
            return GLX_BAD_ATTRIBUTE;
    }
    if (value != NULL)
        *value = result;
    (void)pthread_mutex_unlock(&glx_context_lifetime_lock);
    return 0;
}

XVisualInfo *glXGetVisualFromFBConfig(Display *dpy, GLXFBConfig config) {
    XVisualInfo templ;
    int count = 0;
    if (dpy == NULL || !glx_config_valid(config) || !config->x_renderable ||
        !glx_valid_screen(dpy, config->screen))
        return NULL;
    memset(&templ, 0, sizeof(templ));
    templ.screen = config->screen;
    templ.visualid = XVisualIDFromVisual(DefaultVisual(dpy, config->screen));
    return XGetVisualInfo(dpy, VisualScreenMask | VisualIDMask, &templ, &count);
}

GLXWindow glXCreateWindow(Display *dpy, GLXFBConfig config, Window win, const int *attrib_list) {
    int screen;
    VisualID visual_id;
    unsigned width, height;
    GLXWindow window;
    if (dpy == NULL)
        return 0;
    if (!glx_config_valid(config)) {
        glx_send_error(dpy, 0, 9, 0, 31);
        return 0;
    }
    if (!(config->drawable_type & GLX_WINDOW_BIT) || !config->x_renderable ||
        (attrib_list != NULL && attrib_list[0] != None)) {
        glx_send_error(dpy, 1, BadMatch, (unsigned long)win, 31);
        return 0;
    }
    if (!mgl_glx_native_window_info(dpy, win, &screen, &visual_id, &width, &height)) {
        glx_send_error(dpy, 1, BadWindow, (unsigned long)win, 31);
        return 0;
    }
    if (screen != config->screen || visual_id != XVisualIDFromVisual(DefaultVisual(dpy, screen))) {
        glx_send_error(dpy, 1, BadMatch, (unsigned long)win, 31);
        return 0;
    }
    if (mgl_glx_window_has_config(dpy, win)) {
        glx_send_error(dpy, 1, BadAlloc, (unsigned long)win, 31);
        return 0;
    }
    window = (GLXWindow)mgl_glx_drawable_create(dpy, MGL_GLX_DRAWABLE_WINDOW, win, width, height);
    if (window == 0)
        glx_send_error(dpy, 1, BadAlloc, (unsigned long)win, 31);
    else
        (void)mgl_glx_drawable_set_format(dpy, window, screen, config->id, visual_id);
    return window;
}

void glXDestroyWindow(Display *dpy, GLXWindow window) {
    mgl_android_present_unregister(dpy, window);
    mgl_glx_drawable_destroy(dpy, window);
}

int wine_android_register_glx_window(void *display, unsigned long drawable, uint64_t hwnd,
                                     uint32_t whole_window) {
    Display *dpy = (Display *)display;
    enum mgl_glx_drawable_kind kind;
    Drawable native_drawable;
    if (dpy == NULL || drawable == 0ul || hwnd == 0u)
        return 0;
    if (!mgl_glx_drawable_native_identity(dpy, (GLXDrawable)drawable, &kind, &native_drawable) ||
        kind != MGL_GLX_DRAWABLE_WINDOW)
        return 0;
    if (!mgl_android_present_register(dpy, (GLXDrawable)drawable, native_drawable, hwnd,
                                      whole_window))
        return 0;
    (void)mgl_glx_drawable_watch(dpy, (GLXDrawable)drawable);
    /* Direct submission is asynchronous; preserve explicit application swap intervals. */
    (void)mgl_glx_drawable_set_initial_swap_interval(dpy, (GLXDrawable)drawable, 0);
    return 1;
}

static int parse_pbuffer_attributes(const int *attrib_list, unsigned *width, unsigned *height,
                                    int *preserved, int *largest) {
    const int *p;
    *width = 1u;
    *height = 1u;
    *preserved = True;
    *largest = False;
    for (p = attrib_list; p != NULL && p[0] != None; p += 2) {
        switch (p[0]) {
            case GLX_PBUFFER_WIDTH:
                if (p[1] <= 0)
                    return 0;
                *width = (unsigned)p[1];
                break;
            case GLX_PBUFFER_HEIGHT:
                if (p[1] <= 0)
                    return 0;
                *height = (unsigned)p[1];
                break;
            case GLX_PRESERVED_CONTENTS:
                if (p[1] != False && p[1] != True)
                    return 0;
                *preserved = p[1];
                break;
            case GLX_LARGEST_PBUFFER:
                if (p[1] != False && p[1] != True)
                    return 0;
                *largest = p[1];
                break;
            default:
                return 0;
        }
    }
    return 1;
}

GLXPbuffer glXCreatePbuffer(Display *dpy, GLXFBConfig config, const int *attrib_list) {
    unsigned width, height;
    int preserved, largest;
    GLXPbuffer pbuffer;
    if (config != NULL && !glx_config_valid(config)) {
        glx_send_error(dpy, 0, 9, 0, 27);
        return 0;
    }
    if (config != NULL && !(config->drawable_type & GLX_PBUFFER_BIT)) {
        glx_send_error(dpy, 1, BadMatch, 0, 27);
        return 0;
    }
    if (!parse_pbuffer_attributes(attrib_list, &width, &height, &preserved, &largest)) {
        glx_send_error(dpy, 1, BadValue, 0, 27);
        return 0;
    }
    pbuffer = (GLXPbuffer)mgl_glx_drawable_create(dpy, MGL_GLX_DRAWABLE_PBUFFER, 0, width, height);
    if (pbuffer != 0) {
        (void)mgl_glx_drawable_set_format(dpy, pbuffer, config != NULL ? config->screen : 0,
                                          config != NULL ? config->id : default_fbconfig.id, 0);
        (void)mgl_glx_drawable_set_pbuffer_attributes(dpy, pbuffer, preserved, largest);
    }
    return pbuffer;
}

void glXDestroyPbuffer(Display *dpy, GLXPbuffer pbuffer) {
    mgl_glx_drawable_destroy(dpy, pbuffer);
}

static GLXPixmap create_glx_pixmap_record(Display *dpy, Pixmap pixmap, int screen, int fbconfig_id,
                                          int required_depth) {
    int actual_screen;
    unsigned width, height, depth;
    GLXPixmap glx_pixmap;
    if (!mgl_glx_native_pixmap_info(dpy, pixmap, &actual_screen, &width, &height, &depth) ||
        (screen >= 0 && actual_screen != screen) ||
        (required_depth > 0 && depth != (unsigned)required_depth))
        return 0;
    glx_pixmap =
        (GLXPixmap)mgl_glx_drawable_create(dpy, MGL_GLX_DRAWABLE_PIXMAP, pixmap, width, height);
    if (glx_pixmap != 0)
        (void)mgl_glx_drawable_set_format(dpy, glx_pixmap, actual_screen, fbconfig_id, 0);
    return glx_pixmap;
}

GLXPixmap glXCreatePixmap(Display *dpy, GLXFBConfig config, Pixmap pixmap, const int *attrib_list) {
    if (dpy == NULL || !glx_config_valid(config) || !(config->drawable_type & GLX_PIXMAP_BIT) ||
        (attrib_list != NULL && attrib_list[0] != None))
        return 0;
    return create_glx_pixmap_record(dpy, pixmap, config->screen, config->id,
                                    config->x_renderable ? DefaultDepth(dpy, config->screen) : 0);
}

void glXDestroyPixmap(Display *dpy, GLXPixmap pixmap) {
    mgl_glx_drawable_destroy(dpy, pixmap);
}

GLXPixmap glXCreateGLXPixmap(Display *dpy, XVisualInfo *vis, Pixmap pixmap) {
    if (dpy == NULL || vis == NULL || !glx_valid_screen(dpy, vis->screen) ||
        vis->visualid != XVisualIDFromVisual(DefaultVisual(dpy, vis->screen)))
        return 0;
    return create_glx_pixmap_record(dpy, pixmap, vis->screen, 1, vis->depth);
}

void glXDestroyGLXPixmap(Display *dpy, GLXPixmap pixmap) {
    glXDestroyPixmap(dpy, pixmap);
}

void glXQueryDrawable(Display *dpy, GLXDrawable draw, int attribute, unsigned int *value) {
    enum mgl_glx_drawable_kind kind;
    unsigned width, height;
    int interval, fbconfig_id, preserved, largest;
    if (value == NULL)
        return;
    if (attribute == GLX_WIDTH || attribute == GLX_HEIGHT)
        (void)mgl_glx_drawable_refresh(dpy, draw);
    if (!mgl_glx_drawable_info(dpy, draw, &kind, NULL, &width, &height, &interval)) {
        glx_send_error(dpy, 0, 2, (unsigned long)draw, 29);
        return;
    }
    switch (attribute) {
        case GLX_WIDTH:
            *value = width;
            break;
        case GLX_HEIGHT:
            *value = height;
            break;
        case GLX_FBCONFIG_ID:
            if (!mgl_glx_drawable_get_format(dpy, draw, NULL, &fbconfig_id, NULL))
                return;
            *value = (unsigned)fbconfig_id;
            break;
        case GLX_PRESERVED_CONTENTS:
        case GLX_LARGEST_PBUFFER:
            if (kind == MGL_GLX_DRAWABLE_PBUFFER &&
                mgl_glx_drawable_get_pbuffer_attributes(dpy, draw, &preserved, &largest))
                *value = (unsigned)(attribute == GLX_PRESERVED_CONTENTS ? preserved : largest);
            break;
        case GLX_SWAP_INTERVAL_EXT:
            *value = interval < 0 ? 0u : (unsigned)interval;
            break;
        default:
            glx_send_error(dpy, 1, BadValue, (unsigned long)attribute, 29);
            break;
    }
}

void glXSelectEvent(Display *dpy, GLXDrawable draw, unsigned long event_mask) {
    if ((event_mask & ~GLX_PBUFFER_CLOBBER_MASK) != 0ul) {
        glx_send_error(dpy, 1, BadValue, event_mask, 30);
        return;
    }
    if (!mgl_glx_drawable_set_event_mask(dpy, draw, event_mask))
        glx_send_error(dpy, 0, 2, (unsigned long)draw, 30);
}

void glXGetSelectedEvent(Display *dpy, GLXDrawable draw, unsigned long *event_mask) {
    if (event_mask == NULL)
        return;
    if (!mgl_glx_drawable_get_event_mask(dpy, draw, event_mask))
        glx_send_error(dpy, 0, 2, (unsigned long)draw, 29);
}

void glXSwapIntervalEXT(Display *dpy, GLXDrawable drawable, int interval) {
    if (interval < 0)
        return;
    if (!mgl_glx_drawable_set_swap_interval(dpy, drawable, interval))
        return;
    if (mgl_context_current() != NULL && tls_glx_context != NULL && tls_display == dpy &&
        tls_drawable == drawable) {
        apply_current_swap_interval(dpy, drawable, tls_glx_context);
    }
}

int glXSwapIntervalMESA(unsigned int interval) {
    if (tls_glx_context == NULL)
        return 1;
    glXSwapIntervalEXT(tls_display, tls_drawable, (int)interval);
    return 0;
}

int glXGetSwapIntervalMESA(void) {
    int interval = 0;
    if (tls_glx_context == NULL)
        return 0;
    if (!mgl_glx_drawable_info(tls_display, tls_drawable, NULL, NULL, NULL, NULL, &interval))
        return 0;
    return interval;
}

int glXSwapIntervalSGI(int interval) {
    if (interval <= 0 || tls_glx_context == NULL)
        return 1;
    glXSwapIntervalEXT(tls_display, tls_drawable, interval);
    return 0;
}

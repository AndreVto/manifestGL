#define _POSIX_C_SOURCE 200809L
#include "presentation/android_direct.h"

#include "core/context.h"
#include "core/render_diag.h"
#include "presentation/android_ahb_presentation.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MGL_PRESENT_CONTROL_MAGIC 0x57505243u
#define MGL_PRESENT_CONTROL_VERSION 6u
#define MGL_PRESENT_CONTROL_FLAG_ACTIVE 0x1u
#define MGL_PRESENT_CONTROL_FLAG_NATIVE_PRESENT 0x2u

struct mgl_present_control_shared {
    uint32_t magic;
    uint32_t version;
    uint32_t target_fps_millihz;
    uint32_t flags;
    uint64_t session_id;
    uint64_t present_count;
    uint64_t last_present_ns;
    uint64_t reserved[4];
};

struct mgl_android_present {
    struct mgl_android_present *next;
    Display *display;
    GLXDrawable drawable;
    Drawable native_drawable;
    uint64_t hwnd;
    uint32_t whole_window;
    uint64_t owner;
    unsigned refs;
    int registered;
    int requested;
    int invalid;
    int ever_window;
    int width;
    int height;
    int swap_interval;
    void *native_window;
    struct mgl_native_dispatch *native_dispatch;
    MGL_EGLDisplay egl_display;
    MGL_EGLConfig egl_config;
    MGL_EGLSurface egl_surface;
    struct mgl_ahb_presentation *ahb_presentation;
    struct mgl_context *ahb_presentation_owner;
    int ahb_presentation_probed;
    uint64_t session_id;
    uint64_t presents;
    uint64_t limiter_deadline_ns;
    uint32_t limiter_target_millihz;
};

struct mgl_android_bridge {
    void *library;
    void *(*acquire)(uint64_t, uint32_t, uint32_t, uint64_t, uint32_t);
    void *(*peek)(uint64_t, uint64_t);
    void (*release)(void *);
    void (*retire)(uint64_t, uint64_t);
    int enabled;
    int ready;
    int swap_interval_override_set;
    int swap_interval_override;
    struct mgl_present_control_shared *control;
};

static pthread_once_t bridge_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t present_lock = PTHREAD_MUTEX_INITIALIZER;
static struct mgl_android_bridge bridge;
static struct mgl_android_present *presentations;
/* Owner IDs use the 0x474c namespace to reject callbacks from retired generations. */
static atomic_uint_fast64_t next_owner = ATOMIC_VAR_INIT(UINT64_C(0x474c000000000001));

static uint64_t now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0u;
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

static int direct_present_explicitly_disabled(const char *value) {
    if (value == NULL || value[0] == '\0')
        return 0;
    return strcmp(value, "0") == 0 || strcasecmp(value, "false") == 0 ||
           strcasecmp(value, "off") == 0 || strcasecmp(value, "no") == 0;
}

static void bridge_initialize(void) {
    const char *policy = getenv("WINE_ANDROID_DIRECT_PRESENT");
    const char *path = getenv("WINE_ANDROID_DIRECT_PRESENT_LIB");
    const char *interval;
    const char *control_path;

    bridge.enabled = !direct_present_explicitly_disabled(policy);
    if (!bridge.enabled)
        return;

    if (path != NULL && path[0] != '\0') {
        bridge.library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (bridge.library != NULL) {
            *(void **)(&bridge.acquire) =
                dlsym(bridge.library, "android_direct_present_acquire_gl");
            *(void **)(&bridge.peek) = dlsym(bridge.library, "android_direct_present_peek");
            *(void **)(&bridge.release) = dlsym(bridge.library, "android_direct_present_release");
            *(void **)(&bridge.retire) = dlsym(bridge.library, "android_direct_present_retire");
        }
    } else {
        *(void **)(&bridge.acquire) = dlsym(RTLD_DEFAULT, "android_direct_present_acquire_gl");
        *(void **)(&bridge.peek) = dlsym(RTLD_DEFAULT, "android_direct_present_peek");
        *(void **)(&bridge.release) = dlsym(RTLD_DEFAULT, "android_direct_present_release");
        *(void **)(&bridge.retire) = dlsym(RTLD_DEFAULT, "android_direct_present_retire");
    }
    bridge.ready = bridge.acquire != NULL && bridge.peek != NULL && bridge.release != NULL &&
                   bridge.retire != NULL;
    if (!bridge.ready && path != NULL && path[0] != '\0') {
        (void)fprintf(stderr, "MGL_PRESENT: Android direct-present bridge ABI is unavailable\n");
    }

    interval = getenv("WINE_ANDROID_GL_SWAP_INTERVAL");
    if (interval != NULL && interval[0] != '\0') {
        char *end = NULL;
        long value = strtol(interval, &end, 10);
        if (end != interval && *end == '\0' && value >= 0 && value <= INT_MAX) {
            bridge.swap_interval_override_set = 1;
            bridge.swap_interval_override = (int)value;
        }
    }

    control_path = getenv("WINE_ANDROID_PRESENT_CONTROL_SHM");
    if (control_path != NULL && control_path[0] != '\0') {
        int fd = open(control_path, O_RDWR | O_CLOEXEC);
        if (fd >= 0) {
            struct stat st;
            if (fstat(fd, &st) == 0 && st.st_size >= (off_t)sizeof(*bridge.control)) {
                void *mapped =
                    mmap(NULL, sizeof(*bridge.control), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                if (mapped != MAP_FAILED) {
                    struct mgl_present_control_shared *control = mapped;
                    if (control->magic == MGL_PRESENT_CONTROL_MAGIC &&
                        control->version == MGL_PRESENT_CONTROL_VERSION) {
                        bridge.control = control;
                    } else {
                        (void)munmap(mapped, sizeof(*bridge.control));
                    }
                }
            }
            (void)close(fd);
        }
    }
}

static void ensure_bridge(void) {
    (void)pthread_once(&bridge_once, bridge_initialize);
}

int mgl_android_direct_enabled(void) {
    ensure_bridge();
    return bridge.enabled && bridge.ready;
}

static struct mgl_android_present *find_locked(Display *dpy, GLXDrawable drawable) {
    struct mgl_android_present *it;
    for (it = presentations; it != NULL; it = it->next) {
        if (it->display == dpy && it->drawable == drawable && it->registered)
            return it;
    }
    return NULL;
}

static struct mgl_android_present *create_present_locked(Display *dpy, GLXDrawable drawable,
                                                         Drawable native_drawable, uint64_t hwnd,
                                                         uint32_t whole_window, int registered) {
    struct mgl_android_present *present = calloc(1, sizeof(*present));
    if (present == NULL)
        return NULL;
    present->display = dpy;
    present->drawable = drawable;
    present->native_drawable = native_drawable;
    present->hwnd = hwnd;
    present->whole_window = whole_window;
    present->owner = atomic_fetch_add_explicit(&next_owner, 1u, memory_order_relaxed);
    present->refs = 1u;
    present->registered = registered;
    present->swap_interval = 1;
    present->egl_surface = MGL_EGL_NO_SURFACE;
    if (bridge.control != NULL)
        present->session_id = __atomic_load_n(&bridge.control->session_id, __ATOMIC_ACQUIRE);
    if (registered) {
        present->next = presentations;
        presentations = present;
    }
    return present;
}

static void destroy_locked(struct mgl_android_present *present) {
    struct mgl_android_present **link;
    if (present == NULL)
        return;
    link = &presentations;
    while (*link != NULL && *link != present)
        link = &(*link)->next;
    if (*link == present)
        *link = present->next;
    if (present->ahb_presentation != NULL) {
        mgl_ahb_presentation_destroy(present->ahb_presentation,
                                     mgl_context_current() == present->ahb_presentation_owner
                                         ? present->ahb_presentation_owner
                                         : NULL);
        present->ahb_presentation = NULL;
    }
    if (present->egl_surface != MGL_EGL_NO_SURFACE && present->egl_display != MGL_EGL_NO_DISPLAY &&
        present->native_dispatch != NULL && present->native_dispatch->eglDestroySurface != NULL) {
        (void)present->native_dispatch->eglDestroySurface(present->egl_display,
                                                          present->egl_surface);
    }
    if (present->native_window != NULL && bridge.release != NULL)
        bridge.release(present->native_window);
    if (present->requested && bridge.retire != NULL)
        bridge.retire(present->hwnd, present->owner);
    free(present);
}

int mgl_android_present_register(Display *dpy, GLXDrawable drawable, Drawable native_drawable,
                                 uint64_t hwnd, uint32_t whole_window) {
    struct mgl_android_present *present;
    int matches;
    ensure_bridge();
    if (!bridge.enabled || !bridge.ready || dpy == NULL || drawable == 0 || hwnd == 0u)
        return 0;

    (void)pthread_mutex_lock(&present_lock);
    present = find_locked(dpy, drawable);
    if (present != NULL) {
        matches = present->hwnd == hwnd && present->whole_window == whole_window;
        (void)pthread_mutex_unlock(&present_lock);
        return matches;
    }
    present = create_present_locked(dpy, drawable,
                                    native_drawable != 0 ? native_drawable : (Drawable)drawable,
                                    hwnd, whole_window, 1);
    if (present == NULL) {
        (void)pthread_mutex_unlock(&present_lock);
        return 0;
    }
    (void)pthread_mutex_unlock(&present_lock);
    return 1;
}

struct mgl_android_present *mgl_android_present_create(Display *dpy, Drawable native_drawable,
                                                       uint64_t hwnd, uint32_t whole_window) {
    struct mgl_android_present *present;
    ensure_bridge();
    if (!bridge.enabled || !bridge.ready || dpy == NULL || native_drawable == 0 || hwnd == 0u)
        return NULL;
    (void)pthread_mutex_lock(&present_lock);
    present = create_present_locked(dpy, 0, native_drawable, hwnd, whole_window, 0);
    (void)pthread_mutex_unlock(&present_lock);
    return present;
}

void mgl_android_present_unregister(Display *dpy, GLXDrawable drawable) {
    struct mgl_android_present *present;
    (void)pthread_mutex_lock(&present_lock);
    present = find_locked(dpy, drawable);
    if (present != NULL) {
        present->registered = 0;
        present->drawable = 0;
        if (present->refs > 0u)
            --present->refs;
        if (present->refs == 0u)
            destroy_locked(present);
    }
    (void)pthread_mutex_unlock(&present_lock);
}

struct mgl_android_present *mgl_android_present_ref(Display *dpy, GLXDrawable drawable) {
    struct mgl_android_present *present;
    (void)pthread_mutex_lock(&present_lock);
    present = find_locked(dpy, drawable);
    if (present != NULL)
        ++present->refs;
    (void)pthread_mutex_unlock(&present_lock);
    return present;
}

void mgl_android_present_unref(struct mgl_android_present *present) {
    if (present == NULL)
        return;
    (void)pthread_mutex_lock(&present_lock);
    if (present->refs > 0u)
        --present->refs;
    if (present->refs == 0u)
        destroy_locked(present);
    (void)pthread_mutex_unlock(&present_lock);
}

static int create_surface_locked(struct mgl_android_present *present, struct mgl_context *ctx,
                                 void *native_window, int width, int height) {
    MGL_EGLSurface surface = MGL_EGL_NO_SURFACE;
    if (present->ahb_presentation != NULL) {
        mgl_ahb_presentation_destroy(present->ahb_presentation, ctx);
        present->ahb_presentation = NULL;
    }
    present->ahb_presentation_probed = 0;
    if (present->egl_surface != MGL_EGL_NO_SURFACE) {
        if (ctx->native_dispatch->eglDestroySurface == NULL)
            return 0;
        (void)ctx->native_dispatch->eglDestroySurface(present->egl_display, present->egl_surface);
        present->egl_surface = MGL_EGL_NO_SURFACE;
    }
    if (present->native_window != NULL && bridge.release != NULL) {
        bridge.release(present->native_window);
        present->native_window = NULL;
    }

    if (native_window != NULL && ctx->native_dispatch->eglCreateWindowSurface != NULL) {
        const MGL_EGLint attrs[] = {MGL_EGL_NONE};
        surface = ctx->native_dispatch->eglCreateWindowSurface(
            ctx->egl_display, ctx->egl_config, (MGL_EGLNativeWindowType)native_window, attrs);
        if (surface != MGL_EGL_NO_SURFACE)
            present->ever_window = 1;
    }
    if (surface == MGL_EGL_NO_SURFACE) {
        MGL_EGLint attrs[5];
        if (native_window != NULL && !present->ever_window)
            return 0;
        /* Peek/acquire returns an owned bridge reference. Connection failure releases
         * the new reference before offscreen recovery. */
        if (native_window != NULL && bridge.release != NULL)
            bridge.release(native_window);
        native_window = NULL;
        attrs[0] = MGL_EGL_WIDTH;
        attrs[1] = width;
        attrs[2] = MGL_EGL_HEIGHT;
        attrs[3] = height;
        attrs[4] = MGL_EGL_NONE;
        surface =
            ctx->native_dispatch->eglCreatePbufferSurface(ctx->egl_display, ctx->egl_config, attrs);
    }
    if (surface == MGL_EGL_NO_SURFACE)
        return 0;

    present->native_window = native_window;
    present->native_dispatch = ctx->native_dispatch;
    present->egl_display = ctx->egl_display;
    present->egl_config = ctx->egl_config;
    present->egl_surface = surface;
    present->width = width;
    present->height = height;
    present->invalid = 0;
    present->presents = 0u;
    present->limiter_deadline_ns = 0u;
    present->limiter_target_millihz = 0u;
    (void)fprintf(stderr, "MGL_PRESENT: %s hwnd=%llx owner=%llx egl=%p\n",
                  native_window != NULL ? "Android native window surface" : "recovery pbuffer",
                  (unsigned long long)present->hwnd, (unsigned long long)present->owner, surface);
    return 1;
}

MGL_EGLSurface mgl_android_present_prepare_surface(struct mgl_android_present *present,
                                                   struct mgl_context *ctx, int width, int height) {
    void *native_window = NULL;
    MGL_EGLSurface result = MGL_EGL_NO_SURFACE;
    if (present == NULL || ctx == NULL || ctx->native_dispatch == NULL)
        return MGL_EGL_NO_SURFACE;
    ensure_bridge();
    if (!bridge.ready)
        return MGL_EGL_NO_SURFACE;
    if (width < 1)
        width = 1;
    if (height < 1)
        height = 1;

    (void)pthread_mutex_lock(&present_lock);
    if (present->egl_display != MGL_EGL_NO_DISPLAY && present->egl_display != ctx->egl_display) {
        (void)pthread_mutex_unlock(&present_lock);
        return MGL_EGL_NO_SURFACE;
    }
    if (!present->requested) {
        present->requested = 1;
        native_window = bridge.acquire(present->hwnd, (uint32_t)present->native_drawable,
                                       present->whole_window, present->owner, 5000u);
        if (native_window == NULL && !present->ever_window) {
            (void)pthread_mutex_unlock(&present_lock);
            return MGL_EGL_NO_SURFACE;
        }
    } else {
        native_window = bridge.peek(present->hwnd, present->owner);
    }

    if (present->egl_surface != MGL_EGL_NO_SURFACE && !present->invalid &&
        present->native_window == native_window && present->egl_display == ctx->egl_display &&
        present->egl_config == ctx->egl_config && present->width == width &&
        present->height == height) {
        if (native_window != NULL)
            bridge.release(native_window);
        result = present->egl_surface;
        (void)pthread_mutex_unlock(&present_lock);
        return result;
    }

    if (!create_surface_locked(present, ctx, native_window, width, height)) {
        if (native_window != NULL)
            bridge.release(native_window);
        (void)pthread_mutex_unlock(&present_lock);
        return MGL_EGL_NO_SURFACE;
    }
    result = present->egl_surface;
    (void)pthread_mutex_unlock(&present_lock);
    return result;
}

int mgl_android_present_configure(struct mgl_android_present *present, struct mgl_context *ctx,
                                  int width, int height) {
    MGL_EGLSurface surface;
    if (present == NULL || ctx == NULL)
        return 0;
    if (mgl_context_current() == ctx &&
        (mgl_android_present_changed(present) ||
         (mgl_android_present_is_window(present) &&
          (present->width != width || present->height != height))) &&
        !mgl_context_make_current(NULL))
        return 0;
    surface = mgl_android_present_prepare_surface(present, ctx, width, height);
    if (surface == MGL_EGL_NO_SURFACE)
        return 0;
    if (present->ahb_presentation && present->ahb_presentation_owner == ctx)
        ctx->native_default_framebuffer =
            mgl_ahb_presentation_framebuffer(present->ahb_presentation);
    else
        ctx->native_default_framebuffer = 0u;
    return mgl_context_use_external_surface(ctx, surface, (GLsizei)width, (GLsizei)height);
}

static int rebind_current(struct mgl_android_present *present, struct mgl_context *ctx, int width,
                          int height) {
    int requested_interval;
    if (mgl_context_current() == ctx && !mgl_context_make_current(NULL))
        return 0;
    if (!mgl_android_present_configure(present, ctx, width, height)) {
        if (!mgl_context_configure_pbuffers(ctx, (GLsizei)width, (GLsizei)height, (GLsizei)width,
                                            (GLsizei)height))
            return 0;
    }
    if (!mgl_context_make_current(ctx))
        return 0;

    /* Reapply the logical swap interval when a replacement native EGLSurface becomes current. */
    (void)pthread_mutex_lock(&present_lock);
    requested_interval = present->swap_interval;
    (void)pthread_mutex_unlock(&present_lock);
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->eglSwapInterval != NULL &&
        ctx->native_dispatch->eglSwapInterval(
            ctx->egl_display,
            (MGL_EGLint)mgl_android_effective_swap_interval(requested_interval)) != MGL_EGL_TRUE)
        return 0;
    return 1;
}

static int present_control_session_valid(const struct mgl_android_present *present);

/* AHardwareBuffer presentation paces after submission; native EGL fallback paces before swap. */
static void mgl_android_present_pace(struct mgl_android_present *present) {
    uint32_t target = 0u;
    uint64_t deadline = 0u;
    uint64_t now;
    uint64_t period;
    if (present == NULL)
        return;
    (void)pthread_mutex_lock(&present_lock);
    if (bridge.control != NULL && present_control_session_valid(present))
        target = __atomic_load_n(&bridge.control->target_fps_millihz, __ATOMIC_ACQUIRE);
    if (target == 0u) {
        present->limiter_target_millihz = 0u;
        present->limiter_deadline_ns = 0u;
        (void)pthread_mutex_unlock(&present_lock);
        return;
    }
    now = now_ns();
    if (now == 0u) {
        (void)pthread_mutex_unlock(&present_lock);
        return;
    }
    period = (UINT64_C(1000000000000) + (uint64_t)target / 2u) / (uint64_t)target;
    if (period == 0u)
        period = 1u;
    if (present->limiter_target_millihz != target || present->limiter_deadline_ns == 0u ||
        now >= present->limiter_deadline_ns) {
        present->limiter_target_millihz = target;
        present->limiter_deadline_ns = now + period;
    } else {
        deadline = present->limiter_deadline_ns;
        present->limiter_deadline_ns += period;
    }
    (void)pthread_mutex_unlock(&present_lock);
    if (deadline != 0u) {
        struct timespec ts;
        int result;
        ts.tv_sec = (time_t)(deadline / UINT64_C(1000000000));
        ts.tv_nsec = (long)(deadline % UINT64_C(1000000000));
        do {
            result = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
        } while (result == EINTR);
    }
}

int mgl_android_present_swap(struct mgl_android_present *present, struct mgl_context *ctx,
                             int width, int height) {
    uint64_t perf_started = mgl_perf_diag_now();
    if (present == NULL || ctx == NULL || ctx->native_dispatch == NULL)
        return 0;
    if (mgl_android_present_changed(present) ||
        (mgl_android_present_is_window(present) &&
         (present->width != width || present->height != height))) {
        if (present->ahb_presentation != NULL && mgl_context_current() == ctx) {
            mgl_ahb_presentation_destroy(present->ahb_presentation, ctx);
            present->ahb_presentation = NULL;
            present->ahb_presentation_probed = 0;
        }
        return rebind_current(present, ctx, width, height);
    }
    if (mgl_android_present_is_window(present)) {
        /* Initialize AHB presentation after the first successful EGL swap. */
        if (present->ahb_presentation != NULL &&
            !mgl_ahb_presentation_dimensions_match(present->ahb_presentation, width, height)) {
            (void)fprintf(
                stderr,
                "MGL_PRESENT: AHardwareBuffer presentation resize %dx%d; rebuilding image queue\n",
                width, height);
            mgl_ahb_presentation_destroy(present->ahb_presentation, ctx);
            present->ahb_presentation = NULL;
            present->ahb_presentation_probed = 0;
        }
        if (present->ahb_presentation != NULL) {
            if (mgl_ahb_presentation_present(present->ahb_presentation, ctx)) {
                mgl_android_presented(present);
                {
                    uint64_t perf_submitted = mgl_perf_diag_now();
                    mgl_android_present_pace(present);
                    mgl_perf_diag_present(ctx, perf_started, perf_submitted, mgl_perf_diag_now());
                }
                return 1;
            }
            (void)fprintf(
                stderr,
                "MGL_PRESENT: AHardwareBuffer presentation submit failed; restoring EGL surface\n");
            mgl_ahb_presentation_destroy(present->ahb_presentation, ctx);
            present->ahb_presentation = NULL;
            present->ahb_presentation_probed = 1;
            if (ctx->native_dispatch->glesBindFramebuffer != NULL)
                ctx->native_dispatch->glesBindFramebuffer(GL_FRAMEBUFFER, 0u);
        }
        mgl_android_present_pace(present);
        if (ctx->native_dispatch->eglSwapBuffers != NULL &&
            ctx->native_dispatch->eglSwapBuffers(ctx->egl_display, ctx->egl_draw) == MGL_EGL_TRUE) {
            mgl_android_presented(present);
            mgl_perf_diag_present(ctx, perf_started, mgl_perf_diag_now(), mgl_perf_diag_now());
            if (!present->ahb_presentation_probed) {
                present->ahb_presentation_probed = 1;
                present->ahb_presentation =
                    mgl_ahb_presentation_create(ctx, present->native_window, width, height);
                if (present->ahb_presentation != NULL)
                    present->ahb_presentation_owner = ctx;
            }
            return 1;
        }
        if (ctx->native_dispatch->eglGetError != NULL) {
            MGL_EGLint error = ctx->native_dispatch->eglGetError();
            if (error == MGL_EGL_BAD_SURFACE || error == MGL_EGL_BAD_NATIVE_WINDOW) {
                mgl_android_present_invalidate(present);
                return rebind_current(present, ctx, width, height);
            }
        }
        return 0;
    }
    if (ctx->native_dispatch->glesFlush != NULL)
        ctx->native_dispatch->glesFlush();
    mgl_perf_diag_present(ctx, perf_started, mgl_perf_diag_now(), mgl_perf_diag_now());
    return 1;
}

int mgl_android_present_set_swap_interval(struct mgl_android_present *present,
                                          struct mgl_context *ctx, int requested) {
    int interval;
    if (present == NULL || ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->eglSwapInterval == NULL)
        return 0;
    (void)pthread_mutex_lock(&present_lock);
    present->swap_interval = requested;
    (void)pthread_mutex_unlock(&present_lock);
    interval = mgl_android_effective_swap_interval(requested);
    return ctx->native_dispatch->eglSwapInterval(ctx->egl_display, (MGL_EGLint)interval) ==
           MGL_EGL_TRUE;
}

int mgl_android_present_changed(struct mgl_android_present *present) {
    void *window;
    int changed;
    if (present == NULL)
        return 0;
    ensure_bridge();
    (void)pthread_mutex_lock(&present_lock);
    window = present->requested && bridge.peek != NULL ? bridge.peek(present->hwnd, present->owner)
                                                       : NULL;
    changed = present->invalid || present->egl_surface == MGL_EGL_NO_SURFACE ||
              window != present->native_window;
    if (window != NULL && bridge.release != NULL)
        bridge.release(window);
    (void)pthread_mutex_unlock(&present_lock);
    return changed;
}

int mgl_android_present_is_window(struct mgl_android_present *present) {
    int result;
    if (present == NULL)
        return 0;
    (void)pthread_mutex_lock(&present_lock);
    result = present->native_window != NULL && !present->invalid;
    (void)pthread_mutex_unlock(&present_lock);
    return result;
}

void mgl_android_present_invalidate(struct mgl_android_present *present) {
    if (present == NULL)
        return;
    (void)pthread_mutex_lock(&present_lock);
    present->invalid = 1;
    (void)pthread_mutex_unlock(&present_lock);
}

static int present_control_session_valid(const struct mgl_android_present *present) {
    uint32_t flags;
    uint64_t session;
    if (bridge.control == NULL || present == NULL || present->session_id == 0u)
        return 0;
    session = __atomic_load_n(&bridge.control->session_id, __ATOMIC_ACQUIRE);
    flags = __atomic_load_n(&bridge.control->flags, __ATOMIC_ACQUIRE);
    return session == present->session_id && (flags & MGL_PRESENT_CONTROL_FLAG_ACTIVE) != 0u;
}

void mgl_android_presented(struct mgl_android_present *present) {
    uint64_t timestamp;
    if (present == NULL || !mgl_android_present_is_window(present))
        return;
    (void)pthread_mutex_lock(&present_lock);
    ++present->presents;
    if (present_control_session_valid(present)) {
        timestamp = now_ns();
        (void)__atomic_add_fetch(&bridge.control->present_count, UINT64_C(1), __ATOMIC_ACQ_REL);
        __atomic_store_n(&bridge.control->last_present_ns, timestamp, __ATOMIC_RELEASE);
        (void)__atomic_or_fetch(&bridge.control->flags, MGL_PRESENT_CONTROL_FLAG_NATIVE_PRESENT,
                                __ATOMIC_ACQ_REL);
    }
    (void)pthread_mutex_unlock(&present_lock);
}

int mgl_android_effective_swap_interval(int requested) {
    ensure_bridge();
    return bridge.swap_interval_override_set ? bridge.swap_interval_override : requested;
}

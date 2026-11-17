#define _POSIX_C_SOURCE 200809L
#include "presentation/android_ahb_presentation.h"
#include "core/render_diag.h"
#include "core/context.h"
#include "presentation/present_control_recording.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

/* Dynamically resolved NDK interfaces use opaque native types and ABI tokens. */
typedef struct AHardwareBuffer AHardwareBuffer;
typedef struct ASurfaceControl ASurfaceControl;
typedef struct ASurfaceTransaction ASurfaceTransaction;
typedef struct ASurfaceTransactionStats ASurfaceTransactionStats;

typedef struct {
    uint32_t width, height, layers, format;
    uint64_t usage;
    uint32_t stride, rfu0, rfu1;
} mgl_ahb_desc;

typedef struct {
    int32_t left, top, right, bottom;
} mgl_rect;

#define MGL_AHB_RGBA8 1u
#define MGL_AHB_GPU_SAMPLED (UINT64_C(1) << 8)
#define MGL_AHB_GPU_COLOR (UINT64_C(1) << 9)
/* Direct SurfaceControl buffer submission requires composer overlay usage. */
#define MGL_AHB_COMPOSER (UINT64_C(1) << 11)
#define MGL_EGL_NATIVE_BUFFER_ANDROID 0x3140
#define MGL_EGL_SYNC_NATIVE_FENCE_ANDROID 0x3144
#define MGL_EGL_SYNC_NATIVE_FENCE_FD_ANDROID 0x3145
#define MGL_AHB_MIN_IMAGES 3u
#define MGL_AHB_DEFAULT_IMAGES 5u
#define MGL_AHB_MAX_IMAGES 64u

static unsigned mgl_ahb_image_count(void) {
    const char *configured = getenv("WRAPPER_MAX_IMAGE_COUNT");
    char *end = NULL;
    unsigned long requested;
    if (!configured || !configured[0])
        return MGL_AHB_DEFAULT_IMAGES;
    errno = 0;
    requested = strtoul(configured, &end, 10);
    if (errno || end == configured || *end || requested == 0 || requested > MGL_AHB_MAX_IMAGES)
        return MGL_AHB_DEFAULT_IMAGES;
    return requested > MGL_AHB_MIN_IMAGES ? (unsigned)requested : MGL_AHB_MIN_IMAGES;
}

/* GLES FBO pixels have a bottom-left origin; SurfaceFlinger buffers have a
 * top-left origin. Mirror vertically at composition, without a GPU blit. */
#define MGL_NATIVE_WINDOW_TRANSFORM_FLIP_V 0x02

struct mgl_sc_api {
    void *lib;
    int (*allocate)(const mgl_ahb_desc *, AHardwareBuffer **);
    void (*release)(AHardwareBuffer *);
    ASurfaceControl *(*create_control)(void *, const char *);
    void (*release_control)(ASurfaceControl *);
    ASurfaceTransaction *(*create_tx)(void);
    void (*delete_tx)(ASurfaceTransaction *);
    void (*apply_tx)(ASurfaceTransaction *);
    void (*visibility)(ASurfaceTransaction *, ASurfaceControl *, int8_t);
    void (*z_order)(ASurfaceTransaction *, ASurfaceControl *, int32_t);
    void (*geometry)(ASurfaceTransaction *, ASurfaceControl *, const mgl_rect *, const mgl_rect *,
                     int32_t);
    void (*transparency)(ASurfaceTransaction *, ASurfaceControl *, int8_t);
    void (*set_buffer)(ASurfaceTransaction *, ASurfaceControl *, AHardwareBuffer *, int);
    void (*reparent)(ASurfaceTransaction *, ASurfaceControl *, ASurfaceControl *);
    void (*on_complete)(ASurfaceTransaction *, void *,
                        void (*)(void *, ASurfaceTransactionStats *));
    int (*release_fence)(ASurfaceTransactionStats *, ASurfaceControl *);
};

struct mgl_ahb_image {
    AHardwareBuffer *buffer;
    void *egl_image;
    GLuint color_renderbuffer;
    GLuint depth_renderbuffer;
    GLuint framebuffer;
    int submitted;
    int release_pending;
    int release_fence_fd;
    int record_release_fd;
    int screenshot_release_fd;
};

struct mgl_ahb_submission {
    int index;
    int acquire_fd;
};

struct mgl_ahb_presentation {
    struct mgl_sc_api sc;
    struct mgl_native_dispatch *native_dispatch;
    MGL_EGLDisplay display;
    void *(*get_client_buffer)(AHardwareBuffer *);
    void *(*create_image)(MGL_EGLDisplay, MGL_EGLContext, unsigned, void *, const MGL_EGLint *);
    MGL_EGLBoolean (*destroy_image)(MGL_EGLDisplay, void *);
    void (*image_renderbuffer)(GLenum, void *);
    void *(*create_sync)(MGL_EGLDisplay, unsigned, const MGL_EGLint *);
    MGL_EGLBoolean (*destroy_sync)(MGL_EGLDisplay, void *);
    int (*dup_fence)(MGL_EGLDisplay, void *);
    ASurfaceControl *layer;
    struct mgl_ahb_image *images;
    unsigned image_count;
    pthread_mutex_t mutex;
    pthread_cond_t submit_condition;
    pthread_t submit_thread;
    struct mgl_ahb_submission submit_queue[MGL_AHB_MAX_IMAGES + 1u];
    unsigned submit_head;
    unsigned submit_count;
    int submit_condition_initialized;
    int submit_thread_started;
    int submit_thread_stop;
    uint64_t submit_prepare_ns;
    uint64_t submit_prepare_max_ns;
    uint64_t submit_apply_ns;
    uint64_t submit_apply_max_ns;
    unsigned submit_apply_calls;
    int wake_fd;
    unsigned next_scan;
    unsigned pending_callbacks;
    int current;
    int last_submitted;
    int width, height;
    int stopping;
    struct wine_present_control_shared *record_control;
    uint64_t record_session;
    uint64_t record_generation;
    uint32_t native_transform;
    uint64_t record_last_start_logged;
    int record_invalid_logged;
    int record_attach_error_logged;
};

struct mgl_ahb_completion {
    struct mgl_ahb_presentation *chain;
    int previous_image;
};

#define RESOLVE_LIB(field, name)                                                                   \
    do {                                                                                           \
        *(void **)(&chain->sc.field) = dlsym(chain->sc.lib, name);                                 \
        if (!chain->sc.field)                                                                      \
            goto fail;                                                                             \
    } while (0)
#define RESOLVE_EGL(field, name)                                                                   \
    do {                                                                                           \
        if (ctx->native_dispatch->eglGetProcAddress) {                                             \
            mgl_egl_proc_t p = ctx->native_dispatch->eglGetProcAddress(name);                      \
            memcpy(&chain->field, &p, sizeof(chain->field));                                       \
        }                                                                                          \
        if (!chain->field)                                                                         \
            goto fail;                                                                             \
    } while (0)

static int fence_ready(int fd) {
    struct pollfd p = {.fd = fd, .events = POLLIN};
    int rc;
    if (fd < 0)
        return 1;
    do {
        rc = poll(&p, 1, 0);
    } while (rc < 0 && errno == EINTR);
    return rc > 0 && (p.revents & (POLLIN | POLLHUP | POLLERR)) != 0;
}

typedef int (*mgl_record_start_fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint64_t);
typedef int (*mgl_record_attach_fn)(uint64_t, uint32_t, uint32_t, uint32_t);
typedef int (*mgl_record_detach_fn)(uint64_t);
typedef int (*mgl_record_offer_fn)(AHardwareBuffer *, int, int *, uint64_t, uint32_t);
typedef int (*mgl_screenshot_fn)(AHardwareBuffer *, int, int *, uint32_t, uint64_t);

static struct {
    void *library;
    mgl_record_start_fn start;
    mgl_record_attach_fn attach;
    mgl_record_detach_fn detach;
    mgl_record_offer_fn offer;
    mgl_screenshot_fn screenshot;
} mgl_record;

static pthread_once_t mgl_record_once = PTHREAD_ONCE_INIT;

static void mgl_record_load(void) {
    const char *path = getenv("WINE_ANDROID_DIRECT_PRESENT_LIB");
    if (!path || !path[0]) {
        fprintf(stderr,
                "MGL_RECORD: bridge unavailable pid=%ld reason=missing "
                "WINE_ANDROID_DIRECT_PRESENT_LIB\n",
                (long)getpid());
        return;
    }
    mgl_record.library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!mgl_record.library) {
        fprintf(stderr, "MGL_RECORD: bridge dlopen failed pid=%ld path=%s error=%s\n",
                (long)getpid(), path, dlerror());
        return;
    }
    *(void **)(&mgl_record.start) =
        dlsym(mgl_record.library, "android_direct_present_record_start");
    *(void **)(&mgl_record.attach) =
        dlsym(mgl_record.library, "android_direct_present_record_attach_source");
    *(void **)(&mgl_record.detach) =
        dlsym(mgl_record.library, "android_direct_present_record_detach_source");
    *(void **)(&mgl_record.offer) =
        dlsym(mgl_record.library, "android_direct_present_record_offer");
    *(void **)(&mgl_record.screenshot) =
        dlsym(mgl_record.library, "android_direct_present_screenshot");
    fprintf(stderr,
            "MGL_RECORD: bridge loaded pid=%ld start=%d attach=%d detach=%d offer=%d path=%s\n",
            (long)getpid(), mgl_record.start != NULL, mgl_record.attach != NULL,
            mgl_record.detach != NULL, mgl_record.offer != NULL, path);
}

static struct wine_present_control_shared *mgl_record_control(struct mgl_ahb_presentation *chain) {
    struct wine_present_control_shared *control = chain->record_control;
    if (!control || !chain->record_session || !chain->record_generation ||
        wine_present_control_load_u64(&control->session_id) != chain->record_session ||
        !(wine_present_control_load_u32(&control->flags) & WINE_PRESENT_CONTROL_FLAG_ACTIVE))
        return NULL;
    return control;
}

static void mgl_record_initialize(struct mgl_ahb_presentation *chain) {
    const char *path = getenv("WINE_ANDROID_PRESENT_CONTROL_SHM");
    struct stat stat_buf;
    int fd;
    void *mapped;
    if (!path || !path[0]) {
        fprintf(stderr,
                "MGL_RECORD: shared control unavailable pid=%ld size=%dx%d reason=missing "
                "WINE_ANDROID_PRESENT_CONTROL_SHM\n",
                (long)getpid(), chain->width, chain->height);
        return;
    }
    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr,
                "MGL_RECORD: shared control open failed pid=%ld size=%dx%d errno=%d path=%s\n",
                (long)getpid(), chain->width, chain->height, errno, path);
        return;
    }
    if (fstat(fd, &stat_buf) != 0) {
        fprintf(stderr, "MGL_RECORD: shared control fstat failed pid=%ld errno=%d\n",
                (long)getpid(), errno);
        close(fd);
        return;
    }
    if (stat_buf.st_size < (off_t)sizeof(struct wine_present_control_shared)) {
        fprintf(stderr, "MGL_RECORD: shared control size invalid pid=%ld got=%lld required=%zu\n",
                (long)getpid(), (long long)stat_buf.st_size,
                sizeof(struct wine_present_control_shared));
        close(fd);
        return;
    }
    mapped = mmap(NULL, sizeof(struct wine_present_control_shared), PROT_READ | PROT_WRITE,
                  MAP_SHARED, fd, 0);
    close(fd);
    if (mapped == MAP_FAILED) {
        fprintf(stderr, "MGL_RECORD: shared control mmap failed pid=%ld errno=%d\n", (long)getpid(),
                errno);
        return;
    }
    chain->record_control = mapped;
    if (chain->record_control->magic != WINE_PRESENT_CONTROL_MAGIC ||
        chain->record_control->version != WINE_PRESENT_CONTROL_VERSION ||
        !(wine_present_control_load_u32(&chain->record_control->flags) &
          WINE_PRESENT_CONTROL_FLAG_ACTIVE)) {
        fprintf(stderr,
                "MGL_RECORD: shared control rejected pid=%ld magic=0x%08x version=%u flags=0x%x "
                "expected_version=%u\n",
                (long)getpid(), chain->record_control->magic, chain->record_control->version,
                wine_present_control_load_u32(&chain->record_control->flags),
                WINE_PRESENT_CONTROL_VERSION);
        munmap(mapped, sizeof(struct wine_present_control_shared));
        chain->record_control = NULL;
        return;
    }
    chain->record_session = wine_present_control_load_u64(&chain->record_control->session_id);
    chain->record_generation = wine_present_control_add_u64(
        &chain->record_control->reserved[WINE_PRESENT_CONTROL_SC_SWAPCHAIN_GENERATION_INDEX], 1);
    fprintf(
        stderr,
        "MGL_RECORD: shared control ready pid=%ld size=%dx%d session=%llu generation=%llu "
        "command_serial=%llu ack_serial=%llu state=%u source_generation=%llu\n",
        (long)getpid(), chain->width, chain->height, (unsigned long long)chain->record_session,
        (unsigned long long)chain->record_generation,
        (unsigned long long)wine_present_control_load_u64(
            &chain->record_control->record_command_serial),
        (unsigned long long)wine_present_control_load_u64(
            &chain->record_control->record_ack_serial),
        wine_present_control_load_u32(&chain->record_control->record_state),
        (unsigned long long)wine_present_control_load_u64(
            &chain->record_control->reserved[WINE_PRESENT_CONTROL_RECORD_SOURCE_GENERATION_INDEX]));
}

static void mgl_record_detach(struct mgl_ahb_presentation *chain) {
    struct wine_present_control_shared *control = mgl_record_control(chain);
    if (!control)
        return;
    uint64_t expected = chain->record_generation;
    if (!__atomic_compare_exchange_n(
            &control->reserved[WINE_PRESENT_CONTROL_RECORD_SOURCE_GENERATION_INDEX], &expected, 0,
            0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return;
    (void)pthread_once(&mgl_record_once, mgl_record_load);
    if (mgl_record.detach)
        (void)mgl_record.detach(chain->record_generation);
}

static void mgl_record_update(struct mgl_ahb_presentation *chain) {
    struct wine_present_control_shared *control = mgl_record_control(chain);
    uint32_t state, command;
    uint64_t serial, expected;
    if (!control) {
        if (!chain->record_invalid_logged) {
            fprintf(
                stderr,
                "MGL_RECORD: recording control unavailable during present pid=%ld size=%dx%d "
                "mapped=%d local_session=%llu shared_session=%llu generation=%llu flags=0x%x\n",
                (long)getpid(), chain->width, chain->height, chain->record_control != NULL,
                (unsigned long long)chain->record_session,
                (unsigned long long)(chain->record_control ? wine_present_control_load_u64(
                                                                 &chain->record_control->session_id)
                                                           : 0),
                (unsigned long long)chain->record_generation,
                chain->record_control ? wine_present_control_load_u32(&chain->record_control->flags)
                                      : 0);
            chain->record_invalid_logged = 1;
        }
        return;
    }
    (void)pthread_once(&mgl_record_once, mgl_record_load);
    serial = wine_present_control_load_u64(&control->record_command_serial);
    command = wine_present_control_load_u32(&control->record_command);
    if (serial && command == WINE_PRESENT_RECORD_COMMAND_START) {
        uint64_t ack = wine_present_control_load_u64(&control->record_ack_serial);
        if (chain->record_last_start_logged != serial) {
            chain->record_last_start_logged = serial;
            fprintf(stderr,
                    "MGL_RECORD: START observed pid=%ld size=%dx%d session=%llu generation=%llu "
                    "serial=%llu ack=%llu state=%u owner=%llu\n",
                    (long)getpid(), chain->width, chain->height,
                    (unsigned long long)chain->record_session,
                    (unsigned long long)chain->record_generation, (unsigned long long)serial,
                    (unsigned long long)ack, wine_present_control_load_u32(&control->record_state),
                    (unsigned long long)wine_present_control_load_u64(
                        &control->reserved[WINE_PRESENT_CONTROL_RECORD_SOURCE_GENERATION_INDEX]));
        }
        if (serial != ack) {
            state = wine_present_control_load_u32(&control->record_state);
            if (state == WINE_PRESENT_RECORD_STATE_IDLE || state == WINE_PRESENT_RECORD_STATE_ERROR)
                mgl_record_detach(chain);
            if (!__atomic_compare_exchange_n(&control->record_ack_serial, &ack, serial, 0,
                                             __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
                fprintf(stderr,
                        "MGL_RECORD: START ack claim lost pid=%ld generation=%llu serial=%llu "
                        "current_ack=%llu\n",
                        (long)getpid(), (unsigned long long)chain->record_generation,
                        (unsigned long long)serial, (unsigned long long)ack);
                return;
            }
            expected = 0;
            int rc = -EBUSY;
            const int bridge_ready =
                mgl_record.start && mgl_record.attach && mgl_record.detach && mgl_record.offer;
            if (bridge_ready &&
                __atomic_compare_exchange_n(
                    &control->reserved[WINE_PRESENT_CONTROL_RECORD_SOURCE_GENERATION_INDEX],
                    &expected, chain->record_generation, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
                uint32_t fps = wine_present_control_load_u32(&control->record_target_fps_millihz);
                fprintf(stderr,
                        "MGL_RECORD: START invoking recorder pid=%ld generation=%llu size=%dx%d "
                        "fps_millihz=%u transform=%u\n",
                        (long)getpid(), (unsigned long long)chain->record_generation, chain->width,
                        chain->height, fps ? fps : 60000u, chain->native_transform);
                wine_present_control_store_u32(&control->record_error_code, 0);
                wine_present_control_store_u32(&control->record_state,
                                               WINE_PRESENT_RECORD_STATE_STARTING);
                rc = mgl_record.start((uint32_t)chain->width, (uint32_t)chain->height,
                                      fps ? fps : 60000u, chain->native_transform,
                                      chain->record_generation);
                if (rc)
                    mgl_record_detach(chain);
            } else if (!bridge_ready) {
                rc = -ENOSYS;
            }
            if (rc) {
                wine_present_control_store_u32(&control->record_error_code, (uint32_t)-rc);
                wine_present_control_store_u32(&control->record_state,
                                               WINE_PRESENT_RECORD_STATE_ERROR);
            }
            fprintf(stderr,
                    "MGL_RECORD: START result pid=%ld generation=%llu serial=%llu rc=%d state=%u "
                    "error=%u owner=%llu bridge_ready=%d prior_owner=%llu\n",
                    (long)getpid(), (unsigned long long)chain->record_generation,
                    (unsigned long long)serial, rc,
                    wine_present_control_load_u32(&control->record_state),
                    wine_present_control_load_u32(&control->record_error_code),
                    (unsigned long long)wine_present_control_load_u64(
                        &control->reserved[WINE_PRESENT_CONTROL_RECORD_SOURCE_GENERATION_INDEX]),
                    bridge_ready, (unsigned long long)expected);
            return;
        }
    }
    state = wine_present_control_load_u32(&control->record_state);
    if ((state == WINE_PRESENT_RECORD_STATE_RECORDING ||
         state == WINE_PRESENT_RECORD_STATE_STARTING ||
         state == WINE_PRESENT_RECORD_STATE_PAUSED) &&
        mgl_record.attach) {
        expected = 0;
        if (__atomic_compare_exchange_n(
                &control->reserved[WINE_PRESENT_CONTROL_RECORD_SOURCE_GENERATION_INDEX], &expected,
                chain->record_generation, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            int rc = mgl_record.attach(chain->record_generation, (uint32_t)chain->width,
                                       (uint32_t)chain->height, chain->native_transform);
            if (rc) {
                if (!chain->record_attach_error_logged) {
                    fprintf(stderr,
                            "MGL_RECORD: source attach failed pid=%ld size=%dx%d generation=%llu "
                            "rc=%d state=%u\n",
                            (long)getpid(), chain->width, chain->height,
                            (unsigned long long)chain->record_generation, rc, state);
                    chain->record_attach_error_logged = 1;
                }
                mgl_record_detach(chain);
            } else {
                fprintf(stderr,
                        "MGL_RECORD: source attached pid=%ld size=%dx%d generation=%llu state=%u\n",
                        (long)getpid(), chain->width, chain->height,
                        (unsigned long long)chain->record_generation, state);
            }
        }
    }
}

static void mgl_record_offer_frame(struct mgl_ahb_presentation *chain, int index, int fence_fd) {
    struct wine_present_control_shared *control = mgl_record_control(chain);
    unsigned held = 0;
    int duplicate, release = -1;
    if (!control || !mgl_record.offer ||
        wine_present_control_load_u32(&control->record_state) !=
            WINE_PRESENT_RECORD_STATE_RECORDING ||
        wine_present_control_load_u64(
            &control->reserved[WINE_PRESENT_CONTROL_RECORD_SOURCE_GENERATION_INDEX]) !=
            chain->record_generation)
        return;
    for (unsigned i = 0; i < chain->image_count; ++i)
        if (chain->images[i].record_release_fd >= 0 &&
            !fence_ready(chain->images[i].record_release_fd))
            ++held;
    if (held >= (chain->image_count > 2 ? chain->image_count - 2 : 1))
        return;
    duplicate = fcntl(fence_fd, F_DUPFD_CLOEXEC, 0);
    if (duplicate < 0)
        return;
    if (mgl_record.offer(chain->images[index].buffer, duplicate, &release, chain->record_generation,
                         chain->native_transform) == 0 &&
        release >= 0) {
        if (chain->images[index].record_release_fd >= 0)
            close(chain->images[index].record_release_fd);
        chain->images[index].record_release_fd = release;
    } else if (release >= 0)
        close(release);
}

static void mgl_screenshot_update(struct mgl_ahb_presentation *chain, int index, int acquire_fd) {
    struct wine_present_control_shared *control = mgl_record_control(chain);
    if (!control || chain->width < 320 || chain->height < 240)
        return;
    uint64_t serial = wine_present_control_load_u64(&control->screenshot_request_serial);
    uint64_t ack = wine_present_control_load_u64(&control->screenshot_ack_serial);
    if (!serial || serial == ack)
        return;
    (void)pthread_once(&mgl_record_once, mgl_record_load);
    if (!mgl_record.screenshot) {
        fprintf(stderr, "MGL_SCREENSHOT: bridge unavailable serial=%llu\n",
                (unsigned long long)serial);
        return;
    }
    int duplicate = acquire_fd < 0 ? -1 : fcntl(acquire_fd, F_DUPFD_CLOEXEC, 0);
    if (acquire_fd >= 0 && duplicate < 0)
        return;
    if (!__atomic_compare_exchange_n(&control->screenshot_ack_serial, &ack, serial, 0,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        if (duplicate >= 0)
            close(duplicate);
        return;
    }
    int release_fd = -1;
    int rc = mgl_record.screenshot(chain->images[index].buffer, duplicate, &release_fd,
                                   chain->native_transform, serial);
    if (rc == 0 && release_fd >= 0) {
        if (chain->images[index].screenshot_release_fd >= 0)
            close(chain->images[index].screenshot_release_fd);
        chain->images[index].screenshot_release_fd = release_fd;
    } else if (release_fd >= 0)
        close(release_fd);
    fprintf(stderr, "MGL_SCREENSHOT: dispatched serial=%llu size=%dx%d rc=%d\n",
            (unsigned long long)serial, chain->width, chain->height, rc);
}

static void completion(void *arg, ASurfaceTransactionStats *stats) {
    struct mgl_ahb_completion *token = arg;
    struct mgl_ahb_presentation *chain = token->chain;
    int prev = token->previous_image;
    int fence = -1;
    if (prev >= 0)
        fence = chain->sc.release_fence(stats, chain->layer);
    pthread_mutex_lock(&chain->mutex);
    if (prev >= 0) {
        struct mgl_ahb_image *img = &chain->images[prev];
        if (img->release_fence_fd >= 0)
            close(img->release_fence_fd);
        img->release_fence_fd = fence;
        img->release_pending = 1;
    }
    /* Signal under the mutex: teardown cannot free the chain after seeing
     * pending_callbacks==0 but before the eventfd write completes. */
    uint64_t one = 1;
    ssize_t rc;
    do {
        rc = write(chain->wake_fd, &one, sizeof(one));
    } while (rc < 0 && errno == EINTR);
    if (chain->pending_callbacks)
        --chain->pending_callbacks;
    pthread_mutex_unlock(&chain->mutex);
    free(token);
}

static int apply_submission(struct mgl_ahb_presentation *chain, int index, int acquire_fd,
                            int worker) {
    ASurfaceTransaction *tx;
    struct mgl_ahb_completion *token;
    uint64_t start = worker ? mgl_perf_diag_now() : 0;
    uint64_t elapsed;
    tx = chain->sc.create_tx();
    token = calloc(1, sizeof(*token));
    if (!tx || !token) {
        if (tx)
            chain->sc.delete_tx(tx);
        free(token);
        if (acquire_fd >= 0)
            close(acquire_fd);
        return 0;
    }
    token->chain = chain;
    token->previous_image = chain->last_submitted;
    if (index >= 0)
        chain->sc.set_buffer(tx, chain->layer, chain->images[index].buffer, acquire_fd);
    else
        chain->sc.reparent(tx, chain->layer, NULL);
    chain->sc.on_complete(tx, token, completion);
    pthread_mutex_lock(&chain->mutex);
    ++chain->pending_callbacks;
    if (index >= 0)
        chain->images[index].submitted = 1;
    chain->last_submitted = index;
    pthread_mutex_unlock(&chain->mutex);
    if (worker && start) {
        elapsed = mgl_perf_diag_now();
        if (elapsed >= start) {
            elapsed -= start;
            chain->submit_prepare_ns += elapsed;
            if (elapsed > chain->submit_prepare_max_ns)
                chain->submit_prepare_max_ns = elapsed;
        }
    }
    start = worker ? mgl_perf_diag_now() : 0;
    chain->sc.apply_tx(tx);
    if (worker && start) {
        elapsed = mgl_perf_diag_now();
        if (elapsed >= start) {
            elapsed -= start;
            chain->submit_apply_ns += elapsed;
            if (elapsed > chain->submit_apply_max_ns)
                chain->submit_apply_max_ns = elapsed;
            ++chain->submit_apply_calls;
        }
    }
    chain->sc.delete_tx(tx);
    return 1;
}

static void *submit_worker(void *arg) {
    struct mgl_ahb_presentation *chain = arg;
    for (;;) {
        struct mgl_ahb_submission job;
        pthread_mutex_lock(&chain->mutex);
        while (!chain->submit_count && !chain->submit_thread_stop)
            pthread_cond_wait(&chain->submit_condition, &chain->mutex);
        if (!chain->submit_count && chain->submit_thread_stop) {
            pthread_mutex_unlock(&chain->mutex);
            break;
        }
        job = chain->submit_queue[chain->submit_head];
        chain->submit_head = (chain->submit_head + 1u) % (chain->image_count + 1u);
        --chain->submit_count;
        pthread_cond_signal(&chain->submit_condition);
        pthread_mutex_unlock(&chain->mutex);
        if (!apply_submission(chain, job.index, job.acquire_fd, 1)) {
            uint64_t one = 1;
            pthread_mutex_lock(&chain->mutex);
            if (job.index >= 0)
                chain->images[job.index].submitted = 0;
            (void)write(chain->wake_fd, &one, sizeof(one));
            pthread_mutex_unlock(&chain->mutex);
            fprintf(stderr,
                    "MGL_PRESENT: asynchronous SurfaceControl transaction preparation failed\n");
        }
    }
    return NULL;
}

static int submit(struct mgl_ahb_presentation *chain, int index, int acquire_fd) {
    if (!chain->submit_thread_started)
        return apply_submission(chain, index, acquire_fd, 0);
    pthread_mutex_lock(&chain->mutex);
    while (chain->submit_count == chain->image_count + 1u)
        pthread_cond_wait(&chain->submit_condition, &chain->mutex);
    if (index >= 0)
        chain->images[index].submitted = 1;
    unsigned tail = (chain->submit_head + chain->submit_count) % (chain->image_count + 1u);
    chain->submit_queue[tail] = (struct mgl_ahb_submission){index, acquire_fd};
    ++chain->submit_count;
    pthread_cond_signal(&chain->submit_condition);
    pthread_mutex_unlock(&chain->mutex);
    return 1;
}

static void drain_wake(struct mgl_ahb_presentation *chain) {
    uint64_t value;
    ssize_t rc;
    do {
        rc = read(chain->wake_fd, &value, sizeof(value));
    } while (rc < 0 && errno == EINTR);
}

static int wait_release_progress(struct mgl_ahb_presentation *chain) {
    struct pollfd stack_fds[13];
    size_t max_fds = (size_t)chain->image_count * 3 + 1;
    struct pollfd *fds = stack_fds;
    size_t capacity = sizeof(stack_fds) / sizeof(stack_fds[0]);
    nfds_t count = 0;
    int rc;
    if (max_fds > capacity) {
        fds = malloc(max_fds * sizeof(*fds));
        if (fds)
            capacity = max_fds;
        else
            fds = stack_fds;
    }
    pthread_mutex_lock(&chain->mutex);
    fds[count++] = (struct pollfd){.fd = chain->wake_fd, .events = POLLIN};
    for (unsigned i = 0; i < chain->image_count && count < capacity; ++i) {
        const struct mgl_ahb_image *img = &chain->images[i];
        if (img->submitted && img->release_pending && img->release_fence_fd >= 0)
            fds[count++] = (struct pollfd){.fd = img->release_fence_fd, .events = POLLIN};
        if (img->record_release_fd >= 0 && !fence_ready(img->record_release_fd) && count < capacity)
            fds[count++] = (struct pollfd){.fd = img->record_release_fd, .events = POLLIN};
        if (img->screenshot_release_fd >= 0 && !fence_ready(img->screenshot_release_fd) &&
            count < capacity)
            fds[count++] = (struct pollfd){.fd = img->screenshot_release_fd, .events = POLLIN};
    }
    pthread_mutex_unlock(&chain->mutex);
    rc = poll(fds, count, -1);
    if (rc > 0 && (fds[0].revents & (POLLIN | POLLHUP | POLLERR)))
        drain_wake(chain);
    if (fds != stack_fds)
        free(fds);
    if (rc < 0)
        return errno == EINTR ? 1 : -1;
    return rc > 0 ? 1 : 0;
}

static int acquire_image(struct mgl_ahb_presentation *chain) {
    /* No acquisition timeout is exposed; wait for a reusable image. */
    for (;;) {
        pthread_mutex_lock(&chain->mutex);
        for (unsigned n = 0; n < chain->image_count; ++n) {
            unsigned i = (chain->next_scan + n) % chain->image_count;
            struct mgl_ahb_image *img = &chain->images[i];
            if (img->submitted && img->release_pending && fence_ready(img->release_fence_fd)) {
                if (img->release_fence_fd >= 0)
                    close(img->release_fence_fd);
                img->release_fence_fd = -1;
                img->submitted = 0;
                img->release_pending = 0;
            }
            if (img->record_release_fd >= 0 && fence_ready(img->record_release_fd)) {
                close(img->record_release_fd);
                img->record_release_fd = -1;
            }
            if (img->screenshot_release_fd >= 0 && fence_ready(img->screenshot_release_fd)) {
                close(img->screenshot_release_fd);
                img->screenshot_release_fd = -1;
            }
            if (!img->submitted && img->record_release_fd < 0 && img->screenshot_release_fd < 0 &&
                (int)i != chain->current) {
                chain->next_scan = (i + 1) % chain->image_count;
                pthread_mutex_unlock(&chain->mutex);
                return (int)i;
            }
        }
        pthread_mutex_unlock(&chain->mutex);
        if (wait_release_progress(chain) < 0)
            return -1;
    }
}

static void release_resources(struct mgl_ahb_presentation *chain, struct mgl_context *ctx) {
    int i;
    int has_current =
        ctx && mgl_context_current() == ctx && ctx->native_dispatch == chain->native_dispatch;
    if (has_current) {
        ctx->native_default_framebuffer = 0u;
        if (chain->native_dispatch->glesBindFramebuffer)
            chain->native_dispatch->glesBindFramebuffer(GL_FRAMEBUFFER, 0u);
    }
    for (i = 0; i < (int)chain->image_count; ++i) {
        struct mgl_ahb_image *img = &chain->images[i];
        if (has_current && img->framebuffer && chain->native_dispatch->glesDeleteFramebuffers)
            chain->native_dispatch->glesDeleteFramebuffers(1, &img->framebuffer);
        if (has_current && img->color_renderbuffer &&
            chain->native_dispatch->glesDeleteRenderbuffers)
            chain->native_dispatch->glesDeleteRenderbuffers(1, &img->color_renderbuffer);
        if (has_current && img->depth_renderbuffer &&
            chain->native_dispatch->glesDeleteRenderbuffers)
            chain->native_dispatch->glesDeleteRenderbuffers(1, &img->depth_renderbuffer);
        if (img->egl_image && chain->destroy_image)
            chain->destroy_image(chain->display, img->egl_image);
        if (img->buffer && chain->sc.release)
            chain->sc.release(img->buffer);
        if (img->release_fence_fd >= 0)
            close(img->release_fence_fd);
        if (img->record_release_fd >= 0)
            close(img->record_release_fd);
        if (img->screenshot_release_fd >= 0)
            close(img->screenshot_release_fd);
    }
    if (chain->record_control)
        munmap(chain->record_control, sizeof(struct wine_present_control_shared));
    if (chain->layer && chain->sc.release_control)
        chain->sc.release_control(chain->layer);
    if (chain->sc.lib)
        dlclose(chain->sc.lib);
    if (chain->submit_condition_initialized)
        pthread_cond_destroy(&chain->submit_condition);
    pthread_mutex_destroy(&chain->mutex);
    if (chain->wake_fd >= 0)
        close(chain->wake_fd);
    free(chain->images);
    free(chain);
}

void mgl_ahb_presentation_destroy(struct mgl_ahb_presentation *chain, struct mgl_context *ctx) {
    if (!chain)
        return;
    chain->stopping = 1;
    mgl_record_detach(chain);
    if (chain->layer && chain->sc.create_tx && chain->sc.reparent)
        (void)submit(chain, -1, -1);
    if (chain->submit_thread_started) {
        pthread_mutex_lock(&chain->mutex);
        chain->submit_thread_stop = 1;
        pthread_cond_broadcast(&chain->submit_condition);
        pthread_mutex_unlock(&chain->mutex);
        pthread_join(chain->submit_thread, NULL);
        chain->submit_thread_started = 0;
        if (chain->submit_apply_calls)
            fprintf(stderr,
                    "MGL_AHB_ASYNC_DIAG pid=%ld apply_calls=%u apply_total_ms=%.3f "
                    "apply_max_ms=%.3f prepare_total_ms=%.3f prepare_max_ms=%.3f\n",
                    (long)getpid(), chain->submit_apply_calls,
                    (double)chain->submit_apply_ns / 1000000.0,
                    (double)chain->submit_apply_max_ns / 1000000.0,
                    (double)chain->submit_prepare_ns / 1000000.0,
                    (double)chain->submit_prepare_max_ns / 1000000.0);
    }
    int still_busy = 0;
    for (;;) {
        int pending;
        int busy = 0;
        int i;
        pthread_mutex_lock(&chain->mutex);
        pending = (int)chain->pending_callbacks;
        for (i = 0; i < (int)chain->image_count; ++i) {
            struct mgl_ahb_image *img = &chain->images[i];
            if (img->submitted && (!img->release_pending || !fence_ready(img->release_fence_fd)))
                busy = 1;
            if (img->record_release_fd >= 0 && !fence_ready(img->record_release_fd))
                busy = 1;
            if (img->screenshot_release_fd >= 0 && !fence_ready(img->screenshot_release_fd))
                busy = 1;
        }
        pthread_mutex_unlock(&chain->mutex);
        still_busy = pending || busy;
        if (!still_busy)
            break;
        if (wait_release_progress(chain) < 0)
            break;
    }
    pthread_mutex_lock(&chain->mutex);
    if (chain->pending_callbacks || still_busy) {
        /* Never free compositor-owned images after a failed retire wait; defer cleanup to
         * process exit. */
        pthread_mutex_unlock(&chain->mutex);
        fprintf(stderr, "MGL_PRESENT: AHardwareBuffer presentation retire wait failed, deferring "
                        "lifetime to process exit\n");
        if (ctx)
            ctx->native_default_framebuffer = 0u;
        return;
    }
    pthread_mutex_unlock(&chain->mutex);
    release_resources(chain, ctx);
}

struct mgl_ahb_presentation *
mgl_ahb_presentation_create(struct mgl_context *ctx, void *native_window, int width, int height) {
    struct mgl_ahb_presentation *chain;
    int i;
    GLint prev_fbo = 0, prev_rb = 0;
    const MGL_EGLint img_attrs[] = {MGL_EGL_NONE};
    const char *phase = "Android native library";
    if (!ctx || !native_window || !ctx->native_dispatch || mgl_context_current() != ctx ||
        width < 1 || height < 1)
        return NULL;
    chain = calloc(1, sizeof(*chain));
    if (!chain)
        return NULL;
    chain->image_count = mgl_ahb_image_count();
    chain->images = calloc(chain->image_count, sizeof(*chain->images));
    if (!chain->images) {
        fprintf(stderr, "MGL_PRESENT: AHardwareBuffer presentation unavailable (image array "
                        "allocation); using EGL fallback\n");
        free(chain);
        return NULL;
    }
    pthread_mutex_init(&chain->mutex, NULL);
    chain->submit_condition_initialized = pthread_cond_init(&chain->submit_condition, NULL) == 0;
    chain->wake_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (chain->wake_fd < 0) {
        fprintf(stderr, "MGL_PRESENT: AHardwareBuffer presentation unavailable (eventfd); using "
                        "EGL fallback\n");
        release_resources(chain, NULL);
        return NULL;
    }
    chain->width = width;
    chain->height = height;
    chain->display = ctx->egl_display;
    chain->native_dispatch = ctx->native_dispatch;
    chain->current = -1;
    chain->last_submitted = -1;
    for (i = 0; i < (int)chain->image_count; ++i) {
        chain->images[i].release_fence_fd = -1;
        chain->images[i].record_release_fd = -1;
        chain->images[i].screenshot_release_fd = -1;
    }
    mgl_record_initialize(chain);
#ifdef __ANDROID__
    chain->sc.lib = dlopen("/system/lib64/libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (!chain->sc.lib)
        chain->sc.lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
#else
    chain->sc.lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
#endif
    if (!chain->sc.lib)
        goto fail;
    phase = "AHardwareBuffer and SurfaceControl symbols";
    RESOLVE_LIB(allocate, "AHardwareBuffer_allocate");
    RESOLVE_LIB(release, "AHardwareBuffer_release");
    RESOLVE_LIB(create_control, "ASurfaceControl_createFromWindow");
    RESOLVE_LIB(release_control, "ASurfaceControl_release");
    RESOLVE_LIB(create_tx, "ASurfaceTransaction_create");
    RESOLVE_LIB(delete_tx, "ASurfaceTransaction_delete");
    RESOLVE_LIB(apply_tx, "ASurfaceTransaction_apply");
    RESOLVE_LIB(visibility, "ASurfaceTransaction_setVisibility");
    RESOLVE_LIB(z_order, "ASurfaceTransaction_setZOrder");
    RESOLVE_LIB(transparency, "ASurfaceTransaction_setBufferTransparency");
    RESOLVE_LIB(set_buffer, "ASurfaceTransaction_setBuffer");
    RESOLVE_LIB(reparent, "ASurfaceTransaction_reparent");
    RESOLVE_LIB(on_complete, "ASurfaceTransaction_setOnComplete");
    RESOLVE_LIB(release_fence, "ASurfaceTransactionStats_getPreviousReleaseFenceFd");
    *(void **)(&chain->sc.geometry) = dlsym(chain->sc.lib, "ASurfaceTransaction_setGeometry");
    chain->native_transform = chain->sc.geometry ? MGL_NATIVE_WINDOW_TRANSFORM_FLIP_V : 0;
    phase = "EGL image and native-fence extensions";
    RESOLVE_EGL(get_client_buffer, "eglGetNativeClientBufferANDROID");
    RESOLVE_EGL(create_image, "eglCreateImageKHR");
    RESOLVE_EGL(destroy_image, "eglDestroyImageKHR");
    RESOLVE_EGL(image_renderbuffer, "glEGLImageTargetRenderbufferStorageOES");
    RESOLVE_EGL(create_sync, "eglCreateSyncKHR");
    RESOLVE_EGL(destroy_sync, "eglDestroySyncKHR");
    RESOLVE_EGL(dup_fence, "eglDupNativeFenceFDANDROID");
    phase = "GLES framebuffer functions";
    if (!chain->native_dispatch->glesGenFramebuffers ||
        !chain->native_dispatch->glesGenRenderbuffers ||
        !chain->native_dispatch->glesFramebufferRenderbuffer ||
        !chain->native_dispatch->glesRenderbufferStorage ||
        !chain->native_dispatch->glesCheckFramebufferStatus ||
        !chain->native_dispatch->glesBindRenderbuffer)
        goto fail;
    phase = "SurfaceControl child creation";
    chain->layer = chain->sc.create_control(native_window, "mGL window");
    if (!chain->layer)
        goto fail;
    chain->native_dispatch->glesGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);
    chain->native_dispatch->glesGetIntegerv(GL_RENDERBUFFER_BINDING, &prev_rb);
    phase = "AHardwareBuffer allocation and EGL FBO import";
    for (i = 0; i < (int)chain->image_count; ++i) {
        struct mgl_ahb_image *img = &chain->images[i];
        mgl_ahb_desc desc = {0};
        desc.width = (uint32_t)width;
        desc.height = (uint32_t)height;
        desc.layers = 1;
        desc.format = MGL_AHB_RGBA8;
        desc.usage = MGL_AHB_GPU_SAMPLED | MGL_AHB_GPU_COLOR | MGL_AHB_COMPOSER;
        if (chain->sc.allocate(&desc, &img->buffer) != 0 || !img->buffer)
            goto fail_restore;
        img->egl_image =
            chain->create_image(ctx->egl_display, MGL_EGL_NO_CONTEXT, MGL_EGL_NATIVE_BUFFER_ANDROID,
                                chain->get_client_buffer(img->buffer), img_attrs);
        if (!img->egl_image)
            goto fail_restore;
        chain->native_dispatch->glesGenRenderbuffers(1, &img->color_renderbuffer);
        chain->native_dispatch->glesBindRenderbuffer(GL_RENDERBUFFER, img->color_renderbuffer);
        chain->image_renderbuffer(GL_RENDERBUFFER, img->egl_image);
        chain->native_dispatch->glesGenRenderbuffers(1, &img->depth_renderbuffer);
        chain->native_dispatch->glesBindRenderbuffer(GL_RENDERBUFFER, img->depth_renderbuffer);
        chain->native_dispatch->glesRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width,
                                                        height);
        chain->native_dispatch->glesGenFramebuffers(1, &img->framebuffer);
        chain->native_dispatch->glesBindFramebuffer(GL_FRAMEBUFFER, img->framebuffer);
        chain->native_dispatch->glesFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, img->color_renderbuffer);
        chain->native_dispatch->glesFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, img->depth_renderbuffer);
        if (chain->native_dispatch->glesCheckFramebufferStatus(GL_FRAMEBUFFER) !=
            GL_FRAMEBUFFER_COMPLETE)
            goto fail_restore;
    }
    chain->native_dispatch->glesBindRenderbuffer(GL_RENDERBUFFER, (GLuint)prev_rb);
    chain->native_dispatch->glesBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
    phase = "initial SurfaceControl transaction";
    {
        ASurfaceTransaction *tx = chain->sc.create_tx();
        if (!tx)
            goto fail;
        chain->sc.visibility(tx, chain->layer, 1);
        chain->sc.z_order(tx, chain->layer, 1);
        chain->sc.transparency(tx, chain->layer, 2);
        if (chain->sc.geometry) {
            mgl_rect rect = {0, 0, width, height};
            chain->sc.geometry(tx, chain->layer, &rect, &rect, MGL_NATIVE_WINDOW_TRANSFORM_FLIP_V);
        }
        chain->sc.apply_tx(tx);
        chain->sc.delete_tx(tx);
    }
    chain->current = 0;
    chain->next_scan = 1 % chain->image_count;
    ctx->native_default_framebuffer = chain->images[0].framebuffer;
    chain->native_dispatch->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER, chain->images[0].framebuffer);
    chain->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER, chain->images[0].framebuffer);
    if (chain->native_dispatch->glesDrawBuffers) {
        GLenum attachment = GL_COLOR_ATTACHMENT0;
        chain->native_dispatch->glesDrawBuffers(1, &attachment);
    }
    if (chain->native_dispatch->glesReadBuffer)
        chain->native_dispatch->glesReadBuffer(GL_COLOR_ATTACHMENT0);
    if (chain->submit_condition_initialized &&
        pthread_create(&chain->submit_thread, NULL, submit_worker, chain) == 0) {
        chain->submit_thread_started = 1;
    } else {
        fprintf(stderr, "MGL_PRESENT: AHardwareBuffer asynchronous submit worker unavailable; "
                        "using synchronous submission\n");
    }
    fprintf(stderr,
            "MGL_PRESENT: AHardwareBuffer SurfaceControl presentation active %dx%d images=%u "
            "async_submit=%d\n",
            width, height, chain->image_count, chain->submit_thread_started);
    return chain;
fail_restore:
    chain->native_dispatch->glesBindRenderbuffer(GL_RENDERBUFFER, (GLuint)prev_rb);
    chain->native_dispatch->glesBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
fail:
    fprintf(stderr,
            "MGL_PRESENT: AHardwareBuffer presentation unavailable (%s); using EGL fallback\n",
            phase);

    release_resources(chain, ctx);
    return NULL;
}

int mgl_ahb_presentation_present(struct mgl_ahb_presentation *chain, struct mgl_context *ctx) {
    int next, fd;
    void *sync;
    const MGL_EGLint fence_attrs[] = {MGL_EGL_SYNC_NATIVE_FENCE_FD_ANDROID, -1, MGL_EGL_NONE};
    if (!chain || !ctx || mgl_context_current() != ctx || chain->current < 0)
        return 0;
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_AHB_PRESENT);
    /* Pass GPU completion to SurfaceFlinger without a synchronous finish. */
    sync = chain->create_sync(chain->display, MGL_EGL_SYNC_NATIVE_FENCE_ANDROID, fence_attrs);
    if (!sync)
        return 0;
    if (chain->native_dispatch->glesFlush)
        chain->native_dispatch->glesFlush();
    fd = chain->dup_fence(chain->display, sync);
    chain->destroy_sync(chain->display, sync);
    if (fd < 0)
        return 0;
    mgl_record_update(chain);
    mgl_screenshot_update(chain, chain->current, fd);
    mgl_record_offer_frame(chain, chain->current, fd);
    {
        uint64_t deep_started = mgl_perf_diag_now();
        int ok = submit(chain, chain->current, fd);
        mgl_deep_record(ctx, MGL_DEEP_AHB_SUBMIT, deep_started);
        if (!ok)
            return 0;
    }
    {
        uint64_t deep_started = mgl_perf_diag_now();
        next = acquire_image(chain);
        mgl_deep_record(ctx, MGL_DEEP_AHB_ACQUIRE, deep_started);
    }
    if (next < 0)
        return 0;
    chain->current = next;
    ctx->native_default_framebuffer = chain->images[next].framebuffer;

    if (ctx->draw_framebuffer == 0u) {
        chain->native_dispatch->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                    chain->images[next].framebuffer);
        if (chain->native_dispatch->glesDrawBuffers) {
            GLenum color = ctx->default_draw_buffers[0] == GL_NONE ? GL_NONE : GL_COLOR_ATTACHMENT0;
            chain->native_dispatch->glesDrawBuffers(1, &color);
        }
    }
    if (ctx->read_framebuffer == 0u) {
        chain->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER,
                                                    chain->images[next].framebuffer);
        if (chain->native_dispatch->glesReadBuffer)
            chain->native_dispatch->glesReadBuffer(
                ctx->default_read_buffer == GL_NONE ? GL_NONE : GL_COLOR_ATTACHMENT0);
    }
    return 1;
}

unsigned mgl_ahb_presentation_framebuffer(const struct mgl_ahb_presentation *chain) {
    return chain && chain->current >= 0 ? chain->images[chain->current].framebuffer : 0u;
}

int mgl_ahb_presentation_dimensions_match(const struct mgl_ahb_presentation *chain, int width,
                                          int height) {
    return chain && chain->width == width && chain->height == height;
}

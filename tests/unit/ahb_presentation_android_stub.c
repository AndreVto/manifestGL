#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

typedef struct AHardwareBuffer {
    unsigned id;
} AHardwareBuffer;

typedef struct ASurfaceControl {
    unsigned tag;
} ASurfaceControl;

typedef struct ASurfaceTransactionStats {
    unsigned tag;
} ASurfaceTransactionStats;

typedef struct ASurfaceTransaction {
    ASurfaceControl *layer;
    AHardwareBuffer *buffer;
    void *callback_context;
    void (*callback)(void *, ASurfaceTransactionStats *);
} ASurfaceTransaction;

static unsigned allocations, submissions, releases, most_recent_id;
static unsigned apply_delay_ms, apply_on_render_thread;
static unsigned tx_create_on_render_thread, set_buffer_on_render_thread,
    on_complete_on_render_thread;
static pthread_t render_thread;
static int geometry_transform = -1;
static ASurfaceControl layer;
static ASurfaceTransactionStats stats;

int AHardwareBuffer_allocate(const void *desc, AHardwareBuffer **out) {
    (void)desc;
    *out = calloc(1, sizeof(**out));
    if (!*out)
        return -1;
    (*out)->id = ++allocations;
    return 0;
}

void AHardwareBuffer_release(AHardwareBuffer *b) {
    free(b);
}

ASurfaceControl *ASurfaceControl_createFromWindow(void *window, const char *name) {
    (void)name;
    return window ? &layer : NULL;
}

void ASurfaceControl_release(ASurfaceControl *p) {
    (void)p;
}

ASurfaceTransaction *ASurfaceTransaction_create(void) {
    if (pthread_equal(pthread_self(), render_thread))
        __atomic_fetch_add(&tx_create_on_render_thread, 1u, __ATOMIC_RELAXED);
    return calloc(1, sizeof(ASurfaceTransaction));
}

void ASurfaceTransaction_delete(ASurfaceTransaction *tx) {
    free(tx);
}

void ASurfaceTransaction_setVisibility(ASurfaceTransaction *tx, ASurfaceControl *c,
                                       int8_t visible) {
    (void)tx;
    (void)c;
    (void)visible;
}

void ASurfaceTransaction_setZOrder(ASurfaceTransaction *tx, ASurfaceControl *c, int32_t order) {
    (void)tx;
    (void)c;
    (void)order;
}

void ASurfaceTransaction_setBufferTransparency(ASurfaceTransaction *tx, ASurfaceControl *c,
                                               int8_t value) {
    (void)tx;
    (void)c;
    (void)value;
}

void ASurfaceTransaction_setGeometry(ASurfaceTransaction *tx, ASurfaceControl *c, const void *src,
                                     const void *dst, int32_t transform) {
    (void)tx;
    (void)c;
    (void)src;
    (void)dst;
    geometry_transform = transform;
}

void ASurfaceTransaction_reparent(ASurfaceTransaction *tx, ASurfaceControl *c,
                                  ASurfaceControl *parent) {
    (void)parent;
    tx->layer = c;
    tx->buffer = NULL;
}

void ASurfaceTransaction_setBuffer(ASurfaceTransaction *tx, ASurfaceControl *c, AHardwareBuffer *b,
                                   int fd) {
    if (pthread_equal(pthread_self(), render_thread))
        __atomic_fetch_add(&set_buffer_on_render_thread, 1u, __ATOMIC_RELAXED);
    tx->layer = c;
    tx->buffer = b;
    if (fd >= 0) {
        extern int close(int);
        close(fd);
    }
}

void ASurfaceTransaction_setOnComplete(ASurfaceTransaction *tx, void *ctx,
                                       void (*callback)(void *, ASurfaceTransactionStats *)) {
    if (pthread_equal(pthread_self(), render_thread))
        __atomic_fetch_add(&on_complete_on_render_thread, 1u, __ATOMIC_RELAXED);
    tx->callback_context = ctx;
    tx->callback = callback;
}

int ASurfaceTransactionStats_getPreviousReleaseFenceFd(ASurfaceTransactionStats *st,
                                                       ASurfaceControl *sc) {
    (void)st;
    (void)sc;
    __atomic_fetch_add(&releases, 1u, __ATOMIC_RELAXED);
    return -1;
}

void ASurfaceTransaction_apply(ASurfaceTransaction *tx) {
    if (tx->buffer) {
        unsigned delay = __atomic_load_n(&apply_delay_ms, __ATOMIC_RELAXED);
        if (delay) {
            struct timespec ts = {.tv_sec = delay / 1000u,
                                  .tv_nsec = (long)(delay % 1000u) * 1000000L};
            nanosleep(&ts, NULL);
        }
        if (pthread_equal(pthread_self(), render_thread))
            __atomic_store_n(&apply_on_render_thread, 1u, __ATOMIC_RELAXED);
        __atomic_store_n(&most_recent_id, tx->buffer->id, __ATOMIC_RELAXED);
        __atomic_fetch_add(&submissions, 1u, __ATOMIC_RELAXED);
    }
    if (tx->callback)
        tx->callback(tx->callback_context, &stats);
}

unsigned mgl_test_ahb_allocations(void) {
    return allocations;
}

unsigned mgl_test_ahb_submissions(void) {
    return __atomic_load_n(&submissions, __ATOMIC_RELAXED);
}

unsigned mgl_test_ahb_releases(void) {
    return __atomic_load_n(&releases, __ATOMIC_RELAXED);
}

unsigned mgl_test_ahb_last_id(void) {
    return __atomic_load_n(&most_recent_id, __ATOMIC_RELAXED);
}

void mgl_test_ahb_set_apply_delay(unsigned ms) {
    __atomic_store_n(&apply_delay_ms, ms, __ATOMIC_RELAXED);
}

void mgl_test_ahb_set_render_thread(void) {
    render_thread = pthread_self();
}

unsigned mgl_test_ahb_applied_on_render_thread(void) {
    return __atomic_load_n(&apply_on_render_thread, __ATOMIC_RELAXED);
}

int mgl_test_ahb_geometry_transform(void) {
    return geometry_transform;
}

unsigned mgl_test_ahb_tx_create_on_render_thread(void) {
    return __atomic_load_n(&tx_create_on_render_thread, __ATOMIC_RELAXED);
}

unsigned mgl_test_ahb_set_buffer_on_render_thread(void) {
    return __atomic_load_n(&set_buffer_on_render_thread, __ATOMIC_RELAXED);
}

unsigned mgl_test_ahb_on_complete_on_render_thread(void) {
    return __atomic_load_n(&on_complete_on_render_thread, __ATOMIC_RELAXED);
}

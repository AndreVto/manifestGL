#define _POSIX_C_SOURCE 200809L
#include "presentation/android_ahb_presentation.h"
#include "core/context.h"
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static GLuint name_counter = 100;
static unsigned blit_count;
static unsigned renderbuffer_imports;
static void *last_egl_buffer;
static void *(*android_buffers_last)(void);

static void get_int(GLenum name, GLint *out) {
    (void)name;
    *out = 0;
}

static void gen(GLsizei count, GLuint *names) {
    while (count-- > 0)
        *names++ = name_counter++;
}

static void bind(GLenum target, GLuint value) {
    (void)target;
    (void)value;
}

static void storage(GLenum target, GLenum format, GLsizei w, GLsizei h) {
    (void)target;
    (void)format;
    (void)w;
    (void)h;
}

static void attachment(GLenum fbtarget, GLenum attachment, GLenum rbtarget, GLuint name) {
    (void)fbtarget;
    (void)attachment;
    (void)rbtarget;
    (void)name;
}

static GLenum status(GLenum target) {
    (void)target;
    return GL_FRAMEBUFFER_COMPLETE;
}

static void delete(GLsizei count, const GLuint *names) {
    (void)count;
    (void)names;
}

static void flush(void) {
}

static void blit(GLint a, GLint b, GLint c, GLint d, GLint e, GLint f, GLint g, GLint h,
                 GLbitfield mask, GLenum filter) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    (void)g;
    (void)h;
    (void)mask;
    (void)filter;
    ++blit_count;
}

static void *get_native_buffer(void *buffer) {
    return buffer;
}

static void *create_image(void *d, void *context, unsigned type, void *buffer,
                          const int32_t *attrs) {
    (void)d;
    (void)context;
    (void)type;
    (void)attrs;
    last_egl_buffer = buffer;
    return buffer;
}

static uint32_t destroy_image(void *d, void *image) {
    (void)d;
    (void)image;
    return 1;
}

static void import_renderbuffer(GLenum target, void *image) {
    (void)target;
    if (image && image == last_egl_buffer)
        ++renderbuffer_imports;
}

static void *create_sync(void *d, unsigned type, const int32_t *attrs) {
    (void)d;
    (void)type;
    (void)attrs;
    return (void *)(uintptr_t)123u;
}

static uint32_t destroy_sync(void *d, void *sync) {
    (void)d;
    (void)sync;
    return 1;
}

static int dup_fence(void *d, void *sync) {
    int fds[2];
    (void)d;
    (void)sync;
    if (pipe(fds) != 0)
        return -1;
    close(fds[1]);
    return fds[0];
}

static mgl_egl_proc_t resolve(const char *name) {
#define PROC(n, f)                                                                                 \
    if (strcmp(name, n) == 0) {                                                                    \
        mgl_egl_proc_t proc = NULL;                                                                \
        void (*fn)(void) = (void (*)(void))(f);                                                    \
        memcpy(&proc, &fn, sizeof(proc));                                                          \
        return proc;                                                                               \
    }
    PROC("eglGetNativeClientBufferANDROID", get_native_buffer)
    PROC("eglCreateImageKHR", create_image)
    PROC("eglDestroyImageKHR", destroy_image)
    PROC("glEGLImageTargetRenderbufferStorageOES", import_renderbuffer)
    PROC("eglCreateSyncKHR", create_sync)
    PROC("eglDestroySyncKHR", destroy_sync)
    PROC("eglDupNativeFenceFDANDROID", dup_fence)
#undef PROC
    return NULL;
}

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "ahb_presentation_android: failed assertion at line %d: %s\n",         \
                    __LINE__, #c);                                                                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_ahb_presentation *chain;
    void *android;
    unsigned (*allocations)(void), (*submissions)(void), (*released)(void);
    int (*transform)(void);
    void (*set_apply_delay)(unsigned), (*set_render_thread)(void);
    unsigned (*applied_on_render_thread)(void);
    unsigned (*tx_create_on_render_thread)(void);
    unsigned (*set_buffer_on_render_thread)(void);
    unsigned (*on_complete_on_render_thread)(void);
    unsigned initial_creates;
    struct timespec start, end;
    double elapsed_ms;
    int i;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.eglGetProcAddress = resolve;
    native.glesGetIntegerv = get_int;
    native.glesGenRenderbuffers = gen;
    native.glesGenFramebuffers = gen;
    native.glesDeleteFramebuffers = delete;
    native.glesDeleteRenderbuffers = delete;
    native.glesBindRenderbuffer = bind;
    native.glesBindFramebuffer = bind;
    native.glesRenderbufferStorage = storage;
    native.glesFramebufferRenderbuffer = attachment;
    native.glesCheckFramebufferStatus = status;
    native.glesBlitFramebuffer = blit;
    native.glesFlush = flush;
    ctx.native_dispatch = &native;
    ctx.egl_display = (void *)(uintptr_t)17;
    ctx.egl_context = (void *)(uintptr_t)19;
    ctx.default_draw_buffers[0] = GL_BACK;
    ctx.default_read_buffer = GL_BACK;
    android = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    CHECK(android != NULL);
    *(void **)(&allocations) = dlsym(android, "mgl_test_ahb_allocations");
    *(void **)(&submissions) = dlsym(android, "mgl_test_ahb_submissions");
    *(void **)(&released) = dlsym(android, "mgl_test_ahb_releases");
    *(void **)(&transform) = dlsym(android, "mgl_test_ahb_geometry_transform");
    *(void **)(&set_apply_delay) = dlsym(android, "mgl_test_ahb_set_apply_delay");
    *(void **)(&set_render_thread) = dlsym(android, "mgl_test_ahb_set_render_thread");
    *(void **)(&applied_on_render_thread) = dlsym(android, "mgl_test_ahb_applied_on_render_thread");
    *(void **)(&tx_create_on_render_thread) =
        dlsym(android, "mgl_test_ahb_tx_create_on_render_thread");
    *(void **)(&set_buffer_on_render_thread) =
        dlsym(android, "mgl_test_ahb_set_buffer_on_render_thread");
    *(void **)(&on_complete_on_render_thread) =
        dlsym(android, "mgl_test_ahb_on_complete_on_render_thread");
    CHECK(tx_create_on_render_thread && set_buffer_on_render_thread &&
          on_complete_on_render_thread);
    CHECK(allocations && submissions && released && transform && set_apply_delay &&
          set_render_thread && applied_on_render_thread);
    set_render_thread();
    mgl_context_set_current_for_test(&ctx);
    CHECK(unsetenv("WRAPPER_MAX_IMAGE_COUNT") == 0);
    chain = mgl_ahb_presentation_create(&ctx, (void *)(uintptr_t)0x1000u, 128, 72);
    CHECK(chain != NULL && ctx.native_default_framebuffer != 0u);
    CHECK(mgl_ahb_presentation_dimensions_match(chain, 128, 72));
    CHECK(!mgl_ahb_presentation_dimensions_match(chain, 72, 128));
    CHECK(transform() == 2);
    CHECK(allocations() == 5 && renderbuffer_imports == 5 && blit_count == 0);
    initial_creates = tx_create_on_render_thread();
    set_apply_delay(250);
    CHECK(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
    CHECK(mgl_ahb_presentation_present(chain, &ctx));
    CHECK(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
    elapsed_ms = (double)(end.tv_sec - start.tv_sec) * 1000.0 +
                 (double)(end.tv_nsec - start.tv_nsec) / 1000000.0;
    CHECK(elapsed_ms < 170.0);
    for (i = 0; i < 1000 && submissions() == 0; ++i) {
        struct timespec pause = {.tv_sec = 0, .tv_nsec = 1000000L};
        nanosleep(&pause, NULL);
    }
    CHECK(submissions() == 1);
    CHECK(tx_create_on_render_thread() == initial_creates);
    CHECK(set_buffer_on_render_thread() == 0);
    CHECK(on_complete_on_render_thread() == 0);
    set_apply_delay(0);
    for (i = 1; i < 12; ++i)
        CHECK(mgl_ahb_presentation_present(chain, &ctx));
    mgl_ahb_presentation_destroy(chain, &ctx);
    CHECK(submissions() == 12 && released() >= 12 && blit_count == 0);
    CHECK(applied_on_render_thread() == 0);
    CHECK(tx_create_on_render_thread() == initial_creates);
    CHECK(set_buffer_on_render_thread() == 0);
    CHECK(on_complete_on_render_thread() == 0);
    CHECK(ctx.native_default_framebuffer == 0u);
    CHECK(setenv("WRAPPER_MAX_IMAGE_COUNT", "3", 1) == 0);
    {
        unsigned before_allocations = allocations();
        unsigned before_imports = renderbuffer_imports;
        chain = mgl_ahb_presentation_create(&ctx, (void *)(uintptr_t)0x1000u, 128, 72);
        CHECK(chain != NULL);
        CHECK(allocations() == before_allocations + 3);
        CHECK(renderbuffer_imports == before_imports + 3);
        mgl_ahb_presentation_destroy(chain, &ctx);
    }
    CHECK(setenv("WRAPPER_MAX_IMAGE_COUNT", "invalid", 1) == 0);
    {
        unsigned before_allocations = allocations();
        chain = mgl_ahb_presentation_create(&ctx, (void *)(uintptr_t)0x1000u, 128, 72);
        CHECK(chain != NULL);
        CHECK(allocations() == before_allocations + 5);
        mgl_ahb_presentation_destroy(chain, &ctx);
    }
    CHECK(unsetenv("WRAPPER_MAX_IMAGE_COUNT") == 0);
    mgl_context_set_current_for_test(NULL);
    dlclose(android);
    return 0;
}

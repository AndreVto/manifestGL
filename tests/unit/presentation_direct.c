#define _POSIX_C_SOURCE 200809L
#include "mgl/glx.h"
#include "mgl/platform.h"
#include "native/dispatch.h"
#include "core/context.h"
#include "glx/drawable.h"
#include "glx/glx_internal.h"
#include "presentation/android_direct.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Mirror the presentation-control ABI prefix for the host-only shared-memory test. */
struct test_present_control {
    uint32_t magic;
    uint32_t version;
    uint32_t target_fps_millihz;
    uint32_t flags;
    uint64_t session_id;
    uint64_t present_count;
    uint64_t last_present_ns;
    uint64_t reserved[4];
};

static unsigned make_current_calls;
static unsigned destroy_surface_calls;
static unsigned create_window_calls;
static unsigned create_pbuffer_calls;
static unsigned swap_calls;
static unsigned flush_calls;
static unsigned swap_interval_calls;
static int last_swap_interval;
static int fail_swap;
static int fail_create_window;
static MGL_EGLint next_egl_error = MGL_EGL_BAD_SURFACE;
static uintptr_t next_surface = ((uintptr_t)0x8000u);
static void *last_native_window;

static MGL_EGLSurface fake_create_window(MGL_EGLDisplay d, MGL_EGLConfig c,
                                         MGL_EGLNativeWindowType window, const MGL_EGLint *attrs) {
    (void)d;
    (void)c;
    (void)attrs;
    ++create_window_calls;
    last_native_window = window;
    if (fail_create_window)
        return MGL_EGL_NO_SURFACE;
    next_surface += ((uintptr_t)0x10u);
    return (MGL_EGLSurface)next_surface;
}

static MGL_EGLSurface fake_create_pbuffer(MGL_EGLDisplay d, MGL_EGLConfig c,
                                          const MGL_EGLint *attrs) {
    (void)d;
    (void)c;
    (void)attrs;
    ++create_pbuffer_calls;
    next_surface += ((uintptr_t)0x10u);
    return (MGL_EGLSurface)next_surface;
}

static MGL_EGLBoolean fake_destroy_surface(MGL_EGLDisplay d, MGL_EGLSurface s) {
    (void)d;
    (void)s;
    ++destroy_surface_calls;
    return MGL_EGL_TRUE;
}

static MGL_EGLBoolean fake_make_current(MGL_EGLDisplay d, MGL_EGLSurface draw, MGL_EGLSurface read,
                                        MGL_EGLContext context) {
    (void)d;
    (void)draw;
    (void)read;
    (void)context;
    ++make_current_calls;
    return MGL_EGL_TRUE;
}

static MGL_EGLBoolean fake_swap(MGL_EGLDisplay d, MGL_EGLSurface s) {
    (void)d;
    (void)s;
    ++swap_calls;
    return fail_swap ? MGL_EGL_FALSE : MGL_EGL_TRUE;
}

static MGL_EGLBoolean fake_swap_interval(MGL_EGLDisplay d, MGL_EGLint interval) {
    (void)d;
    ++swap_interval_calls;
    last_swap_interval = interval;
    return MGL_EGL_TRUE;
}

static MGL_EGLint fake_get_error(void) {
    return next_egl_error;
}

static void fake_gles_version(GLenum pname, GLint *value) {
    *value = pname == GL_MAJOR_VERSION ? 3 : 2;
}

static void fake_flush(void) {
    ++flush_calls;
}

#define CHECK(cond, msg)                                                                           \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "presentation_direct: %s\n", msg);                                     \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    char telemetry_path[] = "/tmp/mgl-fps-telemetry-XXXXXX";
    int telemetry_fd;
    struct test_present_control *telemetry;
    const size_t telemetry_size = 4096u;
    const char *bridge_path = getenv("WINE_ANDROID_DIRECT_PRESENT_LIB");
    void *bridge_lib;
    void (*set_window)(uintptr_t);
    unsigned (*acquire_count)(void);
    unsigned (*release_count)(void);
    unsigned (*retire_count)(void);
    uint32_t (*last_drawable_fn)(void);
    uint32_t (*last_whole_fn)(void);
    uint64_t (*last_hwnd_fn)(void);
    uint64_t (*last_owner_fn)(void);
    struct mgl_native_dispatch native;
    struct mgl_context core;
    GLXContext glx;
    struct mgl_context *owned_core;
    Display *dpy = (Display *)(uintptr_t)0x1234u;
    GLXDrawable window;
    GLXDrawable pbuffer;

    telemetry_fd = mkstemp(telemetry_path);
    CHECK(telemetry_fd >= 0, "could not create fake app client telemetry");
    CHECK(ftruncate(telemetry_fd, (off_t)telemetry_size) == 0, "cannot size telemetry mapping");
    telemetry = mmap(NULL, telemetry_size, PROT_READ | PROT_WRITE, MAP_SHARED, telemetry_fd, 0);
    CHECK(telemetry != MAP_FAILED, "cannot map fake app client telemetry");
    memset(telemetry, 0, telemetry_size);
    telemetry->magic = UINT32_C(0x57505243);
    telemetry->version = 6u; /* Presentation-control ABI version. */
    telemetry->session_id = UINT64_C(17);
    telemetry->flags = 1u; /* Active presentation flag. */
    CHECK(setenv("WINE_ANDROID_PRESENT_CONTROL_SHM", telemetry_path, 1) == 0,
          "cannot publish telemetry path");
    /* The file remains mapped and available after unlink, like normal POSIX
     * shared-memory telemetry whose lifetime follows open descriptors. */
    CHECK(bridge_path != NULL && bridge_path[0] != '\0', "bridge path missing");
    bridge_lib = dlopen(bridge_path, RTLD_NOW | RTLD_LOCAL);
    CHECK(bridge_lib != NULL, "could not open fake bridge");
    *(void **)(&set_window) = dlsym(bridge_lib, "mgl_test_bridge_set_window");
    *(void **)(&acquire_count) = dlsym(bridge_lib, "mgl_test_bridge_acquire_count");
    *(void **)(&release_count) = dlsym(bridge_lib, "mgl_test_bridge_release_count");
    *(void **)(&retire_count) = dlsym(bridge_lib, "mgl_test_bridge_retire_count");
    *(void **)(&last_drawable_fn) = dlsym(bridge_lib, "mgl_test_bridge_last_drawable");
    *(void **)(&last_whole_fn) = dlsym(bridge_lib, "mgl_test_bridge_last_whole");
    *(void **)(&last_hwnd_fn) = dlsym(bridge_lib, "mgl_test_bridge_last_hwnd");
    *(void **)(&last_owner_fn) = dlsym(bridge_lib, "mgl_test_bridge_last_owner");
    CHECK(set_window && acquire_count && release_count && retire_count && last_drawable_fn &&
              last_whole_fn && last_hwnd_fn && last_owner_fn,
          "fake bridge controls missing");

    memset(&native, 0, sizeof(native));
    native.eglCreateWindowSurface = fake_create_window;
    native.eglCreatePbufferSurface = fake_create_pbuffer;
    native.eglDestroySurface = fake_destroy_surface;
    native.eglMakeCurrent = fake_make_current;
    native.eglSwapBuffers = fake_swap;
    native.eglSwapInterval = fake_swap_interval;
    native.eglGetError = fake_get_error;
    native.glesFlush = fake_flush;
    native.glesGetIntegerv = fake_gles_version;

    memset(&core, 0, sizeof(core));
    core.native_dispatch = &native;
    core.egl_display = (MGL_EGLDisplay)(uintptr_t)0x1u;
    core.egl_config = (MGL_EGLConfig)(uintptr_t)0x2u;
    core.egl_context = (MGL_EGLContext)(uintptr_t)0x3u;
    core.egl_draw = (MGL_EGLSurface)(uintptr_t)0x4u;
    core.egl_read = core.egl_draw;
    core.egl_draw_owned = GL_TRUE;
    core.egl_read_owned = GL_TRUE;
    core.egl_draw_width = core.egl_read_width = 1;
    core.egl_draw_height = core.egl_read_height = 1;

    /* Use a registry-owned GLX handle for this
     * internal fake-presentation probe. Temporarily install the isolated
     * fake core; restore its actual owned core before context destruction. */
    glx = glXCreateNewContext(dpy, NULL, GLX_RGBA_TYPE, NULL, True);
    CHECK(glx != NULL, "registered context creation failed");
    owned_core = glx->core;
    glx->core = &core;

    window = mgl_glx_drawable_create(dpy, MGL_GLX_DRAWABLE_WINDOW, 0, 640u, 360u);
    CHECK(window != 0, "window record creation failed");
    CHECK(wine_android_register_glx_window(dpy, (unsigned long)window, UINT64_C(0xABCDEF), 77u) ==
              1,
          "app client window registration failed");

    CHECK(glXMakeContextCurrent(dpy, window, window, glx) == True, "direct make-current failed");
    CHECK(core.egl_draw_owned == GL_FALSE && core.egl_read_owned == GL_FALSE,
          "direct surface not borrowed");
    CHECK(create_window_calls == 1u && create_pbuffer_calls == 0u,
          "native surface creation mismatch");
    CHECK(last_native_window == (void *)(uintptr_t)0x1000u, "wrong native window passed to EGL");
    CHECK(acquire_count() == 1u, "bridge acquire count mismatch");
    CHECK(last_drawable_fn() == (uint32_t)window, "bridge did not receive drawable identity");
    CHECK(last_whole_fn() == 77u && last_hwnd_fn() == UINT64_C(0xABCDEF),
          "bridge identity mismatch");
    CHECK((last_owner_fn() >> 48) == UINT64_C(0x474c),
          "bridge owner is outside app client GL namespace");
    CHECK(swap_interval_calls == 1u && last_swap_interval == 0,
          "native async default must use zero swap interval");

    glXSwapBuffers(dpy, window);
    CHECK(swap_calls == 1u, "native EGL swap not used");
    CHECK(telemetry->present_count == UINT64_C(1),
          "OpenGL swap not recorded by presentation telemetry");
    CHECK((telemetry->flags & 2u) != 0u && telemetry->last_present_ns != 0u,
          "OpenGL native-present flag/timestamp not published");

    /* Shared FPS pacing uses monotonic deadlines and preserves counters. */
    telemetry->target_fps_millihz = 120000u;
    glXSwapBuffers(dpy, window);
    glXSwapBuffers(dpy, window);
    CHECK(swap_calls == 3u && telemetry->present_count == UINT64_C(3),
          "target-paced asynchronous native swaps not submitted/counting");
    telemetry->target_fps_millihz = 0u;

    glXSwapIntervalEXT(dpy, window, 2);
    CHECK(swap_interval_calls == 2u && last_swap_interval == 2, "swap interval not applied");

    set_window((uintptr_t)0u);
    glXSwapBuffers(dpy, window);
    CHECK(create_pbuffer_calls == 1u, "surface loss did not create recovery pbuffer");
    CHECK(swap_calls == 3u, "surface-loss frame should be dropped");
    CHECK(telemetry->present_count == UINT64_C(3), "lost surface incorrectly counted as a frame");
    CHECK(swap_interval_calls == 3u && last_swap_interval == 2,
          "surface-loss rebind did not preserve swap interval");

    set_window(((uintptr_t)0x2000u));
    glXSwapBuffers(dpy, window);
    CHECK(create_window_calls == 2u, "replacement native surface not created");
    CHECK(swap_calls == 3u, "replacement-generation frame should be dropped");
    CHECK(swap_interval_calls == 4u && last_swap_interval == 2,
          "replacement native surface did not restore swap interval");
    glXSwapBuffers(dpy, window);
    CHECK(swap_calls == 4u, "replacement native surface did not swap");
    CHECK(telemetry->present_count == UINT64_C(4),
          "replacement surface should count one more frame");

    {
        unsigned releases_before = release_count();
        set_window((uintptr_t)0x3000u);
        fail_create_window = 1;
        glXSwapBuffers(dpy, window);
        CHECK(create_window_calls == 3u, "transient replacement window create was not attempted");
        CHECK(create_pbuffer_calls == 2u,
              "transient window-create failure did not enter recovery pbuffer");
        CHECK(release_count() == releases_before + 3u,
              "transient window-create failure leaked an acquired native-window reference");
        fail_create_window = 0;
        glXSwapBuffers(dpy, window);
        CHECK(create_window_calls == 4u,
              "window generation did not recover after transient create failure");
        glXSwapBuffers(dpy, window);
        CHECK(swap_calls == 5u, "recovered native surface did not swap");
        CHECK(telemetry->present_count == UINT64_C(5),
              "recovery should count only successful native swaps");
    }

    fail_swap = 1;
    next_egl_error = MGL_EGL_BAD_NATIVE_WINDOW;
    glXSwapBuffers(dpy, window);
    CHECK(create_window_calls == 5u, "bad native window did not trigger recreation");
    fail_swap = 0;
    CHECK(telemetry->present_count == UINT64_C(5), "failed native swap must not count as a frame");

    {
        struct mgl_android_present *present = mgl_android_present_ref(dpy, window);
        unsigned prior_created = create_window_calls;
        unsigned prior_destroyed = destroy_surface_calls;
        unsigned prior_swapped = swap_calls;
        CHECK(present != NULL, "native presentation not registered");
        CHECK(mgl_android_present_swap(present, &core, 1560, 720) == 1,
              "same-window fullscreen resize rebind failed");
        CHECK(create_window_calls == prior_created + 1u &&
                  destroy_surface_calls == prior_destroyed + 1u,
              "fullscreen resize retained stale native EGL surface");
        CHECK(swap_calls == prior_swapped, "fullscreen resize presented old-generation frame");
        CHECK(core.egl_draw_width == 1560 && core.egl_draw_height == 720,
              "fullscreen EGL extent not updated");
        CHECK(mgl_android_present_swap(present, &core, 1560, 720) == 1,
              "stable fullscreen presentation failed");
        CHECK(create_window_calls == prior_created + 1u && swap_calls == prior_swapped + 1u,
              "stable extent unnecessarily recreated EGL surface");
        CHECK(mgl_android_present_swap(present, &core, 1280, 705) == 1,
              "same-window windowed resize rebind failed");
        CHECK(create_window_calls == prior_created + 2u &&
                  destroy_surface_calls == prior_destroyed + 2u,
              "windowed resize retained fullscreen EGL surface");
        CHECK(swap_calls == prior_swapped + 1u, "windowed resize presented old-generation frame");
        CHECK(core.egl_draw_width == 1280 && core.egl_draw_height == 705,
              "windowed EGL extent not updated");
        mgl_android_present_unref(present);
    }

    glXDestroyWindow(dpy, (GLXWindow)window);
    CHECK(retire_count() == 0u, "current drawable retired before context released it");

    pbuffer = mgl_glx_drawable_create(dpy, MGL_GLX_DRAWABLE_PBUFFER, 0, 16u, 16u);
    CHECK(pbuffer != 0, "pbuffer record creation failed");
    CHECK(glXMakeContextCurrent(dpy, pbuffer, pbuffer, glx) == True, "pbuffer rebound failed");
    CHECK(core.egl_draw_owned == GL_TRUE && core.egl_read_owned == GL_TRUE,
          "pbuffer ownership not restored");
    CHECK(retire_count() == 1u, "retired drawable not released after context moved away");

    CHECK(glXMakeContextCurrent(dpy, 0, 0, NULL) == True, "final unbind failed");
    mgl_glx_drawable_destroy(dpy, pbuffer);
    glx->core = owned_core;
    glXDestroyContext(dpy, glx);
    (void)dlclose(bridge_lib);
    CHECK(munmap(telemetry, telemetry_size) == 0, "cannot unmap fake telemetry");
    CHECK(close(telemetry_fd) == 0, "cannot close fake telemetry");
    CHECK(unlink(telemetry_path) == 0, "cannot delete fake telemetry");
    puts("presentation_direct: PASS");
    return 0;
}

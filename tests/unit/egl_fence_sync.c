#include "mgl/egl.h"

#include <dlfcn.h>
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

typedef unsigned (*count_fn)(void);

int main(int argc, char **argv) {
    EGLDisplay d;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;
    EGLSync sync;
    EGLSync sync2;
    EGLAttrib value = (EGLAttrib)-7;
    EGLint count = 0;
    EGLint result;
    static const EGLint pbuffer_attrs[] = {EGL_WIDTH, 4, EGL_HEIGHT, 4, EGL_NONE};
    static const EGLAttrib bad_attrs[] = {0x7fffffff, 1, EGL_NONE};
    void *native;
    count_fn finish_count;
    count_fn fence_count;
    count_fn delete_count;
    unsigned finish_before, fence_before, delete_before;

    if (argc != 2)
        return 2;
    native = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    CHECK(native != NULL);
    *(void **)(&finish_count) = dlsym(native, "mgl_test_native_egl_finish_count");
    *(void **)(&fence_count) = dlsym(native, "mgl_test_native_egl_fence_sync_count");
    *(void **)(&delete_count) = dlsym(native, "mgl_test_native_egl_delete_sync_count");
    CHECK(finish_count != NULL && fence_count != NULL && delete_count != NULL);

    d = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL);
    CHECK(d != EGL_NO_DISPLAY);
    CHECK(eglInitialize(d, NULL, NULL));
    CHECK(eglBindAPI(EGL_OPENGL_API));
    CHECK(eglGetConfigs(d, &config, 1, &count) && count > 0);
    surface = eglCreatePbufferSurface(d, config, pbuffer_attrs);
    CHECK(surface != EGL_NO_SURFACE);
    context = eglCreateContext(d, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, NULL);
    CHECK(context != EGL_NO_CONTEXT);

    CHECK(eglCreateSync(d, EGL_SYNC_FENCE, NULL) == EGL_NO_SYNC);
    CHECK(eglGetError() == EGL_BAD_MATCH);
    CHECK(eglCreateSync(d, 0x7fffffff, NULL) == EGL_NO_SYNC);
    CHECK(eglGetError() == EGL_BAD_PARAMETER);
    CHECK(eglCreateSync(d, EGL_SYNC_FENCE, bad_attrs) == EGL_NO_SYNC);
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);

    CHECK(eglMakeCurrent(d, surface, surface, context));
    finish_before = finish_count();
    fence_before = fence_count();
    delete_before = delete_count();
    sync = eglCreateSync(d, EGL_SYNC_FENCE, NULL);
    CHECK(sync != EGL_NO_SYNC);
    CHECK(fence_count() == fence_before + 1u);
    CHECK(finish_count() == finish_before + 1u);
    CHECK(delete_count() == delete_before + 1u);

    CHECK(eglGetSyncAttrib(d, sync, EGL_SYNC_TYPE, &value));
    CHECK(value == EGL_SYNC_FENCE);
    CHECK(eglGetSyncAttrib(d, sync, EGL_SYNC_STATUS, &value));
    CHECK(value == EGL_SIGNALED);
    CHECK(eglGetSyncAttrib(d, sync, EGL_SYNC_CONDITION, &value));
    CHECK(value == EGL_SYNC_PRIOR_COMMANDS_COMPLETE);
    value = (EGLAttrib)0x12345678;
    CHECK(!eglGetSyncAttrib(d, sync, 0x7fffffff, &value));
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
    CHECK(value == (EGLAttrib)0x12345678);

    /* Client waits do not require a current context. The synchronous lowering
     * has already completed the fence, so neither timeout nor flush adds work. */
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    finish_before = finish_count();
    result = eglClientWaitSync(d, sync, 0, 0);
    CHECK(result == EGL_CONDITION_SATISFIED);
    CHECK(eglClientWaitSync(d, sync, EGL_SYNC_FLUSH_COMMANDS_BIT, EGL_FOREVER) ==
          EGL_CONDITION_SATISFIED);
    CHECK(finish_count() == finish_before);
    CHECK(eglClientWaitSync(d, sync, 2, 0) == EGL_FALSE);
    CHECK(eglGetError() == EGL_BAD_PARAMETER);

    CHECK(!eglWaitSync(d, sync, 0));
    CHECK(eglGetError() == EGL_BAD_MATCH);
    CHECK(eglMakeCurrent(d, surface, surface, context));
    CHECK(!eglWaitSync(d, sync, 1));
    CHECK(eglGetError() == EGL_BAD_PARAMETER);
    CHECK(eglWaitSync(d, sync, 0));

    CHECK(eglDestroySync(d, sync));
    value = 91;
    CHECK(!eglGetSyncAttrib(d, sync, EGL_SYNC_STATUS, &value));
    CHECK(eglGetError() == EGL_BAD_PARAMETER);
    CHECK(value == 91);
    CHECK(!eglDestroySync(d, sync));
    CHECK(eglGetError() == EGL_BAD_PARAMETER);

    /* Termination invalidates live sync handles immediately. */
    sync2 = eglCreateSync(d, EGL_SYNC_FENCE, NULL);
    CHECK(sync2 != EGL_NO_SYNC);
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    CHECK(eglDestroyContext(d, context));
    CHECK(eglDestroySurface(d, surface));
    CHECK(eglTerminate(d));
    CHECK(!eglDestroySync(d, sync2));
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);

    dlclose(native);
    return 0;
}

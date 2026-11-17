#include "mgl/egl.h"

#include <dlfcn.h>
#include <pthread.h>
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

struct hidden_binding_case {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int force_release_without_select;
    int result;
};

static void *hidden_binding_thread(void *opaque) {
    struct hidden_binding_case *c = (struct hidden_binding_case *)opaque;
    c->result = 1;
    if (eglQueryAPI() != EGL_NONE)
        return NULL;
    if (!eglMakeCurrent(c->display, c->surface, c->surface, c->context))
        return NULL;
    if (eglGetCurrentContext() != EGL_NO_CONTEXT)
        return NULL;
    if (!eglWaitClient() || !eglWaitNative(EGL_CORE_NATIVE_ENGINE))
        return NULL;
    if (c->force_release_without_select) {
        if (!eglReleaseThread())
            return NULL;
        if (!eglBindAPI(EGL_OPENGL_API))
            return NULL;
        if (eglGetCurrentContext() != EGL_NO_CONTEXT)
            return NULL;
        c->result = 0;
        return NULL;
    }
    if (!eglMakeCurrent(c->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
        return NULL;
    if (eglGetCurrentContext() != EGL_NO_CONTEXT)
        return NULL;
    if (!eglBindAPI(EGL_OPENGL_API))
        return NULL;
    if (eglGetCurrentContext() != c->context)
        return NULL;
    if (!eglBindAPI(EGL_OPENGL_API))
        return NULL;
    if (!eglReleaseThread())
        return NULL;
    if (eglQueryAPI() != EGL_NONE || eglGetCurrentContext() != EGL_NO_CONTEXT)
        return NULL;
    c->result = 0;
    return NULL;
}

int main(int argc, char **argv) {
    static const EGLint pbuffer_attrs[] = {EGL_WIDTH, 4, EGL_HEIGHT, 4, EGL_NONE};
    EGLDisplay d;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;
    EGLint count = 0;
    void *native;
    count_fn finish_count;
    unsigned before;
    pthread_t worker;
    struct hidden_binding_case hidden;

    if (argc != 2)
        return 2;
    native = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    CHECK(native != NULL);
    *(void **)(&finish_count) = dlsym(native, "mgl_test_native_egl_finish_count");
    CHECK(finish_count != NULL);

    d = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL);
    CHECK(d != EGL_NO_DISPLAY);
    CHECK(eglInitialize(d, NULL, NULL));
    CHECK(eglBindAPI(EGL_OPENGL_API));
    CHECK(eglGetConfigs(d, &config, 1, &count) && count > 0);
    surface = eglCreatePbufferSurface(d, config, pbuffer_attrs);
    CHECK(surface != EGL_NO_SURFACE);
    context = eglCreateContext(d, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, NULL);
    CHECK(context != EGL_NO_CONTEXT);

    before = finish_count();
    CHECK(eglWaitClient());
    CHECK(finish_count() == before);
    CHECK(eglWaitNative(EGL_CORE_NATIVE_ENGINE));
    CHECK(finish_count() == before);
    CHECK(!eglWaitNative(0x7fffffff));
    CHECK(eglGetError() == EGL_BAD_PARAMETER);

    CHECK(eglMakeCurrent(d, surface, surface, context));
    before = finish_count();
    CHECK(eglWaitClient());
    CHECK(finish_count() == before + 1u);
    before = finish_count();
    CHECK(eglWaitNative(EGL_CORE_NATIVE_ENGINE));
    CHECK(finish_count() == before);

    before = finish_count();
    CHECK(eglWaitGL());
    CHECK(finish_count() == before);
    CHECK(eglQueryAPI() == EGL_OPENGL_API);

    CHECK(eglDestroySurface(d, surface));
    before = finish_count();
    CHECK(!eglWaitClient());
    CHECK(eglGetError() == EGL_BAD_CURRENT_SURFACE);
    CHECK(finish_count() == before);
    CHECK(!eglWaitNative(EGL_CORE_NATIVE_ENGINE));
    CHECK(eglGetError() == EGL_BAD_CURRENT_SURFACE);
    before = finish_count();
    CHECK(eglWaitGL());
    CHECK(finish_count() == before);
    CHECK(eglQueryAPI() == EGL_OPENGL_API);

    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, context));
    before = finish_count();
    CHECK(eglWaitClient());
    CHECK(finish_count() == before + 1u);
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

    hidden.display = d;
    hidden.surface = EGL_NO_SURFACE;
    hidden.context = context;
    hidden.force_release_without_select = 0;
    hidden.result = 1;
    CHECK(pthread_create(&worker, NULL, hidden_binding_thread, &hidden) == 0);
    CHECK(pthread_join(worker, NULL) == 0);
    CHECK(hidden.result == 0);
    hidden.force_release_without_select = 1;
    hidden.result = 1;
    CHECK(pthread_create(&worker, NULL, hidden_binding_thread, &hidden) == 0);
    CHECK(pthread_join(worker, NULL) == 0);
    CHECK(hidden.result == 0);

    CHECK(eglDestroyContext(d, context));
    CHECK(eglTerminate(d));
    dlclose(native);
    return 0;
}

#include "mgl/egl.h"
#include "mgl/gl.h"

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

struct thread_case {
    EGLDisplay display;
    EGLSurface busy_surface;
    EGLSurface free_surface;
    EGLContext context;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int ready;
    int release;
    int result;
};

static void publish_thread_ready(struct thread_case *t, int wait_for_release) {
    pthread_mutex_lock(&t->mutex);
    t->ready = 1;
    pthread_cond_broadcast(&t->cond);
    while (wait_for_release && !t->release)
        pthread_cond_wait(&t->cond, &t->mutex);
    pthread_mutex_unlock(&t->mutex);
}

static void *surface_thread(void *opaque) {
    struct thread_case *t = (struct thread_case *)opaque;
    t->result = 1;
    if (!eglBindAPI(EGL_OPENGL_API)) {
        publish_thread_ready(t, 0);
        return NULL;
    }
    if (eglMakeCurrent(t->display, t->busy_surface, t->busy_surface, t->context)) {
        publish_thread_ready(t, 0);
        return NULL;
    }
    if (eglGetError() != EGL_BAD_ACCESS) {
        publish_thread_ready(t, 0);
        return NULL;
    }
    if (!eglMakeCurrent(t->display, t->free_surface, t->free_surface, t->context)) {
        publish_thread_ready(t, 0);
        return NULL;
    }
    if (eglGetCurrentContext() != t->context) {
        publish_thread_ready(t, 0);
        return NULL;
    }
    t->result = 0;
    publish_thread_ready(t, 1);
    if (!eglMakeCurrent(t->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
        t->result = 1;
    return NULL;
}

int main(void) {
    static const EGLint pbuffer_attrs[] = {EGL_WIDTH, 8, EGL_HEIGHT, 8, EGL_NONE};
    static const EGLint context_33_attrs[] = {EGL_CONTEXT_MAJOR_VERSION,
                                              3,
                                              EGL_CONTEXT_MINOR_VERSION,
                                              3,
                                              EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                                              EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR,
                                              EGL_NONE};
    static const EGLint context_34_attrs[] = {EGL_CONTEXT_MAJOR_VERSION, 3,
                                              EGL_CONTEXT_MINOR_VERSION, 4, EGL_NONE};
    EGLDisplay d;
    EGLConfig config;
    EGLContext context, other;
    EGLSurface surface, other_surface;
    EGLint count = 0, value = 0, config_id = 0;
    pthread_t thread;
    struct thread_case tc;
    int worker_query_ok;

    d = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL);
    CHECK(d != EGL_NO_DISPLAY);
    CHECK(eglInitialize(d, NULL, NULL));
    CHECK(eglBindAPI(EGL_OPENGL_API));
    CHECK(eglGetConfigs(d, &config, 1, &count) && count > 0);
    context = eglCreateContext(d, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, context_33_attrs);
    CHECK(context != EGL_NO_CONTEXT);
    CHECK(eglQueryContext(d, context, EGL_CONFIG_ID, &value) && value == 0);
    CHECK(eglQueryContext(d, context, EGL_RENDER_BUFFER, &value) && value == EGL_NONE);
    CHECK(eglQueryContext(d, context, EGL_CONTEXT_MAJOR_VERSION, &value) && value == 3);
    CHECK(eglQueryContext(d, context, EGL_CONTEXT_MINOR_VERSION, &value) && value == 3);
    CHECK(eglDestroyContext(d, context));
    context = eglCreateContext(d, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, context_34_attrs);
    CHECK(context == EGL_NO_CONTEXT);
    CHECK(eglGetError() == EGL_BAD_MATCH);
    surface = eglCreatePbufferSurface(d, config, pbuffer_attrs);
    CHECK(surface != EGL_NO_SURFACE);
    other_surface = eglCreatePbufferSurface(d, config, pbuffer_attrs);
    CHECK(other_surface != EGL_NO_SURFACE);
    context = eglCreateContext(d, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, NULL);
    CHECK(context != EGL_NO_CONTEXT);
    other = eglCreateContext(d, config, EGL_NO_CONTEXT, NULL);
    CHECK(other != EGL_NO_CONTEXT);
    CHECK(eglGetConfigAttrib(d, config, EGL_CONFIG_ID, &config_id));
    CHECK(eglQueryContext(d, other, EGL_CONFIG_ID, &value) && value == config_id);
    CHECK(eglMakeCurrent(d, surface, surface, context));
    {
        GLint viewport[4] = {-1, -1, -1, -1}, scissor[4] = {-1, -1, -1, -1};
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_SCISSOR_BOX, scissor);
        CHECK(viewport[0] == 0 && viewport[1] == 0 && viewport[2] == 8 && viewport[3] == 8);
        CHECK(scissor[0] == 0 && scissor[1] == 0 && scissor[2] == 8 && scissor[3] == 8);
    }
    CHECK(eglGetCurrentDisplay() == d);
    CHECK(eglGetCurrentContext() == context);
    CHECK(eglGetCurrentSurface(EGL_DRAW) == surface);
    CHECK(eglQueryContext(d, context, EGL_RENDER_BUFFER, &value) && value == EGL_BACK_BUFFER);

    tc.display = d;
    tc.busy_surface = surface;
    tc.free_surface = other_surface;
    tc.context = other;
    tc.ready = 0;
    tc.release = 0;
    tc.result = 1;
    CHECK(pthread_mutex_init(&tc.mutex, NULL) == 0);
    CHECK(pthread_cond_init(&tc.cond, NULL) == 0);
    CHECK(pthread_create(&thread, NULL, surface_thread, &tc) == 0);
    pthread_mutex_lock(&tc.mutex);
    while (!tc.ready)
        pthread_cond_wait(&tc.cond, &tc.mutex);
    pthread_mutex_unlock(&tc.mutex);
    worker_query_ok = tc.result == 0 && eglQueryContext(d, other, EGL_RENDER_BUFFER, &value) &&
                      value == EGL_BACK_BUFFER;
    pthread_mutex_lock(&tc.mutex);
    tc.release = 1;
    pthread_cond_broadcast(&tc.cond);
    pthread_mutex_unlock(&tc.mutex);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(worker_query_ok);
    CHECK(tc.result == 0);
    CHECK(eglQueryContext(d, other, EGL_RENDER_BUFFER, &value) && value == EGL_NONE);
    CHECK(pthread_cond_destroy(&tc.cond) == 0);
    CHECK(pthread_mutex_destroy(&tc.mutex) == 0);
    CHECK(eglGetCurrentContext() == context);
    CHECK(eglGetCurrentSurface(EGL_DRAW) == surface);

    CHECK(eglTerminate(d));
    CHECK(eglGetCurrentDisplay() == d);
    CHECK(eglGetCurrentContext() == context);
    CHECK(eglGetCurrentSurface(EGL_DRAW) == surface);
    CHECK(!eglGetConfigs(d, NULL, 0, &count));
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    value = 1234;
    CHECK(!eglQuerySurface(d, surface, EGL_WIDTH, &value));
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(value == 1234);
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    CHECK(eglGetCurrentContext() == EGL_NO_CONTEXT);

    CHECK(eglInitialize(d, NULL, NULL));
    value = 5678;
    CHECK(!eglQuerySurface(d, surface, EGL_WIDTH, &value));
    CHECK(eglGetError() == EGL_BAD_SURFACE);
    CHECK(value == 5678);
    CHECK(!eglDestroyContext(d, context));
    CHECK(eglGetError() == EGL_BAD_CONTEXT);
    CHECK(!eglDestroyContext(d, other));
    CHECK(eglGetError() == EGL_BAD_CONTEXT);
    CHECK(!eglDestroySurface(d, other_surface));
    CHECK(eglGetError() == EGL_BAD_SURFACE);

    CHECK(eglGetConfigs(d, &config, 1, &count) && count > 0);
    surface = eglCreatePbufferSurface(d, config, pbuffer_attrs);
    CHECK(surface != EGL_NO_SURFACE);
    context = eglCreateContext(d, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, NULL);
    CHECK(context != EGL_NO_CONTEXT);
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, context));
    {
        GLint viewport[4] = {-1, -1, -1, -1}, scissor[4] = {-1, -1, -1, -1};
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_SCISSOR_BOX, scissor);
        CHECK(viewport[0] == 0 && viewport[1] == 0 && viewport[2] == 0 && viewport[3] == 0);
        CHECK(scissor[0] == 0 && scissor[1] == 0 && scissor[2] == 0 && scissor[3] == 0);
    }
    CHECK(eglTerminate(d));
    CHECK(eglReleaseThread());
    CHECK(eglGetCurrentContext() == EGL_NO_CONTEXT);
    CHECK(eglQueryAPI() == EGL_NONE);
    CHECK(eglInitialize(d, NULL, NULL));
    CHECK(eglTerminate(d));
    CHECK(eglTerminate(d));
    return 0;
}

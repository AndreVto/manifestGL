#include "native/egl_abi.h"
#include "mgl/types.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static MGL_EGLint fake_error = 0x3000;
static unsigned create_pbuffer_count;
static unsigned create_window_count;
static unsigned destroy_surface_count;
static unsigned destroy_context_count;
static unsigned make_current_count;
static unsigned swap_count;
static unsigned swap_interval_count;
static unsigned finish_count;
static unsigned flush_count;
static unsigned fence_sync_count;
static unsigned delete_sync_count;
static MGL_EGLint last_swap_interval;
static MGL_EGLNativeWindowType last_native_window;
static int fail_swap;

static uintptr_t fake_generic(void) {
    return 0u;
}

MGL_EGLDisplay eglGetDisplay(MGL_EGLNativeDisplayType display) {
    (void)display;
    return (MGL_EGLDisplay)(uintptr_t)0x11u;
}

MGL_EGLBoolean eglInitialize(MGL_EGLDisplay d, MGL_EGLint *major, MGL_EGLint *minor) {
    (void)d;
    if (major)
        *major = 1;
    if (minor)
        *minor = 5;
    return MGL_EGL_TRUE;
}

MGL_EGLBoolean eglTerminate(MGL_EGLDisplay d) {
    (void)d;
    return MGL_EGL_TRUE;
}

MGL_EGLBoolean eglChooseConfig(MGL_EGLDisplay d, const MGL_EGLint *a, MGL_EGLConfig *c,
                               MGL_EGLint n, MGL_EGLint *count) {
    (void)d;
    (void)a;
    if (count)
        *count = 1;
    if (c && n > 0)
        c[0] = (MGL_EGLConfig)(uintptr_t)0x22u;
    return MGL_EGL_TRUE;
}

MGL_EGLBoolean eglBindAPI(uint32_t api) {
    (void)api;
    return MGL_EGL_TRUE;
}

MGL_EGLContext eglCreateContext(MGL_EGLDisplay d, MGL_EGLConfig c, MGL_EGLContext share,
                                const MGL_EGLint *a) {
    (void)d;
    (void)c;
    (void)share;
    (void)a;
    return malloc(1);
}

MGL_EGLBoolean eglDestroyContext(MGL_EGLDisplay d, MGL_EGLContext c) {
    (void)d;
    ++destroy_context_count;
    free(c);
    return MGL_EGL_TRUE;
}

MGL_EGLSurface eglCreatePbufferSurface(MGL_EGLDisplay d, MGL_EGLConfig c, const MGL_EGLint *a) {
    (void)d;
    (void)c;
    (void)a;
    ++create_pbuffer_count;
    return malloc(1);
}

MGL_EGLSurface eglCreateWindowSurface(MGL_EGLDisplay d, MGL_EGLConfig c, MGL_EGLNativeWindowType w,
                                      const MGL_EGLint *a) {
    (void)d;
    (void)c;
    (void)a;
    ++create_window_count;
    last_native_window = w;
    return malloc(1);
}

MGL_EGLBoolean eglDestroySurface(MGL_EGLDisplay d, MGL_EGLSurface s) {
    (void)d;
    ++destroy_surface_count;
    free(s);
    return MGL_EGL_TRUE;
}

MGL_EGLBoolean eglMakeCurrent(MGL_EGLDisplay d, MGL_EGLSurface draw, MGL_EGLSurface read,
                              MGL_EGLContext c) {
    (void)d;
    (void)draw;
    (void)read;
    (void)c;
    ++make_current_count;
    return MGL_EGL_TRUE;
}

MGL_EGLBoolean eglSwapBuffers(MGL_EGLDisplay d, MGL_EGLSurface s) {
    (void)d;
    (void)s;
    ++swap_count;
    return fail_swap ? MGL_EGL_FALSE : MGL_EGL_TRUE;
}

MGL_EGLBoolean eglSwapInterval(MGL_EGLDisplay d, MGL_EGLint i) {
    (void)d;
    ++swap_interval_count;
    last_swap_interval = i;
    return MGL_EGL_TRUE;
}

MGL_EGLint eglGetError(void) {
    MGL_EGLint e = fake_error;
    fake_error = 0x3000;
    return e;
}

const char *eglQueryString(MGL_EGLDisplay d, MGL_EGLint n) {
    (void)d;
    (void)n;
    return "fake native EGL";
}

void mgl_test_native_egl_set_fail_swap(int fail, MGL_EGLint error) {
    fail_swap = fail;
    fake_error = error;
}

unsigned mgl_test_native_egl_create_pbuffer_count(void) {
    return create_pbuffer_count;
}

unsigned mgl_test_native_egl_create_window_count(void) {
    return create_window_count;
}

unsigned mgl_test_native_egl_destroy_surface_count(void) {
    return destroy_surface_count;
}

unsigned mgl_test_native_egl_destroy_context_count(void) {
    return destroy_context_count;
}

unsigned mgl_test_native_egl_make_current_count(void) {
    return make_current_count;
}

unsigned mgl_test_native_egl_swap_count(void) {
    return swap_count;
}

unsigned mgl_test_native_egl_swap_interval_count(void) {
    return swap_interval_count;
}

MGL_EGLint mgl_test_native_egl_last_swap_interval(void) {
    return last_swap_interval;
}

MGL_EGLNativeWindowType mgl_test_native_egl_last_native_window(void) {
    return last_native_window;
}

unsigned mgl_test_native_egl_finish_count(void) {
    return finish_count;
}

unsigned mgl_test_native_egl_flush_count(void) {
    return flush_count;
}

unsigned mgl_test_native_egl_fence_sync_count(void) {
    return fence_sync_count;
}

unsigned mgl_test_native_egl_delete_sync_count(void) {
    return delete_sync_count;
}

const GLubyte *glGetString(GLenum name) {
    (void)name;
    return (const GLubyte *)"fake GLES 3.0";
}

GLenum glGetError(void) {
    return GL_NO_ERROR;
}

GLenum glGetGraphicsResetStatus(void) {
    return GL_NO_ERROR;
}

void glGetIntegerv(GLenum pname, GLint *value) {
    (void)pname;
    if (value)
        *value = 16;
}

void glBindFramebuffer(GLenum target, GLuint fbo) {
    (void)target;
    (void)fbo;
}

void glFinish(void) {
    ++finish_count;
}

void glFlush(void) {
    ++flush_count;
}

GLsync glFenceSync(GLenum condition, GLbitfield flags) {
    (void)condition;
    (void)flags;
    ++fence_sync_count;
    return malloc(1);
}

void glDeleteSync(GLsync sync) {
    ++delete_sync_count;
    free(sync);
}

void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type,
                  void *pixels) {
    (void)x;
    (void)y;
    (void)format;
    (void)type;
    if (pixels && w > 0 && h > 0)
        memset(pixels, 0, (size_t)w * (size_t)h * 4u);
}

mgl_egl_proc_t eglGetProcAddress(const char *name) {
    union {
        void *obj;
        mgl_egl_proc_t fn;
    } cast;

    void *p = NULL;
    if (!name)
        return NULL;
    if (!strcmp(name, "glGetString"))
        p = (void *)(uintptr_t)&glGetString;
    else if (!strcmp(name, "glGetError"))
        p = (void *)(uintptr_t)&glGetError;
    else if (!strcmp(name, "glGetGraphicsResetStatus"))
        p = (void *)(uintptr_t)&glGetGraphicsResetStatus;
    else if (!strcmp(name, "glGetIntegerv"))
        p = (void *)(uintptr_t)&glGetIntegerv;
    else if (!strcmp(name, "glBindFramebuffer"))
        p = (void *)(uintptr_t)&glBindFramebuffer;
    else if (!strcmp(name, "glFinish"))
        p = (void *)(uintptr_t)&glFinish;
    else if (!strcmp(name, "glFlush"))
        p = (void *)(uintptr_t)&glFlush;
    else if (!strcmp(name, "glFenceSync"))
        p = (void *)(uintptr_t)&glFenceSync;
    else if (!strcmp(name, "glDeleteSync"))
        p = (void *)(uintptr_t)&glDeleteSync;
    else if (!strcmp(name, "glReadPixels"))
        p = (void *)(uintptr_t)&glReadPixels;
    else
        p = (void *)(uintptr_t)&fake_generic;
    cast.obj = p;
    return cast.fn;
}

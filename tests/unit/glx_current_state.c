#include "mgl/glx.h"
#include "mgl/gl.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct thread_probe {
    int ok;
};

static int fail(int n, const char *message) {
    fprintf(stderr, "GLX current-state failure %d: %s\n", n, message);
    return n;
}

static void *probe_empty_thread(void *opaque) {
    struct thread_probe *probe = (struct thread_probe *)opaque;
    probe->ok = glXGetCurrentContext() == NULL && glXGetCurrentDisplay() == NULL &&
                glXGetCurrentDrawable() == 0 && glXGetCurrentReadDrawable() == 0;
    return NULL;
}

int main(void) {
    Display *display = (Display *)(uintptr_t)0x12340u;
    GLXContext context;
    GLXContext shared;
    GLXContext foreign = (GLXContext)(uintptr_t)0x56780u;
    Display *other_display = (Display *)(uintptr_t)0x43210u;
    GLXPbuffer draw;
    GLXPbuffer read;
    pthread_t thread;
    struct thread_probe probe = {0};
    void *native_handle;
    unsigned (*destroy_context_count)(void);
    unsigned destroy_count_before;
    const char *native_path;
    static const int draw_attrs[] = {GLX_PBUFFER_WIDTH, 7, GLX_PBUFFER_HEIGHT, 5, 0};
    static const int read_attrs[] = {GLX_PBUFFER_WIDTH, 3, GLX_PBUFFER_HEIGHT, 9, 0};

    if (glXGetCurrentContext() != NULL || glXGetCurrentDisplay() != NULL ||
        glXGetCurrentDrawable() != 0 || glXGetCurrentReadDrawable() != 0)
        return fail(1, "initial thread-local current state");

    if (glXGetProcAddress((const GLubyte *)"glGetString") == NULL ||
        glXGetProcAddress((const GLubyte *)"glXGetCurrentContext") == NULL ||
        glXGetProcAddress((const GLubyte *)"glXCreateContextAttribsARB") == NULL ||
        glXGetProcAddressARB((const GLubyte *)"glGetString") == NULL ||
        glXGetProcAddress((const GLubyte *)"mglDefinitelyNotAFunction") != NULL)
        return fail(2, "proc-address core/GLX/extension/unknown behavior");

    draw = glXCreatePbuffer(display, NULL, draw_attrs);
    read = glXCreatePbuffer(display, NULL, read_attrs);
    if (draw == 0 || read == 0 || draw == read)
        return fail(3, "pbuffer setup");

    context = glXCreateNewContext(display, NULL, GLX_RGBA_TYPE, NULL, True);
    if (context == NULL)
        return fail(4, "context creation");

    /* Registry check must precede dereference and
     * native work across create/share, destroy, bind, and query entrypoints. */
    if (glXCreateNewContext(display, NULL, GLX_RGBA_TYPE, foreign, True) != NULL ||
        glXCreateNewContext(other_display, NULL, GLX_RGBA_TYPE, context, True) != NULL)
        return fail(19, "unknown or foreign-display share handle accepted");
    shared = glXCreateNewContext(display, NULL, GLX_RGBA_TYPE, context, True);
    if (shared == NULL)
        return fail(20, "valid same-display share context refused");
    glXDestroyContext(other_display, shared);
    if (glXIsDirect(display, shared) != True)
        return fail(21, "wrong-display destroy changed live context");
    glXDestroyContext(display, foreign);
    glXDestroyContext(display, shared);
    glXDestroyContext(display, shared);
    if (glXIsDirect(display, shared) != False)
        return fail(22, "destroyed context remained live or duplicate destroy failed");
    if (glXMakeContextCurrent(display, 0, 0, foreign) != False ||
        glXMakeCurrent(display, 0, shared) != False ||
        glXMakeContextCurrent(other_display, 0, 0, context) != False)
        return fail(23, "unknown, destroyed, or wrong-display context bind succeeded");
    if (glXGetCurrentContext() != NULL)
        return fail(24, "failed initial binds installed a context");

    native_path = getenv("MGL_EGL_LIBRARY");
    if (native_path == NULL)
        return fail(5, "missing fake-native path");
    native_handle = dlopen(native_path, RTLD_NOW | RTLD_LOCAL);
    if (native_handle == NULL)
        return fail(6, "open fake native for lifetime probe");
    *(void **)(&destroy_context_count) =
        dlsym(native_handle, "mgl_test_native_egl_destroy_context_count");
    if (destroy_context_count == NULL)
        return fail(7, "resolve native context-destroy counter");
    destroy_count_before = destroy_context_count();

    if (!glXMakeContextCurrent(display, draw, read, context))
        return fail(8, "make current");
    if (glXGetCurrentContext() != context || glXGetCurrentDisplay() != display ||
        glXGetCurrentDrawable() != draw || glXGetCurrentReadDrawable() != read)
        return fail(9, "current getters after bind");
    if (glXMakeContextCurrent(display, 0, 0, foreign) != False ||
        glXMakeContextCurrent(other_display, draw, read, context) != False ||
        glXMakeCurrent(display, 0, shared) != False)
        return fail(25, "invalid bind unexpectedly succeeded while another context was current");
    if (glXGetCurrentContext() != context || glXGetCurrentDisplay() != display ||
        glXGetCurrentDrawable() != draw || glXGetCurrentReadDrawable() != read)
        return fail(26, "invalid bind changed current state");

    if (pthread_create(&thread, NULL, probe_empty_thread, &probe) != 0)
        return fail(10, "thread creation");
    if (pthread_join(thread, NULL) != 0 || !probe.ok)
        return fail(11, "current state leaked across threads");

    glXDestroyContext(display, context);
    if (glXQueryContext(display, context, GLX_SCREEN, NULL) != GLX_BAD_CONTEXT ||
        glXCreateNewContext(display, NULL, GLX_RGBA_TYPE, context, True) != NULL)
        return fail(27, "delete-pending context remains available for query or sharing");
    if (destroy_context_count() != destroy_count_before)
        return fail(12, "destroy-current released native context prematurely");
    if (glXGetCurrentContext() != context || glXGetCurrentDisplay() != display ||
        glXGetCurrentDrawable() != draw || glXGetCurrentReadDrawable() != read)
        return fail(13, "destroy-current did not preserve current binding");

    if (glXMakeContextCurrent(display, draw, read, context) != False)
        return fail(14, "delete-pending context accepted by MakeContextCurrent");
    if (glXGetCurrentContext() != context || glXGetCurrentDisplay() != display)
        return fail(15, "failed rebind changed current state");

    /* Releasing the last current reference completes deferred destruction. */
    if (!glXMakeContextCurrent(display, 0, 0, NULL))
        return fail(16, "release current");
    if (destroy_context_count() != destroy_count_before + 1u)
        return fail(17, "last current release did not destroy native context exactly once");
    glXDestroyContext(display, context);
    if (glXMakeContextCurrent(display, 0, 0, context) != False ||
        glXQueryContext(display, context, GLX_SCREEN, NULL) != GLX_BAD_CONTEXT ||
        glXCreateNewContext(display, NULL, GLX_RGBA_TYPE, context, True) != NULL)
        return fail(28, "stale pointer accepted after final destruction");
    if (glXGetCurrentContext() != NULL || glXGetCurrentDisplay() != NULL ||
        glXGetCurrentDrawable() != 0 || glXGetCurrentReadDrawable() != 0)
        return fail(18, "current getters after release");

    glXDestroyPbuffer(display, draw);
    glXDestroyPbuffer(display, read);
    (void)dlclose(native_handle);

    puts("GLX current-state and deferred-destroy semantics passed");
    return 0;
}

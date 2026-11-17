#include "mgl/egl.h"
#include "mgl/gl.h"

#include <stdio.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "CHECK failed %s:%d: %s (egl=%#x)\n", __FILE__, __LINE__, #x,          \
                    eglGetError());                                                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static EGLContext create(EGLDisplay d, EGLContext share, const EGLint *attrs, EGLint error) {
    EGLContext c = eglCreateContext(d, EGL_NO_CONFIG_KHR, share, attrs);
    if (error == EGL_SUCCESS) {
        if (c == EGL_NO_CONTEXT)
            fprintf(stderr, "expected context, egl=%#x\n", eglGetError());
    } else {
        if (c != EGL_NO_CONTEXT || eglGetError() != error) {
            fprintf(stderr, "expected error %#x\n", error);
            return (EGLContext)(uintptr_t)1;
        }
    }
    return c;
}

int main(void) {
    static const EGLint compat33[] = {EGL_CONTEXT_MAJOR_VERSION,
                                      3,
                                      EGL_CONTEXT_MINOR_VERSION,
                                      3,
                                      EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                                      EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR,
                                      EGL_NONE};
    static const EGLint compat32[] = {EGL_CONTEXT_MAJOR_VERSION,
                                      3,
                                      EGL_CONTEXT_MINOR_VERSION,
                                      2,
                                      EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                                      EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR,
                                      EGL_NONE};
    static const EGLint default33[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3,
                                       EGL_NONE};
    static const EGLint core33[] = {EGL_CONTEXT_MAJOR_VERSION,
                                    3,
                                    EGL_CONTEXT_MINOR_VERSION,
                                    3,
                                    EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                                    EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
                                    EGL_NONE};
    static const EGLint both33[] = {EGL_CONTEXT_MAJOR_VERSION,
                                    3,
                                    EGL_CONTEXT_MINOR_VERSION,
                                    3,
                                    EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                                    EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR |
                                        EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR,
                                    EGL_NONE};
    static const EGLint bad_profile_bits[] = {EGL_CONTEXT_MAJOR_VERSION,
                                              3,
                                              EGL_CONTEXT_MINOR_VERSION,
                                              3,
                                              EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                                              0x40,
                                              EGL_NONE};
    static const EGLint debug_flags[] = {EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR,
                                         EGL_NONE};
    static const EGLint debug_no_error[] = {EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR,
                                            EGL_CONTEXT_OPENGL_NO_ERROR_KHR, EGL_TRUE, EGL_NONE};
    static const EGLint forward_flags[] = {EGL_CONTEXT_MAJOR_VERSION,
                                           3,
                                           EGL_CONTEXT_MINOR_VERSION,
                                           3,
                                           EGL_CONTEXT_FLAGS_KHR,
                                           EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
                                           EGL_NONE};
    static const EGLint robust_flags[] = {EGL_CONTEXT_FLAGS_KHR,
                                          EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR, EGL_NONE};
    static const EGLint bad_flags[] = {EGL_CONTEXT_FLAGS_KHR, 0x40, EGL_NONE};
    static const EGLint reset_default[] = {EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR,
                                           EGL_NO_RESET_NOTIFICATION_KHR, EGL_NONE};
    static const EGLint reset_bad[] = {EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, 0x1234,
                                       EGL_NONE};
    static const EGLint no_error[] = {EGL_CONTEXT_OPENGL_NO_ERROR_KHR, EGL_TRUE, EGL_NONE};
    static const EGLint bogus_attr[] = {0x12345678, 0, EGL_NONE};
    EGLDisplay d;
    EGLContext c, c2;
    EGLint value = 0;

    d = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL);
    CHECK(d != EGL_NO_DISPLAY);
    CHECK(eglInitialize(d, NULL, NULL));
    CHECK(eglBindAPI(EGL_OPENGL_API));

    c = create(d, EGL_NO_CONTEXT, compat33, EGL_SUCCESS);
    CHECK(c != EGL_NO_CONTEXT);
    CHECK(eglQueryContext(d, c, EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, &value));
    CHECK(value == EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR);
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, c));
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &value);
    CHECK(value == (EGLint)GL_CONTEXT_COMPATIBILITY_PROFILE_BIT && glGetError() == GL_NO_ERROR);
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    CHECK(eglQueryContext(d, c, EGL_CONTEXT_FLAGS_KHR, &value));
    CHECK(value == 0);
    CHECK(eglDestroyContext(d, c));
    c = create(d, EGL_NO_CONTEXT, compat32, EGL_SUCCESS);
    CHECK(c != EGL_NO_CONTEXT);
    CHECK(eglDestroyContext(d, c));

    c = create(d, EGL_NO_CONTEXT, default33, EGL_SUCCESS);
    CHECK(c != EGL_NO_CONTEXT);
    CHECK(eglQueryContext(d, c, EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, &value));
    CHECK(value == EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR);
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, c));
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &value);
    CHECK(value == (EGLint)GL_CONTEXT_CORE_PROFILE_BIT && glGetError() == GL_NO_ERROR);
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    CHECK(eglDestroyContext(d, c));
    c = create(d, EGL_NO_CONTEXT, core33, EGL_SUCCESS);
    CHECK(c != EGL_NO_CONTEXT);
    CHECK(eglQueryContext(d, c, EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, &value));
    CHECK(value == EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR);
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, c));
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &value);
    CHECK(value == (EGLint)GL_CONTEXT_CORE_PROFILE_BIT && glGetError() == GL_NO_ERROR);
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    CHECK(eglDestroyContext(d, c));
    CHECK(create(d, EGL_NO_CONTEXT, both33, EGL_BAD_MATCH) == EGL_NO_CONTEXT);
    {
        static const EGLint zero33[] = {EGL_CONTEXT_MAJOR_VERSION,
                                        3,
                                        EGL_CONTEXT_MINOR_VERSION,
                                        3,
                                        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                                        0,
                                        EGL_NONE};
        static const EGLint version31[] = {EGL_CONTEXT_MAJOR_VERSION,
                                           3,
                                           EGL_CONTEXT_MINOR_VERSION,
                                           1,
                                           EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                                           EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR,
                                           EGL_NONE};
        static const EGLint version30[] = {EGL_CONTEXT_MAJOR_VERSION,
                                           3,
                                           EGL_CONTEXT_MINOR_VERSION,
                                           0,
                                           EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                                           EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
                                           EGL_NONE};
        static const EGLint default31[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION,
                                           1, EGL_NONE};
        static const EGLint ignored31[] = {EGL_CONTEXT_MAJOR_VERSION,
                                           3,
                                           EGL_CONTEXT_MINOR_VERSION,
                                           1,
                                           EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                                           0x40,
                                           EGL_NONE};
        CHECK(create(d, EGL_NO_CONTEXT, zero33, EGL_BAD_MATCH) == EGL_NO_CONTEXT);
        c = create(d, EGL_NO_CONTEXT, version31, EGL_SUCCESS);
        CHECK(c != EGL_NO_CONTEXT);
        CHECK(eglQueryContext(d, c, EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, &value));
        CHECK(value == EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR);
        CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, c));
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &value);
        CHECK(value == (EGLint)GL_CONTEXT_CORE_PROFILE_BIT && glGetError() == GL_NO_ERROR);
        CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        CHECK(eglDestroyContext(d, c));
        c = create(d, EGL_NO_CONTEXT, default31, EGL_SUCCESS);
        CHECK(c != EGL_NO_CONTEXT);
        CHECK(eglDestroyContext(d, c));
        c = create(d, EGL_NO_CONTEXT, ignored31, EGL_SUCCESS);
        CHECK(c != EGL_NO_CONTEXT);
        CHECK(eglDestroyContext(d, c));
        c = create(d, EGL_NO_CONTEXT, version30, EGL_SUCCESS);
        CHECK(c != EGL_NO_CONTEXT);
        CHECK(eglQueryContext(d, c, EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, &value));
        CHECK(value == 0);
        CHECK(eglDestroyContext(d, c));
    }
    CHECK(create(d, EGL_NO_CONTEXT, bad_profile_bits, EGL_BAD_ATTRIBUTE) == EGL_NO_CONTEXT);
    c = create(d, EGL_NO_CONTEXT, debug_flags, EGL_SUCCESS);
    CHECK(c != EGL_NO_CONTEXT);
    CHECK(eglQueryContext(d, c, EGL_CONTEXT_FLAGS_KHR, &value));
    CHECK(value == EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR);
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, c));
    glGetIntegerv(GL_CONTEXT_FLAGS, &value);
    CHECK(value == (EGLint)GL_CONTEXT_FLAG_DEBUG_BIT);
    CHECK(glIsEnabled(GL_DEBUG_OUTPUT));
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    CHECK(eglDestroyContext(d, c));
    CHECK(create(d, EGL_NO_CONTEXT, debug_no_error, EGL_BAD_MATCH) == EGL_NO_CONTEXT);
    CHECK(create(d, EGL_NO_CONTEXT, forward_flags, EGL_BAD_MATCH) == EGL_NO_CONTEXT);
    CHECK(create(d, EGL_NO_CONTEXT, robust_flags, EGL_BAD_MATCH) == EGL_NO_CONTEXT);
    CHECK(create(d, EGL_NO_CONTEXT, bad_flags, EGL_BAD_ATTRIBUTE) == EGL_NO_CONTEXT);
    CHECK(create(d, EGL_NO_CONTEXT, reset_default, EGL_BAD_MATCH) == EGL_NO_CONTEXT);
    CHECK(create(d, EGL_NO_CONTEXT, reset_bad, EGL_BAD_ATTRIBUTE) == EGL_NO_CONTEXT);
    CHECK(create(d, EGL_NO_CONTEXT, bogus_attr, EGL_BAD_ATTRIBUTE) == EGL_NO_CONTEXT);

    c = create(d, EGL_NO_CONTEXT, no_error, EGL_SUCCESS);
    CHECK(c != EGL_NO_CONTEXT);
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, c));
    glGetIntegerv(GL_CONTEXT_FLAGS, &value);
    CHECK(value == (EGLint)GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR);
    CHECK(eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    CHECK(create(d, c, NULL, EGL_BAD_MATCH) == EGL_NO_CONTEXT);
    c2 = create(d, c, no_error, EGL_SUCCESS);
    CHECK(c2 != EGL_NO_CONTEXT);
    CHECK(eglDestroyContext(d, c2));
    CHECK(eglDestroyContext(d, c));

    c = create(d, EGL_NO_CONTEXT, NULL, EGL_SUCCESS);
    CHECK(c != EGL_NO_CONTEXT);
    CHECK(create(d, c, no_error, EGL_BAD_MATCH) == EGL_NO_CONTEXT);
    CHECK(eglDestroyContext(d, c));
    CHECK(eglTerminate(d));
    return 0;
}

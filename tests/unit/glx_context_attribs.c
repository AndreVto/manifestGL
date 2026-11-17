#include "mgl/glx.h"
#include "mgl/gl.h"
#include "abi/proc_address.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(int n, const char *message) {
    fprintf(stderr, "GLX context-attrib failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    static const int attrs_33[] = {GLX_CONTEXT_MAJOR_VERSION_ARB, 3, GLX_CONTEXT_MINOR_VERSION_ARB,
                                   3, 0};
    static const int attrs_34[] = {GLX_CONTEXT_MAJOR_VERSION_ARB, 3, GLX_CONTEXT_MINOR_VERSION_ARB,
                                   4, 0};
    static const int attrs_compat[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                       3,
                                       GLX_CONTEXT_MINOR_VERSION_ARB,
                                       3,
                                       GLX_CONTEXT_PROFILE_MASK_ARB,
                                       GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                                       0};
    static const int attrs_compat32[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                         3,
                                         GLX_CONTEXT_MINOR_VERSION_ARB,
                                         2,
                                         GLX_CONTEXT_PROFILE_MASK_ARB,
                                         GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                                         0};
    static const int attrs_core[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                     3,
                                     GLX_CONTEXT_MINOR_VERSION_ARB,
                                     3,
                                     GLX_CONTEXT_PROFILE_MASK_ARB,
                                     GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
                                     0};
    static const int attrs_both[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                     3,
                                     GLX_CONTEXT_MINOR_VERSION_ARB,
                                     3,
                                     GLX_CONTEXT_PROFILE_MASK_ARB,
                                     GLX_CONTEXT_CORE_PROFILE_BIT_ARB |
                                         GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                                     0};
    static const int attrs_invalid_profile[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                                3,
                                                GLX_CONTEXT_MINOR_VERSION_ARB,
                                                3,
                                                GLX_CONTEXT_PROFILE_MASK_ARB,
                                                0x40,
                                                0};
    static const int attrs_debug[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                      3,
                                      GLX_CONTEXT_MINOR_VERSION_ARB,
                                      3,
                                      GLX_CONTEXT_PROFILE_MASK_ARB,
                                      GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                                      GLX_CONTEXT_FLAGS_ARB,
                                      GLX_CONTEXT_DEBUG_BIT_ARB,
                                      0};
    GLXContext ctx;
    GLXFBConfig *configs;
    int count = -1;

    if (glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_34) != NULL)
        return fail(1, "unsupported 3.4 context admitted");
    if (glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_invalid_profile) != NULL)
        return fail(2, "unknown profile bits admitted");
    {
        GLint profile = 0;
        ctx = glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_core);
        if (ctx == NULL)
            return fail(3, "core profile request rejected");
        if (!glXMakeContextCurrent(NULL, 0, 0, ctx))
            return fail(24, "core context make-current");
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
        if (profile != (GLint)GL_CONTEXT_CORE_PROFILE_BIT || glGetError() != GL_NO_ERROR)
            return fail(25, "core context profile reporting");
        if (!glXMakeContextCurrent(NULL, 0, 0, NULL))
            return fail(26, "core context release");
        glXDestroyContext(NULL, ctx);
        ctx = glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_33);
        if (ctx == NULL)
            return fail(12, "default core profile request rejected");
        if (!glXMakeContextCurrent(NULL, 0, 0, ctx))
            return fail(27, "default core context make-current");
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
        if (profile != (GLint)GL_CONTEXT_CORE_PROFILE_BIT || glGetError() != GL_NO_ERROR)
            return fail(28, "default core context profile reporting");
        if (!glXMakeContextCurrent(NULL, 0, 0, NULL))
            return fail(29, "default core context release");
        glXDestroyContext(NULL, ctx);
    }
    if (glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_both) != NULL)
        return fail(13, "combined profile mask admitted");
    {
        static const int attrs_zero[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                         3,
                                         GLX_CONTEXT_MINOR_VERSION_ARB,
                                         3,
                                         GLX_CONTEXT_PROFILE_MASK_ARB,
                                         0,
                                         0};
        static const int attrs_unknown[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                            3,
                                            GLX_CONTEXT_MINOR_VERSION_ARB,
                                            3,
                                            0x7ff0,
                                            1,
                                            GLX_CONTEXT_PROFILE_MASK_ARB,
                                            GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                                            0};
        static const int attrs_render_type[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                                3,
                                                GLX_CONTEXT_MINOR_VERSION_ARB,
                                                3,
                                                GLX_CONTEXT_PROFILE_MASK_ARB,
                                                GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                                                GLX_RENDER_TYPE,
                                                GLX_RGBA_TYPE,
                                                0};
        static const int attrs_bad_render_type[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                                    3,
                                                    GLX_CONTEXT_MINOR_VERSION_ARB,
                                                    3,
                                                    GLX_CONTEXT_PROFILE_MASK_ARB,
                                                    GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                                                    GLX_RENDER_TYPE,
                                                    GLX_COLOR_INDEX_BIT,
                                                    0};
        static const int attrs_version31[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                              3,
                                              GLX_CONTEXT_MINOR_VERSION_ARB,
                                              1,
                                              GLX_CONTEXT_PROFILE_MASK_ARB,
                                              GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                                              0};
        if (glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_zero) != NULL)
            return fail(14, "zero profile mask admitted");
        if (glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_unknown) != NULL)
            return fail(15, "unknown attribute admitted");
        if (glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_bad_render_type) != NULL)
            return fail(16, "unsupported render type admitted");
        ctx = glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_version31);
        if (ctx == NULL)
            return fail(17, "OpenGL 3.1 request rejected");
        if (!glXMakeContextCurrent(NULL, 0, 0, ctx))
            return fail(30, "OpenGL 3.1 make-current");
        {
            GLint profile = 0;
            glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
            if (profile != (GLint)GL_CONTEXT_CORE_PROFILE_BIT || glGetError() != GL_NO_ERROR)
                return fail(31, "OpenGL 3.1 returned profile");
        }
        if (!glXMakeContextCurrent(NULL, 0, 0, NULL))
            return fail(32, "OpenGL 3.1 release");
        glXDestroyContext(NULL, ctx);
        {
            static const int attrs_default31[] = {GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
                                                  GLX_CONTEXT_MINOR_VERSION_ARB, 1, 0};
            static const int attrs_ignored31[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                                  3,
                                                  GLX_CONTEXT_MINOR_VERSION_ARB,
                                                  1,
                                                  GLX_CONTEXT_PROFILE_MASK_ARB,
                                                  0x40,
                                                  0};
            ctx = glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_default31);
            if (ctx == NULL)
                return fail(33, "default OpenGL 3.1 request rejected");
            glXDestroyContext(NULL, ctx);
            ctx = glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_ignored31);
            if (ctx == NULL)
                return fail(34, "ignored OpenGL 3.1 profile mask rejected");
            glXDestroyContext(NULL, ctx);
        }
        ctx = glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_render_type);
        if (ctx == NULL)
            return fail(18, "valid explicit RGBA render type rejected");
        glXDestroyContext(NULL, ctx);
    }
    {
        GLint profile = 0;
        ctx = glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_compat);
        if (ctx == NULL)
            return fail(19, "compatibility request rejected");
        if (!glXMakeContextCurrent(NULL, 0, 0, ctx))
            return fail(20, "compatibility context make-current");
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
        if (profile != (GLint)GL_CONTEXT_COMPATIBILITY_PROFILE_BIT || glGetError() != GL_NO_ERROR)
            return fail(21, "compatibility context profile reporting");
        if (!glXMakeContextCurrent(NULL, 0, 0, NULL))
            return fail(22, "compatibility context release");
        glXDestroyContext(NULL, ctx);
    }

    ctx = glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_compat32);
    if (ctx == NULL)
        return fail(23, "OpenGL 3.2 compatibility context rejected");
    glXDestroyContext(NULL, ctx);

    ctx = glXCreateContextAttribsARB(NULL, NULL, NULL, True, attrs_debug);
    if (ctx == NULL)
        return fail(4, "debug compatibility context rejected");
    if (!glXMakeContextCurrent(NULL, 0, 0, ctx))
        return fail(5, "debug context make-current");
    {
        GLint flags = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (flags != (GLint)GL_CONTEXT_FLAG_DEBUG_BIT || !glIsEnabled(GL_DEBUG_OUTPUT))
            return fail(6, "debug context state not represented");
    }
    if (!glXMakeContextCurrent(NULL, 0, 0, NULL))
        return fail(7, "debug context release");
    glXDestroyContext(NULL, ctx);

    ctx = glXCreateContextAttribs(NULL, NULL, NULL, True, attrs_compat);
    if (ctx == NULL)
        return fail(8, "unsuffixed compatibility alias rejected");
    glXDestroyContext(NULL, ctx);

    configs = glXChooseFBConfigSGIX(NULL, 7, NULL, &count);
    if (configs == NULL || count <= 0 || configs[0] == NULL)
        return fail(9, "SGIX FBConfig alias result");
    free(configs);

    if (mgl_proc_address("glXCreateContextAttribs") == NULL ||
        mgl_proc_address("glXChooseFBConfigSGIX") == NULL)
        return fail(10, "alias proc-address coverage");

    if (strstr(glXGetClientString(NULL, GLX_EXTENSIONS), "GLX_ARB_create_context_profile") == NULL)
        return fail(11, "profile extension not advertised");

    puts("GLX context attribute admission and aliases passed");
    return 0;
}

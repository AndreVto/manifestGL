#include "mgl/glx.h"
#include "abi/proc_address.h"
#include "glx/glx_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(int n, const char *message) {
    fprintf(stderr, "GLX context query failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    XVisualInfo visual;
    GLXContext ctx;
    GLXContext foreign = (GLXContext)(uintptr_t)0x56780u;
    Display *other_display = (Display *)(uintptr_t)0x43210u;
    GLXFBConfig *configs;
    int n = -1;
    int value = -123;

    /* Query a real registry-owned context instead
     * of a fabricated structure. Public APIs must reject borrowed pointers. */
    memset(&visual, 0, sizeof(visual));
    visual.screen = 4;
    ctx = glXCreateContext(NULL, &visual, NULL, True);
    if (ctx == NULL)
        return fail(40, "create registered context for metadata queries");

    if (glXQueryContext(NULL, ctx, GLX_FBCONFIG_ID, &value) != 0 || value != 1)
        return fail(1, "FBCONFIG_ID");
    value = -123;
    if (glXQueryContext(NULL, ctx, GLX_RENDER_TYPE, &value) != 0 || value != GLX_RGBA_TYPE)
        return fail(2, "RENDER_TYPE");
    value = -123;
    if (glXQueryContext(NULL, ctx, GLX_SCREEN, &value) != 0 || value != 4)
        return fail(3, "SCREEN");

    value = 77;
    if (glXQueryContext(NULL, ctx, 0x7fffffff, &value) != GLX_BAD_ATTRIBUTE || value != 77)
        return fail(4, "bad attribute preservation");
    if (glXQueryContext(NULL, NULL, GLX_SCREEN, &value) != GLX_BAD_CONTEXT)
        return fail(5, "null context");
    value = 77;
    if (glXQueryContext(NULL, foreign, GLX_SCREEN, &value) != GLX_BAD_CONTEXT || value != 77)
        return fail(41, "unknown pointer is not a context and preserves output");
    if (glXQueryContext(other_display, ctx, GLX_SCREEN, &value) != GLX_BAD_CONTEXT || value != 77)
        return fail(42, "foreign display query rejected without changing output");

    configs = glXGetFBConfigs(NULL, 6, &n);
    if (configs == NULL || n != 1)
        return fail(6, "GetFBConfigs singleton");
    if (configs[0] == NULL || configs[0]->screen != 6)
        return fail(7, "GetFBConfigs screen");

#define CHECK_FB_ATTR(attr, expected, code)                                                        \
    do {                                                                                           \
        value = -123;                                                                              \
        if (glXGetFBConfigAttrib(NULL, configs[0], (attr), &value) != 0 || value != (expected))    \
            return fail((code), #attr);                                                            \
    } while (0)
    CHECK_FB_ATTR(GLX_BUFFER_SIZE, 32, 8);
    CHECK_FB_ATTR(GLX_LEVEL, 0, 9);
    CHECK_FB_ATTR(GLX_DOUBLEBUFFER, True, 10);
    CHECK_FB_ATTR(GLX_STEREO, False, 11);
    CHECK_FB_ATTR(GLX_AUX_BUFFERS, 0, 12);
    CHECK_FB_ATTR(GLX_ACCUM_RED_SIZE, 0, 13);
    CHECK_FB_ATTR(GLX_SAMPLE_BUFFERS, 0, 14);
    CHECK_FB_ATTR(GLX_SAMPLES, 0, 15);
    CHECK_FB_ATTR(GLX_CONFIG_CAVEAT, GLX_NON_CONFORMANT_CONFIG, 16);
    CHECK_FB_ATTR(GLX_TRANSPARENT_TYPE, GLX_NONE, 17);
    CHECK_FB_ATTR(GLX_MAX_PBUFFER_WIDTH, 16384, 18);
    CHECK_FB_ATTR(GLX_MAX_PBUFFER_HEIGHT, 16384, 19);
    CHECK_FB_ATTR(GLX_MAX_PBUFFER_PIXELS, 16384 * 16384, 20);
#undef CHECK_FB_ATTR

    value = 77;
    if (glXGetFBConfigAttrib(NULL, configs[0], 0x20F0, &value) != GLX_BAD_ATTRIBUTE || value != 77)
        return fail(21, "invalid FBConfig attribute preservation");
    free(configs);

    configs = glXGetFBConfigs(NULL, 2, NULL);
    if (configs == NULL)
        return fail(22, "GetFBConfigs null count");
    free(configs);
    configs = glXGetFBConfigs(NULL, 2, &n);
    if (configs == NULL)
        return fail(23, "GetFBConfigs repeat");
    free(configs);

    if (mgl_proc_address("glXGetFBConfigs") == NULL ||
        mgl_proc_address("glXQueryContext") == NULL ||
        mgl_proc_address("glXGetFBConfigAttrib") == NULL ||
        mgl_proc_address("glXChooseFBConfig") == NULL)
        return fail(24, "proc addresses");

    {
        const int min_red[] = {GLX_RED_SIZE, 8, 0};
        const int too_much_red[] = {GLX_RED_SIZE, 9, 0};
        const int rgba[] = {GLX_RENDER_TYPE, GLX_RGBA_BIT, 0};
        const int color_index[] = {GLX_RENDER_TYPE, GLX_COLOR_INDEX_BIT, 0};
        const int pbuffer[] = {GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT, 0};
        const int unsupported_drawable[] = {GLX_DRAWABLE_TYPE, 0x8, 0};
        const int stereo[] = {GLX_STEREO, True, 0};
        const int config_id_override[] = {GLX_FBCONFIG_ID, 1, GLX_STEREO, True, 0};
        const int missing_config_id[] = {GLX_FBCONFIG_ID, 2, 0};
        const int ignored_limit[] = {GLX_MAX_PBUFFER_WIDTH, 1, 0};
        const int nonconformant[] = {GLX_CONFIG_CAVEAT, GLX_NON_CONFORMANT_CONFIG, 0};
        const int conformant[] = {GLX_CONFIG_CAVEAT, GLX_NONE, 0};
        const int invalid_attr[] = {0x7ffffffe, 1, 0};
        const int level_dont_care[] = {GLX_LEVEL, (int)GLX_DONT_CARE, 0};

#define CHECK_CHOOSE_MATCH(attrs, code)                                                            \
    do {                                                                                           \
        GLXFBConfig *chosen;                                                                       \
        n = -1;                                                                                    \
        chosen = glXChooseFBConfig(NULL, 0, (attrs), &n);                                          \
        if (chosen == NULL || n != 1)                                                              \
            return fail((code), #attrs " should match");                                           \
        free(chosen);                                                                              \
    } while (0)
#define CHECK_CHOOSE_NONE(attrs, code)                                                             \
    do {                                                                                           \
        GLXFBConfig *chosen;                                                                       \
        n = -1;                                                                                    \
        chosen = glXChooseFBConfig(NULL, 0, (attrs), &n);                                          \
        if (chosen != NULL || n != 0)                                                              \
            return fail((code), #attrs " should not match");                                       \
    } while (0)
        CHECK_CHOOSE_MATCH(NULL, 25);
        CHECK_CHOOSE_MATCH(min_red, 26);
        CHECK_CHOOSE_NONE(too_much_red, 27);
        CHECK_CHOOSE_MATCH(rgba, 28);
        CHECK_CHOOSE_NONE(color_index, 29);
        CHECK_CHOOSE_MATCH(pbuffer, 30);
        CHECK_CHOOSE_NONE(unsupported_drawable, 31);
        CHECK_CHOOSE_NONE(stereo, 32);
        CHECK_CHOOSE_MATCH(config_id_override, 33);
        CHECK_CHOOSE_NONE(missing_config_id, 34);
        CHECK_CHOOSE_MATCH(ignored_limit, 35);
        CHECK_CHOOSE_MATCH(nonconformant, 36);
        CHECK_CHOOSE_NONE(conformant, 37);
        CHECK_CHOOSE_NONE(invalid_attr, 38);
        CHECK_CHOOSE_NONE(level_dont_care, 39);
#undef CHECK_CHOOSE_MATCH
#undef CHECK_CHOOSE_NONE
    }

    glXDestroyContext(NULL, ctx);
    value = 77;
    if (glXQueryContext(NULL, ctx, GLX_SCREEN, &value) != GLX_BAD_CONTEXT || value != 77)
        return fail(43, "destroyed handle query rejected without dereference");

    puts("GLX context query semantics passed");
    return 0;
}

#include "mgl/glx.h"
#include "mgl/gl.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int fail(int n, const char *message) {
    fprintf(stderr, "GLX query surface failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    Display *display = (Display *)(uintptr_t)0x12340u;
    const char *vendor, *version, *extensions;
    int major = -1, minor = -1;
    GLXContext ctx;
    GLXPbuffer pbuffer;
    unsigned int result;
    const int attrs[] = {GLX_PBUFFER_WIDTH,
                         7,
                         GLX_PBUFFER_HEIGHT,
                         5,
                         GLX_PRESERVED_CONTENTS,
                         True,
                         GLX_LARGEST_PBUFFER,
                         True,
                         None};
    const int invalid_attrs[] = {GLX_PRESERVED_CONTENTS, 2, None};
    const int unpreserved_attrs[] = {GLX_PRESERVED_CONTENTS, False, None};

    pbuffer = glXCreatePbuffer(NULL, NULL, attrs);
    if (pbuffer == 0)
        return fail(20, "pbuffer creation");
    result = 0;
    glXQueryDrawable(NULL, pbuffer, GLX_WIDTH, &result);
    if (result != 7u)
        return fail(21, "pbuffer width");
    glXQueryDrawable(NULL, pbuffer, GLX_HEIGHT, &result);
    if (result != 5u)
        return fail(22, "pbuffer height");
    glXQueryDrawable(NULL, pbuffer, GLX_FBCONFIG_ID, &result);
    if (result == 0u)
        return fail(23, "pbuffer FBConfig ID");
    glXQueryDrawable(NULL, pbuffer, GLX_PRESERVED_CONTENTS, &result);
    if (result != True)
        return fail(24, "pbuffer preserved contents");
    glXQueryDrawable(NULL, pbuffer, GLX_LARGEST_PBUFFER, &result);
    if (result != True)
        return fail(25, "pbuffer largest attribute");
    glXDestroyPbuffer(NULL, pbuffer);
    pbuffer = glXCreatePbuffer(NULL, NULL, NULL);
    if (pbuffer == 0)
        return fail(27, "default pbuffer creation");
    glXQueryDrawable(NULL, pbuffer, GLX_PRESERVED_CONTENTS, &result);
    if (result != True)
        return fail(28, "default preserved contents");
    glXQueryDrawable(NULL, pbuffer, GLX_LARGEST_PBUFFER, &result);
    if (result != False)
        return fail(29, "default largest attribute");
    glXDestroyPbuffer(NULL, pbuffer);
    pbuffer = glXCreatePbuffer(NULL, NULL, unpreserved_attrs);
    if (pbuffer == 0)
        return fail(30, "unpreserved pbuffer creation");
    glXQueryDrawable(NULL, pbuffer, GLX_PRESERVED_CONTENTS, &result);
    if (result != False)
        return fail(31, "unpreserved pbuffer attribute");
    glXDestroyPbuffer(NULL, pbuffer);
    if (glXCreatePbuffer(NULL, NULL, invalid_attrs) != 0)
        return fail(26, "invalid pbuffer attribute");
    if (!glXQueryVersion(display, &major, &minor) || major <= 0 || minor < 0)
        return fail(1, "reported GLX version");

    vendor = glXGetClientString(display, GLX_VENDOR);
    version = glXGetClientString(display, GLX_VERSION);
    extensions = glXGetClientString(display, GLX_EXTENSIONS);
    if (vendor == NULL || *vendor == '\0' || version == NULL || *version == '\0' ||
        extensions == NULL || strstr(extensions, "GLX_ARB_create_context_profile") == NULL)
        return fail(2, "client query strings");

    if (glXGetClientString(display, -1) != NULL)
        return fail(3, "unsupported client string selector");

    if (glXIsDirect(display, NULL) != False)
        return fail(5, "null context must not be reported direct");
    ctx = glXCreateNewContext(display, NULL, GLX_RGBA_TYPE, NULL, True);
    if (ctx == NULL)
        return fail(6, "context creation");
    if (glXIsDirect(display, ctx) != True)
        return fail(7, "owned context is direct");
    if (glXIsDirect((Display *)(uintptr_t)0x43210u, ctx) != False)
        return fail(9, "context queried on a different display");
    if (glXIsDirect(display, (GLXContext)(uintptr_t)0x56780u) != False)
        return fail(10, "unknown non-null handle must not be dereferenced or accepted");
    if (!glXMakeContextCurrent(display, 0, 0, ctx))
        return fail(11, "bind for deferred-deletion query");
    glXDestroyContext(display, ctx);
    if (glXIsDirect(display, ctx) != False)
        return fail(12, "delete-pending context is no longer a valid query handle");
    if (!glXMakeContextCurrent(display, 0, 0, NULL))
        return fail(13, "release delete-pending context");
    if (glXIsDirect(display, ctx) != False)
        return fail(14, "destroyed context handle must not be dereferenced or accepted");

    if (glXGetProcAddress((const GLubyte *)"glXQueryVersion") == NULL ||
        glXGetProcAddress((const GLubyte *)"glXGetClientString") == NULL ||
        glXGetProcAddress((const GLubyte *)"glXQueryServerString") == NULL ||
        glXGetProcAddress((const GLubyte *)"glXQueryExtensionsString") == NULL ||
        glXGetProcAddress((const GLubyte *)"glXIsDirect") == NULL)
        return fail(8, "GLX query symbols in proc table");

    puts("GLX query surface focused semantics passed");
    return 0;
}

#include "mgl/gl.h"
#include "mgl/glx.h"
#include <stdio.h>

int main(void) {
    int major = 0, minor = 0;
    if (!glXQueryVersion(NULL, &major, &minor) || major <= 0 || minor < 0) {
        return 1;
    }
    if (glXGetProcAddress((const unsigned char *)"glClear") == NULL) {
        return 2;
    }
    if (glXGetProcAddress((const unsigned char *)"definitely_not_mgl") != NULL) {
        return 3;
    }
    if (glXGetClientString(NULL, GLX_VENDOR) == NULL ||
        glXGetClientString(NULL, GLX_VERSION) == NULL)
        return 4;
    {
        const int attrs[] = {GLX_PBUFFER_WIDTH, 37, GLX_PBUFFER_HEIGHT, 23, 0};
        GLXPbuffer pbuffer = glXCreatePbuffer(NULL, NULL, attrs);
        unsigned int width = 0, height = 0;
        if (pbuffer == 0)
            return 6;
        glXQueryDrawable(NULL, pbuffer, GLX_WIDTH, &width);
        glXQueryDrawable(NULL, pbuffer, GLX_HEIGHT, &height);
        if (width != 37u || height != 23u)
            return 7;
        glXDestroyPbuffer(NULL, pbuffer);
    }
    puts("mGL bootstrap smoke: PASS");
    return 0;
}

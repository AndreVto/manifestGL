#include "mgl/glx.h"
#include "mgl/gl.h"
#include "abi/proc_address.h"

#include <stdio.h>

static int check(int condition, const char *message) {
    if (!condition)
        fprintf(stderr, "GLX copy: %s\n", message);
    return condition;
}

int main(void) {
    GLXContext src = glXCreateNewContext(NULL, NULL, GLX_RGBA_TYPE, NULL, True);
    GLXContext dst = glXCreateNewContext(NULL, NULL, GLX_RGBA_TYPE, NULL, True);
    GLint viewport[4] = {0};
    GLint depth_func = 0;
    GLint unpack_alignment = 0;
    if (!check(src != NULL && dst != NULL, "context creation"))
        return 1;
    if (!check(mgl_proc_address("glXCopyContext") != NULL, "procedure address"))
        return 2;
    if (!check(glXMakeContextCurrent(NULL, 0, 0, src), "source current"))
        return 3;
    glViewport(5, 7, 61, 43);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (!check(glXMakeContextCurrent(NULL, 0, 0, dst), "destination current"))
        return 4;
    glViewport(0, 0, 10, 10);
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if (!check(glXMakeContextCurrent(NULL, 0, 0, NULL), "release destination"))
        return 5;
    glXCopyContext(NULL, src, dst, GL_VIEWPORT_BIT | GL_DEPTH_BUFFER_BIT | GL_PIXEL_MODE_BIT);
    if (!check(glXMakeContextCurrent(NULL, 0, 0, dst), "restore destination"))
        return 6;
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
    if (!check(viewport[0] == 5 && viewport[1] == 7 && viewport[2] == 61 && viewport[3] == 43,
               "viewport copy"))
        return 7;
    if (!check(glIsEnabled(GL_DEPTH_TEST) == GL_TRUE && depth_func == GL_GREATER,
               "depth state copy"))
        return 8;
    if (!check(unpack_alignment == 4, "client state preserved"))
        return 9;
    if (!check(glGetError() == GL_NO_ERROR, "GL error"))
        return 10;
    if (!check(glXMakeContextCurrent(NULL, 0, 0, NULL), "release"))
        return 11;
    glXDestroyContext(NULL, dst);
    glXDestroyContext(NULL, src);
    return 0;
}

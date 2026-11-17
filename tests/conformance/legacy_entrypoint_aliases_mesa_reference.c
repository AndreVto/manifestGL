#include "mgl/glx.h"
#include "mgl/gl.h"
#define MGL_CORE_ENTRYPOINT_REFERENCE 1
#include "legacy_entrypoint_aliases_cases.h"
#include <stdio.h>
#include <string.h>

static GLuint legacy_atomic_program(void) {
    const char *source = "#version 420\nlayout(binding=3, offset=0) uniform atomic_uint ac;\n"
                         "void main() { uint value = atomicCounterIncrement(ac); "
                         "gl_Position = vec4(float(value), 0.0, 0.0, 1.0); }\n";
    GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint program;
    GLint ok = 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glDeleteShader(shader);
        return 0;
    }
    program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    glDeleteShader(shader);
    if (!ok) {
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

int main(void) {
    Display *display = XOpenDisplay(NULL);
    XVisualInfo *visual;
    GLXContext context;
    Colormap colormap;
    XSetWindowAttributes attributes;
    Window window;
    int screen;
    int visual_attributes[] = {GLX_RGBA, None};
    const GLubyte *vendor;

    if (!display)
        return 2;
    screen = DefaultScreen(display);
    visual = glXChooseVisual(display, screen, visual_attributes);
    if (!visual)
        return 3;
    colormap = XCreateColormap(display, RootWindow(display, screen), visual->visual, AllocNone);
    attributes.colormap = colormap;
    attributes.border_pixel = 0;
    window = XCreateWindow(display, RootWindow(display, screen), 0, 0, 64, 64, 0, visual->depth,
                           InputOutput, visual->visual, CWColormap | CWBorderPixel, &attributes);
    context = glXCreateContext(display, visual, NULL, True);
    if (!context || !glXMakeCurrent(display, window, context))
        return 4;
    vendor = glGetString(GL_VENDOR);
    if (!vendor || strstr((const char *)glGetString(GL_VERSION), "ManifestGL"))
        return 5;
    fprintf(stderr, "Mesa reference: %s\n", vendor);

    legacy_entrypoint_aliases_trace();

    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, context);
    XDestroyWindow(display, window);
    XFreeColormap(display, colormap);
    XFree(visual);
    XCloseDisplay(display);
    return 0;
}

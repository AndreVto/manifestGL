#include "core/context.h"

#include <stdio.h>
#include <string.h>

static const char *extension_name;

static MGL_EGLBoolean fake_make_current(MGL_EGLDisplay display, MGL_EGLSurface draw,
                                        MGL_EGLSurface read, MGL_EGLContext context) {
    (void)display;
    (void)draw;
    (void)read;
    (void)context;
    return MGL_EGL_TRUE;
}

static void fake_get_integer(GLenum name, GLint *value) {
    if (name == GL_MAJOR_VERSION)
        *value = 3;
    else if (name == GL_MINOR_VERSION)
        *value = 2;
    else if (name == GL_NUM_EXTENSIONS)
        *value = extension_name != NULL ? 1 : 0;
    else if (name == GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS || name == GL_MAX_TEXTURE_IMAGE_UNITS ||
             name == GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS)
        *value = 16;
    else
        *value = 0;
}

static const GLubyte *fake_get_string_i(GLenum name, GLuint index) {
    if (name != GL_EXTENSIONS || index != 0u)
        return NULL;
    return (const GLubyte *)extension_name;
}

static void native_core(GLenum mode, GLsizei count, GLenum type, const void *indices,
                        GLint basevertex) {
    (void)mode;
    (void)count;
    (void)type;
    (void)indices;
    (void)basevertex;
}

static void native_oes(GLenum mode, GLsizei count, GLenum type, const void *indices,
                       GLint basevertex) {
    (void)mode;
    (void)count;
    (void)type;
    (void)indices;
    (void)basevertex;
}

static void native_ext(GLenum mode, GLsizei count, GLenum type, const void *indices,
                       GLint basevertex) {
    (void)mode;
    (void)count;
    (void)type;
    (void)indices;
    (void)basevertex;
}

static int check_case(const char *name, int core, int oes, int ext,
                      void (*expected)(GLenum, GLsizei, GLenum, const void *, GLint)) {
    struct mgl_native_dispatch native;
    struct mgl_context ctx;
    memset(&native, 0, sizeof(native));
    memset(&ctx, 0, sizeof(ctx));
    extension_name = name;
    native.eglMakeCurrent = fake_make_current;
    native.glesGetIntegerv = fake_get_integer;
    native.glesGetStringi = fake_get_string_i;
    native.glesDrawElementsBaseVertex = core ? native_core : NULL;
    native.glesDrawElementsBaseVertexOES = oes ? native_oes : NULL;
    native.glesDrawElementsBaseVertexEXT = ext ? native_ext : NULL;
    ctx.native_dispatch = &native;
    if (!mgl_context_make_current(&ctx))
        return 0;
    if (ctx.native_draw_elements_base_vertex != expected)
        return 0;
    return mgl_context_make_current(NULL);
}

int main(void) {
    if (!check_case("GL_OES_draw_elements_base_vertex", 0, 1, 0, native_oes) ||
        !check_case("GL_EXT_draw_elements_base_vertex", 0, 0, 1, native_ext) ||
        !check_case("GL_EXT_draw_elements_base_vertex", 0, 1, 1, native_ext) ||
        !check_case("GL_OES_draw_elements_base_vertex", 0, 1, 1, native_oes) ||
        !check_case("GL_OES_draw_elements_base_vertex", 1, 1, 1, native_core) ||
        !check_case("GL_UNRELATED_extension", 0, 1, 1, NULL) || !check_case(NULL, 0, 1, 1, NULL) ||
        !check_case("GL_OES_draw_elements_base_vertex", 0, 0, 1, NULL)) {
        fputs("basevertex extension selection failed\n", stderr);
        return 1;
    }
    return 0;
}

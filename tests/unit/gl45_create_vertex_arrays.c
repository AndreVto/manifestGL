#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static GLuint next_native;

static void gen_native(GLsizei count, GLuint *arrays) {
    GLsizei i;
    for (i = 0; i < count; ++i)
        arrays[i] = ++next_native;
}

static void bind_native(GLuint array) {
    (void)array;
}

static void delete_native(GLsizei count, const GLuint *arrays) {
    (void)count;
    (void)arrays;
}

static int fail(int code, const char *msg) {
    fprintf(stderr, "GL 4.5 CreateVertexArrays failure %d: %s\n", code, msg);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint reserved = 0u, created[2] = {0u, 0u};
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenVertexArrays = gen_native;
    native.glesBindVertexArray = bind_native;
    native.glesDeleteVertexArrays = delete_native;
    ctx.native_dispatch = &native;
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glGenVertexArrays(1, &reserved);
    if (glGetError() != GL_NO_ERROR || reserved == 0u || glIsVertexArray(reserved))
        return fail(1, "glGen must reserve without realizing");
    glCreateVertexArrays(2, created);
    if (glGetError() != GL_NO_ERROR || created[0] == 0u || created[1] == 0u ||
        created[0] == created[1] || !glIsVertexArray(created[0]) || !glIsVertexArray(created[1]))
        return fail(2, "created arrays must be live without binding");
    if (ctx.current_vao != &ctx.default_vao || mgl_vao_find(&ctx, created[0])->generic[0].size != 4)
        return fail(3, "creation must not bind or corrupt default state");
    glCreateVertexArrays(-1, created);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(4, "negative count");
    glBindVertexArray(created[0]);
    if (glGetError() != GL_NO_ERROR || ctx.current_vao->name != created[0])
        return fail(5, "created VAO bind");
    glDeleteVertexArrays(2, created);
    if (glGetError() != GL_NO_ERROR || ctx.current_vao != &ctx.default_vao ||
        glIsVertexArray(created[0]) || glIsVertexArray(created[1]))
        return fail(6, "VAO lifetime");
    if (glXGetProcAddress((const GLubyte *)"glCreateVertexArrays") == NULL)
        return fail(7, "proc address");
    glDeleteVertexArrays(1, &reserved);
    mgl_context_set_current_for_test(NULL);
    puts("mGL OpenGL 4.5 CreateVertexArrays: PASS");
    return 0;
}

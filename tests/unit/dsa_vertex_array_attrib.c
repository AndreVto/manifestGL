#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static GLint fake_max_attribs = 8;

static void fake_get_integerv(GLenum pname, GLint *value) {
    if (pname == GL_MAX_VERTEX_ATTRIBS && value != NULL)
        *value = fake_max_attribs;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "DSA vertex-array attrib failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_vertex_array *vao1;
    struct mgl_vertex_array *vao2;
    GLuint names[2] = {0u, 0u};

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetIntegerv = fake_get_integerv;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.next_vao_name = 1u;
    mgl_context_set_current_for_test(&ctx);

    glGenVertexArrays(2, names);
    if (names[0] == 0u || names[1] == 0u)
        return fail(2, "VAO allocation");
    glBindVertexArray(names[0]);
    vao1 = mgl_vao_find(&ctx, names[0]);
    vao2 = mgl_vao_find(&ctx, names[1]);
    if (vao1 == NULL || vao2 == NULL)
        return fail(3, "VAO lookup");
    /* glGenVertexArrays reserves names; make the second object a real named VAO for DSA validation.
     */
    glBindVertexArray(names[1]);
    glBindVertexArray(names[0]);

    glEnableVertexArrayAttrib(names[1], 3u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || vao2->generic[3].enabled != GL_TRUE ||
        vao1->generic[3].enabled != GL_FALSE || ctx.current_vao != vao1)
        return fail(4, "selector-free enable");

    glDisableVertexArrayAttribEXT(names[1], 3u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || vao2->generic[3].enabled != GL_FALSE ||
        ctx.current_vao != vao1)
        return fail(5, "EXT alias disable");

    glEnableVertexArrayAttribEXT(names[1], 2u);
    glDisableVertexArrayAttrib(names[1], 2u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || vao2->generic[2].enabled != GL_FALSE)
        return fail(6, "core/EXT alias equivalence");

    glEnableVertexArrayAttrib(0u, 1u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(7, "zero VAO rejected");
    glEnableVertexArrayAttrib(999u, 1u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(8, "unknown VAO rejected");
    glEnableVertexArrayAttrib(names[1], 8u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(9, "attribute limit validation");

    glDeleteVertexArrays(1, &names[1]);
    glEnableVertexArrayAttrib(names[1], 1u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(10, "deleted VAO rejected");

    if (glXGetProcAddress((const GLubyte *)"glEnableVertexArrayAttrib") == NULL ||
        glXGetProcAddress((const GLubyte *)"glDisableVertexArrayAttrib") == NULL ||
        glXGetProcAddress((const GLubyte *)"glEnableVertexArrayAttribEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glDisableVertexArrayAttribEXT") == NULL)
        return fail(11, "proc-address coverage");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL DSA vertex-array attrib unit test: PASS");
    return 0;
}

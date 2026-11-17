#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "native/dispatch.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static int fail(int n, const char *why) {
    fprintf(stderr, "query/selector semantics failure %d: %s\n", n, why);
    return n;
}

static int expect_error(GLenum expected, int n, const char *why) {
    GLenum got = glGetError();
    if (got != expected) {
        fprintf(stderr, "query/selector semantics failure %d: %s (got 0x%x expected 0x%x)\n", n,
                why, (unsigned)got, (unsigned)expected);
        return n;
    }
    return 0;
}

static const GLubyte *fake_gpu_identity(GLenum name) {
    if (name == GL_VENDOR)
        return (const GLubyte *)"Actual GPU Vendor";
    if (name == GL_RENDERER)
        return (const GLubyte *)"Actual GPU Model";
    return NULL;
}

static GLint client_selector(void) {
    GLint value = -1;
    glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &value);
    return value;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint list;
    const GLubyte *text;
    GLenum err;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetString = fake_gpu_identity;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 320, 240);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    if (glGetError() != GL_NO_ERROR)
        return fail(2, "initial error state");
    if (glGetString(0xdeadbeefu) != NULL)
        return fail(3, "invalid string enum returned data");
    if ((rc = expect_error(GL_INVALID_ENUM, 4, "GetString invalid enum")) != 0)
        return rc;
    if (glGetError() != GL_NO_ERROR)
        return fail(5, "GetError did not clear pending flag");

    if ((text = glGetString(GL_VENDOR)) == NULL ||
        strcmp((const char *)text, "Actual GPU Vendor") != 0)
        return fail(6, "vendor string");
    if ((text = glGetString(GL_RENDERER)) == NULL ||
        strcmp((const char *)text, "Actual GPU Model") != 0)
        return fail(7, "renderer string");
    if ((text = glGetString(GL_VERSION)) == NULL || text[0] == '\0')
        return fail(8, "version string");
    if ((text = glGetString(GL_SHADING_LANGUAGE_VERSION)) == NULL || text[0] == '\0')
        return fail(9, "GLSL string");
    if ((text = glGetString(GL_EXTENSIONS)) == NULL || text[0] == '\0')
        return fail(10, "extension string");
    if (glGetError() != GL_NO_ERROR)
        return fail(11, "valid string query error");

    /* ClientActiveTexture owns exactly the implementation's client texture-coordinate units. */
    if (client_selector() != (GLint)GL_TEXTURE0 || glGetError() != GL_NO_ERROR)
        return fail(12, "initial client selector");
    glClientActiveTexture(GL_TEXTURE0 + MGL_TEXTURE_UNITS - 1u);
    if (client_selector() != (GLint)(GL_TEXTURE0 + MGL_TEXTURE_UNITS - 1u) ||
        glGetError() != GL_NO_ERROR)
        return fail(13, "last valid client selector");
    glClientActiveTexture(GL_TEXTURE0 + MGL_TEXTURE_UNITS);
    if ((rc = expect_error(GL_INVALID_ENUM, 14, "client selector upper bound")) != 0 ||
        client_selector() != (GLint)(GL_TEXTURE0 + MGL_TEXTURE_UNITS - 1u))
        return fail(14, "invalid selector mutated state");
    glClientActiveTexture(GL_TEXTURE0 - 1u);
    if ((rc = expect_error(GL_INVALID_ENUM, 15, "client selector lower bound")) != 0 ||
        client_selector() != (GLint)(GL_TEXTURE0 + MGL_TEXTURE_UNITS - 1u))
        return fail(15, "lower invalid selector mutated state");

    /* Query/client selector calls reject inside Begin/End before ordinary validation/state changes.
     */
    glClientActiveTexture(GL_TEXTURE2);
    glBegin(GL_POINTS);
    glClientActiveTexture(0xdeadbeefu);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 16, "ClientActiveTexture Begin/End")) != 0 ||
        client_selector() != (GLint)GL_TEXTURE2)
        return fail(16, "ClientActiveTexture Begin/End mutation");

    glBegin(GL_POINTS);
    text = glGetString(0xdeadbeefu);
    glEnd();
    if (text != NULL || (rc = expect_error(GL_INVALID_OPERATION, 17, "GetString Begin/End")) != 0)
        return fail(17, "GetString Begin/End ordering");

    glBegin(GL_POINTS);
    text = glGetStringi(0xdeadbeefu, ~0u);
    glEnd();
    if (text != NULL || (rc = expect_error(GL_INVALID_OPERATION, 18, "GetStringi Begin/End")) != 0)
        return fail(18, "GetStringi Begin/End ordering");

    glBegin(GL_POINTS);
    err = glGetError();
    glEnd();
    if (err != GL_NO_ERROR || glGetError() != GL_INVALID_OPERATION || glGetError() != GL_NO_ERROR)
        return fail(19, "GetError Begin/End generation");

    if (glGetString(0xdeadbeefu) != NULL)
        return fail(20, "seed pending error");
    glBegin(GL_POINTS);
    err = glGetError();
    if (err != GL_NO_ERROR) {
        glEnd();
        return fail(21, "GetError consumed pending error");
    }
    glEnd();
    if (glGetError() != GL_INVALID_ENUM || glGetError() != GL_NO_ERROR)
        return fail(22, "GetError Begin/End pending error preservation");

    list = glGenLists(1);
    if (list == 0u)
        return fail(23, "list allocation");
    glClientActiveTexture(GL_TEXTURE1);
    glNewList(list, GL_COMPILE);
    glClientActiveTexture(GL_TEXTURE3);
    text = glGetString(GL_VERSION);
    if (text == NULL)
        return fail(24, "GetString did not execute during compile");
    if (glGetString(0xdeadbeefu) != NULL || glGetError() != GL_INVALID_ENUM)
        return fail(25, "GetError did not execute during compile");
    glEndList();
    if (client_selector() != (GLint)GL_TEXTURE3 || glGetError() != GL_NO_ERROR)
        return fail(26, "compile-time client selector/query state");

    glClientActiveTexture(GL_TEXTURE1);
    if (glGetString(0xdeadbeefu) != NULL)
        return fail(27, "seed replay error");
    glCallList(list);
    if (client_selector() != (GLint)GL_TEXTURE1)
        return fail(28, "client selector was retained in list");
    if (glGetError() != GL_INVALID_ENUM || glGetError() != GL_NO_ERROR)
        return fail(29, "query was retained in list or replay changed error state");

    if (glXGetProcAddress((const GLubyte *)"glGetError") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetString") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetStringi") == NULL ||
        glXGetProcAddress((const GLubyte *)"glClientActiveTexture") == NULL)
        return fail(30, "proc-address coverage");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("query/selector semantics passed");
    return 0;
}

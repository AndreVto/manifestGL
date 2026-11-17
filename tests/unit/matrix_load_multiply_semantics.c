#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(int n, const char *what) {
    fprintf(stderr, "matrix load/multiply failure %d: %s\n", n, what);
    return n;
}

static int same_matrix(const GLfloat *actual, const GLfloat *expected) {
    unsigned i;
    for (i = 0u; i < 16u; ++i)
        if (fabsf(actual[i] - expected[i]) > 0.00001f)
            return 0;
    return 1;
}

static int error_is(GLenum expected, int n, const char *what) {
    GLenum actual = glGetError();
    if (actual != expected) {
        fprintf(stderr, "matrix load/multiply failure %d: %s (error 0x%x, expected 0x%x)\n", n,
                what, (unsigned)actual, (unsigned)expected);
        return n;
    }
    return 0;
}

static void load_identity(GLfloat *m) {
    unsigned i;
    for (i = 0u; i < 16u; ++i)
        m[i] = (i % 5u == 0u) ? 1.0f : 0.0f;
}

int main(void) {
    struct mgl_context ctx;
    GLfloat translation[16], scale[16], expected[16], saved[16];
    GLfloat captured[16], list_operand[16];
    GLdouble translation_d[16], scale_d[16];
    GLuint list, list_execute;
    unsigned i;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group allocation");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 320, 240);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    load_identity(translation);
    translation[12] = 2.0f;
    translation[13] = 3.0f;
    translation[14] = 4.0f;
    load_identity(scale);
    scale[0] = 2.0f;
    scale[5] = 3.0f;
    scale[10] = 4.0f;
    for (i = 0u; i < 16u; ++i) {
        translation_d[i] = (GLdouble)translation[i];
        scale_d[i] = (GLdouble)scale[i];
    }

    /* Matrix multiplication is postmultiplication, not componentwise or pre-multiply. */
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(translation);
    glMultMatrixf(scale);
    memcpy(expected, scale, sizeof(expected));
    expected[12] = 2.0f;
    expected[13] = 3.0f;
    expected[14] = 4.0f;
    if (!same_matrix(mgl_compat_modelview(&ctx.compat), expected))
        return fail(2, "float load and postmultiply order");

    glLoadMatrixd(scale_d);
    glMultMatrixd(translation_d);
    memcpy(expected, scale, sizeof(expected));
    expected[12] = 4.0f;
    expected[13] = 9.0f;
    expected[14] = 16.0f;
    if (!same_matrix(mgl_compat_modelview(&ctx.compat), expected))
        return fail(3, "double load and postmultiply order");

    memcpy(saved, mgl_compat_modelview(&ctx.compat), sizeof(saved));
    glBegin(GL_POINTS);
    glLoadMatrixf(translation);
    glEnd();
    if ((rc = error_is(GL_INVALID_OPERATION, 4, "glLoadMatrixf inside Begin/End")) != 0 ||
        !same_matrix(saved, mgl_compat_modelview(&ctx.compat)))
        return fail(4, "float load changed matrix illegally");
    glBegin(GL_POINTS);
    glLoadMatrixd(translation_d);
    glEnd();
    if ((rc = error_is(GL_INVALID_OPERATION, 5, "glLoadMatrixd inside Begin/End")) != 0 ||
        !same_matrix(saved, mgl_compat_modelview(&ctx.compat)))
        return fail(5, "double load changed matrix illegally");
    glBegin(GL_POINTS);
    glMultMatrixf(scale);
    glEnd();
    if ((rc = error_is(GL_INVALID_OPERATION, 6, "glMultMatrixf inside Begin/End")) != 0 ||
        !same_matrix(saved, mgl_compat_modelview(&ctx.compat)))
        return fail(6, "float multiply changed matrix illegally");
    glBegin(GL_POINTS);
    glMultMatrixd(scale_d);
    glEnd();
    if ((rc = error_is(GL_INVALID_OPERATION, 7, "glMultMatrixd inside Begin/End")) != 0 ||
        !same_matrix(saved, mgl_compat_modelview(&ctx.compat)))
        return fail(7, "double multiply changed matrix illegally");

    /* Matrix mode and texture unit decide the destination at call time. */
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(translation);
    glActiveTexture(GL_TEXTURE0);
    glMatrixMode(GL_TEXTURE);
    glLoadMatrixd(scale_d);
    glActiveTexture(GL_TEXTURE1);
    glLoadMatrixf(translation);
    glMultMatrixd(scale_d);
    memcpy(expected, scale, sizeof(expected));
    expected[12] = 2.0f;
    expected[13] = 3.0f;
    expected[14] = 4.0f;
    if (!same_matrix(mgl_compat_texture_matrix(&ctx.compat, 1u), expected) ||
        !same_matrix(mgl_compat_texture_matrix(&ctx.compat, 0u), scale) ||
        !same_matrix(mgl_compat_projection(&ctx.compat), translation) ||
        !same_matrix(mgl_compat_modelview(&ctx.compat), saved))
        return fail(8, "matrix stacks or texture units are not isolated");

    /* GL_COMPILE must copy the client payload, then defer both operations. */
    glMatrixMode(GL_MODELVIEW);
    list = glGenLists(1);
    if (list == 0u)
        return fail(9, "display list allocation");
    memcpy(captured, translation, sizeof(captured));
    memcpy(list_operand, scale, sizeof(list_operand));
    glNewList(list, GL_COMPILE);
    glLoadMatrixf(captured);
    glMultMatrixd(scale_d);
    glEndList();
    memset(captured, 0, sizeof(captured));
    memset(scale_d, 0, sizeof(scale_d));
    if (!same_matrix(mgl_compat_modelview(&ctx.compat), saved))
        return fail(10, "GL_COMPILE executed matrix operations");
    glCallList(list);
    memcpy(expected, list_operand, sizeof(expected));
    expected[12] = 2.0f;
    expected[13] = 3.0f;
    expected[14] = 4.0f;
    if (!same_matrix(mgl_compat_modelview(&ctx.compat), expected))
        return fail(11, "list replay did not use copied matrix payloads");
    glDeleteLists(list, 1);

    /* GL_COMPILE_AND_EXECUTE acts immediately and replays with current state later. */
    list_execute = glGenLists(1);
    if (list_execute == 0u)
        return fail(12, "compile-and-execute allocation");
    glLoadMatrixd(translation_d);
    glNewList(list_execute, GL_COMPILE_AND_EXECUTE);
    glMultMatrixf(scale);
    glEndList();
    if (!same_matrix(mgl_compat_modelview(&ctx.compat), expected))
        return fail(13, "GL_COMPILE_AND_EXECUTE did not multiply immediately");
    glLoadMatrixd(translation_d);
    glCallList(list_execute);
    if (!same_matrix(mgl_compat_modelview(&ctx.compat), expected))
        return fail(14, "replayed multiply used wrong matrix or base state");
    glDeleteLists(list_execute, 1);

    /* Illegal commands must not enter a list being compiled inside Begin/End. */
    list = glGenLists(1);
    if (list == 0u)
        return fail(17, "invalid-command list allocation");
    memcpy(saved, mgl_compat_modelview(&ctx.compat), sizeof(saved));
    glNewList(list, GL_COMPILE);
    glBegin(GL_POINTS);
    glLoadMatrixf(translation);
    glMultMatrixd(translation_d);
    glEnd();
    glEndList();
    if ((rc = error_is(GL_INVALID_OPERATION, 18, "matrix command during list Begin/End")) != 0)
        return rc;
    glCallList(list);
    if (!same_matrix(saved, mgl_compat_modelview(&ctx.compat)))
        return fail(19, "invalid matrix command was recorded into display list");
    glDeleteLists(list, 1);

    if (glXGetProcAddress((const GLubyte *)"glLoadMatrixf") == NULL ||
        glXGetProcAddress((const GLubyte *)"glLoadMatrixd") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultMatrixf") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultMatrixd") == NULL)
        return fail(15, "matrix load/multiply proc addresses");
    if ((rc = error_is(GL_NO_ERROR, 16, "unexpected final GL error")) != 0)
        return rc;

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("matrix load/multiply semantics passed");
    return 0;
}

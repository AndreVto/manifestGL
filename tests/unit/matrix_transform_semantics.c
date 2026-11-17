#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(int id, const char *message) {
    fprintf(stderr, "matrix transform failure %d: %s\n", id, message);
    return id;
}

static int matrix_equal(const GLfloat *a, const GLfloat *b) {
    unsigned i;
    for (i = 0; i < 16; ++i)
        if (fabsf(a[i] - b[i]) > 0.0001f)
            return 0;
    return 1;
}

static void identity(GLfloat m[16]) {
    unsigned i;
    for (i = 0; i < 16; ++i)
        m[i] = (i % 5u == 0u) ? 1.0f : 0.0f;
}

static int error_is(GLenum expected, int id, const char *message) {
    GLenum got = glGetError();
    if (got != expected) {
        fprintf(stderr, "matrix transform failure %d: %s (error 0x%x, expected 0x%x)\n", id,
                message, (unsigned)got, (unsigned)expected);
        return id;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    GLfloat expected[16], saved[16], projection[16], texture0[16];
    GLfloat base[16];
    GLuint list;
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

    /* Independent column-major reference: T*S*R, not R*S*T. */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(2.0f, 3.0f, 4.0f);
    glScaled(2.0, 3.0, 4.0);
    glRotatef(90.0f, 0.0f, 0.0f, 3.0f);
    identity(expected);
    expected[0] = 0;
    expected[1] = 3;
    expected[4] = -2;
    expected[5] = 0;
    expected[10] = 4;
    expected[12] = 2;
    expected[13] = 3;
    expected[14] = 4;
    if (!matrix_equal(mgl_compat_modelview(&ctx.compat), expected))
        return fail(2, "translate/scale/rotate ordering or axis normalization");

    glLoadIdentity();
    glTranslated(2.0, 3.0, 4.0);
    glScalef(2.0f, 3.0f, 4.0f);
    glRotated(90.0, 0.0, 0.0, 3.0);
    if (!matrix_equal(mgl_compat_modelview(&ctx.compat), expected))
        return fail(3, "double variants differ from float transformations");
    memcpy(saved, expected, sizeof(saved));

    /* Ortho accepts negative near/far distances; Frustum requires strictly positive. */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-2.0, 6.0, -3.0, 5.0, -4.0, 8.0);
    identity(projection);
    projection[0] = 0.25f;
    projection[5] = 0.25f;
    projection[10] = -1.0f / 6.0f;
    projection[12] = -0.5f;
    projection[13] = -0.25f;
    projection[14] = -1.0f / 3.0f;
    if (!matrix_equal(mgl_compat_projection(&ctx.compat), projection))
        return fail(4, "orthographic projection coefficients");
    glOrtho(1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    if ((rc = error_is(GL_INVALID_VALUE, 5, "degenerate ortho")) != 0 ||
        !matrix_equal(mgl_compat_projection(&ctx.compat), projection))
        return fail(5, "degenerate ortho mutated projection");

    glLoadIdentity();
    glFrustum(-2.0, 2.0, -1.0, 3.0, 2.0, 10.0);
    memset(projection, 0, sizeof(projection));
    projection[0] = 1;
    projection[5] = 1;
    projection[9] = 0.5f;
    projection[10] = -1.5f;
    projection[11] = -1.0f;
    projection[14] = -5;
    if (!matrix_equal(mgl_compat_projection(&ctx.compat), projection))
        return fail(6, "perspective frustum coefficients");
    glFrustum(-1.0, 1.0, -1.0, 1.0, 0.0, 5.0);
    if ((rc = error_is(GL_INVALID_VALUE, 7, "nonpositive frustum near")) != 0 ||
        !matrix_equal(mgl_compat_projection(&ctx.compat), projection))
        return fail(7, "invalid frustum mutated projection");
    glFrustum(0.0, 0.0, -1.0, 1.0, 1.0, 5.0);
    if ((rc = error_is(GL_INVALID_VALUE, 8, "degenerate frustum")) != 0 ||
        !matrix_equal(mgl_compat_projection(&ctx.compat), projection))
        return fail(8, "degenerate frustum mutated projection");

        /* Every entry point must refuse Begin/End before parameter validation. */
#define REJECT(command, id)                                                                        \
    do {                                                                                           \
        glBegin(GL_POINTS);                                                                        \
        command;                                                                                   \
        glEnd();                                                                                   \
        if ((rc = error_is(GL_INVALID_OPERATION, (id), #command)) != 0 ||                          \
            !matrix_equal(mgl_compat_projection(&ctx.compat), projection))                         \
            return fail((id), #command " changed matrix inside Begin/End");                        \
    } while (0)
    REJECT(glTranslatef(1.0f, 2.0f, 3.0f), 9);
    REJECT(glTranslated(1.0, 2.0, 3.0), 10);
    REJECT(glScalef(1.0f, 2.0f, 3.0f), 11);
    REJECT(glScaled(1.0, 2.0, 3.0), 12);
    REJECT(glRotatef(45.0f, 0.0f, 0.0f, 1.0f), 13);
    REJECT(glRotated(45.0, 0.0, 0.0, 1.0), 14);
    REJECT(glRotatef(45.0f, 0.0f, 0.0f, 0.0f), 15);
    REJECT(glRotated(45.0, 0.0, 0.0, 0.0), 16);
    REJECT(glOrtho(-1.0, 1.0, -1.0, 1.0, 1.0, 5.0), 17);
    REJECT(glFrustum(-1.0, 1.0, -1.0, 1.0, 1.0, 5.0), 18);
    REJECT(glOrtho(0.0, 0.0, -1.0, 1.0, 1.0, 5.0), 19);
    REJECT(glFrustum(-1.0, 1.0, -1.0, 1.0, -1.0, 5.0), 20);
#undef REJECT

    /* Projection changes must not affect modelview or another texture unit. */
    glActiveTexture(GL_TEXTURE0);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(7.0f, 0.0f, 0.0f);
    memcpy(texture0, mgl_compat_texture_matrix(&ctx.compat, 0u), sizeof(texture0));
    glActiveTexture(GL_TEXTURE1);
    glLoadIdentity();
    glScalef(2.0f, 3.0f, 4.0f);
    identity(expected);
    expected[0] = 2;
    expected[5] = 3;
    expected[10] = 4;
    if (!matrix_equal(mgl_compat_texture_matrix(&ctx.compat, 1u), expected) ||
        !matrix_equal(mgl_compat_texture_matrix(&ctx.compat, 0u), texture0) ||
        !matrix_equal(mgl_compat_modelview(&ctx.compat), saved) ||
        !matrix_equal(mgl_compat_projection(&ctx.compat), projection))
        return fail(21, "matrix-mode or texture-unit state leaked");

    /* Valid transform operands must be deferred in GL_COMPILE, applied on replay. */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    list = glGenLists(1);
    if (list == 0u)
        return fail(22, "display list allocation");
    glNewList(list, GL_COMPILE);
    glTranslatef(2, 3, 4);
    glScalef(2, 3, 4);
    glRotated(90, 0, 0, 1);
    glEndList();
    identity(base);
    if (!matrix_equal(mgl_compat_modelview(&ctx.compat), base))
        return fail(23, "GL_COMPILE prematurely executed transformations");
    glCallList(list);
    memcpy(expected, saved, sizeof(expected));
    if (!matrix_equal(mgl_compat_modelview(&ctx.compat), expected))
        return fail(24, "compiled transforms did not replay correctly");
    glDeleteLists(list, 1);

    /* A list's illegal commands inside Begin/End must not replay later. */
    list = glGenLists(1);
    if (list == 0u)
        return fail(25, "invalid-command list allocation");
    glNewList(list, GL_COMPILE);
    glBegin(GL_POINTS);
    glRotatef(45.0f, 0.0f, 0.0f, 0.0f);
    glOrtho(0, 0, -1, 1, 1, 5);
    glFrustum(-1, 1, -1, 1, -1, 5);
    glTranslatef(5, 6, 7);
    glScalef(5, 6, 7);
    glEnd();
    glEndList();
    if ((rc = error_is(GL_INVALID_OPERATION, 26, "invalid list compilation")) != 0)
        return rc;
    memcpy(expected, mgl_compat_modelview(&ctx.compat), sizeof(expected));
    glCallList(list);
    if (!matrix_equal(mgl_compat_modelview(&ctx.compat), expected) ||
        !matrix_equal(mgl_compat_projection(&ctx.compat), projection))
        return fail(27, "illegal transformations recorded into display list");
    glDeleteLists(list, 1);

    glLoadIdentity();
    list = glGenLists(1);
    if (list == 0u)
        return fail(28, "execute-list allocation");
    glNewList(list, GL_COMPILE_AND_EXECUTE);
    glTranslated(5, 6, 7);
    glEndList();
    identity(expected);
    expected[12] = 5;
    expected[13] = 6;
    expected[14] = 7;
    if (!matrix_equal(mgl_compat_modelview(&ctx.compat), expected))
        return fail(29, "GL_COMPILE_AND_EXECUTE failed to apply transformation");
    glLoadIdentity();
    glCallList(list);
    if (!matrix_equal(mgl_compat_modelview(&ctx.compat), expected))
        return fail(30, "GL_COMPILE_AND_EXECUTE replay failed");
    glDeleteLists(list, 1);

    /* Both projection commands must record scalars, defer execution, and replay. */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    list = glGenLists(1);
    if (list == 0u)
        return fail(33, "projection list allocation");
    glNewList(list, GL_COMPILE);
    glOrtho(-1, 1, -1, 1, -1, 1);
    glFrustum(-1, 1, -1, 1, 1, 3);
    glEndList();
    identity(base);
    if (!matrix_equal(mgl_compat_projection(&ctx.compat), base))
        return fail(34, "GL_COMPILE executed projection commands");
    glCallList(list);
    memset(expected, 0, sizeof(expected));
    expected[0] = 1;
    expected[5] = 1;
    expected[10] = 2;
    expected[11] = -1;
    expected[14] = 3;
    if (!matrix_equal(mgl_compat_projection(&ctx.compat), expected))
        return fail(35, "compiled projections did not postmultiply in order");
    glDeleteLists(list, 1);

    /* Axis normalization must not overflow/underflow for finite large/small axes. */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(90.0f, 0.0f, 0.0f, 1.0e20f);
    identity(expected);
    expected[0] = 0;
    expected[1] = 1;
    expected[4] = -1;
    expected[5] = 0;
    if (!matrix_equal(mgl_compat_modelview(&ctx.compat), expected))
        return fail(36, "large float rotation axis lost direction");
    glLoadIdentity();
    glRotatef(90.0f, 0.0f, 0.0f, 1.0e-30f);
    if (!matrix_equal(mgl_compat_modelview(&ctx.compat), expected))
        return fail(37, "small float rotation axis lost direction");
    glLoadIdentity();
    glRotated(90.0, 0.0, 0.0, 1.0e200);
    if (!matrix_equal(mgl_compat_modelview(&ctx.compat), expected))
        return fail(38, "large double rotation axis lost direction");
    glLoadIdentity();
    glRotated(90.0, 0.0, 0.0, 1.0e-200);
    if (!matrix_equal(mgl_compat_modelview(&ctx.compat), expected))
        return fail(39, "small double rotation axis lost direction");

    if (glXGetProcAddress((const GLubyte *)"glTranslatef") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTranslated") == NULL ||
        glXGetProcAddress((const GLubyte *)"glScalef") == NULL ||
        glXGetProcAddress((const GLubyte *)"glScaled") == NULL ||
        glXGetProcAddress((const GLubyte *)"glRotatef") == NULL ||
        glXGetProcAddress((const GLubyte *)"glRotated") == NULL ||
        glXGetProcAddress((const GLubyte *)"glOrtho") == NULL ||
        glXGetProcAddress((const GLubyte *)"glFrustum") == NULL)
        return fail(31, "transformation proc addresses");
    if ((rc = error_is(GL_NO_ERROR, 32, "unexpected final error")) != 0)
        return rc;

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("matrix transform semantics passed");
    return 0;
}

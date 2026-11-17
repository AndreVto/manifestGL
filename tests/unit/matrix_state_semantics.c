#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(int n, const char *why) {
    fprintf(stderr, "matrix-state semantics failure %d: %s\n", n, why);
    return n;
}

static int error_is(GLenum expected, int n, const char *why) {
    GLenum got = glGetError();
    if (got != expected) {
        fprintf(stderr, "matrix-state semantics failure %d: %s (got 0x%x expected 0x%x)\n", n, why,
                (unsigned)got, (unsigned)expected);
        return n;
    }
    return 0;
}

static int nearf(GLfloat x, GLfloat y) {
    return fabsf(x - y) < 0.0001f;
}

static int identity(const GLfloat *m) {
    unsigned i;
    for (i = 0u; i < 16u; ++i) {
        if (!nearf(m[i], (i % 5u) == 0u ? 1.0f : 0.0f))
            return 0;
    }
    return 1;
}

static GLint matrix_mode(void) {
    GLint value = -1;
    glGetIntegerv(GL_MATRIX_MODE, &value);
    return value;
}

int main(void) {
    struct mgl_context ctx;
    GLfloat saved[16];
    GLuint list, list2;
    unsigned i;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
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

    if (matrix_mode() != (GLint)GL_MODELVIEW || !identity(mgl_compat_modelview(&ctx.compat)))
        return fail(2, "initial modelview matrix/mode");

    glMatrixMode(GL_MODELVIEW);
    glTranslatef(2.0f, 3.0f, 4.0f);
    memcpy(saved, mgl_compat_modelview(&ctx.compat), sizeof(saved));
    if (!nearf(saved[12], 2.0f))
        return fail(3, "setup translation");
    glMatrixMode(0xdeadbeefu);
    if ((rc = error_is(GL_INVALID_ENUM, 4, "invalid matrix mode")) != 0 ||
        matrix_mode() != (GLint)GL_MODELVIEW)
        return fail(4, "invalid mode changed selector");

    /* INVALID_OPERATION takes precedence over invalid enum and preserves matrix state. */
    glBegin(GL_POINTS);
    glMatrixMode(0xdeadbeefu);
    glEnd();
    if ((rc = error_is(GL_INVALID_OPERATION, 5, "MatrixMode inside Begin/End")) != 0 ||
        matrix_mode() != (GLint)GL_MODELVIEW)
        return fail(5, "MatrixMode Begin/End state");

    glBegin(GL_POINTS);
    glMatrixMode(GL_PROJECTION);
    glEnd();
    if ((rc = error_is(GL_INVALID_OPERATION, 24, "valid MatrixMode inside Begin/End")) != 0 ||
        matrix_mode() != (GLint)GL_MODELVIEW)
        return fail(24, "MatrixMode changed selector inside Begin/End");

    glBegin(GL_POINTS);
    glLoadIdentity();
    glEnd();
    if ((rc = error_is(GL_INVALID_OPERATION, 6, "LoadIdentity inside Begin/End")) != 0 ||
        memcmp(saved, mgl_compat_modelview(&ctx.compat), sizeof(saved)) != 0)
        return fail(6, "LoadIdentity changed matrix inside Begin/End");

    glBegin(GL_POINTS);
    glPushMatrix();
    glEnd();
    if ((rc = error_is(GL_INVALID_OPERATION, 7, "PushMatrix inside Begin/End")) != 0 ||
        ctx.compat.modelview_top != 0u)
        return fail(7, "PushMatrix changed stack inside Begin/End");

    glPushMatrix();
    glTranslatef(5.0f, 0.0f, 0.0f);
    glBegin(GL_POINTS);
    glPopMatrix();
    glEnd();
    if ((rc = error_is(GL_INVALID_OPERATION, 8, "PopMatrix inside Begin/End")) != 0 ||
        ctx.compat.modelview_top != 1u || !nearf(mgl_compat_modelview(&ctx.compat)[12], 7.0f))
        return fail(8, "PopMatrix changed stack inside Begin/End");
    glPopMatrix();
    if (ctx.compat.modelview_top != 0u ||
        memcmp(saved, mgl_compat_modelview(&ctx.compat), sizeof(saved)) != 0)
        return fail(9, "modelview push/pop did not restore saved matrix");

    /* Minimum compatibility-profile stack capacity, overflow/underflow and isolation. */
    for (i = 1u; i < MGL_MODELVIEW_STACK_DEPTH; ++i)
        glPushMatrix();
    if (ctx.compat.modelview_top != MGL_MODELVIEW_STACK_DEPTH - 1u ||
        (rc = error_is(GL_NO_ERROR, 10, "modelview capacity")) != 0)
        return fail(10, "modelview stack did not reach maximum depth");
    glPushMatrix();
    if ((rc = error_is(GL_STACK_OVERFLOW, 11, "modelview overflow")) != 0 ||
        ctx.compat.modelview_top != MGL_MODELVIEW_STACK_DEPTH - 1u)
        return fail(11, "modelview overflow changed stack");
    for (i = 1u; i < MGL_MODELVIEW_STACK_DEPTH; ++i)
        glPopMatrix();
    glPopMatrix();
    if ((rc = error_is(GL_STACK_UNDERFLOW, 12, "modelview underflow")) != 0 ||
        ctx.compat.modelview_top != 0u ||
        memcmp(saved, mgl_compat_modelview(&ctx.compat), sizeof(saved)) != 0)
        return fail(12, "modelview underflow corrupted matrix");

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glTranslatef(9.0f, 0.0f, 0.0f);
    glPushMatrix();
    glLoadIdentity();
    if (!identity(mgl_compat_projection(&ctx.compat)))
        return fail(13, "projection LoadIdentity");
    glPushMatrix();
    if ((rc = error_is(GL_STACK_OVERFLOW, 14, "projection capacity")) != 0 ||
        ctx.compat.projection_top != 1u)
        return fail(14, "projection stack overflow");
    glPopMatrix();
    if (!nearf(mgl_compat_projection(&ctx.compat)[12], 9.0f) ||
        !nearf(mgl_compat_modelview(&ctx.compat)[12], 2.0f))
        return fail(15, "projection stack isolation/restoration");

    glActiveTexture(GL_TEXTURE0);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(1.0f, 0.0f, 0.0f);
    glActiveTexture(GL_TEXTURE1);
    glLoadIdentity();
    glTranslatef(3.0f, 0.0f, 0.0f);
    glPushMatrix();
    glLoadIdentity();
    if (!nearf(mgl_compat_texture_matrix(&ctx.compat, 0u)[12], 1.0f) ||
        !identity(mgl_compat_texture_matrix(&ctx.compat, 1u)))
        return fail(16, "texture stack shared between units");
    glPopMatrix();
    if (!nearf(mgl_compat_texture_matrix(&ctx.compat, 1u)[12], 3.0f) ||
        ctx.compat.texture_top[0] != 0u || ctx.compat.texture_top[1] != 0u)
        return fail(17, "texture unit stack pop");

    /* Display lists defer matrix operations in COMPILE and replay at call time. */
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(6.0f, 0.0f, 0.0f);
    memcpy(saved, mgl_compat_modelview(&ctx.compat), sizeof(saved));
    list = glGenLists(1);
    if (list == 0u)
        return fail(18, "list allocation");
    glNewList(list, GL_COMPILE);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEndList();
    if (matrix_mode() != (GLint)GL_MODELVIEW || ctx.compat.projection_top != 0u ||
        memcmp(saved, mgl_compat_modelview(&ctx.compat), sizeof(saved)) != 0 ||
        !nearf(mgl_compat_projection(&ctx.compat)[12], 9.0f))
        return fail(19, "GL_COMPILE executed matrix operations");
    glCallList(list);
    if (matrix_mode() != (GLint)GL_MODELVIEW || !identity(mgl_compat_modelview(&ctx.compat)) ||
        ctx.compat.projection_top != 1u || !identity(mgl_compat_projection(&ctx.compat)))
        return fail(20, "list matrix replay order/matrix target");
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    if (!nearf(mgl_compat_projection(&ctx.compat)[12], 9.0f))
        return fail(21, "list PushMatrix saved pre-replay projection");
    glDeleteLists(list, 1);

    /* COMPILE_AND_EXECUTE modifies state once, then retains push/pop for later calls. */
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(5.0f, 0.0f, 0.0f);
    memcpy(saved, mgl_compat_modelview(&ctx.compat), sizeof(saved));
    list2 = glGenLists(1);
    if (list2 == 0u)
        return fail(25, "compile-and-execute list allocation");
    glNewList(list2, GL_COMPILE_AND_EXECUTE);
    glPushMatrix();
    glLoadIdentity();
    glPopMatrix();
    glEndList();
    if (ctx.compat.modelview_top != 0u ||
        memcmp(saved, mgl_compat_modelview(&ctx.compat), sizeof(saved)) != 0)
        return fail(26, "compile-and-execute matrix push/pop");
    glLoadIdentity();
    glTranslatef(11.0f, 0.0f, 0.0f);
    glCallList(list2);
    if (ctx.compat.modelview_top != 0u || !nearf(mgl_compat_modelview(&ctx.compat)[12], 11.0f))
        return fail(27, "replayed matrix push/pop lost current matrix");
    glDeleteLists(list2, 1);

    if (glXGetProcAddress((const GLubyte *)"glMatrixMode") == NULL ||
        glXGetProcAddress((const GLubyte *)"glLoadIdentity") == NULL ||
        glXGetProcAddress((const GLubyte *)"glPushMatrix") == NULL ||
        glXGetProcAddress((const GLubyte *)"glPopMatrix") == NULL)
        return fail(22, "missing GLX proc-address entry");
    if ((rc = error_is(GL_NO_ERROR, 23, "unexpected final GL error")) != 0)
        return rc;
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("matrix-state semantics passed");
    return 0;
}

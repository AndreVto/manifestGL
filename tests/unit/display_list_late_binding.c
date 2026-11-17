#include "mgl/gl.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "shader/translator.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int closef(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 0.0001f;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "display-list failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    GLuint base;

    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    ctx.compat_current[MGL_COMPAT_SEM_COLOR][0] = 1.0f;
    ctx.compat_current[MGL_COMPAT_SEM_COLOR][1] = 1.0f;
    ctx.compat_current[MGL_COMPAT_SEM_COLOR][2] = 1.0f;
    ctx.compat_current[MGL_COMPAT_SEM_COLOR][3] = 1.0f;
    mgl_context_set_current_for_test(&ctx);

    base = glGenLists(3);
    if (base == 0u)
        return fail(2, "glGenLists");

    glNewList(base + 1u, GL_COMPILE);
    glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    glEndList();
    if (!glIsList(base + 1u))
        return fail(3, "child list defined");
    if (!closef(ctx.compat_current[MGL_COMPAT_SEM_COLOR][0], 1.0f) ||
        !closef(ctx.compat_current[MGL_COMPAT_SEM_COLOR][1], 1.0f))
        return fail(4, "GL_COMPILE executed captured color");

    glNewList(base, GL_COMPILE);
    glCallList(base + 1u);
    glEndList();
    glCallList(base);
    if (!closef(ctx.compat_current[MGL_COMPAT_SEM_COLOR][0], 1.0f) ||
        !closef(ctx.compat_current[MGL_COMPAT_SEM_COLOR][1], 0.0f))
        return fail(5, "initial nested call result");

    glNewList(base + 1u, GL_COMPILE);
    glColor4f(0.0f, 0.0f, 1.0f, 1.0f);
    glEndList();
    glColor4f(0.25f, 0.25f, 0.25f, 1.0f);
    glCallList(base);
    if (!closef(ctx.compat_current[MGL_COMPAT_SEM_COLOR][0], 0.0f) ||
        !closef(ctx.compat_current[MGL_COMPAT_SEM_COLOR][2], 1.0f))
        return fail(6, "nested CallList was not late-bound by name");

    glNewList(base + 2u, GL_COMPILE);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(2.0f, 3.0f, 4.0f);
    glEndList();
    if (!closef(ctx.compat.modelview[0][12], 0.0f) || !closef(ctx.compat.modelview[0][13], 0.0f))
        return fail(11, "GL_COMPILE executed matrix operations");
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(5.0f, 5.0f, 5.0f);
    glCallList(base + 2u);
    if (!closef(ctx.compat.modelview[0][0], 1.0f) || !closef(ctx.compat.modelview[0][5], 1.0f) ||
        !closef(ctx.compat.modelview[0][10], 1.0f) || !closef(ctx.compat.modelview[0][12], 2.0f) ||
        !closef(ctx.compat.modelview[0][13], 3.0f) || !closef(ctx.compat.modelview[0][14], 4.0f))
        return fail(12, "matrix commands did not replay semantically");

    glNewList(base + 1u, GL_COMPILE_AND_EXECUTE);
    glColor4f(0.0f, 1.0f, 0.0f, 0.5f);
    glEndList();
    if (!closef(ctx.compat_current[MGL_COMPAT_SEM_COLOR][1], 1.0f) ||
        !closef(ctx.compat_current[MGL_COMPAT_SEM_COLOR][3], 0.5f))
        return fail(7, "compile-and-execute immediate effect");
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glCallList(base + 1u);
    if (!closef(ctx.compat_current[MGL_COMPAT_SEM_COLOR][1], 1.0f) ||
        !closef(ctx.compat_current[MGL_COMPAT_SEM_COLOR][0], 0.0f))
        return fail(8, "compile-and-execute retained command");

    glDeleteLists(base, 3);
    if (glIsList(base) || glIsList(base + 1u) || glIsList(base + 2u))
        return fail(9, "delete lists");
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(10, "unexpected GL error");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL display-list late-binding unit test: PASS");
    return 0;
}

#include "mgl/gl.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "shader/translator.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int closef(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 1.0e-5f;
}

static int require(int cond, const char *msg) {
    if (!cond)
        fprintf(stderr, "%s\n", msg);
    return cond;
}

int main(void) {
    struct mgl_context ctx;
    GLfloat m[16];
    GLfloat n[9];
    memset(&ctx, 0, sizeof(ctx));
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_context_set_current_for_test(&ctx);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(2.0f, 3.0f, 4.0f);
    glScalef(2.0f, 3.0f, 4.0f);
    memcpy(m, mgl_compat_modelview(&ctx.compat), sizeof(m));
    if (!require(closef(m[0], 2.0f) && closef(m[5], 3.0f) && closef(m[10], 4.0f),
                 "scale matrix mismatch") ||
        !require(closef(m[12], 2.0f) && closef(m[13], 3.0f) && closef(m[14], 4.0f),
                 "translation matrix mismatch"))
        return 1;

    glPushMatrix();
    glTranslatef(1.0f, 0.0f, 0.0f);
    if (!require(ctx.compat.modelview_top == 1u, "modelview push failed"))
        return 1;
    glPopMatrix();
    if (!require(ctx.compat.modelview_top == 0u &&
                     closef(mgl_compat_modelview(&ctx.compat)[12], 2.0f),
                 "modelview pop failed"))
        return 1;
    glPopMatrix();
    if (!require(mgl_error_take(&ctx.errors) == GL_STACK_UNDERFLOW,
                 "matrix underflow error missing"))
        return 1;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -2.0, 2.0, 1.0, 11.0);
    if (!require(closef(mgl_compat_projection(&ctx.compat)[0], 1.0f), "ortho X scale mismatch"))
        return 1;

    glActiveTexture(GL_TEXTURE3);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(0.25f, 0.5f, 0.0f);
    if (!require(closef(mgl_compat_texture_matrix(&ctx.compat, 3u)[12], 0.25f),
                 "texture matrix unit mismatch"))
        return 1;

    glColor4f(0.1f, 0.2f, 0.3f, 0.4f);
    glSecondaryColor3f(0.5f, 0.6f, 0.7f);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glMultiTexCoord4f(GL_TEXTURE3, 1.0f, 2.0f, 3.0f, 4.0f);
    if (!require(closef(ctx.compat_current[MGL_COMPAT_SEM_COLOR][3], 0.4f),
                 "current color mismatch") ||
        !require(closef(ctx.compat_current[MGL_COMPAT_SEM_SECONDARY][1], 0.6f),
                 "secondary color mismatch") ||
        !require(closef(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 3u][2], 3.0f),
                 "multitex current mismatch"))
        return 1;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(2.0f, 4.0f, 8.0f);
    mgl_compat_normal_matrix(&ctx.compat, n);
    if (!require(closef(n[0], 0.5f) && closef(n[4], 0.25f) && closef(n[8], 0.125f),
                 "normal matrix mismatch"))
        return 1;

    mgl_context_set_current_for_test(NULL);
    return 0;
}

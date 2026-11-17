#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int closef(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 1.0e-5f;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "single-precision compatibility failure %d: %s\n", n, message);
    return n;
}

static int matrix_eq(const GLfloat *a, const GLfloat *b) {
    unsigned i;
    for (i = 0u; i < 16u; ++i)
        if (!closef(a[i], b[i]))
            return 0;
    return 1;
}

int main(void) {
    struct mgl_context ctx;
    GLfloat ortho_f[16];
    GLfloat ortho_d[16];
    GLfloat frustum_f[16];
    GLfloat frustum_d[16];
    GLfloat before[16];
    GLfloat clip_f[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    GLfloat got_f[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GLfloat sentinel[4] = {11.0f, 12.0f, 13.0f, 14.0f};

    memset(&ctx, 0, sizeof(ctx));
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_context_set_current_for_test(&ctx);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(-2.0f, 2.0f, -4.0f, 4.0f, 1.0f, 9.0f);
    memcpy(ortho_f, mgl_compat_projection(&ctx.compat), sizeof(ortho_f));
    if (!closef(ortho_f[0], 0.5f) || !closef(ortho_f[5], 0.25f) || !closef(ortho_f[10], -0.25f) ||
        !closef(ortho_f[14], -1.25f))
        return fail(1, "glOrthof matrix");

    glLoadIdentity();
    glOrtho(-2.0, 2.0, -4.0, 4.0, 1.0, 9.0);
    memcpy(ortho_d, mgl_compat_projection(&ctx.compat), sizeof(ortho_d));
    if (!matrix_eq(ortho_f, ortho_d))
        return fail(2, "glOrthof/glOrtho mismatch");

    glLoadIdentity();
    glFrustumf(-1.0f, 1.0f, -2.0f, 2.0f, 1.0f, 9.0f);
    memcpy(frustum_f, mgl_compat_projection(&ctx.compat), sizeof(frustum_f));
    if (!closef(frustum_f[0], 1.0f) || !closef(frustum_f[5], 0.5f) ||
        !closef(frustum_f[10], -1.25f) || !closef(frustum_f[11], -1.0f) ||
        !closef(frustum_f[14], -2.25f))
        return fail(3, "glFrustumf matrix");

    glLoadIdentity();
    glFrustum(-1.0, 1.0, -2.0, 2.0, 1.0, 9.0);
    memcpy(frustum_d, mgl_compat_projection(&ctx.compat), sizeof(frustum_d));
    if (!matrix_eq(frustum_f, frustum_d))
        return fail(4, "glFrustumf/glFrustum mismatch");

    glLoadIdentity();
    memcpy(before, mgl_compat_projection(&ctx.compat), sizeof(before));
    glOrthof(1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    if (glGetError() != GL_INVALID_VALUE || !matrix_eq(before, mgl_compat_projection(&ctx.compat)))
        return fail(5, "glOrthof invalid volume");
    glFrustumf(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 9.0f);
    if (glGetError() != GL_INVALID_VALUE || !matrix_eq(before, mgl_compat_projection(&ctx.compat)))
        return fail(6, "glFrustumf invalid near plane");

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(3.0f, 0.0f, 0.0f);
    glClipPlanef(GL_CLIP_PLANE0, clip_f);
    glGetClipPlanef(GL_CLIP_PLANE0, got_f);
    if (!closef(got_f[0], 1.0f) || !closef(got_f[1], 0.0f) || !closef(got_f[2], 0.0f) ||
        !closef(got_f[3], -3.0f))
        return fail(7, "single-precision clip plane transform/query");

    memcpy(got_f, sentinel, sizeof(got_f));
    glGetClipPlanef(GL_CLIP_PLANE0 + MGL_MAX_CLIP_PLANES, got_f);
    if (glGetError() != GL_INVALID_ENUM || memcmp(got_f, sentinel, sizeof(got_f)) != 0)
        return fail(8, "glGetClipPlanef error output preservation");

    if (glXGetProcAddress((const GLubyte *)"glFrustumf") == NULL ||
        glXGetProcAddress((const GLubyte *)"glOrthof") == NULL ||
        glXGetProcAddress((const GLubyte *)"glClipPlanef") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetClipPlanef") == NULL)
        return fail(9, "proc-address exposure");

    mgl_context_set_current_for_test(NULL);
    puts("mGL single-precision compatibility unit test: PASS");
    return 0;
}

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

static int fail(int code, const char *message) {
    fprintf(stderr, "EXT DSA matrix failure %d: %s\n", code, message);
    return code;
}

static int identity(const GLfloat *m) {
    unsigned i;
    for (i = 0u; i < 16u; ++i) {
        GLfloat expected = (i == 0u || i == 5u || i == 10u || i == 15u) ? 1.0f : 0.0f;
        if (!closef(m[i], expected))
            return 0;
    }
    return 1;
}

int main(void) {
    struct mgl_context ctx;
    GLuint lists;
    GLfloat source[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    GLfloat original[16];
    const GLfloat *m;

    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_context_set_current_for_test(&ctx);

    glActiveTexture(GL_TEXTURE2);
    glMatrixMode(GL_PROJECTION);
    glMatrixLoadIdentityEXT(GL_MODELVIEW);
    glMatrixTranslatefEXT(GL_MODELVIEW, 2.0f, 3.0f, 4.0f);
    if (ctx.compat.active_texture != 2u || ctx.compat.matrix_mode != GL_PROJECTION)
        return fail(2, "modelview DSA disturbed selectors");
    m = mgl_compat_modelview(&ctx.compat);
    if (!closef(m[12], 2.0f) || !closef(m[13], 3.0f) || !closef(m[14], 4.0f))
        return fail(3, "modelview DSA update");

    glMatrixLoadIdentityEXT(GL_TEXTURE3);
    glMatrixTranslatefEXT(GL_TEXTURE3, 0.25f, 0.5f, 0.75f);
    if (ctx.compat.active_texture != 2u || ctx.compat.matrix_mode != GL_PROJECTION)
        return fail(4, "fixed texture-unit DSA disturbed selectors");
    if (!closef(mgl_compat_texture_matrix(&ctx.compat, 3u)[12], 0.25f) ||
        !identity(mgl_compat_texture_matrix(&ctx.compat, 2u)))
        return fail(5, "fixed texture-unit selection");

    glMatrixLoadTransposefEXT(GL_PROJECTION, source);
    m = mgl_compat_projection(&ctx.compat);
    if (!closef(m[1], 5.0f) || !closef(m[4], 2.0f) || !closef(m[14], 12.0f))
        return fail(6, "transpose load");
    if (ctx.compat.active_texture != 2u || ctx.compat.matrix_mode != GL_PROJECTION)
        return fail(7, "transpose DSA disturbed selectors");

    glMatrixLoadIdentityEXT(GL_TEXTURE3);
    glMatrixPushEXT(GL_TEXTURE3);
    glMatrixTranslatefEXT(GL_TEXTURE3, 8.0f, 0.0f, 0.0f);
    if (ctx.compat.texture_top[3] != 1u ||
        !closef(mgl_compat_texture_matrix(&ctx.compat, 3u)[12], 8.0f))
        return fail(8, "selector-free push");
    glMatrixPopEXT(GL_TEXTURE3);
    if (ctx.compat.texture_top[3] != 0u || !identity(mgl_compat_texture_matrix(&ctx.compat, 3u)))
        return fail(9, "selector-free pop");

    glMatrixLoadIdentityEXT(GL_TEXTURE0 + MGL_TEXTURE_UNITS);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(10, "out-of-range texture matrix selector error");

    lists = glGenLists(2);
    if (lists == 0u)
        return fail(11, "glGenLists");

    glMatrixLoadIdentityEXT(GL_TEXTURE4);
    glMatrixLoadIdentityEXT(GL_TEXTURE5);
    glNewList(lists, GL_COMPILE);
    glMatrixLoadIdentityEXT(GL_TEXTURE);
    glMatrixTranslatefEXT(GL_TEXTURE, 1.5f, 0.0f, 0.0f);
    glEndList();
    if (!identity(mgl_compat_texture_matrix(&ctx.compat, 4u)) ||
        !identity(mgl_compat_texture_matrix(&ctx.compat, 5u)))
        return fail(12, "GL_COMPILE executed GL_TEXTURE DSA command");

    glActiveTexture(GL_TEXTURE4);
    glMatrixMode(GL_MODELVIEW);
    glCallList(lists);
    if (!closef(mgl_compat_texture_matrix(&ctx.compat, 4u)[12], 1.5f) ||
        !identity(mgl_compat_texture_matrix(&ctx.compat, 5u)))
        return fail(13, "GL_TEXTURE did not resolve replay-time unit 4");
    if (ctx.compat.active_texture != 4u || ctx.compat.matrix_mode != GL_MODELVIEW)
        return fail(14, "list replay disturbed selectors");

    glActiveTexture(GL_TEXTURE5);
    glCallList(lists);
    if (!closef(mgl_compat_texture_matrix(&ctx.compat, 5u)[12], 1.5f))
        return fail(15, "GL_TEXTURE did not resolve replay-time unit 5");

    memcpy(original, source, sizeof(original));
    glNewList(lists + 1u, GL_COMPILE);
    glMatrixLoadfEXT(GL_TEXTURE3, source);
    glEndList();
    memset(source, 0, sizeof(source));
    glMatrixLoadIdentityEXT(GL_TEXTURE3);
    glActiveTexture(GL_TEXTURE1);
    glMatrixMode(GL_PROJECTION);
    glCallList(lists + 1u);
    m = mgl_compat_texture_matrix(&ctx.compat, 3u);
    if (memcmp(m, original, sizeof(original)) != 0)
        return fail(16, "matrix pointer payload was not captured by list");
    if (ctx.compat.active_texture != 1u || ctx.compat.matrix_mode != GL_PROJECTION)
        return fail(17, "captured matrix replay disturbed selectors");

    if (glXGetProcAddress((const GLubyte *)"glMatrixLoadfEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMatrixMultTransposefEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glLoadTransposeMatrixf") == NULL)
        return fail(18, "proc-address exposure");

    /* EXT DSA matrix commands reject calls during Begin/End before validating other arguments. */
    glMatrixLoadIdentityEXT(GL_PROJECTION);
    glBegin(GL_POINTS);
    glMatrixLoadfEXT(GL_PROJECTION, original);
    glMatrixPushEXT(GL_PROJECTION);
    glMatrixTranslatefEXT(GL_PROJECTION, 8.0f, 1.0f, 2.0f);
    glMatrixRotatefEXT(GL_PROJECTION, 90.0f, 0.0f, 0.0f, 0.0f);
    glEnd();
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || ctx.compat.projection_top != 0u ||
        !identity(mgl_compat_projection(&ctx.compat)))
        return fail(20, "Begin/End matrix DSA mutated projection or stack");

    glBegin(GL_POINTS);
    glMatrixFrustumEXT(GL_PROJECTION, 0., 0., -1., 1., 1., 9.);
    glEnd();
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION ||
        !identity(mgl_compat_projection(&ctx.compat)))
        return fail(21, "Begin/End must precede invalid frustum arguments");
    glBegin(GL_POINTS);
    glMatrixOrthoEXT(GL_PROJECTION, 0., 0., -1., 1., 1., 9.);
    glEnd();
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION ||
        !identity(mgl_compat_projection(&ctx.compat)))
        return fail(22, "Begin/End must precede invalid orthographic arguments");
    glMatrixOrthoEXT(GL_PROJECTION, 0., 0., -1., 1., 1., 9.);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE ||
        !identity(mgl_compat_projection(&ctx.compat)))
        return fail(23, "invalid orthographic dimensions preserve projection");
    glMatrixTranslatefEXT(GL_PROJECTION, 2.f, 3.f, 4.f);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        !closef(mgl_compat_projection(&ctx.compat)[12], 2.f) ||
        !closef(mgl_compat_projection(&ctx.compat)[13], 3.f))
        return fail(24, "legal matrix DSA must recover after errors");

    glDeleteLists(lists, 2);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(19, "unexpected GL error");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL EXT DSA matrix unit test: PASS");
    return 0;
}

#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "state/state.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int closef(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 1.0e-6f;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "normalized color compatibility failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    const GLubyte rgb[3] = {0u, 128u, 255u};
    const GLubyte rgba[4] = {255u, 64u, 0u, 32u};
    GLfloat color[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    memset(&ctx, 0, sizeof(ctx));
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 64, 64);
    mgl_compat_state_init(&ctx.compat);
    mgl_context_set_current_for_test(&ctx);

    glColor3ubv(rgb);
    glGetFloatv(GL_CURRENT_COLOR, color);
    if (!closef(color[0], 0.0f) || !closef(color[1], 128.0f / 255.0f) || !closef(color[2], 1.0f) ||
        !closef(color[3], 1.0f) || glGetError() != GL_NO_ERROR)
        return fail(1, "glColor3ubv normalized conversion");

    glColor4ub(255u, 128u, 64u, 0u);
    glGetFloatv(GL_CURRENT_COLOR, color);
    if (!closef(color[0], 1.0f) || !closef(color[1], 128.0f / 255.0f) ||
        !closef(color[2], 64.0f / 255.0f) || !closef(color[3], 0.0f))
        return fail(2, "glColor4ub normalized conversion");

    glColor4ubv(rgba);
    glGetFloatv(GL_CURRENT_COLOR, color);
    if (!closef(color[0], 1.0f) || !closef(color[1], 64.0f / 255.0f) || !closef(color[2], 0.0f) ||
        !closef(color[3], 32.0f / 255.0f))
        return fail(3, "glColor4ubv normalized conversion");

    glColor4f(0.25f, 0.5f, 0.75f, 1.0f);
    glColor4ubv(NULL);
    glGetFloatv(GL_CURRENT_COLOR, color);
    if (!closef(color[0], 0.25f) || !closef(color[1], 0.5f) || !closef(color[2], 0.75f) ||
        !closef(color[3], 1.0f))
        return fail(4, "NULL vector must not modify current color");

    if (glXGetProcAddress((const GLubyte *)"glColor3ubv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glColor4ub") == NULL ||
        glXGetProcAddress((const GLubyte *)"glColor4ubv") == NULL)
        return fail(5, "proc-address exposure");

    mgl_context_set_current_for_test(NULL);
    puts("mGL normalized color compatibility unit test: PASS");
    return 0;
}

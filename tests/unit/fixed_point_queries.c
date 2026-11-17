#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "state/state.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define FX_ONE ((GLfixed)65536)
#define FX_QUARTER ((GLfixed)16384)
#define FX_HALF ((GLfixed)32768)
#define FX_THREE_QUARTERS ((GLfixed)49152)

static int fail(int code, const char *message) {
    fprintf(stderr, "fixed-point query failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    GLfixed values[16];
    GLfixed sentinel[4] = {101, 102, 103, 104};

    memset(&ctx, 0, sizeof(ctx));
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 320, 240);
    mgl_compat_state_init(&ctx.compat);
    mgl_context_set_current_for_test(&ctx);

    glViewport(3, 5, 17, 19);
    memset(values, 0, sizeof(values));
    glGetFixedv(GL_VIEWPORT, values);
    if (values[0] != 3 * FX_ONE || values[1] != 5 * FX_ONE || values[2] != 17 * FX_ONE ||
        values[3] != 19 * FX_ONE || glGetError() != GL_NO_ERROR)
        return fail(1, "integer state must scale by 2^16");

    glDepthRange(0.25, 0.75);
    glGetFixedv(GL_DEPTH_RANGE, values);
    if (values[0] != FX_QUARTER || values[1] != FX_THREE_QUARTERS || glGetError() != GL_NO_ERROR)
        return fail(2, "floating state must retain fractional fixed-point value");

    glFogi(GL_FOG_MODE, GL_EXP2);
    glGetFixedv(GL_FOG_MODE, values);
    if (values[0] != (GLfixed)GL_EXP2 || glGetError() != GL_NO_ERROR)
        return fail(3, "enumerated state must remain unscaled");

    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);
    glGetFixedv(GL_LIGHT_MODEL_TWO_SIDE, values);
    if (values[0] != FX_ONE || glGetError() != GL_NO_ERROR)
        return fail(4, "boolean state must convert to fixed one");

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.5f, -0.25f, 2.0f);
    memset(values, 0, sizeof(values));
    glGetFixedv(GL_MODELVIEW_MATRIX, values);
    if (values[0] != FX_ONE || values[5] != FX_ONE || values[10] != FX_ONE ||
        values[15] != FX_ONE || values[12] != FX_HALF || values[13] != -FX_QUARTER ||
        values[14] != 2 * FX_ONE || glGetError() != GL_NO_ERROR)
        return fail(5, "matrix query conversion/cardinality");

    glPrimitiveRestartIndex(40000u);
    glGetFixedv(GL_PRIMITIVE_RESTART_INDEX, values);
    if (values[0] != (GLfixed)INT32_MAX || glGetError() != GL_NO_ERROR)
        return fail(6, "fixed query must clamp unrepresentable positive values");

    memcpy(values, sentinel, sizeof(sentinel));
    glGetFixedv(0xffffffffu, values);
    if (glGetError() != GL_INVALID_ENUM || memcmp(values, sentinel, sizeof(sentinel)) != 0)
        return fail(7, "failed query must preserve caller output");

    if (glXGetProcAddress((const GLubyte *)"glGetFixedv") == NULL)
        return fail(8, "proc-address exposure");

    mgl_context_set_current_for_test(NULL);
    puts("mGL fixed-point query unit test: PASS");
    return 0;
}

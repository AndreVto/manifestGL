#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "state/state.h"
#include "texture/texture_store.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define FX_ONE ((GLfixed)65536)
#define FX_HALF ((GLfixed)32768)
#define FX_QUARTER ((GLfixed)16384)
#define FX_TWO ((GLfixed)131072)
#define FX_THREE ((GLfixed)196608)

static int closef(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 1.0e-5f;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "fixed-point parameter failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    GLfixed fog_color[4] = {FX_QUARTER, FX_HALF, FX_ONE, FX_HALF};
    GLfixed border[4] = {FX_QUARTER, FX_HALF, FX_ONE, FX_TWO};
    GLfixed gotx[4] = {0, 0, 0, 0};
    GLfixed sentinel[4] = {101, 102, 103, 104};
    GLfloat gotf[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GLint goti = 0;

    memset(&ctx, 0, sizeof(ctx));
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 320, 240);
    mgl_compat_state_init(&ctx.compat);
    mgl_context_set_current_for_test(&ctx);

    glFogx(GL_FOG_MODE, (GLfixed)GL_EXP2);
    glGetIntegerv(GL_FOG_MODE, &goti);
    if (goti != (GLint)GL_EXP2 || glGetError() != GL_NO_ERROR)
        return fail(1, "fog enum must be unscaled");

    glFogx(GL_FOG_DENSITY, FX_HALF);
    glGetFloatv(GL_FOG_DENSITY, gotf);
    if (!closef(gotf[0], 0.5f) || glGetError() != GL_NO_ERROR)
        return fail(2, "fog numeric fixed conversion");

    glFogxv(GL_FOG_COLOR, fog_color);
    glGetFloatv(GL_FOG_COLOR, gotf);
    if (!closef(gotf[0], 0.25f) || !closef(gotf[1], 0.5f) || !closef(gotf[2], 1.0f) ||
        !closef(gotf[3], 0.5f))
        return fail(3, "fog vector fixed conversion");

    glFogx(GL_FOG_DENSITY, -FX_ONE);
    if (glGetError() != GL_INVALID_VALUE || !closef(ctx.compat.fog_density, 0.5f))
        return fail(4, "fog validation forwarding");

    glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (GLfixed)GL_REPLACE);
    gotx[0] = 0;
    glGetTexEnvxv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, gotx);
    if (gotx[0] != (GLfixed)GL_REPLACE || glGetError() != GL_NO_ERROR)
        return fail(5, "texture-env enum fixed conversion");

    memcpy(gotx, sentinel, sizeof(gotx));
    glGetTexEnvxv(GL_TEXTURE_ENV, 0xffffffffu, gotx);
    if (glGetError() != GL_INVALID_ENUM || memcmp(gotx, sentinel, sizeof(gotx)) != 0)
        return fail(6, "texture-env query output preservation");

    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfixed)GL_LINEAR);
    gotx[0] = 0;
    glGetTexParameterxv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gotx);
    if (gotx[0] != (GLfixed)GL_LINEAR || glGetError() != GL_NO_ERROR)
        return fail(7, "texture enum must be unscaled");

    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, FX_HALF);
    gotx[0] = 0;
    glGetTexParameterxv(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, gotx);
    if (gotx[0] != FX_HALF || glGetError() != GL_NO_ERROR)
        return fail(8, "texture float parameter fixed roundtrip");

    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, FX_THREE);
    gotx[0] = 0;
    glGetTexParameterxv(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, gotx);
    if (gotx[0] != FX_THREE || glGetError() != GL_NO_ERROR)
        return fail(9, "texture integer parameter fixed scaling");

    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, FX_HALF);
    gotx[0] = 0;
    glGetTexParameterxv(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, gotx);
    if (gotx[0] != FX_HALF || glGetError() != GL_NO_ERROR)
        return fail(10, "texture priority fixed roundtrip");

    glTexParameterxv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    memset(gotx, 0, sizeof(gotx));
    glGetTexParameterxv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, gotx);
    if (gotx[0] != FX_QUARTER || gotx[1] != FX_HALF || gotx[2] != FX_ONE || gotx[3] != FX_ONE ||
        glGetError() != GL_NO_ERROR)
        return fail(11, "texture border fixed vector conversion/clamp");

    gotx[0] = 0;
    glGetTexParameterxv(GL_TEXTURE_2D, GL_TEXTURE_RESIDENT, gotx);
    if (gotx[0] != FX_ONE || glGetError() != GL_NO_ERROR)
        return fail(12, "integer query fixed scaling");

    memcpy(gotx, sentinel, sizeof(gotx));
    glGetTexParameterxv(GL_TEXTURE_2D, 0xffffffffu, gotx);
    if (glGetError() != GL_INVALID_ENUM || memcmp(gotx, sentinel, sizeof(gotx)) != 0)
        return fail(13, "texture query output preservation");

    if (glXGetProcAddress((const GLubyte *)"glFogx") == NULL ||
        glXGetProcAddress((const GLubyte *)"glFogxv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTexEnvx") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTexEnvxv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetTexEnvxv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTexParameterx") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTexParameterxv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetTexParameterxv") == NULL)
        return fail(14, "proc-address exposure");

    mgl_context_set_current_for_test(NULL);
    mgl_texture_release_context_defaults(&ctx);
    puts("mGL fixed-point parameter unit test: PASS");
    return 0;
}

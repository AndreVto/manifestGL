#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "state/state.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define FX_ONE ((GLfixed)65536)
#define FX_HALF ((GLfixed)32768)
#define FX_QUARTER ((GLfixed)16384)
#define FX_TWO ((GLfixed)131072)
#define FX_NEG_ONE ((GLfixed) - 65536)

static int closef(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 1.0e-5f;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "fixed-point compatibility failure %d: %s\n", n, message);
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
    GLfloat matrix_x[16];
    GLfloat matrix_f[16];
    GLfixed identity_x[16] = {FX_ONE, 0, 0, 0, 0, FX_ONE, 0, 0, 0, 0, FX_ONE, 0, 0, 0, 0, FX_ONE};
    GLfixed light_diffuse[4] = {FX_QUARTER, FX_HALF, FX_ONE, FX_HALF};
    GLfixed material_diffuse[4] = {FX_HALF, FX_QUARTER, FX_ONE, FX_ONE};
    GLfixed model_ambient[4] = {FX_QUARTER, FX_QUARTER, FX_QUARTER, FX_ONE};
    GLfixed plane[4] = {FX_ONE, 0, 0, 0};
    GLfixed got_x[4] = {0, 0, 0, 0};
    GLfixed sentinel[4] = {11, 12, 13, 14};
    GLfloat got_f[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    memset(&ctx, 0, sizeof(ctx));
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 320, 240);
    mgl_compat_state_init(&ctx.compat);
    mgl_context_set_current_for_test(&ctx);

    glClearColorx(FX_NEG_ONE, FX_HALF, FX_ONE, FX_TWO);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, got_f);
    if (!closef(got_f[0], 0.0f) || !closef(got_f[1], 0.5f) || !closef(got_f[2], 1.0f) ||
        !closef(got_f[3], 1.0f))
        return fail(1, "clampx clear-color conversion");

    glClearDepthx(FX_HALF);
    glDepthRangex(FX_QUARTER, FX_TWO);
    if (!closef(ctx.canonical_state.clear_depth, 0.5f) ||
        !closef(ctx.canonical_state.depth_range[0], 0.25f) ||
        !closef(ctx.canonical_state.depth_range[1], 1.0f))
        return fail(2, "depth fixed conversion");

    glAlphaFuncx(GL_LESS, FX_HALF);
    glSampleCoveragex(FX_QUARTER, GL_TRUE);
    glLineWidthx(FX_TWO);
    glPolygonOffsetx(FX_HALF, FX_NEG_ONE);
    if (!closef(ctx.canonical_state.alpha_ref, 0.5f) ||
        !closef(ctx.canonical_state.sample_coverage_value, 0.25f) ||
        !ctx.canonical_state.sample_coverage_invert ||
        !closef(ctx.canonical_state.line_width, 2.0f) ||
        !closef(ctx.canonical_state.polygon_offset_factor, 0.5f) ||
        !closef(ctx.canonical_state.polygon_offset_units, -1.0f))
        return fail(3, "scalar fixed state conversion");

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixx(identity_x);
    glTranslatex(FX_ONE, FX_TWO, FX_NEG_ONE);
    memcpy(matrix_x, mgl_compat_modelview(&ctx.compat), sizeof(matrix_x));
    glLoadIdentity();
    glTranslatef(1.0f, 2.0f, -1.0f);
    memcpy(matrix_f, mgl_compat_modelview(&ctx.compat), sizeof(matrix_f));
    if (!matrix_eq(matrix_x, matrix_f))
        return fail(4, "fixed translate matrix equivalence");

    glLoadMatrixx(identity_x);
    glScalex(FX_TWO, FX_HALF, FX_ONE);
    glRotatex((GLfixed)(90 * 65536), 0, 0, FX_ONE);
    memcpy(matrix_x, mgl_compat_modelview(&ctx.compat), sizeof(matrix_x));
    glLoadIdentity();
    glScalef(2.0f, 0.5f, 1.0f);
    glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
    memcpy(matrix_f, mgl_compat_modelview(&ctx.compat), sizeof(matrix_f));
    if (!matrix_eq(matrix_x, matrix_f))
        return fail(5, "fixed scale/rotate matrix equivalence");

    identity_x[12] = FX_HALF;
    glLoadIdentity();
    glMultMatrixx(identity_x);
    if (!closef(mgl_compat_modelview(&ctx.compat)[12], 0.5f))
        return fail(6, "glMultMatrixx conversion");
    identity_x[12] = 0;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthox(FX_NEG_ONE, FX_ONE, FX_NEG_ONE, FX_ONE, FX_ONE, (GLfixed)(9 * 65536));
    memcpy(matrix_x, mgl_compat_projection(&ctx.compat), sizeof(matrix_x));
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, 1.0, 9.0);
    memcpy(matrix_f, mgl_compat_projection(&ctx.compat), sizeof(matrix_f));
    if (!matrix_eq(matrix_x, matrix_f))
        return fail(7, "glOrthox equivalence");

    glLoadIdentity();
    glFrustumx(FX_NEG_ONE, FX_ONE, FX_NEG_ONE, FX_ONE, FX_ONE, (GLfixed)(9 * 65536));
    memcpy(matrix_x, mgl_compat_projection(&ctx.compat), sizeof(matrix_x));
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -1.0, 1.0, 1.0, 9.0);
    memcpy(matrix_f, mgl_compat_projection(&ctx.compat), sizeof(matrix_f));
    if (!matrix_eq(matrix_x, matrix_f))
        return fail(8, "glFrustumx equivalence");

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatex(FX_TWO, 0, 0);
    glClipPlanex(GL_CLIP_PLANE0, plane);
    glGetClipPlanex(GL_CLIP_PLANE0, got_x);
    if (got_x[0] != FX_ONE || got_x[1] != 0 || got_x[2] != 0 || got_x[3] != -FX_TWO)
        return fail(9, "fixed clip plane transform/query");

    memcpy(got_x, sentinel, sizeof(got_x));
    glGetClipPlanex(GL_CLIP_PLANE0 + MGL_MAX_CLIP_PLANES, got_x);
    if (glGetError() != GL_INVALID_ENUM || memcmp(got_x, sentinel, sizeof(got_x)) != 0)
        return fail(10, "fixed clip error output preservation");

    glColor4x(FX_QUARTER, FX_HALF, FX_ONE, FX_HALF);
    glGetFloatv(GL_CURRENT_COLOR, got_f);
    if (!closef(got_f[0], 0.25f) || !closef(got_f[1], 0.5f) || !closef(got_f[2], 1.0f) ||
        !closef(got_f[3], 0.5f))
        return fail(11, "fixed current color");
    glNormal3x(0, 0, FX_ONE);
    glGetFloatv(GL_CURRENT_NORMAL, got_f);
    if (!closef(got_f[0], 0.0f) || !closef(got_f[1], 0.0f) || !closef(got_f[2], 1.0f))
        return fail(14, "fixed current normal");
    glMultiTexCoord4x(GL_TEXTURE0, FX_ONE, FX_HALF, FX_QUARTER, FX_ONE);
    glGetFloatv(GL_CURRENT_TEXTURE_COORDS, got_f);
    if (!closef(got_f[0], 1.0f) || !closef(got_f[1], 0.5f) || !closef(got_f[2], 0.25f) ||
        !closef(got_f[3], 1.0f))
        return fail(15, "fixed current texcoord");

    glLightxv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glGetLightxv(GL_LIGHT0, GL_DIFFUSE, got_x);
    if (memcmp(got_x, light_diffuse, sizeof(light_diffuse)) != 0)
        return fail(14, "fixed light vector roundtrip");
    glLightx(GL_LIGHT0, GL_SPOT_EXPONENT, FX_TWO);
    glGetLightxv(GL_LIGHT0, GL_SPOT_EXPONENT, got_x);
    if (got_x[0] != FX_TWO)
        return fail(15, "fixed light scalar roundtrip");

    glLightModelxv(GL_LIGHT_MODEL_AMBIENT, model_ambient);
    glLightModelx(GL_LIGHT_MODEL_TWO_SIDE, FX_ONE);
    if (!closef(ctx.compat.light_model_ambient[0], 0.25f) || !ctx.compat.light_model_two_side)
        return fail(16, "fixed light-model state");
    glLightModelx(GL_LIGHT_MODEL_AMBIENT, FX_HALF);
    if (glGetError() != GL_INVALID_ENUM || !closef(ctx.compat.light_model_ambient[0], 0.25f))
        return fail(17, "fixed scalar light-model cardinality");

    glMaterialxv(GL_FRONT, GL_DIFFUSE, material_diffuse);
    glGetMaterialxv(GL_FRONT, GL_DIFFUSE, got_x);
    if (memcmp(got_x, material_diffuse, sizeof(material_diffuse)) != 0)
        return fail(18, "fixed material vector roundtrip");
    glMaterialx(GL_FRONT, GL_SHININESS, (GLfixed)(32 * 65536));
    glGetMaterialxv(GL_FRONT, GL_SHININESS, got_x);
    if (got_x[0] != (GLfixed)(32 * 65536))
        return fail(19, "fixed material scalar roundtrip");

    memcpy(got_x, sentinel, sizeof(got_x));
    glGetLightxv(GL_LIGHT0, 0xffffffffu, got_x);
    if (glGetError() != GL_INVALID_ENUM || memcmp(got_x, sentinel, sizeof(got_x)) != 0)
        return fail(20, "fixed light query output preservation");
    memcpy(got_x, sentinel, sizeof(got_x));
    glGetMaterialxv(GL_FRONT, 0xffffffffu, got_x);
    if (glGetError() != GL_INVALID_ENUM || memcmp(got_x, sentinel, sizeof(got_x)) != 0)
        return fail(21, "fixed material query output preservation");

    if (glXGetProcAddress((const GLubyte *)"glAlphaFuncx") == NULL ||
        glXGetProcAddress((const GLubyte *)"glClearColorx") == NULL ||
        glXGetProcAddress((const GLubyte *)"glClipPlanex") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetClipPlanex") == NULL ||
        glXGetProcAddress((const GLubyte *)"glFrustumx") == NULL ||
        glXGetProcAddress((const GLubyte *)"glOrthox") == NULL ||
        glXGetProcAddress((const GLubyte *)"glLightxv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetLightxv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMaterialxv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetMaterialxv") == NULL)
        return fail(22, "proc-address exposure");

    mgl_context_set_current_for_test(NULL);
    puts("mGL fixed-point compatibility unit test: PASS");
    return 0;
}

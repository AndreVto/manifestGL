#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(int n, const char *why) {
    fprintf(stderr, "fog-parameter semantics failure %d: %s\n", n, why);
    return n;
}

static int nearf(GLfloat a, GLfloat b) {
    return fabsf(a - b) <= 0.00001f;
}

static GLfloat signed_int_color(GLint value) {
    const double scale = 2.0 * (double)INT_MAX + 1.0;
    return (GLfloat)((2.0 * (double)value + 1.0) / scale);
}

static void begin_fogf_invalid(void) {
    glFogf(0xdeadu, 1.0f);
}

static void begin_fogi_invalid(void) {
    glFogi(GL_FOG_MODE, 0xdead);
}

static void begin_fogfv_invalid(void) {
    const GLfloat v[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    glFogfv(0xdeadu, v);
}

static void begin_fogiv_invalid(void) {
    const GLint v[4] = {1, 2, 3, 4};
    glFogiv(0xdeadu, v);
}

static int expect_invalid_operation_inside_begin(void (*command)(void), int n, const char *why) {
    glBegin(GL_POINTS);
    command();
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(n, why);
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    GLuint list;
    GLfloat f = -1.0f;
    GLfloat color[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    const GLint icolor[4] = {INT_MIN, 0, INT_MAX / 2, INT_MAX};
    GLint mode = 0;
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

    glGetFloatv(GL_FOG_INDEX, &f);
    if (!nearf(f, 0.0f))
        return fail(2, "default fog index");
    glGetIntegerv(GL_FOG_MODE, &mode);
    if (mode != (GLint)GL_EXP)
        return fail(3, "default fog mode");

    glFogf(GL_FOG_INDEX, 7.5f);
    glGetFloatv(GL_FOG_INDEX, &f);
    if (!nearf(f, 7.5f))
        return fail(4, "float fog index");
    glFogi(GL_FOG_INDEX, 4);
    glGetFloatv(GL_FOG_INDEX, &f);
    if (!nearf(f, 4.0f))
        return fail(5, "integer fog index conversion");

    glFogfv(GL_FOG_COLOR, color);
    if (memcmp(ctx.compat.fog_color, color, sizeof(color)) != 0)
        return fail(6, "float fog color canonical state");
    glFogiv(GL_FOG_COLOR, icolor);
    if (!nearf(ctx.compat.fog_color[0], 0.0f) ||
        !nearf(ctx.compat.fog_color[1], signed_int_color(0)) ||
        !nearf(ctx.compat.fog_color[2], signed_int_color(INT_MAX / 2)) ||
        !nearf(ctx.compat.fog_color[3], 1.0f))
        return fail(7, "signed integer fog-color conversion");

    glFogi(GL_FOG_MODE, (GLint)GL_LINEAR);
    glFogf(GL_FOG_MODE, (GLfloat)GL_EXP + 0.25f);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(8, "non-integral float fog mode validation");
    glGetIntegerv(GL_FOG_MODE, &mode);
    if (mode != (GLint)GL_LINEAR)
        return fail(9, "invalid fog mode mutated state");
    glFogi(GL_FOG_COORD_SRC, (GLint)GL_FOG_COORD);
    glFogf(GL_FOG_COORD_SRC, (GLfloat)GL_FOG_COORD + 0.25f);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(10, "non-integral float fog source validation");
    glGetIntegerv(GL_FOG_COORD_SRC, &mode);
    if (mode != (GLint)GL_FOG_COORD)
        return fail(11, "invalid fog source mutated state");

    glFogf(GL_FOG_DENSITY, 0.625f);
    glFogf(GL_FOG_DENSITY, -0.25f);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(12, "negative density validation");
    glGetFloatv(GL_FOG_DENSITY, &f);
    if (!nearf(f, 0.625f))
        return fail(13, "invalid density mutated state");
    glFogf(0xdeadu, 1.0f);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(14, "invalid pname");

    glPushAttrib(GL_FOG_BIT);
    glFogf(GL_FOG_INDEX, 19.0f);
    glFogf(GL_FOG_DENSITY, 0.25f);
    glPopAttrib();
    glGetFloatv(GL_FOG_INDEX, &f);
    if (!nearf(f, 4.0f))
        return fail(15, "fog-index attribute restoration");
    glGetFloatv(GL_FOG_DENSITY, &f);
    if (!nearf(f, 0.625f))
        return fail(16, "fog-density attribute restoration");

    list = glGenLists(1);
    if (list == 0u)
        return fail(17, "display-list allocation");
    glNewList(list, GL_COMPILE);
    glFogi(GL_FOG_MODE, (GLint)GL_EXP2);
    glFogf(GL_FOG_DENSITY, 0.375f);
    glFogf(GL_FOG_INDEX, 11.5f);
    glFogfv(GL_FOG_COLOR, color);
    glEndList();
    glGetIntegerv(GL_FOG_MODE, &mode);
    glGetFloatv(GL_FOG_DENSITY, &f);
    if (mode != (GLint)GL_LINEAR || !nearf(f, 0.625f) || !nearf(ctx.compat.fog_index, 4.0f))
        return fail(18, "GL_COMPILE mutated fog state");

    glCallList(list);
    glGetIntegerv(GL_FOG_MODE, &mode);
    if (mode != (GLint)GL_EXP2)
        return fail(19, "display-list fog mode replay");
    glGetFloatv(GL_FOG_DENSITY, &f);
    if (!nearf(f, 0.375f))
        return fail(20, "display-list fog density replay");
    glGetFloatv(GL_FOG_INDEX, &f);
    if (!nearf(f, 11.5f))
        return fail(21, "display-list fog index replay");
    if (memcmp(ctx.compat.fog_color, color, sizeof(color)) != 0)
        return fail(22, "display-list fog color replay");

    if ((rc = expect_invalid_operation_inside_begin(begin_fogf_invalid, 23,
                                                    "glFogf Begin/End precedence")) != 0)
        return rc;
    if ((rc = expect_invalid_operation_inside_begin(begin_fogi_invalid, 24,
                                                    "glFogi Begin/End precedence")) != 0)
        return rc;
    if ((rc = expect_invalid_operation_inside_begin(begin_fogfv_invalid, 25,
                                                    "glFogfv Begin/End precedence")) != 0)
        return rc;
    if ((rc = expect_invalid_operation_inside_begin(begin_fogiv_invalid, 26,
                                                    "glFogiv Begin/End precedence")) != 0)
        return rc;
    glGetFloatv(GL_FOG_INDEX, &f);
    if (!nearf(f, 11.5f))
        return fail(27, "Begin/End rejection mutated fog state");

    if (glXGetProcAddress((const GLubyte *)"glFogf") == NULL ||
        glXGetProcAddress((const GLubyte *)"glFogi") == NULL ||
        glXGetProcAddress((const GLubyte *)"glFogfv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glFogiv") == NULL)
        return fail(28, "proc-address surface");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL fog-parameter semantics unit test: PASS");
    return 0;
}

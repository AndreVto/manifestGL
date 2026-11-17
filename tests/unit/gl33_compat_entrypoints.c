#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "state/state.h"
#include "compat/fixed_function.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static GLenum logic_last_op;
static unsigned logic_calls;
static unsigned logic_enables;
static unsigned logic_disables;

static const GLubyte *logic_extensions(GLenum name) {
    return name == GL_EXTENSIONS ? (const GLubyte *)"GL_ANGLE_logic_op" : NULL;
}

static const GLubyte *logic_no_extensions(GLenum name) {
    return name == GL_EXTENSIONS ? (const GLubyte *)"GL_EXT_draw_buffers_indexed" : NULL;
}

static void logic_get_extensions_count(GLenum pname, GLint *data) {
    if (pname == GL_NUM_EXTENSIONS)
        *data = 2;
}

static const GLubyte *logic_get_indexed_extension(GLenum name, GLuint index) {
    if (name != GL_EXTENSIONS)
        return NULL;
    return index == 0u   ? (const GLubyte *)"GL_EXT_draw_buffers_indexed"
           : index == 1u ? (const GLubyte *)"GL_ANGLE_logic_op"
                         : NULL;
}

static void logic_op(GLenum op) {
    logic_last_op = op;
    ++logic_calls;
}

static void logic_enable(GLenum cap) {
    if (cap == GL_COLOR_LOGIC_OP)
        ++logic_enables;
}

static void logic_disable(GLenum cap) {
    if (cap == GL_COLOR_LOGIC_OP)
        ++logic_disables;
}

static int closef(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 0.00002f;
}

static int matches(const GLfloat *v, const GLfloat *expected) {
    unsigned i;
    for (i = 0u; i < 4u; ++i)
        if (!closef(v[i], expected[i]))
            return 0;
    return 1;
}

static int fail(int line) {
    fprintf(stderr, "GL 3.3 entrypoint check failed at line %d\n", line);
    return 1;
}

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr))                                                                               \
            return fail(__LINE__);                                                                 \
    } while (0)

int main(void) {
    struct mgl_context ctx;
    GLfloat v[4] = {0.0f};
    const GLfloat expected_secondary[4] = {1.0f, 128.0f / 255.0f, 0.0f, 1.0f};
    const GLfloat expected_color[4] = {1.0f, 512.0f / 1023.0f, 0.0f, 1.0f};
    const GLfloat expected_float_attrib[4] = {1023.0f, 0.0f, 0.0f, 1.0f};
    const GLfloat expected_signed[4] = {-1.0f, 1.0f, 1.0f / 1023.0f, -1.0f};
    const GLfloat expected_signed_color[4] = {-1.0f, 1.0f, 1.0f / 1023.0f, 1.0f / 3.0f};
    const GLfloat expected_single_attrib[4] = {1023.0f, 0.0f, 0.0f, 1.0f};
    const GLubyte secondary[3] = {255u, 128u, 0u};
    const GLbyte integer_attrib[4] = {-128, 11, -5, 127};
    const GLushort unsigned_attrib[4] = {0u, 10u, 65535u, 1u};
    GLint int_value[4] = {0};
    GLuint uint_value[4] = {0u};
    GLuint signed_packed = 0u;
    GLuint packed = 0u;

    memset(&ctx, 0, sizeof(ctx));
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 64, 64);
    mgl_compat_state_init(&ctx.compat);
    mgl_context_set_current_for_test(&ctx);

    glSecondaryColor3ubv(secondary);
    glGetFloatv(GL_CURRENT_SECONDARY_COLOR, v);
    CHECK(matches(v, expected_secondary));
    glSecondaryColor3ubEXT(255u, 128u, 0u);
    glGetFloatv(GL_CURRENT_SECONDARY_COLOR, v);
    CHECK(matches(v, expected_secondary));
    glSecondaryColor3ubvEXT(secondary);
    glGetFloatv(GL_CURRENT_SECONDARY_COLOR, v);
    CHECK(matches(v, expected_secondary));

    glVertexAttribI4bv(2u, integer_attrib);
    glGetVertexAttribIiv(2u, GL_CURRENT_VERTEX_ATTRIB, int_value);
    CHECK(int_value[0] == -128 && int_value[1] == 11 && int_value[2] == -5 && int_value[3] == 127);
    glVertexAttribI4usvEXT(2u, unsigned_attrib);
    glGetVertexAttribIuiv(2u, GL_CURRENT_VERTEX_ATTRIB, uint_value);
    CHECK(uint_value[0] == 0u && uint_value[1] == 10u && uint_value[2] == 65535u &&
          uint_value[3] == 1u);

    packed = 1023u | (512u << 10) | (3u << 30);
    glColorP4ui(GL_UNSIGNED_INT_2_10_10_10_REV, packed);
    glGetFloatv(GL_CURRENT_COLOR, v);
    CHECK(matches(v, expected_color));
    glVertexAttribP1ui(3u, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, packed);
    glGetVertexAttribfv(3u, GL_CURRENT_VERTEX_ATTRIB, v);
    CHECK(matches(v, expected_float_attrib));
    glVertexAttribP4ui(3u, GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, packed);
    glGetVertexAttribfv(3u, GL_CURRENT_VERTEX_ATTRIB, v);
    CHECK(matches(v, expected_color));

    signed_packed = 512u | (511u << 10) | (2u << 30);
    glVertexAttribP4uiv(3u, GL_INT_2_10_10_10_REV, GL_TRUE, &signed_packed);
    glGetVertexAttribfv(3u, GL_CURRENT_VERTEX_ATTRIB, v);
    CHECK(matches(v, expected_signed));
    glColorP4uiv(GL_INT_2_10_10_10_REV, &signed_packed);
    glGetFloatv(GL_CURRENT_COLOR, v);
    CHECK(matches(v, expected_signed));
    glSecondaryColorP3ui(GL_INT_2_10_10_10_REV, signed_packed);
    glGetFloatv(GL_CURRENT_SECONDARY_COLOR, v);
    CHECK(closef(v[0], -1.0f) && closef(v[1], 1.0f) && closef(v[2], 1.0f / 1023.0f));
    glColorP4ui(GL_INT_2_10_10_10_REV, 512u | (511u << 10));
    glGetFloatv(GL_CURRENT_COLOR, v);
    CHECK(matches(v, expected_signed_color));
    glVertexAttribP1uiv(3u, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, &packed);
    glGetVertexAttribfv(3u, GL_CURRENT_VERTEX_ATTRIB, v);
    CHECK(matches(v, expected_single_attrib));
    glNormalP3ui(GL_INT_2_10_10_10_REV, signed_packed);
    glGetFloatv(GL_CURRENT_NORMAL, v);
    CHECK(closef(v[0], -1.0f) && closef(v[1], 1.0f));
    glTexCoordP2ui(GL_UNSIGNED_INT_2_10_10_10_REV, packed);
    glGetFloatv(GL_CURRENT_TEXTURE_COORDS, v);
    CHECK(closef(v[0], 1023.0f) && closef(v[1], 512.0f) && closef(v[2], 0.0f) &&
          closef(v[3], 1.0f));
    glMultiTexCoordP4ui(GL_TEXTURE0, GL_UNSIGNED_INT_2_10_10_10_REV, packed);
    glGetFloatv(GL_CURRENT_TEXTURE_COORDS, v);
    CHECK(closef(v[0], 1023.0f) && closef(v[1], 512.0f) && closef(v[2], 0.0f) &&
          closef(v[3], 3.0f));

    glColorP4ui(GL_UNSIGNED_BYTE, 0u);
    CHECK(glGetError() == GL_INVALID_ENUM);
    glVertexAttribP1ui(3u, GL_FLOAT, GL_TRUE, 1u);
    CHECK(glGetError() == GL_INVALID_ENUM);
    glVertexAttribP1ui(999u, GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, 1u);
    CHECK(glGetError() == GL_INVALID_VALUE);

    glRasterPos2i(0, 0);
    CHECK(glGetError() == GL_NO_ERROR);
    glRasterPos2s(0, 0);
    CHECK(glGetError() == GL_NO_ERROR);
    glRectfv(NULL, NULL);
    CHECK(glGetError() == GL_NO_ERROR);
    glBegin(GL_POINTS);
    glRecti(0, 0, 1, 1);
    glEnd();
    CHECK(glGetError() == GL_INVALID_OPERATION);
    glIndexub(17u);
    glGetFloatv(GL_CURRENT_INDEX, v);
    CHECK(closef(v[0], 17.0f));

    {
        struct mgl_native_dispatch native;
        GLint logic_mode = 0;
        GLfloat point_attenuation[3] = {0.7f, 0.2f, 0.1f};
        GLfloat point_result[3] = {0.0f};
        memset(&native, 0, sizeof(native));
        native.glesGetString = logic_extensions;
        native.glesLogicOpANGLE = logic_op;
        native.glesEnable = logic_enable;
        native.glesDisable = logic_disable;
        ctx.native_dispatch = &native;
        CHECK(ctx.canonical_state.logic_op_mode == GL_COPY);
        glLogicOp(GL_XOR);
        glGetIntegerv(GL_LOGIC_OP_MODE, &logic_mode);
        CHECK(logic_mode == (GLint)GL_XOR && logic_last_op == GL_XOR && logic_calls == 1u);
        glEnable(GL_COLOR_LOGIC_OP);
        CHECK(glIsEnabled(GL_COLOR_LOGIC_OP) == GL_TRUE && logic_enables == 1u);
        glPushAttrib(GL_COLOR_BUFFER_BIT);
        glLogicOp(GL_AND);
        glDisable(GL_COLOR_LOGIC_OP);
        glPopAttrib();
        CHECK(glIsEnabled(GL_COLOR_LOGIC_OP) == GL_TRUE && logic_last_op == GL_XOR &&
              logic_enables == 2u);
        glLogicOp(0xdeadbeefu);
        CHECK(glGetError() == GL_INVALID_ENUM);
        glDisable(GL_COLOR_LOGIC_OP);
        CHECK(logic_disables == 2u && glIsEnabled(GL_COLOR_LOGIC_OP) == GL_FALSE);
        glPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE, 4.5f);
        glGetFloatv(GL_POINT_FADE_THRESHOLD_SIZE, point_result);
        CHECK(point_result[0] == 4.5f);
        glPointParameterfvARB(GL_POINT_DISTANCE_ATTENUATION, point_attenuation);
        glGetFloatv(GL_POINT_DISTANCE_ATTENUATION, point_result);
        CHECK(closef(point_result[0], 0.7f) && closef(point_result[1], 0.2f) &&
              closef(point_result[2], 0.1f));
        glPointParameterfEXT(GL_POINT_SIZE_MAX, 4.0f);
        glGetFloatv(GL_POINT_SIZE_MAX, point_result);
        CHECK(point_result[0] == 4.0f);
        glPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE, -1.0f);
        CHECK(glGetError() == GL_INVALID_VALUE);
        glPointParameterf(GL_POINT_SPRITE_COORD_ORIGIN, (GLfloat)GL_LOWER_LEFT);
        CHECK(ctx.canonical_state.point_sprite_coord_origin == GL_LOWER_LEFT);
        glPointParameterf(GL_POINT_SPRITE_COORD_ORIGIN, 22.0f);
        CHECK(glGetError() == GL_INVALID_ENUM);
        glPointParameterf(GL_POINT_DISTANCE_ATTENUATION, 1.0f);
        CHECK(glGetError() == GL_INVALID_ENUM);
        native.glesGetString = logic_no_extensions;
        glLogicOp(GL_COPY);
        CHECK(logic_calls == 3u && ctx.canonical_state.logic_op_mode == GL_COPY);
        glEnable(GL_COLOR_LOGIC_OP);
        CHECK(logic_enables == 2u && glIsEnabled(GL_COLOR_LOGIC_OP) == GL_TRUE);
        glDisable(GL_COLOR_LOGIC_OP);
        CHECK(logic_disables == 2u);
        native.glesGetIntegerv = logic_get_extensions_count;
        native.glesGetStringi = logic_get_indexed_extension;
        glLogicOp(GL_XOR);
        CHECK(logic_calls == 4u && logic_last_op == GL_XOR);
        ctx.native_dispatch = NULL;
    }

    CHECK(glXGetProcAddress((const GLubyte *)"glVertexAttribP4ui") != NULL);
    CHECK(glXGetProcAddress((const GLubyte *)"glSecondaryColor3ub") != NULL);
    CHECK(glXGetProcAddress((const GLubyte *)"glRectf") != NULL);

    mgl_context_set_current_for_test(NULL);
    puts("mGL GL 3.3 compatibility entrypoints: PASS");
    return 0;
}

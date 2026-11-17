#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

struct capture {
    GLfloat depth_near, depth_far;
    GLfloat blend_color[4];
    GLfloat primitive_bounds[8];
    GLfloat line_width;
    GLfloat polygon_factor, polygon_units;
    GLfloat sample_value;
    GLboolean sample_invert;
    GLint scissor[4];
    GLenum depth_func, cull_face, front_face;
    GLboolean depth_mask, color_mask[4];
    unsigned depth_calls, blend_calls, line_calls, polygon_calls, sample_calls;
    unsigned scissor_calls, depth_func_calls, depth_mask_calls, color_mask_calls, cull_face_calls,
        front_face_calls;
};
static struct capture cap;

static void fake_depth_range(GLfloat n, GLfloat f) {
    cap.depth_near = n;
    cap.depth_far = f;
    ++cap.depth_calls;
}

static void fake_blend_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    cap.blend_color[0] = r;
    cap.blend_color[1] = g;
    cap.blend_color[2] = b;
    cap.blend_color[3] = a;
    ++cap.blend_calls;
}

static void fake_primitive_bounding_box(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
                                        GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW) {
    const GLfloat values[8] = {minX, minY, minZ, minW, maxX, maxY, maxZ, maxW};
    memcpy(cap.primitive_bounds, values, sizeof(values));
}

static void fake_get_float(GLenum pname, GLfloat *data) {
    if (pname == GL_PRIMITIVE_BOUNDING_BOX)
        memcpy(data, cap.primitive_bounds, sizeof(cap.primitive_bounds));
}

static void fake_get_int(GLenum pname, GLint *data) {
    int i;
    if (pname == GL_PRIMITIVE_BOUNDING_BOX)
        for (i = 0; i < 8; ++i)
            data[i] = (GLint)cap.primitive_bounds[i];
}

static void fake_get_int64(GLenum pname, GLint64 *data) {
    int i;
    if (pname == GL_PRIMITIVE_BOUNDING_BOX)
        for (i = 0; i < 8; ++i)
            data[i] = (GLint64)cap.primitive_bounds[i];
}

static void fake_line_width(GLfloat w) {
    cap.line_width = w;
    ++cap.line_calls;
}

static void fake_polygon_offset(GLfloat f, GLfloat u) {
    cap.polygon_factor = f;
    cap.polygon_units = u;
    ++cap.polygon_calls;
}

static void fake_sample_coverage(GLfloat v, GLboolean i) {
    cap.sample_value = v;
    cap.sample_invert = i;
    ++cap.sample_calls;
}

static void fake_scissor(GLint x, GLint y, GLsizei w, GLsizei h) {
    cap.scissor[0] = x;
    cap.scissor[1] = y;
    cap.scissor[2] = w;
    cap.scissor[3] = h;
    ++cap.scissor_calls;
}

static void fake_depth_func(GLenum func) {
    cap.depth_func = func;
    ++cap.depth_func_calls;
}

static void fake_depth_mask(GLboolean flag) {
    cap.depth_mask = flag;
    ++cap.depth_mask_calls;
}

static void fake_color_mask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    cap.color_mask[0] = r;
    cap.color_mask[1] = g;
    cap.color_mask[2] = b;
    cap.color_mask[3] = a;
    ++cap.color_mask_calls;
}

static void fake_cull_face(GLenum mode) {
    cap.cull_face = mode;
    ++cap.cull_face_calls;
}

static void fake_front_face(GLenum mode) {
    cap.front_face = mode;
    ++cap.front_face_calls;
}

static int closef(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 0.0001f;
}

static int fail(int n, const char *why) {
    fprintf(stderr, "extended raster-state failure %d: %s\n", n, why);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLfloat v[4] = {0};
    GLboolean bv = GL_FALSE;
    GLboolean masks[4] = {0};
    GLint iv[4] = {0};
    GLuint list;
    unsigned before[5];
    unsigned basic_before[6];

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesDepthRangef = fake_depth_range;
    native.glesPrimitiveBoundingBox = fake_primitive_bounding_box;
    native.glesGetFloatv = fake_get_float;
    native.glesGetIntegerv = fake_get_int;
    native.glesGetInteger64v = fake_get_int64;
    native.glesBlendColor = fake_blend_color;
    native.glesLineWidth = fake_line_width;
    native.glesPolygonOffset = fake_polygon_offset;
    native.glesSampleCoverage = fake_sample_coverage;
    native.glesScissor = fake_scissor;
    native.glesDepthFunc = fake_depth_func;
    native.glesDepthMask = fake_depth_mask;
    native.glesColorMask = fake_color_mask;
    native.glesCullFace = fake_cull_face;
    native.glesFrontFace = fake_front_face;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 640, 480);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    {
        GLfloat bounds[8] = {0};
        GLdouble double_bounds[8] = {0};
        GLint integer_bounds[8] = {0};
        GLint64 int64_bounds[8] = {0};
        GLboolean boolean_bounds[8] = {0};
        glPrimitiveBoundingBox(-1.5f, -2.0f, -3.0f, 1.0f,
                               2.0f, 3.0f, 4.0f, 1.0f);
        glGetFloatv(GL_PRIMITIVE_BOUNDING_BOX, bounds);
        glGetDoublev(GL_PRIMITIVE_BOUNDING_BOX, double_bounds);
        glGetBooleanv(GL_PRIMITIVE_BOUNDING_BOX, boolean_bounds);
        if (glGetError() != GL_NO_ERROR || !closef(bounds[0], -1.5f) ||
            !closef(bounds[1], -2.0f) || !closef(bounds[6], 4.0f) ||
            double_bounds[0] != -1.5 || !boolean_bounds[0] || !boolean_bounds[7])
            return fail(38, "primitive bounding box floating-point/boolean queries");
        glPrimitiveBoundingBoxARB(-1.0f, -2.0f, -3.0f, 1.0f,
                                  2.0f, 3.0f, 4.0f, 1.0f);
        glGetIntegerv(GL_PRIMITIVE_BOUNDING_BOX, integer_bounds);
        if (glGetError() != GL_NO_ERROR || integer_bounds[0] != -1 ||
            integer_bounds[1] != -2 || integer_bounds[6] != 4)
            return fail(39, "primitive bounding box integer query");
        glGetInteger64v(GL_PRIMITIVE_BOUNDING_BOX, int64_bounds);
        if (glGetError() != GL_NO_ERROR || int64_bounds[0] != -1 ||
            int64_bounds[1] != -2 || int64_bounds[6] != 4 || int64_bounds[7] != 1)
            return fail(41, "primitive bounding box 64-bit query");
        glPrimitiveBoundingBoxEXT(-1.0f, -2.0f, -3.0f, 1.0f,
                                  2.0f, 3.0f, 4.0f, 1.0f);
        glPrimitiveBoundingBoxOES(-1.0f, -2.0f, -3.0f, 1.0f,
                                  2.0f, 3.0f, 4.0f, 1.0f);
        if (glGetError() != GL_NO_ERROR ||
            glXGetProcAddress((const GLubyte *)"glPrimitiveBoundingBox") == NULL ||
            glXGetProcAddress((const GLubyte *)"glPrimitiveBoundingBoxARB") == NULL)
            return fail(40, "primitive bounding box aliases");
    }
    glGetFloatv(GL_DEPTH_RANGE, v);
    if (!closef(v[0], 0.0f) || !closef(v[1], 1.0f))
        return fail(2, "depth defaults");
    glGetFloatv(GL_LINE_WIDTH, v);
    if (!closef(v[0], 1.0f))
        return fail(3, "line default");
    glGetFloatv(GL_SAMPLE_COVERAGE_VALUE, v);
    if (!closef(v[0], 1.0f))
        return fail(4, "sample default");
    glGetBooleanv(GL_SAMPLE_COVERAGE_INVERT, &bv);
    if (bv)
        return fail(5, "sample invert default");

    if (glIsEnabled(GL_MULTISAMPLE) != GL_TRUE)
        return fail(30, "multisample desktop default");
    glDisable(GL_MULTISAMPLE);
    if (glIsEnabled(GL_MULTISAMPLE) != GL_FALSE || glGetError() != GL_NO_ERROR)
        return fail(31, "multisample disable/query");
    glEnable(GL_MULTISAMPLE);
    if (glIsEnabled(GL_MULTISAMPLE) != GL_TRUE || glGetError() != GL_NO_ERROR)
        return fail(32, "multisample enable/query");

    glDepthRange(-2.0, 3.0);
    glBlendColor(-1.0f, 0.25f, 2.0f, 0.75f);
    glLineWidth(2.5f);
    glPolygonOffset(1.5f, -2.0f);
    glSampleCoverage(1.5f, (GLboolean)7);
    if (glGetError() != GL_NO_ERROR)
        return fail(6, "valid setters");
    if (!closef(cap.depth_near, 0.0f) || !closef(cap.depth_far, 1.0f) || cap.depth_calls != 1)
        return fail(7, "depth lowering");
    if (!closef(cap.blend_color[0], 0.0f) || !closef(cap.blend_color[1], 0.25f) ||
        !closef(cap.blend_color[2], 1.0f) || !closef(cap.blend_color[3], 0.75f) ||
        cap.blend_calls != 1)
        return fail(8, "blend lowering");
    if (!closef(cap.line_width, 2.5f) || cap.line_calls != 1)
        return fail(9, "line lowering");
    if (!closef(cap.polygon_factor, 1.5f) || !closef(cap.polygon_units, -2.0f) ||
        cap.polygon_calls != 1)
        return fail(10, "polygon lowering");
    if (!closef(cap.sample_value, 1.0f) || cap.sample_invert != GL_TRUE || cap.sample_calls != 1)
        return fail(11, "sample lowering");

    glLineWidth(0.0f);
    if (glGetError() != GL_INVALID_VALUE || cap.line_calls != 1)
        return fail(12, "line validation");
    glGetFloatv(GL_BLEND_COLOR, v);
    if (!closef(v[0], 0) || !closef(v[1], .25f) || !closef(v[2], 1) || !closef(v[3], .75f))
        return fail(13, "blend query");
    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, v);
    if (!closef(v[0], 1.5f))
        return fail(14, "polygon factor query");
    glGetFloatv(GL_POLYGON_OFFSET_UNITS, v);
    if (!closef(v[0], -2.0f))
        return fail(15, "polygon units query");
    glGetBooleanv(GL_SAMPLE_COVERAGE_INVERT, &bv);
    if (!bv)
        return fail(16, "sample invert query");

    glPushAttrib(GL_VIEWPORT_BIT | GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_POLYGON_BIT |
                 GL_MULTISAMPLE_BIT);
    glDepthRangef(.2f, .8f);
    glBlendColor(.1f, .2f, .3f, .4f);
    glLineWidth(4.0f);
    glPolygonOffset(8.0f, 9.0f);
    glSampleCoverage(.3f, GL_FALSE);
    glDisable(GL_MULTISAMPLE);
    glPopAttrib();
    if (glIsEnabled(GL_MULTISAMPLE) != GL_TRUE || glGetError() != GL_NO_ERROR)
        return fail(33, "multisample attrib restoration");
    glGetFloatv(GL_DEPTH_RANGE, v);
    if (!closef(v[0], 0) || !closef(v[1], 1))
        return fail(17, "depth attrib restore");
    glGetFloatv(GL_LINE_WIDTH, v);
    if (!closef(v[0], 2.5f))
        return fail(18, "line attrib restore");
    glGetFloatv(GL_BLEND_COLOR, v);
    if (!closef(v[1], .25f) || !closef(v[2], 1.0f))
        return fail(19, "blend attrib restore");
    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, v);
    if (!closef(v[0], 1.5f))
        return fail(20, "polygon attrib restore");
    glGetFloatv(GL_SAMPLE_COVERAGE_VALUE, v);
    if (!closef(v[0], 1.0f))
        return fail(21, "sample attrib restore");

    before[0] = cap.depth_calls;
    before[1] = cap.blend_calls;
    before[2] = cap.line_calls;
    before[3] = cap.polygon_calls;
    before[4] = cap.sample_calls;
    list = glGenLists(1);
    if (!list)
        return fail(22, "list name");
    glNewList(list, GL_COMPILE);
    glDepthRange(.4, .6);
    glBlendColor(.2f, .3f, .4f, .5f);
    glLineWidth(3.0f);
    glPolygonOffset(-3.0f, 4.0f);
    glSampleCoverage(.6f, GL_FALSE);
    glEndList();
    if (cap.depth_calls != before[0] || cap.blend_calls != before[1] ||
        cap.line_calls != before[2] || cap.polygon_calls != before[3] ||
        cap.sample_calls != before[4])
        return fail(23, "list compile executed native");
    glGetFloatv(GL_LINE_WIDTH, v);
    if (!closef(v[0], 2.5f))
        return fail(24, "list compile mutated state");
    glCallList(list);
    if (!closef(cap.depth_near, .4f) || !closef(cap.depth_far, .6f) ||
        cap.depth_calls != before[0] + 1)
        return fail(25, "list depth replay");
    if (!closef(cap.line_width, 3.0f) || cap.line_calls != before[2] + 1)
        return fail(26, "list line replay");
    if (!closef(cap.polygon_factor, -3.0f) || !closef(cap.polygon_units, 4.0f) ||
        cap.polygon_calls != before[3] + 1)
        return fail(27, "list polygon replay");
    if (!closef(cap.sample_value, .6f) || cap.sample_invert != GL_FALSE ||
        cap.sample_calls != before[4] + 1)
        return fail(28, "list sample replay");
    glGetFloatv(GL_BLEND_COLOR, v);
    if (!closef(v[0], .2f) || !closef(v[3], .5f))
        return fail(29, "list blend replay");

    glDeleteLists(list, 1);

    glScissor(-3, 4, 320, 200);
    glDepthFunc(GL_GEQUAL);
    glDepthMask((GLboolean)7);
    glColorMask(GL_FALSE, (GLboolean)2, GL_FALSE, (GLboolean)5);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CW);
    if (glGetError() != GL_NO_ERROR)
        return fail(30, "basic raster setters");
    if (cap.scissor_calls == 0u || cap.scissor[0] != -3 || cap.scissor[1] != 4 ||
        cap.scissor[2] != 320 || cap.scissor[3] != 200)
        return fail(31, "scissor lowering");
    if (cap.depth_func != GL_GEQUAL || cap.depth_mask != GL_TRUE || cap.cull_face != GL_FRONT ||
        cap.front_face != GL_CW)
        return fail(32, "basic enum/boolean lowering");
    if (cap.color_mask[0] != GL_FALSE || cap.color_mask[1] != GL_TRUE ||
        cap.color_mask[2] != GL_FALSE || cap.color_mask[3] != GL_TRUE)
        return fail(33, "color mask normalization/lowering");
    glGetIntegerv(GL_SCISSOR_BOX, iv);
    if (iv[0] != -3 || iv[1] != 4 || iv[2] != 320 || iv[3] != 200)
        return fail(34, "scissor query");
    glGetIntegerv(GL_DEPTH_FUNC, iv);
    if (iv[0] != (GLint)GL_GEQUAL)
        return fail(35, "depth func query");
    glGetBooleanv(GL_DEPTH_WRITEMASK, &bv);
    if (bv != GL_TRUE)
        return fail(36, "depth mask query");
    glGetBooleanv(GL_COLOR_WRITEMASK, masks);
    if (masks[0] || !masks[1] || masks[2] || !masks[3])
        return fail(37, "color mask query");
    {
        GLfloat float_masks[5] = {-1.f, -1.f, -1.f, -1.f, -17.f};
        GLdouble double_masks[5] = {-1., -1., -1., -1., -17.};
        GLint64 int64_masks[5] = {-1, -1, -1, -1, -17};
        GLint depth = -1;
        GLfloat depth_float = -1.f;
        GLdouble depth_double = -1.;
        GLint64 depth_int64 = -1;
        GLint color_masks[5] = {-1, -1, -1, -1, -17};
        glGetIntegerv(GL_COLOR_WRITEMASK, color_masks);
        glGetIntegerv(GL_DEPTH_WRITEMASK, &depth);
        if (glGetError() != GL_NO_ERROR || color_masks[0] != 0 || color_masks[1] != 1 ||
            color_masks[2] != 0 || color_masks[3] != 1 || color_masks[4] != -17 || depth != 1)
            return fail(55, "integer write-mask queries");
        glGetFloatv(GL_COLOR_WRITEMASK, float_masks);
        glGetDoublev(GL_COLOR_WRITEMASK, double_masks);
        glGetInteger64v(GL_COLOR_WRITEMASK, int64_masks);
        glGetFloatv(GL_DEPTH_WRITEMASK, &depth_float);
        glGetDoublev(GL_DEPTH_WRITEMASK, &depth_double);
        glGetInteger64v(GL_DEPTH_WRITEMASK, &depth_int64);
        if (glGetError() != GL_NO_ERROR ||
            float_masks[0] != 0.f || float_masks[1] != 1.f ||
            float_masks[2] != 0.f || float_masks[3] != 1.f || float_masks[4] != -17.f ||
            double_masks[0] != 0. || double_masks[1] != 1. ||
            double_masks[2] != 0. || double_masks[3] != 1. || double_masks[4] != -17. ||
            int64_masks[0] != 0 || int64_masks[1] != 1 ||
            int64_masks[2] != 0 || int64_masks[3] != 1 || int64_masks[4] != -17 ||
            depth_float != 1.f || depth_double != 1. || depth_int64 != 1)
            return fail(56, "typed write-mask queries");
    }
    glGetIntegerv(GL_CULL_FACE_MODE, iv);
    if (iv[0] != (GLint)GL_FRONT)
        return fail(38, "cull face query");
    glGetIntegerv(GL_FRONT_FACE, iv);
    if (iv[0] != (GLint)GL_CW)
        return fail(39, "front face query");

    basic_before[0] = cap.scissor_calls;
    basic_before[1] = cap.depth_func_calls;
    basic_before[2] = cap.depth_mask_calls;
    basic_before[3] = cap.color_mask_calls;
    basic_before[4] = cap.cull_face_calls;
    basic_before[5] = cap.front_face_calls;
    glScissor(1, 2, -1, 3);
    if (glGetError() != GL_INVALID_VALUE || cap.scissor_calls != basic_before[0])
        return fail(40, "scissor validation");
    glDepthFunc(0xdeadu);
    if (glGetError() != GL_INVALID_ENUM || cap.depth_func_calls != basic_before[1])
        return fail(41, "depth func validation");
    glCullFace(0xdeadu);
    if (glGetError() != GL_INVALID_ENUM || cap.cull_face_calls != basic_before[4])
        return fail(42, "cull face validation");
    glFrontFace(0xdeadu);
    if (glGetError() != GL_INVALID_ENUM || cap.front_face_calls != basic_before[5])
        return fail(43, "front face validation");

    list = glGenLists(1);
    if (!list)
        return fail(44, "basic-state list name");
    glNewList(list, GL_COMPILE);
    glScissor(10, 11, 12, 13);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEndList();
    if (cap.scissor_calls != basic_before[0] || cap.depth_func_calls != basic_before[1] ||
        cap.depth_mask_calls != basic_before[2] || cap.color_mask_calls != basic_before[3] ||
        cap.cull_face_calls != basic_before[4] || cap.front_face_calls != basic_before[5])
        return fail(45, "basic-state list compile reached native");
    glGetIntegerv(GL_SCISSOR_BOX, iv);
    if (iv[0] != -3 || iv[2] != 320)
        return fail(46, "basic-state list compile mutated state");
    glCallList(list);
    if (cap.scissor_calls != basic_before[0] + 1 || cap.depth_func_calls != basic_before[1] + 1 ||
        cap.depth_mask_calls != basic_before[2] + 1 ||
        cap.color_mask_calls != basic_before[3] + 1 || cap.cull_face_calls != basic_before[4] + 1 ||
        cap.front_face_calls != basic_before[5] + 1)
        return fail(47, "basic-state list replay native counts");
    glGetIntegerv(GL_SCISSOR_BOX, iv);
    if (iv[0] != 10 || iv[1] != 11 || iv[2] != 12 || iv[3] != 13)
        return fail(48, "basic-state list replay scissor");
    glGetIntegerv(GL_DEPTH_FUNC, iv);
    if (iv[0] != (GLint)GL_ALWAYS)
        return fail(49, "basic-state list replay depth func");
    glGetBooleanv(GL_DEPTH_WRITEMASK, &bv);
    if (bv != GL_FALSE)
        return fail(50, "basic-state list replay depth mask");
    glGetBooleanv(GL_COLOR_WRITEMASK, masks);
    if (!masks[0] || masks[1] || !masks[2] || masks[3])
        return fail(51, "basic-state list replay color mask");
    {
        GLint color_masks[4] = {-1, -1, -1, -1};
        GLint depth = -1;
        glGetIntegerv(GL_COLOR_WRITEMASK, color_masks);
        glGetIntegerv(GL_DEPTH_WRITEMASK, &depth);
        if (glGetError() != GL_NO_ERROR || color_masks[0] != 1 || color_masks[1] != 0 ||
            color_masks[2] != 1 || color_masks[3] != 0 || depth != 0)
            return fail(57, "write-mask queries after display-list replay");
    }
    glGetIntegerv(GL_CULL_FACE_MODE, iv);
    if (iv[0] != (GLint)GL_BACK)
        return fail(52, "basic-state list replay cull face");
    glGetIntegerv(GL_FRONT_FACE, iv);
    if (iv[0] != (GLint)GL_CCW)
        return fail(53, "basic-state list replay front face");
    glDeleteLists(list, 1);

    if (glXGetProcAddress((const GLubyte *)"glScissor") == NULL ||
        glXGetProcAddress((const GLubyte *)"glDepthFunc") == NULL ||
        glXGetProcAddress((const GLubyte *)"glDepthMask") == NULL ||
        glXGetProcAddress((const GLubyte *)"glColorMask") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCullFace") == NULL ||
        glXGetProcAddress((const GLubyte *)"glFrontFace") == NULL)
        return fail(54, "basic-state proc-address exposure");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL extended raster-state unit test: PASS");
    return 0;
}

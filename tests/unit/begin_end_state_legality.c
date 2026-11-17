#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static int fail(int n, const char *why) {
    fprintf(stderr, "Begin/End state legality failure %d: %s\n", n, why);
    return n;
}

static void do_depth_range(void) {
    glDepthRange(-1.0, 2.0);
}

static void do_blend_color(void) {
    glBlendColor(-1.0f, 0.2f, 2.0f, 0.4f);
}

static void do_line_width(void) {
    glLineWidth(-1.0f);
}

static void do_polygon_offset(void) {
    glPolygonOffset(1.0f, 2.0f);
}

static void do_sample_coverage(void) {
    glSampleCoverage(0.5f, GL_TRUE);
}

static void do_hint(void) {
    glHint(0xdeadbeefu, 0xdeadbeefu);
}

static void do_alpha_func(void) {
    glAlphaFunc(0xdeadbeefu, 0.5f);
}

static void do_scissor(void) {
    glScissor(0, 0, -1, 1);
}

static void do_depth_func(void) {
    glDepthFunc(0xdeadbeefu);
}

static void do_depth_mask(void) {
    glDepthMask(GL_FALSE);
}

static void do_stencil_func(void) {
    glStencilFunc(0xdeadbeefu, 3, 0xffu);
}

static void do_stencil_func_separate(void) {
    glStencilFuncSeparate(0xdeadbeefu, GL_ALWAYS, 3, 0xffu);
}

static void do_stencil_mask(void) {
    glStencilMask(0x1234u);
}

static void do_stencil_mask_separate(void) {
    glStencilMaskSeparate(0xdeadbeefu, 0x1234u);
}

static void do_stencil_op(void) {
    glStencilOp(0xdeadbeefu, GL_KEEP, GL_KEEP);
}

static void do_stencil_op_separate(void) {
    glStencilOpSeparate(0xdeadbeefu, GL_KEEP, GL_KEEP, GL_KEEP);
}

static void do_color_mask(void) {
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
}

static void do_cull_face(void) {
    glCullFace(0xdeadbeefu);
}

static void do_front_face(void) {
    glFrontFace(0xdeadbeefu);
}

static void do_blend_func(void) {
    glBlendFunc(0xdeadbeefu, GL_ONE);
}

static void do_blend_func_separate(void) {
    glBlendFuncSeparate(0xdeadbeefu, GL_ONE, GL_ONE, GL_ZERO);
}

static void do_blend_equation(void) {
    glBlendEquation(0xdeadbeefu);
}

static void do_blend_equation_separate(void) {
    glBlendEquationSeparate(0xdeadbeefu, GL_FUNC_ADD);
}

static GLfloat map_floats[2] = {0.25f, 0.75f};
static GLuint map_uints[2] = {0u, 4294967295u};
static GLushort map_ushorts[2] = {0u, 65535u};
static GLfloat getter_floats[2] = {-17.0f, -17.0f};
static GLuint getter_uints[2] = {17u, 17u};
static GLushort getter_ushorts[2] = {17u, 17u};

static void do_push_attrib(void) {
    glPushAttrib(GL_LINE_BIT);
}

static void do_pop_attrib(void) {
    glPopAttrib();
}

static void do_push_client_attrib(void) {
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
}

static void do_pop_client_attrib(void) {
    glPopClientAttrib();
}

static void do_pixel_map_f(void) {
    glPixelMapfv(GL_PIXEL_MAP_R_TO_R, 2, map_floats);
}

static void do_pixel_map_ui(void) {
    glPixelMapuiv(GL_PIXEL_MAP_R_TO_R, 2, map_uints);
}

static void do_pixel_map_us(void) {
    glPixelMapusv(GL_PIXEL_MAP_R_TO_R, 2, map_ushorts);
}

static void do_get_pixel_map_f(void) {
    glGetPixelMapfv(GL_PIXEL_MAP_R_TO_R, getter_floats);
}

static void do_get_pixel_map_ui(void) {
    glGetPixelMapuiv(GL_PIXEL_MAP_R_TO_R, getter_uints);
}

static void do_get_pixel_map_us(void) {
    glGetPixelMapusv(GL_PIXEL_MAP_R_TO_R, getter_ushorts);
}

static void do_pixel_transfer_f(void) {
    glPixelTransferf(GL_RED_SCALE, 2.5f);
}

static void do_pixel_transfer_i(void) {
    glPixelTransferi(GL_RED_SCALE, 3);
}

static int expect_invalid_operation(void (*fn)(void), int n, const char *why) {
    glBegin(GL_POINTS);
    fn();
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(n, why);
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_raster_state before;
    struct mgl_pixel_transfer_state pixel_before;

    struct {
        void (*fn)(void);
        const char *name;
    } cases[] = {
        {do_depth_range, "glDepthRange"},
        {do_blend_color, "glBlendColor"},
        {do_line_width, "glLineWidth"},
        {do_polygon_offset, "glPolygonOffset"},
        {do_sample_coverage, "glSampleCoverage"},
        {do_hint, "glHint"},
        {do_alpha_func, "glAlphaFunc"},
        {do_scissor, "glScissor"},
        {do_depth_func, "glDepthFunc"},
        {do_depth_mask, "glDepthMask"},
        {do_stencil_func, "glStencilFunc"},
        {do_stencil_func_separate, "glStencilFuncSeparate"},
        {do_stencil_mask, "glStencilMask"},
        {do_stencil_mask_separate, "glStencilMaskSeparate"},
        {do_stencil_op, "glStencilOp"},
        {do_stencil_op_separate, "glStencilOpSeparate"},
        {do_color_mask, "glColorMask"},
        {do_cull_face, "glCullFace"},
        {do_front_face, "glFrontFace"},
        {do_blend_func, "glBlendFunc"},
        {do_blend_func_separate, "glBlendFuncSeparate"},
        {do_blend_equation, "glBlendEquation"},
        {do_blend_equation_separate, "glBlendEquationSeparate"},
        {do_push_attrib, "glPushAttrib"},
        {do_pop_attrib, "glPopAttrib"},
        {do_push_client_attrib, "glPushClientAttrib"},
        {do_pop_client_attrib, "glPopClientAttrib"},
        {do_pixel_map_f, "glPixelMapfv"},
        {do_pixel_map_ui, "glPixelMapuiv"},
        {do_pixel_map_us, "glPixelMapusv"},
        {do_get_pixel_map_f, "glGetPixelMapfv"},
        {do_get_pixel_map_ui, "glGetPixelMapuiv"},
        {do_get_pixel_map_us, "glGetPixelMapusv"},
        {do_pixel_transfer_f, "glPixelTransferf"},
        {do_pixel_transfer_i, "glPixelTransferi"},
    };

    unsigned i;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
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
    before = ctx.canonical_state;
    pixel_before = ctx.pixel_transfer;

    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        int rc = expect_invalid_operation(cases[i].fn, (int)i + 2, cases[i].name);
        if (rc != 0)
            return rc;
        if (memcmp(&ctx.canonical_state, &before, sizeof(before)) != 0)
            return fail(100 + (int)i, "rejected command mutated raster state");
        if (memcmp(&ctx.pixel_transfer, &pixel_before, sizeof(pixel_before)) != 0 ||
            ctx.attrib_stack_depth != 0u || ctx.client_attrib_stack_depth != 0u)
            return fail(200 + (int)i, "rejected command mutated pixel state or stack depth");
    }

    if (getter_floats[0] != -17.0f || getter_floats[1] != -17.0f || getter_uints[0] != 17u ||
        getter_uints[1] != 17u || getter_ushorts[0] != 17u || getter_ushorts[1] != 17u)
        return fail(300, "rejected pixel-map getters modified caller outputs");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL Begin/End state legality unit test: PASS");
    return 0;
}

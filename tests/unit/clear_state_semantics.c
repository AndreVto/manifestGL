#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

struct capture {
    GLfloat clear_color[4];
    GLfloat clear_depth;
    GLint clear_stencil;
    GLbitfield clear_mask;
    GLint viewport[4];
    unsigned clear_color_calls;
    unsigned clear_depth_calls;
    unsigned clear_stencil_calls;
    unsigned clear_calls;
    unsigned viewport_calls;
} cap;

static int fail(int n, const char *why) {
    fprintf(stderr, "clear-state semantics failure %d: %s\n", n, why);
    return n;
}

static void fake_clear_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    cap.clear_color[0] = r;
    cap.clear_color[1] = g;
    cap.clear_color[2] = b;
    cap.clear_color[3] = a;
    ++cap.clear_color_calls;
}

static void fake_clear_depth(GLfloat depth) {
    cap.clear_depth = depth;
    ++cap.clear_depth_calls;
}

static void fake_clear_stencil(GLint stencil) {
    cap.clear_stencil = stencil;
    ++cap.clear_stencil_calls;
}

static void fake_clear(GLbitfield mask) {
    cap.clear_mask = mask;
    ++cap.clear_calls;
}

static void fake_viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    cap.viewport[0] = x;
    cap.viewport[1] = y;
    cap.viewport[2] = width;
    cap.viewport[3] = height;
    ++cap.viewport_calls;
}

static int expect_invalid_operation_inside_begin(void (*command)(void), int n, const char *why) {
    glBegin(GL_POINTS);
    command();
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(n, why);
    return 0;
}

static void begin_clear_color(void) {
    glClearColor(-1.0f, 0.0f, 1.0f, 2.0f);
}

static void begin_clear_depth(void) {
    glClearDepth(-2.0);
}

static void begin_clear_stencil(void) {
    glClearStencil(3);
}

static void begin_clear(void) {
    glClear(0x1u);
}

static void begin_viewport(void) {
    glViewport(0, 0, -1, 1);
}

static void begin_alpha(void) {
    glAlphaFunc(0xdeadbeefu, 0.5f);
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint list;
    GLfloat fv[4] = {0};
    GLint iv[4] = {0};
    unsigned c0, c1, c2, c3, c4;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesClearColor = fake_clear_color;
    native.glesClearDepthf = fake_clear_depth;
    native.glesClearStencil = fake_clear_stencil;
    native.glesClear = fake_clear;
    native.glesViewport = fake_viewport;
    ctx.native_dispatch = &native;
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

    glClearColor(-0.5f, 0.25f, 1.5f, 0.75f);
    if (cap.clear_color_calls != 1u || cap.clear_color[0] != 0.0f || cap.clear_color[1] != 0.25f ||
        cap.clear_color[2] != 1.0f || cap.clear_color[3] != 0.75f)
        return fail(2, "clear-color native clamp");
    glGetFloatv(GL_COLOR_CLEAR_VALUE, fv);
    if (fv[0] != 0.0f || fv[1] != 0.25f || fv[2] != 1.0f || fv[3] != 0.75f)
        return fail(3, "clear-color canonical clamp");

    glClearDepth(2.0);
    if (cap.clear_depth_calls != 1u || cap.clear_depth != 1.0f ||
        ctx.canonical_state.clear_depth != 1.0f)
        return fail(4, "clear-depth clamp");
    glClearStencil(11);
    if (cap.clear_stencil_calls != 1u || cap.clear_stencil != 11 ||
        ctx.canonical_state.clear_stencil != 11)
        return fail(5, "clear-stencil direct");
    glViewport(3, 4, 500, 600);
    if (cap.viewport_calls != 1u || cap.viewport[0] != 3 || cap.viewport[1] != 4 ||
        cap.viewport[2] != 500 || cap.viewport[3] != 600)
        return fail(6, "viewport direct");
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (cap.clear_calls != 1u || cap.clear_mask != (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT))
        return fail(7, "clear direct");
    glAlphaFunc(GL_GREATER, 2.0f);
    if (ctx.canonical_state.alpha_func != GL_GREATER || ctx.canonical_state.alpha_ref != 1.0f)
        return fail(8, "alpha clamp/direct state");

    glViewport(0, 0, -1, 1);
    if (glGetError() != GL_INVALID_VALUE || cap.viewport_calls != 1u)
        return fail(9, "viewport validation");
    glClear(0x1u);
    if (glGetError() != GL_INVALID_VALUE || cap.clear_calls != 1u)
        return fail(10, "clear mask validation");
    glAlphaFunc(0xdeadbeefu, 0.5f);
    if (glGetError() != GL_INVALID_ENUM || ctx.canonical_state.alpha_func != GL_GREATER)
        return fail(11, "alpha validation");

    list = glGenLists(1);
    if (list == 0u)
        return fail(12, "display-list allocation");
    c0 = cap.clear_color_calls;
    c1 = cap.clear_depth_calls;
    c2 = cap.clear_stencil_calls;
    c3 = cap.clear_calls;
    c4 = cap.viewport_calls;
    glNewList(list, GL_COMPILE);
    glClearColor(0.125f, 0.25f, 0.5f, 1.25f);
    glClearDepth(-1.0);
    glClearStencil(23);
    glClear(GL_STENCIL_BUFFER_BIT);
    glViewport(8, 9, 640, 480);
    glAlphaFunc(GL_LEQUAL, 0.375f);
    glEndList();
    if (cap.clear_color_calls != c0 || cap.clear_depth_calls != c1 ||
        cap.clear_stencil_calls != c2 || cap.clear_calls != c3 || cap.viewport_calls != c4)
        return fail(13, "GL_COMPILE executed native commands");
    if (ctx.canonical_state.clear_color[0] != 0.0f || ctx.canonical_state.clear_depth != 1.0f ||
        ctx.canonical_state.clear_stencil != 11 || ctx.canonical_state.viewport[0] != 3 ||
        ctx.canonical_state.alpha_func != GL_GREATER)
        return fail(14, "GL_COMPILE mutated canonical state");

    glCallList(list);
    if (cap.clear_color_calls != c0 + 1u || cap.clear_depth_calls != c1 + 1u ||
        cap.clear_stencil_calls != c2 + 1u || cap.clear_calls != c3 + 1u ||
        cap.viewport_calls != c4 + 1u)
        return fail(15, "display-list replay calls");
    if (cap.clear_color[0] != 0.125f || cap.clear_color[3] != 1.0f || cap.clear_depth != 0.0f ||
        cap.clear_stencil != 23 || cap.clear_mask != GL_STENCIL_BUFFER_BIT ||
        cap.viewport[0] != 8 || cap.viewport[1] != 9 || cap.viewport[2] != 640 ||
        cap.viewport[3] != 480)
        return fail(16, "display-list replay arguments");
    glGetFloatv(GL_COLOR_CLEAR_VALUE, fv);
    glGetIntegerv(GL_VIEWPORT, iv);
    if (fv[0] != 0.125f || fv[3] != 1.0f || ctx.canonical_state.clear_depth != 0.0f ||
        ctx.canonical_state.clear_stencil != 23 || iv[0] != 8 || iv[1] != 9 || iv[2] != 640 ||
        iv[3] != 480 || ctx.canonical_state.alpha_func != GL_LEQUAL ||
        ctx.canonical_state.alpha_ref != 0.375f)
        return fail(17, "display-list canonical replay state");

    if ((rc = expect_invalid_operation_inside_begin(begin_clear_color, 18,
                                                    "clear color Begin/End legality")) != 0)
        return rc;
    if ((rc = expect_invalid_operation_inside_begin(begin_clear_depth, 19,
                                                    "clear depth Begin/End legality")) != 0)
        return rc;
    if ((rc = expect_invalid_operation_inside_begin(begin_clear_stencil, 20,
                                                    "clear stencil Begin/End legality")) != 0)
        return rc;
    if ((rc = expect_invalid_operation_inside_begin(begin_clear, 21,
                                                    "clear Begin/End legality/order")) != 0)
        return rc;
    if ((rc = expect_invalid_operation_inside_begin(begin_viewport, 22,
                                                    "viewport Begin/End legality/order")) != 0)
        return rc;
    if ((rc = expect_invalid_operation_inside_begin(begin_alpha, 23,
                                                    "alpha Begin/End legality/order")) != 0)
        return rc;

    if (glXGetProcAddress((const GLubyte *)"glClearColor") == NULL ||
        glXGetProcAddress((const GLubyte *)"glClearDepth") == NULL ||
        glXGetProcAddress((const GLubyte *)"glClearDepthf") == NULL ||
        glXGetProcAddress((const GLubyte *)"glClearStencil") == NULL ||
        glXGetProcAddress((const GLubyte *)"glClear") == NULL ||
        glXGetProcAddress((const GLubyte *)"glViewport") == NULL ||
        glXGetProcAddress((const GLubyte *)"glAlphaFunc") == NULL)
        return fail(24, "proc-address surface");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL clear-state semantics unit test: PASS");
    return 0;
}

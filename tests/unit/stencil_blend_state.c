#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

struct stencil_capture {
    unsigned mask_calls;
    unsigned op_calls;
    GLenum mask_face;
    GLuint mask;
    GLenum op_face;
    GLenum sfail, dpfail, dppass;
};
static struct stencil_capture cap;

static void fake_stencil_mask_separate(GLenum face, GLuint mask) {
    ++cap.mask_calls;
    cap.mask_face = face;
    cap.mask = mask;
}

static void fake_stencil_op_separate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    ++cap.op_calls;
    cap.op_face = face;
    cap.sfail = sfail;
    cap.dpfail = dpfail;
    cap.dppass = dppass;
}

static int fail(int n, const char *why) {
    fprintf(stderr, "stencil/blend failure %d: %s\n", n, why);
    return n;
}

static int get_i(GLenum p) {
    GLint v = 0;
    glGetIntegerv(p, &v);
    return v;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint list;
    unsigned mask_calls_before, op_calls_before;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesStencilMaskSeparate = fake_stencil_mask_separate;
    native.glesStencilOpSeparate = fake_stencil_op_separate;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (!ctx.share_group)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 1, 1);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glStencilFuncSeparate(GL_FRONT, GL_LESS, 3, 0x0fu);
    glStencilFuncSeparate(GL_BACK, GL_GREATER, 7, 0xf0u);
    glStencilMaskSeparate(GL_FRONT, 0x33u);
    glStencilMaskSeparate(GL_BACK, 0xccu);
    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR, GL_REPLACE);
    glStencilOpSeparate(GL_BACK, GL_ZERO, GL_DECR_WRAP, GL_INVERT);
    if (glGetError() != GL_NO_ERROR)
        return fail(2, "initial stencil error");
    if (get_i(GL_STENCIL_FUNC) != (GLint)GL_LESS ||
        get_i(GL_STENCIL_BACK_FUNC) != (GLint)GL_GREATER)
        return fail(3, "front/back funcs");
    if (get_i(GL_STENCIL_REF) != 3 || get_i(GL_STENCIL_BACK_REF) != 7)
        return fail(4, "front/back refs");
    if ((GLuint)get_i(GL_STENCIL_WRITEMASK) != 0x33u ||
        (GLuint)get_i(GL_STENCIL_BACK_WRITEMASK) != 0xccu)
        return fail(5, "write masks");
    if (get_i(GL_STENCIL_PASS_DEPTH_FAIL) != (GLint)GL_INCR ||
        get_i(GL_STENCIL_BACK_PASS_DEPTH_FAIL) != (GLint)GL_DECR_WRAP)
        return fail(6, "ops");

    glPushAttrib(GL_STENCIL_BUFFER_BIT);
    glStencilFunc(GL_ALWAYS, 1, 0xffu);
    glStencilMask(0xffu);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glPopAttrib();
    if (get_i(GL_STENCIL_FUNC) != (GLint)GL_LESS ||
        get_i(GL_STENCIL_BACK_FUNC) != (GLint)GL_GREATER)
        return fail(7, "push/pop funcs");
    if ((GLuint)get_i(GL_STENCIL_WRITEMASK) != 0x33u ||
        (GLuint)get_i(GL_STENCIL_BACK_WRITEMASK) != 0xccu)
        return fail(8, "push/pop masks");

    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_REVERSE_SUBTRACT);
    if (get_i(GL_BLEND_SRC_RGB) != (GLint)GL_SRC_ALPHA ||
        get_i(GL_BLEND_SRC_ALPHA) != (GLint)GL_ONE)
        return fail(9, "blend factors");
    if (get_i(GL_BLEND_EQUATION_RGB) != (GLint)GL_FUNC_ADD ||
        get_i(GL_BLEND_EQUATION_ALPHA) != (GLint)GL_FUNC_REVERSE_SUBTRACT)
        return fail(10, "blend equations");
    glPushAttrib(GL_COLOR_BUFFER_BIT);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_SUBTRACT);
    glPopAttrib();
    if (get_i(GL_BLEND_SRC_RGB) != (GLint)GL_SRC_ALPHA ||
        get_i(GL_BLEND_SRC_ALPHA) != (GLint)GL_ONE)
        return fail(11, "blend push/pop");
    if (get_i(GL_BLEND_EQUATION_ALPHA) != (GLint)GL_FUNC_REVERSE_SUBTRACT)
        return fail(12, "equation push/pop");

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glStencilFuncSeparate(GL_FRONT, GL_GEQUAL, 11, 0xaau);
    glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT);
    glEndList();
    if (get_i(GL_STENCIL_FUNC) != (GLint)GL_LESS ||
        get_i(GL_BLEND_EQUATION_RGB) != (GLint)GL_FUNC_ADD)
        return fail(13, "list compiled state executed early");
    glCallList(list);
    if (get_i(GL_STENCIL_FUNC) != (GLint)GL_GEQUAL || get_i(GL_STENCIL_REF) != 11)
        return fail(14, "list stencil replay");
    if (get_i(GL_BLEND_EQUATION_RGB) != (GLint)GL_FUNC_REVERSE_SUBTRACT ||
        get_i(GL_BLEND_EQUATION_ALPHA) != (GLint)GL_FUNC_SUBTRACT)
        return fail(15, "list blend replay");

    glStencilMask(0x5au);
    if (glGetError() != GL_NO_ERROR || cap.mask_face != GL_FRONT_AND_BACK || cap.mask != 0x5au)
        return fail(16, "scalar stencil mask lowering");
    if ((GLuint)get_i(GL_STENCIL_WRITEMASK) != 0x5au ||
        (GLuint)get_i(GL_STENCIL_BACK_WRITEMASK) != 0x5au)
        return fail(17, "scalar stencil mask canonical state");

    glStencilOp(GL_INCR_WRAP, GL_DECR, GL_REPLACE);
    if (glGetError() != GL_NO_ERROR || cap.op_face != GL_FRONT_AND_BACK ||
        cap.sfail != GL_INCR_WRAP || cap.dpfail != GL_DECR || cap.dppass != GL_REPLACE)
        return fail(18, "scalar stencil op lowering");
    if (get_i(GL_STENCIL_FAIL) != (GLint)GL_INCR_WRAP ||
        get_i(GL_STENCIL_BACK_FAIL) != (GLint)GL_INCR_WRAP ||
        get_i(GL_STENCIL_PASS_DEPTH_FAIL) != (GLint)GL_DECR ||
        get_i(GL_STENCIL_BACK_PASS_DEPTH_PASS) != (GLint)GL_REPLACE)
        return fail(19, "scalar stencil op canonical state");

    mask_calls_before = cap.mask_calls;
    glStencilMaskSeparate(0xdeadu, 0u);
    if (glGetError() != GL_INVALID_ENUM || cap.mask_calls != mask_calls_before ||
        (GLuint)get_i(GL_STENCIL_WRITEMASK) != 0x5au ||
        (GLuint)get_i(GL_STENCIL_BACK_WRITEMASK) != 0x5au)
        return fail(20, "invalid stencil mask face mutated state/native");

    op_calls_before = cap.op_calls;
    glStencilOpSeparate(GL_FRONT, 0xdeadu, GL_KEEP, GL_KEEP);
    if (glGetError() != GL_INVALID_ENUM || cap.op_calls != op_calls_before ||
        get_i(GL_STENCIL_FAIL) != (GLint)GL_INCR_WRAP)
        return fail(21, "invalid stencil op mutated state/native");

    if (glXGetProcAddress((const GLubyte *)"glStencilMask") == NULL ||
        glXGetProcAddress((const GLubyte *)"glStencilMaskSeparate") == NULL ||
        glXGetProcAddress((const GLubyte *)"glStencilOp") == NULL ||
        glXGetProcAddress((const GLubyte *)"glStencilOpSeparate") == NULL)
        return fail(22, "stencil mask/op proc-address exposure");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL stencil/blend state unit test: PASS");
    return 0;
}

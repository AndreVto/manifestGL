#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "shader/shader_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    int dual_source_extension;
    unsigned blend_func_calls;
    unsigned blend_func_i_calls;
    unsigned blend_equation_calls;
    GLenum scalar_equation, indexed_equation;
    unsigned barrier_calls;
    GLenum src_rgb;
    GLenum dst_rgb;
    GLenum src_alpha;
    GLenum dst_alpha;
    GLenum eq_rgb;
    GLenum eq_alpha;
    unsigned draw_calls;
    GLuint next_buffer;
} cap;

static const GLubyte *fake_get_string(GLenum name) {
    static const GLubyte with_ext[] = "GL_EXT_blend_func_extended GL_EXT_mock";
    static const GLubyte without_ext[] = "GL_EXT_mock";
    if (name != GL_EXTENSIONS)
        return (const GLubyte *)"";
    return cap.dual_source_extension ? with_ext : without_ext;
}

static void fake_blend_func_separate(GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha,
                                     GLenum dst_alpha) {
    ++cap.blend_func_calls;
    cap.src_rgb = src_rgb;
    cap.dst_rgb = dst_rgb;
    cap.src_alpha = src_alpha;
    cap.dst_alpha = dst_alpha;
}

static void fake_blend_func_separate_i(GLuint index, GLenum src_rgb, GLenum dst_rgb,
                                       GLenum src_alpha, GLenum dst_alpha) {
    (void)index;
    ++cap.blend_func_i_calls;
    cap.src_rgb = src_rgb;
    cap.dst_rgb = dst_rgb;
    cap.src_alpha = src_alpha;
    cap.dst_alpha = dst_alpha;
}

static void fake_blend_barrier(void) {
    ++cap.barrier_calls;
}

static void fake_blend_equation(GLenum mode) {
    cap.scalar_equation = mode;
}

static void fake_blend_equation_i(GLuint index, GLenum mode) {
    if (index == 1u)
        cap.indexed_equation = mode;
}

static void fake_blend_equation_separate(GLenum rgb, GLenum alpha) {
    ++cap.blend_equation_calls;
    cap.eq_rgb = rgb;
    cap.eq_alpha = alpha;
}

static void fake_use_program(GLuint program) {
    (void)program;
}

static void fake_vertex_attrib_pointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                       GLsizei stride, const void *pointer) {
    (void)index;
    (void)size;
    (void)type;
    (void)normalized;
    (void)stride;
    (void)pointer;
}

static void fake_disable_vertex_attrib(GLuint index) {
    (void)index;
}

static void fake_bind_buffer(GLenum target, GLuint buffer) {
    (void)target;
    (void)buffer;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)target;
    (void)size;
    (void)data;
    (void)usage;
}

static void fake_gen_buffers(GLsizei n, GLuint *buffers) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        buffers[i] = ++cap.next_buffer;
}

static void fake_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    (void)mode;
    (void)first;
    (void)count;
    ++cap.draw_calls;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "blend-extended failure %d: %s\n", code, message);
    return code;
}

static int expect_error(GLenum expected, int code, const char *message) {
    GLenum actual = glGetError();
    if (actual != expected) {
        fprintf(stderr, "blend-extended failure %d: %s (got 0x%04x expected 0x%04x)\n", code,
                message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    struct mgl_framebuffer *fb;
    GLint value = 0;
    unsigned i;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));

    native.glesGetString = fake_get_string;
    native.glesBlendFuncSeparate = fake_blend_func_separate;
    native.glesBlendFuncSeparatei = fake_blend_func_separate_i;
    native.glesBlendEquationSeparate = fake_blend_equation_separate;
    native.glesBlendEquation = fake_blend_equation;
    native.glesBlendEquationi = fake_blend_equation_i;
    native.glesBlendBarrier = fake_blend_barrier;
    native.glesUseProgram = fake_use_program;
    native.glesVertexAttribPointer = fake_vertex_attrib_pointer;
    native.glesDisableVertexAttribArray = fake_disable_vertex_attrib;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDrawArrays = fake_draw_arrays;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 1, 1);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.default_draw_buffers[0] = GL_BACK;
    for (i = 1u; i < MGL_MAX_DRAW_BUFFERS; ++i)
        ctx.default_draw_buffers[i] = GL_NONE;

    program = calloc(1u, sizeof(*program));
    if (program == NULL)
        return fail(2, "program allocation");
    program->name = 7u;
    program->native_name = 70u;
    program->linked = GL_TRUE;
    for (i = 0u; i < MGL_COMPAT_SEMANTIC_COUNT; ++i)
        program->compat_attrib[i] = -1;
    program->next = ctx.share_group->programs;
    ctx.share_group->programs = program;
    ctx.current_program = program->name;
    mgl_context_set_current_for_test(&ctx);

    glBlendFuncSeparate(GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR, GL_CONSTANT_ALPHA,
                        GL_ONE_MINUS_CONSTANT_ALPHA);
    if ((rc = expect_error(GL_NO_ERROR, 3, "constant blend factors")) != 0)
        return rc;
    if (cap.blend_func_calls != 1u || cap.src_rgb != GL_CONSTANT_COLOR ||
        cap.dst_alpha != GL_ONE_MINUS_CONSTANT_ALPHA)
        return fail(4, "constant blend factor lowering");

    glBlendEquationSeparate(GL_MIN, GL_MAX);
    if ((rc = expect_error(GL_NO_ERROR, 5, "min/max blend equations")) != 0)
        return rc;
    if (cap.blend_equation_calls != 1u || cap.eq_rgb != GL_MIN || cap.eq_alpha != GL_MAX)
        return fail(6, "min/max equation lowering");

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if ((rc = expect_error(GL_NO_ERROR, 23, "scalar blend function")) != 0)
        return rc;
    if (cap.blend_func_calls != 2u || cap.src_rgb != GL_SRC_ALPHA ||
        cap.dst_rgb != GL_ONE_MINUS_SRC_ALPHA || cap.src_alpha != GL_SRC_ALPHA ||
        cap.dst_alpha != GL_ONE_MINUS_SRC_ALPHA)
        return fail(24, "scalar blend lowering/state expansion");
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)GL_SRC_ALPHA)
        return fail(25, "scalar blend query");

    glBlendFuncSeparate(0xdeadu, GL_ONE, GL_ONE, GL_ZERO);
    if ((rc = expect_error(GL_INVALID_ENUM, 26, "invalid separate blend factor")) != 0 ||
        cap.blend_func_calls != 2u)
        return fail(26, "invalid separate blend factor reached native");
    glGetIntegerv(GL_BLEND_SRC_RGB, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)GL_SRC_ALPHA)
        return fail(27, "invalid separate blend factor mutated state");

    glBlendFunc(GL_ONE, 0xdeadu);
    if ((rc = expect_error(GL_INVALID_ENUM, 28, "invalid scalar blend factor")) != 0 ||
        cap.blend_func_calls != 2u)
        return fail(28, "invalid scalar blend factor reached native");

    cap.dual_source_extension = 0;
    glBlendFuncSeparate(GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR, GL_SRC1_ALPHA,
                        GL_ONE_MINUS_SRC1_ALPHA);
    if ((rc = expect_error(GL_NO_ERROR, 7, "canonical SRC1 state without native extension")) != 0)
        return rc;
    if (cap.blend_func_calls != 2u)
        return fail(8, "unsupported SRC1 state leaked to GLES");
    glGetIntegerv(GL_BLEND_SRC_RGB, &value);
    if (value != (GLint)GL_SRC1_COLOR)
        return fail(9, "SRC1 canonical query");

    glEnable(GL_BLEND);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if ((rc = expect_error(GL_INVALID_OPERATION, 10, "unsupported SRC1 draw")) != 0)
        return rc;
    if (cap.draw_calls != 0u)
        return fail(11, "unsupported SRC1 reached native draw");

    cap.dual_source_extension = 1;
    glBlendFuncSeparate(GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR, GL_SRC1_ALPHA,
                        GL_ONE_MINUS_SRC1_ALPHA);
    if ((rc = expect_error(GL_NO_ERROR, 12, "supported SRC1 state")) != 0)
        return rc;
    if (cap.blend_func_calls != 3u || cap.src_alpha != GL_SRC1_ALPHA)
        return fail(13, "supported SRC1 lowering");

    glDrawArrays(GL_TRIANGLES, 0, 3);
    if ((rc = expect_error(GL_NO_ERROR, 14, "single-buffer SRC1 draw")) != 0)
        return rc;
    if (cap.draw_calls != 1u)
        return fail(15, "supported SRC1 draw did not reach native");

    glBlendFuncSeparatei(0u, GL_SRC1_ALPHA, GL_ONE, GL_ONE, GL_ZERO);
    if ((rc = expect_error(GL_NO_ERROR, 16, "indexed SRC1 state")) != 0)
        return rc;
    if (cap.blend_func_i_calls != 1u)
        return fail(17, "indexed SRC1 lowering");
    value = 0;
    glGetIntegeri_v(GL_BLEND_SRC_RGB, 0u, &value);
    if (value != (GLint)GL_SRC1_ALPHA)
        return fail(18, "indexed SRC1 query");

    fb = mgl_framebuffer_ensure(&ctx, 9u, 0);
    if (fb == NULL)
        return fail(19, "framebuffer allocation");
    fb->draw_buffers[0] = GL_COLOR_ATTACHMENT0;
    fb->draw_buffers[1] = GL_COLOR_ATTACHMENT1;
    ctx.draw_framebuffer = fb->name;
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if ((rc = expect_error(GL_INVALID_OPERATION, 20, "dual-source multi-buffer draw limit")) != 0)
        return rc;
    if (cap.draw_calls != 1u)
        return fail(21, "draw-buffer limit reached native draw");

    glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
    glBlendFuncSeparatei(0u, GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
    glBlendEquation(GL_SCREEN_KHR);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)GL_SCREEN_KHR ||
        cap.scalar_equation != GL_SCREEN_KHR)
        return fail(30, "advanced blend equation and query");
    {
        const GLenum advanced_modes[] = {
            GL_MULTIPLY_KHR, GL_SCREEN_KHR, GL_OVERLAY_KHR, GL_DARKEN_KHR,
            GL_LIGHTEN_KHR, GL_COLORDODGE_KHR, GL_COLORBURN_KHR,
            GL_HARDLIGHT_KHR, GL_SOFTLIGHT_KHR, GL_DIFFERENCE_KHR,
            GL_EXCLUSION_KHR, GL_HSL_HUE_KHR, GL_HSL_SATURATION_KHR,
            GL_HSL_COLOR_KHR, GL_HSL_LUMINOSITY_KHR
        };
        unsigned mode_index;
        for (mode_index = 0; mode_index < sizeof(advanced_modes) / sizeof(advanced_modes[0]);
             ++mode_index) {
            glBlendEquation(advanced_modes[mode_index]);
            glGetIntegerv(GL_BLEND_EQUATION_RGB, &value);
            if (glGetError() != GL_NO_ERROR || value != (GLint)advanced_modes[mode_index] ||
                cap.scalar_equation != advanced_modes[mode_index])
                return fail(41, "core advanced blend equation coverage");
        }
        glBlendEquation(GL_SCREEN_KHR);
    }
    glBlendEquationi(1u, GL_MULTIPLY_KHR);
    glGetIntegeri_v(GL_BLEND_EQUATION_RGB, 1u, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)GL_MULTIPLY_KHR ||
        cap.indexed_equation != GL_MULTIPLY_KHR)
        return fail(31, "indexed advanced blend equation");
    glBlendEquationSeparate(GL_SCREEN_KHR, GL_SCREEN_KHR);
    if (glGetError() != GL_INVALID_ENUM || cap.scalar_equation != GL_SCREEN_KHR)
        return fail(32, "advanced mode cannot be applied to separate equation");
    glBlendEquationSeparatei(1u, GL_MULTIPLY_KHR, GL_MULTIPLY_KHR);
    if (glGetError() != GL_INVALID_ENUM || cap.indexed_equation != GL_MULTIPLY_KHR)
        return fail(33, "indexed separate mode validation");
    glBlendBarrier();
    glBlendBarrierKHR();
    if (glGetError() != GL_NO_ERROR || cap.barrier_calls != 2u)
        return fail(34, "advanced blend barrier");
    ctx.draw_framebuffer = fb->name;
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (glGetError() != GL_INVALID_OPERATION || cap.draw_calls != 1u)
        return fail(35, "advanced blending with multiple active draw buffers");
    ctx.draw_framebuffer = 0u;
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (glGetError() != GL_NO_ERROR || cap.draw_calls != 2u)
        return fail(36, "single-target advanced blending draw");
    {
        GLuint list = glGenLists(1);
        if (list == 0u)
            return fail(37, "advanced blend list creation");
        glNewList(list, GL_COMPILE);
        glBlendEquation(GL_DIFFERENCE_KHR);
        glBlendEquationi(1u, GL_OVERLAY_KHR);
        glEndList();
        if (glGetError() != GL_NO_ERROR || cap.scalar_equation != GL_SCREEN_KHR ||
            cap.indexed_equation != GL_MULTIPLY_KHR)
            return fail(38, "advanced blend list compile semantics");
        glCallList(list);
        if (glGetError() != GL_NO_ERROR || cap.scalar_equation != GL_DIFFERENCE_KHR ||
            cap.indexed_equation != GL_OVERLAY_KHR)
            return fail(39, "advanced blend list replay");
        glDeleteLists(list, 1);
    }
    if (glXGetProcAddress((const GLubyte *)"glBlendBarrier") == NULL ||
        glXGetProcAddress((const GLubyte *)"glBlendBarrierKHR") == NULL ||
        glXGetProcAddress((const GLubyte *)"glBlendFunc") == NULL ||
        glXGetProcAddress((const GLubyte *)"glBlendFuncSeparate") == NULL)
        return fail(29, "blend proc-address exposure");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL extended blend unit test: PASS");
    return 0;
}

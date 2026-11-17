#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

struct clear_capture {
    GLuint next_fb;
    GLuint draw_fb;
    GLuint read_fb;
    GLenum last_buffer;
    GLint last_drawbuffer;
    GLint iv[4];
    GLuint uiv[4];
    GLfloat fv[4];
    GLfloat depth;
    GLint stencil;
    unsigned iv_calls;
    unsigned uiv_calls;
    unsigned fv_calls;
    unsigned fi_calls;
    int force_incomplete;
} cap;

static int fail(int n, const char *why) {
    fprintf(stderr, "clear-buffer failure %d: %s\n", n, why);
    return n;
}

static void fake_gen_fb(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_fb;
}

static void fake_del_fb(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_fb(GLenum target, GLuint name) {
    if (target == GL_FRAMEBUFFER) {
        cap.draw_fb = name;
        cap.read_fb = name;
    } else if (target == GL_DRAW_FRAMEBUFFER)
        cap.draw_fb = name;
    else if (target == GL_READ_FRAMEBUFFER)
        cap.read_fb = name;
}

static GLenum fake_check_fb(GLenum target) {
    (void)target;
    return cap.force_incomplete ? GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT : GL_FRAMEBUFFER_COMPLETE;
}

static void fake_clear_iv(GLenum buffer, GLint drawbuffer, const GLint *value) {
    cap.last_buffer = buffer;
    cap.last_drawbuffer = drawbuffer;
    memcpy(cap.iv, value, (buffer == GL_COLOR ? 4u : 1u) * sizeof(GLint));
    ++cap.iv_calls;
}

static void fake_clear_uiv(GLenum buffer, GLint drawbuffer, const GLuint *value) {
    cap.last_buffer = buffer;
    cap.last_drawbuffer = drawbuffer;
    memcpy(cap.uiv, value, 4u * sizeof(GLuint));
    ++cap.uiv_calls;
}

static void fake_clear_fv(GLenum buffer, GLint drawbuffer, const GLfloat *value) {
    cap.last_buffer = buffer;
    cap.last_drawbuffer = drawbuffer;
    memcpy(cap.fv, value, (buffer == GL_COLOR ? 4u : 1u) * sizeof(GLfloat));
    ++cap.fv_calls;
}

static void fake_clear_fi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    cap.last_buffer = buffer;
    cap.last_drawbuffer = drawbuffer;
    cap.depth = depth;
    cap.stencil = stencil;
    ++cap.fi_calls;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_framebuffer *fb0, *fb1, *fb2;
    GLuint fbs[3] = {0u, 0u, 0u};
    GLfloat color[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    GLfloat list_color[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    GLint stencil_value[1] = {7};
    GLuint uint_color[4] = {1u, 2u, 3u, 4u};
    GLuint list;
    unsigned before;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.next_fb = 1000u;
    native.glesGenFramebuffers = fake_gen_fb;
    native.glesDeleteFramebuffers = fake_del_fb;
    native.glesBindFramebuffer = fake_bind_fb;
    native.glesCheckFramebufferStatus = fake_check_fb;
    native.glesClearBufferiv = fake_clear_iv;
    native.glesClearBufferuiv = fake_clear_uiv;
    native.glesClearBufferfv = fake_clear_fv;
    native.glesClearBufferfi = fake_clear_fi;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 1, 1);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glCreateFramebuffers(3, fbs);
    fb0 = mgl_framebuffer_find(&ctx, fbs[0]);
    fb1 = mgl_framebuffer_find(&ctx, fbs[1]);
    fb2 = mgl_framebuffer_find(&ctx, fbs[2]);
    if (fb0 == NULL || fb1 == NULL || fb2 == NULL)
        return fail(2, "framebuffer creation");
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbs[0]);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbs[1]);

    glClearColor(0.8f, 0.7f, 0.6f, 0.5f);
    glClearDepth(0.35);
    glClearStencil(19);
    glClearBufferfv(GL_COLOR, 2, color);
    if (cap.fv_calls != 1u || cap.last_buffer != GL_COLOR || cap.last_drawbuffer != 2 ||
        cap.fv[2] != 0.3f)
        return fail(3, "direct color clear");
    if (ctx.canonical_state.clear_color[0] != 0.8f || ctx.canonical_state.clear_depth != 0.35f ||
        ctx.canonical_state.clear_stencil != 19)
        return fail(4, "clear state mutated");
    glClearBufferiv(GL_STENCIL, 0, stencil_value);
    glClearBufferuiv(GL_COLOR, 1, uint_color);
    glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.6f, 23);
    if (cap.iv_calls != 1u || cap.uiv_calls != 1u || cap.fi_calls != 1u || cap.depth != 0.6f ||
        cap.stencil != 23)
        return fail(5, "typed clear variants");

    glClearBufferfv(GL_DEPTH, 1, color);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(6, "depth drawbuffer validation");
    glClearBufferuiv(GL_STENCIL, 0, uint_color);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(7, "variant buffer validation");
    glClearBufferfv(GL_COLOR, (GLint)MGL_MAX_DRAW_BUFFERS, color);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(8, "color drawbuffer range");

    list = glGenLists(1);
    before = cap.fv_calls;
    glNewList(list, GL_COMPILE);
    glClearBufferfv(GL_COLOR, 3, list_color);
    glEndList();
    if (cap.fv_calls != before)
        return fail(9, "list clear executed at compile");
    list_color[0] = 9.0f;
    list_color[1] = 9.0f;
    list_color[2] = 9.0f;
    list_color[3] = 9.0f;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbs[2]);
    glCallList(list);
    if (cap.fv_calls != before + 1u || cap.draw_fb != fb2->native_name || cap.fv[0] != 0.25f ||
        cap.fv[2] != 0.75f)
        return fail(10, "list capture/replay-time fbo");

    cap.force_incomplete = 1;
    before = cap.fv_calls;
    glCallList(list);
    if (cap.fv_calls != before || glGetError() != GL_INVALID_FRAMEBUFFER_OPERATION)
        return fail(11, "replay completeness");
    cap.force_incomplete = 0;

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbs[0]);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbs[1]);
    before = cap.fv_calls;
    glClearNamedFramebufferfv(fbs[2], GL_COLOR, 1, color);
    if (cap.fv_calls != before + 1u || cap.fv[1] != 0.2f)
        return fail(12, "named clear executed");
    if (ctx.draw_framebuffer != fbs[0] || ctx.read_framebuffer != fbs[1])
        return fail(13, "named clear canonical binding mutation");
    if (cap.draw_fb != fb0->native_name || cap.read_fb != fb1->native_name)
        return fail(14, "named clear native restore");

    before = cap.iv_calls;
    glClearNamedFramebufferiv(0u, GL_STENCIL, 0, stencil_value);
    if (cap.iv_calls != before + 1u)
        return fail(15, "default named clear");
    if (cap.draw_fb != fb0->native_name || cap.read_fb != fb1->native_name)
        return fail(16, "default named restore");

    glClearNamedFramebufferfv(0xdeadbeefu, GL_COLOR, 0, color);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(17, "unknown named framebuffer");
    cap.force_incomplete = 1;
    before = cap.fv_calls;
    glClearNamedFramebufferfv(fbs[2], GL_COLOR, 0, color);
    if (cap.fv_calls != before || glGetError() != GL_INVALID_FRAMEBUFFER_OPERATION)
        return fail(18, "named completeness");
    if (cap.draw_fb != fb0->native_name || cap.read_fb != fb1->native_name)
        return fail(19, "incomplete named restore");
    cap.force_incomplete = 0;

    if (glXGetProcAddress((const GLubyte *)"glClearBufferfv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glClearNamedFramebufferfv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glClearNamedFramebufferfvEXT") == NULL)
        return fail(20, "proc surface");

    glDeleteLists(list, 1);
    glDeleteFramebuffers(3, fbs);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL clear-buffer unit test: PASS");
    return 0;
}

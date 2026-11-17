#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

struct buffer_state_capture {
    GLuint next_fb;
    GLuint draw_fb;
    GLuint read_fb;
    GLenum draw_values[MGL_MAX_DRAW_BUFFERS];
    GLsizei draw_count;
    GLenum read_value;
    unsigned draw_calls;
    unsigned read_calls;
    GLuint attachment_renderbuffer[MGL_MAX_DRAW_BUFFERS];
    GLuint cleared_draw_fb;
    GLuint checked_draw_fb;
} cap;

static int fail(int n, const char *why) {
    fprintf(stderr, "framebuffer-buffer-state failure %d: %s\n", n, why);
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
    } else if (target == GL_DRAW_FRAMEBUFFER) {
        cap.draw_fb = name;
    } else if (target == GL_READ_FRAMEBUFFER) {
        cap.read_fb = name;
    }
}

static void fake_draw_buffers(GLsizei n, const GLenum *buffers) {
    GLsizei i;
    cap.draw_count = n;
    for (i = 0; i < n && i < (GLsizei)MGL_MAX_DRAW_BUFFERS; ++i)
        cap.draw_values[i] = buffers[i];
    for (; i < (GLsizei)MGL_MAX_DRAW_BUFFERS; ++i)
        cap.draw_values[i] = GL_NONE;
    ++cap.draw_calls;
}

static void fake_attachment_renderbuffer(GLenum target, GLenum attachment, GLenum rb_target,
                                         GLuint rb) {
    (void)target;
    (void)rb_target;
    if (attachment >= GL_COLOR_ATTACHMENT0 &&
        attachment < GL_COLOR_ATTACHMENT0 + MGL_MAX_DRAW_BUFFERS)
        cap.attachment_renderbuffer[attachment - GL_COLOR_ATTACHMENT0] = rb;
}

static void fake_clear(GLbitfield mask) {
    (void)mask;
    cap.cleared_draw_fb = cap.draw_fb;
}

static GLenum fake_check_framebuffer(GLenum target) {
    (void)target;
    cap.checked_draw_fb = cap.draw_fb;
    return GL_FRAMEBUFFER_COMPLETE;
}

static void fake_clear_buffer_fv(GLenum buffer, GLint drawbuffer, const GLfloat *values) {
    (void)buffer;
    (void)drawbuffer;
    (void)values;
    cap.cleared_draw_fb = cap.draw_fb;
}

static void fake_read_buffer(GLenum buffer) {
    cap.read_value = buffer;
    ++cap.read_calls;
}

static void fake_limits(GLenum pname, GLint *value) {
    if (pname == GL_MAX_DRAW_BUFFERS)
        *value = 4;
    else if (pname == GL_MAX_COLOR_ATTACHMENTS)
        *value = 6;
    else
        *value = 0;
}

static GLint get_int(GLenum pname) {
    GLint value = -1;
    glGetIntegerv(pname, &value);
    return value;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_framebuffer *fb_a;
    struct mgl_framebuffer *fb_b;
    GLuint fbs[2] = {0u, 0u};
    struct mgl_renderbuffer *rb0, *rb1;
    GLuint list;
    GLenum two_buffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    GLenum wrong_output[1] = {GL_COLOR_ATTACHMENT1};
    GLenum swapped_buffers[2] = {GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT0};
    GLenum front_left[1] = {GL_FRONT_LEFT};
    GLenum back_left[1] = {GL_BACK_LEFT};
    unsigned before;
    unsigned i;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.next_fb = 4000u;

    native.glesGenFramebuffers = fake_gen_fb;
    native.glesDeleteFramebuffers = fake_del_fb;
    native.glesBindFramebuffer = fake_bind_fb;
    native.glesDrawBuffers = fake_draw_buffers;
    native.glesFramebufferRenderbuffer = fake_attachment_renderbuffer;
    native.glesClear = fake_clear;
    native.glesCheckFramebufferStatus = fake_check_framebuffer;
    native.glesClearBufferfv = fake_clear_buffer_fv;
    native.glesReadBuffer = fake_read_buffer;
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
    ctx.default_draw_buffers[0] = GL_BACK;
    for (i = 1u; i < MGL_MAX_DRAW_BUFFERS; ++i)
        ctx.default_draw_buffers[i] = GL_NONE;
    ctx.default_read_buffer = GL_BACK;
    mgl_context_set_current_for_test(&ctx);

    if (get_int(GL_DRAW_BUFFER) != (GLint)GL_BACK || get_int(GL_DRAW_BUFFER0) != (GLint)GL_BACK ||
        get_int(GL_DRAW_BUFFER1) != (GLint)GL_NONE || get_int(GL_READ_BUFFER) != (GLint)GL_BACK)
        return fail(2, "default/query initialization");
    native.glesGetIntegerv = fake_limits;
    if (get_int(GL_MAX_DRAW_BUFFERS) != 4 || get_int(GL_MAX_COLOR_ATTACHMENTS) != 6)
        return fail(40, "native render target limits are not respected");

    glDrawBuffer(GL_NONE);
    if (get_int(GL_DRAW_BUFFER0) != (GLint)GL_NONE || cap.draw_calls != 1u || cap.draw_count != 1 ||
        cap.draw_values[0] != GL_NONE)
        return fail(3, "default draw none lowering");
    glDrawBuffer(GL_BACK);
    if (get_int(GL_DRAW_BUFFER0) != (GLint)GL_BACK || cap.draw_calls != 2u ||
        cap.draw_values[0] != GL_BACK)
        return fail(4, "default draw back lowering");

    before = cap.draw_calls;
    glDrawBuffer(GL_FRONT);
    if (get_int(GL_DRAW_BUFFER0) != (GLint)GL_FRONT || cap.draw_calls != before)
        return fail(5, "front canonical without false GLES lowering");
    glDrawBuffers(1, front_left);
    if (get_int(GL_DRAW_BUFFER0) != (GLint)GL_FRONT_LEFT || cap.draw_calls != before)
        return fail(6, "front-left drawbuffers canonical");
    glDrawBuffers(1, back_left);
    if (get_int(GL_DRAW_BUFFER0) != (GLint)GL_BACK_LEFT || cap.draw_calls != before + 1u ||
        cap.draw_values[0] != GL_BACK)
        return fail(7, "back-left drawbuffers lowering");

    glDrawBuffers(1, (const GLenum[]){GL_FRONT});
    if (glGetError() != GL_INVALID_ENUM)
        return fail(8, "default drawbuffers aggregate enum rejection");
    glDrawBuffers(1, (const GLenum[]){GL_FRONT_RIGHT});
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(9, "default unavailable stereo buffer");

    before = cap.read_calls;
    glReadBuffer(GL_FRONT);
    if (get_int(GL_READ_BUFFER) != (GLint)GL_FRONT || cap.read_calls != before)
        return fail(10, "front read canonical without false GLES lowering");
    glReadBuffer(GL_BACK);
    if (get_int(GL_READ_BUFFER) != (GLint)GL_BACK || cap.read_calls != before + 1u ||
        cap.read_value != GL_BACK)
        return fail(11, "back read lowering");
    glReadBuffer(GL_RIGHT);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(12, "default unavailable right read buffer");

    glCreateFramebuffers(2, fbs);
    fb_a = mgl_framebuffer_find(&ctx, fbs[0]);
    fb_b = mgl_framebuffer_find(&ctx, fbs[1]);
    if (fb_a == NULL || fb_b == NULL)
        return fail(13, "framebuffer creation");

    glBindFramebuffer(GL_FRAMEBUFFER, fbs[0]);
    if (get_int(GL_DRAW_BUFFER0) != (GLint)GL_COLOR_ATTACHMENT0 ||
        get_int(GL_DRAW_BUFFER1) != (GLint)GL_NONE ||
        get_int(GL_READ_BUFFER) != (GLint)GL_COLOR_ATTACHMENT0)
        return fail(14, "fbo initial buffer state");

    native.glesGetIntegerv = fake_limits;
    glDrawBuffers(5,
                  (const GLenum[]){GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
                                   GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4});
    if (glGetError() != GL_INVALID_VALUE)
        return fail(41, "draw buffers exceed native limit");
    glReadBuffer(GL_COLOR_ATTACHMENT6);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(42, "read attachment exceeds native limit");
    before = cap.draw_calls;
    glDrawBuffers(2, two_buffers);
    if (cap.draw_calls != before + 1u || cap.draw_count != 2 ||
        cap.draw_values[0] != GL_COLOR_ATTACHMENT0 || cap.draw_values[1] != GL_COLOR_ATTACHMENT1 ||
        get_int(GL_DRAW_BUFFER0) != (GLint)GL_COLOR_ATTACHMENT0 ||
        get_int(GL_DRAW_BUFFER1) != (GLint)GL_COLOR_ATTACHMENT1 ||
        get_int(GL_DRAW_BUFFER2) != (GLint)GL_NONE)
        return fail(15, "fbo multidraw state/lowering");

    before = cap.draw_calls;
    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    if (get_int(GL_DRAW_BUFFER0) != (GLint)GL_COLOR_ATTACHMENT1 ||
        get_int(GL_DRAW_BUFFER1) != (GLint)GL_NONE || cap.draw_calls != before)
        return fail(16, "desktop drawbuffer attachment remap retained canonically");

    glDrawBuffers(1, wrong_output);
    if (glGetError() != GL_NO_ERROR || get_int(GL_DRAW_BUFFER0) != (GLint)GL_COLOR_ATTACHMENT1)
        return fail(17, "desktop drawbuffers nonidentity accepted");
    glDrawBuffer(GL_BACK);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(18, "fbo default-buffer enum rejection");

    before = cap.read_calls;
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    if (get_int(GL_READ_BUFFER) != (GLint)GL_COLOR_ATTACHMENT1 || cap.read_calls != before + 1u ||
        cap.read_value != GL_COLOR_ATTACHMENT1)
        return fail(19, "fbo read buffer state/lowering");
    glReadBuffer(GL_COLOR_ATTACHMENT0 + MGL_MAX_DRAW_BUFFERS);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(20, "fbo read attachment range");

    glBindFramebuffer(GL_FRAMEBUFFER, fbs[1]);
    if (get_int(GL_DRAW_BUFFER0) != (GLint)GL_COLOR_ATTACHMENT0 ||
        get_int(GL_READ_BUFFER) != (GLint)GL_COLOR_ATTACHMENT0)
        return fail(21, "per-fbo independent initial state");
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (fb_b->draw_buffers[0] != GL_NONE || fb_b->read_buffer != GL_NONE ||
        fb_a->draw_buffers[0] != GL_COLOR_ATTACHMENT1 || fb_a->read_buffer != GL_COLOR_ATTACHMENT1)
        return fail(22, "per-fbo persistence");

    glBindFramebuffer(GL_FRAMEBUFFER, fbs[0]);
    before = cap.draw_calls;
    glNamedFramebufferDrawBuffers(fbs[1], 2, two_buffers);
    if (fb_b->draw_buffers[0] != GL_COLOR_ATTACHMENT0 ||
        fb_b->draw_buffers[1] != GL_COLOR_ATTACHMENT1 || ctx.draw_framebuffer != fbs[0] ||
        ctx.read_framebuffer != fbs[0] || cap.draw_calls != before + 1u ||
        cap.draw_fb != fb_a->native_name || cap.read_fb != fb_a->native_name)
        return fail(23, "named drawbuffers selector preservation");

    before = cap.read_calls;
    glNamedFramebufferReadBuffer(fbs[1], GL_COLOR_ATTACHMENT1);
    if (fb_b->read_buffer != GL_COLOR_ATTACHMENT1 || cap.read_calls != before + 1u ||
        cap.read_value != GL_COLOR_ATTACHMENT1 || cap.draw_fb != fb_a->native_name ||
        cap.read_fb != fb_a->native_name)
        return fail(24, "named readbuffer selector preservation");

    before = cap.draw_calls;
    glNamedFramebufferDrawBuffer(fbs[1], GL_COLOR_ATTACHMENT1);
    if (fb_b->draw_buffers[0] != GL_COLOR_ATTACHMENT1 || fb_b->draw_buffers[1] != GL_NONE ||
        cap.draw_calls != before || cap.draw_fb != fb_a->native_name ||
        cap.read_fb != fb_a->native_name)
        return fail(25, "named desktop attachment remap retained canonically");

    before = cap.draw_calls;
    glNamedFramebufferDrawBuffer(0u, GL_FRONT);
    if (ctx.default_draw_buffers[0] != GL_FRONT || cap.draw_calls != before ||
        ctx.draw_framebuffer != fbs[0] || cap.draw_fb != fb_a->native_name)
        return fail(26, "named default draw state without binding mutation");
    before = cap.read_calls;
    glNamedFramebufferReadBuffer(0u, GL_FRONT);
    if (ctx.default_read_buffer != GL_FRONT || cap.read_calls != before ||
        ctx.read_framebuffer != fbs[0] || cap.read_fb != fb_a->native_name)
        return fail(27, "named default read state without binding mutation");

    glNamedFramebufferDrawBuffer(0xdeadbeefu, GL_NONE);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(28, "unknown named draw framebuffer");
    glNamedFramebufferReadBuffer(0xdeadbeefu, GL_NONE);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(29, "unknown named read framebuffer");

    list = glGenLists(1);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbs[0]);
    if (get_int(GL_DRAW_BUFFER0) != (GLint)GL_COLOR_ATTACHMENT1)
        return fail(30, "pre-list fbo A state");
    glNewList(list, GL_COMPILE);
    glDrawBuffer(GL_NONE);
    glEndList();
    if (get_int(GL_DRAW_BUFFER0) != (GLint)GL_COLOR_ATTACHMENT1)
        return fail(31, "list compile mutated state");
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbs[1]);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glCallList(list);
    if (fb_b->draw_buffers[0] != GL_NONE || fb_a->draw_buffers[0] != GL_COLOR_ATTACHMENT1)
        return fail(32, "list replay-time framebuffer binding");

    if (glXGetProcAddress((const GLubyte *)"glDrawBuffer") == NULL ||
        glXGetProcAddress((const GLubyte *)"glDrawBuffers") == NULL ||
        glXGetProcAddress((const GLubyte *)"glReadBuffer") == NULL ||
        glXGetProcAddress((const GLubyte *)"glNamedFramebufferDrawBuffer") == NULL ||
        glXGetProcAddress((const GLubyte *)"glNamedFramebufferDrawBuffers") == NULL ||
        glXGetProcAddress((const GLubyte *)"glNamedFramebufferReadBuffer") == NULL)
        return fail(33, "proc surface");

    glBindFramebuffer(GL_FRAMEBUFFER, fbs[0]);
    rb0 = mgl_renderbuffer_ensure(&ctx, 23u, 0);
    rb1 = mgl_renderbuffer_ensure(&ctx, 24u, 0);
    if (rb0 == NULL || rb1 == NULL)
        return fail(34, "renderbuffer tracking");
    rb0->native_name = 1234u;
    rb0->realized = GL_TRUE;
    rb1->native_name = 5678u;
    rb1->realized = GL_TRUE;
    fb_a->color_renderbuffer = rb0->name;
    fb_a->extra_color_renderbuffers[1] = rb1->name;
    glDrawBuffers(2, swapped_buffers);
    if (glGetError() != GL_NO_ERROR || get_int(GL_DRAW_BUFFER0) != (GLint)GL_COLOR_ATTACHMENT1 ||
        get_int(GL_DRAW_BUFFER1) != (GLint)GL_COLOR_ATTACHMENT0)
        return fail(34, "desktop MRT swapped mapping stored");
    if (mgl_framebuffer_draw_route_begin(&ctx) != 1 ||
        cap.attachment_renderbuffer[0] != rb1->native_name ||
        cap.attachment_renderbuffer[1] != rb0->native_name ||
        cap.draw_values[0] != GL_COLOR_ATTACHMENT0 || cap.draw_values[1] != GL_COLOR_ATTACHMENT1 ||
        cap.draw_fb != fb_a->routed_draw_native_name)
        return fail(35, "swapped MRT native attachments");
    mgl_framebuffer_draw_route_end(&ctx);
    if (cap.draw_fb != fb_a->native_name || cap.read_fb != fb_a->native_name)
        return fail(36, "swapped MRT native restored");
    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    if (mgl_framebuffer_draw_route_begin(&ctx) != 1 ||
        cap.draw_fb != fb_a->routed_draw_native_name ||
        cap.draw_values[0] != GL_COLOR_ATTACHMENT0 || cap.draw_count != 1 ||
        cap.attachment_renderbuffer[0] != rb1->native_name || cap.attachment_renderbuffer[1] != 0u)
        return fail(37, "draw framebuffer remap created");
    mgl_framebuffer_draw_route_end(&ctx);
    if (cap.draw_fb != fb_a->native_name || cap.read_fb != fb_a->native_name)
        return fail(38, "draw framebuffer remap restored");
    glClear(GL_COLOR_BUFFER_BIT);
    if (cap.cleared_draw_fb != fb_a->routed_draw_native_name || cap.draw_fb != fb_a->native_name ||
        get_int(GL_DRAW_BUFFER0) != (GLint)GL_COLOR_ATTACHMENT1)
        return fail(39, "clear uses remap without interface changes");
    if (glCheckNamedFramebufferStatus(fbs[0], GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE ||
        cap.checked_draw_fb != fb_a->routed_draw_native_name || cap.draw_fb != fb_a->native_name ||
        cap.read_fb != fb_a->native_name)
        return fail(41, "named draw completeness uses remapped framebuffer");
    glClearNamedFramebufferfv(fbs[0], GL_COLOR, 0, (const GLfloat[]){0.0f, 0.0f, 0.0f, 0.0f});
    if (cap.cleared_draw_fb != fb_a->routed_draw_native_name || cap.draw_fb != fb_a->native_name ||
        cap.read_fb != fb_a->native_name || glGetError() != GL_NO_ERROR)
        return fail(42, "named clear uses remapped framebuffer");
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    cap.cleared_draw_fb = 0u;
    glClear(GL_COLOR_BUFFER_BIT);
    if (cap.cleared_draw_fb != fb_a->native_name)
        return fail(40, "identity draw bypasses remap");

    glBlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_TEXTURE_2D);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(43, "blit filter enum validation");
    glBlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, 0x80000000u, GL_NEAREST);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(44, "blit mask value validation");
    glBlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, GL_DEPTH_BUFFER_BIT, GL_LINEAR);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(45, "depth blit nearest validation");
    glBlitNamedFramebuffer(fbs[0], fbs[1], 0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT,
                           GL_TEXTURE_2D);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(46, "named blit filter enum validation");
    glBlitNamedFramebuffer(fbs[0], fbs[1], 0, 0, 1, 1, 0, 0, 1, 1, 0x80000000u, GL_NEAREST);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(47, "named blit mask value validation");
    glDeleteLists(list, 1);
    glDeleteFramebuffers(2, fbs);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL framebuffer draw/read buffer state unit test: PASS");
    return 0;
}

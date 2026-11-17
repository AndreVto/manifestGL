#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_fb;
    GLuint next_rb;
    GLuint draw_fb;
    GLuint read_fb;
    GLuint rb;
    GLuint attached_rb;
    GLenum attach;
    GLsizei samples;
    unsigned storage_calls;
    unsigned multisample_calls;
    GLint reported_samples;
    GLenum storage_format;
    GLenum multisample_format;
    unsigned blit_calls;
    GLbitfield blit_mask;
    GLenum blit_filter;
} cap;

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
    return GL_FRAMEBUFFER_COMPLETE;
}

static void fake_blit(GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint dx0, GLint dy0, GLint dx1,
                      GLint dy1, GLbitfield mask, GLenum filter) {
    (void)sx0;
    (void)sy0;
    (void)sx1;
    (void)sy1;
    (void)dx0;
    (void)dy0;
    (void)dx1;
    (void)dy1;
    ++cap.blit_calls;
    cap.blit_mask = mask;
    cap.blit_filter = filter;
}

static void fake_attach_rb(GLenum target, GLenum attachment, GLenum rbtarget, GLuint rb) {
    (void)target;
    (void)rbtarget;
    cap.attach = attachment;
    cap.attached_rb = rb;
}

static void fake_gen_rb(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_rb;
}

static void fake_del_rb(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_rb(GLenum target, GLuint name) {
    (void)target;
    cap.rb = name;
}

static void fake_storage(GLenum target, GLenum format, GLsizei w, GLsizei h) {
    (void)target;
    (void)w;
    (void)h;
    cap.storage_format = format;
    cap.reported_samples = 0;
    ++cap.storage_calls;
}

static void fake_storage_ms(GLenum target, GLsizei samples, GLenum format, GLsizei w, GLsizei h) {
    (void)target;
    (void)w;
    (void)h;
    cap.multisample_format = format;
    cap.samples = samples;
    cap.reported_samples = samples;
    ++cap.multisample_calls;
}

static void fake_get_rb_param(GLenum target, GLenum pname, GLint *value) {
    (void)target;
    if (pname == GL_RENDERBUFFER_SAMPLES)
        *value = cap.reported_samples;
    if (pname == GL_RENDERBUFFER_DEPTH_SIZE) {
        GLenum format = cap.multisample_calls != 0u && cap.reported_samples > 0
                            ? cap.multisample_format
                            : cap.storage_format;
        *value = format == GL_DEPTH_COMPONENT16                                    ? 16
                 : format == GL_DEPTH_COMPONENT24 || format == GL_DEPTH24_STENCIL8 ? 24
                 : format == GL_DEPTH_COMPONENT32F                                 ? 32
                                                                                   : 0;
    }
}

static int fail(int n, const char *msg) {
    fprintf(stderr, "DSA framebuffer failure %d: %s\n", n, msg);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_framebuffer *fb0, *fb1;
    struct mgl_renderbuffer *rb0, *rb1;
    GLuint fbs[2] = {0u, 0u}, rbs[2] = {0u, 0u};
    GLint iv = 0;
    GLenum fbo_invalidate[2] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT};
    GLenum default_invalidate[3] = {GL_COLOR, GL_DEPTH, GL_STENCIL};
    GLenum bad_invalidate = GL_COLOR;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.next_fb = 800u;
    cap.next_rb = 900u;
    native.glesGenFramebuffers = fake_gen_fb;
    native.glesDeleteFramebuffers = fake_del_fb;
    native.glesBindFramebuffer = fake_bind_fb;
    native.glesCheckFramebufferStatus = fake_check_fb;
    native.glesFramebufferRenderbuffer = fake_attach_rb;
    native.glesBlitFramebuffer = fake_blit;
    native.glesGenRenderbuffers = fake_gen_rb;
    native.glesDeleteRenderbuffers = fake_del_rb;
    native.glesBindRenderbuffer = fake_bind_rb;
    native.glesRenderbufferStorage = fake_storage;
    native.glesRenderbufferStorageMultisample = fake_storage_ms;
    native.glesGetRenderbufferParameteriv = fake_get_rb_param;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glCreateFramebuffers(2, fbs);
    glCreateRenderbuffers(2, rbs);
    if (!glIsFramebuffer(fbs[0]) || !glIsFramebuffer(fbs[1]) || !glIsRenderbuffer(rbs[0]) ||
        !glIsRenderbuffer(rbs[1]))
        return fail(2, "create realizes objects");
    fb0 = mgl_framebuffer_find(&ctx, fbs[0]);
    fb1 = mgl_framebuffer_find(&ctx, fbs[1]);
    rb0 = mgl_renderbuffer_find(&ctx, rbs[0]);
    rb1 = mgl_renderbuffer_find(&ctx, rbs[1]);
    if (fb0 == NULL || fb1 == NULL || rb0 == NULL || rb1 == NULL)
        return fail(3, "object lookup");

    glBindFramebuffer(GL_FRAMEBUFFER, fbs[0]);
    glBindRenderbuffer(GL_RENDERBUFFER, rbs[0]);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_RGBA4, 32, 16);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.multisample_calls != 1u ||
        cap.samples != 2)
        return fail(18, "core multisample renderbuffer native");
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &iv);
    if (iv != 2)
        return fail(19, "core multisample sample query");
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 0, GL_RGBA4, 16, 8);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.storage_calls != 1u)
        return fail(20, "zero-sample core storage lowering");
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &iv);
    if (iv != 0)
        return fail(21, "zero-sample core query");
    glRenderbufferStorageMultisample(0xdeadbeefu, 1, GL_RGBA4, 8, 8);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(22, "core multisample target validation");
    glNamedRenderbufferStorage(rbs[1], GL_RGBA4, 64, 32);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &iv);
    if ((GLuint)iv != rbs[0] || cap.rb != rb0->native_name)
        return fail(4, "named storage preserves rb binding");
    glGetNamedRenderbufferParameteriv(rbs[1], GL_RENDERBUFFER_WIDTH, &iv);
    if (iv != 64)
        return fail(5, "named rb width");
    glGetNamedRenderbufferParameteriv(rbs[1], GL_RENDERBUFFER_SAMPLES, &iv);
    if (iv != 0)
        return fail(6, "named rb samples default");
    glNamedRenderbufferStorageMultisample(rbs[1], 4, GL_DEPTH24_STENCIL8, 128, 96);
    if (cap.multisample_calls != 2u || cap.samples != 4 || cap.rb != rb0->native_name)
        return fail(7, "multisample realizes and restores");
    glGetNamedRenderbufferParameteriv(rbs[1], GL_RENDERBUFFER_SAMPLES, &iv);
    if (iv != 4)
        return fail(8, "sample query");

    glNamedFramebufferRenderbuffer(fbs[1], GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbs[1]);
    if (cap.attach != GL_DEPTH_STENCIL_ATTACHMENT || cap.attached_rb != rb1->native_name)
        return fail(9, "named attachment native");
    glGetNamedFramebufferAttachmentParameteriv(fbs[1], GL_DEPTH_ATTACHMENT,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &iv);
    if (iv != (GLint)GL_RENDERBUFFER)
        return fail(10, "attachment type");
    glGetNamedFramebufferAttachmentParameteriv(fbs[1], GL_DEPTH_ATTACHMENT,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &iv);
    if ((GLuint)iv != rbs[1])
        return fail(11, "attachment object name");
    glBindFramebuffer(GL_FRAMEBUFFER, fbs[1]);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &iv);
    if ((GLuint)iv != rbs[1] || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(23, "bound attachment query");
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &iv);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(24, "non-texture attachment query validation");
    glBindFramebuffer(GL_FRAMEBUFFER, fbs[0]);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &iv);
    if ((GLuint)iv != fbs[0])
        return fail(12, "interface draw binding preserved");
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &iv);
    if ((GLuint)iv != fbs[0])
        return fail(13, "interface read binding preserved");
    if (cap.draw_fb != fb0->native_name || cap.read_fb != fb0->native_name)
        return fail(14, "native fbo bindings restored");

    if (glCheckNamedFramebufferStatus(fbs[1], GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return fail(15, "named status");
    if (cap.draw_fb != fb0->native_name || cap.read_fb != fb0->native_name)
        return fail(16, "status restores fbo bindings");

    glBlitNamedFramebuffer(fbs[1], fbs[0], 0, 0, 8, 8, 0, 0, 8, 8, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.blit_calls != 1u ||
        cap.blit_mask != GL_COLOR_BUFFER_BIT || cap.blit_filter != GL_NEAREST)
        return fail(25, "named blit");
    if (cap.draw_fb != fb0->native_name || cap.read_fb != fb0->native_name)
        return fail(26, "named blit restores native bindings");
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &iv);
    if ((GLuint)iv != fbs[0])
        return fail(27, "named blit preserves interface draw binding");
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &iv);
    if ((GLuint)iv != fbs[0])
        return fail(28, "named blit preserves interface read binding");
    glBlitNamedFramebuffer(fbs[1], fbs[0], 0, 0, 8, 8, 0, 0, 8, 8, GL_DEPTH_BUFFER_BIT, GL_LINEAR);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || cap.blit_calls != 1u)
        return fail(29, "named blit depth filter validation");
    glBlitNamedFramebuffer(0xdeadbeefu, fbs[0], 0, 0, 8, 8, 0, 0, 8, 8, GL_COLOR_BUFFER_BIT,
                           GL_NEAREST);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || cap.blit_calls != 1u)
        return fail(30, "named blit object validation");

    glInvalidateFramebuffer(GL_FRAMEBUFFER, 2, fbo_invalidate);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(31, "bound FBO invalidate");
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &bad_invalidate);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(32, "bound FBO invalidate attachment validation");
    glBindFramebuffer(GL_FRAMEBUFFER, 0u);
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 3, default_invalidate);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(33, "default framebuffer invalidate");
    bad_invalidate = GL_COLOR_ATTACHMENT0;
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &bad_invalidate);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(34, "default invalidate attachment validation");
    glBindFramebuffer(GL_FRAMEBUFFER, fbs[0]);

    glInvalidateNamedFramebufferData(fbs[1], 2, fbo_invalidate);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(35, "named invalidate data");
    glInvalidateNamedFramebufferData(0u, 3, default_invalidate);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(36, "named default invalidate data");
    glInvalidateNamedFramebufferData(0xdeadbeefu, 2, fbo_invalidate);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(37, "named invalidate object validation");
    glInvalidateNamedFramebufferData(fbs[1], -1, fbo_invalidate);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(38, "named invalidate count validation");
    glInvalidateNamedFramebufferSubData(fbs[1], 2, fbo_invalidate, 0, 0, 4, 4);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(39, "named invalidate subdata");
    glInvalidateNamedFramebufferSubData(fbs[1], 2, fbo_invalidate, 0, 0, -1, 4);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(40, "named invalidate subdata size validation");
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &iv);
    if ((GLuint)iv != fbs[0])
        return fail(41, "invalidation changed interface binding");
    if (cap.draw_fb != fb0->native_name || cap.read_fb != fb0->native_name)
        return fail(42, "invalidation changed native binding");

    glNamedFramebufferRenderbuffer(0xdeadbeefu, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbs[1]);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(17, "unknown named framebuffer rejected");

    /* Rejected calls must preserve attachment ownership, outputs and GLES state. */
    glBindFramebuffer(GL_FRAMEBUFFER, fbs[1]);
    {
        GLint original_name = -1, observed_name = -1;
        unsigned storage_before = cap.storage_calls;
        unsigned multisample_before = cap.multisample_calls;
        unsigned blits_before = cap.blit_calls;
        const GLenum attachment = GL_DEPTH_ATTACHMENT;
        glGetFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &original_name);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || original_name != (GLint)rbs[1])
            return fail(43, "baseline attachment for Begin/End rejection");
        glBegin(GL_POINTS);
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &attachment);
        glEnd();
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
            return fail(44, "bound framebuffer invalidation inside Begin");
        glBegin(GL_POINTS);
        glInvalidateNamedFramebufferData(fbs[1], 1, &attachment);
        glEnd();
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
            return fail(45, "named framebuffer invalidation inside Begin");
        glBegin(GL_POINTS);
        glInvalidateNamedFramebufferSubData(fbs[1], 1, &attachment, 0, 0, 2, 2);
        glEnd();
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
            return fail(46, "named framebuffer sub-invalidation inside Begin");
        glBegin(GL_POINTS);
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, 0, 0);
        glEnd();
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
            return fail(47, "texture detach inside Begin");
        glBegin(GL_POINTS);
        glNamedRenderbufferStorage(rbs[1], GL_RGBA4, 2, 2);
        glEnd();
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
            return fail(48, "named storage inside Begin");
        glGetFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &observed_name);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || observed_name != original_name ||
            cap.storage_calls != storage_before || cap.multisample_calls != multisample_before ||
            cap.blit_calls != blits_before)
            return fail(49, "rejected calls changed attachment ownership or GLES effects");
    }

    glBindRenderbuffer(GL_RENDERBUFFER, rbs[0]);
    /* GLES 3.2 core supports sRGB8/alpha8 as renderbuffer storage.
     * Format support is checked before framebuffer selection. */
    glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8_ALPHA8, 16, 16);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &iv);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.storage_format != GL_SRGB8_ALPHA8 ||
        (GLenum)iv != GL_SRGB8_ALPHA8 || rb0->internal_format != GL_SRGB8_ALPHA8)
        return fail(304, "sRGB renderbuffer storage reaches GLES unchanged");
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 1280, 720);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &iv);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.storage_format != GL_DEPTH_COMPONENT24 ||
        (GLenum)iv != GL_DEPTH_COMPONENT24 || rb0->internal_format != GL_DEPTH_COMPONENT24)
        return fail(54, "unsized depth renderbuffer should prefer GLES depth24");
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_DEPTH_COMPONENT, 640, 480);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        cap.multisample_format != GL_DEPTH_COMPONENT24 ||
        rb0->internal_format != GL_DEPTH_COMPONENT24)
        return fail(55, "unsized multisample depth format lowering");
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX, 64, 64);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.storage_format != GL_STENCIL_INDEX8)
        return fail(56, "unsized stencil renderbuffer format lowering");
    glNamedRenderbufferStorage(rbs[1], GL_DEPTH_COMPONENT, 1280, 720);
    glGetNamedRenderbufferParameteriv(rbs[1], GL_RENDERBUFFER_INTERNAL_FORMAT, &iv);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.storage_format != GL_DEPTH_COMPONENT24 ||
        (GLenum)iv != GL_DEPTH_COMPONENT24 || cap.rb != rb0->native_name)
        return fail(57, "named unsized depth storage and binding restoration");
    glNamedRenderbufferStorageMultisample(rbs[1], 2, GL_DEPTH_COMPONENT, 128, 96);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        cap.multisample_format != GL_DEPTH_COMPONENT24 ||
        rb1->internal_format != GL_DEPTH_COMPONENT24 || cap.rb != rb0->native_name)
        return fail(58, "named multisample unsized depth storage and binding restoration");

    glBindRenderbuffer(GL_RENDERBUFFER, rbs[0]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 64, 64);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_DEPTH_SIZE, &iv);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.storage_format != GL_DEPTH_COMPONENT16 ||
        iv != 16)
        return fail(59, "explicit depth16 remains depth16");
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_DEPTH_COMPONENT24, 64, 64);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        cap.multisample_format != GL_DEPTH_COMPONENT24)
        return fail(60, "explicit depth24 multisample remains depth24");
    glNamedRenderbufferStorage(rbs[1], GL_DEPTH_COMPONENT32, 64, 64);
    glGetNamedRenderbufferParameteriv(rbs[1], GL_RENDERBUFFER_INTERNAL_FORMAT, &iv);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.storage_format != GL_DEPTH_COMPONENT32F ||
        (GLenum)iv != GL_DEPTH_COMPONENT32F || cap.rb != rb0->native_name)
        return fail(61, "desktop depth32 translates to GLES depth32F");
    glNamedRenderbufferStorageMultisample(rbs[1], 2, GL_DEPTH_COMPONENT16, 64, 64);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        cap.multisample_format != GL_DEPTH_COMPONENT16 || cap.rb != rb0->native_name)
        return fail(62, "explicit named depth16 multisample remains depth16");
    glNamedRenderbufferStorage(rbs[1], GL_DEPTH_COMPONENT24, 64, 64);
    glGetNamedRenderbufferParameteriv(rbs[1], GL_RENDERBUFFER_DEPTH_SIZE, &iv);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.storage_format != GL_DEPTH_COMPONENT24 ||
        iv != 24 || cap.rb != rb0->native_name)
        return fail(63, "named depth-size query matches native and preserves binding");
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 64, 64);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_DEPTH_SIZE, &iv);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.storage_format != GL_DEPTH_COMPONENT24 ||
        iv != 24)
        return fail(64, "unsized depth-size query reports real native precision");

    glDeleteFramebuffers(2, fbs);
    glDeleteRenderbuffers(2, rbs);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL DSA framebuffer/renderbuffer unit test: PASS");
    return 0;
}

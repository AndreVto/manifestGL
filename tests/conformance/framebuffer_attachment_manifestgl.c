#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "core/error.h"
#include "conformance/framebuffer_attachment_cases.h"
#include <string.h>

static GLuint next_name;

static void gen(GLsizei n, GLuint *names) {
    for (GLsizei i = 0; i < n; ++i)
        names[i] = ++next_name;
}

static void texbind(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void image(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height,
                  GLint border, GLenum format, GLenum type, const void *data) {
    (void)target;
    (void)level;
    (void)internal;
    (void)width;
    (void)height;
    (void)border;
    (void)format;
    (void)type;
    (void)data;
}

static void fbbind(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void attachment(GLenum target, GLenum att, GLenum textarget, GLuint name, GLint level) {
    (void)target;
    (void)att;
    (void)textarget;
    (void)name;
    (void)level;
}

static void layered(GLenum target, GLenum att, GLuint name, GLint level) {
    (void)target;
    (void)att;
    (void)name;
    (void)level;
}

static void fbparameter(GLenum target, GLenum pname, GLint value) {
    (void)target;
    (void)pname;
    (void)value;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenTextures = gen;
    native.glesGenBuffers = gen;
    native.glesBindTexture = texbind;
    native.glesTexImage2D = image;
    native.glesGenFramebuffers = gen;
    native.glesGenRenderbuffers = gen;
    native.glesBindFramebuffer = fbbind;
    native.glesFramebufferTexture2D = attachment;
    native.glesFramebufferParameteri = fbparameter;
    native.glesFramebufferTexture = layered;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (!ctx.share_group)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    ctx.unpack_alignment = 4;
    ctx.next_framebuffer_name = 1;
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);
    if (!glXGetProcAddress((const GLubyte *)"glFramebufferTexture") ||
        !glXGetProcAddress((const GLubyte *)"glFramebufferParameteri") ||
        !glXGetProcAddress((const GLubyte *)"glGetFramebufferParameteriv"))
        return 2;
    framebuffer_attachment_trace();
    if (legacy_alias_trace())
        return 6;
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

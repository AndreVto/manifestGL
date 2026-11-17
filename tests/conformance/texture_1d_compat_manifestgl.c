#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "state/state.h"
#include "conformance/texture_1d_compat_cases.h"
#include <string.h>

static GLuint name;

static void gen(GLsizei n, GLuint *out) {
    for (GLsizei i = 0; i < n; ++i)
        out[i] = ++name;
}

static void bind_tex(GLenum target, GLuint name_arg) {
    (void)target;
    (void)name_arg;
}

static void image2d(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border,
                    GLenum fmt, GLenum typ, const void *p) {
    (void)target;
    (void)level;
    (void)internal;
    (void)w;
    (void)h;
    (void)border;
    (void)fmt;
    (void)typ;
    (void)p;
}

static void sub2d(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt,
                  GLenum typ, const void *p) {
    (void)target;
    (void)level;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)fmt;
    (void)typ;
    (void)p;
}

static void param(GLenum target, GLenum key, GLint value) {
    (void)target;
    (void)key;
    (void)value;
}

static void bind_fbo(GLenum target, GLuint value) {
    (void)target;
    (void)value;
}

static void attach(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                   GLint level) {
    (void)target;
    (void)attachment;
    (void)textarget;
    (void)texture;
    (void)level;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenTextures = gen;
    native.glesBindTexture = bind_tex;
    native.glesTexImage2D = image2d;
    native.glesTexSubImage2D = sub2d;
    native.glesTexParameteri = param;
    native.glesGenFramebuffers = gen;
    native.glesBindFramebuffer = bind_fbo;
    native.glesFramebufferTexture2D = attach;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    ctx.unpack_alignment = 4;
    ctx.pack_alignment = 4;
    ctx.next_framebuffer_name = 1;
    mgl_context_set_current_for_test(&ctx);
    texture_1d_compat_trace();
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

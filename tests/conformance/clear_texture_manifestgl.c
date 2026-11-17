#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "state/state.h"
#include "conformance/clear_texture_cases.h"
#include <string.h>
static GLuint next_name;

static void gen(GLsizei n, GLuint *values) {
    for (GLsizei i = 0; i < n; ++i)
        values[i] = ++next_name;
}

static void bindtex(GLenum target, GLuint value) {
    (void)target;
    (void)value;
}

static void tex2d(GLenum a, GLint b, GLint c, GLsizei d, GLsizei e, GLint f, GLenum g, GLenum h,
                  const void *i) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    (void)g;
    (void)h;
    (void)i;
}

static void sub2d(GLenum a, GLint b, GLint c, GLint d, GLsizei e, GLsizei f, GLenum g, GLenum h,
                  const void *i) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    (void)g;
    (void)h;
    (void)i;
}

static void tex3d(GLenum a, GLint b, GLint c, GLsizei d, GLsizei e, GLsizei f, GLint g, GLenum h,
                  GLenum i, const void *j) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    (void)g;
    (void)h;
    (void)i;
    (void)j;
}

static void sub3d(GLenum a, GLint b, GLint c, GLint d, GLint e, GLsizei f, GLsizei g, GLsizei h,
                  GLenum i, GLenum j, const void *k) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    (void)g;
    (void)h;
    (void)i;
    (void)j;
    (void)k;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenBuffers = gen;
    native.glesGenTextures = gen;
    native.glesBindTexture = bindtex;
    native.glesTexImage2D = tex2d;
    native.glesTexSubImage2D = sub2d;
    native.glesTexImage3D = tex3d;
    native.glesTexSubImage3D = sub3d;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (!ctx.share_group)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    ctx.unpack_alignment = 4;
    ctx.pack_alignment = 4;
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);
    if (!glXGetProcAddress((const GLubyte *)"glClearTexImage") ||
        !glXGetProcAddress((const GLubyte *)"glClearTexSubImage"))
        return 2;
    clear_texture_trace();
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

#include "mgl/gl.h"
#include "core/context.h"
#include "core/error.h"
#include "texture/texture_store.h"
#include <string.h>

static GLuint next_id;
static GLuint copied_src, copied_dst;
static GLsizei copied_width, copied_height, copied_depth;
static unsigned copies, barriers;

static const GLubyte *extensions(GLenum name) {
    return name == GL_EXTENSIONS ? (const GLubyte *)"GL_EXT_copy_image GL_NV_texture_barrier"
                                 : (const GLubyte *)"OpenGL ES 3.2";
}

static void gen(GLsizei n, GLuint *names) {
    for (GLsizei i = 0; i < n; ++i)
        names[i] = ++next_id;
}

static void bind(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void upload(GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty,
                   const void *d) {
    (void)t;
    (void)l;
    (void)i;
    (void)w;
    (void)h;
    (void)b;
    (void)f;
    (void)ty;
    (void)d;
}

static void copy_image(GLuint sn, GLenum st, GLint sl, GLint sx, GLint sy, GLint sz, GLuint dn,
                       GLenum dt, GLint dl, GLint dx, GLint dy, GLint dz, GLsizei w, GLsizei h,
                       GLsizei dep) {
    (void)st;
    (void)sl;
    (void)sx;
    (void)sy;
    (void)sz;
    (void)dt;
    (void)dl;
    (void)dx;
    (void)dy;
    (void)dz;
    copied_src = sn;
    copied_dst = dn;
    copied_width = w;
    copied_height = h;
    copied_depth = dep;
    ++copies;
}

static void barrier(void) {
    ++barriers;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint names[2];
    GLint binding;
    unsigned char values[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    unsigned char result[16] = {0};
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetString = extensions;
    native.glesGenTextures = gen;
    native.glesBindTexture = bind;
    native.glesTexImage2D = upload;
    native.glesCopyImageSubData = copy_image;
    native.glesTextureBarrier = barrier;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (!ctx.share_group)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);
    glGenTextures(2, names);
    glBindTexture(GL_TEXTURE_2D, names[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, values);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, names[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    if (glGetError() != GL_NO_ERROR)
        return 2;
    glCopyImageSubData(names[0], GL_TEXTURE_2D, 0, 1, 0, 0, names[1], GL_TEXTURE_2D, 0, 0, 1, 0, 1,
                       1, 1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, result);
    if (glGetError() != GL_NO_ERROR || copies != 1 ||
        copied_src != mgl_texture_find(&ctx, names[0])->native_name ||
        copied_dst != mgl_texture_find(&ctx, names[1])->native_name || copied_width != 1 ||
        copied_height != 1 || copied_depth != 1 || binding != (GLint)names[1] ||
        memcmp(result + 8, values + 4, 4) != 0)
        return 3;
    glCopyImageSubData(names[0], GL_TEXTURE_2D, 0, 2, 0, 0, names[1], GL_TEXTURE_2D, 0, 0, 0, 0, 1,
                       1, 1);
    if (glGetError() != GL_INVALID_VALUE || copies != 1u)
        return 4;
    glTextureBarrierNV();
    glTextureBarrier();
    if (glGetError() != GL_NO_ERROR || barriers != 2u)
        return 5;
    ctx.immediate.active = GL_TRUE;
    glTextureBarrierNV();
    if (glGetError() != GL_NO_ERROR || barriers != 2u)
        return 6;
    ctx.immediate.active = GL_FALSE;
    if (glGetError() != GL_INVALID_OPERATION)
        return 8;
    glDeleteTextures(2, names);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

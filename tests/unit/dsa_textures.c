#include "mgl/gl.h"
#include "core/context.h"
#include "texture/texture_store.h"
#include "objects/object_store.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    GLuint active_unit;
    GLuint bound[8];
    GLenum last_pname;
    GLint last_param;
    GLfloat last_fparam;
    unsigned float_param_calls;
    unsigned subimage_calls;
} cap;

static void fake_gen(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_texture;
}

static void fake_delete(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_active(GLenum texture) {
    cap.active_unit = (GLuint)(texture - GL_TEXTURE0);
}

static void fake_bind(GLenum target, GLuint name) {
    (void)target;
    if (cap.active_unit < 8u)
        cap.bound[cap.active_unit] = name;
}

static void fake_tex_image(GLenum target, GLint level, GLint internalformat, GLsizei width,
                           GLsizei height, GLint border, GLenum format, GLenum type,
                           const void *pixels) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)border;
    (void)format;
    (void)type;
    (void)pixels;
}

static void fake_tex_sub(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h,
                         GLenum format, GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)format;
    (void)type;
    (void)pixels;
    ++cap.subimage_calls;
}

static void fake_tex_param(GLenum target, GLenum pname, GLint param) {
    (void)target;
    cap.last_pname = pname;
    cap.last_param = param;
}

static void fake_tex_paramf(GLenum target, GLenum pname, GLfloat param) {
    (void)target;
    cap.last_pname = pname;
    cap.last_fparam = param;
    ++cap.float_param_calls;
}

static void fake_pixel_store(GLenum pname, GLint value) {
    (void)pname;
    (void)value;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static int fail(int n, const char *msg) {
    fprintf(stderr, "DSA texture failure %d: %s\n", n, msg);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj0, *obj1;
    GLuint tex[2] = {0u, 0u};
    unsigned char pixels0[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    unsigned char pixels1[16] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    unsigned char patch[4] = {99, 98, 97, 96};
    GLint iv = 0;
    GLfloat fv = 0.0f;
    GLuint uiv = 0u;
    GLuint border_uiv[4] = {(GLuint)INT_MAX, (GLuint)(INT_MAX / 2), 0u, (GLuint)INT_MAX};

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.next_texture = 700u;
    native.glesGenTextures = fake_gen;
    native.glesDeleteTextures = fake_delete;
    native.glesActiveTexture = fake_active;
    native.glesBindTexture = fake_bind;
    native.glesTexImage2D = fake_tex_image;
    native.glesTexSubImage2D = fake_tex_sub;
    native.glesTexParameteri = fake_tex_param;
    native.glesTexParameterf = fake_tex_paramf;
    native.glesPixelStorei = fake_pixel_store;
    native.glesBindBuffer = fake_bind_buffer;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.unpack_alignment = 4;
    ctx.pack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glCreateTextures(GL_TEXTURE_2D, 2, tex);
    if (!glIsTexture(tex[0]) || !glIsTexture(tex[1]))
        return fail(2, "create textures realizes target/object");
    obj0 = mgl_texture_find(&ctx, tex[0]);
    obj1 = mgl_texture_find(&ctx, tex[1]);
    if (obj0 == NULL || obj1 == NULL || obj0->target != GL_TEXTURE_2D ||
        obj1->target != GL_TEXTURE_2D)
        return fail(3, "target identity");

    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels0);
    glBindTexture(GL_TEXTURE_2D, tex[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels1);
    glBindTexture(GL_TEXTURE_2D, tex[0]);

    glTextureParameteri(tex[1], GL_TEXTURE_MIN_FILTER, (GLint)GL_NEAREST);
    glGetTextureParameteriv(tex[1], GL_TEXTURE_MIN_FILTER, &iv);
    if (iv != (GLint)GL_NEAREST || cap.last_pname != GL_TEXTURE_MIN_FILTER ||
        cap.last_param != (GLint)GL_NEAREST)
        return fail(4, "named parameter");
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &iv);
    if ((GLuint)iv != tex[0] || cap.bound[0] != obj0->native_name)
        return fail(5, "parameter preserves binding");

    glTextureSubImage2D(tex[1], 0, 1, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, patch);
    if (cap.subimage_calls == 0u)
        return fail(6, "named subimage native");
    if (obj1->levels[0].data[4] != 99 || obj1->levels[0].data[5] != 98 ||
        obj1->levels[0].data[6] != 97 || obj1->levels[0].data[7] != 96)
        return fail(7, "named subimage canonical data");
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &iv);
    if ((GLuint)iv != tex[0] || cap.bound[0] != obj0->native_name)
        return fail(8, "subimage preserves active binding");

    glGetTextureLevelParameteriv(tex[1], 0, GL_TEXTURE_WIDTH, &iv);
    if (iv != 2)
        return fail(9, "level width query");
    glGetTextureLevelParameterfv(tex[1], 0, GL_TEXTURE_HEIGHT, &fv);
    if (fv != 2.0f)
        return fail(10, "level float query");
    glGetTextureParameterIuiv(tex[1], GL_TEXTURE_WRAP_S, &uiv);
    if (uiv != GL_REPEAT)
        return fail(11, "integer parameter query");
    glTextureParameterIuiv(tex[1], GL_TEXTURE_BORDER_COLOR, border_uiv);
    glGetTextureParameterIuiv(tex[1], GL_TEXTURE_BORDER_COLOR, border_uiv);
    if (border_uiv[0] != (GLuint)INT_MAX || border_uiv[1] == 0u || border_uiv[2] != 0u ||
        border_uiv[3] != (GLuint)INT_MAX)
        return fail(21, "integer border-color vector query");
    glTextureParameterf(tex[1], GL_TEXTURE_MIN_LOD, -3.25f);
    glGetTextureParameterfv(tex[1], GL_TEXTURE_MIN_LOD, &fv);
    if (fv != -3.25f || cap.last_pname != GL_TEXTURE_MIN_LOD || cap.last_fparam != -3.25f ||
        cap.float_param_calls != 1u)
        return fail(18, "named float LOD parameter precision");
    glTextureParameterf(tex[1], GL_TEXTURE_PRIORITY, 1.5f);
    glGetTextureParameterfv(tex[1], GL_TEXTURE_PRIORITY, &fv);
    if (fv != 1.0f || cap.float_param_calls != 1u)
        return fail(19, "named priority interface ownership/clamp");
    glGetTextureParameteriv(tex[1], GL_TEXTURE_RESIDENT, &iv);
    if (iv != (GLint)GL_TRUE)
        return fail(20, "named resident query");

    glActiveTexture(GL_TEXTURE0);
    glBindTextureUnit(2u, tex[1]);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &iv);
    if (iv != (GLint)GL_TEXTURE0)
        return fail(12, "bind texture unit preserves active selector");
    if (ctx.texture_2d_binding[2] != tex[1] || cap.bound[2] != obj1->native_name ||
        cap.active_unit != 0u)
        return fail(13, "bind texture unit isolated unit state");

    glTextureParameteri(0xdeadbeefu, GL_TEXTURE_MIN_FILTER, (GLint)GL_LINEAR);
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &uiv);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(14, "unknown texture rejected");
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || uiv == 0u)
        return fail(15, "cube DSA creation with prior pending error");
    if (mgl_texture_find(&ctx, uiv) == NULL ||
        mgl_texture_find(&ctx, uiv)->target != GL_TEXTURE_CUBE_MAP)
        return fail(16, "cube DSA target identity");
    glBindTextureUnit(3u, uiv);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || ctx.texture_cube_binding[3] != uiv)
        return fail(17, "cube DSA unit binding");

    glDeleteTextures(1, &uiv);
    glDeleteTextures(2, tex);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL DSA texture unit test: PASS");
    return 0;
}

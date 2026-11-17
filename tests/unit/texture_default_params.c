#include "mgl/gl.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static GLuint next_native_texture = 300u;
static GLenum native_active_texture = GL_TEXTURE0;
static GLuint native_bound_2d = 0u;
static GLenum native_last_pname = 0u;
static GLint native_last_iparam = 0;
static GLfloat native_last_fparam = 0.0f;
static unsigned native_image_calls = 0u;

static void fake_gen_textures(GLsizei n, GLuint *textures) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        textures[i] = next_native_texture++;
}

static void fake_delete_textures(GLsizei n, const GLuint *textures) {
    (void)n;
    (void)textures;
}

static void fake_active_texture(GLenum texture) {
    native_active_texture = texture;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    if (target == GL_TEXTURE_2D)
        native_bound_2d = texture;
}

static void fake_tex_parameteri(GLenum target, GLenum pname, GLint param) {
    (void)target;
    native_last_pname = pname;
    native_last_iparam = param;
}

static void fake_tex_parameterf(GLenum target, GLenum pname, GLfloat param) {
    (void)target;
    native_last_pname = pname;
    native_last_fparam = param;
}

static void fake_pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
}

static void fake_tex_image_2d(GLenum target, GLint level, GLint internalformat, GLsizei width,
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
    ++native_image_calls;
}

static int nearf(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 1.0e-6f;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "default texture/parameter failure %d: %s\n", code, message);
    return code;
}

static void init_context(struct mgl_context *ctx, struct mgl_native_dispatch *native,
                         struct mgl_share_group *group) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->native_dispatch = native;
    ctx->share_group = group;
    mgl_error_init(&ctx->errors);
    mgl_compat_state_init(&ctx->compat);
    mgl_vertex_array_init(&ctx->default_vao);
    ctx->current_vao = &ctx->default_vao;
    ctx->unpack_alignment = 4;
    ctx->pack_alignment = 4;
}

int main(void) {
    struct mgl_context a, b;
    struct mgl_native_dispatch native;
    struct mgl_share_group *group;
    GLuint named = 0u;
    GLuint list = 0u;
    GLint iv = 0;
    GLfloat fv = 0.0f;
    const unsigned char pixel[4] = {1u, 2u, 3u, 4u};

    memset(&native, 0, sizeof(native));
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexParameteri = fake_tex_parameteri;
    native.glesTexParameterf = fake_tex_parameterf;
    native.glesPixelStorei = fake_pixel_store;
    native.glesTexImage2D = fake_tex_image_2d;

    group = mgl_share_group_create();
    if (group == NULL)
        return fail(1, "share group");
    init_context(&a, &native, group);
    mgl_share_group_retain(group);
    init_context(&b, &native, group);

    mgl_context_set_current_for_test(&a);
    if (glIsTexture(0u) != GL_FALSE)
        return fail(2, "name zero must not be an IsTexture object");
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &iv);
    if (iv != (GLint)GL_NEAREST_MIPMAP_LINEAR || mgl_error_take(&a.errors) != GL_NO_ERROR)
        return fail(3, "default 2D parameter defaults/query");

    glActiveTexture(GL_TEXTURE1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)GL_CLAMP_TO_EDGE);
    if (native_bound_2d != 0u || native_last_pname != GL_TEXTURE_WRAP_S ||
        native_last_iparam != (GLint)GL_CLAMP_TO_EDGE)
        return fail(4, "default-object native realization");
    glActiveTexture(GL_TEXTURE3);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &iv);
    if (iv != (GLint)GL_CLAMP_TO_EDGE)
        return fail(5, "default object is common across texture units");

    glMultiTexParameterf(GL_TEXTURE5, GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, -2.5f);
    glGetMultiTexParameterfv(GL_TEXTURE5, GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, &fv);
    if (!nearf(fv, -2.5f) || a.compat.active_texture != 3u ||
        native_active_texture != GL_TEXTURE3 || native_last_pname != GL_TEXTURE_MIN_LOD ||
        !nearf(native_last_fparam, -2.5f))
        return fail(6, "explicit-unit float LOD/default selector isolation");

    glMultiTexParameteri(GL_TEXTURE6, GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
                         (GLint)GL_COMPARE_R_TO_TEXTURE);
    glMultiTexParameteri(GL_TEXTURE6, GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, (GLint)GL_GEQUAL);
    glGetMultiTexParameteriv(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, &iv);
    if (iv != (GLint)GL_COMPARE_REF_TO_TEXTURE)
        return fail(7, "default compare mode state");
    glGetMultiTexParameteriv(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, &iv);
    if (iv != (GLint)GL_GEQUAL)
        return fail(8, "default compare func state");

    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    if (mgl_error_take(&a.errors) != GL_NO_ERROR || native_image_calls != 1u)
        return fail(9, "default object image upload");
    glActiveTexture(GL_TEXTURE7);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &iv);
    if (iv != 1)
        return fail(10, "default image shared across texture units");

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 4.5f);
    list = glGenLists(1);
    if (list == 0u)
        return fail(11, "list allocation");
    glNewList(list, GL_COMPILE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 7.25f);
    glEndList();
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, &fv);
    if (!nearf(fv, 4.5f))
        return fail(12, "GL_COMPILE executed float texture parameter");
    glCallList(list);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, &fv);
    if (!nearf(fv, 7.25f))
        return fail(13, "float texture parameter list replay precision");

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, &iv);
    if (iv != GL_RED || mgl_error_take(&a.errors) != GL_NO_ERROR)
        return fail(19, "default texture swizzle");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ZERO);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, &iv);
    if (iv != GL_ONE || native_last_pname != GL_TEXTURE_SWIZZLE_A ||
        native_last_iparam != GL_ZERO || mgl_error_take(&a.errors) != GL_NO_ERROR)
        return fail(20, "swizzle native forwarding and querying");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_NEAREST);
    if (mgl_error_take(&a.errors) != GL_INVALID_OPERATION)
        return fail(21, "invalid swizzle value error");

    glGenTextures(1, &named);
    glBindTexture(GL_TEXTURE_2D, named);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)GL_MIRRORED_REPEAT);
    {
        const GLint set_mask[4] = {GL_BLUE, GL_ONE, GL_RED, GL_ZERO};
        const GLfloat float_mask[4] = {(GLfloat)GL_RED, (GLfloat)GL_BLUE, (GLfloat)GL_ALPHA,
                                       (GLfloat)GL_ONE};
        const GLint invalid_mask[4] = {GL_RED, GL_GREEN, GL_NEAREST, GL_ALPHA};
        GLint actual[4] = {-1, -1, -1, -1};
        GLfloat actual_f[4] = {-1.f, -1.f, -1.f, -1.f};
        glTextureParameteriv(named, GL_TEXTURE_SWIZZLE_RGBA, set_mask);
        glGetTextureParameteriv(named, GL_TEXTURE_SWIZZLE_RGBA, actual);
        if (memcmp(actual, set_mask, sizeof(actual)) != 0 ||
            mgl_error_take(&a.errors) != GL_NO_ERROR)
            return fail(22, "named RGBA swizzle integer roundtrip");
        glGetTextureParameterfv(named, GL_TEXTURE_SWIZZLE_RGBA, actual_f);
        if (actual_f[0] != (GLfloat)GL_BLUE || actual_f[1] != (GLfloat)GL_ONE ||
            actual_f[2] != (GLfloat)GL_RED || actual_f[3] != (GLfloat)GL_ZERO ||
            mgl_error_take(&a.errors) != GL_NO_ERROR)
            return fail(23, "named RGBA swizzle float getter");
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, actual);
        if (memcmp(actual, set_mask, sizeof(actual)) != 0)
            return fail(24, "bound RGBA swizzle getter");
        glTextureParameterfv(named, GL_TEXTURE_SWIZZLE_RGBA, float_mask);
        glGetTextureParameteriv(named, GL_TEXTURE_SWIZZLE_RGBA, actual);
        if (actual[0] != GL_RED || actual[1] != GL_BLUE || actual[2] != GL_ALPHA ||
            actual[3] != GL_ONE)
            return fail(25, "named RGBA swizzle float setter");
        glTextureParameteriv(named, GL_TEXTURE_SWIZZLE_RGBA, invalid_mask);
        if (mgl_error_take(&a.errors) != GL_INVALID_ENUM)
            return fail(26, "invalid named RGBA swizzle rejected");
        glGetTextureParameteriv(named, GL_TEXTURE_SWIZZLE_RGBA, actual);
        if (actual[0] != GL_RED || actual[1] != GL_BLUE || actual[2] != GL_ALPHA ||
            actual[3] != GL_ONE)
            return fail(27, "invalid swizzle preserved all four components");
    }
    glDeleteTextures(1, &named);
    if (native_bound_2d != 0u)
        return fail(14, "deleting bound texture did not restore native default");
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &iv);
    if (iv != (GLint)GL_CLAMP_TO_EDGE)
        return fail(15, "deleting bound texture lost default state");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, 0x7fffffff);
    if (mgl_error_take(&a.errors) != GL_INVALID_ENUM)
        return fail(16, "invalid compare func validation");

    mgl_context_set_current_for_test(&b);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &iv);
    if (iv != (GLint)GL_REPEAT)
        return fail(17, "default texture state leaked across shared contexts");
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &iv);
    if (iv != 0)
        return fail(18, "default texture image leaked across shared contexts");

    mgl_context_set_current_for_test(NULL);
    mgl_texture_release_context_defaults(&a);
    mgl_texture_release_context_defaults(&b);
    mgl_share_group_release(group);
    mgl_share_group_release(group);
    puts("mGL default texture/parameter unit test: PASS");
    return 0;
}

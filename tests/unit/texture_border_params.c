#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static GLuint next_native_texture = 40u;
static GLenum native_active = GL_TEXTURE0;
static GLuint native_bound_2d = 0u;
static GLenum native_last_pname = 0u;
static GLint native_last_iparam = 0;
static unsigned native_border_calls = 0u;
static int native_anisotropy_available = 1;
static GLfloat native_last_fparam = 0.0f;
static GLfloat native_last_border[4];

static const GLubyte *fake_get_string(GLenum name) {
    return name == GL_EXTENSIONS
               ? (const GLubyte *)(native_anisotropy_available
                                       ? "GL_EXT_texture_filter_anisotropic" : "")
               : NULL;
}

static void fake_get_floatv(GLenum pname, GLfloat *value) {
    if (pname == GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT)
        *value = 8.0f;
}

static void fake_get_integerv(GLenum pname, GLint *value) {
    if (pname == GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT)
        *value = 8;
}

static void fake_gen_textures(GLsizei n, GLuint *textures) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        textures[i] = ++next_native_texture;
}

static void fake_delete_textures(GLsizei n, const GLuint *textures) {
    (void)n;
    (void)textures;
}

static void fake_active_texture(GLenum texture) {
    native_active = texture;
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

static void fake_tex_parameterfv(GLenum target, GLenum pname, const GLfloat *params) {
    (void)target;
    if (pname == GL_TEXTURE_BORDER_COLOR) {
        ++native_border_calls;
        memcpy(native_last_border, params, sizeof(native_last_border));
    }
}

static void fake_tex_parameterf(GLenum target, GLenum pname, GLfloat param) {
    (void)target;
    native_last_pname = pname;
    native_last_fparam = param;
}

static int nearf(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 2.0e-6f;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "texture border/parameter failure %d: %s\n", code, message);
    return code;
}

static void init_context(struct mgl_context *ctx, struct mgl_native_dispatch *native) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->native_dispatch = native;
    ctx->share_group = mgl_share_group_create();
    mgl_error_init(&ctx->errors);
    mgl_compat_state_init(&ctx->compat);
    mgl_vertex_array_init(&ctx->default_vao);
    ctx->current_vao = &ctx->default_vao;
    ctx->unpack_alignment = 4;
    ctx->pack_alignment = 4;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLfloat borderf[4] = {-0.25f, 0.25f, 0.75f, 1.25f};
    GLfloat gotf[4] = {0};
    GLint borderi[4] = {INT_MIN, INT_MAX / 4, INT_MAX / 2, INT_MAX};
    GLint goti[4] = {0};
    GLfloat list_values[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    GLfloat multi_values[4] = {0.6f, 0.5f, 0.4f, 0.3f};
    GLuint texture = 0u;
    GLuint list = 0u;
    GLint iv = 0;

    memset(&native, 0, sizeof(native));
    native.glesGetString = fake_get_string;
    native.glesGetFloatv = fake_get_floatv;
    native.glesGetIntegerv = fake_get_integerv;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexParameteri = fake_tex_parameteri;
    native.glesTexParameterf = fake_tex_parameterf;
    native.glesTexParameterfv = fake_tex_parameterfv;

    init_context(&ctx, &native);
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_context_set_current_for_test(&ctx);

    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderf);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, gotf);
    if (!nearf(gotf[0], 0.0f) || !nearf(gotf[1], 0.25f) || !nearf(gotf[2], 0.75f) ||
        !nearf(gotf[3], 1.0f) || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(2, "float border color clamp/query");

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, goti);
    if (goti[0] != 0 || goti[3] != INT_MAX || goti[1] < INT_MAX / 4 - 2 ||
        goti[1] > INT_MAX / 4 + 2)
        return fail(3, "integer border query conversion");

    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderi);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, gotf);
    if (!nearf(gotf[0], 0.0f) ||
        !nearf(gotf[1], (GLfloat)((double)(INT_MAX / 4) / (double)INT_MAX)) ||
        !nearf(gotf[2], (GLfloat)((double)(INT_MAX / 2) / (double)INT_MAX)) ||
        !nearf(gotf[3], 1.0f))
        return fail(4, "integer border setter conversion");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, INT_MAX / 2);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, gotf);
    if (!nearf(gotf[0], (GLfloat)((double)(INT_MAX / 2) / (double)INT_MAX)))
        return fail(5, "integer priority normalized state");
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, &iv);
    if (iv < INT_MAX / 2 - 2 || iv > INT_MAX / 2 + 2)
        return fail(6, "integer priority query conversion");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)GL_CLAMP);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &iv);
    if (iv != (GLint)GL_CLAMP || native_last_pname != GL_TEXTURE_WRAP_S ||
        native_last_iparam != (GLint)GL_CLAMP_TO_BORDER)
        return fail(7, "legacy clamp to border");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)GL_CLAMP_TO_BORDER);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &iv);
    if (iv != (GLint)GL_CLAMP_TO_BORDER || native_last_iparam != (GLint)GL_CLAMP_TO_BORDER)
        return fail(8, "border clamp native");

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, 0.5f);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(9, "scalar border color must be rejected");

    list = glGenLists(1);
    if (list == 0u)
        return fail(10, "display list allocation");
    glNewList(list, GL_COMPILE);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, list_values);
    glEndList();
    list_values[0] = list_values[1] = list_values[2] = list_values[3] = 0.9f;
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, gotf);
    if (!nearf(gotf[0], 0.0f))
        return fail(11, "compile-only border command executed early");
    glCallList(list);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, gotf);
    if (!nearf(gotf[0], 0.1f) || !nearf(gotf[1], 0.2f) || !nearf(gotf[2], 0.3f) ||
        !nearf(gotf[3], 0.4f))
        return fail(12, "display list vector payload ownership");

    glActiveTexture(GL_TEXTURE3);
    glMultiTexParameterfv(GL_TEXTURE5, GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, multi_values);
    memset(gotf, 0, sizeof(gotf));
    glGetMultiTexParameterfv(GL_TEXTURE1, GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, gotf);
    if (!nearf(gotf[0], 0.6f) || !nearf(gotf[1], 0.5f) || !nearf(gotf[2], 0.4f) ||
        !nearf(gotf[3], 0.3f) || ctx.compat.active_texture != 3u || native_active != GL_TEXTURE3)
        return fail(13, "explicit-unit border state/selector isolation");

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTextureParameterfv(texture, GL_TEXTURE_BORDER_COLOR, borderf);
    memset(gotf, 0, sizeof(gotf));
    glGetTextureParameterfv(texture, GL_TEXTURE_BORDER_COLOR, gotf);
    if (!nearf(gotf[0], 0.0f) || !nearf(gotf[1], 0.25f) || !nearf(gotf[2], 0.75f) ||
        !nearf(gotf[3], 1.0f))
        return fail(14, "DSA border color state/query");
    glTextureParameteri(texture, GL_TEXTURE_PRIORITY, INT_MAX);
    glGetTextureParameterfv(texture, GL_TEXTURE_PRIORITY, gotf);
    if (!nearf(gotf[0], 1.0f))
        return fail(15, "DSA integer priority");

    if (strstr((const char *)glGetString(GL_EXTENSIONS),
               "GL_EXT_texture_filter_anisotropic") == NULL)
        return fail(17, "native anisotropic extension advertisement");
    iv = 0;
    glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &iv);
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, gotf);
    if (iv != 8 || !nearf(gotf[0], 8.0f) || glGetError() != GL_NO_ERROR)
        return fail(18, "native anisotropy limit queries");
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gotf);
    if (!nearf(gotf[0], 1.0f))
        return fail(19, "default anisotropy");
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2.5f);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gotf);
    if (glGetError() != GL_NO_ERROR || !nearf(gotf[0], 2.5f) ||
        native_last_pname != GL_TEXTURE_MAX_ANISOTROPY_EXT ||
        !nearf(native_last_fparam, 2.5f))
        return fail(20, "fractional anisotropy state and native forwarding");
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 40.0f);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gotf);
    if (glGetError() != GL_NO_ERROR || !nearf(gotf[0], 8.0f))
        return fail(21, "anisotropy clamped to native limit");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(22, "invalid anisotropy value");
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gotf);
    if (!nearf(gotf[0], 8.0f))
        return fail(23, "invalid anisotropy did not change state");
    glMultiTexParameterf(GL_TEXTURE0, GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 3.5f);
    glGetMultiTexParameterfv(GL_TEXTURE0, GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gotf);
    if (glGetError() != GL_NO_ERROR || !nearf(gotf[0], 3.5f) ||
        ctx.compat.active_texture != 3u || native_active != GL_TEXTURE3)
        return fail(24, "multitexture anisotropy state isolation");
    glTextureParameterf(texture, GL_TEXTURE_MAX_ANISOTROPY_EXT, 4.5f);
    glGetTextureParameterfv(texture, GL_TEXTURE_MAX_ANISOTROPY_EXT, gotf);
    if (glGetError() != GL_NO_ERROR || !nearf(gotf[0], 4.5f))
        return fail(25, "DSA anisotropy");
    glTextureParameteriEXT(texture, GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 5);
    glGetTextureParameterivEXT(texture, GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &iv);
    if (glGetError() != GL_NO_ERROR || iv != 5)
        return fail(26, "EXT DSA anisotropy");
    native_anisotropy_available = 0;
    if (strstr((const char *)glGetString(GL_EXTENSIONS),
               "GL_EXT_texture_filter_anisotropic") != NULL)
        return fail(27, "unavailable extension not advertised");
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2.0f);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(28, "unavailable anisotropy setter");
    iv = -19;
    glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &iv);
    if (glGetError() != GL_INVALID_ENUM || iv != -19)
        return fail(29, "unavailable anisotropy limit");
    native_anisotropy_available = 1;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    if (native_last_iparam != (GLint)GL_CLAMP_TO_BORDER)
        return fail(32, "border clamp wrap forwarded");
    native_border_calls = 0u;
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, list_values);
    if (native_border_calls != 1u || !nearf(native_last_border[0], 0.9f))
        return fail(33, "border float vector forwarded");
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, multi_values);
    if (native_border_calls != 2u || !nearf(native_last_border[0], 0.6f))
        return fail(34, "repeated border color upload");

    if (native_bound_2d == 0u)
        return fail(16, "named texture native realization");

    glDeleteTextures(1, &texture);
    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_texture_release_context_defaults(&ctx);
    mgl_share_group_release(ctx.share_group);
    puts("mGL texture border/parameter unit test: PASS");
    return 0;
}

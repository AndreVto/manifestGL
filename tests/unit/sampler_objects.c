#include "mgl/gl.h"
#include "core/context.h"
#include "objects/sampler.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_sampler;
    GLuint bound[8];
    GLenum last_pname;
    GLint last_i;
    GLfloat last_f;
    GLenum last_tex_target;
    unsigned bind_calls;
    unsigned tex_param_calls;
    unsigned tex_param_f_calls;
    unsigned signed_border_calls;
    unsigned unsigned_border_calls;
    GLint last_signed_border[4];
    GLuint last_unsigned_border[4];
} cap;

static int has_anisotropy = 1;

static const GLubyte *fake_get_string(GLenum pname) {
    return pname == GL_EXTENSIONS
               ? (const GLubyte *)(has_anisotropy ? "GL_EXT_texture_filter_anisotropic" : "")
               : NULL;
}

static void fake_get_floatv(GLenum pname, GLfloat *params) {
    if (pname == GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT)
        *params = 8.0f;
}

static void fake_gen(GLsizei n, GLuint *ids) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        ids[i] = ++cap.next_sampler;
}

static void fake_delete(GLsizei n, const GLuint *ids) {
    (void)n;
    (void)ids;
}

static void fake_bind(GLuint unit, GLuint sampler) {
    if (unit < 8u)
        cap.bound[unit] = sampler;
    ++cap.bind_calls;
}

static void fake_parameter_i(GLuint sampler, GLenum pname, GLint value) {
    (void)sampler;
    cap.last_pname = pname;
    cap.last_i = value;
}

static void fake_parameter_f(GLuint sampler, GLenum pname, GLfloat value) {
    (void)sampler;
    cap.last_pname = pname;
    cap.last_f = value;
}

static void fake_parameter_fv(GLuint sampler, GLenum pname, const GLfloat *value) {
    (void)sampler;
    cap.last_pname = pname;
    cap.last_f = value[0];
}

static void fake_parameter_iiv(GLuint sampler, GLenum pname, const GLint *value) {
    (void)sampler;
    if (pname != GL_TEXTURE_BORDER_COLOR || value == NULL)
        return;
    ++cap.signed_border_calls;
    memcpy(cap.last_signed_border, value, sizeof(cap.last_signed_border));
}

static void fake_parameter_iuiv(GLuint sampler, GLenum pname, const GLuint *value) {
    (void)sampler;
    if (pname != GL_TEXTURE_BORDER_COLOR || value == NULL)
        return;
    ++cap.unsigned_border_calls;
    memcpy(cap.last_unsigned_border, value, sizeof(cap.last_unsigned_border));
}

static void fake_tex_parameter_i(GLenum target, GLenum pname, GLint value) {
    cap.last_tex_target = target;
    cap.last_pname = pname;
    cap.last_i = value;
    ++cap.tex_param_calls;
}

static void fake_active_texture(GLenum texture) {
    (void)texture;
}

static void fake_bind_texture(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static int fail(int n, const char *msg) {
    fprintf(stderr, "sampler failure %d: %s\n", n, msg);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_sampler *obj;
    GLuint ids[2] = {0u, 0u};
    GLint iv = 0;
    GLint signed_border[4] = {INT_MIN, -123456789, 123456789, INT_MAX};
    GLint signed_out[4] = {0, 0, 0, 0};
    GLuint unsigned_border[4] = {UINT_MAX, UINT_MAX - 1u, UINT32_C(0x80000000), 17u};
    GLuint unsigned_out[4] = {0u, 0u, 0u, 0u};
    GLfloat fv[4] = {0};

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGetString = fake_get_string;
    native.glesGetFloatv = fake_get_floatv;
    native.glesGenSamplers = fake_gen;
    native.glesDeleteSamplers = fake_delete;
    native.glesBindSampler = fake_bind;
    native.glesSamplerParameteri = fake_parameter_i;
    native.glesSamplerParameterf = fake_parameter_f;
    native.glesSamplerParameterfv = fake_parameter_fv;
    native.glesSamplerParameterIiv = fake_parameter_iiv;
    native.glesSamplerParameterIuiv = fake_parameter_iuiv;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexParameteri = fake_tex_parameter_i;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glGenSamplers(2, ids);
    if (ids[0] == 0u || ids[1] == 0u || ids[0] == ids[1] || !glIsSampler(ids[0]))
        return fail(2, "generation/object identity");
    glGetSamplerParameteriv(ids[0], GL_TEXTURE_MIN_FILTER, &iv);
    if (iv != (GLint)GL_NEAREST_MIPMAP_LINEAR)
        return fail(3, "default min filter");
    glGetSamplerParameterfv(ids[0], GL_TEXTURE_MIN_LOD, fv);
    if (fv[0] != -1000.0f)
        return fail(4, "default min lod");

    glGetSamplerParameterfv(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT, fv);
    if (glGetError() != GL_NO_ERROR || fv[0] != 1.0f)
        return fail(25, "default sampler anisotropy");
    glSamplerParameterf(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT, 2.5f);
    glGetSamplerParameterfv(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT, fv);
    if (glGetError() != GL_NO_ERROR || fv[0] != 2.5f ||
        cap.last_pname != GL_TEXTURE_MAX_ANISOTROPY_EXT || cap.last_f != 2.5f)
        return fail(26, "fractional sampler anisotropy state/native");
    glSamplerParameteri(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT, 25);
    glGetSamplerParameterfv(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT, fv);
    if (glGetError() != GL_NO_ERROR || fv[0] != 8.0f)
        return fail(27, "sampler anisotropy limit clamp");
    glSamplerParameterf(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT, 0.0f);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(28, "sampler anisotropy minimum validation");
    glGetSamplerParameterfv(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT, fv);
    if (glGetError() != GL_NO_ERROR || fv[0] != 8.0f)
        return fail(29, "sampler anisotropy retains valid state");
    has_anisotropy = 0;
    glSamplerParameterf(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT, 2.0f);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(30, "unsupported native sampler anisotropy");
    has_anisotropy = 1;

    glSamplerParameterf(ids[0], GL_TEXTURE_MAX_LOD, 2.5f);
    glGetSamplerParameteriv(ids[0], GL_TEXTURE_MAX_LOD, &iv);
    if (glGetError() != GL_NO_ERROR || iv != 3)
        return fail(22, "positive halfway LOD getter");
    glSamplerParameterf(ids[0], GL_TEXTURE_MAX_LOD, -2.5f);
    glGetSamplerParameteriv(ids[0], GL_TEXTURE_MAX_LOD, &iv);
    if (glGetError() != GL_NO_ERROR || iv != -3)
        return fail(23, "negative halfway LOD getter");

    glSamplerParameteri(ids[0], GL_TEXTURE_MIN_FILTER, (GLint)GL_LINEAR);
    glSamplerParameteri(ids[0], GL_TEXTURE_WRAP_S, (GLint)GL_CLAMP);
    glSamplerParameterf(ids[0], GL_TEXTURE_MAX_LOD, 12.5f);
    {
        const GLfloat border[4] = {0.25f, 0.5f, 0.75f, 1.0f};
        glSamplerParameterfv(ids[0], GL_TEXTURE_BORDER_COLOR, border);
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(5, "parameter setters");
    glGetSamplerParameteriv(ids[0], GL_TEXTURE_MIN_FILTER, &iv);
    if (iv != (GLint)GL_LINEAR)
        return fail(6, "canonical filter query");
    glGetSamplerParameterfv(ids[0], GL_TEXTURE_MAX_LOD, fv);
    if (fv[0] != 12.5f)
        return fail(7, "canonical lod query");
    glGetSamplerParameterfv(ids[0], GL_TEXTURE_BORDER_COLOR, fv);
    if (fv[0] != 0.25f || fv[1] != 0.5f || fv[2] != 0.75f || fv[3] != 1.0f)
        return fail(8, "border query");

    glSamplerParameterIiv(ids[0], GL_TEXTURE_BORDER_COLOR, signed_border);
    memset(signed_out, 0, sizeof(signed_out));
    glGetSamplerParameterIiv(ids[0], GL_TEXTURE_BORDER_COLOR, signed_out);
    obj = mgl_sampler_find(&ctx, ids[0]);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL ||
        obj->border_color_type != GL_INT ||
        memcmp(signed_out, signed_border, sizeof(signed_border)) != 0 ||
        cap.signed_border_calls != 1u ||
        memcmp(cap.last_signed_border, signed_border, sizeof(signed_border)) != 0)
        return fail(19, "exact signed integer border state/native");

    glSamplerParameterIuiv(ids[0], GL_TEXTURE_BORDER_COLOR, unsigned_border);
    memset(unsigned_out, 0, sizeof(unsigned_out));
    glGetSamplerParameterIuiv(ids[0], GL_TEXTURE_BORDER_COLOR, unsigned_out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj->border_color_type != GL_UNSIGNED_INT ||
        memcmp(unsigned_out, unsigned_border, sizeof(unsigned_border)) != 0 ||
        cap.unsigned_border_calls != 1u ||
        memcmp(cap.last_unsigned_border, unsigned_border, sizeof(unsigned_border)) != 0)
        return fail(20, "exact unsigned integer border state/native");

    signed_out[0] = 0x13572468;
    glGetSamplerParameterIiv(0xdeadbeefu, GL_TEXTURE_BORDER_COLOR, signed_out);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || signed_out[0] != 0x13572468)
        return fail(21, "invalid sampler integer query output preservation");

    glBindSampler(2u, ids[0]);
    glActiveTexture(GL_TEXTURE2);
    glGetIntegerv(GL_SAMPLER_BINDING, &iv);
    if ((GLuint)iv != ids[0] || obj == NULL || cap.bound[2] != obj->native_name)
        return fail(9, "per-unit binding/query/native");
    {
        ctx.texture_bindings_dirty = GL_TRUE;
        ctx.texture_bindings_dirty_units[2] = 1u;
        mgl_texture_realize_bindings(&ctx);
        if (cap.bound[2] != obj->native_name)
            return fail(24, "texture refresh preserved native sampler binding");
    }

    glBindSamplers(3u, 2, ids);
    if (ctx.sampler_binding[3] != ids[0] || ctx.sampler_binding[4] != ids[1])
        return fail(10, "multi-bind");
    glBindSamplers(3u, 2, NULL);
    if (ctx.sampler_binding[3] != 0u || ctx.sampler_binding[4] != 0u)
        return fail(11, "multi-unbind");
    glBindSamplers(MGL_TEXTURE_UNITS, 0, NULL);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(16, "zero-count upper-bound multibind");
    glBindSamplers(MGL_TEXTURE_UNITS + 1u, 0, NULL);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(17, "zero-count out-of-range multibind");

    glSamplerParameteri(ids[0], GL_TEXTURE_MAG_FILTER, (GLint)GL_NEAREST_MIPMAP_LINEAR);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(12, "invalid mag filter");
    glBindSampler(0u, 0xdeadbeefu);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(13, "invalid sampler binding");

    glDeleteSamplers(1, &ids[0]);
    if (glIsSampler(ids[0]) || ctx.sampler_binding[0] != 0u)
        return fail(15, "delete/unbind");
    glDeleteSamplers(1, &ids[1]);

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL sampler object unit test: PASS");
    return 0;
}

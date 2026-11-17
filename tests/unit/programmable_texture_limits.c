#include "mgl/gl.h"
#include "core/context.h"
#include "texture/texture_store.h"
#include <stdio.h>
#include <string.h>

static GLenum active_unit;
static unsigned bound_units[128];
static unsigned bound_samplers[128];
static GLuint next_name = 1000u;

static MGL_EGLBoolean fake_make_current(MGL_EGLDisplay display, MGL_EGLSurface draw,
                                        MGL_EGLSurface read, MGL_EGLContext context) {
    (void)display;
    (void)draw;
    (void)read;
    (void)context;
    return MGL_EGL_TRUE;
}

static void fake_get_integer(GLenum pname, GLint *value) {
    switch (pname) {
        case GL_MAJOR_VERSION:
            *value = 3;
            break;
        case GL_MINOR_VERSION:
            *value = 2;
            break;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
            *value = 96;
            break;
        case GL_MAX_TEXTURE_IMAGE_UNITS:
            *value = 24;
            break;
        case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
            *value = 32;
            break;
        default:
            *value = 0;
            break;
    }
}

static void fake_active(GLenum value) {
    active_unit = value - GL_TEXTURE0;
}

static void fake_gen(GLsizei count, GLuint *names) {
    GLsizei i;
    for (i = 0; i < count; ++i)
        names[i] = ++next_name;
}

static void fake_bind(GLenum target, GLuint name) {
    if (target == GL_TEXTURE_2D && active_unit < 128u)
        bound_units[active_unit] = name;
}

static void fake_bind_sampler(GLuint unit, GLuint sampler) {
    if (unit < 128u)
        bound_samplers[unit] = sampler;
}

static int failed(int number, const char *reason) {
    fprintf(stderr, "programmable texture limit test %d: %s\n", number, reason);
    return number;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    static const unsigned units[] = {8u, 15u, 31u, 47u, 95u};
    unsigned i;
    GLint value = -1;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.eglMakeCurrent = fake_make_current;
    native.glesGetIntegerv = fake_get_integer;
    native.glesActiveTexture = fake_active;
    native.glesGenTextures = fake_gen;
    native.glesBindTexture = fake_bind;
    native.glesBindSampler = fake_bind_sampler;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return failed(1, "share group creation");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    if (!mgl_context_make_current(&ctx))
        return failed(2, "mock GLES 3.2 bind");
    if (ctx.programmable_texture_units != 96u || ctx.fragment_texture_image_units != 24 ||
        ctx.vertex_texture_image_units != 32)
        return failed(3, "negotiated limits");
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &value);
    if (value != 8)
        return failed(4, "legacy fixed stage limit");
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &value);
    if (value != 24)
        return failed(5, "fragment stage limit");
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &value);
    if (value != 32)
        return failed(6, "vertex stage limit");
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &value);
    if (value != 96)
        return failed(7, "combined unit limit");
    for (i = 0u; i < sizeof(units) / sizeof(units[0]); ++i) {
        unsigned unit = units[i];
        GLuint name = 500u + unit;
        struct mgl_texture *texture;
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, name);
        texture = mgl_texture_find(&ctx, name);
        if (texture == NULL || ctx.texture_2d_binding[unit] != name)
            return failed(8, "independent high-unit binding");
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
        if (value != (GLint)name)
            return failed(9, "active texture binding query");
        mgl_texture_realize_bindings(&ctx);
        if (bound_units[unit] != texture->native_name)
            return failed(10, "native realization for high unit");
        glBindSampler(unit, 0u);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return failed(11, "sampler validation for high unit");
    }
    glBindTextureUnit(47u, 0u);
    if (ctx.texture_2d_binding[47] != 0u || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return failed(18, "direct high-unit bind");
    ctx.texture_buffer_binding[47u] = 721u;
    glBindTextureUnit(47u, 0u);
    if (ctx.texture_buffer_binding[47u] != 0u || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return failed(20, "direct unbind clears texture buffer target");
    glActiveTexture(GL_TEXTURE0 + 95u);
    mgl_texture_bind_native_unit(&ctx, GL_TEXTURE_2D, 0u, 95u);
    if ((ctx.native_texture_known[95u] & 1u) == 0u || ctx.native_texture_names[95u][0] != 0u)
        return failed(21, "high-unit native binding cache");
    mgl_texture_realize_bindings(&ctx);
    ctx.compat.texture_2d_enabled[7u] = GL_TRUE;
    if (mgl_compat_selected_texture_target(&ctx.compat, 7u) != GL_TEXTURE_2D ||
        mgl_compat_selected_texture_target(&ctx.compat, 95u) != 0u)
        return failed(22, "fixed stages cannot index programmable unit 95");
    mgl_texture_realize_fixed_bindings(&ctx);
    ctx.compat.texture_2d_enabled[7u] = GL_FALSE;
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return failed(23, "fixed binding realization with high native limit");
    glBindSamplers(47u, 2, NULL);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || ctx.sampler_binding[47] != 0u ||
        ctx.sampler_binding[48] != 0u)
        return failed(19, "multi-unit sampler binding");
    glActiveTexture(GL_TEXTURE0 + 96u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return failed(12, "reject out-of-range active unit");
    glBindSampler(96u, 0u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return failed(13, "reject out-of-range sampler unit");
    glBindSamplers(95u, 2, NULL);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return failed(14, "reject multi-bind crossing limit");
    glActiveTexture(GL_TEXTURE0 + 47u);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return failed(15, "legacy texture matrix bounds");
    glMatrixMode(GL_MODELVIEW);
    glActiveTexture(GL_TEXTURE0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return failed(16, "restore fixed texture selector");
    mgl_context_set_current_for_test(NULL);
    return 0;
}

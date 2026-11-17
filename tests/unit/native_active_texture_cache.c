#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

static GLenum gles_active = GL_TEXTURE0;
static unsigned gles_active_calls;
static unsigned bind_calls;
static GLenum last_bound_unit;
static GLuint last_bound_name;
static GLenum last_parameter_unit;
static GLuint next_texture = 101u;

static void fake_active(GLenum texture) {
    gles_active = texture;
    ++gles_active_calls;
}

static void fake_gen(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = next_texture++;
}

static void fake_bind(GLenum target, GLuint name) {
    (void)target;
    ++bind_calls;
    last_bound_unit = gles_active;
    last_bound_name = name;
}

static void fake_parameter(GLenum target, GLenum pname, GLint value) {
    (void)target;
    (void)pname;
    (void)value;
    last_parameter_unit = gles_active;
}

static const GLubyte *fake_get_string(GLenum name) {
    (void)name;
    return (const GLubyte *)"";
}

static int fail(int n, const char *why) {
    fprintf(stderr, "native active texture cache failure %d: %s\n", n, why);
    return n;
}

static void init_ctx(struct mgl_context *ctx, struct mgl_native_dispatch *b,
                     struct mgl_share_group *share) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->native_dispatch = b;
    ctx->share_group = share;
    mgl_error_init(&ctx->errors);
    mgl_compat_state_init(&ctx->compat);
    mgl_vertex_array_init(&ctx->default_vao);
    ctx->current_vao = &ctx->default_vao;
}

int main(void) {
    struct mgl_context a, b;
    struct mgl_native_dispatch native;
    struct mgl_share_group *share;
    GLuint tex;
    GLuint tex_1d;
    GLuint list;
    unsigned before;
    unsigned binds_before;

    memset(&native, 0, sizeof(native));
    native.glesActiveTexture = fake_active;
    native.glesGenTextures = fake_gen;
    native.glesBindTexture = fake_bind;
    native.glesTexParameteri = fake_parameter;
    native.glesGetString = fake_get_string;
    share = mgl_share_group_create();
    if (share == NULL)
        return fail(1, "share group allocation");
    init_ctx(&a, &native, share);
    mgl_share_group_retain(share);
    init_ctx(&b, &native, share);
    mgl_context_set_current_for_test(&a);

    /* An unknown native state must dispatch, even when canonical GL selects unit zero. */
    glActiveTexture(GL_TEXTURE0);
    if (gles_active_calls != 1u || gles_active != GL_TEXTURE0)
        return fail(2, "initial selector");
    glActiveTexture(GL_TEXTURE0);
    glActiveTexture(GL_TEXTURE0);
    if (gles_active != GL_TEXTURE0)
        return fail(3, "active texture selection");
    glActiveTexture(GL_TEXTURE3);
    glActiveTexture(GL_TEXTURE3);
    if (gles_active != GL_TEXTURE3)
        return fail(4, "interface unit change");

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    if (bind_calls == 0u || last_bound_unit != GL_TEXTURE3 ||
        last_bound_name == 0u)
        return fail(5, "canonical bind did not reuse active unit");

    /* GL functions can select another unit without modifying the interface selector. */
    before = gles_active_calls;
    glMultiTexParameteri(GL_TEXTURE5, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    if (gles_active != GL_TEXTURE3 ||
        last_parameter_unit != GL_TEXTURE5 || a.compat.active_texture != 3u)
        return fail(6, "temporary explicit-unit selector was not restored");
    before = gles_active_calls;
    glMultiTexParameteri(GL_TEXTURE3, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (gles_active != GL_TEXTURE3 ||
        last_parameter_unit != GL_TEXTURE3)
        return fail(7, "explicit current-unit calls should not dispatch");

    /* Canonical and internal bindings must respect the same actual GLES selector. */
    mgl_native_select_active_texture(&a, GL_TEXTURE7);
    if (gles_active != GL_TEXTURE7 || a.compat.active_texture != 3u)
        return fail(8, "internal selector changed canonical state");
    before = gles_active_calls;
    glBindTexture(GL_TEXTURE_2D, 0u);
    if (last_bound_unit != GL_TEXTURE3 ||
        gles_active != GL_TEXTURE3)
        return fail(9, "bind did not recover from an internal selector change");

    /* Texture-realization passes may touch other units, but must restore the interface unit. */
    mgl_texture_mark_all_bindings_dirty(&a);
    binds_before = bind_calls;
    mgl_texture_realize_bindings(&a);
    if (gles_active != GL_TEXTURE3 || a.compat.active_texture != 3u || bind_calls == binds_before)
        return fail(10, "realization didn't restore actual active unit");
    mgl_texture_realize_bindings(&a);
    if (gles_active != GL_TEXTURE3)
        return fail(11, "clean realization changed active unit");

    /* Unexecuted display-list commands and rejected GL calls issue no native dispatch. */
    before = gles_active_calls;
    glActiveTexture(GL_TEXTURE0 + MGL_TEXTURE_UNITS);
    if (mgl_error_take(&a.errors) != GL_INVALID_ENUM || gles_active_calls != before)
        return fail(12, "invalid active texture reached native");
    list = glGenLists(1);
    if (list == 0u)
        return fail(13, "display list creation");
    glNewList(list, GL_COMPILE);
    glActiveTexture(GL_TEXTURE2);
    glEndList();
    if (gles_active_calls != before || a.compat.active_texture != 3u)
        return fail(14, "compile-only active unit executed");
    glCallList(list);
    if (gles_active_calls != before + 1u || gles_active != GL_TEXTURE2 ||
        a.compat.active_texture != 2u)
        return fail(15, "active-unit display-list replay");
    glDeleteLists(list, 1);

    /* A newly current EGL context must invalidate knowledge of its selector. */
    mgl_context_set_current_for_test(&b);
    before = gles_active_calls;
    glActiveTexture(GL_TEXTURE2);
    if (gles_active_calls != before + 1u || gles_active != GL_TEXTURE2)
        return fail(16, "new context assumed known native state");
    glActiveTexture(GL_TEXTURE2);
    if (gles_active != GL_TEXTURE2)
        return fail(17, "second context selector");
    mgl_context_set_current_for_test(&a);
    before = gles_active_calls;
    glActiveTexture(GL_TEXTURE2);
    if (gles_active_calls != before + 1u)
        return fail(18, "returning context must resynchronize");

    glGenTextures(1, &tex_1d);
    glBindTexture(GL_TEXTURE_1D, tex_1d);
    if (a.texture_1d_binding[2] != tex_1d || a.texture_2d_multisample_array_binding[2] != 0u)
        return fail(19, "1d bind changed multisample array binding");

    mgl_context_set_current_for_test(NULL);
    mgl_texture_release_context_defaults(&a);
    mgl_texture_release_context_defaults(&b);
    mgl_share_group_release(share);
    mgl_share_group_release(share);
    puts("mGL native active texture cache unit test: PASS");
    return 0;
}

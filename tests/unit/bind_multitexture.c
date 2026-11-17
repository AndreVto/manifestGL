#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "texture/texture_store.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static GLuint next_native = 1000u;
static GLuint active_native_unit;
static GLuint bound_native[8][4];
static unsigned bind_calls[8];
static unsigned extension_queries;

static const GLubyte *fake_get_string(GLenum name) {
    (void)name;
    ++extension_queries;
    return (const GLubyte *)"";
}

static void fake_gen(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++next_native;
}

static void fake_sampler(GLuint unit, GLuint sampler) {
    (void)unit;
    (void)sampler;
}

static void fake_active(GLenum texture) {
    active_native_unit = (GLuint)(texture - GL_TEXTURE0);
}

static unsigned target_slot(GLenum target) {
    if (target == GL_TEXTURE_2D)
        return 0u;
    if (target == GL_TEXTURE_3D)
        return 1u;
    if (target == GL_TEXTURE_2D_ARRAY)
        return 2u;
    return 3u;
}

static void fake_bind(GLenum target, GLuint name) {
    if (active_native_unit < 8u) {
        bound_native[active_native_unit][target_slot(target)] = name;
        ++bind_calls[active_native_unit];
    }
}

static int fail(int n, const char *m) {
    fprintf(stderr, "BindMultiTexture failure %d: %s\n", n, m);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj;
    GLuint list;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(bound_native, 0, sizeof(bound_native));
    native.glesGenTextures = fake_gen;
    native.glesActiveTexture = fake_active;
    native.glesBindTexture = fake_bind;
    native.glesGetString = fake_get_string;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (!ctx.share_group)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glActiveTexture(GL_TEXTURE0);
    glBindMultiTexture(GL_TEXTURE0 + 2u, GL_TEXTURE_2D, 77u);
    obj = mgl_texture_find(&ctx, 77u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL ||
        ctx.texture_2d_binding[2] != 77u || ctx.compat.active_texture != 0u ||
        active_native_unit != 0u || bound_native[2][0] != obj->native_name)
        return fail(2, "selector-free bind/create");

    glBindMultiTextureEXT(GL_TEXTURE0 + 2u, GL_TEXTURE_CUBE_MAP, 88u);
    obj = mgl_texture_find(&ctx, 88u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL ||
        ctx.texture_cube_binding[2] != 88u || ctx.texture_2d_binding[2] != 77u ||
        ctx.compat.active_texture != 0u || active_native_unit != 0u ||
        bound_native[2][3] != obj->native_name)
        return fail(3, "EXT alias and independent target binding");

    list = glGenLists(1);
    if (list == 0u)
        return fail(4, "list allocation");
    glNewList(list, GL_COMPILE);
    glBindMultiTexture(GL_TEXTURE0 + 1u, GL_TEXTURE_2D, 99u);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || ctx.texture_2d_binding[1] != 0u ||
        mgl_texture_find(&ctx, 99u) != NULL || ctx.compat.active_texture != 0u)
        return fail(5, "compile suppresses execution");
    glCallList(list);
    obj = mgl_texture_find(&ctx, 99u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL ||
        ctx.texture_2d_binding[1] != 99u || ctx.compat.active_texture != 0u ||
        active_native_unit != 0u || bound_native[1][0] != obj->native_name)
        return fail(6, "explicit unit retained across replay");

    glBindMultiTexture(GL_TEXTURE0 - 1u, GL_TEXTURE_2D, 1u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(7, "low texunit validation");
    glBindMultiTexture(GL_TEXTURE0 + MGL_TEXTURE_UNITS, GL_TEXTURE_2D, 1u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(8, "high texunit validation");
    glBindMultiTexture(GL_TEXTURE0, 0xdeadbeefu, 1u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(9, "target validation");

    if (glXGetProcAddress((const GLubyte *)"glBindMultiTexture") == NULL ||
        glXGetProcAddress((const GLubyte *)"glBindMultiTextureEXT") == NULL)
        return fail(10, "proc-address coverage");

    mgl_texture_realize_bindings(&ctx);
    if (extension_queries != 0u)
        return fail(16, "GLES core texture buffers require no extension lookup");
    {
        unsigned prior[8], i;
        memcpy(prior, bind_calls, sizeof(prior));
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, 77u);
        mgl_texture_realize_bindings(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || bind_calls[2] != prior[2] ||
            active_native_unit != 2u)
            return fail(11, "redundant texture bind on unchanged unit");
        for (i = 0; i < 8; ++i)
            if (i != 2u && bind_calls[i] != prior[i])
                return fail(12, "texture update rebound unrelated unit");
        memcpy(prior, bind_calls, sizeof(prior));
        glBindMultiTexture(GL_TEXTURE3, GL_TEXTURE_2D, 77u);
        mgl_texture_realize_bindings(&ctx);
        if (bind_calls[3] <= prior[3] || active_native_unit != 2u)
            return fail(13, "explicit-unit texture realization");
        for (i = 0; i < 8; ++i)
            if (i != 3u && bind_calls[i] != prior[i])
                return fail(14, "explicit-unit update rebound unrelated unit");
        memcpy(prior, bind_calls, sizeof(prior));
        mgl_texture_bind_native(&ctx, GL_TEXTURE_2D, 0u);
        mgl_texture_realize_bindings(&ctx);
        for (i = 0; i < 8; ++i)
            if (bind_calls[i] <= prior[i])
                return fail(15, "internal bind must invalidate all units");
        if (extension_queries != 0u)
            return fail(17, "GLES extension queried during texture realization");
        native.glesBindSampler = fake_sampler;
        mgl_texture_realize_bindings(&ctx);
        glBindTexture(GL_TEXTURE_2D, 99u);
        if (ctx.texture_bindings_dirty || ctx.texture_bindings_dirty_units[0] != 0u)
            return fail(18, "canonical bind should not schedule redundant realization");
        memcpy(prior, bind_calls, sizeof(prior));
        mgl_texture_realize_bindings(&ctx);
        for (i = 0; i < 8; ++i)
            if (bind_calls[i] != prior[i])
                return fail(19, "canonical bind should already be realized");
        mgl_texture_bind_native_unit(&ctx, GL_TEXTURE_2D, 0u, 2u);
        if (!ctx.texture_bindings_dirty)
            return fail(20, "temporary native binding must remain dirty");
        mgl_texture_realize_bindings(&ctx);
        if (bound_native[2u][0] != mgl_texture_find(&ctx, 99u)->native_name)
            return fail(21, "temporary native binding restores canonical texture");
        glBindMultiTexture(GL_TEXTURE3, GL_TEXTURE_2D, 99u);
        if (ctx.texture_bindings_dirty || active_native_unit != 2u ||
            bound_native[3u][0] != mgl_texture_find(&ctx, 99u)->native_name)
            return fail(23, "explicit canonical bind remains realized");
        mgl_texture_bind_native_unit(&ctx, GL_TEXTURE_2D, 0u, 2u);
        glBindTexture(GL_TEXTURE_2D, 99u);
        if (!ctx.texture_bindings_dirty)
            return fail(24, "pending native restoration is preserved");
        mgl_texture_realize_bindings(&ctx);
    }

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL BindMultiTexture unit test: PASS");
    return 0;
}

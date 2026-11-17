#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_native_texture;
    unsigned active_unit;
    GLuint bound_2d[8];
    unsigned tex_parameter_i_calls;
    unsigned tex_parameter_f_calls;
    GLuint last_parameter_texture;
    GLenum last_parameter_pname;
    GLint last_parameter_i;
    GLfloat last_parameter_f;
} cap;

static void fake_active_texture(GLenum texture) {
    cap.active_unit = (unsigned)(texture - GL_TEXTURE0);
}

static void fake_gen_textures(GLsizei n, GLuint *textures) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        textures[i] = ++cap.next_native_texture;
}

static void fake_delete_textures(GLsizei n, const GLuint *textures) {
    (void)n;
    (void)textures;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    if (target == GL_TEXTURE_2D && cap.active_unit < 8u)
        cap.bound_2d[cap.active_unit] = texture;
}

static void fake_tex_parameter_i(GLenum target, GLenum pname, GLint param) {
    if (target != GL_TEXTURE_2D)
        return;
    ++cap.tex_parameter_i_calls;
    cap.last_parameter_texture = cap.active_unit < 8u ? cap.bound_2d[cap.active_unit] : 0u;
    cap.last_parameter_pname = pname;
    cap.last_parameter_i = param;
}

static void fake_tex_parameter_f(GLenum target, GLenum pname, GLfloat param) {
    if (target != GL_TEXTURE_2D)
        return;
    ++cap.tex_parameter_f_calls;
    cap.last_parameter_texture = cap.active_unit < 8u ? cap.bound_2d[cap.active_unit] : 0u;
    cap.last_parameter_pname = pname;
    cap.last_parameter_f = param;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "EXT DSA texture parameter failure %d: %s\n", code, message);
    return code;
}

static int expect_error(struct mgl_context *ctx, GLenum expected, int code, const char *message) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr, "EXT DSA texture parameter failure %d: %s (got 0x%04x expected 0x%04x)\n",
                code, message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *caller_obj, *named_obj, *default_obj, *mismatch_obj;
    GLuint tex[5] = {0u, 0u, 0u, 0u, 0u};
    GLuint list;
    GLuint caller_native;
    unsigned calls_before;
    GLfloat border_f[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    const GLint border_i[4] = {INT_MAX, 0, INT_MAX, 0};
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));

    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexParameteri = fake_tex_parameter_i;
    native.glesTexParameterf = fake_tex_parameter_f;

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

    glGenTextures(5, tex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 2, "caller binding")) != 0)
        return rc;
    caller_obj = mgl_texture_find(&ctx, tex[0]);
    if (caller_obj == NULL)
        return fail(3, "caller object");
    caller_native = caller_obj->native_name;

    /* GL_COMPILE records the named update but must not realize an unused name
       until replay. */
    if (glIsTexture(tex[1]) != GL_FALSE)
        return fail(4, "unused-name precondition");
    calls_before = cap.tex_parameter_i_calls;
    list = glGenLists(1);
    if (list == 0u)
        return fail(5, "display-list allocation");
    glNewList(list, GL_COMPILE);
    glTextureParameteriEXT(tex[1], GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)GL_NEAREST);
    glEndList();
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 6, "compile named parameter")) != 0)
        return rc;
    if (glIsTexture(tex[1]) != GL_FALSE || cap.tex_parameter_i_calls != calls_before)
        return fail(7, "GL_COMPILE realized/executed named parameter");
    glCallList(list);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 8, "replay named parameter")) != 0)
        return rc;
    named_obj = mgl_texture_find(&ctx, tex[1]);
    if (named_obj == NULL || named_obj->target != GL_TEXTURE_2D || !named_obj->realized ||
        named_obj->min_filter != (GLint)GL_NEAREST || glIsTexture(tex[1]) != GL_TRUE)
        return fail(9, "replay did not realize/update named texture");
    if (ctx.texture_2d_binding[2] != tex[0] || cap.active_unit != 2u ||
        cap.bound_2d[2] != caller_native || cap.tex_parameter_i_calls != calls_before + 1u ||
        cap.last_parameter_texture != named_obj->native_name)
        return fail(10, "named integer parameter did not restore caller binding");

    glNewList(list, GL_COMPILE);
    glTextureParameterfvEXT(tex[1], GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_f);
    glEndList();
    border_f[0] = border_f[1] = border_f[2] = border_f[3] = 0.0f;
    glCallList(list);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 11, "float-vector replay")) != 0)
        return rc;
    if (named_obj->border_color[0] != 0.25f || named_obj->border_color[1] != 0.5f ||
        named_obj->border_color[2] != 0.75f || named_obj->border_color[3] != 1.0f)
        return fail(12, "display-list vector payload was not captured by value");

    glTextureParameterivEXT(tex[1], GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_i);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 13, "integer border parameter")) != 0)
        return rc;
    if (named_obj->border_color[0] != 1.0f || named_obj->border_color[1] != 0.0f ||
        named_obj->border_color[2] != 1.0f || named_obj->border_color[3] != 0.0f)
        return fail(14, "integer border conversion");

    glTextureParameterfEXT(tex[1], GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, -2.5f);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 15, "float scalar parameter")) != 0)
        return rc;
    if (named_obj->min_lod != -2.5f || cap.tex_parameter_f_calls == 0u ||
        cap.last_parameter_texture != named_obj->native_name || cap.bound_2d[2] != caller_native)
        return fail(16, "float parameter/native restoration");

    /* Texture zero addresses the default object and must leave the caller's
       nonzero binding untouched. */
    default_obj = mgl_default_texture(&ctx, GL_TEXTURE_2D);
    glTextureParameteriEXT(0u, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)GL_CLAMP_TO_EDGE);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 17, "default texture parameter")) != 0)
        return rc;
    if (default_obj == NULL || default_obj->wrap_s != (GLint)GL_CLAMP_TO_EDGE ||
        ctx.texture_2d_binding[2] != tex[0] || cap.bound_2d[2] != caller_native)
        return fail(18, "texture-zero/default-object semantics");

    /* A used texture cannot be retyped by the EXT named command, and an error
       must not alter its previous state. */
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex[2]);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0u);
    mismatch_obj = mgl_texture_find(&ctx, tex[2]);
    if (mismatch_obj == NULL)
        return fail(19, "mismatch object setup");
    glTextureParameteriEXT(tex[2], GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)GL_LINEAR);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 20, "target mismatch")) != 0)
        return rc;
    if (mismatch_obj->target != GL_TEXTURE_CUBE_MAP ||
        mismatch_obj->min_filter != (GLint)GL_NEAREST_MIPMAP_LINEAR ||
        ctx.texture_2d_binding[2] != tex[0])
        return fail(21, "target mismatch mutated object/binding");

    /* Invalid multisample sampler state is rejected before an unused name is
       realized. */
    if (glIsTexture(tex[3]) != GL_FALSE)
        return fail(22, "multisample unused-name precondition");
    glTextureParameteriEXT(tex[3], GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER,
                           (GLint)GL_NEAREST);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 23, "multisample sampler restriction")) != 0)
        return rc;
    if (glIsTexture(tex[3]) != GL_FALSE)
        return fail(24, "error path realized multisample name");

    /* COMPILE_AND_EXECUTE performs the update immediately and retains the same
       named update for later replay. */
    glNewList(list, GL_COMPILE_AND_EXECUTE);
    glTextureParameteriEXT(tex[4], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)GL_CLAMP_TO_EDGE);
    glEndList();
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 25, "compile-and-execute")) != 0)
        return rc;
    if (glIsTexture(tex[4]) != GL_TRUE ||
        mgl_texture_find(&ctx, tex[4])->wrap_t != (GLint)GL_CLAMP_TO_EDGE)
        return fail(26, "compile-and-execute did not execute");
    glTextureParameteriEXT(tex[4], GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)GL_REPEAT);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 27, "post-list mutation")) != 0)
        return rc;
    glCallList(list);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 28, "compile-and-execute replay")) != 0)
        return rc;
    if (mgl_texture_find(&ctx, tex[4])->wrap_t != (GLint)GL_CLAMP_TO_EDGE)
        return fail(29, "retained named parameter did not replay");

    if (glXGetProcAddress((const GLubyte *)"glTextureParameteriEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTextureParameterivEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTextureParameterfEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTextureParameterfvEXT") == NULL)
        return fail(30, "proc-address exposure");

    glDeleteLists(list, 1);
    glDeleteTextures(5, tex);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 31, "unexpected final GL error")) != 0)
        return rc;
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL EXT DSA named texture parameter unit test: PASS");
    return 0;
}

#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_native_texture;
    unsigned active_unit;
    GLuint bound_2d[8];
    unsigned signed_calls;
    unsigned unsigned_calls;
    GLuint last_parameter_texture;
    GLint last_signed[4];
    GLuint last_unsigned[4];
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

static void fake_tex_parameter_iiv(GLenum target, GLenum pname, const GLint *params) {
    if (target != GL_TEXTURE_2D || pname != GL_TEXTURE_BORDER_COLOR || params == NULL)
        return;
    ++cap.signed_calls;
    cap.last_parameter_texture = cap.active_unit < 8u ? cap.bound_2d[cap.active_unit] : 0u;
    memcpy(cap.last_signed, params, sizeof(cap.last_signed));
}

static void fake_tex_parameter_iuiv(GLenum target, GLenum pname, const GLuint *params) {
    if (target != GL_TEXTURE_2D || pname != GL_TEXTURE_BORDER_COLOR || params == NULL)
        return;
    ++cap.unsigned_calls;
    cap.last_parameter_texture = cap.active_unit < 8u ? cap.bound_2d[cap.active_unit] : 0u;
    memcpy(cap.last_unsigned, params, sizeof(cap.last_unsigned));
}

static int fail(int code, const char *message) {
    fprintf(stderr, "EXT DSA integer texture parameter failure %d: %s\n", code, message);
    return code;
}

static int expect_error(struct mgl_context *ctx, GLenum expected, int code, const char *message) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr,
                "EXT DSA integer texture parameter failure %d: %s (got 0x%04x expected 0x%04x)\n",
                code, message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *caller_obj, *signed_obj, *unsigned_obj, *default_obj, *core_obj;
    struct mgl_texture *bound_compile_obj, *bound_replay_obj, *multi_replay_obj;
    GLuint tex[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    GLuint core_tex = 0u;
    GLuint list;
    GLuint caller_native;
    GLint signed_in[4] = {INT_MIN, -123456789, 123456789, INT_MAX};
    const GLint signed_expected[4] = {INT_MIN, -123456789, 123456789, INT_MAX};
    GLint signed_out[4] = {0, 0, 0, 0};
    GLuint unsigned_in[4] = {UINT_MAX, UINT_MAX - 1u, UINT32_C(0x80000000), 17u};
    GLuint unsigned_out[4] = {0u, 0u, 0u, 0u};
    GLint iv = -1;
    unsigned signed_calls_before;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));

    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexParameterIiv = fake_tex_parameter_iiv;
    native.glesTexParameterIuiv = fake_tex_parameter_iuiv;

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

    glGenTextures(8, tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 2, "caller binding")) != 0)
        return rc;
    caller_obj = mgl_texture_find(&ctx, tex[0]);
    if (caller_obj == NULL)
        return fail(3, "caller object");
    caller_native = caller_obj->native_name;

    /* Integer border colors must remain exact integer state.  GL_COMPILE must
       capture the four values by value without realizing the unused name. */
    if (glIsTexture(tex[1]) != GL_FALSE)
        return fail(4, "signed unused-name precondition");
    signed_calls_before = cap.signed_calls;
    list = glGenLists(1);
    if (list == 0u)
        return fail(5, "list allocation");
    glNewList(list, GL_COMPILE);
    glTextureParameterIivEXT(tex[1], GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, signed_in);
    glEndList();
    signed_in[0] = signed_in[1] = signed_in[2] = signed_in[3] = 0;
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 6, "signed list compile")) != 0)
        return rc;
    if (glIsTexture(tex[1]) != GL_FALSE || cap.signed_calls != signed_calls_before)
        return fail(7, "signed list compile realized/executed");
    glCallList(list);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 8, "signed list replay")) != 0)
        return rc;
    signed_obj = mgl_texture_find(&ctx, tex[1]);
    if (signed_obj == NULL || signed_obj->border_color_type != GL_INT ||
        memcmp(signed_obj->border_color_i, signed_expected, sizeof(signed_expected)) != 0)
        return fail(9, "signed canonical state");
    glGetTextureParameterIivEXT(tex[1], GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, signed_out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 10, "signed exact query")) != 0)
        return rc;
    if (memcmp(signed_out, signed_expected, sizeof(signed_expected)) != 0)
        return fail(11, "signed integer round-trip lost precision");
    if (cap.signed_calls != signed_calls_before + 1u ||
        memcmp(cap.last_signed, signed_expected, sizeof(signed_expected)) != 0 ||
        cap.last_parameter_texture != signed_obj->native_name || cap.bound_2d[1] != caller_native ||
        ctx.texture_2d_binding[1] != tex[0] || cap.active_unit != 1u)
        return fail(12, "signed native/binding restoration");

    /* Unsigned state must preserve values above INT_MAX without signed casts. */
    glTextureParameterIuivEXT(tex[2], GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, unsigned_in);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 13, "unsigned named set")) != 0)
        return rc;
    unsigned_obj = mgl_texture_find(&ctx, tex[2]);
    if (unsigned_obj == NULL || unsigned_obj->border_color_type != GL_UNSIGNED_INT ||
        memcmp(unsigned_obj->border_color_ui, unsigned_in, sizeof(unsigned_in)) != 0)
        return fail(14, "unsigned canonical state");
    glGetTextureParameterIuivEXT(tex[2], GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, unsigned_out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 15, "unsigned exact query")) != 0)
        return rc;
    if (memcmp(unsigned_out, unsigned_in, sizeof(unsigned_in)) != 0 || cap.unsigned_calls != 1u ||
        memcmp(cap.last_unsigned, unsigned_in, sizeof(unsigned_in)) != 0 ||
        cap.last_parameter_texture != unsigned_obj->native_name || cap.bound_2d[1] != caller_native)
        return fail(16, "unsigned round-trip/native restoration");

    glCreateTextures(GL_TEXTURE_2D, 1, &core_tex);
    glTextureParameterIiv(core_tex, GL_TEXTURE_BORDER_COLOR, signed_expected);
    memset(signed_out, 0, sizeof(signed_out));
    glGetTextureParameterIiv(core_tex, GL_TEXTURE_BORDER_COLOR, signed_out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 17, "core signed DSA round-trip")) != 0)
        return rc;
    core_obj = mgl_texture_find(&ctx, core_tex);
    if (core_obj == NULL || core_obj->border_color_type != GL_INT ||
        memcmp(signed_out, signed_expected, sizeof(signed_expected)) != 0)
        return fail(18, "core DSA integer state still normalized");

    /* Texture zero selects the default target object while preserving the
       caller binding and exact unsigned values. */
    default_obj = mgl_default_texture(&ctx, GL_TEXTURE_2D);
    glTextureParameterIuivEXT(0u, GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, unsigned_in);
    memset(unsigned_out, 0, sizeof(unsigned_out));
    glGetTextureParameterIuivEXT(0u, GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, unsigned_out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 19, "default unsigned state")) != 0)
        return rc;
    if (default_obj == NULL || default_obj->border_color_type != GL_UNSIGNED_INT ||
        memcmp(unsigned_out, unsigned_in, sizeof(unsigned_in)) != 0 ||
        ctx.texture_2d_binding[1] != tex[0] || cap.bound_2d[1] != caller_native)
        return fail(20, "texture-zero integer state/binding");

    /* Existing target identity is immutable and query output is untouched on
       mismatch. */
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex[3]);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0u);
    signed_out[0] = 0x12345678;
    glGetTextureParameterIivEXT(tex[3], GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, signed_out);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 21, "integer query target mismatch")) != 0)
        return rc;
    if (signed_out[0] != 0x12345678 || ctx.texture_2d_binding[1] != tex[0])
        return fail(22, "integer mismatch mutated output/binding");

    /* Non-border integer queries inherit normal parameter semantics and realize
       an unused name before querying its initial state. */
    if (glIsTexture(tex[4]) != GL_FALSE)
        return fail(23, "integer query unused-name precondition");
    glGetTextureParameterIivEXT(tex[4], GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &iv);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 24, "integer query unused-name realization")) != 0)
        return rc;
    if (iv != (GLint)GL_NEAREST_MIPMAP_LINEAR || glIsTexture(tex[4]) != GL_TRUE ||
        ctx.texture_2d_binding[1] != tex[0])
        return fail(25, "integer non-border query semantics");

    memset(signed_out, 0, sizeof(signed_out));
    glNewList(list, GL_COMPILE);
    glGetTextureParameterIivEXT(tex[1], GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, signed_out);
    glEndList();
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 26, "integer query during compile")) != 0)
        return rc;
    if (memcmp(signed_out, signed_expected, sizeof(signed_expected)) != 0)
        return fail(27, "integer query incorrectly retained");
    signed_out[0] = 77;
    glCallList(list);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 28, "query list replay")) != 0)
        return rc;
    if (signed_out[0] != 77)
        return fail(29, "integer query unexpectedly replayed");

    /* Bound integer texture commands are display-list state updates: the
       payload is captured at compile time, but the bound texture is resolved
       at replay. */
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex[5]);
    bound_compile_obj = mgl_texture_find(&ctx, tex[5]);
    if (bound_compile_obj == NULL)
        return fail(30, "bound compile object");
    memcpy(signed_in, signed_expected, sizeof(signed_in));
    glNewList(list, GL_COMPILE);
    glTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, signed_in);
    glEndList();
    memset(signed_in, 0, sizeof(signed_in));
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 31, "bound signed list compile")) != 0)
        return rc;
    if (bound_compile_obj->border_color_type != GL_FLOAT)
        return fail(32, "bound signed list executed during compile");
    glBindTexture(GL_TEXTURE_2D, tex[6]);
    bound_replay_obj = mgl_texture_find(&ctx, tex[6]);
    if (bound_replay_obj == NULL)
        return fail(33, "bound replay object");
    glCallList(list);
    memset(signed_out, 0, sizeof(signed_out));
    glGetTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, signed_out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 34, "bound signed list replay/query")) != 0)
        return rc;
    if (bound_compile_obj->border_color_type != GL_FLOAT ||
        bound_replay_obj->border_color_type != GL_INT ||
        memcmp(signed_out, signed_expected, sizeof(signed_expected)) != 0 ||
        cap.active_unit != 2u || cap.last_parameter_texture != bound_replay_obj->native_name)
        return fail(35, "bound integer list late-binding/exact state");

    glTexParameterIuiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, unsigned_in);
    memset(unsigned_out, 0, sizeof(unsigned_out));
    glGetTexParameterIuiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, unsigned_out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 36, "bound unsigned exact query")) != 0)
        return rc;
    if (memcmp(unsigned_out, unsigned_in, sizeof(unsigned_in)) != 0 ||
        bound_replay_obj->border_color_type != GL_UNSIGNED_INT)
        return fail(37, "bound unsigned round-trip");

    /* MultiTex integer commands capture the explicit unit and values, resolve
       that unit's texture at replay, and never expose a selector change. */
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, tex[5]);
    glActiveTexture(GL_TEXTURE1);
    memcpy(unsigned_out, unsigned_in, sizeof(unsigned_out));
    glNewList(list, GL_COMPILE);
    glMultiTexParameterIuivEXT(GL_TEXTURE3, GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, unsigned_out);
    glEndList();
    memset(unsigned_out, 0, sizeof(unsigned_out));
    glBindMultiTextureEXT(GL_TEXTURE3, GL_TEXTURE_2D, tex[7]);
    multi_replay_obj = mgl_texture_find(&ctx, tex[7]);
    if (multi_replay_obj == NULL)
        return fail(38, "multitex replay object");
    glCallList(list);
    glGetMultiTexParameterIuivEXT(GL_TEXTURE3, GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR,
                                  unsigned_out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 39, "multitex unsigned replay/query")) != 0)
        return rc;
    if (memcmp(unsigned_out, unsigned_in, sizeof(unsigned_in)) != 0 ||
        multi_replay_obj->border_color_type != GL_UNSIGNED_INT || ctx.compat.active_texture != 1u ||
        cap.active_unit != 1u || cap.last_parameter_texture != multi_replay_obj->native_name)
        return fail(40, "multitex integer late-binding/selector restoration");

    signed_out[0] = 0x13572468;
    glGetMultiTexParameterIivEXT(GL_TEXTURE0 + 99u, GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR,
                                 signed_out);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 41, "multitex invalid unit")) != 0)
        return rc;
    if (signed_out[0] != 0x13572468 || ctx.compat.active_texture != 1u)
        return fail(42, "multitex invalid unit mutated output/selector");

    if (glXGetProcAddress((const GLubyte *)"glTextureParameterIivEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTextureParameterIuivEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetTextureParameterIivEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetTextureParameterIuivEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTexParameterIiv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTexParameterIuiv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetTexParameterIiv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetTexParameterIuiv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexParameterIivEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexParameterIuivEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetMultiTexParameterIivEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetMultiTexParameterIuivEXT") == NULL)
        return fail(43, "integer texture proc-address exposure");

    glDeleteLists(list, 1);
    glDeleteTextures(8, tex);
    glDeleteTextures(1, &core_tex);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 44, "unexpected final GL error")) != 0)
        return rc;
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL EXT DSA integer texture parameter unit test: PASS");
    return 0;
}

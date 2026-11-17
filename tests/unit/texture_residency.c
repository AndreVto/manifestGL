#include "mgl/gl.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static GLuint next_native_texture = 700u;
static unsigned float_parameter_calls = 0u;

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
    (void)texture;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    (void)target;
    (void)texture;
}

static void fake_tex_parameterf(GLenum target, GLenum pname, GLfloat param) {
    (void)target;
    (void)pname;
    (void)param;
    ++float_parameter_calls;
}

static int nearf(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 1.0e-6f;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "texture residency/priority failure %d: %s\n", code, message);
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
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_share_group *group;
    GLuint names[3] = {0u, 0u, 0u};
    GLuint query_names[2];
    GLboolean residences[2] = {GL_FALSE, GL_FALSE};
    GLfloat priorities[4];
    GLuint priority_names[4];
    GLfloat value = -1.0f;
    GLint resident = 0;
    GLuint list;
    GLfloat list_priority = 0.75f;
    GLuint list_name;

    memset(&native, 0, sizeof(native));
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexParameterf = fake_tex_parameterf;

    group = mgl_share_group_create();
    if (group == NULL)
        return fail(1, "share group");
    init_context(&ctx, &native, group);
    mgl_context_set_current_for_test(&ctx);

    glGenTextures(3, names);
    if (names[0] == 0u || names[1] == 0u || names[2] == 0u)
        return fail(2, "generated names");
    glBindTexture(GL_TEXTURE_2D, names[0]);
    glBindTexture(GL_TEXTURE_2D, names[1]);

    query_names[0] = names[0];
    query_names[1] = names[1];
    if (!glAreTexturesResident(2, query_names, residences) || !residences[0] || !residences[1] ||
        mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(3, "valid textures must be resident in the no-eviction model");

    residences[0] = GL_FALSE;
    glBindTexture(GL_TEXTURE_2D, names[0]);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_RESIDENT, &resident);
    if (resident != (GLint)GL_TRUE || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(4, "TEXTURE_RESIDENT query");

    query_names[0] = names[2];
    if (glAreTexturesResident(1, query_names, residences) != GL_FALSE ||
        mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(5, "generated-but-unbound name must not be a texture object");
    query_names[0] = 0u;
    if (glAreTexturesResident(1, query_names, residences) != GL_FALSE ||
        mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(6, "zero is invalid for residency query");
    if (glAreTexturesResident(-1, query_names, residences) != GL_FALSE ||
        mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(7, "negative residency count");
    if (glAreTexturesResident(0, query_names, residences) != GL_TRUE ||
        mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(8, "zero-length residency query");

    priority_names[0] = names[0];
    priority_names[1] = names[1];
    priority_names[2] = names[2];
    priority_names[3] = 0u;
    priorities[0] = -0.25f;
    priorities[1] = 0.35f;
    priorities[2] = 0.80f;
    priorities[3] = 0.20f;
    glPrioritizeTextures(4, priority_names, priorities);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(9, "prioritize valid/ignored names");
    glBindTexture(GL_TEXTURE_2D, names[0]);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, &value);
    if (!nearf(value, 0.0f))
        return fail(10, "priority low clamp");
    glBindTexture(GL_TEXTURE_2D, names[1]);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, &value);
    if (!nearf(value, 0.35f))
        return fail(11, "priority assignment");

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1.5f);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, &value);
    if (!nearf(value, 1.0f) || float_parameter_calls != 0u)
        return fail(12, "single-texture priority clamp/interface ownership");

    list = glGenLists(1);
    if (list == 0u)
        return fail(13, "display-list allocation");
    list_name = names[1];
    glNewList(list, GL_COMPILE);
    glPrioritizeTextures(1, &list_name, &list_priority);
    glEndList();
    list_name = 0u;
    list_priority = 0.10f;
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, &value);
    if (!nearf(value, 1.0f))
        return fail(14, "GL_COMPILE executed PrioritizeTextures");
    glCallList(list);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, &value);
    if (!nearf(value, 0.75f))
        return fail(15, "PrioritizeTextures list payload lifetime");

    glNewList(list, GL_COMPILE);
    query_names[0] = names[0];
    residences[0] = GL_FALSE;
    if (!glAreTexturesResident(1, query_names, residences) || !residences[0])
        return fail(16, "AreTexturesResident must execute during list compilation");
    glEndList();

    glPrioritizeTextures(-1, priority_names, priorities);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(17, "negative priority count");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(group);
    puts("mGL texture residency/priority unit test: PASS");
    return 0;
}

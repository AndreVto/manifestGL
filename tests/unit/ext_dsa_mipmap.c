#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_texture_native;
    unsigned active_unit;
    unsigned generate_calls;
    unsigned generate_unit;
    GLenum generate_target;
} cap;

static void fake_gen_textures(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_texture_native;
}

static void fake_delete_textures(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_active_texture(GLenum texture) {
    cap.active_unit = (unsigned)(texture - GL_TEXTURE0);
}

static void fake_bind_texture(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_generate_mipmap(GLenum target) {
    ++cap.generate_calls;
    cap.generate_unit = cap.active_unit;
    cap.generate_target = target;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "EXT DSA mipmap failure %d: %s\n", code, message);
    return code;
}

static int expect_error(struct mgl_context *ctx, GLenum expected, int code, const char *message) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr, "EXT DSA mipmap failure %d: %s (got 0x%04x expected 0x%04x)\n", code,
                message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

static int level_defined(struct mgl_context *ctx, GLuint texture, GLint level) {
    struct mgl_texture *obj = mgl_texture_find(ctx, texture);
    return obj != NULL && level >= 0 && level < (GLint)MGL_MAX_TEXTURE_LEVELS &&
           obj->levels[level].defined;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *unused_obj;
    struct mgl_texture *default_obj;
    GLuint textures[4] = {0u, 0u, 0u, 0u};
    GLuint unused = 0u;
    GLuint list;
    unsigned char pixels[4u * 4u * 4u];
    unsigned calls_before;
    GLint binding = -1;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    memset(pixels, 0x61, sizeof(pixels));

    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesGenerateMipmap = fake_generate_mipmap;

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

    glGenTextures(4, textures);
    glBindMultiTexture(GL_TEXTURE2, GL_TEXTURE_2D, textures[0]);
    glBindMultiTexture(GL_TEXTURE3, GL_TEXTURE_2D, textures[1]);
    glBindMultiTexture(GL_TEXTURE4, GL_TEXTURE_2D, textures[2]);
    glBindMultiTexture(GL_TEXTURE1, GL_TEXTURE_2D, textures[3]);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 2, "explicit texture bindings")) != 0)
        return rc;

    glMultiTexImage2D(GL_TEXTURE2, GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      pixels);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 33, "unit 2 base image")) != 0)
        return rc;
    glMultiTexImage2D(GL_TEXTURE3, GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      pixels);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 34, "unit 3 base image")) != 0)
        return rc;
    glMultiTexImage2D(GL_TEXTURE4, GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      pixels);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 3, "base image setup")) != 0)
        return rc;
    if (level_defined(&ctx, textures[0], 1) || level_defined(&ctx, textures[1], 1) ||
        level_defined(&ctx, textures[2], 1))
        return fail(4, "unexpected pre-generated level");

    glActiveTexture(GL_TEXTURE1);
    glGenerateMultiTexMipmapEXT(GL_TEXTURE2, GL_TEXTURE_2D);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 5, "explicit-unit generation")) != 0)
        return rc;
    if (!level_defined(&ctx, textures[0], 1) || level_defined(&ctx, textures[1], 1))
        return fail(6, "generation did not isolate unit 2");
    if (ctx.compat.active_texture != 1u || cap.active_unit != 1u)
        return fail(7, "explicit-unit generation changed active selector");
    if (cap.generate_calls != 1u || cap.generate_unit != 2u || cap.generate_target != GL_TEXTURE_2D)
        return fail(8, "native generation used wrong explicit unit");

    list = glGenLists(1);
    if (list == 0u)
        return fail(9, "display-list allocation");
    calls_before = cap.generate_calls;
    glNewList(list, GL_COMPILE);
    glGenerateMultiTexMipmap(GL_TEXTURE3, GL_TEXTURE_2D);
    glEndList();
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 10, "display-list compile-time generation")) != 0)
        return rc;
    if (!level_defined(&ctx, textures[1], 1) || cap.generate_calls != calls_before + 1u ||
        cap.generate_unit != 3u)
        return fail(11, "GenerateMultiTexMipmap was not executed immediately");
    calls_before = cap.generate_calls;
    glCallList(list);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 12, "empty replay after immediate command")) != 0)
        return rc;
    if (cap.generate_calls != calls_before)
        return fail(13, "GenerateMultiTexMipmap was incorrectly retained in display list");

    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    if ((GLuint)binding != textures[3])
        return fail(14, "active-unit binding setup");
    glGenerateTextureMipmapEXT(textures[2], GL_TEXTURE_2D);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 15, "named EXT generation")) != 0)
        return rc;
    if (!level_defined(&ctx, textures[2], 1))
        return fail(16, "named texture level generation");
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    if ((GLuint)binding != textures[3] || ctx.compat.active_texture != 1u || cap.active_unit != 1u)
        return fail(17, "named EXT generation changed caller binding/selector");

    glBindTexture(GL_TEXTURE_2D, 0u);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 18, "default texture base")) != 0)
        return rc;
    default_obj = mgl_default_texture(&ctx, GL_TEXTURE_2D);
    if (default_obj == NULL || default_obj->levels[1].defined)
        return fail(19, "default texture precondition");
    glBindTexture(GL_TEXTURE_2D, textures[3]);
    glGenerateTextureMipmapEXT(0u, GL_TEXTURE_2D);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 20, "default named EXT generation")) != 0)
        return rc;
    if (!default_obj->levels[1].defined)
        return fail(21, "texture zero did not address default object");
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    if ((GLuint)binding != textures[3])
        return fail(22, "default named generation changed binding");

    glGenTextures(1, &unused);
    if (unused == 0u || glIsTexture(unused) != GL_FALSE)
        return fail(23, "generated-only name precondition");
    glGenerateTextureMipmapEXT(unused, GL_TEXTURE_2D);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 24, "unused name has undefined base")) != 0)
        return rc;
    unused_obj = mgl_texture_find(&ctx, unused);
    if (unused_obj == NULL || unused_obj->target != GL_TEXTURE_2D || !unused_obj->realized ||
        glIsTexture(unused) != GL_TRUE)
        return fail(25, "unused name was not converted to a texture object before generation");

    glGenerateTextureMipmapEXT(textures[2], GL_TEXTURE_3D);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 26, "named target mismatch")) != 0)
        return rc;
    glGenerateTextureMipmapEXT(textures[2], GL_TEXTURE_BUFFER);
    if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 27, "invalid named generation target")) != 0)
        return rc;
    glGenerateMultiTexMipmap(GL_TEXTURE0 - 1u, GL_TEXTURE_2D);
    if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 28, "low explicit unit")) != 0)
        return rc;
    glGenerateMultiTexMipmap(GL_TEXTURE0 + MGL_TEXTURE_UNITS, GL_TEXTURE_2D);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 29, "high explicit unit")) != 0)
        return rc;
    glGenerateMultiTexMipmap(GL_TEXTURE2, GL_TEXTURE_BUFFER);
    if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 30, "invalid explicit-unit target")) != 0)
        return rc;
    if (ctx.compat.active_texture != 1u || cap.active_unit != 1u)
        return fail(31, "validation path changed active selector");

    if (glXGetProcAddress((const GLubyte *)"glGenerateTextureMipmapEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGenerateMultiTexMipmap") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGenerateMultiTexMipmapEXT") == NULL)
        return fail(32, "proc-address coverage");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL EXT DSA mipmap unit test: PASS");
    return 0;
}

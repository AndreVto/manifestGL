#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_texture_native;
    GLuint next_buffer_native;
    unsigned active_unit;
    GLuint bound_buffer_texture[8];
    unsigned tex_buffer_calls;
    unsigned tex_buffer_unit;
    GLuint tex_buffer_texture;
    GLenum tex_buffer_format;
    GLuint tex_buffer_name;
} cap;

static const GLubyte *fake_get_string(GLenum name) {
    static const GLubyte extensions[] = "GL_EXT_texture_buffer";
    return name == GL_EXTENSIONS ? extensions : (const GLubyte *)"";
}

static void fake_active_texture(GLenum texture) {
    cap.active_unit = (unsigned)(texture - GL_TEXTURE0);
}

static void fake_gen_textures(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_texture_native;
}

static void fake_delete_textures(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_texture(GLenum target, GLuint name) {
    if (target == GL_TEXTURE_BUFFER && cap.active_unit < 8u)
        cap.bound_buffer_texture[cap.active_unit] = name;
}

static void fake_tex_buffer(GLenum target, GLenum internalformat, GLuint buffer) {
    if (target != GL_TEXTURE_BUFFER)
        return;
    ++cap.tex_buffer_calls;
    cap.tex_buffer_unit = cap.active_unit;
    cap.tex_buffer_texture = cap.active_unit < 8u ? cap.bound_buffer_texture[cap.active_unit] : 0u;
    cap.tex_buffer_format = internalformat;
    cap.tex_buffer_name = buffer;
}

static void fake_gen_buffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_buffer_native;
}

static void fake_delete_buffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)target;
    (void)size;
    (void)data;
    (void)usage;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "TextureBufferEXT failure %d: %s\n", code, message);
    return code;
}

static int expect_error(struct mgl_context *ctx, GLenum expected, int code, const char *message) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr, "TextureBufferEXT failure %d: %s (got 0x%04x expected 0x%04x)\n", code,
                message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *named_obj, *generated_obj, *default_obj, *list_obj;
    struct mgl_buffer *buffer_a_obj, *buffer_b_obj;
    GLuint textures[4] = {0u, 0u, 0u, 0u};
    GLuint buffers[2] = {0u, 0u};
    GLuint arbitrary_name = 7001u;
    GLuint list;
    unsigned char data_a[64], data_b[16];
    GLuint restored_native;
    unsigned calls_before;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    memset(data_a, 0x31, sizeof(data_a));
    memset(data_b, 0x72, sizeof(data_b));

    native.glesGetString = fake_get_string;
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexBufferEXT = fake_tex_buffer;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;

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

    glGenBuffers(2, buffers);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(data_a), data_a, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(data_b), data_b, GL_STATIC_DRAW);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 2, "buffer realization")) != 0)
        return rc;
    buffer_a_obj = mgl_buffer_find(&ctx, buffers[0]);
    buffer_b_obj = mgl_buffer_find(&ctx, buffers[1]);
    if (buffer_a_obj == NULL || buffer_b_obj == NULL)
        return fail(3, "canonical buffers");

    glGenTextures(4, textures);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, textures[0]);
    glBindTexture(GL_TEXTURE_2D, textures[3]);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 4, "caller bindings")) != 0)
        return rc;
    restored_native = mgl_texture_find(&ctx, textures[0])->native_name;

    /* Generated-only names are unused texture names in EXT DSA. */
    if (glIsTexture(textures[1]) != GL_FALSE)
        return fail(5, "generated-only precondition");
    glTextureBufferEXT(textures[1], GL_TEXTURE_BUFFER, GL_RGBA8, buffers[0]);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 6, "generated-only named attach")) != 0)
        return rc;
    generated_obj = mgl_texture_find(&ctx, textures[1]);
    if (generated_obj == NULL || generated_obj->target != GL_TEXTURE_BUFFER ||
        !generated_obj->realized || generated_obj->buffer_texture_buffer != buffer_a_obj ||
        glIsTexture(textures[1]) != GL_TRUE)
        return fail(7, "generated-only name was not realized/attached");
    if (ctx.texture_buffer_binding[1] != textures[0] ||
        cap.bound_buffer_texture[1] != restored_native || cap.tex_buffer_calls != 1u ||
        cap.tex_buffer_unit != 1u || cap.tex_buffer_texture != generated_obj->native_name ||
        cap.tex_buffer_name != buffer_a_obj->native_name)
        return fail(8, "named attach did not restore caller buffer binding");

    /* An arbitrary unused non-zero name is also created by the EXT named command. */
    glTextureBufferEXT(arbitrary_name, GL_TEXTURE_BUFFER, GL_R8, buffers[1]);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 9, "arbitrary unused named attach")) != 0)
        return rc;
    named_obj = mgl_texture_find(&ctx, arbitrary_name);
    if (named_obj == NULL || named_obj->target != GL_TEXTURE_BUFFER || !named_obj->realized ||
        named_obj->buffer_texture_buffer != buffer_b_obj)
        return fail(10, "arbitrary unused name state");

    /* Texture name zero addresses the default texture-buffer object. */
    default_obj = mgl_default_texture(&ctx, GL_TEXTURE_BUFFER);
    glTextureBufferEXT(0u, GL_TEXTURE_BUFFER, GL_R8, buffers[1]);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 11, "default texture-buffer attach")) != 0)
        return rc;
    if (default_obj == NULL || default_obj->buffer_texture_buffer != buffer_b_obj ||
        ctx.texture_buffer_binding[1] != textures[0] ||
        cap.bound_buffer_texture[1] != restored_native)
        return fail(12, "texture zero/default object semantics");

    glTextureBufferEXT(textures[3], GL_TEXTURE_BUFFER, GL_R8, buffers[0]);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 13, "used target mismatch")) != 0)
        return rc;
    glTextureBufferEXT(textures[1], GL_TEXTURE_2D, GL_R8, buffers[0]);
    if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 14, "invalid target token")) != 0)
        return rc;
    calls_before = cap.tex_buffer_calls;
    glTextureBufferEXT(textures[1], GL_TEXTURE_BUFFER, GL_RGBA, buffers[0]);
    if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 15, "invalid internal format")) != 0)
        return rc;
    glTextureBufferEXT(textures[1], GL_TEXTURE_BUFFER, GL_R8, 0x7fffffffu);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 16, "unknown buffer")) != 0)
        return rc;
    if (generated_obj->buffer_texture_buffer != buffer_a_obj ||
        cap.tex_buffer_calls != calls_before || cap.bound_buffer_texture[1] != restored_native)
        return fail(17, "error path mutated state/native binding");

    /* TextureBufferEXT is a selector-free state update: compile stores names by value and
     * suppresses execution. */
    glBindTexture(GL_TEXTURE_BUFFER, textures[2]);
    list_obj = mgl_texture_find(&ctx, textures[2]);
    if (list_obj == NULL)
        return fail(18, "list target realization");
    calls_before = cap.tex_buffer_calls;
    list = glGenLists(1);
    if (list == 0u)
        return fail(19, "display-list allocation");
    glNewList(list, GL_COMPILE);
    glTextureBufferEXT(textures[2], GL_TEXTURE_BUFFER, GL_R8, buffers[1]);
    glEndList();
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 20, "display-list compile")) != 0)
        return rc;
    if (list_obj->buffer_texture_buffer != NULL || cap.tex_buffer_calls != calls_before)
        return fail(21, "TextureBufferEXT executed during GL_COMPILE");
    glCallList(list);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 22, "display-list replay")) != 0)
        return rc;
    if (list_obj->buffer_texture_buffer != buffer_b_obj ||
        cap.tex_buffer_calls != calls_before + 1u)
        return fail(23, "TextureBufferEXT replay did not attach recorded names");

    glTextureBufferEXT(textures[2], GL_TEXTURE_BUFFER, GL_R8, 0u);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 24, "named detach")) != 0)
        return rc;
    if (list_obj->buffer_texture_buffer != NULL)
        return fail(25, "named detach state");

    if (glXGetProcAddress((const GLubyte *)"glTextureBufferEXT") == NULL)
        return fail(26, "proc-address coverage");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL TextureBufferEXT unit test: PASS");
    return 0;
}

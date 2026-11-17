#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    GLuint next_buffer_native;
    unsigned delete_buffer_calls;
    unsigned bind_texture_buffer_calls;
    unsigned tex_buffer_calls;
    GLenum tex_buffer_internal_format;
    GLuint tex_buffer_native_name;
    int texture_buffer_extension;
} cap;

static const GLubyte *fake_get_string(GLenum name) {
    static const GLubyte with_ext[] = "GL_EXT_texture_buffer";
    static const GLubyte without_ext[] = "";
    if (name == GL_EXTENSIONS)
        return cap.texture_buffer_extension ? with_ext : without_ext;
    return (const GLubyte *)"";
}

static void fake_active_texture(GLenum texture) {
    (void)texture;
}

static void fake_gen_textures(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_texture;
}

static void fake_delete_textures(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_texture(GLenum target, GLuint name) {
    (void)name;
    if (target == GL_TEXTURE_BUFFER)
        ++cap.bind_texture_buffer_calls;
}

static void fake_tex_buffer(GLenum target, GLenum internalformat, GLuint buffer) {
    if (target != GL_TEXTURE_BUFFER)
        return;
    ++cap.tex_buffer_calls;
    cap.tex_buffer_internal_format = internalformat;
    cap.tex_buffer_native_name = buffer;
}

static void fake_gen_buffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_buffer_native;
}

static void fake_delete_buffers(GLsizei n, const GLuint *names) {
    (void)names;
    cap.delete_buffer_calls += (unsigned)n;
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
    fprintf(stderr, "texture buffer failure %d: %s\n", code, message);
    return code;
}

static int expect_error(struct mgl_context *ctx, GLenum expected, int code, const char *message) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr, "texture buffer failure %d: %s (got 0x%04x expected 0x%04x)\n", code,
                message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *texture_obj;
    struct mgl_buffer *attached;
    GLuint buffer = 0u, texture = 0u;
    GLuint old_native_name;
    GLint value = 0;
    unsigned char data64[64];
    unsigned char data8[8];
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    memset(data64, 0x3c, sizeof(data64));
    memset(data8, 0xa5, sizeof(data8));

    native.glesGetString = fake_get_string;
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexBufferEXT = NULL;
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
    mgl_context_set_current_for_test(&ctx);

    glGenBuffers(1, &buffer);
    if (buffer == 0u)
        return fail(4, "buffer name");
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_BUFFER, texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, buffer);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 38,
                           "generated-only buffer is not an existing buffer object")) != 0)
        return rc;
    glBindBuffer(GL_TEXTURE_BUFFER, buffer);
    glBufferData(GL_TEXTURE_BUFFER, (GLsizeiptr)sizeof(data64), data64, GL_STATIC_DRAW);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 5, "buffer store")) != 0)
        return rc;
    glGetIntegerv(GL_TEXTURE_BUFFER_BINDING, &value);
    if ((GLuint)value != buffer)
        return fail(6, "generic texture-buffer binding query");

    glBindTexture(GL_TEXTURE_BUFFER, texture);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 7, "texture-buffer bind without native extension")) !=
        0)
        return rc;
    if (cap.bind_texture_buffer_calls != 0u)
        return fail(8, "unsupported GLES target was forwarded");
    glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &value);
    if ((GLuint)value != texture)
        return fail(9, "texture object binding query");

    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, buffer);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 10, "canonical texture-buffer attach")) != 0)
        return rc;
    if (cap.tex_buffer_calls != 0u)
        return fail(11, "unsupported native TexBuffer was called");
    texture_obj = mgl_texture_find(&ctx, texture);
    if (texture_obj == NULL || texture_obj->buffer_texture_buffer == NULL ||
        texture_obj->buffer_texture_internal_format != GL_RGBA8)
        return fail(12, "canonical attachment ownership");
    attached = texture_obj->buffer_texture_buffer;
    old_native_name = attached->native_name;
    if (attached->texture_refcount != 1u || attached->size != sizeof(data64))
        return fail(13, "attachment retain state");

    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_DATA_STORE_BINDING, &value);
    if ((GLuint)value != buffer)
        return fail(14, "data-store binding query");
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_INTERNAL_FORMAT, &value);
    if ((GLenum)value != GL_RGBA8)
        return fail(15, "internal format query");
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &value);
    if (value != 16)
        return fail(16, "texel width query");
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_RED_TYPE, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != GL_UNSIGNED_NORMALIZED)
        return fail(41, "texture-buffer component type");
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_ALPHA_SIZE, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 8)
        return fail(42, "texture-buffer component size");
    glGetTextureLevelParameteriv(texture, 0, GL_TEXTURE_ALPHA_TYPE, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != GL_UNSIGNED_NORMALIZED)
        return fail(43, "named texture-buffer component type");

    glDeleteBuffers(1, &buffer);
    if (glIsBuffer(buffer) != GL_FALSE)
        return fail(17, "deleted name remains valid");
    if (cap.delete_buffer_calls != 0u || !attached->delete_pending ||
        attached->texture_refcount != 1u)
        return fail(18, "attached storage was not retained after DeleteBuffers");
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &value);
    if (value != 16)
        return fail(19, "retained texture storage changed after name deletion");

    /* Reuse the deleted numeric name.  The texture must continue to reference the
     * old object until TexBuffer explicitly attaches the new one. */
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(data8), data8, GL_DYNAMIC_DRAW);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 20, "numeric name reuse")) != 0)
        return rc;
    if (glIsBuffer(buffer) != GL_TRUE)
        return fail(21, "reused name did not create a new buffer object");
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &value);
    if (value != 16 || texture_obj->buffer_texture_buffer != attached)
        return fail(22, "reused name redirected existing texture attachment");

    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, buffer);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 23, "reattach reused buffer name")) != 0)
        return rc;
    if (texture_obj->buffer_texture_buffer == attached ||
        texture_obj->buffer_texture_buffer->size != sizeof(data8))
        return fail(24, "new buffer object was not attached");
    if (cap.delete_buffer_calls != 1u)
        return fail(25, "retired storage was not reclaimed on last release");
    if (texture_obj->buffer_texture_buffer->native_name == old_native_name)
        return fail(26, "reused name retained old native storage");
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &value);
    if (value != 2)
        return fail(27, "reattached width query");

    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 1, GL_TEXTURE_WIDTH, &value);
    if ((rc = expect_error(&ctx, GL_INVALID_VALUE, 28, "nonzero texture-buffer level")) != 0)
        return rc;
    glTexBuffer(GL_TEXTURE_2D, GL_RGBA8, buffer);
    if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 29, "invalid TexBuffer target")) != 0)
        return rc;
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA, buffer);
    if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 30, "invalid TexBuffer internal format")) != 0)
        return rc;
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, 0x7fffffffu);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 31, "unknown TexBuffer buffer")) != 0)
        return rc;

    cap.texture_buffer_extension = 1;
    native.glesTexBufferEXT = fake_tex_buffer;
    glBindTexture(GL_TEXTURE_BUFFER, texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32F, buffer);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 32, "native EXT_texture_buffer attach")) != 0)
        return rc;
    if (cap.bind_texture_buffer_calls == 0u || cap.tex_buffer_calls != 1u ||
        cap.tex_buffer_internal_format != GL_RG32F ||
        cap.tex_buffer_native_name != texture_obj->buffer_texture_buffer->native_name)
        return fail(33, "native texture-buffer lowering");
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &value);
    if (value != 1)
        return fail(34, "RG32F texel width");

    glTexBuffer(GL_TEXTURE_BUFFER, GL_R16, buffer);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 39, "desktop-only normalized R16 state")) != 0)
        return rc;
    if (cap.tex_buffer_calls != 1u || texture_obj->buffer_texture_internal_format != GL_R16)
        return fail(40, "unsupported GLES R16 format was forwarded");

    cap.texture_buffer_extension = 0;
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R8, 0u);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 35, "texture-buffer detach through GLES core without extension string")) != 0)
        return rc;
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_DATA_STORE_BINDING, &value);
    if (value != 0 || texture_obj->buffer_texture_buffer != NULL)
        return fail(36, "detach state");
    if (cap.tex_buffer_calls != 2u || cap.tex_buffer_native_name != 0u)
        return fail(37, "native detach lowering");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL texture buffer unit test: PASS");
    return 0;
}

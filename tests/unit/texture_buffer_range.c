#include "mgl/gl.h"
#include "abi/proc_address.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    GLuint next_buffer;
    GLenum last_bind_target;
    GLuint last_bind_texture;
    unsigned bind_buffer_texture_calls;
    unsigned tex_buffer_calls;
    unsigned tex_buffer_range_calls;
    GLenum internal_format;
    GLuint native_buffer;
    GLintptr offset;
    GLsizeiptr size;
    GLint alignment;
} cap;

static const GLubyte *fake_get_string(GLenum name) {
    (void)name;
    return (const GLubyte *)"";
}

static void fake_get_integerv(GLenum pname, GLint *value) {
    if (pname == GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT)
        *value = cap.alignment;
    else
        *value = 0;
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

static void fake_bind_texture(GLenum target, GLuint texture) {
    cap.last_bind_target = target;
    cap.last_bind_texture = texture;
    if (target == GL_TEXTURE_BUFFER)
        ++cap.bind_buffer_texture_calls;
}

static void fake_tex_buffer(GLenum target, GLenum internalformat, GLuint buffer) {
    if (target != GL_TEXTURE_BUFFER)
        return;
    ++cap.tex_buffer_calls;
    cap.internal_format = internalformat;
    cap.native_buffer = buffer;
    cap.offset = 0;
    cap.size = -1;
}

static void fake_tex_buffer_range(GLenum target, GLenum internalformat, GLuint buffer,
                                  GLintptr offset, GLsizeiptr size) {
    if (target != GL_TEXTURE_BUFFER)
        return;
    ++cap.tex_buffer_range_calls;
    cap.internal_format = internalformat;
    cap.native_buffer = buffer;
    cap.offset = offset;
    cap.size = size;
}

static void fake_gen_buffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_buffer;
}

static void fake_delete_buffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_buffer(GLenum target, GLuint buffer) {
    (void)target;
    (void)buffer;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)target;
    (void)size;
    (void)data;
    (void)usage;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "texture buffer range failure %d: %s\n", code, message);
    return code;
}

static int expect_error(struct mgl_context *ctx, GLenum expected, int code, const char *message) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr, "texture buffer range failure %d: %s (got 0x%04x expected 0x%04x)\n", code,
                message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj;
    struct mgl_texture *bound_obj;
    GLuint buffer = 0u, bound_tex = 0u, dsa_tex = 0u, ext_tex = 0u;
    unsigned char data[128];
    GLint value = 0;
    unsigned before_bind_calls;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    memset(data, 0x6d, sizeof(data));
    cap.alignment = 16;

    native.glesGetString = fake_get_string;
    native.glesGetIntegerv = fake_get_integerv;
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexBufferEXT = fake_tex_buffer;
    native.glesTexBufferRangeEXT = fake_tex_buffer_range;
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

    glGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, &value);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 2, "alignment query")) != 0)
        return rc;
    if (value != 16)
        return fail(3, "native alignment not surfaced");

    glGenBuffers(1, &buffer);
    glBindBuffer(GL_TEXTURE_BUFFER, buffer);
    glBufferData(GL_TEXTURE_BUFFER, (GLsizeiptr)sizeof(data), data, GL_STATIC_DRAW);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 4, "buffer storage")) != 0)
        return rc;

    glGenTextures(1, &bound_tex);
    glBindTexture(GL_TEXTURE_BUFFER, bound_tex);
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA8, buffer, 16, 64);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 5, "bound range attach")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, bound_tex);
    if (obj == NULL || obj->buffer_texture_buffer == NULL || obj->buffer_texture_offset != 16 ||
        obj->buffer_texture_size != 64)
        return fail(6, "range state ownership");
    if (cap.tex_buffer_range_calls != 1u || cap.offset != 16 || cap.size != 64 ||
        cap.internal_format != GL_RGBA8)
        return fail(7, "range native lowering");

    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_OFFSET, &value);
    if (value != 16)
        return fail(8, "offset query");
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_SIZE, &value);
    if (value != 64)
        return fail(9, "size query");
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &value);
    if (value != 16)
        return fail(10, "range-aware width");

    glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA8, buffer, 3, 32);
    if ((rc = expect_error(&ctx, GL_INVALID_VALUE, 11, "misaligned range")) != 0)
        return rc;
    if (obj->buffer_texture_offset != 16 || obj->buffer_texture_size != 64 ||
        cap.tex_buffer_range_calls != 1u)
        return fail(12, "misaligned range mutated state");
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA8, buffer, 112, 32);
    if ((rc = expect_error(&ctx, GL_INVALID_VALUE, 13, "out-of-bounds range")) != 0)
        return rc;
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA8, buffer, 16, 0);
    if ((rc = expect_error(&ctx, GL_INVALID_VALUE, 14, "zero range size")) != 0)
        return rc;

    /* Full range is semantically equivalent to TexBuffer and may use the whole-buffer native API.
     */
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_RG32F, buffer, 0, (GLsizeiptr)sizeof(data));
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 15, "full range attach")) != 0)
        return rc;
    if (cap.tex_buffer_calls != 1u || cap.tex_buffer_range_calls != 1u)
        return fail(16, "full range did not use equivalent native path");

    /* Core DSA must update the named object without changing the active binding. */
    glCreateTextures(GL_TEXTURE_BUFFER, 1, &dsa_tex);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 17, "create buffer texture")) != 0)
        return rc;
    before_bind_calls = cap.bind_buffer_texture_calls;
    glTextureBufferRange(dsa_tex, GL_R8, buffer, 32, 32);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 18, "DSA range attach")) != 0)
        return rc;
    glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &value);
    if ((GLuint)value != bound_tex)
        return fail(19, "DSA changed active texture binding");
    obj = mgl_texture_find(&ctx, dsa_tex);
    if (obj == NULL || obj->buffer_texture_offset != 32 || obj->buffer_texture_size != 32)
        return fail(20, "DSA range state");
    glGetTextureLevelParameteriv(dsa_tex, 0, GL_TEXTURE_BUFFER_OFFSET, &value);
    if (value != 32)
        return fail(21, "DSA offset query");
    glGetTextureLevelParameteriv(dsa_tex, 0, GL_TEXTURE_WIDTH, &value);
    if (value != 32)
        return fail(22, "DSA width query");
    if (cap.bind_buffer_texture_calls < before_bind_calls + 2u ||
        cap.last_bind_texture != mgl_texture_find(&ctx, bound_tex)->native_name)
        return fail(23, "DSA native binding restore");

    /* glBindTextureUnit must accept represented buffer textures and update the selected unit only.
     */
    glBindTextureUnit(3u, dsa_tex);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 24, "BindTextureUnit buffer target")) != 0)
        return rc;
    if (ctx.texture_buffer_binding[3] != dsa_tex || ctx.texture_buffer_binding[0] != bound_tex)
        return fail(25, "BindTextureUnit buffer ownership");

    glTextureBuffer(dsa_tex, GL_RGBA8, buffer);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 26, "DSA whole buffer attach")) != 0)
        return rc;
    if (obj->buffer_texture_offset != 0 || obj->buffer_texture_size != (GLsizeiptr)sizeof(data))
        return fail(27, "DSA whole-buffer state");

    /* EXT DSA accepts unused generated texture names; arbitrary unknown names are rejected. */
    glGenTextures(1, &ext_tex);
    glTextureBufferRangeEXT(ext_tex, GL_TEXTURE_BUFFER, GL_R8, buffer, 48, 16);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 28, "EXT generated-name range")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, ext_tex);
    if (obj == NULL || obj->target != GL_TEXTURE_BUFFER || obj->buffer_texture_offset != 48 ||
        obj->buffer_texture_size != 16)
        return fail(29, "EXT generated-name ownership");
    glTextureBufferRangeEXT(0x7fffffffu, GL_TEXTURE_BUFFER, GL_R8, buffer, 0, 16);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 30, "EXT unknown texture name")) != 0)
        return rc;
    glTextureBufferRangeEXT(ext_tex, GL_TEXTURE_2D, GL_R8, buffer, 0, 16);
    if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 31, "EXT invalid target")) != 0)
        return rc;

    glTexBufferRange(GL_TEXTURE_BUFFER, GL_R8, 0u, 99, 99);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 32, "range detach")) != 0)
        return rc;
    bound_obj = mgl_texture_find(&ctx, bound_tex);
    if (bound_obj == NULL || bound_obj->buffer_texture_buffer != NULL ||
        bound_obj->buffer_texture_offset != 0 || bound_obj->buffer_texture_size != 0)
        return fail(33, "detach did not reset range state");

    if (mgl_proc_address("glTexBufferRange") == NULL ||
        mgl_proc_address("glTextureBuffer") == NULL ||
        mgl_proc_address("glTextureBufferRange") == NULL ||
        mgl_proc_address("glTextureBufferRangeEXT") == NULL)
        return fail(34, "proc-address exposure");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL texture buffer range test: PASS");
    return 0;
}

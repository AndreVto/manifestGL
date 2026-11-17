#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static GLuint next_texture = 300u;
static GLuint next_buffer = 700u;
static GLenum native_active_texture = GL_TEXTURE0;

static void fake_gen_textures(GLsizei n, GLuint *textures) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        textures[i] = next_texture++;
}

static void fake_delete_textures(GLsizei n, const GLuint *textures) {
    (void)n;
    (void)textures;
}

static void fake_active_texture(GLenum texture) {
    native_active_texture = texture;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    (void)target;
    (void)texture;
}

static void fake_pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
}

static void fake_gen_buffers(GLsizei n, GLuint *buffers) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        buffers[i] = next_buffer++;
}

static void fake_delete_buffers(GLsizei n, const GLuint *buffers) {
    (void)n;
    (void)buffers;
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

static void fake_buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size,
                                 const void *data) {
    (void)target;
    (void)offset;
    (void)size;
    (void)data;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "EXT DSA MultiTex query failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_buffer *pack;
    GLuint textures[3] = {0u, 0u, 0u};
    GLuint pbo = 0u;
    const unsigned char image_a[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    const unsigned char image_b[8] = {11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u};
    const unsigned char compressed[8] = {31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u};
    unsigned char out[16];
    GLint iv;
    GLfloat fv;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesPixelStorei = fake_pixel_store;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesBufferSubData = fake_buffer_sub_data;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.unpack_alignment = 4;
    ctx.pack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glGenTextures(3, textures);
    if (textures[0] == 0u || textures[1] == 0u || textures[2] == 0u)
        return fail(2, "texture generation");

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_a);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_b);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, textures[2]);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 4, 4, 0,
                           (GLsizei)sizeof(compressed), compressed);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(3, "texture setup");

    glActiveTexture(GL_TEXTURE1);
    memset(out, 0, sizeof(out));
    glGetMultiTexImage(GL_TEXTURE3, GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || memcmp(out, image_b, sizeof(image_b)) != 0)
        return fail(4, "selector-free image query");
    if (ctx.compat.active_texture != 1u || native_active_texture != GL_TEXTURE1)
        return fail(5, "image query disturbed active texture");

    memset(out, 0, sizeof(out));
    glGetMultiTexImageEXT(GL_TEXTURE1, GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || memcmp(out, image_a, sizeof(image_a)) != 0 ||
        ctx.compat.active_texture != 1u)
        return fail(6, "EXT image query alias");

    iv = -1;
    glGetMultiTexLevelParameteriv(GL_TEXTURE3, GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &iv);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || iv != 2 || ctx.compat.active_texture != 1u)
        return fail(7, "integer level query");
    fv = -1.0f;
    glGetMultiTexLevelParameterfvEXT(GL_TEXTURE3, GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &fv);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || fv != 1.0f || ctx.compat.active_texture != 1u)
        return fail(8, "float level query alias");

    memset(out, 0, sizeof(out));
    glGetCompressedMultiTexImage(GL_TEXTURE4, GL_TEXTURE_2D, 0, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(out, compressed, sizeof(compressed)) != 0 || ctx.compat.active_texture != 1u)
        return fail(9, "compressed explicit-unit readback");
    memset(out, 0, sizeof(out));
    glGetCompressedMultiTexImageEXT(GL_TEXTURE4, GL_TEXTURE_2D, 0, out);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(out, compressed, sizeof(compressed)) != 0)
        return fail(10, "compressed EXT alias");

    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, 32, NULL, GL_STATIC_DRAW);
    glGetMultiTexImage(GL_TEXTURE3, GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                       (void *)(uintptr_t)5u);
    pack = mgl_buffer_find(&ctx, pbo);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || pack == NULL || pack->shadow == NULL ||
        memcmp(pack->shadow + 5u, image_b, sizeof(image_b)) != 0 || ctx.compat.active_texture != 1u)
        return fail(11, "pixel-pack-buffer inheritance");
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0u);

    iv = 0x12345678;
    glGetMultiTexLevelParameteriv(GL_TEXTURE_2D, GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &iv);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM || iv != 0x12345678 ||
        ctx.compat.active_texture != 1u)
        return fail(12, "non-unit selector validation/output preservation");
    iv = 0x23456789;
    glGetMultiTexLevelParameteriv(GL_TEXTURE0 + MGL_TEXTURE_UNITS, GL_TEXTURE_2D, 0,
                                  GL_TEXTURE_WIDTH, &iv);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || iv != 0x23456789 ||
        ctx.compat.active_texture != 1u)
        return fail(13, "unit range validation/output preservation");
    iv = 0x3456789a;
    glGetMultiTexLevelParameteriv(GL_TEXTURE3, GL_RENDERBUFFER, 0, GL_TEXTURE_WIDTH, &iv);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM || iv != 0x3456789a ||
        ctx.compat.active_texture != 1u)
        return fail(14, "target validation/output preservation");

    if (glXGetProcAddress((const GLubyte *)"glGetMultiTexImage") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetMultiTexImageEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetMultiTexLevelParameteriv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetMultiTexLevelParameterivEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetMultiTexLevelParameterfv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetMultiTexLevelParameterfvEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetCompressedMultiTexImage") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetCompressedMultiTexImageEXT") == NULL)
        return fail(15, "proc-address exposure");

    glDeleteBuffers(1, &pbo);
    glDeleteTextures(3, textures);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(16, "unexpected final GL error");
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL EXT DSA MultiTex query unit test: PASS");
    return 0;
}

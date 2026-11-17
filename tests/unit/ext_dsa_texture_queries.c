#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static GLuint next_texture = 2300u;
static GLuint next_buffer = 2700u;
static GLenum native_active_texture = GL_TEXTURE0;
static GLuint native_bound_2d;
static GLuint native_bound_cube;

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
    if (target == GL_TEXTURE_2D)
        native_bound_2d = texture;
    else if (target == GL_TEXTURE_CUBE_MAP)
        native_bound_cube = texture;
}

static void fake_tex_image_2d(GLenum target, GLint level, GLint internalformat, GLsizei width,
                              GLsizei height, GLint border, GLenum format, GLenum type,
                              const void *pixels) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)border;
    (void)format;
    (void)type;
    (void)pixels;
}

static void fake_tex_parameteri(GLenum target, GLenum pname, GLint param) {
    (void)target;
    (void)pname;
    (void)param;
}

static void fake_pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
}

static void fake_bind_buffer(GLenum target, GLuint buffer) {
    (void)target;
    (void)buffer;
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
    fprintf(stderr, "EXT DSA named texture query failure %d: %s\n", code, message);
    return code;
}

static int expect_error(struct mgl_context *ctx, GLenum expected, int code, const char *message) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr, "EXT DSA named texture query failure %d: %s (got 0x%04x expected 0x%04x)\n",
                code, message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *bound_2d_obj;
    struct mgl_texture *bound_cube_obj;
    struct mgl_buffer *pack;
    GLuint tex[5] = {0u, 0u, 0u, 0u, 0u};
    GLuint cube = 0u;
    GLuint pbo = 0u;
    const unsigned char image[16] = {1u, 2u,  3u,  4u,  5u,  6u,  7u,  8u,
                                     9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u};
    const unsigned char cube_px[2][4] = {{31u, 32u, 33u, 34u}, {41u, 42u, 43u, 44u}};
    const unsigned char compressed[8] = {51u, 52u, 53u, 54u, 55u, 56u, 57u, 58u};
    unsigned char out[64];
    GLint iv[4] = {0, 0, 0, 0};
    GLfloat fv[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GLint before_iv;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexImage2D = fake_tex_image_2d;
    native.glesTexParameteri = fake_tex_parameteri;
    native.glesPixelStorei = fake_pixel_store;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
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

    glGenTextures(5, tex);
    if (tex[0] == 0u || tex[4] == 0u)
        return fail(2, "texture generation");

    /* Keep a nonzero 2D texture bound on a non-default unit so every named query
       has observable selector and native state to preserve. */
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    bound_2d_obj = mgl_texture_find(&ctx, tex[0]);
    if (bound_2d_obj == NULL)
        return fail(3, "bound 2D object");

    /* Querying an unused generated name must realize it with the supplied target
       before returning that target's initial state. */
    if (glIsTexture(tex[1]) != GL_FALSE)
        return fail(4, "generated texture unexpectedly realized");
    iv[0] = -1;
    glGetTextureParameterivEXT(tex[1], GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, iv);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 5, "unused-name parameter query")) != 0)
        return rc;
    if (iv[0] != (GLint)GL_NEAREST_MIPMAP_LINEAR || glIsTexture(tex[1]) != GL_TRUE ||
        ctx.texture_2d_binding[3] != tex[0] || native_active_texture != GL_TEXTURE3 ||
        native_bound_2d != bound_2d_obj->native_name)
        return fail(6, "unused-name realization/query disturbed binding");

    glTextureImage2DEXT(tex[2], GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        image);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 7, "2D setup")) != 0)
        return rc;
    memset(out, 0, sizeof(out));
    glGetTextureImageEXT(tex[2], GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 8, "2D named image query")) != 0)
        return rc;
    if (memcmp(out, image, sizeof(image)) != 0 || ctx.texture_2d_binding[3] != tex[0] ||
        native_active_texture != GL_TEXTURE3 || native_bound_2d != bound_2d_obj->native_name)
        return fail(9, "2D named image payload/binding");

    iv[0] = -1;
    glGetTextureLevelParameterivEXT(tex[2], GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, iv);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 10, "integer level query")) != 0 || iv[0] != 2)
        return fail(11, "integer level value");
    fv[0] = -1.0f;
    glGetTextureLevelParameterfvEXT(tex[2], GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, fv);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 12, "float level query")) != 0 || fv[0] != 2.0f)
        return fail(13, "float level value");

    {
        const GLint border[4] = {INT32_MAX, 0, INT32_MAX, 0};
        glTextureParameteriv(tex[2], GL_TEXTURE_BORDER_COLOR, border);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 14, "border setup")) != 0)
            return rc;
        memset(iv, 0, sizeof(iv));
        glGetTextureParameterivEXT(tex[2], GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, iv);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 15, "integer border query")) != 0)
            return rc;
        if (iv[0] != border[0] || iv[1] != border[1] || iv[2] != border[2] || iv[3] != border[3])
            return fail(16, "integer border vector");
        memset(fv, 0, sizeof(fv));
        glGetTextureParameterfvEXT(tex[2], GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, fv);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 17, "float border query")) != 0)
            return rc;
        if (fv[0] != 1.0f || fv[1] != 0.0f || fv[2] != 1.0f || fv[3] != 0.0f)
            return fail(18, "float border vector conversion");
    }

    /* Texture zero denotes the default object for target and must not disturb
       the application's currently bound nonzero object. */
    glBindTexture(GL_TEXTURE_2D, 0u);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    iv[0] = -1;
    glGetTextureParameterivEXT(0u, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, iv);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 19, "default texture query")) != 0)
        return rc;
    if (iv[0] != (GLint)GL_CLAMP_TO_EDGE || ctx.texture_2d_binding[3] != tex[0] ||
        native_bound_2d != bound_2d_obj->native_name)
        return fail(20, "default texture zero/binding restoration");

    /* Used-name target mismatch must not write query output. */
    glBindTexture(GL_TEXTURE_2D, tex[3]);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    before_iv = 0x12345678;
    iv[0] = before_iv;
    glGetTextureLevelParameterivEXT(tex[3], GL_TEXTURE_3D, 0, GL_TEXTURE_WIDTH, iv);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 21, "target mismatch")) != 0)
        return rc;
    if (iv[0] != before_iv || ctx.texture_2d_binding[3] != tex[0])
        return fail(22, "target mismatch mutated output/binding");

    /* Pack PBO state remains latched exactly as for GetTexImage. */
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, 64, NULL, GL_STATIC_DRAW);
    glGetTextureImageEXT(tex[2], GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         (void *)(uintptr_t)7u);
    pack = mgl_buffer_find(&ctx, pbo);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 23, "pack PBO image query")) != 0)
        return rc;
    if (pack == NULL || pack->shadow == NULL ||
        memcmp(pack->shadow + 7u, image, sizeof(image)) != 0 || ctx.texture_2d_binding[3] != tex[0])
        return fail(24, "pack PBO payload/binding");
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0u);

    /* Cube queries address an individual face while object ownership remains
       GL_TEXTURE_CUBE_MAP. */
    glGenTextures(1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 cube_px[0]);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 cube_px[1]);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex[4]);
    bound_cube_obj = mgl_texture_find(&ctx, tex[4]);
    if (bound_cube_obj == NULL)
        return fail(25, "bound cube object");
    memset(out, 0, sizeof(out));
    glGetTextureImageEXT(cube, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 26, "cube face image query")) != 0)
        return rc;
    if (memcmp(out, cube_px[1], sizeof(cube_px[1])) != 0 || ctx.texture_cube_binding[3] != tex[4] ||
        native_bound_cube != bound_cube_obj->native_name)
        return fail(27, "cube face payload/binding");
    iv[0] = -1;
    glGetTextureLevelParameterivEXT(cube, GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, iv);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 28, "cube face level query")) != 0 || iv[0] != 1)
        return fail(29, "cube face level value");

    glCompressedTextureImage2DEXT(cube, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 1, GL_COMPRESSED_RED_RGTC1,
                                  4, 4, 0, (GLsizei)sizeof(compressed), compressed);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 30, "compressed cube setup")) != 0)
        return rc;
    memset(out, 0, sizeof(out));
    glGetCompressedTextureImageEXT(cube, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 1, out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 31, "compressed cube query")) != 0)
        return rc;
    if (memcmp(out, compressed, sizeof(compressed)) != 0 || ctx.texture_cube_binding[3] != tex[4] ||
        native_bound_cube != bound_cube_obj->native_name)
        return fail(32, "compressed cube payload/binding");

    {
        GLuint list = glGenLists(1);
        iv[0] = -1;
        glNewList(list, GL_COMPILE);
        glGetTextureLevelParameterivEXT(tex[2], GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, iv);
        glEndList();
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 33, "query during list compilation")) != 0)
            return rc;
        if (iv[0] != 2)
            return fail(34, "query was incorrectly retained");
        iv[0] = 77;
        glCallList(list);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 35, "query list replay")) != 0)
            return rc;
        if (iv[0] != 77)
            return fail(36, "query unexpectedly replayed");
        glDeleteLists(list, 1);
    }

    if (glXGetProcAddress((const GLubyte *)"glGetTextureImageEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetCompressedTextureImageEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetTextureParameterivEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetTextureParameterfvEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetTextureLevelParameterivEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetTextureLevelParameterfvEXT") == NULL)
        return fail(37, "proc-address exposure");

    glDeleteBuffers(1, &pbo);
    glDeleteTextures(5, tex);
    glDeleteTextures(1, &cube);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 38, "unexpected final GL error")) != 0)
        return rc;
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL EXT DSA named texture query unit test: PASS");
    return 0;
}

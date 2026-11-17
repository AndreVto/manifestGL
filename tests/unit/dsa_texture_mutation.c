#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    GLuint active_unit;
    GLuint bound_2d;
    GLuint bound_3d;
    GLuint bound_array;
    GLuint bound_cube;
    unsigned char read_seed;
    unsigned read_calls;
} cap;

static GLenum fake_get_error(void) {
    return GL_NO_ERROR;
}

static void fake_active_texture(GLenum texture) {
    cap.active_unit = (GLuint)(texture - GL_TEXTURE0);
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
    if (cap.active_unit != 0u)
        return;
    if (target == GL_TEXTURE_2D)
        cap.bound_2d = name;
    else if (target == GL_TEXTURE_3D)
        cap.bound_3d = name;
    else if (target == GL_TEXTURE_2D_ARRAY)
        cap.bound_array = name;
    else if (target == GL_TEXTURE_CUBE_MAP)
        cap.bound_cube = name;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_pixel_store(GLenum pname, GLint value) {
    (void)pname;
    (void)value;
}

static void fake_bind_framebuffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
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

static void fake_tex_sub_image_2d(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                  GLsizei width, GLsizei height, GLenum format, GLenum type,
                                  const void *pixels) {
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)width;
    (void)height;
    (void)format;
    (void)type;
    (void)pixels;
}

static void fake_tex_image_3d(GLenum target, GLint level, GLint internalformat, GLsizei width,
                              GLsizei height, GLsizei depth, GLint border, GLenum format,
                              GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)depth;
    (void)border;
    (void)format;
    (void)type;
    (void)pixels;
}

static void fake_tex_sub_image_3d(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                  GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                  GLenum format, GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)zoffset;
    (void)width;
    (void)height;
    (void)depth;
    (void)format;
    (void)type;
    (void)pixels;
}

static void fake_read_pixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                             GLenum type, void *pixels) {
    unsigned char *dst = pixels;
    size_t count = (size_t)width * (size_t)height * 4u;
    size_t i;
    (void)x;
    (void)y;
    (void)format;
    (void)type;
    ++cap.read_calls;
    for (i = 0u; i < count; ++i)
        dst[i] = (unsigned char)(cap.read_seed + (unsigned char)i);
}

static int fail(int code, const char *message) {
    fprintf(stderr, "DSA texture mutation failure %d: %s\n", code, message);
    return code;
}

static int expect_error(struct mgl_context *ctx, GLenum expected, int code, const char *message) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr, "DSA texture mutation failure %d: %s (got 0x%04x expected 0x%04x)\n", code,
                message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj;
    GLuint tex2d[4] = {0u, 0u, 0u, 0u};
    GLuint arrays[2] = {0u, 0u};
    GLuint tex3d[2] = {0u, 0u};
    GLuint cubes[4] = {0u, 0u, 0u, 0u};
    unsigned char rgba2d[16] = {0};
    unsigned char rgba3d[32] = {0};
    unsigned char array_rgba[32] = {0};
    unsigned char compressed_2d[16] = {1u, 2u,  3u,  4u,  5u,  6u,  7u,  8u,
                                       9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u};
    unsigned char compressed_array[16] = {20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u,
                                          30u, 31u, 32u, 33u, 34u, 35u, 36u, 37u};
    unsigned char patch[8] = {90u, 91u, 92u, 93u, 94u, 95u, 96u, 97u};
    unsigned char cube_patch[8] = {101u, 102u, 103u, 104u, 111u, 112u, 113u, 114u};
    unsigned char compressed_cube_face[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    unsigned char compressed_cube_patch[16] = {130u, 131u, 132u, 133u, 134u, 135u, 136u, 137u,
                                               140u, 141u, 142u, 143u, 144u, 145u, 146u, 147u};
    GLint binding = 0;
    GLuint bound_native;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.next_texture = 1100u;
    native.glesGetError = fake_get_error;
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesPixelStorei = fake_pixel_store;
    native.glesBindFramebuffer = fake_bind_framebuffer;
    native.glesTexImage2D = fake_tex_image_2d;
    native.glesTexSubImage2D = fake_tex_sub_image_2d;
    native.glesTexImage3D = fake_tex_image_3d;
    native.glesTexSubImage3D = fake_tex_sub_image_3d;
    native.glesReadPixels = fake_read_pixels;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glCreateTextures(GL_TEXTURE_2D, 4, tex2d);
    glBindTexture(GL_TEXTURE_2D, tex2d[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba2d);
    glBindTexture(GL_TEXTURE_2D, tex2d[1]);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 8, 4, 0,
                           (GLsizei)sizeof(compressed_2d), compressed_2d);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 2, "compressed 2D setup")) != 0)
        return rc;
    glBindTexture(GL_TEXTURE_2D, tex2d[0]);
    obj = mgl_texture_find(&ctx, tex2d[0]);
    if (obj == NULL)
        return fail(3, "bound 2D object");
    bound_native = obj->native_name;
    glCompressedTextureSubImage2D(tex2d[1], 0, 4, 0, 4, 4, GL_COMPRESSED_RED_RGTC1, 8, patch);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 4, "named compressed 2D subimage")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, tex2d[1]);
    if (obj == NULL || memcmp(obj->levels[0].data, compressed_2d, 8u) != 0 ||
        memcmp(obj->levels[0].data + 8u, patch, 8u) != 0)
        return fail(5, "named compressed 2D canonical update");
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    if ((GLuint)binding != tex2d[0] || cap.bound_2d != bound_native)
        return fail(6, "named compressed 2D preserves binding");

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 2, arrays);
    glBindTexture(GL_TEXTURE_2D_ARRAY, arrays[0]);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 array_rgba);
    glBindTexture(GL_TEXTURE_2D_ARRAY, arrays[1]);
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 2, 0,
                           (GLsizei)sizeof(compressed_array), compressed_array);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 7, "compressed array setup")) != 0)
        return rc;
    glBindTexture(GL_TEXTURE_2D_ARRAY, arrays[0]);
    obj = mgl_texture_find(&ctx, arrays[0]);
    if (obj == NULL)
        return fail(8, "bound array object");
    bound_native = obj->native_name;
    glCompressedTextureSubImage3D(arrays[1], 0, 0, 0, 1, 4, 4, 1, GL_COMPRESSED_RED_RGTC1, 8,
                                  patch);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 9, "named compressed array subimage")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, arrays[1]);
    if (obj == NULL || memcmp(obj->levels[0].data, compressed_array, 8u) != 0 ||
        memcmp(obj->levels[0].data + 8u, patch, 8u) != 0)
        return fail(10, "named compressed array canonical update");
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &binding);
    if ((GLuint)binding != arrays[0] || cap.bound_array != bound_native)
        return fail(11, "named compressed array preserves binding");

    glBindTexture(GL_TEXTURE_2D, tex2d[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba2d);
    glBindTexture(GL_TEXTURE_2D, tex2d[0]);
    obj = mgl_texture_find(&ctx, tex2d[0]);
    bound_native = obj != NULL ? obj->native_name : 0u;
    cap.read_seed = 40u;
    glCopyTextureSubImage2D(tex2d[2], 0, 1, 1, 0, 0, 1, 1);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 12, "named copy 2D")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, tex2d[2]);
    if (obj == NULL ||
        memcmp(obj->levels[0].data + 12u, (const unsigned char[]){40u, 41u, 42u, 43u}, 4u) != 0)
        return fail(13, "named copy 2D canonical update");
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    if ((GLuint)binding != tex2d[0] || cap.bound_2d != bound_native)
        return fail(14, "named copy 2D preserves binding");

    glCreateTextures(GL_TEXTURE_3D, 2, tex3d);
    glBindTexture(GL_TEXTURE_3D, tex3d[0]);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba3d);
    glBindTexture(GL_TEXTURE_3D, tex3d[1]);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba3d);
    glBindTexture(GL_TEXTURE_3D, tex3d[0]);
    obj = mgl_texture_find(&ctx, tex3d[0]);
    bound_native = obj != NULL ? obj->native_name : 0u;
    cap.read_seed = 70u;
    glCopyTextureSubImage3D(tex3d[1], 0, 1, 0, 1, 0, 0, 1, 1);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 15, "named copy 3D")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, tex3d[1]);
    if (obj == NULL ||
        memcmp(obj->levels[0].data + 20u, (const unsigned char[]){70u, 71u, 72u, 73u}, 4u) != 0)
        return fail(16, "named copy 3D canonical update");
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &binding);
    if ((GLuint)binding != tex3d[0] || cap.bound_3d != bound_native)
        return fail(17, "named copy 3D preserves binding");

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 4, cubes);
    glTextureStorage2D(cubes[1], 1, GL_RGBA8, 2, 2);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 18, "cube storage setup")) != 0)
        return rc;
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubes[0]);
    obj = mgl_texture_find(&ctx, cubes[0]);
    if (obj == NULL)
        return fail(19, "bound cube object");
    bound_native = obj->native_name;
    glTextureSubImage3D(cubes[1], 0, 1, 0, 2, 1, 1, 2, GL_RGBA, GL_UNSIGNED_BYTE, cube_patch);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 20, "named cube 3D subimage")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, cubes[1]);
    if (obj == NULL || memcmp(obj->cube_levels[2][0].data + 4u, cube_patch, 4u) != 0 ||
        memcmp(obj->cube_levels[3][0].data + 4u, cube_patch + 4u, 4u) != 0)
        return fail(21, "named cube 3D canonical update");
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &binding);
    if ((GLuint)binding != cubes[0] || cap.bound_cube != bound_native)
        return fail(22, "named cube 3D preserves binding");

    cap.read_seed = 120u;
    glCopyTextureSubImage3D(cubes[1], 0, 0, 1, 4, 0, 0, 1, 1);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 23, "named cube copy 3D")) != 0)
        return rc;
    if (memcmp(obj->cube_levels[4][0].data + 8u, (const unsigned char[]){120u, 121u, 122u, 123u},
               4u) != 0)
        return fail(24, "named cube copy canonical update");
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &binding);
    if ((GLuint)binding != cubes[0] || cap.bound_cube != bound_native)
        return fail(25, "named cube copy preserves binding");

    glBindTexture(GL_TEXTURE_CUBE_MAP, cubes[2]);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 rgba2d);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 26, "partial cube setup")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, cubes[2]);
    if (obj == NULL)
        return fail(27, "partial cube object");
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubes[0]);
    glTextureSubImage3D(cubes[2], 0, 0, 0, 0, 1, 1, 2, GL_RGBA, GL_UNSIGNED_BYTE, cube_patch);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 28, "cube 3D prevalidation")) != 0)
        return rc;
    if (memcmp(obj->cube_levels[0][0].data, rgba2d, sizeof(rgba2d)) != 0)
        return fail(29, "cube 3D error is atomic");

    glTextureSubImage3D(cubes[1], 0, 0, 0, 5, 1, 1, 2, GL_RGBA, GL_UNSIGNED_BYTE, cube_patch);
    if ((rc = expect_error(&ctx, GL_INVALID_VALUE, 30, "cube 3D face range")) != 0)
        return rc;
    glCopyTextureSubImage3D(cubes[1], 0, 0, 0, 6, 0, 0, 1, 1);
    if ((rc = expect_error(&ctx, GL_INVALID_VALUE, 31, "cube copy face range")) != 0)
        return rc;

    glBindTexture(GL_TEXTURE_CUBE_MAP, cubes[3]);
    for (unsigned face = 0u; face < 6u; ++face) {
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_COMPRESSED_RED_RGTC1, 4,
                               4, 0, (GLsizei)sizeof(compressed_cube_face), compressed_cube_face);
    }
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 32, "compressed cube setup")) != 0)
        return rc;
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubes[0]);
    obj = mgl_texture_find(&ctx, cubes[0]);
    bound_native = obj != NULL ? obj->native_name : 0u;
    glCompressedTextureSubImage3D(cubes[3], 0, 0, 0, 1, 4, 4, 2, GL_COMPRESSED_RED_RGTC1,
                                  (GLsizei)sizeof(compressed_cube_patch), compressed_cube_patch);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 33, "named compressed cube subimage")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, cubes[3]);
    if (obj == NULL || memcmp(obj->cube_levels[1][0].data, compressed_cube_patch, 8u) != 0 ||
        memcmp(obj->cube_levels[2][0].data, compressed_cube_patch + 8u, 8u) != 0)
        return fail(34, "named compressed cube canonical update");
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &binding);
    if ((GLuint)binding != cubes[0] || cap.bound_cube != bound_native)
        return fail(35, "named compressed cube preserves binding");
    glCompressedTextureSubImage3D(cubes[3], 0, 0, 0, 5, 4, 4, 2, GL_COMPRESSED_RED_RGTC1,
                                  (GLsizei)sizeof(compressed_cube_patch), compressed_cube_patch);
    if ((rc = expect_error(&ctx, GL_INVALID_VALUE, 36, "compressed cube face range")) != 0)
        return rc;

    glCompressedTextureSubImage2D(arrays[1], 0, 0, 0, 4, 4, GL_COMPRESSED_RED_RGTC1, 8, patch);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 37, "named compressed target mismatch")) !=
        0)
        return rc;
    glCopyTextureSubImage2D(0xdeadbeefu, 0, 0, 0, 0, 0, 1, 1);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 38, "unknown named copy texture")) != 0)
        return rc;

    if (glXGetProcAddress((const GLubyte *)"glCompressedTextureSubImage2D") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCompressedTextureSubImage3D") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCopyTextureSubImage2D") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCopyTextureSubImage3D") == NULL)
        return fail(39, "proc-address exposure");

    if (cap.read_calls != 3u)
        return fail(40, "unexpected framebuffer read count");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL DSA texture mutation unit test: PASS");
    return 0;
}

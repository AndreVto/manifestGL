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
    GLuint bound_cube;
} cap;

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
    else if (target == GL_TEXTURE_CUBE_MAP)
        cap.bound_cube = name;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_gen_buffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_texture;
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

static void fake_pixel_store(GLenum pname, GLint value) {
    (void)pname;
    (void)value;
}

static void fake_tex_parameteri(GLenum target, GLenum pname, GLint value) {
    (void)target;
    (void)pname;
    (void)value;
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

static int fail(int code, const char *message) {
    fprintf(stderr, "DSA texture readback failure %d: %s\n", code, message);
    return code;
}

static int expect_error(struct mgl_context *ctx, GLenum expected, int code, const char *message) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr, "DSA texture readback failure %d: %s (got 0x%04x expected 0x%04x)\n", code,
                message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *bound_obj;
    GLuint textures[3] = {0u, 0u, 0u};
    GLuint cube = 0u, cube_red = 0u, compressed = 0u;
    unsigned char a[16], b[16], out[256], before[256];
    unsigned char faces[6][16];
    unsigned char block[8] = {17u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    unsigned char volume[32], compressed_pair[16];
    GLuint array_tex = 0u, pack_pbo = 0u;
    GLint binding = 0;
    unsigned face;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.next_texture = 900u;
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesBufferData = fake_buffer_data;
    native.glesBufferSubData = fake_buffer_sub_data;
    native.glesPixelStorei = fake_pixel_store;
    native.glesTexParameteri = fake_tex_parameteri;
    native.glesTexImage2D = fake_tex_image_2d;
    native.glesTexImage3D = fake_tex_image_3d;

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

    memset(a, 0x11, sizeof(a));
    memset(b, 0x72, sizeof(b));
    glCreateTextures(GL_TEXTURE_2D, 3, textures);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, a);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, b);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    bound_obj = mgl_texture_find(&ctx, textures[0]);
    if (bound_obj == NULL)
        return fail(2, "bound object");

    memset(out, 0xcc, sizeof(out));
    glGetTextureImage(textures[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, 16, out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 3, "2D DSA readback")) != 0)
        return rc;
    if (memcmp(out, b, sizeof(b)) != 0)
        return fail(4, "2D DSA payload");
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    if ((GLuint)binding != textures[0] || cap.bound_2d != bound_obj->native_name)
        return fail(5, "2D DSA readback preserves binding");

    memset(out, 0xa5, sizeof(out));
    memcpy(before, out, sizeof(out));
    glGetTextureImage(textures[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, 15, out);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 6, "too-small DSA buffer")) != 0)
        return rc;
    if (memcmp(out, before, sizeof(out)) != 0)
        return fail(7, "too-small DSA buffer does not write");
    glGetTextureImage(textures[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, -1, out);
    if ((rc = expect_error(&ctx, GL_INVALID_VALUE, 8, "negative DSA buffer size")) != 0)
        return rc;
    glGetTextureImage(0xdeadbeefu, 0, GL_RGBA, GL_UNSIGNED_BYTE, 16, out);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 9, "unknown DSA texture")) != 0)
        return rc;
    glGetTextureImage(textures[2], 0, GL_RGBA, GL_UNSIGNED_BYTE, 16, out);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 10, "undefined DSA image")) != 0)
        return rc;

    memset(out, 0xa5, sizeof(out));
    memcpy(before, out, sizeof(out));
    glGetnTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, 15, out);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 20, "bounded readback size")) != 0)
        return rc;
    if (memcmp(out, before, sizeof(out)) != 0)
        return fail(21, "bounded readback does not write");
    glGetnTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, 16, out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 22, "bounded readback")) != 0)
        return rc;
    if (memcmp(out, a, sizeof(a)) != 0)
        return fail(23, "bounded readback payload");
    glGetnTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, -1, out);
    if ((rc = expect_error(&ctx, GL_INVALID_VALUE, 24, "negative bounded size")) != 0)
        return rc;

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    for (face = 0u; face < 6u; ++face) {
        size_t i;
        GLenum target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
        for (i = 0u; i < sizeof(faces[face]); ++i)
            faces[face][i] = (unsigned char)(face * 32u + i);
        glTexImage2D(target, 0, (GLint)GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, faces[face]);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0u);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 3);
    glPixelStorei(GL_PACK_IMAGE_HEIGHT, 3);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 1);
    glPixelStorei(GL_PACK_SKIP_ROWS, 1);
    glPixelStorei(GL_PACK_SKIP_IMAGES, 1);
    memset(out, 0xee, sizeof(out));
    glGetTextureImage(cube, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLsizei)sizeof(out), out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 11, "cube DSA readback")) != 0)
        return rc;
    for (face = 0u; face < 6u; ++face) {
        size_t image_stride = 36u;
        size_t start = 36u + 12u + 4u;
        unsigned char *face_out = out + start + (size_t)face * image_stride;
        if (memcmp(face_out, faces[face], 8u) != 0 ||
            memcmp(face_out + 12u, faces[face] + 8u, 8u) != 0)
            return fail(12, "cube DSA face ordering/layout");
    }
    if (out[0] != 0xeeu)
        return fail(13, "cube DSA pack skip preservation");

    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_IMAGE_HEIGHT, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_IMAGES, 0);
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cube_red);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube_red);
    for (face = 0u; face < 6u; ++face) {
        unsigned char value = (unsigned char)(10u + face);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, (GLint)GL_R8, 1, 1, 0, GL_RED,
                     GL_UNSIGNED_BYTE, &value);
    }
    memset(out, 0xcd, sizeof(out));
    glGetTextureImage(cube_red, 0, GL_RED, GL_UNSIGNED_BYTE, 6, out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 60, "single-channel cube readback")) != 0)
        return rc;
    for (face = 0u; face < 6u; ++face)
        if (out[face] != (unsigned char)(10u + face))
            return fail(61, "single-channel cube values");
    if (out[6] != 0xcdu)
        return fail(62, "single-channel cube guard");
    memset(out, 0xcd, sizeof(out));
    glGetTextureImage(cube_red, 0, GL_RGBA, GL_UNSIGNED_BYTE, 24, out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 63, "expanded cube readback")) != 0)
        return rc;
    for (face = 0u; face < 6u; ++face) {
        if (out[4u * face] != (unsigned char)(10u + face) || out[4u * face + 1u] != 0u ||
            out[4u * face + 2u] != 0u || out[4u * face + 3u] != 255u)
            return fail(64, "expanded cube values");
    }
    glCreateTextures(GL_TEXTURE_2D, 1, &compressed);
    glBindTexture(GL_TEXTURE_2D, compressed);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8, block);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 14, "compressed setup")) != 0)
        return rc;
    memset(out, 0, sizeof(out));
    glGetCompressedTextureImage(compressed, 0, 8, out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 15, "compressed DSA readback")) != 0)
        return rc;
    if (memcmp(out, block, sizeof(block)) != 0)
        return fail(16, "compressed DSA payload");
    memset(out, 0x5a, sizeof(out));
    memcpy(before, out, sizeof(out));
    glGetCompressedTextureImage(compressed, 0, 7, out);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 17, "compressed DSA size check")) != 0)
        return rc;
    if (memcmp(out, before, sizeof(out)) != 0)
        return fail(18, "compressed size failure does not write");

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 3);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 1);
    glPixelStorei(GL_PACK_SKIP_ROWS, 1);
    memset(out, 0xa5, sizeof(out));
    glGetTextureSubImage(textures[1], 0, 1, 0, 0, 1, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                         (GLsizei)sizeof(out), out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 30, "subimage 2D readback")) != 0)
        return rc;
    if (memcmp(out + 16, b + 4, 4) || memcmp(out + 28, b + 12, 4) || out[0] != 0xa5u)
        return fail(31, "subimage 2D pack layout");
    glGetTextureSubImage(textures[1], 0, 1, 0, 0, 1, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, 19, out);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 32, "subimage size validation")) != 0)
        return rc;
    glGetTextureSubImage(textures[1], 0, 2, 0, 0, 1, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                         (GLsizei)sizeof(out), out);
    if ((rc = expect_error(&ctx, GL_INVALID_VALUE, 33, "subimage bounds")) != 0)
        return rc;
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    memset(out, 0x3a, sizeof(out));
    glGetTextureSubImage(cube, 0, 1, 1, 2, 1, 1, 2, GL_RGBA, GL_UNSIGNED_BYTE, (GLsizei)sizeof(out),
                         out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 34, "subimage cube slice")) != 0)
        return rc;
    if (memcmp(out, faces[2] + 12, 4) || memcmp(out + 4, faces[3] + 12, 4))
        return fail(35, "subimage cube face order");
    glGetCompressedTextureSubImage(compressed, 0, 0, 0, 0, 4, 4, 1, 8, out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 36, "compressed subimage")) != 0)
        return rc;
    if (memcmp(out, block, 8))
        return fail(37, "compressed subimage bytes");
    memset(out, 0x33, sizeof(out));
    glGetCompressedTextureSubImage(compressed, 0, 0, 0, 0, 4, 4, 1, 7, out);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 38, "compressed subimage size")) != 0)
        return rc;
    if (out[0] != 0x33u)
        return fail(39, "compressed subimage preserve output");
    glGetCompressedTextureSubImage(compressed, 0, 1, 0, 0, 3, 4, 1, 8, out);
    if ((rc = expect_error(&ctx, GL_INVALID_VALUE, 40, "compressed block offset")) != 0)
        return rc;

    memset(volume, 0x11, 16);
    memset(volume + 16, 0x92, 16);
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &array_tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, array_tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, volume);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 41, "array image setup")) != 0)
        return rc;
    memset(out, 0, sizeof(out));
    glGetTextureSubImage(array_tex, 0, 1, 0, 1, 1, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, 8, out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 42, "array subimage")) != 0)
        return rc;
    if (memcmp(out, volume + 20, 4) || memcmp(out + 4, volume + 28, 4))
        return fail(43, "array layer/row order");
    memcpy(compressed_pair, block, 8);
    memset(compressed_pair + 8, 0xcd, 8);
    glBindTexture(GL_TEXTURE_2D, compressed);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 8, 4, 0, 16, compressed_pair);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 44, "two-block compressed setup")) != 0)
        return rc;
    memset(out, 0, sizeof(out));
    glGetCompressedTextureSubImage(compressed, 0, 4, 0, 0, 4, 4, 1, 8, out);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 45, "compressed partial block")) != 0)
        return rc;
    if (memcmp(out, compressed_pair + 8, 8))
        return fail(46, "compressed partial payload");

    glGenBuffers(1, &pack_pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pack_pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, 64, NULL, GL_STATIC_DRAW);
    glGetTextureSubImage(textures[1], 0, 1, 0, 0, 1, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, 8,
                         (void *)(uintptr_t)5u);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 47, "subimage pack-buffer")) != 0)
        return rc;
    {
        struct mgl_buffer *pack = mgl_buffer_find(&ctx, pack_pbo);
        if (pack == NULL || pack->shadow == NULL || memcmp(pack->shadow + 5, b + 4, 4) ||
            memcmp(pack->shadow + 9, b + 12, 4))
            return fail(48, "subimage pack-buffer data");
    }
    glGetCompressedTextureSubImage(compressed, 0, 4, 0, 0, 4, 4, 1, 8, (void *)(uintptr_t)23u);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 49, "compressed pack-buffer")) != 0)
        return rc;
    {
        struct mgl_buffer *pack = mgl_buffer_find(&ctx, pack_pbo);
        if (pack == NULL || pack->shadow == NULL ||
            memcmp(pack->shadow + 23, compressed_pair + 8, 8))
            return fail(50, "compressed pack-buffer data");
    }
    glGetCompressedTextureSubImage(compressed, 0, 4, 0, 0, 4, 4, 1, 8, (void *)(uintptr_t)60u);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 51,
                           "compressed pack-buffer out of bounds")) != 0)
        return rc;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    if (glXGetProcAddress((const GLubyte *)"glGetTextureSubImage") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetCompressedTextureSubImage") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetTextureImage") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetCompressedTextureImage") == NULL)
        return fail(19, "proc-address exposure");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL DSA texture readback unit test: PASS");
    return 0;
}

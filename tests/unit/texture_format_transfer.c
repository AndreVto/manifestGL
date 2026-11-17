#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static struct {
    GLuint next_name;
    GLint internal;
    GLenum format;
    GLenum type;
    GLenum target;
    unsigned image_calls;
    unsigned subimage_calls;
    unsigned char data[32];
} cap;

static void fake_gen(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i)
        names[i] = ++cap.next_name;
}

static void fake_bind(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_delete(GLsizei count, const GLuint *names) {
    (void)count;
    (void)names;
}

static void capture_image(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height,
                          GLint border, GLenum format, GLenum type, const void *data) {
    size_t components = format == GL_RED ? 1u : format == GL_RG ? 2u : format == GL_RGB ? 3u : 4u;
    size_t bytes = (size_t)width * (size_t)height * components * (type == GL_HALF_FLOAT ? 2u : 1u);
    (void)level;
    (void)border;
    cap.target = target;
    cap.internal = internal;
    cap.format = format;
    cap.type = type;
    ++cap.image_calls;
    if (data != NULL && bytes <= sizeof(cap.data))
        memcpy(cap.data, data, bytes);
}

static void capture_image_3d(GLenum target, GLint level, GLint internal, GLsizei width,
                             GLsizei height, GLsizei depth, GLint border, GLenum format,
                             GLenum type, const void *data) {
    capture_image(target, level, internal, width, height * depth, border, format, type, data);
}

static void capture_subimage(GLenum target, GLint level, GLint x, GLint y, GLsizei width,
                             GLsizei height, GLenum format, GLenum type, const void *data) {
    (void)x;
    (void)y;
    capture_image(target, level, 0, width, height, 0, format, type, data);
    ++cap.subimage_calls;
}

static int check(int ok, const char *name) {
    if (ok)
        return 0;
    fprintf(stderr, "texture format transfer: %s\n", name);
    return 1;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint name;
    uint16_t half;
    const uint32_t bgra[2] = {0xffff4005u, 0xff0080ffu};
    const GLubyte rg[4] = {0u, 255u, 255u, 0u};
    const GLubyte red[2] = {255u, 0u};
    const GLubyte rgba3d[8] = {0u, 255u, 255u, 0u, 255u, 0u, 0u, 255u};
    const uint16_t half_rg[4] = {0x3c00u, 0u, 0u, 0x3c00u};
    const GLfloat rgba_float[8] = {2.0f, -1.0f, 0.0f, 1.0f, -2.0f, 1.0f, 0.0f, 1.0f};
    const GLushort rgba_short[8] = {65535u, 32768u, 0u, 65535u, 0u, 0u, 0u, 65535u};
    int failed = 0;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGenTextures = fake_gen;
    native.glesBindTexture = fake_bind;
    native.glesDeleteTextures = fake_delete;
    native.glesTexImage2D = capture_image;
    native.glesTexImage3D = capture_image_3d;
    native.glesTexSubImage2D = capture_subimage;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.unpack_alignment = 4;
    ctx.pack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);
    glGenTextures(1, &name);
    glBindTexture(GL_TEXTURE_2D, name);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 2, 1, 0, GL_RED, GL_UNSIGNED_BYTE, red);
    failed |=
        check(glGetError() == GL_NO_ERROR && cap.internal == (GLint)GL_R8 && cap.format == GL_RED &&
                  cap.type == GL_UNSIGNED_BYTE && cap.data[0] == 255u && cap.data[1] == 0u,
              "R8 red");

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, 2, 1, 0, GL_RG, GL_UNSIGNED_BYTE, rg);
    failed |=
        check(glGetError() == GL_NO_ERROR && cap.internal == (GLint)GL_RG8 && cap.format == GL_RG &&
                  cap.type == GL_UNSIGNED_BYTE && memcmp(cap.data, rg, sizeof(rg)) == 0,
              "RG8 rg");

    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 2, 1, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, bgra);
    failed |=
        check(glGetError() == GL_NO_ERROR && cap.format == GL_RED && cap.type == GL_UNSIGNED_BYTE &&
                  cap.data[0] == 0xffu && cap.data[1] == 0x00u,
              "R8 BGRA packed");

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, 2, 1, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, bgra);
    failed |= check(glGetError() == GL_NO_ERROR && cap.format == GL_RG &&
                        cap.type == GL_UNSIGNED_BYTE && cap.data[0] == 0xffu &&
                        cap.data[1] == 0x40u && cap.data[2] == 0x00u && cap.data[3] == 0x80u,
                    "RG8 BGRA packed");

    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, 2, 1, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, bgra);
    failed |= check(glGetError() == GL_NO_ERROR && cap.internal == (GLint)GL_R16F &&
                        cap.format == GL_RED && cap.type == GL_HALF_FLOAT,
                    "R16F BGRA packed");
    memcpy(&half, cap.data, sizeof(half));
    failed |= check(half == 0x3c00u, "R16F half precision");

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 2, 1, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, bgra);
    failed |= check(glGetError() == GL_NO_ERROR && cap.internal == (GLint)GL_RG16F &&
                        cap.format == GL_RG && cap.type == GL_HALF_FLOAT,
                    "RG16F BGRA packed");
    memcpy(&half, cap.data + 2u, sizeof(half));
    failed |= check(half > 0u && half < 0x3c00u, "RG16F green channel");

    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, 2, 1, 0, GL_RED, GL_UNSIGNED_BYTE, red);
    failed |=
        check(glGetError() == GL_NO_ERROR && cap.format == GL_RED && cap.type == GL_HALF_FLOAT,
              "R16F red byte");
    memcpy(&half, cap.data, sizeof(half));
    failed |= check(half == 0x3c00u, "R16F full precision");

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 2, 1, 0, GL_RG, GL_UNSIGNED_BYTE, rg);
    failed |= check(glGetError() == GL_NO_ERROR && cap.format == GL_RG && cap.type == GL_HALF_FLOAT,
                    "RG16F rg byte");

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RG, GL_UNSIGNED_BYTE, rg);
    failed |= check(glGetError() == GL_NO_ERROR && cap.subimage_calls == 1u &&
                        cap.format == GL_RG && cap.type == GL_HALF_FLOAT,
                    "RG16F subimage");

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 2, 1, 0, GL_RGBA, GL_FLOAT, rgba_float);
    failed |= check(glGetError() == GL_NO_ERROR && cap.format == GL_RG && cap.type == GL_HALF_FLOAT,
                    "RG16F floating-point input");
    memcpy(&half, cap.data, sizeof(half));
    failed |= check(half == 0x4000u, "RG16F positive float");
    memcpy(&half, cap.data + 2u, sizeof(half));
    failed |= check(half == 0xbc00u, "RG16F negative float");

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_SHORT, rgba_short);
    failed |= check(glGetError() == GL_NO_ERROR && cap.format == GL_RG &&
                        cap.type == GL_UNSIGNED_BYTE && cap.data[0] == 255u && cap.data[1] == 128u,
                    "RG8 unsigned-short input");

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 2, 1, 0, GL_RG, GL_HALF_FLOAT, half_rg);
    failed |=
        check(glGetError() == GL_NO_ERROR && cap.format == GL_RG && cap.type == GL_HALF_FLOAT &&
                  memcmp(cap.data, half_rg, sizeof(half_rg)) == 0,
              "RG16F half-float source");
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RG, GL_UNSIGNED_BYTE, rg);
    memcpy(&half, cap.data + 2u, sizeof(half));
    failed |= check(glGetError() == GL_NO_ERROR && cap.format == GL_RG &&
                        cap.type == GL_HALF_FLOAT && half == 0x3c00u,
                    "RG16F half-float shadow update");

    glGenTextures(1, &name);
    glBindTexture(GL_TEXTURE_1D, name);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_R8, 2, 0, GL_RED, GL_UNSIGNED_BYTE, red);
    failed |= check(glGetError() == GL_NO_ERROR && cap.target == GL_TEXTURE_2D &&
                        cap.internal == (GLint)GL_R8 && cap.format == GL_RED,
                    "R8 texture 1D");

    glGenTextures(1, &name);
    glBindTexture(GL_TEXTURE_3D, name);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RG16F, 2, 1, 2, 0, GL_RG, GL_UNSIGNED_BYTE, rgba3d);
    failed |= check(glGetError() == GL_NO_ERROR && cap.target == GL_TEXTURE_3D &&
                        cap.internal == (GLint)GL_RG16F && cap.format == GL_RG &&
                        cap.type == GL_HALF_FLOAT,
                    "RG16F texture 3D");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return failed ? 1 : 0;
}

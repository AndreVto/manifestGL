#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

static struct {
    GLuint next;
    unsigned images, subimages, storages;
    GLenum format, type;
    GLint internalformat;
    unsigned char pixel[32];
} cap;

static void fake_gen(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i)
        names[i] = ++cap.next;
}

static void fake_bind(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_delete(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_active(GLenum target) {
    (void)target;
}

static void fake_parameter(GLenum target, GLenum name, GLint value) {
    (void)target;
    (void)name;
    (void)value;
}

static void fake_image(GLenum target, GLint level, GLint internalformat, GLsizei width,
                       GLsizei height, GLint border, GLenum format, GLenum type,
                       const void *pixels) {
    (void)target;
    (void)level;
    (void)width;
    (void)height;
    (void)border;
    (void)pixels;
    cap.images++;
    cap.internalformat = internalformat;
    cap.format = format;
    cap.type = type;
}

static void fake_image3d(GLenum target, GLint level, GLint internalformat, GLsizei width,
                         GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type,
                         const void *pixels) {
    (void)depth;
    fake_image(target, level, internalformat, width, height, border, format, type, pixels);
}

static void fake_sub(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
                     GLenum format, GLenum type, const void *pixels) {
    (void)x;
    (void)y;
    cap.subimages++;
    fake_image(target, level, 0, width, height, 0, format, type, pixels);
}

static void fake_sub3d(GLenum target, GLint level, GLint x, GLint y, GLint z, GLsizei width,
                       GLsizei height, GLsizei depth, GLenum format, GLenum type,
                       const void *pixels) {
    (void)z;
    (void)depth;
    fake_sub(target, level, x, y, width, height, format, type, pixels);
}

static void fake_storage2d(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                           GLsizei height) {
    (void)target;
    (void)levels;
    (void)width;
    (void)height;
    cap.storages++;
    cap.internalformat = (GLint)internalformat;
}

static void fake_storage3d(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                           GLsizei height, GLsizei depth) {
    (void)depth;
    fake_storage2d(target, levels, internalformat, width, height);
}

static const struct format_case {
    GLenum internal, format, type;
    size_t bytes;
    const char *name;
} cases[] = {
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4u, "RGBA8/RGBA/UNSIGNED_BYTE"},
    {GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, 4u, "RGBA8_SNORM/RGBA/BYTE"},
    {GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 2u, "RGBA4/RGBA/UNSIGNED_SHORT_4_4_4_4"},
    {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 2u, "RGB5_A1/RGBA/UNSIGNED_SHORT_5_5_5_1"},
    {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 4u,
     "RGB10_A2/RGBA/UNSIGNED_INT_2_10_10_10_REV"},
    {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8u, "RGBA16F/RGBA/HALF_FLOAT"},
    {GL_RGBA32F, GL_RGBA, GL_FLOAT, 16u, "RGBA32F/RGBA/FLOAT"},
    {GL_RGBA16F, GL_RGBA, GL_FLOAT, 16u, "RGBA16F/RGBA/FLOAT"},
    {GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, 4u, "RGBA8UI/RGBA_INTEGER/UNSIGNED_BYTE"},
    {GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, 4u, "RGBA8I/RGBA_INTEGER/BYTE"},
    {GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, 8u, "RGBA16UI/RGBA_INTEGER/UNSIGNED_SHORT"},
    {GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT, 8u, "RGBA16I/RGBA_INTEGER/SHORT"},
    {GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, 16u, "RGBA32UI/RGBA_INTEGER/UNSIGNED_INT"},
    {GL_RGBA32I, GL_RGBA_INTEGER, GL_INT, 16u, "RGBA32I/RGBA_INTEGER/INT"},
    {GL_RGB10_A2UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, 4u,
     "RGB10_A2UI/RGBA_INTEGER/UNSIGNED_INT_2_10_10_10_REV"},
    {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, 3u, "RGB8/RGB/UNSIGNED_BYTE"},
    {GL_RGB8_SNORM, GL_RGB, GL_BYTE, 3u, "RGB8_SNORM/RGB/BYTE"},
    {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2u, "RGB565/RGB/UNSIGNED_SHORT_5_6_5"},
    {GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, 4u,
     "R11F_G11F_B10F/RGB/UNSIGNED_INT_10F_11F_11F_REV"},
    {GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, 4u, "RGB9_E5/RGB/UNSIGNED_INT_5_9_9_9_REV"},
    {GL_RGB16F, GL_RGB, GL_HALF_FLOAT, 6u, "RGB16F/RGB/HALF_FLOAT"},
    {GL_RGB32F_ARB, GL_RGB, GL_FLOAT, 12u, "RGB32F/RGB/FLOAT"},
    {GL_RGB16F, GL_RGB, GL_FLOAT, 12u, "RGB16F/RGB/FLOAT"},
    {GL_R11F_G11F_B10F, GL_RGB, GL_FLOAT, 12u, "R11F_G11F_B10F/RGB/FLOAT"},
    {GL_RGB9_E5, GL_RGB, GL_FLOAT, 12u, "RGB9_E5/RGB/FLOAT"},
    {GL_R11F_G11F_B10F, GL_RGB, GL_HALF_FLOAT, 6u, "R11F_G11F_B10F/RGB/HALF_FLOAT"},
    {GL_RGB9_E5, GL_RGB, GL_HALF_FLOAT, 6u, "RGB9_E5/RGB/HALF_FLOAT"},
    {GL_RGB8UI, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, 3u, "RGB8UI/RGB_INTEGER/UNSIGNED_BYTE"},
    {GL_RGB8I, GL_RGB_INTEGER, GL_BYTE, 3u, "RGB8I/RGB_INTEGER/BYTE"},
    {GL_RGB16UI, GL_RGB_INTEGER, GL_UNSIGNED_SHORT, 6u, "RGB16UI/RGB_INTEGER/UNSIGNED_SHORT"},
    {GL_RGB16I, GL_RGB_INTEGER, GL_SHORT, 6u, "RGB16I/RGB_INTEGER/SHORT"},
    {GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT, 12u, "RGB32UI/RGB_INTEGER/UNSIGNED_INT"},
    {GL_RGB32I, GL_RGB_INTEGER, GL_INT, 12u, "RGB32I/RGB_INTEGER/INT"},
    {GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2u, "RG8/RG/UNSIGNED_BYTE"},
    {GL_RG8_SNORM, GL_RG, GL_BYTE, 2u, "RG8_SNORM/RG/BYTE"},
    {GL_RG16F, GL_RG, GL_HALF_FLOAT, 4u, "RG16F/RG/HALF_FLOAT"},
    {GL_RG32F, GL_RG, GL_FLOAT, 8u, "RG32F/RG/FLOAT"},
    {GL_RG16F, GL_RG, GL_FLOAT, 8u, "RG16F/RG/FLOAT"},
    {GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE, 2u, "RG8UI/RG_INTEGER/UNSIGNED_BYTE"},
    {GL_RG8I, GL_RG_INTEGER, GL_BYTE, 2u, "RG8I/RG_INTEGER/BYTE"},
    {GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT, 4u, "RG16UI/RG_INTEGER/UNSIGNED_SHORT"},
    {GL_RG16I, GL_RG_INTEGER, GL_SHORT, 4u, "RG16I/RG_INTEGER/SHORT"},
    {GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, 8u, "RG32UI/RG_INTEGER/UNSIGNED_INT"},
    {GL_RG32I, GL_RG_INTEGER, GL_INT, 8u, "RG32I/RG_INTEGER/INT"},
    {GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1u, "R8/RED/UNSIGNED_BYTE"},
    {GL_R8_SNORM, GL_RED, GL_BYTE, 1u, "R8_SNORM/RED/BYTE"},
    {GL_R16F, GL_RED, GL_HALF_FLOAT, 2u, "R16F/RED/HALF_FLOAT"},
    {GL_R32F, GL_RED, GL_FLOAT, 4u, "R32F/RED/FLOAT"},
    {GL_R16F, GL_RED, GL_FLOAT, 4u, "R16F/RED/FLOAT"},
    {GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, 1u, "R8UI/RED_INTEGER/UNSIGNED_BYTE"},
    {GL_R8I, GL_RED_INTEGER, GL_BYTE, 1u, "R8I/RED_INTEGER/BYTE"},
    {GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, 2u, "R16UI/RED_INTEGER/UNSIGNED_SHORT"},
    {GL_R16I, GL_RED_INTEGER, GL_SHORT, 2u, "R16I/RED_INTEGER/SHORT"},
    {GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, 4u, "R32UI/RED_INTEGER/UNSIGNED_INT"},
    {GL_R32I, GL_RED_INTEGER, GL_INT, 4u, "R32I/RED_INTEGER/INT"},
    {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 2u,
     "DEPTH_COMPONENT16/DEPTH_COMPONENT/UNSIGNED_SHORT"},
    {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 4u,
     "DEPTH_COMPONENT16/DEPTH_COMPONENT/UNSIGNED_INT"},
    {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 4u,
     "DEPTH_COMPONENT24/DEPTH_COMPONENT/UNSIGNED_INT"},
    {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, 4u,
     "DEPTH_COMPONENT32F/DEPTH_COMPONENT/FLOAT"},
    {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 4u,
     "DEPTH24_STENCIL8/DEPTH_STENCIL/UNSIGNED_INT_24_8"},
    {GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 8u,
     "DEPTH32F_STENCIL8/DEPTH_STENCIL/FLOAT_32_UNSIGNED_INT_24_8_REV"},
    {GL_STENCIL_INDEX8, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, 1u,
     "STENCIL_INDEX8/STENCIL_INDEX/UNSIGNED_BYTE"},
};

static int native_format_known(GLint internalformat, GLenum format, GLenum type) {
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (internalformat == (GLint)cases[i].internal && format == cases[i].format &&
            type == cases[i].type)
            return 1;
    }
    return 0;
}

static void check_native_format(const char *operation, const struct format_case *c,
                                GLint internalformat, GLenum format, GLenum type, int *failures) {
    if (native_format_known(internalformat, format, type))
        return;
    fprintf(stderr, "invalid GLES backend %s %s -> 0x%x/0x%x/0x%x\n", operation, c->name,
            (unsigned)internalformat, format, type);
    ++*failures;
}

static int check_native_call(const char *operation, const struct format_case *c, GLenum error,
                             unsigned calls, int *failures) {
    if (error == GL_NO_ERROR && calls != 0u)
        return 1;
    fprintf(stderr, "%s %s err=0x%x calls=%u\n", operation, c->name, error, calls);
    ++*failures;
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint texture, texture3d;
    int failures = 0;
    unsigned checks = 0;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGenTextures = fake_gen;
    native.glesBindTexture = fake_bind;
    native.glesDeleteTextures = fake_delete;
    native.glesActiveTexture = fake_active;
    native.glesTexParameteri = fake_parameter;
    native.glesTexImage2D = fake_image;
    native.glesTexImage3D = fake_image3d;
    native.glesTexSubImage2D = fake_sub;
    native.glesTexSubImage3D = fake_sub3d;
    native.glesTexStorage2D = fake_storage2d;
    native.glesTexStorage3D = fake_storage3d;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 1;
    mgl_context_set_current_for_test(&ctx);
    glGenTextures(1, &texture);
    glGenTextures(1, &texture3d);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindTexture(GL_TEXTURE_3D, texture3d);
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const struct format_case *c = &cases[i];
        const unsigned char data[32] = {0};
        unsigned before = cap.images;
        GLint native_internal;
        GLenum err;
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)c->internal, 1, 1, 0, c->format, c->type, data);
        err = glGetError();
        checks++;
        if (!check_native_call("image2D", c, err, cap.images - before, &failures))
            continue;
        check_native_format("image2D", c, cap.internalformat, cap.format, cap.type, &failures);
        native_internal = cap.internalformat;
        {
            GLenum expected_native = 0;
            switch (c->internal) {
                case GL_R32F:
                    expected_native = GL_R16F;
                    break;
                case GL_RG32F:
                    expected_native = GL_RG16F;
                    break;
                case GL_RGBA32F:
                case GL_RGB32F_ARB:
                    expected_native = GL_RGBA16F;
                    break;
                default:
                    break;
            }
            if (expected_native && cap.internalformat != (GLint)expected_native) {
                fprintf(stderr, "float-linear fallback %s: expected 0x%x, got 0x%x\n", c->name,
                        expected_native, (unsigned)cap.internalformat);
                failures++;
            }
        }
        before = cap.subimages;
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, c->format, c->type, data);
        err = glGetError();
        checks++;
        (void)check_native_call("sub2D", c, err, cap.subimages - before, &failures);
        check_native_format("sub2D", c, native_internal, cap.format, cap.type, &failures);
        before = cap.subimages;
        glTextureSubImage2D(texture, 0, 0, 0, 1, 1, c->format, c->type, data);
        err = glGetError();
        checks++;
        (void)check_native_call("DSA sub2D", c, err, cap.subimages - before, &failures);
        check_native_format("DSA sub2D", c, native_internal, cap.format, cap.type, &failures);
        before = cap.images;
        glTexImage3D(GL_TEXTURE_3D, 0, (GLint)c->internal, 1, 1, 1, 0, c->format, c->type, data);
        err = glGetError();
        checks++;
        if (!check_native_call("image3D", c, err, cap.images - before, &failures))
            continue;
        check_native_format("image3D", c, cap.internalformat, cap.format, cap.type, &failures);
        native_internal = cap.internalformat;
        before = cap.subimages;
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, c->format, c->type, data);
        err = glGetError();
        checks++;
        (void)check_native_call("sub3D", c, err, cap.subimages - before, &failures);
        check_native_format("sub3D", c, native_internal, cap.format, cap.type, &failures);
    }
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const struct format_case *c = &cases[i];
        const unsigned char data[32] = {0};
        GLuint stored2d, stored3d;
        unsigned before;
        GLenum err;
        int duplicate = 0;
        for (size_t j = 0; j < i; ++j) {
            if (cases[j].internal == c->internal)
                duplicate = 1;
        }
        if (duplicate)
            continue;
        glGenTextures(1, &stored2d);
        glBindTexture(GL_TEXTURE_2D, stored2d);
        before = cap.storages;
        glTexStorage2D(GL_TEXTURE_2D, 1, c->internal, 1, 1);
        err = glGetError();
        checks++;
        if (check_native_call("storage2D", c, err, cap.storages - before, &failures)) {
            before = cap.subimages;
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, c->format, c->type, data);
            err = glGetError();
            checks++;
            (void)check_native_call("storage sub2D", c, err, cap.subimages - before, &failures);
        }
        glDeleteTextures(1, &stored2d);
        glGenTextures(1, &stored3d);
        glBindTexture(GL_TEXTURE_3D, stored3d);
        before = cap.storages;
        glTexStorage3D(GL_TEXTURE_3D, 1, c->internal, 1, 1, 1);
        err = glGetError();
        checks++;
        if (check_native_call("storage3D", c, err, cap.storages - before, &failures)) {
            before = cap.subimages;
            glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, c->format, c->type, data);
            err = glGetError();
            checks++;
            (void)check_native_call("storage sub3D", c, err, cap.subimages - before, &failures);
        }
        glDeleteTextures(1, &stored3d);
    }
    glDeleteTextures(1, &texture);
    glDeleteTextures(1, &texture3d);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    printf("GLES 3.2 required format/type matrix: %u checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

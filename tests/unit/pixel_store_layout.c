#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    GLuint next_buffer;
    unsigned tex2d_calls;
    unsigned tex3d_calls;
    unsigned pixel_store_calls;
    unsigned non_alignment_pixel_store_calls;
    unsigned read_calls;
    GLenum last_read_type;
    unsigned buffer_sub_data_calls;
    unsigned char upload[128];
    const void *upload_source;
    size_t upload_size;
    GLuint unpack_binding;
    GLuint upload_unpack_binding;
    uintptr_t upload_pbo_offset;
    unsigned pbo_upload_calls;
    unsigned char native_buffer[256];
    size_t native_buffer_size;
} cap;

static GLenum fake_get_error(void) {
    return GL_NO_ERROR;
}

static GLenum fake_get_reset_status(void) {
    return 0x8253u;
}

static void fake_active_texture(GLenum unit) {
    (void)unit;
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
    (void)target;
    (void)name;
}

static void fake_tex_parameteri(GLenum target, GLenum pname, GLint value) {
    (void)target;
    (void)pname;
    (void)value;
}

static void fake_pixel_store(GLenum pname, GLint value) {
    (void)value;
    ++cap.pixel_store_calls;
    if (pname != GL_PACK_ALIGNMENT && pname != GL_UNPACK_ALIGNMENT)
        ++cap.non_alignment_pixel_store_calls;
}

static void capture_upload(GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                           const void *pixels) {
    size_t bpp =
        format == GL_RGB ? 3u : (format == GL_RGBA ? 4u : (type == GL_UNSIGNED_SHORT ? 2u : 4u));
    size_t size = (size_t)width * (size_t)height * (size_t)depth * bpp;
    if (size > sizeof(cap.upload))
        size = sizeof(cap.upload);
    cap.upload_size = size;
    cap.upload_source = pixels;
    cap.upload_unpack_binding = cap.unpack_binding;
    if (cap.unpack_binding != 0u) {
        size_t offset = (size_t)(uintptr_t)pixels;
        cap.upload_pbo_offset = offset;
        ++cap.pbo_upload_calls;
        if (offset <= cap.native_buffer_size && size <= cap.native_buffer_size - offset)
            memcpy(cap.upload, cap.native_buffer + offset, size);
    } else if (size != 0u && pixels != NULL) {
        memcpy(cap.upload, pixels, size);
    }
}

static void fake_tex_image_2d(GLenum target, GLint level, GLint internalformat, GLsizei width,
                              GLsizei height, GLint border, GLenum format, GLenum type,
                              const void *pixels) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)border;
    (void)type;
    capture_upload(width, height, 1, format, type, pixels);
    ++cap.tex2d_calls;
}

static void fake_tex_image_3d(GLenum target, GLint level, GLint internalformat, GLsizei width,
                              GLsizei height, GLsizei depth, GLint border, GLenum format,
                              GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)border;
    (void)type;
    capture_upload(width, height, depth, format, type, pixels);
    ++cap.tex3d_calls;
}

static void fake_tex_sub_image_2d(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                  GLsizei width, GLsizei height, GLenum format, GLenum type,
                                  const void *pixels) {
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    capture_upload(width, height, 1, format, type, pixels);
}

static void fake_tex_sub_image_3d(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                  GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                  GLenum format, GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)zoffset;
    capture_upload(width, height, depth, format, type, pixels);
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

static void fake_bind_buffer(GLenum target, GLuint name) {
    if (target == GL_PIXEL_UNPACK_BUFFER)
        cap.unpack_binding = name;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)usage;
    if (target == GL_PIXEL_UNPACK_BUFFER && cap.unpack_binding != 0u && size >= 0 &&
        (size_t)size <= sizeof(cap.native_buffer)) {
        cap.native_buffer_size = (size_t)size;
        if (data != NULL)
            memcpy(cap.native_buffer, data, cap.native_buffer_size);
        else
            memset(cap.native_buffer, 0, cap.native_buffer_size);
    }
}

static void fake_buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size,
                                 const void *data) {
    ++cap.buffer_sub_data_calls;
    if (target == GL_PIXEL_UNPACK_BUFFER && cap.unpack_binding != 0u && offset >= 0 && size >= 0 &&
        (size_t)offset <= cap.native_buffer_size &&
        (size_t)size <= cap.native_buffer_size - (size_t)offset)
        memcpy(cap.native_buffer + (size_t)offset, data, (size_t)size);
}

static void fake_bind_framebuffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_read_pixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                             GLenum type, void *pixels) {
    unsigned char *dst = (unsigned char *)pixels;
    size_t bytes = (size_t)width * (size_t)height * 4u, i;
    (void)x;
    (void)y;
    (void)format;
    ++cap.read_calls;
    cap.last_read_type = type;
    if (type == GL_FLOAT) {
        const float values[4] = {1.5f, -0.25f, 2.0f, 0.5f};
        if (width == 1 && height == 1)
            memcpy(dst, values, sizeof(values));
    } else {
        for (i = 0u; i < bytes; ++i)
            dst[i] = (unsigned char)(10u + i);
    }
}

static int fail(int code, const char *why) {
    fprintf(stderr, "pixel-store layout failure %d: %s\n", code, why);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *tex2d_obj, *tex3d_obj;
    GLuint tex2d = 0u, tex3d = 0u, depthtex = 0u, pbo = 0u;
    GLint value = -1;
    unsigned char source2d[48];
    unsigned char source3d[108];
    unsigned char packed2d[48];
    unsigned char readback[40];
    unsigned char depth_source[2] = {0x12u, 0x34u};
    unsigned char depth_readback[2] = {0u, 0u};
    GLboolean boolean_value = GL_FALSE;
    size_t i;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGetError = fake_get_error;
    native.glesGetGraphicsResetStatus = fake_get_reset_status;
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexParameteri = fake_tex_parameteri;
    native.glesPixelStorei = fake_pixel_store;
    native.glesTexImage2D = fake_tex_image_2d;
    native.glesTexImage3D = fake_tex_image_3d;
    native.glesTexSubImage2D = fake_tex_sub_image_2d;
    native.glesTexSubImage3D = fake_tex_sub_image_3d;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesBufferSubData = fake_buffer_sub_data;
    native.glesBindFramebuffer = fake_bind_framebuffer;
    native.glesReadPixels = fake_read_pixels;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 64, 64);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &value);
    if (value != 0)
        return fail(2, "default unpack row length");
    glGetIntegerv(GL_PACK_SKIP_IMAGES, &value);
    if (value != 0)
        return fail(3, "default pack skip images");
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 4);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 1);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.non_alignment_pixel_store_calls != 0u)
        return fail(4, "interface layout state leaked into GLES pixel-store state");
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &value);
    if (value != 1)
        return fail(5, "unpack query");

    for (i = 0u; i < sizeof(source2d); ++i)
        source2d[i] = (unsigned char)i;
    glGenTextures(1, &tex2d);
    glBindTexture(GL_TEXTURE_2D, tex2d);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, source2d);
    if (cap.upload_source == source2d + 20)
        return fail(30, "strided 2D image must stage");
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.tex2d_calls != 1u ||
        cap.upload_size != 16u || memcmp(cap.upload, source2d + 20, 8u) != 0 ||
        memcmp(cap.upload + 8, source2d + 36, 8u) != 0)
        return fail(6, "2D unpack row/skip selection");
    tex2d_obj = mgl_bound_texture_2d(&ctx);
    if (tex2d_obj == NULL || memcmp(tex2d_obj->levels[0].data, cap.upload, 16u) != 0)
        return fail(7, "2D canonical shadow");

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 3);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 3);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 1);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 1);
    glPixelStorei(GL_UNPACK_SKIP_IMAGES, 1);
    for (i = 0u; i < sizeof(source3d); ++i)
        source3d[i] = (unsigned char)(100u + i);
    glGenTextures(1, &tex3d);
    glBindTexture(GL_TEXTURE_3D, tex3d);
    glTexImage3D(GL_TEXTURE_3D, 0, (GLint)GL_RGBA8, 1, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 source3d);
    if (cap.upload_source == source3d + 52)
        return fail(31, "strided 3D image must stage");
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.tex3d_calls != 1u ||
        cap.upload_size != 8u || memcmp(cap.upload, source3d + 52, 4u) != 0 ||
        memcmp(cap.upload + 4, source3d + 88, 4u) != 0)
        return fail(8, "3D unpack image-height/skip-images selection");
    tex3d_obj = mgl_bound_texture(&ctx, GL_TEXTURE_3D);
    if (tex3d_obj == NULL || tex3d_obj->levels[0].data_size != 8u)
        return fail(18, "3D canonical shadow");
    memset(source3d, 0xee, sizeof(source3d));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 3);
    glPixelStorei(GL_PACK_IMAGE_HEIGHT, 3);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 1);
    glPixelStorei(GL_PACK_SKIP_ROWS, 1);
    glPixelStorei(GL_PACK_SKIP_IMAGES, 1);
    glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_UNSIGNED_BYTE, source3d);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(source3d + 52, tex3d_obj->levels[0].data, 4u) != 0 ||
        memcmp(source3d + 88, tex3d_obj->levels[0].data + 4, 4u) != 0 || source3d[0] != 0xeeu)
        return fail(19, "3D pack image-height/skip-images selection");

    glBindTexture(GL_TEXTURE_2D, tex2d);
    memset(packed2d, 0xee, sizeof(packed2d));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 4);
    glPixelStorei(GL_PACK_SKIP_ROWS, 1);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 1);
    glPixelStorei(GL_PACK_IMAGE_HEIGHT, 9);
    glPixelStorei(GL_PACK_SKIP_IMAGES, 7);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, packed2d);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(packed2d + 20, tex2d_obj->levels[0].data, 8u) != 0 ||
        memcmp(packed2d + 36, tex2d_obj->levels[0].data + 8, 8u) != 0 || packed2d[0] != 0xeeu)
        return fail(9, "2D pack row/skip selection or image-mode leakage");

    memset(readback, 0xee, sizeof(readback));
    glPixelStorei(GL_PACK_ROW_LENGTH, 4);
    glPixelStorei(GL_PACK_SKIP_ROWS, 1);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 1);
    glReadPixels(0, 0, 2, 2, GL_RGB, GL_UNSIGNED_BYTE, readback);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.read_calls != 1u ||
        memcmp(readback + 15, (const unsigned char[]){10u, 11u, 12u, 14u, 15u, 16u}, 6u) != 0 ||
        memcmp(readback + 27, (const unsigned char[]){18u, 19u, 20u, 22u, 23u, 24u}, 6u) != 0 ||
        readback[0] != 0xeeu)
        return fail(10, "ReadPixels pack row/skip layout");

    memset(readback, 0xee, sizeof(readback));
    glReadnPixels(0, 0, 2, 2, GL_RGB, GL_UNSIGNED_BYTE, 32, readback);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || cap.read_calls != 1u ||
        readback[15] != 0xeeu)
        return fail(45, "bounded read pixels rejects short packed destination");
    glReadnPixels(0, 0, 2, 2, GL_RGB, GL_UNSIGNED_BYTE, 33, readback);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.read_calls != 2u || readback[15] != 10u)
        return fail(46, "bounded read pixels respects pack layout");
    if (glGetGraphicsResetStatus() != 0x8253u || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(48, "native GLES 3.2 reset status is reported");
    glReadnPixels(0, 0, 2, 2, GL_RGB, GL_UNSIGNED_BYTE, -1, readback);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(47, "bounded read pixels rejects negative size");

    {
        uint16_t half[4] = {0};
        struct mgl_framebuffer *fb;
        const float float_texel[4] = {1.5f, -0.25f, 2.0f, 0.5f};
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_HALF_FLOAT, half);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.last_read_type != GL_UNSIGNED_BYTE ||
            half[0] != 0x2905u || half[1] != 0x2986u || half[2] != 0x2a06u || half[3] != 0x2a87u)
            return fail(23, "half-float normalized framebuffer read converts component values");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, float_texel);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(24, "float texture for framebuffer read");
        fb = mgl_framebuffer_ensure(&ctx, 77u, 0);
        if (fb == NULL)
            return fail(25, "create tracked framebuffer");
        fb->read_buffer = GL_COLOR_ATTACHMENT0;
        fb->color_texture = tex2d;
        fb->color_texture_level = 0;
        fb->color_texture_target = GL_TEXTURE_2D;
        ctx.read_framebuffer = fb->name;
        memset(half, 0, sizeof(half));
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_HALF_FLOAT, half);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.last_read_type != GL_FLOAT ||
            half[0] != 0x3e00u || half[1] != 0xb400u || half[2] != 0x4000u || half[3] != 0x3800u)
            return fail(26, "float framebuffer read preserves out-of-range HDR pixels");
        {
            float floats[4] = {0};
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, floats);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.last_read_type != GL_FLOAT ||
                floats[0] != 1.5f || floats[1] != -0.25f || floats[2] != 2.0f || floats[3] != 0.5f)
                return fail(27, "floating readback preserves signed HDR range");
            glClampColor(GL_CLAMP_READ_COLOR, GL_TRUE);
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, floats);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || floats[0] != 1.0f ||
                floats[1] != 0.0f || floats[2] != 1.0f || floats[3] != 0.5f)
                return fail(28, "GL_CLAMP_READ_COLOR applies to floating readback");
            glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, floats);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || floats[0] != 1.5f ||
                floats[1] != -0.25f || floats[2] != 2.0f)
                return fail(29, "GL_CLAMP_READ_COLOR false preserves HDR");
            glClampColor(GL_CLAMP_READ_COLOR, GL_FIXED_ONLY);
        }
        ctx.read_framebuffer = 0u;
        glPixelStorei(GL_PACK_ROW_LENGTH, 4);
        glPixelStorei(GL_PACK_SKIP_ROWS, 1);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 1);
    }
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
    glPopClientAttrib();
    glGetIntegerv(GL_PACK_ROW_LENGTH, &value);
    if (value != 4)
        return fail(11, "client attrib pack restore");
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &value);
    if (value != 3)
        return fail(12, "client attrib unpack restore");
    glGetIntegerv(GL_UNPACK_SKIP_IMAGES, &value);
    if (value != 1)
        return fail(13, "client attrib image restore");

    glPixelStoref(GL_PACK_ROW_LENGTH, 4.6f);
    glGetIntegerv(GL_PACK_ROW_LENGTH, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 5)
        return fail(14, "PixelStoref nearest-integer row length");
    glPixelStorei(GL_PACK_SKIP_ROWS, -1);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(15, "negative layout value validation");

    glPixelStoref(GL_UNPACK_LSB_FIRST, 0.25f);
    glGetBooleanv(GL_UNPACK_LSB_FIRST, &boolean_value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || !boolean_value)
        return fail(20, "PixelStoref boolean conversion");
    glPixelStorei(GL_UNPACK_LSB_FIRST, 0);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
    glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
    glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
    glGenTextures(1, &depthtex);
    glBindTexture(GL_TEXTURE_2D, depthtex);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_DEPTH_COMPONENT16, 1, 1, 0, GL_DEPTH_COMPONENT,
                 GL_UNSIGNED_SHORT, depth_source);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_size != 2u ||
        cap.upload[0] != 0x34u || cap.upload[1] != 0x12u)
        return fail(21, "unpack byte swapping");
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_IMAGE_HEIGHT, 0);
    glPixelStorei(GL_PACK_SKIP_IMAGES, 0);
    glPixelStorei(GL_PACK_SWAP_BYTES, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, depth_readback);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || depth_readback[0] != 0x12u ||
        depth_readback[1] != 0x34u)
        return fail(22, "pack byte swapping");
    glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
    glPixelStorei(GL_PACK_SWAP_BYTES, 0);

    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, 32, NULL, GL_STATIC_DRAW);
    glPixelStorei(GL_PACK_ROW_LENGTH, 4);
    glPixelStorei(GL_PACK_SKIP_ROWS, 1);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 1);
    glReadPixels(0, 0, 2, 2, GL_RGB, GL_UNSIGNED_BYTE, (void *)(uintptr_t)0u);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(16, "PBO span bounds include row/skip layout");
    glBufferData(GL_PIXEL_PACK_BUFFER, 33, NULL, GL_STATIC_DRAW);
    glReadPixels(0, 0, 2, 2, GL_RGB, GL_UNSIGNED_BYTE, (void *)(uintptr_t)0u);
    {
        struct mgl_buffer *buffer = mgl_buffer_find(&ctx, pbo);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || buffer == NULL ||
            buffer->shadow == NULL ||
            memcmp(buffer->shadow + 15, (const unsigned char[]){10u, 11u, 12u, 14u, 15u, 16u},
                   6u) != 0 ||
            cap.buffer_sub_data_calls == 0u)
            return fail(17, "PBO packed destination and native mirror");
    }

    {
        const unsigned char direct2d[32] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                                            12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                                            23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
        const unsigned char direct3d[32] = {32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22,
                                            21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11,
                                            10, 9,  8,  7,  6,  5,  4,  3,  2,  1};
        struct mgl_buffer *buffer;
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0u);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
        glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
        glBindTexture(GL_TEXTURE_2D, tex2d);
        glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, direct2d);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(tex2d_obj->levels[1].data, direct2d, 16u) != 0)
            return fail(32, "contiguous 2D image borrows source and preserves shadow");
        glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, direct2d + 16);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_source != direct2d + 16 ||
            memcmp(tex2d_obj->levels[1].data, direct2d + 16, 16u) != 0)
            return fail(33, "contiguous 2D subimage borrows source");
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 2);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, direct2d);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_source != direct2d + 8 ||
            memcmp(tex2d_obj->levels[1].data, direct2d + 8, 16u) != 0)
            return fail(34, "tightly packed rows with start skip borrow source");
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 3);
        glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, direct2d);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_source == direct2d + 12 ||
            memcmp(tex2d_obj->levels[1].data, direct2d + 12, 8u) != 0 ||
            memcmp(tex2d_obj->levels[1].data + 8, direct2d + 24, 8u) != 0)
            return fail(35, "strided 2D subimage stages and repacks");
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glBindTexture(GL_TEXTURE_3D, tex3d);
        glTexImage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, direct3d);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(tex3d_obj->levels[1].data, direct3d, 32u) != 0)
            return fail(36, "contiguous 3D image borrows source");
        glTexSubImage3D(GL_TEXTURE_3D, 1, 0, 0, 0, 2, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, direct3d);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_source != direct3d)
            return fail(37, "contiguous 3D subimage borrows source");
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, (GLsizeiptr)sizeof(direct2d), direct2d,
                     GL_STATIC_DRAW);
        buffer = mgl_buffer_find(&ctx, pbo);
        glBindTexture(GL_TEXTURE_2D, tex2d);
        glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                        (const void *)(uintptr_t)4u);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || buffer == NULL ||
            cap.upload_source != (const void *)(uintptr_t)4u ||
            cap.upload_unpack_binding != buffer->native_name ||
            cap.unpack_binding != buffer->native_name || cap.upload_pbo_offset != 4u ||
            cap.pbo_upload_calls == 0u || memcmp(cap.upload, direct2d + 4u, 16u) != 0 ||
            memcmp(tex2d_obj->levels[1].data, direct2d + 4u, 16u) != 0)
            return fail(38, "contiguous PBO upload forwards native buffer and byte offset");
        glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     (const void *)(uintptr_t)8u);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.upload_source != (const void *)(uintptr_t)8u ||
            cap.upload_unpack_binding != buffer->native_name ||
            memcmp(cap.upload, direct2d + 8u, 16u) != 0 ||
            memcmp(tex2d_obj->levels[2].data, direct2d + 8u, 16u) != 0)
            return fail(39, "2D PBO TexImage forwards offset and preserves CPU shadow");
        glBindTexture(GL_TEXTURE_3D, tex3d);
        glTexImage3D(GL_TEXTURE_3D, 2, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     (const void *)(uintptr_t)0u);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.upload_source != (const void *)(uintptr_t)0u ||
            cap.upload_unpack_binding != buffer->native_name ||
            memcmp(tex3d_obj->levels[2].data, direct2d, 32u) != 0)
            return fail(40, "3D PBO TexImage forwards zero byte offset");
        glTexSubImage3D(GL_TEXTURE_3D, 2, 0, 0, 0, 2, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                        (const void *)(uintptr_t)0u);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.upload_unpack_binding != buffer->native_name ||
            memcmp(cap.upload, direct2d, 32u) != 0)
            return fail(41, "3D PBO subimage forwards native offset");
        glBindTexture(GL_TEXTURE_2D, tex2d);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 3);
        glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                        (const void *)(uintptr_t)0u);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_unpack_binding != 0u ||
            cap.unpack_binding != buffer->native_name || memcmp(cap.upload, direct2d, 8u) != 0 ||
            memcmp(cap.upload + 8, direct2d + 12u, 8u) != 0)
            return fail(42, "strided PBO upload falls back to CPU unpack");
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                        (const void *)(uintptr_t)4u);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.upload_unpack_binding != buffer->native_name || cap.upload_pbo_offset != 8u ||
            memcmp(cap.upload, direct2d + 8u, 16u) != 0)
            return fail(45, "PBO forwarding includes unpack pixel skips in byte offset");
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glTexImage2D(GL_TEXTURE_2D, 3, GL_RGBA16F, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     (const void *)(uintptr_t)0u);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_unpack_binding != 0u ||
            cap.unpack_binding != buffer->native_name)
            return fail(43, "PBO upload requiring half float conversion uses CPU source");
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0u);
        glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, direct2d);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_unpack_binding != 0u ||
            cap.upload_source != direct2d || cap.unpack_binding != 0u)
            return fail(44, "normal client-pointer upload restores unpack state");
    }

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL pixel-store layout unit test: PASS");
    return 0;
}

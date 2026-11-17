#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    GLuint next_buffer;
    GLuint next_framebuffer;
    GLenum last_bind_target;
    GLuint last_bound_texture;
    unsigned tex_image_3d_calls;
    unsigned tex_sub_image_3d_calls;
    GLenum sub_format, sub_type;
    unsigned char sub_upload[16];
    size_t sub_size;
    unsigned generate_mipmap_calls;
    GLsizei upload_width, upload_height, upload_depth;
    unsigned char *upload;
    size_t upload_size;
    GLenum upload_format, upload_type;
    GLint upload_internal;
    GLuint layer_texture;
    GLint layer_level;
    GLint layer_index;
    unsigned layer_calls;
} cap;

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
    cap.last_bind_target = target;
    cap.last_bound_texture = name;
}

static void fake_pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
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
    size_t components = format == GL_RED || format == GL_DEPTH_COMPONENT ? 1u
                        : format == GL_RG                                ? 2u
                        : format == GL_RGB                               ? 3u
                                                                         : 4u;
    size_t bpp = components * ((type == GL_HALF_FLOAT || type == GL_UNSIGNED_SHORT) ? 2u
                               : type == GL_FLOAT || type == GL_UNSIGNED_INT        ? 4u
                                                                                    : 1u);
    size_t size = (size_t)width * (size_t)height * (size_t)depth * bpp;
    (void)target;
    (void)level;
    (void)internalformat;
    (void)border;
    (void)type;
    free(cap.upload);
    cap.upload = NULL;
    cap.upload_size = 0u;
    if (size != 0u && pixels != NULL) {
        cap.upload = malloc(size);
        if (cap.upload != NULL) {
            memcpy(cap.upload, pixels, size);
            cap.upload_size = size;
        }
    }
    cap.upload_width = width;
    cap.upload_height = height;
    cap.upload_depth = depth;
    cap.upload_format = format;
    cap.upload_type = type;
    cap.upload_internal = internalformat;
    ++cap.tex_image_3d_calls;
}

static void fake_tex_sub_image_3d(GLenum target, GLint level, GLint x, GLint y, GLint z,
                                  GLsizei width, GLsizei height, GLsizei depth, GLenum format,
                                  GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)x;
    (void)y;
    (void)z;
    cap.sub_format = format;
    cap.sub_type = type;
    cap.sub_size = (size_t)width * (size_t)height * (size_t)depth *
                   (format == GL_RED || format == GL_DEPTH_COMPONENT ? 1u
                    : format == GL_RG                                ? 2u
                    : format == GL_RGB                               ? 3u
                                                                     : 4u) *
                   (type == GL_UNSIGNED_SHORT || type == GL_HALF_FLOAT ? 2u
                    : type == GL_FLOAT || type == GL_UNSIGNED_INT      ? 4u
                                                                       : 1u);
    if (cap.sub_size <= sizeof(cap.sub_upload) && pixels != NULL)
        memcpy(cap.sub_upload, pixels, cap.sub_size);
    ++cap.tex_sub_image_3d_calls;
}

static void fake_generate_mipmap(GLenum target) {
    (void)target;
    ++cap.generate_mipmap_calls;
}

static void fake_tex_parameteri(GLenum target, GLenum pname, GLint value) {
    (void)target;
    (void)pname;
    (void)value;
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
    (void)target;
    (void)name;
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

static void fake_gen_framebuffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_framebuffer;
}

static void fake_delete_framebuffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_framebuffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static GLenum fake_check_framebuffer(GLenum target) {
    (void)target;
    return GL_FRAMEBUFFER_COMPLETE;
}

static void fake_framebuffer_layer(GLenum target, GLenum attachment, GLuint texture, GLint level,
                                   GLint layer) {
    (void)target;
    (void)attachment;
    cap.layer_texture = texture;
    cap.layer_level = level;
    cap.layer_index = layer;
    ++cap.layer_calls;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "3D texture failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj;
    struct mgl_texture *list_obj;
    GLuint tex2d = 0u, tex3d = 0u, tex_other = 0u, list_tex = 0u, pbo = 0u, fbo = 0u, list = 0u;
    GLint value = 0;
    const unsigned char padded[15] = {1u, 2u, 3u, 99u, 4u,  5u,  6u, 98u,
                                      7u, 8u, 9u, 97u, 10u, 11u, 12u};
    const unsigned char tight[12] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u};
    const unsigned char patch[3] = {20u, 21u, 22u};
    unsigned char list_patch[3] = {50u, 51u, 52u};
    unsigned char readback[15];
    unsigned char pbo_data[16];
    unsigned char zero_payload[15];

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesPixelStorei = fake_pixel_store;
    native.glesTexImage2D = fake_tex_image_2d;
    native.glesTexImage3D = fake_tex_image_3d;
    native.glesTexSubImage3D = fake_tex_sub_image_3d;
    native.glesGenerateMipmap = fake_generate_mipmap;
    native.glesTexParameteri = fake_tex_parameteri;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesBufferSubData = fake_buffer_sub_data;
    native.glesGenFramebuffers = fake_gen_framebuffers;
    native.glesDeleteFramebuffers = fake_delete_framebuffers;
    native.glesBindFramebuffer = fake_bind_framebuffer;
    native.glesCheckFramebufferStatus = fake_check_framebuffer;
    native.glesFramebufferTextureLayer = fake_framebuffer_layer;

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
    ctx.next_framebuffer_name = 1u;
    mgl_context_set_current_for_test(&ctx);

    glGenTextures(1, &tex2d);
    glGenTextures(1, &tex3d);
    glBindTexture(GL_TEXTURE_2D, tex2d);
    glBindTexture(GL_TEXTURE_3D, tex3d);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
    if ((GLuint)value != tex2d)
        return fail(2, "independent 2D binding");
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &value);
    if ((GLuint)value != tex3d)
        return fail(3, "3D binding query");

    glTexImage3D(GL_TEXTURE_3D, 0, (GLint)GL_RGB8, 1, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, padded);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.tex_image_3d_calls != 1u ||
        cap.upload_size != sizeof(tight) || memcmp(cap.upload, tight, sizeof(tight)) != 0)
        return fail(4, "aligned volume upload");
    obj = mgl_bound_texture(&ctx, GL_TEXTURE_3D);
    if (obj == NULL || obj->levels[0].depth != 2 || obj->levels[0].data_size != sizeof(tight) ||
        memcmp(obj->levels[0].data, tight, sizeof(tight)) != 0)
        return fail(5, "canonical volume shadow");
    glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_DEPTH, &value);
    if (value != 2)
        return fail(6, "depth query");
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, (GLint)GL_CLAMP_TO_EDGE);
    glGetTexParameteriv(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, &value);
    if (value != (GLint)GL_CLAMP_TO_EDGE)
        return fail(7, "wrap R ownership");

    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 1, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, patch);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.tex_sub_image_3d_calls != 1u ||
        memcmp(obj->levels[0].data + 6u, patch, 3u) != 0)
        return fail(8, "3D subimage");

    memset(readback, 0xee, sizeof(readback));
    glGetTexImage(GL_TEXTURE_3D, 0, GL_RGB, GL_UNSIGNED_BYTE, readback);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(readback, (const unsigned char[]){1u, 2u, 3u}, 3u) != 0 || readback[3] != 0xeeu ||
        memcmp(readback + 4, (const unsigned char[]){4u, 5u, 6u}, 3u) != 0 ||
        readback[7] != 0xeeu || memcmp(readback + 8, patch, 3u) != 0 || readback[11] != 0xeeu ||
        memcmp(readback + 12, (const unsigned char[]){10u, 11u, 12u}, 3u) != 0)
        return fail(9, "3D pack-aligned readback");

    memcpy(pbo_data + 1, padded, sizeof(padded));
    pbo_data[0] = 42u;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, (GLsizeiptr)sizeof(pbo_data), pbo_data, GL_STATIC_DRAW);
    glTexImage3D(GL_TEXTURE_3D, 1, (GLint)GL_RGB8, 1, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 (const void *)(uintptr_t)1u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj->levels[1].data_size != sizeof(tight) ||
        memcmp(obj->levels[1].data, tight, sizeof(tight)) != 0)
        return fail(10, "3D PBO logical offset");
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0u);

    glGenerateMipmap(GL_TEXTURE_3D);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.generate_mipmap_calls != 1u ||
        !obj->levels[1].defined || obj->levels[1].width != 1 || obj->levels[1].height != 1 ||
        obj->levels[1].depth != 1)
        return fail(11, "3D mipmap generation");

    glGenTextures(1, &tex_other);
    glBindTexture(GL_TEXTURE_3D, tex_other);
    glTextureSubImage3D(tex3d, 0, 0, 1, 0, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE,
                        (const unsigned char[]){30u, 31u, 32u});
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || (GLuint)value != tex_other ||
        cap.tex_sub_image_3d_calls != 2u ||
        memcmp(obj->levels[0].data + 3u, (const unsigned char[]){30u, 31u, 32u}, 3u) != 0)
        return fail(12, "DSA 3D subimage preserves binding");
    glGenerateTextureMipmap(tex3d);
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || (GLuint)value != tex_other ||
        cap.generate_mipmap_calls != 2u)
        return fail(13, "DSA mipmap preserves binding");
    glGetTextureLevelParameteriv(tex3d, 0, GL_TEXTURE_DEPTH, &value);
    if (value != 2)
        return fail(14, "named 3D level depth query");

    glBindTexture(GL_TEXTURE_3D, tex3d);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex3d, 0, 1);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.layer_texture != obj->native_name ||
        cap.layer_level != 0 || cap.layer_index != 1)
        return fail(15, "layered native attachment");
    glGetNamedFramebufferAttachmentParameteriv(fbo, GL_COLOR_ATTACHMENT0,
                                               GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 1)
        return fail(16, "canonical layer query");

    {
        unsigned calls = cap.layer_calls;
        glFramebufferTexture3D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, tex3d, 0, 0);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.layer_calls != calls + 1u ||
            cap.layer_texture != obj->native_name || cap.layer_level != 0 || cap.layer_index != 0)
            return fail(17, "FramebufferTexture3D native lowering");
        glGetNamedFramebufferAttachmentParameteriv(fbo, GL_COLOR_ATTACHMENT0,
                                                   GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 0)
            return fail(18, "FramebufferTexture3D canonical zoffset");

        glFramebufferTexture3D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex3d, 0, 0);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || cap.layer_calls != calls + 1u)
            return fail(19, "FramebufferTexture3D target mismatch");
        glFramebufferTexture3D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, 0xdeadbeefu, 0,
                               0);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE || cap.layer_calls != calls + 1u)
            return fail(20, "FramebufferTexture3D unknown texture");
        glFramebufferTexture3D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, tex3d, 0, -1);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE || cap.layer_calls != calls + 1u)
            return fail(21, "FramebufferTexture3D negative zoffset");

        glFramebufferTexture3D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0xdeadbeefu, 0u, -5, -7);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.layer_calls != calls + 2u ||
            cap.layer_texture != 0u || cap.layer_level != 0 || cap.layer_index != 0)
            return fail(22, "FramebufferTexture3D detach ignores image parameters");
        glGetNamedFramebufferAttachmentParameteriv(fbo, GL_COLOR_ATTACHMENT0,
                                                   GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != (GLint)GL_NONE)
            return fail(23, "FramebufferTexture3D detach canonical state");
        if (glXGetProcAddress((const GLubyte *)"glFramebufferTexture3D") == NULL)
            return fail(24, "FramebufferTexture3D proc address");
    }

    memcpy(pbo_data + 1, padded, sizeof(padded));
    pbo_data[0] = 77u;
    memset(zero_payload, 0, sizeof(zero_payload));
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, (GLsizeiptr)sizeof(pbo_data), pbo_data, GL_STATIC_DRAW);
    glGenTextures(1, &list_tex);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBindTexture(GL_TEXTURE_3D, list_tex);
    glTexImage3D(GL_TEXTURE_3D, 0, (GLint)GL_RGB8, 1, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 (const void *)(uintptr_t)1u);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0u);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 1, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, list_patch);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(25, "3D list compilation");

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 1, (GLsizeiptr)sizeof(zero_payload), zero_payload);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0u);
    memset(list_patch, 0, sizeof(list_patch));
    glCallList(list);
    list_obj = mgl_texture_find(&ctx, list_tex);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || list_obj == NULL ||
        !list_obj->levels[0].defined || list_obj->levels[0].data_size != sizeof(tight) ||
        memcmp(list_obj->levels[0].data, tight, 6u) != 0 ||
        memcmp(list_obj->levels[0].data + 6u, (const unsigned char[]){50u, 51u, 52u}, 3u) != 0 ||
        memcmp(list_obj->levels[0].data + 9u, tight + 9u, 3u) != 0)
        return fail(26, "3D list owns PBO image and client subimage payloads");

    {
        const unsigned char bgr[6] = {1u, 3u, 5u, 7u, 9u, 11u};
        const unsigned char rgb[6] = {5u, 3u, 1u, 11u, 9u, 7u};
        const unsigned char bgra[4] = {10u, 40u, 80u, 120u};
        const unsigned char rgba[4] = {80u, 40u, 10u, 120u};
        glBindTexture(GL_TEXTURE_3D, tex3d);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB8, 1, 1, 2, 0, GL_BGR, GL_UNSIGNED_BYTE, bgr);
        obj = mgl_bound_texture(&ctx, GL_TEXTURE_3D);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL ||
            obj->levels[0].format != GL_RGB || cap.upload_size != sizeof(rgb) ||
            memcmp(cap.upload, rgb, sizeof(rgb)) != 0 ||
            memcmp(obj->levels[0].data, rgb, sizeof(rgb)) != 0)
            return fail(27, "3D BGR volume normalized before upload");
        if (!mgl_texture_apply_image_3d_tight(&ctx, GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 1, 0, GL_BGRA,
                                              GL_UNSIGNED_BYTE, bgra, sizeof(bgra)) ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj->levels[0].format != GL_RGBA ||
            cap.upload_size != sizeof(rgba) || memcmp(cap.upload, rgba, sizeof(rgba)) != 0 ||
            memcmp(obj->levels[0].data, rgba, sizeof(rgba)) != 0)
            return fail(28, "direct 3D BGRA texture canonicalized");
        if (!mgl_texture_apply_sub_image_3d_tight(&ctx, GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, GL_BGRA,
                                                  GL_UNSIGNED_BYTE, bgra, sizeof(bgra)) ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(obj->levels[0].data, rgba, sizeof(rgba)) != 0)
            return fail(29, "direct 3D BGRA texture subimage canonicalized");
        {
            const uint16_t bgr16[6] = {0u, 32768u, 65535u, 257u, 1028u, 3855u};
            const uint16_t rgb16[6] = {65535u, 32768u, 0u, 3855u, 1028u, 257u};
            const uint16_t patch16[3] = {0u, 65535u, 32768u};
            const unsigned char gpu8[6] = {255u, 128u, 0u, 15u, 4u, 1u};
            const unsigned char sub8[3] = {128u, 255u, 0u};
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB8, 1, 1, 2, 0, GL_BGR, GL_UNSIGNED_SHORT, bgr16);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGB ||
                cap.upload_type != GL_UNSIGNED_BYTE || cap.upload_size != sizeof(gpu8) ||
                memcmp(cap.upload, gpu8, sizeof(gpu8)) != 0 ||
                obj->levels[0].type != GL_UNSIGNED_SHORT ||
                memcmp(obj->levels[0].data, rgb16, sizeof(rgb16)) != 0)
                return fail(32, "3D unsigned short BGR upload");
            glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 1, 1, 1, 1, GL_BGR, GL_UNSIGNED_SHORT, patch16);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.sub_format != GL_RGB ||
                cap.sub_type != GL_UNSIGNED_BYTE || cap.sub_size != sizeof(sub8) ||
                memcmp(cap.sub_upload, sub8, sizeof(sub8)) != 0 ||
                memcmp(obj->levels[0].data + 6u, (uint16_t[]){32768u, 65535u, 0u}, 6u) != 0)
                return fail(33, "3D unsigned short BGR subimage");
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    /* GLES 3D texture uploads apply the same RGBA8-to-RGBA16F type conversion as 2D uploads. */
    {
        const unsigned char rgba8[4] = {255u, 128u, 64u, 0u};
        const uint16_t expected_half[4] = {0x3c00u, 0x3804u, 0x3404u, 0x0000u};
        const uint16_t rgba16[4] = {65535u, 32768u, 0u, 65535u};
        const unsigned char expected_rgba8[4] = {255u, 128u, 0u, 255u};
        glBindTexture(GL_TEXTURE_3D, tex3d);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba8);
        obj = mgl_bound_texture(&ctx, GL_TEXTURE_3D);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != GL_RGBA16F ||
            cap.upload_format != GL_RGBA || cap.upload_type != GL_HALF_FLOAT ||
            cap.upload_size != sizeof(expected_half) ||
            memcmp(cap.upload, expected_half, sizeof(expected_half)) != 0 ||
            obj->levels[0].type != GL_UNSIGNED_BYTE ||
            memcmp(obj->levels[0].data, rgba8, sizeof(rgba8)) != 0)
            return fail(30, "3D RGBA16F byte source converted to half float for GLES");
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_SHORT, rgba16);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != GL_RGBA8 ||
            cap.upload_format != GL_RGBA || cap.upload_type != GL_UNSIGNED_BYTE ||
            cap.upload_size != sizeof(expected_rgba8) ||
            memcmp(cap.upload, expected_rgba8, sizeof(expected_rgba8)) != 0 ||
            obj->levels[0].type != GL_UNSIGNED_SHORT ||
            memcmp(obj->levels[0].data, rgba16, sizeof(rgba16)) != 0)
            return fail(31, "3D RGBA16 norm16 capability fallback");
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    {
        const unsigned char rgba[4] = {100u, 120u, 140u, 160u};
        const unsigned char rgb[3] = {100u, 120u, 140u};
        const unsigned char source_rgb[3] = {40u, 50u, 60u};
        const unsigned char expected_rgba[4] = {40u, 50u, 60u, 255u};
        glBindTexture(GL_TEXTURE_3D, tex3d);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB8, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        obj = mgl_bound_texture(&ctx, GL_TEXTURE_3D);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.sub_format != GL_RGB ||
            cap.sub_type != GL_UNSIGNED_BYTE || cap.sub_size != sizeof(rgb) ||
            memcmp(cap.sub_upload, rgb, sizeof(rgb)) != 0 ||
            memcmp(obj->levels[0].data, rgb, sizeof(rgb)) != 0)
            return fail(70, "3D RGBA update into RGB storage");
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, source_rgb);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.sub_format != GL_RGBA ||
            cap.sub_size != sizeof(expected_rgba) ||
            memcmp(cap.sub_upload, expected_rgba, sizeof(expected_rgba)) != 0 ||
            memcmp(obj->levels[0].data, expected_rgba, sizeof(expected_rgba)) != 0)
            return fail(71, "3D RGB update into RGBA storage");
    }

    {
        const unsigned char input[4] = {60u, 80u, 100u, 120u};
        const unsigned char scaled[4] = {120u, 80u, 100u, 120u};
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glBindTexture(GL_TEXTURE_3D, tex3d);
        glPixelTransferf(GL_RED_SCALE, 2.0f);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, input);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_size != sizeof(scaled) ||
            memcmp(cap.upload, scaled, sizeof(scaled)))
            return fail(240, "3D image color transfer");
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, input);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.sub_size != sizeof(scaled) ||
            memcmp(cap.sub_upload, scaled, sizeof(scaled)))
            return fail(241, "3D subimage color transfer");
        glPixelTransferf(GL_RED_SCALE, 1.0f);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    {
        const float rgb[3] = {0.25f, 0.5f, 0.75f};
        float rgba[4];
        glBindTexture(GL_TEXTURE_3D, tex3d);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F_ARB, 1, 1, 1, 0, GL_RGB, GL_FLOAT, rgb);
        obj = mgl_bound_texture(&ctx, GL_TEXTURE_3D);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != GL_RGBA16F ||
            cap.upload_format != GL_RGBA || cap.upload_type != GL_FLOAT ||
            cap.upload_size != sizeof(rgba) || obj->levels[0].data_size != sizeof(rgb))
            return fail(270, "3D RGB float backing");
        memcpy(rgba, cap.upload, sizeof(rgba));
        if (rgba[0] != rgb[0] || rgba[1] != rgb[1] || rgba[2] != rgb[2] || rgba[3] != 1.0f)
            return fail(271, "3D RGB float expansion");
        {
            const unsigned char rgb8[3] = {0u, 128u, 255u};
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB16F, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb8);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != GL_RGBA16F ||
                cap.upload_format != GL_RGBA || cap.upload_type != GL_HALF_FLOAT ||
                cap.upload_size != 8u)
                return fail(273, "3D RGB16F byte source expansion");
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F_ARB, 1, 1, 1, 0, GL_RGB, GL_FLOAT, rgb);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
                return fail(274, "3D RGB float restore");
        }
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, GL_RGB, GL_FLOAT, rgb);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.sub_format != GL_RGBA ||
            cap.sub_type != GL_FLOAT || cap.sub_size != sizeof(rgba))
            return fail(272, "3D RGB float subimage");
    }

    free(cap.upload);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL 3D texture unit test: PASS");
    return 0;
}

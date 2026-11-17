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
    GLuint next_framebuffer;
    unsigned generated_framebuffer_calls;
    GLenum active_texture;
    GLuint bound_texture;
    GLuint bound_texture_2d[MGL_TEXTURE_UNITS];
    unsigned native_texture_binds;
    unsigned native_cube_binds;
    GLint unpack_alignment;
    unsigned char *upload;
    size_t upload_size;
    GLenum upload_format;
    GLenum image_target;
    GLuint texture_at_image;
    GLenum framebuffer_textarget;
    GLenum upload_type;
    GLint upload_internal;
    GLsizei upload_width;
    GLsizei upload_height;
    GLuint attached_texture;
    GLuint attached_colors[MGL_MAX_DRAW_BUFFERS];
    unsigned attachment_calls[MGL_MAX_DRAW_BUFFERS];
    GLuint read_fbo;
    GLuint draw_fbo;
    GLint pack_alignment;
    unsigned gpu_read_calls;
    GLint swizzle[4];
    GLenum readback_status;
    GLenum pending_native_error;
    int native_norm16;
    int float_linear;
    int require_float_readback;
    GLenum native_read_type;
} cap;

static const GLubyte *fake_get_string(GLenum name) {
    if (name != GL_EXTENSIONS)
        return (const GLubyte *)"";
    if (cap.float_linear)
        return (const GLubyte *)"GL_OES_texture_float_linear GL_EXT_texture_norm16";
    if (cap.native_norm16)
        return (const GLubyte *)"GL_EXT_texture_norm16";
    return (const GLubyte *)"";
}

static void fake_active_texture(GLenum texture) {
    cap.active_texture = texture;
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
    cap.bound_texture = name;
    if (target == GL_TEXTURE_2D && cap.active_texture >= GL_TEXTURE0 &&
        cap.active_texture < GL_TEXTURE0 + MGL_TEXTURE_UNITS)
        cap.bound_texture_2d[cap.active_texture - GL_TEXTURE0] = name;
    ++cap.native_texture_binds;
    if (target == GL_TEXTURE_CUBE_MAP)
        ++cap.native_cube_binds;
}

static void fake_pixel_store(GLenum pname, GLint param) {
    if (pname == GL_UNPACK_ALIGNMENT)
        cap.unpack_alignment = param;
    if (pname == GL_PACK_ALIGNMENT)
        cap.pack_alignment = param;
}

static void fake_tex_image(GLenum target, GLint level, GLint internalformat, GLsizei width,
                           GLsizei height, GLint border, GLenum format, GLenum type,
                           const void *pixels) {
    size_t components = (format == GL_DEPTH_COMPONENT || format == GL_RED)
                            ? 1u
                            : (format == GL_RGB ? 3u : (format == GL_RG ? 2u : 4u));
    size_t size = (size_t)width * (size_t)height *
                  (type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV ? 8u
                   : type == GL_UNSIGNED_SHORT_5_6_5
                       ? 2u
                       : components * ((type == GL_UNSIGNED_SHORT || type == GL_HALF_FLOAT)
                                           ? 2u
                                           : (type == GL_FLOAT ? 4u : 1u)));
    cap.image_target = target;
    cap.texture_at_image = cap.bound_texture;
    (void)level;
    (void)border;
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
    cap.upload_format = format;
    cap.upload_type = type;
    cap.upload_internal = internalformat;
    cap.upload_width = width;
    cap.upload_height = height;
}

static void fake_tex_sub_image(GLenum target, GLint level, GLint x, GLint y, GLsizei width,
                               GLsizei height, GLenum format, GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)x;
    (void)y;
    (void)type;
    fake_tex_image(GL_TEXTURE_2D, level, (GLint)format, width, height, 0, format, type, pixels);
}

static void fake_tex_parameteri(GLenum target, GLenum pname, GLint value) {
    (void)target;
    if (pname >= GL_TEXTURE_SWIZZLE_R && pname <= GL_TEXTURE_SWIZZLE_A)
        cap.swizzle[pname - GL_TEXTURE_SWIZZLE_R] = value;
}

static void fake_tex_parameterf(GLenum target, GLenum pname, GLfloat value) {
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
    cap.generated_framebuffer_calls += (unsigned)n;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_framebuffer;
}

static void fake_delete_framebuffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_framebuffer(GLenum target, GLuint name) {
    if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER)
        cap.read_fbo = name;
    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER)
        cap.draw_fbo = name;
}

static GLenum fake_check_framebuffer(GLenum target) {
    (void)target;
    return cap.readback_status != 0u ? cap.readback_status : GL_FRAMEBUFFER_COMPLETE;
}

static void fake_get_integer(GLenum pname, GLint *value) {
    if (pname == GL_IMPLEMENTATION_COLOR_READ_FORMAT)
        *value = GL_RGBA;
    else if (pname == GL_IMPLEMENTATION_COLOR_READ_TYPE)
        *value = (GLint)cap.native_read_type;
    else
        *value = 0;
}

static void fake_read_pixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                             GLenum type, void *data) {
    static const unsigned char rendered[8] = {255u, 0u, 0u, 255u, 0u, 0u, 255u, 255u};
    (void)x;
    (void)y;
    if (cap.require_float_readback) {
        if (width == 2 && height == 1 && format == GL_RGBA && type == cap.native_read_type &&
            type == GL_FLOAT) {
            const float values[8] = {0.5f, 0.25f, 1.0f, 0.75f, 0.0f, 1.0f, 0.5f, 1.0f};
            memcpy(data, values, sizeof(values));
        } else if (width == 2 && height == 1 && format == GL_RGBA && type == cap.native_read_type &&
                   type == GL_HALF_FLOAT) {
            const uint16_t values[8] = {0x3800u, 0x3400u, 0x3c00u, 0x3a00u,
                                        0u,      0x3c00u, 0x3800u, 0x3c00u};
            memcpy(data, values, sizeof(values));
        } else
            cap.pending_native_error = GL_INVALID_OPERATION;
        cap.gpu_read_calls++;
        return;
    }
    if (width == 2 && height == 1 && format == GL_RGBA && type == GL_UNSIGNED_BYTE)
        memcpy(data, rendered, sizeof(rendered));
    cap.gpu_read_calls++;
}

static GLenum fake_get_error(void) {
    GLenum error = cap.pending_native_error;
    cap.pending_native_error = GL_NO_ERROR;
    return error;
}

static void fake_framebuffer_texture(GLenum target, GLenum attachment, GLenum textarget,
                                     GLuint texture, GLint level) {
    (void)target;
    (void)level;
    cap.attached_texture = texture;
    cap.framebuffer_textarget = textarget;
    if (attachment >= GL_COLOR_ATTACHMENT0 &&
        attachment < GL_COLOR_ATTACHMENT0 + MGL_MAX_DRAW_BUFFERS) {
        unsigned idx = (unsigned)(attachment - GL_COLOR_ATTACHMENT0);
        cap.attached_colors[idx] = texture;
        ++cap.attachment_calls[idx];
    }
}

static int fail(int n, const char *message) {
    fprintf(stderr, "texture semantic failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj;
    GLuint texture = 0u, fbo = 0u, pbo = 0u;
    GLint value = 0;
    const unsigned char padded_rgb[7] = {1u, 2u, 3u, 99u, 4u, 5u, 6u};
    const unsigned char tight_rgb[6] = {1u, 2u, 3u, 4u, 5u, 6u};
    const unsigned char sub_rgb[3] = {7u, 8u, 9u};
    const unsigned char pbo_data[8] = {42u, 10u, 11u, 12u, 99u, 13u, 14u, 15u};
    const unsigned char pbo_tight[6] = {10u, 11u, 12u, 13u, 14u, 15u};
    unsigned char packed_rgb[7] = {0xeeu, 0xeeu, 0xeeu, 0xeeu, 0xeeu, 0xeeu, 0xeeu};
    unsigned char packed_rgba[8] = {0};
    unsigned char packed_bgra[8] = {0};
    const unsigned char packed_bgra_input[8] = {11u, 22u, 33u, 44u, 55u, 66u, 77u, 88u};
    const unsigned char expected_rgba[8] = {33u, 22u, 11u, 44u, 77u, 66u, 55u, 88u};

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGetString = fake_get_string;
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesPixelStorei = fake_pixel_store;
    native.glesTexImage2D = fake_tex_image;
    native.glesTexSubImage2D = fake_tex_sub_image;
    native.glesTexParameteri = fake_tex_parameteri;
    native.glesTexParameterf = fake_tex_parameterf;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesBufferSubData = fake_buffer_sub_data;
    native.glesGenFramebuffers = fake_gen_framebuffers;
    native.glesDeleteFramebuffers = fake_delete_framebuffers;
    native.glesBindFramebuffer = fake_bind_framebuffer;
    native.glesCheckFramebufferStatus = fake_check_framebuffer;
    native.glesFramebufferTexture2D = fake_framebuffer_texture;

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

    glGenTextures(1, &texture);
    if (texture == 0u || glIsTexture(texture))
        return fail(2, "generated texture must not be realized before bind");
    glBindTexture(GL_TEXTURE_2D, texture);
    if (!glIsTexture(texture) || cap.bound_texture == 0u)
        return fail(3, "texture realization");
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
    if ((GLuint)value != texture)
        return fail(4, "interface texture binding query");

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGB8, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, padded_rgb);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(5, "RGB upload error");
    obj = mgl_bound_texture_2d(&ctx);
    if (obj == NULL || !obj->levels[0].defined || obj->levels[0].data_size != 6u ||
        memcmp(obj->levels[0].data, tight_rgb, 6u) != 0)
        return fail(6, "canonical unpack-aligned shadow");
    if (cap.upload_size != 6u || memcmp(cap.upload, tight_rgb, 6u) != 0 ||
        cap.unpack_alignment != 4)
        return fail(7, "native receives tight upload and unpack state is restored");

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, sub_rgb);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(obj->levels[0].data + 3u, sub_rgb, 3u) != 0)
        return fail(8, "subimage shadow update");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)GL_LINEAR);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &value);
    if (value != (GLint)GL_LINEAR)
        return fail(9, "texture parameter ownership");

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, texture);
    ctx.texture_bindings_dirty = GL_FALSE;
    memset(ctx.texture_bindings_dirty_units, 0, sizeof(ctx.texture_bindings_dirty_units));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)GL_CLAMP_TO_EDGE);
    if (!ctx.texture_bindings_dirty || ctx.texture_bindings_dirty_units[2] != 1u)
        return fail(47, "active texture parameter invalidation scope");
    ctx.texture_bindings_dirty = GL_FALSE;
    memset(ctx.texture_bindings_dirty_units, 0, sizeof(ctx.texture_bindings_dirty_units));
    glBindTextureUnit(3u, texture);
    if (!ctx.texture_bindings_dirty || ctx.texture_bindings_dirty_units[3] != 1u)
        return fail(48, "explicit texture unit invalidation scope");
    glActiveTexture(GL_TEXTURE0);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &value);
    if (value != 1)
        return fail(13, "texture level width query");
    {
        static const struct {
            GLenum pname;
            GLint expected;
        } checks[] = {
            {GL_TEXTURE_RED_SIZE, 8},
            {GL_TEXTURE_GREEN_SIZE, 8},
            {GL_TEXTURE_BLUE_SIZE, 8},
            {GL_TEXTURE_ALPHA_SIZE, 0},
            {GL_TEXTURE_STENCIL_SIZE, 0},
            {GL_TEXTURE_SHARED_SIZE, 0},
            {GL_TEXTURE_RED_TYPE, GL_UNSIGNED_NORMALIZED},
            {GL_TEXTURE_GREEN_TYPE, GL_UNSIGNED_NORMALIZED},
            {GL_TEXTURE_BLUE_TYPE, GL_UNSIGNED_NORMALIZED},
            {GL_TEXTURE_ALPHA_TYPE, GL_NONE},
            {GL_TEXTURE_DEPTH_TYPE, GL_NONE},
        };
        size_t i;
        for (i = 0; i < sizeof(checks) / sizeof(checks[0]); ++i) {
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, checks[i].pname, &value);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != checks[i].expected)
                return fail(270, "bound texture component query");
            glGetTextureLevelParameteriv(texture, 0, checks[i].pname, &value);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != checks[i].expected)
                return fail(271, "named texture component query");
        }
        glGetTexParameteriv(GL_TEXTURE_2D, GL_IMAGE_FORMAT_COMPATIBILITY_TYPE, &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            value != GL_IMAGE_FORMAT_COMPATIBILITY_BY_SIZE)
            return fail(277, "texture image-format compatibility query");
        glGetTextureParameteriv(texture, GL_IMAGE_FORMAT_COMPATIBILITY_TYPE, &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            value != GL_IMAGE_FORMAT_COMPATIBILITY_BY_SIZE)
            return fail(278, "named texture image-format compatibility query");
        glGetTexParameteriv(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != GL_DEPTH_COMPONENT)
            return fail(279, "default depth-stencil texture mode");
        glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != GL_STENCIL_INDEX)
            return fail(280, "depth-stencil texture mode update");
        glGetTextureParameteriv(texture, GL_DEPTH_STENCIL_TEXTURE_MODE, &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != GL_STENCIL_INDEX)
            return fail(281, "named depth-stencil texture mode query");
        glGetMultiTexParameteriv(GL_TEXTURE0, GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE,
                                 &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != GL_STENCIL_INDEX)
            return fail(284, "multitexture depth-stencil mode query");
        glTextureParameteri(texture, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != GL_DEPTH_COMPONENT)
            return fail(282, "named depth-stencil texture mode update");
        glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_RGBA);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
            return fail(283, "invalid depth-stencil texture mode");
        value = 123;
        glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP, 0, GL_TEXTURE_WIDTH, &value);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM || value != 123)
            return fail(272, "cube map texture level requires a face");
    }

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &value);
    if (value != 2)
        return fail(14, "texture level height query");
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &value);
    if (value != (GLint)GL_RGB8)
        return fail(15, "texture level internal format query");

    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, packed_rgb);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || packed_rgb[0] != 1u || packed_rgb[1] != 2u ||
        packed_rgb[2] != 3u || packed_rgb[3] != 0xeeu || packed_rgb[4] != 7u ||
        packed_rgb[5] != 8u || packed_rgb[6] != 9u)
        return fail(16, "pack-aligned RGB GetTexImage");
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, packed_rgba);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(packed_rgba, (const unsigned char[]){1u, 2u, 3u, 255u, 7u, 8u, 9u, 255u}, 8u) != 0)
        return fail(17, "RGB to RGBA GetTexImage conversion");

    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, (GLsizeiptr)sizeof(pbo_data), pbo_data, GL_STATIC_DRAW);
    glTexImage2D(GL_TEXTURE_2D, 1, (GLint)GL_RGB8, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 (const void *)(uintptr_t)1u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj->levels[1].data_size != 6u ||
        memcmp(obj->levels[1].data, pbo_tight, 6u) != 0)
        return fail(10, "PBO upload uses logical CPU shadow plus byte offset");

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0u);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, 10, NULL, GL_STATIC_DRAW);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, (void *)(uintptr_t)1u);
    {
        struct mgl_buffer *pack = mgl_buffer_find(&ctx, pbo);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || pack == NULL || pack->shadow == NULL ||
            pack->shadow[1] != 1u || pack->shadow[2] != 2u || pack->shadow[3] != 3u ||
            pack->shadow[5] != 7u || pack->shadow[6] != 8u || pack->shadow[7] != 9u)
            return fail(18, "PBO GetTexImage uses logical pack-buffer offset and alignment");
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0u);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.attached_texture != obj->native_name)
        return fail(11, "framebuffer texture native realization");
    if (mgl_bound_framebuffer(&ctx, GL_FRAMEBUFFER)->color_texture != texture)
        return fail(12, "framebuffer canonical texture attachment");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    /* Rectangle textures are distinct interface targets backed by GLES 2D.
     * Their level-zero image must attach to an FBO without target errors. */
    {
        GLuint rect = 0u;
        struct mgl_texture *rect_obj;
        GLint normal_binding = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &normal_binding);
        glGenTextures(1, &rect);
        glBindTexture(GL_TEXTURE_RECTANGLE, rect);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(330, "rectangle binding");
        glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE, &value);
        if ((GLuint)value != rect)
            return fail(331, "rectangle binding query");
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
        if (value != normal_binding)
            return fail(332, "rectangle binding must not change 2D interface binding");
        rect_obj = mgl_bound_texture(&ctx, GL_TEXTURE_RECTANGLE);
        if (rect_obj == NULL || rect_obj->target != GL_TEXTURE_RECTANGLE ||
            rect_obj->min_filter != GL_LINEAR || rect_obj->wrap_s != GL_CLAMP_TO_EDGE)
            return fail(333, "rectangle sampler defaults");
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, 3, 5, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.image_target != GL_TEXTURE_2D ||
            !rect_obj->levels[0].defined || rect_obj->levels[0].width != 3 ||
            rect_obj->levels[0].height != 5)
            return fail(334, "rectangle image GLES 2D backing");
        glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE, 0, GL_TEXTURE_WIDTH, &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 3)
            return fail(335, "rectangle level query");
        glTexImage2D(GL_TEXTURE_RECTANGLE, 1, GL_RGBA, 3, 5, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
            return fail(336, "rectangle forbids mipmaps");
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
            return fail(337, "rectangle forbids mipmap filtering");
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, rect, 0);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.framebuffer_textarget != GL_TEXTURE_2D ||
            cap.attached_texture != rect_obj->native_name ||
            glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            return fail(338, "rectangle framebuffer attachment");
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, 0, 0);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(339, "rectangle framebuffer detach");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.texture_at_image != obj->native_name)
            return fail(340, "2D image chooses its own native binding after rectangle operations");
        glBindTexture(GL_TEXTURE_RECTANGLE, 0);
        glDeleteTextures(1, &rect);
    }
    /* A float RGB rectangle render target uses GLES color-renderable RGBA16F. */
    {
        GLuint rect = 0u, alpha_tex = 0u, one_d = 0u;
        const float rgb[3] = {0.25f, 0.5f, 0.75f};
        const unsigned char alpha = 127u;
        float rgba[4];
        struct mgl_texture *rect_obj;
        glGenTextures(1, &rect);
        glBindTexture(GL_TEXTURE_RECTANGLE, rect);
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGB16F, 1, 1, 0, GL_RGB, GL_FLOAT, rgb);
        rect_obj = mgl_bound_texture(&ctx, GL_TEXTURE_RECTANGLE);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || rect_obj == NULL ||
            rect_obj->levels[0].internal_format != GL_RGB16F ||
            cap.upload_internal != (GLint)GL_RGBA16F || cap.upload_format != GL_RGBA ||
            cap.upload_type != GL_FLOAT || cap.upload_size != sizeof(rgba))
            return fail(341, "float RGB rectangle texture storage and upload");
        memcpy(rgba, cap.upload, sizeof(rgba));
        if (rgba[0] != rgb[0] || rgba[1] != rgb[1] || rgba[2] != rgb[2] || rgba[3] != 1.0f)
            return fail(342, "float RGB rectangle alpha expansion");
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, rect, 0);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.attached_texture != rect_obj->native_name ||
            glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            return fail(343, "float rectangle framebuffer attachment");
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, 0, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteTextures(1, &rect);

        glGenTextures(1, &alpha_tex);
        glBindTexture(GL_TEXTURE_2D, alpha_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 1, 1, 0, GL_ALPHA, GL_UNSIGNED_BYTE, &alpha);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != (GLint)GL_R8 ||
            cap.upload_format != GL_RED || cap.upload_size != 1u || cap.upload[0] != alpha ||
            cap.swizzle[0] != GL_ZERO || cap.swizzle[1] != GL_ZERO || cap.swizzle[2] != GL_ZERO ||
            cap.swizzle[3] != GL_RED)
            return fail(344, "alpha-only texture GLES swizzle");
        glDeleteTextures(1, &alpha_tex);

        glGenTextures(1, &one_d);
        glBindTexture(GL_TEXTURE_2D, texture);
        glEnable(GL_TEXTURE_1D);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || !glIsEnabled(GL_TEXTURE_1D))
            return fail(345, "1D texture enable state");
        glBindTexture(GL_TEXTURE_1D, one_d);
        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 1, 0, GL_RGB, GL_UNSIGNED_BYTE,
                     (const unsigned char[]){15u, 25u, 35u});
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.image_target != GL_TEXTURE_2D ||
            cap.upload_height != 1 ||
            cap.texture_at_image != mgl_bound_texture(&ctx, GL_TEXTURE_1D)->native_name)
            return fail(346, "1D texture GLES storage");
        mgl_texture_realize_bindings(&ctx);
        if (cap.bound_texture_2d[0] != mgl_bound_texture(&ctx, GL_TEXTURE_1D)->native_name)
            return fail(347, "1D texture remains selected for draw");
        glBindTexture(GL_TEXTURE_2D, texture);
        mgl_texture_realize_bindings(&ctx);
        if (cap.bound_texture_2d[0] != obj->native_name)
            return fail(348, "2D binding restored after 1D selection");
        glDisable(GL_TEXTURE_1D);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || glIsEnabled(GL_TEXTURE_1D))
            return fail(349, "1D texture disable state");
        glDeleteTextures(1, &one_d);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA8, 2, 1, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                 packed_bgra_input);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGBA ||
        cap.upload_size != 8u || memcmp(cap.upload, expected_rgba, 8u) != 0 ||
        obj->levels[0].format != GL_RGBA || obj->levels[0].type != GL_UNSIGNED_BYTE ||
        memcmp(obj->levels[0].data, expected_rgba, 8u) != 0)
        return fail(19, "BGRA/REV tex image canonical RGBA CPU + GLES storage");
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, packed_bgra);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(packed_bgra, packed_bgra_input, 8u) != 0)
        return fail(20, "BGRA/REV readback conversion");

    /* Packed RGBA/REV is also a legal desktop format/type pair.
     * It must not be sent to GLES directly, including through a PBO. */
    {
        unsigned char packed_rgba_rev[8];
        size_t k;
        for (k = 0u; k < 2u; ++k) {
            uint32_t packed = (uint32_t)expected_rgba[k * 4u] |
                              ((uint32_t)expected_rgba[k * 4u + 1u] << 8u) |
                              ((uint32_t)expected_rgba[k * 4u + 2u] << 16u) |
                              ((uint32_t)expected_rgba[k * 4u + 3u] << 24u);
            memcpy(packed_rgba_rev + k * 4u, &packed, sizeof(packed));
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV,
                     packed_rgba_rev);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_type != GL_UNSIGNED_BYTE ||
            cap.upload_format != GL_RGBA || memcmp(cap.upload, expected_rgba, 8u) != 0)
            return fail(300, "RGBA/REV tex image conversion");
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(packed_rgba_rev), packed_rgba_rev,
                     GL_STATIC_DRAW);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(obj->levels[0].data, expected_rgba, 8u) != 0)
            return fail(301, "RGBA/REV pixel unpack buffer conversion");
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        /* WinD3D also asks for half-float storage from a packed BGRA upload.
         * mGL converts the byte payload to half-float for GLES, preserving
         * the requested internal storage format and canonical CPU colors. */
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 2, 1, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                     packed_bgra_input);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != GL_RGBA16F ||
            cap.upload_type != GL_HALF_FLOAT || cap.upload_format != GL_RGBA ||
            obj->levels[0].internal_format != GL_RGBA16F ||
            memcmp(obj->levels[0].data, expected_rgba, 8u) != 0)
            return fail(302, "half-float storage and BGRA packed upload");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV,
                     packed_rgba_rev);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(303, "restore byte storage");
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                    packed_bgra_input);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(obj->levels[0].data, expected_rgba, 8u) != 0 || cap.upload_format != GL_RGBA ||
        cap.upload_size != 8u)
        return fail(21, "BGRA/REV subimage conversion");

    /* PBO NULL pointers are buffer offsets, not missing texture data. */
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(packed_bgra_input), packed_bgra_input,
                 GL_STATIC_DRAW);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(obj->levels[0].data, expected_rgba, 8u) != 0)
        return fail(22, "BGRA/REV pixel unpack buffer conversion");
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(packed_bgra_input), NULL, GL_STATIC_DRAW);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(mgl_buffer_find(&ctx, pbo)->shadow, packed_bgra_input, 8u) != 0)
        return fail(23, "BGRA/REV pixel pack buffer readback");
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    {
        const unsigned char bgra_bytes[8] = {11u, 22u, 33u, 44u, 55u, 66u, 77u, 88u};
        const unsigned char rgba_bytes[8] = {33u, 22u, 11u, 44u, 77u, 66u, 55u, 88u};
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA, 2, 1, 0, GL_BGRA, GL_UNSIGNED_BYTE,
                     bgra_bytes);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGBA ||
            cap.upload_type != GL_UNSIGNED_BYTE || cap.upload_size != sizeof(rgba_bytes) ||
            memcmp(cap.upload, rgba_bytes, sizeof(rgba_bytes)) != 0 ||
            obj->levels[0].format != GL_RGBA ||
            memcmp(obj->levels[0].data, rgba_bytes, sizeof(rgba_bytes)) != 0)
            return fail(80, "BGRA byte upload normalized to GLES RGBA");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_BGRA, GL_UNSIGNED_BYTE, bgra_bytes);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGBA ||
            cap.upload_size != sizeof(rgba_bytes) ||
            memcmp(cap.upload, rgba_bytes, sizeof(rgba_bytes)) != 0)
            return fail(81, "BGRA byte subimage normalized to GLES RGBA");
    }
    {
        const unsigned char depth_bytes[3] = {0u, 128u, 255u};
        const unsigned char depth_sub[1] = {64u};
        const uint16_t depth16[3] = {0u, 32896u, 65535u};
        const uint16_t sub16 = 16448u;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_DEPTH_COMPONENT, 3, 1, 0, GL_DEPTH_COMPONENT,
                     GL_UNSIGNED_BYTE, depth_bytes);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.upload_internal != (GLint)GL_DEPTH_COMPONENT16 ||
            cap.upload_format != GL_DEPTH_COMPONENT || cap.upload_type != GL_UNSIGNED_SHORT ||
            cap.upload_size != sizeof(depth16) ||
            memcmp(cap.upload, depth16, sizeof(depth16)) != 0 || obj == NULL ||
            obj->levels[0].type != GL_UNSIGNED_BYTE ||
            obj->levels[0].data_size != sizeof(depth_bytes) ||
            memcmp(obj->levels[0].data, depth_bytes, sizeof(depth_bytes)) != 0)
            return fail(82, "depth byte upload to GLES depth16 preserving desktop shadow");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 0, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE,
                        depth_sub);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_type != GL_UNSIGNED_SHORT ||
            cap.upload_size != sizeof(sub16) || memcmp(cap.upload, &sub16, sizeof(sub16)) != 0 ||
            obj->levels[0].data[1] != 64u)
            return fail(83, "depth byte subimage to GLES depth16 preserving desktop shadow");
        {
            unsigned char readback[3] = {0};
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, readback);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                memcmp(readback, (unsigned char[]){0u, 64u, 255u}, sizeof(readback)) != 0)
                return fail(84, "depth byte readback preserves desktop values");
            glPixelStorei(GL_PACK_ALIGNMENT, 4);
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    {
        GLint proxy_width = -1, proxy_height = -1, real_width = -1;
        unsigned old_binds = cap.native_texture_binds;
        GLenum old_upload_target = cap.image_target;
        glTexImage2D(GL_PROXY_TEXTURE_2D, 1, GL_ALPHA, 64, 64, 0, GL_ALPHA, GL_UNSIGNED_BYTE,
                     (const void *)(uintptr_t)1u);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.native_texture_binds != old_binds ||
            cap.image_target != old_upload_target)
            return fail(420, "proxy definition has no native image or pixel access");
        glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &proxy_width);
        glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 1, GL_TEXTURE_HEIGHT, &proxy_height);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || proxy_width != 64 || proxy_height != 64)
            return fail(421, "proxy image reports supported dimensions");
        glTexImage2D(GL_PROXY_TEXTURE_2D, 1, GL_ALPHA, 8192, 8192, 0, GL_ALPHA, GL_UNSIGNED_BYTE,
                     NULL);
        glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &proxy_width);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || proxy_width != 0)
            return fail(422, "unsupported proxy dimensions are zero without GL error");
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &real_width);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || real_width != 3)
            return fail(423, "proxy image does not modify bound 2D texture");
    }
    {
        const unsigned char rgba8[8] = {0u, 255u, 128u, 64u, 255u, 0u, 64u, 128u};
        const uint16_t half[8] = {0x0000u, 0x3c00u, 0x3804u, 0x3404u,
                                  0x3c00u, 0x0000u, 0x3404u, 0x3804u};
        const unsigned char sub[4] = {255u, 128u, 64u, 0u};
        const uint16_t sub_half[4] = {0x3c00u, 0x3804u, 0x3404u, 0x0000u};
        GLint bits = -1;
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA16F, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     rgba8);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.upload_internal != (GLint)GL_RGBA16F || cap.upload_format != GL_RGBA ||
            cap.upload_type != GL_HALF_FLOAT || cap.upload_size != sizeof(half) ||
            memcmp(cap.upload, half, sizeof(half)) != 0 || obj == NULL ||
            obj->levels[0].type != GL_UNSIGNED_BYTE || obj->levels[0].data_size != sizeof(rgba8) ||
            memcmp(obj->levels[0].data, rgba8, sizeof(rgba8)) != 0)
            return fail(85, "RGBA16F byte-source expands to GLES half floats");
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE, &bits);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || bits != 16)
            return fail(86, "RGBA16F reports half-float component size");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, sub);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_type != GL_HALF_FLOAT ||
            cap.upload_size != sizeof(sub_half) ||
            memcmp(cap.upload, sub_half, sizeof(sub_half)) != 0 ||
            memcmp(obj->levels[0].data + 4, sub, sizeof(sub)) != 0)
            return fail(87, "RGBA16F byte-source subimage preserves desktop shadow");
    }
    {
        const uint16_t half_input[4] = {0x3c00u, 0x3800u, 0x0000u, 0x3c00u};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1, 1, 0, GL_RGBA, GL_HALF_FLOAT, half_input);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.upload_internal != (GLint)GL_RGBA16F || cap.upload_type != GL_HALF_FLOAT ||
            cap.upload_format != GL_RGBA || cap.upload_size != sizeof(half_input) ||
            memcmp(cap.upload, half_input, sizeof(half_input)) != 0 || obj == NULL ||
            obj->levels[0].type != GL_HALF_FLOAT || obj->levels[0].data_size != sizeof(half_input))
            return fail(409, "RGBA16F accepts native half-float source");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, 2, 2, 0, GL_DEPTH_STENCIL,
                     GL_FLOAT_32_UNSIGNED_INT_24_8_REV, NULL);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.upload_internal != (GLint)GL_DEPTH32F_STENCIL8 ||
            cap.upload_format != GL_DEPTH_STENCIL ||
            cap.upload_type != GL_FLOAT_32_UNSIGNED_INT_24_8_REV || obj == NULL ||
            obj->levels[0].data_size != 32u)
            return fail(410, "depth32f-stencil8 texture allocation is valid");
    }
    {
        const float input[8] = {0.25f, 0.5f, 1.5f, 1.0f, -0.25f, 2.0f, 4.0f, 0.75f};
        cap.float_linear = 1;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 2, 1, 0, GL_RGBA, GL_FLOAT, input);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.upload_internal != (GLint)GL_RGBA32F || cap.upload_format != GL_RGBA ||
            cap.upload_type != GL_FLOAT || cap.upload_size != sizeof(input) ||
            memcmp(cap.upload, input, sizeof(input)) != 0 || obj == NULL ||
            obj->levels[0].type != GL_FLOAT || obj->levels[0].data_size != sizeof(input) ||
            memcmp(obj->levels[0].data, input, sizeof(input)) != 0)
            return fail(411, "RGBA32F preserves native float source and native upload");
        cap.float_linear = 0;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 2, 1, 0, GL_RGBA, GL_FLOAT, input);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.upload_internal != (GLint)GL_RGBA16F || cap.upload_format != GL_RGBA ||
            cap.upload_type != GL_FLOAT || cap.upload_size != sizeof(input) ||
            memcmp(cap.upload, input, sizeof(input)) != 0 || value != (GLint)GL_RGBA32F ||
            obj->levels[0].internal_format != GL_RGBA32F ||
            obj->levels[0].data_size != sizeof(input) ||
            memcmp(obj->levels[0].data, input, sizeof(input)) != 0)
            return fail(412,
                        "RGBA32F without float-linear uses RGBA16F native, keeps desktop shadow");
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 2, 1, 0, GL_RGBA, GL_FLOAT, input);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != (GLint)GL_RGBA16F)
            return fail(416, "nearest-only RGBA32F uses 16F fallback without float-linear");
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 2, 1, 0, GL_RGBA, GL_FLOAT, input);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != (GLint)GL_RGBA16F)
            return fail(417, "mipmap-linear RGBA32F uses filterable fallback");
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 2, 1, 0, GL_RGBA, GL_FLOAT, input);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != (GLint)GL_RGBA16F)
            return fail(418, "linear magnification RGBA32F uses filterable fallback");
        cap.float_linear = 1;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 2, 1, 0, GL_RGBA, GL_FLOAT, input);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != (GLint)GL_RGBA32F)
            return fail(413, "RGBA32F is preserved when float-linear is advertised");
        cap.float_linear = 0;
    }
    {
        GLuint storage_texture = 0u;
        glGenTextures(1, &storage_texture);
        glBindTexture(GL_TEXTURE_2D, storage_texture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 2, 1);
        obj = mgl_bound_texture_2d(&ctx);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.upload_internal != (GLint)GL_RGBA16F || obj == NULL || !obj->immutable_format ||
            obj->levels[0].internal_format != GL_RGBA32F || value != (GLint)GL_RGBA32F)
            return fail(414, "immutable RGBA32F uses filterable native without extension");
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenTextures(1, &storage_texture);
        glBindTexture(GL_TEXTURE_2D, storage_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 2, 1);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != (GLint)GL_RGBA16F)
            return fail(419, "nearest-only immutable RGBA32F uses 16F fallback");
        cap.float_linear = 1;
        glGenTextures(1, &storage_texture);
        glBindTexture(GL_TEXTURE_2D, storage_texture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 2, 1);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != (GLint)GL_RGBA32F)
            return fail(415, "immutable RGBA32F retains native precision with extension");
        cap.float_linear = 0;
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    {
        const unsigned char indices[2] = {0u, 1u};
        const unsigned char rgba[8] = {0u, 255u, 0u, 255u, 255u, 0u, 128u, 255u};
        const unsigned char subindex[1] = {1u};
        const GLfloat r[2] = {0.0f, 1.0f}, g[2] = {1.0f, 0.0f};
        const GLfloat b[2] = {0.0f, 0.5f}, a[2] = {1.0f, 1.0f};
        glPixelMapfv(GL_PIXEL_MAP_I_TO_R, 2, r);
        glPixelMapfv(GL_PIXEL_MAP_I_TO_G, 2, g);
        glPixelMapfv(GL_PIXEL_MAP_I_TO_B, 2, b);
        glPixelMapfv(GL_PIXEL_MAP_I_TO_A, 2, a);
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA, 2, 1, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
                     indices);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != (GLint)GL_RGBA ||
            cap.upload_format != GL_RGBA || cap.upload_type != GL_UNSIGNED_BYTE ||
            cap.upload_size != sizeof(rgba) || memcmp(cap.upload, rgba, sizeof(rgba)) != 0 ||
            obj == NULL || obj->levels[0].data_size != sizeof(rgba) ||
            memcmp(obj->levels[0].data, rgba, sizeof(rgba)) != 0)
            return fail(88, "color-index texture converted through pixel maps");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, subindex);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGBA ||
            cap.upload_size != 4u || memcmp(cap.upload, rgba + 4u, 4u) != 0 ||
            memcmp(obj->levels[0].data, rgba + 4u, 4u) != 0)
            return fail(89, "color-index subimage uses live palette");
        /* GL_MAP_COLOR must not route index-to-RGBA transfers through I_TO_I.
         * That map defaults to a single zero entry, so incorrectly applying
         * it erases the index and makes valid palette textures uniform. */
        glPixelTransferi(GL_MAP_COLOR, GL_TRUE);
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA, 2, 1, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
                     indices);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_size != sizeof(rgba) ||
            memcmp(cap.upload, rgba, sizeof(rgba)) != 0 || obj == NULL ||
            obj->levels[0].data_size != sizeof(rgba) ||
            memcmp(obj->levels[0].data, rgba, sizeof(rgba)) != 0)
            return fail(
                90, "color-index to RGBA ignores index-to-index map even when MAP_COLOR enabled");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, subindex);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_size != 4u ||
            memcmp(cap.upload, rgba + 4u, 4u) != 0)
            return fail(91, "color-index subimage ignores index-to-index map");
        glPixelTransferi(GL_MAP_COLOR, GL_FALSE);
    }

    {
        const unsigned char pixels[4] = {18u, 73u, 129u, 244u};
        const unsigned char replacement[1] = {215u};
        unsigned char readback[4] = {0};
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_LUMINANCE8, 2, 2, 0, GL_RED, GL_UNSIGNED_BYTE,
                     pixels);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL ||
            obj->levels[0].internal_format != GL_LUMINANCE8 ||
            obj->levels[0].data_size != sizeof(pixels) ||
            memcmp(obj->levels[0].data, pixels, sizeof(pixels)) != 0 ||
            cap.upload_internal != (GLint)GL_R8 || cap.upload_format != GL_RED ||
            cap.upload_type != GL_UNSIGNED_BYTE || cap.upload_size != sizeof(pixels) ||
            memcmp(cap.upload, pixels, sizeof(pixels)) != 0 || cap.swizzle[0] != GL_RED ||
            cap.swizzle[1] != GL_RED || cap.swizzle[2] != GL_RED || cap.swizzle[3] != GL_ONE) {
            return fail(96, "luminance8 red-byte upload and swizzle");
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE, replacement);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RED ||
            cap.upload_size != sizeof(replacement) || obj->levels[0].data[1] != replacement[0])
            return fail(97, "luminance8 red-byte subimage");
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, readback);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(readback, (unsigned char[]){18u, 215u, 129u, 244u}, sizeof(readback)) != 0)
            return fail(98, "luminance8 red-byte readback");
        {
            const unsigned char rgba_pixels[16] = {10u, 20u,  30u,  40u,  50u,  60u,  70u,  80u,
                                                   90u, 100u, 110u, 120u, 130u, 140u, 150u, 160u};
            const unsigned char dxt1[8] = {0u};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         rgba_pixels);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.swizzle[0] != GL_RED ||
                cap.swizzle[1] != GL_GREEN || cap.swizzle[2] != GL_BLUE ||
                cap.swizzle[3] != GL_ALPHA)
                return fail(241, "redefining luminance storage as RGBA restores normal channels");
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, 2, 2, 0, GL_RED, GL_UNSIGNED_BYTE,
                         pixels);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.swizzle[1] != GL_RED ||
                cap.swizzle[2] != GL_RED || cap.swizzle[3] != GL_ONE)
                return fail(242, "redefining RGB storage as luminance applies luminance channels");
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 4, 4, 0,
                                   (GLsizei)sizeof(dxt1), dxt1);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.swizzle[0] != GL_RED ||
                cap.swizzle[1] != GL_GREEN || cap.swizzle[2] != GL_BLUE ||
                cap.swizzle[3] != GL_ALPHA)
                return fail(243, "redefining luminance as BC1 restores normal channels");
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    {
        const unsigned char pixels[8] = {17u, 40u, 70u, 101u, 207u, 3u, 5u, 9u};
        const unsigned char update[4] = {99u, 21u, 43u, 55u};
        unsigned char result[8] = {0};
        GLint effective = 0, compressed = 1;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_INTENSITY8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL ||
            obj->levels[0].internal_format != GL_INTENSITY8 || obj->levels[0].format != GL_RED ||
            obj->levels[0].data_size != 2u ||
            memcmp(obj->levels[0].data, (unsigned char[]){17u, 207u}, 2u) != 0 ||
            cap.upload_internal != (GLint)GL_R8 || cap.upload_format != GL_RED ||
            cap.upload_type != GL_UNSIGNED_BYTE || cap.upload_size != 2u ||
            memcmp(cap.upload, (unsigned char[]){17u, 207u}, 2u) != 0 || cap.swizzle[0] != GL_RED ||
            cap.swizzle[1] != GL_RED || cap.swizzle[2] != GL_RED || cap.swizzle[3] != GL_RED)
            return fail(416, "intensity8 red channel and all-channel swizzle");
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, result);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(result, (unsigned char[]){17u, 17u, 17u, 17u, 207u, 207u, 207u, 207u},
                   sizeof(result)) != 0)
            return fail(417, "intensity8 desktop RGBA readback");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, update);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RED ||
            cap.upload_size != 1u || cap.upload[0] != 99u || obj->levels[0].data[1] != 99u)
            return fail(418, "intensity8 subimage red channel");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_INTENSITY, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            obj->levels[0].internal_format != GL_INTENSITY8 ||
            cap.upload_internal != (GLint)GL_R8 || cap.swizzle[3] != GL_RED)
            return fail(422, "unsized intensity has intensity8 storage semantics");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.swizzle[0] != GL_RED ||
            cap.swizzle[1] != GL_GREEN || cap.swizzle[2] != GL_BLUE || cap.swizzle[3] != GL_ALPHA)
            return fail(419, "intensity8 redefinition restores RGBA swizzle");
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &effective);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &compressed);
        if (effective != (GLint)GL_RGBA8 || compressed != 0 ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(420, "texture format query reports actual RGBA storage");
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }
    {
        const unsigned char pixels[16] = {11u, 22u,  33u,  44u,  55u,  66u,  77u,  88u,
                                          99u, 111u, 122u, 133u, 144u, 155u, 166u, 177u};
        const GLenum compressed_formats[4] = {
            GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
            GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT};
        unsigned i;
        for (i = 0; i < 4u; ++i) {
            GLint internal = 0, is_compressed = -1;
            const GLint expected_internal = i == 0u ? (GLint)GL_RGB8 : (GLint)GL_RGBA8;
            const GLenum expected_format = i == 0u ? GL_RGB : GL_RGBA;
            glTexImage2D(GL_TEXTURE_2D, 0, compressed_formats[i], 2, 2, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, pixels);
            obj = mgl_bound_texture_2d(&ctx);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &is_compressed);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL ||
                internal != expected_internal || is_compressed != GL_FALSE ||
                obj->levels[0].compressed || cap.upload_internal != expected_internal ||
                cap.upload_format != expected_format || cap.upload_type != GL_UNSIGNED_BYTE ||
                cap.upload_size != (i == 0u ? 12u : 16u) || cap.swizzle[0] != GL_RED ||
                cap.swizzle[1] != GL_GREEN || cap.swizzle[2] != GL_BLUE ||
                cap.swizzle[3] != GL_ALPHA)
                return fail(421, "uncompressed pixels with S3TC requested storage");
        }
    }

    /* Normalized 16-bit storage precision probe. Without EXT_texture_norm16 the
     * GPU stores RGBA8; report its real channel precision, not requested 16. */
    {
        const uint16_t rgba16[8] = {65535u, 0u, 32768u, 65535u, 257u, 65535u, 0u, 65535u};
        const uint16_t replacement[4] = {0u, 32768u, 65535u, 65535u};
        uint16_t readback[8] = {0};
        unsigned char readback8[8] = {0};
        GLint size = -1;
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA16, 2, 1, 0, GL_RGBA, GL_UNSIGNED_SHORT,
                     rgba16);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != (GLint)GL_RGBA8 ||
            cap.upload_type != GL_UNSIGNED_BYTE || cap.upload_size != 8u ||
            obj->levels[0].type != GL_UNSIGNED_SHORT ||
            obj->levels[0].data_size != sizeof(rgba16) ||
            memcmp(obj->levels[0].data, rgba16, sizeof(rgba16)) != 0)
            return fail(24, "RGBA16 normalized shadow and honest GLES8 fallback");
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE, &size);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || size != 8)
            return fail(25, "RGBA16 fallback reports actual 8-bit red size");
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_SIZE, &size);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || size != 8)
            return fail(26, "RGBA16 fallback reports actual 8-bit alpha size");
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_SHORT, readback);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(readback, rgba16, sizeof(rgba16)) != 0)
            return fail(27, "RGBA16 CPU shadow readback preserves 16-bit normalized values");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT, replacement);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_type != GL_UNSIGNED_BYTE ||
            cap.upload_size != 4u ||
            memcmp(obj->levels[0].data, replacement, sizeof(replacement)) != 0)
            return fail(28, "RGBA16 2D subimage preserves shadow and downconverts GLES upload");
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, readback8);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(readback8, (unsigned char[]){0u, 128u, 255u, 255u, 1u, 255u, 0u, 255u}, 8u) != 0)
            return fail(29, "RGBA16 GetTexImage to RGBA8 conversion");
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA16, 4, 4, 0, GL_RGBA, GL_UNSIGNED_SHORT, NULL);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != (GLint)GL_RGBA8)
            return fail(30, "RGBA16 NULL allocation probe");
    }

    {
        GLuint textures[2] = {0u, 0u};
        const unsigned char packed_rgba_input[8] = {0x78u, 0x56u, 0x34u, 0x12u,
                                                    0x04u, 0x03u, 0x02u, 0x01u};
        const unsigned char expected_packed_rgba[8] = {0x12u, 0x34u, 0x56u, 0x78u,
                                                       0x01u, 0x02u, 0x03u, 0x04u};
        const uint16_t rgb565[2] = {0xf800u, 0x07e0u};
        unsigned char rgb_readback[6] = {0};
        uint16_t rgb565_readback[2] = {0};
        unsigned char rgba8888_readback[8] = {0};
        GLint bits;
        struct mgl_framebuffer *fb;
        unsigned old_refresh0, old_refresh1;
        glGenTextures(2, textures);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
                     packed_rgba_input);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != (GLint)GL_RGBA8 ||
            cap.upload_type != GL_UNSIGNED_BYTE || cap.upload_size != 8u ||
            memcmp(cap.upload, expected_packed_rgba, 8u) != 0)
            return fail(31, "packed RGBA8888 normalized upload");
        obj = mgl_bound_texture_2d(&ctx);
        if (obj == NULL || memcmp(obj->levels[0].data, expected_packed_rgba, 8u) != 0)
            return fail(32, "packed RGBA8888 canonical shadow");
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, rgba8888_readback);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(rgba8888_readback, packed_rgba_input, 8u) != 0)
            return fail(40, "packed RGBA8888 desktop readback");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
                        packed_rgba_input);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_type != GL_UNSIGNED_BYTE ||
            memcmp(obj->levels[0].data, expected_packed_rgba, 8u) != 0)
            return fail(33, "packed RGBA8888 subimage");
        glBindTexture(GL_TEXTURE_2D, textures[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGB5, 2, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                     rgb565);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != (GLint)GL_RGB565 ||
            cap.upload_type != GL_UNSIGNED_SHORT_5_6_5 || cap.upload_size != sizeof(rgb565) ||
            memcmp(cap.upload, rgb565, sizeof(rgb565)) != 0)
            return fail(34, "RGB5 lowered to native RGB565");
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_GREEN_SIZE, &bits);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || bits != 6)
            return fail(35, "RGB5 reports actual native green bits");
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, rgb565_readback);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(rgb565_readback, rgb565, sizeof(rgb565)) != 0)
            return fail(41, "RGB565 packed desktop readback");
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_readback);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(rgb_readback, (unsigned char[]){255u, 0u, 0u, 0u, 255u, 0u},
                   sizeof(rgb_readback)) != 0)
            return fail(36, "RGB565 shadow desktop RGB readback");
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               textures[0], 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                               textures[1], 0);
        fb = mgl_bound_framebuffer(&ctx, GL_DRAW_FRAMEBUFFER);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || fb == NULL ||
            fb->color_texture != textures[0] || fb->extra_color_textures[1] != textures[1] ||
            cap.attached_colors[0] != mgl_texture_find(&ctx, textures[0])->native_name ||
            cap.attached_colors[1] != mgl_texture_find(&ctx, textures[1])->native_name ||
            glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            return fail(37, "mixed color attachments");
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &bits);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || (GLuint)bits != textures[1])
            return fail(38, "second color attachment query");
        old_refresh0 = cap.attachment_calls[0];
        old_refresh1 = cap.attachment_calls[1];
        if (!mgl_texture_refresh_framebuffer_attachments(&ctx, fb) ||
            cap.attachment_calls[0] <= old_refresh0 || cap.attachment_calls[1] <= old_refresh1)
            return fail(39, "both attachments survive native framebuffer refresh");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        /* Framebuffer rendering invalidates the CPU upload shadow. glGetTexImage observes
         * current GLES storage. */
        {
            static const unsigned char rendered_bgra[8] = {0u, 0u, 255u, 255u, 255u, 0u, 0u, 255u};
            unsigned char rendered_readback[8] = {0};
            unsigned gpu_reads = cap.gpu_read_calls;
            native.glesReadPixels = fake_read_pixels;
            native.glesGetError = fake_get_error;
            /* A failure from a GLES draw must be visible at the next public
             * glGetError(), not first noticed during a later GPU readback.
             * An earlier interface error must not discard the GLES error. */
            cap.pending_native_error = GL_INVALID_OPERATION;
            if (glGetError() != GL_INVALID_OPERATION || glGetError() != GL_NO_ERROR)
                return fail(45, "GLES native error visible through desktop glGetError");
            mgl_error_set(&ctx.errors, GL_INVALID_ENUM);
            cap.pending_native_error = GL_INVALID_VALUE;
            if (glGetError() != GL_INVALID_ENUM || glGetError() != GL_INVALID_VALUE ||
                glGetError() != GL_NO_ERROR)
                return fail(46, "interface and GLES errors both delivered without loss");
            glBindTexture(GL_TEXTURE_2D, textures[0]);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                          rendered_readback);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                memcmp(rendered_readback, rendered_bgra, sizeof(rendered_bgra)) != 0 ||
                cap.gpu_read_calls != gpu_reads + 1u || cap.read_fbo != 0u || cap.draw_fbo != 0u ||
                cap.pack_alignment != 4 ||
                memcmp(mgl_bound_texture_2d(&ctx)->levels[0].data, expected_packed_rgba, 8u) != 0)
                return fail(42, "rendered FBO image readback overrides stale CPU upload shadow");
            /* An incomplete temporary FBO must not return stale shadow pixels. */
            memset(rendered_readback, 0x5a, sizeof(rendered_readback));
            cap.readback_status = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                          rendered_readback);
            if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION ||
                rendered_readback[0] != 0x5au || cap.read_fbo != 0u || cap.draw_fbo != 0u ||
                cap.pack_alignment != 4)
                return fail(43, "incomplete native readback preserves state and destination");
            cap.readback_status = 0u;
            /* A readback can use a color texture already bound to the current
             * draw/read framebuffer. Reusing that attachment must avoid
             * generating a duplicate FBO, and must preserve both bindings. */
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBindTexture(GL_TEXTURE_2D, textures[0]);
            {
                unsigned creates = cap.generated_framebuffer_calls;
                unsigned reads = cap.gpu_read_calls;
                GLuint native_fbo = mgl_framebuffer_find(&ctx, fbo)->native_name;
                memset(rendered_readback, 0u, sizeof(rendered_readback));
                glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                              rendered_readback);
                if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                    memcmp(rendered_readback, rendered_bgra, sizeof(rendered_bgra)) != 0 ||
                    cap.generated_framebuffer_calls != creates ||
                    cap.gpu_read_calls != reads + 1u || cap.read_fbo != native_fbo ||
                    cap.draw_fbo != native_fbo)
                    return fail(44, "bound-FBO GPU readback without redundant framebuffer");
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0u);
            {
                GLuint probe_texture = 0u;
                const unsigned char pixels_in[8] = {16u, 32u, 48u, 64u, 80u, 96u, 112u, 128u};
                const unsigned char pixels_bgra[8] = {48u, 32u, 16u, 64u, 112u, 96u, 80u, 128u};
                unsigned char pixels_out[8] = {0};
                unsigned reads = cap.gpu_read_calls;
                glGenTextures(1, &probe_texture);
                glBindTexture(GL_TEXTURE_2D, probe_texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             pixels_in);
                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                       probe_texture, 0);
                cap.readback_status = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
                    GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
                    return fail(47, "unsupported probe framebuffer status");
                glBindFramebuffer(GL_FRAMEBUFFER, 0u);
                glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels_out);
                if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                    memcmp(pixels_out, pixels_bgra, sizeof(pixels_bgra)) != 0 ||
                    cap.gpu_read_calls != reads || cap.read_fbo != 0u || cap.draw_fbo != 0u ||
                    cap.pack_alignment != 4)
                    return fail(48, "unrenderable format probe reads CPU upload");
                cap.readback_status = 0u;
                glDeleteTextures(1, &probe_texture);
            }
            {
                GLuint float_texture = 0u;
                unsigned char float_output[8] = {0};
                const unsigned char expected_float_output[8] = {255u, 64u,  128u, 191u,
                                                                128u, 255u, 0u,   255u};
                const unsigned char float_upload[8] = {128u, 64u, 255u, 191u, 0u, 255u, 128u, 255u};
                unsigned reads;
                glGenTextures(1, &float_texture);
                glBindTexture(GL_TEXTURE_2D, float_texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             float_upload);
                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                       float_texture, 0);
                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                    return fail(49, "floating color attachment framebuffer complete");
                glBindFramebuffer(GL_FRAMEBUFFER, 0u);
                cap.require_float_readback = 1;
                cap.native_read_type = GL_FLOAT;
                native.glesGetIntegerv = fake_get_integer;
                reads = cap.gpu_read_calls;
                glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, float_output);
                if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                    memcmp(float_output, expected_float_output, 8u) != 0 ||
                    cap.gpu_read_calls != reads + 2u || cap.read_fbo != 0u || cap.draw_fbo != 0u ||
                    cap.pack_alignment != 4)
                    return fail(50, "native floating-color readback fallback");
                cap.native_read_type = GL_HALF_FLOAT;
                memset(float_output, 0u, sizeof(float_output));
                reads = cap.gpu_read_calls;
                glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, float_output);
                if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                    memcmp(float_output, expected_float_output, sizeof(float_output)) != 0 ||
                    cap.gpu_read_calls != reads + 2u)
                    return fail(52, "native half-float color readback fallback");
                cap.native_read_type = GL_UNSIGNED_INT;
                reads = cap.gpu_read_calls;
                memset(float_output, 0x5au, sizeof(float_output));
                glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, float_output);
                if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION ||
                    cap.gpu_read_calls != reads + 1u || float_output[0] != 0x5au ||
                    cap.read_fbo != 0u || cap.draw_fbo != 0u || cap.pack_alignment != 4)
                    return fail(51, "unsupported native readback cannot use stale shadow");
                cap.require_float_readback = 0;
                native.glesGetIntegerv = NULL;
                glDeleteTextures(1, &float_texture);
            }
            native.glesReadPixels = NULL;
            native.glesGetError = NULL;
        }
        glDeleteTextures(2, textures);
    }

    {
        unsigned old_binds, old_cubes;
        mgl_texture_mark_all_bindings_dirty(&ctx);
        mgl_texture_realize_bindings(&ctx);
        old_binds = cap.native_texture_binds;
        mgl_texture_mark_unit_dirty(&ctx, 0u);
        mgl_texture_realize_bindings(&ctx);
        mgl_texture_bind_native_unit(&ctx, GL_TEXTURE_CUBE_MAP, 888u, 0u);
        old_binds = cap.native_texture_binds;
        old_cubes = cap.native_cube_binds;
        mgl_texture_bind_native_unit(&ctx, GL_TEXTURE_CUBE_MAP, 888u, 0u);
        mgl_texture_realize_bindings(&ctx);
        if (cap.native_texture_binds <= old_binds || cap.native_cube_binds <= old_cubes)
            return fail(202, "native texture binding restoration");
    }

    {
        struct mgl_context shared_context;
        GLuint i;
        memset(&shared_context, 0, sizeof(shared_context));
        shared_context.share_group = ctx.share_group;
        for (i = 0u; i < 2048u; ++i) {
            struct mgl_texture *entry = mgl_texture_ensure(&ctx, 100000u + i, GL_TEXTURE_2D, 0);
            if (entry == NULL || mgl_texture_find(&ctx, 100000u + i) != entry)
                return fail(204, "texture index insertion");
        }
        for (i = 0u; i < 2048u; ++i) {
            struct mgl_texture *entry = mgl_texture_find(&ctx, 100000u + i);
            if (entry == NULL || entry->name != 100000u + i)
                return fail(205, "texture index lookup");
        }
        if (mgl_texture_find(&ctx, 99999u) != NULL ||
            mgl_texture_find(&shared_context, 101023u) != mgl_texture_find(&ctx, 101023u))
            return fail(206, "texture index shared context lookup");
        for (i = 0u; i < 2048u; i += 2u)
            mgl_texture_remove(&ctx, 100000u + i);
        for (i = 0u; i < 2048u; ++i) {
            struct mgl_texture *entry = mgl_texture_find(&ctx, 100000u + i);
            if ((i % 2u == 0u && entry != NULL) ||
                (i % 2u == 1u && (entry == NULL || entry->name != 100000u + i)))
                return fail(207, "texture index deletion");
        }
        if (mgl_texture_ensure(&ctx, 100000u, GL_TEXTURE_CUBE_MAP, 0) == NULL ||
            mgl_texture_find(&ctx, 100000u)->target != GL_TEXTURE_CUBE_MAP)
            return fail(208, "texture name reuse after deletion");
        for (i = 0u; i < 2048u; ++i)
            mgl_texture_remove(&ctx, 100000u + i);
    }

    /* Desktop BGR input must be accepted, tightly unpacked, and converted
     * before GLES sees the bytes; DSA/extension tight entry points must not
     * skip the same normalization. */
    {
        const unsigned char padded_bgr[16] = {1u, 2u, 3u, 4u,  5u,  6u,  0xfdu, 0xfdu,
                                              7u, 8u, 9u, 10u, 11u, 12u, 0xfdu, 0xfdu};
        const unsigned char rgb[12] = {3u, 2u, 1u, 6u, 5u, 4u, 9u, 8u, 7u, 12u, 11u, 10u};
        const unsigned char replacement_bgr[3] = {23u, 45u, 67u};
        const unsigned char replacement_rgb[3] = {67u, 45u, 23u};
        const unsigned char bgra[4] = {13u, 29u, 57u, 71u};
        const unsigned char rgba[4] = {57u, 29u, 13u, 71u};
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGB8, 2, 2, 0, GL_BGR, GL_UNSIGNED_BYTE,
                     padded_bgr);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL ||
            obj->levels[0].format != GL_RGB || obj->levels[0].type != GL_UNSIGNED_BYTE ||
            cap.upload_format != GL_RGB || cap.upload_type != GL_UNSIGNED_BYTE ||
            cap.upload_size != sizeof(rgb) || memcmp(cap.upload, rgb, sizeof(rgb)) != 0 ||
            memcmp(obj->levels[0].data, rgb, sizeof(rgb)) != 0)
            return fail(209, "BGR texture upload and row padding");
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 0, 1, 1, GL_BGR, GL_UNSIGNED_BYTE, replacement_bgr);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGB ||
            cap.upload_size != sizeof(replacement_rgb) ||
            memcmp(cap.upload, replacement_rgb, sizeof(replacement_rgb)) != 0 ||
            memcmp(obj->levels[0].data + 3u, replacement_rgb, sizeof(replacement_rgb)) != 0)
            return fail(210, "BGR texture subimage");
        if (!mgl_texture_apply_image_2d_tight(&ctx, GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_BGRA,
                                              GL_UNSIGNED_BYTE, bgra, sizeof(bgra)) ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGBA ||
            cap.upload_type != GL_UNSIGNED_BYTE || cap.upload_size != sizeof(rgba) ||
            memcmp(cap.upload, rgba, sizeof(rgba)) != 0 || obj->levels[0].format != GL_RGBA ||
            obj->levels[0].type != GL_UNSIGNED_BYTE ||
            memcmp(obj->levels[0].data, rgba, sizeof(rgba)) != 0)
            return fail(211, "tight DSA/image BGRA conversion");
        if (!mgl_texture_apply_sub_image_2d_tight(&ctx, GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_BGRA,
                                                  GL_UNSIGNED_BYTE, bgra, sizeof(bgra)) ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGBA ||
            cap.upload_type != GL_UNSIGNED_BYTE || cap.upload_size != sizeof(rgba) ||
            memcmp(cap.upload, rgba, sizeof(rgba)) != 0)
            return fail(212, "tight DSA/subimage BGRA conversion");
        {
            const uint16_t bgr16[6] = {0u, 32768u, 65535u, 257u, 1028u, 3855u};
            const uint16_t rgb16[6] = {65535u, 32768u, 0u, 3855u, 1028u, 257u};
            const uint16_t patch16[3] = {0u, 65535u, 32768u};
            const unsigned char gpu8[6] = {255u, 128u, 0u, 15u, 4u, 1u};
            const unsigned char sub8[3] = {128u, 255u, 0u};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 1, 0, GL_BGR, GL_UNSIGNED_SHORT, bgr16);
            obj = mgl_bound_texture_2d(&ctx);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGB ||
                cap.upload_type != GL_UNSIGNED_BYTE || cap.upload_size != sizeof(gpu8) ||
                memcmp(cap.upload, gpu8, sizeof(gpu8)) != 0 ||
                obj->levels[0].type != GL_UNSIGNED_SHORT ||
                memcmp(obj->levels[0].data, rgb16, sizeof(rgb16)) != 0)
                return fail(213, "2D unsigned short BGR upload");
            glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 0, 1, 1, GL_BGR, GL_UNSIGNED_SHORT, patch16);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGB ||
                cap.upload_type != GL_UNSIGNED_BYTE || cap.upload_size != sizeof(sub8) ||
                memcmp(cap.upload, sub8, sizeof(sub8)) != 0 ||
                memcmp(obj->levels[0].data + 6u, (uint16_t[]){32768u, 65535u, 0u}, 6u) != 0)
                return fail(214, "2D unsigned short BGR subimage");
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    /* Desktop GL accepts RGB source pixels for RGBA storage and the inverse.
     * GLES 3 requires the sized storage's matching base upload format. */
    {
        const unsigned char rgb[6] = {12u, 34u, 56u, 78u, 90u, 123u};
        const unsigned char expected_rgba[8] = {12u, 34u, 56u, 255u, 78u, 90u, 123u, 255u};
        const unsigned char rgba[8] = {21u, 42u, 63u, 84u, 105u, 126u, 147u, 168u};
        const unsigned char expected_rgb[6] = {21u, 42u, 63u, 105u, 126u, 147u};
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != GL_RGBA8 ||
            cap.upload_format != GL_RGBA || cap.upload_size != sizeof(expected_rgba) ||
            memcmp(cap.upload, expected_rgba, sizeof(expected_rgba)) != 0 ||
            obj->levels[0].format != GL_RGB || obj->levels[0].data_size != sizeof(rgb) ||
            memcmp(obj->levels[0].data, rgb, sizeof(rgb)) != 0)
            return fail(213, "RGB source promoted to GLES RGBA8 without corrupting CPU shadow");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGBA ||
            cap.upload_size != sizeof(expected_rgba) ||
            memcmp(cap.upload, expected_rgba, sizeof(expected_rgba)) != 0)
            return fail(214, "RGB subimage promoted for GLES RGBA8 storage");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != GL_RGB8 ||
            cap.upload_format != GL_RGB || cap.upload_size != sizeof(expected_rgb) ||
            memcmp(cap.upload, expected_rgb, sizeof(expected_rgb)) != 0 ||
            obj->levels[0].format != GL_RGBA || obj->levels[0].data_size != sizeof(rgba) ||
            memcmp(obj->levels[0].data, rgba, sizeof(rgba)) != 0)
            return fail(215, "RGBA source reduced for GLES RGB8 storage without changing shadow");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGB ||
            cap.upload_size != sizeof(expected_rgb) ||
            memcmp(cap.upload, expected_rgb, sizeof(expected_rgb)) != 0)
            return fail(216, "RGBA subimage reduced for GLES RGB8 storage");
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    /* Native normalized-16 storage uses U16-expanded source pixels. */
    {
        const unsigned char rgba8[4] = {0u, 64u, 128u, 255u};
        const uint16_t rgba16[4] = {0u, 16448u, 32896u, 65535u};
        cap.native_norm16 = 1;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba8);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != GL_RGBA16 ||
            cap.upload_format != GL_RGBA || cap.upload_type != GL_UNSIGNED_SHORT ||
            cap.upload_size != sizeof(rgba16) || memcmp(cap.upload, rgba16, sizeof(rgba16)) != 0 ||
            obj->levels[0].type != GL_UNSIGNED_BYTE ||
            memcmp(obj->levels[0].data, rgba8, sizeof(rgba8)) != 0)
            return fail(217, "norm16 extension native upload expands U8 channels to U16");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba8);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_type != GL_UNSIGNED_SHORT ||
            cap.upload_size != sizeof(rgba16) || memcmp(cap.upload, rgba16, sizeof(rgba16)) != 0)
            return fail(218, "norm16 extension subimage expands U8 channels to U16");
        cap.native_norm16 = 0;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    {
        const unsigned char base_rgb[6] = {1u, 2u, 3u, 4u, 5u, 6u};
        const unsigned char patch_rgba[8] = {17u, 34u, 51u, 68u, 85u, 102u, 119u, 136u};
        const unsigned char rgb_shadow[6] = {17u, 34u, 51u, 85u, 102u, 119u};
        const unsigned char la_patch[2] = {90u, 140u};
        const unsigned char rgba_from_la[4] = {90u, 90u, 90u, 140u};
        const uint16_t rgba16_patch[4] = {0u, 32896u, 65535u, 16448u};
        const unsigned char rgba8_patch[4] = {0u, 128u, 255u, 64u};
        const unsigned char init_rgba[4] = {1u, 2u, 3u, 4u};
        unsigned char saved[4];
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 2, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, base_rgb);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, patch_rgba);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGB ||
            cap.upload_type != GL_UNSIGNED_BYTE || cap.upload_size != sizeof(rgb_shadow) ||
            memcmp(cap.upload, rgb_shadow, sizeof(rgb_shadow)) != 0 ||
            obj->levels[0].data_size != sizeof(rgb_shadow) ||
            memcmp(obj->levels[0].data, rgb_shadow, sizeof(rgb_shadow)) != 0)
            return fail(220, "RGBA subimage into RGB source-defined storage");

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, patch_rgba);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGB, GL_UNSIGNED_BYTE, base_rgb);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGBA ||
            cap.upload_size != sizeof(base_rgb) + 2u ||
            memcmp(obj->levels[0].data, (const unsigned char[]){1u, 2u, 3u, 255u, 4u, 5u, 6u, 255u},
                   8u) != 0 ||
            memcmp(cap.upload, obj->levels[0].data, 8u) != 0)
            return fail(221, "RGB subimage adds opaque alpha to RGBA shadow");

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, base_rgb);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, patch_rgba);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGBA ||
            cap.upload_size != 4u || obj->levels[0].format != GL_RGBA ||
            obj->levels[0].type != GL_UNSIGNED_BYTE ||
            obj->levels[0].data_size != sizeof(patch_rgba) ||
            memcmp(obj->levels[0].data,
                   (const unsigned char[]){17u, 34u, 51u, 68u, 4u, 5u, 6u, 255u}, 8u) != 0 ||
            memcmp(cap.upload, patch_rgba, 4u) != 0)
            return fail(222, "RGBA subimage preserves alpha and untouched RGB pixels");

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, init_rgba);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE,
                        la_patch);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGBA ||
            cap.upload_size != sizeof(rgba_from_la) ||
            memcmp(cap.upload, rgba_from_la, sizeof(rgba_from_la)) != 0 ||
            memcmp(obj->levels[0].data, rgba_from_la, sizeof(rgba_from_la)) != 0)
            return fail(223, "luminance-alpha source expands to RGBA channels");

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT, rgba16_patch);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RGBA ||
            cap.upload_type != GL_UNSIGNED_BYTE || cap.upload_size != sizeof(rgba8_patch) ||
            memcmp(cap.upload, rgba8_patch, sizeof(rgba8_patch)) != 0 ||
            memcmp(obj->levels[0].data, rgba8_patch, sizeof(rgba8_patch)) != 0)
            return fail(224, "unsigned-short subimage converted to 8-bit storage");

        memcpy(saved, obj->levels[0].data, sizeof(saved));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE,
                        (const unsigned char[]){42u});
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION ||
            memcmp(saved, obj->levels[0].data, sizeof(saved)) != 0)
            return fail(225, "depth data cannot update a color texture");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, init_rgba);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE ||
            memcmp(saved, obj->levels[0].data, sizeof(saved)) != 0)
            return fail(226, "out-of-bounds color subimage still rejected");
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    {
        const unsigned char original[4] = {64u, 128u, 192u, 128u};
        const unsigned char scaled[4] = {128u, 192u, 96u, 0u};
        const unsigned char patch[4] = {64u, 128u, 192u, 255u};
        const unsigned char mapped[4] = {255u, 255u, 255u, 255u};
        const GLfloat invert[2] = {1.0f, 0.0f};
        const GLfloat identity[2] = {0.0f, 1.0f};
        const uint16_t rgba16[4] = {0u, 32768u, 65535u, 32768u};
        const uint16_t transfer16[4] = {0u, 65535u, 65535u, 32768u};
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelTransferf(GL_RED_SCALE, 2.0f);
        glPixelTransferf(GL_GREEN_BIAS, 0.25f);
        glPixelTransferf(GL_BLUE_SCALE, 0.5f);
        glPixelTransferf(GL_ALPHA_SCALE, 0.0f);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, original);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_size != sizeof(scaled) ||
            memcmp(cap.upload, scaled, sizeof(scaled)) ||
            memcmp(obj->levels[0].data, scaled, sizeof(scaled)))
            return fail(230, "texture upload applies per-channel scale and bias");
        glPixelTransferf(GL_RED_SCALE, 1.0f);
        glPixelTransferf(GL_GREEN_BIAS, 0.0f);
        glPixelTransferf(GL_BLUE_SCALE, 1.0f);
        glPixelTransferf(GL_ALPHA_SCALE, 1.0f);
        glPixelMapfv(GL_PIXEL_MAP_R_TO_R, 2, invert);
        glPixelMapfv(GL_PIXEL_MAP_G_TO_G, 2, identity);
        glPixelMapfv(GL_PIXEL_MAP_B_TO_B, 2, identity);
        glPixelMapfv(GL_PIXEL_MAP_A_TO_A, 2, identity);
        glPixelTransferi(GL_MAP_COLOR, GL_TRUE);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, patch);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_size != sizeof(mapped) ||
            memcmp(cap.upload, mapped, sizeof(mapped)) ||
            memcmp(obj->levels[0].data, mapped, sizeof(mapped)))
            return fail(231, "texture subimage applies component color maps");
        glPixelTransferf(GL_RED_SCALE, 2.0f);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, patch);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload[0] != 0u ||
            obj->levels[0].data[0] != 0u)
            return fail(234, "scale/bias occurs before color-map lookup");
        glPixelTransferf(GL_RED_SCALE, 1.0f);
        glPixelTransferi(GL_MAP_COLOR, GL_FALSE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, original);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(cap.upload, original, sizeof(original)))
            return fail(232, "default pixel-transfer fast path preserves bytes");
        glPixelTransferf(GL_GREEN_SCALE, 2.0f);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, 1, 1, 0, GL_RGBA, GL_UNSIGNED_SHORT, rgba16);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            obj->levels[0].type != GL_UNSIGNED_SHORT ||
            obj->levels[0].data_size != sizeof(transfer16) ||
            memcmp(obj->levels[0].data, transfer16, sizeof(transfer16)))
            return fail(233, "16-bit pixel transfer retains channel precision");
        glPixelTransferf(GL_GREEN_SCALE, 1.0f);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    {
        const float rgb[3] = {0.25f, 0.5f, 2.0f};
        const float alpha[1] = {0.75f};
        const float la[2] = {0.5f, 0.25f};
        const uint32_t depth[1] = {UINT32_C(0x80000000)};
        float gpu[4] = {0};
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F_ARB, 1, 1, 0, GL_RGB, GL_FLOAT, rgb);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != GL_RGBA16F ||
            cap.upload_format != GL_RGBA || cap.upload_type != GL_FLOAT ||
            cap.upload_size != sizeof(gpu) || obj->levels[0].internal_format != GL_RGB32F_ARB ||
            obj->levels[0].data_size != sizeof(rgb) ||
            memcmp(obj->levels[0].data, rgb, sizeof(rgb)) != 0)
            return fail(260, "RGB32F GLES backing");
        memcpy(gpu, cap.upload, sizeof(gpu));
        if (gpu[0] != rgb[0] || gpu[1] != rgb[1] || gpu[2] != rgb[2] || gpu[3] != 1.0f)
            return fail(261, "RGB32F alpha expansion");
        {
            const unsigned char rgb8[3] = {0u, 128u, 255u};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F_ARB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb8);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != GL_RGBA16F ||
                cap.upload_format != GL_RGBA || cap.upload_type != GL_HALF_FLOAT ||
                cap.upload_size != 8u)
                return fail(268, "RGB32F byte source expansion");
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA16F_ARB, 1, 1, 0, GL_ALPHA, GL_FLOAT, alpha);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != GL_R16F ||
            cap.upload_format != GL_RED || cap.upload_type != GL_HALF_FLOAT ||
            cap.upload_size != 2u || cap.swizzle[0] != GL_ZERO || cap.swizzle[1] != GL_ZERO ||
            cap.swizzle[2] != GL_ZERO || cap.swizzle[3] != GL_RED)
            return fail(262, "float alpha storage and swizzle");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA32F_ARB, 1, 1, 0, GL_LUMINANCE_ALPHA,
                     GL_FLOAT, la);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_internal != GL_RG16F ||
            cap.upload_format != GL_RG || cap.upload_type != GL_FLOAT ||
            cap.upload_size != sizeof(la) || cap.swizzle[0] != GL_RED || cap.swizzle[1] != GL_RED ||
            cap.swizzle[2] != GL_RED || cap.swizzle[3] != GL_GREEN)
            return fail(263, "float luminance alpha storage and swizzle");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_LUMINANCE_ALPHA, GL_FLOAT, la);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_format != GL_RG ||
            cap.upload_type != GL_FLOAT || cap.upload_size != sizeof(la))
            return fail(264, "float luminance alpha subimage");
        {
            const unsigned char red_texel = 128u;
            GLint previous_binding = 0;
            GLuint named_red_texture = 0u;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_binding);
            glGenTextures(1, &named_red_texture);
            glBindTexture(GL_TEXTURE_2D, named_red_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE,
                         &red_texel);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
                return fail(285, "single-channel texture allocation");
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE, &value);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 8)
                return fail(286, "single-channel texture component size");
            glGetTextureLevelParameteriv(named_red_texture, 0, GL_TEXTURE_GREEN_TYPE, &value);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != GL_NONE)
                return fail(287, "absent named texture component type");
            glBindTexture(GL_TEXTURE_2D, (GLuint)previous_binding);
            glDeleteTextures(1, &named_red_texture);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, 1, 1, 0, GL_DEPTH_COMPONENT,
                     GL_UNSIGNED_INT, depth);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.upload_internal != GL_DEPTH_COMPONENT32F ||
            cap.upload_format != GL_DEPTH_COMPONENT || cap.upload_type != GL_FLOAT ||
            cap.upload_size != sizeof(float))
            return fail(265, "depth32 float backing");
        memcpy(gpu, cap.upload, sizeof(float));
        if (gpu[0] < 0.4999f || gpu[0] > 0.5001f)
            return fail(266, "depth32 normalized upload");
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_DEPTH_SIZE, &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 32)
            return fail(267, "depth32 size query");
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_DEPTH_TYPE, &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != GL_FLOAT)
            return fail(273, "depth texture component type");
        {
            GLuint shared_exponent_texel = 0u;
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB9_E5, 1, 1, 0, GL_RGB,
                         GL_UNSIGNED_INT_5_9_9_9_REV, &shared_exponent_texel);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
                return fail(274, "shared exponent texture allocation");
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_SHARED_SIZE, &value);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 5)
                return fail(275, "shared exponent size query");
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_RED_TYPE, &value);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != GL_FLOAT)
                return fail(276, "shared exponent component type");
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    free(cap.upload);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL texture semantic unit test: PASS");
    return 0;
}

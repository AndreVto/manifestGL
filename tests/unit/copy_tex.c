#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    GLuint next_buffer;
    unsigned read_calls;
    unsigned gpu_copy_image_calls;
    unsigned gpu_copy_sub_calls;
    unsigned char seed;
    GLuint last_pack_buffer;
    GLint pack_alignment;
    unsigned tex_sub_3d_calls;
    GLenum last_tex_sub_3d_target;
    GLint last_tex_sub_3d_zoffset;
    GLsizei last_tex_sub_3d_depth;
    unsigned char last_tex_sub_3d_pixel[4];
    int precise_source;
    GLenum precise_type;
    int norm16_available;
} cap;

static GLenum fake_get_error(void) {
    return GL_NO_ERROR;
}

static const GLubyte *fake_get_string(GLenum name) {
    return name == GL_EXTENSIONS && cap.norm16_available ? (const GLubyte *)"GL_EXT_texture_norm16"
                                                         : (const GLubyte *)"";
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

static void fake_active_texture(GLenum texture) {
    (void)texture;
}

static void fake_tex_image(GLenum target, GLint level, GLint internalformat, GLsizei width,
                           GLsizei height, GLint border, GLenum format, GLenum type,
                           const void *data) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)border;
    (void)format;
    (void)type;
    (void)data;
}

static void fake_copy_image(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
                            GLsizei width, GLsizei height, GLint border) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)border;
    ++cap.gpu_copy_image_calls;
}

static void fake_copy_sub(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x,
                          GLint y, GLsizei width, GLsizei height) {
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    ++cap.gpu_copy_sub_calls;
}

static void fake_gen_framebuffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_buffer;
}

static void fake_delete_framebuffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_attach_color(GLenum target, GLenum attachment, GLenum textarget, GLuint tex,
                              GLint level) {
    (void)target;
    (void)attachment;
    (void)textarget;
    (void)tex;
    (void)level;
}

static GLenum fake_framebuffer_status(GLenum target) {
    (void)target;
    return GL_FRAMEBUFFER_COMPLETE;
}

static void fake_tex_sub(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                         GLsizei height, GLenum format, GLenum type, const void *data) {
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)width;
    (void)height;
    (void)format;
    (void)type;
    (void)data;
}

static void fake_tex_image_3d(GLenum target, GLint level, GLint internalformat, GLsizei width,
                              GLsizei height, GLsizei depth, GLint border, GLenum format,
                              GLenum type, const void *data) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)depth;
    (void)border;
    (void)format;
    (void)type;
    (void)data;
}

static void fake_tex_sub_3d(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                            GLsizei width, GLsizei height, GLsizei depth, GLenum format,
                            GLenum type, const void *data) {
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)width;
    (void)height;
    (void)format;
    (void)type;
    ++cap.tex_sub_3d_calls;
    cap.last_tex_sub_3d_target = target;
    cap.last_tex_sub_3d_zoffset = zoffset;
    cap.last_tex_sub_3d_depth = depth;
    /* The RGB copy path supplies three bytes,
     * not an RGBA pixel. The native spy must respect the supplied format. */
    memset(cap.last_tex_sub_3d_pixel, 0, sizeof(cap.last_tex_sub_3d_pixel));
    if (data != NULL) {
        size_t pixel_bytes = format == GL_RGB ? 3u : 4u;
        memcpy(cap.last_tex_sub_3d_pixel, data, pixel_bytes);
    }
}

static void fake_pixel_store(GLenum pname, GLint param) {
    if (pname == GL_PACK_ALIGNMENT)
        cap.pack_alignment = param;
}

static void fake_bind_framebuffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
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
    cap.precise_type = type;
    if (cap.precise_source && type == GL_UNSIGNED_SHORT) {
        const uint16_t values[4] = {1u, 257u, 32768u, 65535u};
        memcpy(pixels, values, sizeof(values));
        return;
    }
    if (cap.precise_source == 2 && type == GL_UNSIGNED_INT_2_10_10_10_REV) {
        const uint32_t packed = 1u | (257u << 10u) | (777u << 20u) | (2u << 30u);
        memcpy(pixels, &packed, sizeof(packed));
        return;
    }
    if (cap.precise_source == 3 && type == GL_FLOAT) {
        const float values[4] = {0.1234567f, 0.2345678f, 0.3456789f, 0.4567891f};
        memcpy(pixels, values, sizeof(values));
        return;
    }
    for (i = 0u; i < count; ++i)
        dst[i] = (unsigned char)(cap.seed + (unsigned char)i);
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
    if (target == GL_PIXEL_PACK_BUFFER)
        cap.last_pack_buffer = name;
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

static int fail(int n, const char *msg) {
    fprintf(stderr, "CopyTex failure %d: %s\n", n, msg);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj;
    struct mgl_buffer *pack;
    GLuint texture = 0u, pbo = 0u, list_texture = 0u, list = 0u;
    GLuint texture3d = 0u, array_texture = 0u, list_texture3d = 0u, list3d = 0u;
    struct mgl_texture *attachment;
    struct mgl_framebuffer *read_fb;
    unsigned char zero_rgba_3d[32] = {0};
    unsigned char zero_rgb_array[12] = {0};
    const unsigned char expected_rgba[16] = {10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u,
                                             18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u};
    const unsigned char expected_rgb[12] = {10u, 11u, 12u, 14u, 15u, 16u,
                                            18u, 19u, 20u, 22u, 23u, 24u};
    const unsigned char replay_rgba[4] = {80u, 81u, 82u, 83u};

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGetError = fake_get_error;
    native.glesGetString = fake_get_string;
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexImage2D = fake_tex_image;
    native.glesTexSubImage2D = fake_tex_sub;
    native.glesTexImage3D = fake_tex_image_3d;
    native.glesTexSubImage3D = fake_tex_sub_3d;
    native.glesPixelStorei = fake_pixel_store;
    native.glesBindFramebuffer = fake_bind_framebuffer;
    native.glesReadPixels = fake_read_pixels;
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
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    cap.seed = 10u;
    cap.norm16_available = 1;
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 3, 2, 2, 0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.read_calls != 1u)
        return fail(2, "RGBA copy read");
    obj = mgl_bound_texture_2d(&ctx);
    if (obj == NULL || !obj->levels[0].defined || obj->levels[0].compressed ||
        obj->levels[0].data_size != sizeof(expected_rgba) ||
        memcmp(obj->levels[0].data, expected_rgba, sizeof(expected_rgba)) != 0)
        return fail(3, "RGBA canonical CopyTexImage");

    cap.seed = 10u;
    glCopyTexImage2D(GL_TEXTURE_2D, 1, GL_RGB8, 0, 0, 2, 2, 0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        obj->levels[1].data_size != sizeof(expected_rgb) ||
        memcmp(obj->levels[1].data, expected_rgb, sizeof(expected_rgb)) != 0)
        return fail(4, "RGB conversion CopyTexImage");

    cap.seed = 40u;
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 0, 0, 1, 1);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(obj->levels[0].data + 12u, (const unsigned char[]){40u, 41u, 42u, 43u}, 4u) != 0)
        return fail(5, "CopyTexSubImage canonical update");

    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, 64, NULL, GL_STATIC_DRAW);
    pack = mgl_buffer_find(&ctx, pbo);
    if (pack == NULL || pack->native_name == 0u)
        return fail(6, "pack buffer realization");
    cap.seed = 60u;
    glCopyTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, 0, 0, 1, 1, 0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.last_pack_buffer != pack->native_name ||
        cap.pack_alignment != ctx.pack_alignment)
        return fail(7, "CopyTex internal readback did not restore application pack state");
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0u);

    glGenTextures(1, &list_texture);
    list = glGenLists(1);
    cap.seed = 70u;
    glNewList(list, GL_COMPILE);
    glBindTexture(GL_TEXTURE_2D, list_texture);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 1, 1, 0);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.read_calls != 4u)
        return fail(8, "GL_COMPILE CopyTex executed framebuffer read");
    cap.seed = 80u;
    glCallList(list);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.read_calls != 5u)
        return fail(9, "CopyTex list replay read");
    obj = mgl_texture_find(&ctx, list_texture);
    if (obj == NULL || !obj->levels[0].defined ||
        memcmp(obj->levels[0].data, replay_rgba, sizeof(replay_rgba)) != 0)
        return fail(10, "CopyTex list must observe framebuffer at replay time");

    glGenTextures(1, &texture3d);
    glBindTexture(GL_TEXTURE_3D, texture3d);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, zero_rgba_3d);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(11, "3D texture setup");
    cap.seed = 100u;
    glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 1, 0, 1, 0, 0, 1, 1);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.read_calls != 6u ||
        cap.tex_sub_3d_calls != 1u)
        return fail(12, "3D copy execution");
    obj = mgl_texture_find(&ctx, texture3d);
    if (obj == NULL ||
        memcmp(obj->levels[0].data + 20u, (const unsigned char[]){100u, 101u, 102u, 103u}, 4u) != 0)
        return fail(13, "3D canonical slice update");
    if (cap.last_tex_sub_3d_target != GL_TEXTURE_3D || cap.last_tex_sub_3d_zoffset != 1 ||
        cap.last_tex_sub_3d_depth != 1 ||
        memcmp(cap.last_tex_sub_3d_pixel, (const unsigned char[]){100u, 101u, 102u, 103u}, 4u) != 0)
        return fail(14, "3D native subimage lowering");

    glGenTextures(1, &array_texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, array_texture);
    /* The 12-byte RGB fixture has tight rows;
     * default UNPACK_ALIGNMENT=4 would read beyond its second slice. */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, 2, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 zero_rgb_array);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    cap.seed = 120u;
    glCopyTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, 0, 0, 1, 1);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.read_calls != 7u ||
        cap.tex_sub_3d_calls != 2u)
        return fail(15, "2D-array copy execution");
    obj = mgl_texture_find(&ctx, array_texture);
    if (obj == NULL ||
        memcmp(obj->levels[0].data + 6u, (const unsigned char[]){120u, 121u, 122u}, 3u) != 0)
        return fail(16, "2D-array canonical layer update");

    glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 2, 0, 0, 1, 1);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE || cap.read_calls != 7u)
        return fail(17, "3D zoffset bounds");
    glCopyTexSubImage3D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 0, 1, 1);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM || cap.read_calls != 7u)
        return fail(18, "3D target validation");

    glGenTextures(1, &list_texture3d);
    glBindTexture(GL_TEXTURE_3D, list_texture3d);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, zero_rgba_3d);
    list3d = glGenLists(1);
    cap.seed = 130u;
    glNewList(list3d, GL_COMPILE);
    glBindTexture(GL_TEXTURE_3D, list_texture3d);
    glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 1, 1);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.read_calls != 7u)
        return fail(19, "GL_COMPILE CopyTexSubImage3D executed framebuffer read");
    cap.seed = 140u;
    glCallList(list3d);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.read_calls != 8u ||
        cap.tex_sub_3d_calls != 3u)
        return fail(20, "CopyTexSubImage3D list replay read");
    obj = mgl_texture_find(&ctx, list_texture3d);
    if (obj == NULL ||
        memcmp(obj->levels[0].data, (const unsigned char[]){140u, 141u, 142u, 143u}, 4u) != 0)
        return fail(21, "CopyTexSubImage3D list must observe framebuffer at replay time");

    {
        GLuint fast_texture = 0u;
        unsigned char image[16] = {0};
        const unsigned char pixel[4] = {9u, 8u, 7u, 6u};
        unsigned reads_before = cap.read_calls;
        native.glesCopyTexImage2D = fake_copy_image;
        native.glesCopyTexSubImage2D = fake_copy_sub;
        native.glesGenFramebuffers = fake_gen_framebuffers;
        native.glesDeleteFramebuffers = fake_delete_framebuffers;
        native.glesFramebufferTexture2D = fake_attach_color;
        native.glesCheckFramebufferStatus = fake_framebuffer_status;
        glGenTextures(1, &fast_texture);
        glBindTexture(GL_TEXTURE_2D, fast_texture);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 2, 2, 0);
        obj = mgl_bound_texture_2d(&ctx);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.gpu_copy_image_calls != 1u ||
            cap.read_calls != reads_before || obj == NULL || !obj->levels[0].shadow_stale)
            return fail(22, "GPU image copy avoids immediate readback");
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.gpu_copy_sub_calls != 1u ||
            cap.read_calls != reads_before || !obj->levels[0].shadow_stale)
            return fail(23, "GPU subimage copy avoids immediate readback");
        cap.seed = 20u;
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.read_calls != reads_before + 1u ||
            image[0] != 20u || image[15] != 35u)
            return fail(24, "GPU copy readback on demand");
        cap.seed = 30u;
        glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.read_calls != reads_before + 2u ||
            obj->levels[0].shadow_stale || obj->levels[0].data[0] != 30u ||
            memcmp(obj->levels[0].data + 12u, pixel, sizeof(pixel)) != 0)
            return fail(25, "partial upload synchronizes stale shadow");
        {
            const unsigned char rgb_pixel[3] = {17u, 18u, 19u};
            unsigned previous_reads = cap.read_calls;
            glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 0, 0, 2, 2, 0);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.gpu_copy_image_calls != 2u ||
                cap.read_calls != previous_reads || obj->levels[0].data_size != 12u ||
                !obj->levels[0].shadow_stale)
                return fail(27, "RGB GPU image copy");
            cap.seed = 50u;
            glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb_pixel);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                cap.read_calls != previous_reads + 1u || obj->levels[0].shadow_stale ||
                memcmp(obj->levels[0].data, (const unsigned char[]){50u, 51u, 52u}, 3u) != 0 ||
                memcmp(obj->levels[0].data + 9u, rgb_pixel, 3u) != 0)
                return fail(28, "RGB shadow synchronization");
        }
        {
            unsigned previous_reads = cap.read_calls;
            unsigned previous_gpu_copies = cap.gpu_copy_sub_calls;
            glPixelTransferf(GL_RED_SCALE, 0.5f);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
            glPixelTransferf(GL_RED_SCALE, 1.0f);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                cap.read_calls != previous_reads + 1u ||
                cap.gpu_copy_sub_calls != previous_gpu_copies)
                return fail(26, "pixel-transfer copy retains conversion fallback");
        }
        glDeleteTextures(1, &fast_texture);
    }

    {
        GLuint one_d = 0u;
        GLuint one_d_list = 0u;
        unsigned before = cap.read_calls;
        glGenTextures(1, &one_d);
        glBindTexture(GL_TEXTURE_1D, one_d);
        cap.seed = 150u;
        glCopyTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 0, 0, 2, 0);
        obj = mgl_texture_find(&ctx, one_d);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL || !obj->levels[0].defined ||
            obj->levels[0].height != 1 || obj->levels[0].data_size != 8u ||
            cap.read_calls != before + 1u ||
            memcmp(obj->levels[0].data,
                   (const unsigned char[]){150u, 151u, 152u, 153u, 154u, 155u, 156u, 157u},
                   8u) != 0)
            return fail(30, "CopyTexImage1D pixel data");
        cap.seed = 160u;
        glCopyTexSubImage1D(GL_TEXTURE_1D, 0, 1, 0, 0, 1);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.read_calls != before + 2u ||
            memcmp(obj->levels[0].data + 4u, (const unsigned char[]){160u, 161u, 162u, 163u}, 4u) !=
                0)
            return fail(31, "CopyTexSubImage1D pixel data");
        glCopyTexImage1D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 1, 0);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
            return fail(32, "1D target validation");
        one_d_list = glGenLists(1);
        glNewList(one_d_list, GL_COMPILE);
        glCopyTexImage1D(GL_TEXTURE_1D, 1, GL_RGBA8, 0, 0, 1, 0);
        glEndList();
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.read_calls != before + 2u)
            return fail(33, "1D copy list compilation");
        cap.seed = 170u;
        glCallList(one_d_list);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.read_calls != before + 3u ||
            memcmp(obj->levels[1].data, (const unsigned char[]){170u, 171u, 172u, 173u}, 4u) != 0)
            return fail(34, "1D copy list replay");
        glDeleteLists(one_d_list, 1);
        glDeleteTextures(1, &one_d);
    }

    attachment = mgl_texture_ensure(&ctx, 901u, GL_TEXTURE_2D, 0);
    read_fb = mgl_framebuffer_ensure(&ctx, 902u, 0);
    if (attachment == NULL || read_fb == NULL)
        return fail(40, "precision attachment setup");
    attachment->levels[0].defined = GL_TRUE;
    attachment->levels[0].internal_format = GL_RGBA16;
    read_fb->color_texture = attachment->name;
    read_fb->color_texture_target = GL_TEXTURE_2D;
    read_fb->color_texture_level = 0;
    read_fb->read_buffer = GL_COLOR_ATTACHMENT0;
    ctx.read_framebuffer = read_fb->name;
    cap.precise_source = 1;
    glBindTexture(GL_TEXTURE_2D, texture);
    glCopyTexImage2D(GL_TEXTURE_2D, 4, GL_RGBA16, 0, 0, 1, 1, 0);
    obj = mgl_texture_find(&ctx, texture);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL ||
        obj->levels[4].type != GL_FLOAT || obj->levels[4].data_size != 16u ||
        cap.precise_type != GL_UNSIGNED_SHORT ||
        fabsf(((float *)obj->levels[4].data)[1] - 257.0f / 65535.0f) > 0.000001f)
        return fail(41, "CopyTexImage2D preserves normalized 16-bit source");
    glCopyTexSubImage2D(GL_TEXTURE_2D, 4, 0, 0, 0, 0, 1, 1);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj->levels[4].type != GL_FLOAT ||
        fabsf(((float *)obj->levels[4].data)[2] - 32768.0f / 65535.0f) > 0.000001f)
        return fail(42, "CopyTexSubImage2D retains float shadow");
    cap.norm16_available = 0;
    {
        unsigned before = cap.read_calls;
        glCopyTexImage2D(GL_TEXTURE_2D, 5, GL_RGBA16, 0, 0, 1, 1, 0);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || cap.read_calls != before)
            return fail(43, "native extension gate for precision copy");
    }
    cap.norm16_available = 1;
    {
        GLuint precise_1d = 0u;
        glGenTextures(1, &precise_1d);
        glBindTexture(GL_TEXTURE_1D, precise_1d);
        glCopyTexImage1D(GL_TEXTURE_1D, 4, GL_RGBA16, 0, 0, 1, 0);
        obj = mgl_texture_find(&ctx, precise_1d);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj->levels[4].type != GL_FLOAT ||
            fabsf(((float *)obj->levels[4].data)[1] - 257.0f / 65535.0f) > 0.000001f)
            return fail(44, "CopyTexImage1D high precision source");
        glCopyTexSubImage1D(GL_TEXTURE_1D, 4, 0, 0, 0, 1);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj->levels[4].type != GL_FLOAT)
            return fail(45, "CopyTexSubImage1D high precision source");
        glDeleteTextures(1, &precise_1d);
    }
    glBindTexture(GL_TEXTURE_3D, texture3d);
    glTexImage3D(GL_TEXTURE_3D, 3, GL_RGBA16, 1, 1, 1, 0, GL_RGBA, GL_FLOAT,
                 (const float[]){0.0f, 0.0f, 0.0f, 0.0f});
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(46, "float 3D destination");
    glCopyTexSubImage3D(GL_TEXTURE_3D, 3, 0, 0, 0, 0, 0, 1, 1);
    obj = mgl_texture_find(&ctx, texture3d);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj->levels[3].type != GL_FLOAT ||
        fabsf(((float *)obj->levels[3].data)[1] - 257.0f / 65535.0f) > 0.000001f)
        return fail(47, "CopyTexSubImage3D high precision source");
    attachment->levels[0].internal_format = GL_RGB10_A2;
    cap.precise_source = 2;
    glBindTexture(GL_TEXTURE_2D, texture);
    glCopyTexImage2D(GL_TEXTURE_2D, 5, GL_RGB10_A2, 0, 0, 1, 1, 0);
    obj = mgl_texture_find(&ctx, texture);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        cap.precise_type != GL_UNSIGNED_INT_2_10_10_10_REV || obj->levels[5].type != GL_FLOAT ||
        fabsf(((float *)obj->levels[5].data)[2] - 777.0f / 1023.0f) > 0.000001f)
        return fail(48, "CopyTexImage2D packed 10-bit source");
    attachment->levels[0].internal_format = GL_RGBA16F;
    cap.precise_source = 3;
    glCopyTexImage2D(GL_TEXTURE_2D, 6, GL_RGBA16F, 0, 0, 1, 1, 0);
    obj = mgl_texture_find(&ctx, texture);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.precise_type != GL_FLOAT ||
        obj->levels[6].type != GL_FLOAT ||
        fabsf(((float *)obj->levels[6].data)[0] - 0.1234567f) > 0.000001f)
        return fail(49, "CopyTexImage2D floating source");
    attachment->levels[0].internal_format = GL_RGBA16;
    cap.precise_source = 1;
    {
        GLuint narrow = 0u;
        const unsigned char zero_rg[2] = {0u, 0u};
        glGenTextures(1, &narrow);
        glBindTexture(GL_TEXTURE_3D, narrow);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RG16, 1, 1, 1, 0, GL_RG, GL_UNSIGNED_BYTE, zero_rg);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(50, "RG16 narrow destination setup");
        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 1, 1);
        obj = mgl_texture_find(&ctx, narrow);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj->levels[0].type != GL_FLOAT ||
            obj->levels[0].format != GL_RG || obj->levels[0].data_size != 8u ||
            fabsf(((float *)obj->levels[0].data)[1] - 257.0f / 65535.0f) > 0.000001f)
            return fail(51, "RG16 two-channel copy precision");
        glDeleteTextures(1, &narrow);
    }
    cap.precise_source = 0;
    ctx.read_framebuffer = 0u;

    glDeleteLists(list3d, 1);
    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL CopyTex unit test: PASS");
    return 0;
}

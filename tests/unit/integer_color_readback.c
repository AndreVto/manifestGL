#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static GLuint next_texture = 100u;
static GLuint next_framebuffer = 200u;
static GLenum last_format, last_type;
static unsigned read_calls;
static GLint current_pack_row_length, current_pack_skip_pixels;
static int current_signed;

static void gen_textures(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i)
        names[i] = ++next_texture;
}

static void gen_framebuffers(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i)
        names[i] = ++next_framebuffer;
}

static void bind_texture(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void bind_framebuffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void framebuffer_texture(GLenum a, GLenum b, GLenum c, GLuint d, GLint e) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
}

static void texture_storage(GLenum target, GLsizei levels, GLenum internal, GLsizei width,
                            GLsizei height) {
    (void)target;
    (void)levels;
    (void)internal;
    (void)width;
    (void)height;
}

static void pixel_store(GLenum pname, GLint value) {
    if (pname == GL_PACK_ROW_LENGTH)
        current_pack_row_length = value;
    if (pname == GL_PACK_SKIP_PIXELS)
        current_pack_skip_pixels = value;
}

static void bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static GLenum get_error(void) {
    return GL_NO_ERROR;
}

static void read_pixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
                        void *pixels) {
    (void)x;
    (void)y;
    ++read_calls;
    last_format = format;
    last_type = type;
    if (width == 1 && height == 1) {
        if (current_signed) {
            const GLint values[4] = {5, -6, 127, -128};
            memcpy(pixels, values, sizeof(values));
        } else {
            const GLuint values[4] = {11u, 22u, 33u, 300u};
            memcpy(pixels, values, sizeof(values));
        }
    }
}

static int fail(int code, const char *description) {
    fprintf(stderr, "integer color readback %d: %s\n", code, description);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_framebuffer *fb;
    GLuint signed_texture = 0u, unsigned_texture = 0u;
    GLint native_value;
    signed char signed_pixel[4] = {0};
    unsigned char unsigned_pixel[4] = {0};
    unsigned char guarded[16];
    GLint wide[4] = {0};

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenTextures = gen_textures;
    native.glesGenFramebuffers = gen_framebuffers;
    native.glesBindTexture = bind_texture;
    native.glesBindFramebuffer = bind_framebuffer;
    native.glesFramebufferTexture2D = framebuffer_texture;
    native.glesTexStorage2D = texture_storage;
    native.glesReadPixels = read_pixels;
    native.glesPixelStorei = pixel_store;
    native.glesBindBuffer = bind_buffer;
    native.glesGetError = get_error;
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

    glGenTextures(1, &signed_texture);
    glBindTexture(GL_TEXTURE_2D, signed_texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8I, 2, 2);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(2, "signed immutable storage");
    glGenTextures(1, &unsigned_texture);
    glBindTexture(GL_TEXTURE_2D, unsigned_texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8UI, 2, 2);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(3, "unsigned immutable storage");

    fb = mgl_framebuffer_ensure(&ctx, 77u, 1);
    if (fb == NULL || fb->native_name == 0u)
        return fail(4, "framebuffer setup");
    fb->realized = GL_TRUE;
    fb->read_buffer = GL_COLOR_ATTACHMENT0;
    fb->color_texture = signed_texture;
    fb->color_texture_target = GL_TEXTURE_2D;
    fb->color_texture_level = 0;
    fb->color_texture_layer = -1;
    ctx.read_framebuffer = fb->name;
    current_signed = 1;

    glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_BYTE, signed_pixel);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || last_format != GL_RGBA_INTEGER ||
        last_type != GL_INT || signed_pixel[0] != 5 || signed_pixel[1] != -6 ||
        signed_pixel[2] != 127 || signed_pixel[3] != -128)
        return fail(5, "signed integer conversion");

    glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_INT, wide);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || wide[0] != 5 || wide[1] != -6 ||
        wide[2] != 127 || wide[3] != -128)
        return fail(6, "signed 32-bit data");

    memset(guarded, 0xa5, sizeof(guarded));
    glReadnPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_BYTE, 3, guarded + 2);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || guarded[2] != 0xa5)
        return fail(7, "short bounded integer read");
    glReadnPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_BYTE, 4, guarded + 2);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || guarded[1] != 0xa5 || guarded[2] != 5 ||
        guarded[3] != (unsigned char)-6 || guarded[4] != 127 || guarded[5] != 128 ||
        guarded[6] != 0xa5)
        return fail(8, "bounded signed integer conversion and guards");

    fb->color_texture = unsigned_texture;
    current_signed = 0;
    glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, unsigned_pixel);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || last_type != GL_UNSIGNED_INT ||
        unsigned_pixel[0] != 11u || unsigned_pixel[1] != 22u || unsigned_pixel[2] != 33u ||
        unsigned_pixel[3] != 255u)
        return fail(9, "unsigned integer conversion and clamp");

    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, unsigned_pixel);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(10, "normalized read from integer attachment");

    memset(guarded, 0xa5, sizeof(guarded));
    glPixelStorei(GL_PACK_ROW_LENGTH, 3);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 1);
    glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, guarded);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || guarded[0] != 0xa5 || guarded[3] != 0xa5 ||
        guarded[4] != 11u || guarded[5] != 22u || guarded[6] != 33u || guarded[7] != 255u ||
        guarded[8] != 0xa5 || current_pack_row_length != 3 || current_pack_skip_pixels != 1)
        return fail(11, "pixel pack layout and restored GLES parameters");

    glGetIntegerv(GL_PACK_ROW_LENGTH, &native_value);
    if (native_value != 3 || read_calls != 5u)
        return fail(12, "read call count and canonical pack state");
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    {
        uint16_t bgra[4] = {0};
        uint32_t packed = 0u;
        glReadPixels(0, 0, 1, 1, GL_BGRA_INTEGER, GL_UNSIGNED_SHORT, bgra);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || bgra[0] != 33u || bgra[1] != 22u ||
            bgra[2] != 11u || bgra[3] != 300u)
            return fail(13, "unsigned BGRA integer swizzle");
        glReadPixels(0, 0, 1, 1, GL_BLUE_INTEGER, GL_UNSIGNED_INT, &packed);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || packed != 33u)
            return fail(14, "integer blue component");
        glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, &packed);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            packed != (11u | (22u << 10u) | (33u << 20u) | (3u << 30u)))
            return fail(15, "integer packed 10-10-10-2 transfer");
        fb->color_texture = signed_texture;
        current_signed = 1;
        glReadPixels(0, 0, 1, 1, GL_BGR_INTEGER, GL_SHORT, bgra);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || (int16_t)bgra[0] != 127 ||
            (int16_t)bgra[1] != -6 || (int16_t)bgra[2] != 5)
            return fail(16, "signed BGR integer swizzle");
    }
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static GLenum native_component = GL_UNSIGNED_NORMALIZED;
static GLint native_bits[4] = {8, 8, 8, 8};
static GLuint native_read_binding = 37u;
static GLuint virtual_default_native;
static GLenum last_attachment;
static GLenum last_read_type;
static unsigned native_queries;
static unsigned native_reads;
static GLenum native_error;
static int fail_native_query;

static GLenum fake_error(void) {
    GLenum result = native_error;
    native_error = GL_NO_ERROR;
    return result;
}

static const GLubyte *fake_extensions(GLenum name) {
    return name == GL_EXTENSIONS ? (const GLubyte *)"GL_EXT_texture_norm16" : (const GLubyte *)"";
}

static void fake_get_integer(GLenum pname, GLint *params) {
    if (pname == GL_READ_FRAMEBUFFER_BINDING)
        *params = (GLint)native_read_binding;
}

static void fake_bind_framebuffer(GLenum target, GLuint name) {
    if (target == GL_READ_FRAMEBUFFER)
        native_read_binding = name != 0u ? name : virtual_default_native;
}

static void fake_bind_framebuffer_raw(GLenum target, GLuint name) {
    if (target == GL_READ_FRAMEBUFFER)
        native_read_binding = name;
}

static void fake_get_attachment(GLenum target, GLenum attachment, GLenum pname, GLint *value) {
    (void)target;
    last_attachment = attachment;
    ++native_queries;
    if (fail_native_query) {
        native_error = GL_INVALID_ENUM;
        return;
    }
    switch (pname) {
        case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
            *value = (GLint)native_component;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
            *value = native_bits[0];
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
            *value = native_bits[1];
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
            *value = native_bits[2];
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
            *value = native_bits[3];
            break;
        default:
            native_error = GL_INVALID_ENUM;
            break;
    }
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_pixel_store(GLenum pname, GLint value) {
    (void)pname;
    (void)value;
}

static void fake_read_pixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                             GLenum type, void *pixels) {
    (void)x;
    (void)y;
    ++native_reads;
    last_read_type = type;
    if (width != 1 || height != 1) {
        native_error = GL_INVALID_ENUM;
        return;
    }
    if (format == GL_RGBA_INTEGER && type == GL_INT) {
        const int32_t values[4] = {-7, 2, -15, 1};
        memcpy(pixels, values, sizeof(values));
        return;
    }
    if (format == GL_RGBA_INTEGER && type == GL_UNSIGNED_INT) {
        const uint32_t values[4] = {3u, 17u, 257u, 999u};
        memcpy(pixels, values, sizeof(values));
        return;
    }
    if (format != GL_RGBA) {
        native_error = GL_INVALID_ENUM;
        return;
    }
    if (type == GL_BYTE) {
        const int8_t values[4] = {32, -64, 96, 127};
        memcpy(pixels, values, sizeof(values));
    } else if (type == GL_SHORT) {
        const int16_t values[4] = {8192, -16384, 24576, 32767};
        memcpy(pixels, values, sizeof(values));
    } else if (type == GL_FLOAT) {
        const float values[4] = {0.25f, -0.5f, 0.75f, 1.0f};
        memcpy(pixels, values, sizeof(values));
    } else if (type == GL_UNSIGNED_SHORT) {
        const uint16_t values[4] = {1u, 257u, 32768u, 65535u};
        memcpy(pixels, values, sizeof(values));
    } else if (type == GL_UNSIGNED_INT_2_10_10_10_REV) {
        const uint32_t word = 1u | (257u << 10u) | (777u << 20u) | (2u << 30u);
        memcpy(pixels, &word, sizeof(word));
    } else if (type == GL_UNSIGNED_BYTE) {
        const uint8_t values[4] = {7u, 11u, 13u, 255u};
        memcpy(pixels, values, sizeof(values));
    } else {
        native_error = GL_INVALID_ENUM;
    }
}

static int check(int cond, int code, const char *message) {
    if (cond)
        return 0;
    fprintf(stderr, "read attachment coverage %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_framebuffer *fb;
    struct mgl_texture *tex;
    float pixels[4] = {0};
    unsigned char bytes[4] = {0};
    uint16_t words[4] = {0};
    uint32_t packed = 0;
    int32_t signed_values[4] = {0};
    uint32_t unsigned_values[4] = {0};
    GLenum transfer_type;
    void *copy = NULL;
    size_t copy_bytes = 0u;
    unsigned before;
    int result;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetError = fake_error;
    native.glesGetString = fake_extensions;
    native.glesGetIntegerv = fake_get_integer;
    native.glesGetFramebufferAttachmentParameteriv = fake_get_attachment;
    native.glesBindFramebuffer = fake_bind_framebuffer;
    native.glesBindFramebufferRaw = fake_bind_framebuffer_raw;
    native.glesReadPixels = fake_read_pixels;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesPixelStorei = fake_pixel_store;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 64, 64);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    ctx.default_read_buffer = GL_BACK;
    native_component = GL_FLOAT;
    native_bits[0] = native_bits[1] = native_bits[2] = native_bits[3] = 16;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixels);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && native_reads == 1u &&
                       last_attachment == GL_BACK && last_read_type == GL_FLOAT &&
                       fabsf(pixels[0] - .25f) < .00001f && fabsf(pixels[1] + .5f) < .00001f,
                   2, "default floating framebuffer precision");
    if (result)
        return result;

    native_component = GL_UNSIGNED_NORMALIZED;
    native_bits[0] = native_bits[1] = native_bits[2] = 10;
    native_bits[3] = 2;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, &packed);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR &&
                       last_read_type == GL_UNSIGNED_INT_2_10_10_10_REV &&
                       packed == (1u | (257u << 10u) | (777u << 20u) | (2u << 30u)),
                   3, "default 10:10:10:2 precision");
    if (result)
        return result;

    native_bits[0] = native_bits[1] = native_bits[2] = native_bits[3] = 16;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT, words);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR &&
                       last_read_type == GL_UNSIGNED_SHORT && words[0] == 1u && words[1] == 257u,
                   4, "default normalized 16-bit precision");
    if (result)
        return result;

    ctx.canonical_state.clamp_read_color = GL_FALSE;
    native_component = GL_SIGNED_NORMALIZED;
    native_bits[0] = native_bits[1] = native_bits[2] = native_bits[3] = 8;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixels);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && last_read_type == GL_BYTE &&
                       fabsf(pixels[1] + 64.0f / 127.0f) < .00001f,
                   5, "default signed-normalized values remain signed");
    if (result)
        return result;

    native_component = GL_INT;
    glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_INT, signed_values);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && signed_values[0] == -7 &&
                       signed_values[2] == -15,
                   13, "untracked signed integer readback");
    if (result)
        return result;

    native_component = GL_UNSIGNED_INT;
    glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, unsigned_values);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && unsigned_values[0] == 3u &&
                       unsigned_values[2] == 257u,
                   14, "untracked unsigned integer readback");
    if (result)
        return result;

    native_component = GL_UNSIGNED_NORMALIZED;
    native_bits[0] = native_bits[1] = native_bits[2] = native_bits[3] = 8;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, bytes);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR &&
                       last_read_type == GL_UNSIGNED_BYTE && bytes[0] == 7u,
                   6, "default 8-bit still works");
    if (result)
        return result;

    native_component = GL_FLOAT;
    native_bits[0] = native_bits[1] = native_bits[2] = native_bits[3] = 16;
    ctx.native_default_framebuffer = 27u;
    virtual_default_native = 27u;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixels);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR &&
                       last_attachment == GL_COLOR_ATTACHMENT0 && last_read_type == GL_FLOAT &&
                       native_read_binding == 27u,
                   7, "redirected default framebuffer attachment");
    if (result)
        return result;
    ctx.native_default_framebuffer = 0u;
    virtual_default_native = 0u;

    fb = mgl_framebuffer_ensure(&ctx, 39u, 0);
    tex = mgl_texture_ensure(&ctx, 50u, GL_TEXTURE_2D, 0);
    if (fb == NULL || tex == NULL)
        return check(0, 8, "framebuffer and texture creation");
    fb->native_name = 55u;
    fb->read_buffer = GL_COLOR_ATTACHMENT2;
    fb->extra_color_textures[2] = tex->name;
    fb->extra_color_targets[2] = GL_TEXTURE_2D;
    fb->extra_color_levels[2] = 0;
    ctx.read_framebuffer = fb->name;
    native_component = GL_FLOAT;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixels);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR &&
                       last_attachment == GL_COLOR_ATTACHMENT2 && last_read_type == GL_FLOAT,
                   9, "untracked nonzero color attachment");
    if (result)
        return result;

    tex->levels[0].internal_format = GL_RGBA8_SNORM;
    before = native_queries;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixels);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR &&
                       fabsf(pixels[1] + 64.0f / 127.0f) < .00001f && native_queries == before &&
                       last_read_type == GL_BYTE,
                   10, "known signed-normalized format avoids RGBA8 and native query");
    if (result)
        return result;

    tex->levels[0].internal_format = GL_RGBA16_SNORM;
    transfer_type = 0;
    result = check(mgl_read_color_for_copy(&ctx, 0, 0, 1, 1, &copy, &copy_bytes, &transfer_type) &&
                       copy_bytes == 4u * sizeof(float) && transfer_type == GL_FLOAT &&
                       fabsf(((float *)copy)[1] + 16384.0f / 32767.0f) < .00001f &&
                       last_read_type == GL_SHORT,
                   11, "signed normalized copy uses float staging");
    free(copy);
    if (result)
        return result;

    tex->levels[0].internal_format = GL_RGBA8_SNORM;
    ctx.canonical_state.clamp_read_color = GL_FIXED_ONLY;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixels);
    result = check(mgl_error_take(&ctx.errors) == GL_NO_ERROR && pixels[1] == 0.0f, 15,
                   "fixed-point read clamping applies to signed-normalized sources");
    if (result)
        return result;
    ctx.canonical_state.clamp_read_color = GL_FALSE;

    tex->levels[0].internal_format = 0u;
    fail_native_query = 1;
    before = native_reads;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, bytes);
    result = check(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION && native_reads == before,
                   12, "failed native metadata query must not fall back to RGBA8");
    if (result)
        return result;

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL read attachment coverage: PASS");
    return 0;
}

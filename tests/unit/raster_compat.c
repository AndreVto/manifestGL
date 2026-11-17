#include "mgl/gl.h"
#include "compat/raster.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "state/state.h"
#include "texture/texture_store.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_shader;
    GLuint next_program;
    GLuint next_texture;
    unsigned draw_calls;
    unsigned read_calls;
    unsigned upload_calls;
    unsigned char uploads[8][16];
    size_t upload_sizes[8];
    unsigned char read_seed;
    GLfloat last_rect[4];
    int precise_source;
    int norm16_available;
    GLenum last_upload_type;
    GLint last_upload_internal;
    float last_float_upload[16];
    unsigned char last_byte_upload[16];
} cap;

static GLenum fake_get_error(void) {
    return GL_NO_ERROR;
}

static const GLubyte *fake_get_string(GLenum name) {
    return name == GL_EXTENSIONS && cap.norm16_available ? (const GLubyte *)"GL_EXT_texture_norm16"
                                                         : (const GLubyte *)"";
}

static GLuint fake_create_shader(GLenum type) {
    (void)type;
    return ++cap.next_shader;
}

static void fake_shader_source(GLuint shader, GLsizei count, const GLchar *const *strings,
                               const GLint *lengths) {
    (void)shader;
    (void)count;
    (void)strings;
    (void)lengths;
}

static void fake_compile_shader(GLuint shader) {
    (void)shader;
}

static void fake_get_shaderiv(GLuint shader, GLenum pname, GLint *value) {
    (void)shader;
    *value = pname == GL_COMPILE_STATUS ? GL_TRUE : 0;
}

static void fake_delete_shader(GLuint shader) {
    (void)shader;
}

static GLuint fake_create_program(void) {
    return ++cap.next_program + 100u;
}

static void fake_attach_shader(GLuint program, GLuint shader) {
    (void)program;
    (void)shader;
}

static void fake_link_program(GLuint program) {
    (void)program;
}

static void fake_get_programiv(GLuint program, GLenum pname, GLint *value) {
    (void)program;
    *value = pname == GL_LINK_STATUS ? GL_TRUE : 0;
}

static void fake_delete_program(GLuint program) {
    (void)program;
}

static void fake_use_program(GLuint program) {
    (void)program;
}

static GLint fake_get_uniform_location(GLuint program, const GLchar *name) {
    (void)program;
    if (strcmp(name, "uRect") == 0)
        return 1;
    if (strcmp(name, "uViewport") == 0)
        return 2;
    if (strcmp(name, "uDepth") == 0)
        return 3;
    if (strcmp(name, "uPixels") == 0)
        return 4;
    if (strcmp(name, "uBitmap") == 0)
        return 5;
    if (strcmp(name, "uRasterColor") == 0)
        return 6;
    if (strcmp(name, "uAlphaEnabled") == 0)
        return 7;
    if (strcmp(name, "uAlphaFunc") == 0)
        return 8;
    if (strcmp(name, "uAlphaRef") == 0)
        return 9;
    return -1;
}

static void fake_uniform1i(GLint location, GLint value) {
    (void)location;
    (void)value;
}

static void fake_uniform1f(GLint location, GLfloat value) {
    (void)location;
    (void)value;
}

static void fake_uniform4f(GLint location, GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
    if (location == 1) {
        cap.last_rect[0] = a;
        cap.last_rect[1] = b;
        cap.last_rect[2] = c;
        cap.last_rect[3] = d;
    }
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

static void fake_bind_texture(GLenum target, GLuint texture) {
    (void)target;
    (void)texture;
}

static void fake_active_texture(GLenum texture) {
    (void)texture;
}

static void fake_tex_parameteri(GLenum target, GLenum pname, GLint value) {
    (void)target;
    (void)pname;
    (void)value;
}

static void fake_tex_image(GLenum target, GLint level, GLint internalformat, GLsizei width,
                           GLsizei height, GLint border, GLenum format, GLenum type,
                           const void *pixels) {
    size_t bytes = (size_t)width * (size_t)height * (type == GL_FLOAT ? 16u : 4u);
    unsigned index = cap.upload_calls;
    (void)target;
    (void)level;
    (void)border;
    (void)format;
    cap.last_upload_type = type;
    cap.last_upload_internal = internalformat;
    if (type == GL_UNSIGNED_BYTE && pixels != NULL && width > 0 && height > 0) {
        size_t count = (size_t)width * (size_t)height * 4u;
        if (count > sizeof(cap.last_byte_upload))
            count = sizeof(cap.last_byte_upload);
        memcpy(cap.last_byte_upload, pixels, count);
    }
    if (type == GL_FLOAT && pixels != NULL && width > 0 && height > 0) {
        size_t count = (size_t)width * (size_t)height * 4u;
        if (count > 16u)
            count = 16u;
        memcpy(cap.last_float_upload, pixels, count * sizeof(float));
    }
    if (index < 8u) {
        if (bytes > sizeof(cap.uploads[index]))
            bytes = sizeof(cap.uploads[index]);
        cap.upload_sizes[index] = bytes;
        if (bytes != 0u && pixels != NULL)
            memcpy(cap.uploads[index], pixels, bytes);
    }
    ++cap.upload_calls;
}

static void fake_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    (void)mode;
    (void)first;
    (void)count;
    ++cap.draw_calls;
}

static void fake_bind_framebuffer(GLenum target, GLuint framebuffer) {
    (void)target;
    (void)framebuffer;
}

static void fake_bind_buffer(GLenum target, GLuint buffer) {
    (void)target;
    (void)buffer;
}

static void fake_pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
}

static void fake_read_pixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                             GLenum type, void *pixels) {
    unsigned char *dst = (unsigned char *)pixels;
    size_t bytes = (size_t)width * (size_t)height * 4u, i;
    (void)x;
    (void)y;
    (void)format;
    (void)type;
    ++cap.read_calls;
    if (cap.precise_source && type == GL_UNSIGNED_SHORT) {
        const uint16_t values[4] = {1u, 257u, 32768u, 65535u};
        memcpy(pixels, values, sizeof(values));
        return;
    }
    for (i = 0u; i < bytes; ++i)
        dst[i] = (unsigned char)(cap.read_seed + (unsigned char)i);
}

static void fake_enable(GLenum value) {
    (void)value;
}

static void fake_disable(GLenum value) {
    (void)value;
}

static void fake_color_mask(GLboolean a, GLboolean b, GLboolean c, GLboolean d) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
}

static void fake_depth_mask(GLboolean value) {
    (void)value;
}

static int fail(int code, const char *why) {
    fprintf(stderr, "raster compatibility failure %d: %s\n", code, why);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLfloat v[4] = {0};
    GLboolean valid = GL_FALSE;
    GLuint list;
    unsigned char pixels[4] = {1u, 2u, 3u, 4u};
    unsigned char bitmap[4] = {0x80u, 0u, 0u, 0u};
    unsigned char lsb_bitmap[1] = {0x01u};

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGetError = fake_get_error;
    native.glesGetString = fake_get_string;
    native.glesCreateShader = fake_create_shader;
    native.glesShaderSource = fake_shader_source;
    native.glesCompileShader = fake_compile_shader;
    native.glesGetShaderiv = fake_get_shaderiv;
    native.glesDeleteShader = fake_delete_shader;
    native.glesCreateProgram = fake_create_program;
    native.glesAttachShader = fake_attach_shader;
    native.glesLinkProgram = fake_link_program;
    native.glesGetProgramiv = fake_get_programiv;
    native.glesDeleteProgram = fake_delete_program;
    native.glesUseProgram = fake_use_program;
    native.glesGetUniformLocation = fake_get_uniform_location;
    native.glesUniform1i = fake_uniform1i;
    native.glesUniform1f = fake_uniform1f;
    native.glesUniform4f = fake_uniform4f;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesActiveTexture = fake_active_texture;
    native.glesTexParameteri = fake_tex_parameteri;
    native.glesTexImage2D = fake_tex_image;
    native.glesDrawArrays = fake_draw_arrays;
    native.glesBindFramebuffer = fake_bind_framebuffer;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesPixelStorei = fake_pixel_store;
    native.glesReadPixels = fake_read_pixels;
    native.glesEnable = fake_enable;
    native.glesDisable = fake_disable;
    native.glesColorMask = fake_color_mask;
    native.glesDepthMask = fake_depth_mask;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 100, 100);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_compat_raster_init(&ctx.compat_raster);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glRasterPos2f(0.0f, 0.0f);
    glGetFloatv(GL_CURRENT_RASTER_POSITION, v);
    glGetBooleanv(GL_CURRENT_RASTER_POSITION_VALID, &valid);
    if (!valid || fabsf(v[0] - 50.0f) > 0.001f || fabsf(v[1] - 50.0f) > 0.001f ||
        fabsf(v[2] - 0.5f) > 0.001f)
        return fail(2, "raster position/query");

    glPixelZoom(2.0f, 3.0f);
    glGetFloatv(GL_ZOOM_X, v);
    glGetFloatv(GL_ZOOM_Y, v + 1);
    if (fabsf(v[0] - 2.0f) > 0.001f || fabsf(v[1] - 3.0f) > 0.001f)
        return fail(3, "pixel zoom query");

    glPushAttrib(GL_CURRENT_BIT | GL_PIXEL_MODE_BIT);
    glRasterPos2f(-0.5f, -0.5f);
    glPixelZoom(7.0f, 8.0f);
    glPopAttrib();
    glGetFloatv(GL_CURRENT_RASTER_POSITION, v);
    glGetFloatv(GL_ZOOM_X, v + 2);
    glGetFloatv(GL_ZOOM_Y, v + 3);
    if (fabsf(v[0] - 50.0f) > 0.001f || fabsf(v[1] - 50.0f) > 0.001f ||
        fabsf(v[2] - 2.0f) > 0.001f || fabsf(v[3] - 3.0f) > 0.001f)
        return fail(4, "attrib restoration");

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_LSB_FIRST, 1);
    glBitmap(1, 1, 0.0f, 0.0f, 0.0f, 0.0f, lsb_bitmap);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.upload_calls != 1u ||
        cap.uploads[0][3] != 255u)
        return fail(14, "Bitmap UNPACK_LSB_FIRST");
    glPixelStorei(GL_UNPACK_LSB_FIRST, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    cap.draw_calls = 0u;
    cap.read_calls = 0u;
    cap.upload_calls = 0u;

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glRasterPos2f(0.0f, 0.0f);
    glPixelZoom(2.0f, 3.0f);
    glDrawPixels(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBitmap(1, 1, 0.0f, 0.0f, 5.0f, 0.0f, bitmap);
    glCopyPixels(0, 0, 1, 1, GL_COLOR);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(5, "list compile");
    if (cap.draw_calls != 0u || cap.read_calls != 0u || cap.upload_calls != 0u)
        return fail(6, "GL_COMPILE executed raster operations");

    memset(pixels, 0xee, sizeof(pixels));
    bitmap[0] = 0u;
    cap.read_seed = 50u;
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(1.0f, 0.0f, 0.0f);
    glCallList(list);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(7, "list replay");
    if (cap.draw_calls != 3u || cap.read_calls != 1u || cap.upload_calls != 3u)
        return fail(8, "raster replay counts");
    if (cap.upload_sizes[0] != 4u ||
        memcmp(cap.uploads[0], (const unsigned char[]){1u, 2u, 3u, 4u}, 4u) != 0)
        return fail(9, "DrawPixels payload not owned at compile time");
    if (cap.uploads[1][3] != 255u)
        return fail(10, "Bitmap payload not owned at compile time");
    if (memcmp(cap.uploads[2], (const unsigned char[]){50u, 51u, 52u, 53u}, 4u) != 0)
        return fail(11, "CopyPixels did not observe framebuffer at replay");
    if (fabsf(cap.last_rect[0] - 105.0f) > 0.001f)
        return fail(12, "RasterPos/Bitmap replay timing");

    glGetFloatv(GL_CURRENT_RASTER_POSITION, v);
    if (fabsf(v[0] - 105.0f) > 0.001f || fabsf(v[1] - 50.0f) > 0.001f)
        return fail(13, "bitmap raster movement");

    {
        unsigned reads = cap.read_calls, draws = cap.draw_calls, uploads = cap.upload_calls;
        unsigned char sentinel[4] = {0x5a, 0x5a, 0x5a, 0x5a};
        glBegin(GL_POINTS);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, sentinel);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || sentinel[0] != 0x5a || sentinel[3] != 0x5a)
            return fail(15, "ReadPixels in Begin/End changed output or error");
        glBegin(GL_POINTS);
        glReadPixels(0, 0, -1, 1, 0xDEAD, 0xDEAD, sentinel);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION)
            return fail(16, "ReadPixels error precedence");
        glBegin(GL_POINTS);
        glCopyPixels(0, 0, -1, 1, 0xDEAD);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION)
            return fail(17, "CopyPixels error precedence");
        glBegin(GL_POINTS);
        glCopyPixels(0, 0, 0, 0, GL_COLOR);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION)
            return fail(18, "CopyPixels empty during Begin/End");
        if (cap.read_calls != reads || cap.draw_calls != draws || cap.upload_calls != uploads)
            return fail(19, "illegal raster operations reached GLES");
    }
    /* GL_BGRA/GL_UNSIGNED_INT_8_8_8_8_REV readback converts native RGBA8
     * data to desktop pixel order and applies per-element pack byte swapping. */
    {
        unsigned char readback[20] = {0};
        const unsigned char expected_bgra[8] = {12u, 11u, 10u, 13u, 16u, 15u, 14u, 17u};
        unsigned char swapped[4] = {0};
        cap.read_seed = 10u;
        glReadPixels(0, 0, 2, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, readback);
        if (glGetError() != GL_NO_ERROR ||
            memcmp(readback, expected_bgra, sizeof(expected_bgra)) != 0)
            return fail(20, "packed BGRA readback");
        memset(readback, 0xa5, sizeof(readback));
        glPixelStorei(GL_PACK_ROW_LENGTH, 3);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 1);
        glReadPixels(0, 0, 2, 1, GL_BGRA, GL_UNSIGNED_BYTE, readback);
        if (glGetError() != GL_NO_ERROR || readback[0] != 0xa5 ||
            memcmp(readback + 4, expected_bgra, sizeof(expected_bgra)) != 0 || readback[12] != 0xa5)
            return fail(21, "packed BGRA with row length and pixel skip");
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_PACK_SWAP_BYTES, GL_TRUE);
        glReadPixels(0, 0, 1, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, swapped);
        if (glGetError() != GL_NO_ERROR ||
            memcmp(swapped, (const unsigned char[]){13u, 10u, 11u, 12u}, 4u) != 0)
            return fail(22, "packed BGRA byte swap");
        glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, swapped);
        if (glGetError() != GL_NO_ERROR ||
            memcmp(swapped, (const unsigned char[]){10u, 11u, 12u, 13u}, 4u) != 0)
            return fail(23, "packed RGBA REV readback");
        glReadPixels(0, 0, 1, 1, GL_BGRA, GL_UNSIGNED_SHORT, readback);
        if (glGetError() != GL_NO_ERROR)
            return fail(24, "valid BGRA short readback rejected");
        memset(readback, 0xa5, sizeof(readback));
        glReadPixels(0, 0, 1, 1, GL_BGR, GL_UNSIGNED_SHORT_5_6_5, readback);
        if (glGetError() != GL_INVALID_OPERATION || readback[0] != 0xa5u)
            return fail(89, "packed RGB type requires GL_RGB");
        glReadPixels(0, 0, 1, 1, GL_BGR, GL_UNSIGNED_BYTE_3_3_2, readback);
        if (glGetError() != GL_INVALID_OPERATION || readback[0] != 0xa5u)
            return fail(90, "packed RGB byte requires GL_RGB");
    }
    {
        struct mgl_texture *attachment = mgl_texture_ensure(&ctx, 951u, GL_TEXTURE_2D, 0);
        struct mgl_framebuffer *fb = mgl_framebuffer_ensure(&ctx, 952u, 0);
        unsigned before;
        if (attachment == NULL || fb == NULL)
            return fail(25, "precision read attachment");
        attachment->levels[0].defined = GL_TRUE;
        attachment->levels[0].internal_format = GL_RGBA16;
        fb->color_texture = attachment->name;
        fb->color_texture_target = GL_TEXTURE_2D;
        fb->color_texture_level = 0;
        fb->read_buffer = GL_COLOR_ATTACHMENT0;
        ctx.read_framebuffer = fb->name;
        cap.norm16_available = 1;
        cap.precise_source = 1;
        before = cap.draw_calls;
        glCopyPixels(0, 0, 1, 1, GL_COLOR);
        if (glGetError() != GL_NO_ERROR || cap.draw_calls != before + 1u ||
            cap.last_upload_type != GL_FLOAT || cap.last_upload_internal != (GLint)GL_RGBA32F ||
            fabsf(cap.last_float_upload[1] - 257.0f / 65535.0f) > 0.000001f ||
            fabsf(cap.last_float_upload[2] - 32768.0f / 65535.0f) > 0.000001f)
            return fail(26, "CopyPixels high precision staging");
        cap.norm16_available = 0;
        before = cap.draw_calls;
        glCopyPixels(0, 0, 1, 1, GL_COLOR);
        if (glGetError() != GL_INVALID_OPERATION || cap.draw_calls != before)
            return fail(27, "CopyPixels native extension rejection");
        ctx.read_framebuffer = 0u;
        cap.precise_source = 0;
    }
    {
        const float floating[4] = {0.25f, 1.5f, -0.5f, 0.75f};
        const uint16_t two_pixels[8] = {65535u, 32768u, 0u, 65535u, 0u, 65535u, 65535u, 32768u};
        const uint16_t reversed_565 = 0x001fu;
        const uint16_t reversed_5551 = 0x001fu;
        const uint32_t reversed_1010102 = 0x000003ffu;
        const uint32_t rgb9e5_one = (16u << 27u) | 256u;
        const uint8_t reversed_233 = 0x07u;
        const uint8_t luminance_alpha[2] = {128u, 255u};
        const uint8_t blue_green_red[3] = {10u, 20u, 30u};
        const uint16_t swapped_short = 0x0080u;
        unsigned before;
        glDrawPixels(1, 1, GL_RGBA, GL_FLOAT, floating);
        if (glGetError() != GL_NO_ERROR || cap.last_upload_type != GL_FLOAT ||
            cap.last_upload_internal != (GLint)GL_RGBA32F ||
            fabsf(cap.last_float_upload[0] - 0.25f) > 0.00001f ||
            fabsf(cap.last_float_upload[1] - 1.5f) > 0.00001f ||
            fabsf(cap.last_float_upload[2] + 0.5f) > 0.00001f)
            return fail(91, "DrawPixels floating color precision");
        glDrawPixels(2, 1, GL_RGBA, GL_UNSIGNED_SHORT, two_pixels);
        if (glGetError() != GL_NO_ERROR || cap.last_upload_type != GL_FLOAT ||
            fabsf(cap.last_float_upload[0] - 1.0f) > 0.00001f ||
            fabsf(cap.last_float_upload[1] - 32768.0f / 65535.0f) > 0.00001f ||
            fabsf(cap.last_float_upload[4]) > 0.00001f ||
            fabsf(cap.last_float_upload[5] - 1.0f) > 0.00001f)
            return fail(92, "DrawPixels multi-pixel short stride");
        glDrawPixels(1, 1, GL_BGR, GL_UNSIGNED_BYTE, blue_green_red);
        if (glGetError() != GL_NO_ERROR || cap.last_upload_type != GL_UNSIGNED_BYTE ||
            memcmp(cap.last_byte_upload, (const unsigned char[]){30u, 20u, 10u, 255u}, 4u) != 0)
            return fail(93, "DrawPixels BGR scalar");
        glDrawPixels(1, 1, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, luminance_alpha);
        if (glGetError() != GL_NO_ERROR ||
            memcmp(cap.last_byte_upload, (const unsigned char[]){128u, 128u, 128u, 255u}, 4u) != 0)
            return fail(94, "DrawPixels luminance alpha");
        glDrawPixels(1, 1, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, &reversed_565);
        if (glGetError() != GL_NO_ERROR || fabsf(cap.last_float_upload[0] - 1.0f) > 0.00001f ||
            fabsf(cap.last_float_upload[1]) > 0.00001f ||
            fabsf(cap.last_float_upload[2]) > 0.00001f)
            return fail(95, "DrawPixels packed 565 reversed");
        glDrawPixels(1, 1, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, &reversed_5551);
        if (glGetError() != GL_NO_ERROR || fabsf(cap.last_float_upload[0] - 1.0f) > 0.00001f ||
            fabsf(cap.last_float_upload[3]) > 0.00001f)
            return fail(96, "DrawPixels packed 5551 reversed");
        glDrawPixels(1, 1, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, &reversed_1010102);
        if (glGetError() != GL_NO_ERROR || fabsf(cap.last_float_upload[0] - 1.0f) > 0.00001f ||
            fabsf(cap.last_float_upload[3]) > 0.00001f)
            return fail(97, "DrawPixels packed 1010102 reversed");
        glDrawPixels(1, 1, GL_RGB, GL_UNSIGNED_BYTE_2_3_3_REV, &reversed_233);
        if (glGetError() != GL_NO_ERROR || fabsf(cap.last_float_upload[0] - 1.0f) > 0.00001f ||
            fabsf(cap.last_float_upload[2]) > 0.00001f)
            return fail(98, "DrawPixels packed 233 reversed");
        glDrawPixels(1, 1, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, &rgb9e5_one);
        if (glGetError() != GL_NO_ERROR || fabsf(cap.last_float_upload[0] - 1.0f) > 0.00001f)
            return fail(99, "DrawPixels packed RGB9E5");
        glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
        glDrawPixels(1, 1, GL_RED, GL_UNSIGNED_SHORT, &swapped_short);
        glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
        if (glGetError() != GL_NO_ERROR ||
            fabsf(cap.last_float_upload[0] - 32768.0f / 65535.0f) > 0.00001f)
            return fail(100, "DrawPixels unpack byte swap");
        {
            const uint16_t half = 0x3800u;
            const int8_t negative = -127;
            const int32_t integer = INT32_MAX;
            const uint32_t r11g11b10_one = 0x3c0u;
            glDrawPixels(1, 1, GL_RED, GL_HALF_FLOAT, &half);
            if (glGetError() != GL_NO_ERROR || fabsf(cap.last_float_upload[0] - 0.5f) > 0.00001f)
                return fail(102, "DrawPixels half float");
            glDrawPixels(1, 1, GL_RED, GL_BYTE, &negative);
            if (glGetError() != GL_NO_ERROR || fabsf(cap.last_float_upload[0] + 1.0f) > 0.00001f)
                return fail(103, "DrawPixels signed normalized byte");
            glDrawPixels(1, 1, GL_RED, GL_INT, &integer);
            if (glGetError() != GL_NO_ERROR || fabsf(cap.last_float_upload[0] - 1.0f) > 0.00001f)
                return fail(104, "DrawPixels signed normalized integer");
            glDrawPixels(1, 1, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, &r11g11b10_one);
            if (glGetError() != GL_NO_ERROR || fabsf(cap.last_float_upload[0] - 1.0f) > 0.00001f)
                return fail(105, "DrawPixels packed unsigned float");
        }
        {
            static const struct {
                GLenum format;
                GLenum type;
                uint32_t first_red;
            } packed_cases[] = {{GL_RGB, GL_UNSIGNED_BYTE_3_3_2, 0xe0u},
                                {GL_RGB, GL_UNSIGNED_BYTE_2_3_3_REV, 0x07u},
                                {GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0xf800u},
                                {GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, 0x001fu},
                                {GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 0xf000u},
                                {GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4_REV, 0x000fu},
                                {GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 0xf800u},
                                {GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, 0x001fu},
                                {GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, 0xff000000u},
                                {GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, 0x000000ffu},
                                {GL_RGBA, GL_UNSIGNED_INT_10_10_10_2, 0xffc00000u},
                                {GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 0x000003ffu},
                                {GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, 0x000003c0u},
                                {GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, (16u << 27u) | 256u}};

            size_t k;
            for (k = 0u; k < sizeof(packed_cases) / sizeof(packed_cases[0]); ++k) {
                glDrawPixels(1, 1, packed_cases[k].format, packed_cases[k].type,
                             &packed_cases[k].first_red);
                if (glGetError() != GL_NO_ERROR || cap.last_upload_type != GL_FLOAT ||
                    fabsf(cap.last_float_upload[0] - 1.0f) > 0.00001f ||
                    fabsf(cap.last_float_upload[1]) > 0.00001f ||
                    fabsf(cap.last_float_upload[2]) > 0.00001f)
                    return fail(106, "DrawPixels packed type matrix");
            }
        }
        before = cap.upload_calls;
        glDrawPixels(1, 1, GL_BGR, GL_UNSIGNED_SHORT_5_6_5_REV, &reversed_565);
        if (glGetError() != GL_INVALID_OPERATION || cap.upload_calls != before)
            return fail(101, "DrawPixels invalid packed BGR");
    }
    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL raster compatibility unit test: PASS");
    return 0;
}

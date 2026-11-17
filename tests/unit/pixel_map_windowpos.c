#include "mgl/gl.h"
#include "abi/proc_address.h"
#include "compat/raster.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "state/state.h"

#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_shader;
    GLuint next_program;
    GLuint next_texture;
    unsigned draw_calls;
    unsigned upload_calls;
    unsigned char upload[16];
    size_t upload_size;
} cap;

static GLenum fake_get_error(void) {
    return GL_NO_ERROR;
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
    (void)name;
    return 1;
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
    (void)location;
    (void)a;
    (void)b;
    (void)c;
    (void)d;
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
    size_t bytes = (size_t)width * (size_t)height * 4u;
    (void)target;
    (void)level;
    (void)internalformat;
    (void)border;
    (void)format;
    (void)type;
    if (bytes > sizeof(cap.upload))
        bytes = sizeof(cap.upload);
    cap.upload_size = bytes;
    if (bytes != 0u && pixels != NULL)
        memcpy(cap.upload, pixels, bytes);
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
    fprintf(stderr, "pixel-map/window-position failure %d: %s\n", code, why);
    return code;
}

static int closef(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 0.001f;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLfloat position[4] = {0};
    GLfloat got[4] = {0};
    GLuint got_ui[4] = {0u};
    GLushort got_us[4] = {0u};
    const GLuint ui_map[4] = {0u, UINT_MAX / 3u, (UINT_MAX / 3u) * 2u, UINT_MAX};
    const GLushort us_map[4] = {0u, (GLushort)(USHRT_MAX / 3u), (GLushort)((USHRT_MAX / 3u) * 2u),
                                USHRT_MAX};
    GLfloat red_map[4] = {0.0f, 0.25f, 0.5f, 1.0f};
    GLfloat green_map[4] = {1.0f, 0.5f, 0.25f, 0.0f};
    GLfloat blue_map[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    GLfloat alpha_map[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat list_map[4] = {0.0f, 0.1f, 0.6f, 1.0f};
    GLfloat replacement[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat window2[2] = {21.0f, 22.0f};
    GLfloat window3[3] = {23.0f, 24.0f, 1.25f};
    GLfloat list_window[3] = {7.0f, 8.0f, 0.25f};
    GLubyte index = 2u;
    GLint iv = 0;
    GLuint list;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGetError = fake_get_error;
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

    glWindowPos2fv(window2);
    glGetFloatv(GL_CURRENT_RASTER_POSITION, position);
    if (!closef(position[0], 21.0f) || !closef(position[1], 22.0f) || !closef(position[2], 0.0f) ||
        !closef(position[3], 1.0f))
        return fail(17, "glWindowPos2fv exact vector semantics");

    glWindowPos3fv(window3);
    glGetFloatv(GL_CURRENT_RASTER_POSITION, position);
    if (!closef(position[0], 23.0f) || !closef(position[1], 24.0f) || !closef(position[2], 1.0f) ||
        !closef(position[3], 1.0f))
        return fail(18, "glWindowPos3fv exact vector/clamped-depth semantics");

    glBegin(GL_POINTS);
    glWindowPos2fv(window2);
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(19, "glWindowPos2fv Begin/End legality");

    glWindowPos3f(12.0f, 34.0f, 2.0f);
    glGetFloatv(GL_CURRENT_RASTER_POSITION, position);
    if (!closef(position[0], 12.0f) || !closef(position[1], 34.0f) || !closef(position[2], 1.0f) ||
        !closef(position[3], 1.0f))
        return fail(2, "window position/clamped depth");

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glWindowPos3fv(list_window);
    glPixelMapfv(GL_PIXEL_MAP_I_TO_R, 4, list_map);
    glEndList();
    glGetFloatv(GL_CURRENT_RASTER_POSITION, position);
    if (!closef(position[0], 12.0f) || !closef(position[1], 34.0f))
        return fail(3, "GL_COMPILE executed WindowPos");
    glGetPixelMapfv(GL_PIXEL_MAP_I_TO_R, got);
    if (!closef(got[2], 0.0f))
        return fail(4, "GL_COMPILE executed PixelMap");
    list_map[2] = 0.0f;
    list_window[0] = 70.0f;
    list_window[1] = 80.0f;
    list_window[2] = 0.75f;
    glPixelMapfv(GL_PIXEL_MAP_I_TO_R, 4, replacement);
    glCallList(list);
    glGetFloatv(GL_CURRENT_RASTER_POSITION, position);
    glGetPixelMapfv(GL_PIXEL_MAP_I_TO_R, got);
    if (!closef(position[0], 7.0f) || !closef(position[1], 8.0f) || !closef(position[2], 0.25f))
        return fail(5, "WindowPos replay");
    if (!closef(got[2], 0.6f))
        return fail(6, "PixelMap list payload ownership");

    glPixelMapfv(GL_PIXEL_MAP_I_TO_R, 4, red_map);
    glPixelMapfv(GL_PIXEL_MAP_I_TO_G, 4, green_map);
    glPixelMapfv(GL_PIXEL_MAP_I_TO_B, 4, blue_map);
    glPixelMapfv(GL_PIXEL_MAP_I_TO_A, 4, alpha_map);
    glGetIntegerv(GL_PIXEL_MAP_I_TO_R_SIZE, &iv);
    if (iv != 4)
        return fail(7, "pixel-map size query");
    glGetIntegerv(GL_MAX_PIXEL_MAP_TABLE, &iv);
    if (iv < 4)
        return fail(8, "max pixel-map table query");
    glPixelMapuiv(GL_PIXEL_MAP_R_TO_R, 4, ui_map);
    glGetPixelMapuiv(GL_PIXEL_MAP_R_TO_R, got_ui);
    if (got_ui[1] != ui_map[1] || got_ui[3] != UINT_MAX)
        return fail(15, "uint pixel-map roundtrip");
    glPixelMapusv(GL_PIXEL_MAP_G_TO_G, 4, us_map);
    glGetPixelMapusv(GL_PIXEL_MAP_G_TO_G, got_us);
    if (got_us[1] != us_map[1] || got_us[3] != USHRT_MAX)
        return fail(16, "ushort pixel-map roundtrip");

    glPushAttrib(GL_PIXEL_MODE_BIT);
    glPixelMapfv(GL_PIXEL_MAP_I_TO_R, 4, replacement);
    glPopAttrib();
    glGetPixelMapfv(GL_PIXEL_MAP_I_TO_R, got);
    if (!closef(got[2], 0.5f))
        return fail(9, "GL_PIXEL_MODE_BIT pixel-map restoration");

    glWindowPos2f(1.0f, 2.0f);
    glDrawPixels(1, 1, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, &index);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(10, "COLOR_INDEX draw error");
    if (cap.upload_calls != 1u || cap.draw_calls != 1u || cap.upload_size != 4u)
        return fail(11, "COLOR_INDEX native path");
    if (cap.upload[0] != 128u || cap.upload[1] != 64u || cap.upload[2] != 255u ||
        cap.upload[3] != 255u)
        return fail(12, "COLOR_INDEX map conversion");

    if (mgl_proc_address("glDrawPixels") == NULL || mgl_proc_address("glWindowPos3f") == NULL ||
        mgl_proc_address("glWindowPos2fv") == NULL || mgl_proc_address("glWindowPos3fv") == NULL ||
        mgl_proc_address("glPixelMapfv") == NULL || mgl_proc_address("glEvalCoord1f") == NULL ||
        mgl_proc_address("glSelectBuffer") == NULL || mgl_proc_address("glGenQueries") == NULL ||
        mgl_proc_address("glTexGenf") == NULL || mgl_proc_address("glClipPlane") == NULL)
        return fail(13, "compatibility proc-address coverage");

    glPixelMapfv(GL_PIXEL_MAP_I_TO_R, 3, red_map);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(14, "non-power-of-two pixel map validation");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL pixel-map/window-position unit test: PASS");
    return 0;
}

#include "mgl/gl.h"
#include "native/dispatch.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"
#include "texture/depth_readback.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    GLuint next_framebuffer;
    GLuint next_shader;
    GLuint next_program;
    unsigned read_calls;
    unsigned depth_read_calls;
    GLenum last_read_format;
    GLenum last_read_type;
    GLuint bound_pack_buffer;
    GLint pack_alignment;
    unsigned depth_blits;
    GLint blit_width, blit_height;
    GLenum last_blit_mask, last_blit_filter;
    unsigned depth_write_draws;
    unsigned copy_shader_compiles;
} cap;

static GLenum fake_get_error(void) {
    return GL_NO_ERROR;
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

static void fake_active_texture(GLenum unit) {
    (void)unit;
}

static void fake_bind_texture(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_tex_image(GLenum target, GLint level, GLint internalformat, GLsizei width,
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

static void fake_tex_parameteri(GLenum target, GLenum pname, GLint value) {
    (void)target;
    (void)pname;
    (void)value;
}

static void fake_pixel_store(GLenum pname, GLint value) {
    if (pname == GL_PACK_ALIGNMENT)
        cap.pack_alignment = value;
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

static void fake_framebuffer_texture(GLenum target, GLenum attachment, GLenum textarget,
                                     GLuint texture, GLint level) {
    (void)target;
    (void)attachment;
    (void)textarget;
    (void)texture;
    (void)level;
}

static GLenum fake_check_framebuffer(GLenum target) {
    (void)target;
    return GL_FRAMEBUFFER_COMPLETE;
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
    ++cap.copy_shader_compiles;
}

static void fake_get_shaderiv(GLuint shader, GLenum pname, GLint *value) {
    (void)shader;
    if (pname == GL_COMPILE_STATUS && value != NULL)
        *value = GL_TRUE;
}

static void fake_delete_shader(GLuint shader) {
    (void)shader;
}

static GLuint fake_create_program(void) {
    return ++cap.next_program;
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
    if (pname == GL_LINK_STATUS && value != NULL)
        *value = GL_TRUE;
}

static void fake_delete_program(GLuint program) {
    (void)program;
}

static void fake_use_program(GLuint program) {
    (void)program;
}

static GLint fake_get_uniform_location(GLuint program, const GLchar *name) {
    (void)program;
    return strcmp(name, "uDepth") == 0 ? 3 : 4;
}

static void fake_uniform1i(GLint location, GLint value) {
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

static void fake_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    (void)mode;
    (void)first;
    (void)count;
    ++cap.depth_write_draws;
}

static void fake_blit_framebuffer(GLint x0, GLint y0, GLint x1, GLint y1, GLint dx0, GLint dy0,
                                  GLint dx1, GLint dy1, GLbitfield mask, GLenum filter) {
    (void)x0;
    (void)y0;
    (void)dx0;
    (void)dy0;
    (void)dx1;
    (void)dy1;
    ++cap.depth_blits;
    cap.blit_width = x1 - x0;
    cap.blit_height = y1 - y0;
    cap.last_blit_mask = mask;
    cap.last_blit_filter = filter;
}

static void fake_draw_buffers(GLsizei n, const GLenum *buffers) {
    (void)n;
    (void)buffers;
}

static void fake_depth_func(GLenum function) {
    (void)function;
}

static void fake_depth_mask(GLboolean enable) {
    (void)enable;
}

static void fake_viewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

static void fake_scissor(GLint x, GLint y, GLsizei w, GLsizei h) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

static void fake_enable(GLenum capname) {
    (void)capname;
}

static void fake_disable(GLenum capname) {
    (void)capname;
}

static void fake_color_mask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    (void)r;
    (void)g;
    (void)b;
    (void)a;
}

static void fake_bind_vertex_array(GLuint vao) {
    (void)vao;
}

static void fake_bind_buffer(GLenum target, GLuint buffer) {
    if (target == GL_PIXEL_PACK_BUFFER)
        cap.bound_pack_buffer = buffer;
}

static void fake_read_pixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                             GLenum type, void *pixels) {
    unsigned char *dst = pixels;
    size_t count = (size_t)width * (size_t)height;
    size_t i;
    (void)x;
    (void)y;
    ++cap.read_calls;
    cap.last_read_format = format;
    cap.last_read_type = type;
    if (format == GL_DEPTH_COMPONENT)
        ++cap.depth_read_calls;
    if (format != GL_RGBA || type != GL_UNSIGNED_BYTE || dst == NULL)
        return;
    for (i = 0u; i < count; ++i) {
        /* The helper reconstructs r + g/256 + b/65536. Keep g/b zero so the
         * expected value is easy to assert while still proving RGBA observation. */
        dst[i * 4u + 0u] = (unsigned char)(i == 0u ? 64u : 192u);
        dst[i * 4u + 1u] = 0u;
        dst[i * 4u + 2u] = 0u;
        dst[i * 4u + 3u] = 255u;
    }
}

static int fail(int n, const char *msg) {
    fprintf(stderr, "depth-readback failure %d: %s\n", n, msg);
    return n;
}

static int nearf(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 0.0025f;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint source = 0u, destination = 0u, fbo = 0u;
    GLfloat source_pixels[2] = {0.10f, 0.90f};
    GLfloat observed[2] = {0.0f, 0.0f};
    GLfloat copied[2] = {0.0f, 0.0f};

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));

    native.glesGetError = fake_get_error;
    native.glesEnable = fake_enable;
    native.glesDisable = fake_disable;
    native.glesViewport = fake_viewport;
    native.glesScissor = fake_scissor;
    native.glesColorMask = fake_color_mask;
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexImage2D = fake_tex_image;
    native.glesTexParameteri = fake_tex_parameteri;
    native.glesPixelStorei = fake_pixel_store;
    native.glesGenFramebuffers = fake_gen_framebuffers;
    native.glesDeleteFramebuffers = fake_delete_framebuffers;
    native.glesBindFramebuffer = fake_bind_framebuffer;
    native.glesFramebufferTexture2D = fake_framebuffer_texture;
    native.glesCheckFramebufferStatus = fake_check_framebuffer;
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
    native.glesUniform4f = fake_uniform4f;
    native.glesDrawArrays = fake_draw_arrays;
    native.glesReadPixels = fake_read_pixels;
    native.glesBlitFramebuffer = fake_blit_framebuffer;
    native.glesDrawBuffers = fake_draw_buffers;
    native.glesDepthFunc = fake_depth_func;
    native.glesDepthMask = fake_depth_mask;
    native.glesBindVertexArray = fake_bind_vertex_array;
    native.glesBindBuffer = fake_bind_buffer;

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
    ctx.canonical_state.viewport[2] = 640;
    ctx.canonical_state.viewport[3] = 480;
    mgl_context_set_current_for_test(&ctx);

    glGenTextures(1, &source);
    glBindTexture(GL_TEXTURE_2D, source);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_DEPTH_COMPONENT32F, 2, 1, 0, GL_DEPTH_COMPONENT,
                 GL_FLOAT, source_pixels);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(2, "depth upload");

    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, observed);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(3, "depth GetTexImage");
    if (cap.read_calls != 1u || cap.depth_read_calls != 0u || cap.last_read_format != GL_RGBA ||
        cap.last_read_type != GL_UNSIGNED_BYTE)
        return fail(4, "depth observation must use RGBA ReadPixels only");
    if (!nearf(observed[0], 64.0f / 255.0f) || !nearf(observed[1], 192.0f / 255.0f))
        return fail(5, "shader-observed depth values");
    if (cap.pack_alignment != ctx.pack_alignment || cap.bound_pack_buffer != 0u)
        return fail(6, "pack state restore after depth observation");

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, source, 0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(7, "depth FBO attachment");

    glGenTextures(1, &destination);
    glBindTexture(GL_TEXTURE_2D, destination);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 0, 0, 2, 1, 0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(8, "depth CopyTexImage");
    if (cap.read_calls != 2u || cap.depth_read_calls != 0u)
        return fail(9, "depth CopyTex must not issue depth ReadPixels");

    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, copied);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(10, "copied depth GetTexImage");
    if (!nearf(copied[0], 64.0f / 255.0f) || !nearf(copied[1], 192.0f / 255.0f))
        return fail(11, "depth CopyTex canonical values");
    if (cap.depth_read_calls != 0u)
        return fail(12, "native depth ReadPixels was called");

    glBindFramebuffer(GL_FRAMEBUFFER, 0u);
    glBindTexture(GL_TEXTURE_2D, destination);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 1280, 720, 0, GL_DEPTH_COMPONENT,
                 GL_UNSIGNED_SHORT, NULL);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(13, "destination depth storage");
    {
        unsigned read_before = cap.read_calls;
        unsigned shader_before = cap.copy_shader_compiles;
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1280, 720);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.depth_blits != 1u ||
            cap.blit_width != 1280 || cap.blit_height != 720 ||
            cap.last_blit_mask != GL_DEPTH_BUFFER_BIT || cap.last_blit_filter != GL_NEAREST ||
            cap.depth_write_draws == 0u || cap.read_calls != read_before)
            return fail(14, "default framebuffer depth GPU copy");
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1280, 720);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.depth_blits != 2u ||
            cap.copy_shader_compiles != shader_before + 2u || cap.read_calls != read_before)
            return fail(15, "depth GPU copy must reuse compiled shader and avoid CPU reads");
    }
    mgl_depth_copy_cleanup(&ctx);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL depth readback unit test: PASS");
    return 0;
}

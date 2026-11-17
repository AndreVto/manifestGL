#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static GLuint next_native_name = 1000u;

static void fake_gen(GLsizei n, GLuint *out) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        out[i] = next_native_name++;
}

static void fake_delete(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_bind_texture(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_active_texture(GLenum texture) {
    (void)texture;
}

static void fake_bind_vao(GLuint name) {
    (void)name;
}

static void fake_bind_fb(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_bind_rb(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static GLuint fake_create_shader(GLenum type) {
    (void)type;
    return next_native_name++;
}

static GLuint fake_create_program(void) {
    return next_native_name++;
}

static void fake_delete_shader(GLuint name) {
    (void)name;
}

static void fake_delete_program(GLuint name) {
    (void)name;
}

static int fail(int n, const char *why) {
    fprintf(stderr, "object Is* semantics failure %d: %s\n", n, why);
    return n;
}

static int expect_begin_error(GLboolean (*fn)(GLuint), int n, const char *why) {
    glBegin(GL_POINTS);
    if (fn(0u) != GL_FALSE) {
        glEnd();
        return fail(n, why);
    }
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(n, why);
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint buffer = 0u, texture = 0u, vao = 0u, fb = 0u, rb = 0u;
    GLuint shader, program, list, compile_list;
    GLboolean during_compile[8];
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenBuffers = fake_gen;
    native.glesDeleteBuffers = fake_delete;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesGenTextures = fake_gen;
    native.glesDeleteTextures = fake_delete;
    native.glesBindTexture = fake_bind_texture;
    native.glesActiveTexture = fake_active_texture;
    native.glesGenVertexArrays = fake_gen;
    native.glesDeleteVertexArrays = fake_delete;
    native.glesBindVertexArray = fake_bind_vao;
    native.glesGenFramebuffers = fake_gen;
    native.glesDeleteFramebuffers = fake_delete;
    native.glesBindFramebuffer = fake_bind_fb;
    native.glesGenRenderbuffers = fake_gen;
    native.glesDeleteRenderbuffers = fake_delete;
    native.glesBindRenderbuffer = fake_bind_rb;
    native.glesCreateShader = fake_create_shader;
    native.glesCreateProgram = fake_create_program;
    native.glesDeleteShader = fake_delete_shader;
    native.glesDeleteProgram = fake_delete_program;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 320, 240);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.next_vao_name = 1u;
    ctx.next_framebuffer_name = 1u;
    mgl_context_set_current_for_test(&ctx);

    if (glIsBuffer(0u) || glIsTexture(0u) || glIsVertexArray(0u) || glIsFramebuffer(0u) ||
        glIsRenderbuffer(0u) || glIsShader(0u) || glIsProgram(0u) || glIsList(0u) ||
        glGetError() != GL_NO_ERROR)
        return fail(2, "zero/unknown baseline");

    glGenBuffers(1, &buffer);
    glGenTextures(1, &texture);
    glGenVertexArrays(1, &vao);
    glGenFramebuffers(1, &fb);
    glGenRenderbuffers(1, &rb);
    if (buffer == 0u || texture == 0u || vao == 0u || fb == 0u || rb == 0u)
        return fail(3, "name generation");
    if (glIsBuffer(buffer) || glIsTexture(texture) || glIsVertexArray(vao) || glIsFramebuffer(fb) ||
        glIsRenderbuffer(rb))
        return fail(4, "generated-only names realized early");

    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(vao);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    if (!glIsBuffer(buffer) || !glIsTexture(texture) || !glIsVertexArray(vao) ||
        !glIsFramebuffer(fb) || !glIsRenderbuffer(rb) || glGetError() != GL_NO_ERROR)
        return fail(5, "bound object recognition");

    shader = glCreateShader(GL_VERTEX_SHADER);
    program = glCreateProgram();
    if (shader == 0u || program == 0u || !glIsShader(shader) || !glIsProgram(program))
        return fail(6, "shader/program recognition");
    if (glIsShader(program) || glIsProgram(shader) || glIsShader(999999u) || glIsProgram(999999u) ||
        glGetError() != GL_NO_ERROR)
        return fail(7, "cross-kind/unknown query must be false without error");

    list = glGenLists(1);
    if (list == 0u || !glIsList(list))
        return fail(8, "generated display-list recognition");

    compile_list = glGenLists(1);
    if (compile_list == 0u)
        return fail(9, "compile-list allocation");
    glNewList(compile_list, GL_COMPILE);
    during_compile[0] = glIsBuffer(buffer);
    during_compile[1] = glIsTexture(texture);
    during_compile[2] = glIsVertexArray(vao);
    during_compile[3] = glIsFramebuffer(fb);
    during_compile[4] = glIsRenderbuffer(rb);
    during_compile[5] = glIsShader(shader);
    during_compile[6] = glIsProgram(program);
    during_compile[7] = glIsList(list);
    glEndList();
    for (rc = 0; rc < 8; ++rc)
        if (!during_compile[rc])
            return fail(10, "display-list compile-time query");
    if (glGetError() != GL_NO_ERROR)
        return fail(11, "display-list query error");

    if ((rc = expect_begin_error(glIsBuffer, 12, "glIsBuffer Begin/End/zero ordering")) != 0)
        return rc;
    if ((rc = expect_begin_error(glIsTexture, 13, "glIsTexture Begin/End/zero ordering")) != 0)
        return rc;
    if ((rc = expect_begin_error(glIsVertexArray, 14, "glIsVertexArray Begin/End/zero ordering")) !=
        0)
        return rc;
    if ((rc = expect_begin_error(glIsFramebuffer, 15, "glIsFramebuffer Begin/End/zero ordering")) !=
        0)
        return rc;
    if ((rc = expect_begin_error(glIsRenderbuffer, 16,
                                 "glIsRenderbuffer Begin/End/zero ordering")) != 0)
        return rc;
    if ((rc = expect_begin_error(glIsShader, 17, "glIsShader Begin/End/zero ordering")) != 0)
        return rc;
    if ((rc = expect_begin_error(glIsProgram, 18, "glIsProgram Begin/End/zero ordering")) != 0)
        return rc;
    if ((rc = expect_begin_error(glIsList, 19, "glIsList Begin/End/zero ordering")) != 0)
        return rc;

    glDeleteBuffers(1, &buffer);
    glDeleteTextures(1, &texture);
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &fb);
    glDeleteRenderbuffers(1, &rb);
    glDeleteShader(shader);
    glDeleteProgram(program);
    glDeleteLists(list, 1);
    if (glIsBuffer(buffer) || glIsTexture(texture) || glIsVertexArray(vao) || glIsFramebuffer(fb) ||
        glIsRenderbuffer(rb) || glIsShader(shader) || glIsProgram(program) || glIsList(list) ||
        glGetError() != GL_NO_ERROR)
        return fail(20, "deleted object recognition");

    if (glXGetProcAddress((const GLubyte *)"glIsBuffer") == NULL ||
        glXGetProcAddress((const GLubyte *)"glIsTexture") == NULL ||
        glXGetProcAddress((const GLubyte *)"glIsVertexArray") == NULL ||
        glXGetProcAddress((const GLubyte *)"glIsFramebuffer") == NULL ||
        glXGetProcAddress((const GLubyte *)"glIsRenderbuffer") == NULL ||
        glXGetProcAddress((const GLubyte *)"glIsShader") == NULL ||
        glXGetProcAddress((const GLubyte *)"glIsProgram") == NULL ||
        glXGetProcAddress((const GLubyte *)"glIsList") == NULL)
        return fail(21, "proc-address surface");

    glDeleteLists(compile_list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL object Is* semantics unit test: PASS");
    return 0;
}

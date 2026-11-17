#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/objects.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    unsigned calls;
    unsigned kind;
    GLint location;
    GLsizei count;
    GLfloat f[16];
    GLint i[8];
    GLuint u[8];
    GLuint query_program;
    GLint query_location;
} cap;

enum { CAP_NONE = 0, CAP_2F, CAP_4I, CAP_3UI, CAP_2FV, CAP_3IV, CAP_4UIV, CAP_MAT2, CAP_MAT3X4 };

static void reset_capture(void) {
    memset(&cap, 0, sizeof(cap));
}

static void fake_uniform2f(GLint location, GLfloat a, GLfloat b) {
    ++cap.calls;
    cap.kind = CAP_2F;
    cap.location = location;
    cap.f[0] = a;
    cap.f[1] = b;
}

static void fake_uniform4i(GLint location, GLint a, GLint b, GLint c, GLint d) {
    ++cap.calls;
    cap.kind = CAP_4I;
    cap.location = location;
    cap.i[0] = a;
    cap.i[1] = b;
    cap.i[2] = c;
    cap.i[3] = d;
}

static void fake_uniform3ui(GLint location, GLuint a, GLuint b, GLuint c) {
    ++cap.calls;
    cap.kind = CAP_3UI;
    cap.location = location;
    cap.u[0] = a;
    cap.u[1] = b;
    cap.u[2] = c;
}

static void fake_uniform2fv(GLint location, GLsizei count, const GLfloat *v) {
    ++cap.calls;
    cap.kind = CAP_2FV;
    cap.location = location;
    cap.count = count;
    memcpy(cap.f, v, (size_t)count * 2u * sizeof(GLfloat));
}

static void fake_uniform3iv(GLint location, GLsizei count, const GLint *v) {
    ++cap.calls;
    cap.kind = CAP_3IV;
    cap.location = location;
    cap.count = count;
    memcpy(cap.i, v, (size_t)count * 3u * sizeof(GLint));
}

static void fake_uniform4uiv(GLint location, GLsizei count, const GLuint *v) {
    ++cap.calls;
    cap.kind = CAP_4UIV;
    cap.location = location;
    cap.count = count;
    memcpy(cap.u, v, (size_t)count * 4u * sizeof(GLuint));
}

static void fake_matrix2(GLint location, GLsizei count, GLboolean transpose, const GLfloat *v) {
    (void)transpose;
    ++cap.calls;
    cap.kind = CAP_MAT2;
    cap.location = location;
    cap.count = count;
    memcpy(cap.f, v, (size_t)count * 4u * sizeof(GLfloat));
}

static void fake_matrix3x4(GLint location, GLsizei count, GLboolean transpose, const GLfloat *v) {
    (void)transpose;
    ++cap.calls;
    cap.kind = CAP_MAT3X4;
    cap.location = location;
    cap.count = count;
    memcpy(cap.f, v, (size_t)count * 12u * sizeof(GLfloat));
}

static void fake_get_uniformfv(GLuint program, GLint location, GLfloat *params) {
    cap.query_program = program;
    cap.query_location = location;
    params[0] = 3.25f;
}

static void fake_get_uniformiv(GLuint program, GLint location, GLint *params) {
    cap.query_program = program;
    cap.query_location = location;
    params[0] = -17;
}

static void fake_get_uniformuiv(GLuint program, GLint location, GLuint *params) {
    cap.query_program = program;
    cap.query_location = location;
    params[0] = 99u;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "uniform families failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLfloat fv[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    GLint iv[6] = {1, 2, 3, 4, 5, 6};
    GLuint uiv[4] = {7u, 8u, 9u, 10u};
    GLfloat mat2[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    GLfloat mat3x4[12] = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f};
    GLfloat fquery = 0.0f;
    GLint iquery = 0;
    GLuint uquery = 0u;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    reset_capture();

    native.glesUniform2f = fake_uniform2f;
    native.glesUniform4i = fake_uniform4i;
    native.glesUniform3ui = fake_uniform3ui;
    native.glesUniform2fv = fake_uniform2fv;
    native.glesUniform3iv = fake_uniform3iv;
    native.glesUniform4uiv = fake_uniform4uiv;
    native.glesUniformMatrix2fv = fake_matrix2;
    native.glesUniformMatrix3x4fv = fake_matrix3x4;
    native.glesGetUniformfv = fake_get_uniformfv;
    native.glesGetUniformiv = fake_get_uniformiv;
    native.glesGetUniformuiv = fake_get_uniformuiv;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    program = calloc(1u, sizeof(*program));
    if (program == NULL)
        return fail(2, "program allocation");
    program->name = 7u;
    program->native_name = 77u;
    program->linked = GL_TRUE;
    program->next = ctx.share_group->programs;
    ctx.share_group->programs = program;
    ctx.current_program = 7u;

    glUniform2f(3, 1.5f, -2.0f);
    if (cap.kind != CAP_2F || cap.location != 3 || cap.f[0] != 1.5f || cap.f[1] != -2.0f)
        return fail(3, "float scalar vector forwarding");

    reset_capture();
    glUniform4i(4, -1, 2, -3, 4);
    if (cap.kind != CAP_4I || cap.i[0] != -1 || cap.i[3] != 4)
        return fail(4, "integer vector forwarding");

    reset_capture();
    glUniform3ui(5, 10u, 20u, 30u);
    if (cap.kind != CAP_3UI || cap.u[2] != 30u)
        return fail(5, "unsigned vector forwarding");

    reset_capture();
    glUniform2fv(6, 2, fv);
    if (cap.kind != CAP_2FV || cap.count != 2 || cap.f[3] != 4.0f)
        return fail(6, "float array forwarding");

    reset_capture();
    glUniform3iv(7, 2, iv);
    if (cap.kind != CAP_3IV || cap.count != 2 || cap.i[5] != 6)
        return fail(7, "integer array forwarding");

    reset_capture();
    glUniform4uiv(8, 1, uiv);
    if (cap.kind != CAP_4UIV || cap.u[3] != 10u)
        return fail(8, "unsigned array forwarding");

    reset_capture();
    glUniformMatrix2fv(9, 1, GL_FALSE, mat2);
    if (cap.kind != CAP_MAT2 || cap.f[3] != 4.0f)
        return fail(9, "square matrix forwarding");

    reset_capture();
    glUniformMatrix3x4fv(10, 1, GL_FALSE, mat3x4);
    if (cap.kind != CAP_MAT3X4 || cap.f[11] != 11.0f)
        return fail(10, "non-square matrix forwarding");

    reset_capture();
    glUniform2f(-1, 9.0f, 8.0f);
    if (cap.calls != 0u || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(11, "minus-one location is ignored");

    glUniform2fv(1, -1, fv);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(12, "negative uniform count validation");
    reset_capture();
    glUniformMatrix2fv(1, 1, GL_TRUE, mat2);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.kind != CAP_MAT2 || cap.calls != 1u ||
        cap.f[0] != mat2[0] || cap.f[1] != mat2[2] || cap.f[2] != mat2[1] || cap.f[3] != mat2[3])
        return fail(13, "desktop matrix transpose forwarding");

    glGetUniformfv(7u, 12, &fquery);
    if (cap.query_program != 77u || cap.query_location != 12 || fquery != 3.25f)
        return fail(15, "float getter program mapping");
    glGetUniformiv(7u, 13, &iquery);
    if (cap.query_program != 77u || iquery != -17)
        return fail(16, "integer getter program mapping");
    glGetUniformuiv(7u, 14, &uquery);
    if (cap.query_program != 77u || uquery != 99u)
        return fail(17, "unsigned getter program mapping");

    glGetUniformfv(999u, 0, &fquery);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(18, "invalid getter program validation");

    if (glXGetProcAddress((const GLubyte *)"glUniform4uiv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glUniformMatrix3x4fv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetUniformuiv") == NULL)
        return fail(19, "proc-address coverage");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL uniform families unit test: PASS");
    return 0;
}

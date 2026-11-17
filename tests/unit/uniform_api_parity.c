#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/objects.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned calls;
static GLuint last_program;
static GLint last_location;

#define DEF_UF1(name)                                                                              \
    static void name(GLint l, GLfloat a) {                                                         \
        (void)a;                                                                                   \
        ++calls;                                                                                   \
        last_location = l;                                                                         \
    }
#define DEF_UF2(name)                                                                              \
    static void name(GLint l, GLfloat a, GLfloat b) {                                              \
        (void)a;                                                                                   \
        (void)b;                                                                                   \
        ++calls;                                                                                   \
        last_location = l;                                                                         \
    }
#define DEF_UF3(name)                                                                              \
    static void name(GLint l, GLfloat a, GLfloat b, GLfloat c) {                                   \
        (void)a;                                                                                   \
        (void)b;                                                                                   \
        (void)c;                                                                                   \
        ++calls;                                                                                   \
        last_location = l;                                                                         \
    }
#define DEF_UF4(name)                                                                              \
    static void name(GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d) {                        \
        (void)a;                                                                                   \
        (void)b;                                                                                   \
        (void)c;                                                                                   \
        (void)d;                                                                                   \
        ++calls;                                                                                   \
        last_location = l;                                                                         \
    }
#define DEF_UI1(name, T)                                                                           \
    static void name(GLint l, T a) {                                                               \
        (void)a;                                                                                   \
        ++calls;                                                                                   \
        last_location = l;                                                                         \
    }
#define DEF_UI2(name, T)                                                                           \
    static void name(GLint l, T a, T b) {                                                          \
        (void)a;                                                                                   \
        (void)b;                                                                                   \
        ++calls;                                                                                   \
        last_location = l;                                                                         \
    }
#define DEF_UI3(name, T)                                                                           \
    static void name(GLint l, T a, T b, T c) {                                                     \
        (void)a;                                                                                   \
        (void)b;                                                                                   \
        (void)c;                                                                                   \
        ++calls;                                                                                   \
        last_location = l;                                                                         \
    }
#define DEF_UI4(name, T)                                                                           \
    static void name(GLint l, T a, T b, T c, T d) {                                                \
        (void)a;                                                                                   \
        (void)b;                                                                                   \
        (void)c;                                                                                   \
        (void)d;                                                                                   \
        ++calls;                                                                                   \
        last_location = l;                                                                         \
    }
#define DEF_UA(name, T)                                                                            \
    static void name(GLint l, GLsizei c, const T *v) {                                             \
        (void)c;                                                                                   \
        (void)v;                                                                                   \
        ++calls;                                                                                   \
        last_location = l;                                                                         \
    }
#define DEF_UM(name)                                                                               \
    static void name(GLint l, GLsizei c, GLboolean t, const GLfloat *v) {                          \
        (void)c;                                                                                   \
        (void)t;                                                                                   \
        (void)v;                                                                                   \
        ++calls;                                                                                   \
        last_location = l;                                                                         \
    }
#define DEF_PF1(name)                                                                              \
    static void name(GLuint p, GLint l, GLfloat a) {                                               \
        (void)a;                                                                                   \
        ++calls;                                                                                   \
        last_program = p;                                                                          \
        last_location = l;                                                                         \
    }
#define DEF_PF2(name)                                                                              \
    static void name(GLuint p, GLint l, GLfloat a, GLfloat b) {                                    \
        (void)a;                                                                                   \
        (void)b;                                                                                   \
        ++calls;                                                                                   \
        last_program = p;                                                                          \
        last_location = l;                                                                         \
    }
#define DEF_PF3(name)                                                                              \
    static void name(GLuint p, GLint l, GLfloat a, GLfloat b, GLfloat c) {                         \
        (void)a;                                                                                   \
        (void)b;                                                                                   \
        (void)c;                                                                                   \
        ++calls;                                                                                   \
        last_program = p;                                                                          \
        last_location = l;                                                                         \
    }
#define DEF_PF4(name)                                                                              \
    static void name(GLuint p, GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d) {              \
        (void)a;                                                                                   \
        (void)b;                                                                                   \
        (void)c;                                                                                   \
        (void)d;                                                                                   \
        ++calls;                                                                                   \
        last_program = p;                                                                          \
        last_location = l;                                                                         \
    }
#define DEF_PI1(name, T)                                                                           \
    static void name(GLuint p, GLint l, T a) {                                                     \
        (void)a;                                                                                   \
        ++calls;                                                                                   \
        last_program = p;                                                                          \
        last_location = l;                                                                         \
    }
#define DEF_PI2(name, T)                                                                           \
    static void name(GLuint p, GLint l, T a, T b) {                                                \
        (void)a;                                                                                   \
        (void)b;                                                                                   \
        ++calls;                                                                                   \
        last_program = p;                                                                          \
        last_location = l;                                                                         \
    }
#define DEF_PI3(name, T)                                                                           \
    static void name(GLuint p, GLint l, T a, T b, T c) {                                           \
        (void)a;                                                                                   \
        (void)b;                                                                                   \
        (void)c;                                                                                   \
        ++calls;                                                                                   \
        last_program = p;                                                                          \
        last_location = l;                                                                         \
    }
#define DEF_PI4(name, T)                                                                           \
    static void name(GLuint p, GLint l, T a, T b, T c, T d) {                                      \
        (void)a;                                                                                   \
        (void)b;                                                                                   \
        (void)c;                                                                                   \
        (void)d;                                                                                   \
        ++calls;                                                                                   \
        last_program = p;                                                                          \
        last_location = l;                                                                         \
    }
#define DEF_PA(name, T)                                                                            \
    static void name(GLuint p, GLint l, GLsizei c, const T *v) {                                   \
        (void)c;                                                                                   \
        (void)v;                                                                                   \
        ++calls;                                                                                   \
        last_program = p;                                                                          \
        last_location = l;                                                                         \
    }
#define DEF_PM(name)                                                                               \
    static void name(GLuint p, GLint l, GLsizei c, GLboolean t, const GLfloat *v) {                \
        (void)c;                                                                                   \
        (void)t;                                                                                   \
        (void)v;                                                                                   \
        ++calls;                                                                                   \
        last_program = p;                                                                          \
        last_location = l;                                                                         \
    }

DEF_UF1(f_u1f)
DEF_UF2(f_u2f) DEF_UF3(f_u3f) DEF_UF4(f_u4f) DEF_UI1(f_u1i, GLint) DEF_UI2(f_u2i, GLint) DEF_UI3(
    f_u3i, GLint) DEF_UI4(f_u4i, GLint) DEF_UI1(f_u1ui, GLuint) DEF_UI2(f_u2ui, GLuint)
    DEF_UI3(f_u3ui, GLuint) DEF_UI4(f_u4ui, GLuint) DEF_UA(f_u1fv, GLfloat) DEF_UA(f_u2fv, GLfloat)
        DEF_UA(f_u3fv, GLfloat) DEF_UA(f_u4fv, GLfloat) DEF_UA(f_u1iv, GLint) DEF_UA(f_u2iv, GLint)
            DEF_UA(f_u3iv, GLint) DEF_UA(f_u4iv, GLint) DEF_UA(f_u1uiv, GLuint)
                DEF_UA(f_u2uiv, GLuint) DEF_UA(f_u3uiv, GLuint) DEF_UA(f_u4uiv, GLuint)
                    DEF_UM(f_um2) DEF_UM(f_um3) DEF_UM(f_um4) DEF_UM(f_um2x3) DEF_UM(f_um3x2)
                        DEF_UM(f_um2x4) DEF_UM(f_um4x2) DEF_UM(f_um3x4) DEF_UM(f_um4x3)

                            DEF_PF1(f_p1f) DEF_PF2(f_p2f) DEF_PF3(f_p3f) DEF_PF4(f_p4f)
                                DEF_PI1(f_p1i, GLint) DEF_PI2(f_p2i, GLint) DEF_PI3(f_p3i, GLint)
                                    DEF_PI4(f_p4i, GLint) DEF_PI1(f_p1ui, GLuint) DEF_PI2(f_p2ui,
                                                                                          GLuint)
                                        DEF_PI3(f_p3ui, GLuint) DEF_PI4(f_p4ui, GLuint)
                                            DEF_PA(f_p1fv, GLfloat) DEF_PA(f_p2fv, GLfloat) DEF_PA(
                                                f_p3fv, GLfloat) DEF_PA(f_p4fv, GLfloat)
                                                DEF_PA(f_p1iv, GLint) DEF_PA(f_p2iv, GLint) DEF_PA(
                                                    f_p3iv,
                                                    GLint) DEF_PA(f_p4iv,
                                                                  GLint) DEF_PA(f_p1uiv, GLuint)
                                                    DEF_PA(f_p2uiv,
                                                           GLuint) DEF_PA(f_p3uiv, GLuint)
                                                        DEF_PA(f_p4uiv,
                                                               GLuint) DEF_PM(f_pm2) DEF_PM(f_pm3)
                                                            DEF_PM(f_pm4) DEF_PM(f_pm2x3) DEF_PM(
                                                                f_pm3x2) DEF_PM(f_pm2x4)
                                                                DEF_PM(f_pm4x2)
                                                                    DEF_PM(f_pm3x4)
                                                                        DEF_PM(f_pm4x3)

                                                                            static void f_getfv(
                                                                                GLuint p, GLint l,
                                                                                GLfloat *v) {
    ++calls;
    last_program = p;
    last_location = l;
    v[0] = 1.25f;
}

static void f_getiv(GLuint p, GLint l, GLint *v) {
    ++calls;
    last_program = p;
    last_location = l;
    v[0] = -7;
}

static void f_getuiv(GLuint p, GLint l, GLuint *v) {
    ++calls;
    last_program = p;
    last_location = l;
    v[0] = 9u;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "uniform API parity failure %d: %s\n", code, message);
    return code;
}

#define CALL_CHECK(expr, code)                                                                     \
    do {                                                                                           \
        unsigned before = calls;                                                                   \
        expr;                                                                                      \
        if (calls != before + 1u || mgl_error_take(&ctx.errors) != GL_NO_ERROR)                    \
            return fail((code), #expr);                                                            \
    } while (0)

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLfloat fv[16] = {0};
    GLint iv[8] = {0};
    GLuint uv[8] = {0};
    int code = 10;
    const char *const names[] = {"glUniform1f",
                                 "glUniform2f",
                                 "glUniform3f",
                                 "glUniform4f",
                                 "glUniform1i",
                                 "glUniform2i",
                                 "glUniform3i",
                                 "glUniform4i",
                                 "glUniform1ui",
                                 "glUniform2ui",
                                 "glUniform3ui",
                                 "glUniform4ui",
                                 "glUniform1fv",
                                 "glUniform2fv",
                                 "glUniform3fv",
                                 "glUniform4fv",
                                 "glUniform1iv",
                                 "glUniform2iv",
                                 "glUniform3iv",
                                 "glUniform4iv",
                                 "glUniform1uiv",
                                 "glUniform2uiv",
                                 "glUniform3uiv",
                                 "glUniform4uiv",
                                 "glUniformMatrix2fv",
                                 "glUniformMatrix3fv",
                                 "glUniformMatrix4fv",
                                 "glUniformMatrix2x3fv",
                                 "glUniformMatrix3x2fv",
                                 "glUniformMatrix2x4fv",
                                 "glUniformMatrix4x2fv",
                                 "glUniformMatrix3x4fv",
                                 "glUniformMatrix4x3fv",
                                 "glGetUniformfv",
                                 "glGetUniformiv",
                                 "glGetUniformuiv",
                                 "glProgramUniform1f",
                                 "glProgramUniform2f",
                                 "glProgramUniform3f",
                                 "glProgramUniform4f",
                                 "glProgramUniform1i",
                                 "glProgramUniform2i",
                                 "glProgramUniform3i",
                                 "glProgramUniform4i",
                                 "glProgramUniform1ui",
                                 "glProgramUniform2ui",
                                 "glProgramUniform3ui",
                                 "glProgramUniform4ui",
                                 "glProgramUniform1fv",
                                 "glProgramUniform2fv",
                                 "glProgramUniform3fv",
                                 "glProgramUniform4fv",
                                 "glProgramUniform1iv",
                                 "glProgramUniform2iv",
                                 "glProgramUniform3iv",
                                 "glProgramUniform4iv",
                                 "glProgramUniform1uiv",
                                 "glProgramUniform2uiv",
                                 "glProgramUniform3uiv",
                                 "glProgramUniform4uiv",
                                 "glProgramUniformMatrix2fv",
                                 "glProgramUniformMatrix3fv",
                                 "glProgramUniformMatrix4fv",
                                 "glProgramUniformMatrix2x3fv",
                                 "glProgramUniformMatrix3x2fv",
                                 "glProgramUniformMatrix2x4fv",
                                 "glProgramUniformMatrix4x2fv",
                                 "glProgramUniformMatrix3x4fv",
                                 "glProgramUniformMatrix4x3fv"};
    size_t i;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesUniform1f = f_u1f;
    native.glesUniform2f = f_u2f;
    native.glesUniform3f = f_u3f;
    native.glesUniform4f = f_u4f;
    native.glesUniform1i = f_u1i;
    native.glesUniform2i = f_u2i;
    native.glesUniform3i = f_u3i;
    native.glesUniform4i = f_u4i;
    native.glesUniform1ui = f_u1ui;
    native.glesUniform2ui = f_u2ui;
    native.glesUniform3ui = f_u3ui;
    native.glesUniform4ui = f_u4ui;
    native.glesUniform1fv = f_u1fv;
    native.glesUniform2fv = f_u2fv;
    native.glesUniform3fv = f_u3fv;
    native.glesUniform4fv = f_u4fv;
    native.glesUniform1iv = f_u1iv;
    native.glesUniform2iv = f_u2iv;
    native.glesUniform3iv = f_u3iv;
    native.glesUniform4iv = f_u4iv;
    native.glesUniform1uiv = f_u1uiv;
    native.glesUniform2uiv = f_u2uiv;
    native.glesUniform3uiv = f_u3uiv;
    native.glesUniform4uiv = f_u4uiv;
    native.glesUniformMatrix2fv = f_um2;
    native.glesUniformMatrix3fv = f_um3;
    native.glesUniformMatrix4fv = f_um4;
    native.glesUniformMatrix2x3fv = f_um2x3;
    native.glesUniformMatrix3x2fv = f_um3x2;
    native.glesUniformMatrix2x4fv = f_um2x4;
    native.glesUniformMatrix4x2fv = f_um4x2;
    native.glesUniformMatrix3x4fv = f_um3x4;
    native.glesUniformMatrix4x3fv = f_um4x3;
    native.glesGetUniformfv = f_getfv;
    native.glesGetUniformiv = f_getiv;
    native.glesGetUniformuiv = f_getuiv;
    native.glesProgramUniform1f = f_p1f;
    native.glesProgramUniform2f = f_p2f;
    native.glesProgramUniform3f = f_p3f;
    native.glesProgramUniform4f = f_p4f;
    native.glesProgramUniform1i = f_p1i;
    native.glesProgramUniform2i = f_p2i;
    native.glesProgramUniform3i = f_p3i;
    native.glesProgramUniform4i = f_p4i;
    native.glesProgramUniform1ui = f_p1ui;
    native.glesProgramUniform2ui = f_p2ui;
    native.glesProgramUniform3ui = f_p3ui;
    native.glesProgramUniform4ui = f_p4ui;
    native.glesProgramUniform1fv = f_p1fv;
    native.glesProgramUniform2fv = f_p2fv;
    native.glesProgramUniform3fv = f_p3fv;
    native.glesProgramUniform4fv = f_p4fv;
    native.glesProgramUniform1iv = f_p1iv;
    native.glesProgramUniform2iv = f_p2iv;
    native.glesProgramUniform3iv = f_p3iv;
    native.glesProgramUniform4iv = f_p4iv;
    native.glesProgramUniform1uiv = f_p1uiv;
    native.glesProgramUniform2uiv = f_p2uiv;
    native.glesProgramUniform3uiv = f_p3uiv;
    native.glesProgramUniform4uiv = f_p4uiv;
    native.glesProgramUniformMatrix2fv = f_pm2;
    native.glesProgramUniformMatrix3fv = f_pm3;
    native.glesProgramUniformMatrix4fv = f_pm4;
    native.glesProgramUniformMatrix2x3fv = f_pm2x3;
    native.glesProgramUniformMatrix3x2fv = f_pm3x2;
    native.glesProgramUniformMatrix2x4fv = f_pm2x4;
    native.glesProgramUniformMatrix4x2fv = f_pm4x2;
    native.glesProgramUniformMatrix3x4fv = f_pm3x4;
    native.glesProgramUniformMatrix4x3fv = f_pm4x3;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (!ctx.share_group)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);
    program = calloc(1, sizeof(*program));
    if (!program)
        return fail(2, "program");
    program->name = 7u;
    program->native_name = 77u;
    program->linked = GL_TRUE;
    ctx.share_group->programs = program;
    ctx.current_program = 7u;

    CALL_CHECK(glUniform1f(1, 1), code++);
    CALL_CHECK(glUniform2f(1, 1, 2), code++);
    CALL_CHECK(glUniform3f(1, 1, 2, 3), code++);
    CALL_CHECK(glUniform4f(1, 1, 2, 3, 4), code++);
    CALL_CHECK(glUniform1i(1, 1), code++);
    CALL_CHECK(glUniform2i(1, 1, 2), code++);
    CALL_CHECK(glUniform3i(1, 1, 2, 3), code++);
    CALL_CHECK(glUniform4i(1, 1, 2, 3, 4), code++);
    CALL_CHECK(glUniform1ui(1, 1), code++);
    CALL_CHECK(glUniform2ui(1, 1, 2), code++);
    CALL_CHECK(glUniform3ui(1, 1, 2, 3), code++);
    CALL_CHECK(glUniform4ui(1, 1, 2, 3, 4), code++);
    CALL_CHECK(glUniform1fv(1, 1, fv), code++);
    CALL_CHECK(glUniform2fv(1, 1, fv), code++);
    CALL_CHECK(glUniform3fv(1, 1, fv), code++);
    CALL_CHECK(glUniform4fv(1, 1, fv), code++);
    CALL_CHECK(glUniform1iv(1, 1, iv), code++);
    CALL_CHECK(glUniform2iv(1, 1, iv), code++);
    CALL_CHECK(glUniform3iv(1, 1, iv), code++);
    CALL_CHECK(glUniform4iv(1, 1, iv), code++);
    CALL_CHECK(glUniform1uiv(1, 1, uv), code++);
    CALL_CHECK(glUniform2uiv(1, 1, uv), code++);
    CALL_CHECK(glUniform3uiv(1, 1, uv), code++);
    CALL_CHECK(glUniform4uiv(1, 1, uv), code++);
    CALL_CHECK(glUniformMatrix2fv(1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glUniformMatrix3fv(1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glUniformMatrix4fv(1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glUniformMatrix2x3fv(1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glUniformMatrix3x2fv(1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glUniformMatrix2x4fv(1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glUniformMatrix4x2fv(1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glUniformMatrix3x4fv(1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glUniformMatrix4x3fv(1, 1, GL_FALSE, fv), code++);
    {
        GLfloat v = 0;
        CALL_CHECK(glGetUniformfv(7, 2, &v), code++);
        if (v != 1.25f || last_program != 77u)
            return fail(code++, "float getter");
    }
    {
        GLint v = 0;
        CALL_CHECK(glGetUniformiv(7, 2, &v), code++);
        if (v != -7 || last_program != 77u)
            return fail(code++, "int getter");
    }
    {
        GLuint v = 0;
        CALL_CHECK(glGetUniformuiv(7, 2, &v), code++);
        if (v != 9u || last_program != 77u)
            return fail(code++, "uint getter");
    }

    CALL_CHECK(glProgramUniform1f(7, 1, 1), code++);
    CALL_CHECK(glProgramUniform2f(7, 1, 1, 2), code++);
    CALL_CHECK(glProgramUniform3f(7, 1, 1, 2, 3), code++);
    CALL_CHECK(glProgramUniform4f(7, 1, 1, 2, 3, 4), code++);
    CALL_CHECK(glProgramUniform1i(7, 1, 1), code++);
    CALL_CHECK(glProgramUniform2i(7, 1, 1, 2), code++);
    CALL_CHECK(glProgramUniform3i(7, 1, 1, 2, 3), code++);
    CALL_CHECK(glProgramUniform4i(7, 1, 1, 2, 3, 4), code++);
    CALL_CHECK(glProgramUniform1ui(7, 1, 1), code++);
    CALL_CHECK(glProgramUniform2ui(7, 1, 1, 2), code++);
    CALL_CHECK(glProgramUniform3ui(7, 1, 1, 2, 3), code++);
    CALL_CHECK(glProgramUniform4ui(7, 1, 1, 2, 3, 4), code++);
    CALL_CHECK(glProgramUniform1fv(7, 1, 1, fv), code++);
    CALL_CHECK(glProgramUniform2fv(7, 1, 1, fv), code++);
    CALL_CHECK(glProgramUniform3fv(7, 1, 1, fv), code++);
    CALL_CHECK(glProgramUniform4fv(7, 1, 1, fv), code++);
    CALL_CHECK(glProgramUniform1iv(7, 1, 1, iv), code++);
    CALL_CHECK(glProgramUniform2iv(7, 1, 1, iv), code++);
    CALL_CHECK(glProgramUniform3iv(7, 1, 1, iv), code++);
    CALL_CHECK(glProgramUniform4iv(7, 1, 1, iv), code++);
    CALL_CHECK(glProgramUniform1uiv(7, 1, 1, uv), code++);
    CALL_CHECK(glProgramUniform2uiv(7, 1, 1, uv), code++);
    CALL_CHECK(glProgramUniform3uiv(7, 1, 1, uv), code++);
    CALL_CHECK(glProgramUniform4uiv(7, 1, 1, uv), code++);
    CALL_CHECK(glProgramUniformMatrix2fv(7, 1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glProgramUniformMatrix3fv(7, 1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glProgramUniformMatrix4fv(7, 1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glProgramUniformMatrix2x3fv(7, 1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glProgramUniformMatrix3x2fv(7, 1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glProgramUniformMatrix2x4fv(7, 1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glProgramUniformMatrix4x2fv(7, 1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glProgramUniformMatrix3x4fv(7, 1, 1, GL_FALSE, fv), code++);
    CALL_CHECK(glProgramUniformMatrix4x3fv(7, 1, 1, GL_FALSE, fv), code++);

    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (glXGetProcAddress((const GLubyte *)names[i]) == NULL)
            return fail(code++, names[i]);
    if (last_program != 77u || last_location != 1)
        return fail(code++, "native program mapping");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL uniform API parity unit test: PASS");
    return 0;
}

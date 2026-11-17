#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "shader/shader_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned reads;

static void native_interface(GLuint program, GLenum interface, GLenum pname, GLint *count) {
    (void)program;
    (void)interface;
    (void)pname;
    *count = 4;
}

static void native_resource(GLuint program, GLenum interface, GLuint index, GLsizei propCount,
                            const GLenum *props, GLsizei bufSize, GLsizei *length, GLint *values) {
    static const GLint metadata[4][3] = {
        {0x8B52, 2, 1}, {0x8B5C, 3, 1}, {0x8DC6, 5, 2}, {0x1404, 9, 1}};
    (void)program;
    (void)interface;
    (void)bufSize;
    for (GLsizei i = 0; i < propCount; ++i) {
        if (props[i] == GL_TYPE)
            values[i] = metadata[index][0];
        else if (props[i] == GL_LOCATION)
            values[i] = metadata[index][1];
        else if (props[i] == GL_ARRAY_SIZE)
            values[i] = metadata[index][2];
    }
    if (length)
        *length = propCount;
}

static void native_float(GLuint program, GLint location, GLfloat *params) {
    ++reads;
    if (program != 77u)
        return;
    if (location == 2)
        for (unsigned i = 0; i < 4; ++i)
            params[i] = (GLfloat)(i + 1);
    if (location == 3)
        for (unsigned i = 0; i < 16; ++i)
            params[i] = (GLfloat)(i + 1);
}

static void native_int(GLuint program, GLint location, GLint *params) {
    ++reads;
    if (program == 77u && location == 9)
        params[0] = -17;
}

static void native_uint(GLuint program, GLint location, GLuint *params) {
    ++reads;
    if (program == 77u && (location == 5 || location == 6)) {
        params[0] = 0xffffffffu;
        params[1] = 42u;
    }
}

#define CHECK(cond, n)                                                                             \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "robust uniform test %d\n", n);                                        \
            return n;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    struct mgl_context ctx = {0};
    struct mgl_native_dispatch native = {0};
    struct mgl_program *program;
    GLfloat fv[18];
    GLint iv[4] = {11, 12, 13, 14};
    GLuint uv[4] = {11, 12, 13, 14};
    GLdouble dv[18];
    native.glesGetProgramInterfaceiv = native_interface;
    native.glesGetProgramResourceiv = native_resource;
    native.glesGetUniformfv = native_float;
    native.glesGetUniformiv = native_int;
    native.glesGetUniformuiv = native_uint;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    CHECK(ctx.share_group != NULL, 1);
    mgl_error_init(&ctx.errors);
    mgl_context_set_current_for_test(&ctx);
    program = calloc(1, sizeof(*program));
    CHECK(program != NULL, 2);
    program->name = 7;
    program->native_name = 77;
    program->linked = GL_TRUE;
    ctx.share_group->programs = program;

    for (unsigned i = 0; i < 18; ++i)
        fv[i] = -99.0f;
    glGetnUniformfv(7, 3, 60, fv);
    CHECK(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION && fv[0] == -99.0f && reads == 0, 3);
    glGetnUniformfv(7, 3, 64, fv);
    CHECK(mgl_error_take(&ctx.errors) == GL_NO_ERROR && fv[15] == 16.0f && fv[16] == -99.0f, 4);

    glGetnUniformiv(7, 9, 4, iv);
    CHECK(mgl_error_take(&ctx.errors) == GL_NO_ERROR && iv[0] == -17, 5);
    glGetnUniformuiv(7, 6, 8, uv);
    CHECK(mgl_error_take(&ctx.errors) == GL_NO_ERROR && uv[0] == 0xffffffffu && uv[1] == 42u, 6);
    glGetnUniformdv(7, 5, 16, dv);
    CHECK(mgl_error_take(&ctx.errors) == GL_NO_ERROR && dv[0] == 4294967295.0 && dv[1] == 42.0, 7);
    glGetnUniformdv(7, 3, 128, dv);
    CHECK(mgl_error_take(&ctx.errors) == GL_NO_ERROR && dv[15] == 16.0, 8);

    for (unsigned i = 0; i < 18; ++i)
        dv[i] = -99.0;
    glGetUniformdv(7, 3, dv);
    CHECK(mgl_error_take(&ctx.errors) == GL_NO_ERROR && dv[15] == 16.0 && dv[16] == -99.0, 13);
    glGetUniformdv(7, 9, dv);
    CHECK(mgl_error_take(&ctx.errors) == GL_NO_ERROR && dv[0] == -17.0, 14);
    glGetUniformdv(7, 99, dv);
    CHECK(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION, 15);
    CHECK(glXGetProcAddress((const GLubyte *)"glGetUniformdv") != NULL, 16);

    glGetnUniformfv(7, 99, 72, fv);
    CHECK(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION, 9);
    glGetnUniformfv(7, 2, -1, fv);
    CHECK(mgl_error_take(&ctx.errors) == GL_INVALID_VALUE, 10);
    glGetnUniformfv(999, 2, 16, fv);
    CHECK(mgl_error_take(&ctx.errors) == GL_INVALID_VALUE, 11);
    CHECK(glXGetProcAddress((const GLubyte *)"glGetnUniformdv") != NULL &&
              glXGetProcAddress((const GLubyte *)"glGetnUniformuiv") != NULL,
          12);

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL robust uniform readback: PASS");
    return 0;
}

#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "shader/shader_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char expected_binary[] = {0x4d, 0x47, 0x4c, 0x42, 0x01, 0x7f};
static GLenum expected_format = 0x9abcu;
static GLint native_link_status = GL_TRUE;
static GLuint last_parameter_program;
static GLenum last_parameter_pname;
static GLint last_parameter_value;
static GLuint last_binary_program;
static GLenum last_binary_format;
static GLsizei last_binary_length;
static unsigned char last_binary[32];

static void fake_get_integerv(GLenum pname, GLint *data) {
    if (pname == GL_NUM_PROGRAM_BINARY_FORMATS)
        data[0] = 1;
    else if (pname == GL_PROGRAM_BINARY_FORMATS)
        data[0] = (GLint)expected_format;
    else
        data[0] = 0;
}

static void fake_program_parameteri(GLuint program, GLenum pname, GLint value) {
    last_parameter_program = program;
    last_parameter_pname = pname;
    last_parameter_value = value;
}

static void fake_get_program_binary(GLuint program, GLsizei buf_size, GLsizei *length,
                                    GLenum *format, void *binary) {
    GLsizei n = (GLsizei)sizeof(expected_binary);
    (void)program;
    if (n > buf_size)
        n = buf_size;
    if (binary != NULL && n > 0)
        memcpy(binary, expected_binary, (size_t)n);
    if (length != NULL)
        *length = n;
    if (format != NULL)
        *format = expected_format;
}

static void fake_program_binary(GLuint program, GLenum format, const void *binary, GLsizei length) {
    last_binary_program = program;
    last_binary_format = format;
    last_binary_length = length;
    memset(last_binary, 0, sizeof(last_binary));
    if (binary != NULL && length > 0 && (size_t)length <= sizeof(last_binary)) {
        memcpy(last_binary, binary, (size_t)length);
    }
    native_link_status = format == expected_format && length == (GLsizei)sizeof(expected_binary) &&
                                 memcmp(binary, expected_binary, sizeof(expected_binary)) == 0
                             ? GL_TRUE
                             : GL_FALSE;
}

static void fake_get_programiv(GLuint program, GLenum pname, GLint *value) {
    (void)program;
    switch (pname) {
        case GL_LINK_STATUS:
            *value = native_link_status;
            break;
        case GL_INFO_LOG_LENGTH:
            *value = 1;
            break;
        case GL_ACTIVE_ATTRIBUTES:
            *value = 0;
            break;
        case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
            *value = 0;
            break;
        case GL_PROGRAM_BINARY_LENGTH:
            *value = (GLint)sizeof(expected_binary);
            break;
        default:
            *value = 0;
            break;
    }
}

static void fake_get_program_info_log(GLuint program, GLsizei buf_size, GLsizei *length,
                                      GLchar *log) {
    (void)program;
    if (length != NULL)
        *length = 0;
    if (buf_size > 0 && log != NULL)
        log[0] = '\0';
}

static GLint fake_get_attrib_location(GLuint program, const GLchar *name) {
    (void)program;
    (void)name;
    return -1;
}

static GLint fake_get_uniform_location(GLuint program, const GLchar *name) {
    (void)program;
    (void)name;
    return -1;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "program binary failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    unsigned char binary[32];
    GLsizei length = -1;
    GLenum format = 0u;
    GLint value = 0;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetIntegerv = fake_get_integerv;
    native.glesProgramParameteri = fake_program_parameteri;
    native.glesGetProgramBinary = fake_get_program_binary;
    native.glesProgramBinary = fake_program_binary;
    native.glesGetProgramiv = fake_get_programiv;
    native.glesGetProgramInfoLog = fake_get_program_info_log;
    native.glesGetAttribLocation = fake_get_attrib_location;
    native.glesGetUniformLocation = fake_get_uniform_location;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_context_set_current_for_test(&ctx);

    program = calloc(1, sizeof(*program));
    if (program == NULL)
        return fail(2, "program allocation");
    mgl_program_init_metadata(program);
    program->name = 88u;
    program->native_name = 188u;
    program->linked = GL_TRUE;
    program->next = ctx.share_group->programs;
    ctx.share_group->programs = program;

    glProgramParameteri(program->name, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        last_parameter_program != program->native_name ||
        last_parameter_pname != GL_PROGRAM_BINARY_RETRIEVABLE_HINT ||
        last_parameter_value != GL_TRUE) {
        return fail(3, "binary hint forwarding");
    }
    glProgramParameteri(program->name, GL_PROGRAM_SEPARABLE, GL_TRUE);
    glGetProgramiv(program->name, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, &value);
    if (value != GL_TRUE)
        return fail(4, "binary hint query");
    glGetProgramiv(program->name, GL_PROGRAM_SEPARABLE, &value);
    if (value != GL_TRUE)
        return fail(5, "separable query");
    glGetProgramiv(program->name, GL_PROGRAM_BINARY_LENGTH, &value);
    if (value != (GLint)sizeof(expected_binary))
        return fail(6, "binary length query");

    glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &value);
    if (value != 1)
        return fail(7, "binary format count");
    glGetIntegerv(GL_PROGRAM_BINARY_FORMATS, &value);
    if ((GLenum)value != expected_format)
        return fail(8, "binary formats query");

    memset(binary, 0, sizeof(binary));
    glGetProgramBinary(program->name, (GLsizei)sizeof(binary), &length, &format, binary);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || length != (GLsizei)sizeof(expected_binary) ||
        format != expected_format ||
        memcmp(binary, expected_binary, sizeof(expected_binary)) != 0) {
        return fail(9, "binary retrieval");
    }

    program->linked = GL_FALSE;
    native_link_status = GL_FALSE;
    glProgramBinary(program->name, expected_format, expected_binary,
                    (GLsizei)sizeof(expected_binary));
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || !program->linked ||
        last_binary_program != program->native_name || last_binary_format != expected_format ||
        last_binary_length != (GLsizei)sizeof(expected_binary) ||
        memcmp(last_binary, expected_binary, sizeof(expected_binary)) != 0) {
        return fail(10, "binary load and link-state refresh");
    }

    glProgramParameteri(program->name, 0xdeadbeefu, GL_TRUE);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(11, "program parameter enum validation");
    glGetProgramBinary(program->name, -1, NULL, NULL, NULL);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(12, "negative buffer-size validation");
    glProgramBinary(program->name, expected_format, NULL, 1);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(13, "binary pointer validation");

    if (glXGetProcAddress((const GLubyte *)"glProgramParameteri") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetProgramBinary") == NULL ||
        glXGetProcAddress((const GLubyte *)"glProgramBinary") == NULL) {
        return fail(16, "proc-address coverage");
    }

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("program binary semantics passed");
    return 0;
}

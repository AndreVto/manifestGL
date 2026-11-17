#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "shader/shader_store.h"

#include <stdio.h>
#include <string.h>

#define TEST_BINARY_FORMAT 0xB17Au

static GLuint next_shader_native = 301u;
static GLuint next_program_native = 401u;
static unsigned binary_calls;
static GLsizei binary_last_count;
static GLuint binary_last_names[3];
static GLenum binary_last_format;
static GLsizei binary_last_length;
static GLenum pending_error;
static GLenum shader_binary_error;
static GLboolean native_compiled[3];
static const char *native_logs[3];

static GLuint fake_create_shader(GLenum type) {
    (void)type;
    return next_shader_native++;
}

static GLuint fake_create_program(void) {
    return next_program_native++;
}

static void fake_delete_shader(GLuint shader) {
    (void)shader;
}

static void fake_delete_program(GLuint program) {
    (void)program;
}

static int shader_index(GLuint shader) {
    if (shader < 301u || shader > 303u)
        return -1;
    return (int)(shader - 301u);
}

static void fake_get_integerv(GLenum pname, GLint *data) {
    if (pname == GL_NUM_SHADER_BINARY_FORMATS)
        *data = 1;
    else if (pname == GL_SHADER_BINARY_FORMATS)
        data[0] = (GLint)TEST_BINARY_FORMAT;
}

static void fake_shader_binary(GLsizei count, const GLuint *shaders, GLenum format,
                               const void *binary, GLsizei length) {
    GLsizei i;
    (void)binary;
    ++binary_calls;
    if (shader_binary_error != GL_NO_ERROR) {
        pending_error = shader_binary_error;
        shader_binary_error = GL_NO_ERROR;
    }
    binary_last_count = count;
    binary_last_format = format;
    binary_last_length = length;
    for (i = 0; i < count && i < 3; ++i)
        binary_last_names[i] = shaders[i];
}

static void fake_get_shader_iv(GLuint shader, GLenum pname, GLint *params) {
    int i = shader_index(shader);
    const char *log = (i >= 0 && native_logs[i] != NULL) ? native_logs[i] : "";
    if (pname == GL_COMPILE_STATUS)
        *params = (i >= 0 && native_compiled[i]) ? GL_TRUE : GL_FALSE;
    else if (pname == GL_INFO_LOG_LENGTH)
        *params = (GLint)strlen(log) + 1;
    else
        *params = 0;
}

static void fake_get_shader_info_log(GLuint shader, GLsizei buf_size, GLsizei *length,
                                     GLchar *info_log) {
    int i = shader_index(shader);
    const char *log = (i >= 0 && native_logs[i] != NULL) ? native_logs[i] : "";
    size_t n = strlen(log);
    size_t copy = 0u;
    if (buf_size > 0 && info_log != NULL) {
        copy = n < (size_t)(buf_size - 1) ? n : (size_t)(buf_size - 1);
        if (copy > 0u)
            memcpy(info_log, log, copy);
        info_log[copy] = '\0';
    }
    if (length != NULL)
        *length = (GLsizei)copy;
}

static GLenum fake_get_error(void) {
    GLenum out = pending_error;
    pending_error = GL_NO_ERROR;
    return out;
}

static int fail(int n, const char *msg) {
    fprintf(stderr, "shader binary failure %d: %s\n", n, msg);
    return n;
}

static int expect_error(GLenum expected, int n, const char *msg) {
    GLenum got = glGetError();
    if (got != expected) {
        fprintf(stderr, "shader binary failure %d: %s (got 0x%x expected 0x%x)\n", n, msg,
                (unsigned)got, (unsigned)expected);
        return n;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_shader *obj;
    GLuint vert, frag, vert2, program;
    GLuint pair[2];
    GLuint duplicate_type[2];
    const unsigned char bytes[4] = {1u, 2u, 3u, 4u};
    GLint value;
    GLchar log[64];
    GLsizei log_len;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesCreateShader = fake_create_shader;
    native.glesCreateProgram = fake_create_program;
    native.glesDeleteShader = fake_delete_shader;
    native.glesDeleteProgram = fake_delete_program;
    native.glesGetIntegerv = fake_get_integerv;
    native.glesShaderBinary = fake_shader_binary;
    native.glesGetShaderiv = fake_get_shader_iv;
    native.glesGetShaderInfoLog = fake_get_shader_info_log;
    native.glesGetError = fake_get_error;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_context_set_current_for_test(&ctx);

    vert = glCreateShader(GL_VERTEX_SHADER);
    frag = glCreateShader(GL_FRAGMENT_SHADER);
    vert2 = glCreateShader(GL_VERTEX_SHADER);
    program = glCreateProgram();
    if (vert == 0u || frag == 0u || vert2 == 0u || program == 0u)
        return fail(2, "object creation");

    value = -1;
    glGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &value);
    if (glGetError() != GL_NO_ERROR || value < 1)
        return fail(3, "NUM_SHADER_BINARY_FORMATS forwarding");
    {
        GLint reported_formats[2] = {-1, -1};
        if (value > 2)
            return fail(3, "unexpected shader binary formats");
        glGetIntegerv(GL_SHADER_BINARY_FORMATS, reported_formats);
        if (glGetError() != GL_NO_ERROR || (GLenum)reported_formats[0] != TEST_BINARY_FORMAT)
            return fail(4, "SHADER_BINARY_FORMATS forwarding");
    }

    pair[0] = vert;
    pair[1] = frag;
    native_compiled[0] = GL_TRUE;
    native_logs[0] = "vertex-binary-ok";
    native_compiled[1] = GL_TRUE;
    native_logs[1] = "fragment-binary-ok";
    glShaderBinary(2, pair, TEST_BINARY_FORMAT, bytes, (GLsizei)sizeof(bytes));
    if ((rc = expect_error(GL_NO_ERROR, 5, "valid ShaderBinary")) != 0 || binary_calls != 1u ||
        binary_last_count != 2 || binary_last_names[0] != 301u || binary_last_names[1] != 302u ||
        binary_last_format != TEST_BINARY_FORMAT || binary_last_length != (GLsizei)sizeof(bytes))
        return fail(5, "ShaderBinary native translation");
    value = GL_FALSE;
    glGetShaderiv(vert, GL_COMPILE_STATUS, &value);
    if (glGetError() != GL_NO_ERROR || value != GL_TRUE)
        return fail(6, "vertex compile status synchronization");
    memset(log, 0, sizeof(log));
    log_len = -1;
    glGetShaderInfoLog(frag, (GLsizei)sizeof(log), &log_len, log);
    if (glGetError() != GL_NO_ERROR || strcmp(log, "fragment-binary-ok") != 0)
        return fail(7, "fragment log synchronization");

    glShaderBinary(2, pair, TEST_BINARY_FORMAT + 1u, bytes, (GLsizei)sizeof(bytes));
    if ((rc = expect_error(GL_INVALID_ENUM, 8, "unsupported binary format")) != 0 ||
        binary_calls != 1u)
        return fail(8, "unsupported format reached native");
    glShaderBinary(-1, pair, TEST_BINARY_FORMAT, bytes, (GLsizei)sizeof(bytes));
    if ((rc = expect_error(GL_INVALID_VALUE, 9, "negative count")) != 0 || binary_calls != 1u)
        return rc;
    glShaderBinary(1, pair, TEST_BINARY_FORMAT, bytes, -1);
    if ((rc = expect_error(GL_INVALID_VALUE, 10, "negative length")) != 0 || binary_calls != 1u)
        return rc;
    glShaderBinary(1, NULL, TEST_BINARY_FORMAT, bytes, (GLsizei)sizeof(bytes));
    if ((rc = expect_error(GL_INVALID_VALUE, 11, "null shader list")) != 0 || binary_calls != 1u)
        return rc;

    pair[0] = program;
    glShaderBinary(1, pair, TEST_BINARY_FORMAT, bytes, (GLsizei)sizeof(bytes));
    if ((rc = expect_error(GL_INVALID_OPERATION, 12, "program handle")) != 0 || binary_calls != 1u)
        return rc;
    pair[0] = 99999u;
    glShaderBinary(1, pair, TEST_BINARY_FORMAT, bytes, (GLsizei)sizeof(bytes));
    if ((rc = expect_error(GL_INVALID_VALUE, 13, "unknown handle")) != 0 || binary_calls != 1u)
        return rc;

    duplicate_type[0] = vert;
    duplicate_type[1] = vert2;
    glShaderBinary(2, duplicate_type, TEST_BINARY_FORMAT, bytes, (GLsizei)sizeof(bytes));
    if ((rc = expect_error(GL_INVALID_OPERATION, 14, "duplicate shader type")) != 0 ||
        binary_calls != 1u)
        return rc;

    pair[0] = vert;
    native_compiled[0] = GL_FALSE;
    native_logs[0] = "bad-binary";
    shader_binary_error = GL_INVALID_VALUE;
    glShaderBinary(1, pair, TEST_BINARY_FORMAT, bytes, (GLsizei)sizeof(bytes));
    if ((rc = expect_error(GL_INVALID_VALUE, 15, "native binary rejection")) != 0 ||
        binary_calls != 2u)
        return fail(15, "native rejection propagation");
    value = GL_TRUE;
    glGetShaderiv(vert, GL_COMPILE_STATUS, &value);
    if (glGetError() != GL_NO_ERROR || value != GL_FALSE)
        return fail(16, "failed binary compile status synchronization");
    memset(log, 0, sizeof(log));
    glGetShaderInfoLog(vert, (GLsizei)sizeof(log), NULL, log);
    if (glGetError() != GL_NO_ERROR || strcmp(log, "bad-binary") != 0)
        return fail(17, "failed binary log synchronization");
    (void)pthread_mutex_lock(&ctx.share_group->lock);
    obj = mgl_shader_find(&ctx, vert);
    if (obj == NULL || obj->translated_source != NULL) {
        (void)pthread_mutex_unlock(&ctx.share_group->lock);
        return fail(18, "binary load did not invalidate translated source cache");
    }
    (void)pthread_mutex_unlock(&ctx.share_group->lock);

    if (glXGetProcAddress((const GLubyte *)"glShaderBinary") == NULL)
        return fail(19, "proc-address exposure");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

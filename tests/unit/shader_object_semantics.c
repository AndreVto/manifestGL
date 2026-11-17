#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "shader/shader_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint next_shader_native = 101u;
static GLuint next_program_native = 201u;
static unsigned attach_calls;
static unsigned detach_calls;
static unsigned delete_shader_calls;
static unsigned delete_program_calls;
static GLuint last_attach_program;
static GLuint last_attach_shader;

static GLuint fake_create_shader(GLenum type) {
    (void)type;
    return next_shader_native++;
}

static GLuint fake_create_program(void) {
    return next_program_native++;
}

static void fake_attach_shader(GLuint program, GLuint shader) {
    ++attach_calls;
    last_attach_program = program;
    last_attach_shader = shader;
}

static void fake_detach_shader(GLuint program, GLuint shader) {
    ++detach_calls;
    last_attach_program = program;
    last_attach_shader = shader;
}

static void fake_delete_shader(GLuint shader) {
    (void)shader;
    ++delete_shader_calls;
}

static void fake_delete_program(GLuint program) {
    (void)program;
    ++delete_program_calls;
}

static char *copy_text(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n + 1u);
    if (out != NULL)
        memcpy(out, s, n + 1u);
    return out;
}

static int fail(int n, const char *msg) {
    fprintf(stderr, "shader object semantics failure %d: %s\n", n, msg);
    return n;
}

static int expect_error(GLenum expected, int n, const char *msg) {
    GLenum got = glGetError();
    if (got != expected) {
        fprintf(stderr, "shader object semantics failure %d: %s (got 0x%x expected 0x%x)\n", n, msg,
                (unsigned)got, (unsigned)expected);
        return n;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_shader *shader_obj;
    GLuint shader, program;
    const GLchar *source = "void main() { }";
    GLint value;
    GLsizei length;
    GLchar text[64];
    GLuint attached[2];
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesCreateShader = fake_create_shader;
    native.glesCreateProgram = fake_create_program;
    native.glesAttachShader = fake_attach_shader;
    native.glesDetachShader = fake_detach_shader;
    native.glesDeleteShader = fake_delete_shader;
    native.glesDeleteProgram = fake_delete_program;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_context_set_current_for_test(&ctx);

    shader = glCreateShader(GL_VERTEX_SHADER);
    program = glCreateProgram();
    if (shader == 0u || program == 0u || shader == program)
        return fail(2, "shared shader/program namespace creation");
    {
        struct mgl_program *created = mgl_program_find(&ctx, program);
        if (created == NULL || created->compat_uniform_fog_coord_explicit != -1)
            return fail(42, "new user program has a spurious fog uniform location");
    }

    (void)pthread_mutex_lock(&ctx.share_group->lock);
    shader_obj = mgl_shader_find(&ctx, shader);
    if (shader_obj == NULL) {
        (void)pthread_mutex_unlock(&ctx.share_group->lock);
        return fail(3, "shader lookup");
    }
    shader_obj->compiled = GL_TRUE;
    shader_obj->translated_source = copy_text("translated-before");
    shader_obj->info_log = copy_text("compile-log-before");
    if (shader_obj->translated_source == NULL || shader_obj->info_log == NULL) {
        (void)pthread_mutex_unlock(&ctx.share_group->lock);
        return fail(4, "seed allocation");
    }
    (void)pthread_mutex_unlock(&ctx.share_group->lock);

    glShaderSource(shader, 1, &source, NULL);
    if ((rc = expect_error(GL_NO_ERROR, 5, "ShaderSource valid")) != 0)
        return rc;
    value = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
    if ((rc = expect_error(GL_NO_ERROR, 6, "GetShaderiv after source replacement")) != 0 ||
        value != GL_TRUE)
        return fail(6, "ShaderSource changed previous compile status");
    memset(text, 0, sizeof(text));
    length = -1;
    glGetShaderInfoLog(shader, (GLsizei)sizeof(text), &length, text);
    if ((rc = expect_error(GL_NO_ERROR, 7, "GetShaderInfoLog")) != 0 ||
        strcmp(text, "compile-log-before") != 0)
        return fail(7, "ShaderSource changed previous compile log");
    memset(text, 0, sizeof(text));
    length = -1;
    glGetShaderSource(shader, (GLsizei)sizeof(text), &length, text);
    if ((rc = expect_error(GL_NO_ERROR, 8, "GetShaderSource")) != 0 || strcmp(text, source) != 0)
        return fail(8, "ShaderSource did not replace source text");
    (void)pthread_mutex_lock(&ctx.share_group->lock);
    shader_obj = mgl_shader_find(&ctx, shader);
    if (shader_obj == NULL || shader_obj->translated_source == NULL ||
        strcmp(shader_obj->translated_source, "translated-before") != 0) {
        (void)pthread_mutex_unlock(&ctx.share_group->lock);
        return fail(9, "ShaderSource changed previous compiled translation");
    }
    (void)pthread_mutex_unlock(&ctx.share_group->lock);

    glShaderSource(shader, 0, NULL, NULL);
    if ((rc = expect_error(GL_NO_ERROR, 10, "ShaderSource count zero")) != 0)
        return rc;
    value = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
    if (glGetError() != GL_NO_ERROR || value != GL_TRUE)
        return fail(11, "zero-count ShaderSource changed compile status");

    value = 12345;
    glGetShaderiv(program, GL_COMPILE_STATUS, &value);
    if ((rc = expect_error(GL_INVALID_OPERATION, 12, "GetShaderiv program object")) != 0 ||
        value != 12345)
        return fail(12, "GetShaderiv wrong-kind handling/output mutation");
    glShaderSource(program, 1, &source, NULL);
    if ((rc = expect_error(GL_INVALID_OPERATION, 13, "ShaderSource program object")) != 0)
        return rc;
    glCompileShader(program);
    if ((rc = expect_error(GL_INVALID_OPERATION, 14, "CompileShader program object")) != 0)
        return rc;
    length = 91;
    strcpy(text, "unchanged");
    glGetShaderInfoLog(program, (GLsizei)sizeof(text), &length, text);
    if ((rc = expect_error(GL_INVALID_OPERATION, 15, "GetShaderInfoLog program object")) != 0 ||
        length != 91 || strcmp(text, "unchanged") != 0)
        return fail(15, "GetShaderInfoLog mutated output on wrong-kind error");
    length = 92;
    strcpy(text, "unchanged");
    glGetShaderSource(program, (GLsizei)sizeof(text), &length, text);
    if ((rc = expect_error(GL_INVALID_OPERATION, 16, "GetShaderSource program object")) != 0 ||
        length != 92 || strcmp(text, "unchanged") != 0)
        return fail(16, "GetShaderSource mutated output on wrong-kind error");
    glDeleteShader(program);
    if ((rc = expect_error(GL_INVALID_OPERATION, 17, "DeleteShader program object")) != 0)
        return rc;

    glShaderSource(99999u, 1, &source, NULL);
    if ((rc = expect_error(GL_INVALID_VALUE, 18, "ShaderSource unknown name")) != 0)
        return rc;
    glDeleteShader(99999u);
    if ((rc = expect_error(GL_INVALID_VALUE, 19, "DeleteShader unknown name")) != 0)
        return rc;
    glDeleteShader(0u);
    if ((rc = expect_error(GL_NO_ERROR, 20, "DeleteShader zero")) != 0)
        return rc;

    glAttachShader(shader, shader);
    if ((rc = expect_error(GL_INVALID_OPERATION, 21, "AttachShader shader as program")) != 0)
        return rc;
    glAttachShader(program, program);
    if ((rc = expect_error(GL_INVALID_OPERATION, 22, "AttachShader program as shader")) != 0)
        return rc;
    glAttachShader(99999u, shader);
    if ((rc = expect_error(GL_INVALID_VALUE, 23, "AttachShader unknown program")) != 0)
        return rc;
    glAttachShader(program, 99999u);
    if ((rc = expect_error(GL_INVALID_VALUE, 24, "AttachShader unknown shader")) != 0)
        return rc;

    glAttachShader(program, shader);
    if ((rc = expect_error(GL_NO_ERROR, 25, "AttachShader valid")) != 0 || attach_calls != 1u ||
        last_attach_program != 201u || last_attach_shader != 101u)
        return fail(25, "AttachShader native translation");
    glAttachShader(program, shader);
    if ((rc = expect_error(GL_INVALID_OPERATION, 26, "AttachShader duplicate")) != 0 ||
        attach_calls != 1u)
        return fail(26, "duplicate AttachShader reached native");
    value = -1;
    glGetProgramiv(program, GL_ATTACHED_SHADERS, &value);
    if (glGetError() != GL_NO_ERROR || value != 1)
        return fail(27, "attached shader count");
    attached[0] = 0u;
    length = -1;
    glGetAttachedShaders(program, 1, &length, attached);
    if (glGetError() != GL_NO_ERROR || length != 1 || attached[0] != shader)
        return fail(28, "GetAttachedShaders interface names");

    glDetachShader(program, shader);
    if ((rc = expect_error(GL_NO_ERROR, 29, "DetachShader valid")) != 0 || detach_calls != 1u ||
        last_attach_program != 201u || last_attach_shader != 101u)
        return fail(29, "DetachShader native translation");
    glDetachShader(program, shader);
    if ((rc = expect_error(GL_INVALID_OPERATION, 30, "DetachShader unattached")) != 0 ||
        detach_calls != 1u)
        return fail(30, "unattached DetachShader reached native");

    value = 777;
    glGetProgramiv(shader, GL_LINK_STATUS, &value);
    if ((rc = expect_error(GL_INVALID_OPERATION, 31, "GetProgramiv shader object")) != 0 ||
        value != 777)
        return fail(31, "GetProgramiv wrong-kind handling/output mutation");
    length = 93;
    strcpy(text, "unchanged");
    glGetProgramInfoLog(shader, (GLsizei)sizeof(text), &length, text);
    if ((rc = expect_error(GL_INVALID_OPERATION, 32, "GetProgramInfoLog shader object")) != 0 ||
        length != 93 || strcmp(text, "unchanged") != 0)
        return fail(32, "GetProgramInfoLog mutated output on wrong-kind error");
    length = 94;
    attached[0] = 0xabcdu;
    glGetAttachedShaders(shader, 1, &length, attached);
    if ((rc = expect_error(GL_INVALID_OPERATION, 33, "GetAttachedShaders shader object")) != 0 ||
        length != 94 || attached[0] != 0xabcdu)
        return fail(33, "GetAttachedShaders mutated output on wrong-kind error");
    glUseProgram(shader);
    if ((rc = expect_error(GL_INVALID_OPERATION, 34, "UseProgram shader object")) != 0)
        return rc;
    glDeleteProgram(shader);
    if ((rc = expect_error(GL_INVALID_OPERATION, 35, "DeleteProgram shader object")) != 0)
        return rc;
    glDeleteProgram(99999u);
    if ((rc = expect_error(GL_INVALID_VALUE, 36, "DeleteProgram unknown name")) != 0)
        return rc;
    glDeleteProgram(0u);
    if ((rc = expect_error(GL_NO_ERROR, 37, "DeleteProgram zero")) != 0)
        return rc;

    if (glIsShader(program) != GL_FALSE || glIsProgram(shader) != GL_FALSE ||
        glGetError() != GL_NO_ERROR)
        return fail(38, "IsShader/IsProgram cross-kind queries");

    if (glXGetProcAddress((const GLubyte *)"glShaderSource") == NULL ||
        glXGetProcAddress((const GLubyte *)"glAttachShader") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetProgramiv") == NULL)
        return fail(39, "proc-address exposure");

    {
        const GLenum stages[] = {GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER};
        for (unsigned i = 0; i < 2u; ++i) {
            GLuint tess = glCreateShader(stages[i]);
            GLint type = 0;
            if (!tess || glGetError() != GL_NO_ERROR)
                return fail(43, "tessellation shader creation");
            glGetShaderiv(tess, GL_SHADER_TYPE, &type);
            if (glGetError() != GL_NO_ERROR || (GLenum)type != stages[i])
                return fail(44, "tessellation shader type");
            glDeleteShader(tess);
            if (glGetError() != GL_NO_ERROR)
                return fail(45, "tessellation shader deletion");
        }
    }
    glDeleteShader(shader);
    if (glGetError() != GL_NO_ERROR || delete_shader_calls != 3u)
        return fail(40, "DeleteShader valid");
    glDeleteProgram(program);
    if (glGetError() != GL_NO_ERROR || delete_program_calls != 1u)
        return fail(41, "DeleteProgram valid");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

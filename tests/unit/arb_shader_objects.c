#include "mgl/gl.h"
#include "core/context.h"
#include "shader/shader_store.h"
#include "abi/proc_address.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_shader;
    GLuint next_program;
    GLuint last_attached_program;
    GLuint last_attached_shader;
    GLuint last_detached_program;
    GLuint last_detached_shader;
    GLuint last_used_program;
    unsigned delete_shader_calls;
    unsigned delete_program_calls;
    unsigned attach_calls;
    unsigned detach_calls;
    unsigned use_calls;
} cap;

static GLuint fake_create_shader(GLenum type) {
    (void)type;
    return ++cap.next_shader + 100u;
}

static void fake_delete_shader(GLuint shader) {
    (void)shader;
    ++cap.delete_shader_calls;
}

static GLuint fake_create_program(void) {
    return ++cap.next_program + 200u;
}

static void fake_delete_program(GLuint program) {
    (void)program;
    ++cap.delete_program_calls;
}

static void fake_attach_shader(GLuint program, GLuint shader) {
    cap.last_attached_program = program;
    cap.last_attached_shader = shader;
    ++cap.attach_calls;
}

static void fake_detach_shader(GLuint program, GLuint shader) {
    cap.last_detached_program = program;
    cap.last_detached_shader = shader;
    ++cap.detach_calls;
}

static void fake_use_program(GLuint program) {
    cap.last_used_program = program;
    ++cap.use_calls;
}

static void fake_get_programiv(GLuint program, GLenum pname, GLint *value) {
    (void)program;
    if (pname == GL_ACTIVE_UNIFORMS)
        *value = 3;
    else if (pname == GL_ACTIVE_UNIFORM_MAX_LENGTH)
        *value = 17;
    else
        *value = 0;
}

static char *owned_text(const char *text) {
    size_t size = strlen(text) + 1u;
    char *copy = malloc(size);
    if (copy != NULL)
        memcpy(copy, text, size);
    return copy;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "ARB shader objects failure %d: %s\n", n, message);
    return n;
}

static int expect_error(GLenum expected, int n, const char *message) {
    GLenum actual = glGetError();
    if (actual != expected) {
        fprintf(stderr, "ARB shader objects failure %d: %s (got 0x%04x expected 0x%04x)\n", n,
                message, (unsigned)actual, (unsigned)expected);
        return n;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLhandleARB shader;
    GLhandleARB program;
    GLuint core_shader;
    struct mgl_shader *shader_state;
    struct mgl_program *program_state;
    const GLchar *source = "void main(){}";
    GLint value;
    GLfloat fvalue;
    GLsizei count;
    GLhandleARB attached[2];
    GLsizei length;
    GLcharARB log[32];
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesCreateShader = fake_create_shader;
    native.glesDeleteShader = fake_delete_shader;
    native.glesCreateProgram = fake_create_program;
    native.glesDeleteProgram = fake_delete_program;
    native.glesAttachShader = fake_attach_shader;
    native.glesDetachShader = fake_detach_shader;
    native.glesUseProgram = fake_use_program;
    native.glesGetProgramiv = fake_get_programiv;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share-group creation");
    mgl_error_init(&ctx.errors);
    mgl_context_set_current_for_test(&ctx);

    shader = glCreateShaderObjectARB(GL_VERTEX_SHADER);
    program = glCreateProgramObjectARB();
    core_shader = glCreateShader(GL_FRAGMENT_SHADER);
    if (shader == 0u || program == 0u || core_shader == 0u || shader == program ||
        shader == core_shader || program == core_shader)
        return fail(2, "shared shader/program namespace");
    if ((rc = expect_error(GL_NO_ERROR, 3, "object creation")) != 0)
        return rc;

    if (mgl_proc_address("glCreateShaderObjectARB") == NULL ||
        mgl_proc_address("glCreateProgramObjectARB") == NULL ||
        mgl_proc_address("glDeleteObjectARB") == NULL ||
        mgl_proc_address("glAttachObjectARB") == NULL ||
        mgl_proc_address("glDetachObjectARB") == NULL ||
        mgl_proc_address("glUseProgramObjectARB") == NULL ||
        mgl_proc_address("glGetHandleARB") == NULL ||
        mgl_proc_address("glGetObjectParameterfvARB") == NULL ||
        mgl_proc_address("glGetObjectParameterivARB") == NULL ||
        mgl_proc_address("glGetInfoLogARB") == NULL ||
        mgl_proc_address("glGetAttachedObjectsARB") == NULL)
        return fail(4, "proc-address coverage");

    glShaderSource((GLuint)shader, 1, &source, NULL);
    if ((rc = expect_error(GL_NO_ERROR, 5, "shader source")) != 0)
        return rc;

    (void)pthread_mutex_lock(&ctx.share_group->lock);
    shader_state = mgl_shader_find(&ctx, (GLuint)shader);
    program_state = mgl_program_find(&ctx, (GLuint)program);
    if (shader_state == NULL || program_state == NULL) {
        (void)pthread_mutex_unlock(&ctx.share_group->lock);
        return fail(6, "object lookup");
    }
    shader_state->info_log = owned_text("shader-log");
    program_state->info_log = owned_text("program-log");
    program_state->linked = GL_TRUE;
    if (shader_state->info_log == NULL || program_state->info_log == NULL) {
        (void)pthread_mutex_unlock(&ctx.share_group->lock);
        return fail(7, "log allocation");
    }
    (void)pthread_mutex_unlock(&ctx.share_group->lock);

    value = -1;
    glGetObjectParameterivARB(shader, GL_OBJECT_TYPE_ARB, &value);
    if (value != (GLint)GL_SHADER_OBJECT_ARB)
        return fail(8, "shader object type");
    glGetObjectParameterivARB(shader, GL_OBJECT_SUBTYPE_ARB, &value);
    if (value != (GLint)GL_VERTEX_SHADER)
        return fail(9, "shader subtype");
    glGetObjectParameterivARB(shader, GL_OBJECT_SHADER_SOURCE_LENGTH_ARB, &value);
    if (value != (GLint)(strlen(source) + 1u))
        return fail(10, "shader source length");
    glGetObjectParameterivARB(shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &value);
    if (value != 11)
        return fail(11, "shader info-log length");

    fvalue = -1.0f;
    glGetObjectParameterfvARB(program, GL_OBJECT_TYPE_ARB, &fvalue);
    if (fvalue != (GLfloat)GL_PROGRAM_OBJECT_ARB)
        return fail(12, "program object type float query");
    glGetObjectParameterivARB(program, GL_OBJECT_ACTIVE_UNIFORMS_ARB, &value);
    if (value != 3)
        return fail(13, "active uniform count");
    glGetObjectParameterivARB(program, GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB, &value);
    if (value != 17)
        return fail(14, "active uniform max length");

    value = 12345;
    glGetObjectParameterivARB(program, GL_OBJECT_SUBTYPE_ARB, &value);
    if ((rc = expect_error(GL_INVALID_OPERATION, 15, "wrong-type object query")) != 0)
        return rc;
    if (value != 12345)
        return fail(16, "query output changed on error");

    value = 23456;
    glGetObjectParameterivARB(99999u, GL_OBJECT_TYPE_ARB, &value);
    if ((rc = expect_error(GL_INVALID_VALUE, 17, "unknown object query")) != 0)
        return rc;
    if (value != 23456)
        return fail(18, "unknown query output changed");

    length = -1;
    memset(log, 0, sizeof(log));
    glGetInfoLogARB(shader, (GLsizei)sizeof(log), &length, log);
    if (length != 10 || strcmp(log, "shader-log") != 0)
        return fail(19, "shader info log");
    glGetInfoLogARB(program, (GLsizei)sizeof(log), &length, log);
    if (length != 11 || strcmp(log, "program-log") != 0)
        return fail(20, "program info log");

    length = 7;
    log[0] = 'X';
    glGetInfoLogARB(99999u, (GLsizei)sizeof(log), &length, log);
    if ((rc = expect_error(GL_INVALID_VALUE, 21, "unknown info-log handle")) != 0)
        return rc;
    if (length != 7 || log[0] != 'X')
        return fail(22, "info-log outputs changed on error");

    glAttachObjectARB(shader, program);
    if ((rc = expect_error(GL_INVALID_OPERATION, 23, "wrong attach object types")) != 0)
        return rc;
    glAttachObjectARB(program, 99999u);
    if ((rc = expect_error(GL_INVALID_VALUE, 24, "unknown attach handle")) != 0)
        return rc;

    glAttachObjectARB(program, shader);
    if ((rc = expect_error(GL_NO_ERROR, 25, "valid attach")) != 0)
        return rc;
    if (cap.attach_calls != 1u)
        return fail(26, "native attach count");
    value = -1;
    glGetObjectParameterivARB(program, GL_OBJECT_ATTACHED_OBJECTS_ARB, &value);
    if (value != 1)
        return fail(27, "attached-object count query");
    count = -1;
    attached[0] = attached[1] = 0u;
    glGetAttachedObjectsARB(program, 2, &count, attached);
    if (count != 1 || attached[0] != shader)
        return fail(28, "attached-object list");

    glAttachObjectARB(program, shader);
    if ((rc = expect_error(GL_INVALID_OPERATION, 29, "duplicate attach")) != 0)
        return rc;

    count = 9;
    attached[0] = 777u;
    glGetAttachedObjectsARB(shader, 1, &count, attached);
    if ((rc = expect_error(GL_INVALID_OPERATION, 30, "attached query wrong type")) != 0)
        return rc;
    if (count != 9 || attached[0] != 777u)
        return fail(31, "attached-query outputs changed on error");

    glDeleteObjectARB(shader);
    if (cap.delete_shader_calls != 0u)
        return fail(32, "attached shader deleted too early");
    value = GL_FALSE;
    glGetObjectParameterivARB(shader, GL_OBJECT_DELETE_STATUS_ARB, &value);
    if (value != GL_TRUE)
        return fail(33, "shader delete-pending status");

    glDetachObjectARB(program, shader);
    if ((rc = expect_error(GL_NO_ERROR, 34, "valid detach")) != 0)
        return rc;
    if (cap.detach_calls != 1u || cap.delete_shader_calls != 1u)
        return fail(35, "deferred shader deletion");

    value = 34567;
    glGetObjectParameterivARB(shader, GL_OBJECT_TYPE_ARB, &value);
    if ((rc = expect_error(GL_INVALID_VALUE, 36, "deleted shader handle")) != 0)
        return rc;
    if (value != 34567)
        return fail(37, "deleted-handle query output changed");

    glUseProgramObjectARB((GLhandleARB)core_shader);
    if ((rc = expect_error(GL_INVALID_OPERATION, 38, "shader passed as program")) != 0)
        return rc;
    glUseProgramObjectARB(program);
    if ((rc = expect_error(GL_NO_ERROR, 39, "use linked program")) != 0)
        return rc;
    if (glGetHandleARB(GL_PROGRAM_OBJECT_ARB) != program || cap.use_calls != 1u)
        return fail(40, "current program handle");
    if (glGetHandleARB(GL_OBJECT_TYPE_ARB) != 0u)
        return fail(41, "invalid GetHandle pname");
    if ((rc = expect_error(GL_NO_ERROR, 42, "invalid GetHandle pname must not error")) != 0)
        return rc;

    glDeleteObjectARB(program);
    if (cap.delete_program_calls != 0u)
        return fail(43, "current program deleted too early");
    value = GL_FALSE;
    glGetObjectParameterivARB(program, GL_OBJECT_DELETE_STATUS_ARB, &value);
    if (value != GL_TRUE)
        return fail(44, "program delete-pending status");
    glUseProgramObjectARB(0u);
    if ((rc = expect_error(GL_NO_ERROR, 45, "unbind program")) != 0)
        return rc;
    if (cap.last_used_program != 0u || cap.delete_program_calls != 1u ||
        glGetHandleARB(GL_PROGRAM_OBJECT_ARB) != 0u)
        return fail(46, "deferred program deletion");

    glDeleteObjectARB(99999u);
    if ((rc = expect_error(GL_INVALID_VALUE, 47, "delete unknown handle")) != 0)
        return rc;
    glDeleteObjectARB(0u);
    if ((rc = expect_error(GL_NO_ERROR, 48, "delete zero handle")) != 0)
        return rc;

    glDeleteObjectARB((GLhandleARB)core_shader);
    if ((rc = expect_error(GL_NO_ERROR, 49, "delete remaining shader")) != 0)
        return rc;
    if (cap.delete_shader_calls != 2u)
        return fail(50, "remaining shader deletion");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("arb shader objects tests passed");
    return 0;
}

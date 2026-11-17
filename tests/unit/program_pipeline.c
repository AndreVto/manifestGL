#include "mgl/gl.h"
#include "core/context.h"
#include "shader/shader_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint next_name = 250u, native_binding, stages[6], active_program;
static int uniform_writes;
static unsigned binds, sets, deletes, validates;
static const GLenum stage_enums[6] = {GL_VERTEX_SHADER,          GL_FRAGMENT_SHADER,
                                      GL_GEOMETRY_SHADER,        GL_TESS_CONTROL_SHADER,
                                      GL_TESS_EVALUATION_SHADER, GL_COMPUTE_SHADER};
static const GLbitfield stage_bits[6] = {1u, 2u, 4u, 8u, 16u, 32u};

static void native_gen(GLsizei n, GLuint *names) {
    for (GLsizei i = 0; i < n; ++i)
        names[i] = next_name++;
}

static void native_bind(GLuint name) {
    native_binding = name;
    ++binds;
}

static void native_delete(GLsizei n, const GLuint *names) {
    (void)names;
    deletes += (unsigned)n;
}

static void native_stages(GLuint pipeline, GLbitfield bits, GLuint program) {
    (void)pipeline;
    ++sets;
    for (unsigned i = 0; i < 6u; ++i)
        if (bits == GL_ALL_SHADER_BITS || (bits & stage_bits[i]))
            stages[i] = (program == 777u && (i == 0u || i == 1u)) ? program : 0u;
}

static void native_active(GLuint pipeline, GLuint program) {
    (void)pipeline;
    active_program = program;
}

static void native_validate(GLuint pipeline) {
    (void)pipeline;
    ++validates;
}

static void native_query(GLuint pipeline, GLenum pname, GLint *params) {
    (void)pipeline;
    if (pname == GL_ACTIVE_PROGRAM) {
        *params = (GLint)active_program;
        return;
    }
    if (pname == GL_VALIDATE_STATUS) {
        *params = 1;
        return;
    }
    if (pname == GL_INFO_LOG_LENGTH) {
        *params = 1;
        return;
    }
    for (unsigned i = 0; i < 6u; ++i)
        if (pname == stage_enums[i]) {
            *params = (GLint)stages[i];
            return;
        }
    *params = 0;
}

static void native_log(GLuint pipeline, GLsizei size, GLsizei *length, GLchar *log) {
    (void)pipeline;
    if (length)
        *length = 0;
    if (size && log)
        *log = 0;
}

static GLboolean native_is(GLuint name) {
    return name != 0u;
}

static void native_uniform(GLint location, GLint value) {
    if (location == 3 && value == 9)
        ++uniform_writes;
}

static int failed(int where) {
    fprintf(stderr, "program pipeline test failure %d\n", where);
    return where;
}

#define TEST(c, n)                                                                                 \
    do {                                                                                           \
        if (!(c))                                                                                  \
            return failed(n);                                                                      \
    } while (0)

int main(void) {
    struct mgl_context ctx = {0};
    struct mgl_native_dispatch native = {0};
    struct mgl_program *program;
    GLuint reserved, created, removed[2];
    GLint value;
    GLchar log[4];

    native.glesUniform1i = native_uniform;
    native.glesGenProgramPipelines = native_gen;
    native.glesDeleteProgramPipelines = native_delete;
    native.glesBindProgramPipeline = native_bind;
    native.glesIsProgramPipeline = native_is;
    native.glesUseProgramStages = native_stages;
    native.glesActiveShaderProgram = native_active;
    native.glesValidateProgramPipeline = native_validate;
    native.glesGetProgramPipelineiv = native_query;
    native.glesGetProgramPipelineInfoLog = native_log;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    TEST(ctx.share_group != NULL, 1);
    mgl_error_init(&ctx.errors);
    mgl_context_set_current_for_test(&ctx);
    program = calloc(1, sizeof(*program));
    TEST(program != NULL, 2);
    program->name = 42u;
    program->native_name = 777u;
    program->linked = GL_TRUE;
    program->separable = GL_TRUE;
    program->linked_separable = GL_TRUE;
    ctx.share_group->programs = program;

    glGenProgramPipelines(1, &reserved);
    TEST(reserved == 250u && !glIsProgramPipeline(reserved), 3);
    glBindProgramPipeline(reserved);
    glGetIntegerv(GL_PROGRAM_PIPELINE_BINDING, &value);
    TEST(value == (GLint)reserved && glIsProgramPipeline(reserved) && native_binding == reserved,
         4);
    glUseProgramStages(reserved, GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, program->name);
    glGetProgramPipelineiv(reserved, GL_VERTEX_SHADER, &value);
    TEST(value == 42 && stages[0] == 777u && program->use_count == 2, 5);
    glGetProgramPipelineiv(reserved, GL_FRAGMENT_SHADER, &value);
    TEST(value == 42 && sets == 1, 6);
    glUseProgramStages(reserved, GL_FRAGMENT_SHADER_BIT, 0u);
    glGetProgramPipelineiv(reserved, GL_FRAGMENT_SHADER, &value);
    TEST(value == 0 && program->use_count == 1, 7);

    glActiveShaderProgram(reserved, program->name);
    glGetProgramPipelineiv(reserved, GL_ACTIVE_PROGRAM, &value);
    TEST(value == 42 && active_program == 777u && program->use_count == 2, 8);
    glUniform1i(3, 9);
    TEST(uniform_writes == 1 && mgl_error_take(&ctx.errors) == GL_NO_ERROR, 19);
    glActiveShaderProgram(reserved, 0u);
    glGetProgramPipelineiv(reserved, GL_ACTIVE_PROGRAM, &value);
    TEST(value == 0 && program->use_count == 1, 22);
    glActiveShaderProgram(reserved, program->name);
    glUseProgramStages(reserved, GL_VERTEX_SHADER_BIT, 999u);
    TEST(mgl_error_take(&ctx.errors) == GL_INVALID_VALUE, 23);
    glValidateProgramPipeline(reserved);
    glGetProgramPipelineiv(reserved, GL_VALIDATE_STATUS, &value);
    TEST(value == 1 && validates == 1, 9);
    glGetProgramPipelineInfoLog(reserved, sizeof(log), NULL, log);
    TEST(log[0] == 0 && mgl_error_take(&ctx.errors) == GL_NO_ERROR, 10);

    glUseProgramStages(reserved, 0x80000000u, 0u);
    TEST(mgl_error_take(&ctx.errors) == GL_INVALID_VALUE && sets == 2, 11);
    program->linked_separable = GL_FALSE;
    glUseProgramStages(reserved, GL_VERTEX_SHADER_BIT, program->name);
    TEST(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION, 12);
    program->linked_separable = GL_TRUE;
    glBindProgramPipeline(999u);
    TEST(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION && ctx.current_pipeline == reserved,
         13);
    glGetProgramPipelineiv(reserved, 0xdead, &value);
    TEST(mgl_error_take(&ctx.errors) == GL_INVALID_ENUM, 14);

    glCreateProgramPipelines(1, &created);
    TEST(created == 251u && glIsProgramPipeline(created) && native_binding == reserved, 15);
    removed[0] = reserved;
    removed[1] = created;
    glDeleteProgramPipelines(2, removed);
    TEST(ctx.current_pipeline == 0u && native_binding == 0u && program->use_count == 0, 16);
    TEST(!glIsProgramPipeline(reserved) && deletes == 2u, 17);
    TEST(mgl_error_take(&ctx.errors) == GL_NO_ERROR, 18);
    {
        const GLchar *text = "void main() {}";
        TEST(glCreateShaderProgramv(GL_VERTEX_SHADER, -1, &text) == 0u &&
                 mgl_error_take(&ctx.errors) == GL_INVALID_VALUE,
             20);
        TEST(glCreateShaderProgramv(0xffffu, 1, &text) == 0u &&
                 mgl_error_take(&ctx.errors) == GL_INVALID_ENUM,
             21);
    }
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

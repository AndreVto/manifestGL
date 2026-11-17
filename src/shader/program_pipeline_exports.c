#include "mgl/gl.h"
#include "core/context.h"
#include "shader/shader_store.h"

#include <stdlib.h>
#include <string.h>

static const GLbitfield pipeline_stage_bits[6] = {
    GL_VERTEX_SHADER_BIT,       GL_FRAGMENT_SHADER_BIT,        GL_GEOMETRY_SHADER_BIT,
    GL_TESS_CONTROL_SHADER_BIT, GL_TESS_EVALUATION_SHADER_BIT, GL_COMPUTE_SHADER_BIT};

static const GLenum pipeline_stage_enums[6] = {GL_VERTEX_SHADER,          GL_FRAGMENT_SHADER,
                                               GL_GEOMETRY_SHADER,        GL_TESS_CONTROL_SHADER,
                                               GL_TESS_EVALUATION_SHADER, GL_COMPUTE_SHADER};

static struct mgl_pipeline *find_pipeline(struct mgl_context *ctx, GLuint name) {
    struct mgl_pipeline *it;
    for (it = ctx->pipelines; it != NULL; it = it->next)
        if (it->name == name)
            return it;
    return NULL;
}

static int pipeline_reject(struct mgl_context *ctx) {
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 1;
    }
    return 0;
}

static void release_program(struct mgl_context *ctx, struct mgl_program *program,
                            int native_available) {
    if (program == NULL)
        return;
    if (program->use_count)
        --program->use_count;
    if (native_available)
        mgl_program_maybe_destroy(ctx, program);
}

static void replace_program(struct mgl_context *ctx, struct mgl_program **slot,
                            struct mgl_program *next) {
    struct mgl_program *old = *slot;
    if (old == next)
        return;
    if (next != NULL)
        ++next->use_count;
    *slot = next;
    release_program(ctx, old, 1);
}

static void destroy_pipeline(struct mgl_context *ctx, struct mgl_pipeline *pipeline,
                             int native_available) {
    unsigned i;
    for (i = 0u; i < 6u; ++i)
        release_program(ctx, pipeline->stages[i], native_available);
    release_program(ctx, pipeline->active, native_available);
    if (native_available && ctx->native_dispatch->glesDeleteProgramPipelines)
        ctx->native_dispatch->glesDeleteProgramPipelines(1, &pipeline->name);
    free(pipeline);
}

void mgl_pipeline_cleanup(struct mgl_context *ctx, int native_available) {
    struct mgl_pipeline *it;
    if (ctx == NULL)
        return;
    it = ctx->pipelines;
    ctx->pipelines = NULL;
    ctx->current_pipeline = 0u;
    while (it != NULL) {
        struct mgl_pipeline *next = it->next;
        destroy_pipeline(ctx, it, native_available);
        it = next;
    }
}

void glGenProgramPipelines(GLsizei n, GLuint *pipelines) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint *names;
    GLsizei i;
    if (ctx == NULL || pipeline_reject(ctx))
        return;
    if (n < 0 || (n > 0 && pipelines == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0)
        return;
    names = calloc((size_t)n, sizeof(*names));
    if (names == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    ctx->native_dispatch->glesGenProgramPipelines(n, names);
    for (i = 0; i < n; ++i) {
        struct mgl_pipeline *obj = calloc(1, sizeof(*obj));
        if (obj == NULL || names[i] == 0u) {
            GLsizei j;
            free(obj);
            for (j = i; j < n; ++j)
                if (names[j])
                    ctx->native_dispatch->glesDeleteProgramPipelines(1, &names[j]);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            break;
        }
        obj->name = names[i];
        obj->next = ctx->pipelines;
        ctx->pipelines = obj;
        pipelines[i] = names[i];
    }
    free(names);
}

void glCreateProgramPipelines(GLsizei n, GLuint *pipelines) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint prior;
    GLsizei i;
    if (ctx == NULL || pipeline_reject(ctx))
        return;
    if (n < 0 || (n > 0 && pipelines == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0)
        return;
    prior = ctx->current_pipeline;
    glGenProgramPipelines(n, pipelines);
    for (i = 0; i < n; ++i) {
        struct mgl_pipeline *obj = find_pipeline(ctx, pipelines[i]);
        if (obj == NULL)
            break;
        ctx->native_dispatch->glesBindProgramPipeline(obj->name);
        obj->realized = GL_TRUE;
    }
    ctx->native_dispatch->glesBindProgramPipeline(prior);
}

void glBindProgramPipeline(GLuint pipeline) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_pipeline *obj;
    if (ctx == NULL || pipeline_reject(ctx))
        return;
    obj = pipeline ? find_pipeline(ctx, pipeline) : NULL;
    if (pipeline != 0u && obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesBindProgramPipeline(pipeline);
    ctx->current_pipeline = pipeline;
    if (obj != NULL)
        obj->realized = GL_TRUE;
}

GLboolean glIsProgramPipeline(GLuint pipeline) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_pipeline *obj;
    if (ctx == NULL || pipeline_reject(ctx))
        return GL_FALSE;
    obj = find_pipeline(ctx, pipeline);
    return obj != NULL && obj->realized ? GL_TRUE : GL_FALSE;
}

void glDeleteProgramPipelines(GLsizei n, const GLuint *pipelines) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL || pipeline_reject(ctx))
        return;
    if (n < 0 || (n > 0 && pipelines == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < n; ++i) {
        struct mgl_pipeline **link = &ctx->pipelines;
        while (*link != NULL && (*link)->name != pipelines[i])
            link = &(*link)->next;
        if (*link != NULL) {
            struct mgl_pipeline *obj = *link;
            *link = obj->next;
            if (ctx->current_pipeline == obj->name) {
                ctx->native_dispatch->glesBindProgramPipeline(0u);
                ctx->current_pipeline = 0u;
            }
            destroy_pipeline(ctx, obj, 1);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_pipeline *obj;
    struct mgl_program *source = NULL;
    unsigned i;
    if (ctx == NULL || pipeline_reject(ctx))
        return;
    obj = find_pipeline(ctx, pipeline);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (stages != GL_ALL_SHADER_BITS && (stages & ~UINT32_C(0x3f))) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    if (program) {
        source = mgl_program_find(ctx, program);
        if (source == NULL || !source->linked || !source->linked_separable) {
            GLenum error = source == NULL
                               ? (mgl_shader_find(ctx, program) != NULL ? GL_INVALID_OPERATION
                                                                        : GL_INVALID_VALUE)
                               : GL_INVALID_OPERATION;
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, error);
            return;
        }
    }
    ctx->native_dispatch->glesUseProgramStages(pipeline, stages, source ? source->native_name : 0u);
    obj->realized = GL_TRUE;
    obj->validated = GL_FALSE;
    for (i = 0u; i < 6u; ++i) {
        if (stages == GL_ALL_SHADER_BITS || (stages & pipeline_stage_bits[i])) {
            GLint current_native = 0;
            struct mgl_program *installed = NULL;
            ctx->native_dispatch->glesGetProgramPipelineiv(pipeline, pipeline_stage_enums[i],
                                                           &current_native);
            if (source != NULL && current_native == (GLint)source->native_name)
                installed = source;
            replace_program(ctx, &obj->stages[i], installed);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glActiveShaderProgram(GLuint pipeline, GLuint program) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_pipeline *obj;
    struct mgl_program *source;
    if (ctx == NULL || pipeline_reject(ctx))
        return;
    obj = find_pipeline(ctx, pipeline);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    source = mgl_program_find(ctx, program);
    if (program != 0u && (source == NULL || !source->linked)) {
        GLenum error =
            source == NULL
                ? (mgl_shader_find(ctx, program) != NULL ? GL_INVALID_OPERATION : GL_INVALID_VALUE)
                : GL_INVALID_OPERATION;
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    ctx->native_dispatch->glesActiveShaderProgram(pipeline, source ? source->native_name : 0u);
    obj->realized = GL_TRUE;
    replace_program(ctx, &obj->active, source);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glValidateProgramPipeline(GLuint pipeline) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_pipeline *obj;
    GLint status = 0;
    if (ctx == NULL || pipeline_reject(ctx))
        return;
    obj = find_pipeline(ctx, pipeline);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesValidateProgramPipeline(pipeline);
    ctx->native_dispatch->glesGetProgramPipelineiv(pipeline, GL_VALIDATE_STATUS, &status);
    obj->validated = status ? GL_TRUE : GL_FALSE;
    obj->realized = GL_TRUE;
}

void glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_pipeline *obj;
    unsigned i;
    if (ctx == NULL || pipeline_reject(ctx))
        return;
    obj = find_pipeline(ctx, pipeline);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (pname == GL_ACTIVE_PROGRAM) {
        *params = obj->active ? (GLint)obj->active->name : 0;
        return;
    }
    for (i = 0u; i < 6u; ++i) {
        if (pname == pipeline_stage_enums[i]) {
            *params = obj->stages[i] ? (GLint)obj->stages[i]->name : 0;
            return;
        }
    }
    if (pname == GL_VALIDATE_STATUS) {
        ctx->native_dispatch->glesGetProgramPipelineiv(pipeline, pname, params);
        obj->validated = *params ? GL_TRUE : GL_FALSE;
        return;
    }
    if (pname == GL_INFO_LOG_LENGTH) {
        ctx->native_dispatch->glesGetProgramPipelineiv(pipeline, pname, params);
        return;
    }
    mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
}

void glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei buf_size, GLsizei *length,
                                 GLchar *info_log) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || pipeline_reject(ctx))
        return;
    if (!find_pipeline(ctx, pipeline)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (buf_size < 0 || (buf_size > 0 && info_log == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    ctx->native_dispatch->glesGetProgramPipelineInfoLog(pipeline, buf_size, length, info_log);
}

GLuint glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar *const *strings) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint shader, program;
    if (ctx == NULL || pipeline_reject(ctx))
        return 0u;
    if (count < 0 || (count > 0 && strings == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0u;
    }
    shader = glCreateShader(type);
    if (shader == 0u)
        return 0u;
    glShaderSource(shader, count, strings, NULL);
    glCompileShader(shader);
    program = glCreateProgram();
    if (program != 0u) {
        glProgramParameteri(program, GL_PROGRAM_SEPARABLE, GL_TRUE);
        glAttachShader(program, shader);
        glLinkProgram(program);
        glDetachShader(program, shader);
    }
    glDeleteShader(shader);
    return program;
}

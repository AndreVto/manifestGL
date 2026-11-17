#include "mgl/gl.h"

#include "core/context.h"
#include "shader/shader_store.h"

#include <string.h>

enum mgl_arb_object_kind { MGL_ARB_OBJECT_NONE = 0, MGL_ARB_OBJECT_SHADER, MGL_ARB_OBJECT_PROGRAM };

static enum mgl_arb_object_kind find_object_locked(struct mgl_context *ctx, GLhandleARB handle,
                                                   struct mgl_shader **shader_out,
                                                   struct mgl_program **program_out) {
    struct mgl_shader *shader = mgl_shader_find(ctx, (GLuint)handle);
    struct mgl_program *program = mgl_program_find(ctx, (GLuint)handle);
    if (shader_out != NULL)
        *shader_out = shader;
    if (program_out != NULL)
        *program_out = program;
    if (shader != NULL)
        return MGL_ARB_OBJECT_SHADER;
    if (program != NULL)
        return MGL_ARB_OBJECT_PROGRAM;
    return MGL_ARB_OBJECT_NONE;
}

static void copy_string(const char *source, GLsizei max_length, GLsizei *length, GLcharARB *dest) {
    size_t source_length = source != NULL ? strlen(source) : 0u;
    size_t copy_length = 0u;
    if (max_length > 0 && dest != NULL) {
        copy_length =
            source_length < (size_t)(max_length - 1) ? source_length : (size_t)(max_length - 1);
        if (copy_length != 0u)
            memcpy(dest, source, copy_length);
        dest[copy_length] = '\0';
    }
    if (length != NULL)
        *length = (GLsizei)copy_length;
}

GLhandleARB glCreateShaderObjectARB(GLenum shaderType) {
    return (GLhandleARB)glCreateShader(shaderType);
}

GLhandleARB glCreateProgramObjectARB(void) {
    return (GLhandleARB)glCreateProgram();
}

void glDeleteObjectARB(GLhandleARB obj) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_shader *shader = NULL;
    struct mgl_program *program = NULL;
    enum mgl_arb_object_kind kind;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (obj == 0u)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    kind = find_object_locked(ctx, obj, &shader, &program);
    if (kind == MGL_ARB_OBJECT_SHADER) {
        shader->delete_pending = GL_TRUE;
        mgl_shader_maybe_destroy(ctx, shader);
    } else if (kind == MGL_ARB_OBJECT_PROGRAM) {
        program->delete_pending = GL_TRUE;
        mgl_program_maybe_destroy(ctx, program);
    } else {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glAttachObjectARB(GLhandleARB containerObj, GLhandleARB obj) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_shader *shader = NULL;
    struct mgl_program *program = NULL;
    enum mgl_arb_object_kind container_kind;
    enum mgl_arb_object_kind object_kind;
    unsigned i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    container_kind = find_object_locked(ctx, containerObj, NULL, &program);
    object_kind = find_object_locked(ctx, obj, &shader, NULL);
    if (container_kind == MGL_ARB_OBJECT_NONE || object_kind == MGL_ARB_OBJECT_NONE) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    if (container_kind != MGL_ARB_OBJECT_PROGRAM || object_kind != MGL_ARB_OBJECT_SHADER) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    for (i = 0u; i < program->attached_count; ++i) {
        if (program->attached[i] == shader) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            return;
        }
    }
    if (program->attached_count >= MGL_MAX_ATTACHED_SHADERS) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    program->attached[program->attached_count++] = shader;
    ++shader->attach_count;
    ctx->native_dispatch->glesAttachShader(program->native_name, shader->native_name);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glDetachObjectARB(GLhandleARB containerObj, GLhandleARB attachedObj) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_shader *shader = NULL;
    struct mgl_program *program = NULL;
    enum mgl_arb_object_kind container_kind;
    enum mgl_arb_object_kind object_kind;
    unsigned i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    container_kind = find_object_locked(ctx, containerObj, NULL, &program);
    object_kind = find_object_locked(ctx, attachedObj, &shader, NULL);
    if (container_kind == MGL_ARB_OBJECT_NONE || object_kind == MGL_ARB_OBJECT_NONE) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    if (container_kind != MGL_ARB_OBJECT_PROGRAM || object_kind != MGL_ARB_OBJECT_SHADER) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    for (i = 0u; i < program->attached_count; ++i) {
        if (program->attached[i] == shader) {
            unsigned j;
            ctx->native_dispatch->glesDetachShader(program->native_name, shader->native_name);
            for (j = i + 1u; j < program->attached_count; ++j)
                program->attached[j - 1u] = program->attached[j];
            --program->attached_count;
            program->attached[program->attached_count] = NULL;
            if (shader->attach_count > 0u)
                --shader->attach_count;
            mgl_shader_maybe_destroy(ctx, shader);
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            return;
        }
    }
    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glUseProgramObjectARB(GLhandleARB programObj) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *next = NULL;
    struct mgl_program *old;
    enum mgl_arb_object_kind kind = MGL_ARB_OBJECT_NONE;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    if (programObj != 0u) {
        kind = find_object_locked(ctx, programObj, NULL, &next);
        if (kind == MGL_ARB_OBJECT_NONE) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            return;
        }
        if (kind != MGL_ARB_OBJECT_PROGRAM || !next->linked) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            return;
        }
    }
    old = mgl_program_find(ctx, ctx->current_program);
    if (old == next) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    mgl_draw_use_native_program(ctx, next != NULL ? next->native_name : 0u);
    if (old != NULL && old->use_count > 0u)
        --old->use_count;
    if (next != NULL)
        ++next->use_count;
    ctx->current_program = (GLuint)programObj;
    ctx->current_program_executable = next != NULL ? GL_TRUE : GL_FALSE;
    if (old != NULL)
        mgl_program_maybe_destroy(ctx, old);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

GLhandleARB glGetHandleARB(GLenum pname) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return 0u;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0u;
    }
    if (pname != GL_PROGRAM_OBJECT_ARB)
        return 0u;
    return (GLhandleARB)ctx->current_program;
}

static int get_object_parameter_locked(struct mgl_context *ctx, GLhandleARB obj, GLenum pname,
                                       GLint *value) {
    struct mgl_shader *shader = NULL;
    struct mgl_program *program = NULL;
    enum mgl_arb_object_kind kind = find_object_locked(ctx, obj, &shader, &program);
    if (kind == MGL_ARB_OBJECT_NONE) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    switch (pname) {
        case GL_OBJECT_TYPE_ARB:
            *value = kind == MGL_ARB_OBJECT_SHADER ? (GLint)GL_SHADER_OBJECT_ARB
                                                   : (GLint)GL_PROGRAM_OBJECT_ARB;
            return 1;
        case GL_OBJECT_SUBTYPE_ARB:
            if (kind != MGL_ARB_OBJECT_SHADER)
                break;
            *value = (GLint)shader->type;
            return 1;
        case GL_OBJECT_DELETE_STATUS_ARB:
            *value = kind == MGL_ARB_OBJECT_SHADER ? (shader->delete_pending ? GL_TRUE : GL_FALSE)
                                                   : (program->delete_pending ? GL_TRUE : GL_FALSE);
            return 1;
        case GL_OBJECT_COMPILE_STATUS_ARB:
            if (kind != MGL_ARB_OBJECT_SHADER)
                break;
            *value = shader->compiled ? GL_TRUE : GL_FALSE;
            return 1;
        case GL_OBJECT_LINK_STATUS_ARB:
            if (kind != MGL_ARB_OBJECT_PROGRAM)
                break;
            *value = program->linked ? GL_TRUE : GL_FALSE;
            return 1;
        case GL_OBJECT_VALIDATE_STATUS_ARB:
            if (kind != MGL_ARB_OBJECT_PROGRAM)
                break;
            *value = program->validated ? GL_TRUE : GL_FALSE;
            return 1;
        case GL_OBJECT_INFO_LOG_LENGTH_ARB: {
            const char *log = kind == MGL_ARB_OBJECT_SHADER ? shader->info_log : program->info_log;
            *value = log != NULL ? (GLint)(strlen(log) + 1u) : 0;
            return 1;
        }
        case GL_OBJECT_ATTACHED_OBJECTS_ARB:
            if (kind != MGL_ARB_OBJECT_PROGRAM)
                break;
            *value = (GLint)program->attached_count;
            return 1;
        case GL_OBJECT_ACTIVE_UNIFORMS_ARB:
        case GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB:
            if (kind != MGL_ARB_OBJECT_PROGRAM)
                break;
            ctx->native_dispatch->glesGetProgramiv(program->native_name, pname, value);
            return 1;
        case GL_OBJECT_SHADER_SOURCE_LENGTH_ARB:
            if (kind != MGL_ARB_OBJECT_SHADER)
                break;
            *value = shader->source != NULL ? (GLint)(strlen(shader->source) + 1u) : 0;
            return 1;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return 0;
    }
    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    return 0;
}

void glGetObjectParameterivARB(GLhandleARB obj, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLint value;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    if (get_object_parameter_locked(ctx, obj, pname, &value))
        *params = value;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetObjectParameterfvARB(GLhandleARB obj, GLenum pname, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLint value;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    if (get_object_parameter_locked(ctx, obj, pname, &value))
        *params = (GLfloat)value;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetInfoLogARB(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_shader *shader = NULL;
    struct mgl_program *program = NULL;
    enum mgl_arb_object_kind kind;
    const char *log;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (maxLength < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    kind = find_object_locked(ctx, obj, &shader, &program);
    if (kind == MGL_ARB_OBJECT_NONE) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    log = kind == MGL_ARB_OBJECT_SHADER ? shader->info_log : program->info_log;
    copy_string(log, maxLength, length, infoLog);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetAttachedObjectsARB(GLhandleARB containerObj, GLsizei maxCount, GLsizei *count,
                             GLhandleARB *obj) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program = NULL;
    enum mgl_arb_object_kind kind;
    unsigned n;
    unsigned i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (maxCount < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    kind = find_object_locked(ctx, containerObj, NULL, &program);
    if (kind == MGL_ARB_OBJECT_NONE) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    if (kind != MGL_ARB_OBJECT_PROGRAM) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    n = program->attached_count < (unsigned)maxCount ? program->attached_count : (unsigned)maxCount;
    if (obj != NULL) {
        for (i = 0u; i < n; ++i)
            obj[i] = (GLhandleARB)program->attached[i]->name;
    }
    if (count != NULL)
        *count = (GLsizei)n;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glShaderSourceARB(GLhandleARB shader, GLsizei count, const GLcharARB *const *strings,
                       const GLint *lengths) {
    glShaderSource((GLuint)shader, count, (const GLchar *const *)strings, lengths);
}

void glCompileShaderARB(GLhandleARB shader) {
    glCompileShader((GLuint)shader);
}

void glLinkProgramARB(GLhandleARB program) {
    glLinkProgram((GLuint)program);
}

void glValidateProgramARB(GLhandleARB program) {
    glValidateProgram((GLuint)program);
}

void glBindAttribLocationARB(GLhandleARB program, GLuint index, const GLcharARB *name) {
    glBindAttribLocation((GLuint)program, index, (const GLchar *)name);
}

void glGetActiveUniformARB(GLhandleARB program, GLuint index, GLsizei bufSize, GLsizei *length,
                           GLint *size, GLenum *type, GLcharARB *name) {
    glGetActiveUniform((GLuint)program, index, bufSize, length, size, type, (GLchar *)name);
}

GLint glGetAttribLocationARB(GLhandleARB program, const GLcharARB *name) {
    return glGetAttribLocation((GLuint)program, (const GLchar *)name);
}

GLint glGetUniformLocationARB(GLhandleARB program, const GLcharARB *name) {
    return glGetUniformLocation((GLuint)program, (const GLchar *)name);
}

void glGetUniformfvARB(GLhandleARB program, GLint location, GLfloat *params) {
    glGetUniformfv((GLuint)program, location, params);
}

void glGetUniformivARB(GLhandleARB program, GLint location, GLint *params) {
    glGetUniformiv((GLuint)program, location, params);
}

void glGetShaderSourceARB(GLhandleARB shader, GLsizei bufSize, GLsizei *length, GLcharARB *source) {
    glGetShaderSource((GLuint)shader, bufSize, length, (GLchar *)source);
}

void glGetUniformuivEXT(GLuint program, GLint location, GLuint *params) {
    glGetUniformuiv(program, location, params);
}

void glUniform1fARB(GLint location, GLfloat v0) {
    glUniform1f(location, v0);
}

void glUniform2fARB(GLint location, GLfloat v0, GLfloat v1) {
    glUniform2f(location, v0, v1);
}

void glUniform3fARB(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    glUniform3f(location, v0, v1, v2);
}

void glUniform4fARB(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    glUniform4f(location, v0, v1, v2, v3);
}

void glUniform1iARB(GLint location, GLint v0) {
    glUniform1i(location, v0);
}

void glUniform2iARB(GLint location, GLint v0, GLint v1) {
    glUniform2i(location, v0, v1);
}

void glUniform3iARB(GLint location, GLint v0, GLint v1, GLint v2) {
    glUniform3i(location, v0, v1, v2);
}

void glUniform4iARB(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    glUniform4i(location, v0, v1, v2, v3);
}

void glUniform1uiEXT(GLint location, GLuint v0) {
    glUniform1ui(location, v0);
}

void glUniform2uiEXT(GLint location, GLuint v0, GLuint v1) {
    glUniform2ui(location, v0, v1);
}

void glUniform3uiEXT(GLint location, GLuint v0, GLuint v1, GLuint v2) {
    glUniform3ui(location, v0, v1, v2);
}

void glUniform4uiEXT(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    glUniform4ui(location, v0, v1, v2, v3);
}

void glUniform1fvARB(GLint location, GLsizei count, const GLfloat *values) {
    glUniform1fv(location, count, values);
}

void glUniform2fvARB(GLint location, GLsizei count, const GLfloat *values) {
    glUniform2fv(location, count, values);
}

void glUniform3fvARB(GLint location, GLsizei count, const GLfloat *values) {
    glUniform3fv(location, count, values);
}

void glUniform4fvARB(GLint location, GLsizei count, const GLfloat *values) {
    glUniform4fv(location, count, values);
}

void glUniform1ivARB(GLint location, GLsizei count, const GLint *values) {
    glUniform1iv(location, count, values);
}

void glUniform2ivARB(GLint location, GLsizei count, const GLint *values) {
    glUniform2iv(location, count, values);
}

void glUniform3ivARB(GLint location, GLsizei count, const GLint *values) {
    glUniform3iv(location, count, values);
}

void glUniform4ivARB(GLint location, GLsizei count, const GLint *values) {
    glUniform4iv(location, count, values);
}

void glUniform1uivEXT(GLint location, GLsizei count, const GLuint *values) {
    glUniform1uiv(location, count, values);
}

void glUniform2uivEXT(GLint location, GLsizei count, const GLuint *values) {
    glUniform2uiv(location, count, values);
}

void glUniform3uivEXT(GLint location, GLsizei count, const GLuint *values) {
    glUniform3uiv(location, count, values);
}

void glUniform4uivEXT(GLint location, GLsizei count, const GLuint *values) {
    glUniform4uiv(location, count, values);
}

void glUniformMatrix2fvARB(GLint location, GLsizei count, GLboolean transpose,
                           const GLfloat *values) {
    glUniformMatrix2fv(location, count, transpose, values);
}

void glUniformMatrix3fvARB(GLint location, GLsizei count, GLboolean transpose,
                           const GLfloat *values) {
    glUniformMatrix3fv(location, count, transpose, values);
}

void glUniformMatrix4fvARB(GLint location, GLsizei count, GLboolean transpose,
                           const GLfloat *values) {
    glUniformMatrix4fv(location, count, transpose, values);
}

void glVertexAttribDivisorARB(GLuint index, GLuint divisor) {
    glVertexAttribDivisor(index, divisor);
}

void glDrawArraysInstancedARB(GLenum mode, GLint first, GLsizei count, GLsizei primcount) {
    glDrawArraysInstanced(mode, first, count, primcount);
}

void glDrawElementsInstancedARB(GLenum mode, GLsizei count, GLenum type, const void *indices,
                                GLsizei primcount) {
    glDrawElementsInstanced(mode, count, type, indices, primcount);
}

void glDrawBuffersARB(GLsizei count, const GLenum *buffers) {
    glDrawBuffers(count, buffers);
}

void glTexBufferARB(GLenum target, GLenum internalformat, GLuint buffer) {
    glTexBuffer(target, internalformat, buffer);
}

void glProgramParameteriARB(GLuint program, GLenum pname, GLint value) {
    glProgramParameteri(program, pname, value);
}

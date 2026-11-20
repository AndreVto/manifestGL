#include "mgl/gl.h"

#include "core/context.h"
#include "core/render_diag.h"
#include "shader/shader_store.h"

#include <stdlib.h>

static struct mgl_program *lookup_program(struct mgl_context *ctx, GLuint name) {
    struct mgl_program *program;

    if (ctx == NULL)
        return NULL;
    program = mgl_program_find(ctx, name);
    if (program == NULL) {
        mgl_error_set(&ctx->errors, mgl_shader_find(ctx, name) != NULL
                                       ? GL_INVALID_OPERATION : GL_INVALID_VALUE);
    }
    return program;
}

/* Reflection and block-binding commands are illegal during Begin/End.
 * Check before argument validation and before touching GLES/output buffers. */
static int reflection_reject_inside_begin(struct mgl_context *ctx) {
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 1;
    }
    return 0;
}

void glGetActiveAttrib(GLuint program_name, GLuint index, GLsizei bufSize, GLsizei *length,
                       GLint *size, GLenum *type, GLchar *name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;

    if (ctx == NULL)
        return;
    if (reflection_reject_inside_begin(ctx))
        return;
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        ctx->native_dispatch->glesGetActiveAttrib(program->native_name, index, bufSize, length,
                                                  size, type, name);
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetActiveUniform(GLuint program_name, GLuint index, GLsizei bufSize, GLsizei *length,
                        GLint *size, GLenum *type, GLchar *name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;

    if (ctx == NULL)
        return;
    if (reflection_reject_inside_begin(ctx))
        return;
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        ctx->native_dispatch->glesGetActiveUniform(program->native_name, index, bufSize, length,
                                                   size, type, name);
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetUniformIndices(GLuint program_name, GLsizei uniformCount,
                         const GLchar *const *uniformNames, GLuint *uniformIndices) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;

    if (ctx == NULL)
        return;
    if (reflection_reject_inside_begin(ctx))
        return;
    if (uniformCount < 0 ||
        (uniformCount > 0 && (uniformNames == NULL || uniformIndices == NULL))) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        if (ctx->native_dispatch->glesGetUniformIndices == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        } else {
            ctx->native_dispatch->glesGetUniformIndices(program->native_name, uniformCount,
                                                        uniformNames, uniformIndices);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetActiveUniformsiv(GLuint program_name, GLsizei uniformCount, const GLuint *uniformIndices,
                           GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;

    if (ctx == NULL)
        return;
    if (reflection_reject_inside_begin(ctx))
        return;
    if (uniformCount < 0 || (uniformCount > 0 && (uniformIndices == NULL || params == NULL))) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        if (ctx->native_dispatch->glesGetActiveUniformsiv == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        } else {
            ctx->native_dispatch->glesGetActiveUniformsiv(program->native_name, uniformCount,
                                                          uniformIndices, pname, params);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetActiveUniformName(GLuint program_name, GLuint uniformIndex, GLsizei bufSize,
                            GLsizei *length, GLchar *uniformName) {
    GLint ignored_size = 0;
    GLenum ignored_type = 0u;
    glGetActiveUniform(program_name, uniformIndex, bufSize, length, &ignored_size, &ignored_type,
                       uniformName);
}

GLuint glGetUniformBlockIndex(GLuint program_name, const GLchar *uniformBlockName) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    GLuint result = GL_INVALID_INDEX;

    if (ctx == NULL)
        return GL_INVALID_INDEX;
    if (reflection_reject_inside_begin(ctx))
        return GL_INVALID_INDEX;
    if (uniformBlockName == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return GL_INVALID_INDEX;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        if (ctx->native_dispatch->glesGetUniformBlockIndex == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        } else {
            result = ctx->native_dispatch->glesGetUniformBlockIndex(program->native_name,
                                                                    uniformBlockName);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return result;
}

void glGetActiveUniformBlockiv(GLuint program_name, GLuint uniformBlockIndex, GLenum pname,
                               GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;

    if (ctx == NULL)
        return;
    if (reflection_reject_inside_begin(ctx))
        return;
    if (params == NULL)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        if (ctx->native_dispatch->glesGetActiveUniformBlockiv == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        } else {
            ctx->native_dispatch->glesGetActiveUniformBlockiv(program->native_name,
                                                              uniformBlockIndex, pname, params);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetActiveUniformBlockName(GLuint program_name, GLuint uniformBlockIndex, GLsizei bufSize,
                                 GLsizei *length, GLchar *uniformBlockName) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;

    if (ctx == NULL)
        return;
    if (reflection_reject_inside_begin(ctx))
        return;
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        if (ctx->native_dispatch->glesGetActiveUniformBlockName == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        } else {
            ctx->native_dispatch->glesGetActiveUniformBlockName(
                program->native_name, uniformBlockIndex, bufSize, length, uniformBlockName);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glUniformBlockBinding(GLuint program_name, GLuint uniformBlockIndex,
                           GLuint uniformBlockBinding) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;

    if (ctx == NULL)
        return;
    if (reflection_reject_inside_begin(ctx))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        if (ctx->native_dispatch->glesUniformBlockBinding == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        } else {
            ctx->native_dispatch->glesUniformBlockBinding(program->native_name, uniformBlockIndex,
                                                          uniformBlockBinding);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glShaderStorageBlockBinding(GLuint program_name, GLuint storageBlockIndex,
                                 GLuint storageBlockBinding) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;

    if (ctx == NULL)
        return;
    if (reflection_reject_inside_begin(ctx))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        mgl_render_diag_detail(ctx, "glShaderStorageBlockBinding", "native_program_index",
                               program->native_name, storageBlockIndex);
        if (ctx->native_dispatch == NULL ||
            ctx->native_dispatch->glesShaderStorageBlockBinding == NULL) {
            mgl_render_diag_detail(ctx, "glShaderStorageBlockBinding", "native_entrypoint_missing",
                                   program_name, storageBlockBinding);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        } else {
            ctx->native_dispatch->glesShaderStorageBlockBinding(
                program->native_name, storageBlockIndex, storageBlockBinding);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetProgramInterfaceiv(GLuint program_name, GLenum programInterface, GLenum pname,
                             GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;

    if (ctx == NULL)
        return;
    if (reflection_reject_inside_begin(ctx))
        return;
    if (params == NULL)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        if (ctx->native_dispatch->glesGetProgramInterfaceiv == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        } else {
            ctx->native_dispatch->glesGetProgramInterfaceiv(program->native_name, programInterface,
                                                            pname, params);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

GLuint glGetProgramResourceIndex(GLuint program_name, GLenum programInterface, const GLchar *name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    GLuint result = GL_INVALID_INDEX;

    if (ctx == NULL)
        return GL_INVALID_INDEX;
    if (reflection_reject_inside_begin(ctx))
        return GL_INVALID_INDEX;
    if (name == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return GL_INVALID_INDEX;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        if (ctx->native_dispatch->glesGetProgramResourceIndex == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        } else {
            result = ctx->native_dispatch->glesGetProgramResourceIndex(program->native_name,
                                                                       programInterface, name);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return result;
}

void glGetProgramResourceName(GLuint program_name, GLenum programInterface, GLuint index,
                              GLsizei bufSize, GLsizei *length, GLchar *name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;

    if (ctx == NULL)
        return;
    if (reflection_reject_inside_begin(ctx))
        return;
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        if (ctx->native_dispatch->glesGetProgramResourceName == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        } else {
            ctx->native_dispatch->glesGetProgramResourceName(program->native_name, programInterface,
                                                             index, bufSize, length, name);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

static GLenum atomic_counter_buffer_property(GLenum pname) {
    switch (pname) {
        case GL_ATOMIC_COUNTER_BUFFER_BINDING:
            return GL_BUFFER_BINDING;
        case GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE:
            return GL_BUFFER_DATA_SIZE;
        case GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTERS:
            return GL_NUM_ACTIVE_VARIABLES;
        case GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTER_INDICES:
            return GL_ACTIVE_VARIABLES;
        case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER:
            return GL_REFERENCED_BY_VERTEX_SHADER;
        case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_CONTROL_SHADER:
            return GL_REFERENCED_BY_TESS_CONTROL_SHADER;
        case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_EVALUATION_SHADER:
            return GL_REFERENCED_BY_TESS_EVALUATION_SHADER;
        case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_GEOMETRY_SHADER:
            return GL_REFERENCED_BY_GEOMETRY_SHADER;
        case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_FRAGMENT_SHADER:
            return GL_REFERENCED_BY_FRAGMENT_SHADER;
        case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_COMPUTE_SHADER:
            return GL_REFERENCED_BY_COMPUTE_SHADER;
        default:
            return 0u;
    }
}

void glGetActiveAtomicCounterBufferiv(GLuint program_name, GLuint bufferIndex, GLenum pname,
                                      GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    GLenum property;
    GLint count = 0;
    GLsizei result_count = 1;

    if (ctx == NULL)
        return;
    if (reflection_reject_inside_begin(ctx))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    property = atomic_counter_buffer_property(pname);
    if (!program->linked) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
    } else if (property == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
    } else if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetProgramiv == NULL ||
               ctx->native_dispatch->glesGetProgramResourceiv == NULL) {
        mgl_render_diag_detail(ctx, "glGetActiveAtomicCounterBufferiv", "native_entrypoint_missing",
                               program_name, pname);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    } else {
        ctx->native_dispatch->glesGetProgramiv(program->native_name,
                                               GL_ACTIVE_ATOMIC_COUNTER_BUFFERS, &count);
        mgl_render_diag_detail(ctx, "glGetActiveAtomicCounterBufferiv",
                               "native_program_active_count", program->native_name,
                               (uint64_t)(unsigned)count);
        if (count < 0 || bufferIndex >= (GLuint)count) {
            mgl_render_diag_detail(ctx, "glGetActiveAtomicCounterBufferiv", "index_out_of_range",
                                   bufferIndex, (uint64_t)(unsigned)count);
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        } else if (params != NULL) {
            if (property == GL_ACTIVE_VARIABLES) {
                ctx->native_dispatch->glesGetProgramResourceiv(
                    program->native_name, GL_ATOMIC_COUNTER_BUFFER, bufferIndex, 1,
                    (const GLenum[]){GL_NUM_ACTIVE_VARIABLES}, 1, NULL, &result_count);
                if (result_count <= 0) {
                    (void)pthread_mutex_unlock(&ctx->share_group->lock);
                    return;
                }
            }
            ctx->native_dispatch->glesGetProgramResourceiv(program->native_name,
                                                           GL_ATOMIC_COUNTER_BUFFER, bufferIndex, 1,
                                                           &property, result_count, NULL, params);
            mgl_render_diag_detail(ctx, "glGetActiveAtomicCounterBufferiv", "returned_value", pname,
                                   (uint64_t)(int64_t)*params);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

static int resource_output_index_locked(struct mgl_context *ctx, struct mgl_program *program,
                                        GLuint index, GLint *result) {
    GLint name_size = 0;
    GLchar *name;
    const GLenum property = GL_NAME_LENGTH;
    ctx->native_dispatch->glesGetProgramResourceiv(program->native_name, GL_PROGRAM_OUTPUT, index,
                                                   1, &property, 1, NULL, &name_size);
    if (name_size <= 0)
        return 0;
    name = malloc((size_t)name_size);
    if (name == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    ctx->native_dispatch->glesGetProgramResourceName(program->native_name, GL_PROGRAM_OUTPUT,
                                                     index, name_size, NULL, name);
    *result = mgl_frag_data_index_locked(ctx, program, name);
    free(name);
    return 1;
}

void glGetProgramResourceiv(GLuint program_name, GLenum programInterface, GLuint index,
                            GLsizei propCount, const GLenum *props, GLsizei bufSize,
                            GLsizei *length, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    GLsizei i;
    GLsizei written = 0;
    int needs_index = 0;
    int complete = 1;

    if (ctx == NULL)
        return;
    if (reflection_reject_inside_begin(ctx))
        return;
    if (propCount <= 0 || bufSize < 0 || props == NULL || (bufSize > 0 && params == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < propCount; ++i) {
        if (props[i] == GL_LOCATION_INDEX) {
            if (programInterface != GL_PROGRAM_OUTPUT) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            needs_index = 1;
        }
    }
    if (needs_index && mgl_native_dispatch_has_gles_extension(
                           ctx->native_dispatch, "GL_EXT_blend_func_extended"))
        needs_index = 0;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        mgl_render_diag_detail(ctx, "glGetProgramResourceiv", "native_program_interface",
                               program->native_name, programInterface);
        if (ctx->native_dispatch == NULL ||
            ctx->native_dispatch->glesGetProgramResourceiv == NULL ||
            (needs_index && (ctx->native_dispatch->glesGetProgramInterfaceiv == NULL ||
                             ctx->native_dispatch->glesGetProgramResourceName == NULL))) {
            mgl_render_diag_detail(ctx, "glGetProgramResourceiv", "native_entrypoint_missing",
                                   program_name, index);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        } else if (!needs_index) {
            ctx->native_dispatch->glesGetProgramResourceiv(program->native_name, programInterface,
                                                           index, propCount, props, bufSize, length,
                                                           params);
            if (bufSize > 0)
                mgl_render_diag_detail(ctx, "glGetProgramResourceiv", "property_result", props[0],
                                       (uint64_t)(int64_t)params[0]);
        } else {
            GLint count = 0;
            ctx->native_dispatch->glesGetProgramInterfaceiv(program->native_name,
                                                            GL_PROGRAM_OUTPUT, GL_ACTIVE_RESOURCES,
                                                            &count);
            if (count < 0 || index >= (GLuint)count) {
                mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            } else {
                for (i = 0; i < propCount; ++i) {
                    if (props[i] == GL_LOCATION_INDEX) {
                        if (written < bufSize) {
                            if (!resource_output_index_locked(ctx, program, index,
                                                              &params[written])) {
                                complete = 0;
                                break;
                            }
                            ++written;
                        }
                    } else {
                        GLsizei count_written = 0;
                        GLint discard = 0;
                        const int has_space = written < bufSize;
                        ctx->native_dispatch->glesGetProgramResourceiv(
                            program->native_name, GL_PROGRAM_OUTPUT, index, 1, &props[i],
                            has_space ? bufSize - written : 1, &count_written,
                            has_space ? params + written : &discard);
                        if (count_written <= 0) {
                            complete = 0;
                            break;
                        }
                        if (has_space)
                            written += count_written;
                    }
                }
                if (length != NULL && complete)
                    *length = written;
            }
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

GLint glGetProgramResourceLocation(GLuint program_name, GLenum programInterface,
                                   const GLchar *name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    GLint result = -1;

    if (ctx == NULL)
        return -1;
    if (reflection_reject_inside_begin(ctx))
        return -1;
    if (name == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return -1;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        if (ctx->native_dispatch->glesGetProgramResourceLocation == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        } else {
            result = ctx->native_dispatch->glesGetProgramResourceLocation(program->native_name,
                                                                          programInterface, name);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return result;
}

GLint glGetProgramResourceLocationIndex(GLuint program_name, GLenum programInterface,
                                        const GLchar *name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    GLint result = -1;

    if (ctx == NULL)
        return -1;
    if (reflection_reject_inside_begin(ctx))
        return -1;
    if (name == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return -1;
    }
    if (programInterface != GL_PROGRAM_OUTPUT) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return -1;
    }
    if (ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesGetProgramResourceLocationIndex == NULL ||
        !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                "GL_EXT_blend_func_extended"))
        return glGetFragDataIndex(program_name, name);
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = lookup_program(ctx, program_name);
    if (program != NULL) {
        result = ctx->native_dispatch->glesGetProgramResourceLocationIndex(
            program->native_name, programInterface, name);
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return result;
}

GLint glGetProgramResourceLocationIndexEXT(GLuint program_name, GLenum programInterface,
                                           const GLchar *name) {
    return glGetProgramResourceLocationIndex(program_name, programInterface, name);
}

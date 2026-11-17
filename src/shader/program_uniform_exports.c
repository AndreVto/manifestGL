#include "mgl/gl.h"

#include "core/context.h"
#include "shader/shader_store.h"
#include "list/display_list.h"

#include <stdint.h>
#include <stdlib.h>

static struct mgl_program *program_uniform_target_locked(struct mgl_context *ctx,
                                                         GLuint program_name) {
    struct mgl_program *program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return NULL;
    }
    if (!program->linked) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    return program;
}

/* Uniform locations below -1 are invalid, never ignored. Target validation
 * precedes location validation, matching native GL's object error ordering. */
static struct mgl_program *program_uniform_target_location_locked(struct mgl_context *ctx,
                                                                  GLuint program_name,
                                                                  GLint location) {
    struct mgl_program *program = program_uniform_target_locked(ctx, program_name);
    if (program != NULL && location < -1) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    if (program != NULL && location != -1)
        program->compat_uniform_cache_valid = GL_FALSE;
    return program;
}

static GLuint program_uniform_current_native_locked(struct mgl_context *ctx) {
    struct mgl_program *current;
    if (ctx->current_program == 0u)
        return 0u;
    current = mgl_program_find(ctx, ctx->current_program);
    if (current == NULL || (!current->linked && !ctx->current_program_executable))
        return 0u;
    return current->native_name;
}

/* Returns 0 on failure, 1 when no switch was needed, 2 when a switch must be restored. */
static int program_uniform_begin_fallback(struct mgl_context *ctx, const struct mgl_program *target,
                                          GLuint *restore_native) {
    GLuint current_native = program_uniform_current_native_locked(ctx);
    if (current_native == target->native_name) {
        *restore_native = current_native;
        return 1;
    }
    if (ctx->current_program_executable && ctx->current_program != 0u) {
        struct mgl_program *current = mgl_program_find(ctx, ctx->current_program);
        if (current != NULL && !current->linked) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
    }
    if (ctx->transform_feedback_active && !ctx->transform_feedback_paused) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (ctx->native_dispatch->glesUseProgram == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    mgl_draw_use_native_program(ctx, target->native_name);
    *restore_native = current_native;
    return 2;
}

static void program_uniform_end_fallback(struct mgl_context *ctx, int mode, GLuint restore_native) {
    if (mode == 2)
        mgl_draw_use_native_program(ctx, restore_native);
}

static int program_uniform_reject_inside_begin(struct mgl_context *ctx) {
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 1;
    }
    return 0;
}

/* GLES requires transpose=GL_FALSE; desktop GL accepts row-major input. */
static const GLfloat *program_uniform_matrix_data(struct mgl_context *ctx, GLsizei count,
                                                  GLboolean transpose, const GLfloat *value,
                                                  size_t columns, size_t rows, GLfloat small[16],
                                                  GLfloat **allocated) {
    size_t n, matrix_size, matrix, column, row;
    GLfloat *out;
    *allocated = NULL;
    if (transpose == GL_FALSE || count == 0 || value == NULL)
        return value;
    matrix_size = columns * rows;
    if ((size_t)count > SIZE_MAX / matrix_size / sizeof(GLfloat)) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return NULL;
    }
    n = (size_t)count * matrix_size;
    out = n <= 16u ? small : malloc(n * sizeof(*out));
    if (out == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return NULL;
    }
    if (out != small)
        *allocated = out;
    for (matrix = 0u; matrix < (size_t)count; ++matrix) {
        const GLfloat *source = value + matrix * matrix_size;
        GLfloat *dest = out + matrix * matrix_size;
        for (column = 0u; column < columns; ++column)
            for (row = 0u; row < rows; ++row)
                dest[column * rows + row] = source[row * columns + column];
    }
    return out;
}

static int program_uniform_validate_count(struct mgl_context *ctx, GLsizei count) {
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    return 1;
}

static int program_uniform_validate_matrix(struct mgl_context *ctx, GLsizei count,
                                           GLboolean transpose) {
    if (!program_uniform_validate_count(ctx, count))
        return 0;
    if (transpose != GL_FALSE && transpose != GL_TRUE) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    return 1;
}

#define MGL_PROGRAM_UNIFORM_SCALAR_F1(name, native_member, fallback_member)                        \
    void name(GLuint program_name, GLint location, GLfloat v0) {                                   \
        struct mgl_context *ctx = mgl_context_current();                                           \
        struct mgl_program *program;                                                               \
        GLuint restore_native = 0u;                                                                \
        int mode;                                                                                  \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        if (program_uniform_reject_inside_begin(ctx))                                              \
            return;                                                                                \
        (void)pthread_mutex_lock(&ctx->share_group->lock);                                         \
        program = program_uniform_target_location_locked(ctx, program_name, location);             \
        if (program != NULL && location != -1) {                                                   \
            if (ctx->native_dispatch->native_member != NULL)                                       \
                ctx->native_dispatch->native_member(program->native_name, location, v0);           \
            else if (ctx->native_dispatch->fallback_member != NULL) {                              \
                mode = program_uniform_begin_fallback(ctx, program, &restore_native);              \
                if (mode != 0) {                                                                   \
                    ctx->native_dispatch->fallback_member(location, v0);                           \
                    program_uniform_end_fallback(ctx, mode, restore_native);                       \
                }                                                                                  \
            } else                                                                                 \
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);                                 \
        }                                                                                          \
        (void)pthread_mutex_unlock(&ctx->share_group->lock);                                       \
    }

#define MGL_PROGRAM_UNIFORM_SCALAR_F2(name, native_member, fallback_member)                        \
    void name(GLuint program_name, GLint location, GLfloat v0, GLfloat v1) {                       \
        struct mgl_context *ctx = mgl_context_current();                                           \
        struct mgl_program *program;                                                               \
        GLuint restore_native = 0u;                                                                \
        int mode;                                                                                  \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        if (program_uniform_reject_inside_begin(ctx))                                              \
            return;                                                                                \
        (void)pthread_mutex_lock(&ctx->share_group->lock);                                         \
        program = program_uniform_target_location_locked(ctx, program_name, location);             \
        if (program != NULL && location != -1) {                                                   \
            if (ctx->native_dispatch->native_member != NULL)                                       \
                ctx->native_dispatch->native_member(program->native_name, location, v0, v1);       \
            else if (ctx->native_dispatch->fallback_member != NULL) {                              \
                mode = program_uniform_begin_fallback(ctx, program, &restore_native);              \
                if (mode != 0) {                                                                   \
                    ctx->native_dispatch->fallback_member(location, v0, v1);                       \
                    program_uniform_end_fallback(ctx, mode, restore_native);                       \
                }                                                                                  \
            } else                                                                                 \
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);                                 \
        }                                                                                          \
        (void)pthread_mutex_unlock(&ctx->share_group->lock);                                       \
    }

#define MGL_PROGRAM_UNIFORM_SCALAR_F3(name, native_member, fallback_member)                        \
    void name(GLuint program_name, GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {           \
        struct mgl_context *ctx = mgl_context_current();                                           \
        struct mgl_program *program;                                                               \
        GLuint restore_native = 0u;                                                                \
        int mode;                                                                                  \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        if (program_uniform_reject_inside_begin(ctx))                                              \
            return;                                                                                \
        (void)pthread_mutex_lock(&ctx->share_group->lock);                                         \
        program = program_uniform_target_location_locked(ctx, program_name, location);             \
        if (program != NULL && location != -1) {                                                   \
            if (ctx->native_dispatch->native_member != NULL)                                       \
                ctx->native_dispatch->native_member(program->native_name, location, v0, v1, v2);   \
            else if (ctx->native_dispatch->fallback_member != NULL) {                              \
                mode = program_uniform_begin_fallback(ctx, program, &restore_native);              \
                if (mode != 0) {                                                                   \
                    ctx->native_dispatch->fallback_member(location, v0, v1, v2);                   \
                    program_uniform_end_fallback(ctx, mode, restore_native);                       \
                }                                                                                  \
            } else                                                                                 \
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);                                 \
        }                                                                                          \
        (void)pthread_mutex_unlock(&ctx->share_group->lock);                                       \
    }

#define MGL_PROGRAM_UNIFORM_SCALAR_F4(name, native_member, fallback_member)                        \
    void name(GLuint program_name, GLint location, GLfloat v0, GLfloat v1, GLfloat v2,             \
              GLfloat v3) {                                                                        \
        struct mgl_context *ctx = mgl_context_current();                                           \
        struct mgl_program *program;                                                               \
        GLuint restore_native = 0u;                                                                \
        int mode;                                                                                  \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        if (program_uniform_reject_inside_begin(ctx))                                              \
            return;                                                                                \
        (void)pthread_mutex_lock(&ctx->share_group->lock);                                         \
        program = program_uniform_target_location_locked(ctx, program_name, location);             \
        if (program != NULL && location != -1) {                                                   \
            if (ctx->native_dispatch->native_member != NULL)                                       \
                ctx->native_dispatch->native_member(program->native_name, location, v0, v1, v2,    \
                                                    v3);                                           \
            else if (ctx->native_dispatch->fallback_member != NULL) {                              \
                mode = program_uniform_begin_fallback(ctx, program, &restore_native);              \
                if (mode != 0) {                                                                   \
                    ctx->native_dispatch->fallback_member(location, v0, v1, v2, v3);               \
                    program_uniform_end_fallback(ctx, mode, restore_native);                       \
                }                                                                                  \
            } else                                                                                 \
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);                                 \
        }                                                                                          \
        (void)pthread_mutex_unlock(&ctx->share_group->lock);                                       \
    }

#define MGL_PROGRAM_UNIFORM_SCALAR_I1(name, native_member, fallback_member, type)                  \
    void name(GLuint program_name, GLint location, type v0) {                                      \
        struct mgl_context *ctx = mgl_context_current();                                           \
        struct mgl_program *program;                                                               \
        GLuint restore_native = 0u;                                                                \
        int mode;                                                                                  \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        if (program_uniform_reject_inside_begin(ctx))                                              \
            return;                                                                                \
        (void)pthread_mutex_lock(&ctx->share_group->lock);                                         \
        program = program_uniform_target_location_locked(ctx, program_name, location);             \
        if (program != NULL && location != -1) {                                                   \
            if (ctx->native_dispatch->native_member != NULL)                                       \
                ctx->native_dispatch->native_member(program->native_name, location, v0);           \
            else if (ctx->native_dispatch->fallback_member != NULL) {                              \
                mode = program_uniform_begin_fallback(ctx, program, &restore_native);              \
                if (mode != 0) {                                                                   \
                    ctx->native_dispatch->fallback_member(location, v0);                           \
                    program_uniform_end_fallback(ctx, mode, restore_native);                       \
                }                                                                                  \
            } else                                                                                 \
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);                                 \
        }                                                                                          \
        (void)pthread_mutex_unlock(&ctx->share_group->lock);                                       \
    }

#define MGL_PROGRAM_UNIFORM_SCALAR_I2(name, native_member, fallback_member, type)                  \
    void name(GLuint program_name, GLint location, type v0, type v1) {                             \
        struct mgl_context *ctx = mgl_context_current();                                           \
        struct mgl_program *program;                                                               \
        GLuint restore_native = 0u;                                                                \
        int mode;                                                                                  \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        if (program_uniform_reject_inside_begin(ctx))                                              \
            return;                                                                                \
        (void)pthread_mutex_lock(&ctx->share_group->lock);                                         \
        program = program_uniform_target_location_locked(ctx, program_name, location);             \
        if (program != NULL && location != -1) {                                                   \
            if (ctx->native_dispatch->native_member != NULL)                                       \
                ctx->native_dispatch->native_member(program->native_name, location, v0, v1);       \
            else if (ctx->native_dispatch->fallback_member != NULL) {                              \
                mode = program_uniform_begin_fallback(ctx, program, &restore_native);              \
                if (mode != 0) {                                                                   \
                    ctx->native_dispatch->fallback_member(location, v0, v1);                       \
                    program_uniform_end_fallback(ctx, mode, restore_native);                       \
                }                                                                                  \
            } else                                                                                 \
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);                                 \
        }                                                                                          \
        (void)pthread_mutex_unlock(&ctx->share_group->lock);                                       \
    }

#define MGL_PROGRAM_UNIFORM_SCALAR_I3(name, native_member, fallback_member, type)                  \
    void name(GLuint program_name, GLint location, type v0, type v1, type v2) {                    \
        struct mgl_context *ctx = mgl_context_current();                                           \
        struct mgl_program *program;                                                               \
        GLuint restore_native = 0u;                                                                \
        int mode;                                                                                  \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        if (program_uniform_reject_inside_begin(ctx))                                              \
            return;                                                                                \
        (void)pthread_mutex_lock(&ctx->share_group->lock);                                         \
        program = program_uniform_target_location_locked(ctx, program_name, location);             \
        if (program != NULL && location != -1) {                                                   \
            if (ctx->native_dispatch->native_member != NULL)                                       \
                ctx->native_dispatch->native_member(program->native_name, location, v0, v1, v2);   \
            else if (ctx->native_dispatch->fallback_member != NULL) {                              \
                mode = program_uniform_begin_fallback(ctx, program, &restore_native);              \
                if (mode != 0) {                                                                   \
                    ctx->native_dispatch->fallback_member(location, v0, v1, v2);                   \
                    program_uniform_end_fallback(ctx, mode, restore_native);                       \
                }                                                                                  \
            } else                                                                                 \
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);                                 \
        }                                                                                          \
        (void)pthread_mutex_unlock(&ctx->share_group->lock);                                       \
    }

#define MGL_PROGRAM_UNIFORM_SCALAR_I4(name, native_member, fallback_member, type)                  \
    void name(GLuint program_name, GLint location, type v0, type v1, type v2, type v3) {           \
        struct mgl_context *ctx = mgl_context_current();                                           \
        struct mgl_program *program;                                                               \
        GLuint restore_native = 0u;                                                                \
        int mode;                                                                                  \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        if (program_uniform_reject_inside_begin(ctx))                                              \
            return;                                                                                \
        (void)pthread_mutex_lock(&ctx->share_group->lock);                                         \
        program = program_uniform_target_location_locked(ctx, program_name, location);             \
        if (program != NULL && location != -1) {                                                   \
            if (ctx->native_dispatch->native_member != NULL)                                       \
                ctx->native_dispatch->native_member(program->native_name, location, v0, v1, v2,    \
                                                    v3);                                           \
            else if (ctx->native_dispatch->fallback_member != NULL) {                              \
                mode = program_uniform_begin_fallback(ctx, program, &restore_native);              \
                if (mode != 0) {                                                                   \
                    ctx->native_dispatch->fallback_member(location, v0, v1, v2, v3);               \
                    program_uniform_end_fallback(ctx, mode, restore_native);                       \
                }                                                                                  \
            } else                                                                                 \
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);                                 \
        }                                                                                          \
        (void)pthread_mutex_unlock(&ctx->share_group->lock);                                       \
    }

#define MGL_PROGRAM_UNIFORM_ARRAY(name, native_member, fallback_member, type)                      \
    void name(GLuint program_name, GLint location, GLsizei count, const type *value) {             \
        struct mgl_context *ctx = mgl_context_current();                                           \
        struct mgl_program *program;                                                               \
        GLuint restore_native = 0u;                                                                \
        int mode;                                                                                  \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        if (program_uniform_reject_inside_begin(ctx))                                              \
            return;                                                                                \
        if (!program_uniform_validate_count(ctx, count))                                           \
            return;                                                                                \
        (void)pthread_mutex_lock(&ctx->share_group->lock);                                         \
        program = program_uniform_target_location_locked(ctx, program_name, location);             \
        if (program != NULL && location != -1) {                                                   \
            if (ctx->native_dispatch->native_member != NULL)                                       \
                ctx->native_dispatch->native_member(program->native_name, location, count, value); \
            else if (ctx->native_dispatch->fallback_member != NULL) {                              \
                mode = program_uniform_begin_fallback(ctx, program, &restore_native);              \
                if (mode != 0) {                                                                   \
                    ctx->native_dispatch->fallback_member(location, count, value);                 \
                    program_uniform_end_fallback(ctx, mode, restore_native);                       \
                }                                                                                  \
            } else                                                                                 \
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);                                 \
        }                                                                                          \
        (void)pthread_mutex_unlock(&ctx->share_group->lock);                                       \
    }

#define MGL_PROGRAM_UNIFORM_MATRIX(name, native_member, fallback_member, columns, rows)            \
    void name(GLuint program_name, GLint location, GLsizei count, GLboolean transpose,             \
              const GLfloat *value) {                                                              \
        struct mgl_context *ctx = mgl_context_current();                                           \
        struct mgl_program *program;                                                               \
        GLuint restore_native = 0u;                                                                \
        int mode;                                                                                  \
        GLfloat small[16];                                                                         \
        GLfloat *allocated = NULL;                                                                 \
        const GLfloat *data;                                                                       \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        if (program_uniform_reject_inside_begin(ctx))                                              \
            return;                                                                                \
        if (!program_uniform_validate_matrix(ctx, count, transpose))                               \
            return;                                                                                \
        (void)pthread_mutex_lock(&ctx->share_group->lock);                                         \
        program = program_uniform_target_location_locked(ctx, program_name, location);             \
        if (program != NULL && location != -1) {                                                   \
            if (ctx->native_dispatch->native_member != NULL ||                                     \
                ctx->native_dispatch->fallback_member != NULL) {                                   \
                data = program_uniform_matrix_data(ctx, count, transpose, value, columns, rows,    \
                                                   small, &allocated);                             \
                if (data != NULL || (value == NULL && count == 0)) {                               \
                    if (ctx->native_dispatch->native_member != NULL)                               \
                        ctx->native_dispatch->native_member(program->native_name, location, count, \
                                                            GL_FALSE, data);                       \
                    else {                                                                         \
                        mode = program_uniform_begin_fallback(ctx, program, &restore_native);      \
                        if (mode != 0) {                                                           \
                            ctx->native_dispatch->fallback_member(location, count, GL_FALSE,       \
                                                                  data);                           \
                            program_uniform_end_fallback(ctx, mode, restore_native);               \
                        }                                                                          \
                    }                                                                              \
                }                                                                                  \
                free(allocated);                                                                   \
            } else                                                                                 \
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);                                 \
        }                                                                                          \
        (void)pthread_mutex_unlock(&ctx->share_group->lock);                                       \
    }

MGL_PROGRAM_UNIFORM_SCALAR_F1(glProgramUniform1f, glesProgramUniform1f, glesUniform1f)
MGL_PROGRAM_UNIFORM_SCALAR_F2(glProgramUniform2f, glesProgramUniform2f, glesUniform2f)
MGL_PROGRAM_UNIFORM_SCALAR_F3(glProgramUniform3f, glesProgramUniform3f, glesUniform3f)
MGL_PROGRAM_UNIFORM_SCALAR_F4(glProgramUniform4f, glesProgramUniform4f, glesUniform4f)
MGL_PROGRAM_UNIFORM_SCALAR_I1(glProgramUniform1i, glesProgramUniform1i, glesUniform1i, GLint)
MGL_PROGRAM_UNIFORM_SCALAR_I2(glProgramUniform2i, glesProgramUniform2i, glesUniform2i, GLint)
MGL_PROGRAM_UNIFORM_SCALAR_I3(glProgramUniform3i, glesProgramUniform3i, glesUniform3i, GLint)
MGL_PROGRAM_UNIFORM_SCALAR_I4(glProgramUniform4i, glesProgramUniform4i, glesUniform4i, GLint)
MGL_PROGRAM_UNIFORM_SCALAR_I1(glProgramUniform1ui, glesProgramUniform1ui, glesUniform1ui, GLuint)
MGL_PROGRAM_UNIFORM_SCALAR_I2(glProgramUniform2ui, glesProgramUniform2ui, glesUniform2ui, GLuint)
MGL_PROGRAM_UNIFORM_SCALAR_I3(glProgramUniform3ui, glesProgramUniform3ui, glesUniform3ui, GLuint)
MGL_PROGRAM_UNIFORM_SCALAR_I4(glProgramUniform4ui, glesProgramUniform4ui, glesUniform4ui, GLuint)
MGL_PROGRAM_UNIFORM_ARRAY(glProgramUniform1fv, glesProgramUniform1fv, glesUniform1fv, GLfloat)
MGL_PROGRAM_UNIFORM_ARRAY(glProgramUniform2fv, glesProgramUniform2fv, glesUniform2fv, GLfloat)
MGL_PROGRAM_UNIFORM_ARRAY(glProgramUniform3fv, glesProgramUniform3fv, glesUniform3fv, GLfloat)
MGL_PROGRAM_UNIFORM_ARRAY(glProgramUniform4fv, glesProgramUniform4fv, glesUniform4fv, GLfloat)
MGL_PROGRAM_UNIFORM_ARRAY(glProgramUniform1iv, glesProgramUniform1iv, glesUniform1iv, GLint)
MGL_PROGRAM_UNIFORM_ARRAY(glProgramUniform2iv, glesProgramUniform2iv, glesUniform2iv, GLint)
MGL_PROGRAM_UNIFORM_ARRAY(glProgramUniform3iv, glesProgramUniform3iv, glesUniform3iv, GLint)
MGL_PROGRAM_UNIFORM_ARRAY(glProgramUniform4iv, glesProgramUniform4iv, glesUniform4iv, GLint)
MGL_PROGRAM_UNIFORM_ARRAY(glProgramUniform1uiv, glesProgramUniform1uiv, glesUniform1uiv, GLuint)
MGL_PROGRAM_UNIFORM_ARRAY(glProgramUniform2uiv, glesProgramUniform2uiv, glesUniform2uiv, GLuint)
MGL_PROGRAM_UNIFORM_ARRAY(glProgramUniform3uiv, glesProgramUniform3uiv, glesUniform3uiv, GLuint)
MGL_PROGRAM_UNIFORM_ARRAY(glProgramUniform4uiv, glesProgramUniform4uiv, glesUniform4uiv, GLuint)
MGL_PROGRAM_UNIFORM_MATRIX(glProgramUniformMatrix2fv, glesProgramUniformMatrix2fv,
                           glesUniformMatrix2fv, 2u, 2u)
MGL_PROGRAM_UNIFORM_MATRIX(glProgramUniformMatrix3fv, glesProgramUniformMatrix3fv,
                           glesUniformMatrix3fv, 3u, 3u)
MGL_PROGRAM_UNIFORM_MATRIX(glProgramUniformMatrix4fv, glesProgramUniformMatrix4fv,
                           glesUniformMatrix4fv, 4u, 4u)
MGL_PROGRAM_UNIFORM_MATRIX(glProgramUniformMatrix2x3fv, glesProgramUniformMatrix2x3fv,
                           glesUniformMatrix2x3fv, 2u, 3u)
MGL_PROGRAM_UNIFORM_MATRIX(glProgramUniformMatrix3x2fv, glesProgramUniformMatrix3x2fv,
                           glesUniformMatrix3x2fv, 3u, 2u)
MGL_PROGRAM_UNIFORM_MATRIX(glProgramUniformMatrix2x4fv, glesProgramUniformMatrix2x4fv,
                           glesUniformMatrix2x4fv, 2u, 4u)
MGL_PROGRAM_UNIFORM_MATRIX(glProgramUniformMatrix4x2fv, glesProgramUniformMatrix4x2fv,
                           glesUniformMatrix4x2fv, 4u, 2u)
MGL_PROGRAM_UNIFORM_MATRIX(glProgramUniformMatrix3x4fv, glesProgramUniformMatrix3x4fv,
                           glesUniformMatrix3x4fv, 3u, 4u)
MGL_PROGRAM_UNIFORM_MATRIX(glProgramUniformMatrix4x3fv, glesProgramUniformMatrix4x3fv,
                           glesUniformMatrix4x3fv, 4u, 3u)

#undef MGL_PROGRAM_UNIFORM_SCALAR_F1
#undef MGL_PROGRAM_UNIFORM_SCALAR_F2
#undef MGL_PROGRAM_UNIFORM_SCALAR_F3
#undef MGL_PROGRAM_UNIFORM_SCALAR_F4
#undef MGL_PROGRAM_UNIFORM_SCALAR_I1
#undef MGL_PROGRAM_UNIFORM_SCALAR_I2
#undef MGL_PROGRAM_UNIFORM_SCALAR_I3
#undef MGL_PROGRAM_UNIFORM_SCALAR_I4
#undef MGL_PROGRAM_UNIFORM_ARRAY
#undef MGL_PROGRAM_UNIFORM_MATRIX

#define MGL_PROGRAM_UNIFORM_EXT_SCALAR_F1(extname, corename)                                       \
    void extname(GLuint program, GLint location, GLfloat v0) {                                     \
        struct mgl_context *ctx = mgl_context_current();                                           \
        GLfloat v[1] = {v0};                                                                       \
        if (mgl_list_record_program_uniform_f_ext(ctx, program, location, 1u, 1, v, GL_TRUE))      \
            return;                                                                                \
        corename(program, location, v0);                                                           \
    }
#define MGL_PROGRAM_UNIFORM_EXT_SCALAR_F2(extname, corename)                                       \
    void extname(GLuint program, GLint location, GLfloat v0, GLfloat v1) {                         \
        struct mgl_context *ctx = mgl_context_current();                                           \
        GLfloat v[2] = {v0, v1};                                                                   \
        if (mgl_list_record_program_uniform_f_ext(ctx, program, location, 2u, 1, v, GL_TRUE))      \
            return;                                                                                \
        corename(program, location, v0, v1);                                                       \
    }
#define MGL_PROGRAM_UNIFORM_EXT_SCALAR_F3(extname, corename)                                       \
    void extname(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {             \
        struct mgl_context *ctx = mgl_context_current();                                           \
        GLfloat v[3] = {v0, v1, v2};                                                               \
        if (mgl_list_record_program_uniform_f_ext(ctx, program, location, 3u, 1, v, GL_TRUE))      \
            return;                                                                                \
        corename(program, location, v0, v1, v2);                                                   \
    }
#define MGL_PROGRAM_UNIFORM_EXT_SCALAR_F4(extname, corename)                                       \
    void extname(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) { \
        struct mgl_context *ctx = mgl_context_current();                                           \
        GLfloat v[4] = {v0, v1, v2, v3};                                                           \
        if (mgl_list_record_program_uniform_f_ext(ctx, program, location, 4u, 1, v, GL_TRUE))      \
            return;                                                                                \
        corename(program, location, v0, v1, v2, v3);                                               \
    }
#define MGL_PROGRAM_UNIFORM_EXT_SCALAR_I1(extname, corename)                                       \
    void extname(GLuint program, GLint location, GLint v0) {                                       \
        struct mgl_context *ctx = mgl_context_current();                                           \
        GLint v[1] = {v0};                                                                         \
        if (mgl_list_record_program_uniform_i_ext(ctx, program, location, 1u, 1, v, GL_TRUE))      \
            return;                                                                                \
        corename(program, location, v0);                                                           \
    }
#define MGL_PROGRAM_UNIFORM_EXT_SCALAR_I2(extname, corename)                                       \
    void extname(GLuint program, GLint location, GLint v0, GLint v1) {                             \
        struct mgl_context *ctx = mgl_context_current();                                           \
        GLint v[2] = {v0, v1};                                                                     \
        if (mgl_list_record_program_uniform_i_ext(ctx, program, location, 2u, 1, v, GL_TRUE))      \
            return;                                                                                \
        corename(program, location, v0, v1);                                                       \
    }
#define MGL_PROGRAM_UNIFORM_EXT_SCALAR_I3(extname, corename)                                       \
    void extname(GLuint program, GLint location, GLint v0, GLint v1, GLint v2) {                   \
        struct mgl_context *ctx = mgl_context_current();                                           \
        GLint v[3] = {v0, v1, v2};                                                                 \
        if (mgl_list_record_program_uniform_i_ext(ctx, program, location, 3u, 1, v, GL_TRUE))      \
            return;                                                                                \
        corename(program, location, v0, v1, v2);                                                   \
    }
#define MGL_PROGRAM_UNIFORM_EXT_SCALAR_I4(extname, corename)                                       \
    void extname(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {         \
        struct mgl_context *ctx = mgl_context_current();                                           \
        GLint v[4] = {v0, v1, v2, v3};                                                             \
        if (mgl_list_record_program_uniform_i_ext(ctx, program, location, 4u, 1, v, GL_TRUE))      \
            return;                                                                                \
        corename(program, location, v0, v1, v2, v3);                                               \
    }
#define MGL_PROGRAM_UNIFORM_EXT_ARRAY_F(extname, corename, comps)                                  \
    void extname(GLuint program, GLint location, GLsizei count, const GLfloat *value) {            \
        struct mgl_context *ctx = mgl_context_current();                                           \
        if (ctx != NULL && program_uniform_reject_inside_begin(ctx))                               \
            return;                                                                                \
        if (ctx != NULL && !program_uniform_validate_count(ctx, count))                            \
            return;                                                                                \
        if (mgl_list_record_program_uniform_f_ext(ctx, program, location, comps, count, value,     \
                                                  GL_FALSE))                                       \
            return;                                                                                \
        corename(program, location, count, value);                                                 \
    }
#define MGL_PROGRAM_UNIFORM_EXT_ARRAY_I(extname, corename, comps)                                  \
    void extname(GLuint program, GLint location, GLsizei count, const GLint *value) {              \
        struct mgl_context *ctx = mgl_context_current();                                           \
        if (ctx != NULL && program_uniform_reject_inside_begin(ctx))                               \
            return;                                                                                \
        if (ctx != NULL && !program_uniform_validate_count(ctx, count))                            \
            return;                                                                                \
        if (mgl_list_record_program_uniform_i_ext(ctx, program, location, comps, count, value,     \
                                                  GL_FALSE))                                       \
            return;                                                                                \
        corename(program, location, count, value);                                                 \
    }
#define MGL_PROGRAM_UNIFORM_EXT_MATRIX(extname, corename, dim)                                     \
    void extname(GLuint program, GLint location, GLsizei count, GLboolean transpose,               \
                 const GLfloat *value) {                                                           \
        struct mgl_context *ctx = mgl_context_current();                                           \
        if (ctx != NULL && program_uniform_reject_inside_begin(ctx))                               \
            return;                                                                                \
        if (ctx != NULL && !program_uniform_validate_matrix(ctx, count, transpose))                \
            return;                                                                                \
        if (mgl_list_record_program_uniform_matrix_ext(ctx, program, location, dim, count,         \
                                                       transpose, value))                          \
            return;                                                                                \
        corename(program, location, count, transpose, value);                                      \
    }

MGL_PROGRAM_UNIFORM_EXT_SCALAR_F1(glProgramUniform1fEXT, glProgramUniform1f)
MGL_PROGRAM_UNIFORM_EXT_SCALAR_F2(glProgramUniform2fEXT, glProgramUniform2f)
MGL_PROGRAM_UNIFORM_EXT_SCALAR_F3(glProgramUniform3fEXT, glProgramUniform3f)
MGL_PROGRAM_UNIFORM_EXT_SCALAR_F4(glProgramUniform4fEXT, glProgramUniform4f)
MGL_PROGRAM_UNIFORM_EXT_SCALAR_I1(glProgramUniform1iEXT, glProgramUniform1i)
MGL_PROGRAM_UNIFORM_EXT_SCALAR_I2(glProgramUniform2iEXT, glProgramUniform2i)
MGL_PROGRAM_UNIFORM_EXT_SCALAR_I3(glProgramUniform3iEXT, glProgramUniform3i)
MGL_PROGRAM_UNIFORM_EXT_SCALAR_I4(glProgramUniform4iEXT, glProgramUniform4i)
MGL_PROGRAM_UNIFORM_EXT_ARRAY_F(glProgramUniform1fvEXT, glProgramUniform1fv, 1u)
MGL_PROGRAM_UNIFORM_EXT_ARRAY_F(glProgramUniform2fvEXT, glProgramUniform2fv, 2u)
MGL_PROGRAM_UNIFORM_EXT_ARRAY_F(glProgramUniform3fvEXT, glProgramUniform3fv, 3u)
MGL_PROGRAM_UNIFORM_EXT_ARRAY_F(glProgramUniform4fvEXT, glProgramUniform4fv, 4u)
MGL_PROGRAM_UNIFORM_EXT_ARRAY_I(glProgramUniform1ivEXT, glProgramUniform1iv, 1u)
MGL_PROGRAM_UNIFORM_EXT_ARRAY_I(glProgramUniform2ivEXT, glProgramUniform2iv, 2u)
MGL_PROGRAM_UNIFORM_EXT_ARRAY_I(glProgramUniform3ivEXT, glProgramUniform3iv, 3u)
MGL_PROGRAM_UNIFORM_EXT_ARRAY_I(glProgramUniform4ivEXT, glProgramUniform4iv, 4u)
MGL_PROGRAM_UNIFORM_EXT_MATRIX(glProgramUniformMatrix2fvEXT, glProgramUniformMatrix2fv, 2u)
MGL_PROGRAM_UNIFORM_EXT_MATRIX(glProgramUniformMatrix3fvEXT, glProgramUniformMatrix3fv, 3u)
MGL_PROGRAM_UNIFORM_EXT_MATRIX(glProgramUniformMatrix4fvEXT, glProgramUniformMatrix4fv, 4u)

#undef MGL_PROGRAM_UNIFORM_EXT_SCALAR_F1
#undef MGL_PROGRAM_UNIFORM_EXT_SCALAR_F2
#undef MGL_PROGRAM_UNIFORM_EXT_SCALAR_F3
#undef MGL_PROGRAM_UNIFORM_EXT_SCALAR_F4
#undef MGL_PROGRAM_UNIFORM_EXT_SCALAR_I1
#undef MGL_PROGRAM_UNIFORM_EXT_SCALAR_I2
#undef MGL_PROGRAM_UNIFORM_EXT_SCALAR_I3
#undef MGL_PROGRAM_UNIFORM_EXT_SCALAR_I4
#undef MGL_PROGRAM_UNIFORM_EXT_ARRAY_F
#undef MGL_PROGRAM_UNIFORM_EXT_ARRAY_I
#undef MGL_PROGRAM_UNIFORM_EXT_MATRIX

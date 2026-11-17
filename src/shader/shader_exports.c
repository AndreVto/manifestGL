#include "mgl/gl.h"

#include "core/context.h"
#include "core/render_diag.h"
#include "shader/shader_store.h"
#include "shader/translator.h"
#include "compiler/spirv_toolchain.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *duplicate_text(const char *text) {
    size_t n;
    char *out;
    if (text == NULL)
        text = "";
    n = strlen(text);
    out = malloc(n + 1u);
    if (out != NULL)
        memcpy(out, text, n + 1u);
    return out;
}

static GLenum shader_name_error_locked(struct mgl_context *ctx, GLuint name) {
    return mgl_program_find(ctx, name) != NULL ? GL_INVALID_OPERATION : GL_INVALID_VALUE;
}

static GLenum program_name_error_locked(struct mgl_context *ctx, GLuint name) {
    return mgl_shader_find(ctx, name) != NULL ? GL_INVALID_OPERATION : GL_INVALID_VALUE;
}

static int shader_reject_inside_begin(struct mgl_context *ctx) {
    if (ctx != NULL && ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 1;
    }
    return 0;
}

char *mgl_native_shader_log(struct mgl_context *ctx, GLuint native_shader) {
    GLint len = 0;
    GLsizei written = 0;
    char *log;
    ctx->native_dispatch->glesGetShaderiv(native_shader, GL_INFO_LOG_LENGTH, &len);
    if (len <= 1)
        return duplicate_text("");
    log = calloc((size_t)len, 1u);
    if (log == NULL)
        return NULL;
    ctx->native_dispatch->glesGetShaderInfoLog(native_shader, len, &written, log);
    if (written >= 0 && written < len)
        log[written] = '\0';
    else
        log[len - 1] = '\0';
    return log;
}

static char *native_program_log(struct mgl_context *ctx, GLuint native_program) {
    GLint len = 0;
    GLsizei written = 0;
    char *log;
    ctx->native_dispatch->glesGetProgramiv(native_program, GL_INFO_LOG_LENGTH, &len);
    if (len <= 1)
        return duplicate_text("");
    log = calloc((size_t)len, 1u);
    if (log == NULL)
        return NULL;
    ctx->native_dispatch->glesGetProgramInfoLog(native_program, len, &written, log);
    if (written >= 0 && written < len)
        log[written] = '\0';
    else
        log[len - 1] = '\0';
    return log;
}

GLuint glCreateShader(GLenum type) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glCreateShader",
                        (const struct mgl_error_arg[]){{"type", (uint64_t)(uintptr_t)type}}, 1u);
    struct mgl_shader *shader;
    if (ctx == NULL)
        return 0u;
    if (shader_reject_inside_begin(ctx))
        return 0u;
    if (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER && type != GL_GEOMETRY_SHADER &&
        type != GL_TESS_CONTROL_SHADER && type != GL_TESS_EVALUATION_SHADER &&
        type != GL_COMPUTE_SHADER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0u;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    shader = mgl_shader_create(ctx, type);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    if (shader == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0u;
    }
    return shader->name;
}

void glShaderSource(GLuint shader_name, GLsizei count, const GLchar *const *string,
                    const GLint *length) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_shader *shader;
    size_t total = 0u;
    GLsizei i;
    char *joined;
    char *dst;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    shader = mgl_shader_find(ctx, shader_name);
    if (shader == NULL) {
        GLenum error = shader_name_error_locked(ctx, shader_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    if (count == 0) {
        joined = duplicate_text("");
        if (joined == NULL) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        free(shader->source);
        if (shader->spirv_module != NULL)
            shader->compiled = GL_FALSE;
        mgl_shader_set_spirv_module(shader, NULL);
        shader->spirv_specialized = GL_FALSE;
        shader->source = joined;
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    if (string == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < count; ++i) {
        size_t part;
        if (string[i] == NULL)
            continue;
        part = (length != NULL && length[i] >= 0) ? (size_t)length[i] : strlen(string[i]);
        if (part > SIZE_MAX - total - 1u) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        total += part;
    }
    joined = malloc(total + 1u);
    if (joined == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    dst = joined;
    for (i = 0; i < count; ++i) {
        size_t part;
        if (string[i] == NULL)
            continue;
        part = (length != NULL && length[i] >= 0) ? (size_t)length[i] : strlen(string[i]);
        if (part != 0u) {
            memcpy(dst, string[i], part);
            dst += part;
        }
    }
    *dst = '\0';
    free(shader->source);
    if (shader->spirv_module != NULL)
        shader->compiled = GL_FALSE;
    mgl_shader_set_spirv_module(shader, NULL);
    shader->spirv_specialized = GL_FALSE;
    shader->source = joined;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glCompileShader(GLuint shader_name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_shader *shader;
    char *translation_log = NULL;
    char *translated;
    char *native_log;
    GLint status = GL_FALSE;
    const GLchar *source_ptr;
    char *located = NULL;
    uint64_t perf_started;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    perf_started = mgl_perf_diag_now();
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    shader = mgl_shader_find(ctx, shader_name);
    if (shader == NULL) {
        GLenum error = shader_name_error_locked(ctx, shader_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    if (shader->spirv_module != NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    translated = mgl_translate_glsl(shader->type, shader->source != NULL ? shader->source : "",
                                    &translation_log);
    if (translated == NULL) {
        free(shader->info_log);
        shader->info_log = duplicate_text("mGL shader translation failed");
        shader->compiled = GL_FALSE;
        mgl_render_diag_shader(ctx, "translation", shader_name, shader->native_name, shader->type,
                               shader->compiled,
                               translation_log != NULL ? translation_log : shader->info_log);
        mgl_render_diag_shader_source(ctx, shader_name, "original", shader->source);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        free(translation_log);
        mgl_perf_diag_shader(ctx, 0, perf_started);
        return;
    }
    free(shader->translated_source);
    shader->translated_source = translated;
    if (shader->type == GL_FRAGMENT_SHADER)
        located = mgl_fragment_output_locations(shader->translated_source, 0u, NULL);
    source_ptr = located != NULL ? located : shader->translated_source;
    ctx->native_dispatch->glesShaderSource(shader->native_name, 1, &source_ptr, NULL);
    ctx->native_dispatch->glesCompileShader(shader->native_name);
    ctx->native_dispatch->glesGetShaderiv(shader->native_name, GL_COMPILE_STATUS, &status);
    native_log = mgl_native_shader_log(ctx, shader->native_name);
    free(shader->info_log);
    if (native_log != NULL && native_log[0] != '\0')
        shader->info_log = native_log;
    else {
        free(native_log);
        shader->info_log = translation_log != NULL ? translation_log : duplicate_text("");
        translation_log = NULL;
    }
    shader->compiled = status ? GL_TRUE : GL_FALSE;
    mgl_render_diag_shader(ctx, "gles", shader_name, shader->native_name, shader->type,
                           shader->compiled, shader->info_log);
    if (!shader->compiled) {
        mgl_render_diag_shader_source(ctx, shader_name, "original", shader->source);
        mgl_render_diag_shader_source(ctx, shader_name, "translated", source_ptr);
    }
    free(located);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    free(translation_log);
    mgl_perf_diag_shader(ctx, 0, perf_started);
}

void glShaderBinary(GLsizei count, const GLuint *shader_names, GLenum binary_format,
                    const void *binary, GLsizei length) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glShaderBinary",
        (const struct mgl_error_arg[]){{"count", (uint64_t)(uintptr_t)count},
                                       {"shader_names", (uint64_t)(uintptr_t)shader_names},
                                       {"binary_format", (uint64_t)(uintptr_t)binary_format},
                                       {"binary", (uint64_t)(uintptr_t)binary},
                                       {"length", (uint64_t)(uintptr_t)length}},
        5u);
    struct mgl_shader **shader_objects = NULL;
    GLuint *native_names = NULL;
    GLint *formats = NULL;
    GLint format_count = 0;
    GLenum native_error = GL_NO_ERROR;
    GLsizei i;
    int format_supported = 0;

    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (count < 0 || length < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (count > 0 && shader_names == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (binary_format == GL_SHADER_BINARY_FORMAT_SPIR_V) {
        struct mgl_spirv_module *module;
        if (count == 0)
            return;
        if (binary == NULL || length < 20 || (length & 3) != 0) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        module = mgl_spirv_module_create(binary, (size_t)length);
        if (module == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        if (!mgl_spirv_validate_module(module->words, module->word_count)) {
            mgl_spirv_module_release(module);
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        (void)pthread_mutex_lock(&ctx->share_group->lock);
        for (i = 0; i < count; ++i) {
            if (mgl_shader_find(ctx, shader_names[i]) == NULL) {
                GLenum error = shader_name_error_locked(ctx, shader_names[i]);
                (void)pthread_mutex_unlock(&ctx->share_group->lock);
                mgl_spirv_module_release(module);
                mgl_error_set(&ctx->errors, error);
                return;
            }
        }
        for (i = 0; i < count; ++i) {
            struct mgl_shader *shader = mgl_shader_find(ctx, shader_names[i]);
            mgl_shader_set_spirv_module(shader, module);
            shader->spirv_specialized = GL_FALSE;
            shader->compiled = GL_FALSE;
            free(shader->source);
            shader->source = NULL;
            free(shader->translated_source);
            shader->translated_source = NULL;
            free(shader->info_log);
            shader->info_log = NULL;
        }
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_spirv_module_release(module);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesShaderBinary == NULL ||
        ctx->native_dispatch->glesGetIntegerv == NULL ||
        ctx->native_dispatch->glesGetShaderiv == NULL ||
        ctx->native_dispatch->glesGetShaderInfoLog == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }

    if (count > 0) {
        shader_objects = calloc((size_t)count, sizeof(*shader_objects));
        native_names = calloc((size_t)count, sizeof(*native_names));
        if (shader_objects == NULL || native_names == NULL) {
            free(shader_objects);
            free(native_names);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
    }

    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < count; ++i) {
        GLsizei j;
        shader_objects[i] = mgl_shader_find(ctx, shader_names[i]);
        if (shader_objects[i] == NULL) {
            GLenum error = shader_name_error_locked(ctx, shader_names[i]);
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            free(shader_objects);
            free(native_names);
            mgl_error_set(&ctx->errors, error);
            return;
        }
        for (j = 0; j < i; ++j) {
            if (shader_objects[j]->type == shader_objects[i]->type) {
                (void)pthread_mutex_unlock(&ctx->share_group->lock);
                free(shader_objects);
                free(native_names);
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
        }
        native_names[i] = shader_objects[i]->native_name;
    }

    ctx->native_dispatch->glesGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &format_count);
    if (format_count > 0) {
        formats = calloc((size_t)format_count, sizeof(*formats));
        if (formats == NULL) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            free(shader_objects);
            free(native_names);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        ctx->native_dispatch->glesGetIntegerv(GL_SHADER_BINARY_FORMATS, formats);
        for (i = 0; i < format_count; ++i) {
            if ((GLenum)formats[i] == binary_format) {
                format_supported = 1;
                break;
            }
        }
    }
    free(formats);
    if (!format_supported) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        free(shader_objects);
        free(native_names);
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }

    if (ctx->native_dispatch->glesGetError != NULL)
        while (ctx->native_dispatch->glesGetError() != GL_NO_ERROR) {}
    ctx->native_dispatch->glesShaderBinary(count, native_names, binary_format, binary, length);
    if (ctx->native_dispatch->glesGetError != NULL)
        native_error = ctx->native_dispatch->glesGetError();

    for (i = 0; i < count; ++i) {
        GLint status = GL_FALSE;
        char *log;
        ctx->native_dispatch->glesGetShaderiv(shader_objects[i]->native_name, GL_COMPILE_STATUS,
                                              &status);
        log = mgl_native_shader_log(ctx, shader_objects[i]->native_name);
        free(shader_objects[i]->translated_source);
        free(shader_objects[i]->info_log);
        shader_objects[i]->translated_source = NULL;
        shader_objects[i]->info_log = log != NULL ? log : duplicate_text("");
        mgl_shader_set_spirv_module(shader_objects[i], NULL);
        shader_objects[i]->spirv_specialized = GL_FALSE;
        shader_objects[i]->compiled = status ? GL_TRUE : GL_FALSE;
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    free(shader_objects);
    free(native_names);
    if (native_error != GL_NO_ERROR)
        mgl_error_set(&ctx->errors, native_error);
}

void glSpecializeShader(GLuint shader_name, const GLchar *entry_point, GLuint num_constants,
                        const GLuint *constant_indices, const GLuint *constant_values) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_shader *shader;
    char *gles_source = NULL, *compiler_log = NULL, *native_log = NULL;
    const GLchar *source;
    GLint status = GL_FALSE;
    GLenum error = GL_INVALID_OPERATION;
    if (ctx == NULL || shader_reject_inside_begin(ctx))
        return;
    if (entry_point == NULL ||
        (num_constants != 0u && (constant_indices == NULL || constant_values == NULL))) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    shader = mgl_shader_find(ctx, shader_name);
    if (shader == NULL) {
        error = shader_name_error_locked(ctx, shader_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    if (shader->spirv_module == NULL || shader->spirv_specialized) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!mgl_spirv_specialize_glsl(shader->type, shader->spirv_module->words,
                                   shader->spirv_module->word_count, entry_point, num_constants,
                                   constant_indices, constant_values, &gles_source, &compiler_log,
                                   &error)) {
        if (error == GL_INVALID_VALUE || error == GL_INVALID_OPERATION) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            free(compiler_log);
            mgl_error_set(&ctx->errors, error);
            return;
        }
        free(shader->info_log);
        shader->info_log =
            compiler_log != NULL ? compiler_log : duplicate_text("SPIR-V specialization failed");
        shader->compiled = GL_FALSE;
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    free(compiler_log);
    shader->spirv_specialized = GL_TRUE;
    free(shader->translated_source);
    shader->translated_source = gles_source;
    source = shader->translated_source;
    ctx->native_dispatch->glesShaderSource(shader->native_name, 1, &source, NULL);
    ctx->native_dispatch->glesCompileShader(shader->native_name);
    ctx->native_dispatch->glesGetShaderiv(shader->native_name, GL_COMPILE_STATUS, &status);
    native_log = mgl_native_shader_log(ctx, shader->native_name);
    free(shader->info_log);
    shader->info_log = native_log != NULL ? native_log : duplicate_text("");
    shader->compiled = status ? GL_TRUE : GL_FALSE;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glReleaseShaderCompiler(void) {
    /* ARB_ES2_compatibility defines this as an application hint. mGL owns
     * source translation and must keep later CompileShader usable, so retaining
     * compiler resources is a conforming no-op. */
}

void glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint *range,
                                GLint *precision) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glGetShaderPrecisionFormat",
        (const struct mgl_error_arg[]){{"shadertype", (uint64_t)(uintptr_t)shadertype},
                                       {"precisiontype", (uint64_t)(uintptr_t)precisiontype},
                                       {"range", (uint64_t)(uintptr_t)range},
                                       {"precision", (uint64_t)(uintptr_t)precision}},
        4u);
    if (ctx == NULL || range == NULL || precision == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (shadertype != GL_VERTEX_SHADER && shadertype != GL_FRAGMENT_SHADER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    switch (precisiontype) {
        case GL_LOW_FLOAT:
        case GL_MEDIUM_FLOAT:
        case GL_HIGH_FLOAT:
        case GL_LOW_INT:
        case GL_MEDIUM_INT:
        case GL_HIGH_INT:
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
    }
    if (ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesGetShaderPrecisionFormat == NULL) {
        /* A successfully initialized GLES2+ native always provides this entry
         * point. Keep outputs untouched in synthetic/incomplete contexts. */
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesGetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
}

void glGetShaderiv(GLuint shader_name, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glGetShaderiv",
        (const struct mgl_error_arg[]){{"shader_name", (uint64_t)(uintptr_t)shader_name},
                                       {"pname", (uint64_t)(uintptr_t)pname},
                                       {"params", (uint64_t)(uintptr_t)params}},
        3u);
    struct mgl_shader *shader;
    if (ctx == NULL || params == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    shader = mgl_shader_find(ctx, shader_name);
    if (shader == NULL) {
        GLenum error = shader_name_error_locked(ctx, shader_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    switch (pname) {
        case GL_SHADER_TYPE:
            *params = (GLint)shader->type;
            break;
        case GL_DELETE_STATUS:
            *params = shader->delete_pending ? GL_TRUE : GL_FALSE;
            break;
        case GL_COMPILE_STATUS:
            *params = shader->compiled ? GL_TRUE : GL_FALSE;
            break;
        case GL_INFO_LOG_LENGTH:
            *params = (GLint)(shader->info_log != NULL ? strlen(shader->info_log) + 1u : 0u);
            break;
        case GL_SHADER_SOURCE_LENGTH:
            *params = (GLint)(shader->source != NULL ? strlen(shader->source) + 1u : 0u);
            break;
        case GL_SPIR_V_BINARY:
            *params = shader->spirv_module != NULL ? GL_TRUE : GL_FALSE;
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

static void copy_log_string(const char *source, GLsizei buf_size, GLsizei *length, GLchar *dest) {
    size_t src_len = source != NULL ? strlen(source) : 0u;
    size_t copy_len = 0u;
    if (buf_size > 0 && dest != NULL) {
        copy_len = src_len < (size_t)(buf_size - 1) ? src_len : (size_t)(buf_size - 1);
        if (copy_len != 0u)
            memcpy(dest, source, copy_len);
        dest[copy_len] = '\0';
    }
    if (length != NULL)
        *length = (GLsizei)copy_len;
}

void glGetShaderInfoLog(GLuint shader_name, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_shader *shader;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    shader = mgl_shader_find(ctx, shader_name);
    if (shader == NULL) {
        GLenum error = shader_name_error_locked(ctx, shader_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    copy_log_string(shader->info_log, bufSize, length, infoLog);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetShaderSource(GLuint shader_name, GLsizei bufSize, GLsizei *length, GLchar *source) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_shader *shader;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    shader = mgl_shader_find(ctx, shader_name);
    if (shader == NULL) {
        GLenum error = shader_name_error_locked(ctx, shader_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    copy_log_string(shader->source, bufSize, length, source);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glDeleteShader(GLuint shader_name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_shader *shader;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (shader_name == 0u)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    shader = mgl_shader_find(ctx, shader_name);
    if (shader == NULL) {
        GLenum error = shader_name_error_locked(ctx, shader_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    shader->delete_pending = GL_TRUE;
    mgl_shader_maybe_destroy(ctx, shader);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

GLboolean glIsShader(GLuint shader_name) {
    struct mgl_context *ctx = mgl_context_current();
    GLboolean out;
    if (ctx == NULL)
        return GL_FALSE;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    if (shader_name == 0u)
        return GL_FALSE;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    out = mgl_shader_find(ctx, shader_name) != NULL ? GL_TRUE : GL_FALSE;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return out;
}

GLuint glCreateProgram(void) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    if (ctx == NULL)
        return 0u;
    if (shader_reject_inside_begin(ctx))
        return 0u;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_create(ctx);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    if (program == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0u;
    }
    return program->name;
}

void glAttachShader(GLuint program_name, GLuint shader_name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    struct mgl_shader *shader;
    unsigned i;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    shader = mgl_shader_find(ctx, shader_name);
    if (shader == NULL) {
        GLenum error = shader_name_error_locked(ctx, shader_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    for (i = 0; i < program->attached_count; ++i) {
        if (program->attached[i] == shader) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
    }
    if (program->attached_count >= MGL_MAX_ATTACHED_SHADERS) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    program->attached[program->attached_count++] = shader;
    ++shader->attach_count;
    ctx->native_dispatch->glesAttachShader(program->native_name, shader->native_name);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glDetachShader(GLuint program_name, GLuint shader_name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    struct mgl_shader *shader;
    unsigned i;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    shader = mgl_shader_find(ctx, shader_name);
    if (shader == NULL) {
        GLenum error = shader_name_error_locked(ctx, shader_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    for (i = 0; i < program->attached_count; ++i) {
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
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
}

void glBindAttribLocation(GLuint program_name, GLuint index, const GLchar *name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    if (name == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesBindAttribLocation(program->native_name, index, name);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

static struct mgl_frag_output_binding *find_frag_output_binding(struct mgl_program *program,
                                                                const char *name) {
    size_t i;
    if (program == NULL || name == NULL)
        return NULL;
    for (i = 0u; i < program->frag_output_binding_count; ++i) {
        if (strcmp(program->frag_output_bindings[i].name, name) == 0)
            return &program->frag_output_bindings[i];
    }
    return NULL;
}

static struct mgl_frag_output_binding *
get_or_create_frag_output_binding(struct mgl_context *ctx, struct mgl_program *program,
                                  const char *name) {
    struct mgl_frag_output_binding *binding;
    char *name_copy;
    size_t next_capacity;
    struct mgl_frag_output_binding *next;
    binding = find_frag_output_binding(program, name);
    if (binding != NULL)
        return binding;
    if (program->frag_output_binding_count == program->frag_output_binding_capacity) {
        next_capacity = program->frag_output_binding_capacity != 0u
                            ? program->frag_output_binding_capacity * 2u
                            : 4u;
        if (next_capacity < program->frag_output_binding_capacity ||
            next_capacity > SIZE_MAX / sizeof(*program->frag_output_bindings)) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return NULL;
        }
        next = realloc(program->frag_output_bindings,
                       next_capacity * sizeof(*program->frag_output_bindings));
        if (next == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return NULL;
        }
        program->frag_output_bindings = next;
        program->frag_output_binding_capacity = next_capacity;
    }
    name_copy = duplicate_text(name);
    if (name_copy == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return NULL;
    }
    binding = &program->frag_output_bindings[program->frag_output_binding_count++];
    memset(binding, 0, sizeof(*binding));
    binding->name = name_copy;
    return binding;
}

static void bind_frag_data_location_common(GLuint program_name, GLuint color, GLuint index,
                                           const GLchar *name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    struct mgl_frag_output_binding *binding;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (index > 1u || (index == 0u && color >= MGL_MAX_DRAW_BUFFERS) ||
        (index != 0u && color >= MGL_MAX_DUAL_SOURCE_DRAW_BUFFERS)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    if (name == NULL || strncmp(name, "gl_", 3u) == 0) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    binding = get_or_create_frag_output_binding(ctx, program, name);
    if (binding != NULL) {
        binding->color_number = color;
        binding->index = index;
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glBindFragDataLocation(GLuint program, GLuint color, const GLchar *name) {
    bind_frag_data_location_common(program, color, 0u, name);
}

void glBindFragDataLocationIndexed(GLuint program, GLuint color, GLuint index, const GLchar *name) {
    bind_frag_data_location_common(program, color, index, name);
}

void glBindFragDataLocationEXT(GLuint program, GLuint color, const GLchar *name) {
    glBindFragDataLocation(program, color, name);
}

void glBindFragDataLocationIndexedEXT(GLuint program, GLuint color, GLuint index,
                                      const GLchar *name) {
    glBindFragDataLocationIndexed(program, color, index, name);
}

static int apply_frag_output_bindings(struct mgl_context *ctx, struct mgl_program *program) {
    size_t i;
    int extension_available =
        mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_EXT_blend_func_extended");
    for (i = 0u; i < program->frag_output_binding_count; ++i) {
        const struct mgl_frag_output_binding *binding = &program->frag_output_bindings[i];
        if (binding->index == 0u) {
            if (extension_available && ctx->native_dispatch->glesBindFragDataLocationEXT != NULL) {
                ctx->native_dispatch->glesBindFragDataLocationEXT(
                    program->native_name, binding->color_number, binding->name);
            } else if (extension_available &&
                       ctx->native_dispatch->glesBindFragDataLocationIndexedEXT != NULL) {
                ctx->native_dispatch->glesBindFragDataLocationIndexedEXT(
                    program->native_name, binding->color_number, 0u, binding->name);
            } else {
                /* GLES 3.0 has shader output layout qualifiers but no core
                 * glBindFragDataLocation entry point. The program-specific
                 * fallback below supplies the layout at link time. */
                continue;
            }
        } else {
            if (!extension_available ||
                ctx->native_dispatch->glesBindFragDataLocationIndexedEXT == NULL)
                return 0;
            ctx->native_dispatch->glesBindFragDataLocationIndexedEXT(
                program->native_name, binding->color_number, binding->index, binding->name);
        }
    }
    return 1;
}

/* The link-time layout fallback keeps application shader attachments intact. */
struct mgl_output_link_shader {
    GLuint original;
    GLuint temporary;
};

static void finish_output_link_shader(struct mgl_context *ctx, struct mgl_program *program,
                                      struct mgl_output_link_shader *replacement) {
    if (replacement->temporary == 0u)
        return;
    ctx->native_dispatch->glesDetachShader(program->native_name, replacement->temporary);
    ctx->native_dispatch->glesAttachShader(program->native_name, replacement->original);
    ctx->native_dispatch->glesDeleteShader(replacement->temporary);
    replacement->temporary = 0u;
}

static int prepare_output_link_shader(struct mgl_context *ctx, struct mgl_program *program,
                                      struct mgl_output_link_shader *replacement,
                                      char **failure_log) {
    struct mgl_shader *fragment = NULL;
    char *patched = NULL;
    struct mgl_fragment_output_location *locations = NULL;
    GLuint temporary;
    GLint compiled = GL_FALSE;
    int native_binding, indexed = 0;
    size_t i;
    unsigned s;
    const GLchar *shader_source;
    memset(replacement, 0, sizeof(*replacement));
    if (failure_log != NULL)
        *failure_log = NULL;
    if (program->frag_output_binding_count == 0u)
        return 1;
    native_binding = mgl_native_dispatch_has_gles_extension(
                         ctx->native_dispatch, "GL_EXT_blend_func_extended") &&
                     (ctx->native_dispatch->glesBindFragDataLocationEXT != NULL ||
                      ctx->native_dispatch->glesBindFragDataLocationIndexedEXT != NULL);
    for (i = 0u; i < program->frag_output_binding_count; ++i)
        if (program->frag_output_bindings[i].index != 0u)
            indexed = 1;
    if (indexed && (!native_binding ||
                    ctx->native_dispatch->glesBindFragDataLocationIndexedEXT == NULL)) {
        if (failure_log != NULL)
            *failure_log = duplicate_text("mGL: native lacks indexed fragment-output bindings");
        return 0;
    }
    for (s = 0u; s < program->attached_count; ++s) {
        if (program->attached[s]->type == GL_FRAGMENT_SHADER) {
            if (fragment != NULL) {
                if (failure_log != NULL)
                    *failure_log = duplicate_text("mGL: multiple attached fragment shader objects");
                return 0;
            }
            fragment = program->attached[s];
        }
    }
    if (fragment == NULL && native_binding)
        return 1;
    if (fragment == NULL || !fragment->compiled || fragment->translated_source == NULL ||
        strncmp(fragment->translated_source, "#version 320 es", 15u) != 0 ||
        ctx->native_dispatch->glesCreateShader == NULL ||
        ctx->native_dispatch->glesShaderSource == NULL ||
        ctx->native_dispatch->glesCompileShader == NULL ||
        ctx->native_dispatch->glesGetShaderiv == NULL ||
        ctx->native_dispatch->glesDetachShader == NULL ||
        ctx->native_dispatch->glesAttachShader == NULL ||
        ctx->native_dispatch->glesDeleteShader == NULL) {
        if (failure_log != NULL)
            *failure_log = duplicate_text("mGL: link-time fragment shader or native functions unavailable");
        return 0;
    }
    locations = calloc(program->frag_output_binding_count, sizeof(*locations));
    if (locations == NULL)
        return 0;
    for (i = 0u; i < program->frag_output_binding_count; ++i) {
        locations[i].name = program->frag_output_bindings[i].name;
        locations[i].location = program->frag_output_bindings[i].color_number;
        locations[i].index = program->frag_output_bindings[i].index;
    }
    patched = mgl_fragment_output_locations_log(fragment->translated_source,
                                                program->frag_output_binding_count, locations,
                                                failure_log);
    free(locations);
    if (patched == NULL)
        return 0;
    if (strcmp(patched, fragment->translated_source) == 0) {
        free(patched);
        return 1;
    }
    temporary = ctx->native_dispatch->glesCreateShader(GL_FRAGMENT_SHADER);
    if (temporary == 0u) {
        free(patched);
        return 0;
    }
    shader_source = patched;
    ctx->native_dispatch->glesShaderSource(temporary, 1, &shader_source, NULL);
    ctx->native_dispatch->glesCompileShader(temporary);
    ctx->native_dispatch->glesGetShaderiv(temporary, GL_COMPILE_STATUS, &compiled);
    free(patched);
    if (!compiled) {
        if (failure_log != NULL)
            *failure_log = mgl_native_shader_log(ctx, temporary);
        ctx->native_dispatch->glesDeleteShader(temporary);
        return 0;
    }
    ctx->native_dispatch->glesDetachShader(program->native_name, fragment->native_name);
    ctx->native_dispatch->glesAttachShader(program->native_name, temporary);
    replacement->original = fragment->native_name;
    replacement->temporary = temporary;
    return 1;
}

static void commit_frag_output_bindings(struct mgl_program *program) {
    size_t i;
    for (i = 0u; i < program->frag_output_binding_count; ++i) {
        struct mgl_frag_output_binding *binding = &program->frag_output_bindings[i];
        binding->linked_color_number = binding->color_number;
        binding->linked_index = binding->index;
        binding->linked_explicit = GL_TRUE;
    }
}

static void clear_linked_frag_output_bindings(struct mgl_program *program) {
    size_t i;
    for (i = 0u; i < program->frag_output_binding_count; ++i)
        program->frag_output_bindings[i].linked_explicit = GL_FALSE;
}

static int is_buffer_sampler_type(GLenum type) {
    return type == GL_SAMPLER_BUFFER || type == GL_INT_SAMPLER_BUFFER ||
           type == GL_UNSIGNED_INT_SAMPLER_BUFFER;
}

static void clear_program_buffer_sampler_uniforms(struct mgl_program *program) {
    if (program == NULL)
        return;
    free(program->buffer_sampler_uniforms);
    program->buffer_sampler_uniforms = NULL;
    program->buffer_sampler_uniform_count = 0u;
    program->buffer_sampler_reflection_failed = GL_FALSE;
}

static int append_buffer_sampler_uniform(struct mgl_program *program, GLint location, GLenum type) {
    struct mgl_buffer_sampler_uniform *next;
    size_t count;
    if (program == NULL || location < 0)
        return 1;
    count = program->buffer_sampler_uniform_count;
    if (count == SIZE_MAX / sizeof(*next))
        return 0;
    next = realloc(program->buffer_sampler_uniforms, (count + 1u) * sizeof(*next));
    if (next == NULL)
        return 0;
    program->buffer_sampler_uniforms = next;
    next[count].location = location;
    next[count].type = type;
    program->buffer_sampler_uniform_count = count + 1u;
    return 1;
}

static char *buffer_sampler_array_element_name(const char *name, GLint element) {
    const char *zero;
    char index_text[32];
    int index_len;
    size_t prefix_len;
    size_t suffix_len;
    size_t total;
    char *out;
    if (name == NULL || element < 0)
        return NULL;
    index_len = snprintf(index_text, sizeof(index_text), "[%d]", element);
    if (index_len <= 0 || (size_t)index_len >= sizeof(index_text))
        return NULL;
    zero = strstr(name, "[0]");
    if (zero != NULL) {
        prefix_len = (size_t)(zero - name);
        suffix_len = strlen(zero + 3);
        if (prefix_len > SIZE_MAX - (size_t)index_len - suffix_len - 1u)
            return NULL;
        total = prefix_len + (size_t)index_len + suffix_len + 1u;
        out = malloc(total);
        if (out == NULL)
            return NULL;
        memcpy(out, name, prefix_len);
        memcpy(out + prefix_len, index_text, (size_t)index_len);
        memcpy(out + prefix_len + (size_t)index_len, zero + 3, suffix_len + 1u);
        return out;
    }
    prefix_len = strlen(name);
    if (prefix_len > SIZE_MAX - (size_t)index_len - 1u)
        return NULL;
    total = prefix_len + (size_t)index_len + 1u;
    out = malloc(total);
    if (out == NULL)
        return NULL;
    memcpy(out, name, prefix_len);
    memcpy(out + prefix_len, index_text, (size_t)index_len + 1u);
    return out;
}

static int reflect_program_buffer_samplers(struct mgl_context *ctx, struct mgl_program *program) {
    GLint active_count = 0;
    GLint max_name = 0;
    GLchar *name = NULL;
    GLint i;
    clear_program_buffer_sampler_uniforms(program);
    if (ctx == NULL || program == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesGetProgramiv == NULL ||
        ctx->native_dispatch->glesGetActiveUniform == NULL ||
        ctx->native_dispatch->glesGetUniformLocation == NULL)
        return 1;
    ctx->native_dispatch->glesGetProgramiv(program->native_name, GL_ACTIVE_UNIFORMS, &active_count);
    ctx->native_dispatch->glesGetProgramiv(program->native_name, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                                           &max_name);
    if (active_count <= 0 || max_name <= 0)
        return 1;
    name = malloc((size_t)max_name);
    if (name == NULL)
        return 0;
    for (i = 0; i < active_count; ++i) {
        GLsizei length = 0;
        GLint size = 0;
        GLenum type = 0u;
        GLint element;
        name[0] = '\0';
        ctx->native_dispatch->glesGetActiveUniform(program->native_name, (GLuint)i,
                                                   (GLsizei)max_name, &length, &size, &type, name);
        if (length <= 0 || size <= 0 || !is_buffer_sampler_type(type))
            continue;
        for (element = 0; element < size; ++element) {
            GLint location;
            char *element_name = NULL;
            const char *query_name = name;
            if (element != 0) {
                element_name = buffer_sampler_array_element_name(name, element);
                if (element_name == NULL) {
                    free(name);
                    clear_program_buffer_sampler_uniforms(program);
                    return 0;
                }
                query_name = element_name;
            }
            location =
                ctx->native_dispatch->glesGetUniformLocation(program->native_name, query_name);
            free(element_name);
            if (!append_buffer_sampler_uniform(program, location, type)) {
                free(name);
                clear_program_buffer_sampler_uniforms(program);
                return 0;
            }
        }
    }
    free(name);
    return 1;
}

static int is_compat_attribute_name(const char *name) {
    unsigned i;
    if (name == NULL)
        return 0;
    for (i = 0; i < MGL_COMPAT_SEM_COUNT; ++i) {
        if (strcmp(name, mgl_compat_attribute_names[i]) == 0)
            return 1;
    }
    return 0;
}

static void reflect_program_attributes(struct mgl_context *ctx, struct mgl_program *program) {
    unsigned i;
    GLint active_count = 0;
    GLint max_name = 0;
    GLchar *name = NULL;

    program->active_generic_mask = 0u;
    for (i = 0; i < MGL_MAX_VERTEX_ATTRIBS; ++i)
        program->active_generic_type[i] = GL_FLOAT;
    for (i = 0; i < MGL_COMPAT_SEM_COUNT; ++i) {
        program->compat_attrib[i] = ctx->native_dispatch->glesGetAttribLocation(
            program->native_name, mgl_compat_attribute_names[i]);
    }
    program->compat_uniform_model_view_projection = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_model_view_projection");
    program->compat_uniform_model_view =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_model_view");
    program->compat_uniform_projection =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_projection");
    program->compat_uniform_normal_matrix =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_normal_matrix");
    for (i = 0u; i < 8u; ++i) {
        char uniform_name[48];
        (void)snprintf(uniform_name, sizeof(uniform_name), "mgl_u_texture_matrix[%u]", i);
        program->compat_uniform_texture_matrix[i] =
            ctx->native_dispatch->glesGetUniformLocation(program->native_name, uniform_name);
    }
    program->compat_uniform_sampler0 =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_texture0");
    program->compat_uniform_ff_texture_enabled = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_ff_texture_enabled");
    program->compat_uniform_alpha_enabled = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_alpha_test_enabled");
    program->compat_uniform_alpha_func =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_alpha_test_func");
    program->compat_uniform_alpha_ref =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_alpha_test_ref");
    program->compat_uniform_point_coord_flip = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_point_coord_flip");
    program->compat_uniform_point_size =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_point_size");
    program->compat_uniform_program_point_size = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_program_point_size");
    program->compat_uniform_texture_env_mode0 = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_texture_env_mode0");
    program->compat_env_enabled =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_env_enabled[0]");
    program->compat_env_mode =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_env_mode[0]");
    program->compat_env_color =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_env_color[0]");
    program->compat_env_rgb_mode =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_env_rgb_mode[0]");
    program->compat_env_alpha_mode = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_env_alpha_mode[0]");
    program->compat_env_rgb_source = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_env_rgb_source[0]");
    program->compat_env_alpha_source = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_env_alpha_source[0]");
    program->compat_env_rgb_operand = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_env_rgb_operand[0]");
    program->compat_env_alpha_operand = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_env_alpha_operand[0]");
    program->compat_env_rgb_scale = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_env_rgb_scale[0]");
    program->compat_env_alpha_scale = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_env_alpha_scale[0]");
    program->compat_env_lod_bias =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_env_lod_bias[0]");
    for (i = 0u; i < 8u; ++i) {
        char scale_name[40];
        (void)snprintf(scale_name, sizeof(scale_name), "mgl_u_rect_scale[%u]", i);
        program->compat_uniform_rect_scale[i] =
            ctx->native_dispatch->glesGetUniformLocation(program->native_name, scale_name);
    }
    for (i = 0u; i < 8u; ++i) {
        char sampler_name[40];
        (void)snprintf(sampler_name, sizeof(sampler_name), "mgl_u_texture%u", i);
        program->compat_env_sampler[i] =
            ctx->native_dispatch->glesGetUniformLocation(program->native_name, sampler_name);
    }
    program->compat_uniform_texgen_enabled = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_texgen_enabled[0]");
    program->compat_uniform_texgen_mode =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_texgen_mode[0]");
    program->compat_uniform_texgen_object_plane = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_texgen_object_plane[0]");
    program->compat_uniform_texgen_eye_plane = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_texgen_eye_plane[0]");
    program->compat_uniform_clip_enabled =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_clip_enabled[0]");
    program->compat_uniform_clip_plane =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_clip_plane[0]");
    program->compat_uniform_fog_enabled =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_fog_enabled");
    program->compat_uniform_fog_mode =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_fog_mode");
    program->compat_uniform_fog_density =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_fog_density");
    program->compat_uniform_fog_start =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_fog_start");
    program->compat_uniform_fog_end =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_fog_end");
    program->compat_uniform_fog_color =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name, "mgl_u_fog_color");
    program->compat_uniform_fog_coord_explicit = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_fog_coord_explicit");
    program->compat_uniform_normalize_enabled = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_normalize_enabled");
    program->compat_uniform_lighting_enabled = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_lighting_enabled");
    program->compat_uniform_implicit_texgen_enabled = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_implicit_texgen_enabled[0]");
    program->compat_uniform_implicit_texgen_mode = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_implicit_texgen_mode[0]");
    program->compat_uniform_implicit_texgen_object_plane =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name,
                                                     "mgl_u_implicit_texgen_object_plane[0]");
    program->compat_uniform_implicit_texgen_eye_plane =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name,
                                                     "mgl_u_implicit_texgen_eye_plane[0]");
    program->compat_uniform_light_enabled = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_enabled[0]");
    program->compat_uniform_light_ambient = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_ambient[0]");
    program->compat_uniform_light_diffuse = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_diffuse[0]");
    program->compat_uniform_light_specular = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_specular[0]");
    program->compat_uniform_light_position = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_position[0]");
    program->compat_uniform_light_spot_direction = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_spot_direction[0]");
    program->compat_uniform_light_spot_exponent = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_spot_exponent[0]");
    program->compat_uniform_light_spot_cutoff = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_spot_cutoff[0]");
    program->compat_uniform_light_constant_attenuation =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name,
                                                     "mgl_u_light_constant_attenuation[0]");
    program->compat_uniform_light_linear_attenuation = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_linear_attenuation[0]");
    program->compat_uniform_light_quadratic_attenuation =
        ctx->native_dispatch->glesGetUniformLocation(program->native_name,
                                                     "mgl_u_light_quadratic_attenuation[0]");
    program->compat_uniform_light_model_ambient = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_model_ambient");
    program->compat_uniform_light_model_local_viewer = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_model_local_viewer");
    program->compat_uniform_light_model_two_side = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_light_model_two_side");
    program->compat_uniform_material_ambient = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_ambient");
    program->compat_uniform_material_diffuse = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_diffuse");
    program->compat_uniform_material_specular = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_specular");
    program->compat_uniform_material_emission = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_emission");
    program->compat_uniform_material_shininess = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_shininess");
    program->compat_uniform_material_back_ambient = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_back_ambient");
    program->compat_uniform_material_back_diffuse = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_back_diffuse");
    program->compat_uniform_material_back_specular = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_back_specular");
    program->compat_uniform_material_back_emission = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_back_emission");
    program->compat_uniform_material_back_shininess = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_material_back_shininess");
    program->compat_uniform_color_material_enabled = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_color_material_enabled");
    program->compat_uniform_color_material_mode = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_color_material_mode");
    program->compat_uniform_color_material_face = ctx->native_dispatch->glesGetUniformLocation(
        program->native_name, "mgl_u_color_material_face");

    program->buffer_sampler_reflection_failed =
        reflect_program_buffer_samplers(ctx, program) ? GL_FALSE : GL_TRUE;
    ctx->native_dispatch->glesGetProgramiv(program->native_name, GL_ACTIVE_ATTRIBUTES,
                                           &active_count);
    ctx->native_dispatch->glesGetProgramiv(program->native_name, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                                           &max_name);
    if (active_count <= 0 || max_name <= 0 || ctx->native_dispatch->glesGetActiveAttrib == NULL)
        return;
    name = malloc((size_t)max_name);
    if (name == NULL)
        return;
    for (i = 0; i < (unsigned)active_count; ++i) {
        GLsizei length = 0;
        GLint size = 0;
        GLenum type = 0u;
        GLint location;
        name[0] = '\0';
        ctx->native_dispatch->glesGetActiveAttrib(program->native_name, i, (GLsizei)max_name,
                                                  &length, &size, &type, name);
        (void)size;
        if (length <= 0 || is_compat_attribute_name(name))
            continue;
        location = ctx->native_dispatch->glesGetAttribLocation(program->native_name, name);
        if (location >= 0 && location < (GLint)MGL_MAX_VERTEX_ATTRIBS) {
            program->active_generic_mask |= UINT32_C(1) << (unsigned)location;
            program->active_generic_type[(unsigned)location] = type;
        }
    }
    free(name);
}

void glProgramParameteri(GLuint program_name, GLenum pname, GLint value) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glProgramParameteri",
        (const struct mgl_error_arg[]){{"program_name", (uint64_t)(uintptr_t)program_name},
                                       {"pname", (uint64_t)(uintptr_t)pname},
                                       {"value", (uint64_t)(uintptr_t)value}},
        3u);
    struct mgl_program *program;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (pname != GL_PROGRAM_BINARY_RETRIEVABLE_HINT && pname != GL_PROGRAM_SEPARABLE) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (value != GL_TRUE && value != GL_FALSE) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    if (ctx->native_dispatch->glesProgramParameteri == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesProgramParameteri(program->native_name, pname, value);
    if (pname == GL_PROGRAM_BINARY_RETRIEVABLE_HINT)
        program->binary_retrievable_hint = value ? GL_TRUE : GL_FALSE;
    else
        program->separable = value ? GL_TRUE : GL_FALSE;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetProgramBinary(GLuint program_name, GLsizei buf_size, GLsizei *length,
                        GLenum *binary_format, void *binary) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (buf_size < 0 || (buf_size > 0 && binary == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    if (!program->linked || ctx->native_dispatch->glesGetProgramBinary == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesGetProgramBinary(program->native_name, buf_size, length,
                                               binary_format, binary);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glProgramBinary(GLuint program_name, GLenum binary_format, const void *binary,
                     GLsizei length) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    GLint status = GL_FALSE;
    char *log;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (length < 0 || (length > 0 && binary == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    if (ctx->native_dispatch->glesProgramBinary == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)atomic_fetch_add_explicit(&ctx->share_group->program_link_revision, 1u,
                                    memory_order_release);
    ctx->native_dispatch->glesProgramBinary(program->native_name, binary_format, binary, length);
    ctx->native_dispatch->glesGetProgramiv(program->native_name, GL_LINK_STATUS, &status);
    log = native_program_log(ctx, program->native_name);
    free(program->info_log);
    program->info_log = log != NULL ? log : duplicate_text("");
    program->linked = status ? GL_TRUE : GL_FALSE;
    program->linked_separable = program->linked && program->separable ? GL_TRUE : GL_FALSE;
    program->validated = GL_FALSE;
    if (program->linked) {
        program->linked_compute = GL_FALSE;
        program->compat_uniform_cache_valid = GL_FALSE;
        ctx->draw_native_program_valid = GL_FALSE;
        if (ctx->current_program == program_name)
            ctx->current_program_executable = GL_TRUE;
        clear_linked_frag_output_bindings(program);
        reflect_program_attributes(ctx, program);
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

/* Desktop uniform initializers are link-time defaults. GLES has no syntax
 * for them; the translator carries scalar literal defaults in comments. */
static void restore_scalar_uniform_defaults(struct mgl_context *ctx, struct mgl_program *program) {
    GLint previous = 0;
    unsigned i;
    int bound = 0;
    if (ctx->native_dispatch->glesGetIntegerv == NULL ||
        ctx->native_dispatch->glesUseProgram == NULL ||
        ctx->native_dispatch->glesGetUniformLocation == NULL)
        return;
    for (i = 0; i < program->attached_count; ++i) {
        const struct mgl_shader *shader = program->attached[i];
        const char *p = shader && shader->translated_source ? shader->translated_source : "";
        while ((p = strstr(p, "/*MGL_SCALAR_UNIFORM_DEFAULT ")) != NULL) {
            char type[8], name[96], numeric[96], *end;
            GLint location;
            int n = sscanf(p, "/*MGL_SCALAR_UNIFORM_DEFAULT %7s %95s %95[^*]", type, name, numeric);
            p += strlen("/*MGL_SCALAR_UNIFORM_DEFAULT ");
            if (n != 3)
                continue;
            end = numeric + strlen(numeric);
            while (end > numeric && (end[-1] == ' ' || end[-1] == '\t'))
                *--end = '\0';
            location = ctx->native_dispatch->glesGetUniformLocation(program->native_name, name);
            if (location < 0)
                continue;
            if (!bound) {
                ctx->native_dispatch->glesGetIntegerv(GL_CURRENT_PROGRAM, &previous);
                mgl_draw_use_native_program(ctx, program->native_name);
                bound = 1;
            }
            if (strcmp(type, "float") == 0 && ctx->native_dispatch->glesUniform1f != NULL)
                ctx->native_dispatch->glesUniform1f(location, strtof(numeric, NULL));
            else if (ctx->native_dispatch->glesUniform1i != NULL && strcmp(type, "int") == 0)
                ctx->native_dispatch->glesUniform1i(location, (GLint)strtol(numeric, NULL, 10));
            else if (ctx->native_dispatch->glesUniform1i != NULL && strcmp(type, "bool") == 0)
                ctx->native_dispatch->glesUniform1i(location, strcmp(numeric, "true") == 0 ? 1 : 0);
        }
    }
    if (bound)
        mgl_draw_use_native_program(ctx, (GLuint)previous);
}

void glLinkProgram(GLuint program_name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    GLint status = GL_FALSE;
    char *log;
    struct mgl_output_link_shader output_shader = {0};
    GLuint implicit_stage = 0u;
    unsigned vertex_count = 0u, fragment_count = 0u, geometry_count = 0u, compute_count = 0u;
    unsigned tess_control_count = 0u, tess_evaluation_count = 0u;
    unsigned stage_i;
    uint64_t perf_started;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    perf_started = mgl_perf_diag_now();
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    (void)atomic_fetch_add_explicit(&ctx->share_group->program_link_revision, 1u,
                                    memory_order_release);
    if (program->attached_count > 1u) {
        unsigned i;
        GLboolean spirv = program->attached[0]->spirv_module != NULL;
        for (i = 1u; i < program->attached_count; ++i) {
            if ((program->attached[i]->spirv_module != NULL) != spirv) {
                free(program->info_log);
                program->info_log = duplicate_text("mGL: mixed SPIR-V and GLSL shader attachments");
                program->linked = GL_FALSE;
                program->validated = GL_FALSE;
                (void)pthread_mutex_unlock(&ctx->share_group->lock);
                return;
            }
        }
    }
    for (unsigned i = 0u; i < program->attached_count; ++i) {
        if (program->attached[i]->spirv_module != NULL && !program->attached[i]->compiled) {
            free(program->info_log);
            program->info_log = duplicate_text("mGL: SPIR-V shader not specialized successfully");
            program->linked = GL_FALSE;
            program->validated = GL_FALSE;
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            return;
        }
    }
    if (!apply_frag_output_bindings(ctx, program)) {
        free(program->info_log);
        program->info_log = duplicate_text("mGL: native lacks fragment-output binding support");
        program->linked = GL_FALSE;
        program->validated = GL_FALSE;
        mgl_render_diag_program(ctx, program_name, program->native_name, program->linked,
                                program->info_log);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    if (program->transform_feedback_varying_count > 0) {
        if (ctx->native_dispatch->glesTransformFeedbackVaryings == NULL) {
                free(program->info_log);
            program->info_log =
                duplicate_text("mGL: native lacks transform-feedback varying support");
            program->linked = GL_FALSE;
            program->validated = GL_FALSE;
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            return;
        }
        ctx->native_dispatch->glesTransformFeedbackVaryings(
            program->native_name, program->transform_feedback_varying_count,
            (const GLchar *const *)program->transform_feedback_varyings,
            program->transform_feedback_buffer_mode);
    }
    {
        char *output_error = NULL;
        if (!prepare_output_link_shader(ctx, program, &output_shader, &output_error)) {
                free(program->info_log);
            program->info_log =
                output_error != NULL && output_error[0] != '\0'
                    ? output_error
                    : duplicate_text("mGL: unable to realize desktop fragment-output locations on GLES");
            if (program->info_log != output_error)
                free(output_error);
            program->linked = GL_FALSE;
            program->validated = GL_FALSE;
            mgl_render_diag_program(ctx, program_name, program->native_name, program->linked,
                                    program->info_log);
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            return;
        }
    }
    for (stage_i = 0u; stage_i < program->attached_count; ++stage_i) {
        const struct mgl_shader *attached = program->attached[stage_i];
        if (attached->type == GL_VERTEX_SHADER)
            ++vertex_count;
        else if (attached->type == GL_FRAGMENT_SHADER)
            ++fragment_count;
        else if (attached->type == GL_GEOMETRY_SHADER)
            ++geometry_count;
        else if (attached->type == GL_TESS_CONTROL_SHADER)
            ++tess_control_count;
        else if (attached->type == GL_TESS_EVALUATION_SHADER)
            ++tess_evaluation_count;
        else if (attached->type == GL_COMPUTE_SHADER)
            ++compute_count;
    }
    if (program->attached_count == 1u && compute_count == 0u && geometry_count == 0u &&
        tess_control_count == 0u && tess_evaluation_count == 0u &&
        ((vertex_count == 1u && fragment_count == 0u) ||
         (fragment_count == 1u && vertex_count == 0u)) &&
        program->attached[0]->compiled && !program->separable &&
        ctx->native_dispatch->glesDetachShader != NULL) {
        GLenum type = vertex_count ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER;
        implicit_stage = mgl_compile_implicit_compat_stage(ctx, type, program, NULL);
        if (implicit_stage != 0u)
            ctx->native_dispatch->glesAttachShader(program->native_name, implicit_stage);
    }
    ctx->native_dispatch->glesLinkProgram(program->native_name);
    if (implicit_stage != 0u) {
        ctx->native_dispatch->glesDetachShader(program->native_name, implicit_stage);
        ctx->native_dispatch->glesDeleteShader(implicit_stage);
    }
    finish_output_link_shader(ctx, program, &output_shader);
    ctx->native_dispatch->glesGetProgramiv(program->native_name, GL_LINK_STATUS, &status);
    log = native_program_log(ctx, program->native_name);
    free(program->info_log);
    program->info_log = log != NULL ? log : duplicate_text("");
    program->linked = status ? GL_TRUE : GL_FALSE;
    program->linked_separable = program->linked && program->separable ? GL_TRUE : GL_FALSE;
    mgl_perf_diag_shader(ctx, 1, perf_started);
    mgl_render_diag_program(ctx, program_name, program->native_name, program->linked,
                            program->info_log);
    if (!program->linked) {
        unsigned diag_vertex_count = 0u;
        unsigned diag_fragment_count = 0u;
        unsigned diag_geometry_count = 0u;
        unsigned diag_compute_count = 0u;
        unsigned other_count = 0u;
        unsigned uncompiled_count = 0u;
        unsigned i;
        for (i = 0u; i < program->attached_count; ++i) {
            const struct mgl_shader *attached = program->attached[i];
            if (!attached->compiled)
                ++uncompiled_count;
            if (attached->type == GL_VERTEX_SHADER)
                ++diag_vertex_count;
            else if (attached->type == GL_FRAGMENT_SHADER)
                ++diag_fragment_count;
            else if (attached->type == GL_GEOMETRY_SHADER)
                ++diag_geometry_count;
            else if (attached->type == GL_COMPUTE_SHADER)
                ++diag_compute_count;
            else
                ++other_count;
        }
        (void)fprintf(stderr,
                      "mGL: error: ctx=%p event=program_link_stages program=%u attached=%u vertex=%u "
                      "fragment=%u geometry=%u compute=%u other=%u uncompiled=%u separable=%u\n",
                      (void *)ctx, program_name, program->attached_count, diag_vertex_count,
                      diag_fragment_count, diag_geometry_count, diag_compute_count, other_count,
                      uncompiled_count, (unsigned)program->separable);
    }
    program->validated = GL_FALSE;
    if (program->linked) {
        program->linked_compute = compute_count != 0u ? GL_TRUE : GL_FALSE;
        program->compat_uniform_cache_valid = GL_FALSE;
        ctx->draw_native_program_valid = GL_FALSE;
        if (ctx->current_program == program_name)
            ctx->current_program_executable = GL_TRUE;
        restore_scalar_uniform_defaults(ctx, program);
        commit_frag_output_bindings(program);
        reflect_program_attributes(ctx, program);
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glValidateProgram(GLuint program_name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    GLint status = GL_FALSE;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    ctx->native_dispatch->glesValidateProgram(program->native_name);
    ctx->native_dispatch->glesGetProgramiv(program->native_name, GL_VALIDATE_STATUS, &status);
    program->validated = status ? GL_TRUE : GL_FALSE;
    free(program->info_log);
    program->info_log = native_program_log(ctx, program->native_name);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glUseProgram(GLuint program_name) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_PROGRAM_STATE);
    struct mgl_program *next = NULL;
    struct mgl_program *old = NULL;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (program_name == ctx->current_program &&
        (program_name == 0u || (ctx->use_program_fast_name == program_name &&
                                ctx->use_program_fast_revision ==
                                    atomic_load_explicit(&ctx->share_group->program_link_revision,
                                                         memory_order_acquire)))) {
        ++ctx->use_program_fast_hits;
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    if (program_name != 0u) {
        next = mgl_program_find(ctx, program_name);
        if (next == NULL) {
            GLenum error = program_name_error_locked(ctx, program_name);
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, error);
            return;
        }
        if (!next->linked) {
            (void)fprintf(stderr,
                          "mGL: error: ctx=%p event=program_use_rejected program=%u linked=0\n",
                          (void *)ctx, program_name);
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
    }
    old = mgl_program_find(ctx, ctx->current_program);
    if (old == next) {
        ctx->use_program_fast_name = program_name;
        ctx->use_program_fast_revision =
            atomic_load_explicit(&ctx->share_group->program_link_revision, memory_order_relaxed);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    mgl_draw_use_native_program(ctx, next != NULL ? next->native_name : 0u);
    if (old != NULL && old->use_count > 0u)
        --old->use_count;
    if (next != NULL)
        ++next->use_count;
    ctx->current_program = program_name;
    ctx->current_program_executable = next != NULL ? GL_TRUE : GL_FALSE;
    ctx->use_program_fast_name = program_name;
    ctx->use_program_fast_revision =
        atomic_load_explicit(&ctx->share_group->program_link_revision, memory_order_relaxed);
    if (old != NULL)
        mgl_program_maybe_destroy(ctx, old);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetProgramiv(GLuint program_name, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(
        ctx != NULL ? &ctx->errors : NULL, "glGetProgramiv",
        (const struct mgl_error_arg[]){{"program_name", (uint64_t)(uintptr_t)program_name},
                                       {"pname", (uint64_t)(uintptr_t)pname},
                                       {"params", (uint64_t)(uintptr_t)params}},
        3u);
    struct mgl_program *program;
    if (ctx == NULL || params == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    switch (pname) {
        case GL_DELETE_STATUS:
            *params = program->delete_pending ? GL_TRUE : GL_FALSE;
            break;
        case GL_LINK_STATUS:
            *params = program->linked ? GL_TRUE : GL_FALSE;
            break;
        case GL_VALIDATE_STATUS:
            *params = program->validated ? GL_TRUE : GL_FALSE;
            break;
        case GL_INFO_LOG_LENGTH:
            *params = (GLint)(program->info_log != NULL ? strlen(program->info_log) + 1u : 0u);
            break;
        case GL_ATTACHED_SHADERS:
            *params = (GLint)program->attached_count;
            break;
        case GL_TRANSFORM_FEEDBACK_VARYINGS:
            *params = (GLint)program->transform_feedback_varying_count;
            break;
        case GL_TRANSFORM_FEEDBACK_BUFFER_MODE:
            *params = (GLint)program->transform_feedback_buffer_mode;
            break;
        case GL_PROGRAM_BINARY_RETRIEVABLE_HINT:
            *params = program->binary_retrievable_hint ? GL_TRUE : GL_FALSE;
            break;
        case GL_PROGRAM_SEPARABLE:
            *params = program->separable ? GL_TRUE : GL_FALSE;
            break;
        case GL_PROGRAM_BINARY_LENGTH:
            if (ctx->native_dispatch->glesGetProgramBinary == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            } else {
                ctx->native_dispatch->glesGetProgramiv(program->native_name, pname, params);
            }
            break;
        case GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH: {
            size_t max_len = 0u;
            GLsizei i;
            for (i = 0; i < program->transform_feedback_varying_count; ++i) {
                size_t n = strlen(program->transform_feedback_varyings[i]) + 1u;
                if (n > max_len)
                    max_len = n;
            }
            *params = (GLint)max_len;
            break;
        }
        case GL_ACTIVE_UNIFORMS:
        case GL_ACTIVE_UNIFORM_MAX_LENGTH:
        case GL_ACTIVE_ATTRIBUTES:
        case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
        case GL_ACTIVE_UNIFORM_BLOCKS:
        case GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH:
        case GL_ACTIVE_ATOMIC_COUNTER_BUFFERS:
        case GL_COMPUTE_WORK_GROUP_SIZE:
        case GL_GEOMETRY_VERTICES_OUT:
        case GL_GEOMETRY_INPUT_TYPE:
        case GL_GEOMETRY_OUTPUT_TYPE:
        case GL_GEOMETRY_SHADER_INVOCATIONS:
        case GL_TESS_CONTROL_OUTPUT_VERTICES:
        case GL_TESS_GEN_MODE:
        case GL_TESS_GEN_SPACING:
        case GL_TESS_GEN_VERTEX_ORDER:
        case GL_TESS_GEN_POINT_MODE:
            ctx->native_dispatch->glesGetProgramiv(program->native_name, pname, params);
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetProgramStageiv(GLuint program_name, GLenum shadertype, GLenum pname, GLint *values) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    if (ctx == NULL || shader_reject_inside_begin(ctx))
        return;
    if (shadertype != GL_VERTEX_SHADER && shadertype != GL_FRAGMENT_SHADER &&
        shadertype != GL_GEOMETRY_SHADER && shadertype != GL_TESS_CONTROL_SHADER &&
        shadertype != GL_TESS_EVALUATION_SHADER && shadertype != GL_COMPUTE_SHADER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (pname != GL_ACTIVE_SUBROUTINES && pname != GL_ACTIVE_SUBROUTINE_UNIFORMS &&
        pname != GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS &&
        pname != GL_ACTIVE_SUBROUTINE_MAX_LENGTH &&
        pname != GL_ACTIVE_SUBROUTINE_UNIFORM_MAX_LENGTH) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    if (values != NULL)
        *values = 0;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetProgramInfoLog(GLuint program_name, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    copy_log_string(program->info_log, bufSize, length, infoLog);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetAttachedShaders(GLuint program_name, GLsizei maxCount, GLsizei *count, GLuint *shaders) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    unsigned n, i;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (maxCount < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    n = program->attached_count < (unsigned)maxCount ? program->attached_count : (unsigned)maxCount;
    if (shaders != NULL)
        for (i = 0; i < n; ++i)
            shaders[i] = program->attached[i]->name;
    if (count != NULL)
        *count = (GLsizei)n;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glDeleteProgram(GLuint program_name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (program_name == 0u)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return;
    }
    program->delete_pending = GL_TRUE;
    mgl_program_maybe_destroy(ctx, program);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

GLboolean glIsProgram(GLuint program_name) {
    struct mgl_context *ctx = mgl_context_current();
    GLboolean out;
    if (ctx == NULL)
        return GL_FALSE;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    if (program_name == 0u)
        return GL_FALSE;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    out = mgl_program_find(ctx, program_name) != NULL ? GL_TRUE : GL_FALSE;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return out;
}

GLint glGetAttribLocation(GLuint program_name, const GLchar *name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    GLint result;
    if (ctx == NULL)
        return -1;
    if (shader_reject_inside_begin(ctx))
        return -1;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return -1;
    }
    if (!program->linked || name == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return -1;
    }
    result = ctx->native_dispatch->glesGetAttribLocation(program->native_name, name);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return result;
}

GLint glGetUniformLocation(GLuint program_name, const GLchar *name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    GLint result;
    if (ctx == NULL)
        return -1;
    if (shader_reject_inside_begin(ctx))
        return -1;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return -1;
    }
    if (!program->linked || name == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return -1;
    }
    result = ctx->native_dispatch->glesGetUniformLocation(program->native_name, name);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return result;
}

GLint glGetFragDataLocation(GLuint program_name, const GLchar *name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    GLint result = -1;
    if (ctx == NULL)
        return -1;
    if (shader_reject_inside_begin(ctx))
        return -1;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return -1;
    }
    if (!program->linked || name == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return -1;
    }
    if (ctx->native_dispatch->glesGetFragDataLocation != NULL) {
        result = ctx->native_dispatch->glesGetFragDataLocation(program->native_name, name);
    } else if (ctx->native_dispatch->glesGetProgramResourceLocation != NULL) {
        result = ctx->native_dispatch->glesGetProgramResourceLocation(program->native_name,
                                                                      GL_PROGRAM_OUTPUT, name);
    } else {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return result;
}

GLint glGetFragDataLocationEXT(GLuint program_name, const GLchar *name) {
    return glGetFragDataLocation(program_name, name);
}

GLint mgl_frag_data_index_locked(struct mgl_context *ctx, struct mgl_program *program,
                                const GLchar *name) {
    struct mgl_frag_output_binding *binding;
    GLint location;
    if (!program->linked || name == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return -1;
    }
    if (ctx->native_dispatch != NULL &&
        mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                               "GL_EXT_blend_func_extended") &&
        ctx->native_dispatch->glesGetFragDataIndexEXT != NULL)
        return ctx->native_dispatch->glesGetFragDataIndexEXT(program->native_name, name);
    if (ctx->native_dispatch != NULL &&
        mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                               "GL_EXT_blend_func_extended") &&
        ctx->native_dispatch->glesGetProgramResourceLocationIndex != NULL)
        return ctx->native_dispatch->glesGetProgramResourceLocationIndex(program->native_name,
                                                                         GL_PROGRAM_OUTPUT, name);
    if (ctx->native_dispatch->glesGetFragDataLocation != NULL) {
        location = ctx->native_dispatch->glesGetFragDataLocation(program->native_name, name);
    } else if (ctx->native_dispatch->glesGetProgramResourceLocation != NULL) {
        location = ctx->native_dispatch->glesGetProgramResourceLocation(program->native_name,
                                                                        GL_PROGRAM_OUTPUT, name);
    } else {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return -1;
    }
    if (location < 0)
        return -1;
    binding = find_frag_output_binding(program, name);
    return binding != NULL && binding->linked_explicit ? (GLint)binding->linked_index : 0;
}

GLint glGetFragDataIndex(GLuint program_name, const GLchar *name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    GLint result = -1;
    if (ctx == NULL)
        return -1;
    if (shader_reject_inside_begin(ctx))
        return -1;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        GLenum error = program_name_error_locked(ctx, program_name);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, error);
        return -1;
    }
    result = mgl_frag_data_index_locked(ctx, program, name);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return result;
}

GLint glGetFragDataIndexEXT(GLuint program, const GLchar *name) {
    return glGetFragDataIndex(program, name);
}

static int uniform_ready(struct mgl_context *ctx, GLint location) {
    struct mgl_program *program;
    if (shader_reject_inside_begin(ctx))
        return -1;
    if (location < -1) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return -1;
    }
    if (location == -1)
        return 0;
    program = mgl_program_find(ctx, ctx->current_program);
    if (program == NULL && ctx->current_program == 0u && ctx->current_pipeline != 0u) {
        struct mgl_pipeline *it;
        for (it = ctx->pipelines; it != NULL; it = it->next)
            if (it->name == ctx->current_pipeline) {
                program = it->active;
                break;
            }
    }
    if (program == NULL || (!program->linked &&
                            (ctx->current_program == 0u ||
                             !ctx->current_program_executable))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return -1;
    }

    program->compat_uniform_cache_valid = GL_FALSE;
    return 1;
}

static int uniform_count_ready(struct mgl_context *ctx, GLint location, GLsizei count) {
    if (shader_reject_inside_begin(ctx))
        return -1;
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return -1;
    }
    return uniform_ready(ctx, location);
}

static int uniform_matrix_ready(struct mgl_context *ctx, GLint location, GLsizei count,
                                GLboolean transpose) {
    if (shader_reject_inside_begin(ctx))
        return -1;
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return -1;
    }
    (void)transpose; /* Desktop GL accepts both row-major and column-major input. */
    return uniform_ready(ctx, location);
}

static void uniform_native_missing(struct mgl_context *ctx) {
    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
}

#define MGL_UNIFORM_F1(name, member)                                                               \
    void name(GLint location, GLfloat v0) {                                                        \
        struct mgl_context *ctx = mgl_context_current();                                           \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_ready(ctx, location);                                                      \
        if (ready > 0)                                                                             \
            ctx->native_dispatch->member(location, v0);                                            \
    }

#define MGL_UNIFORM_F2(name, member)                                                               \
    void name(GLint location, GLfloat v0, GLfloat v1) {                                            \
        struct mgl_context *ctx = mgl_context_current();                                           \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_ready(ctx, location);                                                      \
        if (ready > 0)                                                                             \
            ctx->native_dispatch->member(location, v0, v1);                                        \
    }

#define MGL_UNIFORM_F3(name, member)                                                               \
    void name(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {                                \
        struct mgl_context *ctx = mgl_context_current();                                           \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_ready(ctx, location);                                                      \
        if (ready > 0)                                                                             \
            ctx->native_dispatch->member(location, v0, v1, v2);                                    \
    }

#define MGL_UNIFORM_F4(name, member)                                                               \
    void name(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {                    \
        struct mgl_context *ctx = mgl_context_current();                                           \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_ready(ctx, location);                                                      \
        if (ready > 0)                                                                             \
            ctx->native_dispatch->member(location, v0, v1, v2, v3);                                \
    }

#define MGL_UNIFORM_I1(name, member)                                                               \
    void name(GLint location, GLint v0) {                                                          \
        struct mgl_context *ctx = mgl_context_current();                                           \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_ready(ctx, location);                                                      \
        if (ready > 0)                                                                             \
            ctx->native_dispatch->member(location, v0);                                            \
    }

#define MGL_UNIFORM_I2(name, member)                                                               \
    void name(GLint location, GLint v0, GLint v1) {                                                \
        struct mgl_context *ctx = mgl_context_current();                                           \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_ready(ctx, location);                                                      \
        if (ready > 0)                                                                             \
            ctx->native_dispatch->member(location, v0, v1);                                        \
    }

#define MGL_UNIFORM_I3(name, member)                                                               \
    void name(GLint location, GLint v0, GLint v1, GLint v2) {                                      \
        struct mgl_context *ctx = mgl_context_current();                                           \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_ready(ctx, location);                                                      \
        if (ready > 0)                                                                             \
            ctx->native_dispatch->member(location, v0, v1, v2);                                    \
    }

#define MGL_UNIFORM_I4(name, member)                                                               \
    void name(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {                            \
        struct mgl_context *ctx = mgl_context_current();                                           \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_ready(ctx, location);                                                      \
        if (ready > 0)                                                                             \
            ctx->native_dispatch->member(location, v0, v1, v2, v3);                                \
    }

#define MGL_UNIFORM_UI1(name, member)                                                              \
    void name(GLint location, GLuint v0) {                                                         \
        struct mgl_context *ctx = mgl_context_current();                                           \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_ready(ctx, location);                                                      \
        if (ready > 0) {                                                                           \
            if (ctx->native_dispatch->member != NULL)                                              \
                ctx->native_dispatch->member(location, v0);                                        \
            else                                                                                   \
                uniform_native_missing(ctx);                                                       \
        }                                                                                          \
    }

#define MGL_UNIFORM_UI2(name, member)                                                              \
    void name(GLint location, GLuint v0, GLuint v1) {                                              \
        struct mgl_context *ctx = mgl_context_current();                                           \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_ready(ctx, location);                                                      \
        if (ready > 0) {                                                                           \
            if (ctx->native_dispatch->member != NULL)                                              \
                ctx->native_dispatch->member(location, v0, v1);                                    \
            else                                                                                   \
                uniform_native_missing(ctx);                                                       \
        }                                                                                          \
    }

#define MGL_UNIFORM_UI3(name, member)                                                              \
    void name(GLint location, GLuint v0, GLuint v1, GLuint v2) {                                   \
        struct mgl_context *ctx = mgl_context_current();                                           \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_ready(ctx, location);                                                      \
        if (ready > 0) {                                                                           \
            if (ctx->native_dispatch->member != NULL)                                              \
                ctx->native_dispatch->member(location, v0, v1, v2);                                \
            else                                                                                   \
                uniform_native_missing(ctx);                                                       \
        }                                                                                          \
    }

#define MGL_UNIFORM_UI4(name, member)                                                              \
    void name(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {                        \
        struct mgl_context *ctx = mgl_context_current();                                           \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_ready(ctx, location);                                                      \
        if (ready > 0) {                                                                           \
            if (ctx->native_dispatch->member != NULL)                                              \
                ctx->native_dispatch->member(location, v0, v1, v2, v3);                            \
            else                                                                                   \
                uniform_native_missing(ctx);                                                       \
        }                                                                                          \
    }

#define MGL_UNIFORM_ARRAY(name, member, type)                                                      \
    void name(GLint location, GLsizei count, const type *value) {                                  \
        struct mgl_context *ctx = mgl_context_current();                                           \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_count_ready(ctx, location, count);                                         \
        if (ready > 0)                                                                             \
            ctx->native_dispatch->member(location, count, value);                                  \
    }

#define MGL_UNIFORM_ARRAY_OPTIONAL(name, member, type)                                             \
    void name(GLint location, GLsizei count, const type *value) {                                  \
        struct mgl_context *ctx = mgl_context_current();                                           \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_count_ready(ctx, location, count);                                         \
        if (ready > 0) {                                                                           \
            if (ctx->native_dispatch->member != NULL)                                              \
                ctx->native_dispatch->member(location, count, value);                              \
            else                                                                                   \
                uniform_native_missing(ctx);                                                       \
        }                                                                                          \
    }

/* GLES requires column-major input. Desktop GL accepts transpose=GL_TRUE.
 * The temporary is per-call so the native never sees the caller's row-major
 * layout, and the input is neither modified nor retained. */
static const GLfloat *uniform_matrix_column_major(struct mgl_context *ctx, GLsizei count,
                                                  GLboolean transpose, const GLfloat *value,
                                                  size_t columns, size_t rows, GLfloat small[16],
                                                  GLfloat **heap) {
    size_t size = columns * rows, total, matrix, column, row;
    GLfloat *result;
    *heap = NULL;
    if (transpose == GL_FALSE || count == 0 || value == NULL)
        return value;
    if ((size_t)count > SIZE_MAX / size / sizeof(GLfloat)) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return NULL;
    }
    total = (size_t)count * size;
    result = total <= 16u ? small : malloc(total * sizeof(*result));
    if (result == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return NULL;
    }
    if (result != small)
        *heap = result;
    for (matrix = 0u; matrix < (size_t)count; ++matrix) {
        const GLfloat *source = value + matrix * size;
        GLfloat *dest = result + matrix * size;
        for (column = 0u; column < columns; ++column)
            for (row = 0u; row < rows; ++row)
                dest[column * rows + row] = source[row * columns + column];
    }
    return result;
}

#define MGL_UNIFORM_MATRIX(name, member, columns, rows)                                            \
    void name(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {          \
        struct mgl_context *ctx = mgl_context_current();                                           \
        GLfloat small[16], *heap = NULL;                                                           \
        const GLfloat *data;                                                                       \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_matrix_ready(ctx, location, count, transpose);                             \
        if (ready > 0) {                                                                           \
            data = uniform_matrix_column_major(ctx, count, transpose, value, columns, rows, small, \
                                               &heap);                                             \
            if (data != NULL || count == 0)                                                        \
                ctx->native_dispatch->member(location, count, GL_FALSE, data);                     \
            free(heap);                                                                            \
        }                                                                                          \
    }

#define MGL_UNIFORM_MATRIX_OPTIONAL(name, member, columns, rows)                                   \
    void name(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {          \
        struct mgl_context *ctx = mgl_context_current();                                           \
        GLfloat small[16], *heap = NULL;                                                           \
        const GLfloat *data;                                                                       \
        int ready;                                                                                 \
        if (ctx == NULL)                                                                           \
            return;                                                                                \
        ready = uniform_matrix_ready(ctx, location, count, transpose);                             \
        if (ready > 0) {                                                                           \
            if (ctx->native_dispatch->member != NULL) {                                            \
                data = uniform_matrix_column_major(ctx, count, transpose, value, columns, rows,    \
                                                   small, &heap);                                  \
                if (data != NULL || count == 0)                                                    \
                    ctx->native_dispatch->member(location, count, GL_FALSE, data);                 \
                free(heap);                                                                        \
            } else                                                                                 \
                uniform_native_missing(ctx);                                                       \
        }                                                                                          \
    }

MGL_UNIFORM_F1(glUniform1f, glesUniform1f)
MGL_UNIFORM_F2(glUniform2f, glesUniform2f)
MGL_UNIFORM_F3(glUniform3f, glesUniform3f)
MGL_UNIFORM_F4(glUniform4f, glesUniform4f)
MGL_UNIFORM_I1(glUniform1i, glesUniform1i)
MGL_UNIFORM_I2(glUniform2i, glesUniform2i)
MGL_UNIFORM_I3(glUniform3i, glesUniform3i)
MGL_UNIFORM_I4(glUniform4i, glesUniform4i)
MGL_UNIFORM_UI1(glUniform1ui, glesUniform1ui)
MGL_UNIFORM_UI2(glUniform2ui, glesUniform2ui)
MGL_UNIFORM_UI3(glUniform3ui, glesUniform3ui)
MGL_UNIFORM_UI4(glUniform4ui, glesUniform4ui)
MGL_UNIFORM_ARRAY(glUniform1fv, glesUniform1fv, GLfloat)
MGL_UNIFORM_ARRAY(glUniform2fv, glesUniform2fv, GLfloat)
MGL_UNIFORM_ARRAY(glUniform3fv, glesUniform3fv, GLfloat)
MGL_UNIFORM_ARRAY(glUniform4fv, glesUniform4fv, GLfloat)
MGL_UNIFORM_ARRAY(glUniform1iv, glesUniform1iv, GLint)
MGL_UNIFORM_ARRAY(glUniform2iv, glesUniform2iv, GLint)
MGL_UNIFORM_ARRAY(glUniform3iv, glesUniform3iv, GLint)
MGL_UNIFORM_ARRAY(glUniform4iv, glesUniform4iv, GLint)
MGL_UNIFORM_ARRAY_OPTIONAL(glUniform1uiv, glesUniform1uiv, GLuint)
MGL_UNIFORM_ARRAY_OPTIONAL(glUniform2uiv, glesUniform2uiv, GLuint)
MGL_UNIFORM_ARRAY_OPTIONAL(glUniform3uiv, glesUniform3uiv, GLuint)
MGL_UNIFORM_ARRAY_OPTIONAL(glUniform4uiv, glesUniform4uiv, GLuint)
MGL_UNIFORM_MATRIX(glUniformMatrix2fv, glesUniformMatrix2fv, 2u, 2u)
MGL_UNIFORM_MATRIX(glUniformMatrix3fv, glesUniformMatrix3fv, 3u, 3u)
MGL_UNIFORM_MATRIX(glUniformMatrix4fv, glesUniformMatrix4fv, 4u, 4u)
MGL_UNIFORM_MATRIX_OPTIONAL(glUniformMatrix2x3fv, glesUniformMatrix2x3fv, 2u, 3u)
MGL_UNIFORM_MATRIX_OPTIONAL(glUniformMatrix3x2fv, glesUniformMatrix3x2fv, 3u, 2u)
MGL_UNIFORM_MATRIX_OPTIONAL(glUniformMatrix2x4fv, glesUniformMatrix2x4fv, 2u, 4u)
MGL_UNIFORM_MATRIX_OPTIONAL(glUniformMatrix4x2fv, glesUniformMatrix4x2fv, 4u, 2u)
MGL_UNIFORM_MATRIX_OPTIONAL(glUniformMatrix3x4fv, glesUniformMatrix3x4fv, 3u, 4u)
MGL_UNIFORM_MATRIX_OPTIONAL(glUniformMatrix4x3fv, glesUniformMatrix4x3fv, 4u, 3u)

#undef MGL_UNIFORM_F1
#undef MGL_UNIFORM_F2
#undef MGL_UNIFORM_F3
#undef MGL_UNIFORM_F4
#undef MGL_UNIFORM_I1
#undef MGL_UNIFORM_I2
#undef MGL_UNIFORM_I3
#undef MGL_UNIFORM_I4
#undef MGL_UNIFORM_UI1
#undef MGL_UNIFORM_UI2
#undef MGL_UNIFORM_UI3
#undef MGL_UNIFORM_UI4
#undef MGL_UNIFORM_ARRAY
#undef MGL_UNIFORM_ARRAY_OPTIONAL
#undef MGL_UNIFORM_MATRIX
#undef MGL_UNIFORM_MATRIX_OPTIONAL

static struct mgl_program *uniform_query_program(struct mgl_context *ctx, GLuint program_name) {
    struct mgl_program *program = mgl_program_find(ctx, program_name);
    if (program == NULL) {
        mgl_error_set(&ctx->errors, program_name_error_locked(ctx, program_name));
        return NULL;
    }
    if (!program->linked) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    return program;
}

void glGetUniformfv(GLuint program_name, GLint location, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    if (ctx == NULL || params == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = uniform_query_program(ctx, program_name);
    if (program != NULL)
        ctx->native_dispatch->glesGetUniformfv(program->native_name, location, params);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetUniformiv(GLuint program_name, GLint location, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    if (ctx == NULL || params == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = uniform_query_program(ctx, program_name);
    if (program != NULL)
        ctx->native_dispatch->glesGetUniformiv(program->native_name, location, params);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetUniformuiv(GLuint program_name, GLint location, GLuint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    if (ctx == NULL || params == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = uniform_query_program(ctx, program_name);
    if (program != NULL) {
        if (ctx->native_dispatch->glesGetUniformuiv != NULL)
            ctx->native_dispatch->glesGetUniformuiv(program->native_name, location, params);
        else
            uniform_native_missing(ctx);
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

static unsigned uniform_component_count(GLenum type) {
    switch (type) {
        case GL_FLOAT_VEC2:
        case GL_INT_VEC2:
        case GL_UNSIGNED_INT_VEC2:
        case GL_BOOL_VEC2:
            return 2u;
        case GL_FLOAT_VEC3:
        case GL_INT_VEC3:
        case GL_UNSIGNED_INT_VEC3:
        case GL_BOOL_VEC3:
            return 3u;
        case GL_FLOAT_VEC4:
        case GL_INT_VEC4:
        case GL_UNSIGNED_INT_VEC4:
        case GL_BOOL_VEC4:
            return 4u;
        case GL_FLOAT_MAT2:
            return 4u;
        case GL_FLOAT_MAT3:
            return 9u;
        case GL_FLOAT_MAT4:
            return 16u;
        case GL_FLOAT_MAT2x3:
        case GL_FLOAT_MAT3x2:
            return 6u;
        case GL_FLOAT_MAT2x4:
        case GL_FLOAT_MAT4x2:
            return 8u;
        case GL_FLOAT_MAT3x4:
        case GL_FLOAT_MAT4x3:
            return 12u;
        default:
            return 1u;
    }
}

static int uniform_native_type_locked(struct mgl_context *ctx, GLuint native_program,
                                      GLint location, GLenum *type) {
    GLint resources = 0;
    GLint index;
    const GLenum props[3] = {GL_TYPE, GL_LOCATION, GL_ARRAY_SIZE};
    ctx->native_dispatch->glesGetProgramInterfaceiv(native_program, GL_UNIFORM, GL_ACTIVE_RESOURCES,
                                                    &resources);
    for (index = 0; index < resources; ++index) {
        GLint data[3] = {0, -1, 0};
        ctx->native_dispatch->glesGetProgramResourceiv(native_program, GL_UNIFORM, (GLuint)index, 3,
                                                       props, 3, NULL, data);
        if (data[1] >= 0 && location >= data[1] &&
            (int64_t)location - data[1] < (data[2] > 0 ? data[2] : 1)) {
            *type = (GLenum)data[0];
            return 1;
        }
    }
    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    return 0;
}

static void getn_uniform(GLuint program_name, GLint location, GLsizei bufSize, void *params,
                         unsigned kind) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program;
    GLenum type = 0u;
    unsigned count;
    size_t element_size;
    if (ctx == NULL)
        return;
    if (shader_reject_inside_begin(ctx))
        return;
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = uniform_query_program(ctx, program_name);
    if (program != NULL && uniform_native_type_locked(ctx, program->native_name, location, &type)) {
        count = uniform_component_count(type);
        element_size = kind == 3u ? sizeof(GLdouble) : sizeof(GLint);
        if ((size_t)bufSize < count * element_size) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        } else if (params == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        } else if (kind == 0u) {
            ctx->native_dispatch->glesGetUniformfv(program->native_name, location, params);
        } else if (kind == 1u) {
            ctx->native_dispatch->glesGetUniformiv(program->native_name, location, params);
        } else if (kind == 2u) {
            ctx->native_dispatch->glesGetUniformuiv(program->native_name, location, params);
        } else {
            GLfloat values[16] = {0};
            GLdouble *out = params;
            unsigned i;
            if (type == GL_UNSIGNED_INT ||
                (type >= GL_UNSIGNED_INT_VEC2 && type <= GL_UNSIGNED_INT_VEC4)) {
                GLuint unsigned_values[16] = {0};
                ctx->native_dispatch->glesGetUniformuiv(program->native_name, location,
                                                        unsigned_values);
                for (i = 0; i < count; ++i)
                    out[i] = (GLdouble)unsigned_values[i];
            } else if (type == GL_INT || type == GL_BOOL ||
                       (type >= GL_INT_VEC2 && type <= GL_INT_VEC4) ||
                       (type >= GL_BOOL_VEC2 && type <= GL_BOOL_VEC4)) {
                GLint ints[16] = {0};
                ctx->native_dispatch->glesGetUniformiv(program->native_name, location, ints);
                for (i = 0; i < count; ++i)
                    out[i] = (GLdouble)ints[i];
            } else {
                ctx->native_dispatch->glesGetUniformfv(program->native_name, location, values);
                for (i = 0; i < count; ++i)
                    out[i] = (GLdouble)values[i];
            }
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params) {
    getn_uniform(program, location, bufSize, params, 0u);
}

void glGetnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint *params) {
    getn_uniform(program, location, bufSize, params, 1u);
}

void glGetnUniformuiv(GLuint program, GLint location, GLsizei bufSize, GLuint *params) {
    getn_uniform(program, location, bufSize, params, 2u);
}

void glGetnUniformdv(GLuint program, GLint location, GLsizei bufSize, GLdouble *params) {
    getn_uniform(program, location, bufSize, params, 3u);
}

void glGetUniformdv(GLuint program, GLint location, GLdouble *params) {
    getn_uniform(program, location, 16 * (GLsizei)sizeof(GLdouble), params, 3u);
}

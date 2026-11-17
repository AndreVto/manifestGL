#include "compiler/spirv_toolchain.h"

#include <pthread.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include <spirv_cross_c.h>

#include "compiler/spirv_glslang_compile.h"
#include "shader/translator.h"

static pthread_once_t g_glslang_once = PTHREAD_ONCE_INIT;
static int g_glslang_initialized = 0;

static void initialize_glslang(void) {
    g_glslang_initialized = glslang_initialize_process();
}

static char *dup_text(const char *text) {
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

static int source_has_index_layout(const char *source) {
    const char *p = source;
    while (p != NULL && (p = strstr(p, "layout")) != NULL) {
        const char *q = p + 6;
        if (p != source && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) {
            p = q;
            continue;
        }
        while (isspace((unsigned char)*q))
            ++q;
        if (*q++ != '(') {
            p += 6;
            continue;
        }
        while (*q != '\0' && *q != ')') {
            if (strncmp(q, "index", 5u) == 0 &&
                (q == source || !(isalnum((unsigned char)q[-1]) || q[-1] == '_')) &&
                !(isalnum((unsigned char)q[5]) || q[5] == '_')) {
                const char *value = q + 5;
                while (isspace((unsigned char)*value))
                    ++value;
                if (*value == '=')
                    return 1;
            }
            ++q;
        }
        p = q;
    }
    return 0;
}

static int source_has_texture_buffer_sampler(const char *source) {
    static const char *const names[] = {"samplerBuffer", "isamplerBuffer", "usamplerBuffer"};
    size_t i;
    for (i = 0u; i < sizeof(names) / sizeof(names[0]); ++i) {
        const char *p = source;
        size_t n = strlen(names[i]);
        while (p != NULL && (p = strstr(p, names[i])) != NULL) {
            const int left_ok = p == source || !(isalnum((unsigned char)p[-1]) || p[-1] == '_');
            const int right_ok = !(isalnum((unsigned char)p[n]) || p[n] == '_');
            if (left_ok && right_ok)
                return 1;
            p += n;
        }
    }
    return 0;
}

static char *inject_extension_after_version(const char *source, const char *extension_line) {
    const char *line_end;
    size_t prefix_len, ext_len, suffix_len;
    char *out;
    if (source == NULL || extension_line == NULL)
        return NULL;
    line_end = strchr(source, '\n');
    if (line_end == NULL)
        return NULL;
    ++line_end;
    prefix_len = (size_t)(line_end - source);
    ext_len = strlen(extension_line);
    suffix_len = strlen(line_end);
    out = malloc(prefix_len + ext_len + suffix_len + 1u);
    if (out == NULL)
        return NULL;
    memcpy(out, source, prefix_len);
    memcpy(out + prefix_len, extension_line, ext_len);
    memcpy(out + prefix_len + ext_len, line_end, suffix_len + 1u);
    return out;
}

static int finalize_gles_extensions(GLenum shader_type, unsigned gles_version, char **source) {
    char *updated;
    if (source == NULL || *source == NULL)
        return 0;
    if (gles_version < 320u && source_has_texture_buffer_sampler(*source) &&
        strstr(*source, "GL_EXT_texture_buffer") == NULL) {
        updated =
            inject_extension_after_version(*source, "#extension GL_EXT_texture_buffer : require\n");
        if (updated == NULL)
            return 0;
        free(*source);
        *source = updated;
    }
    if (shader_type == GL_FRAGMENT_SHADER && source_has_index_layout(*source) &&
        strstr(*source, "GL_EXT_blend_func_extended") == NULL) {
        updated = inject_extension_after_version(
            *source, "#extension GL_EXT_blend_func_extended : require\n");
        if (updated == NULL)
            return 0;
        free(*source);
        *source = updated;
    }
    return 1;
}

static char *join_logs(const char *phase, const char *a, const char *b) {
    size_t np = phase != NULL ? strlen(phase) : 0u;
    size_t na = a != NULL ? strlen(a) : 0u;
    size_t nb = b != NULL ? strlen(b) : 0u;
    size_t total;
    char *out;
    if (np > SIZE_MAX - na - nb - 8u)
        return NULL;
    total = np + na + nb + 8u;
    out = calloc(total, 1u);
    if (out == NULL)
        return NULL;
    if (phase != NULL && phase[0] != '\0') {
        strcat(out, phase);
        strcat(out, ": ");
    }
    if (a != NULL && a[0] != '\0')
        strcat(out, a);
    if (a != NULL && a[0] != '\0' && b != NULL && b[0] != '\0')
        strcat(out, "\n");
    if (b != NULL && b[0] != '\0')
        strcat(out, b);
    return out;
}

static int map_stage(GLenum shader_type, glslang_stage_t *stage) {
    if (stage == NULL)
        return 0;
    switch (shader_type) {
        case GL_VERTEX_SHADER:
            *stage = GLSLANG_STAGE_VERTEX;
            return 1;
        case GL_FRAGMENT_SHADER:
            *stage = GLSLANG_STAGE_FRAGMENT;
            return 1;
        case GL_GEOMETRY_SHADER:
            *stage = GLSLANG_STAGE_GEOMETRY;
            return 1;
        case GL_TESS_CONTROL_SHADER:
            *stage = GLSLANG_STAGE_TESSCONTROL;
            return 1;
        case GL_TESS_EVALUATION_SHADER:
            *stage = GLSLANG_STAGE_TESSEVALUATION;
            return 1;
        case GL_COMPUTE_SHADER:
            *stage = GLSLANG_STAGE_COMPUTE;
            return 1;
        default:
            return 0;
    }
}

static void mgl_spvc_error_callback(void *userdata, const char *error) {
    char **slot = userdata;
    if (slot == NULL || *slot != NULL)
        return;
    *slot = dup_text(error != NULL ? error : "SPIRV-Cross error");
}

/* Normalize legacy GLSL backslash-newline continuations before preprocessing. */
static char *normalize_glsl_preprocess_input(const char *source) {
    const unsigned char *p = (const unsigned char *)source;
    size_t i = 0u, j = 0u, n = strlen(source);
    char *normalized;
    if (n >= 3u && p[0] == 0xefu && p[1] == 0xbbu && p[2] == 0xbfu) {
        p += 3;
        n -= 3u;
    }
    normalized = malloc(n + 1u);
    if (normalized == NULL)
        return NULL;
    while (i < n) {
        if (p[i] == '\\') {
            size_t k = i + 1u;
            while (k < n && (p[k] == ' ' || p[k] == '\t'))
                ++k;
            if (k < n && p[k] == '\r') {
                i = k + ((k + 1u < n && p[k + 1u] == '\n') ? 2u : 1u);
                continue;
            }
            if (k < n && p[k] == '\n') {
                i = k + 1u;
                continue;
            }
        }
        normalized[j++] = (char)p[i++];
    }
    normalized[j] = '\0';
    return normalized;
}

int mgl_preprocess_desktop_glsl(GLenum shader_type, const char *source, unsigned desktop_version,
                                char **expanded_source, char **compiler_log) {
    glslang_stage_t stage;
    glslang_input_t input;
    glslang_shader_t *shader = NULL;
    const char *expanded;
    char *result = NULL;
    char *normalized = NULL;
    int ok = 0;

    if (expanded_source != NULL)
        *expanded_source = NULL;
    if (compiler_log != NULL)
        *compiler_log = NULL;
    if (source == NULL || expanded_source == NULL || !map_stage(shader_type, &stage))
        return 0;
    if (desktop_version == 0u)
        desktop_version = 110u;

    (void)pthread_once(&g_glslang_once, initialize_glslang);
    if (!g_glslang_initialized) {
        if (compiler_log != NULL)
            *compiler_log = dup_text("glslang initialization failed");
        return 0;
    }

    memset(&input, 0, sizeof(input));
    input.language = GLSLANG_SOURCE_GLSL;
    input.stage = stage;
    /* GLSL preprocessing accepts GLSL 1.10/1.20 compatibility constructs. */
    input.client = GLSLANG_CLIENT_NONE;
    input.target_language = GLSLANG_TARGET_NONE;
    normalized = normalize_glsl_preprocess_input(source);
    if (normalized == NULL)
        return 0;
    input.code = normalized;
    input.default_version = (int)desktop_version;
    input.default_profile = GLSLANG_NO_PROFILE;
    input.messages = GLSLANG_MSG_DEFAULT_BIT;
    input.resource = glslang_default_resource();

    shader = glslang_shader_create(&input);
    if (shader == NULL) {
        if (compiler_log != NULL)
            *compiler_log = dup_text("glslang preprocessor allocation failed");
        free(normalized);
        return 0;
    }
    if (!glslang_shader_preprocess(shader, &input)) {
        if (compiler_log != NULL)
            *compiler_log = join_logs("original-version GLSL preprocessing",
                                      glslang_shader_get_info_log(shader),
                                      glslang_shader_get_info_debug_log(shader));
        goto cleanup;
    }
    expanded = glslang_shader_get_preprocessed_code(shader);
    if (expanded == NULL || expanded[0] == '\0') {
        if (compiler_log != NULL)
            *compiler_log = dup_text("empty GLSL preprocessor output");
        goto cleanup;
    }
    /* Preserve the desktop GLSL version used for macro preprocessing in the later compatibility
     * pass. */
    {
        const char *p = expanded;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            ++p;
        if (strncmp(p, "#version", 8u) == 0 && isspace((unsigned char)p[8])) {
            result = dup_text(expanded);
        } else {
            char version_line[40];
            size_t prefix_len;
            size_t suffix_len = strlen(expanded);
            int n = snprintf(version_line, sizeof(version_line), "#version %u\n", desktop_version);
            if (n <= 0 || (size_t)n >= sizeof(version_line))
                goto cleanup;
            prefix_len = (size_t)n;
            if (prefix_len > SIZE_MAX - suffix_len - 1u)
                goto cleanup;
            result = malloc(prefix_len + suffix_len + 1u);
            if (result != NULL) {
                memcpy(result, version_line, prefix_len);
                memcpy(result + prefix_len, expanded, suffix_len + 1u);
            }
        }
    }
    if (result == NULL)
        goto cleanup;
    *expanded_source = result;
    result = NULL;
    ok = 1;
cleanup:
    free(result);
    glslang_shader_delete(shader);
    free(normalized);
    return ok;
}

int mgl_validate_gles_glsl(GLenum shader_type, const char *source) {
    glslang_stage_t stage;
    glslang_input_t input;
    glslang_shader_t *shader;
    int valid = 0;
    if (source == NULL || !map_stage(shader_type, &stage))
        return 0;
    (void)pthread_once(&g_glslang_once, initialize_glslang);
    if (!g_glslang_initialized)
        return 0;
    memset(&input, 0, sizeof(input));
    input.language = GLSLANG_SOURCE_GLSL;
    input.stage = stage;
    input.client = GLSLANG_CLIENT_NONE;
    input.target_language = GLSLANG_TARGET_NONE;
    input.code = source;
    input.default_version = 100;
    input.default_profile = GLSLANG_ES_PROFILE;
    input.messages = GLSLANG_MSG_DEFAULT_BIT;
    input.resource = glslang_default_resource();
    shader = glslang_shader_create(&input);
    if (shader == NULL)
        return 0;
    if (glslang_shader_preprocess(shader, &input)) {
        glslang_shader_set_preprocessed_code(shader, glslang_shader_get_preprocessed_code(shader));
        valid = glslang_shader_parse(shader, &input);
    }
    glslang_shader_delete(shader);
    return valid;
}

static unsigned desktop_glsl_version(const char *source) {
    const char *p = source;
    char *end;
    unsigned long parsed;
    while (*p != '\0') {
        while (isspace((unsigned char)*p))
            ++p;
        if (p[0] == '/' && p[1] == '/') {
            while (*p != '\0' && *p != '\n')
                ++p;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            const char *close = strstr(p + 2, "*/");
            if (close == NULL)
                return 110u;
            p = close + 2;
            continue;
        }
        break;
    }
    if (*p++ != '#')
        return 110u;
    while (*p == ' ' || *p == '\t')
        ++p;
    if (strncmp(p, "version", 7u) != 0 || !isspace((unsigned char)p[7]))
        return 110u;
    p += 7;
    while (*p == ' ' || *p == '\t')
        ++p;
    parsed = strtoul(p, &end, 10);
    if (end == p || parsed == 0ul || parsed > 999ul)
        return 110u;
    return (unsigned)parsed;
}

static int spirv_execution_model(GLenum shader_type, SpvExecutionModel *model) {
    if (model == NULL)
        return 0;
    switch (shader_type) {
        case GL_VERTEX_SHADER:
            *model = SpvExecutionModelVertex;
            return 1;
        case GL_FRAGMENT_SHADER:
            *model = SpvExecutionModelFragment;
            return 1;
        case GL_GEOMETRY_SHADER:
            *model = SpvExecutionModelGeometry;
            return 1;
        case GL_TESS_CONTROL_SHADER:
            *model = SpvExecutionModelTessellationControl;
            return 1;
        case GL_TESS_EVALUATION_SHADER:
            *model = SpvExecutionModelTessellationEvaluation;
            return 1;
        case GL_COMPUTE_SHADER:
            *model = SpvExecutionModelGLCompute;
            return 1;
        default:
            return 0;
    }
}

int mgl_spirv_validate_module(const uint32_t *words, size_t word_count) {
    spvc_context ctx = NULL;
    spvc_parsed_ir ir = NULL;
    int ok = 0;
    if (words == NULL || word_count < 5u || words[0] != 0x07230203u)
        return 0;
    if (spvc_context_create(&ctx) == SPVC_SUCCESS && ctx != NULL)
        ok = spvc_context_parse_spirv(ctx, (const SpvId *)words, word_count, &ir) == SPVC_SUCCESS;
    if (ctx != NULL)
        spvc_context_destroy(ctx);
    return ok;
}

int mgl_spirv_specialize_glsl(GLenum shader_type, const uint32_t *words, size_t word_count,
                              const char *entry_point, GLuint num_constants,
                              const GLuint *constant_ids, const GLuint *constant_values,
                              char **gles_source, char **compiler_log, GLenum *error) {
    spvc_context ctx = NULL;
    spvc_parsed_ir ir = NULL;
    spvc_compiler compiler = NULL;
    spvc_compiler_options options = NULL;
    const spvc_entry_point *entries = NULL;
    const spvc_specialization_constant *constants = NULL;
    size_t entry_count = 0u, constant_count = 0u, i;
    SpvExecutionModel model;
    const char *translated = NULL;
    char *cross_error = NULL;
    int matching_name = 0, matching_stage = 0;
    int ok = 0;
    if (gles_source != NULL)
        *gles_source = NULL;
    if (compiler_log != NULL)
        *compiler_log = NULL;
    if (error != NULL)
        *error = GL_NO_ERROR;
    if (gles_source == NULL || entry_point == NULL || !spirv_execution_model(shader_type, &model) ||
        words == NULL || word_count < 5u || words[0] != 0x07230203u)
        return 0;
    if (spvc_context_create(&ctx) != SPVC_SUCCESS || ctx == NULL)
        goto cleanup;
    spvc_context_set_error_callback(ctx, mgl_spvc_error_callback, &cross_error);
    if (spvc_context_parse_spirv(ctx, (const SpvId *)words, word_count, &ir) != SPVC_SUCCESS ||
        spvc_context_create_compiler(ctx, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP,
                                     &compiler) != SPVC_SUCCESS)
        goto cleanup;
    if (spvc_compiler_get_entry_points(compiler, &entries, &entry_count) != SPVC_SUCCESS)
        goto cleanup;
    for (i = 0u; i < entry_count; ++i) {
        if (strcmp(entries[i].name, entry_point) == 0) {
            matching_name = 1;
            if (entries[i].execution_model == model)
                matching_stage = 1;
        }
    }
    if (!matching_name || !matching_stage) {
        if (error != NULL)
            *error = matching_name ? GL_INVALID_OPERATION : GL_INVALID_VALUE;
        goto cleanup;
    }
    if (spvc_compiler_set_entry_point(compiler, entry_point, model) != SPVC_SUCCESS)
        goto cleanup;
    if (spvc_compiler_get_specialization_constants(compiler, &constants, &constant_count) !=
        SPVC_SUCCESS)
        goto cleanup;
    for (i = 0u; i < num_constants; ++i) {
        size_t j;
        for (j = 0u; j < constant_count; ++j)
            if (constants[j].constant_id == constant_ids[i])
                break;
        if (j == constant_count) {
            if (error != NULL)
                *error = GL_INVALID_VALUE;
            goto cleanup;
        }
        spvc_constant constant = spvc_compiler_get_constant_handle(compiler, constants[j].id);
        if (constant == NULL)
            goto cleanup;
        spvc_constant_set_scalar_u32(constant, 0, 0, constant_values[i]);
    }
    if (spvc_compiler_create_compiler_options(compiler, &options) != SPVC_SUCCESS ||
        spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, 320u) !=
            SPVC_SUCCESS ||
        spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_TRUE) !=
            SPVC_SUCCESS ||
        spvc_compiler_options_set_bool(options,
                                       SPVC_COMPILER_OPTION_GLSL_ES_DEFAULT_FLOAT_PRECISION_HIGHP,
                                       SPVC_TRUE) != SPVC_SUCCESS ||
        spvc_compiler_options_set_bool(options,
                                       SPVC_COMPILER_OPTION_GLSL_ES_DEFAULT_INT_PRECISION_HIGHP,
                                       SPVC_TRUE) != SPVC_SUCCESS ||
        spvc_compiler_install_compiler_options(compiler, options) != SPVC_SUCCESS ||
        spvc_compiler_compile(compiler, &translated) != SPVC_SUCCESS || translated == NULL)
        goto cleanup;
    *gles_source = dup_text(translated);
    ok = *gles_source != NULL && finalize_gles_extensions(shader_type, 320u, gles_source);
    if (ok && error != NULL)
        *error = GL_NO_ERROR;
cleanup:
    if (!ok && gles_source != NULL) {
        free(*gles_source);
        *gles_source = NULL;
    }
    if (!ok && compiler_log != NULL)
        *compiler_log =
            dup_text(cross_error != NULL ? cross_error : "SPIR-V specialization failed");
    free(cross_error);
    if (ctx != NULL)
        spvc_context_destroy(ctx);
    return ok;
}

int mgl_spirv_translate_glsl(GLenum shader_type, const char *desktop_source, unsigned gles_version,
                             char **gles_source, char **compiler_log) {
    glslang_stage_t stage;
    unsigned int *spirv_words = NULL;
    size_t spirv_word_count = 0u;
    spvc_context cross_context = NULL;
    spvc_parsed_ir parsed_ir = NULL;
    spvc_compiler compiler = NULL;
    spvc_compiler_options options = NULL;
    const char *cross_source = NULL;
    char *cross_error = NULL;
    char *parse_error = NULL;
    int ok = 0;

    if (gles_source != NULL)
        *gles_source = NULL;
    if (compiler_log != NULL)
        *compiler_log = NULL;
    if (desktop_source == NULL || gles_source == NULL || !map_stage(shader_type, &stage))
        return 0;
    if (gles_version < 300u)
        gles_version = 300u;

    (void)pthread_once(&g_glslang_once, initialize_glslang);
    if (!g_glslang_initialized) {
        if (compiler_log != NULL)
            *compiler_log = dup_text("glslang initialization failed");
        return 0;
    }

    if (!mgl_glslang_compile_spirv(shader_type, desktop_source,
                                   desktop_glsl_version(desktop_source), &spirv_words,
                                   &spirv_word_count, &parse_error)) {
        if (compiler_log != NULL)
            *compiler_log = dup_text(parse_error);
        goto cleanup;
    }

    if (spvc_context_create(&cross_context) != SPVC_SUCCESS || cross_context == NULL) {
        if (compiler_log != NULL)
            *compiler_log = dup_text("SPIRV-Cross context creation failed");
        goto cleanup;
    }
    spvc_context_set_error_callback(cross_context, mgl_spvc_error_callback, &cross_error);
    if (spvc_context_parse_spirv(cross_context, (const SpvId *)spirv_words, spirv_word_count,
                                 &parsed_ir) != SPVC_SUCCESS)
        goto cross_fail;
    if (spvc_context_create_compiler(cross_context, SPVC_BACKEND_GLSL, parsed_ir,
                                     SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler) != SPVC_SUCCESS)
        goto cross_fail;
    if (spvc_compiler_create_compiler_options(compiler, &options) != SPVC_SUCCESS)
        goto cross_fail;
    if (spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, gles_version) !=
        SPVC_SUCCESS)
        goto cross_fail;
    if (spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_TRUE) !=
        SPVC_SUCCESS)
        goto cross_fail;
    if (spvc_compiler_options_set_bool(options,
                                       SPVC_COMPILER_OPTION_GLSL_ES_DEFAULT_FLOAT_PRECISION_HIGHP,
                                       SPVC_TRUE) != SPVC_SUCCESS)
        goto cross_fail;
    if (spvc_compiler_options_set_bool(options,
                                       SPVC_COMPILER_OPTION_GLSL_ES_DEFAULT_INT_PRECISION_HIGHP,
                                       SPVC_TRUE) != SPVC_SUCCESS)
        goto cross_fail;
    if (spvc_compiler_install_compiler_options(compiler, options) != SPVC_SUCCESS)
        goto cross_fail;
    if (spvc_compiler_compile(compiler, &cross_source) != SPVC_SUCCESS || cross_source == NULL)
        goto cross_fail;

    *gles_source = dup_text(cross_source);
    if (*gles_source == NULL || !finalize_gles_extensions(shader_type, gles_version, gles_source))
        goto cleanup;
    if (compiler_log != NULL) {
        glslang_version_t ver;
        char version_text[160];
        glslang_get_version(&ver);
        (void)snprintf(version_text, sizeof(version_text),
                       "compiled through glslang %d.%d.%d -> SPIR-V -> SPIRV-Cross -> GLSL ES %u",
                       ver.major, ver.minor, ver.patch, gles_version);
        *compiler_log = dup_text(version_text);
    }
    ok = 1;
    goto cleanup;

cross_fail:
    if (compiler_log != NULL)
        *compiler_log = cross_error != NULL ? dup_text(cross_error)
                                            : dup_text("SPIRV-Cross translation failed");

cleanup:
    free(cross_error);
    free(parse_error);
    free(spirv_words);
    if (cross_context != NULL)
        spvc_context_destroy(cross_context);
    if (!ok && gles_source != NULL) {
        free(*gles_source);
        *gles_source = NULL;
    }
    return ok;
}


char *mgl_fragment_output_locations_log(const char *source, size_t binding_count,
                                        const struct mgl_fragment_output_location *bindings,
                                        char **failure_log) {
    unsigned int *words = NULL;
    size_t word_count = 0u, output_count = 0u, i, j;
    int rewrite = 0;
    char *log = NULL;
    char *result = NULL;
    struct mgl_glslang_fragment_output *outputs = NULL;
    spvc_context context = NULL;
    spvc_parsed_ir parsed = NULL;
    spvc_compiler compiler = NULL;
    spvc_compiler_options options = NULL;
    spvc_resources resources = NULL;
    const spvc_reflected_resource *reflected = NULL;
    size_t reflected_count = 0u;
    const char *generated = NULL;
    if (failure_log != NULL)
        *failure_log = NULL;
    if (source == NULL)
        return NULL;
    if (!mgl_glslang_fragment_output_spirv(source, 320u, binding_count, bindings,
                                           &words, &word_count, &rewrite, &outputs,
                                           &output_count, &log)) {
        if (failure_log != NULL)
            *failure_log = dup_text(log != NULL ? log : "glslang could not process fragment outputs");
        goto cleanup;
    }
    if (!rewrite) {
        result = dup_text(source);
        goto cleanup;
    }
    if (spvc_context_create(&context) != SPVC_SUCCESS || context == NULL ||
        spvc_context_parse_spirv(context, (const SpvId *)words, word_count, &parsed) != SPVC_SUCCESS ||
        spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, parsed,
                                     SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler) != SPVC_SUCCESS ||
        spvc_compiler_create_shader_resources(compiler, &resources) != SPVC_SUCCESS ||
        spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STAGE_OUTPUT,
                                                  &reflected, &reflected_count) != SPVC_SUCCESS)
        goto cleanup;
    for (i = 0u; i < output_count; ++i) {
        for (j = 0u; j < reflected_count; ++j) {
            if (strcmp(outputs[i].name, reflected[j].name) != 0)
                continue;
            spvc_compiler_set_decoration(compiler, reflected[j].id, SpvDecorationLocation,
                                         outputs[i].location);
            if (outputs[i].index == 1u)
                spvc_compiler_set_decoration(compiler, reflected[j].id, SpvDecorationIndex, 1u);
            break;
        }
    }
    if (spvc_compiler_create_compiler_options(compiler, &options) != SPVC_SUCCESS ||
        spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, 320u) != SPVC_SUCCESS ||
        spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_TRUE) != SPVC_SUCCESS ||
        spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES_DEFAULT_FLOAT_PRECISION_HIGHP,
                                       SPVC_TRUE) != SPVC_SUCCESS ||
        spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES_DEFAULT_INT_PRECISION_HIGHP,
                                       SPVC_TRUE) != SPVC_SUCCESS ||
        spvc_compiler_install_compiler_options(compiler, options) != SPVC_SUCCESS ||
        spvc_compiler_compile(compiler, &generated) != SPVC_SUCCESS || generated == NULL)
        goto cleanup;
    result = dup_text(generated);
    if (result != NULL && !finalize_gles_extensions(GL_FRAGMENT_SHADER, 320u, &result)) {
        free(result);
        result = NULL;
    }
cleanup:
    if (result == NULL && failure_log != NULL && *failure_log == NULL)
        *failure_log = dup_text(context != NULL ? spvc_context_get_last_error_string(context)
                                                 : "fragment output processing failed");
    for (i = 0u; i < output_count; ++i)
        free(outputs[i].name);
    free(outputs);
    free(words);
    free(log);
    if (context != NULL)
        spvc_context_destroy(context);
    return result;
}

char *mgl_fragment_output_locations(const char *source, size_t binding_count,
                                    const struct mgl_fragment_output_location *bindings) {
    return mgl_fragment_output_locations_log(source, binding_count, bindings, NULL);
}

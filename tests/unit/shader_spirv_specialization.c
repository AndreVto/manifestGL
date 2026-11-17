#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "shader/shader_store.h"
#include "compiler/spirv_glslang_compile.h"
#include "compiler/spirv_toolchain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint next_native_shader = 501u;
static GLuint next_native_program = 701u;
static unsigned source_calls;
static unsigned compile_calls;
static char last_source[8192];

static GLuint fake_create_shader(GLenum type) {
    (void)type;
    return next_native_shader++;
}

static GLuint fake_create_program(void) {
    return next_native_program++;
}

static void fake_delete_shader(GLuint shader) {
    (void)shader;
}

static void fake_delete_program(GLuint program) {
    (void)program;
}

static void fake_shader_source(GLuint shader, GLsizei count, const GLchar *const *source,
                               const GLint *length) {
    (void)shader;
    (void)length;
    ++source_calls;
    last_source[0] = 0;
    if (count == 1 && source != NULL && source[0] != NULL)
        snprintf(last_source, sizeof(last_source), "%s", source[0]);
}

static void fake_compile_shader(GLuint shader) {
    (void)shader;
    ++compile_calls;
}

static void fake_shader_iv(GLuint shader, GLenum pname, GLint *value) {
    (void)shader;
    if (pname == GL_COMPILE_STATUS)
        *value = GL_TRUE;
    else if (pname == GL_INFO_LOG_LENGTH)
        *value = 1;
    else
        *value = 0;
}

static void fake_shader_log(GLuint shader, GLsizei size, GLsizei *length, GLchar *message) {
    (void)shader;
    if (size > 0 && message != NULL)
        message[0] = 0;
    if (length != NULL)
        *length = 0;
}

static void fake_get_integerv(GLenum pname, GLint *value) {
    if (pname == GL_NUM_SHADER_BINARY_FORMATS)
        *value = 1;
    else if (pname == GL_SHADER_BINARY_FORMATS)
        value[0] = 0xB17A;
    else
        *value = 0;
}

static int check(GLenum expected, const char *label) {
    GLenum actual = glGetError();
    if (actual != expected) {
        fprintf(stderr, "%s: error %x expected %x\n", label, (unsigned)actual, (unsigned)expected);
        return 0;
    }
    return 1;
}

int main(void) {
    static const char fragment_source[] = "#version 450 core\n"
                                          "layout(constant_id = 7) const float shade = 0.5;\n"
                                          "layout(location = 0) out vec4 color;\n"
                                          "void main() { color = vec4(shade, 0.1, 0.2, 1.0); }\n";
    struct mgl_context ctx = {0};
    struct mgl_native_dispatch native = {0};
    unsigned int *spirv = NULL;
    size_t words = 0u;
    char *log = NULL;
    GLuint frag, frag2, vert;
    struct mgl_shader *first, *second;
    GLuint ids[2], constant_id = 7u;
    float expected_shade = 0.75f;
    GLuint bits = 0u;
    GLint status = -1;
    GLint formats[3] = {-1, -1, -1};
    const GLchar *glsl = "#version 330 core\nout vec4 color;void main(){ color=vec4(1); }";
    if (!mgl_glslang_compile_spirv(GL_FRAGMENT_SHADER, fragment_source, 450u, &spirv, &words,
                                   &log)) {
        fprintf(stderr, "failed to make test SPIR-V: %s\n", log != NULL ? log : "no compiler log");
        free(log);
        return 1;
    }
    free(log);
    memcpy(&bits, &expected_shade, sizeof(bits));
    native.glesCreateShader = fake_create_shader;
    native.glesCreateProgram = fake_create_program;
    native.glesDeleteShader = fake_delete_shader;
    native.glesDeleteProgram = fake_delete_program;
    native.glesShaderSource = fake_shader_source;
    native.glesCompileShader = fake_compile_shader;
    native.glesGetShaderiv = fake_shader_iv;
    native.glesGetShaderInfoLog = fake_shader_log;
    native.glesGetIntegerv = fake_get_integerv;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL) {
        free(spirv);
        return 2;
    }
    mgl_error_init(&ctx.errors);
    mgl_context_set_current_for_test(&ctx);

    frag = glCreateShader(GL_FRAGMENT_SHADER);
    frag2 = glCreateShader(GL_FRAGMENT_SHADER);
    vert = glCreateShader(GL_VERTEX_SHADER);
    if (!frag || !frag2 || !vert || !check(GL_NO_ERROR, "shader creation"))
        return 3;
    if (glXGetProcAddress((const GLubyte *)"glSpecializeShader") == NULL)
        return 4;
    glGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &status);
    if (status != 2 || !check(GL_NO_ERROR, "binary format count"))
        return 5;
    glGetIntegerv(GL_SHADER_BINARY_FORMATS, formats);
    if (formats[0] != 0xB17A || (GLenum)formats[1] != GL_SHADER_BINARY_FORMAT_SPIR_V ||
        !check(GL_NO_ERROR, "binary formats"))
        return 6;

    glSpecializeShader(frag, "main", 0u, NULL, NULL);
    if (!check(GL_INVALID_OPERATION, "no SPIR-V"))
        return 7;
    ids[0] = frag;
    ids[1] = frag2;
    glShaderBinary(2, ids, GL_SHADER_BINARY_FORMAT_SPIR_V, spirv,
                   (GLsizei)(words * sizeof(*spirv)));
    if (!check(GL_NO_ERROR, "SPIR-V load with repeated shader stages"))
        return 8;
    first = mgl_shader_find(&ctx, frag);
    second = mgl_shader_find(&ctx, frag2);
    if (first == NULL || second == NULL || first->spirv_module == NULL ||
        first->spirv_module != second->spirv_module ||
        atomic_load(&first->spirv_module->references) != 2u)
        return 24;
    glShaderSource(frag2, 1, &glsl, NULL);
    if (!check(GL_NO_ERROR, "independent source replacement") || second->spirv_module != NULL ||
        first->spirv_module == NULL || atomic_load(&first->spirv_module->references) != 1u)
        return 25;
    glShaderBinary(2, ids, GL_SHADER_BINARY_FORMAT_SPIR_V, spirv,
                   (GLsizei)(words * sizeof(*spirv)));
    if (!check(GL_NO_ERROR, "shared SPIR-V reload") || first->spirv_module == NULL ||
        first->spirv_module != second->spirv_module ||
        atomic_load(&first->spirv_module->references) != 2u)
        return 26;
    glGetShaderiv(frag, GL_SPIR_V_BINARY, &status);
    if (status != GL_TRUE || !check(GL_NO_ERROR, "SPIR-V state"))
        return 9;
    glGetShaderiv(frag, GL_COMPILE_STATUS, &status);
    if (status != GL_FALSE || !check(GL_NO_ERROR, "unspecialized compile status"))
        return 10;
    glCompileShader(frag);
    if (!check(GL_INVALID_OPERATION, "CompileShader on SPIR-V"))
        return 11;
    glSpecializeShader(frag, "no_entry", 0u, NULL, NULL);
    if (!check(GL_INVALID_VALUE, "invalid entry point"))
        return 12;
    constant_id = 999u;
    glSpecializeShader(frag, "main", 1u, &constant_id, &bits);
    if (!check(GL_INVALID_VALUE, "invalid constant id"))
        return 13;
    constant_id = 7u;
    glSpecializeShader(frag, "main", 1u, &constant_id, &bits);
    if (!check(GL_NO_ERROR, "SPIR-V specialization"))
        return 14;
    if (source_calls != 1u || compile_calls != 1u ||
        strstr(last_source, "#version 320 es") == NULL ||
        strstr(last_source, "SPIRV_CROSS_CONSTANT_ID_7 0.75") == NULL) {
        fprintf(stderr, "specialization failed to route through GLES shader: %s\n", last_source);
        return 15;
    }
    glGetShaderiv(frag, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE || !check(GL_NO_ERROR, "specialization compile status"))
        return 16;
    glSpecializeShader(frag, "main", 0u, NULL, NULL);
    if (!check(GL_INVALID_OPERATION, "duplicate specialization"))
        return 17;
    glSpecializeShader(frag2, "main", 0u, NULL, NULL);
    if (!check(GL_NO_ERROR, "independent module specialization") || source_calls != 2u ||
        compile_calls != 2u || strstr(last_source, "0.75") != NULL)
        return 27;

    ids[0] = vert;
    glShaderBinary(1, ids, GL_SHADER_BINARY_FORMAT_SPIR_V, spirv,
                   (GLsizei)(words * sizeof(*spirv)));
    if (!check(GL_NO_ERROR, "SPIR-V load on another stage"))
        return 18;
    glSpecializeShader(vert, "main", 0u, NULL, NULL);
    if (!check(GL_INVALID_OPERATION, "stage mismatch"))
        return 19;

    glShaderSource(frag, 1, &glsl, NULL);
    if (!check(GL_NO_ERROR, "GLSL resets SPIR-V"))
        return 20;
    glGetShaderiv(frag, GL_SPIR_V_BINARY, &status);
    if (status != GL_FALSE || !check(GL_NO_ERROR, "cleared SPIR-V state"))
        return 21;
    glShaderBinary(1, &frag, GL_SHADER_BINARY_FORMAT_SPIR_V, "bad", 3);
    if (!check(GL_INVALID_VALUE, "invalid module size"))
        return 22;
    glGetShaderiv(frag, GL_SPIR_V_BINARY, &status);
    if (status != GL_FALSE || !check(GL_NO_ERROR, "invalid binary preserves GLSL source"))
        return 23;

    glGetIntegerv(GL_NUM_SPIR_V_EXTENSIONS, &status);
    if (status < 0 || !check(GL_NO_ERROR, "SPIR-V extension count"))
        return 28;
    for (GLint i = 0; i < status; ++i) {
        const GLubyte *extension = glGetStringi(GL_SPIR_V_EXTENSIONS, (GLuint)i);
        if (extension == NULL || extension[0] == '\0' ||
            !check(GL_NO_ERROR, "SPIR-V extension indexed string"))
            return 29;
    }
    if (glGetStringi(GL_SPIR_V_EXTENSIONS, (GLuint)status) != NULL ||
        !check(GL_INVALID_VALUE, "invalid SPIR-V extension index"))
        return 29;
    glGetIntegerv(GL_NUM_SHADING_LANGUAGE_VERSIONS, &status);
    if (status <= 0 || !check(GL_NO_ERROR, "GLSL version count"))
        return 30;
    for (GLint i = 0; i < status; ++i) {
        const GLubyte *version = glGetStringi(GL_SHADING_LANGUAGE_VERSION, (GLuint)i);
        if (version == NULL || version[0] == '\0' ||
            !check(GL_NO_ERROR, "GLSL version indexed string"))
            return 31;
    }
    if (glGetStringi(GL_SHADING_LANGUAGE_VERSION, (GLuint)status) != NULL ||
        !check(GL_INVALID_VALUE, "invalid GLSL version index"))
        return 32;
    {
        GLuint repeated[2] = {frag2, frag2};
        glShaderBinary(2, repeated, GL_SHADER_BINARY_FORMAT_SPIR_V, spirv,
                       (GLsizei)(words * sizeof(*spirv)));
        if (!check(GL_NO_ERROR, "duplicate shader handles") || second->spirv_module == NULL ||
            atomic_load(&second->spirv_module->references) != 1u)
            return 33;
        glShaderSource(frag2, 1, &glsl, NULL);
        if (!check(GL_NO_ERROR, "duplicate reload release") || second->spirv_module != NULL)
            return 34;
    }
    {
        static const char buffer_source[] = "#version 450 core\n"
                                            "layout(location = 0) out vec4 color;\n"
                                            "layout(binding = 0) uniform samplerBuffer tex;\n"
                                            "void main() { color = texelFetch(tex, 0); }\n";
        char *translated = NULL, *details = NULL;
        int ok = mgl_spirv_translate_glsl(GL_FRAGMENT_SHADER, buffer_source, 320u, &translated,
                                          &details);
        if (!ok || translated == NULL || strstr(translated, "samplerBuffer") == NULL ||
            strstr(translated, "#extension GL_EXT_texture_buffer : require") != NULL) {
            fprintf(stderr, "GLES 3.20 buffer texture translation: %s\n",
                    details != NULL      ? details
                    : translated != NULL ? translated
                                         : "no output");
            free(translated);
            free(details);
            return 35;
        }
        free(translated);
        free(details);
    }
    {
        static const char ordinary_index[] =
            "#version 450 core\n"
            "layout(location = 0) out vec4 color;\n"
            "uniform int inputValue;\n"
            "void main() { int index = inputValue; color = vec4(float(index)); }\n";
        char *translated = NULL, *details = NULL;
        int ok = mgl_spirv_translate_glsl(GL_FRAGMENT_SHADER, ordinary_index, 320u, &translated,
                                          &details);
        if (!ok || translated == NULL ||
            strstr(translated, "GL_EXT_blend_func_extended : require") != NULL) {
            fprintf(stderr, "unnecessary dual-source extension: %s\n",
                    details != NULL      ? details
                    : translated != NULL ? translated
                                         : "no output");
            free(translated);
            free(details);
            return 40;
        }
        free(translated);
        free(details);
    }
    {
        static const char dual_source[] =
            "#version 450 core\n"
            "layout(location = 0, index = 0) out vec4 primary;\n"
            "layout(location = 0, index = 1) out vec4 secondary;\n"
            "void main() { primary = vec4(0.25); secondary = vec4(0.75); }\n";
        unsigned int *dual_binary = NULL;
        size_t dual_words = 0u;
        char *details = NULL;
        if (!mgl_glslang_compile_spirv(GL_FRAGMENT_SHADER, dual_source, 450u, &dual_binary,
                                       &dual_words, &details)) {
            fprintf(stderr, "dual-source test compilation: %s\n",
                    details != NULL ? details : "no log");
            free(details);
            return 36;
        }
        free(details);
        glShaderBinary(1, &frag2, GL_SHADER_BINARY_FORMAT_SPIR_V, dual_binary,
                       (GLsizei)(dual_words * sizeof(*dual_binary)));
        free(dual_binary);
        if (!check(GL_NO_ERROR, "dual-source SPIR-V load"))
            return 37;
        glSpecializeShader(frag2, "main", 0u, NULL, NULL);
        if (!check(GL_NO_ERROR, "dual-source specialization"))
            return 38;
        if (strstr(last_source, "GL_EXT_blend_func_extended : require") == NULL ||
            strstr(last_source, "index = 1") == NULL) {
            fprintf(stderr, "dual-source GLES shader missing extension: %s\n", last_source);
            return 39;
        }
    }
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    free(spirv);
    return 0;
}

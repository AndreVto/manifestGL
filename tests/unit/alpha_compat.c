#include "mgl/gl.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "shader/translator.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_shader;
    GLuint next_program;
    GLenum shader_type[16];
    char fragment_source[4096];
    unsigned native_enable_calls;
    GLint alpha_enabled;
    GLint alpha_func;
    GLfloat alpha_ref;
} cap;

static GLuint fake_create_shader(GLenum type) {
    GLuint id = ++cap.next_shader;
    if (id < 16u)
        cap.shader_type[id] = type;
    return id;
}

static void fake_shader_source(GLuint shader, GLsizei count, const GLchar *const *strings,
                               const GLint *lengths) {
    GLsizei i;
    size_t used = 0u;
    if (cap.shader_type[shader] != GL_FRAGMENT_SHADER)
        return;
    cap.fragment_source[0] = '\0';
    for (i = 0; i < count && used + 1u < sizeof(cap.fragment_source); ++i) {
        size_t n = lengths != NULL && lengths[i] >= 0 ? (size_t)lengths[i] : strlen(strings[i]);
        if (n > sizeof(cap.fragment_source) - used - 1u)
            n = sizeof(cap.fragment_source) - used - 1u;
        memcpy(cap.fragment_source + used, strings[i], n);
        used += n;
        cap.fragment_source[used] = '\0';
    }
}

static void fake_compile_shader(GLuint shader) {
    (void)shader;
}

static void fake_get_shaderiv(GLuint shader, GLenum pname, GLint *value) {
    (void)shader;
    *value = pname == GL_COMPILE_STATUS ? GL_TRUE : 0;
}

static void fake_delete_shader(GLuint shader) {
    (void)shader;
}

static GLuint fake_create_program(void) {
    return ++cap.next_program + 100u;
}

static void fake_attach_shader(GLuint program, GLuint shader) {
    (void)program;
    (void)shader;
}

static void fake_link_program(GLuint program) {
    (void)program;
}

static void fake_get_programiv(GLuint program, GLenum pname, GLint *value) {
    (void)program;
    *value = pname == GL_LINK_STATUS ? GL_TRUE : 0;
}

static void fake_delete_program(GLuint program) {
    (void)program;
}

static GLint fake_get_attrib_location(GLuint program, const GLchar *name) {
    (void)program;
    if (strcmp(name, "mgl_a_position") == 0)
        return 0;
    if (strcmp(name, "mgl_a_color") == 0)
        return 1;
    return -1;
}

static GLint fake_get_uniform_location(GLuint program, const GLchar *name) {
    (void)program;
    if (strcmp(name, "mgl_u_model_view_projection") == 0)
        return 3;
    if (strcmp(name, "mgl_u_alpha_test_enabled") == 0)
        return 5;
    if (strcmp(name, "mgl_u_alpha_test_func") == 0)
        return 6;
    if (strcmp(name, "mgl_u_alpha_test_ref") == 0)
        return 7;
    return -1;
}

static void fake_uniform1i(GLint location, GLint value) {
    if (location == 5)
        cap.alpha_enabled = value;
    else if (location == 6)
        cap.alpha_func = value;
}

static void fake_uniform1f(GLint location, GLfloat value) {
    if (location == 7)
        cap.alpha_ref = value;
}

static void fake_enable(GLenum cap_enum) {
    (void)cap_enum;
    ++cap.native_enable_calls;
}

static void fake_disable(GLenum cap_enum) {
    (void)cap_enum;
    ++cap.native_enable_calls;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "alpha compatibility failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLint iv = 0;
    GLfloat fv = 0.0f;
    char *translated;
    char *log = NULL;
    const char *legacy_fs = "#version 120\nvoid main(){ gl_FragColor=vec4(1.0,0.5,0.25,0.4); }\n";

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesCreateShader = fake_create_shader;
    native.glesShaderSource = fake_shader_source;
    native.glesCompileShader = fake_compile_shader;
    native.glesGetShaderiv = fake_get_shaderiv;
    native.glesDeleteShader = fake_delete_shader;
    native.glesCreateProgram = fake_create_program;
    native.glesAttachShader = fake_attach_shader;
    native.glesLinkProgram = fake_link_program;
    native.glesGetProgramiv = fake_get_programiv;
    native.glesDeleteProgram = fake_delete_program;
    native.glesGetAttribLocation = fake_get_attrib_location;
    native.glesGetUniformLocation = fake_get_uniform_location;
    native.glesUniform1i = fake_uniform1i;
    native.glesUniform1f = fake_uniform1f;
    native.glesEnable = fake_enable;
    native.glesDisable = fake_disable;
    ctx.native_dispatch = &native;
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 640, 480);
    mgl_compat_state_init(&ctx.compat);
    mgl_context_set_current_for_test(&ctx);

    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.25f);
    if (!glIsEnabled(GL_ALPHA_TEST))
        return fail(1, "interface alpha enable state");
    if (cap.native_enable_calls != 0u)
        return fail(2, "alpha state leaked to GLES enable/disable");
    glGetIntegerv(GL_ALPHA_TEST_FUNC, &iv);
    glGetFloatv(GL_ALPHA_TEST_REF, &fv);
    if (iv != (GLint)GL_GREATER || fabsf(fv - 0.25f) > 0.0001f)
        return fail(3, "alpha getter state");

    program = mgl_compat_fixed_program(&ctx);
    if (program == NULL)
        return fail(4, "fixed program creation");
    if (strstr(cap.fragment_source, "mgl_alpha_test_pass") == NULL ||
        strstr(cap.fragment_source, "discard") == NULL)
        return fail(5, "fixed fragment alpha epilogue");
    mgl_compat_realize_program_uniforms(&ctx, program);
    if (cap.alpha_enabled != 1 || cap.alpha_func != (GLint)GL_GREATER ||
        fabsf(cap.alpha_ref - 0.25f) > 0.0001f)
        return fail(6, "fixed alpha uniform realization");

    translated = mgl_translate_glsl(GL_FRAGMENT_SHADER, legacy_fs, &log);
    if (translated == NULL || strstr(translated, "#define main mgl_user_main") == NULL ||
        strstr(translated, "mgl_alpha_test_pass(mgl_frag_color.a)") == NULL)
        return fail(7, "programmable alpha epilogue");
    free(translated);
    free(log);

    glDisable(GL_ALPHA_TEST);
    mgl_compat_realize_program_uniforms(&ctx, program);
    if (cap.alpha_enabled != 0)
        return fail(8, "dynamic alpha disable uniform");

    mgl_compat_destroy_fixed_program(&ctx);
    mgl_context_set_current_for_test(NULL);
    puts("mGL alpha compatibility unit test: PASS");
    return 0;
}

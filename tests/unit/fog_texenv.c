#include "mgl/gl.h"
#include "compat/fixed_function.h"
#include "core/context.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_shader;
    GLuint next_program;
    GLenum shader_type[16];
    char vertex_source[4096];
    char fragment_source[8192];
    GLint fog_enabled;
    GLint fog_mode;
    GLfloat fog_density;
    GLfloat fog_start;
    GLfloat fog_end;
    GLfloat fog_color[4];
    GLint texenv;
    GLint fog_coord_explicit;
} cap;

static GLuint fake_create_shader(GLenum type) {
    GLuint id = ++cap.next_shader;
    if (id < 16u)
        cap.shader_type[id] = type;
    return id;
}

static void fake_shader_source(GLuint shader, GLsizei count, const GLchar *const *strings,
                               const GLint *lengths) {
    char *dst =
        cap.shader_type[shader] == GL_VERTEX_SHADER ? cap.vertex_source : cap.fragment_source;
    size_t capacity = cap.shader_type[shader] == GL_VERTEX_SHADER ? sizeof(cap.vertex_source)
                                                                  : sizeof(cap.fragment_source),
           used = 0u;
    GLsizei i;
    dst[0] = '\0';
    for (i = 0; i < count && used + 1u < capacity; ++i) {
        size_t n = lengths != NULL && lengths[i] >= 0 ? (size_t)lengths[i] : strlen(strings[i]);
        if (n > capacity - used - 1u)
            n = capacity - used - 1u;
        memcpy(dst + used, strings[i], n);
        used += n;
        dst[used] = '\0';
    }
}

static void fake_compile_shader(GLuint shader) {
    (void)shader;
}

static void fake_get_shaderiv(GLuint shader, GLenum pname, GLint *v) {
    (void)shader;
    *v = pname == GL_COMPILE_STATUS ? GL_TRUE : 0;
}

static void fake_delete_shader(GLuint shader) {
    (void)shader;
}

static GLuint fake_create_program(void) {
    return ++cap.next_program + 100u;
}

static void fake_attach_shader(GLuint p, GLuint s) {
    (void)p;
    (void)s;
}

static void fake_link_program(GLuint p) {
    (void)p;
}

static void fake_get_programiv(GLuint p, GLenum pname, GLint *v) {
    (void)p;
    *v = pname == GL_LINK_STATUS ? GL_TRUE : 0;
}

static void fake_delete_program(GLuint p) {
    (void)p;
}

static GLint fake_get_attrib_location(GLuint p, const GLchar *n) {
    (void)p;
    if (strcmp(n, "mgl_a_position") == 0)
        return 0;
    if (strcmp(n, "mgl_a_color") == 0)
        return 1;
    if (strcmp(n, "mgl_a_texcoord0") == 0)
        return 2;
    if (strcmp(n, "mgl_a_fog") == 0)
        return 3;
    return -1;
}

static GLint fake_get_uniform_location(GLuint p, const GLchar *n) {
    (void)p;
    if (strcmp(n, "mgl_u_model_view_projection") == 0)
        return 1;
    if (strcmp(n, "mgl_u_model_view") == 0)
        return 2;
    if (strcmp(n, "mgl_u_texture0") == 0)
        return 3;
    if (strcmp(n, "mgl_u_texture_env_mode0") == 0)
        return 10;
    if (strcmp(n, "mgl_u_fog_enabled") == 0)
        return 11;
    if (strcmp(n, "mgl_u_fog_mode") == 0)
        return 12;
    if (strcmp(n, "mgl_u_fog_density") == 0)
        return 13;
    if (strcmp(n, "mgl_u_fog_start") == 0)
        return 14;
    if (strcmp(n, "mgl_u_fog_end") == 0)
        return 15;
    if (strcmp(n, "mgl_u_fog_color") == 0)
        return 16;
    if (strcmp(n, "mgl_u_fog_coord_explicit") == 0)
        return 17;
    return -1;
}

static void fake_uniform1i(GLint l, GLint v) {
    if (l == 10)
        cap.texenv = v;
    else if (l == 11)
        cap.fog_enabled = v;
    else if (l == 12)
        cap.fog_mode = v;
    else if (l == 17)
        cap.fog_coord_explicit = v;
}

static void fake_uniform1f(GLint l, GLfloat v) {
    if (l == 13)
        cap.fog_density = v;
    else if (l == 14)
        cap.fog_start = v;
    else if (l == 15)
        cap.fog_end = v;
}

static void fake_uniform4f(GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
    if (l == 16) {
        cap.fog_color[0] = a;
        cap.fog_color[1] = b;
        cap.fog_color[2] = c;
        cap.fog_color[3] = d;
    }
}

static void fake_uniform_matrix4fv(GLint l, GLsizei c, GLboolean t, const GLfloat *v) {
    (void)l;
    (void)c;
    (void)t;
    (void)v;
}

static int fail(int n, const char *m) {
    fprintf(stderr, "fog/texenv failure %d: %s\n", n, m);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLfloat color[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    GLfloat got[4] = {0};
    GLint mode = 0;
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
    native.glesUniform4f = fake_uniform4f;
    native.glesUniformMatrix4fv = fake_uniform_matrix4fv;
    ctx.native_dispatch = &native;
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 640, 480);
    mgl_compat_state_init(&ctx.compat);
    mgl_context_set_current_for_test(&ctx);

    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, (GLint)GL_EXP2);
    glFogf(GL_FOG_DENSITY, 0.4f);
    glFogf(GL_FOG_START, 2.0f);
    glFogf(GL_FOG_END, 9.0f);
    glFogfv(GL_FOG_COLOR, color);
    glGetIntegerv(GL_FOG_COORD_SRC, &mode);
    if (mode != (GLint)GL_FRAGMENT_DEPTH)
        return fail(12, "default fog coordinate source");
    glFogi(GL_FOG_COORD_SRC, (GLint)GL_FOG_COORD);
    glGetIntegerv(GL_FOG_COORD_SRC, &mode);
    if (mode != (GLint)GL_FOG_COORD)
        return fail(13, "explicit fog coordinate source");
    glFogi(GL_FOG_COORD_SRC, (GLint)GL_TEXTURE_2D);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(14, "invalid fog coordinate source error");
    glGetIntegerv(GL_FOG_COORD_SRC, &mode);
    if (mode != (GLint)GL_FOG_COORD)
        return fail(15, "invalid fog source mutated state");
    if (!glIsEnabled(GL_FOG))
        return fail(1, "fog enable state");
    glGetIntegerv(GL_FOG_MODE, &mode);
    if (mode != (GLint)GL_EXP2)
        return fail(2, "fog mode getter");
    glGetFloatv(GL_FOG_COLOR, got);
    if (fabsf(got[2] - 0.3f) > 0.0001f)
        return fail(3, "fog color getter");

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (GLint)GL_REPLACE);
    glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &mode);
    if (mode != (GLint)GL_REPLACE)
        return fail(4, "texture env state");
    got[0] = -7.0f;
    glGetTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, got);
    if (fabsf(got[0] - (GLfloat)GL_REPLACE) > 0.0001f)
        return fail(10, "float texture env query");
    got[0] = -7.0f;
    glGetTexEnvfv(GL_TEXTURE_ENV, 0xffffffffu, got);
    if (glGetError() != GL_INVALID_ENUM || got[0] != -7.0f)
        return fail(11, "invalid float texture env preserves output");
    program = mgl_compat_fixed_program(&ctx);
    if (program == NULL)
        return fail(5, "fixed program");
    if (strstr(cap.vertex_source, "mgl_ff_fog_coord") == NULL ||
        strstr(cap.vertex_source, "attribute float mgl_a_fog") == NULL ||
        strstr(cap.vertex_source, "mgl_u_fog_coord_explicit!=0?mgl_a_fog:abs(eye.z)") == NULL ||
        strstr(cap.fragment_source, "mgl_apply_fog") == NULL ||
        strstr(cap.fragment_source, "mgl_apply_texenv") == NULL)
        return fail(6, "generated shader coverage");
    mgl_compat_realize_program_uniforms(&ctx, program);
    if (cap.texenv != (GLint)GL_REPLACE || cap.fog_enabled != 1 || cap.fog_mode != (GLint)GL_EXP2 ||
        cap.fog_coord_explicit != 1)
        return fail(7, "integer uniform realization");
    if (fabsf(cap.fog_density - 0.4f) > 0.0001f || fabsf(cap.fog_start - 2.0f) > 0.0001f ||
        fabsf(cap.fog_end - 9.0f) > 0.0001f)
        return fail(8, "fog scalar realization");
    if (fabsf(cap.fog_color[1] - 0.2f) > 0.0001f || fabsf(cap.fog_color[3] - 0.4f) > 0.0001f)
        return fail(9, "fog color realization");

    mgl_compat_destroy_fixed_program(&ctx);
    mgl_context_set_current_for_test(NULL);
    puts("mGL fog/texenv unit test: PASS");
    return 0;
}

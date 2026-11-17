#include "mgl/gl.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_shader;
    GLuint next_program;
    GLenum shader_type[16];
    char vertex_source[16384];
    char fragment_source[4096];
    GLint light_enabled[8];
    GLfloat light_diffuse[32];
    GLfloat light_position[32];
    GLfloat material_diffuse[4];
    GLfloat material_shininess;
    GLfloat light_model_ambient[4];
    GLint color_material_enabled;
    GLint color_material_mode;
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
                                                                  : sizeof(cap.fragment_source);
    size_t used = 0u;
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

static void fake_get_shaderiv(GLuint shader, GLenum pname, GLint *value) {
    (void)shader;
    *value = pname == GL_COMPILE_STATUS ? GL_TRUE : 0;
}

static void fake_delete_shader(GLuint shader) {
    (void)shader;
}

static GLuint fake_create_program(void) {
    return 100u + ++cap.next_program;
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
    if (strcmp(name, "mgl_a_normal") == 0)
        return 1;
    if (strcmp(name, "mgl_a_color") == 0)
        return 2;
    return -1;
}

static GLint fake_get_uniform_location(GLuint program, const GLchar *name) {
    (void)program;
    if (strcmp(name, "mgl_u_model_view_projection") == 0)
        return 1;
    if (strcmp(name, "mgl_u_model_view") == 0)
        return 2;
    if (strcmp(name, "mgl_u_normal_matrix") == 0)
        return 3;
    if (strcmp(name, "mgl_u_light_enabled[0]") == 0)
        return 20;
    if (strcmp(name, "mgl_u_light_ambient[0]") == 0)
        return 21;
    if (strcmp(name, "mgl_u_light_diffuse[0]") == 0)
        return 22;
    if (strcmp(name, "mgl_u_light_specular[0]") == 0)
        return 23;
    if (strcmp(name, "mgl_u_light_position[0]") == 0)
        return 24;
    if (strcmp(name, "mgl_u_light_spot_direction[0]") == 0)
        return 25;
    if (strcmp(name, "mgl_u_light_spot_exponent[0]") == 0)
        return 26;
    if (strcmp(name, "mgl_u_light_spot_cutoff[0]") == 0)
        return 27;
    if (strcmp(name, "mgl_u_light_constant_attenuation[0]") == 0)
        return 28;
    if (strcmp(name, "mgl_u_light_linear_attenuation[0]") == 0)
        return 29;
    if (strcmp(name, "mgl_u_light_quadratic_attenuation[0]") == 0)
        return 30;
    if (strcmp(name, "mgl_u_light_model_ambient") == 0)
        return 31;
    if (strcmp(name, "mgl_u_light_model_local_viewer") == 0)
        return 32;
    if (strcmp(name, "mgl_u_material_ambient") == 0)
        return 33;
    if (strcmp(name, "mgl_u_material_diffuse") == 0)
        return 34;
    if (strcmp(name, "mgl_u_material_specular") == 0)
        return 35;
    if (strcmp(name, "mgl_u_material_emission") == 0)
        return 36;
    if (strcmp(name, "mgl_u_material_shininess") == 0)
        return 37;
    if (strcmp(name, "mgl_u_color_material_enabled") == 0)
        return 38;
    if (strcmp(name, "mgl_u_color_material_mode") == 0)
        return 39;
    if (strcmp(name, "mgl_u_normalize_enabled") == 0)
        return 40;
    return -1;
}

static void fake_uniform1iv(GLint location, GLsizei count, const GLint *value) {
    if (location == 20 && count == 8)
        memcpy(cap.light_enabled, value, 8u * sizeof(GLint));
}

static void fake_uniform1i(GLint location, GLint value) {
    if (location == 38)
        cap.color_material_enabled = value;
    else if (location == 39)
        cap.color_material_mode = value;
}

static void fake_uniform1f(GLint location, GLfloat value) {
    if (location == 37)
        cap.material_shininess = value;
}

static void fake_uniform1fv(GLint location, GLsizei count, const GLfloat *value) {
    (void)location;
    (void)count;
    (void)value;
}

static void fake_uniform3fv(GLint location, GLsizei count, const GLfloat *value) {
    (void)location;
    (void)count;
    (void)value;
}

static void fake_uniform4fv(GLint location, GLsizei count, const GLfloat *value) {
    if (location == 22 && count == 8)
        memcpy(cap.light_diffuse, value, 32u * sizeof(GLfloat));
    else if (location == 24 && count == 8)
        memcpy(cap.light_position, value, 32u * sizeof(GLfloat));
    else if (location == 31 && count == 1)
        memcpy(cap.light_model_ambient, value, 4u * sizeof(GLfloat));
    else if (location == 34 && count == 1)
        memcpy(cap.material_diffuse, value, 4u * sizeof(GLfloat));
}

static void fake_uniform_matrix3fv(GLint location, GLsizei count, GLboolean transpose,
                                   const GLfloat *value) {
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
}

static void fake_uniform_matrix4fv(GLint location, GLsizei count, GLboolean transpose,
                                   const GLfloat *value) {
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
}

static int nearf(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 0.0001f;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "lighting/material failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLfloat v[4] = {0};
    GLfloat diffuse[4] = {0.1f, 0.3f, 0.5f, 0.7f};
    GLfloat ambient_model[4] = {0.05f, 0.1f, 0.15f, 1.0f};
    GLfloat position[4] = {1.0f, 2.0f, 3.0f, 1.0f};
    GLuint list;
    GLint iv = 0;

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
    native.glesUniform1iv = fake_uniform1iv;
    native.glesUniform1i = fake_uniform1i;
    native.glesUniform1f = fake_uniform1f;
    native.glesUniform1fv = fake_uniform1fv;
    native.glesUniform3fv = fake_uniform3fv;
    native.glesUniform4fv = fake_uniform4fv;
    native.glesUniformMatrix3fv = fake_uniform_matrix3fv;
    native.glesUniformMatrix4fv = fake_uniform_matrix4fv;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 320, 240);
    mgl_compat_state_init(&ctx.compat);
    mgl_context_set_current_for_test(&ctx);

    glGetLightfv(GL_LIGHT0, GL_DIFFUSE, v);
    if (!nearf(v[0], 1.0f) || !nearf(v[3], 1.0f))
        return fail(2, "light0 defaults");
    glGetLightfv(GL_LIGHT1, GL_DIFFUSE, v);
    if (!nearf(v[0], 0.0f) || !nearf(v[3], 0.0f))
        return fail(3, "other light defaults");

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
    if (!glIsEnabled(GL_LIGHTING) || !glIsEnabled(GL_LIGHT0) || !glIsEnabled(GL_NORMALIZE))
        return fail(4, "lighting enable state");

    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient_model);
    glLightModelf(GL_LIGHT_MODEL_AMBIENT, 0.5f);
    if (glGetError() != GL_INVALID_ENUM ||
        !nearf(ctx.compat.light_model_ambient[0], ambient_model[0]))
        return fail(23, "scalar light-model ambient cardinality");
    glLightModeli(GL_LIGHT_MODEL_AMBIENT, 1);
    if (glGetError() != GL_INVALID_ENUM ||
        !nearf(ctx.compat.light_model_ambient[0], ambient_model[0]))
        return fail(24, "integer scalar light-model ambient cardinality");
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialf(GL_FRONT, GL_SHININESS, 32.0f);
    glColorMaterial(GL_FRONT, GL_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);
    glShadeModel(GL_FLAT);
    glGetMaterialfv(GL_FRONT, GL_DIFFUSE, v);
    if (!nearf(v[1], 0.3f) || !nearf(v[3], 0.7f))
        return fail(5, "material query");
    glGetIntegerv(GL_SHADE_MODEL, &iv);
    if (iv != (GLint)GL_FLAT)
        return fail(6, "shade model query");
    glGetIntegerv(GL_COLOR_MATERIAL_FACE, &iv);
    if (iv != (GLint)GL_FRONT)
        return fail(21, "color material face query");
    glGetIntegerv(GL_COLOR_MATERIAL_PARAMETER, &iv);
    if (iv != (GLint)GL_DIFFUSE)
        return fail(22, "color material mode query");
    glGetFloatv(GL_LIGHT_MODEL_AMBIENT, v);
    if (!nearf(v[2], 0.15f))
        return fail(7, "light model query");

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(10.0f, 20.0f, 30.0f);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glGetLightfv(GL_LIGHT0, GL_POSITION, v);
    if (!nearf(v[0], 11.0f) || !nearf(v[1], 22.0f) || !nearf(v[2], 33.0f) || !nearf(v[3], 1.0f))
        return fail(8, "position transformed at command execution");

    glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, 100.0f);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(9, "spot validation");
    glMaterialf(GL_FRONT, GL_SHININESS, 129.0f);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(10, "shininess validation");

    program = mgl_compat_fixed_program(&ctx);
    if (program == NULL || strstr(cap.vertex_source, "mgl_lit") == NULL ||
        strstr(cap.vertex_source, "mgl_u_light_enabled") == NULL)
        return fail(11, "generated lighting shader");
    mgl_compat_realize_program_uniforms(&ctx, program);
    if (cap.light_enabled[0] != 1 || !nearf(cap.light_diffuse[1], 0.3f) ||
        !nearf(cap.light_position[0], 11.0f))
        return fail(12, "light uniform realization");
    if (!nearf(cap.material_diffuse[2], 0.5f) || !nearf(cap.material_shininess, 32.0f) ||
        cap.color_material_enabled != 1 || cap.color_material_mode != (GLint)GL_DIFFUSE)
        return fail(13, "material uniform realization");

    /* Display-list light position must transform against modelview at replay time. */
    list = glGenLists(1);
    glLoadIdentity();
    glNewList(list, GL_COMPILE);
    glLightfv(GL_LIGHT1, GL_POSITION, position);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
    glMaterialf(GL_FRONT, GL_SHININESS, 12.0f);
    glShadeModel(GL_SMOOTH);
    glColorMaterial(GL_BACK, GL_SPECULAR);
    glEndList();
    glGetLightfv(GL_LIGHT1, GL_POSITION, v);
    if (!nearf(v[0], 0.0f) || !nearf(v[2], 1.0f))
        return fail(14, "GL_COMPILE executed lighting state");
    glGetIntegerv(GL_COLOR_MATERIAL_FACE, &iv);
    if (iv != (GLint)GL_FRONT)
        return fail(23, "GL_COMPILE executed color material");
    glTranslatef(4.0f, 5.0f, 6.0f);
    glCallList(list);
    glGetLightfv(GL_LIGHT1, GL_POSITION, v);
    if (!nearf(v[0], 5.0f) || !nearf(v[1], 7.0f) || !nearf(v[2], 9.0f))
        return fail(15, "list light replay transform timing");
    glGetLightfv(GL_LIGHT1, GL_DIFFUSE, v);
    if (!nearf(v[2], 0.5f))
        return fail(16, "list light payload");
    glGetMaterialfv(GL_FRONT, GL_SHININESS, v);
    if (!nearf(v[0], 12.0f))
        return fail(17, "list material replay");
    glGetIntegerv(GL_SHADE_MODEL, &iv);
    if (iv != (GLint)GL_SMOOTH)
        return fail(18, "list shade model replay");

    glGetIntegerv(GL_COLOR_MATERIAL_FACE, &iv);
    if (iv != (GLint)GL_BACK)
        return fail(24, "list color material face replay");
    glGetIntegerv(GL_COLOR_MATERIAL_PARAMETER, &iv);
    if (iv != (GLint)GL_SPECULAR)
        return fail(25, "list color material mode replay");

    glPushAttrib(GL_LIGHTING_BIT);
    glDisable(GL_LIGHT0);
    glMaterialf(GL_FRONT, GL_SHININESS, 2.0f);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_FLAT);
    glPopAttrib();
    if (!glIsEnabled(GL_LIGHT0))
        return fail(19, "PushAttrib light enable restore");
    glGetMaterialfv(GL_FRONT, GL_SHININESS, v);
    if (!nearf(v[0], 12.0f))
        return fail(20, "PushAttrib material restore");
    glGetIntegerv(GL_COLOR_MATERIAL_FACE, &iv);
    if (iv != (GLint)GL_BACK)
        return fail(26, "PushAttrib color material restore");
    glGetIntegerv(GL_SHADE_MODEL, &iv);
    if (iv != (GLint)GL_SMOOTH)
        return fail(27, "PushAttrib shade model restore");

    mgl_compat_destroy_fixed_program(&ctx);
    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL lighting/material unit test: PASS");
    return 0;
}

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
    char vertex_source[20000];
    char fragment_source[12000];
    GLint texgen_enabled[32];
    GLint texgen_mode[32];
    GLfloat texgen_object_plane[128];
    GLfloat texgen_eye_plane[128];
    GLint clip_enabled[6];
    GLfloat clip_plane[24];
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
    if (strcmp(name, "mgl_a_texcoord0") == 0)
        return 3;
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
    if (strcmp(name, "mgl_u_texture0") == 0)
        return 4;
    if (strcmp(name, "mgl_u_texture_env_mode0") == 0)
        return 5;
    if (strcmp(name, "mgl_u_implicit_texgen_enabled[0]") == 0)
        return 50;
    if (strcmp(name, "mgl_u_implicit_texgen_mode[0]") == 0)
        return 51;
    if (strcmp(name, "mgl_u_implicit_texgen_object_plane[0]") == 0)
        return 52;
    if (strcmp(name, "mgl_u_implicit_texgen_eye_plane[0]") == 0)
        return 53;
    if (strcmp(name, "mgl_u_clip_enabled[0]") == 0)
        return 54;
    if (strcmp(name, "mgl_u_clip_plane[0]") == 0)
        return 55;
    return -1;
}

static void fake_uniform1i(GLint location, GLint value) {
    (void)location;
    (void)value;
}

static void fake_uniform1iv(GLint location, GLsizei count, const GLint *value) {
    if (location == 50 && count == 32)
        memcpy(cap.texgen_enabled, value, 32u * sizeof(GLint));
    else if (location == 51 && count == 32)
        memcpy(cap.texgen_mode, value, 32u * sizeof(GLint));
    else if (location == 54 && count == 6)
        memcpy(cap.clip_enabled, value, 6u * sizeof(GLint));
}

static void fake_uniform4fv(GLint location, GLsizei count, const GLfloat *value) {
    if (location == 52 && count == 32)
        memcpy(cap.texgen_object_plane, value, 128u * sizeof(GLfloat));
    else if (location == 53 && count == 32)
        memcpy(cap.texgen_eye_plane, value, 128u * sizeof(GLfloat));
    else if (location == 55 && count == 6)
        memcpy(cap.clip_plane, value, 24u * sizeof(GLfloat));
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
    fprintf(stderr, "texgen/clip failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLfloat plane[4] = {2.0f, 3.0f, 4.0f, 5.0f};
    GLfloat got[4] = {0};
    GLdouble clip[4] = {1.0, 0.0, 0.0, 0.0};
    GLdouble dgot[4] = {0};
    GLuint list;

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
    native.glesUniform1iv = fake_uniform1iv;
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

    glActiveTexture(GL_TEXTURE0);
    glGetTexGenfv(GL_S, GL_OBJECT_PLANE, got);
    if (!nearf(got[0], 1.0f) || !nearf(got[1], 0.0f))
        return fail(2, "default S object plane");
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, (GLint)GL_OBJECT_LINEAR);
    glTexGenfv(GL_S, GL_OBJECT_PLANE, plane);
    glEnable(GL_TEXTURE_GEN_S);
    if (!glIsEnabled(GL_TEXTURE_GEN_S))
        return fail(3, "texgen enable");
    glGetTexGenfv(GL_S, GL_OBJECT_PLANE, got);
    if (!nearf(got[2], 4.0f) || !nearf(got[3], 5.0f))
        return fail(4, "object plane query");

    glTexGenf(GL_R, GL_OBJECT_PLANE, 6.0f);
    glGetTexGenfv(GL_R, GL_OBJECT_PLANE, got);
    if (!nearf(got[0], 6.0f) || !nearf(got[1], 0.0f) || !nearf(got[2], 0.0f) ||
        !nearf(got[3], 0.0f))
        return fail(18, "scalar TexGen plane expansion");

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(5.0f, 0.0f, 0.0f);
    glTexGenfv(GL_T, GL_EYE_PLANE, (GLfloat[]){1.0f, 0.0f, 0.0f, 0.0f});
    glGetTexGenfv(GL_T, GL_EYE_PLANE, got);
    if (!nearf(got[0], 1.0f) || !nearf(got[3], -5.0f))
        return fail(5, "eye plane transform timing");

    glClipPlane(GL_CLIP_PLANE0, clip);
    glEnable(GL_CLIP_PLANE0);
    glGetClipPlane(GL_CLIP_PLANE0, dgot);
    if (fabs(dgot[0] - 1.0) > 0.0001 || fabs(dgot[3] + 5.0) > 0.0001)
        return fail(6, "clip plane eye transform");
    glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, (GLint)GL_SPHERE_MAP);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(8, "illegal Q sphere map accepted");

    glEnable(GL_TEXTURE_2D);
    program = mgl_compat_fixed_program(&ctx);
    if (program == NULL || strstr(cap.vertex_source, "mgl_texgen") == NULL ||
        strstr(cap.fragment_source, "if(mgl_clip_reject(mgl_v_eye_position))discard;") == NULL)
        return fail(9, "generated shader texgen/clip coverage");
    mgl_compat_realize_program_uniforms(&ctx, program);
    if (cap.texgen_enabled[0] != 1 || cap.texgen_mode[0] != (GLint)GL_OBJECT_LINEAR ||
        !nearf(cap.texgen_object_plane[2], 4.0f))
        return fail(10, "texgen uniform realization");
    if (cap.clip_enabled[0] != 1 || !nearf(cap.clip_plane[3], -5.0f))
        return fail(11, "clip uniform realization");

    /* Display list must defer eye-space transformation until replay. */
    list = glGenLists(1);
    glLoadIdentity();
    glNewList(list, GL_COMPILE);
    glClipPlane(GL_CLIP_PLANE1, clip);
    glTexGenfv(GL_T, GL_EYE_PLANE, (GLfloat[]){1.0f, 0.0f, 0.0f, 0.0f});
    glEndList();
    glGetClipPlane(GL_CLIP_PLANE1, dgot);
    if (fabs(dgot[0]) > 0.0001 || fabs(dgot[1]) > 0.0001 || fabs(dgot[2]) > 0.0001 ||
        fabs(dgot[3]) > 0.0001)
        return fail(12, "GL_COMPILE executed clip plane");
    glTranslatef(7.0f, 0.0f, 0.0f);
    glCallList(list);
    glGetClipPlane(GL_CLIP_PLANE1, dgot);
    if (fabs(dgot[0] - 1.0) > 0.0001 || fabs(dgot[3] + 7.0) > 0.0001)
        return fail(13, "list clip replay transform");
    glGetTexGenfv(GL_T, GL_EYE_PLANE, got);
    if (!nearf(got[0], 1.0f) || !nearf(got[3], -7.0f))
        return fail(14, "list texgen replay transform");

    glPushAttrib(GL_TEXTURE_BIT | GL_TRANSFORM_BIT | GL_ENABLE_BIT);
    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_CLIP_PLANE0);
    glTexGenfv(GL_S, GL_OBJECT_PLANE, (GLfloat[]){9.0f, 9.0f, 9.0f, 9.0f});
    glClipPlane(GL_CLIP_PLANE0, (GLdouble[]){0.0, 1.0, 0.0, 0.0});
    glPopAttrib();
    if (!glIsEnabled(GL_TEXTURE_GEN_S) || !glIsEnabled(GL_CLIP_PLANE0))
        return fail(15, "attrib enable restore");
    glGetTexGenfv(GL_S, GL_OBJECT_PLANE, got);
    if (!nearf(got[0], 2.0f) || !nearf(got[3], 5.0f))
        return fail(16, "attrib texgen restore");
    glGetClipPlane(GL_CLIP_PLANE0, dgot);
    if (fabs(dgot[0] - 1.0) > 0.0001 || fabs(dgot[3] + 5.0) > 0.0001)
        return fail(17, "attrib clip restore");

    mgl_compat_destroy_fixed_program(&ctx);
    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL texgen/clip unit test: PASS");
    return 0;
}

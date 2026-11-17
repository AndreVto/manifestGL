#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "shader/translator.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int nearf(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 1.0e-5f;
}

static int neard(GLdouble a, GLdouble b) {
    return fabs(a - b) < 1.0e-8;
}

static GLuint next_native_texture = 100u;
static GLenum native_active_texture = GL_TEXTURE0;
static GLenum native_last_target = 0u;
static GLenum native_last_pname = 0u;
static GLint native_last_param = 0;

static void fake_gen_textures(GLsizei n, GLuint *textures) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        textures[i] = next_native_texture++;
}

static void fake_delete_textures(GLsizei n, const GLuint *textures) {
    (void)n;
    (void)textures;
}

static void fake_active_texture(GLenum texture) {
    native_active_texture = texture;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    (void)target;
    (void)texture;
}

static void fake_tex_parameteri(GLenum target, GLenum pname, GLint param) {
    native_last_target = target;
    native_last_pname = pname;
    native_last_param = param;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "EXT DSA MultiTex state failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint list;
    GLuint textures[2] = {0u, 0u};
    GLint env = 0;
    GLint param_i = 0;
    GLfloat param_f = 0.0f;
    GLfloat plane[4] = {2.0f, 3.0f, 4.0f, 5.0f};
    GLfloat got[4] = {0};
    GLdouble dplane[4] = {6.0, 7.0, 8.0, 9.0};
    GLdouble dgot[4] = {0};

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexParameteri = fake_tex_parameteri;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glActiveTexture(GL_TEXTURE1);
    glMultiTexEnviEXT(GL_TEXTURE3, GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (GLint)GL_REPLACE);
    if (ctx.compat.active_texture != 1u || ctx.compat.texture_env_mode[3] != GL_REPLACE ||
        ctx.compat.texture_env_mode[1] != GL_MODULATE)
        return fail(2, "MultiTexEnv selector isolation");
    glGetMultiTexEnvivEXT(GL_TEXTURE3, GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &env);
    if (env != (GLint)GL_REPLACE || ctx.compat.active_texture != 1u)
        return fail(3, "MultiTexEnv query");

    glGenTextures(2, textures);
    if (textures[0] == 0u || textures[1] == 0u)
        return fail(21, "texture generation");
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glActiveTexture(GL_TEXTURE1);

    param_i = (GLint)GL_CLAMP_TO_EDGE;
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &param_i);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &param_i);
    if (param_i != (GLint)GL_CLAMP_TO_EDGE || native_active_texture != GL_TEXTURE1 ||
        native_last_target != GL_TEXTURE_2D || native_last_pname != GL_TEXTURE_WRAP_S ||
        native_last_param != (GLint)GL_CLAMP_TO_EDGE)
        return fail(22, "core vector texture parameter");

    param_i = (GLint)GL_MIRRORED_REPEAT;
    glMultiTexParameteriv(GL_TEXTURE3, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &param_i);
    glGetMultiTexParameteriv(GL_TEXTURE3, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &param_i);
    if (param_i != (GLint)GL_MIRRORED_REPEAT || ctx.compat.active_texture != 1u ||
        native_active_texture != GL_TEXTURE1)
        return fail(23, "selector-free integer texture parameter");
    param_f = (GLfloat)GL_NEAREST;
    glMultiTexParameterfvEXT(GL_TEXTURE3, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &param_f);
    glGetMultiTexParameterfvEXT(GL_TEXTURE3, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &param_f);
    if ((GLint)param_f != (GLint)GL_NEAREST || ctx.compat.active_texture != 1u)
        return fail(24, "EXT alias texture parameter");

    glMultiTexGenfvEXT(GL_TEXTURE4, GL_S, GL_OBJECT_PLANE, plane);
    if (ctx.compat.active_texture != 1u)
        return fail(4, "MultiTexGen disturbed active texture");
    glGetMultiTexGenfvEXT(GL_TEXTURE4, GL_S, GL_OBJECT_PLANE, got);
    if (!nearf(got[0], 2.0f) || !nearf(got[3], 5.0f))
        return fail(5, "MultiTexGen float query");

    glMultiTexGenfEXT(GL_TEXTURE4, GL_Q, GL_OBJECT_PLANE, 11.0f);
    glGetMultiTexGenfvEXT(GL_TEXTURE4, GL_Q, GL_OBJECT_PLANE, got);
    if (!nearf(got[0], 11.0f) || !nearf(got[1], 0.0f) || !nearf(got[2], 0.0f) ||
        !nearf(got[3], 0.0f))
        return fail(29, "scalar MultiTexGen plane expansion");

    glActiveTexture(GL_TEXTURE2);
    glTexGendv(GL_T, GL_OBJECT_PLANE, dplane);
    glGetTexGendv(GL_T, GL_OBJECT_PLANE, dgot);
    if (!neard(dgot[0], 6.0) || !neard(dgot[3], 9.0))
        return fail(6, "core double TexGen family");

    glMultiTexGendvEXT(GL_TEXTURE5, GL_R, GL_OBJECT_PLANE, dplane);
    memset(dgot, 0, sizeof(dgot));
    glGetMultiTexGendvEXT(GL_TEXTURE5, GL_R, GL_OBJECT_PLANE, dgot);
    if (!neard(dgot[1], 7.0) || !neard(dgot[2], 8.0))
        return fail(7, "MultiTexGen double family");

    glMultiTexEnviEXT(GL_TEXTURE7 + 1u, GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (GLint)GL_MODULATE);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(8, "out-of-range unit error");
    glMultiTexEnviEXT(GL_TEXTURE_2D, GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (GLint)GL_MODULATE);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(9, "non-unit selector error");

    glMultiTexParameteri(GL_TEXTURE7 + 1u, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)GL_REPEAT);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(25, "MultiTexParameter out-of-range unit error");
    glMultiTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)GL_REPEAT);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(26, "MultiTexParameter non-unit selector error");

    list = glGenLists(1);
    if (list == 0u)
        return fail(10, "glGenLists");
    glMultiTexEnviEXT(GL_TEXTURE6, GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (GLint)GL_MODULATE);
    glMultiTexGenfvEXT(GL_TEXTURE6, GL_T, GL_EYE_PLANE, (GLfloat[]){0.0f, 1.0f, 0.0f, 0.0f});
    glMultiTexParameteri(GL_TEXTURE3, GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, (GLint)GL_REPEAT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glNewList(list, GL_COMPILE);
    glMultiTexEnviEXT(GL_TEXTURE6, GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (GLint)GL_DECAL);
    glMultiTexGenfvEXT(GL_TEXTURE6, GL_T, GL_EYE_PLANE, (GLfloat[]){1.0f, 0.0f, 0.0f, 0.0f});
    glMultiTexParameteri(GL_TEXTURE3, GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, (GLint)GL_CLAMP_TO_EDGE);
    glEndList();
    if (ctx.compat.texture_env_mode[6] != GL_MODULATE)
        return fail(11, "GL_COMPILE executed MultiTexEnv");
    glGetMultiTexParameteriv(GL_TEXTURE3, GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, &param_i);
    if (param_i != (GLint)GL_REPEAT)
        return fail(27, "GL_COMPILE executed MultiTexParameter");
    glGetMultiTexGenfvEXT(GL_TEXTURE6, GL_T, GL_EYE_PLANE, got);
    if (!nearf(got[0], 0.0f) || !nearf(got[1], 1.0f) || !nearf(got[3], 0.0f))
        return fail(12, "GL_COMPILE executed MultiTexGen");

    glTranslatef(9.0f, 0.0f, 0.0f);
    glActiveTexture(GL_TEXTURE2);
    glCallList(list);
    if (ctx.compat.active_texture != 2u || ctx.compat.texture_env_mode[6] != GL_DECAL)
        return fail(13, "list selector isolation/unit capture");
    glGetMultiTexParameteriv(GL_TEXTURE3, GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, &param_i);
    if (param_i != (GLint)GL_CLAMP_TO_EDGE || ctx.compat.active_texture != 2u)
        return fail(28, "list MultiTexParameter replay/selector isolation");
    glGetMultiTexGenfvEXT(GL_TEXTURE6, GL_T, GL_EYE_PLANE, got);
    if (!nearf(got[0], 1.0f) || !nearf(got[3], -9.0f))
        return fail(14, "MultiTexGen replay-time eye transform");

    {
        GLfloat coords_a[8] = {0};
        GLfloat coords_b[8] = {0};
        struct mgl_vertex_binding *unit6 = &ctx.current_vao->compat[MGL_COMPAT_SEM_TEXCOORD0 + 6u];
        ctx.client_active_texture = 1u;
        glMultiTexCoordPointerEXT(GL_TEXTURE6, 2, GL_FLOAT, 0, coords_a);
        if (ctx.client_active_texture != 1u || unit6->data.client_ptr != coords_a ||
            unit6->size != 2)
            return fail(15, "MultiTexCoordPointer selector isolation");

        glNewList(list, GL_COMPILE);
        glMultiTexCoordPointerEXT(GL_TEXTURE6, 3, GL_FLOAT, 0, coords_b);
        glEndList();
        if (unit6->data.client_ptr != coords_b || unit6->size != 3)
            return fail(16, "MultiTexCoordPointer did not execute during list compilation");
        glMultiTexCoordPointerEXT(GL_TEXTURE6, 2, GL_FLOAT, 0, coords_a);
        glCallList(list);
        if (unit6->data.client_ptr != coords_a || unit6->size != 2)
            return fail(17, "MultiTexCoordPointer was incorrectly retained");

        glMultiTexCoordPointerEXT(GL_TEXTURE7 + 1u, 2, GL_FLOAT, 0, coords_a);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
            return fail(18, "MultiTexCoordPointer out-of-range unit error");
    }

    if (glXGetProcAddress((const GLubyte *)"glMultiTexEnvfEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexGendvEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetMultiTexGenfvEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexCoordPointerEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTexGendv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTexParameteriv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTexParameterfv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexParameteri") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexParameteriEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetMultiTexParameteriv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetMultiTexParameterivEXT") == NULL)
        return fail(19, "proc-address exposure");

    glDeleteLists(list, 1);
    glDeleteTextures(2, textures);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(20, "unexpected GL error");
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL EXT DSA MultiTex state unit test: PASS");
    return 0;
}

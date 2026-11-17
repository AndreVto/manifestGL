#include "mgl/gl.h"
#include "core/context.h"
#include "compat/fixed_function.h"
#include "shader/shader_store.h"
#include "texture/texture_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    GLuint id;
    GLenum type;
    int attached;
} shaders[32];

static size_t shader_count;
static GLuint next_shader = 100u, next_program = 200u;
static GLint linked;
static GLuint bound;
static unsigned link_count, temporary_deleted;
static unsigned lighting_uploads, matrix_uploads, texgen_uploads;
static GLint lighting_value, texgen_unit2_enabled, two_sided_value, color_material_face_value;
static unsigned two_sided_uploads, back_material_uploads, color_material_face_uploads;
static GLfloat back_diffuse_last[4];
static GLfloat matrix2_last[16];
static unsigned env_uploads, env_color_uploads, env_scale_uploads;
static GLint env_unit2_enabled, env_unit2_mode, env_unit2_combine, env_unit2_source;
static GLfloat env_unit2_color[4], env_unit2_scale, env_unit2_bias, env_unit3_bias;
static unsigned bias_uploads;
static unsigned rect_scale_uploads;
static GLfloat unit2_rect_scale[2];
static char temporary_source[16384];

static GLuint fake_shader_create(GLenum type) {
    GLuint id = next_shader++;
    if (shader_count >= 32u)
        return 0u;
    shaders[shader_count].id = id;
    shaders[shader_count].type = type;
    shaders[shader_count].attached = 0;
    ++shader_count;
    return id;
}

static GLuint fake_program_create(void) {
    return next_program++;
}

static void fake_source(GLuint id, GLsizei count, const GLchar *const *src, const GLint *len) {
    (void)len;
    if (id >= 100u && src != NULL && count > 0) {
        size_t written = 0u;
        GLsizei i;
        temporary_source[0] = '\0';
        for (i = 0; i < count && src[i] != NULL; ++i) {
            size_t length = strlen(src[i]);
            if (length >= sizeof(temporary_source) - written)
                break;
            memcpy(temporary_source + written, src[i], length + 1u);
            written += length;
        }
    }
}

static void fake_compile(GLuint id) {
    (void)id;
}

static void fake_shader_iv(GLuint id, GLenum pname, GLint *out) {
    (void)id;
    *out = pname == GL_INFO_LOG_LENGTH ? 1 : GL_TRUE;
}

static void fake_program_iv(GLuint id, GLenum pname, GLint *out) {
    (void)id;
    switch (pname) {
        case GL_LINK_STATUS:
            *out = linked;
            break;
        case GL_INFO_LOG_LENGTH:
            *out = 1;
            break;
        case GL_ACTIVE_ATTRIBUTES:
            *out = 0;
            break;
        case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
            *out = 0;
            break;
        default:
            *out = 0;
            break;
    }
}

static void fake_attach(GLuint program, GLuint shader) {
    size_t i;
    (void)program;
    for (i = 0; i < shader_count; ++i)
        if (shaders[i].id == shader)
            shaders[i].attached = 1;
}

static void fake_detach(GLuint program, GLuint shader) {
    size_t i;
    (void)program;
    for (i = 0; i < shader_count; ++i)
        if (shaders[i].id == shader)
            shaders[i].attached = 0;
}

static void fake_delete_shader(GLuint shader) {
    if (shader >= 102u)
        ++temporary_deleted;
}

static void fake_link(GLuint program) {
    unsigned v = 0u, f = 0u;
    size_t i;
    (void)program;
    ++link_count;
    for (i = 0; i < shader_count; ++i)
        if (shaders[i].attached) {
            v += shaders[i].type == GL_VERTEX_SHADER;
            f += shaders[i].type == GL_FRAGMENT_SHADER;
        }
    linked = v > 0u && f > 0u;
}

static void fake_use(GLuint program) {
    bound = program;
}

static GLint fake_uniform(GLuint p, const GLchar *name) {
    (void)p;
    if (strcmp(name, "mgl_u_rect_scale[2]") == 0)
        return 72;
    if (strcmp(name, "mgl_u_rect_scale[0]") == 0)
        return 73;
    if (strcmp(name, "mgl_u_env_enabled[0]") == 0)
        return 50;
    if (strcmp(name, "mgl_u_env_mode[0]") == 0)
        return 51;
    if (strcmp(name, "mgl_u_env_rgb_mode[0]") == 0)
        return 52;
    if (strcmp(name, "mgl_u_env_rgb_source[0]") == 0)
        return 53;
    if (strcmp(name, "mgl_u_env_color[0]") == 0)
        return 54;
    if (strcmp(name, "mgl_u_env_rgb_scale[0]") == 0)
        return 55;
    if (strcmp(name, "mgl_u_env_lod_bias[0]") == 0)
        return 56;
    if (strcmp(name, "mgl_u_lighting_enabled") == 0)
        return 30;
    if (strcmp(name, "mgl_u_light_model_two_side") == 0)
        return 35;
    if (strcmp(name, "mgl_u_material_back_diffuse") == 0)
        return 36;
    if (strcmp(name, "mgl_u_color_material_face") == 0)
        return 37;
    if (strcmp(name, "mgl_u_texture_matrix[2]") == 0)
        return 32;
    if (strcmp(name, "mgl_u_implicit_texgen_enabled[0]") == 0)
        return 42;
    return -1;
}

static void fake_uniform1i(GLint location, GLint value) {
    if (location == 30) {
        ++lighting_uploads;
        lighting_value = value;
    }
    if (location == 35) {
        ++two_sided_uploads;
        two_sided_value = value;
    }
    if (location == 37) {
        ++color_material_face_uploads;
        color_material_face_value = value;
    }
}

static void fake_uniform1iv(GLint location, GLsizei count, const GLint *values) {
    if (count == 8 && location >= 50 && location <= 52) {
        ++env_uploads;
        if (location == 50)
            env_unit2_enabled = values[2];
        if (location == 51)
            env_unit2_mode = values[2];
        if (location == 52)
            env_unit2_combine = values[2];
    }
    if (location == 53 && count == 24) {
        ++env_uploads;
        env_unit2_source = values[2 * 3 + 2];
    }
    if (location == 42 && count == 32) {
        ++texgen_uploads;
        texgen_unit2_enabled = values[8];
    }
}

static void fake_uniform4fv(GLint location, GLsizei count, const GLfloat *values) {
    if (location == 54 && count == 8) {
        ++env_color_uploads;
        memcpy(env_unit2_color, values + 8, sizeof(env_unit2_color));
    }
    if (location == 36 && count == 1) {
        ++back_material_uploads;
        memcpy(back_diffuse_last, values, sizeof(back_diffuse_last));
    }
}

static void fake_uniform2f(GLint location, GLfloat x, GLfloat y) {
    if (location == 72) {
        ++rect_scale_uploads;
        unit2_rect_scale[0] = x;
        unit2_rect_scale[1] = y;
    }
}

static void fake_uniform1fv(GLint location, GLsizei count, const GLfloat *values) {
    if (location == 55 && count == 8) {
        ++env_scale_uploads;
        env_unit2_scale = values[2];
    }
    if (location == 56 && count == 8) {
        ++bias_uploads;
        env_unit2_bias = values[2];
        env_unit3_bias = values[3];
    }
}

static void fake_uniform_matrix4(GLint location, GLsizei count, GLboolean transpose,
                                 const GLfloat *value) {
    if (location == 32 && count == 1 && !transpose) {
        ++matrix_uploads;
        memcpy(matrix2_last, value, sizeof(matrix2_last));
    }
}

static GLint fake_attrib(GLuint p, const GLchar *name) {
    (void)p;
    (void)name;
    return -1;
}

static void fake_program_log(GLuint p, GLsizei size, GLsizei *n, GLchar *s) {
    (void)p;
    (void)size;
    if (n != NULL)
        *n = 0;
    if (s != NULL)
        s[0] = 0;
}

static char *dup_string(const char *s) {
    size_t n = strlen(s) + 1u;
    char *p = malloc(n);
    if (p)
        memcpy(p, s, n);
    return p;
}

static int fail(const char *message) {
    fprintf(stderr, "implicit stage test: %s\n", message);
    return 1;
}

static int try_stage(struct mgl_context *ctx, GLenum type) {
    GLuint shader = glCreateShader(type);
    GLuint program = glCreateProgram();
    GLint status = 0;
    GLsizei num_attached = -1;
    GLuint names[2] = {0, 0};
    struct mgl_shader *obj;
    unsigned links_before = link_count;
    if (shader == 0u || program == 0u)
        return fail("object creation");
    obj = mgl_shader_find(ctx, shader);
    if (obj == NULL)
        return fail("shader lookup");
    obj->compiled = GL_TRUE;
    obj->translated_source = dup_string(
        type == GL_VERTEX_SHADER ? "#version 300 es\nout vec4 mgl_v_front_color; out vec4 "
                                   "mgl_v_back_color; out vec4 mgl_v_texcoord[8];\nvoid main(){}"
                                 : "#version 300 es\nin vec4 mgl_v_front_color; in vec4 "
                                   "mgl_v_texcoord[8];\nvoid main(){}");
    if (obj->translated_source == NULL)
        return fail("translation fixture");
    glAttachShader(program, shader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (glGetError() != GL_NO_ERROR || !status || link_count != links_before + 1u)
        return fail("valid desktop single-stage program did not link");
    if (type == GL_VERTEX_SHADER) {
        if (strstr(temporary_source, "mgl_u_env_enabled[8]") == NULL ||
            strstr(temporary_source, "mgl_u_env_rgb_source[24]") == NULL ||
            strstr(temporary_source, "texture(mgl_u_texture2,(mgl_v_texcoord[2].xy)*mgl_u_rect_"
                                     "scale[2],mgl_u_env_lod_bias[2])") == NULL ||
            strstr(temporary_source, "mgl_ff_combine(") == NULL ||
            strstr(temporary_source, "mgl_u_texture7") == NULL ||
            strstr(temporary_source, "mgl_v_front_color") == NULL ||
            strstr(temporary_source, "gl_FrontFacing ? mgl_v_front_color : mgl_v_back_color") ==
                NULL)
            return fail("fixed-function fragment stage missing state or interface");
    } else if (strstr(temporary_source, "mgl_u_model_view_projection") == NULL ||
               strstr(temporary_source, "mgl_v_texcoord[8]") == NULL ||
               strstr(temporary_source, "mgl_u_texture_matrix[2] * mgl_texgen_unit(2") == NULL ||
               strstr(temporary_source, "mgl_texgen_unit(") == NULL ||
               strstr(temporary_source, "mgl_texgen_unit(2,") == NULL ||
               strstr(temporary_source, "mgl_u_lighting_enabled != 0") == NULL ||
               strstr(temporary_source, "mgl_u_fog_coord_explicit != 0") == NULL ||
               strstr(temporary_source, "mgl_lit(eye,-mgl_a_normal.xyz,true)") == NULL ||
               strstr(temporary_source, "mgl_u_material_back_diffuse") == NULL ||
               strstr(temporary_source, "mgl_u_light_model_two_side") == NULL ||
               strstr(temporary_source, "mgl_u_color_material_face") == NULL)
        return fail("fixed-function vertex stage missing matrix, texgen, lighting or fog behavior");
    glGetAttachedShaders(program, 2, &num_attached, names);
    if (glGetError() != GL_NO_ERROR || num_attached != 1 || names[0] != shader)
        return fail("temporary shader leaked into desktop attachments");
    glUseProgram(program);
    if (glGetError() != GL_NO_ERROR || bound != mgl_program_find(ctx, program)->native_name)
        return fail("program was not usable after successful link");
    if (type == GL_VERTEX_SHADER) {
        struct mgl_program *linked_program = mgl_program_find(ctx, program);
        GLfloat color[4] = {0.25f, 0.5f, 0.75f, 1.0f}, read[4];
        GLint mode, source;
        unsigned uploads_before = env_uploads;
        if (linked_program->compat_uniform_rect_scale[2] != 72)
            return fail("implicit fragment rectangle scale not reflected");
        mgl_compat_realize_program_uniforms(ctx, linked_program);
        if (rect_scale_uploads != 1 || unit2_rect_scale[0] != 1.0f || unit2_rect_scale[1] != 1.0f)
            return fail("implicit fragment 2D coordinate scale not uploaded");
        if (env_uploads != uploads_before + 4 || env_unit2_enabled != 0 ||
            env_unit2_mode != GL_MODULATE || env_unit2_combine != GL_MODULATE)
            return fail("implicit fragment initial combiner uniforms");
        glActiveTexture(GL_TEXTURE2);
        glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_TEXTURE1);
        glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
        glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, 0.5f);
        glMultiTexEnvfEXT(GL_TEXTURE3, GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, -1.25f);
        glGetTexEnvfv(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, read);
        if (read[0] != 0.5f)
            return fail("Mesa filter-control LOD bias roundtrip");
        glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_ENV_MODE, 1.0f);
        if (glGetError() != GL_INVALID_ENUM)
            return fail("invalid filter-control pname");
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_LOD_BIAS, 1.0f);
        if (glGetError() != GL_INVALID_ENUM)
            return fail("wrong LOD bias target");
        glGetTexEnviv(GL_TEXTURE_ENV, GL_SOURCE2_RGB, &source);
        glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &mode);
        glGetTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, read);
        if (glGetError() != GL_NO_ERROR || mode != GL_COMBINE || source != GL_TEXTURE1 ||
            read[1] != color[1])
            return fail("texture environment API state");
        mgl_compat_realize_program_uniforms(ctx, linked_program);
        if (env_unit2_enabled != 1 || env_unit2_mode != GL_COMBINE ||
            env_unit2_combine != GL_INTERPOLATE || env_unit2_source != GL_TEXTURE1 ||
            env_unit2_color[1] != 0.5f || env_unit2_scale != 2.0f || env_unit2_bias != 0.5f ||
            env_unit3_bias != -1.25f)
            return fail("implicit fragment unit-2 combiner uniforms");
        uploads_before = env_uploads;
        {
            unsigned bias_before = bias_uploads;
            mgl_compat_realize_program_uniforms(ctx, linked_program);
            if (bias_uploads != bias_before)
                return fail("unchanged LOD bias uploaded redundantly");
            glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, 1.5f);
            mgl_compat_realize_program_uniforms(ctx, linked_program);
            if (bias_uploads != bias_before + 1 || env_unit2_bias != 1.5f)
                return fail("filter-control-only change not synchronized");
            {
                struct mgl_texture *tex = mgl_texture_ensure(ctx, 100002u, GL_TEXTURE_2D, 0);
                if (tex == NULL)
                    return fail("bias texture creation");
                tex->lod_bias = -0.75f;
                ctx->texture_2d_binding[2] = 100002u;
                mgl_compat_realize_program_uniforms(ctx, linked_program);
                if (bias_uploads != bias_before + 2 || env_unit2_bias != 0.75f)
                    return fail("texture-object LOD bias not combined on binding change");
                tex->lod_bias = 0.25f;
                mgl_compat_realize_program_uniforms(ctx, linked_program);
                if (bias_uploads != bias_before + 3 || env_unit2_bias != 1.75f)
                    return fail("texture-object LOD bias change not synchronized");
                ctx->texture_2d_binding[2] = 0u;
            }
        }
        if (env_uploads != uploads_before)
            return fail("unchanged combiner state uploaded redundantly");
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_COLOR);
        if (glGetError() != GL_INVALID_ENUM)
            return fail("invalid alpha operand validation");
        {
            GLuint list = glGenLists(1);
            const GLfloat changed_color[4] = {0.1f, 0.2f, 0.3f, 0.4f};
            if (list == 0u)
                return fail("texture environment display-list allocation");
            glNewList(list, GL_COMPILE);
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, changed_color);
            glEndList();
            glGetTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, read);
            if (glGetError() != GL_NO_ERROR || read[1] != 0.5f)
                return fail("compiled environment color applied too early");
            glCallList(list);
            glGetTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, read);
            if (glGetError() != GL_NO_ERROR || read[1] != changed_color[1])
                return fail("four-component environment color lost on display-list replay");
            glDeleteLists(list, 1);
        }
        glActiveTexture(GL_TEXTURE0);
    }
    if (type == GL_FRAGMENT_SHADER) {
        struct mgl_program *linked_program = mgl_program_find(ctx, program);
        unsigned lighting_before = lighting_uploads;
        unsigned matrices_before = matrix_uploads;
        unsigned texgen_before = texgen_uploads;
        unsigned sides_before = two_sided_uploads;
        unsigned back_before = back_material_uploads;
        unsigned cm_face_before = color_material_face_uploads;
        mgl_compat_realize_program_uniforms(ctx, linked_program);
        if (lighting_uploads != lighting_before + 1u || lighting_value != 0 ||
            matrix_uploads != matrices_before + 1u || matrix2_last[0] != 1.0f ||
            texgen_uploads != texgen_before + 1u || texgen_unit2_enabled != 0 ||
            two_sided_uploads != sides_before + 1u || two_sided_value != 0 ||
            back_material_uploads != back_before + 1u || back_diffuse_last[0] != 0.8f ||
            color_material_face_uploads != cm_face_before + 1u ||
            color_material_face_value != (GLint)GL_FRONT_AND_BACK)
            return fail("implicit vertex initial fixed-function state was not uploaded");
        ctx->compat.lighting_enabled = GL_TRUE;
        ctx->compat.light_model_two_side = GL_TRUE;
        ctx->compat.material_back.diffuse[0] = 0.4f;
        ctx->compat.color_material_face = GL_BACK;
        ctx->compat.texture[2][0][12] = 0.625f;
        ctx->compat.texgen[2][0].enabled = GL_TRUE;
        mgl_compat_realize_program_uniforms(ctx, linked_program);
        if (lighting_value != 1 || lighting_uploads != lighting_before + 2u ||
            matrix_uploads != matrices_before + 2u || matrix2_last[12] != 0.625f ||
            texgen_uploads != texgen_before + 2u || texgen_unit2_enabled != 1 ||
            two_sided_uploads != sides_before + 2u || two_sided_value != 1 ||
            back_material_uploads != back_before + 1u + 1u || back_diffuse_last[0] != 0.4f ||
            color_material_face_uploads != cm_face_before + 2u ||
            color_material_face_value != (GLint)GL_BACK)
            return fail("implicit vertex lighting or texture matrix changes not applied");
        mgl_compat_realize_program_uniforms(ctx, linked_program);
        if (lighting_uploads != lighting_before + 2u || matrix_uploads != matrices_before + 2u ||
            texgen_uploads != texgen_before + 2u || two_sided_uploads != sides_before + 2u ||
            back_material_uploads != back_before + 2u ||
            color_material_face_uploads != cm_face_before + 2u)
            return fail("unchanged implicit state uploaded redundantly");
        ctx->compat.lighting_enabled = GL_FALSE;
        ctx->compat.light_model_two_side = GL_FALSE;
        ctx->compat.material_back.diffuse[0] = 0.8f;
        ctx->compat.color_material_face = GL_FRONT_AND_BACK;
        ctx->compat.texture[2][0][12] = 0.0f;
        ctx->compat.texgen[2][0].enabled = GL_FALSE;
    }
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (glGetError() != GL_NO_ERROR || !status)
        return fail("repeat link did not work");
    glUseProgram(0);
    glDeleteProgram(program);
    glDeleteShader(shader);
    if (glGetError() != GL_NO_ERROR)
        return fail("cleanup error");
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesCreateShader = fake_shader_create;
    native.glesCreateProgram = fake_program_create;
    native.glesShaderSource = fake_source;
    native.glesCompileShader = fake_compile;
    native.glesGetShaderiv = fake_shader_iv;
    native.glesGetProgramiv = fake_program_iv;
    native.glesAttachShader = fake_attach;
    native.glesDetachShader = fake_detach;
    native.glesDeleteShader = fake_delete_shader;
    native.glesLinkProgram = fake_link;
    native.glesUseProgram = fake_use;
    native.glesGetProgramInfoLog = fake_program_log;
    native.glesGetAttribLocation = fake_attrib;
    native.glesGetUniformLocation = fake_uniform;
    native.glesUniform2f = fake_uniform2f;
    native.glesUniform1i = fake_uniform1i;
    native.glesUniform1iv = fake_uniform1iv;
    native.glesUniform1fv = fake_uniform1fv;
    native.glesUniformMatrix4fv = fake_uniform_matrix4;
    native.glesUniform4fv = fake_uniform4fv;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail("share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_context_set_current_for_test(&ctx);
    if (try_stage(&ctx, GL_VERTEX_SHADER))
        return 1;
    if (try_stage(&ctx, GL_FRAGMENT_SHADER))
        return 1;
    if (temporary_deleted != 4u)
        return fail("temporary stages were not cleaned up");
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

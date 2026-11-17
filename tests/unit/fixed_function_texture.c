#include "mgl/gl.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "shader/translator.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_shader;
    GLuint next_program;
    GLenum shader_type[128];
    char vertex_source[24576];
    char fragment_source[24576];
    char linked_fragment_sources[256][24576];
    GLint sampler_location;
    GLint sampler_value;
    unsigned delete_program_count;
    unsigned shader_compilation_count;
    unsigned link_count;
    GLint enabled[8];
    GLint source[24];
    GLint sampler_unit[8];
    GLint matrix_updates;
    unsigned full_stage_compile_count;
    GLenum uniform_error;
    GLboolean inject_uniform_error;
    unsigned lod_bias_uploads;
    unsigned invalid_lod_bias_uploads;
    GLfloat lod_bias_values[8];
    GLfloat rect_scale[8][2];
    unsigned rect_scale_updates[8];
    GLuint native_2d_texture;
    GLuint native_3d_texture;
    GLuint native_cube_texture;
    unsigned native_active_unit;
    unsigned native_2d_bind_count;
} cap;

static GLuint fake_create_shader(GLenum type) {
    GLuint id = ++cap.next_shader;
    if (id < 128u)
        cap.shader_type[id] = type;
    return id;
}

static void fake_shader_source(GLuint shader, GLsizei count, const GLchar *const *strings,
                               const GLint *lengths) {
    char *dst =
        cap.shader_type[shader] == GL_VERTEX_SHADER ? cap.vertex_source : cap.fragment_source;
    size_t cap_size = 24576u, used = 0u;
    GLsizei i;
    dst[0] = '\0';
    for (i = 0; i < count && used + 1u < cap_size; ++i) {
        size_t n = lengths != NULL && lengths[i] >= 0 ? (size_t)lengths[i] : strlen(strings[i]);
        if (n > cap_size - used - 1u)
            n = cap_size - used - 1u;
        memcpy(dst + used, strings[i], n);
        used += n;
        dst[used] = '\0';
    }
}

static void fake_compile_shader(GLuint shader) {
    (void)shader;
    ++cap.shader_compilation_count;
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
    if (program >= 100u && program - 100u < 256u)
        memcpy(cap.linked_fragment_sources[program - 100u], cap.fragment_source,
               sizeof(cap.fragment_source));
    ++cap.link_count;
}

static const char *fixed_fragment_source(GLuint program) {
    return program >= 100u && program - 100u < 256u ? cap.linked_fragment_sources[program - 100u]
                                                    : "";
}

static void fake_get_programiv(GLuint program, GLenum pname, GLint *value) {
    (void)program;
    *value = pname == GL_LINK_STATUS ? GL_TRUE : 0;
}

static void fake_delete_program(GLuint program) {
    (void)program;
    ++cap.delete_program_count;
}

static GLint fake_get_attrib_location(GLuint program, const GLchar *name) {
    (void)program;
    if (strcmp(name, "mgl_a_position") == 0)
        return 0;
    if (strcmp(name, "mgl_a_color") == 0)
        return 1;
    if (strcmp(name, "mgl_a_texcoord0") == 0)
        return 2;
    if (strcmp(name, "mgl_a_texcoord2") == 0)
        return 5;
    return -1;
}

static GLint fake_get_uniform_location(GLuint program, const GLchar *name) {
    if (strcmp(name, "mgl_u_model_view_projection") == 0)
        return 3;
    if (strcmp(name, "mgl_u_texture0") == 0)
        return 4;
    if (strcmp(name, "mgl_u_rect_scale0") == 0)
        return 40;
    if (strcmp(name, "mgl_u_rect_scale[0]") == 0)
        return 41;
    if (strcmp(name, "mgl_u_rect_scale[2]") == 0)
        return 42;
    if (strcmp(name, "mgl_u_texture2") == 0)
        return 12;
    if (strcmp(name, "mgl_u_env_lod_bias[0]") == 0)
        return strstr(fixed_fragment_source(program), "uniform float mgl_u_env_lod_bias[8]") != NULL
                   ? 30
                   : -1;
    if (strcmp(name, "mgl_u_env_enabled[0]") == 0)
        return 20;
    if (strcmp(name, "mgl_u_env_rgb_source[0]") == 0)
        return 21;
    if (strcmp(name, "mgl_u_texture_matrix[2]") == 0)
        return 22;
    return -1;
}

static void fake_uniform1i(GLint location, GLint value) {
    cap.sampler_location = location;
    cap.sampler_value = value;
    if (location == 4) {
        cap.sampler_unit[0] = value;
        if (cap.inject_uniform_error)
            cap.uniform_error = GL_INVALID_OPERATION;
    }
    if (location == 12)
        cap.sampler_unit[2] = value;
}

static void fake_uniform2f(GLint location, GLfloat x, GLfloat y) {
    unsigned index = location == 40 || location == 41 ? 0u : 2u;
    cap.rect_scale[index][0] = x;
    cap.rect_scale[index][1] = y;
    ++cap.rect_scale_updates[index];
}

static void fake_active_texture(GLenum texture) {
    cap.native_active_unit = texture - GL_TEXTURE0;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    if (cap.native_active_unit == 0u) {
        if (target == GL_TEXTURE_2D) {
            cap.native_2d_texture = texture;
            ++cap.native_2d_bind_count;
        } else if (target == GL_TEXTURE_3D)
            cap.native_3d_texture = texture;
        else if (target == GL_TEXTURE_CUBE_MAP)
            cap.native_cube_texture = texture;
    }
}

static void fake_bind_sampler(GLuint unit, GLuint sampler) {
    (void)unit;
    (void)sampler;
}

static GLenum fake_get_error(void) {
    GLenum error = cap.uniform_error;
    cap.uniform_error = GL_NO_ERROR;
    return error;
}

static void fake_uniform1iv(GLint location, GLsizei n, const GLint *values) {
    if (location == 20 && n == 8)
        memcpy(cap.enabled, values, 8u * sizeof(GLint));
    if (location == 21 && n == 24)
        memcpy(cap.source, values, 24u * sizeof(GLint));
}

static void fake_uniform4fv(GLint location, GLsizei n, const GLfloat *values) {
    (void)location;
    (void)n;
    (void)values;
}

static void fake_uniform1fv(GLint location, GLsizei n, const GLfloat *values) {
    if (location == 0 && n == 8) {
        ++cap.invalid_lod_bias_uploads;
        cap.uniform_error = GL_INVALID_OPERATION;
    }
    if (location == 30 && n == 8 && values != NULL) {
        ++cap.lod_bias_uploads;
        memcpy(cap.lod_bias_values, values, sizeof(cap.lod_bias_values));
    }
}

static void fake_uniform_matrix4fv(GLint location, GLsizei n, GLboolean transpose,
                                   const GLfloat *values) {
    if (location == 22 && n == 1 && transpose == GL_FALSE && values != NULL)
        ++cap.matrix_updates;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "fixed-function texture failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    struct mgl_share_group share;
    struct mgl_texture rectangle;
    struct mgl_texture texture_2d;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    memset(&share, 0, sizeof(share));
    memset(&rectangle, 0, sizeof(rectangle));
    memset(&texture_2d, 0, sizeof(texture_2d));
    ctx.share_group = &share;
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
    native.glesUniform2f = fake_uniform2f;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesBindSampler = fake_bind_sampler;
    native.glesGetError = fake_get_error;
    native.glesUniform1iv = fake_uniform1iv;
    native.glesUniform1fv = fake_uniform1fv;
    native.glesUniform4fv = fake_uniform4fv;
    native.glesUniformMatrix4fv = fake_uniform_matrix4fv;
    ctx.native_dispatch = &native;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_context_set_current_for_test(&ctx);

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    if (!glIsEnabled(GL_TEXTURE_2D))
        return fail(1, "per-unit texture enable state");
    program = mgl_compat_fixed_program(&ctx);
    if (program == NULL || !program->linked)
        return fail(2, "textured fixed program link");
    if (strstr(cap.vertex_source, "mgl_a_texcoord0") == NULL ||
        strstr(cap.fragment_source, "texture2D") == NULL ||
        strstr(cap.fragment_source, "mgl_ff_texcoord0*mgl_u_rect_scale0") == NULL)
        return fail(3, "textured program source variant");
    if (program->compat_attrib[MGL_COMPAT_SEM_TEXCOORD0] != 2 ||
        program->compat_uniform_sampler0 != 4)
        return fail(4, "texture semantic reflection");
    if (program->compat_env_lod_bias != -1)
        return fail(44, "unused lod bias reflection");
    mgl_compat_realize_program_uniforms(&ctx, program);
    if (cap.sampler_location != 4 || cap.sampler_value != 0)
        return fail(5, "sampler unit realization");
    if (cap.rect_scale_updates[0] == 0u || cap.rect_scale[0][0] != 1.0f ||
        cap.rect_scale[0][1] != 1.0f)
        return fail(51, "normalized 2D coordinates");
    if (cap.lod_bias_uploads != 0u || cap.invalid_lod_bias_uploads != 0u ||
        glGetError() != GL_NO_ERROR)
        return fail(45, "unused lod bias upload");
    cap.inject_uniform_error = GL_TRUE;
    program->compat_uniform_cache_valid = GL_FALSE;
    mgl_compat_realize_program_uniforms(&ctx, program);
    cap.inject_uniform_error = GL_FALSE;
    if (ctx.errors.native_pending_count != 0u || glGetError() != GL_INVALID_OPERATION)
        return fail(42, "uniform failure not captured");
    if (glGetError() != GL_NO_ERROR)
        return fail(43, "uniform native error not consumed");

    glDisable(GL_TEXTURE_2D);
    if (glIsEnabled(GL_TEXTURE_2D))
        return fail(6, "texture disable state");
    program = mgl_compat_fixed_program(&ctx);
    if (program == NULL || strstr(cap.fragment_source, "texture2D") != NULL)
        return fail(7, "untextured program variant");
    if (program->compat_env_lod_bias != -1)
        return fail(46, "untextured lod bias reflection");
    mgl_compat_realize_program_uniforms(&ctx, program);
    if (cap.lod_bias_uploads != 0u || cap.invalid_lod_bias_uploads != 0u ||
        glGetError() != GL_NO_ERROR)
        return fail(47, "untextured lod bias upload");
    if (cap.delete_program_count != 0u)
        return fail(8, "switch destroyed reusable fixed-function variant");

    rectangle.name = 27u;
    rectangle.target = GL_TEXTURE_RECTANGLE;
    rectangle.levels[0].width = 320;
    rectangle.levels[0].height = 240;
    share.texture_index[(uint32_t)(27u * UINT32_C(2654435761)) >> 22u] = &rectangle;
    ctx.texture_rectangle_binding[0] = rectangle.name;
    glEnable(GL_TEXTURE_RECTANGLE);
    if (!glIsEnabled(GL_TEXTURE_RECTANGLE))
        return fail(52, "rectangle enable state");
    program = mgl_compat_fixed_program(&ctx);
    if (program == NULL || (ctx.fixed_program_key & 1u) == 0u ||
        program->compat_uniform_rect_scale[0] != 40)
        return fail(53, "rectangle fixed shader selection");
    mgl_compat_realize_program_uniforms(&ctx, program);
    if (cap.rect_scale[0][0] != 1.0f / 320.0f || cap.rect_scale[0][1] != 1.0f / 240.0f)
        return fail(54, "rectangle pixel coordinate normalization");
    rectangle.levels[0].width = 512;
    rectangle.levels[0].height = 256;
    mgl_compat_realize_program_uniforms(&ctx, program);
    if (cap.rect_scale[0][0] != 1.0f / 512.0f || cap.rect_scale[0][1] != 1.0f / 256.0f)
        return fail(55, "rectangle dimension changes");
    rectangle.native_name = 72u;
    rectangle.realized = GL_TRUE;
    texture_2d.name = 29u;
    texture_2d.target = GL_TEXTURE_2D;
    texture_2d.native_name = 84u;
    texture_2d.realized = GL_TRUE;
    share.texture_index[(uint32_t)(29u * UINT32_C(2654435761)) >> 22u] = &texture_2d;
    ctx.texture_2d_binding[0] = texture_2d.name;
    glEnable(GL_TEXTURE_2D);
    ctx.texture_2d_prefer_rectangle[0] = GL_FALSE;
    ctx.texture_bindings_dirty = GL_TRUE;
    ctx.texture_bindings_dirty_units[0] = 1u;
    mgl_texture_realize_bindings(&ctx);
    if (cap.native_2d_texture != 84u)
        return fail(58, "canonical bind order");
    mgl_texture_realize_fixed_bindings(&ctx);
    if (cap.native_2d_texture != 72u || !ctx.fixed_texture_binding_override)
        return fail(59, "rectangle texture precedence");
    {
        unsigned count = cap.native_2d_bind_count;
        mgl_texture_realize_fixed_bindings(&ctx);
        if (cap.native_2d_bind_count != count)
            return fail(63, "redundant rectangle binding");
    }
    ctx.current_program = 123u;
    mgl_texture_realize_bindings(&ctx);
    if (cap.native_2d_texture != 84u || ctx.fixed_texture_binding_override)
        return fail(60, "programmable texture binding restoration");
    ctx.current_program = 0u;
    ctx.texture_2d_prefer_rectangle[0] = GL_TRUE;
    ctx.texture_bindings_dirty = GL_TRUE;
    ctx.texture_bindings_dirty_units[0] = 1u;
    mgl_texture_realize_bindings(&ctx);
    if (cap.native_2d_texture != 72u)
        return fail(61, "rectangle last bind");
    glDisable(GL_TEXTURE_RECTANGLE);
    mgl_texture_realize_fixed_bindings(&ctx);
    if (cap.native_2d_texture != 84u)
        return fail(62, "2D texture precedence after disable");
    glDisable(GL_TEXTURE_2D);
    if (glIsEnabled(GL_TEXTURE_RECTANGLE) || (mgl_compat_fixed_program(&ctx) == NULL) ||
        (ctx.fixed_program_key & 1u) != 0u)
        return fail(56, "rectangle shader disable");

    glActiveTexture(GL_TEXTURE2);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE2);
    if (glGetError() != GL_NO_ERROR)
        return fail(9, "multitexture COMBINE API state");
    program = mgl_compat_fixed_program(&ctx);
    if (program == NULL || (ctx.fixed_program_key & 4u) == 0u)
        return fail(10, "full fixed-stage selection");
    if (strstr(cap.vertex_source, "mgl_a_texcoord2") == NULL ||
        strstr(cap.fragment_source, "mgl_u_rect_scale[8]") == NULL ||
        strstr(cap.fragment_source, "mgl_v_texcoord[2].xy)*mgl_u_rect_scale[2]") == NULL ||
        strstr(cap.vertex_source, "mgl_u_texture_matrix[8]") == NULL ||
        strstr(cap.fragment_source, "mgl_u_env_rgb_source") == NULL ||
        strstr(cap.fragment_source, "texels[2]") == NULL ||
        strstr(cap.fragment_source, "gl_FrontFacing") == NULL)
        return fail(11, "shared full fixed-stage shader source");
    if (program->compat_attrib[MGL_COMPAT_SEM_TEXCOORD0 + 2u] != 5 ||
        program->compat_env_enabled != 20 || program->compat_env_rgb_source != 21 ||
        program->compat_uniform_texture_matrix[2] != 22 || program->compat_env_lod_bias != 30)
        return fail(12, "multitexture reflection");
    mgl_compat_realize_program_uniforms(&ctx, program);
    if (cap.rect_scale_updates[2] == 0u || cap.rect_scale[2][0] != 1.0f ||
        cap.rect_scale[2][1] != 1.0f)
        return fail(57, "multi-unit scale reflection");
    if (cap.enabled[0] != 0 || cap.enabled[2] != 1 || cap.source[6] != GL_TEXTURE2 ||
        cap.sampler_unit[2] != 2 || cap.matrix_updates != 1)
        return fail(13, "multitexture combiner and matrix uniforms");
    if (cap.lod_bias_uploads != 1u || cap.invalid_lod_bias_uploads != 0u ||
        cap.lod_bias_values[2] != 0.0f)
        return fail(48, "active lod bias upload");
    glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, 1.5f);
    if (glGetError() != GL_NO_ERROR)
        return fail(49, "lod bias texture state");
    mgl_compat_realize_program_uniforms(&ctx, program);
    if (cap.lod_bias_uploads != 2u || cap.invalid_lod_bias_uploads != 0u ||
        cap.lod_bias_values[2] != 1.5f || glGetError() != GL_NO_ERROR)
        return fail(50, "lod bias value propagation");
    {
        unsigned old_programs = cap.next_program;
        mgl_compat_realize_program_uniforms(&ctx, program);
        if (cap.matrix_updates != 1)
            return fail(14, "redundant fixed matrix upload");
        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_TEXTURE_2D);
        if (mgl_compat_fixed_program(&ctx) != program || cap.next_program != old_programs)
            return fail(15, "dynamic texture enable caused program relink");
        mgl_compat_realize_program_uniforms(&ctx, program);
        if (cap.enabled[0] != 1 || cap.enabled[2] != 1)
            return fail(16, "dynamic texture enable not uploaded");
    }

    {
        unsigned links = cap.link_count;
        unsigned compiles = cap.shader_compilation_count;
        unsigned iteration;
        GLuint extended_native = ctx.fixed_program_native;
        struct mgl_compat_uniform_cache *extended_snapshot = program->compat_uniform_cache;
        for (iteration = 0u; iteration < 24u; ++iteration) {
            GLuint native_id;
            glActiveTexture(GL_TEXTURE2);
            glDisable(GL_TEXTURE_2D);
            glActiveTexture(GL_TEXTURE0);
            glEnable(GL_TEXTURE_2D);
            program = mgl_compat_fixed_program(&ctx);
            if (program == NULL || ctx.fixed_program_key != 1u)
                return fail(17, "textured variant not restored");
            native_id = ctx.fixed_program_native;
            glDisable(GL_TEXTURE_2D);
            program = mgl_compat_fixed_program(&ctx);
            if (program == NULL || ctx.fixed_program_key != 0u)
                return fail(18, "untextured variant not restored");
            glActiveTexture(GL_TEXTURE2);
            glEnable(GL_TEXTURE_2D);
            program = mgl_compat_fixed_program(&ctx);
            if (program == NULL || ctx.fixed_program_native != extended_native ||
                program->compat_uniform_cache != extended_snapshot || native_id == extended_native)
                return fail(19, "extended variant or uniform cache not restored");
            mgl_compat_realize_program_uniforms(&ctx, program);
        }
        if (cap.link_count != links || cap.shader_compilation_count != compiles ||
            cap.delete_program_count != 0u)
            return fail(20, "variant switching recompiles or deletes GLES programs");
    }
    {
        unsigned links;
        glActiveTexture(GL_TEXTURE2);
        glDisable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        program = mgl_compat_fixed_program(&ctx);
        if (program == NULL || ctx.fixed_program_key != 2u)
            return fail(21, "lit untextured variant");
        glEnable(GL_TEXTURE_2D);
        program = mgl_compat_fixed_program(&ctx);
        if (program == NULL || ctx.fixed_program_key != 3u)
            return fail(22, "lit textured variant");
        links = cap.link_count;
        for (unsigned n = 0u; n < 12u; ++n) {
            glDisable(GL_TEXTURE_2D);
            if (mgl_compat_fixed_program(&ctx) == NULL || ctx.fixed_program_key != 2u)
                return fail(23, "lit untextured cache miss");
            glEnable(GL_TEXTURE_2D);
            if (mgl_compat_fixed_program(&ctx) == NULL || ctx.fixed_program_key != 3u)
                return fail(24, "lit textured cache miss");
        }
        if (cap.link_count != links || cap.next_program != 5u)
            return fail(25, "lit variants compiled more than once");
    }
    /* Derived fixed-function variant selection should be reusable for
     * unchanged state and invalidated by every shader-variant dependency. */
    if (!ctx.compat.fixed_variant_key_valid || ctx.compat.fixed_variant_key != 3u)
        return fail(27, "derived fixed shader key not cached");
    program = mgl_compat_fixed_program(&ctx);
    if (program == NULL || !ctx.compat.fixed_variant_key_valid ||
        ctx.compat.fixed_variant_key != 3u)
        return fail(28, "unchanged fixed shader key not reused");
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    if (!ctx.compat.fixed_variant_key_valid)
        return fail(40, "redundant texture enable invalidated key");
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    if (!ctx.compat.fixed_variant_key_valid)
        return fail(41, "redundant texture environment invalidated key");
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    if (ctx.compat.fixed_variant_key_valid)
        return fail(29, "TexEnv mode did not invalidate key");
    if (mgl_compat_fixed_program(&ctx) == NULL || ctx.fixed_program_key != 3u)
        return fail(30, "TexEnv state recomputation changed unrelated variant");
    glActiveTexture(GL_TEXTURE2);
    glEnable(GL_TEXTURE_2D);
    if (ctx.compat.fixed_variant_key_valid)
        return fail(31, "multitexture enable did not invalidate key");
    if (mgl_compat_fixed_program(&ctx) == NULL || ctx.fixed_program_key != 4u)
        return fail(32, "multitexture requires extended variant");
    {
        GLfloat changed[16] = {0};
        changed[0] = changed[5] = changed[10] = changed[15] = 1.0f;
        changed[12] = 0.5f;
        glMatrixLoadfEXT(GL_TEXTURE2, changed);
        if (ctx.compat.fixed_variant_key_valid)
            return fail(33, "DSA texture matrix did not invalidate key");
        if (mgl_compat_fixed_program(&ctx) == NULL || ctx.fixed_program_key != 4u)
            return fail(34, "texture matrix state lost extended variant");
        glMatrixLoadIdentityEXT(GL_TEXTURE2);
        if (ctx.compat.fixed_variant_key_valid)
            return fail(35, "DSA matrix identity did not invalidate key");
    }
    glDisable(GL_TEXTURE_2D);
    if (mgl_compat_fixed_program(&ctx) == NULL || ctx.fixed_program_key != 3u)
        return fail(36, "texture disable failed to restore lit textured variant");
    glPushAttrib(GL_ENABLE_BIT);
    glEnable(GL_TEXTURE_2D);
    if (mgl_compat_fixed_program(&ctx) == NULL || ctx.fixed_program_key != 4u)
        return fail(37, "pushed texture enable state did not select extended variant");
    glPopAttrib();
    if (ctx.compat.fixed_variant_key_valid)
        return fail(38, "attribute-stack restore did not invalidate key");
    if (mgl_compat_fixed_program(&ctx) == NULL || ctx.fixed_program_key != 3u)
        return fail(39, "attribute-stack restore selected stale fixed shader");
    mgl_compat_destroy_fixed_program(&ctx);
    if (cap.delete_program_count != 5u)
        return fail(26, "variant cache programs not released exactly once");
    {
        struct mgl_texture one_d = {0}, three_d = {0}, cube = {0};
        GLuint first_native;
        GLuint one_d_native;
        GLuint three_d_native;
        unsigned compile_before_restore;
        unsigned link_before_restore;
        unsigned compiled;
        one_d.name = 31u;
        one_d.native_name = 91u;
        one_d.target = GL_TEXTURE_1D;
        one_d.realized = GL_TRUE;
        three_d.name = 33u;
        three_d.native_name = 92u;
        three_d.target = GL_TEXTURE_3D;
        three_d.realized = GL_TRUE;
        cube.name = 35u;
        cube.native_name = 93u;
        cube.target = GL_TEXTURE_CUBE_MAP;
        cube.realized = GL_TRUE;
        share.texture_index[(uint32_t)(one_d.name * UINT32_C(2654435761)) >> 22u] = &one_d;
        share.texture_index[(uint32_t)(three_d.name * UINT32_C(2654435761)) >> 22u] = &three_d;
        share.texture_index[(uint32_t)(cube.name * UINT32_C(2654435761)) >> 22u] = &cube;
        ctx.texture_1d_binding[0] = one_d.name;
        ctx.texture_3d_binding[0] = three_d.name;
        ctx.texture_cube_binding[0] = cube.name;
        glActiveTexture(GL_TEXTURE0);
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_TEXTURE_1D);
        program = mgl_compat_fixed_program(&ctx);
        if (program == NULL || ctx.fixed_program_key != 4u ||
            strstr(fixed_fragment_source(ctx.fixed_program_native), "sampler2D mgl_u_texture0") ==
                NULL ||
            strstr(fixed_fragment_source(ctx.fixed_program_native),
                   "vec2(mgl_v_texcoord[0].x/mgl_v_texcoord[0].w,0.5)") == NULL)
            return fail(70, "1D fixed-function source");
        mgl_compat_realize_program_uniforms(&ctx, program);
        if (cap.enabled[0] != 1)
            return fail(71, "1D texture stage activation");
        ctx.texture_bindings_dirty = GL_TRUE;
        ctx.texture_bindings_dirty_units[0] = 1u;
        mgl_texture_realize_bindings(&ctx);
        mgl_texture_realize_fixed_bindings(&ctx);
        if (cap.native_2d_texture != one_d.native_name)
            return fail(72, "1D emulated backing binding");
        first_native = ctx.fixed_program_native;
        one_d_native = first_native;
        glEnable(GL_TEXTURE_3D);
        program = mgl_compat_fixed_program(&ctx);
        if (program == NULL || ctx.fixed_program_native == first_native ||
            strstr(fixed_fragment_source(ctx.fixed_program_native),
                   "uniform sampler3D mgl_u_texture0") == NULL ||
            strstr(fixed_fragment_source(ctx.fixed_program_native),
                   "mgl_u_texture0,mgl_v_texcoord[0].xyz/mgl_v_texcoord[0].w") == NULL)
            return fail(73, "3D fixed-function sampler");
        mgl_texture_realize_fixed_bindings(&ctx);
        if (cap.native_3d_texture != three_d.native_name)
            return fail(74, "3D backing binding");
        first_native = ctx.fixed_program_native;
        three_d_native = first_native;
        glEnable(GL_TEXTURE_CUBE_MAP);
        program = mgl_compat_fixed_program(&ctx);
        if (program == NULL || ctx.fixed_program_native == first_native ||
            strstr(fixed_fragment_source(ctx.fixed_program_native),
                   "uniform samplerCube mgl_u_texture0") == NULL ||
            strstr(fixed_fragment_source(ctx.fixed_program_native),
                   "mgl_u_texture0,mgl_v_texcoord[0].xyz/mgl_v_texcoord[0].w") == NULL)
            return fail(75, "cubemap fixed-function sampler");
        mgl_texture_realize_fixed_bindings(&ctx);
        if (cap.native_cube_texture != cube.native_name)
            return fail(76, "cubemap backing binding");
        first_native = ctx.fixed_program_native;
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_TEXTURE_RECTANGLE);
        if (mgl_compat_fixed_program(&ctx) == NULL || ctx.fixed_program_native != first_native)
            return fail(77, "cubemap target precedence");
        glDisable(GL_TEXTURE_CUBE_MAP);
        link_before_restore = cap.link_count;
        compile_before_restore = cap.shader_compilation_count;
        program = mgl_compat_fixed_program(&ctx);
        if (program == NULL || ctx.fixed_program_native != three_d_native ||
            cap.link_count != link_before_restore ||
            cap.shader_compilation_count != compile_before_restore ||
            strstr(fixed_fragment_source(ctx.fixed_program_native),
                   "uniform sampler3D mgl_u_texture0") == NULL)
            return fail(78, "3D target precedence");
        glDisable(GL_TEXTURE_3D);
        program = mgl_compat_fixed_program(&ctx);
        if (program == NULL ||
            strstr(fixed_fragment_source(ctx.fixed_program_native),
                   "uniform sampler3D mgl_u_texture0") != NULL ||
            strstr(fixed_fragment_source(ctx.fixed_program_native),
                   "uniform samplerCube mgl_u_texture0") != NULL ||
            (ctx.fixed_program_key != 1u &&
             strstr(fixed_fragment_source(ctx.fixed_program_native),
                    "mgl_v_texcoord[0].xy)*mgl_u_rect_scale[0]") == NULL))
            return fail(79, "rectangle precedence after 3D");
        glDisable(GL_TEXTURE_RECTANGLE);
        glDisable(GL_TEXTURE_2D);
        link_before_restore = cap.link_count;
        compile_before_restore = cap.shader_compilation_count;
        program = mgl_compat_fixed_program(&ctx);
        if (program == NULL || ctx.fixed_program_native != one_d_native ||
            cap.link_count != link_before_restore ||
            cap.shader_compilation_count != compile_before_restore ||
            strstr(fixed_fragment_source(ctx.fixed_program_native),
                   "vec2(mgl_v_texcoord[0].x/mgl_v_texcoord[0].w,0.5)") == NULL)
            return fail(80, "1D restoration after 2D");
        glActiveTexture(GL_TEXTURE2);
        glEnable(GL_TEXTURE_CUBE_MAP);
        compiled = cap.shader_compilation_count;
        program = mgl_compat_fixed_program(&ctx);
        if (program == NULL || cap.shader_compilation_count <= compiled ||
            strstr(fixed_fragment_source(ctx.fixed_program_native),
                   "uniform samplerCube mgl_u_texture2") == NULL ||
            strstr(fixed_fragment_source(ctx.fixed_program_native),
                   "vec2(mgl_v_texcoord[0].x/mgl_v_texcoord[0].w,0.5)") == NULL)
            return fail(81, "mixed dimensional multitexture");
        glDisable(GL_TEXTURE_CUBE_MAP);
        glActiveTexture(GL_TEXTURE0);
        glDisable(GL_TEXTURE_1D);
        if (mgl_compat_fixed_program(&ctx) == NULL || ctx.fixed_program_key != 0u)
            return fail(82, "untextured restore");
        mgl_compat_destroy_fixed_program(&ctx);
        if (glGetError() != GL_NO_ERROR)
            return fail(83, "fixed texture transition error");
    }
    {
        GLuint first_program = 0u;
        GLuint recent_program = 0u;
        unsigned n;
        unsigned before;
        mgl_compat_state_init(&ctx.compat);
        ctx.compat.texture_1d_enabled[0] = GL_TRUE;
        for (n = 0u; n < 34u; ++n) {
            unsigned i;
            for (i = 0u; i < 3u; ++i) {
                unsigned target = (n >> (i * 2u)) & 3u;
                unsigned unit = i + 2u;
                ctx.compat.texture_1d_enabled[unit] = target == 1u;
                ctx.compat.texture_3d_enabled[unit] = target == 2u;
                ctx.compat.texture_cube_enabled[unit] = target == 3u;
            }
            ctx.compat.fixed_variant_key_valid = GL_FALSE;
            if (mgl_compat_fixed_program(&ctx) == NULL || ctx.fixed_program_key != 4u)
                return fail(84, "complex cache variant generation");
            if (n == 0u)
                first_program = ctx.fixed_program_native;
            if (n == 32u)
                recent_program = ctx.fixed_program_native;
        }
        if (ctx.fixed_complex_program_count != 32u)
            return fail(85, "complex cache did not enforce capacity");
        for (unsigned i = 0u; i < 3u; ++i) {
            unsigned target = (32u >> (i * 2u)) & 3u;
            unsigned unit = i + 2u;
            ctx.compat.texture_1d_enabled[unit] = target == 1u;
            ctx.compat.texture_3d_enabled[unit] = target == 2u;
            ctx.compat.texture_cube_enabled[unit] = target == 3u;
        }
        ctx.compat.fixed_variant_key_valid = GL_FALSE;
        before = cap.link_count;
        if (mgl_compat_fixed_program(&ctx) == NULL || ctx.fixed_program_native != recent_program ||
            cap.link_count != before)
            return fail(86, "recent complex shader was not retained");
        for (unsigned i = 2u; i < 5u; ++i) {
            ctx.compat.texture_1d_enabled[i] = GL_FALSE;
            ctx.compat.texture_3d_enabled[i] = GL_FALSE;
            ctx.compat.texture_cube_enabled[i] = GL_FALSE;
        }
        ctx.compat.fixed_variant_key_valid = GL_FALSE;
        before = cap.link_count;
        if (mgl_compat_fixed_program(&ctx) == NULL || ctx.fixed_program_native == first_program ||
            cap.link_count != before + 1u)
            return fail(87, "least-recent complex signature was not evicted");
        mgl_compat_destroy_fixed_program(&ctx);
        if (cap.delete_program_count != cap.next_program)
            return fail(88, "complex cache program cleanup");
    }
    mgl_context_set_current_for_test(NULL);
    puts("mGL fixed-function texture unit test: PASS");
    return 0;
}

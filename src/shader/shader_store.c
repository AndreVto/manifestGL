#include "shader/shader_store.h"

#include <stdlib.h>
#include <string.h>

struct mgl_spirv_module *mgl_spirv_module_create(const void *binary, size_t length) {
    struct mgl_spirv_module *module;
    if (binary == NULL || length < 5u * sizeof(uint32_t) || (length % sizeof(uint32_t)) != 0u ||
        length > SIZE_MAX - sizeof(*module))
        return NULL;
    module = malloc(sizeof(*module) + length);
    if (module == NULL)
        return NULL;
    atomic_init(&module->references, 1u);
    module->word_count = length / sizeof(uint32_t);
    memcpy(module->words, binary, length);
    return module;
}

void mgl_spirv_module_release(struct mgl_spirv_module *module) {
    if (module != NULL &&
        atomic_fetch_sub_explicit(&module->references, 1u, memory_order_acq_rel) == 1u)
        free(module);
}

void mgl_shader_set_spirv_module(struct mgl_shader *shader, struct mgl_spirv_module *module) {
    struct mgl_spirv_module *old;
    if (shader == NULL)
        return;
    if (module != NULL)
        atomic_fetch_add_explicit(&module->references, 1u, memory_order_relaxed);
    old = shader->spirv_module;
    shader->spirv_module = module;
    mgl_spirv_module_release(old);
}

static GLuint next_nonzero(atomic_uint *counter) {
    GLuint out;
    do {
        out = (GLuint)atomic_fetch_add_explicit(counter, 1u, memory_order_relaxed);
    } while (out == 0u);
    return out;
}

struct mgl_shader *mgl_shader_find(struct mgl_context *ctx, GLuint name) {
    struct mgl_shader *it;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return NULL;
    for (it = ctx->share_group->shaders; it != NULL; it = it->next)
        if (it->name == name)
            return it;
    return NULL;
}

struct mgl_program *mgl_program_find(struct mgl_context *ctx, GLuint name) {
    struct mgl_program *it;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return NULL;
    for (it = ctx->share_group->programs; it != NULL; it = it->next)
        if (it->name == name)
            return it;
    return NULL;
}

struct mgl_shader *mgl_shader_create(struct mgl_context *ctx, GLenum type) {
    struct mgl_shader *obj;
    if (ctx == NULL || ctx->share_group == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesCreateShader == NULL)
        return NULL;
    obj = calloc(1, sizeof(*obj));
    if (obj == NULL)
        return NULL;
    obj->name = next_nonzero(&ctx->share_group->next_shader_program_name);
    obj->type = type;
    obj->native_name = ctx->native_dispatch->glesCreateShader(type);
    if (obj->native_name == 0u) {
        free(obj);
        return NULL;
    }
    obj->next = ctx->share_group->shaders;
    ctx->share_group->shaders = obj;
    return obj;
}

void mgl_program_init_metadata(struct mgl_program *obj) {
    unsigned i;
    if (obj == NULL)
        return;
    obj->linked = GL_FALSE;
    obj->linked_compute = GL_FALSE;
    obj->validated = GL_FALSE;
    obj->compat_uniform_cache_valid = GL_FALSE;
    obj->active_generic_mask = 0u;
    for (i = 0u; i < MGL_MAX_VERTEX_ATTRIBS; ++i)
        obj->active_generic_type[i] = GL_FLOAT;
    for (i = 0u; i < MGL_COMPAT_SEMANTIC_COUNT; ++i)
        obj->compat_attrib[i] = -1;
    obj->compat_uniform_model_view_projection = -1;
    obj->compat_uniform_model_view = -1;
    obj->compat_uniform_projection = -1;
    obj->compat_uniform_normal_matrix = -1;
    for (i = 0u; i < 8u; ++i)
        obj->compat_uniform_texture_matrix[i] = -1;
    obj->compat_uniform_sampler0 = -1;
    for (i = 0u; i < 8u; ++i)
        obj->compat_uniform_rect_scale[i] = -1;
    obj->compat_uniform_ff_texture_enabled = -1;
    obj->compat_uniform_alpha_enabled = -1;
    obj->compat_uniform_alpha_func = -1;
    obj->compat_uniform_alpha_ref = -1;
    obj->compat_uniform_point_coord_flip = -1;
    obj->compat_uniform_point_size = -1;
    obj->compat_uniform_program_point_size = -1;
    obj->compat_uniform_texture_env_mode0 = -1;
    obj->compat_env_enabled = -1;
    obj->compat_env_mode = -1;
    obj->compat_env_color = -1;
    obj->compat_env_rgb_mode = -1;
    obj->compat_env_alpha_mode = -1;
    obj->compat_env_rgb_source = -1;
    obj->compat_env_alpha_source = -1;
    obj->compat_env_rgb_operand = -1;
    obj->compat_env_alpha_operand = -1;
    obj->compat_env_rgb_scale = -1;
    obj->compat_env_alpha_scale = -1;
    obj->compat_env_lod_bias = -1;
    for (unsigned texunit = 0u; texunit < 8u; ++texunit)
        obj->compat_env_sampler[texunit] = -1;
    obj->compat_uniform_texgen_enabled = -1;
    obj->compat_uniform_texgen_mode = -1;
    obj->compat_uniform_texgen_object_plane = -1;
    obj->compat_uniform_texgen_eye_plane = -1;
    obj->compat_uniform_clip_enabled = -1;
    obj->compat_uniform_clip_plane = -1;
    obj->compat_uniform_fog_enabled = -1;
    obj->compat_uniform_fog_mode = -1;
    obj->compat_uniform_fog_density = -1;
    obj->compat_uniform_fog_start = -1;
    obj->compat_uniform_fog_end = -1;
    obj->compat_uniform_fog_color = -1;
    obj->compat_uniform_fog_coord_explicit = -1;
    obj->compat_uniform_normalize_enabled = -1;
    obj->compat_uniform_lighting_enabled = -1;
    obj->compat_uniform_implicit_texgen_enabled = -1;
    obj->compat_uniform_implicit_texgen_mode = -1;
    obj->compat_uniform_implicit_texgen_object_plane = -1;
    obj->compat_uniform_implicit_texgen_eye_plane = -1;
    obj->compat_uniform_light_enabled = -1;
    obj->compat_uniform_light_ambient = -1;
    obj->compat_uniform_light_diffuse = -1;
    obj->compat_uniform_light_specular = -1;
    obj->compat_uniform_light_position = -1;
    obj->compat_uniform_light_spot_direction = -1;
    obj->compat_uniform_light_spot_exponent = -1;
    obj->compat_uniform_light_spot_cutoff = -1;
    obj->compat_uniform_light_constant_attenuation = -1;
    obj->compat_uniform_light_linear_attenuation = -1;
    obj->compat_uniform_light_quadratic_attenuation = -1;
    obj->compat_uniform_light_model_ambient = -1;
    obj->compat_uniform_light_model_local_viewer = -1;
    obj->compat_uniform_light_model_two_side = -1;
    obj->compat_uniform_material_ambient = -1;
    obj->compat_uniform_material_diffuse = -1;
    obj->compat_uniform_material_specular = -1;
    obj->compat_uniform_material_emission = -1;
    obj->compat_uniform_material_shininess = -1;
    obj->compat_uniform_material_back_ambient = -1;
    obj->compat_uniform_material_back_diffuse = -1;
    obj->compat_uniform_material_back_specular = -1;
    obj->compat_uniform_material_back_emission = -1;
    obj->compat_uniform_material_back_shininess = -1;
    obj->compat_uniform_color_material_enabled = -1;
    obj->compat_uniform_color_material_mode = -1;
    obj->compat_uniform_color_material_face = -1;
}

struct mgl_program *mgl_program_create(struct mgl_context *ctx) {
    struct mgl_program *obj;
    if (ctx == NULL || ctx->share_group == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesCreateProgram == NULL)
        return NULL;
    obj = calloc(1, sizeof(*obj));
    if (obj == NULL)
        return NULL;
    obj->name = next_nonzero(&ctx->share_group->next_shader_program_name);
    obj->native_name = ctx->native_dispatch->glesCreateProgram();
    if (obj->native_name == 0u) {
        free(obj);
        return NULL;
    }
    mgl_program_init_metadata(obj);
    obj->transform_feedback_buffer_mode = GL_INTERLEAVED_ATTRIBS;
    obj->next = ctx->share_group->programs;
    ctx->share_group->programs = obj;
    return obj;
}

void mgl_shader_maybe_destroy(struct mgl_context *ctx, struct mgl_shader *shader) {
    struct mgl_shader **link;
    if (ctx == NULL || shader == NULL || !shader->delete_pending || shader->attach_count != 0u)
        return;
    link = &ctx->share_group->shaders;
    while (*link != NULL && *link != shader)
        link = &(*link)->next;
    if (*link == shader)
        *link = shader->next;
    if (shader->native_name != 0u && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesDeleteShader != NULL)
        ctx->native_dispatch->glesDeleteShader(shader->native_name);
    free(shader->source);
    mgl_spirv_module_release(shader->spirv_module);
    free(shader->translated_source);
    free(shader->info_log);
    free(shader);
}

void mgl_program_maybe_destroy(struct mgl_context *ctx, struct mgl_program *program) {
    struct mgl_program **link;
    unsigned i;
    if (ctx == NULL || program == NULL || !program->delete_pending || program->use_count != 0u)
        return;
    link = &ctx->share_group->programs;
    while (*link != NULL && *link != program)
        link = &(*link)->next;
    if (*link == program)
        *link = program->next;
    if (ctx != NULL)
        ctx->draw_native_program_valid = GL_FALSE;
    if (program->native_name != 0u && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesDeleteProgram != NULL)
        ctx->native_dispatch->glesDeleteProgram(program->native_name);
    for (i = 0; i < program->attached_count; ++i) {
        struct mgl_shader *shader = program->attached[i];
        if (shader != NULL && shader->attach_count > 0u) {
            --shader->attach_count;
            mgl_shader_maybe_destroy(ctx, shader);
        }
    }
    if (program->transform_feedback_varyings != NULL) {
        GLsizei j;
        for (j = 0; j < program->transform_feedback_varying_count; ++j)
            free(program->transform_feedback_varyings[j]);
        free(program->transform_feedback_varyings);
    }
    if (program->frag_output_bindings != NULL) {
        size_t j;
        for (j = 0u; j < program->frag_output_binding_count; ++j)
            free(program->frag_output_bindings[j].name);
        free(program->frag_output_bindings);
    }
    free(program->compat_uniform_cache);
    free(program->buffer_sampler_uniforms);
    free(program->info_log);
    free(program);
}

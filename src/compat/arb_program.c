#include "mgl/gl.h"

#include "compat/arb_program.h"
#include "core/render_diag.h"
#include "compat/fixed_function.h"
#include "shader/shader_store.h"
#include "shader/translator.h"
#include "list/display_list.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void arb_pair_cache_free_unbound(struct mgl_context *ctx);

static char *dup_n(const char *text, size_t count) {
    char *out = malloc(count + 1u);
    if (out == NULL)
        return NULL;
    if (count != 0u)
        memcpy(out, text, count);
    out[count] = '\0';
    return out;
}

static int valid_target(GLenum target) {
    return target == GL_VERTEX_PROGRAM_ARB || target == GL_FRAGMENT_PROGRAM_ARB;
}

static GLfloat (*env_parameters(struct mgl_context *ctx, GLenum target))[4] {
    return target == GL_VERTEX_PROGRAM_ARB ? ctx->arb_vertex_env : ctx->arb_fragment_env;
}

static GLuint *binding_slot(struct mgl_context *ctx, GLenum target) {
    if (target == GL_VERTEX_PROGRAM_ARB)
        return &ctx->arb_vertex_program_binding;
    if (target == GL_FRAGMENT_PROGRAM_ARB)
        return &ctx->arb_fragment_program_binding;
    return NULL;
}

static struct mgl_arb_program *default_program(struct mgl_context *ctx, GLenum target) {
    struct mgl_arb_program *program;
    if (ctx == NULL || !valid_target(target))
        return NULL;
    program = target == GL_VERTEX_PROGRAM_ARB ? &ctx->arb_default_vertex_program
                                              : &ctx->arb_default_fragment_program;
    if (program->target == 0u) {
        program->name = 0u;
        program->target = target;
        program->serial = 1u;
        program->max_env_index = UINT_MAX;
        program->max_local_index = UINT_MAX;
    }
    return program;
}

static struct mgl_arb_program *bound_program(struct mgl_context *ctx, GLenum target) {
    GLuint *slot = binding_slot(ctx, target);
    if (slot == NULL)
        return NULL;
    return *slot == 0u ? default_program(ctx, target) : mgl_arb_program_find(ctx, *slot);
}

static GLuint next_nonzero(atomic_uint *counter) {
    GLuint value;
    do {
        value = (GLuint)atomic_fetch_add_explicit(counter, 1u, memory_order_relaxed);
    } while (value == 0u);
    return value;
}

struct mgl_arb_program *mgl_arb_program_find(struct mgl_context *ctx, GLuint name) {
    struct mgl_arb_program *it;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return NULL;
    for (it = ctx->share_group->arb_programs; it != NULL; it = it->next)
        if (it->name == name)
            return it;
    return NULL;
}

static struct mgl_arb_program *arb_program_create(struct mgl_context *ctx, GLuint name) {
    struct mgl_arb_program *program;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return NULL;
    program = calloc(1u, sizeof(*program));
    if (program == NULL)
        return NULL;
    program->name = name;
    ++ctx->share_group->arb_programs_revision;
    program->serial = (GLuint)ctx->share_group->arb_programs_revision;
    program->max_env_index = UINT_MAX;
    program->max_local_index = UINT_MAX;
    program->next = ctx->share_group->arb_programs;
    ctx->share_group->arb_programs = program;
    return program;
}

static void arb_program_free(struct mgl_arb_program *program) {
    if (program == NULL)
        return;
    free(program->source);
    free(program->translated_source);
    free(program);
}

void mgl_arb_program_free_all(struct mgl_share_group *group) {
    struct mgl_arb_program *program;
    if (group == NULL)
        return;
    program = group->arb_programs;
    while (program != NULL) {
        struct mgl_arb_program *next = program->next;
        arb_program_free(program);
        program = next;
    }
    group->arb_programs = NULL;
}

void mgl_arb_context_cleanup(struct mgl_context *ctx) {
    struct mgl_arb_program *programs[2];
    unsigned i;
    if (ctx == NULL)
        return;
    /* EGL may already have destroyed the native objects; free only cache metadata during
     * cleanup. */
    arb_pair_cache_free_unbound(ctx);
    programs[0] = &ctx->arb_default_vertex_program;
    programs[1] = &ctx->arb_default_fragment_program;
    for (i = 0u; i < 2u; ++i) {
        free(programs[i]->source);
        free(programs[i]->translated_source);
        programs[i]->source = NULL;
        programs[i]->translated_source = NULL;
        programs[i]->source_length = 0;
        programs[i]->valid = GL_FALSE;
        programs[i]->instruction_count = 0u;
        programs[i]->temporary_count = 0u;
        programs[i]->parameter_count = 0u;
        programs[i]->attrib_count = 0u;
        programs[i]->address_count = 0u;
        programs[i]->texture_count = 0u;
        programs[i]->texture_indirections = 0u;
    }
}

static void set_program_error(struct mgl_context *ctx, GLint position, const char *text) {
    if (ctx == NULL)
        return;
    ctx->arb_program_error_position = position;
    if (text == NULL)
        text = "";
    (void)snprintf(ctx->arb_program_error_string, sizeof(ctx->arb_program_error_string), "%s",
                   text);
}

/* Per-context LRU of linked ARB shader pairs, keyed by object names and
 * source serials. Each inactive entry owns its native shaders, program, and
 * compatibility-uniform cache. */
#define MGL_ARB_PAIR_CACHE_LIMIT 64u

struct mgl_arb_pair_cache {
    struct mgl_arb_pair_cache *next;
    struct mgl_program program;
    GLuint native_program;
    GLuint vertex_shader;
    GLuint fragment_shader;
    GLuint vertex_name;
    GLuint fragment_name;
    GLuint vertex_serial;
    GLuint fragment_serial;
    unsigned stage_mask;
    GLint uniform_vp_env;
    GLint uniform_vp_local;
    GLint uniform_fp_env;
    GLint uniform_fp_local;
    GLint uniform_textures[MGL_TEXTURE_UNITS];
    unsigned vp_env_count;
    unsigned vp_local_count;
    unsigned fp_env_count;
    unsigned fp_local_count;
    uint64_t uploaded_vp_env_revision, uploaded_vp_local_revision;
    uint64_t uploaded_fp_env_revision, uploaded_fp_local_revision;
    unsigned uniform_uploaded_mask;
    GLboolean sampler_uniforms_uploaded;
};

static void arb_pair_entry_destroy(struct mgl_context *ctx, struct mgl_arb_pair_cache *entry) {
    if (entry == NULL)
        return;
    if (ctx->native_dispatch != NULL) {
        if (entry->native_program != 0u && ctx->native_dispatch->glesDeleteProgram != NULL)
            ctx->native_dispatch->glesDeleteProgram(entry->native_program);
        if (entry->vertex_shader != 0u && ctx->native_dispatch->glesDeleteShader != NULL)
            ctx->native_dispatch->glesDeleteShader(entry->vertex_shader);
        if (entry->fragment_shader != 0u && ctx->native_dispatch->glesDeleteShader != NULL)
            ctx->native_dispatch->glesDeleteShader(entry->fragment_shader);
    }
    free(entry->program.compat_uniform_cache);
    free(entry);
}

static void arb_pair_cache_free_unbound(struct mgl_context *ctx) {
    while (ctx != NULL && ctx->arb_pair_cache != NULL) {
        struct mgl_arb_pair_cache *entry = ctx->arb_pair_cache;
        ctx->arb_pair_cache = entry->next;
        free(entry->program.compat_uniform_cache);
        free(entry);
    }
    if (ctx != NULL)
        ctx->arb_pair_cache_count = 0u;
}

static void delete_native_cache(struct mgl_context *ctx);

static void arb_pair_save_active(struct mgl_context *ctx) {
    struct mgl_arb_pair_cache *entry;
    if (ctx->arb_native_program == 0u)
        return;
    entry = calloc(1u, sizeof(*entry));
    if (entry == NULL) {
        /* Without cache-entry allocation, the outgoing native shader pair must be destroyed
         * before replacement. */
        delete_native_cache(ctx);
        return;
    }
    entry->program = ctx->arb_linked_program;
    entry->native_program = ctx->arb_native_program;
    entry->vertex_shader = ctx->arb_vertex_shader_native;
    entry->fragment_shader = ctx->arb_fragment_shader_native;
    entry->vertex_name = ctx->arb_cached_vertex_name;
    entry->fragment_name = ctx->arb_cached_fragment_name;
    entry->vertex_serial = ctx->arb_cached_vertex_serial;
    entry->fragment_serial = ctx->arb_cached_fragment_serial;
    entry->stage_mask = ctx->arb_cached_stage_mask;
    entry->uniform_vp_env = ctx->arb_uniform_vp_env;
    entry->uniform_vp_local = ctx->arb_uniform_vp_local;
    entry->uniform_fp_env = ctx->arb_uniform_fp_env;
    entry->uniform_fp_local = ctx->arb_uniform_fp_local;
    memcpy(entry->uniform_textures, ctx->arb_uniform_textures, sizeof(entry->uniform_textures));
    entry->vp_env_count = ctx->arb_vp_env_count;
    entry->vp_local_count = ctx->arb_vp_local_count;
    entry->fp_env_count = ctx->arb_fp_env_count;
    entry->fp_local_count = ctx->arb_fp_local_count;
    entry->uploaded_vp_env_revision = ctx->arb_uploaded_vp_env_revision;
    entry->uploaded_vp_local_revision = ctx->arb_uploaded_vp_local_revision;
    entry->uploaded_fp_env_revision = ctx->arb_uploaded_fp_env_revision;
    entry->uploaded_fp_local_revision = ctx->arb_uploaded_fp_local_revision;
    entry->uniform_uploaded_mask = ctx->arb_uniform_uploaded_mask;
    entry->sampler_uniforms_uploaded = ctx->arb_sampler_uniforms_uploaded;
    entry->next = ctx->arb_pair_cache;
    ctx->arb_pair_cache = entry;
    ++ctx->arb_pair_cache_count;
    ctx->arb_native_program = 0u;
    ctx->arb_vertex_shader_native = 0u;
    ctx->arb_fragment_shader_native = 0u;
    ctx->arb_linked_program.compat_uniform_cache = NULL;
    ctx->arb_native_ready = GL_FALSE;
    ctx->draw_native_program_valid = GL_FALSE;
    if (ctx->arb_pair_cache_count > MGL_ARB_PAIR_CACHE_LIMIT) {
        struct mgl_arb_pair_cache *previous = NULL;
        struct mgl_arb_pair_cache *last = ctx->arb_pair_cache;
        while (last->next != NULL) {
            previous = last;
            last = last->next;
        }
        if (previous != NULL)
            previous->next = NULL;
        else
            ctx->arb_pair_cache = NULL;
        --ctx->arb_pair_cache_count;
        arb_pair_entry_destroy(ctx, last);
        mgl_perf_diag_program_cache(ctx, MGL_PERF_ARB_PAIR_EVICT);
    }
}

static int arb_pair_restore(struct mgl_context *ctx, GLuint vn, GLuint fn, GLuint vs, GLuint fs,
                            unsigned stage_mask) {
    struct mgl_arb_pair_cache **slot = &ctx->arb_pair_cache;
    while (*slot != NULL) {
        struct mgl_arb_pair_cache *entry = *slot;
        if (entry->vertex_name == vn && entry->fragment_name == fn && entry->vertex_serial == vs &&
            entry->fragment_serial == fs && entry->stage_mask == stage_mask) {
            *slot = entry->next;
            --ctx->arb_pair_cache_count;
            ctx->arb_linked_program = entry->program;
            ctx->arb_native_program = entry->native_program;
            ctx->arb_vertex_shader_native = entry->vertex_shader;
            ctx->arb_fragment_shader_native = entry->fragment_shader;
            ctx->arb_cached_vertex_name = vn;
            ctx->arb_cached_fragment_name = fn;
            ctx->arb_cached_vertex_serial = vs;
            ctx->arb_cached_fragment_serial = fs;
            ctx->arb_cached_stage_mask = stage_mask;
            ctx->arb_uniform_vp_env = entry->uniform_vp_env;
            ctx->arb_uniform_vp_local = entry->uniform_vp_local;
            ctx->arb_uniform_fp_env = entry->uniform_fp_env;
            ctx->arb_uniform_fp_local = entry->uniform_fp_local;
            memcpy(ctx->arb_uniform_textures, entry->uniform_textures,
                   sizeof(entry->uniform_textures));
            ctx->arb_vp_env_count = entry->vp_env_count;
            ctx->arb_vp_local_count = entry->vp_local_count;
            ctx->arb_fp_env_count = entry->fp_env_count;
            ctx->arb_fp_local_count = entry->fp_local_count;
            ctx->arb_uploaded_vp_env_revision = entry->uploaded_vp_env_revision;
            ctx->arb_uploaded_vp_local_revision = entry->uploaded_vp_local_revision;
            ctx->arb_uploaded_fp_env_revision = entry->uploaded_fp_env_revision;
            ctx->arb_uploaded_fp_local_revision = entry->uploaded_fp_local_revision;
            ctx->arb_uniform_uploaded_mask = entry->uniform_uploaded_mask;
            ctx->arb_sampler_uniforms_uploaded = entry->sampler_uniforms_uploaded;
            ctx->arb_native_ready = GL_TRUE;
            ctx->arb_native_failed = GL_FALSE;
            ctx->arb_resolved_revision = ctx->share_group->arb_programs_revision;
            ctx->draw_native_program_valid = GL_FALSE;
            free(entry);
            mgl_perf_diag_program_cache(ctx, MGL_PERF_ARB_PAIR_RESTORE);
            return 1;
        }
        slot = &(*slot)->next;
    }
    return 0;
}

static void delete_native_cache(struct mgl_context *ctx) {
    if (ctx == NULL || ctx->native_dispatch == NULL)
        return;
    ctx->draw_native_program_valid = GL_FALSE;
    if (ctx->arb_native_program != 0u && ctx->native_dispatch->glesDeleteProgram != NULL)
        ctx->native_dispatch->glesDeleteProgram(ctx->arb_native_program);
    if (ctx->arb_vertex_shader_native != 0u && ctx->native_dispatch->glesDeleteShader != NULL)
        ctx->native_dispatch->glesDeleteShader(ctx->arb_vertex_shader_native);
    if (ctx->arb_fragment_shader_native != 0u && ctx->native_dispatch->glesDeleteShader != NULL)
        ctx->native_dispatch->glesDeleteShader(ctx->arb_fragment_shader_native);
    ctx->arb_native_program = 0u;
    ctx->arb_vertex_shader_native = 0u;
    ctx->arb_fragment_shader_native = 0u;
    ctx->arb_native_ready = GL_FALSE;
    ctx->arb_native_failed = GL_FALSE;
    ctx->arb_cached_vertex_name = 0u;
    ctx->arb_cached_fragment_name = 0u;
    ctx->arb_cached_vertex_serial = 0u;
    ctx->arb_cached_fragment_serial = 0u;
    ctx->arb_resolved_revision = 0u;
    ctx->arb_cached_stage_mask = 0u;
    ctx->arb_uniform_uploaded_mask = 0u;
    ctx->arb_sampler_uniforms_uploaded = GL_FALSE;
    ctx->arb_uniform_vp_env = -1;
    ctx->arb_uniform_vp_local = -1;
    ctx->arb_uniform_fp_env = -1;
    ctx->arb_uniform_fp_local = -1;
    for (unsigned unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit)
        ctx->arb_uniform_textures[unit] = -1;
    free(ctx->arb_linked_program.compat_uniform_cache);
    ctx->arb_linked_program.compat_uniform_cache = NULL;
    ctx->arb_linked_program.compat_uniform_cache_valid = GL_FALSE;
    ctx->arb_vp_env_count = 0u;
    ctx->arb_vp_local_count = 0u;
    ctx->arb_fp_env_count = 0u;
    ctx->arb_fp_local_count = 0u;
    mgl_program_init_metadata(&ctx->arb_linked_program);
}

void mgl_arb_destroy_native_program(struct mgl_context *ctx) {
    if (ctx == NULL)
        return;
    delete_native_cache(ctx);
    while (ctx->arb_pair_cache != NULL) {
        struct mgl_arb_pair_cache *entry = ctx->arb_pair_cache;
        ctx->arb_pair_cache = entry->next;
        arb_pair_entry_destroy(ctx, entry);
    }
    ctx->arb_pair_cache_count = 0u;
}

void mgl_arb_invalidate_native_program(struct mgl_context *ctx) {
    if (ctx == NULL)
        return;
    ctx->arb_native_ready = GL_FALSE;
    ctx->arb_native_failed = GL_FALSE;
}

static GLuint compile_shader(struct mgl_context *ctx, GLenum type, const char *source) {
    GLuint shader;
    GLint status = GL_FALSE;
    if (ctx == NULL || ctx->native_dispatch == NULL || source == NULL ||
        ctx->native_dispatch->glesCreateShader == NULL ||
        ctx->native_dispatch->glesShaderSource == NULL ||
        ctx->native_dispatch->glesCompileShader == NULL ||
        ctx->native_dispatch->glesGetShaderiv == NULL)
        return 0u;
    shader = ctx->native_dispatch->glesCreateShader(type);
    if (shader == 0u)
        return 0u;
    ctx->native_dispatch->glesShaderSource(shader, 1, &source, NULL);
    ctx->native_dispatch->glesCompileShader(shader);
    ctx->native_dispatch->glesGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        static atomic_uint failures[2];
        unsigned stage = type == GL_VERTEX_SHADER ? 0u : 1u;
        unsigned logged_index =
            atomic_fetch_add_explicit(&failures[stage], 1u, memory_order_relaxed);
        if (logged_index < 16u) {
            char log[768] = {0};
            GLsizei written = 0;
            unsigned i;
            if (ctx->native_dispatch->glesGetShaderInfoLog != NULL)
                ctx->native_dispatch->glesGetShaderInfoLog(shader, (GLsizei)(sizeof(log) - 1u),
                                                           &written, log);
            if (written < 0)
                written = 0;
            if ((size_t)written >= sizeof(log))
                written = (GLsizei)(sizeof(log) - 1u);
            log[written] = '\0';
            for (i = 0u; i < (unsigned)written; ++i)
                if (log[i] == '\n' || log[i] == '\r')
                    log[i] = ' ';
            fprintf(stderr, "MGL_ARB_GLES_COMPILE_REJECTED stage=%s log=%s\n",
                    stage == 0u ? "vertex" : "fragment", log);
            if (logged_index < 4u) {
                unsigned failed_line = 0u;
                if (sscanf(log, "ERROR: %*u:%u:", &failed_line) == 1 && failed_line != 0u) {
                    const char *cursor = source;
                    unsigned current_line = 1u;
                    while (*cursor != '\0' && current_line < failed_line) {
                        if (*cursor == '\n')
                            ++current_line;
                        ++cursor;
                    }
                    if (current_line == failed_line && *cursor != '\0') {
                        char excerpt[256];
                        size_t size = 0u;
                        while (cursor[size] != '\0' && cursor[size] != '\n' &&
                               size + 1u < sizeof(excerpt)) {
                            excerpt[size] = cursor[size];
                            ++size;
                        }
                        excerpt[size] = '\0';
                        fprintf(stderr, "MGL_ARB_GLES_SOURCE_LINE stage=%s line=%u text=%s\n",
                                stage == 0u ? "vertex" : "fragment", failed_line, excerpt);
                    }
                }
            }
        }
        if (ctx->native_dispatch->glesDeleteShader != NULL)
            ctx->native_dispatch->glesDeleteShader(shader);
        return 0u;
    }
    return shader;
}

static const char default_vertex_source[] =
    "#version 100\n"
    "attribute vec4 mgl_a_position;\n"
    "attribute vec4 mgl_a_color;\n"
    "attribute vec4 mgl_a_secondary;\n"
    "attribute vec4 mgl_a_texcoord0;\n"
    "uniform mat4 mgl_u_model_view_projection;\n"
    "varying vec4 mgl_arb_v_color;\n"
    "varying vec4 mgl_arb_v_secondary;\n"
    "varying vec4 mgl_arb_v_texcoord0;\n"
    "varying vec4 mgl_arb_v_texcoord1;\n"
    "varying vec4 mgl_arb_v_texcoord2;\n"
    "varying vec4 mgl_arb_v_texcoord3;\n"
    "varying vec4 mgl_arb_v_texcoord4;\n"
    "varying vec4 mgl_arb_v_texcoord5;\n"
    "varying vec4 mgl_arb_v_texcoord6;\n"
    "varying vec4 mgl_arb_v_texcoord7;\n"
    "void main(){gl_Position=mgl_u_model_view_projection*mgl_a_position;"
    "mgl_arb_v_color=mgl_a_color;mgl_arb_v_secondary=mgl_a_secondary;"
    "mgl_arb_v_texcoord0=mgl_a_texcoord0;}\n";

static const char default_fragment_source[] = "#version 100\n"
                                              "precision mediump float;\n"
                                              "varying vec4 mgl_arb_v_color;\n"
                                              "varying vec4 mgl_arb_v_secondary;\n"
                                              "varying vec4 mgl_arb_v_texcoord0;\n"
                                              "varying vec4 mgl_arb_v_texcoord1;\n"
                                              "varying vec4 mgl_arb_v_texcoord2;\n"
                                              "varying vec4 mgl_arb_v_texcoord3;\n"
                                              "varying vec4 mgl_arb_v_texcoord4;\n"
                                              "varying vec4 mgl_arb_v_texcoord5;\n"
                                              "varying vec4 mgl_arb_v_texcoord6;\n"
                                              "varying vec4 mgl_arb_v_texcoord7;\n"
                                              "void main(){gl_FragColor=mgl_arb_v_color;}\n";

static void reflect_arb_program(struct mgl_context *ctx, struct mgl_program *program,
                                GLuint generic_mask) {
    unsigned i;
    mgl_program_init_metadata(program);
    program->native_name = ctx->arb_native_program;
    program->linked = GL_TRUE;
    program->active_generic_mask = generic_mask;
    for (i = 0u; i < MGL_COMPAT_SEM_COUNT; ++i)
        program->compat_attrib[i] = ctx->native_dispatch->glesGetAttribLocation(
            ctx->arb_native_program, mgl_compat_attribute_names[i]);
    for (i = 0u; i < 32u; ++i) {
        if ((generic_mask & (UINT32_C(1) << i)) != 0u) {
            char name[32];
            GLint location;
            (void)snprintf(name, sizeof(name), "mgl_g_attr%u", i);
            location = ctx->native_dispatch->glesGetAttribLocation(ctx->arb_native_program, name);
            if (location >= 0 && location < 32) {
                program->active_generic_mask &= ~(UINT32_C(1) << i);
                program->active_generic_mask |= UINT32_C(1) << (unsigned)location;
            }
        }
    }
    program->compat_uniform_model_view_projection = ctx->native_dispatch->glesGetUniformLocation(
        ctx->arb_native_program, "mgl_u_model_view_projection");
    program->compat_uniform_model_view =
        ctx->native_dispatch->glesGetUniformLocation(ctx->arb_native_program, "mgl_u_model_view");
    program->compat_uniform_projection =
        ctx->native_dispatch->glesGetUniformLocation(ctx->arb_native_program, "mgl_u_projection");
    program->compat_uniform_alpha_enabled = ctx->native_dispatch->glesGetUniformLocation(
        ctx->arb_native_program, "mgl_u_alpha_test_enabled");
    program->compat_uniform_alpha_func = ctx->native_dispatch->glesGetUniformLocation(
        ctx->arb_native_program, "mgl_u_alpha_test_func");
    program->compat_uniform_alpha_ref = ctx->native_dispatch->glesGetUniformLocation(
        ctx->arb_native_program, "mgl_u_alpha_test_ref");
    ctx->arb_uniform_vp_env =
        ctx->native_dispatch->glesGetUniformLocation(ctx->arb_native_program, "mgl_arb_vp_env[0]");
    ctx->arb_uniform_vp_local = ctx->native_dispatch->glesGetUniformLocation(
        ctx->arb_native_program, "mgl_arb_vp_local[0]");
    ctx->arb_uniform_fp_env =
        ctx->native_dispatch->glesGetUniformLocation(ctx->arb_native_program, "mgl_arb_fp_env[0]");
    ctx->arb_uniform_fp_local = ctx->native_dispatch->glesGetUniformLocation(
        ctx->arb_native_program, "mgl_arb_fp_local[0]");
    for (unsigned unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit) {
        char name[40];
        (void)snprintf(name, sizeof(name), "mgl_arb_texture%u", unit);
        ctx->arb_uniform_textures[unit] =
            ctx->native_dispatch->glesGetUniformLocation(ctx->arb_native_program, name);
    }
}

static int ensure_native_pair(struct mgl_context *ctx, const struct mgl_arb_program *vp,
                              const struct mgl_arb_program *fp) {
    const char *vs = vp != NULL ? vp->translated_source : default_vertex_source;
    const char *fs = fp != NULL ? fp->translated_source : default_fragment_source;
    GLuint vertex_shader;
    GLuint fragment_shader;
    GLuint program;
    GLint status = GL_FALSE;
    GLuint generic_mask = vp != NULL ? vp->generic_mask : 0u;

    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesCreateProgram == NULL ||
        ctx->native_dispatch->glesAttachShader == NULL ||
        ctx->native_dispatch->glesLinkProgram == NULL ||
        ctx->native_dispatch->glesGetProgramiv == NULL ||
        ctx->native_dispatch->glesGetAttribLocation == NULL ||
        ctx->native_dispatch->glesGetUniformLocation == NULL)
        return 0;

    delete_native_cache(ctx);
    vertex_shader = compile_shader(ctx, GL_VERTEX_SHADER, vs);
    if (vertex_shader == 0u)
        goto fail;
    fragment_shader = compile_shader(ctx, GL_FRAGMENT_SHADER, fs);
    if (fragment_shader == 0u) {
        if (ctx->native_dispatch->glesDeleteShader != NULL)
            ctx->native_dispatch->glesDeleteShader(vertex_shader);
        goto fail;
    }
    program = ctx->native_dispatch->glesCreateProgram();
    if (program == 0u) {
        if (ctx->native_dispatch->glesDeleteShader != NULL) {
            ctx->native_dispatch->glesDeleteShader(vertex_shader);
            ctx->native_dispatch->glesDeleteShader(fragment_shader);
        }
        goto fail;
    }
    ctx->native_dispatch->glesAttachShader(program, vertex_shader);
    ctx->native_dispatch->glesAttachShader(program, fragment_shader);
    if (generic_mask != 0u) {
        unsigned index;
        if (ctx->native_dispatch->glesBindAttribLocation == NULL) {
            if (ctx->native_dispatch->glesDeleteProgram != NULL)
                ctx->native_dispatch->glesDeleteProgram(program);
            if (ctx->native_dispatch->glesDeleteShader != NULL) {
                ctx->native_dispatch->glesDeleteShader(vertex_shader);
                ctx->native_dispatch->glesDeleteShader(fragment_shader);
            }
            goto fail;
        }
        for (index = 0u; index < 32u; ++index) {
            if ((generic_mask & (UINT32_C(1) << index)) != 0u) {
                char name[32];
                (void)snprintf(name, sizeof(name), "mgl_g_attr%u", index);
                ctx->native_dispatch->glesBindAttribLocation(program, index, name);
            }
        }
    }
    ctx->native_dispatch->glesLinkProgram(program);
    ctx->native_dispatch->glesGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        static atomic_uint link_failures;
        if (atomic_fetch_add_explicit(&link_failures, 1u, memory_order_relaxed) < 16u) {
            char log[768] = {0};
            GLsizei written = 0;
            unsigned i;
            if (ctx->native_dispatch->glesGetProgramInfoLog != NULL)
                ctx->native_dispatch->glesGetProgramInfoLog(program, (GLsizei)(sizeof(log) - 1u),
                                                            &written, log);
            if (written < 0)
                written = 0;
            if ((size_t)written >= sizeof(log))
                written = (GLsizei)(sizeof(log) - 1u);
            log[written] = '\0';
            for (i = 0u; i < (unsigned)written; ++i)
                if (log[i] == '\n' || log[i] == '\r')
                    log[i] = ' ';
            fprintf(stderr, "MGL_ARB_GLES_LINK_REJECTED log=%s\n", log);
        }
        if (ctx->native_dispatch->glesDeleteProgram != NULL)
            ctx->native_dispatch->glesDeleteProgram(program);
        if (ctx->native_dispatch->glesDeleteShader != NULL) {
            ctx->native_dispatch->glesDeleteShader(vertex_shader);
            ctx->native_dispatch->glesDeleteShader(fragment_shader);
        }
        goto fail;
    }

    ctx->arb_native_program = program;
    ctx->arb_uniform_uploaded_mask = 0u;
    ctx->arb_sampler_uniforms_uploaded = GL_FALSE;
    ctx->arb_vertex_shader_native = vertex_shader;
    ctx->arb_fragment_shader_native = fragment_shader;
    ctx->arb_cached_vertex_name = vp != NULL ? vp->name : 0u;
    ctx->arb_cached_fragment_name = fp != NULL ? fp->name : 0u;
    ctx->arb_cached_vertex_serial = vp != NULL ? vp->serial : 0u;
    ctx->arb_cached_fragment_serial = fp != NULL ? fp->serial : 0u;
    ctx->arb_cached_stage_mask = (vp != NULL ? 1u : 0u) | (fp != NULL ? 2u : 0u);
    ctx->arb_resolved_revision = ctx->share_group->arb_programs_revision;
    ctx->arb_vp_env_count =
        vp != NULL && vp->max_env_index != UINT_MAX ? vp->max_env_index + 1u : 0u;
    ctx->arb_vp_local_count =
        vp != NULL && vp->max_local_index != UINT_MAX ? vp->max_local_index + 1u : 0u;
    ctx->arb_fp_env_count =
        fp != NULL && fp->max_env_index != UINT_MAX ? fp->max_env_index + 1u : 0u;
    ctx->arb_fp_local_count =
        fp != NULL && fp->max_local_index != UINT_MAX ? fp->max_local_index + 1u : 0u;
    reflect_arb_program(ctx, &ctx->arb_linked_program, generic_mask);
    ctx->arb_native_ready = GL_TRUE;
    ctx->arb_native_failed = GL_FALSE;
    return 1;

fail:
    ctx->arb_native_failed = GL_TRUE;
    return 0;
}

struct mgl_program *mgl_arb_current_linked_program(struct mgl_context *ctx) {
    struct mgl_arb_program *vp = NULL;
    struct mgl_arb_program *fp = NULL;
    if (ctx == NULL ||
        (!ctx->compat.arb_vertex_program_enabled && !ctx->compat.arb_fragment_program_enabled))
        return NULL;
    /* Called under the share-group lock by draw dispatch.  A revision hit
     * guarantees that neither bound name's source nor its lifetime changed,
     * so no shared-list traversal or source serial validation is needed. */
    if (ctx->arb_native_ready &&
        ctx->arb_cached_stage_mask == ((ctx->compat.arb_vertex_program_enabled ? 1u : 0u) |
                                       (ctx->compat.arb_fragment_program_enabled ? 2u : 0u)) &&
        ctx->arb_cached_vertex_name ==
            (ctx->compat.arb_vertex_program_enabled ? ctx->arb_vertex_program_binding : 0u) &&
        ctx->arb_cached_fragment_name ==
            (ctx->compat.arb_fragment_program_enabled ? ctx->arb_fragment_program_binding : 0u) &&
        ctx->arb_resolved_revision == ctx->share_group->arb_programs_revision) {
        mgl_perf_diag_program_cache(ctx, MGL_PERF_ARB_CACHE_HIT);
        return &ctx->arb_linked_program;
    }
    mgl_perf_diag_program_cache(ctx, MGL_PERF_ARB_CACHE_RESOLVE);
    if (ctx->compat.arb_vertex_program_enabled) {
        vp = bound_program(ctx, GL_VERTEX_PROGRAM_ARB);
        if (vp == NULL || !vp->valid || vp->target != GL_VERTEX_PROGRAM_ARB)
            return NULL;
    }
    if (ctx->compat.arb_fragment_program_enabled) {
        fp = bound_program(ctx, GL_FRAGMENT_PROGRAM_ARB);
        if (fp == NULL || !fp->valid || fp->target != GL_FRAGMENT_PROGRAM_ARB)
            return NULL;
    }
    {
        const GLuint vertex_name = vp != NULL ? vp->name : 0u;
        const GLuint fragment_name = fp != NULL ? fp->name : 0u;
        const GLuint vertex_serial = vp != NULL ? vp->serial : 0u;
        const GLuint fragment_serial = fp != NULL ? fp->serial : 0u;
        const unsigned stage_mask = (vp != NULL ? 1u : 0u) | (fp != NULL ? 2u : 0u);
        if (ctx->arb_native_program != 0u && ctx->arb_cached_vertex_name == vertex_name &&
            ctx->arb_cached_fragment_name == fragment_name &&
            ctx->arb_cached_vertex_serial == vertex_serial &&
            ctx->arb_cached_fragment_serial == fragment_serial &&
            ctx->arb_cached_stage_mask == stage_mask) {
            ctx->arb_native_ready = GL_TRUE;
        } else {
            arb_pair_save_active(ctx);
            if (!arb_pair_restore(ctx, vertex_name, fragment_name, vertex_serial, fragment_serial,
                                  stage_mask)) {
                mgl_perf_diag_program_cache(ctx, MGL_PERF_ARB_CACHE_RELINK);
                if (!ensure_native_pair(ctx, vp, fp))
                    return NULL;
            }
        }
    }
    /* A shared object unrelated to this pair may have changed; renew the
     * fast-path revision after validating both bound object serials. */
    ctx->arb_resolved_revision = ctx->share_group->arb_programs_revision;
    return &ctx->arb_linked_program;
}

/* Per-object revisions identify changes to the 96 vec4 ARB parameters.
 * Linked GLES pairs retain independent uploaded-revision snapshots. */
void mgl_arb_realize_uniforms(struct mgl_context *ctx, const struct mgl_program *program) {
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_DRAW_ARB_UNIFORMS);
    struct mgl_arb_program *vp;
    struct mgl_arb_program *fp;
    unsigned set = 0u, reused = 0u;
    uint64_t revision;
    if (ctx == NULL || program != &ctx->arb_linked_program || ctx->native_dispatch == NULL)
        return;
    vp = ctx->arb_cached_vertex_name == 0u ? default_program(ctx, GL_VERTEX_PROGRAM_ARB)
                                           : mgl_arb_program_find(ctx, ctx->arb_cached_vertex_name);
    fp = ctx->arb_cached_fragment_name == 0u
             ? default_program(ctx, GL_FRAGMENT_PROGRAM_ARB)
             : mgl_arb_program_find(ctx, ctx->arb_cached_fragment_name);
    if (ctx->native_dispatch->glesUniform4fv != NULL) {
        if (ctx->arb_uniform_vp_env >= 0 && ctx->arb_vp_env_count != 0u) {
            revision = ctx->arb_vertex_env_revision;
            if (!(ctx->arb_uniform_uploaded_mask & 1u) ||
                ctx->arb_uploaded_vp_env_revision != revision) {
                ctx->native_dispatch->glesUniform4fv(ctx->arb_uniform_vp_env,
                                                     (GLsizei)ctx->arb_vp_env_count,
                                                     &ctx->arb_vertex_env[0][0]);
                ctx->arb_uploaded_vp_env_revision = revision;
                ctx->arb_uniform_uploaded_mask |= 1u;
                ++set;
            } else
                ++reused;
        }
        if (ctx->arb_uniform_vp_local >= 0 && vp != NULL && ctx->arb_vp_local_count != 0u) {
            revision = ctx->arb_cached_vertex_name == 0u ? ctx->arb_default_vertex_local_revision
                                                         : vp->local_revision;
            if (!(ctx->arb_uniform_uploaded_mask & 2u) ||
                ctx->arb_uploaded_vp_local_revision != revision) {
                ctx->native_dispatch->glesUniform4fv(
                    ctx->arb_uniform_vp_local, (GLsizei)ctx->arb_vp_local_count,
                    ctx->arb_cached_vertex_name == 0u ? &ctx->arb_default_vertex_local[0][0]
                                                      : &vp->local[0][0]);
                ctx->arb_uploaded_vp_local_revision = revision;
                ctx->arb_uniform_uploaded_mask |= 2u;
                ++set;
            } else
                ++reused;
        }
        if (ctx->arb_uniform_fp_env >= 0 && ctx->arb_fp_env_count != 0u) {
            revision = ctx->arb_fragment_env_revision;
            if (!(ctx->arb_uniform_uploaded_mask & 4u) ||
                ctx->arb_uploaded_fp_env_revision != revision) {
                ctx->native_dispatch->glesUniform4fv(ctx->arb_uniform_fp_env,
                                                     (GLsizei)ctx->arb_fp_env_count,
                                                     &ctx->arb_fragment_env[0][0]);
                ctx->arb_uploaded_fp_env_revision = revision;
                ctx->arb_uniform_uploaded_mask |= 4u;
                ++set;
            } else
                ++reused;
        }
        if (ctx->arb_uniform_fp_local >= 0 && fp != NULL && ctx->arb_fp_local_count != 0u) {
            revision = ctx->arb_cached_fragment_name == 0u
                           ? ctx->arb_default_fragment_local_revision
                           : fp->local_revision;
            if (!(ctx->arb_uniform_uploaded_mask & 8u) ||
                ctx->arb_uploaded_fp_local_revision != revision) {
                ctx->native_dispatch->glesUniform4fv(
                    ctx->arb_uniform_fp_local, (GLsizei)ctx->arb_fp_local_count,
                    ctx->arb_cached_fragment_name == 0u ? &ctx->arb_default_fragment_local[0][0]
                                                        : &fp->local[0][0]);
                ctx->arb_uploaded_fp_local_revision = revision;
                ctx->arb_uniform_uploaded_mask |= 8u;
                ++set;
            } else
                ++reused;
        }
    }
    if (ctx->native_dispatch->glesUniform1i != NULL) {
        if (!ctx->arb_sampler_uniforms_uploaded) {
            for (unsigned unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit)
                if (ctx->arb_uniform_textures[unit] >= 0) {
                    ctx->native_dispatch->glesUniform1i(ctx->arb_uniform_textures[unit],
                                                        (GLint)unit);
                    ++set;
                }
            ctx->arb_sampler_uniforms_uploaded = GL_TRUE;
        } else {
            for (unsigned unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit)
                if (ctx->arb_uniform_textures[unit] >= 0)
                    ++reused;
        }
    }
    mgl_perf_diag_arb_uniform(ctx, set, reused);
}

void glGenProgramsARB(GLsizei n, GLuint *programs) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (n < 0 || (n > 0 && programs == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < n; ++i) {
        GLuint name = next_nonzero(&ctx->share_group->next_arb_program_name);
        if (arb_program_create(ctx, name) == NULL) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        programs[i] = name;
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glDeleteProgramsARB(GLsizei n, const GLuint *programs) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (n < 0 || (n > 0 && programs == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < n; ++i) {
        struct mgl_arb_program **link;
        if (programs[i] == 0u)
            continue;
        link = &ctx->share_group->arb_programs;
        while (*link != NULL && (*link)->name != programs[i])
            link = &(*link)->next;
        if (*link != NULL) {
            struct mgl_arb_program *victim = *link;
            *link = victim->next;
            ++ctx->share_group->arb_programs_revision;
            if (ctx->arb_vertex_program_binding == victim->name)
                ctx->arb_vertex_program_binding = 0u;
            if (ctx->arb_fragment_program_binding == victim->name)
                ctx->arb_fragment_program_binding = 0u;
            arb_program_free(victim);
            mgl_arb_invalidate_native_program(ctx);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

GLboolean glIsProgramARB(GLuint program) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_arb_program *object;
    if (ctx == NULL)
        return GL_FALSE;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    object = mgl_arb_program_find(ctx, program);
    /* Generated names are reserved until their first successful binding. */
    return object != NULL && object->target != 0u ? GL_TRUE : GL_FALSE;
}

void glBindProgramARB(GLenum target, GLuint program_name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_arb_program *program = NULL;
    GLuint *slot;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    slot = binding_slot(ctx, target);
    if (slot == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    if (program_name != 0u) {
        program = mgl_arb_program_find(ctx, program_name);
        if (program == NULL)
            program = arb_program_create(ctx, program_name);
        if (program == NULL) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        if (program->target != 0u && program->target != target) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        program->target = target;
    }
    if (*slot != program_name) {
        *slot = program_name;
        mgl_arb_invalidate_native_program(ctx);
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glProgramStringARB(GLenum target, GLenum format, GLsizei len, const void *string) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_arb_program *program;
    struct mgl_arb_translation translation;
    char *source;
    GLuint *slot;
    GLint error_position = -1;
    char error_text[256];
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    slot = binding_slot(ctx, target);
    if (slot == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (format != GL_PROGRAM_FORMAT_ASCII_ARB) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (len < 0 || (len > 0 && string == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    source = dup_n((const char *)string, (size_t)len);
    if (source == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    memset(&translation, 0, sizeof(translation));
    if (!mgl_arb_translate(target, source, &translation, &error_position, error_text,
                           sizeof(error_text))) {
        static atomic_uint rejected_program_reports = ATOMIC_VAR_INIT(0u);
        if (atomic_fetch_add_explicit(&rejected_program_reports, 1u, memory_order_relaxed) < 24u)
            (void)fprintf(stderr,
                          "MGL_ARB_PROGRAM_REJECTED target=0x%x position=%d reason=%.240s\n",
                          (unsigned)target, (int)error_position, error_text);
        free(source);
        set_program_error(ctx, error_position, error_text);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_arb_program_string(ctx, target, format, len, string)) {
        free(source);
        mgl_arb_translation_destroy(&translation);
        set_program_error(ctx, -1, "");
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    program = bound_program(ctx, target);
    if (program == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        free(source);
        mgl_arb_translation_destroy(&translation);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    free(program->source);
    free(program->translated_source);
    program->source = source;
    program->translated_source = translation.source;
    translation.source = NULL;
    program->source_length = len;
    program->valid = GL_TRUE;
    program->generic_mask = translation.generic_mask;
    program->max_env_index = translation.uses_env ? translation.max_env_index : UINT_MAX;
    program->max_local_index = translation.uses_local ? translation.max_local_index : UINT_MAX;
    program->instruction_count = translation.instruction_count;
    program->temporary_count = translation.temporary_count;
    program->parameter_count = translation.parameter_count;
    program->attrib_count = translation.attrib_count;
    program->address_count = translation.address_count;
    program->texture_count = translation.texture_count;
    program->texture_indirections = translation.texture_indirections;
    ++ctx->share_group->arb_programs_revision;
    if (program->name != 0u)
        program->serial = (GLuint)ctx->share_group->arb_programs_revision;
    else {
        ++program->serial;
        if (program->serial == 0u)
            ++program->serial;
    }
    set_program_error(ctx, -1, "");
    mgl_arb_invalidate_native_program(ctx);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    mgl_arb_translation_destroy(&translation);
}

static int validate_parameter_index(struct mgl_context *ctx, GLenum target, GLuint index) {
    if (!valid_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (index >= MGL_ARB_MAX_PROGRAM_PARAMETERS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    return 1;
}

static GLfloat (*default_local_parameters(struct mgl_context *ctx, GLenum target))[4] {
    return target == GL_VERTEX_PROGRAM_ARB ? ctx->arb_default_vertex_local
                                           : ctx->arb_default_fragment_local;
}

static GLfloat (*bound_local_parameters(struct mgl_context *ctx, GLenum target))[4] {
    GLuint *slot = binding_slot(ctx, target);
    struct mgl_arb_program *program;
    if (slot == NULL)
        return NULL;
    if (*slot == 0u)
        return default_local_parameters(ctx, target);
    program = mgl_arb_program_find(ctx, *slot);
    return program != NULL ? program->local : NULL;
}

static int reject_parameter_inside_begin(struct mgl_context *ctx) {
    if (ctx != NULL && ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 1;
    }
    return 0;
}

void glProgramEnvParameter4fvARB(GLenum target, GLuint index, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat(*env)[4];
    if (ctx == NULL)
        return;
    if (reject_parameter_inside_begin(ctx))
        return;
    if (params == NULL || !validate_parameter_index(ctx, target, index)) {
        if (params == NULL)
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_arb_program_parameters(ctx, target, index, 1, params, GL_FALSE))
        return;
    env = env_parameters(ctx, target);
    if (memcmp(env[index], params, 4u * sizeof(GLfloat)) != 0) {
        memcpy(env[index], params, 4u * sizeof(GLfloat));
        if (target == GL_VERTEX_PROGRAM_ARB)
            ++ctx->arb_vertex_env_revision;
        else
            ++ctx->arb_fragment_env_revision;
    }
}

void glProgramEnvParameter4fARB(GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z,
                                GLfloat w) {
    const GLfloat params[4] = {x, y, z, w};
    glProgramEnvParameter4fvARB(target, index, params);
}

void glProgramEnvParameter4dvARB(GLenum target, GLuint index, const GLdouble *params) {
    GLfloat values[4];
    unsigned i;
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (reject_parameter_inside_begin(ctx))
        return;
    if (params == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0u; i < 4u; ++i)
        values[i] = (GLfloat)params[i];
    glProgramEnvParameter4fvARB(target, index, values);
}

void glProgramEnvParameter4dARB(GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z,
                                GLdouble w) {
    const GLdouble params[4] = {x, y, z, w};
    glProgramEnvParameter4dvARB(target, index, params);
}

void glProgramEnvParameters4fvEXT(GLenum target, GLuint index, GLsizei count,
                                  const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat(*env)[4];
    size_t parameter_count;
    if (ctx == NULL)
        return;
    if (reject_parameter_inside_begin(ctx))
        return;
    if (!valid_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (count < 0 || (GLuint)count > MGL_ARB_MAX_PROGRAM_PARAMETERS ||
        index > MGL_ARB_MAX_PROGRAM_PARAMETERS - (GLuint)count) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (count == 0)
        return;
    if (params == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_arb_program_parameters(ctx, target, index, count, params, GL_FALSE))
        return;
    parameter_count = (size_t)count;
    env = env_parameters(ctx, target);
    if (memcmp(env[index], params, parameter_count * 4u * sizeof(GLfloat)) != 0) {
        memcpy(env[index], params, parameter_count * 4u * sizeof(GLfloat));
        if (target == GL_VERTEX_PROGRAM_ARB)
            ++ctx->arb_vertex_env_revision;
        else
            ++ctx->arb_fragment_env_revision;
    }
}

void glProgramLocalParameter4fvARB(GLenum target, GLuint index, const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat(*local)[4];
    if (ctx == NULL)
        return;
    if (reject_parameter_inside_begin(ctx))
        return;
    if (params == NULL || !validate_parameter_index(ctx, target, index)) {
        if (params == NULL)
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_arb_program_parameters(ctx, target, index, 1, params, GL_TRUE))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    local = bound_local_parameters(ctx, target);
    if (local == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (memcmp(local[index], params, 4u * sizeof(GLfloat)) != 0) {
        memcpy(local[index], params, 4u * sizeof(GLfloat));
        if (target == GL_VERTEX_PROGRAM_ARB && ctx->arb_vertex_program_binding == 0u)
            ++ctx->arb_default_vertex_local_revision;
        else if (target == GL_FRAGMENT_PROGRAM_ARB && ctx->arb_fragment_program_binding == 0u)
            ++ctx->arb_default_fragment_local_revision;
        else {
            struct mgl_arb_program *bound = bound_program(ctx, target);
            if (bound != NULL)
                ++bound->local_revision;
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glProgramLocalParameter4fARB(GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z,
                                  GLfloat w) {
    const GLfloat params[4] = {x, y, z, w};
    glProgramLocalParameter4fvARB(target, index, params);
}

void glProgramLocalParameter4dvARB(GLenum target, GLuint index, const GLdouble *params) {
    GLfloat values[4];
    unsigned i;
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (reject_parameter_inside_begin(ctx))
        return;
    if (params == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0u; i < 4u; ++i)
        values[i] = (GLfloat)params[i];
    glProgramLocalParameter4fvARB(target, index, values);
}

void glProgramLocalParameter4dARB(GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z,
                                  GLdouble w) {
    const GLdouble params[4] = {x, y, z, w};
    glProgramLocalParameter4dvARB(target, index, params);
}

void glProgramLocalParameters4fvEXT(GLenum target, GLuint index, GLsizei count,
                                    const GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat(*local)[4];
    size_t parameter_count;
    if (ctx == NULL)
        return;
    if (reject_parameter_inside_begin(ctx))
        return;
    if (!valid_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (count < 0 || (GLuint)count > MGL_ARB_MAX_PROGRAM_PARAMETERS ||
        index > MGL_ARB_MAX_PROGRAM_PARAMETERS - (GLuint)count) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (count == 0)
        return;
    if (params == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_arb_program_parameters(ctx, target, index, count, params, GL_TRUE))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    local = bound_local_parameters(ctx, target);
    if (local == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    parameter_count = (size_t)count;
    if (memcmp(local[index], params, parameter_count * 4u * sizeof(GLfloat)) != 0) {
        memcpy(local[index], params, parameter_count * 4u * sizeof(GLfloat));
        if (target == GL_VERTEX_PROGRAM_ARB && ctx->arb_vertex_program_binding == 0u)
            ++ctx->arb_default_vertex_local_revision;
        else if (target == GL_FRAGMENT_PROGRAM_ARB && ctx->arb_fragment_program_binding == 0u)
            ++ctx->arb_default_fragment_local_revision;
        else {
            struct mgl_arb_program *bound = bound_program(ctx, target);
            if (bound != NULL)
                ++bound->local_revision;
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetProgramEnvParameterfvARB(GLenum target, GLuint index, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (reject_parameter_inside_begin(ctx))
        return;
    if (params == NULL)
        return;
    if (!validate_parameter_index(ctx, target, index))
        return;
    memcpy(params, env_parameters(ctx, target)[index], 4u * sizeof(GLfloat));
}

void glGetProgramEnvParameterdvARB(GLenum target, GLuint index, GLdouble *params) {
    GLfloat values[4];
    unsigned i;
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (reject_parameter_inside_begin(ctx))
        return;
    if (params == NULL)
        return;
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetProgramEnvParameterfvARB(target, index, values);
    if (ctx->errors.sequence != error_sequence_before)
        return;
    for (i = 0u; i < 4u; ++i)
        params[i] = (GLdouble)values[i];
}

void glGetProgramLocalParameterfvARB(GLenum target, GLuint index, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat(*local)[4];
    if (ctx == NULL)
        return;
    if (reject_parameter_inside_begin(ctx))
        return;
    if (params == NULL)
        return;
    if (!validate_parameter_index(ctx, target, index))
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    local = bound_local_parameters(ctx, target);
    if (local == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    memcpy(params, local[index], 4u * sizeof(GLfloat));
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetProgramLocalParameterdvARB(GLenum target, GLuint index, GLdouble *params) {
    GLfloat values[4];
    unsigned i;
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (reject_parameter_inside_begin(ctx))
        return;
    if (params == NULL)
        return;
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetProgramLocalParameterfvARB(target, index, values);
    if (ctx->errors.sequence != error_sequence_before)
        return;
    for (i = 0u; i < 4u; ++i)
        params[i] = (GLdouble)values[i];
}

static GLint arb_program_attribute_limit(struct mgl_context *ctx) {
    GLint limit = 16;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegerv != NULL) {
        GLint native_limit = 0;
        ctx->native_dispatch->glesGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &native_limit);
        if (native_limit > 0)
            limit = native_limit;
    }
    return limit < (GLint)MGL_ARB_MAX_PROGRAM_ATTRIBS ? limit : (GLint)MGL_ARB_MAX_PROGRAM_ATTRIBS;
}

void glGetProgramivARB(GLenum target, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_arb_program *program;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    if (!valid_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    program = bound_program(ctx, target);
    switch (pname) {
        case GL_PROGRAM_LENGTH_ARB:
            *params = program != NULL ? program->source_length : 0;
            return;
        case GL_PROGRAM_FORMAT_ARB:
            *params = (GLint)GL_PROGRAM_FORMAT_ASCII_ARB;
            return;
        case GL_PROGRAM_BINDING_ARB:
            *params = (GLint)*binding_slot(ctx, target);
            return;
        case GL_MAX_PROGRAM_ENV_PARAMETERS_ARB:
        case GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB:
            *params = (GLint)MGL_ARB_MAX_PROGRAM_PARAMETERS;
            return;
        case GL_PROGRAM_INSTRUCTIONS_ARB:
        case GL_PROGRAM_NATIVE_INSTRUCTIONS_ARB:
            *params = program != NULL ? (GLint)program->instruction_count : 0;
            return;
        case GL_MAX_PROGRAM_INSTRUCTIONS_ARB:
        case GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB:
            *params = (GLint)MGL_ARB_MAX_PROGRAM_INSTRUCTIONS;
            return;
        case GL_PROGRAM_TEMPORARIES_ARB:
        case GL_PROGRAM_NATIVE_TEMPORARIES_ARB:
            *params = program != NULL ? (GLint)program->temporary_count : 0;
            return;
        case GL_MAX_PROGRAM_TEMPORARIES_ARB:
        case GL_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB:
            *params = (GLint)MGL_ARB_MAX_PROGRAM_TEMPORARIES;
            return;
        case GL_PROGRAM_PARAMETERS_ARB:
        case GL_PROGRAM_NATIVE_PARAMETERS_ARB:
            *params = program != NULL ? (GLint)program->parameter_count : 0;
            return;
        case GL_MAX_PROGRAM_PARAMETERS_ARB:
        case GL_MAX_PROGRAM_NATIVE_PARAMETERS_ARB:
            *params = (GLint)MGL_ARB_MAX_PROGRAM_PARAMETERS;
            return;
        case GL_PROGRAM_ATTRIBS_ARB:
        case GL_PROGRAM_NATIVE_ATTRIBS_ARB:
            *params = program != NULL ? (GLint)program->attrib_count : 0;
            return;
        case GL_MAX_PROGRAM_ATTRIBS_ARB:
        case GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB:
            *params = arb_program_attribute_limit(ctx);
            return;
        case GL_PROGRAM_ADDRESS_REGISTERS_ARB:
        case GL_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB:
            *params = program != NULL ? (GLint)program->address_count : 0;
            return;
        case GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB:
        case GL_MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB:
            *params = target == GL_VERTEX_PROGRAM_ARB ? 1 : 0;
            return;
        case GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB:
            *params = program != NULL && program->name != 0u && program->valid &&
                              program->instruction_count <= MGL_ARB_MAX_PROGRAM_INSTRUCTIONS &&
                              program->temporary_count <= MGL_ARB_MAX_PROGRAM_TEMPORARIES &&
                              program->parameter_count <= MGL_ARB_MAX_PROGRAM_PARAMETERS &&
                              program->attrib_count <= (GLuint)arb_program_attribute_limit(ctx) &&
                              program->address_count <= (target == GL_VERTEX_PROGRAM_ARB ? 1u : 0u)
                          ? GL_TRUE
                          : GL_FALSE;
            return;
        case GL_PROGRAM_ALU_INSTRUCTIONS_ARB:
        case GL_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB:
            if (target != GL_FRAGMENT_PROGRAM_ARB)
                break;
            *params =
                program != NULL ? (GLint)(program->instruction_count - program->texture_count) : 0;
            return;
        case GL_PROGRAM_TEX_INSTRUCTIONS_ARB:
        case GL_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB:
            if (target != GL_FRAGMENT_PROGRAM_ARB)
                break;
            *params = program != NULL ? (GLint)program->texture_count : 0;
            return;
        case GL_PROGRAM_TEX_INDIRECTIONS_ARB:
        case GL_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB:
            if (target != GL_FRAGMENT_PROGRAM_ARB)
                break;
            *params = program != NULL && program->valid ? (GLint)program->texture_indirections : 0;
            return;
        case GL_MAX_PROGRAM_ALU_INSTRUCTIONS_ARB:
        case GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB:
            if (target != GL_FRAGMENT_PROGRAM_ARB)
                break;
            *params = (GLint)MGL_ARB_MAX_PROGRAM_INSTRUCTIONS;
            return;
        case GL_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB:
        case GL_MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB:
        case GL_MAX_PROGRAM_TEX_INDIRECTIONS_ARB:
        case GL_MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB:
            if (target != GL_FRAGMENT_PROGRAM_ARB)
                break;
            *params = (GLint)MGL_ARB_MAX_PROGRAM_TEX_INSTRUCTIONS;
            return;
        default:
            break;
    }
    mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
}

void glGetProgramStringARB(GLenum target, GLenum pname, void *string) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_arb_program *program;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (string == NULL)
        return;
    if (!valid_target(target) || pname != GL_PROGRAM_STRING_ARB) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    program = bound_program(ctx, target);
    if (program == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (program->source_length > 0 && program->source != NULL)
        memcpy(string, program->source, (size_t)program->source_length);
}

#if defined(__ANDROID__)
#define _GNU_SOURCE
#endif

#include "core/context.h"
#include "objects/object_store.h"
#include "compat/arb_program.h"
#include "objects/query.h"
#include "objects/nv_fence.h"
#include "shader/shader_store.h"
#include "list/display_list.h"
#include "texture/texture_store.h"
#include "texture/depth_readback.h"

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if defined(__ANDROID__)
#include <sched.h>
#endif

static _Thread_local struct mgl_context *tls_current_context;

#if defined(__ANDROID__)
static _Thread_local int tls_render_affinity_set;

static void mgl_apply_render_thread_affinity(void) {
    const char *text;
    char *end;
    unsigned long long requested;
    cpu_set_t allowed;
    cpu_set_t selected;
    unsigned cpu;

    if (tls_render_affinity_set)
        return;
    text = getenv("WINE_ANDROID_PERF_CPU_MASK");
    if (text == NULL || *text == '\0' || *text == '-')
        return;
    errno = 0;
    requested = strtoull(text, &end, 0);
    if (errno == ERANGE || end == text || *end != '\0' || requested == 0)
        return;
    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0)
        return;

    CPU_ZERO(&selected);
    for (cpu = 0; cpu < CPU_SETSIZE && cpu < sizeof(requested) * CHAR_BIT; ++cpu) {
        if ((requested & (1ULL << cpu)) && CPU_ISSET(cpu, &allowed))
            CPU_SET(cpu, &selected);
    }
    if (CPU_COUNT(&selected) == 0)
        return;
    if (sched_setaffinity(0, sizeof(selected), &selected) == 0)
        tls_render_affinity_set = 1;
}
#else
static void mgl_apply_render_thread_affinity(void) {
}
#endif

void mgl_context_init_image_units(struct mgl_context *ctx) {
    unsigned i;
    if (ctx == NULL)
        return;
    for (i = 0u; i < MGL_IMAGE_UNIT_CAPACITY; ++i) {
        ctx->image_units[i].texture = 0u;
        ctx->image_units[i].level = 0;
        ctx->image_units[i].layered = GL_FALSE;
        ctx->image_units[i].layer = 0;
        ctx->image_units[i].access = GL_READ_ONLY;
        ctx->image_units[i].format = GL_R8;
    }
}

GLuint mgl_context_image_unit_limit(struct mgl_context *ctx) {
    GLint native_limit = 0;
    if (ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesBindImageTexture == NULL ||
        ctx->native_dispatch->glesGetIntegerv == NULL)
        return 0u;
    ctx->native_dispatch->glesGetIntegerv(GL_MAX_IMAGE_UNITS, &native_limit);
    if (native_limit <= 0)
        return 0u;
    if ((GLuint)native_limit > MGL_IMAGE_UNIT_CAPACITY)
        return MGL_IMAGE_UNIT_CAPACITY;
    return (GLuint)native_limit;
}

GLuint mgl_context_vertex_binding_limit(struct mgl_context *ctx) {
    GLint limit;
    if (ctx == NULL)
        return 0u;
    if (ctx->native_max_vertex_bindings <= 0) {
        limit = 0;
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegerv != NULL)
            ctx->native_dispatch->glesGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &limit);
        if (limit <= 0) {
            limit = ctx->native_max_vertex_attribs;
            if (limit <= 0 && ctx->native_dispatch != NULL &&
                ctx->native_dispatch->glesGetIntegerv != NULL)
                ctx->native_dispatch->glesGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &limit);
        }
        if (limit <= 0 || limit > (GLint)MGL_MAX_VERTEX_ATTRIBS)
            limit = (GLint)MGL_MAX_VERTEX_ATTRIBS;
        ctx->native_max_vertex_bindings = limit;
    }
    return (GLuint)ctx->native_max_vertex_bindings;
}

static void free_share_objects(struct mgl_share_group *group) {
    struct mgl_buffer *buffer = group->buffers;
    struct mgl_renderbuffer *renderbuffer = group->renderbuffers;
    struct mgl_texture *texture = group->textures;
    struct mgl_sampler *sampler = group->samplers;
    struct mgl_sync *sync = group->syncs;
    struct mgl_shader *shader = group->shaders;
    struct mgl_program *program = group->programs;
    struct mgl_object_label *label = group->object_labels;
    while (buffer != NULL) {
        struct mgl_buffer *next = buffer->next;
        free(buffer->shadow);
        free(buffer);
        buffer = next;
    }
    while (renderbuffer != NULL) {
        struct mgl_renderbuffer *next = renderbuffer->next;
        free(renderbuffer);
        renderbuffer = next;
    }
    while (texture != NULL) {
        struct mgl_texture *next = texture->next;
        mgl_texture_release_object(texture);
        texture = next;
    }
    while (sampler != NULL) {
        struct mgl_sampler *next = sampler->next;
        free(sampler);
        sampler = next;
    }
    while (sync != NULL) {
        struct mgl_sync *next = sync->next;
        free(sync);
        sync = next;
    }
    while (shader != NULL) {
        struct mgl_shader *next = shader->next;
        free(shader->source);
        mgl_spirv_module_release(shader->spirv_module);
        free(shader->translated_source);
        free(shader->info_log);
        free(shader);
        shader = next;
    }
    while (program != NULL) {
        struct mgl_program *next = program->next;
        free(program->compat_uniform_cache);
        free(program->info_log);
        free(program);
        program = next;
    }
    while (label != NULL) {
        struct mgl_object_label *next = label->next;
        free(label);
        label = next;
    }
    mgl_display_list_free_all(group);
    mgl_arb_program_free_all(group);
}

struct mgl_share_group *mgl_share_group_create(void) {
    struct mgl_share_group *group = calloc(1, sizeof(*group));
    if (group != NULL) {
        atomic_init(&group->refcount, 1u);
        atomic_init(&group->next_buffer_name, 1u);
        atomic_init(&group->buffer_revision, 0u);
        atomic_init(&group->next_renderbuffer_name, 1u);
        atomic_init(&group->next_texture_name, 1u);
        atomic_init(&group->next_sampler_name, 1u);
        atomic_init(&group->next_shader_program_name, 1u);
        atomic_init(&group->program_link_revision, 1u);
        atomic_init(&group->next_list_name, 1u);
        atomic_init(&group->next_arb_program_name, 1u);
        if (pthread_mutex_init(&group->lock, NULL) != 0) {
            free(group);
            return NULL;
        }
    }
    return group;
}

void mgl_share_group_retain(struct mgl_share_group *group) {
    if (group != NULL) {
        (void)atomic_fetch_add_explicit(&group->refcount, 1u, memory_order_relaxed);
    }
}

void mgl_share_group_release(struct mgl_share_group *group) {
    if (group != NULL &&
        atomic_fetch_sub_explicit(&group->refcount, 1u, memory_order_acq_rel) == 1u) {
        free_share_objects(group);
        (void)pthread_mutex_destroy(&group->lock);
        free(group);
    }
}

static int mgl_egl_extension_present(const char *extensions, const char *name) {
    const size_t length = strlen(name);
    const char *match;
    if (extensions == NULL)
        return 0;
    while ((match = strstr(extensions, name)) != NULL) {
        if ((match == extensions || match[-1] == ' ') &&
            (match[length] == ' ' || match[length] == '\0'))
            return 1;
        extensions = match + length;
    }
    return 0;
}

static int choose_config_for_surface_bits(struct mgl_native_dispatch *b, MGL_EGLDisplay display,
                                          MGL_EGLint surface_bits, MGL_EGLConfig *config) {
    const MGL_EGLint attrs[] = {MGL_EGL_SURFACE_TYPE,
                                surface_bits,
                                MGL_EGL_RENDERABLE_TYPE,
                                MGL_EGL_OPENGL_ES3_BIT,
                                MGL_EGL_RED_SIZE,
                                8,
                                MGL_EGL_GREEN_SIZE,
                                8,
                                MGL_EGL_BLUE_SIZE,
                                8,
                                MGL_EGL_ALPHA_SIZE,
                                8,
                                MGL_EGL_DEPTH_SIZE,
                                24,
                                MGL_EGL_STENCIL_SIZE,
                                8,
                                MGL_EGL_NONE};
    MGL_EGLint count = 0;
    return b->eglChooseConfig(display, attrs, config, 1, &count) == MGL_EGL_TRUE && count > 0;
}

static void mgl_context_detect_client_arrays(struct mgl_context *ctx) {
    struct mgl_native_dispatch *b = ctx->native_dispatch;
    const char *setting = getenv("MGL_ANGLE_CLIENT_ARRAYS");
    const char *lazy_setting = getenv("MGL_ANGLE_LAZY_SHADOWS");
    GLint count = 0;
    GLint enabled = 0;
    GLint i;
    if (b == NULL || b->glesGetStringi == NULL || b->glesGetIntegerv == NULL)
        return;
    b->glesGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (i = 0; i < count; ++i) {
        const GLubyte *name = b->glesGetStringi(GL_EXTENSIONS, (GLuint)i);
        if (name == NULL || strcmp((const char *)name, "GL_ANGLE_client_arrays") != 0)
            continue;
        ctx->angle_lazy_buffer_shadows = lazy_setting == NULL || strcmp(lazy_setting, "0") != 0;
        if (setting == NULL || strcmp(setting, "0") != 0) {
            b->glesGetIntegerv(0x93AAu, &enabled);
            ctx->angle_client_arrays = enabled == (GLint)GL_TRUE ? GL_TRUE : GL_FALSE;
        }
        return;
    }
}

static void mgl_context_select_basevertex(struct mgl_context *ctx) {
    struct mgl_native_dispatch *b = ctx->native_dispatch;
    GLint count = 0;
    GLint i;
    ctx->native_draw_elements_base_vertex = b->glesDrawElementsBaseVertex;
    if (ctx->native_draw_elements_base_vertex != NULL || b->glesGetStringi == NULL ||
        b->glesGetIntegerv == NULL)
        return;
    b->glesGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (i = 0; i < count; ++i) {
        const GLubyte *name = b->glesGetStringi(GL_EXTENSIONS, (GLuint)i);
        if (name == NULL)
            continue;
        if (b->glesDrawElementsBaseVertexOES != NULL &&
            strcmp((const char *)name, "GL_OES_draw_elements_base_vertex") == 0) {
            ctx->native_draw_elements_base_vertex = b->glesDrawElementsBaseVertexOES;
            return;
        }
        if (b->glesDrawElementsBaseVertexEXT != NULL &&
            strcmp((const char *)name, "GL_EXT_draw_elements_base_vertex") == 0) {
            ctx->native_draw_elements_base_vertex = b->glesDrawElementsBaseVertexEXT;
            return;
        }
    }
}

static int choose_config(struct mgl_native_dispatch *b, MGL_EGLDisplay display,
                         MGL_EGLConfig *config) {
    /* Prefer a config supporting both window and pbuffer surfaces. Headless
     * providers may expose pbuffer-only configurations. */
    if (b->eglCreateWindowSurface != NULL &&
        choose_config_for_surface_bits(b, display, MGL_EGL_PBUFFER_BIT | MGL_EGL_WINDOW_BIT,
                                       config)) {
        return 1;
    }
    /* Native ANativeWindow surfaces require EGL_WINDOW_BIT. */
    {
        const char *direct = getenv("WINE_ANDROID_DIRECT_PRESENT");
        if (direct != NULL && strcmp(direct, "1") == 0)
            return 0;
    }
    return choose_config_for_surface_bits(b, display, MGL_EGL_PBUFFER_BIT, config);
}

struct mgl_context *mgl_context_create_offscreen(struct mgl_native_dispatch *b,
                                                 struct mgl_context *share, int width, int height) {
    struct mgl_context *ctx;
    MGL_EGLint major = 0;
    MGL_EGLint minor = 0;
    const MGL_EGLint ctx32[] = {MGL_EGL_CONTEXT_CLIENT_VERSION, 3, MGL_EGL_CONTEXT_MINOR_VERSION, 2,
                                MGL_EGL_NONE};
    const MGL_EGLint ctx32_angle[] = {MGL_EGL_CONTEXT_CLIENT_VERSION,
                                      3,
                                      MGL_EGL_CONTEXT_MINOR_VERSION,
                                      2,
                                      MGL_EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE,
                                      MGL_EGL_TRUE,
                                      MGL_EGL_NONE};
    const MGL_EGLint *context_attrs = ctx32;
    MGL_EGLint pbuffer_attrs[5];

    if (b == NULL || !mgl_native_dispatch_init(b) || width <= 0 || height <= 0) {
        return NULL;
    }

    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }
    ctx->native_dispatch = b;
    ctx->profile_mask = GL_CONTEXT_COMPATIBILITY_PROFILE_BIT;
    mgl_error_init(&ctx->errors);
    ctx->errors.diagnostic_context = ctx;
    mgl_debug_init(&ctx->debug);
    mgl_state_init(&ctx->canonical_state, width, height);
    mgl_compat_state_init(&ctx->compat);
    mgl_evaluator_init(&ctx->evaluator);
    mgl_pixel_transfer_init(&ctx->pixel_transfer);
    mgl_selection_init(&ctx->selection);
    mgl_compat_raster_init(&ctx->compat_raster);
    ctx->current_edge_flag = GL_TRUE;
    ctx->unpack_alignment = 4;
    ctx->pack_alignment = 4;
    mgl_context_init_image_units(ctx);
    ctx->arb_program_error_position = -1;
    ctx->arb_uniform_vp_env = -1;
    ctx->arb_uniform_vp_local = -1;
    ctx->arb_uniform_fp_env = -1;
    ctx->arb_uniform_fp_local = -1;
    for (unsigned unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit)
        ctx->arb_uniform_textures[unit] = -1;
    mgl_program_init_metadata(&ctx->arb_linked_program);
    {
        unsigned i;
        for (i = 0; i < MGL_MAX_VERTEX_ATTRIBS; ++i) {
            ctx->generic_current[i][3] = 1.0f;
            ctx->generic_current_i[i][3] = 1;
            ctx->generic_current_ui[i][3] = 1u;
            ctx->generic_current_type[i] = GL_FLOAT;
        }
        for (i = 0; i < MGL_COMPAT_SEMANTIC_COUNT; ++i)
            ctx->compat_current[i][3] = 1.0f;
        ctx->compat_current[1][2] = 1.0f;
        ctx->compat_current[2][0] = ctx->compat_current[2][1] = ctx->compat_current[2][2] = 1.0f;
        ctx->compat_current[3][0] = ctx->compat_current[3][1] = ctx->compat_current[3][2] = 0.0f;
    }
    mgl_vertex_array_init(&ctx->default_vao);
    ctx->current_vao = &ctx->default_vao;
    ctx->next_vao_name = 1u;
    ctx->next_framebuffer_name = 1u;
    ctx->default_draw_buffers[0] = GL_BACK;
    {
        unsigned i;
        for (i = 1u; i < MGL_MAX_DRAW_BUFFERS; ++i)
            ctx->default_draw_buffers[i] = GL_NONE;
    }
    ctx->default_read_buffer = GL_BACK;
    ctx->next_query_name = 1u;
    ctx->next_transform_feedback_name = 1u;
    ctx->provoking_vertex_mode = GL_LAST_VERTEX_CONVENTION;

    mgl_texture_mark_all_bindings_dirty(ctx);
    if (share != NULL && share->share_group != NULL) {
        ctx->share_group = share->share_group;
        mgl_share_group_retain(ctx->share_group);
    } else {
        ctx->share_group = mgl_share_group_create();
    }
    if (ctx->share_group == NULL) {
        free(ctx);
        return NULL;
    }

    ctx->egl_display = b->eglGetDisplay(MGL_EGL_DEFAULT_DISPLAY);
    if (ctx->egl_display == MGL_EGL_NO_DISPLAY ||
        b->eglInitialize(ctx->egl_display, &major, &minor) != MGL_EGL_TRUE) {
        mgl_context_destroy(ctx);
        return NULL;
    }
    if (b->eglBindAPI(MGL_EGL_OPENGL_ES_API) != MGL_EGL_TRUE ||
        !choose_config(b, ctx->egl_display, &ctx->egl_config)) {
        mgl_context_destroy(ctx);
        return NULL;
    }
    if (b->eglQueryString != NULL &&
        mgl_egl_extension_present(b->eglQueryString(ctx->egl_display, MGL_EGL_EXTENSIONS),
                                  "EGL_ANGLE_create_context_client_arrays"))
        context_attrs = ctx32_angle;

    pbuffer_attrs[0] = MGL_EGL_WIDTH;
    pbuffer_attrs[1] = width;
    pbuffer_attrs[2] = MGL_EGL_HEIGHT;
    pbuffer_attrs[3] = height;
    pbuffer_attrs[4] = MGL_EGL_NONE;
    ctx->egl_draw = b->eglCreatePbufferSurface(ctx->egl_display, ctx->egl_config, pbuffer_attrs);
    ctx->egl_read = ctx->egl_draw;
    ctx->egl_draw_owned = GL_TRUE;
    ctx->egl_read_owned = GL_TRUE;
    ctx->egl_draw_width = width;
    ctx->egl_draw_height = height;
    ctx->egl_read_width = width;
    ctx->egl_read_height = height;
    if (ctx->egl_draw == MGL_EGL_NO_SURFACE) {
        mgl_context_destroy(ctx);
        return NULL;
    }

    ctx->egl_context =
        b->eglCreateContext(ctx->egl_display, ctx->egl_config,
                            share != NULL ? share->egl_context : MGL_EGL_NO_CONTEXT, context_attrs);
    if (ctx->egl_context == MGL_EGL_NO_CONTEXT) {
        (void)fprintf(stderr, "mGL: OpenGL ES 3.2 context creation failed\n");
        mgl_context_destroy(ctx);
        return NULL;
    }
    return ctx;
}

static void free_context_objects(struct mgl_context *ctx) {
    struct mgl_vertex_array *vao = ctx->vaos;
    struct mgl_framebuffer *fb = ctx->framebuffers;
    struct mgl_transform_feedback *tf = ctx->transform_feedbacks;
    while (vao != NULL) {
        struct mgl_vertex_array *next = vao->next;
        if (vao->native_name != 0u && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesDeleteVertexArrays != NULL) {
            ctx->native_dispatch->glesDeleteVertexArrays(1, &vao->native_name);
        }
        free(vao);
        vao = next;
    }
    while (fb != NULL) {
        struct mgl_framebuffer *next = fb->next;
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesDeleteFramebuffers != NULL) {
            if (fb->routed_draw_native_name != 0u)
                ctx->native_dispatch->glesDeleteFramebuffers(1, &fb->routed_draw_native_name);
            if (fb->native_name != 0u)
                ctx->native_dispatch->glesDeleteFramebuffers(1, &fb->native_name);
        }
        free(fb);
        fb = next;
    }
    while (tf != NULL) {
        struct mgl_transform_feedback *next = tf->next;
        if (tf->native_name != 0u && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesDeleteTransformFeedbacks != NULL)
            ctx->native_dispatch->glesDeleteTransformFeedbacks(1, &tf->native_name);
        free(tf);
        tf = next;
    }
    mgl_query_free_all(ctx);
}

static void delete_last_share_group_native_objects(struct mgl_context *ctx) {
    struct mgl_buffer *buffer;
    struct mgl_renderbuffer *renderbuffer;
    struct mgl_texture *texture;
    struct mgl_sampler *sampler;
    struct mgl_sync *sync;
    struct mgl_program *program;
    struct mgl_shader *shader;
    if (ctx->share_group == NULL ||
        atomic_load_explicit(&ctx->share_group->refcount, memory_order_acquire) != 1u)
        return;
    for (buffer = ctx->share_group->buffers; buffer != NULL; buffer = buffer->next) {
        if (buffer->native_name != 0u && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesDeleteBuffers != NULL) {
            ctx->native_dispatch->glesDeleteBuffers(1, &buffer->native_name);
            buffer->native_name = 0u;
        }
    }
    for (renderbuffer = ctx->share_group->renderbuffers; renderbuffer != NULL;
         renderbuffer = renderbuffer->next) {
        if (renderbuffer->native_name != 0u && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesDeleteRenderbuffers != NULL) {
            ctx->native_dispatch->glesDeleteRenderbuffers(1, &renderbuffer->native_name);
            renderbuffer->native_name = 0u;
        }
    }
    for (texture = ctx->share_group->textures; texture != NULL; texture = texture->next) {
        if (texture->native_name != 0u && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesDeleteTextures != NULL) {
            ctx->native_dispatch->glesDeleteTextures(1, &texture->native_name);
            texture->native_name = 0u;
        }
    }
    for (sampler = ctx->share_group->samplers; sampler != NULL; sampler = sampler->next) {
        if (sampler->native_name != 0u && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesDeleteSamplers != NULL) {
            ctx->native_dispatch->glesDeleteSamplers(1, &sampler->native_name);
            sampler->native_name = 0u;
        }
    }
    for (sync = ctx->share_group->syncs; sync != NULL; sync = sync->next) {
        if (sync->native_sync != NULL && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesDeleteSync != NULL) {
            ctx->native_dispatch->glesDeleteSync(sync->native_sync);
            sync->native_sync = NULL;
        }
    }
    for (program = ctx->share_group->programs; program != NULL; program = program->next) {
        if (program->native_name != 0u && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesDeleteProgram != NULL) {
            ctx->native_dispatch->glesDeleteProgram(program->native_name);
            program->native_name = 0u;
        }
    }
    for (shader = ctx->share_group->shaders; shader != NULL; shader = shader->next) {
        if (shader->native_name != 0u && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesDeleteShader != NULL) {
            ctx->native_dispatch->glesDeleteShader(shader->native_name);
            shader->native_name = 0u;
        }
    }
}

void mgl_context_destroy(struct mgl_context *ctx) {
    struct mgl_context *previous;
    int cleanup_context_current = 0;
    if (ctx == NULL) {
        return;
    }

    previous = tls_current_context;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->eglMakeCurrent != NULL &&
        ctx->egl_display != MGL_EGL_NO_DISPLAY && ctx->egl_context != MGL_EGL_NO_CONTEXT &&
        ctx->egl_draw != MGL_EGL_NO_SURFACE && ctx->egl_read != MGL_EGL_NO_SURFACE) {
        if (previous == ctx ||
            ctx->native_dispatch->eglMakeCurrent(ctx->egl_display, ctx->egl_draw, ctx->egl_read,
                                                 ctx->egl_context) == MGL_EGL_TRUE) {
            tls_current_context = ctx;
            cleanup_context_current = 1;
        }
    }

    mgl_display_list_abort_compile(ctx);
    mgl_nv_fence_free_all(ctx, cleanup_context_current);

    if (ctx->share_group != NULL) {
        (void)pthread_mutex_lock(&ctx->share_group->lock);
        if (ctx->current_program != 0u) {
            struct mgl_program *program = mgl_program_find(ctx, ctx->current_program);
            if (program != NULL && program->use_count > 0u) {
                --program->use_count;
                mgl_program_maybe_destroy(ctx, program);
            }
            ctx->current_program = 0u;
        }
        if (cleanup_context_current) {
            mgl_pipeline_cleanup(ctx, 1);
            mgl_depth_copy_cleanup(ctx);
            mgl_compat_destroy_fixed_program(ctx);
            mgl_arb_destroy_native_program(ctx);
            if (ctx->scratch_array_buffer_native != 0u &&
                ctx->native_dispatch->glesDeleteBuffers != NULL) {
                ctx->native_dispatch->glesDeleteBuffers(1, &ctx->scratch_array_buffer_native);
                ctx->scratch_array_buffer_native = 0u;
            }
            if (ctx->scratch_element_buffer_native != 0u &&
                ctx->native_dispatch->glesDeleteBuffers != NULL) {
                ctx->native_dispatch->glesDeleteBuffers(1, &ctx->scratch_element_buffer_native);
                ctx->scratch_element_buffer_native = 0u;
            }
            free_context_objects(ctx);
            delete_last_share_group_native_objects(ctx);
        }
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
    }

    if (cleanup_context_current && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->eglMakeCurrent != NULL) {
        if (previous != NULL && previous != ctx &&
            previous->native_dispatch == ctx->native_dispatch &&
            previous->egl_display != MGL_EGL_NO_DISPLAY &&
            previous->egl_context != MGL_EGL_NO_CONTEXT) {
            if (ctx->native_dispatch->eglMakeCurrent(previous->egl_display, previous->egl_draw,
                                                     previous->egl_read,
                                                     previous->egl_context) == MGL_EGL_TRUE) {
                tls_current_context = previous;
            } else {
                (void)ctx->native_dispatch->eglMakeCurrent(ctx->egl_display, MGL_EGL_NO_SURFACE,
                                                           MGL_EGL_NO_SURFACE, MGL_EGL_NO_CONTEXT);
                tls_current_context = NULL;
            }
        } else {
            (void)ctx->native_dispatch->eglMakeCurrent(ctx->egl_display, MGL_EGL_NO_SURFACE,
                                                       MGL_EGL_NO_SURFACE, MGL_EGL_NO_CONTEXT);
            tls_current_context = NULL;
        }
    } else if (previous == ctx) {
        tls_current_context = NULL;
    }

    if (!cleanup_context_current) {
        mgl_pipeline_cleanup(ctx, 0);
        /* Release CPU ownership. The driver reclaims surviving native objects with
         * the EGL context or share group when explicit deletion is unavailable. */
        struct mgl_vertex_array *vao = ctx->vaos;
        struct mgl_framebuffer *fb = ctx->framebuffers;
        struct mgl_transform_feedback *tf = ctx->transform_feedbacks;
        while (vao != NULL) {
            struct mgl_vertex_array *next = vao->next;
            free(vao);
            vao = next;
        }
        while (fb != NULL) {
            struct mgl_framebuffer *next = fb->next;
            free(fb);
            fb = next;
        }
        while (tf != NULL) {
            struct mgl_transform_feedback *next = tf->next;
            free(tf);
            tf = next;
        }
        mgl_query_free_all(ctx);
    }

    if (ctx->native_dispatch != NULL && ctx->egl_display != MGL_EGL_NO_DISPLAY) {
        if (ctx->egl_context != MGL_EGL_NO_CONTEXT &&
            ctx->native_dispatch->eglDestroyContext != NULL) {
            (void)ctx->native_dispatch->eglDestroyContext(ctx->egl_display, ctx->egl_context);
        }
        if (ctx->egl_read_owned && ctx->egl_read != MGL_EGL_NO_SURFACE &&
            ctx->egl_read != ctx->egl_draw && ctx->native_dispatch->eglDestroySurface != NULL) {
            (void)ctx->native_dispatch->eglDestroySurface(ctx->egl_display, ctx->egl_read);
        }
        if (ctx->egl_draw_owned && ctx->egl_draw != MGL_EGL_NO_SURFACE &&
            ctx->native_dispatch->eglDestroySurface != NULL) {
            (void)ctx->native_dispatch->eglDestroySurface(ctx->egl_display, ctx->egl_draw);
        }
    }
    mgl_debug_destroy(&ctx->debug);
    mgl_evaluator_destroy(&ctx->evaluator);
    mgl_texture_release_context_defaults(ctx);
    mgl_arb_context_cleanup(ctx);

    free(ctx->fixed_program.compat_uniform_cache);

    for (unsigned key = 0u; key < 5u; ++key)
        free(ctx->fixed_program_variants[key].program.compat_uniform_cache);
    while (ctx->fixed_complex_programs != NULL) {
        struct mgl_fixed_program_variant *slot = ctx->fixed_complex_programs;
        ctx->fixed_complex_programs = slot->next;
        free(slot->program.compat_uniform_cache);
        free(slot);
    }
    free(ctx->arb_linked_program.compat_uniform_cache);
    free(ctx->scratch_cpu_storage);
    ctx->scratch_cpu_storage = NULL;
    free(ctx->immediate.vertices);
    ctx->immediate.vertices = NULL;
    mgl_share_group_release(ctx->share_group);
    free(ctx);
}

static MGL_EGLSurface create_pbuffer(struct mgl_context *ctx, GLsizei width, GLsizei height) {
    MGL_EGLint attrs[5];
    attrs[0] = MGL_EGL_WIDTH;
    attrs[1] = (MGL_EGLint)width;
    attrs[2] = MGL_EGL_HEIGHT;
    attrs[3] = (MGL_EGLint)height;
    attrs[4] = MGL_EGL_NONE;
    return ctx->native_dispatch->eglCreatePbufferSurface(ctx->egl_display, ctx->egl_config, attrs);
}

int mgl_context_configure_pbuffers(struct mgl_context *ctx, GLsizei draw_width, GLsizei draw_height,
                                   GLsizei read_width, GLsizei read_height) {
    MGL_EGLSurface new_draw;
    MGL_EGLSurface new_read;
    MGL_EGLSurface old_draw;
    MGL_EGLSurface old_read;
    GLboolean old_draw_owned;
    GLboolean old_read_owned;
    int same_surface;
    if (ctx == NULL || ctx->native_dispatch == NULL || draw_width <= 0 || draw_height <= 0 ||
        read_width <= 0 || read_height <= 0)
        return 0;
    if (ctx->egl_draw_owned && ctx->egl_read_owned && ctx->egl_draw_width == draw_width &&
        ctx->egl_draw_height == draw_height && ctx->egl_read_width == read_width &&
        ctx->egl_read_height == read_height)
        return 1;

    new_draw = create_pbuffer(ctx, draw_width, draw_height);
    if (new_draw == MGL_EGL_NO_SURFACE)
        return 0;
    same_surface = draw_width == read_width && draw_height == read_height;
    new_read = same_surface ? new_draw : create_pbuffer(ctx, read_width, read_height);
    if (new_read == MGL_EGL_NO_SURFACE) {
        (void)ctx->native_dispatch->eglDestroySurface(ctx->egl_display, new_draw);
        return 0;
    }

    old_draw = ctx->egl_draw;
    old_read = ctx->egl_read;
    old_draw_owned = ctx->egl_draw_owned;
    old_read_owned = ctx->egl_read_owned;
    if (tls_current_context == ctx) {
        if (ctx->native_dispatch->eglMakeCurrent(ctx->egl_display, new_draw, new_read,
                                                 ctx->egl_context) != MGL_EGL_TRUE) {
            if (!same_surface)
                (void)ctx->native_dispatch->eglDestroySurface(ctx->egl_display, new_read);
            (void)ctx->native_dispatch->eglDestroySurface(ctx->egl_display, new_draw);
            return 0;
        }
        /* Rebinding an EGL surface invalidates cached native texture-selector and
         * buffer-binding state. */
        ctx->native_active_texture_known = GL_FALSE;
        ctx->native_buffer_binding_known = 0u;
    }
    ctx->egl_draw = new_draw;
    ctx->egl_read = new_read;
    ctx->egl_draw_owned = GL_TRUE;
    ctx->egl_read_owned = GL_TRUE;
    ctx->egl_draw_width = draw_width;
    ctx->egl_draw_height = draw_height;
    ctx->egl_read_width = read_width;
    ctx->egl_read_height = read_height;
    if (old_read_owned && old_read != MGL_EGL_NO_SURFACE && old_read != old_draw)
        (void)ctx->native_dispatch->eglDestroySurface(ctx->egl_display, old_read);
    if (old_draw_owned && old_draw != MGL_EGL_NO_SURFACE)
        (void)ctx->native_dispatch->eglDestroySurface(ctx->egl_display, old_draw);
    return 1;
}

int mgl_context_use_external_surface(struct mgl_context *ctx, MGL_EGLSurface surface, GLsizei width,
                                     GLsizei height) {
    MGL_EGLSurface old_draw;
    MGL_EGLSurface old_read;
    GLboolean old_draw_owned;
    GLboolean old_read_owned;
    if (ctx == NULL || ctx->native_dispatch == NULL || surface == MGL_EGL_NO_SURFACE ||
        width <= 0 || height <= 0)
        return 0;
    if (!ctx->egl_draw_owned && !ctx->egl_read_owned && ctx->egl_draw == surface &&
        ctx->egl_read == surface && ctx->egl_draw_width == width &&
        ctx->egl_draw_height == height && ctx->egl_read_width == width &&
        ctx->egl_read_height == height)
        return 1;

    old_draw = ctx->egl_draw;
    old_read = ctx->egl_read;
    old_draw_owned = ctx->egl_draw_owned;
    old_read_owned = ctx->egl_read_owned;
    if (tls_current_context == ctx) {
        if (ctx->native_dispatch->eglMakeCurrent(ctx->egl_display, surface, surface,
                                                 ctx->egl_context) != MGL_EGL_TRUE)
            return 0;
        ctx->native_active_texture_known = GL_FALSE;
        ctx->native_buffer_binding_known = 0u;
    }
    ctx->egl_draw = surface;
    ctx->egl_read = surface;
    ctx->egl_draw_owned = GL_FALSE;
    ctx->egl_read_owned = GL_FALSE;
    ctx->egl_draw_width = width;
    ctx->egl_draw_height = height;
    ctx->egl_read_width = width;
    ctx->egl_read_height = height;
    if (old_read_owned && old_read != MGL_EGL_NO_SURFACE && old_read != old_draw)
        (void)ctx->native_dispatch->eglDestroySurface(ctx->egl_display, old_read);
    if (old_draw_owned && old_draw != MGL_EGL_NO_SURFACE)
        (void)ctx->native_dispatch->eglDestroySurface(ctx->egl_display, old_draw);
    return 1;
}

void mgl_context_set_creation_flags(struct mgl_context *ctx, GLbitfield flags) {
    if (ctx == NULL)
        return;
    ctx->context_flags = flags;
    if ((flags & GL_CONTEXT_FLAG_DEBUG_BIT) != 0u)
        ctx->debug.output_enabled = GL_TRUE;
}

void mgl_context_set_profile_mask(struct mgl_context *ctx, GLbitfield profile) {
    if (ctx == NULL)
        return;
    ctx->profile_mask = profile;
}

int mgl_context_make_current(struct mgl_context *ctx) {
    if (ctx == NULL) {
        struct mgl_context *previous = tls_current_context;
        if (previous != NULL && previous->native_dispatch != NULL &&
            previous->native_dispatch->eglMakeCurrent != NULL) {
            if (previous->native_dispatch->eglMakeCurrent(previous->egl_display, MGL_EGL_NO_SURFACE,
                                                          MGL_EGL_NO_SURFACE,
                                                          MGL_EGL_NO_CONTEXT) != MGL_EGL_TRUE) {
                return 0;
            }
        }
        tls_current_context = NULL;
        return 1;
    }
    struct mgl_context *previous = tls_current_context;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->eglMakeCurrent == NULL) {
        return 0;
    }
    if (ctx->native_dispatch->eglMakeCurrent(ctx->egl_display, ctx->egl_draw, ctx->egl_read,
                                             ctx->egl_context) != MGL_EGL_TRUE) {
        return 0;
    }
    tls_current_context = ctx;
    if (!ctx->native_gles32_verified) {
        GLint major_version = 0;
        GLint minor_version = 0;
        if (ctx->native_dispatch->glesGetIntegerv != NULL) {
            ctx->native_dispatch->glesGetIntegerv(GL_MAJOR_VERSION, &major_version);
            ctx->native_dispatch->glesGetIntegerv(GL_MINOR_VERSION, &minor_version);
        }
        if (major_version < 3 || (major_version == 3 && minor_version < 2)) {
            (void)fprintf(stderr, "mGL: OpenGL ES 3.2 required; native reported %d.%d\n",
                          major_version, minor_version);
            if (previous != NULL && previous != ctx && previous->native_dispatch != NULL &&
                previous->native_dispatch->eglMakeCurrent != NULL &&
                previous->native_dispatch->eglMakeCurrent(
                    previous->egl_display, previous->egl_draw, previous->egl_read,
                    previous->egl_context) == MGL_EGL_TRUE) {
                tls_current_context = previous;
            } else {
                (void)ctx->native_dispatch->eglMakeCurrent(ctx->egl_display, MGL_EGL_NO_SURFACE,
                                                           MGL_EGL_NO_SURFACE, MGL_EGL_NO_CONTEXT);
                tls_current_context = NULL;
            }
            return 0;
        }
        {
            GLint combined = 0, fragment = 0, vertex = 0;
            ctx->native_dispatch->glesGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &combined);
            ctx->native_dispatch->glesGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &fragment);
            ctx->native_dispatch->glesGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &vertex);
            if (combined <= 0)
                combined = (GLint)MGL_TEXTURE_UNITS;
            if (fragment <= 0)
                fragment = (GLint)MGL_TEXTURE_UNITS;
            if (vertex <= 0)
                vertex = (GLint)MGL_TEXTURE_UNITS;
            ctx->programmable_texture_units = combined > (GLint)MGL_PROGRAMMABLE_TEXTURE_CAPACITY
                                                  ? MGL_PROGRAMMABLE_TEXTURE_CAPACITY
                                                  : (unsigned)combined;
            ctx->fragment_texture_image_units = fragment < (GLint)ctx->programmable_texture_units
                                                    ? fragment
                                                    : (GLint)ctx->programmable_texture_units;
            ctx->vertex_texture_image_units = vertex < (GLint)ctx->programmable_texture_units
                                                  ? vertex
                                                  : (GLint)ctx->programmable_texture_units;
        }
        mgl_context_select_basevertex(ctx);
        mgl_context_detect_client_arrays(ctx);
        if (ctx->native_dispatch->glesGetString != NULL) {
            const GLubyte *renderer = ctx->native_dispatch->glesGetString(GL_RENDERER);
            const char *option = getenv("MGL_ANGLE_TEXTURE_PASSTHROUGH");
            ctx->angle_direct_texture_passthrough =
                renderer != NULL && strstr((const char *)renderer, "ANGLE") != NULL &&
                        (option == NULL || strcmp(option, "0") != 0)
                    ? GL_TRUE
                    : GL_FALSE;
        }
        ctx->native_gles32_verified = GL_TRUE;
    }
    tls_current_context = ctx;
    mgl_realize_copied_server_state(ctx);
    mgl_apply_render_thread_affinity();
    ctx->draw_native_program_valid = GL_FALSE;
    ctx->native_active_texture_known = GL_FALSE;
    ctx->native_buffer_binding_known = 0u;
    ctx->draw_native_pointer_valid = 0u;
    ctx->draw_constant_valid = 0u;
    ctx->draw_enabled_mask_valid = GL_FALSE;
    mgl_texture_refresh_current_framebuffers(ctx);
    return 1;
}

struct mgl_context *mgl_context_current(void) {
    return tls_current_context;
}

void mgl_context_set_current_for_test(struct mgl_context *ctx) {
    if (ctx != NULL) {
        ctx->draw_native_program_valid = GL_FALSE;
        ctx->native_active_texture_known = GL_FALSE;
        ctx->native_buffer_binding_known = 0u;
        ctx->draw_native_pointer_valid = 0u;
        ctx->draw_constant_valid = 0u;
        ctx->draw_enabled_mask_valid = GL_FALSE;
    }
    tls_current_context = ctx;
}

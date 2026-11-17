#include "mgl/gl.h"
#include "core/context.h"
#include "core/render_diag.h"
#include "objects/query.h"
#include "objects/object_store.h"
#include "shader/shader_store.h"
#include "texture/texture_store.h"

#include <string.h>

void glBlendBarrier(void) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesBlendBarrier == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesBlendBarrier();
}

void glBlendBarrierKHR(void) {
    glBlendBarrier();
}

void glPrimitiveBoundingBox(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
                            GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesPrimitiveBoundingBox == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesPrimitiveBoundingBox(minX, minY, minZ, minW, maxX, maxY, maxZ,
                                                   maxW);
}

void glPrimitiveBoundingBoxARB(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
                               GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW) {
    glPrimitiveBoundingBox(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
}

void glPrimitiveBoundingBoxEXT(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
                               GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW) {
    glPrimitiveBoundingBox(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
}

void glPrimitiveBoundingBoxOES(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
                               GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW) {
    glPrimitiveBoundingBox(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
}

void glTextureBarrier(void) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesTextureBarrier == NULL ||
        (!mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_NV_texture_barrier") &&
         !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_EXT_texture_barrier") &&
         !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_ARB_texture_barrier"))) {
        unsigned extension = ctx->native_dispatch != NULL &&
                             (mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                                     "GL_NV_texture_barrier") ||
                              mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                                     "GL_EXT_texture_barrier") ||
                              mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                                     "GL_ARB_texture_barrier"));
        unsigned entrypoint =
            ctx->native_dispatch != NULL && ctx->native_dispatch->glesTextureBarrier != NULL;
        mgl_render_diag_detail(ctx, "glTextureBarrier", "native_extension_entrypoint", extension,
                               entrypoint);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesTextureBarrier();
}

void glTextureBarrierNV(void) {
    glTextureBarrier();
}

static int conditional_mode(GLenum mode, int *wait) {
    switch (mode) {
        case GL_QUERY_WAIT:
        case GL_QUERY_BY_REGION_WAIT:
            *wait = 1;
            return 1;
        case GL_QUERY_NO_WAIT:
        case GL_QUERY_BY_REGION_NO_WAIT:
            *wait = 0;
            return 1;
        default:
            return 0;
    }
}

void glBeginConditionalRender(GLuint id, GLenum mode) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glBeginConditionalRender",
                        (const struct mgl_error_arg[]){{"id", (uint64_t)(uintptr_t)id},
                                                       {"mode", (uint64_t)(uintptr_t)mode}},
                        2u);
    GLboolean available = GL_FALSE;
    GLboolean passed = GL_TRUE;
    int wait = 0;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (id == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!conditional_mode(mode, &wait)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (ctx->conditional_render_active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!mgl_query_conditional_result(ctx, id, wait, &available, &passed)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->conditional_render_active = GL_TRUE;
    ctx->conditional_render_query = id;
    ctx->conditional_render_mode = mode;
    /* NO_WAIT must not stall. If the result is unavailable, rendering proceeds. */
    ctx->conditional_render_skip = available && !passed ? GL_TRUE : GL_FALSE;
}

void glEndConditionalRender(void) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!ctx->conditional_render_active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->conditional_render_active = GL_FALSE;
    ctx->conditional_render_skip = GL_FALSE;
    ctx->conditional_render_query = 0u;
    ctx->conditional_render_mode = 0u;
}

void glPrimitiveRestartIndex(GLuint index) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->primitive_restart_index = index;
}

static int native_has_angle_provoking_vertex(const struct mgl_context *ctx) {
    return ctx != NULL && ctx->native_dispatch != NULL &&
           ctx->native_dispatch->glesProvokingVertexANGLE != NULL &&
           mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                  "GL_ANGLE_provoking_vertex");
}

void glProvokingVertex(GLenum mode) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glProvokingVertex",
                        (const struct mgl_error_arg[]){{"mode", (uint64_t)(uintptr_t)mode}}, 1u);
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mode != GL_FIRST_VERTEX_CONVENTION && mode != GL_LAST_VERTEX_CONVENTION) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    ctx->provoking_vertex_mode = mode;
    if (native_has_angle_provoking_vertex(ctx))
        ctx->native_dispatch->glesProvokingVertexANGLE(mode);
}

void glProvokingVertexEXT(GLenum mode) {
    glProvokingVertex(mode);
}

static struct mgl_program *active_compute_program(struct mgl_context *ctx) {
    struct mgl_program *program;
    if (ctx->current_program != 0u) {
        program = mgl_program_find(ctx, ctx->current_program);
        if (program == NULL || (!program->linked && !ctx->current_program_executable) ||
            !program->linked_compute)
            return NULL;
        return program;
    }
    if (ctx->current_pipeline != 0u) {
        struct mgl_pipeline *it;
        for (it = ctx->pipelines; it != NULL; it = it->next)
            if (it->name == ctx->current_pipeline)
                return it->stages[5] != NULL && it->stages[5]->linked ? it->stages[5] : NULL;
    }
    return NULL;
}

static int valid_memory_barrier_bits(GLbitfield barriers) {
    const GLbitfield known =
        GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT | GL_UNIFORM_BARRIER_BIT |
        GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_COMMAND_BARRIER_BIT |
        GL_PIXEL_BUFFER_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT |
        GL_FRAMEBUFFER_BARRIER_BIT | GL_TRANSFORM_FEEDBACK_BARRIER_BIT |
        GL_ATOMIC_COUNTER_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_QUERY_BUFFER_BARRIER_BIT;
    return barriers == GL_ALL_BARRIER_BITS || (barriers & ~known) == 0u;
}

void glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z) {
    struct mgl_context *ctx = mgl_context_current();

    if (ctx == NULL)
        return;
    if (ctx->immediate.active || active_compute_program(ctx) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesDispatchCompute == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesDispatchCompute(num_groups_x, num_groups_y, num_groups_z);
}

void glDispatchComputeIndirect(GLintptr indirect) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *buffer;
    GLuint groups[3];
    size_t offset;

    if (ctx == NULL)
        return;
    if (ctx->immediate.active || active_compute_program(ctx) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (indirect < 0 || (indirect & 3) != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    buffer = mgl_bound_buffer(ctx, GL_DISPATCH_INDIRECT_BUFFER);
    if (buffer == NULL ||
        (buffer->mapped && (buffer->map_access & GL_MAP_PERSISTENT_BIT) == 0u)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    offset = (size_t)indirect;
    if (offset > buffer->size || buffer->size - offset < sizeof(groups)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesDispatchComputeIndirect != NULL && buffer->native_name != 0u) {
        mgl_native_dispatch_bind_buffer(ctx, GL_DISPATCH_INDIRECT_BUFFER, buffer->native_name);
        ctx->native_dispatch->glesDispatchComputeIndirect(indirect);
        return;
    }
    if (!mgl_buffer_read_native_bytes(ctx, buffer, offset, sizeof(groups), groups)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    glDispatchCompute(groups[0], groups[1], groups[2]);
}

static int valid_image_access(GLenum access) {
    return access == GL_READ_ONLY || access == GL_WRITE_ONLY || access == GL_READ_WRITE;
}

static int valid_gles_image_format(GLenum format) {
    switch (format) {
        case GL_RGBA32F:
        case GL_RGBA16F:
        case GL_R32F:
        case GL_RGBA32UI:
        case GL_RGBA16UI:
        case GL_RGBA8UI:
        case GL_R32UI:
        case GL_RGBA32I:
        case GL_RGBA16I:
        case GL_RGBA8I:
        case GL_R32I:
        case GL_RGBA8:
        case GL_RGBA8_SNORM:
            return 1;
        default:
            return 0;
    }
}

void glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer,
                        GLenum access, GLenum format) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glBindImageTexture",
                        (const struct mgl_error_arg[]){{"unit", (uint64_t)(uintptr_t)unit},
                                                       {"texture", (uint64_t)(uintptr_t)texture},
                                                       {"level", (uint64_t)(uintptr_t)level},
                                                       {"layered", (uint64_t)(uintptr_t)layered},
                                                       {"layer", (uint64_t)(uintptr_t)layer},
                                                       {"access", (uint64_t)(uintptr_t)access},
                                                       {"format", (uint64_t)(uintptr_t)format}},
                        7u);
    struct mgl_texture *obj = NULL;
    GLuint native_name = 0u;
    GLenum native_format = format;
    GLuint limit;

    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesBindImageTexture == NULL) {
        mgl_render_diag_detail(ctx, "glBindImageTexture", "native_entrypoint_missing", unit,
                               texture);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    limit = mgl_context_image_unit_limit(ctx);
    if (unit >= limit) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (level < 0 || layer < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!valid_image_access(access)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!valid_gles_image_format(format) && !(texture == 0u && format == GL_R8)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }

    if (texture != 0u) {
        (void)pthread_mutex_lock(&ctx->share_group->lock);
        obj = mgl_texture_find(ctx, texture);
        if (obj == NULL || !obj->realized) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        /* GLES image load/store requires immutable texture storage. Mutable desktop textures
         * currently lack image load/store realization. */
        if (!obj->immutable_format) {
            mgl_render_diag_detail(ctx, "glBindImageTexture", "mutable_texture", texture,
                                   obj->native_name);
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        native_name = obj->native_name;
        if (format == GL_RGBA32F &&
            ((obj->target == GL_TEXTURE_CUBE_MAP && obj->cube_levels[0][0].defined &&
              obj->cube_levels[0][0].internal_format == GL_RGBA32F) ||
             (obj->levels[0].defined && obj->levels[0].internal_format == GL_RGBA32F)) &&
            mgl_texture_native_storage_internalformat(ctx, GL_RGBA32F) == GL_RGBA16F)
            native_format = GL_RGBA16F;
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
    }

    ctx->native_dispatch->glesBindImageTexture(
        unit, native_name, level, layered ? GL_TRUE : GL_FALSE, layer, access,
        native_name == 0u && format == GL_R8 ? GL_RGBA8 : native_format);
    ctx->image_units[unit].texture = texture;
    ctx->image_units[unit].level = level;
    ctx->image_units[unit].layered = layered ? GL_TRUE : GL_FALSE;
    ctx->image_units[unit].layer = layer;
    ctx->image_units[unit].access = access;
    ctx->image_units[unit].format = format;
}

void glBindImageTextures(GLuint first, GLsizei count, const GLuint *textures) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint limit;
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    limit = mgl_context_image_unit_limit(ctx);
    if (first > limit || (GLuint)count > limit - first) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    for (i = 0; i < count; ++i) {
        GLuint name = textures != NULL ? textures[i] : 0u;
        GLenum format;
        GLboolean layered = GL_FALSE;
        struct mgl_texture *obj;
        if (name == 0u) {
            glBindImageTexture(first + (GLuint)i, 0u, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
            continue;
        }
        (void)pthread_mutex_lock(&ctx->share_group->lock);
        obj = mgl_texture_find(ctx, name);
        format = GL_NONE;
        if (obj != NULL && obj->realized && obj->immutable_format) {
            layered = obj->target == GL_TEXTURE_3D || obj->target == GL_TEXTURE_2D_ARRAY ||
                      obj->target == GL_TEXTURE_CUBE_MAP ||
                      obj->target == GL_TEXTURE_CUBE_MAP_ARRAY ||
                      obj->target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
            if (obj->target == GL_TEXTURE_CUBE_MAP && obj->cube_levels[0][0].defined)
                format = obj->cube_levels[0][0].internal_format;
            else if (obj->target == GL_TEXTURE_BUFFER)
                format = obj->buffer_texture_internal_format;
            else if (obj->levels[0].defined)
                format = obj->levels[0].internal_format;
        }
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        if (format == GL_NONE || !valid_gles_image_format(format)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            continue;
        }
        glBindImageTexture(first + (GLuint)i, name, 0, layered, 0, GL_READ_WRITE, format);
    }
}

void glMemoryBarrier(GLbitfield barriers) {
    struct mgl_context *ctx = mgl_context_current();

    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_memory_barrier_bits(barriers)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesMemoryBarrier == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesMemoryBarrier(barriers);
}

void glMemoryBarrierByRegion(GLbitfield barriers) {
    const GLbitfield allowed = GL_ATOMIC_COUNTER_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT |
                               GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT |
                               GL_TEXTURE_FETCH_BARRIER_BIT | GL_UNIFORM_BARRIER_BIT;
    struct mgl_context *ctx = mgl_context_current();

    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((barriers & ~allowed) != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesMemoryBarrierByRegion != NULL) {
        ctx->native_dispatch->glesMemoryBarrierByRegion(barriers);
        return;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesMemoryBarrier != NULL) {
        ctx->native_dispatch->glesMemoryBarrier(barriers);
        return;
    }
    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
}

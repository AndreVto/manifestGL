#include "core/render_diag.h"
#include "mgl/gl.h"

#include "core/context.h"
#include "objects/object_store.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int valid_usage(GLenum usage) {
    switch (usage) {
        case GL_STREAM_DRAW:
        case GL_STATIC_DRAW:
        case GL_DYNAMIC_DRAW:
        case GL_STREAM_READ:
        case GL_STATIC_READ:
        case GL_DYNAMIC_READ:
        case GL_STREAM_COPY:
        case GL_STATIC_COPY:
        case GL_DYNAMIC_COPY:
            return 1;
        default:
            return 0;
    }
}

static int reject_during_begin_end(struct mgl_context *ctx) {
    if (ctx == NULL || !ctx->immediate.active)
        return 0;
    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    return 1;
}

static GLenum access_enum_from_flags(GLbitfield access) {
    switch (access & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) {
        case GL_MAP_READ_BIT:
            return GL_READ_ONLY;
        case GL_MAP_WRITE_BIT:
            return GL_WRITE_ONLY;
        default:
            return GL_READ_WRITE;
    }
}

static int validate_map_access(struct mgl_context *ctx, GLbitfield access) {
    const GLbitfield allowed = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT |
                               GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_FLUSH_EXPLICIT_BIT |
                               GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_PERSISTENT_BIT |
                               GL_MAP_COHERENT_BIT;
    if ((access & ~allowed) != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if ((access & GL_MAP_COHERENT_BIT) != 0u && (access & GL_MAP_PERSISTENT_BIT) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if ((access & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) == 0u ||
        ((access & GL_MAP_READ_BIT) != 0u &&
         (access & (GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT |
                    GL_MAP_UNSYNCHRONIZED_BIT)) != 0u) ||
        ((access & GL_MAP_FLUSH_EXPLICIT_BIT) != 0u && (access & GL_MAP_WRITE_BIT) == 0u)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    return 1;
}

static int validate_storage_map(struct mgl_context *ctx, const struct mgl_buffer *obj,
                                GLbitfield access) {
    if (!obj->immutable && (access & (GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT)) != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (obj->immutable &&
        (((access & GL_MAP_READ_BIT) != 0u && (obj->storage_flags & GL_MAP_READ_BIT) == 0u) ||
         ((access & GL_MAP_WRITE_BIT) != 0u && (obj->storage_flags & GL_MAP_WRITE_BIT) == 0u) ||
         ((access & GL_MAP_PERSISTENT_BIT) != 0u &&
          (obj->storage_flags & GL_MAP_PERSISTENT_BIT) == 0u) ||
         ((access & GL_MAP_COHERENT_BIT) != 0u &&
          (obj->storage_flags & GL_MAP_COHERENT_BIT) == 0u))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    return 1;
}

static int checked_range(size_t total, GLintptr offset, GLsizeiptr size, size_t *out_offset,
                         size_t *out_size) {
    size_t off;
    size_t len;
    if (offset < 0 || size < 0)
        return 0;
    off = (size_t)offset;
    len = (size_t)size;
    if (off > total || len > total - off)
        return 0;
    if (out_offset != NULL)
        *out_offset = off;
    if (out_size != NULL)
        *out_size = len;
    return 1;
}

static int ranges_overlap(size_t first_offset, size_t first_length, size_t second_offset,
                          size_t second_length) {
    if (first_length == 0u || second_length == 0u)
        return 0;
    return first_offset < second_offset + second_length &&
           second_offset < first_offset + first_length;
}

static void *buffer_map_pointer(struct mgl_buffer *obj) {
    if (obj == NULL || !obj->mapped)
        return NULL;
    if (obj->native_mapping != NULL)
        return obj->native_mapping;
    if (obj->map_length == 0u)
        return &obj->zero_length_map_byte;
    return obj->shadow + obj->map_offset;
}

static int native_texture_buffer_target_supported(const struct mgl_context *ctx) {
    return ctx != NULL && ctx->native_dispatch != NULL &&
           ctx->native_dispatch->glesTexBufferEXT != NULL;
}

static GLuint native_buffer_name(struct mgl_context *ctx, GLuint name) {
    struct mgl_buffer *obj;
    if (ctx == NULL || name == 0u)
        return 0u;
    obj = mgl_buffer_find(ctx, name);
    return obj != NULL ? obj->native_name : 0u;
}

static int native_texture_buffer_begin(struct mgl_context *ctx, struct mgl_buffer *obj,
                                       GLenum *native_target) {
    if (ctx == NULL || obj == NULL || native_target == NULL || ctx->native_dispatch == NULL)
        return 0;
    if (native_texture_buffer_target_supported(ctx)) {
        *native_target = GL_TEXTURE_BUFFER;
        return 0;
    }
    if (ctx->native_dispatch->glesBindBuffer == NULL)
        return -1;
    *native_target = GL_COPY_WRITE_BUFFER;
    mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER, obj->native_name);
    return 1;
}

static void native_texture_buffer_end(struct mgl_context *ctx, int restore_copy_write) {
    if (ctx == NULL || restore_copy_write <= 0 || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesBindBuffer == NULL)
        return;
    mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER,
                                    native_buffer_name(ctx, ctx->copy_write_buffer));
}

static void native_buffer_subdata_from_shadow(struct mgl_context *ctx, struct mgl_buffer *obj,
                                              GLintptr offset, GLsizeiptr size, const void *data) {
    if (ctx == NULL || obj == NULL || size == 0 || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesBindBuffer == NULL ||
        ctx->native_dispatch->glesBufferSubData == NULL)
        return;
    mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER, obj->native_name);
    ctx->native_dispatch->glesBufferSubData(GL_COPY_WRITE_BUFFER, offset, size, data);
    mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER,
                                    native_buffer_name(ctx, ctx->copy_write_buffer));
}

int mgl_buffer_shadow_ensure(struct mgl_context *ctx, struct mgl_buffer *obj) {
    unsigned char *storage;
    void *mapped;
    GLuint restore;
    if (obj == NULL)
        return 0;
    if (obj->shadow != NULL || obj->size == 0u)
        return 1;
    storage = malloc(obj->size);
    if (storage == NULL) {
        if (ctx != NULL)
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    if (ctx != NULL && ctx->native_dispatch != NULL && obj->native_name != 0u &&
        ctx->native_dispatch->glesBindBuffer != NULL &&
        ctx->native_dispatch->glesMapBufferRange != NULL &&
        ctx->native_dispatch->glesUnmapBuffer != NULL) {
        restore = native_buffer_name(ctx, ctx->copy_read_buffer);
        mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER, obj->native_name);
        mapped = ctx->native_dispatch->glesMapBufferRange(GL_COPY_READ_BUFFER, 0,
                                                          (GLsizeiptr)obj->size, GL_MAP_READ_BIT);
        if (mapped == NULL) {
            mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER, restore);
            free(storage);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        memcpy(storage, mapped, obj->size);
        ctx->native_dispatch->glesUnmapBuffer(GL_COPY_READ_BUFFER);
        mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER, restore);
    } else {
        memset(storage, 0, obj->size);
    }
    obj->shadow = storage;
    return 1;
}

static int buffer_read_back(struct mgl_context *ctx, struct mgl_buffer *obj, size_t offset,
                            size_t length, void *data) {
    void *mapped;
    GLuint restore;
    if (length == 0u)
        return 1;
    if (obj->native_mapping != NULL && offset >= obj->map_offset &&
        length <= obj->map_length - (offset - obj->map_offset)) {
        memcpy(data, (const unsigned char *)obj->native_mapping + offset - obj->map_offset, length);
        return 1;
    }
    if (obj->native_immutable && (obj->storage_flags & GL_MAP_READ_BIT) == 0u)
        return mgl_buffer_read_native_bytes(ctx, obj, offset, length, data);
    if (!obj->mapped && ctx->native_dispatch != NULL && obj->native_name != 0u &&
        ctx->native_dispatch->glesBindBuffer != NULL &&
        ctx->native_dispatch->glesMapBufferRange != NULL &&
        ctx->native_dispatch->glesUnmapBuffer != NULL) {
        restore = native_buffer_name(ctx, ctx->copy_read_buffer);
        mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER, obj->native_name);
        mapped = ctx->native_dispatch->glesMapBufferRange(GL_COPY_READ_BUFFER, (GLintptr)offset,
                                                          (GLsizeiptr)length, GL_MAP_READ_BIT);
        if (mapped == NULL) {
            mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER, restore);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        memcpy(data, mapped, length);
        ctx->native_dispatch->glesUnmapBuffer(GL_COPY_READ_BUFFER);
        mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER, restore);
        return 1;
    }
    if (!mgl_buffer_shadow_ensure(ctx, obj))
        return 0;
    memcpy(data, obj->shadow + offset, length);
    return 1;
}

int mgl_buffer_read_native_bytes(struct mgl_context *ctx, struct mgl_buffer *obj,
                                 size_t offset, size_t length, void *data) {
    struct mgl_native_dispatch *native;
    GLuint temporary = 0u;
    GLuint source;
    void *mapped;
    if (ctx == NULL || obj == NULL || data == NULL || offset > obj->size ||
        length > obj->size - offset ||
        (obj->mapped && (obj->map_access & GL_MAP_PERSISTENT_BIT) == 0u))
        return 0;
    if (length == 0u)
        return 1;
    native = ctx->native_dispatch;
    if (native == NULL || obj->native_name == 0u || native->glesBindBuffer == NULL ||
        native->glesMapBufferRange == NULL || native->glesUnmapBuffer == NULL) {
        if (!mgl_buffer_shadow_ensure(ctx, obj))
            return 0;
        memcpy(data, obj->shadow + offset, length);
        return 1;
    }
    source = obj->native_name;
    if (obj->mapped || (obj->native_immutable && !(obj->storage_flags & GL_MAP_READ_BIT))) {
        if (native->glesGenBuffers == NULL || native->glesDeleteBuffers == NULL ||
            native->glesBufferData == NULL || native->glesCopyBufferSubData == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        native->glesGenBuffers(1, &temporary);
        if (temporary == 0u) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return 0;
        }
        mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER, source);
        mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER, temporary);
        native->glesBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)length, NULL, GL_STREAM_READ);
        native->glesCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                                      (GLintptr)offset, 0, (GLsizeiptr)length);
        source = temporary;
        offset = 0u;
    }
    mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER, source);
    mapped = native->glesMapBufferRange(GL_COPY_READ_BUFFER, (GLintptr)offset,
                                        (GLsizeiptr)length, GL_MAP_READ_BIT);
    if (mapped != NULL) {
        memcpy(data, mapped, length);
        native->glesUnmapBuffer(GL_COPY_READ_BUFFER);
    }
    mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER,
                                    native_buffer_name(ctx, ctx->copy_read_buffer));
    if (temporary != 0u) {
        mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER,
                                        native_buffer_name(ctx, ctx->copy_write_buffer));
        native->glesDeleteBuffers(1, &temporary);
    }
    return mapped != NULL;
}

int mgl_buffer_write_bytes(struct mgl_context *ctx, struct mgl_buffer *obj, size_t offset,
                           size_t length, const void *data, int gpu_copy) {
    struct mgl_native_dispatch *native;
    GLuint temporary = 0u;
    if (ctx == NULL || obj == NULL || (length != 0u && data == NULL))
        return 0;
    if (length == 0u)
        return 1;
    if (!mgl_buffer_shadow_ensure(ctx, obj))
        return 0;
    native = ctx->native_dispatch;
    if (native != NULL && obj->native_name != 0u) {
        if (gpu_copy ||
            (obj->native_immutable && (obj->storage_flags & GL_DYNAMIC_STORAGE_BIT) == 0u)) {
            if (native->glesGenBuffers == NULL || native->glesBufferData == NULL ||
                native->glesCopyBufferSubData == NULL || native->glesDeleteBuffers == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return 0;
            }
            native->glesGenBuffers(1, &temporary);
            if (temporary == 0u) {
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return 0;
            }
            mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER, temporary);
            native->glesBufferData(GL_COPY_READ_BUFFER, (GLsizeiptr)length, data, GL_STREAM_DRAW);
            mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER, obj->native_name);
            native->glesCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0,
                                          (GLintptr)offset, (GLsizeiptr)length);
            mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER,
                                            native_buffer_name(ctx, ctx->copy_read_buffer));
            mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER,
                                            native_buffer_name(ctx, ctx->copy_write_buffer));
            native->glesDeleteBuffers(1, &temporary);
        } else {
            native_buffer_subdata_from_shadow(ctx, obj, (GLintptr)offset, (GLsizeiptr)length, data);
        }
    }
    if (data != obj->shadow + offset)
        memcpy(obj->shadow + offset, data, length);
    return 1;
}

static int buffer_can_defer_shadow(const struct mgl_context *ctx, const struct mgl_buffer *obj,
                                   GLenum target) {
    return (target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER) && ctx != NULL &&
           obj != NULL && ctx->angle_lazy_buffer_shadows && obj->native_name != 0u &&
           ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindBuffer != NULL &&
           ctx->native_dispatch->glesBufferData != NULL &&
           ctx->native_dispatch->glesMapBufferRange != NULL &&
           ctx->native_dispatch->glesUnmapBuffer != NULL;
}

static GLuint next_buffer_name(struct mgl_share_group *group) {
    GLuint name;
    do {
        name =
            (GLuint)atomic_fetch_add_explicit(&group->next_buffer_name, 1u, memory_order_relaxed);
    } while (name == 0u);
    return name;
}

void glGenBuffers(GLsizei n, GLuint *buffers) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0)
        return;
    if (buffers == NULL)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < n; ++i) {
        GLuint name = next_buffer_name(ctx->share_group);
        struct mgl_buffer *obj = mgl_buffer_ensure(ctx, name, 1);
        if (obj == NULL || obj->native_name == 0u) {
            buffers[i] = 0u;
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        } else {
            buffers[i] = name;
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

static void clear_context_buffer_bindings(struct mgl_context *ctx, GLuint name) {
    struct mgl_vertex_array *vao;
    ctx->buffer_name_cache_valid = 0u;
    if (ctx->array_buffer == name)
        ctx->array_buffer = 0u;
    if (ctx->pixel_pack_buffer == name)
        ctx->pixel_pack_buffer = 0u;
    if (ctx->pixel_unpack_buffer == name)
        ctx->pixel_unpack_buffer = 0u;
    if (ctx->copy_read_buffer == name)
        ctx->copy_read_buffer = 0u;
    if (ctx->copy_write_buffer == name)
        ctx->copy_write_buffer = 0u;
    if (ctx->uniform_buffer == name)
        ctx->uniform_buffer = 0u;
    if (ctx->atomic_counter_buffer == name)
        ctx->atomic_counter_buffer = 0u;
    if (ctx->transform_feedback_buffer == name)
        ctx->transform_feedback_buffer = 0u;
    if (ctx->draw_indirect_buffer == name)
        ctx->draw_indirect_buffer = 0u;
    if (ctx->parameter_buffer == name)
        ctx->parameter_buffer = 0u;
    if (ctx->query_buffer == name)
        ctx->query_buffer = 0u;
    if (ctx->dispatch_indirect_buffer == name)
        ctx->dispatch_indirect_buffer = 0u;
    if (ctx->shader_storage_buffer == name)
        ctx->shader_storage_buffer = 0u;
    if (ctx->texture_buffer == name)
        ctx->texture_buffer = 0u;
    {
        unsigned i;
        struct mgl_transform_feedback *tf;
        for (i = 0; i < MGL_MAX_INDEXED_BUFFER_BINDINGS; ++i) {
            if (ctx->uniform_buffer_bindings[i].buffer == name)
                memset(&ctx->uniform_buffer_bindings[i], 0,
                       sizeof(ctx->uniform_buffer_bindings[i]));
            if (ctx->atomic_counter_buffer_bindings[i].buffer == name)
                memset(&ctx->atomic_counter_buffer_bindings[i], 0,
                       sizeof(ctx->atomic_counter_buffer_bindings[i]));
            if (ctx->shader_storage_buffer_bindings[i].buffer == name)
                memset(&ctx->shader_storage_buffer_bindings[i], 0,
                       sizeof(ctx->shader_storage_buffer_bindings[i]));
            if (ctx->transform_feedback_buffer_bindings[i].buffer == name)
                memset(&ctx->transform_feedback_buffer_bindings[i], 0,
                       sizeof(ctx->transform_feedback_buffer_bindings[i]));
        }
        for (tf = ctx->transform_feedbacks; tf != NULL; tf = tf->next) {
            for (i = 0; i < MGL_MAX_INDEXED_BUFFER_BINDINGS; ++i) {
                if (tf->buffer_bindings[i].buffer == name)
                    memset(&tf->buffer_bindings[i], 0, sizeof(tf->buffer_bindings[i]));
            }
        }
    }
    if (ctx->default_vao.element_array_buffer == name)
        ctx->default_vao.element_array_buffer = 0u;
    for (vao = &ctx->default_vao; vao != NULL;
         vao = vao == &ctx->default_vao ? ctx->vaos : vao->next) {
        GLuint i;
        if (vao->element_array_buffer == name)
            vao->element_array_buffer = 0u;
        for (i = 0u; i < MGL_MAX_VERTEX_ATTRIBS; ++i) {
            if (vao->vertex_buffers[i].data.source == MGL_DATA_BUFFER &&
                vao->vertex_buffers[i].data.buffer_name == name) {
                vao->vertex_buffers[i].data.buffer_name = 0u;
                mgl_vertex_array_resolve_slot(vao, i);
            }
        }
    }
}

void glDeleteBuffers(GLsizei n, const GLuint *buffers) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0 || buffers == NULL)
        return;
    ctx->draw_native_pointer_valid = 0u;
    ctx->buffer_name_cache_valid = 0u;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < n; ++i) {
        if (buffers[i] != 0u) {
            clear_context_buffer_bindings(ctx, buffers[i]);
            mgl_buffer_remove(ctx, buffers[i]);
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glBindBuffer(GLenum target, GLuint buffer) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_BUFFER_STATE);
    GLuint *slot;
    GLuint native_name = 0u;
    struct mgl_buffer *obj = NULL;
    unsigned cache_index;
    uint32_t cache_bit;
    unsigned revision;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    slot = mgl_buffer_binding_slot(ctx, target);
    if (slot == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    cache_index = mgl_native_dispatch_buffer_binding_index(target);
    cache_bit = cache_index < 10u ? UINT32_C(1) << cache_index : 0u;
    revision = ctx->share_group != NULL
                   ? atomic_load_explicit(&ctx->share_group->buffer_revision, memory_order_acquire)
                   : 0u;
    if (ctx->buffer_name_cache_revision != revision) {
        ctx->buffer_name_cache_valid = 0u;
        ctx->buffer_name_cache_revision = revision;
    }
    if (cache_bit != 0u && buffer == *slot && (ctx->buffer_name_cache_valid & cache_bit) != 0u &&
        ctx->buffer_name_cache_canonical[cache_index] == buffer &&
        (ctx->native_buffer_binding_known & cache_bit) != 0u &&
        ctx->native_buffer_bindings[cache_index] == ctx->buffer_name_cache_native[cache_index] &&
        ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindBuffer != NULL)
        return;
    if (buffer != 0u) {
        (void)pthread_mutex_lock(&ctx->share_group->lock);
        obj = mgl_buffer_ensure(ctx, buffer, 1);
        if (obj != NULL) {
            obj->realized = GL_TRUE;
            native_name = obj->native_name;
        }
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        if (obj == NULL || native_name == 0u) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
    }
    *slot = buffer;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindBuffer != NULL &&
        (target != GL_TEXTURE_BUFFER || native_texture_buffer_target_supported(ctx)) &&
        target != GL_PARAMETER_BUFFER && target != GL_QUERY_BUFFER) {
        mgl_native_dispatch_bind_buffer(ctx, target, native_name);
        if (cache_bit != 0u) {
            ctx->buffer_name_cache_canonical[cache_index] = buffer;
            ctx->buffer_name_cache_native[cache_index] = native_name;
            ctx->buffer_name_cache_valid |= cache_bit;
        }
    } else {
        ctx->buffer_name_cache_valid &= ~cache_bit;
    }
}

GLboolean glIsBuffer(GLuint buffer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    GLboolean result = GL_FALSE;
    if (ctx == NULL)
        return GL_FALSE;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    if (buffer == 0u)
        return GL_FALSE;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    obj = mgl_buffer_find(ctx, buffer);
    if (obj != NULL)
        result = obj->realized;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return result;
}

static int buffer_storage_flags_valid(struct mgl_context *ctx, GLbitfield flags) {
    const GLbitfield allowed = GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT |
                               GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_CLIENT_STORAGE_BIT;
    if ((flags & ~allowed) != 0u ||
        ((flags & GL_MAP_PERSISTENT_BIT) != 0u &&
         (flags & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) == 0u) ||
        ((flags & GL_MAP_COHERENT_BIT) != 0u && (flags & GL_MAP_PERSISTENT_BIT) == 0u)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    return 1;
}

static void buffer_storage_allocate(struct mgl_context *ctx, struct mgl_buffer *obj, GLenum target,
                                    GLsizeiptr size, const void *data, GLbitfield flags) {
    unsigned char *replacement;
    int native_storage = 0;
    if (size <= 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (obj->immutable || obj->mapped) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!buffer_storage_flags_valid(ctx, flags))
        return;
    native_storage =
        ctx->native_dispatch != NULL && ctx->native_dispatch->glesBufferStorageEXT != NULL &&
        mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_EXT_buffer_storage");
    if ((flags & GL_MAP_PERSISTENT_BIT) != 0u && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesBufferData != NULL &&
        (!native_storage || ctx->native_dispatch->glesMapBufferRange == NULL ||
         ctx->native_dispatch->glesUnmapBuffer == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    replacement = malloc((size_t)size);
    if (replacement == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    if (data != NULL)
        memcpy(replacement, data, (size_t)size);
    else
        memset(replacement, 0, (size_t)size);
    if (ctx->native_dispatch != NULL) {
        GLenum native_target = target;
        int restore =
            target == GL_TEXTURE_BUFFER ? native_texture_buffer_begin(ctx, obj, &native_target) : 0;
        if (target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER) {
            native_target = GL_COPY_WRITE_BUFFER;
            restore = 1;
            mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER, obj->native_name);
        }
        if (restore >= 0) {
            if (native_storage)
                ctx->native_dispatch->glesBufferStorageEXT(native_target, size, data, flags);
            else if (ctx->native_dispatch->glesBufferData != NULL)
                ctx->native_dispatch->glesBufferData(native_target, size, data, GL_DYNAMIC_DRAW);
            native_texture_buffer_end(ctx, restore);
        }
    }
    free(obj->shadow);
    obj->shadow = replacement;
    obj->size = (size_t)size;
    obj->immutable = GL_TRUE;
    obj->storage_flags = flags;
    obj->native_immutable = native_storage ? GL_TRUE : GL_FALSE;
    obj->native_mapping = NULL;
    obj->usage = GL_DYNAMIC_DRAW;
    obj->access = GL_READ_WRITE;
}

void glBufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (mgl_buffer_binding_slot(ctx, target) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_buffer(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    buffer_storage_allocate(ctx, obj, target, size, data, flags);
}

void glBufferStorageEXT(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags) {
    glBufferStorage(target, size, data, flags);
}

void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_BUFFER_STATE);
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_BUFFER_DATA);
    struct mgl_buffer *obj;
    unsigned char *replacement = NULL;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (mgl_buffer_binding_slot(ctx, target) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (size < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!valid_usage(usage)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_buffer(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (obj->immutable) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (size > 0 && !buffer_can_defer_shadow(ctx, obj, target)) {
        replacement = malloc((size_t)size);
        if (replacement == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        if (data != NULL)
            memcpy(replacement, data, (size_t)size);
        else
            memset(replacement, 0, (size_t)size);
    }
    free(obj->shadow);
    obj->shadow = replacement;
    obj->size = (size_t)size;
    obj->usage = usage;
    obj->access = GL_READ_WRITE;
    obj->mapped = GL_FALSE;
    obj->map_offset = 0u;
    obj->map_length = 0u;
    obj->map_access = 0u;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBufferData != NULL) {
        if (target == GL_TEXTURE_BUFFER || target == GL_PARAMETER_BUFFER ||
            target == GL_QUERY_BUFFER) {
            GLenum native_target = GL_TEXTURE_BUFFER;
            int restore = target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER
                              ? 1
                              : native_texture_buffer_begin(ctx, obj, &native_target);
            if (target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER) {
                native_target = GL_COPY_WRITE_BUFFER;
                mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER, obj->native_name);
            }
            if (restore >= 0) {
                {
                    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_BUFFER_DATA);
                    ctx->native_dispatch->glesBufferData(native_target, size, data, usage);
                }
                native_texture_buffer_end(ctx, restore);
            }
        } else {
            {
                MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_BUFFER_DATA);
                ctx->native_dispatch->glesBufferData(target, size, data, usage);
            }
        }
    }
}

void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    size_t off, len;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (mgl_buffer_binding_slot(ctx, target) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_buffer(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!checked_range(obj->size, offset, size, &off, &len)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if ((obj->immutable && (obj->storage_flags & GL_DYNAMIC_STORAGE_BIT) == 0u) ||
        (obj->mapped && (obj->map_access & GL_MAP_PERSISTENT_BIT) == 0u &&
         ranges_overlap(off, len, obj->map_offset, obj->map_length))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (len != 0u && data == NULL)
        return;
    if (len != 0u && obj->shadow != NULL)
        memcpy(obj->shadow + off, data, len);
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBufferSubData != NULL) {
        if (target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER ||
            (target == GL_TEXTURE_BUFFER && !native_texture_buffer_target_supported(ctx)))
            native_buffer_subdata_from_shadow(ctx, obj, offset, size, data);
        else
            ctx->native_dispatch->glesBufferSubData(target, offset, size, data);
    }
}

struct clear_buffer_format {
    GLenum internalformat;
    GLenum format;
    GLenum type;
    size_t bytes;
};

static const struct clear_buffer_format clear_buffer_formats[] = {
    {GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1u},
    {GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2u},
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4u},
    {GL_R32F, GL_RED, GL_FLOAT, 4u},
    {GL_RG32F, GL_RG, GL_FLOAT, 8u},
    {GL_RGBA32F, GL_RGBA, GL_FLOAT, 16u},
    {GL_R32I, GL_RED_INTEGER, GL_INT, 4u},
    {GL_RG32I, GL_RG_INTEGER, GL_INT, 8u},
    {GL_RGBA32I, GL_RGBA_INTEGER, GL_INT, 16u},
    {GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, 4u},
    {GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, 8u},
    {GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, 16u}};

static struct mgl_buffer *named_buffer_checked(struct mgl_context *ctx, GLuint name);

static void clear_buffer_range(struct mgl_context *ctx, GLenum target, GLuint named_buffer,
                               int named, GLenum internalformat, GLintptr offset, GLsizeiptr size,
                               GLenum format, GLenum type, const void *data, int entire_buffer) {
    struct mgl_buffer *obj;
    const struct clear_buffer_format *layout = NULL;
    size_t off, len, index, count;
    unsigned char *pattern;
    unsigned char value[16];
    if (ctx == NULL || reject_during_begin_end(ctx))
        return;
    if (named) {
        obj = named_buffer_checked(ctx, named_buffer);
        if (obj == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
    } else {
        if (mgl_buffer_binding_slot(ctx, target) == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
        }
        obj = mgl_bound_buffer(ctx, target);
        if (obj == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
    }
    for (index = 0u; index < sizeof(clear_buffer_formats) / sizeof(clear_buffer_formats[0]);
         ++index)
        if (clear_buffer_formats[index].internalformat == internalformat) {
            layout = &clear_buffer_formats[index];
            break;
        }
    if (layout == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (layout->format != format ||
        (layout->type != type &&
         !(layout->type == GL_UNSIGNED_INT && type == GL_INT &&
           (format == GL_RED_INTEGER || format == GL_RG_INTEGER || format == GL_RGBA_INTEGER)) &&
         !(layout->type == GL_INT && type == GL_UNSIGNED_INT &&
           (format == GL_RED_INTEGER || format == GL_RG_INTEGER || format == GL_RGBA_INTEGER)) &&
         !(layout->type == GL_UNSIGNED_BYTE && type == GL_FLOAT))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (entire_buffer) {
        offset = 0;
        size = (GLsizeiptr)obj->size;
    }
    if (!checked_range(obj->size, offset, size, &off, &len)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (off % layout->bytes != 0u || len % layout->bytes != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (obj->mapped && !(obj->map_access & GL_MAP_PERSISTENT_BIT)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (len == 0u)
        return;
    if (!mgl_buffer_shadow_ensure(ctx, obj))
        return;
    pattern = obj->shadow + off;
    if (data == NULL)
        memset(pattern, 0, len);
    else {
        if (layout->type == GL_UNSIGNED_BYTE && type == GL_FLOAT) {
            size_t c;
            for (c = 0u; c < layout->bytes; ++c) {
                GLfloat v;
                memcpy(&v, (const unsigned char *)data + c * sizeof(v), sizeof(v));
                if (!(v > 0.0f))
                    value[c] = 0u;
                else if (v >= 1.0f)
                    value[c] = 255u;
                else
                    value[c] = (unsigned char)(v * 255.0f + 0.5f);
            }
        } else if (layout->type == GL_UNSIGNED_INT && type == GL_INT) {
            size_t c;
            for (c = 0u; c < layout->bytes / sizeof(GLuint); ++c) {
                GLint v;
                GLuint u;
                memcpy(&v, (const unsigned char *)data + c * sizeof(v), sizeof(v));
                u = v < 0 ? 0u : (GLuint)v;
                memcpy(value + c * sizeof(u), &u, sizeof(u));
            }
        } else if (layout->type == GL_INT && type == GL_UNSIGNED_INT) {
            size_t c;
            for (c = 0u; c < layout->bytes / sizeof(GLint); ++c) {
                GLuint v;
                GLint i;
                memcpy(&v, (const unsigned char *)data + c * sizeof(v), sizeof(v));
                i = v > (GLuint)INT_MAX ? INT_MAX : (GLint)v;
                memcpy(value + c * sizeof(i), &i, sizeof(i));
            }
        } else
            memcpy(value, data, layout->bytes);
        count = len / layout->bytes;
        for (index = 0u; index < count; ++index)
            memcpy(pattern + index * layout->bytes, value, layout->bytes);
    }
    if (named || target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER ||
        (obj->native_immutable && (obj->storage_flags & GL_DYNAMIC_STORAGE_BIT) == 0u)) {
        (void)mgl_buffer_write_bytes(ctx, obj, off, len, pattern, 0);
    } else if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBufferSubData != NULL) {
        if (target == GL_TEXTURE_BUFFER && !native_texture_buffer_target_supported(ctx))
            native_buffer_subdata_from_shadow(ctx, obj, offset, size, pattern);
        else
            ctx->native_dispatch->glesBufferSubData(target, offset, size, pattern);
    }
}

void glClearBufferData(GLenum target, GLenum internalformat, GLenum format, GLenum type,
                       const void *data) {
    clear_buffer_range(mgl_context_current(), target, 0u, 0, internalformat, 0, 0, format, type,
                       data, 1);
}

void glClearBufferSubData(GLenum target, GLenum internalformat, GLintptr offset, GLsizeiptr size,
                          GLenum format, GLenum type, const void *data) {
    clear_buffer_range(mgl_context_current(), target, 0u, 0, internalformat, offset, size, format,
                       type, data, 0);
}

void glClearNamedBufferData(GLuint buffer, GLenum internalformat, GLenum format, GLenum type,
                            const void *data) {
    clear_buffer_range(mgl_context_current(), 0u, buffer, 1, internalformat, 0, 0, format, type,
                       data, 1);
}

void glClearNamedBufferSubData(GLuint buffer, GLenum internalformat, GLintptr offset,
                               GLsizeiptr size, GLenum format, GLenum type, const void *data) {
    clear_buffer_range(mgl_context_current(), 0u, buffer, 1, internalformat, offset, size, format,
                       type, data, 0);
}

void glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    size_t off, len;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (mgl_buffer_binding_slot(ctx, target) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_buffer(ctx, target);
    if (obj == NULL || (obj->mapped && (obj->map_access & GL_MAP_PERSISTENT_BIT) == 0u)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!checked_range(obj->size, offset, size, &off, &len)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (len != 0u && data == NULL)
        return;
    if (len != 0u)
        (void)buffer_read_back(ctx, obj, off, len, data);
}

static void *buffer_map_native(struct mgl_context *ctx, struct mgl_buffer *obj, GLenum target,
                               GLintptr offset, GLsizeiptr length, GLbitfield access) {
    GLenum native_target = (target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER)
                               ? GL_COPY_WRITE_BUFFER
                               : target;
    if (target == GL_TEXTURE_BUFFER && !native_texture_buffer_target_supported(ctx))
        return mgl_buffer_shadow_ensure(ctx, obj) ? obj->shadow + (size_t)offset : NULL;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesMapBufferRange == NULL ||
        ctx->native_dispatch->glesUnmapBuffer == NULL ||
        ((access & GL_MAP_FLUSH_EXPLICIT_BIT) != 0u &&
         ctx->native_dispatch->glesFlushMappedBufferRange == NULL))
        return mgl_buffer_shadow_ensure(ctx, obj) ? obj->shadow + (size_t)offset : NULL;
    if (target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER)
        mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER, obj->native_name);
    obj->native_mapping =
        ctx->native_dispatch->glesMapBufferRange(native_target, offset, length, access);
    if (target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER)
        mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER,
                                        native_buffer_name(ctx, ctx->copy_write_buffer));
    if (obj->native_mapping == NULL)
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
    return obj->native_mapping;
}

void *glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    size_t off, len;
    if (ctx == NULL)
        return NULL;
    if (reject_during_begin_end(ctx))
        return NULL;
    if (mgl_buffer_binding_slot(ctx, target) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return NULL;
    }
    if (!validate_map_access(ctx, access))
        return NULL;
    obj = mgl_bound_buffer(ctx, target);
    if (obj == NULL || obj->mapped) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    if (!validate_storage_map(ctx, obj, access))
        return NULL;
    if (!checked_range(obj->size, offset, length, &off, &len)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return NULL;
    }
    if (len == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    {
        void *mapped = buffer_map_native(ctx, obj, target, offset, length, access);
        if (mapped == NULL)
            return NULL;
        if (obj->native_mapping != NULL && (access & GL_MAP_WRITE_BIT) != 0u) {
            free(obj->shadow);
            obj->shadow = NULL;
        } else if (obj->native_mapping == NULL && obj->shadow != NULL) {
            if ((access & GL_MAP_INVALIDATE_BUFFER_BIT) != 0u)
                memset(obj->shadow, 0, obj->size);
            else if ((access & GL_MAP_INVALIDATE_RANGE_BIT) != 0u)
                memset(obj->shadow + off, 0, len);
        }
        obj->mapped = GL_TRUE;
        obj->access = access_enum_from_flags(access);
        obj->map_offset = off;
        obj->map_length = len;
        obj->map_access = access;
        return mapped;
    }
}

void *glMapBuffer(GLenum target, GLenum access) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    GLbitfield bits;
    if (ctx == NULL)
        return NULL;
    if (reject_during_begin_end(ctx))
        return NULL;
    if (mgl_buffer_binding_slot(ctx, target) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return NULL;
    }
    obj = mgl_bound_buffer(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    switch (access) {
        case GL_READ_ONLY:
            bits = GL_MAP_READ_BIT;
            break;
        case GL_WRITE_ONLY:
            bits = GL_MAP_WRITE_BIT;
            break;
        case GL_READ_WRITE:
            bits = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return NULL;
    }
    if (obj->size == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    return glMapBufferRange(target, 0, (GLsizeiptr)obj->size, bits);
}

void glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    size_t off, len;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (mgl_buffer_binding_slot(ctx, target) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_buffer(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (offset < 0 || length < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!obj->mapped || (obj->map_access & GL_MAP_FLUSH_EXPLICIT_BIT) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!checked_range(obj->map_length, offset, length, &off, &len)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (len != 0u && obj->native_mapping != NULL) {
        if (ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesFlushMappedBufferRange != NULL) {
            GLenum native_target = (target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER)
                                       ? GL_COPY_WRITE_BUFFER
                                       : target;
            if (target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER)
                mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER, obj->native_name);
            ctx->native_dispatch->glesFlushMappedBufferRange(native_target, offset, length);
            if (target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER)
                mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER,
                                                native_buffer_name(ctx, ctx->copy_write_buffer));
        }
        return;
    }
    if (len != 0u && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesBufferSubData != NULL) {
        if (target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER ||
            (target == GL_TEXTURE_BUFFER && !native_texture_buffer_target_supported(ctx)))
            native_buffer_subdata_from_shadow(ctx, obj, (GLintptr)(obj->map_offset + off),
                                              (GLsizeiptr)len, obj->shadow + obj->map_offset + off);
        else
            ctx->native_dispatch->glesBufferSubData(target, (GLintptr)(obj->map_offset + off),
                                                    (GLsizeiptr)len,
                                                    obj->shadow + obj->map_offset + off);
    }
}

GLboolean glUnmapBuffer(GLenum target) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    if (ctx == NULL)
        return GL_FALSE;
    if (reject_during_begin_end(ctx))
        return GL_FALSE;
    if (mgl_buffer_binding_slot(ctx, target) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return GL_FALSE;
    }
    obj = mgl_bound_buffer(ctx, target);
    if (obj == NULL || !obj->mapped) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    if (obj->native_mapping != NULL) {
        GLboolean ok;
        if (target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER)
            mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER, obj->native_name);
        ok = ctx->native_dispatch->glesUnmapBuffer(
            (target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER) ? GL_COPY_WRITE_BUFFER
                                                                         : target);
        if (target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER)
            mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER,
                                            native_buffer_name(ctx, ctx->copy_write_buffer));
        obj->native_mapping = NULL;
        obj->mapped = GL_FALSE;
        obj->map_offset = 0u;
        obj->map_length = 0u;
        obj->map_access = 0u;
        return ok;
    }
    if (obj->map_length != 0u && (obj->map_access & GL_MAP_WRITE_BIT) != 0u &&
        (obj->map_access & GL_MAP_FLUSH_EXPLICIT_BIT) == 0u && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesBufferSubData != NULL) {
        if (target == GL_PARAMETER_BUFFER || target == GL_QUERY_BUFFER ||
            (target == GL_TEXTURE_BUFFER && !native_texture_buffer_target_supported(ctx)))
            native_buffer_subdata_from_shadow(ctx, obj, (GLintptr)obj->map_offset,
                                              (GLsizeiptr)obj->map_length,
                                              obj->shadow + obj->map_offset);
        else
            ctx->native_dispatch->glesBufferSubData(target, (GLintptr)obj->map_offset,
                                                    (GLsizeiptr)obj->map_length,
                                                    obj->shadow + obj->map_offset);
    }
    obj->mapped = GL_FALSE;
    obj->map_offset = 0u;
    obj->map_length = 0u;
    obj->map_access = 0u;
    return GL_TRUE;
}

void glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    if (ctx == NULL || params == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (mgl_buffer_binding_slot(ctx, target) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_buffer(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    switch (pname) {
        case GL_BUFFER_SIZE:
            if (obj->size > (size_t)INT_MAX)
                *params = INT_MAX;
            else
                *params = (GLint)obj->size;
            break;
        case GL_BUFFER_USAGE:
            *params = (GLint)obj->usage;
            break;
        case GL_BUFFER_ACCESS:
            *params = (GLint)obj->access;
            break;
        case GL_BUFFER_MAPPED:
            *params = obj->mapped ? 1 : 0;
            break;
        case GL_BUFFER_MAP_OFFSET:
            if (obj->map_offset > (size_t)INT_MAX)
                *params = INT_MAX;
            else
                *params = (GLint)obj->map_offset;
            break;
        case GL_BUFFER_MAP_LENGTH:
            if (obj->map_length > (size_t)INT_MAX)
                *params = INT_MAX;
            else
                *params = (GLint)obj->map_length;
            break;
        case GL_BUFFER_ACCESS_FLAGS:
            *params = (GLint)obj->map_access;
            break;
        case GL_BUFFER_IMMUTABLE_STORAGE:
            *params = obj->immutable ? GL_TRUE : GL_FALSE;
            break;
        case GL_BUFFER_STORAGE_FLAGS:
            *params = (GLint)obj->storage_flags;
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
}

void glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    if (ctx == NULL || params == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (mgl_buffer_binding_slot(ctx, target) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_buffer(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    switch (pname) {
        case GL_BUFFER_SIZE:
            *params = (GLint64)obj->size;
            return;
        case GL_BUFFER_MAP_OFFSET:
            *params = (GLint64)obj->map_offset;
            return;
        case GL_BUFFER_MAP_LENGTH:
            *params = (GLint64)obj->map_length;
            return;
        default: {
            GLint value = 0;
            uint64_t error_sequence_before = ctx->errors.sequence;
            glGetBufferParameteriv(target, pname, &value);
            if (ctx->errors.sequence == error_sequence_before)
                *params = (GLint64)value;
        }
    }
}

void glGetBufferPointerv(GLenum target, GLenum pname, void **params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    if (ctx == NULL || params == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (mgl_buffer_binding_slot(ctx, target) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (pname != GL_BUFFER_MAP_POINTER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_bound_buffer(ctx, target);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    *params = buffer_map_pointer(obj);
}

void glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset,
                         GLintptr writeOffset, GLsizeiptr size) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *src;
    struct mgl_buffer *dst;
    size_t roff, woff, len;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (mgl_buffer_binding_slot(ctx, readTarget) == NULL ||
        mgl_buffer_binding_slot(ctx, writeTarget) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    src = mgl_bound_buffer(ctx, readTarget);
    dst = mgl_bound_buffer(ctx, writeTarget);
    if (src == NULL || dst == NULL || src->mapped || dst->mapped) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!checked_range(src->size, readOffset, size, &roff, &len) ||
        !checked_range(dst->size, writeOffset, size, &woff, NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (src == dst && len != 0u && roff < woff + len && woff < roff + len) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (len != 0u && src->shadow != NULL && dst->shadow != NULL)
        memmove(dst->shadow + woff, src->shadow + roff, len);
    else if (len != 0u && dst->shadow != NULL) {
        free(dst->shadow);
        dst->shadow = NULL;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesCopyBufferSubData != NULL &&
        ((readTarget != GL_TEXTURE_BUFFER && writeTarget != GL_TEXTURE_BUFFER) ||
         native_texture_buffer_target_supported(ctx))) {
        if (readTarget == GL_PARAMETER_BUFFER || writeTarget == GL_PARAMETER_BUFFER ||
            readTarget == GL_QUERY_BUFFER || writeTarget == GL_QUERY_BUFFER) {
            mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER, src->native_name);
            mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER, dst->native_name);
            ctx->native_dispatch->glesCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                                                        readOffset, writeOffset, size);
            mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER,
                                            native_buffer_name(ctx, ctx->copy_read_buffer));
            mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER,
                                            native_buffer_name(ctx, ctx->copy_write_buffer));
        } else {
            ctx->native_dispatch->glesCopyBufferSubData(readTarget, writeTarget, readOffset,
                                                        writeOffset, size);
        }
    } else if (len != 0u) {
        if (!mgl_buffer_shadow_ensure(ctx, src) || !mgl_buffer_shadow_ensure(ctx, dst))
            return;
        memmove(dst->shadow + woff, src->shadow + roff, len);
        native_buffer_subdata_from_shadow(ctx, dst, writeOffset, size, dst->shadow + woff);
    }
}

static struct mgl_transform_feedback *bound_transform_feedback(struct mgl_context *ctx) {
    struct mgl_transform_feedback *tf;
    if (ctx == NULL || ctx->transform_feedback_binding == 0u)
        return NULL;
    for (tf = ctx->transform_feedbacks; tf != NULL; tf = tf->next)
        if (tf->name == ctx->transform_feedback_binding)
            return tf;
    return NULL;
}

static GLuint indexed_binding_limit(struct mgl_context *ctx, GLenum target) {
    GLenum pname;
    GLint limit;
    if (target == GL_UNIFORM_BUFFER)
        pname = GL_MAX_UNIFORM_BUFFER_BINDINGS;
    else if (target == GL_SHADER_STORAGE_BUFFER)
        pname = GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS;
    else if (target == GL_ATOMIC_COUNTER_BUFFER)
        pname = GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS;
    else if (target == GL_TRANSFORM_FEEDBACK_BUFFER)
        pname = GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS;
    else
        return 0u;
    limit = (GLint)MGL_MAX_INDEXED_BUFFER_BINDINGS;
    if (ctx != NULL && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesGetIntegerv != NULL) {
        GLint reported = 0;
        ctx->native_dispatch->glesGetIntegerv(pname, &reported);
        if (reported > 0 && reported < limit)
            limit = reported;
    }
    return (GLuint)limit;
}

static struct mgl_indexed_buffer_binding *indexed_binding_slot(struct mgl_context *ctx,
                                                               GLenum target, GLuint index) {
    struct mgl_transform_feedback *tf;
    if (ctx == NULL || index >= indexed_binding_limit(ctx, target))
        return NULL;
    if (target == GL_UNIFORM_BUFFER)
        return &ctx->uniform_buffer_bindings[index];
    if (target == GL_SHADER_STORAGE_BUFFER)
        return &ctx->shader_storage_buffer_bindings[index];
    if (target == GL_ATOMIC_COUNTER_BUFFER)
        return &ctx->atomic_counter_buffer_bindings[index];
    if (target == GL_TRANSFORM_FEEDBACK_BUFFER) {
        tf = bound_transform_feedback(ctx);
        return tf != NULL ? &tf->buffer_bindings[index]
                          : &ctx->transform_feedback_buffer_bindings[index];
    }
    return NULL;
}

void glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_indexed_buffer_binding *slot;
    struct mgl_buffer *obj = NULL;
    GLuint native_name = 0u;
    GLuint *generic;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    slot = indexed_binding_slot(ctx, target, index);
    if (slot == NULL) {
        mgl_error_set(&ctx->errors, (target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER ||
                                       target == GL_TRANSFORM_FEEDBACK_BUFFER ||
                                       target == GL_ATOMIC_COUNTER_BUFFER)
                                  ? GL_INVALID_VALUE : GL_INVALID_ENUM);
        return;
    }
    generic = mgl_buffer_binding_slot(ctx, target);
    if (buffer != 0u) {
        (void)pthread_mutex_lock(&ctx->share_group->lock);
        obj = mgl_buffer_ensure(ctx, buffer, 1);
        if (obj != NULL) {
            obj->realized = GL_TRUE;
            native_name = obj->native_name;
        }
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        if (obj == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
    }
    slot->buffer = buffer;
    slot->offset = 0;
    slot->size = obj != NULL ? (GLsizeiptr)obj->size : 0;
    slot->ranged = GL_FALSE;
    if (generic != NULL)
        *generic = buffer;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindBufferBase != NULL)
        mgl_native_dispatch_bind_buffer_base(ctx, target, index, native_name);
}

void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset,
                       GLsizeiptr size) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_indexed_buffer_binding *slot;
    struct mgl_buffer *obj = NULL;
    GLuint native_name = 0u;
    GLuint *generic;
    size_t off, len;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    slot = indexed_binding_slot(ctx, target, index);
    if (slot == NULL) {
        mgl_error_set(&ctx->errors, (target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER ||
                                       target == GL_TRANSFORM_FEEDBACK_BUFFER ||
                                       target == GL_ATOMIC_COUNTER_BUFFER)
                                  ? GL_INVALID_VALUE : GL_INVALID_ENUM);
        return;
    }
    if (buffer == 0u) {
        if (offset < 0 || size < 0) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        slot->buffer = 0u;
        slot->offset = 0;
        slot->size = 0;
        slot->ranged = GL_FALSE;
        generic = mgl_buffer_binding_slot(ctx, target);
        if (generic != NULL)
            *generic = 0u;
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindBufferRange != NULL)
            mgl_native_dispatch_bind_buffer_range(ctx, target, index, 0u, offset, size);
        return;
    }
    if (size <= 0 || offset < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    obj = mgl_buffer_ensure(ctx, buffer, 1);
    if (obj != NULL) {
        obj->realized = GL_TRUE;
        native_name = obj->native_name;
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    if (!checked_range(obj->size, offset, size, &off, &len) || len == 0u ||
        (target == GL_ATOMIC_COUNTER_BUFFER && (off & 3u) != 0u)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    slot->buffer = buffer;
    slot->offset = (GLintptr)off;
    slot->size = (GLsizeiptr)len;
    slot->ranged = GL_TRUE;
    generic = mgl_buffer_binding_slot(ctx, target);
    if (generic != NULL)
        *generic = buffer;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindBufferRange != NULL)
        mgl_native_dispatch_bind_buffer_range(ctx, target, index, native_name, offset, size);
}

static int indexed_target_supported(GLenum target) {
    return target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER ||
           target == GL_TRANSFORM_FEEDBACK_BUFFER || target == GL_ATOMIC_COUNTER_BUFFER;
}

static int existing_buffer_info(struct mgl_context *ctx, GLuint name, GLuint *native_name,
                                size_t *size) {
    struct mgl_buffer *obj;
    if (ctx == NULL || name == 0u)
        return 0;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    obj = mgl_buffer_find(ctx, name);
    if (obj != NULL) {
        obj->realized = GL_TRUE;
        if (native_name != NULL)
            *native_name = obj->native_name;
        if (size != NULL)
            *size = obj->size;
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return obj != NULL;
}

void glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint *buffers) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (!indexed_target_supported(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (first > indexed_binding_limit(ctx, target) ||
        (GLuint)count > indexed_binding_limit(ctx, target) - first) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    for (i = 0; i < count; ++i) {
        const GLuint index = first + (GLuint)i;
        const GLuint name = buffers != NULL ? buffers[i] : 0u;
        struct mgl_indexed_buffer_binding *slot = indexed_binding_slot(ctx, target, index);
        GLuint native_name = 0u;
        size_t object_size = 0u;
        if (name != 0u && !existing_buffer_info(ctx, name, &native_name, &object_size)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            continue;
        }
        slot->buffer = name;
        slot->offset = 0;
        slot->size = name != 0u ? (GLsizeiptr)object_size : 0;
        slot->ranged = GL_FALSE;
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindBufferBase != NULL)
            mgl_native_dispatch_bind_buffer_base(ctx, target, index, native_name);
    }
}

void glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint *buffers,
                        const GLintptr *offsets, const GLsizeiptr *sizes) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (!indexed_target_supported(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (first > indexed_binding_limit(ctx, target) ||
        (GLuint)count > indexed_binding_limit(ctx, target) - first) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (buffers != NULL && count > 0 && (offsets == NULL || sizes == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < count; ++i) {
        const GLuint index = first + (GLuint)i;
        const GLuint name = buffers != NULL ? buffers[i] : 0u;
        struct mgl_indexed_buffer_binding *slot = indexed_binding_slot(ctx, target, index);
        GLuint native_name = 0u;
        size_t object_size = 0u, off = 0u, len = 0u;
        GLintptr requested_offset = 0;
        GLsizeiptr requested_size = 0;
        if (buffers != NULL) {
            requested_offset = offsets[i];
            requested_size = sizes[i];
            if (requested_offset < 0 || requested_size <= 0) {
                mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
                continue;
            }
        }
        if (name != 0u) {
            if (!existing_buffer_info(ctx, name, &native_name, &object_size)) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                continue;
            }
            if (!checked_range(object_size, requested_offset, requested_size, &off, &len) ||
                len == 0u ||
                (target == GL_ATOMIC_COUNTER_BUFFER && (off & 3u) != 0u)) {
                mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
                continue;
            }
            slot->buffer = name;
            slot->offset = (GLintptr)off;
            slot->size = (GLsizeiptr)len;
            slot->ranged = GL_TRUE;
        } else {
            memset(slot, 0, sizeof(*slot));
        }
        if (ctx->native_dispatch != NULL) {
            if (ctx->native_dispatch->glesBindBufferRange != NULL)
                mgl_native_dispatch_bind_buffer_range(ctx, target, index, native_name,
                                                      name != 0u ? requested_offset : 0,
                                                      name != 0u ? requested_size : 0);
            else if (name == 0u && ctx->native_dispatch->glesBindBufferBase != NULL)
                mgl_native_dispatch_bind_buffer_base(ctx, target, index, 0u);
        }
    }
}

void glBindBufferBaseARB(GLenum target, GLuint index, GLuint buffer) {
    glBindBufferBase(target, index, buffer);
}

void glBindBufferRangeARB(GLenum target, GLuint index, GLuint buffer, GLintptr offset,
                          GLsizeiptr size) {
    glBindBufferRange(target, index, buffer, offset, size);
}

static struct mgl_buffer *named_buffer_checked(struct mgl_context *ctx, GLuint name) {
    struct mgl_buffer *obj;
    if (ctx == NULL || name == 0u)
        return NULL;
    obj = mgl_buffer_find(ctx, name);
    if (obj == NULL || !obj->realized)
        return NULL;
    return obj;
}

static GLuint interface_array_native_name(struct mgl_context *ctx) {
    struct mgl_buffer *obj;
    if (ctx == NULL || ctx->array_buffer == 0u)
        return 0u;
    obj = mgl_buffer_find(ctx, ctx->array_buffer);
    return obj != NULL ? obj->native_name : 0u;
}

static void named_native_bind(struct mgl_context *ctx, struct mgl_buffer *obj) {
    if (ctx != NULL && obj != NULL && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesBindBuffer != NULL)
        mgl_native_dispatch_bind_buffer(ctx, GL_ARRAY_BUFFER, obj->native_name);
}

static void named_native_restore(struct mgl_context *ctx) {
    if (ctx != NULL && ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindBuffer != NULL)
        mgl_native_dispatch_bind_buffer(ctx, GL_ARRAY_BUFFER, interface_array_native_name(ctx));
}

void glCreateBuffers(GLsizei n, GLuint *buffers) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    uint64_t generation_sequence = ctx->errors.sequence;
    glGenBuffers(n, buffers);
    if (n <= 0 || buffers == NULL || ctx->errors.sequence != generation_sequence)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < n; ++i) {
        struct mgl_buffer *obj = mgl_buffer_find(ctx, buffers[i]);
        if (obj != NULL)
            obj->realized = GL_TRUE;
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glNamedBufferData(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    unsigned char *replacement = NULL;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (size < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!valid_usage(usage)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = named_buffer_checked(ctx, buffer);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (obj->immutable) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (size > 0 && !buffer_can_defer_shadow(ctx, obj, GL_ARRAY_BUFFER)) {
        replacement = malloc((size_t)size);
        if (replacement == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        if (data != NULL)
            memcpy(replacement, data, (size_t)size);
        else
            memset(replacement, 0, (size_t)size);
    }
    free(obj->shadow);
    obj->shadow = replacement;
    obj->size = (size_t)size;
    obj->usage = usage;
    obj->access = GL_READ_WRITE;
    obj->mapped = GL_FALSE;
    obj->map_offset = 0u;
    obj->map_length = 0u;
    obj->map_access = 0u;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBufferData != NULL) {
        named_native_bind(ctx, obj);
        ctx->native_dispatch->glesBufferData(GL_ARRAY_BUFFER, size, data, usage);
        named_native_restore(ctx);
    }
}

void glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    obj = named_buffer_checked(ctx, buffer);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    named_native_bind(ctx, obj);
    buffer_storage_allocate(ctx, obj, GL_ARRAY_BUFFER, size, data, flags);
    named_native_restore(ctx);
}

void glNamedBufferStorageEXT(GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags) {
    glNamedBufferStorage(buffer, size, data, flags);
}

void glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    size_t off, len;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    obj = named_buffer_checked(ctx, buffer);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!checked_range(obj->size, offset, size, &off, &len)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if ((obj->immutable && (obj->storage_flags & GL_DYNAMIC_STORAGE_BIT) == 0u) ||
        (obj->mapped && (obj->map_access & GL_MAP_PERSISTENT_BIT) == 0u &&
         ranges_overlap(off, len, obj->map_offset, obj->map_length))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (len != 0u && data == NULL)
        return;
    if (len != 0u && obj->shadow != NULL)
        memcpy(obj->shadow + off, data, len);
    if (len != 0u && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesBufferSubData != NULL) {
        named_native_bind(ctx, obj);
        ctx->native_dispatch->glesBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
        named_native_restore(ctx);
    }
}

void glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, void *data) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    size_t off, len;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    obj = named_buffer_checked(ctx, buffer);
    if (obj == NULL || (obj->mapped && (obj->map_access & GL_MAP_PERSISTENT_BIT) == 0u)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!checked_range(obj->size, offset, size, &off, &len)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (len != 0u && data == NULL)
        return;
    if (len != 0u)
        (void)buffer_read_back(ctx, obj, off, len, data);
}

void *glMapNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    size_t off, len;
    if (ctx == NULL)
        return NULL;
    if (reject_during_begin_end(ctx))
        return NULL;
    if (!validate_map_access(ctx, access))
        return NULL;
    obj = named_buffer_checked(ctx, buffer);
    if (obj == NULL || obj->mapped) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    if (!validate_storage_map(ctx, obj, access))
        return NULL;
    if (!checked_range(obj->size, offset, length, &off, &len)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return NULL;
    }
    if (len == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    {
        void *mapped;
        named_native_bind(ctx, obj);
        mapped = buffer_map_native(ctx, obj, GL_ARRAY_BUFFER, offset, length, access);
        named_native_restore(ctx);
        if (mapped == NULL)
            return NULL;
        if (obj->native_mapping != NULL && (access & GL_MAP_WRITE_BIT) != 0u) {
            free(obj->shadow);
            obj->shadow = NULL;
        } else if (obj->native_mapping == NULL && obj->shadow != NULL) {
            if ((access & GL_MAP_INVALIDATE_BUFFER_BIT) != 0u)
                memset(obj->shadow, 0, obj->size);
            else if ((access & GL_MAP_INVALIDATE_RANGE_BIT) != 0u)
                memset(obj->shadow + off, 0, len);
        }
        obj->mapped = GL_TRUE;
        obj->access = access_enum_from_flags(access);
        obj->map_offset = off;
        obj->map_length = len;
        obj->map_access = access;
        return mapped;
    }
}

void *glMapNamedBuffer(GLuint buffer, GLenum access) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    GLbitfield bits;
    if (ctx == NULL)
        return NULL;
    if (reject_during_begin_end(ctx))
        return NULL;
    obj = named_buffer_checked(ctx, buffer);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    switch (access) {
        case GL_READ_ONLY:
            bits = GL_MAP_READ_BIT;
            break;
        case GL_WRITE_ONLY:
            bits = GL_MAP_WRITE_BIT;
            break;
        case GL_READ_WRITE:
            bits = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return NULL;
    }
    if (obj->size == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    return glMapNamedBufferRange(buffer, 0, (GLsizeiptr)obj->size, bits);
}

void glFlushMappedNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    size_t off, len;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    obj = named_buffer_checked(ctx, buffer);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (offset < 0 || length < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!obj->mapped || (obj->map_access & GL_MAP_FLUSH_EXPLICIT_BIT) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!checked_range(obj->map_length, offset, length, &off, &len)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (len != 0u && obj->native_mapping != NULL) {
        if (ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesFlushMappedBufferRange != NULL) {
            named_native_bind(ctx, obj);
            ctx->native_dispatch->glesFlushMappedBufferRange(GL_ARRAY_BUFFER, offset, length);
            named_native_restore(ctx);
        }
        return;
    }
    if (len != 0u && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesBufferSubData != NULL) {
        named_native_bind(ctx, obj);
        ctx->native_dispatch->glesBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(obj->map_offset + off),
                                                (GLsizeiptr)len,
                                                obj->shadow + obj->map_offset + off);
        named_native_restore(ctx);
    }
}

GLboolean glUnmapNamedBuffer(GLuint buffer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    if (ctx == NULL)
        return GL_FALSE;
    if (reject_during_begin_end(ctx))
        return GL_FALSE;
    obj = named_buffer_checked(ctx, buffer);
    if (obj == NULL || !obj->mapped) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    if (obj->native_mapping != NULL) {
        GLboolean ok;
        named_native_bind(ctx, obj);
        ok = ctx->native_dispatch->glesUnmapBuffer(GL_ARRAY_BUFFER);
        named_native_restore(ctx);
        obj->native_mapping = NULL;
        obj->mapped = GL_FALSE;
        obj->map_offset = 0u;
        obj->map_length = 0u;
        obj->map_access = 0u;
        return ok;
    }
    if (obj->map_length != 0u && (obj->map_access & GL_MAP_WRITE_BIT) != 0u &&
        (obj->map_access & GL_MAP_FLUSH_EXPLICIT_BIT) == 0u && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesBufferSubData != NULL) {
        named_native_bind(ctx, obj);
        ctx->native_dispatch->glesBufferSubData(GL_ARRAY_BUFFER, (GLintptr)obj->map_offset,
                                                (GLsizeiptr)obj->map_length,
                                                obj->shadow + obj->map_offset);
        named_native_restore(ctx);
    }
    obj->mapped = GL_FALSE;
    obj->map_offset = 0u;
    obj->map_length = 0u;
    obj->map_access = 0u;
    return GL_TRUE;
}

void glGetNamedBufferParameteriv(GLuint buffer, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    if (ctx == NULL || params == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    obj = named_buffer_checked(ctx, buffer);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    switch (pname) {
        case GL_BUFFER_SIZE:
            *params = obj->size > (size_t)INT_MAX ? INT_MAX : (GLint)obj->size;
            break;
        case GL_BUFFER_USAGE:
            *params = (GLint)obj->usage;
            break;
        case GL_BUFFER_ACCESS:
            *params = (GLint)obj->access;
            break;
        case GL_BUFFER_MAPPED:
            *params = obj->mapped ? GL_TRUE : GL_FALSE;
            break;
        case GL_BUFFER_MAP_OFFSET:
            *params = obj->map_offset > (size_t)INT_MAX ? INT_MAX : (GLint)obj->map_offset;
            break;
        case GL_BUFFER_MAP_LENGTH:
            *params = obj->map_length > (size_t)INT_MAX ? INT_MAX : (GLint)obj->map_length;
            break;
        case GL_BUFFER_ACCESS_FLAGS:
            *params = (GLint)obj->map_access;
            break;
        case GL_BUFFER_IMMUTABLE_STORAGE:
            *params = obj->immutable ? GL_TRUE : GL_FALSE;
            break;
        case GL_BUFFER_STORAGE_FLAGS:
            *params = (GLint)obj->storage_flags;
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
}

void glGetNamedBufferParameteri64v(GLuint buffer, GLenum pname, GLint64 *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    if (ctx == NULL || params == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    obj = named_buffer_checked(ctx, buffer);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    switch (pname) {
        case GL_BUFFER_SIZE:
            *params = (GLint64)obj->size;
            break;
        case GL_BUFFER_MAP_OFFSET:
            *params = (GLint64)obj->map_offset;
            break;
        case GL_BUFFER_MAP_LENGTH:
            *params = (GLint64)obj->map_length;
            break;
        default: {
            GLint value = 0;
            uint64_t error_sequence_before = ctx->errors.sequence;
            glGetNamedBufferParameteriv(buffer, pname, &value);
            if (ctx->errors.sequence == error_sequence_before)
                *params = (GLint64)value;
            break;
        }
    }
}

void glGetNamedBufferPointerv(GLuint buffer, GLenum pname, void **params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    if (ctx == NULL || params == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (pname != GL_BUFFER_MAP_POINTER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = named_buffer_checked(ctx, buffer);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    *params = buffer_map_pointer(obj);
}

void glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset,
                              GLintptr writeOffset, GLsizeiptr size) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *src, *dst;
    size_t roff, woff, len;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    src = named_buffer_checked(ctx, readBuffer);
    dst = named_buffer_checked(ctx, writeBuffer);
    if (src == NULL || dst == NULL || src->mapped || dst->mapped) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!checked_range(src->size, readOffset, size, &roff, &len) ||
        !checked_range(dst->size, writeOffset, size, &woff, NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (src == dst && len != 0u && roff < woff + len && woff < roff + len) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (len == 0u)
        return;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesCopyBufferSubData != NULL &&
        ctx->native_dispatch->glesBindBuffer != NULL) {
        const GLuint restore_read = native_buffer_name(ctx, ctx->copy_read_buffer);
        const GLuint restore_write = native_buffer_name(ctx, ctx->copy_write_buffer);
        if (src->shadow != NULL && dst->shadow != NULL)
            memmove(dst->shadow + woff, src->shadow + roff, len);
        else if (dst->shadow != NULL) {
            free(dst->shadow);
            dst->shadow = NULL;
        }
        mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER, src->native_name);
        mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER, dst->native_name);
        ctx->native_dispatch->glesCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                                                    readOffset, writeOffset, size);
        mgl_native_dispatch_bind_buffer(ctx, GL_COPY_READ_BUFFER, restore_read);
        mgl_native_dispatch_bind_buffer(ctx, GL_COPY_WRITE_BUFFER, restore_write);
        return;
    }
    if (!mgl_buffer_shadow_ensure(ctx, src) || !mgl_buffer_shadow_ensure(ctx, dst))
        return;
    memmove(dst->shadow + woff, src->shadow + roff, len);
    native_buffer_subdata_from_shadow(ctx, dst, writeOffset, size, dst->shadow + woff);
}

void glNamedBufferDataEXT(GLuint b, GLsizeiptr s, const void *d, GLenum u) {
    glNamedBufferData(b, s, d, u);
}

void glNamedBufferSubDataEXT(GLuint b, GLintptr o, GLsizeiptr s, const void *d) {
    glNamedBufferSubData(b, o, s, d);
}

void glGetNamedBufferSubDataEXT(GLuint b, GLintptr o, GLsizeiptr s, void *d) {
    glGetNamedBufferSubData(b, o, s, d);
}

void *glMapNamedBufferEXT(GLuint b, GLenum a) {
    return glMapNamedBuffer(b, a);
}

void *glMapNamedBufferRangeEXT(GLuint b, GLintptr o, GLsizeiptr l, GLbitfield a) {
    return glMapNamedBufferRange(b, o, l, a);
}

void glFlushMappedNamedBufferRangeEXT(GLuint b, GLintptr o, GLsizeiptr l) {
    glFlushMappedNamedBufferRange(b, o, l);
}

GLboolean glUnmapNamedBufferEXT(GLuint b) {
    return glUnmapNamedBuffer(b);
}

void glGetNamedBufferParameterivEXT(GLuint b, GLenum p, GLint *v) {
    glGetNamedBufferParameteriv(b, p, v);
}

void glGetNamedBufferPointervEXT(GLuint b, GLenum p, void **v) {
    glGetNamedBufferPointerv(b, p, v);
}

void glGenBuffersARB(GLsizei n, GLuint *buffers) {
    glGenBuffers(n, buffers);
}

void glDeleteBuffersARB(GLsizei n, const GLuint *buffers) {
    glDeleteBuffers(n, buffers);
}

void glBindBufferARB(GLenum target, GLuint buffer) {
    glBindBuffer(target, buffer);
}

GLboolean glIsBufferARB(GLuint buffer) {
    return glIsBuffer(buffer);
}

void glBufferDataARB(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    glBufferData(target, size, data, usage);
}

void glBufferSubDataARB(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    glBufferSubData(target, offset, size, data);
}

void glGetBufferSubDataARB(GLenum target, GLintptr offset, GLsizeiptr size, void *data) {
    glGetBufferSubData(target, offset, size, data);
}

void *glMapBufferARB(GLenum target, GLenum access) {
    return glMapBuffer(target, access);
}

GLboolean glUnmapBufferARB(GLenum target) {
    return glUnmapBuffer(target);
}

void glGetBufferParameterivARB(GLenum target, GLenum pname, GLint *params) {
    glGetBufferParameteriv(target, pname, params);
}

void glGetBufferPointervARB(GLenum target, GLenum pname, void **params) {
    glGetBufferPointerv(target, pname, params);
}

void glInvalidateBufferData(GLuint buffer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    obj = mgl_buffer_find(ctx, buffer);
    if (obj == NULL || !obj->realized)
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
}

void glInvalidateBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr length) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    obj = mgl_buffer_find(ctx, buffer);
    if (obj == NULL || !obj->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!checked_range(obj->size, offset, length, NULL, NULL))
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
}

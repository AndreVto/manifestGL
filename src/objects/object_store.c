#include "objects/object_store.h"
#include "shader/translator.h"

#include <stdlib.h>

static void advance_atomic_name(atomic_uint *counter, GLuint name) {
    unsigned current = atomic_load_explicit(counter, memory_order_relaxed);
    while (current <= name &&
           !atomic_compare_exchange_weak_explicit(counter, &current, name + 1u,
                                                  memory_order_relaxed, memory_order_relaxed)) {}
}

/* Buffer objects can number in the thousands. Draws resolve each enabled
 * VBO under the share-group lock; avoid an O(number of buffers) list walk.
 * This uses the same fixed-size multiplicative hash as the texture store. */
static unsigned buffer_bucket(GLuint name) {
    return (unsigned)(((uint32_t)name * UINT32_C(2654435761)) >> 22u);
}

static void buffer_index_unlink(struct mgl_context *ctx, struct mgl_buffer *obj) {
    struct mgl_buffer **link;
    if (ctx == NULL || ctx->share_group == NULL || obj == NULL)
        return;
    link = &ctx->share_group->buffer_index[buffer_bucket(obj->name)];
    while (*link != NULL && *link != obj)
        link = &(*link)->hash_next;
    if (*link == obj)
        *link = obj->hash_next;
    obj->hash_next = NULL;
}

struct mgl_buffer *mgl_buffer_find(struct mgl_context *ctx, GLuint name) {
    struct mgl_buffer *it;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return NULL;
    for (it = ctx->share_group->buffer_index[buffer_bucket(name)]; it != NULL; it = it->hash_next) {
        if (it->name == name && !it->delete_pending)
            return it;
    }
    return NULL;
}

struct mgl_buffer *mgl_buffer_ensure(struct mgl_context *ctx, GLuint name, int generate_native) {
    struct mgl_buffer *obj;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return NULL;
    obj = mgl_buffer_find(ctx, name);
    if (obj != NULL)
        return obj;
    obj = calloc(1, sizeof(*obj));
    if (obj == NULL)
        return NULL;
    obj->name = name;
    obj->usage = GL_STATIC_DRAW;
    obj->access = GL_READ_WRITE;
    if (generate_native && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesGenBuffers != NULL) {
        ctx->native_dispatch->glesGenBuffers(1, &obj->native_name);
    }
    obj->next = ctx->share_group->buffers;
    ctx->share_group->buffers = obj;
    obj->hash_next = ctx->share_group->buffer_index[buffer_bucket(name)];
    ctx->share_group->buffer_index[buffer_bucket(name)] = obj;
    advance_atomic_name(&ctx->share_group->next_buffer_name, name);
    return obj;
}

static void destroy_buffer_object(struct mgl_context *ctx, struct mgl_buffer *victim) {
    if (victim == NULL)
        return;
    if (victim->native_name != 0u && ctx != NULL && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesDeleteBuffers != NULL) {
        ctx->native_buffer_binding_known = 0u;
        ctx->native_dispatch->glesDeleteBuffers(1, &victim->native_name);
    }
    free(victim->shadow);
    free(victim);
}

static void unlink_buffer_object(struct mgl_context *ctx, struct mgl_buffer *victim) {
    struct mgl_buffer **link;
    if (ctx == NULL || ctx->share_group == NULL || victim == NULL)
        return;
    link = &ctx->share_group->buffers;
    while (*link != NULL) {
        if (*link == victim) {
            buffer_index_unlink(ctx, victim);
            *link = victim->next;
            destroy_buffer_object(ctx, victim);
            return;
        }
        link = &(*link)->next;
    }
}

void mgl_buffer_remove(struct mgl_context *ctx, GLuint name) {
    struct mgl_buffer *victim;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return;
    victim = mgl_buffer_find(ctx, name);
    if (victim == NULL)
        return;
    (void)atomic_fetch_add_explicit(&ctx->share_group->buffer_revision, 1u, memory_order_release);
    victim->delete_pending = GL_TRUE;
    /* Texture-buffer attachments may retain the old object after its name
     * has been deleted. Unindex immediately so recreation of that numeric
     * name resolves the new object while the old one remains alive. */
    buffer_index_unlink(ctx, victim);
    if (victim->texture_refcount == 0u)
        unlink_buffer_object(ctx, victim);
}

void mgl_buffer_retain_texture(struct mgl_buffer *buffer) {
    if (buffer != NULL)
        ++buffer->texture_refcount;
}

void mgl_buffer_release_texture(struct mgl_context *ctx, struct mgl_buffer *buffer) {
    if (buffer == NULL || buffer->texture_refcount == 0u)
        return;
    --buffer->texture_refcount;
    if (buffer->texture_refcount == 0u && buffer->delete_pending)
        unlink_buffer_object(ctx, buffer);
}

GLuint *mgl_buffer_binding_slot(struct mgl_context *ctx, GLenum target) {
    if (ctx == NULL)
        return NULL;
    switch (target) {
        case GL_ARRAY_BUFFER:
            return &ctx->array_buffer;
        case GL_ELEMENT_ARRAY_BUFFER:
            return &ctx->current_vao->element_array_buffer;
        case GL_PIXEL_PACK_BUFFER:
            return &ctx->pixel_pack_buffer;
        case GL_PIXEL_UNPACK_BUFFER:
            return &ctx->pixel_unpack_buffer;
        case GL_COPY_READ_BUFFER:
            return &ctx->copy_read_buffer;
        case GL_COPY_WRITE_BUFFER:
            return &ctx->copy_write_buffer;
        case GL_UNIFORM_BUFFER:
            return &ctx->uniform_buffer;
        case GL_ATOMIC_COUNTER_BUFFER:
            return &ctx->atomic_counter_buffer;
        case GL_TRANSFORM_FEEDBACK_BUFFER:
            return &ctx->transform_feedback_buffer;
        case GL_DRAW_INDIRECT_BUFFER:
            return &ctx->draw_indirect_buffer;
        case GL_PARAMETER_BUFFER:
            return &ctx->parameter_buffer;
        case GL_QUERY_BUFFER:
            return &ctx->query_buffer;
        case GL_DISPATCH_INDIRECT_BUFFER:
            return &ctx->dispatch_indirect_buffer;
        case GL_SHADER_STORAGE_BUFFER:
            return &ctx->shader_storage_buffer;
        case GL_TEXTURE_BUFFER:
            return &ctx->texture_buffer;
        default:
            return NULL;
    }
}

struct mgl_buffer *mgl_bound_buffer(struct mgl_context *ctx, GLenum target) {
    GLuint *slot = mgl_buffer_binding_slot(ctx, target);
    return slot != NULL ? mgl_buffer_find(ctx, *slot) : NULL;
}

void mgl_restore_element_array_binding(struct mgl_context *ctx, GLuint name) {
    struct mgl_buffer *buffer;
    if (ctx == NULL || ctx->current_vao == NULL)
        return;
    ctx->current_vao->element_array_buffer = name;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesBindBuffer == NULL)
        return;
    buffer = mgl_buffer_find(ctx, name);
    mgl_native_dispatch_bind_buffer(ctx, GL_ELEMENT_ARRAY_BUFFER,
                                    buffer != NULL ? buffer->native_name : 0u);
}

void mgl_vertex_array_resolve(struct mgl_vertex_array *vao, GLuint index) {
    struct mgl_vertex_binding *attr;
    const struct mgl_vertex_buffer_slot *slot;
    GLuint binding;
    if (vao == NULL || index >= MGL_MAX_VERTEX_ATTRIBS)
        return;
    binding = vao->vertex_attrib_binding[index];
    if (binding >= MGL_MAX_VERTEX_ATTRIBS)
        return;
    attr = &vao->generic[index];
    slot = &vao->vertex_buffers[binding];
    attr->data = slot->data;
    attr->stride = attr->separate_stride ? slot->stride : attr->attrib_array_stride;
    attr->divisor = slot->divisor;
    if (attr->data.source == MGL_DATA_BUFFER) {
        if (attr->data.offset > SIZE_MAX - vao->vertex_attrib_relative_offset[index])
            attr->data.offset = SIZE_MAX;
        else
            attr->data.offset += vao->vertex_attrib_relative_offset[index];
    } else if (attr->data.client_ptr != NULL) {
        attr->data.client_ptr = (const unsigned char *)attr->data.client_ptr +
                                vao->vertex_attrib_relative_offset[index];
    }
}

void mgl_vertex_array_resolve_slot(struct mgl_vertex_array *vao, GLuint slot) {
    GLuint i;
    if (vao == NULL || slot >= MGL_MAX_VERTEX_ATTRIBS)
        return;
    for (i = 0u; i < MGL_MAX_VERTEX_ATTRIBS; ++i)
        if (vao->vertex_attrib_binding[i] == slot)
            mgl_vertex_array_resolve(vao, i);
}

void mgl_vertex_array_init(struct mgl_vertex_array *vao) {
    unsigned i;
    if (vao == NULL)
        return;
    for (i = 0; i < MGL_MAX_VERTEX_ATTRIBS; ++i) {
        vao->generic[i].size = 4;
        vao->generic[i].type = GL_FLOAT;
        vao->vertex_attrib_binding[i] = i;
        vao->vertex_buffers[i].data.source = MGL_DATA_BUFFER;
        vao->vertex_buffers[i].stride = 16;
    }
    for (i = 0; i < MGL_COMPAT_SEMANTIC_COUNT; ++i) {
        vao->compat[i].size = 4;
        vao->compat[i].type = GL_FLOAT;
    }
    vao->compat[1].size = 3;
    vao->compat[3].size = 3;
    vao->compat[MGL_COMPAT_SEM_FOG].size = 1;
    vao->edge_flag.size = 1;
    vao->edge_flag.type = GL_UNSIGNED_BYTE;
    vao->color_index.size = 1;
    vao->color_index.type = GL_FLOAT;
}

struct mgl_vertex_array *mgl_vao_find(struct mgl_context *ctx, GLuint name) {
    struct mgl_vertex_array *it;
    if (ctx == NULL)
        return NULL;
    if (name == 0u)
        return &ctx->default_vao;
    for (it = ctx->vaos; it != NULL; it = it->next) {
        if (it->name == name)
            return it;
    }
    return NULL;
}

struct mgl_vertex_array *mgl_vao_create_reserved(struct mgl_context *ctx, GLuint name) {
    struct mgl_vertex_array *obj;
    if (ctx == NULL || name == 0u)
        return NULL;
    obj = mgl_vao_find(ctx, name);
    if (obj != NULL)
        return obj;
    obj = calloc(1, sizeof(*obj));
    if (obj == NULL)
        return NULL;
    obj->name = name;
    mgl_vertex_array_init(obj);
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGenVertexArrays != NULL) {
        ctx->native_dispatch->glesGenVertexArrays(1, &obj->native_name);
    }
    obj->next = ctx->vaos;
    ctx->vaos = obj;
    if (ctx->next_vao_name <= name)
        ctx->next_vao_name = name + 1u;
    return obj;
}

void mgl_vao_remove(struct mgl_context *ctx, GLuint name) {
    struct mgl_vertex_array **link;
    if (ctx == NULL || name == 0u)
        return;
    link = &ctx->vaos;
    while (*link != NULL) {
        if ((*link)->name == name) {
            struct mgl_vertex_array *victim = *link;
            *link = victim->next;
            if (ctx->current_vao == victim)
                ctx->current_vao = &ctx->default_vao;
            if (victim->native_name != 0u && ctx->native_dispatch != NULL &&
                ctx->native_dispatch->glesDeleteVertexArrays != NULL) {
                ctx->native_dispatch->glesDeleteVertexArrays(1, &victim->native_name);
            }
            free(victim);
            return;
        }
        link = &(*link)->next;
    }
}

struct mgl_framebuffer *mgl_framebuffer_find(struct mgl_context *ctx, GLuint name) {
    struct mgl_framebuffer *it;
    if (ctx == NULL || name == 0u)
        return NULL;
    for (it = ctx->framebuffers; it != NULL; it = it->next) {
        if (it->name == name)
            return it;
    }
    return NULL;
}

struct mgl_framebuffer *mgl_framebuffer_ensure(struct mgl_context *ctx, GLuint name,
                                               int generate_native) {
    struct mgl_framebuffer *obj;
    unsigned i;
    if (ctx == NULL || name == 0u)
        return NULL;
    obj = mgl_framebuffer_find(ctx, name);
    if (obj != NULL)
        return obj;
    obj = calloc(1, sizeof(*obj));
    if (obj == NULL)
        return NULL;
    obj->name = name;
    obj->draw_buffers[0] = GL_COLOR_ATTACHMENT0;
    for (i = 1u; i < MGL_MAX_DRAW_BUFFERS; ++i)
        obj->draw_buffers[i] = GL_NONE;
    obj->read_buffer = GL_COLOR_ATTACHMENT0;
    if (generate_native && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesGenFramebuffers != NULL) {
        ctx->native_dispatch->glesGenFramebuffers(1, &obj->native_name);
    }
    obj->next = ctx->framebuffers;
    ctx->framebuffers = obj;
    if (ctx->next_framebuffer_name <= name)
        ctx->next_framebuffer_name = name + 1u;
    return obj;
}

void mgl_framebuffer_remove(struct mgl_context *ctx, GLuint name) {
    struct mgl_framebuffer **link;
    if (ctx == NULL || name == 0u)
        return;
    link = &ctx->framebuffers;
    while (*link != NULL) {
        if ((*link)->name == name) {
            struct mgl_framebuffer *victim = *link;
            *link = victim->next;
            if (ctx->draw_framebuffer == name)
                ctx->draw_framebuffer = 0u;
            if (ctx->read_framebuffer == name)
                ctx->read_framebuffer = 0u;
            if (ctx->native_dispatch != NULL &&
                ctx->native_dispatch->glesDeleteFramebuffers != NULL) {
                if (victim->routed_draw_native_name != 0u)
                    ctx->native_dispatch->glesDeleteFramebuffers(1,
                                                                 &victim->routed_draw_native_name);
                if (victim->native_name != 0u)
                    ctx->native_dispatch->glesDeleteFramebuffers(1, &victim->native_name);
            }
            free(victim);
            return;
        }
        link = &(*link)->next;
    }
}

GLuint *mgl_framebuffer_binding_slot(struct mgl_context *ctx, GLenum target) {
    if (ctx == NULL)
        return NULL;
    switch (target) {
        case GL_DRAW_FRAMEBUFFER:
            return &ctx->draw_framebuffer;
        case GL_READ_FRAMEBUFFER:
            return &ctx->read_framebuffer;
        case GL_FRAMEBUFFER:
            return &ctx->draw_framebuffer;
        default:
            return NULL;
    }
}

struct mgl_framebuffer *mgl_bound_framebuffer(struct mgl_context *ctx, GLenum target) {
    GLuint name;
    if (ctx == NULL)
        return NULL;
    switch (target) {
        case GL_FRAMEBUFFER:
        case GL_DRAW_FRAMEBUFFER:
            name = ctx->draw_framebuffer;
            break;
        case GL_READ_FRAMEBUFFER:
            name = ctx->read_framebuffer;
            break;
        default:
            return NULL;
    }
    return mgl_framebuffer_find(ctx, name);
}

struct mgl_renderbuffer *mgl_renderbuffer_find(struct mgl_context *ctx, GLuint name) {
    struct mgl_renderbuffer *it;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return NULL;
    for (it = ctx->share_group->renderbuffers; it != NULL; it = it->next) {
        if (it->name == name)
            return it;
    }
    return NULL;
}

struct mgl_renderbuffer *mgl_renderbuffer_ensure(struct mgl_context *ctx, GLuint name,
                                                 int generate_native) {
    struct mgl_renderbuffer *obj;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return NULL;
    obj = mgl_renderbuffer_find(ctx, name);
    if (obj != NULL)
        return obj;
    obj = calloc(1, sizeof(*obj));
    if (obj == NULL)
        return NULL;
    obj->name = name;
    if (generate_native && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesGenRenderbuffers != NULL) {
        ctx->native_dispatch->glesGenRenderbuffers(1, &obj->native_name);
    }
    obj->next = ctx->share_group->renderbuffers;
    ctx->share_group->renderbuffers = obj;
    advance_atomic_name(&ctx->share_group->next_renderbuffer_name, name);
    return obj;
}

void mgl_renderbuffer_remove(struct mgl_context *ctx, GLuint name) {
    struct mgl_renderbuffer **link;
    if (ctx == NULL || ctx->share_group == NULL || name == 0u)
        return;
    link = &ctx->share_group->renderbuffers;
    while (*link != NULL) {
        if ((*link)->name == name) {
            struct mgl_renderbuffer *victim = *link;
            *link = victim->next;
            if (ctx->renderbuffer == name)
                ctx->renderbuffer = 0u;
            if (victim->native_name != 0u && ctx->native_dispatch != NULL &&
                ctx->native_dispatch->glesDeleteRenderbuffers != NULL) {
                ctx->native_dispatch->glesDeleteRenderbuffers(1, &victim->native_name);
            }
            free(victim);
            return;
        }
        link = &(*link)->next;
    }
}

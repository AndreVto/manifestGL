#include "mgl/gl.h"

#include "core/context.h"
#include "core/render_diag.h"
#include "compat/fixed_function.h"
#include "compat/arb_program.h"
#include "compat/selection.h"
#include "objects/object_store.h"
#include "shader/shader_store.h"
#include "shader/translator.h"
#include "texture/texture_store.h"
#include "list/display_list.h"
#include "vertex/draw_snapshot.h"
#include "vertex/draw_feedback.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct mgl_snapshot_binding {
    GLboolean enabled;
    GLint size;
    GLenum type;
    GLboolean normalized;
    GLboolean integer;
    size_t element_size;
    void *data;
    size_t data_size;
    GLfloat current[4];
    GLint current_i[4];
    GLuint current_ui[4];
    GLenum current_type;
};

struct mgl_draw_snapshot {
    GLenum mode;
    GLsizei count;
    GLboolean indexed;
    uint32_t vertex_count;
    struct mgl_snapshot_binding compat[MGL_COMPAT_SEMANTIC_COUNT];
    struct mgl_snapshot_binding generic[MGL_MAX_VERTEX_ATTRIBS];
    uint32_t *indices;
};

struct mgl_realized_input {
    GLuint location;
    const struct mgl_vertex_binding *binding;
    const GLfloat *current;
    const GLint *current_i;
    const GLuint *current_ui;
    GLenum current_type;
    GLenum shader_type;
    size_t packed_offset;
    size_t element_size;
    size_t source_element_size;
    GLenum realized_type;
    /* Valid only within the same draw after native_inputs_ready succeeds. */
    struct mgl_buffer *native_buffer;
};

static int valid_draw_mode(GLenum mode) {
    switch (mode) {
        case GL_POINTS:
        case GL_LINES:
        case GL_LINE_LOOP:
        case GL_LINE_STRIP:
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
        case GL_PATCHES:
            return 1;
        default:
            return 0;
    }
}

static int legacy_draw_mode(GLenum mode) {
    return mode == GL_QUADS || mode == GL_QUAD_STRIP || mode == GL_POLYGON;
}

static int valid_compat_draw_mode(GLenum mode) {
    return valid_draw_mode(mode) || legacy_draw_mode(mode);
}

static uint32_t *triangulate_legacy_indices(GLenum mode, const uint32_t *source, size_t count,
                                            GLsizei *out_count) {
    size_t triangles;
    size_t i;
    size_t next = 0u;
    uint32_t *indices;
    if (mode == GL_QUADS)
        triangles = (count / 4u) * 2u;
    else if (mode == GL_QUAD_STRIP)
        triangles = count >= 4u ? (count / 2u - 1u) * 2u : 0u;
    else
        triangles = count >= 3u ? count - 2u : 0u;
    if (triangles == 0u) {
        *out_count = 0;
        return NULL;
    }
    if (triangles > (size_t)INT32_MAX / 3u || triangles > SIZE_MAX / (3u * sizeof(*indices))) {
        *out_count = -1;
        return NULL;
    }
    indices = malloc(triangles * 3u * sizeof(*indices));
    if (indices == NULL) {
        *out_count = -1;
        return NULL;
    }
#define AT(n) (source != NULL ? source[(n)] : (uint32_t)(n))
    if (mode == GL_QUADS) {
        for (i = 0u; i + 3u < count; i += 4u) {
            indices[next++] = AT(i);
            indices[next++] = AT(i + 1u);
            indices[next++] = AT(i + 2u);
            indices[next++] = AT(i);
            indices[next++] = AT(i + 2u);
            indices[next++] = AT(i + 3u);
        }
    } else if (mode == GL_QUAD_STRIP) {
        for (i = 0u; i + 3u < count; i += 2u) {
            indices[next++] = AT(i);
            indices[next++] = AT(i + 1u);
            indices[next++] = AT(i + 3u);
            indices[next++] = AT(i);
            indices[next++] = AT(i + 3u);
            indices[next++] = AT(i + 2u);
        }
    } else {
        for (i = 1u; i + 1u < count; ++i) {
            indices[next++] = AT(0u);
            indices[next++] = AT(i);
            indices[next++] = AT(i + 1u);
        }
    }
#undef AT
    *out_count = (GLsizei)next;
    return indices;
}

static size_t scalar_size(GLenum type) {
    switch (type) {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return 1u;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_HALF_FLOAT:
            return 2u;
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_FLOAT:
        case GL_FIXED:
        case GL_INT_2_10_10_10_REV:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            return 4u;
        case GL_DOUBLE:
            return 8u;
        default:
            return 0u;
    }
}

static size_t array_element_size(GLenum type, GLint size) {
    if (type == GL_INT_2_10_10_10_REV || type == GL_UNSIGNED_INT_2_10_10_10_REV)
        return sizeof(GLuint);
    return (size_t)size * scalar_size(type);
}

static int checked_mul_size(size_t a, size_t b, size_t *out) {
    if (a != 0u && b > SIZE_MAX / a)
        return 0;
    if (out != NULL)
        *out = a * b;
    return 1;
}

static int checked_add_size(size_t a, size_t b, size_t *out) {
    if (b > SIZE_MAX - a)
        return 0;
    if (out != NULL)
        *out = a + b;
    return 1;
}

static int buffer_mapping_disallowed(const struct mgl_buffer *buffer) {
    return buffer != NULL && buffer->mapped &&
           (buffer->map_access & GL_MAP_PERSISTENT_BIT) == 0u;
}

static int buffer_range(const struct mgl_buffer *buffer, size_t offset, size_t bytes) {
    return buffer != NULL && !buffer_mapping_disallowed(buffer) && offset <= buffer->size &&
           bytes <= buffer->size - offset;
}

static const unsigned char *binding_element_ptr(struct mgl_context *ctx,
                                                const struct mgl_vertex_binding *binding,
                                                uint32_t vertex, size_t element_size) {
    size_t stride =
        binding->stride != 0 || binding->separate_stride ? (size_t)binding->stride : element_size;
    size_t vertex_offset;
    size_t absolute_offset;
    if (!checked_mul_size((size_t)vertex, stride, &vertex_offset))
        return NULL;

    if (binding->data.source == MGL_DATA_CLIENT) {
        if (binding->data.client_ptr == NULL)
            return NULL;
        return (const unsigned char *)binding->data.client_ptr + vertex_offset;
    }

    if (!checked_add_size(binding->data.offset, vertex_offset, &absolute_offset))
        return NULL;
    {
        struct mgl_buffer *buffer = mgl_buffer_find(ctx, binding->data.buffer_name);
        if (!buffer_range(buffer, absolute_offset, element_size))
            return NULL;
        return mgl_buffer_shadow_ensure(ctx, buffer) ? buffer->shadow + absolute_offset : NULL;
    }
}

static int capture_snapshot_binding(struct mgl_context *ctx,
                                    const struct mgl_vertex_binding *binding,
                                    const GLfloat current[4], const GLint current_i[4],
                                    const GLuint current_ui[4], GLenum current_type,
                                    uint32_t first_vertex, uint32_t vertex_count,
                                    struct mgl_snapshot_binding *out) {
    size_t scalar;
    size_t element_size;
    size_t bytes;
    uint32_t n;
    memset(out, 0, sizeof(*out));
    out->enabled = binding->enabled;
    out->size = binding->size;
    out->type = binding->type;
    out->normalized = binding->normalized;
    out->integer = binding->integer;
    out->current_type = current_type != 0u ? current_type : GL_FLOAT;
    memcpy(out->current, current, sizeof(out->current));
    if (current_i != NULL)
        memcpy(out->current_i, current_i, sizeof(out->current_i));
    if (current_ui != NULL)
        memcpy(out->current_ui, current_ui, sizeof(out->current_ui));
    if (!binding->enabled)
        return 1;
    scalar = scalar_size(binding->type);
    if (scalar == 0u || binding->size < 1 || binding->size > 4)
        return 0;
    element_size = array_element_size(binding->type, binding->size);
    if (!checked_mul_size((size_t)vertex_count, element_size, &bytes))
        return 0;
    out->element_size = element_size;
    out->data_size = bytes;
    if (bytes == 0u)
        return 1;
    out->data = malloc(bytes);
    if (out->data == NULL)
        return 0;
    for (n = 0u; n < vertex_count; ++n) {
        const unsigned char *src =
            binding_element_ptr(ctx, binding, first_vertex + n, element_size);
        if (src == NULL)
            return 0;
        memcpy((unsigned char *)out->data + (size_t)n * element_size, src, element_size);
    }
    return 1;
}

static int capture_snapshot_bindings(struct mgl_context *ctx, uint32_t first_vertex,
                                     uint32_t vertex_count, struct mgl_draw_snapshot *snapshot) {
    unsigned i;
    for (i = 0u; i < MGL_COMPAT_SEMANTIC_COUNT; ++i) {
        if (!capture_snapshot_binding(ctx, &ctx->current_vao->compat[i], ctx->compat_current[i],
                                      NULL, NULL, GL_FLOAT, first_vertex, vertex_count,
                                      &snapshot->compat[i]))
            return 0;
    }
    for (i = 0u; i < MGL_MAX_VERTEX_ATTRIBS; ++i) {
        if (!capture_snapshot_binding(ctx, &ctx->current_vao->generic[i], ctx->generic_current[i],
                                      ctx->generic_current_i[i], ctx->generic_current_ui[i],
                                      ctx->generic_current_type[i], first_vertex, vertex_count,
                                      &snapshot->generic[i]))
            return 0;
    }
    return 1;
}

void mgl_draw_snapshot_destroy(struct mgl_draw_snapshot *snapshot) {
    unsigned i;
    if (snapshot == NULL)
        return;
    for (i = 0u; i < MGL_COMPAT_SEMANTIC_COUNT; ++i)
        free(snapshot->compat[i].data);
    for (i = 0u; i < MGL_MAX_VERTEX_ATTRIBS; ++i)
        free(snapshot->generic[i].data);
    free(snapshot->indices);
    free(snapshot);
}

struct mgl_draw_snapshot *mgl_draw_snapshot_capture_arrays(struct mgl_context *ctx, GLenum mode,
                                                           GLint first, GLsizei count) {
    struct mgl_draw_snapshot *snapshot;
    if (ctx == NULL || ctx->current_vao == NULL || first < 0 || count < 0)
        return NULL;
    snapshot = calloc(1, sizeof(*snapshot));
    if (snapshot == NULL)
        return NULL;
    snapshot->mode = mode;
    snapshot->count = count;
    snapshot->vertex_count = (uint32_t)count;
    if (!capture_snapshot_bindings(ctx, (uint32_t)first, (uint32_t)count, snapshot)) {
        mgl_draw_snapshot_destroy(snapshot);
        return NULL;
    }
    return snapshot;
}

static void install_snapshot_binding(struct mgl_vertex_binding *dst,
                                     const struct mgl_snapshot_binding *src) {
    memset(dst, 0, sizeof(*dst));
    dst->enabled = src->enabled;
    dst->size = src->size;
    dst->type = src->type;
    dst->normalized = src->normalized;
    dst->integer = src->integer;
    if (src->enabled) {
        dst->stride = (GLsizei)src->element_size;
        dst->data.source = MGL_DATA_CLIENT;
        dst->data.client_ptr = src->data;
    }
}

void mgl_draw_snapshot_replay(struct mgl_context *ctx, const struct mgl_draw_snapshot *snapshot) {
    struct mgl_vertex_binding saved_compat[MGL_COMPAT_SEMANTIC_COUNT];
    struct mgl_vertex_binding saved_generic[MGL_MAX_VERTEX_ATTRIBS];
    GLfloat saved_compat_current[MGL_COMPAT_SEMANTIC_COUNT][4];
    GLfloat saved_generic_current[MGL_MAX_VERTEX_ATTRIBS][4];
    GLint saved_generic_current_i[MGL_MAX_VERTEX_ATTRIBS][4];
    GLuint saved_generic_current_ui[MGL_MAX_VERTEX_ATTRIBS][4];
    GLenum saved_generic_current_type[MGL_MAX_VERTEX_ATTRIBS];
    GLuint saved_element;
    unsigned i;
    if (ctx == NULL || snapshot == NULL || ctx->current_vao == NULL)
        return;
    memcpy(saved_compat, ctx->current_vao->compat, sizeof(saved_compat));
    memcpy(saved_generic, ctx->current_vao->generic, sizeof(saved_generic));
    memcpy(saved_compat_current, ctx->compat_current, sizeof(saved_compat_current));
    memcpy(saved_generic_current, ctx->generic_current, sizeof(saved_generic_current));
    memcpy(saved_generic_current_i, ctx->generic_current_i, sizeof(saved_generic_current_i));
    memcpy(saved_generic_current_ui, ctx->generic_current_ui, sizeof(saved_generic_current_ui));
    memcpy(saved_generic_current_type, ctx->generic_current_type,
           sizeof(saved_generic_current_type));
    saved_element = ctx->current_vao->element_array_buffer;
    for (i = 0u; i < MGL_COMPAT_SEMANTIC_COUNT; ++i) {
        install_snapshot_binding(&ctx->current_vao->compat[i], &snapshot->compat[i]);
        memcpy(ctx->compat_current[i], snapshot->compat[i].current,
               sizeof(snapshot->compat[i].current));
    }
    for (i = 0u; i < MGL_MAX_VERTEX_ATTRIBS; ++i) {
        install_snapshot_binding(&ctx->current_vao->generic[i], &snapshot->generic[i]);
        memcpy(ctx->generic_current[i], snapshot->generic[i].current,
               sizeof(snapshot->generic[i].current));
        memcpy(ctx->generic_current_i[i], snapshot->generic[i].current_i,
               sizeof(snapshot->generic[i].current_i));
        memcpy(ctx->generic_current_ui[i], snapshot->generic[i].current_ui,
               sizeof(snapshot->generic[i].current_ui));
        ctx->generic_current_type[i] = snapshot->generic[i].current_type;
    }
    ctx->current_vao->element_array_buffer = 0u;
    if (snapshot->indexed) {
        glDrawElements(snapshot->mode, snapshot->count, GL_UNSIGNED_INT, snapshot->indices);
        mgl_restore_element_array_binding(ctx, saved_element);
    } else {
        glDrawArrays(snapshot->mode, 0, snapshot->count);
        if (legacy_draw_mode(snapshot->mode))
            mgl_restore_element_array_binding(ctx, saved_element);
        else
            ctx->current_vao->element_array_buffer = saved_element;
    }
    memcpy(ctx->current_vao->compat, saved_compat, sizeof(saved_compat));
    memcpy(ctx->current_vao->generic, saved_generic, sizeof(saved_generic));
    memcpy(ctx->compat_current, saved_compat_current, sizeof(saved_compat_current));
    memcpy(ctx->generic_current, saved_generic_current, sizeof(saved_generic_current));
    memcpy(ctx->generic_current_i, saved_generic_current_i, sizeof(saved_generic_current_i));
    memcpy(ctx->generic_current_ui, saved_generic_current_ui, sizeof(saved_generic_current_ui));
    memcpy(ctx->generic_current_type, saved_generic_current_type,
           sizeof(saved_generic_current_type));
}

static int ensure_scratch_buffers(struct mgl_context *ctx, int need_elements) {
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGenBuffers == NULL)
        return 0;
    if (ctx->scratch_array_buffer_native == 0u) {
        ctx->native_dispatch->glesGenBuffers(1, &ctx->scratch_array_buffer_native);
        if (ctx->scratch_array_buffer_native == 0u)
            return 0;
    }
    if (need_elements && ctx->scratch_element_buffer_native == 0u) {
        ctx->native_dispatch->glesGenBuffers(1, &ctx->scratch_element_buffer_native);
        if (ctx->scratch_element_buffer_native == 0u)
            return 0;
    }
    return 1;
}

static GLuint native_buffer_name(struct mgl_context *ctx, GLuint name) {
    struct mgl_buffer *buffer;
    if (name == 0u)
        return 0u;
    buffer = mgl_buffer_find(ctx, name);
    return buffer != NULL ? buffer->native_name : 0u;
}

static size_t collect_inputs(struct mgl_context *ctx, const struct mgl_program *program,
                             struct mgl_realized_input *out, size_t capacity) {
    uint32_t claimed = 0u;
    size_t count = 0u;
    unsigned semantic;
    unsigned location;
    unsigned max_attribs = ctx->native_max_vertex_attribs > 0
                               ? (unsigned)ctx->native_max_vertex_attribs
                               : MGL_MAX_VERTEX_ATTRIBS;
    if (max_attribs > MGL_MAX_VERTEX_ATTRIBS)
        max_attribs = MGL_MAX_VERTEX_ATTRIBS;

    for (semantic = 0; semantic < MGL_COMPAT_SEMANTIC_COUNT; ++semantic) {
        GLint loc = program->compat_attrib[semantic];
        if (loc < 0 || (unsigned)loc >= max_attribs || (unsigned)loc >= 32u)
            continue;
        claimed |= UINT32_C(1) << (unsigned)loc;
        if (count < capacity) {
            out[count].location = (GLuint)loc;
            out[count].binding = &ctx->current_vao->compat[semantic];
            out[count].current = ctx->compat_current[semantic];
            out[count].current_i = NULL;
            out[count].current_ui = NULL;
            out[count].current_type = GL_FLOAT;
            out[count].shader_type = GL_FLOAT;
            out[count].packed_offset = 0u;
            out[count].element_size = 0u;
            out[count].source_element_size = 0u;
            out[count].realized_type = out[count].binding->type;
            out[count].native_buffer = NULL;
        }
        ++count;
    }

    for (location = 0; location < max_attribs && location < 32u; ++location) {
        uint32_t bit = UINT32_C(1) << location;
        if ((program->active_generic_mask & bit) == 0u || (claimed & bit) != 0u)
            continue;
        if (count < capacity) {
            out[count].location = location;
            out[count].binding = &ctx->current_vao->generic[location];
            out[count].current = ctx->generic_current[location];
            out[count].current_i = ctx->generic_current_i[location];
            out[count].current_ui = ctx->generic_current_ui[location];
            out[count].current_type = ctx->generic_current_type[location] != 0u
                                          ? ctx->generic_current_type[location]
                                          : GL_FLOAT;
            out[count].shader_type = program->active_generic_type[location] != 0u
                                         ? program->active_generic_type[location]
                                         : GL_FLOAT;
            out[count].packed_offset = 0u;
            out[count].element_size = 0u;
            out[count].source_element_size = 0u;
            out[count].realized_type = out[count].binding->type;
            out[count].native_buffer = NULL;
        }
        ++count;
    }
    return count;
}

static GLfloat draw_half_to_float(GLushort h) {
    const GLuint sign = ((GLuint)h & 0x8000u) << 16;
    GLuint exp = ((GLuint)h >> 10) & 0x1fu;
    GLuint mant = (GLuint)h & 0x03ffu;
    GLuint bits;
    GLfloat out;
    if (exp == 0u) {
        if (mant == 0u)
            bits = sign;
        else {
            GLint e = -14;
            while ((mant & 0x0400u) == 0u) {
                mant <<= 1;
                --e;
            }
            mant &= 0x03ffu;
            bits = sign | ((GLuint)(e + 127) << 23) | (mant << 13);
        }
    } else if (exp == 0x1fu)
        bits = sign | 0x7f800000u | (mant << 13);
    else
        bits = sign | ((exp + 112u) << 23) | (mant << 13);
    memcpy(&out, &bits, sizeof(out));
    return out;
}

static void convert_float_array_element(unsigned char *dst, const unsigned char *src, GLenum type,
                                        GLint components) {
    GLint c;
    for (c = 0; c < components; ++c) {
        GLfloat value;
        if (type == GL_HALF_FLOAT) {
            GLushort h;
            memcpy(&h, src + (size_t)c * sizeof(h), sizeof(h));
            value = draw_half_to_float(h);
        } else {
            GLdouble d;
            memcpy(&d, src + (size_t)c * sizeof(d), sizeof(d));
            value = (GLfloat)d;
        }
        memcpy(dst + (size_t)c * sizeof(value), &value, sizeof(value));
    }
}

/* Cache staging buffers up to 4 MiB per context; large draws use temporary allocations. */
#define MGL_SCRATCH_CPU_CACHE_LIMIT (4u * 1024u * 1024u)

static unsigned char *reserve_packed_storage(struct mgl_context *ctx, size_t bytes) {
    unsigned char *next;
    size_t cap;
    if (bytes == 0u)
        return NULL;
    if (bytes > MGL_SCRATCH_CPU_CACHE_LIMIT)
        return malloc(bytes);
    if (bytes <= ctx->scratch_cpu_capacity)
        return ctx->scratch_cpu_storage;
    cap = ctx->scratch_cpu_capacity != 0u ? ctx->scratch_cpu_capacity : 4096u;
    while (cap < bytes)
        cap *= 2u;
    next = realloc(ctx->scratch_cpu_storage, cap);
    if (next == NULL)
        return NULL;
    ctx->scratch_cpu_storage = next;
    ctx->scratch_cpu_capacity = cap;
    return next;
}

static void release_packed_storage(struct mgl_context *ctx, void *storage, int borrowed) {
    if (!borrowed && storage != ctx->scratch_cpu_storage)
        free(storage);
}

static int pack_inputs_ex(struct mgl_context *ctx, struct mgl_realized_input *inputs,
                          size_t input_count, uint32_t first_vertex, uint32_t vertex_count,
                          uint32_t instance_count, uint32_t base_instance,
                          unsigned char **out_storage, size_t *out_size, int *out_borrowed) {
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_DRAW_PACK);
    size_t total = 0u;
    size_t i;
    unsigned enabled_count = 0u;
    size_t only_enabled = 0u;
    unsigned char *storage;
    *out_borrowed = 0;

    for (i = 0u; i < input_count; ++i) {
        const struct mgl_vertex_binding *binding = inputs[i].binding;
        size_t scalar;
        size_t realized_scalar;
        size_t element_size;
        size_t source_element_size;
        size_t elements;
        size_t bytes;
        if (!binding->enabled)
            continue;
        ++enabled_count;
        only_enabled = i;
        scalar = scalar_size(binding->type);
        if (scalar == 0u || binding->size < 1 || binding->size > 4)
            return 0;
        inputs[i].realized_type = (binding->type == GL_HALF_FLOAT || binding->type == GL_DOUBLE)
                                      ? GL_FLOAT
                                      : binding->type;
        realized_scalar = scalar_size(inputs[i].realized_type);
        if (realized_scalar == 0u)
            return 0;
        if (binding->divisor != 0u) {
            elements = instance_count == 0u
                           ? 0u
                           : ((size_t)instance_count + (size_t)binding->divisor - 1u) /
                                 (size_t)binding->divisor;
            if (elements != 0u && (size_t)base_instance > (size_t)UINT32_MAX - (elements - 1u))
                return 0;
        } else {
            elements = (size_t)vertex_count;
        }
        source_element_size = array_element_size(binding->type, binding->size);
        element_size = array_element_size(inputs[i].realized_type, binding->size);
        if (!checked_mul_size(elements, element_size, &bytes))
            return 0;
        inputs[i].source_element_size = source_element_size;
        inputs[i].element_size = element_size;
        inputs[i].packed_offset = total;
        if (!checked_add_size(total, bytes, &total))
            return 0;
    }

    /* A single tightly packed, unconverted attribute can be uploaded straight
     * from its client pointer (or buffer shadow). The source must remain live
     * through the synchronous GLES BufferData in realize_inputs(). Strided,
     * converted and instanced attributes continue through the packer. */
    if (enabled_count == 1u && vertex_count != 0u) {
        const struct mgl_vertex_binding *b = inputs[only_enabled].binding;
        size_t start, absolute;
        const unsigned char *source = NULL;
        if (b->divisor == 0u && b->type == inputs[only_enabled].realized_type &&
            (!(b->separate_stride && b->stride == 0) &&
             (b->stride == 0 || (size_t)b->stride == inputs[only_enabled].source_element_size)) &&
            checked_mul_size((size_t)first_vertex, inputs[only_enabled].source_element_size,
                             &start)) {
            if (b->data.source == MGL_DATA_CLIENT) {
                if (b->data.client_ptr != NULL)
                    source = (const unsigned char *)b->data.client_ptr + start;
            } else if (checked_add_size(b->data.offset, start, &absolute)) {
                struct mgl_buffer *buffer = mgl_buffer_find(ctx, b->data.buffer_name);
                if (buffer != NULL && buffer->shadow != NULL &&
                    buffer_range(buffer, absolute, total))
                    source =
                        mgl_buffer_shadow_ensure(ctx, buffer) ? buffer->shadow + absolute : NULL;
            }
        }
        if (source != NULL) {
            *out_storage = (unsigned char *)(uintptr_t)source;
            *out_size = total;
            *out_borrowed = 1;
            mgl_perf_diag_direct_upload(ctx);
            return 1;
        }
    }

    storage = reserve_packed_storage(ctx, total);
    if (total != 0u && storage == NULL)
        return 0;

    for (i = 0u; i < input_count; ++i) {
        const struct mgl_vertex_binding *binding = inputs[i].binding;
        const unsigned char *source;
        size_t elements;
        size_t stride;
        size_t first;
        size_t start_offset;
        size_t last_offset;
        size_t step;
        size_t n;
        unsigned char *dest;
        if (!binding->enabled)
            continue;
        if (binding->divisor != 0u) {
            elements = instance_count == 0u
                           ? 0u
                           : ((size_t)instance_count + (size_t)binding->divisor - 1u) /
                                 (size_t)binding->divisor;
            first = (size_t)base_instance;
        } else {
            elements = (size_t)vertex_count;
            first = (size_t)first_vertex;
        }
        if (elements == 0u)
            continue;
        stride = binding->stride != 0 || binding->separate_stride ? (size_t)binding->stride
                                                                  : inputs[i].source_element_size;
        if (!checked_mul_size(first, stride, &start_offset) ||
            !checked_mul_size(elements - 1u, stride, &step) ||
            !checked_add_size(start_offset, step, &last_offset)) {
            release_packed_storage(ctx, storage, 0);
            return 0;
        }
        if (binding->data.source == MGL_DATA_CLIENT) {
            if (binding->data.client_ptr == NULL) {
                release_packed_storage(ctx, storage, 0);
                return 0;
            }
            source = (const unsigned char *)binding->data.client_ptr + start_offset;
        } else {
            struct mgl_buffer *buffer = mgl_buffer_find(ctx, binding->data.buffer_name);
            size_t absolute_first;
            size_t absolute_last;
            if (!checked_add_size(binding->data.offset, start_offset, &absolute_first) ||
                !checked_add_size(binding->data.offset, last_offset, &absolute_last) ||
                !buffer_range(buffer, absolute_last, inputs[i].source_element_size) ||
                !mgl_buffer_shadow_ensure(ctx, buffer)) {
                release_packed_storage(ctx, storage, 0);
                return 0;
            }
            source = buffer->shadow + absolute_first;
        }
        dest = storage + inputs[i].packed_offset;
        if (binding->type != GL_HALF_FLOAT && binding->type != GL_DOUBLE) {
            if (stride == inputs[i].element_size) {
                memcpy(dest, source, elements * inputs[i].element_size);
            } else {
                for (n = 0u; n < elements; ++n)
                    memcpy(dest + n * inputs[i].element_size, source + n * stride,
                           inputs[i].element_size);
            }
        } else {
            for (n = 0u; n < elements; ++n)
                convert_float_array_element(dest + n * inputs[i].element_size, source + n * stride,
                                            binding->type, binding->size);
        }
    }

    *out_storage = storage;
    *out_size = total;
    return 1;
}

static int pack_inputs(struct mgl_context *ctx, struct mgl_realized_input *inputs,
                       size_t input_count, uint32_t first_vertex, uint32_t vertex_count,
                       unsigned char **out_storage, size_t *out_size, int *out_borrowed) {
    return pack_inputs_ex(ctx, inputs, input_count, first_vertex, vertex_count, 1u, 0u, out_storage,
                          out_size, out_borrowed);
}

static int shader_type_signed_integer(GLenum type) {
    return type == GL_INT || type == GL_INT_VEC2 || type == GL_INT_VEC3 || type == GL_INT_VEC4;
}

static int shader_type_unsigned_integer(GLenum type) {
    return type == GL_UNSIGNED_INT || type == GL_UNSIGNED_INT_VEC2 ||
           type == GL_UNSIGNED_INT_VEC3 || type == GL_UNSIGNED_INT_VEC4;
}

static int validate_input_realization(struct mgl_context *ctx,
                                      const struct mgl_realized_input *inputs, size_t input_count) {
    size_t i;
    for (i = 0u; i < input_count; ++i) {
        const struct mgl_vertex_binding *binding = inputs[i].binding;
        if (binding->enabled && binding->integer &&
            ctx->native_dispatch->glesVertexAttribIPointer == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        if (!binding->enabled && shader_type_signed_integer(inputs[i].shader_type) &&
            ctx->native_dispatch->glesVertexAttribI4iv == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        if (!binding->enabled && shader_type_unsigned_integer(inputs[i].shader_type) &&
            ctx->native_dispatch->glesVertexAttribI4uiv == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
    }
    return 1;
}

static void current_as_signed(const struct mgl_realized_input *input, GLint out[4]) {
    unsigned c;
    for (c = 0u; c < 4u; ++c) {
        if (input->current_type == GL_INT)
            out[c] = input->current_i[c];
        else if (input->current_type == GL_UNSIGNED_INT)
            out[c] = (GLint)input->current_ui[c];
        else
            out[c] = (GLint)input->current[c];
    }
}

static void current_as_unsigned(const struct mgl_realized_input *input, GLuint out[4]) {
    unsigned c;
    for (c = 0u; c < 4u; ++c) {
        if (input->current_type == GL_UNSIGNED_INT)
            out[c] = input->current_ui[c];
        else if (input->current_type == GL_INT)
            out[c] = (GLuint)input->current_i[c];
        else
            out[c] = (GLuint)input->current[c];
    }
}

/* Generic attributes are context state. Cache exact native value bits
 * across program and VAO switches, including signed zeros, NaNs, and integers. */
static int realize_constant_input(struct mgl_context *ctx, const struct mgl_realized_input *input) {
    uint32_t bits[4];
    unsigned char kind;
    const uint32_t mask = UINT32_C(1) << input->location;
    if (shader_type_signed_integer(input->shader_type)) {
        GLint values[4];
        current_as_signed(input, values);
        memcpy(bits, values, sizeof(bits));
        kind = 2u;
    } else if (shader_type_unsigned_integer(input->shader_type)) {
        GLuint values[4];
        current_as_unsigned(input, values);
        memcpy(bits, values, sizeof(bits));
        kind = 3u;
    } else {
        memcpy(bits, input->current, sizeof(bits));
        kind = 1u;
    }
    if ((ctx->draw_constant_valid & mask) != 0u &&
        ctx->draw_constant[input->location].kind == kind &&
        memcmp(ctx->draw_constant[input->location].bits, bits, sizeof(bits)) == 0)
        return 0;
    if (kind == 2u) {
        GLint values[4];
        memcpy(values, bits, sizeof(values));
        ctx->native_dispatch->glesVertexAttribI4iv(input->location, values);
    } else if (kind == 3u) {
        GLuint values[4];
        memcpy(values, bits, sizeof(values));
        ctx->native_dispatch->glesVertexAttribI4uiv(input->location, values);
    } else {
        ctx->native_dispatch->glesVertexAttrib4fv(input->location, input->current);
    }
    memcpy(ctx->draw_constant[input->location].bits, bits, sizeof(bits));
    ctx->draw_constant[input->location].kind = kind;
    ctx->draw_constant_valid |= mask;
    return 1;
}

/* glBufferData may orphan storage without invalidating a VAO attribute pointer, which retains the
 * buffer name and offset. */
static int scratch_pointer_matches(const struct mgl_context *ctx, GLuint location,
                                   const struct mgl_vertex_binding *binding, GLenum type,
                                   GLsizei stride, size_t offset) {
    const uint32_t bit = UINT32_C(1) << location;
    if ((ctx->draw_native_pointer_valid & bit) == 0u)
        return 0;
    return ctx->draw_native_pointer[location].buffer == ctx->scratch_array_buffer_native &&
           ctx->draw_native_pointer[location].size == binding->size &&
           ctx->draw_native_pointer[location].type == type &&
           ctx->draw_native_pointer[location].normalized == binding->normalized &&
           ctx->draw_native_pointer[location].integer == binding->integer &&
           ctx->draw_native_pointer[location].stride == stride &&
           ctx->draw_native_pointer[location].offset == offset &&
           ctx->draw_native_pointer[location].divisor == binding->divisor;
}

static void scratch_pointer_remember(struct mgl_context *ctx, GLuint location,
                                     const struct mgl_vertex_binding *binding, GLenum type,
                                     GLsizei stride, size_t offset) {
    ctx->draw_native_pointer[location].buffer = ctx->scratch_array_buffer_native;
    ctx->draw_native_pointer[location].size = binding->size;
    ctx->draw_native_pointer[location].type = type;
    ctx->draw_native_pointer[location].normalized = binding->normalized;
    ctx->draw_native_pointer[location].integer = binding->integer;
    ctx->draw_native_pointer[location].stride = stride;
    ctx->draw_native_pointer[location].offset = offset;
    ctx->draw_native_pointer[location].divisor = binding->divisor;
    ctx->draw_native_pointer_valid |= UINT32_C(1) << location;
}

static void realize_inputs(struct mgl_context *ctx, struct mgl_realized_input *inputs,
                           size_t input_count, const unsigned char *storage, size_t storage_size) {
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_DRAW_UPLOAD);
    unsigned max_attribs = ctx->native_max_vertex_attribs > 0
                               ? (unsigned)ctx->native_max_vertex_attribs
                               : MGL_MAX_VERTEX_ATTRIBS;
    size_t i;
    unsigned location;
    uint32_t constant_sets = 0u, constant_skips = 0u;
    uint32_t pointer_sets = 0u, pointer_skips = 0u;
    uint32_t divisor_sets = 0u, divisor_skips = 0u;
    uint32_t desired_mask = 0u;
    if (max_attribs > MGL_MAX_VERTEX_ATTRIBS)
        max_attribs = MGL_MAX_VERTEX_ATTRIBS;

    /* Track enabled bits separately from attribute pointer descriptors.
     * Replacing scratch-buffer storage does not invalidate GLES VAO pointers. */
    if (ctx->draw_cached_vao != ctx->current_vao) {
        ctx->draw_cached_vao = ctx->current_vao;
        ctx->draw_enabled_mask_valid = GL_FALSE;
        ctx->draw_native_pointer_valid = 0u;
    }
    for (i = 0u; i < input_count; ++i)
        if (inputs[i].binding->enabled)
            desired_mask |= UINT32_C(1) << inputs[i].location;
    for (location = 0u; location < max_attribs; ++location) {
        uint32_t bit = UINT32_C(1) << location;
        if (!ctx->draw_enabled_mask_valid ||
            ((ctx->draw_enabled_mask ^ desired_mask) & bit) != 0u) {
            if ((desired_mask & bit) != 0u)
                ctx->native_dispatch->glesEnableVertexAttribArray(location);
            else
                ctx->native_dispatch->glesDisableVertexAttribArray(location);
        }
    }
    ctx->draw_enabled_mask = desired_mask;
    ctx->draw_enabled_mask_valid = GL_TRUE;

    if (storage_size != 0u || desired_mask != 0u) {
        mgl_native_dispatch_bind_buffer(ctx, GL_ARRAY_BUFFER, ctx->scratch_array_buffer_native);
        if (storage_size != 0u) {
            ctx->native_dispatch->glesBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)storage_size, storage,
                                                 GL_STREAM_DRAW);
        }
    }

    for (i = 0u; i < input_count; ++i) {
        const struct mgl_vertex_binding *binding = inputs[i].binding;
        if (binding->enabled) {
            GLsizei stride = (GLsizei)inputs[i].element_size;
            size_t offset = inputs[i].packed_offset;
            GLuint attribute_location = inputs[i].location;
            if (!scratch_pointer_matches(ctx, attribute_location, binding, inputs[i].realized_type,
                                         stride, offset)) {
                if (binding->integer) {
                    ctx->native_dispatch->glesVertexAttribIPointer(
                        attribute_location, binding->size, inputs[i].realized_type, stride,
                        (const void *)(uintptr_t)offset);
                } else {
                    ctx->native_dispatch->glesVertexAttribPointer(
                        attribute_location, binding->size, inputs[i].realized_type,
                        binding->normalized, stride, (const void *)(uintptr_t)offset);
                }
                /* Pointer changes do not reset GLES attribute divisors. */
                if (ctx->native_dispatch->glesVertexAttribDivisor != NULL) {
                    if ((ctx->draw_native_pointer_valid & (UINT32_C(1) << attribute_location)) ==
                            0u ||
                        ctx->draw_native_pointer[attribute_location].divisor != binding->divisor) {
                        ctx->native_dispatch->glesVertexAttribDivisor(attribute_location,
                                                                      binding->divisor);
                        ++divisor_sets;
                    } else
                        ++divisor_skips;
                }
                scratch_pointer_remember(ctx, attribute_location, binding, inputs[i].realized_type,
                                         stride, offset);
                ++pointer_sets;
            } else {
                ++pointer_skips;
            }
        } else {
            if (realize_constant_input(ctx, &inputs[i]))
                ++constant_sets;
            else
                ++constant_skips;
        }
    }
    mgl_perf_diag_attrib(ctx, pointer_sets, pointer_skips, constant_sets, constant_skips,
                         divisor_sets, divisor_skips);
}

static int native_inputs_ready(struct mgl_context *ctx, struct mgl_realized_input *inputs,
                               size_t count, uint32_t first, uint32_t vertices, int indexed) {
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_DRAW_NATIVE_CHECK);
    size_t i;
    for (i = 0u; i < count; ++i) {
        const struct mgl_vertex_binding *b = inputs[i].binding;
        struct mgl_buffer *buffer;
        size_t element_size, stride, final_offset, step;
        if (!b->enabled)
            continue;
        if ((b->data.source != MGL_DATA_BUFFER &&
             !(ctx->angle_client_arrays && b->data.source == MGL_DATA_CLIENT &&
               b->data.client_ptr != NULL)) ||
            b->type == GL_DOUBLE || (b->integer && b->type == GL_HALF_FLOAT) ||
            (b->divisor != 0u && (ctx->native_dispatch == NULL ||
                                  ctx->native_dispatch->glesVertexAttribDivisor == NULL)) ||
            b->size < 1 || b->size > 4 || scalar_size(b->type) == 0u)
            return 0;
        buffer =
            b->data.source == MGL_DATA_BUFFER ? mgl_buffer_find(ctx, b->data.buffer_name) : NULL;
        if (b->data.source == MGL_DATA_BUFFER &&
            (buffer == NULL || buffer->native_name == 0u || buffer_mapping_disallowed(buffer)))
            return 0;
        element_size = array_element_size(b->type, b->size);
        if (b->separate_stride && b->stride == 0 && b->data.source != MGL_DATA_BUFFER)
            return 0;
        stride = b->stride != 0 || b->separate_stride ? (size_t)b->stride : element_size;
        /* Indexed-buffer fetches use the GPU indices directly. Index values
         * outside an attribute buffer are undefined in desktop OpenGL too. */
        if (b->data.source == MGL_DATA_BUFFER) {
            if (indexed || b->divisor != 0u) {
                if (!buffer_range(buffer, b->data.offset, element_size))
                    return 0;
            } else {
                if (!checked_mul_size((size_t)first + (size_t)vertices - 1u, stride, &step) ||
                    !checked_add_size(b->data.offset, step, &final_offset) ||
                    !buffer_range(buffer, final_offset, element_size))
                    return 0;
            }
        }
        /* The validated buffer remains stable under the share-group lock through
         * draw submission. */
        inputs[i].native_buffer = buffer;
    }
    return 1;
}

static int native_pointer_matches(const struct mgl_context *ctx, GLuint location, GLuint buffer,
                                  const struct mgl_vertex_binding *b, GLsizei stride,
                                  size_t pointer_offset) {
    if ((ctx->draw_native_pointer_valid & (UINT32_C(1) << location)) == 0u)
        return 0;
    return ctx->draw_native_pointer[location].buffer == buffer &&
           ctx->draw_native_pointer[location].size == b->size &&
           ctx->draw_native_pointer[location].type == b->type &&
           ctx->draw_native_pointer[location].normalized == b->normalized &&
           ctx->draw_native_pointer[location].integer == b->integer &&
           ctx->draw_native_pointer[location].stride == stride &&
           ctx->draw_native_pointer[location].offset == pointer_offset &&
           ctx->draw_native_pointer[location].divisor == b->divisor;
}

static void realize_native_inputs(struct mgl_context *ctx, struct mgl_realized_input *inputs,
                                  size_t input_count) {
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_DRAW_NATIVE_ATTRIB);
    unsigned max_attribs = ctx->native_max_vertex_attribs > 0
                               ? (unsigned)ctx->native_max_vertex_attribs
                               : MGL_MAX_VERTEX_ATTRIBS;
    uint32_t desired_mask = 0u;
    uint32_t constant_sets = 0u, constant_skips = 0u;
    uint32_t pointer_sets = 0u, pointer_skips = 0u;
    uint32_t divisor_sets = 0u, divisor_skips = 0u;
    size_t i;
    unsigned location;
    /* The logical array binding is restored before returning. Resolve its
     * native name once, not again after processing every attribute. */
    const GLuint interface_array_bound = native_buffer_name(ctx, ctx->array_buffer);
    GLuint array_bound = interface_array_bound;
    if (max_attribs > MGL_MAX_VERTEX_ATTRIBS)
        max_attribs = MGL_MAX_VERTEX_ATTRIBS;
    if (ctx->draw_cached_vao != ctx->current_vao) {
        ctx->draw_cached_vao = ctx->current_vao;
        ctx->draw_enabled_mask_valid = GL_FALSE;
        ctx->draw_native_pointer_valid = 0u;
    }
    for (i = 0u; i < input_count; ++i)
        if (inputs[i].binding->enabled)
            desired_mask |= UINT32_C(1) << inputs[i].location;
    for (location = 0u; location < max_attribs; ++location) {
        uint32_t bit = UINT32_C(1) << location;
        if (!ctx->draw_enabled_mask_valid ||
            ((ctx->draw_enabled_mask ^ desired_mask) & bit) != 0u) {
            if ((desired_mask & bit) != 0u)
                ctx->native_dispatch->glesEnableVertexAttribArray(location);
            else
                ctx->native_dispatch->glesDisableVertexAttribArray(location);
        }
    }
    ctx->draw_enabled_mask = desired_mask;
    ctx->draw_enabled_mask_valid = GL_TRUE;
    for (i = 0u; i < input_count; ++i) {
        const struct mgl_vertex_binding *b = inputs[i].binding;
        struct mgl_buffer *buffer;
        GLsizei stride;
        GLuint native_name;
        size_t pointer_offset;
        if (!b->enabled) {
            if (realize_constant_input(ctx, &inputs[i]))
                ++constant_sets;
            else
                ++constant_skips;
            continue;
        }
        /* Validated by native_inputs_ready under the same share-group lock. */
        buffer = inputs[i].native_buffer;
        native_name = buffer != NULL ? buffer->native_name : 0u;
        pointer_offset = buffer != NULL ? b->data.offset : (size_t)(uintptr_t)b->data.client_ptr;
        stride = b->stride != 0 || b->separate_stride
                     ? b->stride
                     : (GLsizei)array_element_size(b->type, b->size);
        if (!native_pointer_matches(ctx, inputs[i].location, native_name, b, stride,
                                    pointer_offset)) {
            if (array_bound != native_name) {
                mgl_native_dispatch_bind_buffer(ctx, GL_ARRAY_BUFFER, native_name);
                array_bound = native_name;
            }
            if (b->separate_stride && stride == 0) {
                if (b->integer)
                    ctx->native_dispatch->glesVertexAttribIFormat(inputs[i].location, b->size,
                                                                  b->type, 0u);
                else
                    ctx->native_dispatch->glesVertexAttribFormat(inputs[i].location, b->size,
                                                                 b->type, b->normalized, 0u);
                ctx->native_dispatch->glesVertexAttribBinding(inputs[i].location,
                                                              inputs[i].location);
                ctx->native_dispatch->glesBindVertexBuffer(inputs[i].location, native_name,
                                                           (GLintptr)pointer_offset, 0);
            } else if (b->integer) {
                ctx->native_dispatch->glesVertexAttribIPointer(
                    inputs[i].location, b->size, b->type, stride,
                    (const void *)(uintptr_t)pointer_offset);
            } else {
                ctx->native_dispatch->glesVertexAttribPointer(
                    inputs[i].location, b->size, b->type, b->normalized, stride,
                    (const void *)(uintptr_t)pointer_offset);
            }
            if (ctx->native_dispatch->glesVertexAttribDivisor != NULL) {
                if ((ctx->draw_native_pointer_valid & (UINT32_C(1) << inputs[i].location)) == 0u ||
                    ctx->draw_native_pointer[inputs[i].location].divisor != b->divisor) {
                    ctx->native_dispatch->glesVertexAttribDivisor(inputs[i].location, b->divisor);
                    ++divisor_sets;
                } else
                    ++divisor_skips;
            }
            ctx->draw_native_pointer[inputs[i].location].buffer = native_name;
            ctx->draw_native_pointer[inputs[i].location].size = b->size;
            ctx->draw_native_pointer[inputs[i].location].type = b->type;
            ctx->draw_native_pointer[inputs[i].location].normalized = b->normalized;
            ctx->draw_native_pointer[inputs[i].location].integer = b->integer;
            ctx->draw_native_pointer[inputs[i].location].stride = stride;
            ctx->draw_native_pointer[inputs[i].location].offset = pointer_offset;
            ctx->draw_native_pointer[inputs[i].location].divisor = b->divisor;
            ctx->draw_native_pointer_valid |= UINT32_C(1) << inputs[i].location;
            ++pointer_sets;
        } else {
            ++pointer_skips;
        }
    }
    mgl_perf_diag_attrib(ctx, pointer_sets, pointer_skips, constant_sets, constant_skips,
                         divisor_sets, divisor_skips);
    if (array_bound != interface_array_bound)
        mgl_native_dispatch_bind_buffer(ctx, GL_ARRAY_BUFFER, interface_array_bound);
}

static void restore_array_buffer_binding(struct mgl_context *ctx) {
    mgl_native_dispatch_bind_buffer(ctx, GL_ARRAY_BUFFER,
                                    native_buffer_name(ctx, ctx->array_buffer));
}

static void restore_buffer_bindings(struct mgl_context *ctx) {
    restore_array_buffer_binding(ctx);
    mgl_native_dispatch_bind_buffer(
        ctx, GL_ELEMENT_ARRAY_BUFFER,
        native_buffer_name(ctx, ctx->current_vao->element_array_buffer));
}

static struct mgl_program *current_linked_program(struct mgl_context *ctx) {
    struct mgl_program *program;
    if (ctx->current_program == 0u) {
        if (ctx->current_pipeline != 0u) {
            struct mgl_pipeline *it;
            for (it = ctx->pipelines; it != NULL; it = it->next) {
                if (it->name == ctx->current_pipeline) {
                    if (it->stages[0] != NULL && it->stages[0]->linked)
                        return it->stages[0];
                    return NULL;
                }
            }
            return NULL;
        }
        if (ctx->compat.arb_vertex_program_enabled || ctx->compat.arb_fragment_program_enabled) {
            MGL_DEEP_SCOPE(ctx, MGL_DEEP_DRAW_ARB_LOOKUP);
            mgl_perf_diag_program_mode(ctx, 1u);
            return mgl_arb_current_linked_program(ctx);
        }
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_DRAW_FIXED_LOOKUP);
        mgl_perf_diag_program_mode(ctx, 2u);
        return mgl_compat_fixed_program(ctx);
    }
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_DRAW_GLSL_LOOKUP);
    mgl_perf_diag_program_mode(ctx, 0u);
    program = mgl_program_find(ctx, ctx->current_program);
    if (program == NULL || (!program->linked && !ctx->current_program_executable))
        return NULL;
    return program;
}

static int blend_factor_uses_secondary_source(GLenum factor) {
    return factor == GL_SRC1_COLOR || factor == GL_ONE_MINUS_SRC1_COLOR ||
           factor == GL_SRC1_ALPHA || factor == GL_ONE_MINUS_SRC1_ALPHA;
}

static int draw_buffer_uses_secondary_source(const struct mgl_draw_buffer_state *db) {
    if (db == NULL || !db->blend)
        return 0;
    return blend_factor_uses_secondary_source(db->blend_src_rgb) ||
           blend_factor_uses_secondary_source(db->blend_dst_rgb) ||
           blend_factor_uses_secondary_source(db->blend_src_alpha) ||
           blend_factor_uses_secondary_source(db->blend_dst_alpha);
}

static unsigned active_draw_buffer_count(struct mgl_context *ctx) {
    const GLenum *buffers = ctx->default_draw_buffers;
    struct mgl_framebuffer *fb;
    unsigned i, count = 0u;
    if (ctx->draw_framebuffer != 0u) {
        fb = mgl_framebuffer_find(ctx, ctx->draw_framebuffer);
        if (fb != NULL)
            buffers = fb->draw_buffers;
    }
    for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i)
        if (buffers[i] != GL_NONE)
            ++count;
    return count;
}

static int validate_dual_source_blend_realization(struct mgl_context *ctx) {
    unsigned i;
    int uses_secondary = 0;
    if (ctx == NULL)
        return 0;
    for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i) {
        if (draw_buffer_uses_secondary_source(&ctx->canonical_state.draw_buffers[i])) {
            uses_secondary = 1;
            break;
        }
    }
    if (!uses_secondary)
        return 1;
    if (ctx->native_dispatch == NULL || !mgl_native_dispatch_has_gles_extension(
                                            ctx->native_dispatch, "GL_EXT_blend_func_extended")) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (active_draw_buffer_count(ctx) > MGL_MAX_DUAL_SOURCE_DRAW_BUFFERS) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    return 1;
}

static int advanced_blend_equation(GLenum mode) {
    return (mode >= GL_MULTIPLY_KHR && mode <= GL_SOFTLIGHT_KHR) ||
           mode == GL_DIFFERENCE_KHR || mode == GL_EXCLUSION_KHR ||
           (mode >= GL_HSL_HUE_KHR && mode <= GL_HSL_LUMINOSITY_KHR);
}

static int validate_advanced_blend_realization(struct mgl_context *ctx) {
    const GLenum *buffers = ctx->default_draw_buffers;
    struct mgl_framebuffer *fb;
    unsigned i;
    if (ctx->draw_framebuffer != 0u) {
        fb = mgl_framebuffer_find(ctx, ctx->draw_framebuffer);
        if (fb != NULL)
            buffers = fb->draw_buffers;
    }
    for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i) {
        const struct mgl_draw_buffer_state *db = &ctx->canonical_state.draw_buffers[i];
        if (buffers[i] == GL_NONE ||
            (!advanced_blend_equation(db->blend_equation_rgb) &&
             !advanced_blend_equation(db->blend_equation_alpha)))
            continue;
        if (active_draw_buffer_count(ctx) != 1u || buffers[0] == GL_FRONT_AND_BACK ||
            buffers[0] == GL_NONE) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
    }
    return 1;
}

static int validate_buffer_sampler_realization(struct mgl_context *ctx,
                                               const struct mgl_program *program) {
    size_t i;
    if (ctx == NULL || program == NULL)
        return 0;
    if (program->buffer_sampler_reflection_failed) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (program->buffer_sampler_uniform_count == 0u)
        return 1;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGetUniformiv == NULL ||
        ctx->native_dispatch->glesTexBufferEXT == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    for (i = 0u; i < program->buffer_sampler_uniform_count; ++i) {
        const struct mgl_buffer_sampler_uniform *uniform = &program->buffer_sampler_uniforms[i];
        GLint unit = 0;
        GLuint texture_name;
        struct mgl_texture *texture;
        ctx->native_dispatch->glesGetUniformiv(program->native_name, uniform->location, &unit);
        if (unit < 0 || unit >= (GLint)mgl_programmable_texture_limit(ctx)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        texture_name = ctx->texture_buffer_binding[(unsigned)unit];
        texture = texture_name == 0u ? mgl_default_texture(ctx, GL_TEXTURE_BUFFER)
                                     : mgl_texture_find(ctx, texture_name);
        if (texture != NULL && texture->buffer_texture_buffer != NULL) {
            if (!mgl_texture_buffer_native_format_supported(
                    texture->buffer_texture_internal_format)) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return 0;
            }
            if ((texture->buffer_texture_offset != 0 ||
                 texture->buffer_texture_size !=
                     (GLsizeiptr)texture->buffer_texture_buffer->size) &&
                ctx->native_dispatch->glesTexBufferRangeEXT == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return 0;
            }
        }
    }
    return 1;
}

static int prepare_common(struct mgl_context *ctx, struct mgl_program **out_program,
                          struct mgl_realized_input *inputs, size_t *out_input_count) {
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_DRAW_PREPARE);
    struct mgl_program *program;
    size_t count;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesBufferData == NULL ||
        ctx->native_dispatch->glesVertexAttribPointer == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    {
        /* Separate shader-program lookup timing from texture and vertex-input preparation in
         * deep diagnostics. */
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_DRAW_PROGRAM_LOOKUP);
        program = current_linked_program(ctx);
    }
    if (program == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (ctx->native_dispatch->glesUseProgram == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (!validate_dual_source_blend_realization(ctx) ||
        !validate_advanced_blend_realization(ctx))
        return 0;
    if (!validate_buffer_sampler_realization(ctx, program))
        return 0;
    if (ctx->current_program == 0u && ctx->current_pipeline != 0u) {
        mgl_draw_use_native_program(ctx, 0u);
    } else {
        if (!program->linked) {
            GLint native_current = 0;
            if ((!ctx->draw_native_program_valid ||
                 ctx->draw_native_program != program->native_name) &&
                ctx->native_dispatch->glesGetIntegerv != NULL) {
                ctx->native_dispatch->glesGetIntegerv(GL_CURRENT_PROGRAM, &native_current);
                if (native_current == (GLint)program->native_name) {
                    ctx->draw_native_program = program->native_name;
                    ctx->draw_native_program_valid = GL_TRUE;
                }
            }
            if (!ctx->draw_native_program_valid ||
                ctx->draw_native_program != program->native_name) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return 0;
            }
        } else {
            mgl_draw_use_native_program(ctx, program->native_name);
        }
    }

    mgl_texture_realize_bindings(ctx);
    if (program == &ctx->fixed_program)
        mgl_texture_realize_fixed_bindings(ctx);
    if (ctx->native_max_vertex_attribs <= 0 && ctx->native_dispatch->glesGetIntegerv != NULL) {
        GLint max_attribs = 0;
        ctx->native_dispatch->glesGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_attribs);
        if (max_attribs <= 0 || max_attribs > (GLint)MGL_MAX_VERTEX_ATTRIBS)
            max_attribs = (GLint)MGL_MAX_VERTEX_ATTRIBS;
        ctx->native_max_vertex_attribs = max_attribs;
    }
    count =
        collect_inputs(ctx, program, inputs, MGL_MAX_VERTEX_ATTRIBS + MGL_COMPAT_SEMANTIC_COUNT);
    if (count > MGL_MAX_VERTEX_ATTRIBS + MGL_COMPAT_SEMANTIC_COUNT) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (!validate_input_realization(ctx, inputs, count))
        return 0;
    *out_program = program;
    *out_input_count = count;
    return 1;
}

void mgl_draw_transform_feedback_native(struct mgl_context *ctx, GLenum mode, GLuint native_tf,
                                        GLsizei instancecount) {
    struct mgl_program *program = NULL;
    struct mgl_realized_input inputs[MGL_MAX_VERTEX_ATTRIBS + MGL_COMPAT_SEMANTIC_COUNT];
    size_t input_count = 0u;
    int route;
    if (ctx == NULL || instancecount == 0)
        return;
    if (ctx->conditional_render_active && ctx->conditional_render_skip)
        return;
    if (ctx->compiling_list != NULL && !ctx->list_replaying) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_selection_active(ctx)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    if (!prepare_common(ctx, &program, inputs, &input_count)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return;
    }
    if (!native_inputs_ready(ctx, inputs, input_count, 0u, 0u, 1)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    realize_native_inputs(ctx, inputs, input_count);
    mgl_compat_realize_program_uniforms(ctx, program);
    mgl_arb_realize_uniforms(ctx, program);
    route = mgl_framebuffer_draw_route_begin(ctx);
    if (route >= 0) {
        if (instancecount == 1 && ctx->native_dispatch->glesDrawTransformFeedbackEXT != NULL)
            ctx->native_dispatch->glesDrawTransformFeedbackEXT(mode, native_tf);
        else
            ctx->native_dispatch->glesDrawTransformFeedbackInstancedEXT(mode, native_tf,
                                                                        instancecount);
    }
    if (route > 0)
        mgl_framebuffer_draw_route_end(ctx);
    restore_array_buffer_binding(ctx);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program = NULL;
    struct mgl_realized_input inputs[MGL_MAX_VERTEX_ATTRIBS + MGL_COMPAT_SEMANTIC_COUNT];
    size_t input_count = 0u;
    unsigned char *storage = NULL;
    size_t storage_size = 0u;
    int storage_borrowed = 0;
    uint64_t perf_started;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_compat_draw_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (first < 0 || count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (count == 0)
        return;
    if (ctx->compiling_list != NULL && !ctx->list_replaying) {
        struct mgl_draw_snapshot *snapshot =
            mgl_draw_snapshot_capture_arrays(ctx, mode, first, count);
        if (snapshot == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (mgl_list_record_draw_snapshot(ctx, snapshot))
            return;
    }
    if (ctx->conditional_render_active && ctx->conditional_render_skip)
        return;
    if (legacy_draw_mode(mode)) {
        uint32_t *indices;
        GLsizei index_count = 0;
        GLuint saved_element;
        size_t i;
        indices = triangulate_legacy_indices(mode, NULL, (size_t)count, &index_count);
        if (index_count < 0) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        if (index_count == 0)
            return;
        for (i = 0u; i < (size_t)index_count; ++i)
            indices[i] += (uint32_t)first;
        saved_element = ctx->current_vao->element_array_buffer;
        ctx->current_vao->element_array_buffer = 0u;
        glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, indices);
        mgl_restore_element_array_binding(ctx, saved_element);
        free(indices);
        return;
    }
    if (mgl_selection_active(ctx)) {
        mgl_selection_draw_arrays(ctx, mode, first, count);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesDrawArrays == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }

    perf_started = mgl_perf_diag_now();
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    uint64_t prepare_sequence = ctx->errors.sequence;
    if (!prepare_common(ctx, &program, inputs, &input_count)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        if (ctx->errors.sequence == prepare_sequence)
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    (void)program;
    if (native_inputs_ready(ctx, inputs, input_count, (uint32_t)first, (uint32_t)count, 0)) {
        realize_native_inputs(ctx, inputs, input_count);
        mgl_compat_realize_program_uniforms(ctx, program);
        mgl_arb_realize_uniforms(ctx, program);
        {
            MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW);
            {
                int route = mgl_framebuffer_draw_route_begin(ctx);
                if (route >= 0) {
                    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW_SUBMIT);
                    ctx->native_dispatch->glesDrawArrays(mode, first, count);
                }
                if (route > 0)
                    mgl_framebuffer_draw_route_end(ctx);
            }
        }
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_perf_diag_draw(ctx, 0, perf_started, 0u);
        return;
    }
    if (!ensure_scratch_buffers(ctx, 0)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    if (!pack_inputs(ctx, inputs, input_count, (uint32_t)first, (uint32_t)count, &storage,
                     &storage_size, &storage_borrowed)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    realize_inputs(ctx, inputs, input_count, storage, storage_size);
    mgl_compat_realize_program_uniforms(ctx, program);
    mgl_arb_realize_uniforms(ctx, program);
    {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW);
        {
            int route = mgl_framebuffer_draw_route_begin(ctx);
            if (route >= 0) {
                MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW_SUBMIT);
                ctx->native_dispatch->glesDrawArrays(mode, 0, count);
            }
            if (route > 0)
                mgl_framebuffer_draw_route_end(ctx);
        }
    }
    restore_array_buffer_binding(ctx);
    release_packed_storage(ctx, storage, storage_borrowed);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    mgl_perf_diag_draw(ctx, 1, perf_started, storage_size);
}

static int read_indices(struct mgl_context *ctx, GLsizei count, GLenum type, const void *indices,
                        uint32_t **out_indices, uint32_t *out_min, uint32_t *out_max) {
    size_t index_size;
    size_t bytes;
    const unsigned char *src;
    uint32_t *values;
    uint32_t min_index = UINT32_MAX;
    uint32_t max_index = 0u;
    GLsizei i;

    switch (type) {
        case GL_UNSIGNED_BYTE:
            index_size = 1u;
            break;
        case GL_UNSIGNED_SHORT:
            index_size = 2u;
            break;
        case GL_UNSIGNED_INT:
            index_size = 4u;
            break;
        default:
            return 0;
    }
    if (!checked_mul_size((size_t)count, index_size, &bytes))
        return 0;

    if (ctx->current_vao->element_array_buffer != 0u) {
        struct mgl_buffer *buffer = mgl_buffer_find(ctx, ctx->current_vao->element_array_buffer);
        size_t offset = (size_t)(uintptr_t)indices;
        if (!buffer_range(buffer, offset, bytes))
            return 0;
        if (!mgl_buffer_shadow_ensure(ctx, buffer))
            return 0;
        src = buffer->shadow + offset;
    } else {
        if (indices == NULL)
            return 0;
        src = (const unsigned char *)indices;
    }

    values = malloc((size_t)count * sizeof(*values));
    if (values == NULL)
        return 0;
    for (i = 0; i < count; ++i) {
        uint32_t value;
        if (type == GL_UNSIGNED_BYTE)
            value = src[(size_t)i];
        else if (type == GL_UNSIGNED_SHORT) {
            GLushort tmp;
            memcpy(&tmp, src + (size_t)i * index_size, sizeof(tmp));
            value = tmp;
        } else {
            GLuint tmp;
            memcpy(&tmp, src + (size_t)i * index_size, sizeof(tmp));
            value = tmp;
        }
        values[i] = value;
        if (value < min_index)
            min_index = value;
        if (value > max_index)
            max_index = value;
    }
    *out_indices = values;
    *out_min = min_index;
    *out_max = max_index;
    return 1;
}

struct mgl_draw_snapshot *mgl_draw_snapshot_capture_elements(struct mgl_context *ctx, GLenum mode,
                                                             GLsizei count, GLenum type,
                                                             const void *indices) {
    struct mgl_draw_snapshot *snapshot;
    uint32_t *logical = NULL;
    uint32_t min_index = 0u;
    uint32_t max_index = 0u;
    uint32_t vertex_count;
    GLsizei i;
    if (ctx == NULL || ctx->current_vao == NULL || count < 0)
        return NULL;
    if (!read_indices(ctx, count, type, indices, &logical, &min_index, &max_index))
        return NULL;
    if (max_index - min_index == UINT32_MAX) {
        free(logical);
        return NULL;
    }
    vertex_count = max_index - min_index + 1u;
    snapshot = calloc(1, sizeof(*snapshot));
    if (snapshot == NULL) {
        free(logical);
        return NULL;
    }
    snapshot->mode = mode;
    snapshot->count = count;
    snapshot->indexed = GL_TRUE;
    snapshot->vertex_count = vertex_count;
    snapshot->indices = malloc((size_t)count * sizeof(*snapshot->indices));
    if (snapshot->indices == NULL) {
        free(logical);
        mgl_draw_snapshot_destroy(snapshot);
        return NULL;
    }
    for (i = 0; i < count; ++i)
        snapshot->indices[i] = logical[i] - min_index;
    free(logical);
    if (!capture_snapshot_bindings(ctx, min_index, vertex_count, snapshot)) {
        mgl_draw_snapshot_destroy(snapshot);
        return NULL;
    }
    return snapshot;
}

static void *pack_rebased_indices(const uint32_t *indices, GLsizei count, uint32_t base,
                                  uint32_t span, GLenum *out_type, size_t *out_size) {
    GLsizei i;
    if (span <= UINT8_MAX) {
        GLubyte *out = malloc((size_t)count * sizeof(*out));
        if (out == NULL)
            return NULL;
        for (i = 0; i < count; ++i)
            out[i] = (GLubyte)(indices[i] - base);
        *out_type = GL_UNSIGNED_BYTE;
        *out_size = (size_t)count * sizeof(*out);
        return out;
    }
    if (span <= UINT16_MAX) {
        GLushort *out = malloc((size_t)count * sizeof(*out));
        if (out == NULL)
            return NULL;
        for (i = 0; i < count; ++i)
            out[i] = (GLushort)(indices[i] - base);
        *out_type = GL_UNSIGNED_SHORT;
        *out_size = (size_t)count * sizeof(*out);
        return out;
    }
    {
        GLuint *out = malloc((size_t)count * sizeof(*out));
        if (out == NULL)
            return NULL;
        for (i = 0; i < count; ++i)
            out[i] = indices[i] - base;
        *out_type = GL_UNSIGNED_INT;
        *out_size = (size_t)count * sizeof(*out);
        return out;
    }
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program = NULL;
    struct mgl_realized_input inputs[MGL_MAX_VERTEX_ATTRIBS + MGL_COMPAT_SEMANTIC_COUNT];
    size_t input_count = 0u;
    uint32_t *logical_indices = NULL;
    uint32_t min_index = 0u;
    uint32_t max_index = 0u;
    uint32_t vertex_count;
    unsigned char *vertex_storage = NULL;
    size_t vertex_storage_size = 0u;
    int storage_borrowed = 0;
    void *native_indices = NULL;
    GLenum native_index_type = 0u;
    size_t native_index_size = 0u;
    uint64_t perf_started;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_compat_draw_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (count == 0)
        return;
    if (ctx->compiling_list != NULL && !ctx->list_replaying) {
        struct mgl_draw_snapshot *snapshot =
            mgl_draw_snapshot_capture_elements(ctx, mode, count, type, indices);
        if (snapshot == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (mgl_list_record_draw_snapshot(ctx, snapshot))
            return;
    }
    if (ctx->conditional_render_active && ctx->conditional_render_skip)
        return;
    if (ctx->canonical_state.primitive_restart) {
        uint32_t *restart_indices = NULL;
        uint32_t ignored_min = 0u, ignored_max = 0u;
        GLsizei start = 0, i;
        GLuint saved_element = ctx->current_vao->element_array_buffer;
        if (!read_indices(ctx, count, type, indices, &restart_indices, &ignored_min,
                          &ignored_max)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        ctx->canonical_state.primitive_restart = GL_FALSE;
        ctx->current_vao->element_array_buffer = 0u;
        for (i = 0; i <= count; ++i) {
            if (i == count || restart_indices[i] == ctx->primitive_restart_index) {
                GLsizei segment_count = i - start;
                uint64_t draw_sequence = ctx->errors.sequence;
                if (segment_count > 0)
                    glDrawElements(mode, segment_count, GL_UNSIGNED_INT, restart_indices + start);
                start = i + 1;
                if (ctx->errors.sequence != draw_sequence)
                    break;
            }
        }
        mgl_restore_element_array_binding(ctx, saved_element);
        ctx->canonical_state.primitive_restart = GL_TRUE;
        free(restart_indices);
        return;
    }
    if (legacy_draw_mode(mode)) {
        uint32_t *source = NULL;
        uint32_t *converted;
        GLsizei index_count = 0;
        GLuint saved_element;
        if (!read_indices(ctx, count, type, indices, &source, &min_index, &max_index)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        converted = triangulate_legacy_indices(mode, source, (size_t)count, &index_count);
        free(source);
        if (index_count < 0) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        if (index_count == 0)
            return;
        saved_element = ctx->current_vao->element_array_buffer;
        ctx->current_vao->element_array_buffer = 0u;
        glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, converted);
        mgl_restore_element_array_binding(ctx, saved_element);
        free(converted);
        return;
    }
    if (mgl_selection_active(ctx)) {
        mgl_selection_draw_elements(ctx, mode, count, type, indices);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesDrawElements == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }

    perf_started = mgl_perf_diag_now();
    {
        uint64_t lock_started = mgl_perf_diag_now();
        (void)pthread_mutex_lock(&ctx->share_group->lock);
        mgl_deep_record(ctx, MGL_DEEP_DRAW_ELEMENTS_LOCK_WAIT, lock_started);
    }
    uint64_t prepare_sequence = ctx->errors.sequence;
    if (!prepare_common(ctx, &program, inputs, &input_count)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        if (ctx->errors.sequence == prepare_sequence)
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    (void)program;
    if (ctx->current_vao->element_array_buffer != 0u &&
        native_inputs_ready(ctx, inputs, input_count, 0u, 0u, 1)) {
        struct mgl_buffer *elements = mgl_buffer_find(ctx, ctx->current_vao->element_array_buffer);
        size_t element_size = scalar_size(type);
        size_t bytes;
        size_t offset = (size_t)(uintptr_t)indices;
        if (elements != NULL && elements->native_name != 0u &&
            checked_mul_size((size_t)count, element_size, &bytes) &&
            buffer_range(elements, offset, bytes)) {
            realize_native_inputs(ctx, inputs, input_count);
            mgl_compat_realize_program_uniforms(ctx, program);
            mgl_arb_realize_uniforms(ctx, program);
            {
                MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW);
                {
                    int route = mgl_framebuffer_draw_route_begin(ctx);
                    if (route >= 0) {
                        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW_SUBMIT);
                        ctx->native_dispatch->glesDrawElements(mode, count, type, indices);
                    }
                    if (route > 0)
                        mgl_framebuffer_draw_route_end(ctx);
                }
            }
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_perf_diag_draw(ctx, 0, perf_started, 0u);
            return;
        }
    }
    if (ctx->current_vao->element_array_buffer == 0u && indices != NULL &&
        native_inputs_ready(ctx, inputs, input_count, 0u, 0u, 1)) {
        size_t index_bytes;
        if (!checked_mul_size((size_t)count, scalar_size(type), &index_bytes) ||
            (!ctx->angle_client_arrays && !ensure_scratch_buffers(ctx, 1))) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        realize_native_inputs(ctx, inputs, input_count);
        mgl_compat_realize_program_uniforms(ctx, program);
        mgl_arb_realize_uniforms(ctx, program);
        if (ctx->angle_client_arrays) {
            mgl_native_dispatch_bind_buffer(ctx, GL_ELEMENT_ARRAY_BUFFER, 0u);
        } else {
            mgl_native_dispatch_bind_buffer(ctx, GL_ELEMENT_ARRAY_BUFFER,
                                            ctx->scratch_element_buffer_native);
            {
                uint64_t upload_started = mgl_perf_diag_now();
                ctx->native_dispatch->glesBufferData(
                    GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)index_bytes, indices, GL_STREAM_DRAW);
                mgl_deep_record(ctx, MGL_DEEP_GLES_INDEX_UPLOAD, upload_started);
            }
        }
        {
            MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW);
            {
                int route = mgl_framebuffer_draw_route_begin(ctx);
                if (route >= 0) {
                    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW_SUBMIT);
                    ctx->native_dispatch->glesDrawElements(
                        mode, count, type, ctx->angle_client_arrays ? indices : (const void *)0);
                }
                if (route > 0)
                    mgl_framebuffer_draw_route_end(ctx);
            }
        }
        mgl_native_dispatch_bind_buffer(ctx, GL_ELEMENT_ARRAY_BUFFER, 0u);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_perf_diag_draw(ctx, ctx->angle_client_arrays ? 3 : 2, perf_started,
                           ctx->angle_client_arrays ? 0u : index_bytes);
        return;
    }
    if (!ensure_scratch_buffers(ctx, 1)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    if (!read_indices(ctx, count, type, indices, &logical_indices, &min_index, &max_index)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (max_index - min_index == UINT32_MAX) {
        free(logical_indices);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    vertex_count = max_index - min_index + 1u;
    if (!pack_inputs(ctx, inputs, input_count, min_index, vertex_count, &vertex_storage,
                     &vertex_storage_size, &storage_borrowed)) {
        free(logical_indices);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    native_indices = pack_rebased_indices(logical_indices, count, min_index, max_index - min_index,
                                          &native_index_type, &native_index_size);
    if (native_indices == NULL) {
        release_packed_storage(ctx, vertex_storage, storage_borrowed);
        free(logical_indices);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }

    realize_inputs(ctx, inputs, input_count, vertex_storage, vertex_storage_size);
    mgl_compat_realize_program_uniforms(ctx, program);
    mgl_arb_realize_uniforms(ctx, program);
    mgl_native_dispatch_bind_buffer(ctx, GL_ELEMENT_ARRAY_BUFFER,
                                    ctx->scratch_element_buffer_native);
    {
        uint64_t upload_started = mgl_perf_diag_now();
        ctx->native_dispatch->glesBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)native_index_size,
                                             native_indices, GL_STREAM_DRAW);
        mgl_deep_record(ctx, MGL_DEEP_GLES_INDEX_UPLOAD, upload_started);
    }
    {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW);
        {
            int route = mgl_framebuffer_draw_route_begin(ctx);
            if (route >= 0) {
                MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW_SUBMIT);
                ctx->native_dispatch->glesDrawElements(mode, count, native_index_type,
                                                       (const void *)0);
            }
            if (route > 0)
                mgl_framebuffer_draw_route_end(ctx);
        }
    }
    restore_buffer_bindings(ctx);

    free(native_indices);
    release_packed_storage(ctx, vertex_storage, storage_borrowed);
    free(logical_indices);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    mgl_perf_diag_draw(ctx, 1, perf_started, vertex_storage_size + native_index_size);
}

void glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type,
                         const void *indices) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_compat_draw_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (end < start || count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    glDrawElements(mode, count, type, indices);
}

void glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_compat_draw_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (drawcount < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (drawcount == 0)
        return;
    if (first == NULL || count == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < drawcount; ++i) {
        if (first[i] < 0 || count[i] < 0) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
    }
    for (i = 0; i < drawcount; ++i) {
        uint64_t draw_sequence = ctx->errors.sequence;
        if (count[i] != 0)
            glDrawArrays(mode, first[i], count[i]);
        if (ctx->errors.sequence != draw_sequence)
            return;
    }
}

void glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const void *const *indices,
                         GLsizei drawcount) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_compat_draw_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (drawcount < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (drawcount == 0)
        return;
    if (count == NULL || indices == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < drawcount; ++i) {
        if (count[i] < 0) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
    }
    for (i = 0; i < drawcount; ++i) {
        uint64_t draw_sequence = ctx->errors.sequence;
        if (count[i] != 0)
            glDrawElements(mode, count[i], type, indices[i]);
        if (ctx->errors.sequence != draw_sequence)
            return;
    }
}

static int adjusted_indices_for_basevertex(struct mgl_context *ctx, GLsizei count, GLenum type,
                                           const void *indices, GLint basevertex, uint32_t **out) {
    uint32_t *logical = NULL;
    uint32_t ignored_min = 0u;
    uint32_t ignored_max = 0u;
    GLsizei i;
    if (!read_indices(ctx, count, type, indices, &logical, &ignored_min, &ignored_max))
        return 0;
    for (i = 0; i < count; ++i) {
        int64_t adjusted;
        if (ctx->canonical_state.primitive_restart && logical[i] == ctx->primitive_restart_index)
            continue;
        adjusted = (int64_t)logical[i] + (int64_t)basevertex;
        if (adjusted < 0 || adjusted > (int64_t)UINT32_MAX) {
            free(logical);
            return 0;
        }
        logical[i] = (uint32_t)adjusted;
    }
    *out = logical;
    return 1;
}

static int native_basevertex_draw(struct mgl_context *ctx, GLenum mode, GLsizei count, GLenum type,
                                  const void *indices, GLint basevertex) {
    struct mgl_program *program = NULL;
    struct mgl_realized_input inputs[MGL_MAX_VERTEX_ATTRIBS + MGL_COMPAT_SEMANTIC_COUNT];
    size_t input_count = 0u;
    struct mgl_buffer *elements;
    size_t bytes;
    uint64_t perf_started;
    if (ctx->native_dispatch == NULL || ctx->native_draw_elements_base_vertex == NULL ||
        ctx->current_vao->element_array_buffer == 0u || ctx->canonical_state.primitive_restart ||
        (ctx->conditional_render_active && ctx->conditional_render_skip) ||
        (ctx->compiling_list != NULL && !ctx->list_replaying) || mgl_selection_active(ctx) ||
        !valid_draw_mode(mode) ||
        (type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT) ||
        !checked_mul_size((size_t)count, scalar_size(type), &bytes))
        return 0;
    perf_started = mgl_perf_diag_now();
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    elements = mgl_buffer_find(ctx, ctx->current_vao->element_array_buffer);
    if (elements == NULL || elements->native_name == 0u || buffer_mapping_disallowed(elements) ||
        !buffer_range(elements, (size_t)(uintptr_t)indices, bytes) ||
        !prepare_common(ctx, &program, inputs, &input_count) ||
        !native_inputs_ready(ctx, inputs, input_count, 0u, 0u, 1)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return 0;
    }
    realize_native_inputs(ctx, inputs, input_count);
    if (ctx->native_dispatch->glesBindBuffer != NULL)
        mgl_native_dispatch_bind_buffer(ctx, GL_ELEMENT_ARRAY_BUFFER, elements->native_name);
    mgl_compat_realize_program_uniforms(ctx, program);
    mgl_arb_realize_uniforms(ctx, program);
    {
        int route = mgl_framebuffer_draw_route_begin(ctx);
        if (route >= 0) {
            MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW_SUBMIT);
            ctx->native_draw_elements_base_vertex(mode, count, type, indices, basevertex);
        }
        if (route > 0)
            mgl_framebuffer_draw_route_end(ctx);
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    mgl_perf_diag_draw(ctx, 0, perf_started, 0u);
    return 1;
}

void glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices,
                              GLint basevertex) {
    struct mgl_context *ctx = mgl_context_current();
    uint32_t *adjusted = NULL;
    GLuint saved_element;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_compat_draw_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (count == 0)
        return;
    if (basevertex == 0) {
        glDrawElements(mode, count, type, indices);
        return;
    }
    if (native_basevertex_draw(ctx, mode, count, type, indices, basevertex))
        return;
    if (!adjusted_indices_for_basevertex(ctx, count, type, indices, basevertex, &adjusted)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    saved_element = ctx->current_vao->element_array_buffer;
    ctx->current_vao->element_array_buffer = 0u;
    glDrawElements(mode, count, GL_UNSIGNED_INT, adjusted);
    mgl_restore_element_array_binding(ctx, saved_element);
    free(adjusted);
}

void glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count,
                                   GLenum type, const void *indices, GLint basevertex) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (end < start) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    glDrawElementsBaseVertex(mode, count, type, indices, basevertex);
}

void glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type,
                                   const void *const *indices, GLsizei drawcount,
                                   const GLint *basevertex) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (drawcount < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (drawcount == 0)
        return;
    if (count == NULL || indices == NULL || basevertex == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < drawcount; ++i) {
        if (count[i] < 0) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
    }
    for (i = 0; i < drawcount; ++i) {
        uint64_t draw_sequence = ctx->errors.sequence;
        if (count[i] != 0)
            glDrawElementsBaseVertex(mode, count[i], type, indices[i], basevertex[i]);
        if (ctx->errors.sequence != draw_sequence)
            return;
    }
}

static void draw_arrays_instanced_base_instance(GLenum mode, GLint first, GLsizei count,
                                                GLsizei instancecount, GLuint baseinstance) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program = NULL;
    struct mgl_realized_input inputs[MGL_MAX_VERTEX_ATTRIBS + MGL_COMPAT_SEMANTIC_COUNT];
    size_t input_count = 0u;
    unsigned char *storage = NULL;
    size_t storage_size = 0u;
    int storage_borrowed = 0;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_draw_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (first < 0 || count < 0 || instancecount < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (count == 0 || instancecount == 0)
        return;
    if (ctx->conditional_render_active && ctx->conditional_render_skip)
        return;
    if (mgl_selection_active(ctx)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesDrawArraysInstanced == NULL ||
        ctx->native_dispatch->glesVertexAttribDivisor == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    uint64_t prepare_sequence = ctx->errors.sequence;
    if (!prepare_common(ctx, &program, inputs, &input_count) || !ensure_scratch_buffers(ctx, 0)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        if (ctx->errors.sequence == prepare_sequence)
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    if (!pack_inputs_ex(ctx, inputs, input_count, (uint32_t)first, (uint32_t)count,
                        (uint32_t)instancecount, baseinstance, &storage, &storage_size,
                        &storage_borrowed)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    realize_inputs(ctx, inputs, input_count, storage, storage_size);
    mgl_compat_realize_program_uniforms(ctx, program);
    mgl_arb_realize_uniforms(ctx, program);
    {
        int route = mgl_framebuffer_draw_route_begin(ctx);
        if (route >= 0) {
            MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW_SUBMIT);
            ctx->native_dispatch->glesDrawArraysInstanced(mode, 0, count, instancecount);
        }
        if (route > 0)
            mgl_framebuffer_draw_route_end(ctx);
    }
    restore_array_buffer_binding(ctx);
    release_packed_storage(ctx, storage, storage_borrowed);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count,
                                       GLsizei instancecount, GLuint baseinstance) {
    draw_arrays_instanced_base_instance(mode, first, count, instancecount, baseinstance);
}

void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount) {
    draw_arrays_instanced_base_instance(mode, first, count, instancecount, 0u);
}

static void draw_elements_instanced_base_instance(GLenum mode, GLsizei count, GLenum type,
                                                  const void *indices, GLsizei instancecount,
                                                  GLuint baseinstance) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *program = NULL;
    struct mgl_realized_input inputs[MGL_MAX_VERTEX_ATTRIBS + MGL_COMPAT_SEMANTIC_COUNT];
    size_t input_count = 0u;
    uint32_t *logical_indices = NULL;
    uint32_t min_index = 0u, max_index = 0u, vertex_count;
    unsigned char *vertex_storage = NULL;
    size_t vertex_storage_size = 0u;
    int storage_borrowed = 0;
    void *native_indices = NULL;
    GLenum native_index_type = 0u;
    size_t native_index_size = 0u;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_draw_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (count < 0 || instancecount < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (count == 0 || instancecount == 0)
        return;
    if (ctx->conditional_render_active && ctx->conditional_render_skip)
        return;
    if (mgl_selection_active(ctx)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesDrawElementsInstanced == NULL ||
        ctx->native_dispatch->glesVertexAttribDivisor == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    uint64_t prepare_sequence = ctx->errors.sequence;
    if (!prepare_common(ctx, &program, inputs, &input_count) || !ensure_scratch_buffers(ctx, 1)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        if (ctx->errors.sequence == prepare_sequence)
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    if (!read_indices(ctx, count, type, indices, &logical_indices, &min_index, &max_index)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (max_index - min_index == UINT32_MAX) {
        free(logical_indices);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    vertex_count = max_index - min_index + 1u;
    if (!pack_inputs_ex(ctx, inputs, input_count, min_index, vertex_count, (uint32_t)instancecount,
                        baseinstance, &vertex_storage, &vertex_storage_size, &storage_borrowed)) {
        free(logical_indices);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    native_indices = pack_rebased_indices(logical_indices, count, min_index, max_index - min_index,
                                          &native_index_type, &native_index_size);
    if (native_indices == NULL) {
        release_packed_storage(ctx, vertex_storage, storage_borrowed);
        free(logical_indices);
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    realize_inputs(ctx, inputs, input_count, vertex_storage, vertex_storage_size);
    mgl_compat_realize_program_uniforms(ctx, program);
    mgl_arb_realize_uniforms(ctx, program);
    mgl_native_dispatch_bind_buffer(ctx, GL_ELEMENT_ARRAY_BUFFER,
                                    ctx->scratch_element_buffer_native);
    ctx->native_dispatch->glesBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)native_index_size,
                                         native_indices, GL_STREAM_DRAW);
    {
        MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW_SUBMIT);
        ctx->native_dispatch->glesDrawElementsInstanced(mode, count, native_index_type,
                                                        (const void *)0, instancecount);
    }
    restore_buffer_bindings(ctx);
    free(native_indices);
    release_packed_storage(ctx, vertex_storage, storage_borrowed);
    free(logical_indices);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type,
                                         const void *indices, GLsizei instancecount,
                                         GLuint baseinstance) {
    draw_elements_instanced_base_instance(mode, count, type, indices, instancecount, baseinstance);
}

void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices,
                             GLsizei instancecount) {
    draw_elements_instanced_base_instance(mode, count, type, indices, instancecount, 0u);
}

static void draw_elements_instanced_base_vertex_base_instance(GLenum mode, GLsizei count,
                                                              GLenum type, const void *indices,
                                                              GLsizei instancecount,
                                                              GLint basevertex,
                                                              GLuint baseinstance) {
    struct mgl_context *ctx = mgl_context_current();
    uint32_t *adjusted = NULL;
    GLuint saved_element;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (count < 0 || instancecount < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (count == 0 || instancecount == 0)
        return;
    if (!adjusted_indices_for_basevertex(ctx, count, type, indices, basevertex, &adjusted)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    saved_element = ctx->current_vao->element_array_buffer;
    ctx->current_vao->element_array_buffer = 0u;
    draw_elements_instanced_base_instance(mode, count, GL_UNSIGNED_INT, adjusted, instancecount,
                                          baseinstance);
    mgl_restore_element_array_binding(ctx, saved_element);
    free(adjusted);
}

void glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type,
                                                   const void *indices, GLsizei instancecount,
                                                   GLint basevertex, GLuint baseinstance) {
    draw_elements_instanced_base_vertex_base_instance(mode, count, type, indices, instancecount,
                                                      basevertex, baseinstance);
}

void glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices,
                                       GLsizei instancecount, GLint basevertex) {
    draw_elements_instanced_base_vertex_base_instance(mode, count, type, indices, instancecount,
                                                      basevertex, 0u);
}

struct mgl_draw_arrays_indirect_command {
    GLuint count;
    GLuint instance_count;
    GLuint first;
    GLuint base_instance;
};

struct mgl_draw_elements_indirect_command {
    GLuint count;
    GLuint instance_count;
    GLuint first_index;
    GLint base_vertex;
    GLuint base_instance;
};

static int indirect_bytes(struct mgl_context *ctx, const void *indirect, size_t size, void *out) {
    struct mgl_buffer *buffer;
    size_t offset;
    if (ctx == NULL || out == NULL || ctx->draw_indirect_buffer == 0u)
        return 0;
    buffer = mgl_buffer_find(ctx, ctx->draw_indirect_buffer);
    offset = (size_t)(uintptr_t)indirect;
    if (!buffer_range(buffer, offset, size))
        return 0;
    return mgl_buffer_read_native_bytes(ctx, buffer, offset, size, out);
}

static int native_indirect_draw(struct mgl_context *ctx, GLenum mode, GLenum type,
                                const void *indirect, const void *command, int indexed) {
    struct mgl_program *program = NULL;
    struct mgl_realized_input inputs[MGL_MAX_VERTEX_ATTRIBS + MGL_COMPAT_SEMANTIC_COUNT];
    size_t input_count = 0u;
    struct mgl_buffer *indirect_buffer;
    struct mgl_buffer *elements = NULL;
    size_t bytes;
    size_t offset = (size_t)(uintptr_t)indirect;
    uint64_t perf_started;
    const struct mgl_draw_arrays_indirect_command *arrays = command;
    const struct mgl_draw_elements_indirect_command *indexed_command = command;
    if (ctx->native_dispatch == NULL || !valid_draw_mode(mode) ||
        (indexed && (ctx->current_vao->element_array_buffer == 0u ||
                     ctx->canonical_state.primitive_restart)) ||
        (ctx->compiling_list != NULL && !ctx->list_replaying) || mgl_selection_active(ctx) ||
        (indexed ? indexed_command->base_instance : arrays->base_instance) != 0u ||
        (offset & 3u) != 0u || ctx->draw_indirect_buffer == 0u)
        return 0;
    if (ctx->conditional_render_active && ctx->conditional_render_skip)
        return 1;
    if (indexed ? (indexed_command->count == 0u || indexed_command->instance_count == 0u)
                : (arrays->count == 0u || arrays->instance_count == 0u))
        return 1;
    perf_started = mgl_perf_diag_now();
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    indirect_buffer = mgl_buffer_find(ctx, ctx->draw_indirect_buffer);
    if (indirect_buffer == NULL || indirect_buffer->native_name == 0u ||
        buffer_mapping_disallowed(indirect_buffer)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return 0;
    }
    if (indexed) {
        size_t index_size = scalar_size(type);
        size_t index_offset;
        elements = mgl_buffer_find(ctx, ctx->current_vao->element_array_buffer);
        if (elements == NULL || elements->native_name == 0u || buffer_mapping_disallowed(elements) ||
            !checked_mul_size((size_t)indexed_command->first_index, index_size, &index_offset) ||
            !checked_mul_size((size_t)indexed_command->count, index_size, &bytes) ||
            !buffer_range(elements, index_offset, bytes)) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            return 0;
        }
    }
    if (!prepare_common(ctx, &program, inputs, &input_count) ||
        !native_inputs_ready(ctx, inputs, input_count, indexed ? 0u : arrays->first,
                             indexed ? 0u : arrays->count, indexed)) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        return 0;
    }
    realize_native_inputs(ctx, inputs, input_count);
    if (ctx->native_dispatch->glesBindBuffer != NULL) {
        mgl_native_dispatch_bind_buffer(ctx, GL_DRAW_INDIRECT_BUFFER, indirect_buffer->native_name);
        if (indexed)
            mgl_native_dispatch_bind_buffer(ctx, GL_ELEMENT_ARRAY_BUFFER, elements->native_name);
    }
    mgl_compat_realize_program_uniforms(ctx, program);
    mgl_arb_realize_uniforms(ctx, program);
    {
        int route = mgl_framebuffer_draw_route_begin(ctx);
        if (route >= 0) {
            if (indexed) {
                MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW_SUBMIT);
                ctx->native_dispatch->glesDrawElementsIndirect(mode, type, indirect);
            } else {
                MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_DRAW_SUBMIT);
                ctx->native_dispatch->glesDrawArraysIndirect(mode, indirect);
            }
        }
        if (route > 0)
            mgl_framebuffer_draw_route_end(ctx);
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    mgl_perf_diag_draw(ctx, 0, perf_started, 0u);
    return 1;
}

void glDrawArraysIndirect(GLenum mode, const void *indirect) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_draw_arrays_indirect_command command;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_draw_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!indirect_bytes(ctx, indirect, sizeof(command), &command)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (command.first > (GLuint)INT32_MAX || command.count > (GLuint)INT32_MAX ||
        command.instance_count > (GLuint)INT32_MAX) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (native_indirect_draw(ctx, mode, 0u, indirect, &command, 0))
        return;
    draw_arrays_instanced_base_instance(mode, (GLint)command.first, (GLsizei)command.count,
                                        (GLsizei)command.instance_count, command.base_instance);
}

void glDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_draw_elements_indirect_command command;
    size_t index_size;
    size_t byte_offset;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_draw_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (type == GL_UNSIGNED_BYTE)
        index_size = 1u;
    else if (type == GL_UNSIGNED_SHORT)
        index_size = 2u;
    else if (type == GL_UNSIGNED_INT)
        index_size = 4u;
    else {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!indirect_bytes(ctx, indirect, sizeof(command), &command)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (command.count > (GLuint)INT32_MAX || command.instance_count > (GLuint)INT32_MAX ||
        !checked_mul_size((size_t)command.first_index, index_size, &byte_offset)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (native_indirect_draw(ctx, mode, type, indirect, &command, 1))
        return;
    draw_elements_instanced_base_vertex_base_instance(
        mode, (GLsizei)command.count, type, (const void *)(uintptr_t)byte_offset,
        (GLsizei)command.instance_count, command.base_vertex, command.base_instance);
}

static int mgl_multi_indirect_validate(struct mgl_context *ctx, const void *indirect,
                                       GLsizei drawcount, GLsizei stride, size_t command_size,
                                       int indexed) {
    struct mgl_buffer *buffer;
    size_t start, step, last;
    if (drawcount < 0 || stride < 0 || (stride & 3) != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (indexed && ctx->current_vao->element_array_buffer == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (indexed) {
        struct mgl_buffer *elements = mgl_buffer_find(ctx, ctx->current_vao->element_array_buffer);
        if (elements == NULL || buffer_mapping_disallowed(elements)) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
    }
    buffer = mgl_buffer_find(ctx, ctx->draw_indirect_buffer);
    if (buffer == NULL || buffer_mapping_disallowed(buffer)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    start = (size_t)(uintptr_t)indirect;
    if ((start & 3u) != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (drawcount == 0)
        return 1;
    step = stride != 0 ? (size_t)stride : command_size;
    if ((size_t)(drawcount - 1) > (SIZE_MAX - start) / step) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    last = start + (size_t)(drawcount - 1) * step;
    if (!buffer_range(buffer, last, command_size)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    return 1;
}

void glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount,
                               GLsizei stride) {
    struct mgl_context *ctx = mgl_context_current();
    size_t offset, step;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_draw_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!mgl_multi_indirect_validate(ctx, indirect, drawcount, stride,
                                     sizeof(struct mgl_draw_arrays_indirect_command), 0))
        return;
    offset = (size_t)(uintptr_t)indirect;
    step = stride != 0 ? (size_t)stride : sizeof(struct mgl_draw_arrays_indirect_command);
    for (GLsizei i = 0; i < drawcount; ++i) {
        glDrawArraysIndirect(mode, (const void *)(uintptr_t)offset);
        offset += step;
    }
}

void glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect, GLsizei drawcount,
                                 GLsizei stride) {
    struct mgl_context *ctx = mgl_context_current();
    size_t offset, step;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_draw_mode(mode) ||
        (type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!mgl_multi_indirect_validate(ctx, indirect, drawcount, stride,
                                     sizeof(struct mgl_draw_elements_indirect_command), 1))
        return;
    offset = (size_t)(uintptr_t)indirect;
    step = stride != 0 ? (size_t)stride : sizeof(struct mgl_draw_elements_indirect_command);
    for (GLsizei i = 0; i < drawcount; ++i) {
        glDrawElementsIndirect(mode, type, (const void *)(uintptr_t)offset);
        offset += step;
    }
}

static int indirect_parameter_count(struct mgl_context *ctx, GLintptr offset, GLsizei maxdrawcount,
                                    GLsizei stride, GLsizei *result) {
    struct mgl_buffer *buffer;
    GLint count = 0;
    if (maxdrawcount < 0 || stride < 0 || (stride & 3) != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (ctx->parameter_buffer == 0u || offset < 0 || (offset & 3) != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    buffer = mgl_buffer_find(ctx, ctx->parameter_buffer);
    if (buffer == NULL || buffer_mapping_disallowed(buffer) || (size_t)offset > buffer->size ||
        sizeof(count) > buffer->size - (size_t)offset) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (!mgl_buffer_read_native_bytes(ctx, buffer, (size_t)offset, sizeof(count), &count))
        return 0;
    *result = count <= 0 ? 0 : count > maxdrawcount ? maxdrawcount : count;
    return 1;
}

void glMultiDrawArraysIndirectCount(GLenum mode, const void *indirect, GLintptr drawcount,
                                    GLsizei maxdrawcount, GLsizei stride) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei count;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!indirect_parameter_count(ctx, drawcount, maxdrawcount, stride, &count))
        return;
    glMultiDrawArraysIndirect(mode, indirect, count, stride);
}

void glMultiDrawElementsIndirectCount(GLenum mode, GLenum type, const void *indirect,
                                      GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei count;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!indirect_parameter_count(ctx, drawcount, maxdrawcount, stride, &count))
        return;
    glMultiDrawElementsIndirect(mode, type, indirect, count, stride);
}

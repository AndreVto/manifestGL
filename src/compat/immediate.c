#include "core/render_diag.h"
#include "compat/immediate.h"

#include "core/context.h"
#include "mgl/gl.h"
#include "shader/translator.h"
#include "list/display_list.h"
#include "objects/object_store.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int valid_begin_mode(GLenum mode) {
    switch (mode) {
        case GL_POINTS:
        case GL_LINES:
        case GL_LINE_LOOP:
        case GL_LINE_STRIP:
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
        case GL_QUADS:
        case GL_QUAD_STRIP:
        case GL_POLYGON:
            return 1;
        default:
            return 0;
    }
}

void mgl_immediate_destroy(struct mgl_immediate_state *state) {
    if (state == NULL)
        return;
    free(state->vertices);
    memset(state, 0, sizeof(*state));
}

static int reserve_vertices(struct mgl_immediate_state *state, size_t extra) {
    size_t needed;
    size_t cap;
    struct mgl_immediate_vertex *next;
    if (extra > SIZE_MAX - state->count)
        return 0;
    needed = state->count + extra;
    if (needed <= state->capacity)
        return 1;
    cap = state->capacity != 0u ? state->capacity : 64u;
    while (cap < needed) {
        if (cap > SIZE_MAX / 2u) {
            cap = needed;
            break;
        }
        cap *= 2u;
    }
    if (cap > SIZE_MAX / sizeof(*next))
        return 0;
    next = realloc(state->vertices, cap * sizeof(*next));
    if (next == NULL)
        return 0;
    state->vertices = next;
    state->capacity = cap;
    return 1;
}

void glBegin(GLenum mode) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_begin_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_begin(ctx, mode)) {
        ctx->immediate.active = GL_TRUE;
        ctx->immediate.mode = mode;
        ctx->immediate.count = 0u;
        return;
    }
    ctx->immediate.active = GL_TRUE;
    ctx->immediate.mode = mode;
    ctx->immediate.count = 0u;
}

static void append_vertex(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_immediate_vertex *v;
    unsigned unit;
    if (ctx == NULL)
        return;
    if (!ctx->immediate.active)
        return;
    if (mgl_list_record_vertex4f(ctx, x, y, z, w))
        return;
    if (!reserve_vertices(&ctx->immediate, 1u)) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    v = &ctx->immediate.vertices[ctx->immediate.count++];
    v->position[0] = x;
    v->position[1] = y;
    v->position[2] = z;
    v->position[3] = w;
    memcpy(v->normal, ctx->compat_current[MGL_COMPAT_SEM_NORMAL], sizeof(v->normal));
    memcpy(v->color, ctx->compat_current[MGL_COMPAT_SEM_COLOR], sizeof(v->color));
    memcpy(v->secondary, ctx->compat_current[MGL_COMPAT_SEM_SECONDARY], sizeof(v->secondary));
    v->fog = ctx->compat_current[MGL_COMPAT_SEM_FOG][0];
    v->edge_flag = ctx->current_edge_flag;
    for (unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit)
        memcpy(v->texcoord[unit], ctx->compat_current[MGL_COMPAT_SEM_TEXCOORD0 + unit],
               sizeof(v->texcoord[unit]));
}

void glVertex2f(GLfloat x, GLfloat y) {
    append_vertex(x, y, 0.0f, 1.0f);
}

void glVertex2fv(const GLfloat *v) {
    if (v != NULL)
        glVertex2f(v[0], v[1]);
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    append_vertex(x, y, z, 1.0f);
}

void glVertex3fv(const GLfloat *v) {
    if (v != NULL)
        glVertex3f(v[0], v[1], v[2]);
}

void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    append_vertex(x, y, z, w);
}

void glVertex4fv(const GLfloat *v) {
    if (v != NULL)
        glVertex4f(v[0], v[1], v[2], v[3]);
}

void glVertex2d(GLdouble x, GLdouble y) {
    glVertex2f((GLfloat)x, (GLfloat)y);
}

void glVertex3d(GLdouble x, GLdouble y, GLdouble z) {
    glVertex3f((GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    glVertex4f((GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
}

void glVertex2dv(const GLdouble *v) {
    if (v != NULL)
        glVertex2f((GLfloat)v[0], (GLfloat)v[1]);
}

void glVertex2i(GLint x, GLint y) {
    glVertex2f((GLfloat)x, (GLfloat)y);
}

void glVertex2iv(const GLint *v) {
    if (v != NULL)
        glVertex2f((GLfloat)v[0], (GLfloat)v[1]);
}

void glVertex2s(GLshort x, GLshort y) {
    glVertex2f((GLfloat)x, (GLfloat)y);
}

void glVertex2sv(const GLshort *v) {
    if (v != NULL)
        glVertex2f((GLfloat)v[0], (GLfloat)v[1]);
}

void glVertex3dv(const GLdouble *v) {
    if (v != NULL)
        glVertex3f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2]);
}

void glVertex3i(GLint x, GLint y, GLint z) {
    glVertex3f((GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void glVertex3iv(const GLint *v) {
    if (v != NULL)
        glVertex3f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2]);
}

void glVertex3s(GLshort x, GLshort y, GLshort z) {
    glVertex3f((GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void glVertex3sv(const GLshort *v) {
    if (v != NULL)
        glVertex3f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2]);
}

void glVertex4dv(const GLdouble *v) {
    if (v != NULL)
        glVertex4f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

void glVertex4i(GLint x, GLint y, GLint z, GLint w) {
    glVertex4f((GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
}

void glVertex4iv(const GLint *v) {
    if (v != NULL)
        glVertex4f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

void glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w) {
    glVertex4f((GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
}

void glVertex4sv(const GLshort *v) {
    if (v != NULL)
        glVertex4f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

static int unpack_vertex_p(GLenum type, GLuint packed, GLfloat out[4]) {
    unsigned i;
    if (type == GL_UNSIGNED_INT_2_10_10_10_REV) {
        out[0] = (GLfloat)(packed & 0x3ffu);
        out[1] = (GLfloat)((packed >> 10) & 0x3ffu);
        out[2] = (GLfloat)((packed >> 20) & 0x3ffu);
        out[3] = (GLfloat)((packed >> 30) & 0x3u);
        return 1;
    }
    if (type == GL_INT_2_10_10_10_REV) {
        const unsigned widths[4] = {10u, 10u, 10u, 2u};
        const unsigned shifts[4] = {0u, 10u, 20u, 30u};
        for (i = 0u; i < 4u; ++i) {
            const unsigned width = widths[i];
            const uint32_t mask = (UINT32_C(1) << width) - UINT32_C(1);
            uint32_t bits = ((uint32_t)packed >> shifts[i]) & mask;
            const uint32_t sign = UINT32_C(1) << (width - 1u);
            int32_t value = (int32_t)((bits ^ sign) - sign);
            out[i] = (GLfloat)value;
        }
        return 1;
    }
    return 0;
}

static int vertex_p_values(GLenum type, GLuint packed, GLfloat out[4]) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return 0;
    if (!unpack_vertex_p(type, packed, out)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    return 1;
}

void glVertexP2ui(GLenum type, GLuint value) {
    GLfloat v[4];
    if (vertex_p_values(type, value, v))
        glVertex2f(v[0], v[1]);
}

void glVertexP2uiv(GLenum type, const GLuint *value) {
    if (value != NULL)
        glVertexP2ui(type, *value);
}

void glVertexP3ui(GLenum type, GLuint value) {
    GLfloat v[4];
    if (vertex_p_values(type, value, v))
        glVertex3f(v[0], v[1], v[2]);
}

void glVertexP3uiv(GLenum type, const GLuint *value) {
    if (value != NULL)
        glVertexP3ui(type, *value);
}

void glVertexP4ui(GLenum type, GLuint value) {
    GLfloat v[4];
    if (vertex_p_values(type, value, v))
        glVertex4f(v[0], v[1], v[2], v[3]);
}

void glVertexP4uiv(GLenum type, const GLuint *value) {
    if (value != NULL)
        glVertexP4ui(type, *value);
}

static size_t array_element_scalar_size(GLenum type) {
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
            return 4u;
        case GL_DOUBLE:
            return 8u;
        default:
            return 0u;
    }
}

static GLfloat half_to_float(GLushort h) {
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
            bits = sign | (GLuint)(e + 127) << 23 | mant << 13;
        }
    } else if (exp == 0x1fu)
        bits = sign | 0x7f800000u | (mant << 13);
    else
        bits = sign | ((exp + 112u) << 23) | (mant << 13);
    memcpy(&out, &bits, sizeof(out));
    return out;
}

static GLfloat array_element_component(const unsigned char *src, GLenum type,
                                       GLboolean normalized) {
    switch (type) {
        case GL_BYTE: {
            GLbyte value;
            memcpy(&value, src, sizeof(value));
            if (!normalized)
                return (GLfloat)value;
            return value == INT8_MIN ? -1.0f : (GLfloat)value / (GLfloat)INT8_MAX;
        }
        case GL_UNSIGNED_BYTE: {
            GLubyte value;
            memcpy(&value, src, sizeof(value));
            return normalized ? (GLfloat)value / (GLfloat)UINT8_MAX : (GLfloat)value;
        }
        case GL_SHORT: {
            GLshort value;
            memcpy(&value, src, sizeof(value));
            if (!normalized)
                return (GLfloat)value;
            return value == INT16_MIN ? -1.0f : (GLfloat)value / (GLfloat)INT16_MAX;
        }
        case GL_UNSIGNED_SHORT: {
            GLushort value;
            memcpy(&value, src, sizeof(value));
            return normalized ? (GLfloat)value / (GLfloat)UINT16_MAX : (GLfloat)value;
        }
        case GL_INT: {
            GLint value;
            memcpy(&value, src, sizeof(value));
            if (!normalized)
                return (GLfloat)value;
            return value == INT32_MIN ? -1.0f : (GLfloat)((double)value / (double)INT32_MAX);
        }
        case GL_UNSIGNED_INT: {
            GLuint value;
            memcpy(&value, src, sizeof(value));
            return normalized ? (GLfloat)((double)value / (double)UINT32_MAX) : (GLfloat)value;
        }
        case GL_HALF_FLOAT: {
            GLushort value;
            memcpy(&value, src, sizeof(value));
            return half_to_float(value);
        }
        case GL_FLOAT: {
            GLfloat value;
            memcpy(&value, src, sizeof(value));
            return value;
        }
        case GL_DOUBLE: {
            GLdouble value;
            memcpy(&value, src, sizeof(value));
            return (GLfloat)value;
        }
        default:
            return 0.0f;
    }
}

static struct mgl_buffer *array_element_buffer(struct mgl_context *ctx,
                                               const struct mgl_vertex_binding *binding) {
    struct mgl_buffer *it;
    if (ctx == NULL || ctx->share_group == NULL || binding == NULL ||
        binding->data.source != MGL_DATA_BUFFER || binding->data.buffer_name == 0u)
        return NULL;
    for (it = ctx->share_group->buffers; it != NULL; it = it->next) {
        if (it->name == binding->data.buffer_name)
            return it;
    }
    return NULL;
}

static const unsigned char *array_element_data(struct mgl_context *ctx,
                                               const struct mgl_vertex_binding *binding,
                                               GLuint index, size_t bytes) {
    size_t stride = binding->stride != 0 ? (size_t)binding->stride : bytes;
    size_t element_offset;
    if (index != 0u && stride > SIZE_MAX / (size_t)index)
        return NULL;
    element_offset = stride * (size_t)index;
    if (binding->data.source == MGL_DATA_CLIENT) {
        return binding->data.client_ptr != NULL
                   ? (const unsigned char *)binding->data.client_ptr + element_offset
                   : NULL;
    }
    {
        struct mgl_buffer *buffer = array_element_buffer(ctx, binding);
        size_t absolute;
        if (buffer == NULL || binding->data.offset > SIZE_MAX - element_offset)
            return NULL;
        absolute = binding->data.offset + element_offset;
        if (absolute > buffer->size || bytes > buffer->size - absolute)
            return NULL;
        return mgl_buffer_shadow_ensure(ctx, buffer) ? buffer->shadow + absolute : NULL;
    }
}

static int array_element_values(struct mgl_context *ctx, const struct mgl_vertex_binding *binding,
                                GLuint index, GLfloat out[4]) {
    size_t scalar;
    size_t bytes;
    const unsigned char *src;
    GLint c;
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = 0.0f;
    out[3] = 1.0f;
    scalar = array_element_scalar_size(binding->type);
    if (scalar == 0u || binding->size < 1 || binding->size > 4 ||
        (size_t)binding->size > SIZE_MAX / scalar)
        return 0;
    bytes = (size_t)binding->size * scalar;
    src = array_element_data(ctx, binding, index, bytes);
    if (src == NULL)
        return 0;
    for (c = 0; c < binding->size; ++c)
        out[c] =
            array_element_component(src + (size_t)c * scalar, binding->type, binding->normalized);
    return 1;
}

static int array_element_validate_mapped_buffers(struct mgl_context *ctx) {
    unsigned semantic;
    for (semantic = 0u; semantic < MGL_COMPAT_SEMANTIC_COUNT; ++semantic) {
        const struct mgl_vertex_binding *binding = &ctx->current_vao->compat[semantic];
        struct mgl_buffer *buffer;
        if (!binding->enabled || binding->data.source != MGL_DATA_BUFFER)
            continue;
        buffer = array_element_buffer(ctx, binding);
        if (buffer != NULL && buffer->mapped)
            return 0;
    }
    return 1;
}

void glArrayElement(GLint i) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat value[4];
    unsigned unit;
    const struct mgl_vertex_binding *binding;
    if (ctx == NULL)
        return;
    if (i < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!array_element_validate_mapped_buffers(ctx)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }

    binding = &ctx->current_vao->compat[MGL_COMPAT_SEM_NORMAL];
    if (binding->enabled && array_element_values(ctx, binding, (GLuint)i, value))
        glNormal3f(value[0], value[1], value[2]);

    binding = &ctx->current_vao->compat[MGL_COMPAT_SEM_COLOR];
    if (binding->enabled && array_element_values(ctx, binding, (GLuint)i, value))
        glColor4f(value[0], value[1], value[2], binding->size >= 4 ? value[3] : 1.0f);

    binding = &ctx->current_vao->compat[MGL_COMPAT_SEM_SECONDARY];
    if (binding->enabled && array_element_values(ctx, binding, (GLuint)i, value))
        glSecondaryColor3f(value[0], value[1], value[2]);

    binding = &ctx->current_vao->compat[MGL_COMPAT_SEM_FOG];
    if (binding->enabled && array_element_values(ctx, binding, (GLuint)i, value))
        glFogCoordf(value[0]);

    for (unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit) {
        binding = &ctx->current_vao->compat[MGL_COMPAT_SEM_TEXCOORD0 + unit];
        if (binding->enabled && array_element_values(ctx, binding, (GLuint)i, value))
            glMultiTexCoord4f(GL_TEXTURE0 + unit, value[0], value[1], value[2], value[3]);
    }

    binding = &ctx->current_vao->compat[MGL_COMPAT_SEM_POSITION];
    if (binding->enabled && array_element_values(ctx, binding, (GLuint)i, value)) {
        if (binding->size == 2)
            glVertex2f(value[0], value[1]);
        else if (binding->size == 3)
            glVertex3f(value[0], value[1], value[2]);
        else
            glVertex4f(value[0], value[1], value[2], value[3]);
    }
}

static void install_immediate_bindings(struct mgl_context *ctx,
                                       struct mgl_vertex_binding saved[MGL_COMPAT_SEMANTIC_COUNT]) {
    struct mgl_vertex_binding *b;
    unsigned unit;
    memcpy(saved, ctx->current_vao->compat, sizeof(ctx->current_vao->compat));
#define SET_BINDING(SEM, FIELD, SIZE)                                                              \
    do {                                                                                           \
        b = &ctx->current_vao->compat[(SEM)];                                                      \
        memset(b, 0, sizeof(*b));                                                                  \
        b->enabled = GL_TRUE;                                                                      \
        b->size = (SIZE);                                                                          \
        b->type = GL_FLOAT;                                                                        \
        b->stride = (GLsizei)sizeof(struct mgl_immediate_vertex);                                  \
        b->data.source = MGL_DATA_CLIENT;                                                          \
        b->data.client_ptr = &ctx->immediate.vertices[0].FIELD;                                    \
    } while (0)
    SET_BINDING(MGL_COMPAT_SEM_POSITION, position, 4);
    SET_BINDING(MGL_COMPAT_SEM_NORMAL, normal, 3);
    SET_BINDING(MGL_COMPAT_SEM_COLOR, color, 4);
    SET_BINDING(MGL_COMPAT_SEM_SECONDARY, secondary, 3);
    SET_BINDING(MGL_COMPAT_SEM_FOG, fog, 1);
    for (unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit) {
        b = &ctx->current_vao->compat[MGL_COMPAT_SEM_TEXCOORD0 + unit];
        memset(b, 0, sizeof(*b));
        b->enabled = GL_TRUE;
        b->size = 4;
        b->type = GL_FLOAT;
        b->stride = (GLsizei)sizeof(struct mgl_immediate_vertex);
        b->data.source = MGL_DATA_CLIENT;
        b->data.client_ptr = &ctx->immediate.vertices[0].texcoord[unit][0];
    }
#undef SET_BINDING
}

static uint32_t *build_legacy_indices(GLenum mode, size_t count, GLsizei *out_count) {
    uint32_t *indices;
    size_t triangles;
    size_t q;
    size_t cursor = 0u;
    if (mode == GL_QUADS)
        triangles = (count / 4u) * 2u;
    else if (mode == GL_QUAD_STRIP)
        triangles = count >= 4u ? ((count / 2u) - 1u) * 2u : 0u;
    else if (mode == GL_POLYGON)
        triangles = count >= 3u ? count - 2u : 0u;
    else
        return NULL;
    if (triangles == 0u || triangles > (size_t)INT32_MAX / 3u) {
        *out_count = 0;
        return NULL;
    }
    indices = malloc(triangles * 3u * sizeof(*indices));
    if (indices == NULL) {
        *out_count = -1;
        return NULL;
    }
    if (mode == GL_QUADS) {
        for (q = 0u; q + 3u < count; q += 4u) {
            indices[cursor++] = (uint32_t)q;
            indices[cursor++] = (uint32_t)(q + 1u);
            indices[cursor++] = (uint32_t)(q + 2u);
            indices[cursor++] = (uint32_t)q;
            indices[cursor++] = (uint32_t)(q + 2u);
            indices[cursor++] = (uint32_t)(q + 3u);
        }
    } else if (mode == GL_QUAD_STRIP) {
        for (q = 0u; q + 3u < count; q += 2u) {
            indices[cursor++] = (uint32_t)q;
            indices[cursor++] = (uint32_t)(q + 1u);
            indices[cursor++] = (uint32_t)(q + 3u);
            indices[cursor++] = (uint32_t)q;
            indices[cursor++] = (uint32_t)(q + 3u);
            indices[cursor++] = (uint32_t)(q + 2u);
        }
    } else {
        for (q = 1u; q + 1u < count; ++q) {
            indices[cursor++] = 0u;
            indices[cursor++] = (uint32_t)q;
            indices[cursor++] = (uint32_t)(q + 1u);
        }
    }
    *out_count = (GLsizei)cursor;
    return indices;
}

void glEnd(void) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_IMMEDIATE_END);
    struct mgl_vertex_binding saved[MGL_COMPAT_SEMANTIC_COUNT];
    GLuint saved_element;
    uint32_t *indices = NULL;
    GLsizei index_count = 0;
    GLenum mode;
    if (ctx == NULL)
        return;
    if (!ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_end(ctx)) {
        ctx->immediate.active = GL_FALSE;
        ctx->immediate.count = 0u;
        return;
    }
    ctx->immediate.active = GL_FALSE;
    mode = ctx->immediate.mode;
    if (ctx->immediate.count == 0u)
        return;
    install_immediate_bindings(ctx, saved);
    saved_element = ctx->current_vao->element_array_buffer;
    if (mode == GL_POLYGON) {

        if (ctx->immediate.count > (size_t)INT32_MAX)
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        else if (ctx->immediate.count >= 3u)
            glDrawArrays(GL_TRIANGLE_FAN, 0, (GLsizei)ctx->immediate.count);
    } else if (mode == GL_QUADS || mode == GL_QUAD_STRIP) {
        indices = build_legacy_indices(mode, ctx->immediate.count, &index_count);
        if (index_count < 0)
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        else if (index_count > 0) {
            ctx->current_vao->element_array_buffer = 0u;
            glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, indices);
            mgl_restore_element_array_binding(ctx, saved_element);
        }
    } else if (ctx->immediate.count <= (size_t)INT32_MAX) {
        glDrawArrays(mode, 0, (GLsizei)ctx->immediate.count);
    } else {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
    }
    ctx->current_vao->element_array_buffer = saved_element;
    memcpy(ctx->current_vao->compat, saved, sizeof(saved));
    free(indices);
}

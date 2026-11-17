#include "compat/selection.h"
#include "mgl/gl.h"

#include "compat/fixed_function.h"
#include "core/context.h"
#include "list/display_list.h"
#include "objects/object_store.h"
#include "shader/translator.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct select_vertex {
    GLfloat x, y, z, w;
    GLfloat color[4];
    GLfloat texcoord[4];
};

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
            return 4u;
        default:
            return 0u;
    }
}

static GLfloat half_as_float(uint16_t bits) {
    unsigned exponent = (bits >> 10u) & 31u;
    unsigned fraction = bits & 1023u;
    GLfloat sign = (bits & 0x8000u) ? -1.0f : 1.0f;
    if (exponent == 0u)
        return sign * ldexpf((GLfloat)fraction, -24);
    if (exponent == 31u)
        return fraction == 0u ? sign * INFINITY : NAN;
    return sign * ldexpf((GLfloat)(1024u + fraction), (int)exponent - 25);
}

static GLfloat scalar_as_float(const unsigned char *p, GLenum type) {
    switch (type) {
        case GL_BYTE: {
            GLbyte v;
            memcpy(&v, p, sizeof(v));
            return (GLfloat)v;
        }
        case GL_UNSIGNED_BYTE: {
            GLubyte v;
            memcpy(&v, p, sizeof(v));
            return (GLfloat)v;
        }
        case GL_SHORT: {
            GLshort v;
            memcpy(&v, p, sizeof(v));
            return (GLfloat)v;
        }
        case GL_UNSIGNED_SHORT: {
            GLushort v;
            memcpy(&v, p, sizeof(v));
            return (GLfloat)v;
        }
        case GL_HALF_FLOAT: {
            uint16_t v;
            memcpy(&v, p, sizeof(v));
            return half_as_float(v);
        }
        case GL_INT: {
            GLint v;
            memcpy(&v, p, sizeof(v));
            return (GLfloat)v;
        }
        case GL_UNSIGNED_INT: {
            GLuint v;
            memcpy(&v, p, sizeof(v));
            return (GLfloat)v;
        }
        case GL_FLOAT: {
            GLfloat v;
            memcpy(&v, p, sizeof(v));
            return v;
        }
        default:
            return 0.0f;
    }
}

static const unsigned char *binding_element(struct mgl_context *ctx,
                                            const struct mgl_vertex_binding *binding,
                                            uint32_t index, size_t element_size) {
    size_t stride = binding->stride != 0 ? (size_t)binding->stride : element_size;
    size_t offset;
    if (index != 0u && stride > SIZE_MAX / (size_t)index)
        return NULL;
    offset = stride * (size_t)index;
    if (binding->data.source == MGL_DATA_CLIENT) {
        return binding->data.client_ptr != NULL
                   ? (const unsigned char *)binding->data.client_ptr + offset
                   : NULL;
    }
    {
        struct mgl_buffer *buffer = mgl_buffer_find(ctx, binding->data.buffer_name);
        size_t absolute;
        if (binding->data.offset > SIZE_MAX - offset)
            return NULL;
        absolute = binding->data.offset + offset;
        if (buffer == NULL || buffer->mapped || absolute > buffer->size ||
            element_size > buffer->size - absolute)
            return NULL;
        return mgl_buffer_shadow_ensure(ctx, buffer) ? buffer->shadow + absolute : NULL;
    }
}

static GLfloat attribute_scalar(const unsigned char *p, GLenum type, GLboolean normalized) {
    GLfloat value = scalar_as_float(p, type);
    if (!normalized)
        return value;
    switch (type) {
        case GL_BYTE:
            return fmaxf(value / 127.0f, -1.0f);
        case GL_UNSIGNED_BYTE:
            return value / 255.0f;
        case GL_SHORT:
            return fmaxf(value / 32767.0f, -1.0f);
        case GL_UNSIGNED_SHORT:
            return value / 65535.0f;
        case GL_INT:
            return fmaxf(value / 2147483647.0f, -1.0f);
        case GL_UNSIGNED_INT:
            return value / 4294967295.0f;
        default:
            return value;
    }
}

static void feedback_attribute(struct mgl_context *ctx, unsigned semantic, uint32_t index,
                               GLfloat value[4]) {
    const struct mgl_vertex_binding *binding = &ctx->current_vao->compat[semantic];
    const unsigned char *src;
    size_t scalar, size;
    GLint i;
    memcpy(value, ctx->compat_current[semantic], 4u * sizeof(GLfloat));
    if (!binding->enabled || binding->size < 1 || binding->size > 4)
        return;
    scalar = scalar_size(binding->type);
    if (scalar == 0u)
        return;
    size = (size_t)binding->size * scalar;
    src = binding_element(ctx, binding, index, size);
    if (src == NULL)
        return;
    value[0] = value[1] = value[2] = 0.0f;
    value[3] = 1.0f;
    for (i = 0; i < binding->size; ++i)
        value[i] = attribute_scalar(src + (size_t)i * scalar, binding->type, binding->normalized);
}

static void feedback_attributes(struct mgl_context *ctx, uint32_t index,
                                struct select_vertex *out) {
    const GLfloat *matrix = mgl_compat_texture_matrix(&ctx->compat, 0u);
    GLfloat source[4];
    unsigned i;
    feedback_attribute(ctx, MGL_COMPAT_SEM_COLOR, index, out->color);
    feedback_attribute(ctx, MGL_COMPAT_SEM_TEXCOORD0, index, source);
    for (i = 0u; i < 4u; ++i)
        out->texcoord[i] = matrix[i] * source[0] + matrix[4u + i] * source[1] +
                           matrix[8u + i] * source[2] + matrix[12u + i] * source[3];
}

static int transformed_position(struct mgl_context *ctx, uint32_t index,
                                struct select_vertex *out) {
    const struct mgl_vertex_binding *binding = &ctx->current_vao->compat[MGL_COMPAT_SEM_POSITION];
    size_t scalar = scalar_size(binding->type);
    size_t element_size;
    const unsigned char *src;
    GLfloat v[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    GLfloat mvp[16];
    GLfloat clip[4];
    GLint c;
    if (!binding->enabled || binding->size < 2 || binding->size > 4 || scalar == 0u)
        return 0;
    if ((size_t)binding->size > SIZE_MAX / scalar)
        return 0;
    element_size = (size_t)binding->size * scalar;
    src = binding_element(ctx, binding, index, element_size);
    if (src == NULL)
        return 0;
    for (c = 0; c < binding->size; ++c)
        v[c] = scalar_as_float(src + (size_t)c * scalar, binding->type);
    mgl_compat_mvp(&ctx->compat, mvp);
    clip[0] = mvp[0] * v[0] + mvp[4] * v[1] + mvp[8] * v[2] + mvp[12] * v[3];
    clip[1] = mvp[1] * v[0] + mvp[5] * v[1] + mvp[9] * v[2] + mvp[13] * v[3];
    clip[2] = mvp[2] * v[0] + mvp[6] * v[1] + mvp[10] * v[2] + mvp[14] * v[3];
    clip[3] = mvp[3] * v[0] + mvp[7] * v[1] + mvp[11] * v[2] + mvp[15] * v[3];
    if (fabsf(clip[3]) < 1.0e-20f)
        return 0;
    out->x = clip[0] / clip[3];
    out->y = clip[1] / clip[3];
    out->z = clip[2] / clip[3];
    out->w = clip[3];
    if (ctx->selection.render_mode == GL_FEEDBACK)
        feedback_attributes(ctx, index, out);
    return 1;
}

static int point_inside(const struct select_vertex *v) {
    return v->w > 0.0f && v->x >= -1.0f && v->x <= 1.0f && v->y >= -1.0f && v->y <= 1.0f &&
           v->z >= -1.0f && v->z <= 1.0f;
}

static int segment_intersects(const struct select_vertex *a, const struct select_vertex *b) {
    GLfloat t0 = 0.0f, t1 = 1.0f;
    const GLfloat av[3] = {a->x, a->y, a->z};
    const GLfloat bv[3] = {b->x, b->y, b->z};
    unsigned axis;
    for (axis = 0u; axis < 3u; ++axis) {
        GLfloat d = bv[axis] - av[axis];
        if (fabsf(d) < 1.0e-20f) {
            if (av[axis] < -1.0f || av[axis] > 1.0f)
                return 0;
        } else {
            GLfloat lo = (-1.0f - av[axis]) / d;
            GLfloat hi = (1.0f - av[axis]) / d;
            if (lo > hi) {
                GLfloat tmp = lo;
                lo = hi;
                hi = tmp;
            }
            if (lo > t0)
                t0 = lo;
            if (hi < t1)
                t1 = hi;
            if (t0 > t1)
                return 0;
        }
    }
    return 1;
}

static GLfloat sign2(GLfloat px, GLfloat py, const struct select_vertex *a,
                     const struct select_vertex *b) {
    return (px - b->x) * (a->y - b->y) - (a->x - b->x) * (py - b->y);
}

static int point_in_triangle(GLfloat x, GLfloat y, const struct select_vertex *a,
                             const struct select_vertex *b, const struct select_vertex *c) {
    GLboolean n1 = sign2(x, y, a, b) < 0.0f;
    GLboolean n2 = sign2(x, y, b, c) < 0.0f;
    GLboolean n3 = sign2(x, y, c, a) < 0.0f;
    return n1 == n2 && n2 == n3;
}

static int triangle_intersects(const struct select_vertex *a, const struct select_vertex *b,
                               const struct select_vertex *c) {
    static const GLfloat corners[4][2] = {
        {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
    unsigned i;
    GLfloat zmin = fminf(a->z, fminf(b->z, c->z));
    GLfloat zmax = fmaxf(a->z, fmaxf(b->z, c->z));
    if (zmax < -1.0f || zmin > 1.0f)
        return 0;
    if (point_inside(a) || point_inside(b) || point_inside(c))
        return 1;
    if (segment_intersects(a, b) || segment_intersects(b, c) || segment_intersects(c, a))
        return 1;
    for (i = 0u; i < 4u; ++i)
        if (point_in_triangle(corners[i][0], corners[i][1], a, b, c))
            return 1;
    return 0;
}

static GLfloat window_depth(GLfloat ndc_z) {
    GLfloat value = ndc_z * 0.5f + 0.5f;
    if (value < 0.0f)
        value = 0.0f;
    if (value > 1.0f)
        value = 1.0f;
    return value;
}

static void record_primitive_hit(struct mgl_context *ctx, const struct select_vertex *v,
                                 unsigned count) {
    GLfloat min_depth = 1.0f;
    GLfloat max_depth = 0.0f;
    unsigned i;
    for (i = 0u; i < count; ++i) {
        GLfloat d = window_depth(v[i].z);
        if (d < min_depth)
            min_depth = d;
        if (d > max_depth)
            max_depth = d;
    }
    if (!ctx->selection.pending_hit) {
        ctx->selection.pending_hit = GL_TRUE;
        ctx->selection.min_depth = min_depth;
        ctx->selection.max_depth = max_depth;
    } else {
        if (min_depth < ctx->selection.min_depth)
            ctx->selection.min_depth = min_depth;
        if (max_depth > ctx->selection.max_depth)
            ctx->selection.max_depth = max_depth;
    }
}

static GLuint depth_word(GLfloat depth) {
    double scaled;
    if (depth <= 0.0f)
        return 0u;
    if (depth >= 1.0f)
        return UINT32_MAX;
    scaled = (double)depth * 4294967295.0;
    return (GLuint)(scaled + 0.5);
}

static void flush_hit(struct mgl_context *ctx) {
    struct mgl_selection_state *s = &ctx->selection;
    size_t needed;
    unsigned i;
    if (!s->pending_hit)
        return;
    needed = 3u + (size_t)s->name_depth;
    ++s->hit_count;
    if (!s->overflow && ((size_t)s->buffer_pos > (size_t)s->buffer_size ||
                         needed > (size_t)s->buffer_size - (size_t)s->buffer_pos)) {
        s->overflow = GL_TRUE;
    } else if (!s->overflow) {
        s->buffer[s->buffer_pos++] = (GLuint)s->name_depth;
        s->buffer[s->buffer_pos++] = depth_word(s->min_depth);
        s->buffer[s->buffer_pos++] = depth_word(s->max_depth);
        for (i = 0u; i < s->name_depth; ++i)
            s->buffer[s->buffer_pos++] = s->names[i];
    }
    s->pending_hit = GL_FALSE;
    s->min_depth = 1.0f;
    s->max_depth = 0.0f;
}

void mgl_selection_init(struct mgl_selection_state *s) {
    memset(s, 0, sizeof(*s));
    s->render_mode = GL_RENDER;
    s->feedback_type = GL_2D;
    s->min_depth = 1.0f;
}

int mgl_selection_active(const struct mgl_context *ctx) {
    return ctx != NULL &&
           (ctx->selection.render_mode == GL_SELECT || ctx->selection.render_mode == GL_FEEDBACK);
}

static void feedback_append(struct mgl_selection_state *s, GLfloat value) {
    if (s->feedback_pos < s->feedback_size && s->feedback_buffer != NULL)
        s->feedback_buffer[s->feedback_pos] = value;
    else
        s->feedback_overflow = GL_TRUE;
    if (s->feedback_pos < INT32_MAX)
        ++s->feedback_pos;
    else
        s->feedback_overflow = GL_TRUE;
}

void glFeedbackBuffer(GLsizei size, GLenum type, GLfloat *buffer) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active || ctx->selection.render_mode == GL_FEEDBACK) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (type != GL_2D && type != GL_3D && type != GL_3D_COLOR && type != GL_3D_COLOR_TEXTURE &&
        type != GL_4D_COLOR_TEXTURE) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (size < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (buffer == NULL && size > 0) {
        ctx->selection.feedback_size = 0;
        ctx->selection.feedback_configured = GL_FALSE;
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    ctx->selection.feedback_buffer = buffer;
    ctx->selection.feedback_size = size;
    ctx->selection.feedback_type = type;
    ctx->selection.feedback_configured = GL_TRUE;
}

void glPassThrough(GLfloat token) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->selection.render_mode == GL_FEEDBACK) {
        feedback_append(&ctx->selection, (GLfloat)GL_PASS_THROUGH_TOKEN);
        feedback_append(&ctx->selection, token);
    }
}

void glSelectBuffer(GLsizei size, GLuint *buffer) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (size < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (ctx->selection.render_mode == GL_SELECT) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->selection.buffer = buffer;
    ctx->selection.buffer_size = size;
}

GLint glRenderMode(GLenum mode) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glRenderMode",
                        (const struct mgl_error_arg[]){{"mode", (uint64_t)(uintptr_t)mode}}, 1u);
    GLint result = 0;
    if (ctx == NULL)
        return 0;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (ctx->compiling_list != NULL && !ctx->list_replaying) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (mode != GL_RENDER && mode != GL_SELECT && mode != GL_FEEDBACK) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (mode == GL_FEEDBACK &&
        (!ctx->selection.feedback_configured || ctx->selection.feedback_size == 0)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (mode == GL_SELECT && (ctx->selection.buffer == NULL || ctx->selection.buffer_size == 0)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (ctx->selection.render_mode == GL_SELECT) {
        flush_hit(ctx);
        result = ctx->selection.overflow ? -1 : ctx->selection.hit_count;
    } else if (ctx->selection.render_mode == GL_FEEDBACK) {
        result = ctx->selection.feedback_overflow ? -1 : ctx->selection.feedback_pos;
    }
    if (mode == GL_SELECT) {
        if (ctx->selection.buffer == NULL || ctx->selection.buffer_size == 0) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        ctx->selection.buffer_pos = 0;
        ctx->selection.hit_count = 0;
        ctx->selection.overflow = GL_FALSE;
        ctx->selection.pending_hit = GL_FALSE;
        ctx->selection.min_depth = 1.0f;
        ctx->selection.max_depth = 0.0f;
    }
    if (mode == GL_FEEDBACK) {
        ctx->selection.feedback_pos = 0;
        ctx->selection.feedback_overflow = GL_FALSE;
    }
    ctx->selection.render_mode = mode;
    return result;
}

void glInitNames(void) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (mgl_list_record_init_names(ctx))
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->selection.render_mode == GL_SELECT)
        flush_hit(ctx);
    ctx->selection.name_depth = 0u;
}

void glPushName(GLuint name) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (mgl_list_record_push_name(ctx, name))
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->selection.render_mode == GL_SELECT)
        flush_hit(ctx);
    if (ctx->selection.name_depth >= MGL_NAME_STACK_MAX_DEPTH) {
        mgl_error_set(&ctx->errors, GL_STACK_OVERFLOW);
        return;
    }
    ctx->selection.names[ctx->selection.name_depth++] = name;
}

void glPopName(void) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (mgl_list_record_pop_name(ctx))
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->selection.render_mode == GL_SELECT)
        flush_hit(ctx);
    if (ctx->selection.name_depth == 0u) {
        mgl_error_set(&ctx->errors, GL_STACK_UNDERFLOW);
        return;
    }
    --ctx->selection.name_depth;
}

void glLoadName(GLuint name) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (mgl_list_record_load_name(ctx, name))
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->selection.render_mode == GL_SELECT)
        flush_hit(ctx);
    if (ctx->selection.name_depth == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->selection.names[ctx->selection.name_depth - 1u] = name;
}

static int read_index(struct mgl_context *ctx, GLenum type, const void *indices, GLsizei i,
                      uint32_t *out) {
    size_t size = type == GL_UNSIGNED_BYTE    ? 1u
                  : type == GL_UNSIGNED_SHORT ? 2u
                  : type == GL_UNSIGNED_INT   ? 4u
                                              : 0u;
    const unsigned char *src;
    size_t offset;
    if (size == 0u || i < 0)
        return 0;
    offset = (size_t)i * size;
    if (ctx->current_vao->element_array_buffer != 0u) {
        struct mgl_buffer *buffer = mgl_buffer_find(ctx, ctx->current_vao->element_array_buffer);
        size_t base = (size_t)(uintptr_t)indices;
        if (base > SIZE_MAX - offset)
            return 0;
        offset += base;
        if (buffer == NULL || buffer->mapped || offset > buffer->size ||
            size > buffer->size - offset)
            return 0;
        if (!mgl_buffer_shadow_ensure(ctx, buffer))
            return 0;
        src = buffer->shadow + offset;
    } else {
        if (indices == NULL)
            return 0;
        src = (const unsigned char *)indices + offset;
    }
    if (type == GL_UNSIGNED_BYTE) {
        *out = src[0];
        return 1;
    }
    if (type == GL_UNSIGNED_SHORT) {
        GLushort v;
        memcpy(&v, src, sizeof(v));
        *out = v;
        return 1;
    }
    {
        GLuint v;
        memcpy(&v, src, sizeof(v));
        *out = v;
        return 1;
    }
}

static int primitive_hit(GLenum mode, const struct select_vertex *v, unsigned n) {
    if (mode == GL_POINTS)
        return n == 1u && point_inside(&v[0]);
    if (mode == GL_LINES || mode == GL_LINE_STRIP || mode == GL_LINE_LOOP)
        return n == 2u && segment_intersects(&v[0], &v[1]);
    return n == 3u && triangle_intersects(&v[0], &v[1], &v[2]);
}

static void feedback_vertex(struct mgl_context *ctx, const struct select_vertex *v) {
    const struct mgl_raster_state *r = &ctx->canonical_state;
    GLfloat x = (v->x * 0.5f + 0.5f) * (GLfloat)r->viewport[2] + (GLfloat)r->viewport[0];
    GLfloat y = (v->y * 0.5f + 0.5f) * (GLfloat)r->viewport[3] + (GLfloat)r->viewport[1];
    GLfloat z = (v->z * 0.5f + 0.5f) * (r->depth_range[1] - r->depth_range[0]) + r->depth_range[0];
    feedback_append(&ctx->selection, x);
    feedback_append(&ctx->selection, y);
    if (ctx->selection.feedback_type != GL_2D)
        feedback_append(&ctx->selection, z);
    if (ctx->selection.feedback_type == GL_3D_COLOR ||
        ctx->selection.feedback_type == GL_3D_COLOR_TEXTURE ||
        ctx->selection.feedback_type == GL_4D_COLOR_TEXTURE) {
        unsigned i;
        if (ctx->selection.feedback_type == GL_4D_COLOR_TEXTURE)
            feedback_append(&ctx->selection, v->w);
        for (i = 0; i < 4u; ++i)
            feedback_append(&ctx->selection, v->color[i]);
        if (ctx->selection.feedback_type == GL_3D_COLOR_TEXTURE ||
            ctx->selection.feedback_type == GL_4D_COLOR_TEXTURE)
            for (i = 0; i < 4u; ++i)
                feedback_append(&ctx->selection, v->texcoord[i]);
    }
}

static GLfloat clip_plane_distance(const struct select_vertex *v, unsigned plane) {
    const GLfloat coordinate = plane < 2u ? v->x : plane < 4u ? v->y : v->z;
    return v->w * (1.0f + ((plane & 1u) ? -coordinate : coordinate));
}

static struct select_vertex interpolate_clip(const struct select_vertex *a,
                                             const struct select_vertex *b, GLfloat t) {
    struct select_vertex out;
    GLfloat w = a->w + (b->w - a->w) * t;
    GLfloat aclip[3] = {a->x * a->w, a->y * a->w, a->z * a->w};
    GLfloat bclip[3] = {b->x * b->w, b->y * b->w, b->z * b->w};
    GLfloat *coordinate[3] = {&out.x, &out.y, &out.z};
    unsigned i;
    out.w = w;
    for (i = 0u; i < 3u; ++i)
        *coordinate[i] = w != 0.0f ? (aclip[i] + (bclip[i] - aclip[i]) * t) / w : 0.0f;
    for (i = 0u; i < 4u; ++i) {
        out.color[i] = a->color[i] + (b->color[i] - a->color[i]) * t;
        out.texcoord[i] = a->texcoord[i] + (b->texcoord[i] - a->texcoord[i]) * t;
    }
    return out;
}

static unsigned clip_feedback_polygon(const struct select_vertex *input, unsigned count,
                                      struct select_vertex output[10]) {
    struct select_vertex a[10], b[10];
    unsigned plane, i;
    memcpy(a, input, count * sizeof(*a));
    for (plane = 0u; plane < 6u; ++plane) {
        unsigned next = 0u;
        for (i = 0u; i < count; ++i) {
            const struct select_vertex *previous = &a[(i + count - 1u) % count];
            const struct select_vertex *current = &a[i];
            GLfloat d0 = clip_plane_distance(previous, plane);
            GLfloat d1 = clip_plane_distance(current, plane);
            int was_inside = d0 >= 0.0f;
            int is_inside = d1 >= 0.0f;
            if (was_inside != is_inside && next < 10u)
                b[next++] = interpolate_clip(previous, current, d0 / (d0 - d1));
            if (is_inside && next < 10u)
                b[next++] = *current;
        }
        count = next;
        if (count == 0u)
            return 0u;
        memcpy(a, b, count * sizeof(*a));
    }
    memcpy(output, a, count * sizeof(*a));
    return count;
}

static int clip_feedback_line(const struct select_vertex *input, struct select_vertex output[2]) {
    GLfloat start = 0.0f, end = 1.0f;
    unsigned plane;
    for (plane = 0u; plane < 6u; ++plane) {
        GLfloat d0 = clip_plane_distance(&input[0], plane);
        GLfloat d1 = clip_plane_distance(&input[1], plane);
        if (d0 < 0.0f && d1 < 0.0f)
            return 0;
        if (d0 < 0.0f || d1 < 0.0f) {
            GLfloat t = d0 / (d0 - d1);
            if (d0 < 0.0f && t > start)
                start = t;
            if (d1 < 0.0f && t < end)
                end = t;
            if (start > end)
                return 0;
        }
    }
    output[0] = interpolate_clip(&input[0], &input[1], start);
    output[1] = interpolate_clip(&input[0], &input[1], end);
    return 1;
}

static void feedback_primitive(struct mgl_context *ctx, GLenum mode, const struct select_vertex *v,
                               unsigned n) {
    struct select_vertex clipped[10];
    unsigned count, i;
    if (n == 1u) {
        if (!point_inside(v))
            return;
        count = 1u;
        clipped[0] = v[0];
    } else if (n == 2u) {
        if (!clip_feedback_line(v, clipped))
            return;
        count = 2u;
    } else {
        count = clip_feedback_polygon(v, n, clipped);
        if (count < 3u)
            return;
    }
    if (n == 3u) {
        for (i = 1u; i + 1u < count; ++i) {
            feedback_append(&ctx->selection, (GLfloat)GL_POLYGON_TOKEN);
            feedback_append(&ctx->selection, 3.0f);
            feedback_vertex(ctx, &clipped[0]);
            feedback_vertex(ctx, &clipped[i]);
            feedback_vertex(ctx, &clipped[i + 1u]);
        }
        return;
    }
    feedback_append(&ctx->selection, (GLfloat)(n == 1u            ? GL_POINT_TOKEN
                                               : mode == GL_LINES ? GL_LINE_RESET_TOKEN
                                                                  : GL_LINE_TOKEN));
    for (i = 0u; i < count; ++i)
        feedback_vertex(ctx, &clipped[i]);
}

static void test_primitive(struct mgl_context *ctx, GLenum mode, const uint32_t *idx, unsigned n) {
    struct select_vertex v[3];
    unsigned i;
    for (i = 0u; i < n; ++i)
        if (!transformed_position(ctx, idx[i], &v[i]))
            return;
    if (ctx->selection.render_mode == GL_FEEDBACK) {
        feedback_primitive(ctx, mode, v, n);
    } else if (primitive_hit(mode, v, n))
        record_primitive_hit(ctx, v, n);
}

static void process_indices(struct mgl_context *ctx, GLenum mode, const uint32_t *indices,
                            GLsizei count) {
    GLsizei i;
    if (mode == GL_POINTS) {
        for (i = 0; i < count; ++i)
            test_primitive(ctx, mode, &indices[i], 1u);
    } else if (mode == GL_LINES) {
        for (i = 1; i < count; i += 2)
            test_primitive(ctx, mode, &indices[i - 1], 2u);
    } else if (mode == GL_LINE_STRIP || mode == GL_LINE_LOOP) {
        for (i = 1; i < count; ++i)
            test_primitive(ctx, mode, &indices[i - 1], 2u);
        if (mode == GL_LINE_LOOP && count > 1) {
            uint32_t pair[2] = {indices[count - 1], indices[0]};
            test_primitive(ctx, mode, pair, 2u);
        }
    } else if (mode == GL_TRIANGLES) {
        for (i = 2; i < count; i += 3)
            test_primitive(ctx, mode, &indices[i - 2], 3u);
    } else if (mode == GL_TRIANGLE_STRIP) {
        for (i = 2; i < count; ++i)
            test_primitive(ctx, mode, &indices[i - 2], 3u);
    } else if (mode == GL_TRIANGLE_FAN) {
        for (i = 2; i < count; ++i) {
            uint32_t tri[3] = {indices[0], indices[i - 1], indices[i]};
            test_primitive(ctx, mode, tri, 3u);
        }
    }
}

void mgl_selection_draw_arrays(struct mgl_context *ctx, GLenum mode, GLint first, GLsizei count) {
    uint32_t *indices;
    GLsizei i;
    if (ctx == NULL || count <= 0)
        return;
    if ((size_t)count > SIZE_MAX / sizeof(*indices)) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    indices = malloc((size_t)count * sizeof(*indices));
    if (indices == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    for (i = 0; i < count; ++i)
        indices[i] = (uint32_t)(first + i);
    process_indices(ctx, mode, indices, count);
    free(indices);
}

void mgl_selection_draw_elements(struct mgl_context *ctx, GLenum mode, GLsizei count, GLenum type,
                                 const void *indices) {
    uint32_t *values;
    GLsizei i;
    if (ctx == NULL || count <= 0)
        return;
    if ((size_t)count > SIZE_MAX / sizeof(*values)) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    values = malloc((size_t)count * sizeof(*values));
    if (values == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    for (i = 0; i < count; ++i) {
        if (!read_index(ctx, type, indices, i, &values[i])) {
            free(values);
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
    }
    process_indices(ctx, mode, values, count);
    free(values);
}

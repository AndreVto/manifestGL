#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdint.h>
#include <string.h>

void glEdgeFlag(GLboolean flag) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx != NULL)
        ctx->current_edge_flag = flag ? GL_TRUE : GL_FALSE;
}

void glEdgeFlagv(const GLboolean *flag) {
    if (flag != NULL)
        glEdgeFlag(*flag);
}

void glLineStipple(GLint factor, GLushort pattern) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (factor < 1)
        factor = 1;
    if (factor > 256)
        factor = 256;
    ctx->canonical_state.line_stipple_factor = factor;
    ctx->canonical_state.line_stipple_pattern = pattern;
}

static int stipple_row_stride(const struct mgl_context *ctx, int pack, size_t *stride) {
    GLint length = pack ? ctx->pack_row_length : ctx->unpack_row_length;
    GLint alignment = pack ? ctx->pack_alignment : ctx->unpack_alignment;
    size_t bytes;
    if (length == 0)
        length = 32;
    if (length < 0 || alignment < 1)
        return 0;
    bytes = ((size_t)length + 7u) / 8u;
    *stride = (bytes + (size_t)alignment - 1u) & ~((size_t)alignment - 1u);
    return 1;
}

static int stipple_buffer(struct mgl_context *ctx, int pack, const void *pointer, size_t row_stride,
                          const unsigned char **out, size_t *start) {
    GLuint name = pack ? ctx->pixel_pack_buffer : ctx->pixel_unpack_buffer;
    struct mgl_buffer *buffer;
    GLint skip_rows = pack ? ctx->pack_skip_rows : ctx->unpack_skip_rows;
    GLint skip_pixels = pack ? ctx->pack_skip_pixels : ctx->unpack_skip_pixels;
    size_t offset;
    size_t size;
    size_t end_bit;
    if (skip_rows < 0 || skip_pixels < 0 || row_stride > SIZE_MAX / 32u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if ((size_t)skip_rows > SIZE_MAX / row_stride) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    offset = (size_t)skip_rows * row_stride;
    if (offset > SIZE_MAX - 31u * row_stride) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    size = offset + 31u * row_stride;
    if ((size_t)skip_pixels > SIZE_MAX - 31u ||
        size > SIZE_MAX - (((size_t)skip_pixels + 31u) / 8u + 1u)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    end_bit = (size_t)skip_pixels + 31u;
    size += end_bit / 8u + 1u;
    if (name == 0u) {
        if (pointer == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return 0;
        }
        *out = (const unsigned char *)pointer;
        *start = 0u;
        return 1;
    }
    buffer = mgl_buffer_find(ctx, name);
    if (buffer == NULL || buffer->mapped) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    offset = (size_t)(uintptr_t)pointer;
    if (offset > buffer->size || size > buffer->size - offset) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (!mgl_buffer_shadow_ensure(ctx, buffer))
        return 0;
    *out = buffer->shadow;
    *start = offset;
    return 1;
}

static size_t stipple_bit_offset(const struct mgl_context *ctx, int pack, size_t stride, unsigned x,
                                 unsigned y) {
    size_t skip_rows = (size_t)(pack ? ctx->pack_skip_rows : ctx->unpack_skip_rows);
    size_t skip_pixels = (size_t)(pack ? ctx->pack_skip_pixels : ctx->unpack_skip_pixels);
    return (skip_rows + (size_t)y) * stride * 8u + skip_pixels + (size_t)x;
}

void glPolygonStipple(const GLubyte *mask) {
    struct mgl_context *ctx = mgl_context_current();
    const unsigned char *src;
    size_t stride, start;
    unsigned x, y;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!stipple_row_stride(ctx, 0, &stride) || !stipple_buffer(ctx, 0, mask, stride, &src, &start))
        return;
    for (y = 0u; y < 32u; ++y) {
        for (x = 0u; x < 32u; ++x) {
            size_t bit = stipple_bit_offset(ctx, 0, stride, x, y);
            unsigned shift =
                ctx->unpack_lsb_first ? (unsigned)(bit & 7u) : 7u - (unsigned)(bit & 7u);
            unsigned char v = (src[start + bit / 8u] >> shift) & 1u;
            unsigned char *dst = &ctx->canonical_state.polygon_stipple_mask[y * 4u + x / 8u];
            *dst = (unsigned char)((*dst & ~(1u << (x & 7u))) | (v << (x & 7u)));
        }
    }
}

static void get_polygon_stipple(GLubyte *mask, GLsizei bufSize, int bounded) {
    struct mgl_context *ctx = mgl_context_current();
    const unsigned char *data;
    unsigned char *dst;
    size_t stride, start;
    unsigned x, y;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (bounded && bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!stipple_row_stride(ctx, 1, &stride))
        return;
    if (bounded && ctx->pixel_pack_buffer == 0u) {
        size_t last_bit = stipple_bit_offset(ctx, 1, stride, 31u, 31u);
        if (last_bit / 8u + 1u > (size_t)bufSize) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
    }
    if (!stipple_buffer(ctx, 1, mask, stride, &data, &start))
        return;
    dst = (unsigned char *)data;
    for (y = 0u; y < 32u; ++y) {
        for (x = 0u; x < 32u; ++x) {
            size_t bit = stipple_bit_offset(ctx, 1, stride, x, y);
            unsigned shift = ctx->pack_lsb_first ? (unsigned)(bit & 7u) : 7u - (unsigned)(bit & 7u);
            unsigned char v =
                (ctx->canonical_state.polygon_stipple_mask[y * 4u + x / 8u] >> (x & 7u)) & 1u;
            unsigned char *byte = &dst[start + bit / 8u];
            *byte = (unsigned char)((*byte & ~(1u << shift)) | (v << shift));
        }
    }
    if (ctx->pixel_pack_buffer != 0u) {
        struct mgl_buffer *buffer = mgl_buffer_find(ctx, ctx->pixel_pack_buffer);
        if (buffer != NULL && buffer->native_name != 0u && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesBufferSubData != NULL) {
            for (y = 0u; y < 32u; ++y) {
                size_t bit = stipple_bit_offset(ctx, 1, stride, 0u, y);
                size_t end = stipple_bit_offset(ctx, 1, stride, 31u, y);
                size_t off = start + bit / 8u;
                size_t length = end / 8u - bit / 8u + 1u;
                unsigned char row[8];
                if (length > sizeof(row)) {
                    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                    return;
                }
                memcpy(row, dst + off, length);
                glBufferSubData(GL_PIXEL_PACK_BUFFER, (GLintptr)off, (GLsizeiptr)length, row);
            }
        }
    }
}

void glGetPolygonStipple(GLubyte *mask) {
    get_polygon_stipple(mask, 0, 0);
}

void glGetnPolygonStipple(GLsizei bufSize, GLubyte *pattern) {
    get_polygon_stipple(pattern, bufSize, 1);
}

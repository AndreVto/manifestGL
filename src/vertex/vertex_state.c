#include "mgl/gl.h"

#include "core/context.h"
#include "objects/object_store.h"
#include "shader/translator.h"

#include <stdint.h>
#include <string.h>
#include <limits.h>

static int valid_array_type(GLenum type) {
    switch (type) {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_FLOAT:
        case GL_HALF_FLOAT:
            return 1;
        default:
            return 0;
    }
}

static int packed_generic_array_type(GLenum type) {
    return type == GL_INT_2_10_10_10_REV || type == GL_UNSIGNED_INT_2_10_10_10_REV;
}

static int valid_generic_array_type(GLenum type) {
    return valid_array_type(type) || type == GL_FIXED || packed_generic_array_type(type);
}

static size_t generic_array_element_size(GLenum type, GLint size) {
    if (packed_generic_array_type(type))
        return sizeof(GLuint);
    return (size_t)size * (type == GL_BYTE || type == GL_UNSIGNED_BYTE ? 1u
                             : type == GL_SHORT || type == GL_UNSIGNED_SHORT ||
                                       type == GL_HALF_FLOAT ? 2u : 4u);
}

static int valid_integer_array_type(GLenum type) {
    switch (type) {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_INT:
        case GL_UNSIGNED_INT:
            return 1;
        default:
            return 0;
    }
}

static int valid_generic_index(struct mgl_context *ctx, GLuint index) {
    GLint max_attribs = 0;
    if (index >= MGL_MAX_VERTEX_ATTRIBS)
        return 0;
    if (ctx->native_max_vertex_attribs <= 0 && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesGetIntegerv != NULL) {
        ctx->native_dispatch->glesGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_attribs);
        if (max_attribs <= 0 || max_attribs > (GLint)MGL_MAX_VERTEX_ATTRIBS)
            max_attribs = (GLint)MGL_MAX_VERTEX_ATTRIBS;
        ctx->native_max_vertex_attribs = max_attribs;
    }
    if (ctx->native_max_vertex_attribs <= 0)
        ctx->native_max_vertex_attribs = (GLint)MGL_MAX_VERTEX_ATTRIBS;
    return index < (GLuint)ctx->native_max_vertex_attribs;
}

/* Validate named-VAO buffer ownership before modifying array bindings. */
static int named_vao_requires_array_buffer(struct mgl_context *ctx, const void *pointer) {
    if (pointer != NULL && ctx->current_vao != &ctx->default_vao && ctx->array_buffer == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 1;
    }
    return 0;
}

static void assign_data_ref(struct mgl_context *ctx, struct mgl_vertex_binding *binding,
                            const void *pointer) {
    if (ctx->array_buffer != 0u) {
        binding->data.source = MGL_DATA_BUFFER;
        binding->data.buffer_name = ctx->array_buffer;
        binding->data.offset = (size_t)(uintptr_t)pointer;
        binding->data.client_ptr = NULL;
    } else {
        binding->data.source = MGL_DATA_CLIENT;
        binding->data.client_ptr = pointer;
        binding->data.buffer_name = 0u;
        binding->data.offset = 0u;
    }
}

void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                           GLsizei stride, const void *pointer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_binding *binding;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (size < 1 || size > 4) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!valid_generic_array_type(type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (packed_generic_array_type(type) && size != 4) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (stride < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (named_vao_requires_array_buffer(ctx, pointer))
        return;
    binding = &ctx->current_vao->generic[index];
    binding->size = size;
    binding->type = type;
    binding->normalized = normalized ? GL_TRUE : GL_FALSE;
    binding->integer = GL_FALSE;
    binding->stride = stride;
    binding->attrib_array_stride = stride;
    assign_data_ref(ctx, binding, pointer);
    ctx->current_vao->vertex_attrib_binding[index] = index;
    ctx->current_vao->vertex_attrib_relative_offset[index] = 0u;
    binding->separate_stride = GL_FALSE;
    ctx->current_vao->vertex_buffers[index].data = binding->data;
    ctx->current_vao->vertex_buffers[index].stride =
        stride != 0 ? stride : (GLsizei)generic_array_element_size(type, size);
    mgl_vertex_array_resolve_slot(ctx->current_vao, index);
}

void glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride,
                            const void *pointer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_binding *binding;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (size < 1 || size > 4) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!valid_integer_array_type(type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (stride < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (named_vao_requires_array_buffer(ctx, pointer))
        return;
    binding = &ctx->current_vao->generic[index];
    binding->size = size;
    binding->type = type;
    binding->normalized = GL_FALSE;
    binding->integer = GL_TRUE;
    binding->stride = stride;
    binding->attrib_array_stride = stride;
    assign_data_ref(ctx, binding, pointer);
    ctx->current_vao->vertex_attrib_binding[index] = index;
    ctx->current_vao->vertex_attrib_relative_offset[index] = 0u;
    binding->separate_stride = GL_FALSE;
    ctx->current_vao->vertex_buffers[index].data = binding->data;
    ctx->current_vao->vertex_buffers[index].stride =
        stride != 0
            ? stride
            : (GLsizei)((size_t)size * (type == GL_HALF_FLOAT                           ? 2u
                                        : type == GL_BYTE || type == GL_UNSIGNED_BYTE   ? 1u
                                        : type == GL_SHORT || type == GL_UNSIGNED_SHORT ? 2u
                                                                                        : 4u));
    mgl_vertex_array_resolve_slot(ctx->current_vao, index);
}

void glEnableVertexAttribArray(GLuint index) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    ctx->current_vao->generic[index].enabled = GL_TRUE;
}

void glDisableVertexAttribArray(GLuint index) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    ctx->current_vao->generic[index].enabled = GL_FALSE;
}

void glVertexAttribDivisor(GLuint index, GLuint divisor) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    ctx->current_vao->vertex_attrib_binding[index] = index;
    mgl_vertex_array_resolve(ctx->current_vao, index);
    ctx->current_vao->vertex_buffers[index].divisor = divisor;
    mgl_vertex_array_resolve_slot(ctx->current_vao, index);
}

static void set_current_float(struct mgl_context *ctx, GLuint index, GLint components,
                              const GLfloat *v) {
    GLint i;
    GLfloat values[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    for (i = 0; i < components; ++i)
        values[i] = v[i];
    for (i = 0; i < 4; ++i)
        ctx->generic_current[index][i] = values[i];
    ctx->generic_current_type[index] = GL_FLOAT;
}

static void set_current_int(struct mgl_context *ctx, GLuint index, GLint components,
                            const GLint *v) {
    GLint i;
    GLint values[4] = {0, 0, 0, 1};
    for (i = 0; i < components; ++i)
        values[i] = v[i];
    for (i = 0; i < 4; ++i) {
        ctx->generic_current_i[index][i] = values[i];
        ctx->generic_current[index][i] = (GLfloat)values[i];
    }
    ctx->generic_current_type[index] = GL_INT;
}

static void set_current_uint(struct mgl_context *ctx, GLuint index, GLint components,
                             const GLuint *v) {
    GLint i;
    GLuint values[4] = {0u, 0u, 0u, 1u};
    for (i = 0; i < components; ++i)
        values[i] = v[i];
    for (i = 0; i < 4; ++i) {
        ctx->generic_current_ui[index][i] = values[i];
        ctx->generic_current[index][i] = (GLfloat)values[i];
    }
    ctx->generic_current_type[index] = GL_UNSIGNED_INT;
}

void glVertexAttrib1f(GLuint index, GLfloat x) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat v[1] = {x};
    if (ctx == NULL)
        return;
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    set_current_float(ctx, index, 1, v);
}

void glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat v[2] = {x, y};
    if (ctx == NULL)
        return;
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    set_current_float(ctx, index, 2, v);
}

void glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat v[3] = {x, y, z};
    if (ctx == NULL)
        return;
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    set_current_float(ctx, index, 3, v);
}

void glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat v[4] = {x, y, z, w};
    if (ctx == NULL)
        return;
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    set_current_float(ctx, index, 4, v);
}

static GLfloat half_to_float(GLushort h) {
    uint32_t sign = ((uint32_t)h & 0x8000u) << 16;
    uint32_t exponent = ((uint32_t)h >> 10) & 0x1fu;
    uint32_t fraction = (uint32_t)h & 0x03ffu;
    uint32_t bits;
    GLfloat value;
    if (exponent == 0u) {
        if (fraction == 0u)
            bits = sign;
        else {
            int e = -14;
            while ((fraction & 0x400u) == 0u) {
                fraction <<= 1;
                --e;
            }
            bits = sign | ((uint32_t)(e + 127) << 23) | ((fraction & 0x3ffu) << 13);
        }
    } else if (exponent == 31u) {
        bits = sign | 0x7f800000u | (fraction << 13);
    } else {
        bits = sign | ((exponent + 112u) << 23) | (fraction << 13);
    }
    memcpy(&value, &bits, sizeof(value));
    return value;
}

void glVertexAttrib2hvNV(GLuint index, const GLushort *v) {
    if (v != NULL)
        glVertexAttrib2f(index, half_to_float(v[0]), half_to_float(v[1]));
}

void glVertexAttrib4hvNV(GLuint index, const GLushort *v) {
    if (v != NULL)
        glVertexAttrib4f(index, half_to_float(v[0]), half_to_float(v[1]), half_to_float(v[2]),
                         half_to_float(v[3]));
}

void glColor3hNV(GLushort x, GLushort y, GLushort z) {
    glColor3f(half_to_float(x), half_to_float(y), half_to_float(z));
}

void glColor3hvNV(const GLushort *v) {
    if (v != NULL)
        glColor3hNV(v[0], v[1], v[2]);
}

void glColor4hNV(GLushort x, GLushort y, GLushort z, GLushort w) {
    glColor4f(half_to_float(x), half_to_float(y), half_to_float(z), half_to_float(w));
}

void glColor4hvNV(const GLushort *v) {
    if (v != NULL)
        glColor4hNV(v[0], v[1], v[2], v[3]);
}

void glNormal3hNV(GLushort x, GLushort y, GLushort z) {
    glNormal3f(half_to_float(x), half_to_float(y), half_to_float(z));
}

void glNormal3hvNV(const GLushort *v) {
    if (v != NULL)
        glNormal3hNV(v[0], v[1], v[2]);
}

void glSecondaryColor3hNV(GLushort x, GLushort y, GLushort z) {
    glSecondaryColor3f(half_to_float(x), half_to_float(y), half_to_float(z));
}

void glSecondaryColor3hvNV(const GLushort *v) {
    if (v != NULL)
        glSecondaryColor3hNV(v[0], v[1], v[2]);
}

void glFogCoordhNV(GLushort x) {
    glFogCoordf(half_to_float(x));
}

void glFogCoordhvNV(const GLushort *v) {
    if (v != NULL)
        glFogCoordhNV(v[0]);
}

void glTexCoord1hNV(GLushort x) {
    glTexCoord1f(half_to_float(x));
}

void glTexCoord1hvNV(const GLushort *v) {
    if (v != NULL)
        glTexCoord1hNV(v[0]);
}

void glTexCoord2hNV(GLushort x, GLushort y) {
    glTexCoord2f(half_to_float(x), half_to_float(y));
}

void glTexCoord2hvNV(const GLushort *v) {
    if (v != NULL)
        glTexCoord2hNV(v[0], v[1]);
}

void glTexCoord3hNV(GLushort x, GLushort y, GLushort z) {
    glTexCoord3f(half_to_float(x), half_to_float(y), half_to_float(z));
}

void glTexCoord3hvNV(const GLushort *v) {
    if (v != NULL)
        glTexCoord3hNV(v[0], v[1], v[2]);
}

void glTexCoord4hNV(GLushort x, GLushort y, GLushort z, GLushort w) {
    glTexCoord4f(half_to_float(x), half_to_float(y), half_to_float(z), half_to_float(w));
}

void glTexCoord4hvNV(const GLushort *v) {
    if (v != NULL)
        glTexCoord4hNV(v[0], v[1], v[2], v[3]);
}

void glMultiTexCoord1hNV(GLenum texture, GLushort x) {
    glMultiTexCoord1f(texture, half_to_float(x));
}

void glMultiTexCoord1hvNV(GLenum texture, const GLushort *v) {
    if (v != NULL)
        glMultiTexCoord1hNV(texture, v[0]);
}

void glMultiTexCoord2hNV(GLenum texture, GLushort x, GLushort y) {
    glMultiTexCoord2f(texture, half_to_float(x), half_to_float(y));
}

void glMultiTexCoord2hvNV(GLenum texture, const GLushort *v) {
    if (v != NULL)
        glMultiTexCoord2hNV(texture, v[0], v[1]);
}

void glMultiTexCoord3hNV(GLenum texture, GLushort x, GLushort y, GLushort z) {
    glMultiTexCoord3f(texture, half_to_float(x), half_to_float(y), half_to_float(z));
}

void glMultiTexCoord3hvNV(GLenum texture, const GLushort *v) {
    if (v != NULL)
        glMultiTexCoord3hNV(texture, v[0], v[1], v[2]);
}

void glMultiTexCoord4hNV(GLenum texture, GLushort x, GLushort y, GLushort z, GLushort w) {
    glMultiTexCoord4f(texture, half_to_float(x), half_to_float(y), half_to_float(z),
                      half_to_float(w));
}

void glMultiTexCoord4hvNV(GLenum texture, const GLushort *v) {
    if (v != NULL)
        glMultiTexCoord4hNV(texture, v[0], v[1], v[2], v[3]);
}

void glVertex2hNV(GLushort x, GLushort y) {
    glVertex2f(half_to_float(x), half_to_float(y));
}

void glVertex2hvNV(const GLushort *v) {
    if (v != NULL)
        glVertex2hNV(v[0], v[1]);
}

void glVertex3hNV(GLushort x, GLushort y, GLushort z) {
    glVertex3f(half_to_float(x), half_to_float(y), half_to_float(z));
}

void glVertex3hvNV(const GLushort *v) {
    if (v != NULL)
        glVertex3hNV(v[0], v[1], v[2]);
}

void glVertex4hNV(GLushort x, GLushort y, GLushort z, GLushort w) {
    glVertex4f(half_to_float(x), half_to_float(y), half_to_float(z), half_to_float(w));
}

void glVertex4hvNV(const GLushort *v) {
    if (v != NULL)
        glVertex4hNV(v[0], v[1], v[2], v[3]);
}

void glVertexAttrib1hNV(GLuint index, GLushort x) {
    glVertexAttrib1f(index, half_to_float(x));
}

void glVertexAttrib1hvNV(GLuint index, const GLushort *v) {
    if (v != NULL)
        glVertexAttrib1hNV(index, v[0]);
}

void glVertexAttrib2hNV(GLuint index, GLushort x, GLushort y) {
    glVertexAttrib2f(index, half_to_float(x), half_to_float(y));
}

void glVertexAttrib3hNV(GLuint index, GLushort x, GLushort y, GLushort z) {
    glVertexAttrib3f(index, half_to_float(x), half_to_float(y), half_to_float(z));
}

void glVertexAttrib3hvNV(GLuint index, const GLushort *v) {
    if (v != NULL)
        glVertexAttrib3hNV(index, v[0], v[1], v[2]);
}

void glVertexAttrib4hNV(GLuint index, GLushort x, GLushort y, GLushort z, GLushort w) {
    glVertexAttrib4f(index, half_to_float(x), half_to_float(y), half_to_float(z), half_to_float(w));
}

static void vertex_attribs_half(GLuint index, GLsizei n, const GLushort *v, unsigned components) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    GLuint limit;
    if (ctx == NULL)
        return;
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0)
        return;
    limit = (GLuint)MGL_MAX_VERTEX_ATTRIBS;
    if (ctx->native_max_vertex_attribs <= 0 && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesGetIntegerv != NULL) {
        GLint native_limit = 0;
        ctx->native_dispatch->glesGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &native_limit);
        if (native_limit > 0 && native_limit < (GLint)limit)
            limit = (GLuint)native_limit;
        ctx->native_max_vertex_attribs = (GLint)limit;
    } else if (ctx->native_max_vertex_attribs > 0 &&
               (GLuint)ctx->native_max_vertex_attribs < limit)
        limit = (GLuint)ctx->native_max_vertex_attribs;
    if (index >= limit || (GLuint)n > limit - index) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (v == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = n; i-- > 0;) {
        GLuint slot = index + (GLuint)i;
        const GLushort *values = v + (size_t)i * components;
        switch (components) {
            case 1u: glVertexAttrib1hNV(slot, values[0]); break;
            case 2u: glVertexAttrib2hNV(slot, values[0], values[1]); break;
            case 3u: glVertexAttrib3hNV(slot, values[0], values[1], values[2]); break;
            default: glVertexAttrib4hNV(slot, values[0], values[1], values[2], values[3]); break;
        }
    }
}

void glVertexAttribs1hvNV(GLuint index, GLsizei n, const GLushort *v) {
    vertex_attribs_half(index, n, v, 1u);
}

void glVertexAttribs2hvNV(GLuint index, GLsizei n, const GLushort *v) {
    vertex_attribs_half(index, n, v, 2u);
}

void glVertexAttribs3hvNV(GLuint index, GLsizei n, const GLushort *v) {
    vertex_attribs_half(index, n, v, 3u);
}

void glVertexAttribs4hvNV(GLuint index, GLsizei n, const GLushort *v) {
    vertex_attribs_half(index, n, v, 4u);
}

void glVertexAttrib1fv(GLuint index, const GLfloat *v) {
    if (v != NULL)
        glVertexAttrib1f(index, v[0]);
}

void glVertexAttrib2fv(GLuint index, const GLfloat *v) {
    if (v != NULL)
        glVertexAttrib2f(index, v[0], v[1]);
}

void glVertexAttrib3fv(GLuint index, const GLfloat *v) {
    if (v != NULL)
        glVertexAttrib3f(index, v[0], v[1], v[2]);
}

void glVertexAttrib4fv(GLuint index, const GLfloat *v) {
    if (v != NULL)
        glVertexAttrib4f(index, v[0], v[1], v[2], v[3]);
}

/* Signed fixed-point current attributes use half-step normalization. */
static GLfloat mgl_norm_s8(GLbyte v) {
    return (2.0f * (GLfloat)v + 1.0f) * (1.0f / 255.0f);
}

static GLfloat mgl_norm_s16(GLshort v) {
    return (2.0f * (GLfloat)v + 1.0f) * (1.0f / 65535.0f);
}

static GLfloat mgl_norm_s32(GLint v) {
    return (GLfloat)((2.0 * (GLdouble)v + 1.0) * (1.0 / 4294967294.0));
}

static GLfloat mgl_norm_u8(GLubyte v) {
    return (GLfloat)v / (GLfloat)UCHAR_MAX;
}

static GLfloat mgl_norm_u16(GLushort v) {
    return (GLfloat)v / (GLfloat)USHRT_MAX;
}

static GLfloat mgl_norm_u32(GLuint v) {
    return (GLfloat)((GLdouble)v / (GLdouble)UINT_MAX);
}

void glVertexAttrib1d(GLuint index, GLdouble x) {
    glVertexAttrib1f(index, (GLfloat)x);
}

void glVertexAttrib2d(GLuint index, GLdouble x, GLdouble y) {
    glVertexAttrib2f(index, (GLfloat)x, (GLfloat)y);
}

void glVertexAttrib3d(GLuint index, GLdouble x, GLdouble y, GLdouble z) {
    glVertexAttrib3f(index, (GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void glVertexAttrib4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    glVertexAttrib4f(index, (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
}

void glVertexAttrib1dv(GLuint index, const GLdouble *v) {
    if (v != NULL)
        glVertexAttrib1d(index, v[0]);
}

void glVertexAttrib2dv(GLuint index, const GLdouble *v) {
    if (v != NULL)
        glVertexAttrib2d(index, v[0], v[1]);
}

void glVertexAttrib3dv(GLuint index, const GLdouble *v) {
    if (v != NULL)
        glVertexAttrib3d(index, v[0], v[1], v[2]);
}

void glVertexAttrib4dv(GLuint index, const GLdouble *v) {
    if (v != NULL)
        glVertexAttrib4d(index, v[0], v[1], v[2], v[3]);
}

void glVertexAttrib1s(GLuint index, GLshort x) {
    glVertexAttrib1f(index, (GLfloat)x);
}

void glVertexAttrib2s(GLuint index, GLshort x, GLshort y) {
    glVertexAttrib2f(index, (GLfloat)x, (GLfloat)y);
}

void glVertexAttrib3s(GLuint index, GLshort x, GLshort y, GLshort z) {
    glVertexAttrib3f(index, (GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void glVertexAttrib4s(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w) {
    glVertexAttrib4f(index, (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
}

void glVertexAttrib1sv(GLuint index, const GLshort *v) {
    if (v != NULL)
        glVertexAttrib1s(index, v[0]);
}

void glVertexAttrib2sv(GLuint index, const GLshort *v) {
    if (v != NULL)
        glVertexAttrib2s(index, v[0], v[1]);
}

void glVertexAttrib3sv(GLuint index, const GLshort *v) {
    if (v != NULL)
        glVertexAttrib3s(index, v[0], v[1], v[2]);
}

void glVertexAttrib4sv(GLuint index, const GLshort *v) {
    if (v != NULL)
        glVertexAttrib4s(index, v[0], v[1], v[2], v[3]);
}

void glVertexAttrib4bv(GLuint index, const GLbyte *v) {
    if (v != NULL)
        glVertexAttrib4f(index, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

void glVertexAttrib4iv(GLuint index, const GLint *v) {
    if (v != NULL)
        glVertexAttrib4f(index, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

void glVertexAttrib4ubv(GLuint index, const GLubyte *v) {
    if (v != NULL)
        glVertexAttrib4f(index, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

void glVertexAttrib4usv(GLuint index, const GLushort *v) {
    if (v != NULL)
        glVertexAttrib4f(index, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

void glVertexAttrib4uiv(GLuint index, const GLuint *v) {
    if (v != NULL)
        glVertexAttrib4f(index, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

void glVertexAttrib4Nub(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w) {
    glVertexAttrib4f(index, mgl_norm_u8(x), mgl_norm_u8(y), mgl_norm_u8(z), mgl_norm_u8(w));
}

void glVertexAttrib4Nbv(GLuint index, const GLbyte *v) {
    if (v != NULL)
        glVertexAttrib4f(index, mgl_norm_s8(v[0]), mgl_norm_s8(v[1]), mgl_norm_s8(v[2]),
                         mgl_norm_s8(v[3]));
}

void glVertexAttrib4Nsv(GLuint index, const GLshort *v) {
    if (v != NULL)
        glVertexAttrib4f(index, mgl_norm_s16(v[0]), mgl_norm_s16(v[1]), mgl_norm_s16(v[2]),
                         mgl_norm_s16(v[3]));
}

void glVertexAttrib4Niv(GLuint index, const GLint *v) {
    if (v != NULL)
        glVertexAttrib4f(index, mgl_norm_s32(v[0]), mgl_norm_s32(v[1]), mgl_norm_s32(v[2]),
                         mgl_norm_s32(v[3]));
}

void glVertexAttrib4Nubv(GLuint index, const GLubyte *v) {
    if (v != NULL)
        glVertexAttrib4Nub(index, v[0], v[1], v[2], v[3]);
}

void glVertexAttrib4Nusv(GLuint index, const GLushort *v) {
    if (v != NULL)
        glVertexAttrib4f(index, mgl_norm_u16(v[0]), mgl_norm_u16(v[1]), mgl_norm_u16(v[2]),
                         mgl_norm_u16(v[3]));
}

void glVertexAttrib4Nuiv(GLuint index, const GLuint *v) {
    if (v != NULL)
        glVertexAttrib4f(index, mgl_norm_u32(v[0]), mgl_norm_u32(v[1]), mgl_norm_u32(v[2]),
                         mgl_norm_u32(v[3]));
}

void glVertexAttribI1i(GLuint index, GLint x) {
    struct mgl_context *ctx = mgl_context_current();
    GLint v[1] = {x};
    if (ctx == NULL)
        return;
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    set_current_int(ctx, index, 1, v);
}

void glVertexAttribI2i(GLuint index, GLint x, GLint y) {
    struct mgl_context *ctx = mgl_context_current();
    GLint v[2] = {x, y};
    if (ctx == NULL)
        return;
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    set_current_int(ctx, index, 2, v);
}

void glVertexAttribI3i(GLuint index, GLint x, GLint y, GLint z) {
    struct mgl_context *ctx = mgl_context_current();
    GLint v[3] = {x, y, z};
    if (ctx == NULL)
        return;
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    set_current_int(ctx, index, 3, v);
}

void glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w) {
    struct mgl_context *ctx = mgl_context_current();
    GLint v[4] = {x, y, z, w};
    if (ctx == NULL)
        return;
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    set_current_int(ctx, index, 4, v);
}

void glVertexAttribI1ui(GLuint index, GLuint x) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint v[1] = {x};
    if (ctx == NULL)
        return;
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    set_current_uint(ctx, index, 1, v);
}

void glVertexAttribI2ui(GLuint index, GLuint x, GLuint y) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint v[2] = {x, y};
    if (ctx == NULL)
        return;
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    set_current_uint(ctx, index, 2, v);
}

void glVertexAttribI3ui(GLuint index, GLuint x, GLuint y, GLuint z) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint v[3] = {x, y, z};
    if (ctx == NULL)
        return;
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    set_current_uint(ctx, index, 3, v);
}

void glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint v[4] = {x, y, z, w};
    if (ctx == NULL)
        return;
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    set_current_uint(ctx, index, 4, v);
}

void glVertexAttribI1iv(GLuint index, const GLint *v) {
    if (v != NULL)
        glVertexAttribI1i(index, v[0]);
}

void glVertexAttribI2iv(GLuint index, const GLint *v) {
    if (v != NULL)
        glVertexAttribI2i(index, v[0], v[1]);
}

void glVertexAttribI3iv(GLuint index, const GLint *v) {
    if (v != NULL)
        glVertexAttribI3i(index, v[0], v[1], v[2]);
}

void glVertexAttribI4iv(GLuint index, const GLint *v) {
    if (v != NULL)
        glVertexAttribI4i(index, v[0], v[1], v[2], v[3]);
}

void glVertexAttribI1uiv(GLuint index, const GLuint *v) {
    if (v != NULL)
        glVertexAttribI1ui(index, v[0]);
}

void glVertexAttribI2uiv(GLuint index, const GLuint *v) {
    if (v != NULL)
        glVertexAttribI2ui(index, v[0], v[1]);
}

void glVertexAttribI3uiv(GLuint index, const GLuint *v) {
    if (v != NULL)
        glVertexAttribI3ui(index, v[0], v[1], v[2]);
}

void glVertexAttribI4uiv(GLuint index, const GLuint *v) {
    if (v != NULL)
        glVertexAttribI4ui(index, v[0], v[1], v[2], v[3]);
}

void glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_binding *binding;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    binding = &ctx->current_vao->generic[index];
    switch (pname) {
        case GL_CURRENT_VERTEX_ATTRIB:
            for (unsigned i = 0u; i < 4u; ++i) {
                if (ctx->generic_current_type[index] == GL_INT)
                    params[i] = ctx->generic_current_i[index][i];
                else if (ctx->generic_current_type[index] == GL_UNSIGNED_INT)
                    params[i] = (GLint)ctx->generic_current_ui[index][i];
                else
                    params[i] = (GLint)ctx->generic_current[index][i];
            }
            break;
        case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
            *params = binding->enabled ? 1 : 0;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_SIZE:
            *params = binding->size;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
            *params = binding->attrib_array_stride;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_TYPE:
            *params = (GLint)binding->type;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
            *params = binding->normalized ? 1 : 0;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
            *params = binding->integer ? 1 : 0;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
            *params = (GLint)binding->data.buffer_name;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
            *params = (GLint)binding->divisor;
            break;
        case GL_VERTEX_ATTRIB_BINDING:
            *params = (GLint)ctx->current_vao->vertex_attrib_binding[index];
            break;
        case GL_VERTEX_ATTRIB_RELATIVE_OFFSET:
            *params = (GLint)ctx->current_vao->vertex_attrib_relative_offset[index];
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
}

void glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (pname == GL_CURRENT_VERTEX_ATTRIB) {
        params[0] = ctx->generic_current[index][0];
        params[1] = ctx->generic_current[index][1];
        params[2] = ctx->generic_current[index][2];
        params[3] = ctx->generic_current[index][3];
        return;
    }
    {
        GLint value = 0;
        glGetVertexAttribiv(index, pname, &value);
        params[0] = (GLfloat)value;
    }
}

void glGetVertexAttribdv(GLuint index, GLenum pname, GLdouble *params) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned i;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (pname == GL_CURRENT_VERTEX_ATTRIB) {
        for (i = 0u; i < 4u; ++i)
            params[i] = (GLdouble)ctx->generic_current[index][i];
        return;
    }
    {
        GLint value = 0;
        glGetVertexAttribiv(index, pname, &value);
        params[0] = (GLdouble)value;
    }
}

void glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned i;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (pname != GL_CURRENT_VERTEX_ATTRIB) {
        glGetVertexAttribiv(index, pname, params);
        return;
    }
    for (i = 0u; i < 4u; ++i) {
        if (ctx->generic_current_type[index] == GL_INT)
            params[i] = ctx->generic_current_i[index][i];
        else if (ctx->generic_current_type[index] == GL_UNSIGNED_INT)
            params[i] = (GLint)ctx->generic_current_ui[index][i];
        else
            params[i] = (GLint)ctx->generic_current[index][i];
    }
}

void glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned i;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (pname != GL_CURRENT_VERTEX_ATTRIB) {
        GLint value = 0;
        glGetVertexAttribiv(index, pname, &value);
        params[0] = (GLuint)value;
        return;
    }
    for (i = 0u; i < 4u; ++i) {
        if (ctx->generic_current_type[index] == GL_UNSIGNED_INT)
            params[i] = ctx->generic_current_ui[index][i];
        else if (ctx->generic_current_type[index] == GL_INT)
            params[i] = (GLuint)ctx->generic_current_i[index][i];
        else
            params[i] = (GLuint)ctx->generic_current[index][i];
    }
}

void glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_binding *binding;
    if (ctx == NULL || pointer == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_generic_index(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (pname != GL_VERTEX_ATTRIB_ARRAY_POINTER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    binding = &ctx->current_vao->generic[index];
    *pointer = binding->data.source == MGL_DATA_BUFFER ? (void *)(uintptr_t)binding->data.offset
                                                       : (void *)binding->data.client_ptr;
}

void glVertexAttribPointerARB(GLuint index, GLint size, GLenum type, GLboolean normalized,
                              GLsizei stride, const void *pointer) {
    glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

void glEnableVertexAttribArrayARB(GLuint index) {
    glEnableVertexAttribArray(index);
}

void glDisableVertexAttribArrayARB(GLuint index) {
    glDisableVertexAttribArray(index);
}

void glVertexAttrib1fARB(GLuint index, GLfloat x) {
    glVertexAttrib1f(index, x);
}

void glVertexAttrib2fARB(GLuint index, GLfloat x, GLfloat y) {
    glVertexAttrib2f(index, x, y);
}

void glVertexAttrib3fARB(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    glVertexAttrib3f(index, x, y, z);
}

void glVertexAttrib4fARB(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    glVertexAttrib4f(index, x, y, z, w);
}

void glVertexAttrib1fvARB(GLuint index, const GLfloat *v) {
    glVertexAttrib1fv(index, v);
}

void glVertexAttrib2fvARB(GLuint index, const GLfloat *v) {
    glVertexAttrib2fv(index, v);
}

void glVertexAttrib3fvARB(GLuint index, const GLfloat *v) {
    glVertexAttrib3fv(index, v);
}

void glVertexAttrib4fvARB(GLuint index, const GLfloat *v) {
    glVertexAttrib4fv(index, v);
}

void glVertexAttrib1dARB(GLuint index, GLdouble x) {
    glVertexAttrib1d(index, x);
}

void glVertexAttrib2dARB(GLuint index, GLdouble x, GLdouble y) {
    glVertexAttrib2d(index, x, y);
}

void glVertexAttrib3dARB(GLuint index, GLdouble x, GLdouble y, GLdouble z) {
    glVertexAttrib3d(index, x, y, z);
}

void glVertexAttrib4dARB(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    glVertexAttrib4d(index, x, y, z, w);
}

void glVertexAttrib1dvARB(GLuint index, const GLdouble *v) {
    glVertexAttrib1dv(index, v);
}

void glVertexAttrib2dvARB(GLuint index, const GLdouble *v) {
    glVertexAttrib2dv(index, v);
}

void glVertexAttrib3dvARB(GLuint index, const GLdouble *v) {
    glVertexAttrib3dv(index, v);
}

void glVertexAttrib4dvARB(GLuint index, const GLdouble *v) {
    glVertexAttrib4dv(index, v);
}

void glVertexAttrib1sARB(GLuint index, GLshort x) {
    glVertexAttrib1s(index, x);
}

void glVertexAttrib2sARB(GLuint index, GLshort x, GLshort y) {
    glVertexAttrib2s(index, x, y);
}

void glVertexAttrib3sARB(GLuint index, GLshort x, GLshort y, GLshort z) {
    glVertexAttrib3s(index, x, y, z);
}

void glVertexAttrib4sARB(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w) {
    glVertexAttrib4s(index, x, y, z, w);
}

void glVertexAttrib1svARB(GLuint index, const GLshort *v) {
    glVertexAttrib1sv(index, v);
}

void glVertexAttrib2svARB(GLuint index, const GLshort *v) {
    glVertexAttrib2sv(index, v);
}

void glVertexAttrib3svARB(GLuint index, const GLshort *v) {
    glVertexAttrib3sv(index, v);
}

void glVertexAttrib4svARB(GLuint index, const GLshort *v) {
    glVertexAttrib4sv(index, v);
}

void glVertexAttrib4bvARB(GLuint index, const GLbyte *v) {
    glVertexAttrib4bv(index, v);
}

void glVertexAttrib4ivARB(GLuint index, const GLint *v) {
    glVertexAttrib4iv(index, v);
}

void glVertexAttrib4ubvARB(GLuint index, const GLubyte *v) {
    glVertexAttrib4ubv(index, v);
}

void glVertexAttrib4usvARB(GLuint index, const GLushort *v) {
    glVertexAttrib4usv(index, v);
}

void glVertexAttrib4uivARB(GLuint index, const GLuint *v) {
    glVertexAttrib4uiv(index, v);
}

void glVertexAttrib4NubARB(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w) {
    glVertexAttrib4Nub(index, x, y, z, w);
}

void glVertexAttrib4NbvARB(GLuint index, const GLbyte *v) {
    glVertexAttrib4Nbv(index, v);
}

void glVertexAttrib4NsvARB(GLuint index, const GLshort *v) {
    glVertexAttrib4Nsv(index, v);
}

void glVertexAttrib4NivARB(GLuint index, const GLint *v) {
    glVertexAttrib4Niv(index, v);
}

void glVertexAttrib4NubvARB(GLuint index, const GLubyte *v) {
    glVertexAttrib4Nubv(index, v);
}

void glVertexAttrib4NusvARB(GLuint index, const GLushort *v) {
    glVertexAttrib4Nusv(index, v);
}

void glVertexAttrib4NuivARB(GLuint index, const GLuint *v) {
    glVertexAttrib4Nuiv(index, v);
}

void glGetVertexAttribivARB(GLuint index, GLenum pname, GLint *params) {
    glGetVertexAttribiv(index, pname, params);
}

void glGetVertexAttribfvARB(GLuint index, GLenum pname, GLfloat *params) {
    glGetVertexAttribfv(index, pname, params);
}

void glGetVertexAttribdvARB(GLuint index, GLenum pname, GLdouble *params) {
    glGetVertexAttribdv(index, pname, params);
}

void glGetVertexAttribPointervARB(GLuint index, GLenum pname, void **pointer) {
    glGetVertexAttribPointerv(index, pname, pointer);
}

void glClientActiveTexture(GLenum texture) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (texture < GL_TEXTURE0 || texture >= GL_TEXTURE0 + MGL_TEXTURE_UNITS) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    ctx->client_active_texture = (GLuint)(texture - GL_TEXTURE0);
}

static struct mgl_vertex_binding *compat_binding(struct mgl_context *ctx, unsigned semantic) {
    return semantic < MGL_COMPAT_SEMANTIC_COUNT ? &ctx->current_vao->compat[semantic] : NULL;
}

static void set_compat_pointer(struct mgl_context *ctx, unsigned semantic, GLint size, GLenum type,
                               GLboolean normalized, GLsizei stride, const void *pointer) {
    struct mgl_vertex_binding *binding = compat_binding(ctx, semantic);
    if (binding == NULL)
        return;
    binding->size = size;
    binding->type = type;
    binding->normalized = normalized;
    binding->stride = stride;
    binding->attrib_array_stride = stride;
    assign_data_ref(ctx, binding, pointer);
}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((size < 2 || size > 4)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!valid_array_type(type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (stride < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (named_vao_requires_array_buffer(ctx, pointer))
        return;
    set_compat_pointer(ctx, MGL_COMPAT_SEM_POSITION, size, type, GL_FALSE, stride, pointer);
}

void glNormalPointer(GLenum type, GLsizei stride, const void *pointer) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_array_type(type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (stride < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (named_vao_requires_array_buffer(ctx, pointer))
        return;
    set_compat_pointer(ctx, MGL_COMPAT_SEM_NORMAL, 3, type, type == GL_FLOAT ? GL_FALSE : GL_TRUE,
                       stride, pointer);
}

void glColorPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((size != 3 && size != 4)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!valid_array_type(type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (stride < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (named_vao_requires_array_buffer(ctx, pointer))
        return;
    set_compat_pointer(ctx, MGL_COMPAT_SEM_COLOR, size, type, type == GL_FLOAT ? GL_FALSE : GL_TRUE,
                       stride, pointer);
}

void glSecondaryColorPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (size != 3) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!valid_array_type(type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (stride < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (named_vao_requires_array_buffer(ctx, pointer))
        return;
    set_compat_pointer(ctx, MGL_COMPAT_SEM_SECONDARY, size, type,
                       type == GL_FLOAT ? GL_FALSE : GL_TRUE, stride, pointer);
}

void glFogCoordPointer(GLenum type, GLsizei stride, const void *pointer) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (type != GL_FLOAT && type != GL_HALF_FLOAT && type != GL_DOUBLE) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (stride < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (named_vao_requires_array_buffer(ctx, pointer))
        return;
    set_compat_pointer(ctx, MGL_COMPAT_SEM_FOG, 1, type, GL_FALSE, stride, pointer);
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned semantic;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (size < 1 || size > 4) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!valid_array_type(type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (stride < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    semantic = MGL_COMPAT_SEM_TEXCOORD0 + ctx->client_active_texture;
    if (named_vao_requires_array_buffer(ctx, pointer))
        return;
    set_compat_pointer(ctx, semantic, size, type, GL_FALSE, stride, pointer);
}

void glMultiTexCoordPointerEXT(GLenum texunit, GLint size, GLenum type, GLsizei stride,
                               const void *pointer) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint saved_unit;
    GLuint unit;
    if (ctx == NULL)
        return;
    if (texunit < GL_TEXTURE0) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    unit = (GLuint)(texunit - GL_TEXTURE0);
    if (unit >= MGL_TEXTURE_UNITS) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    saved_unit = ctx->client_active_texture;
    ctx->client_active_texture = unit;
    glTexCoordPointer(size, type, stride, pointer);
    ctx->client_active_texture = saved_unit;
}

struct mgl_interleaved_layout {
    GLboolean texcoord;
    GLboolean color;
    GLboolean normal;
    GLint texcoord_size;
    GLint color_size;
    GLint vertex_size;
    GLenum color_type;
    size_t color_offset;
    size_t normal_offset;
    size_t vertex_offset;
    size_t packed_stride;
};

static int interleaved_layout(GLenum format, struct mgl_interleaved_layout *layout) {
    const size_t f = sizeof(GLfloat);
    const size_t c = ((4u * sizeof(GLubyte) + f - 1u) / f) * f;
    struct mgl_interleaved_layout out = {GL_FALSE, GL_FALSE, GL_FALSE, 0,  0, 0,
                                         GL_FLOAT, 0u,       0u,       0u, 0u};

    switch (format) {
        case GL_V2F:
            out.vertex_size = 2;
            out.vertex_offset = 0u;
            out.packed_stride = 2u * f;
            break;
        case GL_V3F:
            out.vertex_size = 3;
            out.vertex_offset = 0u;
            out.packed_stride = 3u * f;
            break;
        case GL_C4UB_V2F:
            out.color = GL_TRUE;
            out.color_size = 4;
            out.color_type = GL_UNSIGNED_BYTE;
            out.vertex_size = 2;
            out.color_offset = 0u;
            out.vertex_offset = c;
            out.packed_stride = c + 2u * f;
            break;
        case GL_C4UB_V3F:
            out.color = GL_TRUE;
            out.color_size = 4;
            out.color_type = GL_UNSIGNED_BYTE;
            out.vertex_size = 3;
            out.color_offset = 0u;
            out.vertex_offset = c;
            out.packed_stride = c + 3u * f;
            break;
        case GL_C3F_V3F:
            out.color = GL_TRUE;
            out.color_size = 3;
            out.color_type = GL_FLOAT;
            out.vertex_size = 3;
            out.color_offset = 0u;
            out.vertex_offset = 3u * f;
            out.packed_stride = 6u * f;
            break;
        case GL_N3F_V3F:
            out.normal = GL_TRUE;
            out.vertex_size = 3;
            out.normal_offset = 0u;
            out.vertex_offset = 3u * f;
            out.packed_stride = 6u * f;
            break;
        case GL_C4F_N3F_V3F:
            out.color = GL_TRUE;
            out.normal = GL_TRUE;
            out.color_size = 4;
            out.color_type = GL_FLOAT;
            out.vertex_size = 3;
            out.color_offset = 0u;
            out.normal_offset = 4u * f;
            out.vertex_offset = 7u * f;
            out.packed_stride = 10u * f;
            break;
        case GL_T2F_V3F:
            out.texcoord = GL_TRUE;
            out.texcoord_size = 2;
            out.vertex_size = 3;
            out.vertex_offset = 2u * f;
            out.packed_stride = 5u * f;
            break;
        case GL_T4F_V4F:
            out.texcoord = GL_TRUE;
            out.texcoord_size = 4;
            out.vertex_size = 4;
            out.vertex_offset = 4u * f;
            out.packed_stride = 8u * f;
            break;
        case GL_T2F_C4UB_V3F:
            out.texcoord = GL_TRUE;
            out.color = GL_TRUE;
            out.texcoord_size = 2;
            out.color_size = 4;
            out.color_type = GL_UNSIGNED_BYTE;
            out.vertex_size = 3;
            out.color_offset = 2u * f;
            out.vertex_offset = c + 2u * f;
            out.packed_stride = c + 5u * f;
            break;
        case GL_T2F_C3F_V3F:
            out.texcoord = GL_TRUE;
            out.color = GL_TRUE;
            out.texcoord_size = 2;
            out.color_size = 3;
            out.color_type = GL_FLOAT;
            out.vertex_size = 3;
            out.color_offset = 2u * f;
            out.vertex_offset = 5u * f;
            out.packed_stride = 8u * f;
            break;
        case GL_T2F_N3F_V3F:
            out.texcoord = GL_TRUE;
            out.normal = GL_TRUE;
            out.texcoord_size = 2;
            out.vertex_size = 3;
            out.normal_offset = 2u * f;
            out.vertex_offset = 5u * f;
            out.packed_stride = 8u * f;
            break;
        case GL_T2F_C4F_N3F_V3F:
            out.texcoord = GL_TRUE;
            out.color = GL_TRUE;
            out.normal = GL_TRUE;
            out.texcoord_size = 2;
            out.color_size = 4;
            out.color_type = GL_FLOAT;
            out.vertex_size = 3;
            out.color_offset = 2u * f;
            out.normal_offset = 6u * f;
            out.vertex_offset = 9u * f;
            out.packed_stride = 12u * f;
            break;
        case GL_T4F_C4F_N3F_V4F:
            out.texcoord = GL_TRUE;
            out.color = GL_TRUE;
            out.normal = GL_TRUE;
            out.texcoord_size = 4;
            out.color_size = 4;
            out.color_type = GL_FLOAT;
            out.vertex_size = 4;
            out.color_offset = 4u * f;
            out.normal_offset = 8u * f;
            out.vertex_offset = 11u * f;
            out.packed_stride = 15u * f;
            break;
        default:
            return 0;
    }
    *layout = out;
    return 1;
}

static const void *interleaved_pointer_offset(const void *pointer, size_t offset) {
    return (const void *)((uintptr_t)pointer + (uintptr_t)offset);
}

void glInterleavedArrays(GLenum format, GLsizei stride, const void *pointer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_interleaved_layout layout;
    struct mgl_vertex_binding *texcoord;
    struct mgl_vertex_binding *color;
    struct mgl_vertex_binding *normal;
    struct mgl_vertex_binding *secondary;
    struct mgl_vertex_binding *vertex;
    GLsizei actual_stride;

    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!interleaved_layout(format, &layout)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (stride < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (layout.packed_stride > (size_t)INT_MAX) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    actual_stride = stride != 0 ? stride : (GLsizei)layout.packed_stride;
    if (named_vao_requires_array_buffer(ctx, pointer))
        return;

    texcoord = compat_binding(ctx, MGL_COMPAT_SEM_TEXCOORD0 + ctx->client_active_texture);
    color = compat_binding(ctx, MGL_COMPAT_SEM_COLOR);
    normal = compat_binding(ctx, MGL_COMPAT_SEM_NORMAL);
    secondary = compat_binding(ctx, MGL_COMPAT_SEM_SECONDARY);
    vertex = compat_binding(ctx, MGL_COMPAT_SEM_POSITION);
    if (texcoord == NULL || color == NULL || normal == NULL || secondary == NULL ||
        vertex == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }

    secondary->enabled = GL_FALSE;

    texcoord->enabled = layout.texcoord;
    if (layout.texcoord)
        set_compat_pointer(ctx, MGL_COMPAT_SEM_TEXCOORD0 + ctx->client_active_texture,
                           layout.texcoord_size, GL_FLOAT, GL_FALSE, actual_stride, pointer);

    color->enabled = layout.color;
    if (layout.color)
        set_compat_pointer(ctx, MGL_COMPAT_SEM_COLOR, layout.color_size, layout.color_type,
                           layout.color_type == GL_FLOAT ? GL_FALSE : GL_TRUE, actual_stride,
                           interleaved_pointer_offset(pointer, layout.color_offset));

    normal->enabled = layout.normal;
    if (layout.normal)
        set_compat_pointer(ctx, MGL_COMPAT_SEM_NORMAL, 3, GL_FLOAT, GL_FALSE, actual_stride,
                           interleaved_pointer_offset(pointer, layout.normal_offset));

    vertex->enabled = GL_TRUE;
    set_compat_pointer(ctx, MGL_COMPAT_SEM_POSITION, layout.vertex_size, GL_FLOAT, GL_FALSE,
                       actual_stride, interleaved_pointer_offset(pointer, layout.vertex_offset));
}

void glLockArrays(GLint first, GLsizei count) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (first < 0 || count <= 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (ctx->array_lock_count != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->array_lock_first = first;
    ctx->array_lock_count = count;
}

void glUnlockArrays(void) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active || ctx->array_lock_count == 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->array_lock_first = 0;
    ctx->array_lock_count = 0;
}

void glLockArraysEXT(GLint first, GLsizei count) {
    glLockArrays(first, count);
}

void glUnlockArraysEXT(void) {
    glUnlockArrays();
}

static struct mgl_vertex_binding *binding_for_vao_client_array(struct mgl_vertex_array *vao,
                                                               GLenum array, GLuint texture_unit) {
    if (vao == NULL)
        return NULL;
    switch (array) {
        case GL_VERTEX_ARRAY:
            return &vao->compat[MGL_COMPAT_SEM_POSITION];
        case GL_NORMAL_ARRAY:
            return &vao->compat[MGL_COMPAT_SEM_NORMAL];
        case GL_COLOR_ARRAY:
            return &vao->compat[MGL_COMPAT_SEM_COLOR];
        case GL_SECONDARY_COLOR_ARRAY:
            return &vao->compat[MGL_COMPAT_SEM_SECONDARY];
        case GL_FOG_COORD_ARRAY:
            return &vao->compat[MGL_COMPAT_SEM_FOG];
        case GL_EDGE_FLAG_ARRAY:
            return &vao->edge_flag;
        case GL_INDEX_ARRAY:
            return &vao->color_index;
        case GL_TEXTURE_COORD_ARRAY:
            return texture_unit < MGL_TEXTURE_UNITS
                       ? &vao->compat[MGL_COMPAT_SEM_TEXCOORD0 + texture_unit]
                       : NULL;
        default:
            return NULL;
    }
}

static struct mgl_vertex_binding *binding_for_client_array(struct mgl_context *ctx, GLenum array) {
    return ctx != NULL
               ? binding_for_vao_client_array(ctx->current_vao, array, ctx->client_active_texture)
               : NULL;
}

/* Edge flags are fixed-function polygon-boundary metadata. GLES 3.2 has no
 * corresponding vertex attribute; retain the desktop VAO pointer and enable
 * without binding an invented native attribute. */
void glEdgeFlagPointer(GLsizei stride, const void *pointer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_binding *binding;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (stride < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (named_vao_requires_array_buffer(ctx, pointer))
        return;
    binding = &ctx->current_vao->edge_flag;
    binding->size = 1;
    binding->type = GL_UNSIGNED_BYTE;
    binding->normalized = GL_FALSE;
    binding->integer = GL_FALSE;
    binding->stride = stride;
    binding->attrib_array_stride = stride;
    assign_data_ref(ctx, binding, pointer);
}

/* Legacy color-index arrays have no equivalent GLES shader input. Retain
 * their complete desktop VAO state without inventing a native GLES attribute.
 * Mesa varray.c _mesa_IndexPointer allows these five scalar types. */
MGL_EXPORT void glIndexPointer(GLenum type, GLsizei stride, const void *pointer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_binding *binding;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (type != GL_UNSIGNED_BYTE && type != GL_SHORT && type != GL_INT && type != GL_FLOAT &&
        type != GL_DOUBLE) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (stride < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (named_vao_requires_array_buffer(ctx, pointer))
        return;
    binding = &ctx->current_vao->color_index;
    binding->size = 1;
    binding->type = type;
    binding->normalized = GL_FALSE;
    binding->integer = GL_FALSE;
    binding->stride = stride;
    binding->attrib_array_stride = stride;
    assign_data_ref(ctx, binding, pointer);
}

void glEnableClientState(GLenum array) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_binding *binding;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    binding = binding_for_client_array(ctx, array);
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    binding->enabled = GL_TRUE;
}

void glDisableClientState(GLenum array) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_binding *binding;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    binding = binding_for_client_array(ctx, array);
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    binding->enabled = GL_FALSE;
}

static void set_indexed_texture_coord_client_state(GLenum array, GLuint index, GLboolean enabled) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (array != GL_TEXTURE_COORD_ARRAY) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (index >= MGL_TEXTURE_UNITS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    ctx->current_vao->compat[MGL_COMPAT_SEM_TEXCOORD0 + index].enabled = enabled;
}

void glEnableClientStateIndexed(GLenum array, GLuint index) {
    set_indexed_texture_coord_client_state(array, index, GL_TRUE);
}

void glDisableClientStateIndexed(GLenum array, GLuint index) {
    set_indexed_texture_coord_client_state(array, index, GL_FALSE);
}

void glEnableClientStateIndexedEXT(GLenum array, GLuint index) {
    glEnableClientStateIndexed(array, index);
}

void glDisableClientStateIndexedEXT(GLenum array, GLuint index) {
    glDisableClientStateIndexed(array, index);
}

void glEnableClientStatei(GLenum array, GLuint index) {
    glEnableClientStateIndexed(array, index);
}

void glDisableClientStatei(GLenum array, GLuint index) {
    glDisableClientStateIndexed(array, index);
}

void glEnableClientStateiEXT(GLenum array, GLuint index) {
    glEnableClientStateIndexed(array, index);
}

void glDisableClientStateiEXT(GLenum array, GLuint index) {
    glDisableClientStateIndexed(array, index);
}

static void set_named_vao_client_state(GLuint vaobj, GLenum array, GLboolean enabled) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_array *vao;
    struct mgl_vertex_binding *binding;
    GLuint unit;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (vaobj == 0u || (vao = mgl_vao_find(ctx, vaobj)) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (array >= GL_TEXTURE0 && array < GL_TEXTURE0 + MGL_TEXTURE_UNITS) {
        unit = (GLuint)(array - GL_TEXTURE0);
        binding = binding_for_vao_client_array(vao, GL_TEXTURE_COORD_ARRAY, unit);
    } else {
        binding = binding_for_vao_client_array(vao, array, ctx->client_active_texture);
    }
    if (binding == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    vao->realized = GL_TRUE;
    binding->enabled = enabled;
}

void glEnableVertexArray(GLuint vaobj, GLenum array) {
    set_named_vao_client_state(vaobj, array, GL_TRUE);
}

void glDisableVertexArray(GLuint vaobj, GLenum array) {
    set_named_vao_client_state(vaobj, array, GL_FALSE);
}

void glEnableVertexArrayEXT(GLuint vaobj, GLenum array) {
    glEnableVertexArray(vaobj, array);
}

void glDisableVertexArrayEXT(GLuint vaobj, GLenum array) {
    glDisableVertexArray(vaobj, array);
}

void glGetPointerIndexedv(GLenum pname, GLuint index, void **params) {
    struct mgl_context *ctx = mgl_context_current();
    const struct mgl_vertex_binding *binding;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    if (pname != GL_TEXTURE_COORD_ARRAY_POINTER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (index >= MGL_TEXTURE_UNITS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    binding = &ctx->current_vao->compat[MGL_COMPAT_SEM_TEXCOORD0 + index];
    *params = binding->data.source == MGL_DATA_BUFFER ? (void *)(uintptr_t)binding->data.offset
                                                      : (void *)binding->data.client_ptr;
}

void glGetPointerIndexedvEXT(GLenum pname, GLuint index, void **params) {
    glGetPointerIndexedv(pname, index, params);
}

static struct mgl_vertex_array *mgl_vertex_array_target(struct mgl_context *ctx, GLuint array) {
    struct mgl_vertex_array *vao;
    if (array == 0u || (vao = mgl_vao_find(ctx, array)) == NULL || !vao->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    return vao;
}

static void mgl_vertex_buffer_slot_set(struct mgl_context *ctx, struct mgl_vertex_array *vao,
                                       GLuint binding, GLuint buffer, GLintptr offset,
                                       GLsizei stride) {
    struct mgl_vertex_buffer_slot *slot;
    if (binding >= mgl_context_vertex_binding_limit(ctx)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (offset < 0 || stride < 0 || stride > 2048) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (buffer != 0u && mgl_buffer_find(ctx, buffer) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    slot = &vao->vertex_buffers[binding];
    slot->data.source = MGL_DATA_BUFFER;
    slot->data.buffer_name = buffer;
    slot->data.offset = (size_t)offset;
    slot->data.client_ptr = NULL;
    slot->stride = stride;
    for (GLuint i = 0u; i < MGL_MAX_VERTEX_ATTRIBS; ++i)
        if (vao->vertex_attrib_binding[i] == binding)
            vao->generic[i].separate_stride = GL_TRUE;
    mgl_vertex_array_resolve_slot(vao, binding);
}

static void mgl_vertex_attrib_format_set(struct mgl_context *ctx, struct mgl_vertex_array *vao,
                                         GLuint index, GLint size, GLenum type,
                                         GLboolean normalized, GLuint relativeoffset,
                                         GLboolean integer) {
    struct mgl_vertex_binding *attr;
    if (!valid_generic_index(ctx, index) || relativeoffset > 2047u || size < 1 || size > 4) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!(integer ? valid_integer_array_type(type) : valid_generic_array_type(type))) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!integer && packed_generic_array_type(type) && size != 4) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    attr = &vao->generic[index];
    attr->size = size;
    attr->type = type;
    attr->normalized = integer ? GL_FALSE : (normalized ? GL_TRUE : GL_FALSE);
    attr->integer = integer;
    attr->separate_stride = GL_TRUE;
    vao->vertex_attrib_relative_offset[index] = relativeoffset;
    mgl_vertex_array_resolve(vao, index);
}

static void mgl_vertex_attrib_binding_set(struct mgl_context *ctx, struct mgl_vertex_array *vao,
                                          GLuint index, GLuint binding) {
    if (!valid_generic_index(ctx, index) ||
        binding >= mgl_context_vertex_binding_limit(ctx)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    vao->vertex_attrib_binding[index] = binding;
    vao->generic[index].separate_stride = GL_TRUE;
    mgl_vertex_array_resolve(vao, index);
}

static void mgl_vertex_binding_divisor_set(struct mgl_context *ctx, struct mgl_vertex_array *vao,
                                           GLuint binding, GLuint divisor) {
    if (binding >= mgl_context_vertex_binding_limit(ctx)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    vao->vertex_buffers[binding].divisor = divisor;
    mgl_vertex_array_resolve_slot(vao, binding);
}

void glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    mgl_vertex_buffer_slot_set(ctx, ctx->current_vao, bindingindex, buffer, offset, stride);
}

void glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type, GLboolean normalized,
                          GLuint relativeoffset) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    mgl_vertex_attrib_format_set(ctx, ctx->current_vao, attribindex, size, type, normalized,
                                 relativeoffset, GL_FALSE);
}

void glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    mgl_vertex_attrib_format_set(ctx, ctx->current_vao, attribindex, size, type, GL_FALSE,
                                 relativeoffset, GL_TRUE);
}

void glVertexAttribBinding(GLuint attribindex, GLuint bindingindex) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    mgl_vertex_attrib_binding_set(ctx, ctx->current_vao, attribindex, bindingindex);
}

void glVertexBindingDivisor(GLuint bindingindex, GLuint divisor) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    mgl_vertex_binding_divisor_set(ctx, ctx->current_vao, bindingindex, divisor);
}

void glBindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers,
                         const GLintptr *offsets, const GLsizei *strides) {
    struct mgl_context *ctx = mgl_context_current();
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
    const GLuint max_bindings = mgl_context_vertex_binding_limit(ctx);
    if (first > max_bindings || (GLuint)count > max_bindings - first) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (buffers != NULL && (offsets == NULL || strides == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < count; ++i)
        mgl_vertex_buffer_slot_set(
            ctx, ctx->current_vao, first + (GLuint)i, buffers != NULL ? buffers[i] : 0u,
            buffers != NULL ? offsets[i] : 0, buffers != NULL ? strides[i] : 16);
}

void glVertexArrayVertexBuffer(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset,
                               GLsizei stride) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_array *vao;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((vao = mgl_vertex_array_target(ctx, vaobj)) != NULL)
        mgl_vertex_buffer_slot_set(ctx, vao, bindingindex, buffer, offset, stride);
}

void glVertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count, const GLuint *buffers,
                                const GLintptr *offsets, const GLsizei *strides) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_array *vao;
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
    const GLuint max_bindings = mgl_context_vertex_binding_limit(ctx);
    if (first > max_bindings || (GLuint)count > max_bindings - first) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (buffers != NULL && (offsets == NULL || strides == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if ((vao = mgl_vertex_array_target(ctx, vaobj)) == NULL)
        return;
    for (i = 0; i < count; ++i)
        mgl_vertex_buffer_slot_set(ctx, vao, first + (GLuint)i, buffers != NULL ? buffers[i] : 0u,
                                   buffers != NULL ? offsets[i] : 0,
                                   buffers != NULL ? strides[i] : 16);
}

void glVertexArrayAttribFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type,
                               GLboolean normalized, GLuint relativeoffset) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_array *vao;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((vao = mgl_vertex_array_target(ctx, vaobj)) != NULL)
        mgl_vertex_attrib_format_set(ctx, vao, attribindex, size, type, normalized, relativeoffset,
                                     GL_FALSE);
}

void glVertexArrayAttribIFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type,
                                GLuint relativeoffset) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_array *vao;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((vao = mgl_vertex_array_target(ctx, vaobj)) != NULL)
        mgl_vertex_attrib_format_set(ctx, vao, attribindex, size, type, GL_FALSE, relativeoffset,
                                     GL_TRUE);
}

void glVertexArrayAttribBinding(GLuint vaobj, GLuint attribindex, GLuint bindingindex) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_array *vao;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((vao = mgl_vertex_array_target(ctx, vaobj)) != NULL)
        mgl_vertex_attrib_binding_set(ctx, vao, attribindex, bindingindex);
}

void glVertexArrayBindingDivisor(GLuint vaobj, GLuint bindingindex, GLuint divisor) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_array *vao;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((vao = mgl_vertex_array_target(ctx, vaobj)) != NULL)
        mgl_vertex_binding_divisor_set(ctx, vao, bindingindex, divisor);
}

void glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_array *vao;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((vao = mgl_vertex_array_target(ctx, vaobj)) == NULL)
        return;
    if (buffer != 0u && mgl_buffer_find(ctx, buffer) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    vao->element_array_buffer = buffer;
    if (vao == ctx->current_vao)
        mgl_restore_element_array_binding(ctx, buffer);
}

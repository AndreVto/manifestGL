#include "compat/raster.h"

#include "core/context.h"
#include "core/render_diag.h"
#include "mgl/gl.h"
#include "list/display_list.h"
#include "objects/object_store.h"
#include "texture/pixel_layout.h"
#include "shader/shader_store.h"
#include "shader/translator.h"
#include "texture/depth_readback.h"
#include "texture/texture_store.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const GLchar raster_vs[] =
    "#version 300 es\n"
    "uniform vec4 uRect; uniform vec4 uViewport; uniform float uDepth;\n"
    "out vec2 vUV;\n"
    "void main(){\n"
    " vec2 p; int i=gl_VertexID;\n"
    " if(i==0||i==3) p=vec2(0.0,0.0); else if(i==1) p=vec2(1.0,0.0); else if(i==2||i==4) "
    "p=vec2(1.0,1.0); else p=vec2(0.0,1.0);\n"
    " vUV=p; vec2 win=uRect.xy+p*uRect.zw; vec2 ndc=(win-uViewport.xy)/uViewport.zw*2.0-1.0;\n"
    " gl_Position=vec4(ndc,uDepth*2.0-1.0,1.0);\n"
    "}\n";

static const GLchar raster_fs[] =
    "#version 300 es\n"
    "precision highp float;\n"
    "uniform sampler2D uPixels; uniform int uBitmap; uniform vec4 uRasterColor;\n"
    "uniform int uAlphaEnabled; uniform int uAlphaFunc; uniform float uAlphaRef;\n"
    "in vec2 vUV; layout(location=0) out vec4 outColor;\n"
    "bool alphaPass(float a){ if(uAlphaFunc==0) return false; if(uAlphaFunc==1) return "
    "a<uAlphaRef; if(uAlphaFunc==2) return a==uAlphaRef;"
    " if(uAlphaFunc==3) return a<=uAlphaRef; if(uAlphaFunc==4) return a>uAlphaRef; "
    "if(uAlphaFunc==5) return a!=uAlphaRef; if(uAlphaFunc==6) return a>=uAlphaRef; return true; }\n"
    "void main(){ vec4 t=texture(uPixels,vUV); if(uBitmap!=0){ if(t.a<0.5) discard; "
    "t=uRasterColor; }"
    " if(uAlphaEnabled!=0 && !alphaPass(t.a)) discard; outColor=t; }\n";

void mgl_compat_raster_init(struct mgl_compat_raster_state *state) {
    memset(state, 0, sizeof(*state));
    state->valid = GL_TRUE;
    state->position[3] = 1.0f;
    state->color[0] = state->color[1] = state->color[2] = state->color[3] = 1.0f;
    state->secondary[3] = 1.0f;
    state->texcoord[3] = 1.0f;
}

static void mul4(const GLfloat m[16], const GLdouble in[4], GLdouble out[4]) {
    unsigned r;
    for (r = 0u; r < 4u; ++r)
        out[r] = (GLdouble)m[r] * in[0] + (GLdouble)m[4u + r] * in[1] +
                 (GLdouble)m[8u + r] * in[2] + (GLdouble)m[12u + r] * in[3];
}

static void raster_pos(GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    struct mgl_context *ctx = mgl_context_current();
    GLdouble object[4] = {x, y, z, w}, eye[4], clip[4], invw, ndcx, ndcy, ndcz;
    unsigned i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_raster_pos4d(ctx, x, y, z, w))
        return;
    mul4(mgl_compat_modelview(&ctx->compat), object, eye);
    mul4(mgl_compat_projection(&ctx->compat), eye, clip);
    if (clip[3] == 0.0 || clip[0] < -clip[3] || clip[0] > clip[3] || clip[1] < -clip[3] ||
        clip[1] > clip[3] || clip[2] < -clip[3] || clip[2] > clip[3]) {
        ctx->compat_raster.valid = GL_FALSE;
        return;
    }
    invw = 1.0 / clip[3];
    ndcx = clip[0] * invw;
    ndcy = clip[1] * invw;
    ndcz = clip[2] * invw;
    ctx->compat_raster.valid = GL_TRUE;
    ctx->compat_raster.position[0] =
        (GLfloat)((GLdouble)ctx->canonical_state.viewport[0] +
                  (ndcx + 1.0) * 0.5 * (GLdouble)ctx->canonical_state.viewport[2]);
    ctx->compat_raster.position[1] =
        (GLfloat)((GLdouble)ctx->canonical_state.viewport[1] +
                  (ndcy + 1.0) * 0.5 * (GLdouble)ctx->canonical_state.viewport[3]);
    ctx->compat_raster.position[2] = (GLfloat)((ndcz + 1.0) * 0.5);
    ctx->compat_raster.position[3] = (GLfloat)clip[3];
    memcpy(ctx->compat_raster.color, ctx->compat_current[MGL_COMPAT_SEM_COLOR],
           sizeof(ctx->compat_raster.color));
    memcpy(ctx->compat_raster.secondary, ctx->compat_current[MGL_COMPAT_SEM_SECONDARY],
           sizeof(ctx->compat_raster.secondary));
    for (i = 0u; i < 4u; ++i)
        ctx->compat_raster.texcoord[i] =
            ctx->compat_current[MGL_COMPAT_SEM_TEXCOORD0 +
                                (ctx->compat.active_texture < MGL_TEXTURE_UNITS
                                     ? ctx->compat.active_texture
                                     : 0u)][i];
}

void glRasterPos2f(GLfloat x, GLfloat y) {
    raster_pos((GLdouble)x, (GLdouble)y, 0.0, 1.0);
}

void glRasterPos3f(GLfloat x, GLfloat y, GLfloat z) {
    raster_pos((GLdouble)x, (GLdouble)y, (GLdouble)z, 1.0);
}

void glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    raster_pos((GLdouble)x, (GLdouble)y, (GLdouble)z, (GLdouble)w);
}

void glRasterPos2d(GLdouble x, GLdouble y) {
    raster_pos(x, y, 0.0, 1.0);
}

void glRasterPos3d(GLdouble x, GLdouble y, GLdouble z) {
    raster_pos(x, y, z, 1.0);
}

void glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    raster_pos(x, y, z, w);
}

void glRasterPos2fv(const GLfloat *v) {
    if (v != NULL)
        glRasterPos2f(v[0], v[1]);
}

void glRasterPos3fv(const GLfloat *v) {
    if (v != NULL)
        glRasterPos3f(v[0], v[1], v[2]);
}

void glRasterPos4fv(const GLfloat *v) {
    if (v != NULL)
        glRasterPos4f(v[0], v[1], v[2], v[3]);
}

void glRasterPos2dv(const GLdouble *v) {
    if (v != NULL)
        glRasterPos2d(v[0], v[1]);
}

void glRasterPos3dv(const GLdouble *v) {
    if (v != NULL)
        glRasterPos3d(v[0], v[1], v[2]);
}

void glRasterPos4dv(const GLdouble *v) {
    if (v != NULL)
        glRasterPos4d(v[0], v[1], v[2], v[3]);
}

static void window_pos(GLfloat x, GLfloat y, GLfloat z) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_window_pos3f(ctx, x, y, z))
        return;
    if (z < 0.0f)
        z = 0.0f;
    if (z > 1.0f)
        z = 1.0f;
    ctx->compat_raster.valid = GL_TRUE;
    ctx->compat_raster.position[0] = x;
    ctx->compat_raster.position[1] = y;
    ctx->compat_raster.position[2] = z;
    ctx->compat_raster.position[3] = 1.0f;
    memcpy(ctx->compat_raster.color, ctx->compat_current[MGL_COMPAT_SEM_COLOR],
           sizeof(ctx->compat_raster.color));
    memcpy(ctx->compat_raster.secondary, ctx->compat_current[MGL_COMPAT_SEM_SECONDARY],
           sizeof(ctx->compat_raster.secondary));
    for (i = 0u; i < 4u; ++i)
        ctx->compat_raster.texcoord[i] =
            ctx->compat_current[MGL_COMPAT_SEM_TEXCOORD0 +
                                (ctx->compat.active_texture < MGL_TEXTURE_UNITS
                                     ? ctx->compat.active_texture
                                     : 0u)][i];
}

void glWindowPos2f(GLfloat x, GLfloat y) {
    window_pos(x, y, 0.0f);
}

void glWindowPos3f(GLfloat x, GLfloat y, GLfloat z) {
    window_pos(x, y, z);
}

void glWindowPos2d(GLdouble x, GLdouble y) {
    window_pos((GLfloat)x, (GLfloat)y, 0.0f);
}

void glWindowPos3d(GLdouble x, GLdouble y, GLdouble z) {
    window_pos((GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void glWindowPos2i(GLint x, GLint y) {
    window_pos((GLfloat)x, (GLfloat)y, 0.0f);
}

void glWindowPos3i(GLint x, GLint y, GLint z) {
    window_pos((GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void glWindowPos2s(GLshort x, GLshort y) {
    window_pos((GLfloat)x, (GLfloat)y, 0.0f);
}

void glWindowPos3s(GLshort x, GLshort y, GLshort z) {
    window_pos((GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void glWindowPos2fv(const GLfloat *v) {
    if (v != NULL)
        glWindowPos2f(v[0], v[1]);
}

void glWindowPos3fv(const GLfloat *v) {
    if (v != NULL)
        glWindowPos3f(v[0], v[1], v[2]);
}

void glWindowPos2dv(const GLdouble *v) {
    if (v != NULL)
        glWindowPos2d(v[0], v[1]);
}

void glWindowPos3dv(const GLdouble *v) {
    if (v != NULL)
        glWindowPos3d(v[0], v[1], v[2]);
}

void glWindowPos2iv(const GLint *v) {
    if (v != NULL)
        glWindowPos2i(v[0], v[1]);
}

void glWindowPos3iv(const GLint *v) {
    if (v != NULL)
        glWindowPos3i(v[0], v[1], v[2]);
}

void glWindowPos2sv(const GLshort *v) {
    if (v != NULL)
        glWindowPos2s(v[0], v[1]);
}

void glWindowPos3sv(const GLshort *v) {
    if (v != NULL)
        glWindowPos3s(v[0], v[1], v[2]);
}

void glPixelZoom(GLfloat xfactor, GLfloat yfactor) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_pixel_zoom(ctx, xfactor, yfactor))
        return;
    ctx->pixel_transfer.pixel_zoom_x = xfactor;
    ctx->pixel_transfer.pixel_zoom_y = yfactor;
}

static int checked_mul(size_t a, size_t b, size_t *out) {
    if (a != 0u && b > SIZE_MAX / a)
        return 0;
    *out = a * b;
    return 1;
}

static int checked_add(size_t a, size_t b, size_t *out) {
    if (a > SIZE_MAX - b)
        return 0;
    *out = a + b;
    return 1;
}

static void swap_elements(unsigned char *data, size_t bytes, size_t element_size) {
    size_t offset;
    if (data == NULL || element_size <= 1u)
        return;
    for (offset = 0u; offset + element_size <= bytes; offset += element_size) {
        size_t i;
        for (i = 0u; i < element_size / 2u; ++i) {
            unsigned char tmp = data[offset + i];
            data[offset + i] = data[offset + element_size - 1u - i];
            data[offset + element_size - 1u - i] = tmp;
        }
    }
}

static size_t aligned_stride(size_t row, GLint alignment) {
    size_t a = (size_t)alignment;
    return (row + a - 1u) & ~(a - 1u);
}

static const unsigned char *unpack_source(struct mgl_context *ctx, const void *pixels,
                                          size_t bytes) {
    if (ctx->pixel_unpack_buffer != 0u) {
        struct mgl_buffer *buffer = mgl_buffer_find(ctx, ctx->pixel_unpack_buffer);
        size_t offset = (size_t)(uintptr_t)pixels;
        if (buffer == NULL || offset > buffer->size || bytes > buffer->size - offset)
            return NULL;
        return mgl_buffer_shadow_ensure(ctx, buffer) ? buffer->shadow + offset : NULL;
    }
    return (const unsigned char *)pixels;
}

static unsigned pixel_map_slot(GLenum map) {
    switch (map) {
        case GL_PIXEL_MAP_I_TO_I:
            return 0u;
        case GL_PIXEL_MAP_S_TO_S:
            return 1u;
        case GL_PIXEL_MAP_I_TO_R:
            return 2u;
        case GL_PIXEL_MAP_I_TO_G:
            return 3u;
        case GL_PIXEL_MAP_I_TO_B:
            return 4u;
        case GL_PIXEL_MAP_I_TO_A:
            return 5u;
        case GL_PIXEL_MAP_R_TO_R:
            return 6u;
        case GL_PIXEL_MAP_G_TO_G:
            return 7u;
        case GL_PIXEL_MAP_B_TO_B:
            return 8u;
        default:
            return 9u;
    }
}

static GLdouble pixel_map_lookup_index(const struct mgl_context *ctx, unsigned slot, GLuint index) {
    const struct mgl_pixel_map_table *table = &ctx->pixel_transfer.maps[slot];
    if (table->size <= 0)
        return 0.0;
    return table->values[index & (GLuint)(table->size - 1)];
}

static GLdouble pixel_map_lookup_component(const struct mgl_context *ctx, unsigned slot,
                                           unsigned char value) {
    const struct mgl_pixel_map_table *table = &ctx->pixel_transfer.maps[slot];
    size_t index;
    if (table->size <= 1)
        return table->size == 1 ? table->values[0] : 0.0;
    index = ((size_t)value * (size_t)(table->size - 1) + 127u) / 255u;
    return table->values[index];
}

static unsigned char normalized_byte(GLdouble value) {
    if (value <= 0.0)
        return 0u;
    if (value >= 1.0)
        return 255u;
    return (unsigned char)(value * 255.0 + 0.5);
}

static unsigned draw_color_channels(GLenum format) {
    switch (format) {
        case GL_RED:
        case GL_GREEN:
        case GL_BLUE:
        case GL_ALPHA:
        case GL_LUMINANCE:
        case GL_COLOR_INDEX:
            return 1u;
        case GL_RG:
        case GL_LUMINANCE_ALPHA:
            return 2u;
        case GL_RGB:
        case GL_BGR:
            return 3u;
        case GL_RGBA:
        case GL_BGRA:
            return 4u;
        default:
            return 0u;
    }
}

static int draw_pixel_info(GLenum format, GLenum type, size_t *pixel_bytes, size_t *element_bytes,
                           unsigned *channels) {
    unsigned count = draw_color_channels(format);
    size_t bytes;
    if (count == 0u)
        return 0;
    if (format == GL_COLOR_INDEX) {
        bytes = type == GL_UNSIGNED_BYTE    ? 1u
                : type == GL_UNSIGNED_SHORT ? 2u
                : type == GL_UNSIGNED_INT   ? 4u
                                            : 0u;
        if (bytes == 0u)
            return 0;
    } else if (type == GL_UNSIGNED_BYTE || type == GL_BYTE)
        bytes = 1u;
    else if (type == GL_UNSIGNED_SHORT || type == GL_SHORT || type == GL_HALF_FLOAT)
        bytes = 2u;
    else if (type == GL_UNSIGNED_INT || type == GL_INT || type == GL_FLOAT)
        bytes = 4u;
    else {
        switch (type) {
            case GL_UNSIGNED_BYTE_3_3_2:
            case GL_UNSIGNED_BYTE_2_3_3_REV:
                bytes = 1u;
                break;
            case GL_UNSIGNED_SHORT_5_6_5:
            case GL_UNSIGNED_SHORT_5_6_5_REV:
            case GL_UNSIGNED_SHORT_4_4_4_4:
            case GL_UNSIGNED_SHORT_4_4_4_4_REV:
            case GL_UNSIGNED_SHORT_5_5_5_1:
            case GL_UNSIGNED_SHORT_1_5_5_5_REV:
                bytes = 2u;
                break;
            case GL_UNSIGNED_INT_8_8_8_8:
            case GL_UNSIGNED_INT_8_8_8_8_REV:
            case GL_UNSIGNED_INT_10_10_10_2:
            case GL_UNSIGNED_INT_2_10_10_10_REV:
            case GL_UNSIGNED_INT_10F_11F_11F_REV:
            case GL_UNSIGNED_INT_5_9_9_9_REV:
                bytes = 4u;
                break;
            default:
                return 0;
        }
        if ((type == GL_UNSIGNED_BYTE_3_3_2 || type == GL_UNSIGNED_BYTE_2_3_3_REV ||
             type == GL_UNSIGNED_SHORT_5_6_5 || type == GL_UNSIGNED_SHORT_5_6_5_REV ||
             type == GL_UNSIGNED_INT_10F_11F_11F_REV || type == GL_UNSIGNED_INT_5_9_9_9_REV)
                ? format != GL_RGB
                : (format != GL_RGBA && format != GL_BGRA))
            return -1;
        *pixel_bytes = bytes;
        *element_bytes = bytes;
        *channels = count;
        return 1;
    }
    *pixel_bytes = (size_t)count * bytes;
    *element_bytes = bytes;
    *channels = count;
    return 1;
}

static double draw_scalar_value(const unsigned char *src, GLenum type) {
    switch (type) {
        case GL_UNSIGNED_BYTE:
            return (double)src[0] / 255.0;
        case GL_BYTE: {
            int8_t v;
            memcpy(&v, src, sizeof(v));
            return fmax(-1.0, (double)v / 127.0);
        }
        case GL_UNSIGNED_SHORT: {
            uint16_t v;
            memcpy(&v, src, sizeof(v));
            return (double)v / 65535.0;
        }
        case GL_SHORT: {
            int16_t v;
            memcpy(&v, src, sizeof(v));
            return fmax(-1.0, (double)v / 32767.0);
        }
        case GL_UNSIGNED_INT: {
            uint32_t v;
            memcpy(&v, src, sizeof(v));
            return (double)v / 4294967295.0;
        }
        case GL_INT: {
            int32_t v;
            memcpy(&v, src, sizeof(v));
            return fmax(-1.0, (double)v / 2147483647.0);
        }
        case GL_HALF_FLOAT: {
            uint16_t v;
            unsigned exponent, fraction, sign;
            memcpy(&v, src, sizeof(v));
            sign = (unsigned)(v >> 15u);
            exponent = ((unsigned)v >> 10u) & 31u;
            fraction = (unsigned)v & 1023u;
            if (exponent == 31u)
                return sign ? -(fraction ? NAN : INFINITY) : (fraction ? NAN : INFINITY);
            return (sign ? -1.0 : 1.0) *
                   (exponent ? ldexp((double)(1024u + fraction), (int)exponent - 25)
                             : ldexp((double)fraction, -24));
        }
        case GL_FLOAT: {
            float v;
            memcpy(&v, src, sizeof(v));
            return (double)v;
        }
        default:
            return 0.0;
    }
}

static double draw_ufloat_value(unsigned bits, unsigned mantissa_bits) {
    unsigned exponent = bits >> mantissa_bits;
    unsigned fraction = bits & ((1u << mantissa_bits) - 1u);
    if (exponent == 31u)
        return fraction ? NAN : INFINITY;
    if (exponent == 0u)
        return ldexp((double)fraction, -14 - (int)mantissa_bits);
    return ldexp((double)((1u << mantissa_bits) + fraction),
                 (int)exponent - 15 - (int)mantissa_bits);
}

static void draw_unpack_color(const unsigned char *pixel, GLenum type, unsigned channels,
                              size_t pixel_bytes, double values[4]) {
    unsigned widths[4] = {0u}, i, bit;
    int reversed = 0;
    uint32_t word = 0u;
    switch (type) {
        case GL_UNSIGNED_BYTE_3_3_2:
            widths[0] = 3u;
            widths[1] = 3u;
            widths[2] = 2u;
            break;
        case GL_UNSIGNED_BYTE_2_3_3_REV:
            widths[0] = 3u;
            widths[1] = 3u;
            widths[2] = 2u;
            reversed = 1;
            break;
        case GL_UNSIGNED_SHORT_5_6_5:
            widths[0] = 5u;
            widths[1] = 6u;
            widths[2] = 5u;
            break;
        case GL_UNSIGNED_SHORT_5_6_5_REV:
            widths[0] = 5u;
            widths[1] = 6u;
            widths[2] = 5u;
            reversed = 1;
            break;
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
            for (i = 0u; i < 4u; ++i)
                widths[i] = 4u;
            reversed = type == GL_UNSIGNED_SHORT_4_4_4_4_REV;
            break;
        case GL_UNSIGNED_SHORT_5_5_5_1:
            widths[0] = widths[1] = widths[2] = 5u;
            widths[3] = 1u;
            break;
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            widths[0] = widths[1] = widths[2] = 5u;
            widths[3] = 1u;
            reversed = 1;
            break;
        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
            for (i = 0u; i < 4u; ++i)
                widths[i] = 8u;
            reversed = type == GL_UNSIGNED_INT_8_8_8_8_REV;
            break;
        case GL_UNSIGNED_INT_10_10_10_2:
            widths[0] = widths[1] = widths[2] = 10u;
            widths[3] = 2u;
            break;
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            widths[0] = widths[1] = widths[2] = 10u;
            widths[3] = 2u;
            reversed = 1;
            break;
        default:
            break;
    }
    memcpy(&word, pixel, pixel_bytes < sizeof(word) ? pixel_bytes : sizeof(word));
    if (type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
        values[0] = draw_ufloat_value(word & 0x7ffu, 6u);
        values[1] = draw_ufloat_value((word >> 11u) & 0x7ffu, 6u);
        values[2] = draw_ufloat_value((word >> 22u) & 0x3ffu, 5u);
    } else if (type == GL_UNSIGNED_INT_5_9_9_9_REV) {
        for (i = 0u; i < 3u; ++i)
            values[i] = ldexp((double)((word >> (i * 9u)) & 0x1ffu), (int)(word >> 27u) - 24);
    } else if (widths[0] != 0u) {
        bit = reversed ? 0u : (unsigned)pixel_bytes * 8u;
        for (i = 0u; i < channels; ++i) {
            unsigned shift;
            if (reversed) {
                shift = bit;
                bit += widths[i];
            } else {
                bit -= widths[i];
                shift = bit;
            }
            values[i] = (double)((word >> shift) & ((1u << widths[i]) - 1u)) /
                        (double)((1u << widths[i]) - 1u);
        }
    } else {
        size_t element = pixel_bytes / channels;
        for (i = 0u; i < channels; ++i)
            values[i] = draw_scalar_value(pixel + (size_t)i * element, type);
    }
}

static unsigned char *unpack_pixels_tight(struct mgl_context *ctx, GLsizei width, GLsizei height,
                                          GLenum format, GLenum type, const void *pixels,
                                          size_t *out_bytes) {
    size_t pixel_bytes, element_bytes, tight_bytes;
    unsigned comps;
    int valid;
    struct mgl_pixel_layout layout;
    const unsigned char *src;
    unsigned char *tight;
    GLsizei y;
    valid = draw_pixel_info(format, type, &pixel_bytes, &element_bytes, &comps);
    if (valid != 1)
        return NULL;
    if (!mgl_pixel_layout_2d(width, height, pixel_bytes, ctx->unpack_alignment,
                             ctx->unpack_row_length, ctx->unpack_skip_pixels, ctx->unpack_skip_rows,
                             &layout) ||
        !checked_mul(layout.row_bytes, (size_t)height, &tight_bytes))
        return NULL;
    if (layout.span_bytes != 0u && pixels == NULL && ctx->pixel_unpack_buffer == 0u)
        return NULL;
    src = unpack_source(ctx, pixels, layout.span_bytes);
    if (layout.span_bytes != 0u && src == NULL)
        return NULL;
    if (src != NULL)
        src += layout.start_offset;
    tight = malloc(tight_bytes != 0u ? tight_bytes : 1u);
    if (tight == NULL)
        return NULL;
    if (tight_bytes == 0u) {
        *out_bytes = 0u;
        return tight;
    }
    for (y = 0; y < height; ++y)
        memcpy(tight + (size_t)y * layout.row_bytes, src + (size_t)y * layout.row_stride,
               layout.row_bytes);
    if (ctx->unpack_swap_bytes && element_bytes > 1u)
        swap_elements(tight, tight_bytes, element_bytes);
    *out_bytes = tight_bytes;
    return tight;
}

static void draw_apply_transfer(struct mgl_context *ctx, double rgba[4]) {
    const GLfloat scales[4] = {ctx->pixel_transfer.red_scale, ctx->pixel_transfer.green_scale,
                               ctx->pixel_transfer.blue_scale, ctx->pixel_transfer.alpha_scale};
    const GLfloat biases[4] = {ctx->pixel_transfer.red_bias, ctx->pixel_transfer.green_bias,
                               ctx->pixel_transfer.blue_bias, ctx->pixel_transfer.alpha_bias};
    unsigned i;
    for (i = 0u; i < 4u; ++i)
        rgba[i] = rgba[i] * (double)scales[i] + (double)biases[i];
    if (ctx->pixel_transfer.map_color) {
        for (i = 0u; i < 4u; ++i) {
            unsigned slot = pixel_map_slot(GL_PIXEL_MAP_R_TO_R) + i;
            rgba[i] = pixel_map_lookup_component(ctx, slot, normalized_byte(rgba[i]));
        }
    }
}

static void *tight_pixels_to_rgba(struct mgl_context *ctx, GLsizei width, GLsizei height,
                                  GLenum format, GLenum type, const void *pixels, size_t data_size,
                                  size_t *out_bytes, GLenum *out_type) {
    size_t pixel_bytes, element_bytes, row, expected, pixel_count, dst_bytes;
    unsigned comps;
    int valid, as_float;
    const unsigned char *src = (const unsigned char *)pixels;
    unsigned char *dst;
    size_t y;
    valid = draw_pixel_info(format, type, &pixel_bytes, &element_bytes, &comps);
    if (valid != 1)
        return NULL;
    if (!checked_mul((size_t)width, pixel_bytes, &row) ||
        !checked_mul(row, (size_t)height, &expected) || expected != data_size)
        return NULL;
    as_float = type != GL_UNSIGNED_BYTE && format != GL_COLOR_INDEX;
    if (!checked_mul((size_t)width, (size_t)height, &pixel_count) ||
        !checked_mul(pixel_count, as_float ? 4u * sizeof(float) : 4u, &dst_bytes))
        return NULL;
    if (expected != 0u && src == NULL)
        return NULL;
    dst = malloc(dst_bytes != 0u ? dst_bytes : 1u);
    if (dst == NULL)
        return NULL;
    for (y = 0u; y < (size_t)height; ++y) {
        size_t x;
        const unsigned char *row_src = src + y * row;
        unsigned char *row_dst = dst + y * (size_t)width * (as_float ? 4u * sizeof(float) : 4u);
        for (x = 0u; x < (size_t)width; ++x) {
            double rgba[4] = {0.0, 0.0, 0.0, 1.0};
            const unsigned char *pixel = row_src + x * pixel_bytes;
            if (format == GL_COLOR_INDEX) {
                GLuint index;
                GLint64 shifted;
                unsigned i;
                if (type == GL_UNSIGNED_BYTE)
                    index = pixel[0];
                else if (type == GL_UNSIGNED_SHORT) {
                    GLushort v;
                    memcpy(&v, pixel, sizeof(v));
                    index = v;
                } else {
                    GLuint v;
                    memcpy(&v, pixel, sizeof(v));
                    index = v;
                }
                shifted = (GLint64)index;
                if (ctx->pixel_transfer.index_shift > 0 && ctx->pixel_transfer.index_shift < 32)
                    shifted <<= (unsigned)ctx->pixel_transfer.index_shift;
                else if (ctx->pixel_transfer.index_shift < 0 &&
                         ctx->pixel_transfer.index_shift > -32)
                    shifted >>= (unsigned)(-ctx->pixel_transfer.index_shift);
                shifted += (GLint64)ctx->pixel_transfer.index_offset;
                if (shifted < 0)
                    shifted = 0;
                index = (GLuint)shifted;
                if (ctx->pixel_transfer.map_color)
                    index = (GLuint)pixel_map_lookup_index(ctx, pixel_map_slot(GL_PIXEL_MAP_I_TO_I),
                                                           index);
                for (i = 0u; i < 4u; ++i)
                    rgba[i] =
                        pixel_map_lookup_index(ctx, pixel_map_slot(GL_PIXEL_MAP_I_TO_R) + i, index);
            } else {
                double components[4] = {0.0, 0.0, 0.0, 1.0};
                draw_unpack_color(pixel, type, comps, pixel_bytes, components);
                switch (format) {
                    case GL_RGBA:
                    case GL_RGB:
                    case GL_RG:
                    case GL_RED:
                        rgba[0] = components[0];
                        if (comps > 1u)
                            rgba[1] = components[1];
                        if (comps > 2u)
                            rgba[2] = components[2];
                        if (comps > 3u)
                            rgba[3] = components[3];
                        break;
                    case GL_BGRA:
                    case GL_BGR:
                        rgba[0] = components[2];
                        rgba[1] = components[1];
                        rgba[2] = components[0];
                        if (comps == 4u)
                            rgba[3] = components[3];
                        break;
                    case GL_GREEN:
                        rgba[1] = components[0];
                        break;
                    case GL_BLUE:
                        rgba[2] = components[0];
                        break;
                    case GL_ALPHA:
                        rgba[3] = components[0];
                        break;
                    case GL_LUMINANCE:
                        rgba[0] = rgba[1] = rgba[2] = components[0];
                        break;
                    case GL_LUMINANCE_ALPHA:
                        rgba[0] = rgba[1] = rgba[2] = components[0];
                        rgba[3] = components[1];
                        break;
                    default:
                        break;
                }
                draw_apply_transfer(ctx, rgba);
            }
            if (as_float) {
                float out[4] = {(float)rgba[0], (float)rgba[1], (float)rgba[2], (float)rgba[3]};
                memcpy(row_dst + x * sizeof(out), out, sizeof(out));
            } else {
                unsigned i;
                for (i = 0u; i < 4u; ++i)
                    row_dst[x * 4u + i] = normalized_byte(rgba[i]);
            }
        }
    }
    *out_bytes = dst_bytes;
    *out_type = as_float ? GL_FLOAT : GL_UNSIGNED_BYTE;
    return dst;
}

static GLuint compile_shader(struct mgl_context *ctx, GLenum type, const GLchar *source) {
    GLuint shader;
    GLint ok = 0;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesCreateShader == NULL ||
        ctx->native_dispatch->glesShaderSource == NULL ||
        ctx->native_dispatch->glesCompileShader == NULL ||
        ctx->native_dispatch->glesGetShaderiv == NULL)
        return 0u;
    shader = ctx->native_dispatch->glesCreateShader(type);
    if (shader == 0u)
        return 0u;
    ctx->native_dispatch->glesShaderSource(shader, 1, &source, NULL);
    ctx->native_dispatch->glesCompileShader(shader);
    ctx->native_dispatch->glesGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        if (ctx->native_dispatch->glesDeleteShader != NULL)
            ctx->native_dispatch->glesDeleteShader(shader);
        return 0u;
    }
    return shader;
}

static int alpha_func_code(GLenum func) {
    switch (func) {
        case GL_NEVER:
            return 0;
        case GL_LESS:
            return 1;
        case GL_EQUAL:
            return 2;
        case GL_LEQUAL:
            return 3;
        case GL_GREATER:
            return 4;
        case GL_NOTEQUAL:
            return 5;
        case GL_GEQUAL:
            return 6;
        default:
            return 7;
    }
}

static GLuint framebuffer_native(struct mgl_context *ctx, GLuint name) {
    struct mgl_framebuffer *fb = mgl_framebuffer_find(ctx, name);
    return fb != NULL ? fb->native_name : 0u;
}

static GLuint buffer_native(struct mgl_context *ctx, GLuint name) {
    struct mgl_buffer *buffer = mgl_buffer_find(ctx, name);
    return buffer != NULL ? buffer->native_name : 0u;
}

static void set_cap(struct mgl_context *ctx, GLenum cap, GLboolean enabled) {
    if (ctx->native_dispatch == NULL)
        return;
    if (enabled) {
        if (ctx->native_dispatch->glesEnable != NULL)
            ctx->native_dispatch->glesEnable(cap);
    } else if (ctx->native_dispatch->glesDisable != NULL) {
        ctx->native_dispatch->glesDisable(cap);
    }
}

static void restore_native(struct mgl_context *ctx) {
    struct mgl_program *program = mgl_program_find(ctx, ctx->current_program);
    if (ctx->native_dispatch == NULL)
        return;

    if (ctx->native_dispatch->glesBindFramebuffer != NULL) {
        if (ctx->draw_framebuffer == ctx->read_framebuffer) {
            ctx->native_dispatch->glesBindFramebuffer(
                GL_FRAMEBUFFER, framebuffer_native(ctx, ctx->draw_framebuffer));
        } else {
            ctx->native_dispatch->glesBindFramebuffer(
                GL_DRAW_FRAMEBUFFER, framebuffer_native(ctx, ctx->draw_framebuffer));
            ctx->native_dispatch->glesBindFramebuffer(
                GL_READ_FRAMEBUFFER, framebuffer_native(ctx, ctx->read_framebuffer));
        }
    }
    if (ctx->native_dispatch->glesViewport != NULL)
        ctx->native_dispatch->glesViewport(
            ctx->canonical_state.viewport[0], ctx->canonical_state.viewport[1],
            ctx->canonical_state.viewport[2], ctx->canonical_state.viewport[3]);
    if (ctx->native_dispatch->glesScissor != NULL)
        ctx->native_dispatch->glesScissor(
            ctx->canonical_state.scissor[0], ctx->canonical_state.scissor[1],
            ctx->canonical_state.scissor[2], ctx->canonical_state.scissor[3]);

    set_cap(ctx, GL_SCISSOR_TEST, ctx->canonical_state.scissor_test);
    set_cap(ctx, GL_DEPTH_TEST, ctx->canonical_state.depth_test);
    set_cap(ctx, GL_STENCIL_TEST, ctx->canonical_state.stencil_test);
    set_cap(ctx, GL_BLEND, ctx->canonical_state.blend);
    set_cap(ctx, GL_CULL_FACE, ctx->canonical_state.cull_face);
    set_cap(ctx, GL_DITHER, ctx->canonical_state.dither);

    if (ctx->native_dispatch->glesColorMask != NULL)
        ctx->native_dispatch->glesColorMask(
            ctx->canonical_state.color_mask[0], ctx->canonical_state.color_mask[1],
            ctx->canonical_state.color_mask[2], ctx->canonical_state.color_mask[3]);
    if (ctx->native_dispatch->glesDepthMask != NULL)
        ctx->native_dispatch->glesDepthMask(ctx->canonical_state.depth_mask);
    if (ctx->native_dispatch->glesUseProgram != NULL)
        mgl_draw_use_native_program(ctx, program != NULL ? program->native_name : 0u);
    if (ctx->native_dispatch->glesBindVertexArray != NULL)
        ctx->native_dispatch->glesBindVertexArray(
            ctx->current_vao != NULL ? ctx->current_vao->native_name : 0u);
    if (ctx->native_dispatch->glesPixelStorei != NULL) {
        ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, ctx->pack_alignment);
        ctx->native_dispatch->glesPixelStorei(GL_UNPACK_ALIGNMENT, ctx->unpack_alignment);
    }
    if (ctx->native_dispatch->glesBindBuffer != NULL) {
        mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_PACK_BUFFER,
                                        buffer_native(ctx, ctx->pixel_pack_buffer));
        mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_UNPACK_BUFFER,
                                        buffer_native(ctx, ctx->pixel_unpack_buffer));
    }
    mgl_texture_realize_bindings(ctx);
}

static int draw_rgba(struct mgl_context *ctx, GLsizei width, GLsizei height, const void *rgba,
                     GLenum pixel_type, int bitmap) {
    GLuint tex = 0u, vs = 0u, fs = 0u, program = 0u;
    GLint ok = 0, loc;
    int result = 0;

    if (!ctx->compat_raster.valid || width == 0 || height == 0 ||
        ctx->pixel_transfer.pixel_zoom_x == 0.0f || ctx->pixel_transfer.pixel_zoom_y == 0.0f)
        return 1;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGenTextures == NULL ||
        ctx->native_dispatch->glesBindTexture == NULL ||
        ctx->native_dispatch->glesTexImage2D == NULL ||
        ctx->native_dispatch->glesTexParameteri == NULL ||
        ctx->native_dispatch->glesCreateProgram == NULL ||
        ctx->native_dispatch->glesAttachShader == NULL ||
        ctx->native_dispatch->glesLinkProgram == NULL ||
        ctx->native_dispatch->glesGetProgramiv == NULL ||
        ctx->native_dispatch->glesUseProgram == NULL ||
        ctx->native_dispatch->glesGetUniformLocation == NULL ||
        ctx->native_dispatch->glesUniform1i == NULL ||
        ctx->native_dispatch->glesUniform1f == NULL ||
        ctx->native_dispatch->glesUniform4f == NULL || ctx->native_dispatch->glesDrawArrays == NULL)
        return 0;

    vs = compile_shader(ctx, GL_VERTEX_SHADER, raster_vs);
    fs = compile_shader(ctx, GL_FRAGMENT_SHADER, raster_fs);
    if (vs == 0u || fs == 0u)
        goto done;
    program = ctx->native_dispatch->glesCreateProgram();
    if (program == 0u)
        goto done;
    ctx->native_dispatch->glesAttachShader(program, vs);
    ctx->native_dispatch->glesAttachShader(program, fs);
    ctx->native_dispatch->glesLinkProgram(program);
    ctx->native_dispatch->glesGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
        goto done;

    ctx->native_dispatch->glesGenTextures(1, &tex);
    if (tex == 0u)
        goto done;
    if (ctx->native_dispatch->glesActiveTexture != NULL)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0);
    if (ctx->native_dispatch->glesBindBuffer != NULL)
        mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_UNPACK_BUFFER, 0u);
    if (ctx->native_dispatch->glesPixelStorei != NULL)
        ctx->native_dispatch->glesPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    mgl_texture_bind_native(ctx, GL_TEXTURE_2D, tex);
    ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                            (GLint)GL_NEAREST);
    ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                            (GLint)GL_NEAREST);
    ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                            (GLint)GL_CLAMP_TO_EDGE);
    ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                            (GLint)GL_CLAMP_TO_EDGE);
    ctx->native_dispatch->glesTexImage2D(
        GL_TEXTURE_2D, 0, pixel_type == GL_FLOAT ? (GLint)GL_RGBA32F : (GLint)GL_RGBA8, width,
        height, 0, GL_RGBA, pixel_type, rgba);

    if (ctx->native_dispatch->glesBindFramebuffer != NULL)
        ctx->native_dispatch->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                  framebuffer_native(ctx, ctx->draw_framebuffer));
    mgl_draw_use_native_program(ctx, program);
    if (ctx->native_dispatch->glesBindVertexArray != NULL)
        ctx->native_dispatch->glesBindVertexArray(0u);
    ctx->draw_enabled_mask_valid = GL_FALSE;
    ctx->draw_native_pointer_valid = 0u;

    loc = ctx->native_dispatch->glesGetUniformLocation(program, "uPixels");
    if (loc >= 0)
        ctx->native_dispatch->glesUniform1i(loc, 0);
    loc = ctx->native_dispatch->glesGetUniformLocation(program, "uRect");
    if (loc >= 0)
        ctx->native_dispatch->glesUniform4f(loc, ctx->compat_raster.position[0],
                                            ctx->compat_raster.position[1],
                                            (GLfloat)width * ctx->pixel_transfer.pixel_zoom_x,
                                            (GLfloat)height * ctx->pixel_transfer.pixel_zoom_y);
    loc = ctx->native_dispatch->glesGetUniformLocation(program, "uViewport");
    if (loc >= 0)
        ctx->native_dispatch->glesUniform4f(loc, (GLfloat)ctx->canonical_state.viewport[0],
                                            (GLfloat)ctx->canonical_state.viewport[1],
                                            (GLfloat)ctx->canonical_state.viewport[2],
                                            (GLfloat)ctx->canonical_state.viewport[3]);
    loc = ctx->native_dispatch->glesGetUniformLocation(program, "uDepth");
    if (loc >= 0)
        ctx->native_dispatch->glesUniform1f(loc, ctx->compat_raster.position[2]);
    loc = ctx->native_dispatch->glesGetUniformLocation(program, "uBitmap");
    if (loc >= 0)
        ctx->native_dispatch->glesUniform1i(loc, bitmap);
    loc = ctx->native_dispatch->glesGetUniformLocation(program, "uRasterColor");
    if (loc >= 0)
        ctx->native_dispatch->glesUniform4f(
            loc, ctx->compat_raster.color[0], ctx->compat_raster.color[1],
            ctx->compat_raster.color[2], ctx->compat_raster.color[3]);
    loc = ctx->native_dispatch->glesGetUniformLocation(program, "uAlphaEnabled");
    if (loc >= 0)
        ctx->native_dispatch->glesUniform1i(loc, ctx->canonical_state.alpha_test ? 1 : 0);
    loc = ctx->native_dispatch->glesGetUniformLocation(program, "uAlphaFunc");
    if (loc >= 0)
        ctx->native_dispatch->glesUniform1i(loc, alpha_func_code(ctx->canonical_state.alpha_func));
    loc = ctx->native_dispatch->glesGetUniformLocation(program, "uAlphaRef");
    if (loc >= 0)
        ctx->native_dispatch->glesUniform1f(loc, ctx->canonical_state.alpha_ref);

    set_cap(ctx, GL_CULL_FACE, GL_FALSE);
    {
        int route = mgl_framebuffer_draw_route_begin(ctx);
        if (route < 0)
            goto done;
        ctx->native_dispatch->glesDrawArrays(GL_TRIANGLES, 0, 6);
        if (route > 0)
            mgl_framebuffer_draw_route_end(ctx);
    }
    result = 1;

done:
    if (tex != 0u && ctx->native_dispatch->glesDeleteTextures != NULL)
        ctx->native_dispatch->glesDeleteTextures(1, &tex);
    if (program != 0u)
        ctx->draw_native_program_valid = GL_FALSE;
    if (program != 0u && ctx->native_dispatch->glesDeleteProgram != NULL)
        ctx->native_dispatch->glesDeleteProgram(program);
    if (fs != 0u && ctx->native_dispatch->glesDeleteShader != NULL)
        ctx->native_dispatch->glesDeleteShader(fs);
    if (vs != 0u && ctx->native_dispatch->glesDeleteShader != NULL)
        ctx->native_dispatch->glesDeleteShader(vs);
    restore_native(ctx);
    return result;
}

int mgl_raster_draw_pixels_tight(struct mgl_context *ctx, GLsizei width, GLsizei height,
                                 GLenum format, GLenum type, const void *data, size_t data_size) {
    void *rgba;
    size_t rgba_bytes = 0u;
    GLenum rgba_type = GL_UNSIGNED_BYTE;
    if (ctx == NULL)
        return 0;
    rgba = tight_pixels_to_rgba(ctx, width, height, format, type, data, data_size, &rgba_bytes,
                                &rgba_type);
    (void)rgba_bytes;
    if (rgba == NULL && width != 0 && height != 0)
        return 0;
    if (!draw_rgba(ctx, width, height, rgba, rgba_type, 0)) {
        free(rgba);
        return 0;
    }
    free(rgba);
    return 1;
}

void glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned char *tight;
    size_t tight_bytes = 0u, pixel_bytes = 0u, element_bytes = 0u;
    unsigned channels = 0u;
    int suppress, valid;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    valid = draw_pixel_info(format, type, &pixel_bytes, &element_bytes, &channels);
    if (valid != 1) {
        mgl_error_set(&ctx->errors, valid < 0 ? GL_INVALID_OPERATION : GL_INVALID_ENUM);
        return;
    }
    tight = unpack_pixels_tight(ctx, width, height, format, type, pixels, &tight_bytes);
    if (tight == NULL && width != 0 && height != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    suppress = mgl_list_record_draw_pixels(ctx, width, height, format, type, tight, tight_bytes);
    if (!suppress &&
        !mgl_raster_draw_pixels_tight(ctx, width, height, format, type, tight, tight_bytes))
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    free(tight);
}

int mgl_raster_bitmap_tight(struct mgl_context *ctx, GLsizei width, GLsizei height, GLfloat xorig,
                            GLfloat yorig, GLfloat xmove, GLfloat ymove, const void *data,
                            size_t data_size) {
    size_t row, pixels;
    const unsigned char *src = (const unsigned char *)data;
    unsigned char *rgba;
    GLsizei y;
    if (ctx == NULL)
        return 0;
    if (!ctx->compat_raster.valid)
        return 1;
    row = ((size_t)width + 7u) / 8u;
    if (data_size != row * (size_t)height)
        return 0;
    if (!checked_mul((size_t)width, (size_t)height, &pixels) || pixels > SIZE_MAX / 4u)
        return 0;
    if (data_size != 0u && src == NULL)
        return 0;
    rgba = calloc(pixels != 0u ? pixels * 4u : 1u, 1u);
    if (rgba == NULL)
        return 0;
    for (y = 0; y < height; ++y) {
        size_t x;
        for (x = 0u; x < (size_t)width; ++x)
            if ((src[(size_t)y * row + x / 8u] & (unsigned char)(0x80u >> (x & 7u))) != 0u)
                rgba[((size_t)y * (size_t)width + x) * 4u + 3u] = 255u;
    }
    ctx->compat_raster.position[0] -= xorig;
    ctx->compat_raster.position[1] -= yorig;
    if (!draw_rgba(ctx, width, height, rgba, GL_UNSIGNED_BYTE, 1)) {
        free(rgba);
        return 0;
    }
    ctx->compat_raster.position[0] += xorig + xmove;
    ctx->compat_raster.position[1] += yorig + ymove;
    free(rgba);
    return 1;
}

void glBitmap(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove,
              GLfloat ymove, const GLubyte *bitmap) {
    struct mgl_context *ctx = mgl_context_current();
    size_t row, storage_row, stride, source_bytes, tight_bytes, start_bytes, last_row_bytes;
    size_t bit_skip;
    const unsigned char *src;
    unsigned char *tight;
    GLsizei y;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    row = ((size_t)width + 7u) / 8u;
    storage_row =
        ((size_t)(ctx->unpack_row_length != 0 ? ctx->unpack_row_length : width) + 7u) / 8u;
    stride = aligned_stride(storage_row, ctx->unpack_alignment);
    bit_skip = (size_t)ctx->unpack_skip_pixels;
    {
        size_t skipped_rows;
        if (!checked_mul((size_t)ctx->unpack_skip_rows, stride, &skipped_rows) ||
            !checked_add(skipped_rows, bit_skip / 8u, &start_bytes)) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
    }
    last_row_bytes = (bit_skip % 8u + (size_t)width + 7u) / 8u;
    if (!checked_mul(row, (size_t)height, &tight_bytes)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (width == 0 || height == 0)
        source_bytes = 0u;
    else {
        size_t rows_prefix;
        if (!checked_mul(stride, (size_t)height - 1u, &rows_prefix) ||
            start_bytes > SIZE_MAX - rows_prefix ||
            start_bytes + rows_prefix > SIZE_MAX - last_row_bytes) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        source_bytes = start_bytes + rows_prefix + last_row_bytes;
    }
    src = unpack_source(ctx, bitmap, source_bytes);
    if (source_bytes != 0u && src == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    tight = calloc(tight_bytes != 0u ? tight_bytes : 1u, 1u);
    if (tight == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    for (y = 0; y < height && width > 0; ++y) {
        size_t x;
        const unsigned char *src_row = src + start_bytes + (size_t)y * stride;
        size_t first_bit = bit_skip & 7u;
        for (x = 0u; x < (size_t)width; ++x) {
            size_t source_bit = first_bit + x;
            unsigned char source_mask = ctx->unpack_lsb_first
                                            ? (unsigned char)(0x01u << (source_bit & 7u))
                                            : (unsigned char)(0x80u >> (source_bit & 7u));
            if ((src_row[source_bit / 8u] & source_mask) != 0u)
                tight[(size_t)y * row + x / 8u] |= (unsigned char)(0x80u >> (x & 7u));
        }
    }
    suppress =
        mgl_list_record_bitmap(ctx, width, height, xorig, yorig, xmove, ymove, tight, tight_bytes);
    if (!suppress && !mgl_raster_bitmap_tight(ctx, width, height, xorig, yorig, xmove, ymove, tight,
                                              tight_bytes))
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    free(tight);
}

static unsigned char *pack_destination(struct mgl_context *ctx, void *pixels, size_t bytes,
                                       struct mgl_buffer **buffer, size_t *offset) {
    *buffer = NULL;
    *offset = 0u;
    if (ctx->pixel_pack_buffer != 0u) {
        struct mgl_buffer *b = mgl_buffer_find(ctx, ctx->pixel_pack_buffer);
        size_t off = (size_t)(uintptr_t)pixels;
        if (b == NULL || off > b->size || bytes > b->size - off)
            return NULL;
        *buffer = b;
        *offset = off;
        return mgl_buffer_shadow_ensure(ctx, b) ? b->shadow + off : NULL;
    }
    return (unsigned char *)pixels;
}

static uint16_t pixel_float_to_half(float value) {
    uint32_t bits, sign, exponent, mantissa;
    memcpy(&bits, &value, sizeof(bits));
    sign = (bits >> 16u) & 0x8000u;
    exponent = (bits >> 23u) & 0xffu;
    mantissa = bits & 0x7fffffu;
    if (exponent == 255u)
        return (uint16_t)(sign | 0x7c00u | (mantissa ? 0x200u : 0u));
    if (exponent > 142u)
        return (uint16_t)(sign | 0x7c00u);
    if (exponent < 102u)
        return (uint16_t)sign;
    if (exponent < 113u) {
        uint32_t shift = 126u - exponent;
        uint32_t rounded = mantissa | 0x800000u;
        uint32_t rem = rounded & ((1u << shift) - 1u);
        uint32_t half = rounded >> shift;
        uint32_t midpoint = 1u << (shift - 1u);
        if (rem > midpoint || (rem == midpoint && (half & 1u)))
            ++half;
        return (uint16_t)(sign | half);
    }
    {
        uint32_t result = ((exponent - 112u) << 10u) | (mantissa >> 13u);
        uint32_t remainder = mantissa & 0x1fffu;
        if (remainder > 0x1000u || (remainder == 0x1000u && (result & 1u)))
            ++result;
        return (uint16_t)(sign | result);
    }
}

static GLenum read_attachment_internalformat(const struct mgl_context *ctx) {
    struct mgl_framebuffer *fb;
    struct mgl_texture *texture;
    struct mgl_renderbuffer *renderbuffer;
    GLuint tex, rb;
    GLenum internal = 0u;
    GLint level;
    unsigned index;
    if (ctx == NULL || ctx->read_framebuffer == 0u)
        return 0u;
    fb = mgl_framebuffer_find((struct mgl_context *)ctx, ctx->read_framebuffer);
    if (fb == NULL || fb->read_buffer < GL_COLOR_ATTACHMENT0 ||
        fb->read_buffer >= GL_COLOR_ATTACHMENT0 + MGL_MAX_DRAW_BUFFERS)
        return 0u;
    index = (unsigned)(fb->read_buffer - GL_COLOR_ATTACHMENT0);
    tex = index == 0u ? fb->color_texture : fb->extra_color_textures[index];
    rb = index == 0u ? fb->color_renderbuffer : fb->extra_color_renderbuffers[index];
    level = index == 0u ? fb->color_texture_level : fb->extra_color_levels[index];
    if (tex != 0u) {
        texture = mgl_texture_find((struct mgl_context *)ctx, tex);
        if (texture != NULL && level >= 0 && level < (GLint)MGL_MAX_TEXTURE_LEVELS) {
            GLenum target = index == 0u ? fb->color_texture_target : fb->extra_color_targets[index];
            if (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X &&
                target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
                internal = texture->cube_levels[target - GL_TEXTURE_CUBE_MAP_POSITIVE_X][level]
                               .internal_format;
            else
                internal = texture->levels[level].internal_format;
        }
    } else if (rb != 0u) {
        renderbuffer = mgl_renderbuffer_find((struct mgl_context *)ctx, rb);
        if (renderbuffer != NULL)
            internal = renderbuffer->internal_format;
    }
    return internal;
}

struct mgl_attachment_color_info {
    GLenum read_type;
    int integer_sign;
};

static int native_attachment_color_info(struct mgl_context *ctx,
                                        struct mgl_attachment_color_info *info) {
    struct mgl_native_dispatch *native = ctx->native_dispatch;
    struct mgl_framebuffer *fb;
    GLint saved = 0, component = 0, bits[4] = {0};
    GLenum attachment, error = GL_NO_ERROR;
    const GLenum bit_queries[4] = {
        GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
        GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE};
    unsigned i;
    if (native == NULL || native->glesGetFramebufferAttachmentParameteriv == NULL ||
        native->glesGetIntegerv == NULL || native->glesBindFramebuffer == NULL ||
        native->glesGetError == NULL)
        return 0;
    fb = mgl_framebuffer_find(ctx, ctx->read_framebuffer);
    if (ctx->read_framebuffer != 0u && (fb == NULL || fb->read_buffer == GL_NONE))
        return 0;
    if (ctx->read_framebuffer == 0u && ctx->default_read_buffer == GL_NONE)
        return 0;
    attachment = fb != NULL                              ? fb->read_buffer
                 : ctx->native_default_framebuffer != 0u ? GL_COLOR_ATTACHMENT0
                                                         : GL_BACK;
    mgl_texture_refresh_current_framebuffers(ctx);
    native->glesGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &saved);
    while (native->glesGetError() != GL_NO_ERROR) {}
    native->glesBindFramebuffer(GL_READ_FRAMEBUFFER, fb != NULL ? fb->native_name : 0u);
    native->glesGetFramebufferAttachmentParameteriv(
        GL_READ_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &component);
    for (i = 0u; i < 4u; ++i)
        native->glesGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, attachment,
                                                        bit_queries[i], &bits[i]);
    error = native->glesGetError();
    if (native->glesBindFramebufferRaw != NULL)
        native->glesBindFramebufferRaw(GL_READ_FRAMEBUFFER, (GLuint)saved);
    else
        native->glesBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)saved);
    if (error != GL_NO_ERROR)
        return -1;
    switch ((GLenum)component) {
        case GL_FLOAT:
            info->read_type = GL_FLOAT;
            return 1;
        case GL_SIGNED_NORMALIZED: {
            GLint max_bits = bits[0];
            for (i = 1u; i < 4u; ++i)
                if (bits[i] > max_bits)
                    max_bits = bits[i];
            if (max_bits <= 0 || max_bits > 16)
                return -1;
            info->read_type = max_bits > 8 ? GL_SHORT : GL_BYTE;
            return 1;
        }
        case GL_INT:
            info->integer_sign = -1;
            info->read_type = GL_INT;
            return 1;
        case GL_UNSIGNED_INT:
            info->integer_sign = 1;
            info->read_type = GL_UNSIGNED_INT;
            return 1;
        case GL_UNSIGNED_NORMALIZED: {
            GLint max_bits = bits[0];
            for (i = 1u; i < 4u; ++i)
                if (bits[i] > max_bits)
                    max_bits = bits[i];
            if (max_bits <= 0 || max_bits > 16)
                return -1;
            if (bits[0] == 10 && bits[1] == 10 && bits[2] == 10 && bits[3] == 2)
                info->read_type = GL_UNSIGNED_INT_2_10_10_10_REV;
            else if (max_bits > 8)
                info->read_type = GL_UNSIGNED_SHORT;
            else
                info->read_type = GL_UNSIGNED_BYTE;
            return 1;
        }
        default:
            return -1;
    }
}

static int read_attachment_color_info(struct mgl_context *ctx,
                                      struct mgl_attachment_color_info *info) {
    GLenum internal = read_attachment_internalformat(ctx);
    int native_result;
    info->read_type = GL_UNSIGNED_BYTE;
    info->integer_sign = 0;
    switch (internal) {
        case GL_RGBA16F:
        case GL_RGBA32F:
        case GL_RGB16F:
        case GL_RGB32F_ARB:
        case GL_R11F_G11F_B10F:
        case GL_R16F:
        case GL_R32F:
        case GL_RG16F:
        case GL_RG32F:
            info->read_type = GL_FLOAT;
            return 1;
        case GL_R8_SNORM:
        case GL_RG8_SNORM:
        case GL_RGB8_SNORM:
        case GL_RGBA8_SNORM:
            info->read_type = GL_BYTE;
            return 1;
        case GL_R16_SNORM:
        case GL_RG16_SNORM:
        case GL_RGB16_SNORM:
        case GL_RGBA16_SNORM:
            info->read_type = GL_SHORT;
            return 1;
        case GL_R16:
        case GL_RG16:
        case GL_RGB16:
        case GL_RGBA16:
            info->read_type = GL_UNSIGNED_SHORT;
            return 1;
        case GL_RGB10_A2:
            info->read_type = GL_UNSIGNED_INT_2_10_10_10_REV;
            return 1;
        case GL_R8I:
        case GL_R16I:
        case GL_R32I:
        case GL_RG8I:
        case GL_RG16I:
        case GL_RG32I:
        case GL_RGB8I:
        case GL_RGB16I:
        case GL_RGB32I:
        case GL_RGBA8I:
        case GL_RGBA16I:
        case GL_RGBA32I:
            info->integer_sign = -1;
            info->read_type = GL_INT;
            return 1;
        case GL_R8UI:
        case GL_R16UI:
        case GL_R32UI:
        case GL_RG8UI:
        case GL_RG16UI:
        case GL_RG32UI:
        case GL_RGB8UI:
        case GL_RGB16UI:
        case GL_RGB32UI:
        case GL_RGBA8UI:
        case GL_RGBA16UI:
        case GL_RGBA32UI:
        case GL_RGB10_A2UI:
            info->integer_sign = 1;
            info->read_type = GL_UNSIGNED_INT;
            return 1;
        case GL_R8:
        case GL_RG8:
        case GL_RGB8:
        case GL_RGBA8:
        case GL_RGB565:
        case GL_RGBA4:
        case GL_RGB5_A1:
        case GL_SRGB8:
        case GL_SRGB8_ALPHA8:
            return 1;
        default:
            break;
    }
    native_result = native_attachment_color_info(ctx, info);
    if (native_result < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    return 1;
}

int mgl_read_color_for_copy(struct mgl_context *ctx, GLint x, GLint y, GLsizei width,
                            GLsizei height, void **out_rgba, size_t *out_size, GLenum *out_type) {
    struct mgl_attachment_color_info source;
    GLenum native_type;
    size_t pixels;
    if (ctx == NULL || out_rgba == NULL || out_size == NULL || out_type == NULL || width < 0 ||
        height < 0 || (size_t)width > SIZE_MAX / ((size_t)height ? (size_t)height : 1u))
        return 0;
    if (!read_attachment_color_info(ctx, &source))
        return 0;
    if (source.integer_sign != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    native_type = source.read_type;
    pixels = (size_t)width * (size_t)height;
    *out_rgba = NULL;
    *out_size = 0u;
    if (native_type == GL_UNSIGNED_BYTE) {
        unsigned char *bytes = NULL;
        if (!mgl_read_color_rgba(ctx, x, y, width, height, &bytes, out_size))
            return 0;
        *out_rgba = bytes;
        *out_type = GL_UNSIGNED_BYTE;
        return 1;
    }
    if ((native_type == GL_UNSIGNED_SHORT || native_type == GL_SHORT) &&
        !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch, "GL_EXT_texture_norm16")) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (pixels > SIZE_MAX / (4u * sizeof(float))) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    {
        float *values = NULL;
        if (!mgl_read_color_rgba_precise(ctx, x, y, width, height, native_type, &values))
            return 0;
        *out_rgba = values;
        *out_size = pixels * 4u * sizeof(float);
        *out_type = GL_FLOAT;
    }
    return 1;
}

static unsigned read_integer_components(GLenum format) {
    switch (format) {
        case GL_RED_INTEGER:
        case GL_GREEN_INTEGER:
        case GL_BLUE_INTEGER:
            return 1u;
        case GL_RG_INTEGER:
            return 2u;
        case GL_RGB_INTEGER:
        case GL_BGR_INTEGER:
            return 3u;
        case GL_RGBA_INTEGER:
        case GL_BGRA_INTEGER:
            return 4u;
        default:
            return 0u;
    }
}

static unsigned read_color_channels(GLenum format, unsigned indices[4]) {
    switch (format) {
        case GL_RED:
        case GL_RED_INTEGER:
        case GL_LUMINANCE:
            indices[0] = 0u;
            return 1u;
        case GL_GREEN:
        case GL_GREEN_INTEGER:
            indices[0] = 1u;
            return 1u;
        case GL_BLUE:
        case GL_BLUE_INTEGER:
            indices[0] = 2u;
            return 1u;
        case GL_ALPHA:
            indices[0] = 3u;
            return 1u;
        case GL_RG:
        case GL_RG_INTEGER:
            indices[0] = 0u;
            indices[1] = 1u;
            return 2u;
        case GL_LUMINANCE_ALPHA:
            indices[0] = 0u;
            indices[1] = 3u;
            return 2u;
        case GL_RGB:
        case GL_RGB_INTEGER:
            indices[0] = 0u;
            indices[1] = 1u;
            indices[2] = 2u;
            return 3u;
        case GL_BGR:
        case GL_BGR_INTEGER:
            indices[0] = 2u;
            indices[1] = 1u;
            indices[2] = 0u;
            return 3u;
        case GL_RGBA:
        case GL_RGBA_INTEGER:
            indices[0] = 0u;
            indices[1] = 1u;
            indices[2] = 2u;
            indices[3] = 3u;
            return 4u;
        case GL_BGRA:
        case GL_BGRA_INTEGER:
            indices[0] = 2u;
            indices[1] = 1u;
            indices[2] = 0u;
            indices[3] = 3u;
            return 4u;
        default:
            return 0u;
    }
}

static size_t read_color_pixel_size(GLenum format, GLenum type) {
    unsigned indices[4] = {0u};
    unsigned components = read_color_channels(format, indices);
    unsigned is_integer = read_integer_components(format) != 0u;
    if (components == 0u)
        return 0u;
    switch (type) {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return (size_t)components;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_HALF_FLOAT:
            return is_integer && type == GL_HALF_FLOAT ? 0u : (size_t)components * 2u;
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_FLOAT:
            return is_integer && type == GL_FLOAT ? 0u : (size_t)components * 4u;
        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_BYTE_2_3_3_REV:
            return !is_integer && format == GL_RGB ? 1u : 0u;
        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
            return !is_integer && format == GL_RGB ? 2u : 0u;
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            return !is_integer && (format == GL_RGBA || format == GL_BGRA) ? 2u : 0u;
        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            return (format == GL_RGBA || format == GL_BGRA ||
                    ((format == GL_RGBA_INTEGER || format == GL_BGRA_INTEGER) &&
                     (type == GL_UNSIGNED_INT_10_10_10_2 ||
                      type == GL_UNSIGNED_INT_2_10_10_10_REV)))
                       ? 4u
                       : 0u;
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
        case GL_UNSIGNED_INT_5_9_9_9_REV:
            return !is_integer && format == GL_RGB ? 4u : 0u;
        default:
            return 0u;
    }
}

static size_t read_integer_component_size(GLenum type) {
    switch (type) {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return 1u;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return 2u;
        case GL_INT:
        case GL_UNSIGNED_INT:
            return 4u;
        default:
            return 0u;
    }
}

static void write_integer_component(unsigned char *dst, GLenum type, int64_t value,
                                    GLboolean swap) {
    size_t size = read_integer_component_size(type);
    size_t i;
    if (type == GL_BYTE) {
        int8_t result = (int8_t)(value < INT8_MIN ? INT8_MIN : value > INT8_MAX ? INT8_MAX : value);
        memcpy(dst, &result, sizeof(result));
    } else if (type == GL_UNSIGNED_BYTE) {
        uint8_t result = (uint8_t)(value < 0 ? 0 : value > UINT8_MAX ? UINT8_MAX : value);
        memcpy(dst, &result, sizeof(result));
    } else if (type == GL_SHORT) {
        int16_t result = (int16_t)(value < INT16_MIN   ? INT16_MIN
                                   : value > INT16_MAX ? INT16_MAX
                                                       : value);
        memcpy(dst, &result, sizeof(result));
    } else if (type == GL_UNSIGNED_SHORT) {
        uint16_t result = (uint16_t)(value < 0 ? 0 : value > UINT16_MAX ? UINT16_MAX : value);
        memcpy(dst, &result, sizeof(result));
    } else if (type == GL_INT) {
        int32_t result = (int32_t)(value < INT32_MIN   ? INT32_MIN
                                   : value > INT32_MAX ? INT32_MAX
                                                       : value);
        memcpy(dst, &result, sizeof(result));
    } else if (type == GL_UNSIGNED_INT) {
        uint32_t result = (uint32_t)(value < 0 ? 0 : value > UINT32_MAX ? UINT32_MAX : value);
        memcpy(dst, &result, sizeof(result));
    }
    if (swap && size > 1u) {
        for (i = 0u; i < size / 2u; ++i) {
            unsigned char tmp = dst[i];
            dst[i] = dst[size - 1u - i];
            dst[size - 1u - i] = tmp;
        }
    }
}

static int read_rgba_integer_native(struct mgl_context *ctx, GLint x, GLint y, GLsizei width,
                                    GLsizei height, int sign, uint32_t **out_values) {
    struct mgl_framebuffer *read_fb;
    uint32_t *values;
    size_t pixels, bytes;
    GLenum native_error = GL_NO_ERROR;
    if (!checked_mul((size_t)width, (size_t)height, &pixels) ||
        !checked_mul(pixels, 4u * sizeof(uint32_t), &bytes)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    values = malloc(bytes != 0u ? bytes : 1u);
    if (values == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesReadPixels == NULL ||
        ctx->native_dispatch->glesBindFramebuffer == NULL ||
        ctx->native_dispatch->glesBindBuffer == NULL ||
        ctx->native_dispatch->glesPixelStorei == NULL) {
        free(values);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    mgl_texture_refresh_current_framebuffers(ctx);
    read_fb = mgl_framebuffer_find(ctx, ctx->read_framebuffer);
    ctx->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER,
                                              read_fb != NULL ? read_fb->native_name : 0u);
    mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_PACK_BUFFER, 0u);
    ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, 1);
    ctx->native_dispatch->glesPixelStorei(GL_PACK_ROW_LENGTH, 0);
    ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_ROWS, 0);
    if (ctx->native_dispatch->glesGetError != NULL)
        while (ctx->native_dispatch->glesGetError() != GL_NO_ERROR) {}
    ctx->native_dispatch->glesReadPixels(x, y, width, height, GL_RGBA_INTEGER,
                                         sign < 0 ? GL_INT : GL_UNSIGNED_INT, values);
    if (ctx->native_dispatch->glesGetError != NULL)
        native_error = ctx->native_dispatch->glesGetError();
    ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, ctx->pack_alignment);
    ctx->native_dispatch->glesPixelStorei(GL_PACK_ROW_LENGTH, ctx->pack_row_length);
    ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_PIXELS, ctx->pack_skip_pixels);
    ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_ROWS, ctx->pack_skip_rows);
    mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_PACK_BUFFER,
                                    buffer_native(ctx, ctx->pixel_pack_buffer));
    if (native_error != GL_NO_ERROR) {
        free(values);
        mgl_render_diag_detail(ctx, "glReadPixels", "native_integer_read_failed",
                               GL_RGBA_INTEGER, sign < 0 ? GL_INT : GL_UNSIGNED_INT);
        mgl_error_set(&ctx->errors, native_error);
        return 0;
    }
    *out_values = values;
    return 1;
}

static size_t read_depth_stencil_pixel_size(GLenum format, GLenum type) {
    if (format == GL_DEPTH_STENCIL) {
        if (type == GL_UNSIGNED_INT_24_8)
            return sizeof(uint32_t);
        if (type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV)
            return 2u * sizeof(uint32_t);
    } else if (format == GL_DEPTH_COMPONENT) {
        if (type == GL_UNSIGNED_SHORT)
            return sizeof(uint16_t);
        if (type == GL_UNSIGNED_INT || type == GL_FLOAT)
            return sizeof(uint32_t);
    } else if (format == GL_STENCIL_INDEX && type == GL_UNSIGNED_BYTE) {
        return sizeof(uint8_t);
    }
    return 0u;
}

static const char *read_depth_stencil_extension(GLenum format) {
    if (format == GL_DEPTH_COMPONENT)
        return "GL_NV_read_depth";
    if (format == GL_STENCIL_INDEX)
        return "GL_NV_read_stencil";
    return "GL_NV_read_depth_stencil";
}

static int read_depth_stencil_native(struct mgl_context *ctx, GLint x, GLint y, GLsizei width,
                                     GLsizei height, GLenum format, GLenum type,
                                     unsigned char **out_values) {
    size_t pixels, bytes, pixel_bytes = read_depth_stencil_pixel_size(format, type);
    unsigned char *values;
    struct mgl_framebuffer *read_fb;
    GLenum native_error = GL_NO_ERROR;
    if (!checked_mul((size_t)width, (size_t)height, &pixels) ||
        !checked_mul(pixels, pixel_bytes, &bytes)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (ctx->native_dispatch == NULL ||
        !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                read_depth_stencil_extension(format)) ||
        ctx->native_dispatch->glesReadPixels == NULL ||
        ctx->native_dispatch->glesBindFramebuffer == NULL ||
        ctx->native_dispatch->glesBindBuffer == NULL ||
        ctx->native_dispatch->glesPixelStorei == NULL) {
        mgl_render_diag_detail(ctx, "glReadPixels", "native_depth_stencil_read_unavailable",
                               format, type);
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    values = malloc(bytes != 0u ? bytes : 1u);
    if (values == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return 0;
    }
    mgl_texture_refresh_current_framebuffers(ctx);
    read_fb = mgl_framebuffer_find(ctx, ctx->read_framebuffer);
    ctx->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER,
                                              read_fb != NULL ? read_fb->native_name : 0u);
    mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_PACK_BUFFER, 0u);
    ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, 1);
    ctx->native_dispatch->glesPixelStorei(GL_PACK_ROW_LENGTH, 0);
    ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_ROWS, 0);
    if (ctx->native_dispatch->glesGetError != NULL)
        while (ctx->native_dispatch->glesGetError() != GL_NO_ERROR) {}
    ctx->native_dispatch->glesReadPixels(x, y, width, height, format, type, values);
    if (ctx->native_dispatch->glesGetError != NULL)
        native_error = ctx->native_dispatch->glesGetError();
    if (native_error != GL_NO_ERROR && format == GL_DEPTH_COMPONENT && type == GL_FLOAT) {
        ctx->native_dispatch->glesReadPixels(x, y, width, height, GL_DEPTH_COMPONENT,
                                             GL_UNSIGNED_INT, values);
        if (ctx->native_dispatch->glesGetError() == GL_NO_ERROR) {
            size_t pixel;
            for (pixel = 0u; pixel < pixels; ++pixel) {
                uint32_t depth_integer;
                float depth_float;
                memcpy(&depth_integer, values + pixel * sizeof(depth_integer), sizeof(depth_integer));
                depth_float = (float)((double)depth_integer / (double)UINT32_MAX);
                memcpy(values + pixel * sizeof(depth_float), &depth_float, sizeof(depth_float));
            }
            native_error = GL_NO_ERROR;
        }
    }
    ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, ctx->pack_alignment);
    ctx->native_dispatch->glesPixelStorei(GL_PACK_ROW_LENGTH, ctx->pack_row_length);
    ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_PIXELS, ctx->pack_skip_pixels);
    ctx->native_dispatch->glesPixelStorei(GL_PACK_SKIP_ROWS, ctx->pack_skip_rows);
    mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_PACK_BUFFER,
                                    buffer_native(ctx, ctx->pixel_pack_buffer));
    if (native_error != GL_NO_ERROR) {
        free(values);
        mgl_render_diag_detail(ctx, "glReadPixels", "native_depth_stencil_read_failed",
                               format, type);
        mgl_error_set(&ctx->errors, native_error);
        return 0;
    }
    *out_values = values;
    return 1;
}

static int valid_read_format_type(GLenum format, GLenum type) {
    if (format == GL_DEPTH_STENCIL || format == GL_DEPTH_COMPONENT || format == GL_STENCIL_INDEX)
        return read_depth_stencil_pixel_size(format, type) != 0u;
    return read_color_pixel_size(format, type) != 0u;
}

static double clamp_read_component(double value, double minimum, double maximum) {
    if (isnan(value))
        return 0.0;
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

static uint32_t quantize_unsigned(double value, unsigned bits) {
    double maximum = (double)((UINT64_C(1) << bits) - 1u);
    return (uint32_t)nearbyint(clamp_read_component(value, 0.0, 1.0) * maximum);
}

static uint32_t pack_unsigned_float(double value, unsigned mantissa_bits) {
    int exponent;
    double fraction;
    uint32_t mantissa_max = 1u << mantissa_bits;
    uint32_t mantissa;
    if (isnan(value))
        return (31u << mantissa_bits) | 1u;
    if (value <= 0.0)
        return 0u;
    if (isinf(value))
        return 31u << mantissa_bits;
    fraction = frexp(value, &exponent);
    exponent += 14;
    if (exponent >= 31)
        return 31u << mantissa_bits;
    if (exponent <= 0) {
        double rounded = nearbyint(ldexp(value, (int)mantissa_bits + 14));
        if (rounded >= (double)mantissa_max)
            return mantissa_max;
        return (uint32_t)rounded;
    }
    mantissa = (uint32_t)nearbyint((fraction * 2.0 - 1.0) * (double)mantissa_max);
    if (mantissa == mantissa_max) {
        ++exponent;
        mantissa = 0u;
    }
    return exponent >= 31 ? 31u << mantissa_bits : ((uint32_t)exponent << mantissa_bits) | mantissa;
}

static uint32_t pack_rgb9e5(const double values[4]) {
    double c[3], maximum, scale;
    int exponent;
    uint32_t encoded[3];
    unsigned i;
    for (i = 0u; i < 3u; ++i)
        c[i] = clamp_read_component(values[i], 0.0, 65408.0);
    maximum = fmax(c[0], fmax(c[1], c[2]));
    if (maximum == 0.0)
        return 0u;
    frexp(maximum, &exponent);
    exponent += 15;
    if (exponent < 0)
        exponent = 0;
    if (exponent > 31)
        exponent = 31;
    scale = ldexp(1.0, exponent - 24);
    if (nearbyint(maximum / scale) >= 512.0 && exponent < 31) {
        ++exponent;
        scale *= 2.0;
    }
    for (i = 0u; i < 3u; ++i) {
        double rounded = nearbyint(c[i] / scale);
        encoded[i] = (uint32_t)(rounded > 511.0 ? 511.0 : rounded);
    }
    return (uint32_t)exponent << 27u | encoded[2] << 18u | encoded[1] << 9u | encoded[0];
}

static void write_read_color_scalar(unsigned char *dst, GLenum type, double value, GLboolean swap) {
    size_t bytes = 1u;
    switch (type) {
        case GL_BYTE: {
            int8_t output =
                (int8_t)nearbyint((clamp_read_component(value, -1.0, 1.0) * 255.0 - 1.0) * 0.5);
            memcpy(dst, &output, sizeof(output));
            break;
        }
        case GL_UNSIGNED_BYTE: {
            uint8_t output = (uint8_t)quantize_unsigned(value, 8u);
            memcpy(dst, &output, sizeof(output));
            break;
        }
        case GL_SHORT: {
            int16_t output =
                (int16_t)nearbyint((clamp_read_component(value, -1.0, 1.0) * 65535.0 - 1.0) * 0.5);
            memcpy(dst, &output, sizeof(output));
            bytes = 2u;
            break;
        }
        case GL_UNSIGNED_SHORT: {
            uint16_t output = (uint16_t)quantize_unsigned(value, 16u);
            memcpy(dst, &output, sizeof(output));
            bytes = 2u;
            break;
        }
        case GL_INT: {
            int32_t output = (int32_t)nearbyint(
                (clamp_read_component(value, -1.0, 1.0) * 4294967295.0 - 1.0) * 0.5);
            memcpy(dst, &output, sizeof(output));
            bytes = 4u;
            break;
        }
        case GL_UNSIGNED_INT: {
            uint32_t output = quantize_unsigned(value, 32u);
            memcpy(dst, &output, sizeof(output));
            bytes = 4u;
            break;
        }
        case GL_HALF_FLOAT: {
            uint16_t output = pixel_float_to_half((float)value);
            memcpy(dst, &output, sizeof(output));
            bytes = 2u;
            break;
        }
        case GL_FLOAT: {
            float output = (float)value;
            memcpy(dst, &output, sizeof(output));
            bytes = 4u;
            break;
        }
        default:
            return;
    }
    if (swap)
        swap_elements(dst, bytes, bytes);
}

static unsigned read_scalar_type_size(GLenum type) {
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

static void write_read_color_packed(unsigned char *dst, GLenum type, const double values[4],
                                    GLboolean swap) {
    unsigned components = 0u, total_bits = 0u, position;
    unsigned widths[4] = {0u};
    uint32_t output = 0u;
    size_t bytes = 4u;
    switch (type) {
        case GL_UNSIGNED_BYTE_3_3_2:
            widths[0] = 3u;
            widths[1] = 3u;
            widths[2] = 2u;
            bytes = 1u;
            components = 3u;
            break;
        case GL_UNSIGNED_BYTE_2_3_3_REV:
            widths[0] = 3u;
            widths[1] = 3u;
            widths[2] = 2u;
            bytes = 1u;
            components = 3u;
            break;
        case GL_UNSIGNED_SHORT_5_6_5:
            widths[0] = 5u;
            widths[1] = 6u;
            widths[2] = 5u;
            bytes = 2u;
            components = 3u;
            break;
        case GL_UNSIGNED_SHORT_5_6_5_REV:
            widths[0] = 5u;
            widths[1] = 6u;
            widths[2] = 5u;
            bytes = 2u;
            components = 3u;
            break;
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
            widths[0] = 4u;
            widths[1] = 4u;
            widths[2] = 4u;
            widths[3] = 4u;
            bytes = 2u;
            components = 4u;
            break;
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            widths[0] = 5u;
            widths[1] = 5u;
            widths[2] = 5u;
            widths[3] = 1u;
            bytes = 2u;
            components = 4u;
            break;
        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
            widths[0] = 8u;
            widths[1] = 8u;
            widths[2] = 8u;
            widths[3] = 8u;
            components = 4u;
            break;
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            widths[0] = 10u;
            widths[1] = 10u;
            widths[2] = 10u;
            widths[3] = 2u;
            components = 4u;
            break;
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
            output = pack_unsigned_float(values[0], 6u) |
                     pack_unsigned_float(values[1], 6u) << 11u |
                     pack_unsigned_float(values[2], 5u) << 22u;
            memcpy(dst, &output, bytes);
            if (swap)
                swap_elements(dst, bytes, bytes);
            return;
        case GL_UNSIGNED_INT_5_9_9_9_REV:
            output = pack_rgb9e5(values);
            memcpy(dst, &output, bytes);
            if (swap)
                swap_elements(dst, bytes, bytes);
            return;
        default:
            return;
    }
    for (position = 0u; position < components; ++position)
        total_bits += widths[position];
    if (type == GL_UNSIGNED_BYTE_2_3_3_REV || type == GL_UNSIGNED_SHORT_5_6_5_REV ||
        type == GL_UNSIGNED_SHORT_4_4_4_4_REV || type == GL_UNSIGNED_SHORT_1_5_5_5_REV ||
        type == GL_UNSIGNED_INT_8_8_8_8_REV || type == GL_UNSIGNED_INT_2_10_10_10_REV) {
        unsigned offset = 0u;
        for (position = 0u; position < components; ++position) {
            unsigned width = widths[position];
            output |= quantize_unsigned(values[position], width) << offset;
            offset += width;
        }
    } else {
        unsigned offset = total_bits;
        for (position = 0u; position < components; ++position) {
            unsigned width = widths[position];
            offset -= width;
            output |= quantize_unsigned(values[position], width) << offset;
        }
    }
    memcpy(dst, &output, bytes);
    if (swap)
        swap_elements(dst, bytes, bytes);
}

void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
                  void *pixels) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned char *rgba = NULL, *dst, *dst_base;
    float *rgba_float = NULL;
    size_t rgba_bytes = 0u, dest_bytes, pixel_bytes, noncolor_pixel_bytes;
    struct mgl_pixel_layout pack_layout;
    struct mgl_buffer *pbo = NULL;
    size_t offset = 0u;
    GLsizei yy;
    int integer_sign;
    struct mgl_attachment_color_info source = {0};
    unsigned integer_components;
    size_t integer_component_bytes;

    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if ((type == GL_UNSIGNED_BYTE_3_3_2 || type == GL_UNSIGNED_BYTE_2_3_3_REV ||
         type == GL_UNSIGNED_SHORT_5_6_5 || type == GL_UNSIGNED_SHORT_5_6_5_REV) &&
        format != GL_RGB) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_read_format_type(format, type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    integer_components = read_integer_components(format);
    integer_component_bytes = read_integer_component_size(type);
    noncolor_pixel_bytes = read_depth_stencil_pixel_size(format, type);
    if (noncolor_pixel_bytes == 0u && !read_attachment_color_info(ctx, &source))
        return;
    integer_sign = source.integer_sign;
    if (noncolor_pixel_bytes == 0u && (integer_sign != 0) != (integer_components != 0u)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (width == 0 || height == 0)
        return;
    pixel_bytes = read_depth_stencil_pixel_size(format, type);
    if (pixel_bytes == 0u)
        pixel_bytes = read_color_pixel_size(format, type);
    if (!mgl_pixel_layout_2d(width, height, pixel_bytes, ctx->pack_alignment, ctx->pack_row_length,
                             ctx->pack_skip_pixels, ctx->pack_skip_rows, &pack_layout)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    dest_bytes = pack_layout.span_bytes;
    dst_base = pack_destination(ctx, pixels, dest_bytes, &pbo, &offset);
    dst = dst_base;
    if (dest_bytes != 0u && dst == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (dest_bytes != 0u)
        dst += pack_layout.start_offset;
    if (noncolor_pixel_bytes != 0u) {
        unsigned char *values = NULL;
        size_t row_index, xx;
        size_t swap_width = format == GL_DEPTH_STENCIL && type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV
                                ? sizeof(uint32_t)
                                : pixel_bytes;
        if (!read_depth_stencil_native(ctx, x, y, width, height, format, type, &values))
            return;
        for (row_index = 0u; row_index < (size_t)height; ++row_index) {
            unsigned char *row = dst + row_index * pack_layout.row_stride;
            const unsigned char *row_source = values + row_index * (size_t)width * pixel_bytes;
            for (xx = 0u; xx < (size_t)width; ++xx) {
                unsigned char *output = row + xx * pixel_bytes;
                memcpy(output, row_source + xx * pixel_bytes, pixel_bytes);
                if (ctx->pack_swap_bytes)
                    swap_elements(output, pixel_bytes, swap_width);
            }
        }
        if (pbo != NULL && dest_bytes != 0u)
            (void)mgl_buffer_write_bytes(ctx, pbo, offset, dest_bytes, dst_base, 0);
        free(values);
        return;
    }
    if (integer_components != 0u) {
        uint32_t *values = NULL;
        if (!read_rgba_integer_native(ctx, x, y, width, height, integer_sign, &values))
            return;
        {
            unsigned indices[4] = {0u};
            unsigned components = read_color_channels(format, indices);
            unsigned scalar_size = (unsigned)integer_component_bytes;
            for (yy = 0; yy < height; ++yy) {
                size_t xx, component;
                unsigned char *row = dst + (size_t)yy * pack_layout.row_stride;
                for (xx = 0u; xx < (size_t)width; ++xx) {
                    double packed[4] = {0.0, 0.0, 0.0, 0.0};
                    for (component = 0u; component < components; ++component) {
                        size_t index = ((size_t)yy * (size_t)width + xx) * 4u + indices[component];
                        int64_t value = integer_sign < 0 ? (int64_t)(int32_t)values[index]
                                                         : (int64_t)values[index];
                        if (scalar_size != 0u)
                            write_integer_component(row + xx * pixel_bytes +
                                                        component * scalar_size,
                                                    type, value, ctx->pack_swap_bytes);
                        else
                            packed[component] = (double)value;
                    }
                    if (scalar_size == 0u) {
                        const unsigned widths[4] = {10u, 10u, 10u, 2u};
                        unsigned bit_offset = type == GL_UNSIGNED_INT_2_10_10_10_REV ? 0u : 32u;
                        uint32_t word = 0u;
                        for (component = 0u; component < 4u; ++component) {
                            uint32_t max = (1u << widths[component]) - 1u;
                            uint32_t part =
                                (uint32_t)clamp_read_component(packed[component], 0.0, (double)max);
                            if (type == GL_UNSIGNED_INT_2_10_10_10_REV) {
                                word |= part << bit_offset;
                                bit_offset += widths[component];
                            } else {
                                bit_offset -= widths[component];
                                word |= part << bit_offset;
                            }
                        }
                        memcpy(row + xx * pixel_bytes, &word, sizeof(word));
                        if (ctx->pack_swap_bytes)
                            swap_elements(row + xx * pixel_bytes, sizeof(word), sizeof(word));
                    }
                }
            }
        }
        if (pbo != NULL && dest_bytes != 0u)
            (void)mgl_buffer_write_bytes(ctx, pbo, offset, dest_bytes, dst_base, 0);
        free(values);
        return;
    }
    {
        uint64_t started = mgl_perf_diag_now();
        int ok;
        GLenum native_type = source.read_type;
        if ((native_type == GL_UNSIGNED_SHORT || native_type == GL_SHORT) &&
            !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                    "GL_EXT_texture_norm16")) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (native_type != GL_UNSIGNED_BYTE)
            ok = mgl_read_color_rgba_precise(ctx, x, y, width, height, native_type, &rgba_float);
        else
            ok = mgl_read_color_rgba(ctx, x, y, width, height, &rgba, &rgba_bytes);
        mgl_perf_diag_extra(ctx, MGL_PERF_READ_PIXELS, started);
        if (!ok)
            return;
    }
    (void)rgba_bytes;

    {
        unsigned indices[4] = {0u};
        unsigned components = read_color_channels(format, indices);
        unsigned scalar_size = read_scalar_type_size(type);
        for (yy = 0; yy < height; ++yy) {
            size_t xx, component;
            unsigned char *row = dst + (size_t)yy * pack_layout.row_stride;
            for (xx = 0u; xx < (size_t)width; ++xx) {
                double values[4] = {0.0, 0.0, 0.0, 1.0};
                for (component = 0u; component < components; ++component) {
                    size_t index = ((size_t)yy * (size_t)width + xx) * 4u + indices[component];
                    double value = rgba_float != NULL ? (double)rgba_float[index]
                                                      : (double)rgba[index] / 255.0;
                    if (ctx->canonical_state.clamp_read_color == GL_TRUE ||
                        (ctx->canonical_state.clamp_read_color == GL_FIXED_ONLY &&
                         source.read_type != GL_FLOAT))
                        value = clamp_read_component(value, 0.0, 1.0);
                    values[component] = value;
                }
                if (scalar_size != 0u) {
                    for (component = 0u; component < components; ++component)
                        write_read_color_scalar(row + xx * pixel_bytes + component * scalar_size,
                                                type, values[component], ctx->pack_swap_bytes);
                } else
                    write_read_color_packed(row + xx * pixel_bytes, type, values,
                                            ctx->pack_swap_bytes);
            }
        }
    }
    if (pbo != NULL && dest_bytes != 0u)
        (void)mgl_buffer_write_bytes(ctx, pbo, offset, dest_bytes, dst_base, 0);
    free(rgba);
    free(rgba_float);
}

void glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
                   GLsizei bufSize, void *data) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_pixel_layout layout;
    size_t pixel_bytes;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (width < 0 || height < 0 || bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!valid_read_format_type(format, type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (width == 0 || height == 0) {
        glReadPixels(x, y, width, height, format, type, data);
        return;
    }
    pixel_bytes = read_depth_stencil_pixel_size(format, type);
    if (pixel_bytes == 0u)
        pixel_bytes = read_color_pixel_size(format, type);
    if (!mgl_pixel_layout_2d(width, height, pixel_bytes, ctx->pack_alignment, ctx->pack_row_length,
                             ctx->pack_skip_pixels, ctx->pack_skip_rows, &layout)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (layout.span_bytes > (size_t)bufSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    glReadPixels(x, y, width, height, format, type, data);
}

void glCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type) {
    struct mgl_context *ctx = mgl_context_current();
    void *rgba = NULL;
    size_t bytes = 0u;
    GLenum pixel_type = GL_UNSIGNED_BYTE;
    if (ctx == NULL)
        return;
    if (mgl_list_record_copy_pixels(ctx, x, y, width, height, type))
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (type != GL_COLOR) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!mgl_read_color_for_copy(ctx, x, y, width, height, &rgba, &bytes, &pixel_type))
        return;
    (void)bytes;
    if (!draw_rgba(ctx, width, height, rgba, pixel_type, 0))
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    free(rgba);
}

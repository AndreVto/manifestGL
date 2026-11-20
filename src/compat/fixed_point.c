#include "mgl/gl.h"
#include "core/context.h"
#include "state/state.h"

#include <limits.h>

#define MGL_FIXED_SCALE 65536.0f

/* Fixed-point setters reject calls during Begin/End before forwarding to immediate-state
 * setters. */
static int mgl_fixed_reject_inside_begin(void) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx != NULL && ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 1;
    }
    return 0;
}

static GLfloat mgl_fixed_to_float(GLfixed value) {
    return (GLfloat)value / MGL_FIXED_SCALE;
}

static GLfloat mgl_clampx_to_float(GLclampx value) {
    GLfloat converted = mgl_fixed_to_float(value);
    if (converted < 0.0f)
        return 0.0f;
    if (converted > 1.0f)
        return 1.0f;
    return converted;
}

static GLfixed mgl_float_to_fixed(GLfloat value) {
    double scaled = (double)value * 65536.0;
    if (scaled >= (double)INT32_MAX)
        return (GLfixed)INT32_MAX;
    if (scaled <= (double)INT32_MIN)
        return (GLfixed)INT32_MIN;
    return (GLfixed)scaled;
}

static GLfixed mgl_int_to_fixed(GLint value) {
    double scaled = (double)value * 65536.0;
    if (scaled >= (double)INT32_MAX)
        return (GLfixed)INT32_MAX;
    if (scaled <= (double)INT32_MIN)
        return (GLfixed)INT32_MIN;
    return (GLfixed)scaled;
}

static int mgl_fixed_query_is_enum(GLenum pname) {
    switch (pname) {
        case GL_ALPHA_TEST_FUNC:
        case GL_FOG_MODE:
        case GL_DEPTH_FUNC:
        case GL_CULL_FACE_MODE:
        case GL_FRONT_FACE:
        case GL_PERSPECTIVE_CORRECTION_HINT:
        case GL_POINT_SMOOTH_HINT:
        case GL_LINE_SMOOTH_HINT:
        case GL_POLYGON_SMOOTH_HINT:
        case GL_FOG_HINT:
        case GL_GENERATE_MIPMAP_HINT:
        case GL_TEXTURE_COMPRESSION_HINT:
        case GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
        case GL_PROVOKING_VERTEX:
        case GL_BLEND_SRC_RGB:
        case GL_BLEND_DST_RGB:
        case GL_BLEND_SRC_ALPHA:
        case GL_BLEND_DST_ALPHA:
        case GL_BLEND_EQUATION_RGB:
        case GL_BLEND_EQUATION_ALPHA:
        case GL_DRAW_BUFFER:
        case GL_READ_BUFFER:
        case GL_ACTIVE_TEXTURE:
        case GL_MATRIX_MODE:
        case GL_SHADE_MODEL:
        case GL_COLOR_MATERIAL_FACE:
        case GL_COLOR_MATERIAL_PARAMETER:
        case GL_STENCIL_FUNC:
        case GL_STENCIL_FAIL:
        case GL_STENCIL_PASS_DEPTH_FAIL:
        case GL_STENCIL_PASS_DEPTH_PASS:
        case GL_STENCIL_BACK_FUNC:
        case GL_STENCIL_BACK_FAIL:
        case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
        case GL_STENCIL_BACK_PASS_DEPTH_PASS:
        case GL_RENDER_MODE:
        case GL_PROGRAM_BINARY_FORMATS:
            return 1;
        default:
            return pname >= GL_DRAW_BUFFER0 && pname < GL_DRAW_BUFFER0 + MGL_MAX_DRAW_BUFFERS;
    }
}

void glGetFixedv(GLenum pname, GLfixed *data) {
    struct mgl_context *ctx = mgl_context_current();
    const int count = mgl_query_component_count(pname);
    int i;

    if (ctx == NULL || data == NULL)
        return;

    if (mgl_fixed_query_is_enum(pname)) {
        GLint values[16] = {0};
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetIntegerv(pname, values);
        if (ctx->errors.sequence != error_sequence_before)
            return;
        for (i = 0; i < count; ++i)
            data[i] = (GLfixed)values[i];
        return;
    }

    {
        GLfloat values[16] = {0.0f};
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetFloatv(pname, values);
        if (ctx->errors.sequence != error_sequence_before)
            return;
        for (i = 0; i < count; ++i)
            data[i] = mgl_float_to_fixed(values[i]);
    }
}

static int mgl_texture_fixed_enum_pname(GLenum pname) {
    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
        case GL_TEXTURE_MAG_FILTER:
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
        case GL_TEXTURE_WRAP_R:
        case GL_TEXTURE_COMPARE_MODE:
        case GL_TEXTURE_COMPARE_FUNC:
        case GL_DEPTH_TEXTURE_MODE:
            return 1;
        default:
            return 0;
    }
}

void glFogx(GLenum pname, GLfixed param) {
    if (pname == GL_FOG_MODE || pname == GL_FOG_COORD_SRC) {
        glFogi(pname, (GLint)param);
        return;
    }
    glFogf(pname, mgl_fixed_to_float(param));
}

void glFogxv(GLenum pname, const GLfixed *params) {
    GLfloat converted[4];
    unsigned i;
    if (params == NULL)
        return;
    if (pname == GL_FOG_MODE || pname == GL_FOG_COORD_SRC) {
        glFogi(pname, (GLint)params[0]);
        return;
    }
    if (pname == GL_FOG_COLOR) {
        for (i = 0u; i < 4u; ++i)
            converted[i] = mgl_fixed_to_float(params[i]);
        glFogfv(pname, converted);
        return;
    }
    glFogx(pname, params[0]);
}

void glTexEnvx(GLenum target, GLenum pname, GLfixed param) {
    if (pname == GL_TEXTURE_ENV_MODE) {
        glTexEnvi(target, pname, (GLint)param);
        return;
    }
    glTexEnvf(target, pname, mgl_fixed_to_float(param));
}

void glTexEnvxv(GLenum target, GLenum pname, const GLfixed *params) {
    if (params != NULL)
        glTexEnvx(target, pname, params[0]);
}

void glGetTexEnvxv(GLenum target, GLenum pname, GLfixed *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLint value = 0;
    if (ctx == NULL || params == NULL)
        return;
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetTexEnviv(target, pname, &value);
    if (ctx->errors.sequence == error_sequence_before)
        params[0] = (GLfixed)value;
}

void glTexParameterx(GLenum target, GLenum pname, GLfixed param) {
    switch (pname) {
        case GL_TEXTURE_MIN_LOD:
        case GL_TEXTURE_MAX_LOD:
        case GL_TEXTURE_PRIORITY:
        case GL_TEXTURE_BASE_LEVEL:
        case GL_TEXTURE_MAX_LEVEL:
            glTexParameterf(target, pname, mgl_fixed_to_float(param));
            return;
        default:
            glTexParameteri(target, pname, (GLint)param);
            return;
    }
}

void glTexParameterxv(GLenum target, GLenum pname, const GLfixed *params) {
    GLfloat converted[4];
    unsigned i;
    if (params == NULL)
        return;
    if (pname == GL_TEXTURE_BORDER_COLOR) {
        for (i = 0u; i < 4u; ++i)
            converted[i] = mgl_fixed_to_float(params[i]);
        glTexParameterfv(target, pname, converted);
        return;
    }
    glTexParameterx(target, pname, params[0]);
}

void glGetTexParameterxv(GLenum target, GLenum pname, GLfixed *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLint ivalue = 0;
    GLfloat fvalue = 0.0f;
    GLfloat fvalues[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    unsigned i;
    if (ctx == NULL || params == NULL)
        return;

    if (mgl_texture_fixed_enum_pname(pname)) {
        uint64_t error_sequence_before = ctx->errors.sequence;
        glGetTexParameteriv(target, pname, &ivalue);
        if (ctx->errors.sequence == error_sequence_before)
            params[0] = (GLfixed)ivalue;
        return;
    }
    switch (pname) {
        case GL_TEXTURE_BASE_LEVEL:
        case GL_TEXTURE_MAX_LEVEL:
        case GL_TEXTURE_RESIDENT: {
            uint64_t error_sequence_before = ctx->errors.sequence;
            glGetTexParameteriv(target, pname, &ivalue);
            if (ctx->errors.sequence == error_sequence_before)
                params[0] = mgl_int_to_fixed(ivalue);
            return;
        }
        case GL_TEXTURE_MIN_LOD:
        case GL_TEXTURE_MAX_LOD:
        case GL_TEXTURE_PRIORITY: {
            uint64_t error_sequence_before = ctx->errors.sequence;
            glGetTexParameterfv(target, pname, &fvalue);
            if (ctx->errors.sequence == error_sequence_before)
                params[0] = mgl_float_to_fixed(fvalue);
            return;
        }
        case GL_TEXTURE_BORDER_COLOR: {
            uint64_t error_sequence_before = ctx->errors.sequence;
            glGetTexParameterfv(target, pname, fvalues);
            if (ctx->errors.sequence != error_sequence_before)
                return;
            for (i = 0u; i < 4u; ++i)
                params[i] = mgl_float_to_fixed(fvalues[i]);
            return;
        }
        default:
            glGetTexParameteriv(target, pname, &ivalue);
            return;
    }
}

static unsigned mgl_light_fixed_count(GLenum pname) {
    switch (pname) {
        case GL_SPOT_DIRECTION:
            return 3u;
        case GL_SPOT_EXPONENT:
        case GL_SPOT_CUTOFF:
        case GL_CONSTANT_ATTENUATION:
        case GL_LINEAR_ATTENUATION:
        case GL_QUADRATIC_ATTENUATION:
            return 1u;
        default:
            return 4u;
    }
}

static unsigned mgl_material_fixed_count(GLenum pname) {
    return pname == GL_SHININESS ? 1u : 4u;
}

void glAlphaFuncx(GLenum func, GLclampx ref) {
    glAlphaFunc(func, mgl_clampx_to_float(ref));
}

void glClearColorx(GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha) {
    glClearColor(mgl_clampx_to_float(red), mgl_clampx_to_float(green), mgl_clampx_to_float(blue),
                 mgl_clampx_to_float(alpha));
}

void glClearDepthx(GLclampx depth) {
    glClearDepthf(mgl_clampx_to_float(depth));
}

void glDepthRangex(GLclampx nearVal, GLclampx farVal) {
    glDepthRangef(mgl_clampx_to_float(nearVal), mgl_clampx_to_float(farVal));
}

void glLineWidthx(GLfixed width) {
    glLineWidth(mgl_fixed_to_float(width));
}

void glPolygonOffsetx(GLfixed factor, GLfixed units) {
    glPolygonOffset(mgl_fixed_to_float(factor), mgl_fixed_to_float(units));
}

void glSampleCoveragex(GLclampx value, GLboolean invert) {
    glSampleCoverage(mgl_clampx_to_float(value), invert);
}

void glLoadMatrixx(const GLfixed *m) {
    GLfloat converted[16];
    unsigned i;
    if (m == NULL)
        return;
    for (i = 0u; i < 16u; ++i)
        converted[i] = mgl_fixed_to_float(m[i]);
    glLoadMatrixf(converted);
}

void glMultMatrixx(const GLfixed *m) {
    GLfloat converted[16];
    unsigned i;
    if (m == NULL)
        return;
    for (i = 0u; i < 16u; ++i)
        converted[i] = mgl_fixed_to_float(m[i]);
    glMultMatrixf(converted);
}

void glTranslatex(GLfixed x, GLfixed y, GLfixed z) {
    glTranslatef(mgl_fixed_to_float(x), mgl_fixed_to_float(y), mgl_fixed_to_float(z));
}

void glScalex(GLfixed x, GLfixed y, GLfixed z) {
    glScalef(mgl_fixed_to_float(x), mgl_fixed_to_float(y), mgl_fixed_to_float(z));
}

void glRotatex(GLfixed angle, GLfixed x, GLfixed y, GLfixed z) {
    glRotatef(mgl_fixed_to_float(angle), mgl_fixed_to_float(x), mgl_fixed_to_float(y),
              mgl_fixed_to_float(z));
}

void glOrthox(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed near_val,
              GLfixed far_val) {
    glOrtho((GLdouble)mgl_fixed_to_float(left), (GLdouble)mgl_fixed_to_float(right),
            (GLdouble)mgl_fixed_to_float(bottom), (GLdouble)mgl_fixed_to_float(top),
            (GLdouble)mgl_fixed_to_float(near_val), (GLdouble)mgl_fixed_to_float(far_val));
}

void glFrustumx(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed near_val,
                GLfixed far_val) {
    glFrustum((GLdouble)mgl_fixed_to_float(left), (GLdouble)mgl_fixed_to_float(right),
              (GLdouble)mgl_fixed_to_float(bottom), (GLdouble)mgl_fixed_to_float(top),
              (GLdouble)mgl_fixed_to_float(near_val), (GLdouble)mgl_fixed_to_float(far_val));
}

void glClipPlanex(GLenum plane, const GLfixed *equation) {
    GLdouble converted[4];
    unsigned i;
    if (equation == NULL)
        return;
    for (i = 0u; i < 4u; ++i)
        converted[i] = (GLdouble)mgl_fixed_to_float(equation[i]);
    glClipPlane(plane, converted);
}

void glGetClipPlanex(GLenum plane, GLfixed *equation) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat converted[4];
    unsigned i;
    if (ctx == NULL || equation == NULL)
        return;
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetClipPlanef(plane, converted);
    if (ctx->errors.sequence != error_sequence_before)
        return;
    for (i = 0u; i < 4u; ++i)
        equation[i] = mgl_float_to_fixed(converted[i]);
}

void glColor4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha) {
    if (mgl_fixed_reject_inside_begin())
        return;
    glColor4f(mgl_fixed_to_float(red), mgl_fixed_to_float(green), mgl_fixed_to_float(blue),
              mgl_fixed_to_float(alpha));
}

void glNormal3x(GLfixed nx, GLfixed ny, GLfixed nz) {
    if (mgl_fixed_reject_inside_begin())
        return;
    glNormal3f(mgl_fixed_to_float(nx), mgl_fixed_to_float(ny), mgl_fixed_to_float(nz));
}

void glMultiTexCoord4x(GLenum texture, GLfixed s, GLfixed t, GLfixed r, GLfixed q) {
    if (mgl_fixed_reject_inside_begin())
        return;
    glMultiTexCoord4f(texture, mgl_fixed_to_float(s), mgl_fixed_to_float(t), mgl_fixed_to_float(r),
                      mgl_fixed_to_float(q));
}

void glLightx(GLenum light, GLenum pname, GLfixed param) {
    glLightf(light, pname, mgl_fixed_to_float(param));
}

void glLightxv(GLenum light, GLenum pname, const GLfixed *params) {
    GLfloat converted[4];
    unsigned i, count;
    if (params == NULL)
        return;
    count = mgl_light_fixed_count(pname);
    for (i = 0u; i < count; ++i)
        converted[i] = mgl_fixed_to_float(params[i]);
    glLightfv(light, pname, converted);
}

void glGetLightxv(GLenum light, GLenum pname, GLfixed *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat converted[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    unsigned i, count;
    if (ctx == NULL || params == NULL)
        return;
    count = mgl_light_fixed_count(pname);
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetLightfv(light, pname, converted);
    if (ctx->errors.sequence != error_sequence_before)
        return;
    for (i = 0u; i < count; ++i)
        params[i] = mgl_float_to_fixed(converted[i]);
}

void glLightModelx(GLenum pname, GLfixed param) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (pname == GL_LIGHT_MODEL_AMBIENT) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    glLightModelf(pname, mgl_fixed_to_float(param));
}

void glLightModelxv(GLenum pname, const GLfixed *params) {
    GLfloat converted[4];
    unsigned i, count;
    if (params == NULL)
        return;
    count = pname == GL_LIGHT_MODEL_AMBIENT ? 4u : 1u;
    for (i = 0u; i < count; ++i)
        converted[i] = mgl_fixed_to_float(params[i]);
    glLightModelfv(pname, converted);
}

void glMaterialx(GLenum face, GLenum pname, GLfixed param) {
    if (mgl_fixed_reject_inside_begin())
        return;
    glMaterialf(face, pname, mgl_fixed_to_float(param));
}

void glMaterialxv(GLenum face, GLenum pname, const GLfixed *params) {
    if (mgl_fixed_reject_inside_begin())
        return;
    GLfloat converted[4];
    unsigned i, count;
    if (params == NULL)
        return;
    count = mgl_material_fixed_count(pname);
    for (i = 0u; i < count; ++i)
        converted[i] = mgl_fixed_to_float(params[i]);
    glMaterialfv(face, pname, converted);
}

void glGetMaterialxv(GLenum face, GLenum pname, GLfixed *params) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat converted[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    unsigned i, count;
    if (ctx == NULL || params == NULL)
        return;
    count = mgl_material_fixed_count(pname);
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGetMaterialfv(face, pname, converted);
    if (ctx->errors.sequence != error_sequence_before)
        return;
    for (i = 0u; i < count; ++i)
        params[i] = mgl_float_to_fixed(converted[i]);
}

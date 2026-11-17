#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static void fake_gles_limits(GLenum pname, GLint *out) {
    switch (pname) {
        case GL_MAX_GEOMETRY_UNIFORM_BLOCKS:
            *out = 13;
            break;
        case GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS:
            *out = 11;
            break;
        case GL_MAX_TEXTURE_SIZE:
            *out = 4096;
            break;
        case GL_MAX_3D_TEXTURE_SIZE:
            *out = 512;
            break;
        case GL_MAX_VERTEX_ATTRIBS:
            *out = 16;
            break;
        case GL_MAX_VERTEX_UNIFORM_VECTORS:
            *out = 256;
            break;
        case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
            *out = 224;
            break;
        case GL_MAX_VARYING_VECTORS:
            *out = 16;
            break;
        case GL_MAX_SAMPLES:
            *out = 4;
            break;
        case GL_ALIASED_POINT_SIZE_RANGE:
            out[0] = 1;
            out[1] = 256;
            break;
        case GL_MAX_VIEWPORT_DIMS:
            out[0] = 4096;
            out[1] = 2048;
            break;
        case GL_NUM_SHADER_BINARY_FORMATS:
        case GL_NUM_PROGRAM_BINARY_FORMATS:
            *out = 2;
            break;
        case GL_SHADER_BINARY_FORMATS:
            out[0] = 0x8E21;
            out[1] = 0x8E22;
            break;
        case GL_PROGRAM_BINARY_FORMATS:
            out[0] = 0x8E23;
            out[1] = 0x8E24;
            break;
        case GL_RED_BITS:
            *out = 8;
            break;
        case GL_GREEN_BITS:
            *out = 8;
            break;
        case GL_BLUE_BITS:
            *out = 8;
            break;
        case GL_ALPHA_BITS:
            *out = 8;
            break;
        case GL_DEPTH_BITS:
            *out = 24;
            break;
        case GL_STENCIL_BITS:
            *out = 8;
            break;
        case GL_SUBPIXEL_BITS:
            *out = 4;
            break;
        case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
            *out = (GLint)GL_RGBA;
            break;
        case GL_IMPLEMENTATION_COLOR_READ_TYPE:
            *out = (GLint)GL_UNSIGNED_BYTE;
            break;
        default:
            *out = 12;
            break;
    }
}

static int fail(int n, const char *msg) {
    fprintf(stderr, "query conversion failure %d: %s\n", n, msg);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLdouble dv[16] = {0.0};
    GLfloat fv[4] = {0.0f};
    GLint iv[4] = {0};
    GLboolean bv[4] = {0};

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetIntegerv = fake_gles_limits;
    ctx.native_dispatch = &native;
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 64, 64);
    mgl_compat_state_init(&ctx.compat);
    mgl_evaluator_init(&ctx.evaluator);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glViewport(3, 5, 17, 19);
    glGetDoublev(GL_VIEWPORT, dv);
    if (dv[0] != 3.0 || dv[1] != 5.0 || dv[2] != 17.0 || dv[3] != 19.0)
        return fail(1, "glGetDoublev viewport conversion");

    glDepthRange(0.25, 0.75);
    glGetDoublev(GL_DEPTH_RANGE, dv);
    if (dv[0] != 0.25 || dv[1] != 0.75)
        return fail(2, "glGetDoublev depth-range conversion");

    /* GL_POINT_SIZE_RANGE aliases GLES point-size limits; glGetDoublev returns two values. */
    {
        GLint point_limits[2] = {-7, -7};
        GLfloat point_float[3] = {-7.0f, -7.0f, 99.0f};
        GLdouble point_double[3] = {-7.0, -7.0, 99.0};
        glGetIntegerv(GL_POINT_SIZE_RANGE, point_limits);
        if (point_limits[0] != 1 || point_limits[1] != 256 ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(18, "desktop point-size range must use GLES aliased limits");
        glGetFloatv(GL_SMOOTH_POINT_SIZE_RANGE, point_float);
        glGetDoublev(GL_POINT_SIZE_RANGE, point_double);
        if (point_float[0] != 1.0f || point_float[1] != 256.0f || point_float[2] != 99.0f ||
            point_double[0] != 1.0 || point_double[1] != 256.0 || point_double[2] != 99.0 ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(19, "point-size range float/double cardinality");
        glGetIntegerv(GL_POINT_SIZE_GRANULARITY, point_limits);
        if (point_limits[0] != 1 || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(20, "point-size granularity");
        ctx.native_dispatch = NULL;
        glGetIntegerv(GL_POINT_SIZE_RANGE, point_limits);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
            return fail(21, "point-size range must not invent a missing native limit");
        ctx.native_dispatch = &native;
    }

    {
        GLint viewport_i[3] = {-1, -1, 777};
        GLfloat viewport_f[3] = {-1.f, -1.f, 777.f};
        GLdouble viewport_d[3] = {-1., -1., 777.};
        GLint64 viewport_i64[3] = {-1, -1, 777};
        GLboolean viewport_b[3] = {GL_FALSE, GL_FALSE, 7};
        glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport_i);
        glGetFloatv(GL_MAX_VIEWPORT_DIMS, viewport_f);
        glGetDoublev(GL_MAX_VIEWPORT_DIMS, viewport_d);
        glGetInteger64v(GL_MAX_VIEWPORT_DIMS, viewport_i64);
        glGetBooleanv(GL_MAX_VIEWPORT_DIMS, viewport_b);
        if (viewport_i[0] != 4096 || viewport_i[1] != 2048 || viewport_i[2] != 777 ||
            viewport_f[0] != 4096.f || viewport_f[1] != 2048.f || viewport_f[2] != 777.f ||
            viewport_d[0] != 4096. || viewport_d[1] != 2048. || viewport_d[2] != 777. ||
            viewport_i64[0] != 4096 || viewport_i64[1] != 2048 || viewport_i64[2] != 777 ||
            viewport_b[0] != GL_TRUE || viewport_b[1] != GL_TRUE || viewport_b[2] != 7 ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(22, "viewport dimension query must preserve two native limits");
        ctx.native_dispatch = NULL;
        glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport_i);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
            return fail(23, "viewport limits require a GLES native");
        ctx.native_dispatch = &native;
    }

    glMapGrid2f(7, -1.0f, 1.0f, 9, -2.0f, 2.0f);
    glGetFloatv(GL_MAP2_GRID_SEGMENTS, fv);
    if (fv[0] != 7.0f || fv[1] != 9.0f)
        return fail(3, "two-component float query cardinality");
    glGetDoublev(GL_MAP2_GRID_SEGMENTS, dv);
    if (dv[0] != 7.0 || dv[1] != 9.0)
        return fail(4, "two-component double query cardinality");

    glColorMaski(2u, GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
    glGetFloatIndexedv(GL_COLOR_WRITEMASK, 2u, fv);
    if (fv[0] != 1.0f || fv[1] != 0.0f || fv[2] != 1.0f || fv[3] != 0.0f)
        return fail(5, "indexed float color mask");
    memset(dv, 0, sizeof(dv));
    glGetDoubleIndexedvEXT(GL_COLOR_WRITEMASK, 2u, dv);
    if (dv[0] != 1.0 || dv[1] != 0.0 || dv[2] != 1.0 || dv[3] != 0.0)
        return fail(6, "indexed double color mask EXT alias");

    glEnablei(GL_BLEND, 2u);
    glGetBooleanIndexedv(GL_BLEND, 2u, bv);
    if (bv[0] != GL_TRUE)
        return fail(7, "indexed boolean alias");
    glGetIntegerIndexedv(GL_BLEND, 2u, iv);
    if (iv[0] != 1)
        return fail(8, "indexed integer alias");

    glGetFloatIndexedvEXT(0xdeadbeefu, 0u, fv);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(9, "indexed invalid target validation");
    glGetDoubleIndexedv(GL_BLEND, MGL_MAX_DRAW_BUFFERS, dv);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(10, "indexed invalid index validation");

    /* Capability discovery may query these without individual glGetError
     * checks. Every query must be legal and initialized. */
    {
        const struct {
            GLenum pname;
            GLint expected;
        } capability_limits[] = {
            {GL_MAX_TEXTURE_SIZE, 4096},
            {GL_MAX_VERTEX_ATTRIBS, 16},
            {GL_MAX_GEOMETRY_UNIFORM_BLOCKS, 13},
            {GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS, 11},
            {GL_MAX_3D_TEXTURE_SIZE, 512},
            {GL_MAX_VERTEX_UNIFORM_COMPONENTS, 1024},
            {GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, 896},
            {GL_MAX_VARYING_FLOATS, 64},
            {GL_MAX_SAMPLES, 4},
        };

        size_t i;
        for (i = 0; i < sizeof(capability_limits) / sizeof(capability_limits[0]); ++i) {
            iv[0] = -1234;
            glGetIntegerv(capability_limits[i].pname, iv);
            if (iv[0] != capability_limits[i].expected || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
                return fail(11, "capability query missing/incorrect");
        }
        glGetIntegerv(0xdeadbeefu, iv);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
            return fail(12, "invalid capability query still rejected");
    }

    {
        const GLenum enable_pnames[] = {
            GL_BLEND, GL_DEPTH_TEST, GL_SCISSOR_TEST, GL_STENCIL_TEST,
            GL_LIGHTING, GL_FOG, GL_TEXTURE_GEN_S, GL_MULTISAMPLE,
            GL_DEBUG_OUTPUT
        };
        for (size_t i = 0; i < sizeof(enable_pnames) / sizeof(enable_pnames[0]); ++i) {
            GLint integer = -1;
            GLint64 integer64 = -1;
            GLfloat floating = -1.0f;
            GLdouble double_value = -1.0;
            GLboolean boolean = 2;
            glEnable(enable_pnames[i]);
            glGetIntegerv(enable_pnames[i], &integer);
            glGetInteger64v(enable_pnames[i], &integer64);
            glGetFloatv(enable_pnames[i], &floating);
            glGetDoublev(enable_pnames[i], &double_value);
            glGetBooleanv(enable_pnames[i], &boolean);
            if (integer != 1 || integer64 != 1 || floating != 1.0f ||
                double_value != 1.0 || boolean != GL_TRUE ||
                mgl_error_take(&ctx.errors) != GL_NO_ERROR)
                return fail(24, "enabled state conversion");
            glDisable(enable_pnames[i]);
            glGetIntegerv(enable_pnames[i], &integer);
            if (integer != 0 || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
                return fail(25, "disabled state conversion");
        }
    }

    {
        GLint colors[5] = {-3, -3, -3, -3, 777};
        GLint64 colors64[5] = {-3, -3, -3, -3, 777};
        GLboolean bool_colors[5] = {2, 2, 2, 2, 7};
        glBlendColor(0.25f, 0.0f, 1.0f, 0.00001f);
        glGetIntegerv(GL_BLEND_COLOR, colors);
        glGetInteger64v(GL_BLEND_COLOR, colors64);
        glGetBooleanv(GL_BLEND_COLOR, bool_colors);
        if (colors[0] != 536870912 || colors[1] != 0 || colors[2] != 2147483647 ||
            colors[3] < 20000 || colors[3] > 22000 || colors[4] != 777 ||
            colors64[0] != colors[0] || colors64[1] != 0 ||
            colors64[2] != colors[2] || colors64[4] != 777 ||
            bool_colors[0] != GL_TRUE || bool_colors[1] != GL_FALSE ||
            bool_colors[2] != GL_TRUE || bool_colors[3] != GL_TRUE ||
            bool_colors[4] != 7 || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(26, "normalized color query and small nonzero booleans");
    }

    {
        GLint matrix[17] = {0};
        GLint64 matrix64[17] = {0};
        GLboolean bool_matrix[17] = {0};
        GLdouble double_matrix[17] = {0};
        matrix[16] = 777;
        matrix64[16] = 777;
        bool_matrix[16] = 7;
        double_matrix[16] = 777.0;
        glGetIntegerv(GL_MODELVIEW_MATRIX, matrix);
        glGetInteger64v(GL_MODELVIEW_MATRIX, matrix64);
        glGetBooleanv(GL_MODELVIEW_MATRIX, bool_matrix);
        glGetDoublev(GL_MODELVIEW_MATRIX, double_matrix);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || matrix[16] != 777 ||
            matrix64[16] != 777 || bool_matrix[16] != 7 || double_matrix[16] != 777.0)
            return fail(27, "16-component matrix query boundaries");
        for (int i = 0; i < 16; ++i) {
            const int diagonal = i % 5 == 0;
            if (matrix[i] != diagonal || matrix64[i] != diagonal ||
                bool_matrix[i] != (GLboolean)diagonal ||
                double_matrix[i] != (GLdouble)diagonal)
                return fail(28, "matrix query typed conversion");
        }
    }

    {
        GLint formats[4] = {-1, -1, -1, 777};
        GLint64 formats64[4] = {-1, -1, -1, 777};
        GLboolean bool_formats[4] = {2, 2, 2, 7};
        glGetIntegerv(GL_SHADER_BINARY_FORMATS, formats);
        glGetInteger64v(GL_SHADER_BINARY_FORMATS, formats64);
        glGetBooleanv(GL_SHADER_BINARY_FORMATS, bool_formats);
        if (formats[0] != 0x8E21 || formats[1] != 0x8E22 ||
            formats[2] != (GLint)GL_SHADER_BINARY_FORMAT_SPIR_V || formats[3] != 777 ||
            formats64[0] != formats[0] || formats64[1] != formats[1] ||
            formats64[2] != formats[2] || formats64[3] != 777 ||
            bool_formats[0] != GL_TRUE || bool_formats[1] != GL_TRUE ||
            bool_formats[2] != GL_TRUE || bool_formats[3] != 7 ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(29, "native plus translated shader binary format count");
    }

    {
        GLint val = 0;
        glGetIntegerv(GL_SHADER_COMPILER, &val);
        if (val != GL_TRUE || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(30, "shader compiler capability");
        glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &val);
        if (val != (GLint)GL_RGBA || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(31, "native implementation read format");
        glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &val);
        if (val != (GLint)GL_UNSIGNED_BYTE || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(32, "native implementation read type");
    }

    {
        GLint results[7] = {0};
        const GLenum pnames[] = {
            GL_RED_BITS, GL_GREEN_BITS, GL_BLUE_BITS, GL_ALPHA_BITS,
            GL_DEPTH_BITS, GL_STENCIL_BITS, GL_SUBPIXEL_BITS
        };
        const GLint expected[] = {8, 8, 8, 8, 24, 8, 4};
        for (size_t i = 0; i < sizeof(pnames) / sizeof(pnames[0]); ++i) {
            glGetIntegerv(pnames[i], &results[i]);
            if (results[i] != expected[i] || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
                return fail(33, "native framebuffer component bit query");
        }
    }

    {
        GLint result = -1;
        GLboolean bool_result = 2;
        glEnableClientState(GL_VERTEX_ARRAY);
        glGetIntegerv(GL_VERTEX_ARRAY, &result);
        glGetBooleanv(GL_VERTEX_ARRAY, &bool_result);
        if (result != 1 || bool_result != GL_TRUE ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(34, "client vertex array query");
        glDisableClientState(GL_VERTEX_ARRAY);
        glGetIntegerv(GL_VERTEX_ARRAY, &result);
        if (result != 0 || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(35, "disabled client vertex array query");
    }


    {
        GLfloat formats[4] = {-1.0f, -1.0f, -1.0f, 777.0f};
        GLdouble formats_double[4] = {-1.0, -1.0, -1.0, 777.0};
        glGetFloatv(GL_SHADER_BINARY_FORMATS, formats);
        glGetDoublev(GL_SHADER_BINARY_FORMATS, formats_double);
        if (formats[0] != (GLfloat)0x8E21 || formats[1] != (GLfloat)0x8E22 ||
            formats[2] != (GLfloat)GL_SHADER_BINARY_FORMAT_SPIR_V || formats[3] != 777.0f ||
            formats_double[0] != (GLdouble)0x8E21 ||
            formats_double[1] != (GLdouble)0x8E22 ||
            formats_double[2] != (GLdouble)GL_SHADER_BINARY_FORMAT_SPIR_V ||
            formats_double[3] != 777.0 || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(37, "shader binary format array cardinality");
    }

    mgl_context_set_current_for_test(NULL);
    puts("mGL query conversion unit test: PASS");
    return 0;
}

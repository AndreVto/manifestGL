#include "core/context.h"
#include "core/error.h"
#include "mgl/gl.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int get_calls;
static int indexed_calls;
static int samples_calls;
static int large_dimensions;
static int storage_calls;
static int viewport_extension_available;

static void fake_get(GLenum pname, GLint *value) {
    ++get_calls;
    if (pname == GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS)
        *value = 12;
    else if (pname == GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS)
        *value = 16;
    else if (pname == GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS)
        *value = 12;
    else if (pname == GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS)
        *value = 1;
    else if (pname == GL_MAX_TEXTURE_SIZE || pname == GL_MAX_CUBE_MAP_TEXTURE_SIZE ||
             pname == GL_MAX_RENDERBUFFER_SIZE)
        *value = 4096;
    else if (pname == GL_MAX_3D_TEXTURE_SIZE)
        *value = large_dimensions ? 4096 : 256;
    else if (pname == GL_MAX_ARRAY_TEXTURE_LAYERS)
        *value = 256;
    else if (pname == GL_MAX_IMAGE_UNITS)
        *value = 8;
    else if (pname == GL_MAX_ELEMENTS_VERTICES)
        *value = 1048576;
    else if (pname == GL_MAX_ELEMENTS_INDICES)
        *value = 3145728;
    else if (pname == GL_MAX_VERTEX_UNIFORM_VECTORS)
        *value = 256;
    else if (pname == GL_MAX_FRAGMENT_UNIFORM_VECTORS)
        *value = 224;
    else if (pname == GL_MAX_VARYING_VECTORS)
        *value = 16;
    else if (pname == GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS)
        *value = 4;
    else if (pname == GL_VIEWPORT_SUBPIXEL_BITS)
        *value = 8;
    else if (pname == GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS || pname == GL_MAX_TEXTURE_IMAGE_UNITS ||
             pname == GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS)
        *value = 16;
    else
        *value = 0;
}

static int float_query_calls;

static void fake_get_float(GLenum pname, GLfloat *value) {
    ++float_query_calls;
    if (pname == GL_MIN_FRAGMENT_INTERPOLATION_OFFSET)
        *value = -0.625f;
    else if (pname == GL_MAX_FRAGMENT_INTERPOLATION_OFFSET)
        *value = 0.625f;
    else if (pname == GL_MULTISAMPLE_LINE_WIDTH_GRANULARITY)
        *value = 0.125f;
    else if (pname == GL_ALIASED_LINE_WIDTH_RANGE || pname == GL_MULTISAMPLE_LINE_WIDTH_RANGE) {
        value[0] = 1.0f;
        value[1] = 3.5f;
    }
}

static void fake_get_64(GLenum pname, GLint64 *value) {
    if (pname == GL_MAX_SHADER_STORAGE_BLOCK_SIZE)
        *value = (INT64_C(1) << 33) + 4096;
    else if (pname == GL_MAX_UNIFORM_BLOCK_SIZE)
        *value = 262144;
    else if (pname == GL_MAX_ELEMENT_INDEX)
        *value = INT64_C(0xFFFFFFFF);
    else if (pname == GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS)
        *value = INT64_C(0x800000100);
    else if (pname == GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS)
        *value = 410000;
    else if (pname == GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS)
        *value = 510000;
    else if (pname == GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS)
        *value = 610000;
    else if (pname == GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS)
        *value = 710000;
    else if (pname == GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS)
        *value = 810000;
    else
        *value = 0;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    (void)target;
    (void)texture;
}

static void fake_tex_image_2d(GLenum target, GLint level, GLint format, GLsizei width,
                              GLsizei height, GLint border, GLenum pixels_format, GLenum type,
                              const void *pixels) {
    (void)target;
    (void)level;
    (void)format;
    (void)width;
    (void)height;
    (void)border;
    (void)pixels_format;
    (void)type;
    (void)pixels;
}

static void fake_tex_image_3d(GLenum target, GLint level, GLint format, GLsizei width,
                              GLsizei height, GLsizei depth, GLint border, GLenum pixels_format,
                              GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)format;
    (void)width;
    (void)height;
    (void)depth;
    (void)border;
    (void)pixels_format;
    (void)type;
    (void)pixels;
}

static void fake_tex_storage_2d(GLenum target, GLsizei levels, GLenum format, GLsizei width,
                                GLsizei height) {
    (void)target;
    (void)levels;
    (void)format;
    (void)width;
    (void)height;
    ++storage_calls;
}

static void fake_tex_storage_3d(GLenum target, GLsizei levels, GLenum format, GLsizei width,
                                GLsizei height, GLsizei depth) {
    (void)target;
    (void)levels;
    (void)format;
    (void)width;
    (void)height;
    (void)depth;
}

static void fake_texture_view(GLuint texture, GLenum target, GLuint origtexture, GLenum format,
                              GLuint minlevel, GLuint numlevels, GLuint minlayer,
                              GLuint numlayers) {
    (void)texture;
    (void)target;
    (void)origtexture;
    (void)format;
    (void)minlevel;
    (void)numlevels;
    (void)minlayer;
    (void)numlayers;
}

static void fake_gen_textures(GLsizei count, GLuint *textures) {
    if (count > 0)
        textures[0] = 1;
}

static const GLubyte *fake_get_string(GLenum pname) {
    if (pname != GL_EXTENSIONS)
        return NULL;
    return (const GLubyte *)(viewport_extension_available
                                 ? "GL_EXT_texture_view GL_OES_viewport_array"
                                 : "GL_EXT_texture_view");
}

static void fake_bind_image(GLuint unit, GLuint texture, GLint level, GLboolean layered,
                            GLint layer, GLenum access, GLenum format) {
    (void)unit;
    (void)texture;
    (void)level;
    (void)layered;
    (void)layer;
    (void)access;
    (void)format;
}

static void fake_renderbuffer_storage(GLenum target, GLenum format, GLsizei width, GLsizei height) {
    (void)target;
    (void)format;
    (void)width;
    (void)height;
}

static void fake_get_indexed(GLenum pname, GLuint index, GLint *value) {
    ++indexed_calls;
    *value = (pname == GL_MAX_COMPUTE_WORK_GROUP_COUNT ? 65535 : 128) - (GLint)index;
}

static void fake_get_internalformat(GLenum target, GLenum format, GLenum pname, GLsizei size,
                                    GLint *values) {
    (void)target;
    (void)format;
    ++samples_calls;
    if (size <= 0)
        return;
    if (pname == GL_NUM_SAMPLE_COUNTS)
        values[0] = 2;
    else if (pname == GL_SAMPLES) {
        values[0] = 4;
        if (size > 1)
            values[1] = 2;
    }
}

static int check(int n, int valid) {
    if (valid)
        return 0;
    fprintf(stderr, "query surface check %d failed\n", n);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLint value = -1;
    GLint64 large[2] = {-1, -1};
    GLint samples[3] = {-1, -1, -1};
    static const char source[] =
        "!!ARBvp1.0\nTEMP temp;\nMOV result.position, vertex.position;\nEND";
    GLuint name = 41u;

    static const struct {
        GLenum target, format, pname;
    } internalformat_queries[] = {{GL_TEXTURE_1D, GL_RG8, GL_VERTEX_TEXTURE},
                        {GL_TEXTURE_2D, GL_RG8, GL_VERTEX_TEXTURE},
                        {GL_TEXTURE_3D, GL_RG8, GL_VERTEX_TEXTURE},
                        {GL_TEXTURE_CUBE_MAP, GL_RG8, GL_VERTEX_TEXTURE},
                        {GL_RENDERBUFFER, GL_RG8, GL_VERTEX_TEXTURE},
                        {GL_TEXTURE_1D, GL_RG8, GL_FILTER},
                        {GL_TEXTURE_2D, GL_RG8, GL_FILTER},
                        {GL_TEXTURE_3D, GL_RG8, GL_SHADER_IMAGE_STORE},
                        {GL_TEXTURE_2D, GL_RG8, GL_SHADER_IMAGE_STORE},
                        {GL_TEXTURE_1D, GL_RG8, GL_SHADER_IMAGE_STORE},
                        {GL_TEXTURE_3D, GL_R8, GL_FILTER},
                        {GL_TEXTURE_2D, GL_R8, GL_FILTER},
                        {GL_TEXTURE_1D, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_VERTEX_TEXTURE},
                        {GL_TEXTURE_2D, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_VERTEX_TEXTURE},
                        {GL_TEXTURE_3D, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, GL_SRGB_WRITE},
                        {GL_TEXTURE_2D, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, GL_SRGB_WRITE},
                        {GL_TEXTURE_1D, GL_RGB32F_ARB, GL_SHADER_IMAGE_STORE},
                        {GL_TEXTURE_2D, GL_RGB32F_ARB, GL_SHADER_IMAGE_STORE},
                        {GL_TEXTURE_3D, GL_RGBA8, GL_SHADER_IMAGE_STORE},
                        {GL_TEXTURE_2D, GL_RGBA8, GL_SHADER_IMAGE_STORE},
                        {GL_TEXTURE_1D, GL_DEPTH24_STENCIL8, GL_VERTEX_TEXTURE},
                        {GL_TEXTURE_2D, GL_DEPTH24_STENCIL8, GL_VERTEX_TEXTURE}};

    size_t i;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetIntegerv = fake_get;
    native.glesGetFloatv = fake_get_float;
    native.glesGetInteger64v = fake_get_64;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexImage2D = fake_tex_image_2d;
    native.glesTexImage3D = fake_tex_image_3d;
    native.glesTexStorage2D = fake_tex_storage_2d;
    native.glesTexStorage3D = fake_tex_storage_3d;
    native.glesTextureView = fake_texture_view;
    native.glesGenTextures = fake_gen_textures;
    native.glesGetString = fake_get_string;
    native.glesBindImageTexture = fake_bind_image;
    native.glesRenderbufferStorage = fake_renderbuffer_storage;
    native.glesGetIntegeri_v = fake_get_indexed;
    native.glesGetInternalformativ = fake_get_internalformat;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_context_set_current_for_test(&ctx);

    glGetIntegerv(GL_VIEWPORT_SUBPIXEL_BITS, &value);
    if ((rc = check(2, value == 0 && glGetError() == GL_NO_ERROR)))
        return rc;
    viewport_extension_available = 1;
    glGetIntegerv(GL_VIEWPORT_SUBPIXEL_BITS, &value);
    if ((rc = check(77, value == 8 && glGetError() == GL_NO_ERROR)))
        return rc;
    viewport_extension_available = 0;
    glGetIntegerv(GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS, &value);
    if ((rc = check(3, value == 12 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetIntegerv(GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS, &value);
    if ((rc = check(4, value == 16 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetIntegerv(GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS, &value);
    if ((rc = check(5, value == 12 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetIntegerv(GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS, &value);
    if ((rc = check(6, value == 1 && glGetError() == GL_NO_ERROR)))
        return rc;
    {
        static const struct {
            GLenum pname;
            GLint expected;
        } vector_limits[] = {
            {GL_MAX_ELEMENTS_VERTICES, 1048576},
            {GL_MAX_ELEMENTS_INDICES, 3145728},
            {GL_MAX_VERTEX_UNIFORM_VECTORS, 256},
            {GL_MAX_FRAGMENT_UNIFORM_VECTORS, 224},
            {GL_MAX_VARYING_VECTORS, 16},
            {GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, 4}
        };
        for (i = 0u; i < sizeof(vector_limits) / sizeof(vector_limits[0]); ++i) {
            value = -1;
            glGetIntegerv(vector_limits[i].pname, &value);
            if ((rc = check(81, value == vector_limits[i].expected &&
                                    glGetError() == GL_NO_ERROR)))
                return rc;
        }
    }
    {
        static const GLenum limits[] = {
            GL_MIN_PROGRAM_TEXEL_OFFSET,
            GL_MAX_PROGRAM_TEXEL_OFFSET,
            GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS,
            GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS,
            GL_MAX_VERTEX_OUTPUT_COMPONENTS,
            GL_MAX_UNIFORM_LOCATIONS,
            GL_MAX_FRAMEBUFFER_WIDTH,
            GL_MAX_FRAMEBUFFER_HEIGHT,
            GL_MAX_FRAMEBUFFER_SAMPLES,
            GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE,
            GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES,
            GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET,
            GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET,
            GL_MAX_COLOR_TEXTURE_SAMPLES,
            GL_MAX_DEPTH_TEXTURE_SAMPLES,
            GL_MAX_INTEGER_SAMPLES,
            GL_MAX_GEOMETRY_UNIFORM_COMPONENTS,
            GL_MAX_GEOMETRY_INPUT_COMPONENTS,
            GL_MAX_GEOMETRY_OUTPUT_COMPONENTS,
            GL_MAX_GEOMETRY_OUTPUT_VERTICES,
            GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS,
            GL_MAX_GEOMETRY_SHADER_INVOCATIONS,
            GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS,
            GL_MAX_GEOMETRY_ATOMIC_COUNTERS,
            GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS,
            GL_MAX_FRAMEBUFFER_LAYERS,
            GL_MIN_FRAGMENT_INTERPOLATION_OFFSET,
            GL_MAX_FRAGMENT_INTERPOLATION_OFFSET,
            GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS,
            GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS,
            GL_MAX_TESS_CONTROL_INPUT_COMPONENTS,
            GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS,
            GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS,
            GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS,
            GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS,
            GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS,
        };
        for (i = 0u; i < sizeof(limits) / sizeof(limits[0]); ++i) {
            int previous_calls = get_calls;
            value = -1;
            glGetIntegerv(limits[i], &value);
            if ((rc = check(84, value == 0 && get_calls == previous_calls + 1 &&
                                    glGetError() == GL_NO_ERROR)))
                return rc;
        }
    }
    {
        static const struct {
            GLenum pname;
            GLfloat expected[2];
            int components;
        } float_limits[] = {
            {GL_MIN_FRAGMENT_INTERPOLATION_OFFSET, {-0.625f, 0.0f}, 1},
            {GL_MAX_FRAGMENT_INTERPOLATION_OFFSET, {0.625f, 0.0f}, 1},
            {GL_MULTISAMPLE_LINE_WIDTH_GRANULARITY, {0.125f, 0.0f}, 1},
            {GL_ALIASED_LINE_WIDTH_RANGE, {1.0f, 3.5f}, 2},
            {GL_MULTISAMPLE_LINE_WIDTH_RANGE, {1.0f, 3.5f}, 2}
        };
        for (i = 0u; i < sizeof(float_limits) / sizeof(float_limits[0]); ++i) {
            GLfloat reported[2] = {-1.0f, -1.0f};
            int before = float_query_calls;
            glGetFloatv(float_limits[i].pname, reported);
            if ((rc = check(85, float_query_calls == before + 1 &&
                                    reported[0] == float_limits[i].expected[0] &&
                                    (float_limits[i].components == 1 ||
                                     reported[1] == float_limits[i].expected[1]) &&
                                    glGetError() == GL_NO_ERROR)))
                return rc;
        }
    }
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0u, &value);
    if ((rc = check(7, value == 65535 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInteger64i_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2u, large);
    if ((rc = check(8, large[0] == 126 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 3u, &value);
    if ((rc = check(9, glGetError() == GL_INVALID_VALUE && indexed_calls == 2)))
        return rc;

    glGetInternalformativ(GL_TEXTURE_2D, GL_RG8, GL_VERTEX_TEXTURE, 1, &value);
    if ((rc = check(10, value == GL_FULL_SUPPORT && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_RG8, GL_FILTER, 1, &value);
    if ((rc = check(11, value == GL_FULL_SUPPORT && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_RG8, GL_SHADER_IMAGE_STORE, 1, &value);
    if ((rc = check(12, value == GL_NONE && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_SHADER_IMAGE_STORE, 1, &value);
    if ((rc = check(13, value == GL_FULL_SUPPORT && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, GL_SRGB_WRITE, 1,
                          &value);
    if ((rc = check(14, value == GL_NONE && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_INTERNALFORMAT_RED_SIZE, 1, &value);
    if ((rc = check(15, value == 8 && glGetError() == GL_NO_ERROR)))
        return rc;
    {
        static const struct {
            GLenum format, target, pname;
            GLint expected;
        } queries[] = {
            {GL_RGB5_A1, GL_RENDERBUFFER, GL_INTERNALFORMAT_SUPPORTED, GL_TRUE},
            {GL_R11F_G11F_B10F, GL_RENDERBUFFER, GL_INTERNALFORMAT_SUPPORTED, GL_TRUE},
            {GL_DEPTH32F_STENCIL8, GL_RENDERBUFFER, GL_INTERNALFORMAT_SUPPORTED, GL_TRUE},
            {GL_R8UI, GL_RENDERBUFFER, GL_INTERNALFORMAT_SUPPORTED, GL_TRUE},
            {GL_RGBA16F, GL_RENDERBUFFER, GL_INTERNALFORMAT_SUPPORTED, GL_TRUE},
            {GL_RGB16F, GL_RENDERBUFFER, GL_INTERNALFORMAT_SUPPORTED, GL_TRUE},
            {GL_DEPTH_COMPONENT32, GL_TEXTURE_2D_MULTISAMPLE, GL_INTERNALFORMAT_SUPPORTED, GL_TRUE},
            {GL_RGB8_SNORM, GL_RENDERBUFFER, GL_INTERNALFORMAT_SUPPORTED, GL_FALSE},
            {GL_R16, GL_RENDERBUFFER, GL_INTERNALFORMAT_SUPPORTED, GL_FALSE},
            {GL_DEPTH32F_STENCIL8, GL_RENDERBUFFER, GL_INTERNALFORMAT_DEPTH_SIZE, 32},
            {GL_DEPTH32F_STENCIL8, GL_RENDERBUFFER, GL_INTERNALFORMAT_STENCIL_SIZE, 8}
        };
        for (i = 0u; i < sizeof(queries) / sizeof(queries[0]); ++i) {
            value = -1;
            glGetInternalformativ(queries[i].target, queries[i].format, queries[i].pname, 1, &value);
            if ((rc = check(80, value == queries[i].expected && glGetError() == GL_NO_ERROR)))
                return rc;
        }
    }
    glGetInternalformativ(GL_TEXTURE_2D, 0xDEADu, GL_INTERNALFORMAT_SUPPORTED, 1, &value);
    if ((rc = check(16, value == GL_FALSE && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_1D, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 1, &value);
    if ((rc = check(17, value == 0 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 1, &value);
    if ((rc = check(18, value == 2 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_SAMPLES, 3, samples);
    if ((rc = check(19, samples[0] == 4 && samples[1] == 2 && samples[2] == -1 &&
                            glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformati64v(GL_RENDERBUFFER, GL_RGBA8, GL_SAMPLES, 2, large);
    if ((rc = check(20, large[0] == 4 && large[1] == 2 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformati64v(GL_TEXTURE_3D, GL_RG8, GL_FILTER, 1, large);
    if ((rc = check(21, large[0] == GL_FULL_SUPPORT && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_RG8, 0xDEADu, 1, &value);
    if ((rc = check(22, glGetError() == GL_INVALID_ENUM)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_RG8, GL_FILTER, -1, &value);
    if ((rc = check(23, glGetError() == GL_INVALID_VALUE)))
        return rc;

    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, &value);
    if ((rc = check(24, value >= 128 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB, &value);
    if ((rc = check(25, value >= 16 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB, &value);
    if ((rc = check(27, glGetError() == GL_INVALID_ENUM)))
        return rc;
    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, name);
    if ((rc = check(28, glGetError() == GL_NO_ERROR)))
        return rc;
    glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof(source) - 1u,
                       source);
    if ((rc = check(29, glGetError() == GL_NO_ERROR)))
        return rc;
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_INSTRUCTIONS_ARB, &value);
    if ((rc = check(30, value == 1 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_TEMPORARIES_ARB, &value);
    if ((rc = check(31, value == 1 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &value);
    if ((rc = check(32, value == GL_TRUE && glGetError() == GL_NO_ERROR)))
        return rc;
    if ((rc = check(33, get_calls >= 4 && samples_calls >= 4)))
        return rc;
    for (i = 0u; i < sizeof(internalformat_queries) / sizeof(internalformat_queries[0]); ++i) {
        glGetInternalformativ(internalformat_queries[i].target, internalformat_queries[i].format, internalformat_queries[i].pname,
                              1, &value);
        if ((rc = check(34, glGetError() == GL_NO_ERROR)))
            return rc;
    }
    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_MAX_COMBINED_DIMENSIONS, 1, &value);
    if ((rc = check(35, value == 16777216 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformati64v(GL_TEXTURE_2D, GL_RGBA8, GL_MAX_COMBINED_DIMENSIONS, 1, large);
    if ((rc = check(36, large[0] == 16777216 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, 0xDEADu, GL_MAX_WIDTH, 1, &value);
    if ((rc = check(37, value == 0 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_DEPTH24_STENCIL8, GL_TEXTURE_IMAGE_FORMAT, 1, &value);
    if ((rc = check(38, value == GL_DEPTH_STENCIL && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_IMAGE_TEXEL_SIZE, 1, &value);
    if ((rc = check(39, value == 32 && glGetError() == GL_NO_ERROR)))
        return rc;
    {
        static const struct {
            GLenum pname;
            GLint64 expected;
        } large_limits[] = {
            {GL_MAX_UNIFORM_BLOCK_SIZE, 262144},
            {GL_MAX_ELEMENT_INDEX, INT64_C(0xFFFFFFFF)},
            {GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS, INT64_C(0x800000100)},
            {GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS, 410000},
            {GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS, 510000},
            {GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS, 610000},
            {GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS, 710000},
            {GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS, 810000}
        };
        for (i = 0u; i < sizeof(large_limits) / sizeof(large_limits[0]); ++i) {
            large[0] = -1;
            glGetInteger64v(large_limits[i].pname, large);
            if ((rc = check(82, large[0] == large_limits[i].expected &&
                                    glGetError() == GL_NO_ERROR)))
                return rc;
            {
                GLdouble dvalue = -1.0;
                GLfloat fvalue = -1.0f;
                glGetDoublev(large_limits[i].pname, &dvalue);
                glGetFloatv(large_limits[i].pname, &fvalue);
                if ((rc = check(91, dvalue == (GLdouble)large_limits[i].expected &&
                                        fvalue == (GLfloat)large_limits[i].expected &&
                                        glGetError() == GL_NO_ERROR)))
                    return rc;
            }
        }
        for (i = 1u; i < sizeof(large_limits) / sizeof(large_limits[0]); ++i) {
            value = -1;
            glGetIntegerv(large_limits[i].pname, &value);
            if ((rc = check(83, value == (large_limits[i].expected > INT_MAX
                                            ? INT_MAX
                                            : (GLint)large_limits[i].expected) &&
                                    glGetError() == GL_NO_ERROR)))
                return rc;
        }
    }
    glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, large);
    if ((rc = check(40, large[0] == (INT64_C(1) << 33) + 4096 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &value);
    if ((rc = check(41, value == INT_MAX && glGetError() == GL_NO_ERROR)))
        return rc;
    {
        GLdouble dv = -1.0;
        GLfloat fv = -1.0f;
        const GLint64 expected = (INT64_C(1) << 33) + 4096;
        glGetDoublev(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &dv);
        glGetFloatv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &fv);
        if ((rc = check(92, dv == (GLdouble)expected && fv == (GLfloat)expected &&
                                glGetError() == GL_NO_ERROR)))
            return rc;
        dv = 1234.0;
        glGetDoublev(0xDEADu, &dv);
        if ((rc = check(93, dv == 1234.0 && glGetError() == GL_INVALID_ENUM)))
            return rc;
    }
    large_dimensions = 1;
    glGetInternalformativ(GL_TEXTURE_3D, GL_RGBA8, GL_MAX_COMBINED_DIMENSIONS, 2, samples);
    if ((rc = check(42, samples[0] == 0 && samples[1] == 16 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformati64v(GL_TEXTURE_3D, GL_RGBA8, GL_MAX_COMBINED_DIMENSIONS, 1, large);
    if ((rc = check(43, large[0] == (INT64_C(1) << 36) && glGetError() == GL_NO_ERROR)))
        return rc;
    large_dimensions = 0;
    glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_MAX_COMBINED_DIMENSIONS, 1, &value);
    if ((rc = check(44, value == 67108864 && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_1D_ARRAY, GL_RGBA8, GL_INTERNALFORMAT_SUPPORTED, 1, &value);
    if ((rc = check(45, value == GL_FALSE && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_FRAMEBUFFER_RENDERABLE, 1, &value);
    if ((rc = check(46, value == GL_NONE && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_SHADER_IMAGE_STORE, 1, &value);
    if ((rc = check(47, value == GL_FULL_SUPPORT && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_RGB32F_ARB, GL_SHADER_IMAGE_STORE, 1, &value);
    if ((rc = check(49, value == GL_NONE && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_FILTER, 1, &value);
    if ((rc = check(50, value == GL_FULL_SUPPORT && glGetError() == GL_NO_ERROR)))
        return rc;

    {
        static const char fp_indirections[] = "!!ARBfp1.0\nTEMP color1, color2;\n"
                                              "TEX color1, fragment.texcoord[0], texture[0], 2D;\n"
                                              "MUL color1, color1, color1;\n"
                                              "TEX color2, fragment.texcoord[1], texture[1], 2D;\n"
                                              "TEX color2, color1, texture[2], 2D;\n"
                                              "TEX result.color, color2, texture[3], 2D;\nEND";
        static const char fp_one[] = "!!ARBfp1.0\nMOV result.color, fragment.color;\nEND";
        static const char vp_parameters[] = "!!ARBvp1.0\nPARAM mvp[4] = { state.matrix.mvp };\n"
                                            "MOV result.position, vertex.position;\nEND";
        GLuint fp_name = 42u;
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fp_name);
        if ((rc = check(52, glGetError() == GL_NO_ERROR)))
            return rc;
        glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                           sizeof(fp_indirections) - 1u, fp_indirections);
        if ((rc = check(53, glGetError() == GL_NO_ERROR)))
            return rc;
        glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_TEX_INDIRECTIONS_ARB, &value);
        if ((rc = check(54, value == 3 && glGetError() == GL_NO_ERROR)))
            return rc;
        glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_TEX_INSTRUCTIONS_ARB, &value);
        if ((rc = check(55, value == 4 && glGetError() == GL_NO_ERROR)))
            return rc;
        glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_TEMPORARIES_ARB, &value);
        if ((rc = check(56, value == 2 && glGetError() == GL_NO_ERROR)))
            return rc;
        glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                           sizeof(fp_one) - 1u, fp_one);
        if ((rc = check(57, glGetError() == GL_NO_ERROR)))
            return rc;
        glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_TEX_INDIRECTIONS_ARB, &value);
        if ((rc = check(58, value == 1 && glGetError() == GL_NO_ERROR)))
            return rc;
        glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                           sizeof(vp_parameters) - 1u, vp_parameters);
        if ((rc = check(59, glGetError() == GL_NO_ERROR)))
            return rc;
        glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_PARAMETERS_ARB, &value);
        if ((rc = check(60, value == 4 && glGetError() == GL_NO_ERROR)))
            return rc;
    }
    {
        static const struct {
            GLenum format;
            GLint cls;
            GLint pixel_format;
            GLint pixel_type;
        } images[] = {{GL_RGBA32F, GL_IMAGE_CLASS_4_X_32, GL_RGBA, GL_FLOAT},
                      {GL_RGBA32I, GL_IMAGE_CLASS_4_X_32, GL_RGBA_INTEGER, GL_INT},
                      {GL_RGBA32UI, GL_IMAGE_CLASS_4_X_32, GL_RGBA_INTEGER, GL_UNSIGNED_INT},
                      {GL_RGBA16F, GL_IMAGE_CLASS_4_X_16, GL_RGBA, GL_HALF_FLOAT},
                      {GL_RGBA16I, GL_IMAGE_CLASS_4_X_16, GL_RGBA_INTEGER, GL_SHORT},
                      {GL_RGBA16UI, GL_IMAGE_CLASS_4_X_16, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT},
                      {GL_RGBA8, GL_IMAGE_CLASS_4_X_8, GL_RGBA, GL_UNSIGNED_BYTE},
                      {GL_RGBA8_SNORM, GL_IMAGE_CLASS_4_X_8, GL_RGBA, GL_BYTE},
                      {GL_RGBA8I, GL_IMAGE_CLASS_4_X_8, GL_RGBA_INTEGER, GL_BYTE},
                      {GL_RGBA8UI, GL_IMAGE_CLASS_4_X_8, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE},
                      {GL_R32F, GL_IMAGE_CLASS_1_X_32, GL_RED, GL_FLOAT},
                      {GL_R32I, GL_IMAGE_CLASS_1_X_32, GL_RED_INTEGER, GL_INT},
                      {GL_R32UI, GL_IMAGE_CLASS_1_X_32, GL_RED_INTEGER, GL_UNSIGNED_INT}};

        static const struct {
            GLenum format;
            GLint cls;
        } views[] = {{GL_RGB8, GL_VIEW_CLASS_24_BITS},
                     {GL_SRGB8, GL_VIEW_CLASS_24_BITS},
                     {GL_RGBA8, GL_VIEW_CLASS_32_BITS},
                     {GL_SRGB8_ALPHA8, GL_VIEW_CLASS_32_BITS}};

        size_t j;
        for (j = 0; j < sizeof(images) / sizeof(images[0]); ++j) {
            glGetInternalformativ(GL_TEXTURE_2D, images[j].format, GL_IMAGE_COMPATIBILITY_CLASS, 1,
                                  &value);
            if ((rc = check(61, value == images[j].cls && glGetError() == GL_NO_ERROR)))
                return rc;
            glGetInternalformativ(GL_TEXTURE_2D, images[j].format, GL_IMAGE_PIXEL_FORMAT, 1,
                                  &value);
            if ((rc = check(62, value == images[j].pixel_format && glGetError() == GL_NO_ERROR)))
                return rc;
            glGetInternalformativ(GL_TEXTURE_2D, images[j].format, GL_IMAGE_PIXEL_TYPE, 1, &value);
            if ((rc = check(63, value == images[j].pixel_type && glGetError() == GL_NO_ERROR)))
                return rc;
        }
        for (j = 0; j < sizeof(views) / sizeof(views[0]); ++j) {
            glGetInternalformativ(GL_TEXTURE_2D, views[j].format, GL_TEXTURE_VIEW, 1, &value);
            if ((rc = check(64, value == GL_CAVEAT_SUPPORT && glGetError() == GL_NO_ERROR)))
                return rc;
            glGetInternalformativ(GL_TEXTURE_2D_ARRAY, views[j].format, GL_VIEW_COMPATIBILITY_CLASS,
                                  1, &value);
            if ((rc = check(65, value == views[j].cls && glGetError() == GL_NO_ERROR)))
                return rc;
            glGetInternalformati64v(GL_TEXTURE_CUBE_MAP, views[j].format,
                                    GL_VIEW_COMPATIBILITY_CLASS, 1, large);
            if ((rc = check(66, large[0] == views[j].cls && glGetError() == GL_NO_ERROR)))
                return rc;
        }
    }
    glGetInternalformativ(GL_TEXTURE_2D, GL_RG8, GL_IMAGE_COMPATIBILITY_CLASS, 1, &value);
    if ((rc = check(67, value == GL_NONE && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_IMAGE_PIXEL_FORMAT, 1, &value);
    if ((rc = check(68, value == GL_NONE && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_DEPTH24_STENCIL8, GL_IMAGE_PIXEL_TYPE, 1, &value);
    if ((rc = check(69, value == GL_NONE && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_2D, GL_RG8, GL_VIEW_COMPATIBILITY_CLASS, 1, &value);
    if ((rc = check(70, value == GL_VIEW_CLASS_16_BITS && glGetError() == GL_NO_ERROR)))
        return rc;
    glGetInternalformativ(GL_TEXTURE_3D, GL_RGBA8, GL_TEXTURE_VIEW, 1, &value);
    if ((rc = check(71, value == GL_NONE && glGetError() == GL_NO_ERROR)))
        return rc;

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

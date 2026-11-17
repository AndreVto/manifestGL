#include "mgl/gl.h"
#include "abi/proc_address.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    int norm16_extension;
    GLuint bound_2d;
    GLuint bound_3d;
    GLuint bound_array;
    GLuint bound_cube;
    unsigned storage2_calls;
    unsigned storage3_calls;
    unsigned image2_calls;
    unsigned image3_calls;
    unsigned subimage2_calls;
    GLenum last_image_internal;
    GLenum last_image_format;
    GLenum last_subimage_format;
    GLenum last_subimage_type;
    unsigned char last_subimage_la[2];
    unsigned char last_subimage_integer[4];
    unsigned swizzle_calls;
    GLint swizzle[4];
    unsigned generate_calls;
    unsigned param_calls;
    GLenum last_param_pname;
    GLenum last_query_pname;
    unsigned query_calls;
    GLint sample_buffers;
    GLint max_cube_size;
    GLint last_param;
    GLsizei storage_levels;
    GLenum storage_format;
    GLsizei storage_width, storage_height, storage_depth;
} cap;

static void fake_gen_textures(GLsizei n, GLuint *out) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        out[i] = ++cap.next_texture;
}

static void fake_delete_textures(GLsizei n, const GLuint *out) {
    (void)n;
    (void)out;
}

static const GLubyte *fake_get_string(GLenum name) {
    if (name != GL_EXTENSIONS)
        return (const GLubyte *)"";
    return cap.norm16_extension ? (const GLubyte *)"GL_EXT_texture_norm16"
                                : (const GLubyte *)"GL_EXT_mock";
}

static void fake_active_texture(GLenum texture) {
    (void)texture;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    if (target == GL_TEXTURE_2D)
        cap.bound_2d = texture;
    else if (target == GL_TEXTURE_3D)
        cap.bound_3d = texture;
    else if (target == GL_TEXTURE_2D_ARRAY)
        cap.bound_array = texture;
    else if (target == GL_TEXTURE_CUBE_MAP)
        cap.bound_cube = texture;
}

static void fake_tex_storage2(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                              GLsizei height) {
    (void)target;
    ++cap.storage2_calls;
    cap.storage_levels = levels;
    cap.storage_format = internalformat;
    cap.storage_width = width;
    cap.storage_height = height;
}

static void fake_tex_storage3(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                              GLsizei height, GLsizei depth) {
    (void)target;
    ++cap.storage3_calls;
    cap.storage_levels = levels;
    cap.storage_format = internalformat;
    cap.storage_width = width;
    cap.storage_height = height;
    cap.storage_depth = depth;
}

static void fake_tex_image2(GLenum target, GLint level, GLint internalformat, GLsizei width,
                            GLsizei height, GLint border, GLenum format, GLenum type,
                            const void *pixels) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)border;
    (void)type;
    (void)pixels;
    ++cap.image2_calls;
    cap.last_image_internal = (GLenum)internalformat;
    cap.last_image_format = format;
}

static void fake_tex_image3(GLenum target, GLint level, GLint internalformat, GLsizei width,
                            GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type,
                            const void *pixels) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)depth;
    (void)border;
    (void)format;
    (void)type;
    (void)pixels;
    ++cap.image3_calls;
}

static void fake_tex_sub2(GLenum target, GLint level, GLint x, GLint y, GLsizei width,
                          GLsizei height, GLenum format, GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)format;
    (void)type;
    (void)pixels;
    ++cap.subimage2_calls;
    cap.last_subimage_format = format;
    cap.last_subimage_type = type;
    if (pixels && format == GL_RGBA_INTEGER && width && height &&
        (type == GL_BYTE || type == GL_UNSIGNED_BYTE))
        memcpy(cap.last_subimage_integer, pixels, 4u);
    if (pixels != NULL && type == GL_UNSIGNED_BYTE && width > 0 && height > 0) {
        const unsigned char *src = pixels;
        cap.last_subimage_la[0] = src[0];
        cap.last_subimage_la[1] = src[1];
    }
}

static void fake_tex_sub3(GLenum target, GLint level, GLint x, GLint y, GLint z, GLsizei width,
                          GLsizei height, GLsizei depth, GLenum format, GLenum type,
                          const void *pixels) {
    (void)target;
    (void)level;
    (void)x;
    (void)y;
    (void)z;
    (void)width;
    (void)height;
    (void)depth;
    (void)format;
    (void)type;
    (void)pixels;
}

static void fake_tex_param(GLenum target, GLenum pname, GLint value) {
    (void)target;
    ++cap.param_calls;
    cap.last_param_pname = pname;
    cap.last_param = value;
    if (pname >= GL_TEXTURE_SWIZZLE_R && pname <= GL_TEXTURE_SWIZZLE_A) {
        cap.swizzle[(unsigned)(pname - GL_TEXTURE_SWIZZLE_R)] = value;
        ++cap.swizzle_calls;
    }
}

static void fake_get_integer(GLenum pname, GLint *out) {
    ++cap.query_calls;
    cap.last_query_pname = pname;
    if (pname == GL_SAMPLE_BUFFERS)
        *out = cap.sample_buffers;
    else if (pname == GL_MAX_CUBE_MAP_TEXTURE_SIZE)
        *out = cap.max_cube_size;
    else
        *out = 0;
}

static void fake_pixel_store(GLenum pname, GLint value) {
    (void)pname;
    (void)value;
}

static void fake_bind_buffer(GLenum target, GLuint buffer) {
    (void)target;
    (void)buffer;
}

static void fake_read_pixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                             GLenum type, void *pixels) {
    (void)x;
    (void)y;
    (void)format;
    (void)type;
    memset(pixels, 0x73, (size_t)width * (size_t)height * 4u);
}

static void fake_generate(GLenum target) {
    (void)target;
    ++cap.generate_calls;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "texture storage failure %d: %s\n", code, message);
    return code;
}

static int expect_error(struct mgl_context *ctx, GLenum expected, int code, const char *message) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr, "texture storage failure %d: %s (got 0x%04x expected 0x%04x)\n", code,
                message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj;
    GLuint tex = 0u, other = 0u, tex3d = 0u, array = 0u, cube = 0u, invalid = 0u,
           legacy = 0u, rgb5 = 0u;
    GLint value = 0;
    unsigned char patch[16];
    unsigned image_calls;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    memset(patch, 0x5a, sizeof(patch));
    cap.next_texture = 900u;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesGetString = fake_get_string;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexStorage2D = fake_tex_storage2;
    native.glesTexStorage3D = fake_tex_storage3;
    native.glesTexImage2D = fake_tex_image2;
    native.glesTexImage3D = fake_tex_image3;
    native.glesTexSubImage2D = fake_tex_sub2;
    native.glesTexSubImage3D = fake_tex_sub3;
    native.glesTexParameteri = fake_tex_param;
    native.glesGetIntegerv = fake_get_integer;
    cap.sample_buffers = 0;
    cap.max_cube_size = 4096;
    native.glesPixelStorei = fake_pixel_store;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesReadPixels = fake_read_pixels;
    native.glesGenerateMipmap = fake_generate;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.unpack_alignment = 4;
    ctx.pack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 2, "default texture storage")) != 0)
        return rc;

    glGetIntegerv(GL_SAMPLE_BUFFERS, &value);
    if (value != 0 || cap.last_query_pname != GL_SAMPLE_BUFFERS ||
        (rc = expect_error(&ctx, GL_NO_ERROR, 70, "sample-buffer query")))
        return rc ? rc : fail(70, "sample buffers");
    cap.sample_buffers = 1;
    glGetIntegerv(GL_SAMPLE_BUFFERS, &value);
    if (value != 1 || (rc = expect_error(&ctx, GL_NO_ERROR, 71, "bound framebuffer sample state")))
        return rc ? rc : fail(71, "samples state not live");
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &value);
    if (value != 4096 || cap.last_query_pname != GL_MAX_CUBE_MAP_TEXTURE_SIZE ||
        (rc = expect_error(&ctx, GL_NO_ERROR, 72, "cube size")))
        return rc ? rc : fail(72, "cube size query");

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    {
        GLuint rg = 0u;
        glGenTextures(1, &rg);
        glBindTexture(GL_TEXTURE_2D, rg);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG8, 128, 128);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 85, "GL_RG8 immutable storage")))
            return rc;
        obj = mgl_texture_find(&ctx, rg);
        if (obj == NULL || !obj->immutable_format || obj->levels[0].data_size != 128u * 128u * 2u ||
            obj->levels[0].format != GL_RG || cap.storage_format != GL_RG8)
            return fail(86, "GL_RG8 metadata/native storage");
        glBindTexture(GL_TEXTURE_2D, tex);
        glDeleteTextures(1, &rg);
        cap.storage2_calls = 0u;
    }

    {
        static const struct {
            GLenum internal, format, type;
            size_t size;
        } supported[] = {{GL_R8I, GL_RED_INTEGER, GL_BYTE, 1u},
                         {GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, 1u},
                         {GL_R16I, GL_RED_INTEGER, GL_SHORT, 2u},
                         {GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, 4u},
                         {GL_RG8I, GL_RG_INTEGER, GL_BYTE, 2u},
                         {GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT, 4u},
                         {GL_RG32I, GL_RG_INTEGER, GL_INT, 8u},
                         {GL_RGB8UI, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, 3u},
                         {GL_RGB16I, GL_RGB_INTEGER, GL_SHORT, 6u},
                         {GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT, 12u},
                         {GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, 4u},
                         {GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, 4u},
                         {GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT, 8u},
                         {GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, 8u},
                         {GL_RGBA32I, GL_RGBA_INTEGER, GL_INT, 16u},
                         {GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, 16u},
                         {GL_RGB10_A2UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, 4u},
                         {GL_R8_SNORM, GL_RED, GL_BYTE, 1u},
                         {GL_RG8_SNORM, GL_RG, GL_BYTE, 2u},
                         {GL_RGB8_SNORM, GL_RGB, GL_BYTE, 3u},
                         {GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, 4u},
                         {GL_R16F, GL_RED, GL_HALF_FLOAT, 2u},
                         {GL_RG16F, GL_RG, GL_HALF_FLOAT, 4u},
                         {GL_RGB16F, GL_RGB, GL_HALF_FLOAT, 6u},
                         {GL_R32F, GL_RED, GL_FLOAT, 4u},
                         {GL_RG32F, GL_RG, GL_FLOAT, 8u},
                         {GL_RGB32F_ARB, GL_RGB, GL_FLOAT, 12u},
                         {GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 2u},
                         {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 2u},
                         {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 4u},
                         {GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, 4u},
                         {GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, 4u},
                         {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 4u},
                         {GL_STENCIL_INDEX8, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, 1u}};

        size_t k;
        for (k = 0u; k < sizeof(supported) / sizeof(supported[0]); ++k) {
            GLuint name = 0u;
            glGenTextures(1, &name);
            glBindTexture(GL_TEXTURE_2D, name);
            glTexStorage2D(GL_TEXTURE_2D, 1, supported[k].internal, 2, 2);
            if ((rc = expect_error(&ctx, GL_NO_ERROR, 200, "supported immutable format")))
                return rc;
            obj = mgl_texture_find(&ctx, name);
            if (obj == NULL || !obj->immutable_format ||
                obj->levels[0].internal_format != supported[k].internal ||
                obj->levels[0].format != supported[k].format ||
                obj->levels[0].type != supported[k].type ||
                obj->levels[0].data_size != supported[k].size * 4u || cap.storage_format == 0u ||
                ((supported[k].format == GL_RED_INTEGER || supported[k].format == GL_RG_INTEGER ||
                  supported[k].format == GL_RGB_INTEGER ||
                  supported[k].format == GL_RGBA_INTEGER) &&
                 cap.storage_format != supported[k].internal)) {
                fprintf(stderr,
                        "format %zu internal=0x%x expected native=0x%x actual native=0x%x "
                        "shadow_format=0x%x shadow_type=0x%x shadow_bytes=%zu\n",
                        k, (unsigned)supported[k].internal,
                        (unsigned)(supported[k].internal == GL_RGB32F_ARB ? GL_RGBA32F
                                                                          : supported[k].internal),
                        (unsigned)cap.storage_format,
                        obj != NULL ? (unsigned)obj->levels[0].format : 0u,
                        obj != NULL ? (unsigned)obj->levels[0].type : 0u,
                        obj != NULL ? obj->levels[0].data_size : 0u);
                return fail(201, "immutable format metadata and native format");
            }
            glDeleteTextures(1, &name);
        }
        {
            GLuint name = 0u;
            glGenTextures(1, &name);
            glBindTexture(GL_TEXTURE_2D_ARRAY, name);
            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8I, 2, 2, 2);
            if ((rc = expect_error(&ctx, GL_NO_ERROR, 202, "signed array immutable storage")))
                return rc;
            obj = mgl_texture_find(&ctx, name);
            if (obj == NULL || obj->levels[0].data_size != 32u || cap.storage_format != GL_RGBA8I)
                return fail(203, "signed array storage metadata");
            glDeleteTextures(1, &name);
        }
        {
            GLuint name = 0u;
            glGenTextures(1, &name);
            glBindTexture(GL_TEXTURE_2D, name);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16, 2, 2);
            if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 204, "norm16 extension required")))
                return rc;
            obj = mgl_texture_find(&ctx, name);
            if (obj == NULL || obj->immutable_format)
                return fail(205, "norm16 rejection preserves texture state");
            glDeleteTextures(1, &name);
        }
        cap.norm16_extension = 1;
        {
            static const struct {
                GLenum internal, format, type;
                size_t size;
            } norm16[] = {{GL_R16, GL_RED, GL_UNSIGNED_SHORT, 2u},
                          {GL_RG16, GL_RG, GL_UNSIGNED_SHORT, 4u},
                          {GL_RGB16, GL_RGB, GL_UNSIGNED_SHORT, 6u},
                          {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, 8u},
                          {GL_R16_SNORM, GL_RED, GL_SHORT, 2u},
                          {GL_RG16_SNORM, GL_RG, GL_SHORT, 4u},
                          {GL_RGB16_SNORM, GL_RGB, GL_SHORT, 6u},
                          {GL_RGBA16_SNORM, GL_RGBA, GL_SHORT, 8u}};

            size_t k;
            for (k = 0u; k < sizeof(norm16) / sizeof(norm16[0]); ++k) {
                GLuint name = 0u;
                glGenTextures(1, &name);
                glBindTexture(GL_TEXTURE_2D, name);
                glTexStorage2D(GL_TEXTURE_2D, 1, norm16[k].internal, 2, 2);
                if ((rc = expect_error(&ctx, GL_NO_ERROR, 206, "native norm16 storage")))
                    return rc;
                obj = mgl_texture_find(&ctx, name);
                if (obj == NULL || !obj->immutable_format ||
                    obj->levels[0].format != norm16[k].format ||
                    obj->levels[0].type != norm16[k].type ||
                    obj->levels[0].data_size != 4u * norm16[k].size ||
                    cap.storage_format != norm16[k].internal)
                    return fail(207, "norm16 signedness and native storage");
                glDeleteTextures(1, &name);
            }
        }
        cap.norm16_extension = 0;
        glBindTexture(GL_TEXTURE_2D, tex);
        cap.storage2_calls = 0u;
        cap.storage3_calls = 0u;
    }

    {
        unsigned initial_param_calls = cap.param_calls;
        GLint result_i = -1;
        GLfloat result_f = -1.0f;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 75, "zero integer LOD bias")))
            return rc;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0.0f);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 76, "zero float LOD bias")))
            return rc;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, &result_i);
        glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, &result_f);
        if (result_i != 0 || result_f != 0.0f || cap.param_calls != initial_param_calls)
            return fail(77, "zero LOD bias getter/native semantics");
        glMultiTexParameteri(GL_TEXTURE0, GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0);
        glGetMultiTexParameteriv(GL_TEXTURE0, GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, &result_i);
        if (result_i != 0 || cap.param_calls != initial_param_calls ||
            (rc = expect_error(&ctx, GL_NO_ERROR, 78, "multitexture zero LOD bias")))
            return rc ? rc : fail(78, "multitexture LOD bias");
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 1);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 79, "integer LOD bias")))
            return rc;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0.5f);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 80, "fractional LOD bias")))
            return rc;
        glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, &result_f);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, &result_i);
        if (result_f != 0.5f || result_i != 1 || cap.param_calls != initial_param_calls)
            return fail(83, "Mesa fractional bias storage/rounding without invalid GLES parameter");
        glMultiTexParameterf(GL_TEXTURE0, GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -1.25f);
        glGetMultiTexParameterfv(GL_TEXTURE0, GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, &result_f);
        if (result_f != -1.25f ||
            (rc = expect_error(&ctx, GL_NO_ERROR, 84, "multitexture fractional bias")))
            return rc ? rc : fail(84, "multitexture LOD bias storage");
    }
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 8, 4);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 3, "2D storage")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, tex);
    if (obj == NULL || !obj->immutable_format || obj->immutable_levels != 3 ||
        !obj->native_immutable_storage)
        return fail(4, "immutable ownership");
    if (!obj->levels[0].defined || obj->levels[0].width != 8 || obj->levels[0].height != 4 ||
        obj->levels[1].width != 4 || obj->levels[1].height != 2 || obj->levels[2].width != 2 ||
        obj->levels[2].height != 1)
        return fail(5, "2D mip dimensions");
    if (cap.storage2_calls != 1u || cap.storage_levels != 3 || cap.storage_format != GL_RGBA8 ||
        cap.storage_width != 8 || cap.storage_height != 4)
        return fail(6, "native storage lowering");
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT, &value);
    if (value != GL_TRUE)
        return fail(7, "immutable format query");
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_LEVELS, &value);
    if (value != 3)
        return fail(8, "immutable levels query");

    glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, patch);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 9, "immutable subimage")) != 0 ||
        cap.subimage2_calls != 1u)
        return rc ? rc : fail(10, "subimage lowering");
    if (memcmp(obj->levels[1].data, patch, 4u) != 0)
        return fail(11, "subimage canonical data");

    image_calls = cap.image2_calls;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 12, "TexImage immutable rejection")) != 0)
        return rc;
    if (cap.image2_calls != image_calls || obj->levels[0].width != 8)
        return fail(13, "TexImage mutated immutable texture");
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 8, 4, 0, 16, patch);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 14, "compressed immutable rejection")) != 0)
        return rc;
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 15, "second storage rejection")) != 0)
        return rc;

    glCreateTextures(GL_TEXTURE_2D, 1, &other);
    glTextureParameteri(other, GL_TEXTURE_LOD_BIAS, 0);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 81, "named-texture zero bias")))
        return rc;
    glTextureParameterf(other, GL_TEXTURE_LOD_BIAS, 0.75f);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 82, "named-texture bias")))
        return rc;
    {
        GLfloat bias = 0.0f;
        glGetTextureParameterfv(other, GL_TEXTURE_LOD_BIAS, &bias);
        if (bias != 0.75f || (rc = expect_error(&ctx, GL_NO_ERROR, 85, "named-texture bias query")))
            return rc ? rc : fail(85, "named texture bias roundtrip");
    }
    glTextureStorage2D(other, 2, GL_RGB8, 4, 4);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 16, "DSA storage")) != 0)
        return rc;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
    if ((GLuint)value != tex)
        return fail(17, "DSA interface binding preservation");
    if (cap.bound_2d != obj->native_name)
        return fail(18, "DSA native binding restoration");
    glGetTextureParameteriv(other, GL_TEXTURE_IMMUTABLE_LEVELS, &value);
    if (value != 2)
        return fail(19, "DSA immutable query");
    glTextureStorage2D(0u, 1, GL_RGBA8, 1, 1);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 20, "DSA zero texture")) != 0)
        return rc;

    glCreateTextures(GL_TEXTURE_3D, 1, &tex3d);
    glTextureStorage3D(tex3d, 3, GL_RGBA8, 8, 4, 4);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 21, "3D storage")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, tex3d);
    if (obj->levels[0].depth != 4 || obj->levels[1].depth != 2 || obj->levels[2].depth != 1 ||
        cap.storage3_calls != 1u)
        return fail(22, "3D mip depth");

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &array);
    glTextureStorage3D(array, 3, GL_RGBA8, 8, 4, 5);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 23, "array storage")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, array);
    if (obj->levels[0].depth != 5 || obj->levels[1].depth != 5 || obj->levels[2].depth != 5)
        return fail(24, "array layer count changed across mip levels");

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cube);
    glTextureStorage2D(cube, 3, GL_DEPTH_COMPONENT24, 4, 4);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 25, "cube storage")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, cube);
    {
        unsigned face;
        for (face = 0u; face < 6u; ++face)
            if (!obj->cube_levels[face][2].defined || obj->cube_levels[face][2].width != 1 ||
                obj->cube_levels[face][2].height != 1)
                return fail(26, "cube face storage");
    }

    glGenTextures(1, &invalid);
    glBindTexture(GL_TEXTURE_2D, invalid);
    glTexStorage2D(GL_TEXTURE_2D, 4, GL_RGBA8, 4, 4);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 27, "too many levels")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, invalid);
    if (obj == NULL || obj->immutable_format || obj->levels[0].defined)
        return fail(28, "failed storage mutated object");
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA, 4, 4);
    if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 29, "unsized internal format")) != 0)
        return rc;

    /* Mutable luminance-alpha textures use the LA8-to-RG8 conversion. */
    {
        const unsigned char la[8] = {10, 220, 15, 190, 44, 140, 70, 80};
        unsigned char update[2] = {120, 40};
        GLuint mutable_la = 0;
        GLint query = 0;
        unsigned image_before = cap.image2_calls;
        unsigned swizzle_before = cap.swizzle_calls;
        glGenTextures(1, &mutable_la);
        glBindTexture(GL_TEXTURE_2D, mutable_la);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 2, 2, 0, GL_LUMINANCE_ALPHA,
                     GL_UNSIGNED_BYTE, la);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 101, "mutable unsized LA glTexImage2D")))
            return rc;
        obj = mgl_texture_find(&ctx, mutable_la);
        if (obj == NULL || !obj->levels[0].defined ||
            obj->levels[0].internal_format != GL_LUMINANCE8_ALPHA8 ||
            obj->levels[0].format != GL_LUMINANCE_ALPHA || obj->levels[0].data_size != sizeof(la) ||
            memcmp(obj->levels[0].data, la, sizeof(la)) || cap.image2_calls != image_before + 1 ||
            cap.last_image_internal != GL_RG8 || cap.last_image_format != GL_RG ||
            cap.swizzle_calls != swizzle_before + 4 || cap.swizzle[0] != GL_RED ||
            cap.swizzle[1] != GL_RED || cap.swizzle[2] != GL_RED || cap.swizzle[3] != GL_GREEN)
            return fail(102, "mutable LA texture needs RG8 data plus luminance alpha swizzle");
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &query);
        if (query != (GLint)GL_LUMINANCE8_ALPHA8 ||
            (rc = expect_error(&ctx, GL_NO_ERROR, 103, "canonical mutable LA storage query")))
            return rc ? rc : fail(103, "canonical mutable LA query");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, update);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 104, "mutable LA subupload")))
            return rc;
        if (cap.last_subimage_format != GL_RG || obj->levels[0].data[6] != 120 ||
            obj->levels[0].data[7] != 40)
            return fail(105, "mutable LA subupload must preserve two channels");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     NULL);
        if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 106,
                               "LA internalformat with incompatible external format")))
            return rc;
    }

    /* ARB_texture_storage GLES dependency explicitly allows
     * GL_LUMINANCE8_ALPHA8. GLES core RG8 plus (R,R,R,G) swizzle must
     * preserve luminance/alpha channels during immutable uploads. */
    {
        const unsigned char la[8] = {10u, 250u, 33u, 120u, 64u, 192u, 89u, 5u};
        unsigned swizzles = cap.swizzle_calls;
        unsigned storage_before = cap.storage2_calls;
        native.glesTexStorage2D = fake_tex_storage2;
        glGenTextures(1, &legacy);
        glBindTexture(GL_TEXTURE_2D, legacy);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_LUMINANCE8_ALPHA8, 2, 2);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 90, "legacy LA immutable allocation")))
            return rc;
        obj = mgl_texture_find(&ctx, legacy);
        if (obj == NULL || !obj->immutable_format || obj->levels[0].format != GL_LUMINANCE_ALPHA ||
            obj->levels[0].type != GL_UNSIGNED_BYTE || obj->levels[0].data_size != 8u ||
            cap.storage2_calls != storage_before + 1u || cap.storage_format != GL_RG8)
            return fail(91, "canonical LA8 / native RG8 storage");
        if (cap.swizzle_calls != swizzles + 4u || cap.swizzle[0] != (GLint)GL_RED ||
            cap.swizzle[1] != (GLint)GL_RED || cap.swizzle[2] != (GLint)GL_RED ||
            cap.swizzle[3] != (GLint)GL_GREEN)
            return fail(92, "legacy LA8 native color swizzle");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, la);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 93, "LA8 two-channel upload")))
            return rc;
        if (memcmp(obj->levels[0].data, la, sizeof(la)) != 0 || cap.last_subimage_format != GL_RG ||
            cap.last_subimage_la[0] != 10u || cap.last_subimage_la[1] != 250u)
            return fail(94, "LA8 canonical data / GPU RG upload");
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2);
        if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 95, "immutable cannot change format")))
            return rc;

        glGenTextures(1, &invalid);
        glBindTexture(GL_TEXTURE_2D, invalid);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_LUMINANCE_ALPHA, 2, 2);
        if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 96, "unsized luminance-alpha storage")))
            return rc;
        {
            unsigned storage_before = cap.storage2_calls;
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_LUMINANCE8_ALPHA8, 2, 2);
            if ((rc = expect_error(&ctx, GL_NO_ERROR, 97, "legacy LA8 immutable storage")))
                return rc;
            obj = mgl_texture_find(&ctx, invalid);
            if (obj == NULL || !obj->immutable_format || cap.storage2_calls != storage_before + 1u ||
                cap.storage_format != GL_RG8)
                return fail(98, "legacy LA8 uses native RG8 immutable storage");
        }
    }

    /* GLES immutable RGB5 storage must be realized as RGB565. */
    native.glesTexStorage2D = fake_tex_storage2;
    glGenTextures(1, &rgb5);
    glBindTexture(GL_TEXTURE_2D, rgb5);
    {
        static const unsigned char pixel565[2] = {0x1fu, 0xf8u};
        GLuint initial_storage = cap.storage2_calls;
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB5, 1560, 720);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 110, "desktop RGB5 immutable storage")))
            return rc;
        obj = mgl_texture_find(&ctx, rgb5);
        if (obj == NULL || !obj->immutable_format || obj->levels[0].internal_format != GL_RGB5 ||
            obj->levels[0].format != GL_RGB || obj->levels[0].type != GL_UNSIGNED_SHORT_5_6_5 ||
            obj->levels[0].data_size != (size_t)1560u * 720u * 2u ||
            cap.storage2_calls != initial_storage + 1u || cap.storage_format != GL_RGB565 ||
            cap.storage_width != 1560 || cap.storage_height != 720)
            return fail(111, "RGB5 interface shadow and native GLES RGB565 storage");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixel565);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 112, "RGB565 immutable subimage")))
            return rc;
        if (cap.last_subimage_format != GL_RGB ||
            cap.last_subimage_type != GL_UNSIGNED_SHORT_5_6_5 ||
            memcmp(obj->levels[0].data, pixel565, sizeof(pixel565)))
            return fail(113, "RGB565 subimage unchanged through translation");
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &value);
        if (value != GL_RGB5 ||
            (rc = expect_error(&ctx, GL_NO_ERROR, 114, "RGB5 desktop format query")))
            return rc ? rc : fail(114, "RGB5 desktop format query");
    }
    glBindTexture(GL_TEXTURE_2D, tex);
    {
        static const signed char signed_data[4] = {-8, 11, 120, -127};
        static const unsigned char unsigned_data[4] = {5u, 16u, 253u, 255u};
        static const unsigned short wide_data[4] = {5u, 16u, 300u, 255u};
        static const unsigned char clamped_data[4] = {5u, 16u, 255u, 255u};
        GLuint integer_tex = 0u;
        glGenTextures(1, &integer_tex);
        glBindTexture(GL_TEXTURE_2D, integer_tex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8I, 1, 1);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 240, "signed integer storage")))
            return rc;
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA_INTEGER, GL_BYTE, signed_data);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 241, "signed integer upload")))
            return rc;
        obj = mgl_texture_find(&ctx, integer_tex);
        if (!obj || memcmp(obj->levels[0].data, signed_data, 4u) ||
            cap.last_subimage_format != GL_RGBA_INTEGER || cap.last_subimage_type != GL_BYTE ||
            memcmp(cap.last_subimage_integer, signed_data, 4u))
            return fail(242, "signed integer data preservation");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_BYTE, signed_data);
        if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 243, "integer format validation")))
            return rc;
        glDeleteTextures(1, &integer_tex);

        glGenTextures(1, &integer_tex);
        glBindTexture(GL_TEXTURE_2D, integer_tex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8UI, 1, 1);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 244, "unsigned integer storage")))
            return rc;
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,
                        unsigned_data);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 245, "unsigned integer upload")))
            return rc;
        obj = mgl_texture_find(&ctx, integer_tex);
        if (!obj || memcmp(obj->levels[0].data, unsigned_data, 4u) ||
            cap.last_subimage_format != GL_RGBA_INTEGER ||
            cap.last_subimage_type != GL_UNSIGNED_BYTE ||
            memcmp(cap.last_subimage_integer, unsigned_data, 4u))
            return fail(246, "unsigned integer data preservation");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT,
                        wide_data);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 247, "unsigned integer conversion")))
            return rc;
        if (memcmp(obj->levels[0].data, clamped_data, 4u) ||
            cap.last_subimage_type != GL_UNSIGNED_BYTE ||
            memcmp(cap.last_subimage_integer, clamped_data, 4u))
            return fail(248, "unsigned integer conversion preserved");
        glDeleteTextures(1, &integer_tex);

        glGenTextures(1, &integer_tex);
        glBindTexture(GL_TEXTURE_2D, integer_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 1, 1, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,
                     unsigned_data);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 249, "mutable unsigned integer allocation")))
            return rc;
        obj = mgl_texture_find(&ctx, integer_tex);
        if (!obj || obj->levels[0].internal_format != GL_RGBA8UI ||
            obj->levels[0].format != GL_RGBA_INTEGER || cap.last_image_internal != GL_RGBA8UI ||
            cap.last_image_format != GL_RGBA_INTEGER ||
            memcmp(obj->levels[0].data, unsigned_data, 4u))
            return fail(250, "mutable unsigned integer native format and data");
        glDeleteTextures(1, &integer_tex);
        glBindTexture(GL_TEXTURE_2D, tex);
    }

    {
        GLuint one_d[2] = {0u, 0u};
        struct mgl_texture *source, *dest;
        GLint current = -1;
        unsigned char texels[16] = {1u, 2u,  3u,  4u,  5u,  6u,  7u,  8u,
                                    9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u};
        glCreateTextures(GL_TEXTURE_1D, 2, one_d);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 117, "created 1D textures")))
            return rc;
        glBindTexture(GL_TEXTURE_1D, one_d[0]);
        source = mgl_texture_find(&ctx, one_d[0]);
        dest = mgl_texture_find(&ctx, one_d[1]);
        if (source == NULL || dest == NULL)
            return fail(118, "1D texture objects");
        glTextureStorage1D(one_d[1], 1, GL_RGBA8, 4);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 119, "1D immutable DSA storage")))
            return rc;
        if (!dest->immutable_format || dest->levels[0].width != 4 || dest->levels[0].height != 1 ||
            !ctx.texture_2d_prefer_1d[ctx.compat.active_texture] ||
            cap.bound_2d != source->native_name)
            return fail(120, "1D DSA storage and native binding restoration");
        glTextureSubImage1D(one_d[1], 0, 0, 4, GL_RGBA, GL_UNSIGNED_BYTE, texels);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 121, "1D DSA subimage")))
            return rc;
        if (!dest->levels[0].defined || dest->levels[0].data_size < sizeof(texels) ||
            memcmp(dest->levels[0].data, texels, sizeof(texels)) ||
            cap.bound_2d != source->native_name)
            return fail(122, "1D DSA subimage data and native restoration");
        glGetIntegerv(GL_TEXTURE_BINDING_1D, &current);
        if (current != (GLint)one_d[0] ||
            (rc = expect_error(&ctx, GL_NO_ERROR, 123, "1D DSA logical binding")))
            return rc ? rc : fail(123, "1D DSA logical binding");
        glTextureStorage1D(one_d[1], 1, GL_RGBA8, 4);
        if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 124,
                               "immutable 1D storage cannot be redefined")))
            return rc;
        glTextureSubImage1D(one_d[1], 0, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, texels);
        if ((rc = expect_error(&ctx, GL_INVALID_VALUE, 125, "1D subimage out of range")))
            return rc;
        glTextureStorage1D(0u, 1, GL_RGBA8, 4);
        if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 126, "zero DSA name")))
            return rc;
        glTextureStorage1D(tex, 1, GL_RGBA8, 4);
        if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 127, "wrong DSA target")))
            return rc;
        glCopyTextureSubImage1D(one_d[1], 0, 0, 0, 0, 1);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 131, "1D DSA framebuffer copy")))
            return rc;
        if (dest->levels[0].data[0] != 0x73u || cap.bound_2d != source->native_name)
            return fail(132, "1D copy data and native restoration");
        {
            GLuint compressed = 0u;
            unsigned char init_block[8] = {0};
            unsigned char next_block[8] = {4u, 3u, 2u, 1u, 0u, 0u, 0u, 0u};
            struct mgl_texture *compressed_obj;
            glCreateTextures(GL_TEXTURE_1D, 1, &compressed);
            glBindTexture(GL_TEXTURE_1D, compressed);
            glCompressedTexImage1D(GL_TEXTURE_1D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 4, 0, 8,
                                   init_block);
            if ((rc =
                     expect_error(&ctx, GL_INVALID_ENUM, 133, "specific BCn format on 1D texture")))
                return rc;
            glBindTexture(GL_TEXTURE_1D, one_d[0]);
            glCompressedTextureSubImage1D(compressed, 0, 0, 4, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 8,
                                          next_block);
            if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 134,
                                   "specific BCn format on 1D subimage")))
                return rc;
            compressed_obj = mgl_texture_find(&ctx, compressed);
            if (compressed_obj == NULL || compressed_obj->levels[0].defined ||
                cap.bound_2d != source->native_name)
                return fail(135, "invalid compressed 1D call preserved texture and binding");
            glDeleteTextures(1, &compressed);
        }
        glCompressedTextureSubImage1D(one_d[1], 0, 0, 4, GL_RGBA8, 0, texels);
        if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 128, "invalid compressed format")))
            return rc;
        glCopyTextureSubImage1D(one_d[1], 0, 0, 0, 0, -1);
        if ((rc = expect_error(&ctx, GL_INVALID_VALUE, 129, "invalid 1D copy width")))
            return rc;
        if (cap.bound_2d != source->native_name)
            return fail(130, "1D DSA error restoration");
        glDeleteTextures(2, one_d);
        glBindTexture(GL_TEXTURE_2D, tex);
    }

    if (mgl_proc_address("glTexStorage2D") == NULL || mgl_proc_address("glTexStorage3D") == NULL ||
        mgl_proc_address("glTextureStorage2D") == NULL ||
        mgl_proc_address("glTextureStorage3D") == NULL ||
        mgl_proc_address("glTextureStorage1D") == NULL ||
        mgl_proc_address("glTextureSubImage1D") == NULL ||
        mgl_proc_address("glCompressedTextureSubImage1D") == NULL ||
        mgl_proc_address("glCopyTextureSubImage1D") == NULL)
        return fail(37, "proc-address exposure");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL immutable texture storage test: PASS");
    return 0;
}

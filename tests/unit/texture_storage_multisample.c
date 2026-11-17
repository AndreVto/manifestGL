#include "mgl/gl.h"
#include "abi/proc_address.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    GLuint bound_ms;
    GLuint bound_msa;
    unsigned storage2_calls;
    unsigned storage3_calls;
    unsigned delete_calls;
    unsigned param_calls;
    GLenum last_param_pname;
    GLint last_param;
    GLsizei samples, width, height, depth;
    GLenum internalformat;
    GLboolean fixed;
} cap;

static void fake_gen(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_texture;
}

static void fake_delete(GLsizei n, const GLuint *names) {
    (void)names;
    if (n > 0)
        ++cap.delete_calls;
}

static void fake_active(GLenum texture) {
    (void)texture;
}

static void fake_bind(GLenum target, GLuint texture) {
    if (target == GL_TEXTURE_2D_MULTISAMPLE)
        cap.bound_ms = texture;
    else if (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        cap.bound_msa = texture;
}

static void fake_storage2(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                          GLsizei height, GLboolean fixed) {
    (void)target;
    ++cap.storage2_calls;
    cap.samples = samples;
    cap.internalformat = internalformat;
    cap.width = width;
    cap.height = height;
    cap.fixed = fixed;
}

static void fake_storage3(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                          GLsizei height, GLsizei depth, GLboolean fixed) {
    (void)target;
    ++cap.storage3_calls;
    cap.samples = samples;
    cap.internalformat = internalformat;
    cap.width = width;
    cap.height = height;
    cap.depth = depth;
    cap.fixed = fixed;
}

static int optional_formats_available;

static const GLubyte *fake_get_string(GLenum pname) {
    if (pname != GL_EXTENSIONS)
        return NULL;
    return (const GLubyte *)(optional_formats_available
                                 ? "GL_EXT_render_snorm GL_EXT_texture_norm16 GL_OES_texture_float_linear"
                                 : "GL_OES_texture_float_linear");
}

static void fake_param(GLenum target, GLenum pname, GLint param) {
    (void)target;
    ++cap.param_calls;
    cap.last_param_pname = pname;
    cap.last_param = param;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "multisample storage failure %d: %s\n", code, message);
    return code;
}

static int expect_error(struct mgl_context *ctx, GLenum expected, int code, const char *message) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr, "multisample storage failure %d: %s (got 0x%04x expected 0x%04x)\n", code,
                message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj;
    GLuint tex = 0u, other = 0u, array = 0u, mutable_tex = 0u;
    GLint value = 0;
    unsigned calls;
    GLuint old_native;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.next_texture = 1200u;
    native.glesGenTextures = fake_gen;
    native.glesDeleteTextures = fake_delete;
    native.glesActiveTexture = fake_active;
    native.glesBindTexture = fake_bind;
    native.glesTexStorage2DMultisample = fake_storage2;
    native.glesTexStorage3DMultisample = fake_storage3;
    native.glesTexParameteri = fake_param;
    native.glesGetString = fake_get_string;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, 16, 8, GL_TRUE);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 2, "default multisample texture")) != 0)
        return rc;

    glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &tex);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex);
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, 16, 8, GL_TRUE);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 3, "2D multisample storage")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, tex);
    if (obj == NULL || !obj->immutable_format || obj->immutable_levels != 1 ||
        !obj->native_immutable_storage || !obj->levels[0].defined || obj->levels[0].samples != 4 ||
        obj->levels[0].width != 16 || obj->levels[0].height != 8)
        return fail(4, "2D immutable multisample ownership");
    if (cap.storage2_calls != 1u || cap.samples != 4 || cap.internalformat != GL_RGBA8 ||
        cap.width != 16 || cap.height != 8 || !cap.fixed)
        return fail(5, "2D multisample native lowering");
    glGetTexParameteriv(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_IMMUTABLE_FORMAT, &value);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 6, "bound immutable query")) != 0 || value != GL_TRUE)
        return rc ? rc : fail(7, "bound immutable query value");
    glGetTexParameteriv(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_IMMUTABLE_LEVELS, &value);
    if (value != 1)
        return fail(8, "bound immutable levels");

    calls = cap.storage2_calls;
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 2, GL_RGBA8, 4, 4, GL_FALSE);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 9, "mutable redefine immutable storage")) !=
        0)
        return rc;
    if (cap.storage2_calls != calls || obj->levels[0].width != 16)
        return fail(10, "immutable redefine mutated state/native");
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 2, GL_RGBA8, 4, 4, GL_FALSE);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 11, "second immutable storage")) != 0)
        return rc;

    calls = cap.param_calls;
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 12, "multisample sampler parameter")) != 0)
        return rc;
    if (cap.param_calls != calls)
        return fail(13, "forbidden multisample parameter reached native");
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_BASE_LEVEL, 1);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 14, "multisample nonzero base level")) != 0)
        return rc;
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_BASE_LEVEL, 0);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 15, "multisample zero base level")) != 0)
        return rc;
    if (cap.last_param_pname != GL_TEXTURE_BASE_LEVEL || cap.last_param != 0)
        return fail(16, "legal base level not lowered");

    glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &other);
    glTextureStorage2DMultisample(other, 8, GL_RGB8, 32, 16, GL_FALSE);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 17, "DSA multisample storage")) != 0)
        return rc;
    glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE, &value);
    if ((GLuint)value != tex)
        return fail(18, "DSA changed interface binding");
    if (cap.bound_ms != mgl_texture_find(&ctx, tex)->native_name)
        return fail(19, "DSA native binding not restored");
    glGetTextureParameteriv(other, GL_TEXTURE_IMMUTABLE_FORMAT, &value);
    if (value != GL_TRUE)
        return fail(20, "DSA immutable query");
    glBindTextureUnit(3u, other);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 21, "BindTextureUnit multisample")) != 0 ||
        ctx.texture_2d_multisample_binding[3] != other)
        return rc ? rc : fail(22, "BindTextureUnit multisample ownership");

    glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 1, &array);
    glTextureStorage3DMultisample(array, 2, GL_DEPTH_COMPONENT24, 8, 4, 6, GL_TRUE);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 23, "array multisample storage")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, array);
    if (obj == NULL || !obj->immutable_format || obj->levels[0].depth != 6 ||
        obj->levels[0].samples != 2 || cap.storage3_calls != 1u)
        return fail(24, "array immutable multisample ownership");
    glTexImage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 4, GL_DEPTH_COMPONENT24, 4, 4, 2,
                            GL_FALSE);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 25,
                           "array mutable redefine immutable storage")) != 0)
        return rc;

    /* Storage after a mutable multisample definition must recreate the native object before issuing
     * immutable storage. */
    glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &mutable_tex);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mutable_tex);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 2, GL_RGBA8, 8, 8, GL_TRUE);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 26, "mutable multisample definition")) != 0)
        return rc;
    obj = mgl_texture_find(&ctx, mutable_tex);
    old_native = obj->native_name;
    calls = cap.delete_calls;
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, 8, 8, GL_TRUE);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 27, "storage replaces mutable multisample")) != 0)
        return rc;
    if (!obj->immutable_format || obj->native_name == old_native || cap.delete_calls != calls + 1u)
        return fail(28, "mutable-to-immutable native recreation");

    {
        GLuint invalid_tex = 0u;
        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &invalid_tex);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, invalid_tex);
    }
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, 0, 4, GL_TRUE);
    if ((rc = expect_error(&ctx, GL_INVALID_VALUE, 29, "zero storage width")) != 0)
        return rc;

    {
        static const GLenum formats[] = {
            GL_R8, GL_RG8, GL_RGB8, GL_RGBA8,
            GL_SRGB8_ALPHA8, GL_RGB565, GL_RGB5_A1, GL_RGBA4,
            GL_RGB10_A2, GL_RGB10_A2UI, GL_R11F_G11F_B10F, GL_R16F,
            GL_RG16F, GL_RGBA16F, GL_R32F, GL_RG32F,
            GL_RGBA32F, GL_R8I, GL_R8UI, GL_RG8I,
            GL_RG8UI, GL_RGBA8I, GL_RGBA8UI, GL_R16I,
            GL_R16UI, GL_RG16I, GL_RG16UI, GL_RGBA16I,
            GL_RGBA16UI, GL_R32I, GL_R32UI, GL_RG32I,
            GL_RG32UI, GL_RGBA32I, GL_RGBA32UI, GL_DEPTH_COMPONENT16,
            GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT32F, GL_DEPTH24_STENCIL8,
            GL_DEPTH32F_STENCIL8, GL_STENCIL_INDEX8,
        };
        GLuint format_tex = 0u;
        size_t i;
        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &format_tex);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, format_tex);
        for (i = 0u; i < sizeof(formats) / sizeof(formats[0]); ++i) {
            unsigned before = cap.storage2_calls;
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, formats[i], 12, 8, GL_TRUE);
            if ((rc = expect_error(&ctx, GL_NO_ERROR, 31, "multisample renderable format")) != 0)
                return rc;
            GLenum expected = formats[i] == GL_DEPTH_COMPONENT32 ? GL_DEPTH_COMPONENT32F
                                                                  : formats[i];
            if (cap.storage2_calls != before + 1u || cap.internalformat != expected) {
                fprintf(stderr, "format 0x%x native 0x%x calls %u/%u\n",
                        formats[i], cap.internalformat, cap.storage2_calls, before);
                return fail(32, "multisample native storage format");
            }
        }
        calls = cap.storage2_calls;
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGB8_SNORM, 12, 8, GL_TRUE);
        if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 33, "nonrenderable multisample format")) != 0)
            return rc;
        if (cap.storage2_calls != calls)
            return fail(34, "invalid multisample format reached native");
        {
            static const struct {
                GLenum format, native_format;
            } aliases[] = {
                {GL_RGB5, GL_RGB565}, {GL_RGB16F, GL_RGBA16F},
                {GL_RGB32F_ARB, GL_RGBA32F}, {GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT32F}
            };
            for (i = 0u; i < sizeof(aliases) / sizeof(aliases[0]); ++i) {
                calls = cap.storage2_calls;
                glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, aliases[i].format,
                                        12, 8, GL_TRUE);
                if ((rc = expect_error(&ctx, GL_NO_ERROR, 37,
                                       "desktop multisample storage conversion")) != 0)
                    return rc;
                if (cap.storage2_calls != calls + 1u ||
                    cap.internalformat != aliases[i].native_format)
                    return fail(38, "desktop multisample native format");
            }
        }
        {
            static const GLenum optional_formats[] = {
                GL_R8_SNORM, GL_RG8_SNORM, GL_RGBA8_SNORM,
                GL_R16, GL_RG16, GL_RGBA16,
                GL_R16_SNORM, GL_RG16_SNORM, GL_RGBA16_SNORM
            };
            for (i = 0u; i < sizeof(optional_formats) / sizeof(optional_formats[0]); ++i) {
                calls = cap.storage2_calls;
                glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1,
                                        optional_formats[i], 12, 8, GL_TRUE);
                if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 39,
                                       "unsupported optional renderable format")) != 0)
                    return rc;
                if (cap.storage2_calls != calls)
                    return fail(40, "unsupported optional format forwarded");
            }
            optional_formats_available = 1;
            for (i = 0u; i < sizeof(optional_formats) / sizeof(optional_formats[0]); ++i) {
                calls = cap.storage2_calls;
                glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1,
                                        optional_formats[i], 12, 8, GL_TRUE);
                if ((rc = expect_error(&ctx, GL_NO_ERROR, 41,
                                       "optional renderable format")) != 0)
                    return rc;
                if (cap.storage2_calls != calls + 1u ||
                    cap.internalformat != optional_formats[i])
                    return fail(42, "optional format native storage");
            }
            optional_formats_available = 0;
        }
    }
    {
        GLuint depth_array = 0u;
        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 1, &depth_array);
        calls = cap.storage3_calls;
        glTextureStorage3DMultisample(depth_array, 4, GL_DEPTH24_STENCIL8, 32, 16, 3, GL_TRUE);
        if ((rc = expect_error(&ctx, GL_NO_ERROR, 35, "depth-stencil multisample array storage")) != 0)
            return rc;
        if (cap.storage3_calls != calls + 1u || cap.internalformat != GL_DEPTH24_STENCIL8 ||
            cap.depth != 3 || cap.samples != 4)
            return fail(36, "depth-stencil multisample array native storage");
    }

    if (mgl_proc_address("glTexStorage2DMultisample") == NULL ||
        mgl_proc_address("glTexStorage3DMultisample") == NULL ||
        mgl_proc_address("glTextureStorage2DMultisample") == NULL ||
        mgl_proc_address("glTextureStorage3DMultisample") == NULL)
        return fail(30, "proc-address exposure");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL immutable multisample texture storage test: PASS");
    return 0;
}

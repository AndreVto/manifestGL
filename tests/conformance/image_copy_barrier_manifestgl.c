#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "core/error.h"
#include "conformance/image_copy_barrier_cases.h"
#include <string.h>
#include <stdlib.h>

static GLuint next_id;
static const char *extension_text = "GL_EXT_copy_image GL_NV_texture_barrier GL_EXT_texture_view";

static const GLubyte *extensions(GLenum type) {
    (void)type;
    return (const GLubyte *)extension_text;
}

static void gen(GLsizei n, GLuint *names) {
    for (GLsizei i = 0; i < n; ++i)
        names[i] = ++next_id;
}

static void bind(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void upload(GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty,
                   const void *d) {
    (void)t;
    (void)l;
    (void)i;
    (void)w;
    (void)h;
    (void)b;
    (void)f;
    (void)ty;
    (void)d;
}

static void copy_image(GLuint sn, GLenum st, GLint sl, GLint sx, GLint sy, GLint sz, GLuint dn,
                       GLenum dt, GLint dl, GLint dx, GLint dy, GLint dz, GLsizei w, GLsizei h,
                       GLsizei dep) {
    (void)sn;
    (void)st;
    (void)sl;
    (void)sx;
    (void)sy;
    (void)sz;
    (void)dn;
    (void)dt;
    (void)dl;
    (void)dx;
    (void)dy;
    (void)dz;
    (void)w;
    (void)h;
    (void)dep;
}

static void barrier(void) {
}

static void storage(GLenum target, GLsizei n, GLenum f, GLsizei w, GLsizei h) {
    (void)target;
    (void)n;
    (void)f;
    (void)w;
    (void)h;
}

static GLenum injected_error;
static unsigned deleted_views;

static GLenum get_native_error(void) {
    GLenum e = injected_error;
    injected_error = GL_NO_ERROR;
    return e;
}

static void delete_native(GLsizei n, const GLuint *ids) {
    if (n == 1 && ids != NULL && ids[0] != 0u)
        ++deleted_views;
}

static unsigned view_calls, array_view_calls, cube_view_calls, cross_target_calls;
static unsigned srgb_view_calls, srgb_alpha_view_calls;
static GLuint viewed_name, source_name, viewed_level;
static GLenum viewed_format;
static GLuint viewed_numlevels;

static void view_create(GLuint view, GLenum target, GLuint source, GLenum format, GLuint minlevel,
                        GLuint numlevels, GLuint minlayer, GLuint numlayers) {
    (void)target;
    (void)numlevels;
    (void)minlayer;
    (void)numlayers;
    ++view_calls;
    if (format == GL_SRGB8)
        ++srgb_view_calls;
    if (format == GL_SRGB8_ALPHA8)
        ++srgb_alpha_view_calls;
    if (target == GL_TEXTURE_2D && numlayers == 1u && (minlayer == 2u || minlayer == 4u))
        ++cross_target_calls;
    if (target == GL_TEXTURE_2D_ARRAY) {
        if (numlayers == 0u || minlayer + numlayers > 4u)
            abort();
        ++array_view_calls;
    }
    if (target == GL_TEXTURE_CUBE_MAP) {
        if (numlayers != 6u)
            abort();
        ++cube_view_calls;
    }
    viewed_name = view;
    source_name = source;
    viewed_level = minlevel;
    viewed_format = format;
    viewed_numlevels = numlevels;
}

static void storage3d(GLenum target, GLsizei n, GLenum format, GLsizei w, GLsizei h, GLsizei d) {
    (void)target;
    (void)n;
    (void)format;
    (void)w;
    (void)h;
    (void)d;
}

static void upload3d(GLenum target, GLint level, GLint internalformat, GLsizei width,
                     GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type,
                     const void *data) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)depth;
    (void)border;
    (void)format;
    (void)type;
    (void)data;
}

static void subimage3d(GLenum target, GLint level, GLint x, GLint y, GLint z, GLsizei w, GLsizei h,
                       GLsizei d, GLenum format, GLenum type, const void *data) {
    (void)target;
    (void)level;
    (void)x;
    (void)y;
    (void)z;
    (void)w;
    (void)h;
    (void)d;
    (void)format;
    (void)type;
    (void)data;
}

static void subimage(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h,
                     GLenum format, GLenum type, const void *data) {
    (void)target;
    (void)level;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)format;
    (void)type;
    (void)data;
}

static void compressed_upload(GLenum target, GLint level, GLint x, GLint y, GLsizei width,
                              GLsizei height, GLenum format, GLsizei size, const void *data) {
    (void)target;
    (void)level;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)format;
    (void)size;
    (void)data;
}

static int texture_view_class_cases(struct mgl_native_dispatch *native) {
    static const struct {
        GLenum original, view, klass;
        GLenum native_fallback;
        int software_supported;
    } pairs[] = {
        {GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT,
         GL_VIEW_CLASS_S3TC_DXT1_RGB, GL_SRGB8_ALPHA8, 1},
        {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT,
         GL_VIEW_CLASS_S3TC_DXT1_RGBA, GL_SRGB8_ALPHA8, 1},
        {GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT,
         GL_VIEW_CLASS_S3TC_DXT3_RGBA, GL_SRGB8_ALPHA8, 1},
        {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,
         GL_VIEW_CLASS_S3TC_DXT5_RGBA, GL_SRGB8_ALPHA8, 1},
        {GL_COMPRESSED_RED_RGTC1, GL_COMPRESSED_SIGNED_RED_RGTC1, GL_VIEW_CLASS_RGTC1_RED,
         GL_R8_SNORM, 0},
        {GL_COMPRESSED_RG_RGTC2, GL_COMPRESSED_SIGNED_RG_RGTC2, GL_VIEW_CLASS_RGTC2_RG,
         GL_RG8_SNORM, 0},
        {GL_COMPRESSED_RGBA_BPTC_UNORM, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM,
         GL_VIEW_CLASS_BPTC_UNORM, GL_SRGB8_ALPHA8, 1},
        {GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT,
         GL_VIEW_CLASS_BPTC_FLOAT, GL_RGB16F, 0},
        {GL_RGBA8, GL_RG16F, GL_VIEW_CLASS_32_BITS, GL_RG16F, 1},
        {GL_R8, GL_R8I, GL_VIEW_CLASS_8_BITS, GL_R8I, 1},
    };

    static const struct {
        GLenum original, view;
    } invalid[] = {
        {GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT},
        {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_COMPRESSED_RGBA_BPTC_UNORM},
        {GL_COMPRESSED_RED_RGTC1, GL_COMPRESSED_RGB_S3TC_DXT1_EXT},
        {GL_RGBA8, GL_RGB8},
    };

    size_t i;
    GLint klass;
    unsigned native_mode;
    native->glesCompressedTexSubImage2D = compressed_upload;
    for (native_mode = 0u; native_mode < 2u; ++native_mode) {
        extension_text = native_mode
                             ? "GL_EXT_texture_view GL_EXT_texture_compression_rgtc "
                               "GL_EXT_texture_compression_s3tc GL_EXT_texture_compression_bptc"
                             : "GL_EXT_texture_view";
        for (i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i) {
            GLuint source, view;
            GLenum expected;
            glGenTextures(1, &source);
            glGenTextures(1, &view);
            glBindTexture(GL_TEXTURE_2D, source);
            glTexStorage2D(GL_TEXTURE_2D, 1, pairs[i].original, 4, 4);
            if (glGetError() != GL_NO_ERROR)
                return 30;
            glGetInternalformativ(GL_TEXTURE_2D, pairs[i].original, GL_VIEW_COMPATIBILITY_CLASS, 1,
                                  &klass);
            if (klass != (GLint)pairs[i].klass || glGetError() != GL_NO_ERROR)
                return 31;
            glGetInternalformativ(GL_TEXTURE_2D, pairs[i].view, GL_VIEW_COMPATIBILITY_CLASS, 1,
                                  &klass);
            if (klass != (GLint)pairs[i].klass || glGetError() != GL_NO_ERROR)
                return 32;
            glTextureView(view, GL_TEXTURE_2D, source, pairs[i].view, 0, 1, 0, 1);
            expected =
                (!native_mode && !pairs[i].software_supported) ? GL_INVALID_OPERATION : GL_NO_ERROR;
            if (glGetError() != expected)
                return 33;
            if (!!glIsTexture(view) != (expected == GL_NO_ERROR))
                return 34;
            if (expected == GL_NO_ERROR &&
                viewed_format !=
                    (native_mode || (pairs[i].original == GL_RGBA8 || pairs[i].original == GL_R8)
                         ? pairs[i].view
                         : pairs[i].native_fallback))
                return 35;
            glDeleteTextures(1, &source);
            glDeleteTextures(1, &view);
        }
    }
    extension_text = "GL_EXT_texture_view";
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        GLuint source, view;
        glGenTextures(1, &source);
        glGenTextures(1, &view);
        glBindTexture(GL_TEXTURE_2D, source);
        glTexStorage2D(GL_TEXTURE_2D, 1, invalid[i].original, 4, 4);
        if (glGetError() != GL_NO_ERROR)
            return 36;
        glTextureView(view, GL_TEXTURE_2D, source, invalid[i].view, 0, 1, 0, 1);
        if (glGetError() != GL_INVALID_OPERATION || glIsTexture(view))
            return 37;
        glDeleteTextures(1, &source);
        glDeleteTextures(1, &view);
    }
    {
        GLuint source, view, nested;
        GLint levels = -1;
        glGenTextures(1, &source);
        glGenTextures(1, &view);
        glGenTextures(1, &nested);
        glBindTexture(GL_TEXTURE_2D, source);
        glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 8, 8);
        if (glGetError() != GL_NO_ERROR)
            return 38;
        glTextureView(view, GL_TEXTURE_2D, source, GL_SRGB8_ALPHA8, 1, 100, 0, 1);
        if (glGetError() != GL_NO_ERROR || viewed_numlevels != 2u)
            return 39;
        glBindTexture(GL_TEXTURE_2D, view);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_VIEW_NUM_LEVELS, &levels);
        if (glGetError() != GL_NO_ERROR || levels != 2)
            return 40;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_LEVELS, &levels);
        if (glGetError() != GL_NO_ERROR || levels != 3)
            return 41;
        glTextureView(nested, GL_TEXTURE_2D, view, GL_RGBA8, 1, 100, 0, 1);
        if (glGetError() != GL_NO_ERROR || viewed_numlevels != 1u)
            return 42;
        glBindTexture(GL_TEXTURE_2D, nested);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_VIEW_MIN_LEVEL, &levels);
        if (glGetError() != GL_NO_ERROR || levels != 2)
            return 43;
        glDeleteTextures(1, &source);
        glDeleteTextures(1, &view);
        glDeleteTextures(1, &nested);
    }
    extension_text = "GL_EXT_copy_image GL_NV_texture_barrier GL_EXT_texture_view";
    native->glesCompressedTexSubImage2D = NULL;
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetString = extensions;
    native.glesGenTextures = gen;
    native.glesBindTexture = bind;
    native.glesTexImage2D = upload;
    native.glesCopyImageSubData = copy_image;
    native.glesTextureBarrier = barrier;
    native.glesTexStorage2D = storage;
    native.glesTexStorage3D = storage3d;
    native.glesTexImage3D = upload3d;
    native.glesTexSubImage3D = subimage3d;
    native.glesTextureView = view_create;
    native.glesTexSubImage2D = subimage;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (!ctx.share_group)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);
    if (!glXGetProcAddress((const GLubyte *)"glCopyImageSubData") ||
        !glXGetProcAddress((const GLubyte *)"glTextureBarrier") ||
        !glXGetProcAddress((const GLubyte *)"glTextureBarrierNV"))
        return 2;
    image_copy_barrier_trace();
    texture_view_trace();
    if (view_calls != 2u || viewed_name == 0u || source_name == 0u || viewed_name == source_name ||
        viewed_level != 0u)
        return 3;
    {
        GLuint source, target;
        glGenTextures(1, &source);
        glGenTextures(1, &target);
        glBindTexture(GL_TEXTURE_2D, source);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2);
        if (glGetError() != GL_NO_ERROR)
            return 4;
        native.glesTextureView = NULL;
        glTextureView(target, GL_TEXTURE_2D, source, GL_RGBA8, 0, 1, 0, 1);
        if (glGetError() != GL_INVALID_OPERATION || glIsTexture(target))
            return 5;
        native.glesTextureView = view_create;
        native.glesGetError = get_native_error;
        native.glesDeleteTextures = delete_native;
        injected_error = GL_OUT_OF_MEMORY;
        glTextureView(target, GL_TEXTURE_2D, source, GL_RGBA8, 0, 1, 0, 1);
        if (glGetError() != GL_OUT_OF_MEMORY || glIsTexture(target) || deleted_views != 1u)
            return 6;
        glTextureView(target, GL_TEXTURE_2D, source, GL_RGBA8, 0, 1, 0, 1);
        if (glGetError() != GL_NO_ERROR || !glIsTexture(target))
            return 7;
        glDeleteTextures(1, &source);
        glDeleteTextures(1, &target);
    }
    texture_layer_view_trace();
    texture_cross_target_view_trace();
    texture_format_view_trace();
    if (srgb_view_calls != 1u || srgb_alpha_view_calls != 1u)
        return 15;
    if (array_view_calls != 2u || cube_view_calls != 2u || cross_target_calls != 2u)
        return 8;
    {
        GLuint array, invalid;
        glGenTextures(1, &array);
        glGenTextures(1, &invalid);
        glBindTexture(GL_TEXTURE_2D_ARRAY, array);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 2, 2, 3);
        if (glGetError() != GL_NO_ERROR)
            return 9;
        glTextureView(invalid, GL_TEXTURE_2D_ARRAY, array, GL_RGBA8, 0, 1, 2, 2);
        if (glGetError() != GL_NO_ERROR || !glIsTexture(invalid))
            return 10;
        glDeleteTextures(1, &invalid);
        glDeleteTextures(1, &array);
    }
    {
        GLuint array, cube, square, invalid;
        unsigned native_calls = view_calls;
        glGenTextures(1, &array);
        glGenTextures(1, &cube);
        glGenTextures(1, &square);
        glGenTextures(1, &invalid);
        glBindTexture(GL_TEXTURE_2D_ARRAY, array);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 2, 3, 6);
        if (glGetError() != GL_NO_ERROR)
            return 11;
        glTextureView(cube, GL_TEXTURE_CUBE_MAP, array, GL_RGBA8, 0, 1, 0, 6);
        if (glGetError() != GL_INVALID_OPERATION || glIsTexture(cube) || view_calls != native_calls)
            return 12;
        glBindTexture(GL_TEXTURE_2D, square);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2);
        if (glGetError() != GL_NO_ERROR)
            return 13;
        glTextureView(invalid, GL_TEXTURE_2D_ARRAY, square, GL_RGBA8, 0, 1, 0, 1);
        if (glGetError() != GL_NO_ERROR || !glIsTexture(invalid) || view_calls != native_calls + 1u)
            return 14;
        glDeleteTextures(1, &array);
        glDeleteTextures(1, &cube);
        glDeleteTextures(1, &square);
        glDeleteTextures(1, &invalid);
    }
    {
        GLuint array, cube;
        GLint layer = -1;
        glGenTextures(1, &array);
        glGenTextures(1, &cube);
        glBindTexture(GL_TEXTURE_2D_ARRAY, array);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 2, 2, 8);
        if (glGetError() != GL_NO_ERROR)
            return 16;
        glTextureView(cube, GL_TEXTURE_CUBE_MAP, array, GL_RGBA8, 0, 1, 1, 6);
        if (glGetError() != GL_NO_ERROR || !glIsTexture(cube))
            return 17;
        glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
        glGetTexParameteriv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_VIEW_MIN_LAYER, &layer);
        if (glGetError() != GL_NO_ERROR || layer != 1)
            return 18;
        glDeleteTextures(1, &array);
        glDeleteTextures(1, &cube);
    }
    {
        int result = texture_view_class_cases(&native);
        if (result != 0)
            return result;
    }
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

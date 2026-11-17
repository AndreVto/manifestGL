#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    unsigned bind_image_calls;
    GLuint unit;
    GLuint texture;
    GLint level;
    GLboolean layered;
    GLint layer;
    GLenum access;
    GLenum format;
} cap;

static const GLubyte *fake_get_string(GLenum pname) {
    return pname == GL_EXTENSIONS ? (const GLubyte *)"GL_OES_texture_float_linear"
                                  : (const GLubyte *)"";
}

static void fake_get_integerv(GLenum pname, GLint *value) {
    if (value == NULL)
        return;
    *value = pname == GL_MAX_IMAGE_UNITS ? 4 : 0;
}

static void fake_gen_textures(GLsizei n, GLuint *textures) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        textures[i] = ++cap.next_texture;
}

static void fake_delete_textures(GLsizei n, const GLuint *textures) {
    (void)n;
    (void)textures;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    (void)target;
    (void)texture;
}

static void fake_tex_storage_2d(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                                GLsizei height) {
    (void)target;
    (void)levels;
    (void)internalformat;
    (void)width;
    (void)height;
}

static void fake_bind_image_texture(GLuint unit, GLuint texture, GLint level, GLboolean layered,
                                    GLint layer, GLenum access, GLenum format) {
    ++cap.bind_image_calls;
    cap.unit = unit;
    cap.texture = texture;
    cap.level = level;
    cap.layered = layered;
    cap.layer = layer;
    cap.access = access;
    cap.format = format;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "image binding failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj;
    GLuint texture = 0u, mutable_texture = 0u, float_texture = 0u, native_float_texture = 0u;
    GLint value = -1;
    GLint64 value64 = -1;
    GLboolean boolean_value = GL_TRUE;
    unsigned calls_before;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGetIntegerv = fake_get_integerv;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexStorage2D = fake_tex_storage_2d;
    native.glesBindImageTexture = fake_bind_image_texture;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 64, 64);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_init_image_units(&ctx);
    mgl_context_set_current_for_test(&ctx);

    glGetIntegerv(GL_MAX_IMAGE_UNITS, &value);
    if (glGetError() != GL_NO_ERROR || value != 4)
        return fail(2, "max image units query");

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    if (glGetError() != GL_NO_ERROR)
        return fail(3, "immutable texture setup");
    obj = mgl_texture_find(&ctx, texture);
    if (obj == NULL || !obj->realized || !obj->immutable_format || obj->native_name == 0u)
        return fail(4, "texture realization");

    glBindImageTexture(1u, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    if (glGetError() != GL_NO_ERROR || cap.bind_image_calls != 1u || cap.unit != 1u ||
        cap.texture != obj->native_name || cap.level != 0 || cap.layered != GL_FALSE ||
        cap.layer != 0 || cap.access != GL_READ_WRITE || cap.format != GL_RGBA8)
        return fail(5, "native image binding lowering");

    glGetIntegeri_v(GL_IMAGE_BINDING_NAME, 1u, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)texture)
        return fail(6, "binding name query");
    glGetIntegeri_v(GL_IMAGE_BINDING_LEVEL, 1u, &value);
    if (glGetError() != GL_NO_ERROR || value != 0)
        return fail(7, "binding level query");
    glGetIntegeri_v(GL_IMAGE_BINDING_LAYERED, 1u, &value);
    if (glGetError() != GL_NO_ERROR || value != GL_FALSE)
        return fail(8, "binding layered query");
    glGetIntegeri_v(GL_IMAGE_BINDING_LAYER, 1u, &value);
    if (glGetError() != GL_NO_ERROR || value != 0)
        return fail(9, "binding layer query");
    glGetIntegeri_v(GL_IMAGE_BINDING_ACCESS, 1u, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)GL_READ_WRITE)
        return fail(10, "binding access query");
    glGetIntegeri_v(GL_IMAGE_BINDING_FORMAT, 1u, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)GL_RGBA8)
        return fail(11, "binding format query");
    glGetInteger64i_v(GL_IMAGE_BINDING_NAME, 1u, &value64);
    if (glGetError() != GL_NO_ERROR || value64 != (GLint64)texture)
        return fail(12, "64-bit binding query");
    glGetBooleani_v(GL_IMAGE_BINDING_LAYERED, 1u, &boolean_value);
    if (glGetError() != GL_NO_ERROR || boolean_value != GL_FALSE)
        return fail(13, "boolean binding query");

    calls_before = cap.bind_image_calls;
    glBindImageTexture(4u, texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    if (glGetError() != GL_INVALID_VALUE || cap.bind_image_calls != calls_before)
        return fail(14, "unit validation");
    glBindImageTexture(0u, texture, -1, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    if (glGetError() != GL_INVALID_VALUE || cap.bind_image_calls != calls_before)
        return fail(15, "level validation");
    glBindImageTexture(0u, texture, 0, GL_FALSE, 0, 0xDEADu, GL_RGBA8);
    if (glGetError() != GL_INVALID_ENUM || cap.bind_image_calls != calls_before)
        return fail(16, "access validation");
    glBindImageTexture(0u, texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGB8);
    if (glGetError() != GL_INVALID_VALUE || cap.bind_image_calls != calls_before)
        return fail(17, "format validation");
    glBindImageTexture(0u, texture + 999u, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    if (glGetError() != GL_INVALID_VALUE || cap.bind_image_calls != calls_before)
        return fail(18, "name validation");

    glGenTextures(1, &mutable_texture);
    glBindTexture(GL_TEXTURE_2D, mutable_texture);
    if (glGetError() != GL_NO_ERROR)
        return fail(19, "mutable texture setup");
    glBindImageTexture(0u, mutable_texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    if (glGetError() != GL_INVALID_OPERATION || cap.bind_image_calls != calls_before)
        return fail(20, "GLES immutable-storage limitation");

    glDeleteTextures(1, &texture);
    glGetIntegeri_v(GL_IMAGE_BINDING_NAME, 1u, &value);
    if (glGetError() != GL_NO_ERROR || value != 0)
        return fail(21, "delete detaches image binding");
    glGetIntegeri_v(GL_IMAGE_BINDING_ACCESS, 1u, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)GL_READ_ONLY)
        return fail(22, "delete resets access");
    glGetIntegeri_v(GL_IMAGE_BINDING_FORMAT, 1u, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)GL_R8)
        return fail(23, "delete resets format");

    glBindImageTexture(2u, 0u, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    if (glGetError() != GL_NO_ERROR || cap.bind_image_calls != calls_before + 1u ||
        cap.texture != 0u)
        return fail(24, "explicit image unbind");

    if (glXGetProcAddress((const GLubyte *)"glBindImageTexture") == NULL)
        return fail(25, "proc address coverage");

    glGenTextures(1, &float_texture);
    glBindTexture(GL_TEXTURE_2D, float_texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 4, 4);
    if (glGetError() != GL_NO_ERROR)
        return fail(27, "filterable floating texture storage");
    glBindImageTexture(0u, float_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glGetIntegeri_v(GL_IMAGE_BINDING_FORMAT, 0u, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)GL_RGBA32F || cap.format != GL_RGBA16F)
        return fail(28, "floating image native format follows filtering-first storage");
    glBindImageTextures(0u, 1, &float_texture);
    if (glGetError() != GL_NO_ERROR || cap.format != GL_RGBA16F ||
        ctx.image_units[0].format != GL_RGBA32F)
        return fail(29, "multi-bind floating image native format");
    native.glesGetString = fake_get_string;
    glGenTextures(1, &native_float_texture);
    glBindTexture(GL_TEXTURE_2D, native_float_texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 4, 4);
    if (glGetError() != GL_NO_ERROR)
        return fail(30, "native floating texture storage");
    glBindImageTextures(0u, 1, &native_float_texture);
    if (glGetError() != GL_NO_ERROR || cap.format != GL_RGBA32F ||
        ctx.image_units[0].format != GL_RGBA32F)
        return fail(31, "extension-backed 32-bit floating image binding");
    glDeleteTextures(1, &native_float_texture);
    glDeleteTextures(1, &float_texture);
    glDeleteTextures(1, &mutable_texture);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL image binding unit test: PASS");
    return 0;
}

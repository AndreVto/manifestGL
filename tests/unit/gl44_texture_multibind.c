#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static struct {
    GLuint next_native;
    GLuint active_unit;
    GLuint native_2d[4];
    GLuint native_images[4];
    GLenum image_format[4];
    GLboolean image_layered[4];
    GLenum image_access[4];
    unsigned image_calls;
} capture;

static void get_integer(GLenum pname, GLint *value) {
    *value = pname == GL_MAX_IMAGE_UNITS ? 4 : 0;
}

static void gen_textures(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++capture.next_native;
}

static void bind_texture(GLenum target, GLuint name) {
    if (target == GL_TEXTURE_2D && capture.active_unit < 4u)
        capture.native_2d[capture.active_unit] = name;
}

static void active_texture(GLenum unit) {
    capture.active_unit = unit - GL_TEXTURE0;
}

static void tex_storage(GLenum target, GLsizei levels, GLenum format, GLsizei width,
                        GLsizei height) {
    (void)target;
    (void)levels;
    (void)format;
    (void)width;
    (void)height;
}

static void tex_image3d(GLenum target, GLint level, GLint internalformat, GLsizei width,
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
}

static void bind_image(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer,
                       GLenum access, GLenum format) {
    (void)level;
    (void)layer;
    ++capture.image_calls;
    if (unit >= 4u)
        return;
    capture.native_images[unit] = texture;
    capture.image_format[unit] = format;
    capture.image_layered[unit] = layered;
    capture.image_access[unit] = access;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "GL 4.4 multibind failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint textures[2] = {0u, 0u};
    GLuint array_texture = 0u;
    GLuint selection[3];
    GLint value = -1;
    unsigned calls;
    int i;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&capture, 0, sizeof(capture));
    native.glesGetIntegerv = get_integer;
    native.glesGenTextures = gen_textures;
    native.glesBindTexture = bind_texture;
    native.glesActiveTexture = active_texture;
    native.glesTexStorage2D = tex_storage;
    native.glesTexImage3D = tex_image3d;
    native.glesBindImageTexture = bind_image;
    ctx.native_dispatch = &native;
    ctx.programmable_texture_units = 4u;
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

    glCreateTextures(GL_TEXTURE_2D, 2, textures);
    if (glGetError() != GL_NO_ERROR || textures[0] == 0u || textures[1] == 0u)
        return fail(2, "texture creation");
    for (i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    }
    if (glGetError() != GL_NO_ERROR)
        return fail(3, "immutable storage");

    glActiveTexture(GL_TEXTURE0 + 1u);
    glBindTextureUnit(1u, textures[0]);
    selection[0] = textures[0];
    selection[1] = 0x00abcdefu;
    selection[2] = textures[1];
    glBindTextures(0u, 3, selection);
    if (glGetError() != GL_INVALID_OPERATION || ctx.texture_2d_binding[0] != textures[0] ||
        ctx.texture_2d_binding[1] != textures[0] || ctx.texture_2d_binding[2] != textures[1] ||
        ctx.compat.active_texture != 1u || capture.active_unit != 1u)
        return fail(4, "per-entry error and selector preservation");
    if (capture.native_2d[0] == 0u || capture.native_2d[2] == 0u)
        return fail(5, "GLES texture binding forwarding");

    glBindTextures(1u, 2, NULL);
    if (glGetError() != GL_NO_ERROR || ctx.texture_2d_binding[1] != 0u ||
        ctx.texture_2d_binding[2] != 0u || capture.native_2d[1] != 0u ||
        capture.native_2d[2] != 0u || ctx.texture_2d_binding[0] != textures[0])
        return fail(6, "null texture array unbind");
    glBindTextures(3u, 2, selection);
    if (glGetError() != GL_INVALID_OPERATION || ctx.texture_2d_binding[3] != 0u)
        return fail(7, "texture range validation");
    glBindTextures(0u, -1, selection);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(8, "negative texture count");
    glBindTextures(4u, 0, NULL);
    if (glGetError() != GL_NO_ERROR)
        return fail(9, "empty texture range");

    selection[0] = textures[0];
    selection[1] = textures[1];
    glBindImageTextures(0u, 2, selection);
    if (glGetError() != GL_NO_ERROR || ctx.image_units[0].texture != textures[0] ||
        ctx.image_units[1].texture != textures[1] || ctx.image_units[0].format != GL_RGBA8 ||
        ctx.image_units[0].layered || ctx.image_units[0].access != GL_READ_WRITE ||
        capture.native_images[0] == 0u || capture.native_images[1] == 0u ||
        capture.image_format[0] != GL_RGBA8 || capture.image_layered[0] ||
        capture.image_access[1] != GL_READ_WRITE)
        return fail(10, "image bind and native forwarding");

    selection[0] = 0x00abcdefu;
    selection[1] = textures[0];
    calls = capture.image_calls;
    glBindImageTextures(1u, 2, selection);
    if (glGetError() != GL_INVALID_OPERATION || capture.image_calls != calls + 1u ||
        ctx.image_units[1].texture != textures[1] || ctx.image_units[2].texture != textures[0])
        return fail(11, "per-entry image errors");
    glBindImageTextures(2u, 2, NULL);
    glGetIntegeri_v(GL_IMAGE_BINDING_FORMAT, 2u, &value);
    if (glGetError() != GL_NO_ERROR || ctx.image_units[2].texture != 0u ||
        ctx.image_units[2].access != GL_READ_ONLY || value != (GLint)GL_R8 ||
        capture.native_images[2] != 0u || capture.image_format[2] != GL_RGBA8)
        return fail(12, "image reset defaults and native compatibility");
    glBindImageTextures(3u, 2, selection);
    if (glGetError() != GL_INVALID_OPERATION || ctx.image_units[3].texture != 0u)
        return fail(13, "image range validation");
    glBindImageTextures(0u, -1, selection);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(14, "negative image count");
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &array_texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, array_texture);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 4, 4, 2);
    if (glGetError() != GL_NO_ERROR)
        return fail(15, "array texture setup");
    glBindImageTextures(3u, 1, &array_texture);
    if (glGetError() != GL_NO_ERROR || ctx.image_units[3].texture != array_texture ||
        !ctx.image_units[3].layered || !capture.image_layered[3])
        return fail(16, "layered image binding");
    if (glXGetProcAddress((const GLubyte *)"glBindTextures") == NULL ||
        glXGetProcAddress((const GLubyte *)"glBindImageTextures") == NULL)
        return fail(17, "proc address registration");

    glDeleteTextures(2, textures);
    glDeleteTextures(1, &array_texture);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL OpenGL 4.4 texture/image multibind: PASS");
    return 0;
}

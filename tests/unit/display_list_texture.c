#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    GLuint bound_texture;
    unsigned upload_calls;
    unsigned char upload[16];
    size_t upload_size;
} cap;

static void fake_active_texture(GLenum texture) {
    (void)texture;
}

static void fake_gen_textures(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_texture;
}

static void fake_delete_textures(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_texture(GLenum target, GLuint name) {
    (void)target;
    cap.bound_texture = name;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
}

static void fake_tex_image(GLenum target, GLint level, GLint internalformat, GLsizei width,
                           GLsizei height, GLint border, GLenum format, GLenum type,
                           const void *pixels) {
    size_t components = format == GL_RGB ? 3u : 4u;
    size_t size = (size_t)width * (size_t)height * components;
    (void)target;
    (void)level;
    (void)internalformat;
    (void)border;
    (void)type;
    cap.upload_size = size < sizeof(cap.upload) ? size : sizeof(cap.upload);
    if (cap.upload_size != 0u && pixels != NULL)
        memcpy(cap.upload, pixels, cap.upload_size);
    ++cap.upload_calls;
}

static void fake_tex_sub_image(GLenum target, GLint level, GLint x, GLint y, GLsizei width,
                               GLsizei height, GLenum format, GLenum type, const void *pixels) {
    (void)x;
    (void)y;
    fake_tex_image(target, level, (GLint)format, width, height, 0, format, type, pixels);
}

static int fail(int n, const char *why) {
    fprintf(stderr, "display-list texture failure %d: %s\n", n, why);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj;
    GLuint texture;
    GLuint list;
    unsigned char source[7] = {1u, 2u, 3u, 99u, 4u, 5u, 6u};
    const unsigned char expected[6] = {1u, 2u, 3u, 4u, 5u, 6u};

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesPixelStorei = fake_pixel_store;
    native.glesTexImage2D = fake_tex_image;
    native.glesTexSubImage2D = fake_tex_sub_image;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    ctx.unpack_alignment = 4;
    ctx.pack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glGenTextures(1, &texture);
    list = glGenLists(1);
    if (texture == 0u || list == 0u)
        return fail(2, "names");

    glNewList(list, GL_COMPILE);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGB8, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, source);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(3, "compile error");
    if (ctx.texture_2d_binding[0] != 0u || cap.upload_calls != 0u)
        return fail(4, "GL_COMPILE executed texture commands");
    obj = mgl_texture_find(&ctx, texture);
    if (obj == NULL || obj->levels[0].defined)
        return fail(5, "GL_COMPILE changed texture storage");

    memset(source, 0xee, sizeof(source));
    glCallList(list);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(6, "replay error");
    obj = mgl_texture_find(&ctx, texture);
    if (obj == NULL || !obj->levels[0].defined || obj->levels[0].data_size != sizeof(expected) ||
        memcmp(obj->levels[0].data, expected, sizeof(expected)) != 0)
        return fail(7, "list did not own compile-time texture payload");
    if (cap.upload_calls != 1u || cap.upload_size != sizeof(expected) ||
        memcmp(cap.upload, expected, sizeof(expected)) != 0)
        return fail(8, "native replay payload");

    memset(obj->levels[0].data, 0x55, obj->levels[0].data_size);
    glCallList(list);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(obj->levels[0].data, expected, sizeof(expected)) != 0)
        return fail(9, "repeated replay did not retain owned payload");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL display-list texture payload unit test: PASS");
    return 0;
}

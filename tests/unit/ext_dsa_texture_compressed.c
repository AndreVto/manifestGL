#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

static GLuint next_native_texture = 1900u;
static GLenum native_active_texture = GL_TEXTURE0;
static GLuint last_bound_2d;
static GLuint last_bound_array;
static GLuint last_bound_cube;

static GLenum fake_get_error(void) {
    return GL_NO_ERROR;
}

static const GLubyte *fake_get_string(GLenum name) {
    (void)name;
    return (const GLubyte *)"";
}

static void fake_gen_textures(GLsizei n, GLuint *textures) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        textures[i] = next_native_texture++;
}

static void fake_delete_textures(GLsizei n, const GLuint *textures) {
    (void)n;
    (void)textures;
}

static void fake_active_texture(GLenum texture) {
    native_active_texture = texture;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    if (target == GL_TEXTURE_2D)
        last_bound_2d = texture;
    else if (target == GL_TEXTURE_2D_ARRAY)
        last_bound_array = texture;
    else if (target == GL_TEXTURE_CUBE_MAP)
        last_bound_cube = texture;
}

static void fake_tex_image_2d(GLenum target, GLint level, GLint internalformat, GLsizei width,
                              GLsizei height, GLint border, GLenum format, GLenum type,
                              const void *pixels) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)border;
    (void)format;
    (void)type;
    (void)pixels;
}

static void fake_tex_image_3d(GLenum target, GLint level, GLint internalformat, GLsizei width,
                              GLsizei height, GLsizei depth, GLint border, GLenum format,
                              GLenum type, const void *pixels) {
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

static void fake_bind_buffer(GLenum target, GLuint buffer) {
    (void)target;
    (void)buffer;
}

static void fake_pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "EXT DSA named compressed texture failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint names[7] = {0u, 0u, 0u, 0u, 0u, 0u, 0u};
    struct mgl_texture *obj;
    struct mgl_texture *saved_obj;
    struct mgl_texture *default_obj;
    unsigned char block[8] = {10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u};
    unsigned char sub_block[8] = {20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u};
    unsigned char array_data[16] = {30u, 31u, 32u, 33u, 34u, 35u, 36u, 37u,
                                    40u, 41u, 42u, 43u, 44u, 45u, 46u, 47u};
    unsigned char array_sub[8] = {50u, 51u, 52u, 53u, 54u, 55u, 56u, 57u};
    unsigned char expected[16];
    GLuint list;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetError = fake_get_error;
    native.glesGetString = fake_get_string;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexImage2D = fake_tex_image_2d;
    native.glesTexImage3D = fake_tex_image_3d;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesPixelStorei = fake_pixel_store;

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

    glGenTextures(7, names);
    if (names[0] == 0u || names[6] == 0u)
        return fail(2, "texture generation");
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, names[0]);
    saved_obj = mgl_texture_find(&ctx, names[0]);
    if (saved_obj == NULL)
        return fail(3, "saved binding");

    list = glGenLists(1);
    if (list == 0u)
        return fail(4, "display list allocation");
    glNewList(list, GL_COMPILE);
    glCompressedTextureImage2DEXT(names[1], GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8,
                                  block);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || glIsTexture(names[1]) != GL_FALSE)
        return fail(5, "compressed image GL_COMPILE realized object");
    memset(block, 99, sizeof(block));
    glCallList(list);
    obj = mgl_texture_find(&ctx, names[1]);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL || !obj->levels[0].defined ||
        !obj->levels[0].compressed || obj->levels[0].data_size != 8u ||
        memcmp(obj->levels[0].data, (const unsigned char[]){10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u},
               8u) != 0 ||
        ctx.texture_2d_binding[4] != names[0] || native_active_texture != GL_TEXTURE4 ||
        last_bound_2d != saved_obj->native_name)
        return fail(6, "compressed image replay/payload/binding");

    glCompressedTextureSubImage2DEXT(names[1], GL_TEXTURE_2D, 0, 0, 0, 4, 4,
                                     GL_COMPRESSED_RED_RGTC1, 8, sub_block);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(obj->levels[0].data, sub_block, 8u) != 0 || ctx.texture_2d_binding[4] != names[0])
        return fail(7, "direct compressed 2D subimage");

    glCompressedTextureImage2DEXT(0u, GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8,
                                  sub_block);
    default_obj = mgl_default_texture(&ctx, GL_TEXTURE_2D);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || default_obj == NULL ||
        !default_obj->levels[0].defined ||
        memcmp(default_obj->levels[0].data, sub_block, 8u) != 0 ||
        ctx.texture_2d_binding[4] != names[0])
        return fail(8, "compressed default texture zero");

    glBindTexture(GL_TEXTURE_2D_ARRAY, names[2]);
    glCompressedTextureImage2DEXT(names[2], GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8,
                                  sub_block);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(9, "compressed target mismatch");

    glBindTexture(GL_TEXTURE_CUBE_MAP, names[3]);
    glBindTexture(GL_TEXTURE_CUBE_MAP, names[4]);
    saved_obj = mgl_texture_find(&ctx, names[4]);
    glCompressedTextureImage2DEXT(names[3], GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0,
                                  GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8, sub_block);
    obj = mgl_texture_find(&ctx, names[3]);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL ||
        !obj->cube_levels[0][0].defined ||
        memcmp(obj->cube_levels[0][0].data, sub_block, 8u) != 0 ||
        ctx.texture_cube_binding[4] != names[4] || saved_obj == NULL ||
        last_bound_cube != saved_obj->native_name)
        return fail(10, "compressed cube face/binding restoration");

    memcpy(expected, array_data, sizeof(expected));
    glCompressedTextureImage3DEXT(names[5], GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 4, 4,
                                  2, 0, 16, array_data);
    obj = mgl_texture_find(&ctx, names[5]);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL ||
        obj->target != GL_TEXTURE_2D_ARRAY || !obj->levels[0].defined ||
        obj->levels[0].data_size != 16u || memcmp(obj->levels[0].data, expected, 16u) != 0)
        return fail(11, "compressed named 2D-array image");

    glBindTexture(GL_TEXTURE_2D_ARRAY, names[6]);
    saved_obj = mgl_texture_find(&ctx, names[6]);
    glDeleteLists(list, 1);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glCompressedTextureSubImage3DEXT(names[5], GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, 4, 4, 1,
                                     GL_COMPRESSED_RED_RGTC1, 8, array_sub);
    glEndList();
    memset(array_sub, 88, sizeof(array_sub));
    glCallList(list);
    memcpy(expected + 8, (const unsigned char[]){50u, 51u, 52u, 53u, 54u, 55u, 56u, 57u}, 8u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(obj->levels[0].data, expected, 16u) != 0 ||
        ctx.texture_2d_array_binding[4] != names[6] || saved_obj == NULL ||
        last_bound_array != saved_obj->native_name)
        return fail(12, "compressed 2D-array subimage list payload/binding");

    glCompressedTextureImage3DEXT(names[1], GL_TEXTURE_3D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 1, 0,
                                  8, sub_block);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(13, "ordinary 3D RGTC must remain rejected");

    if (glXGetProcAddress((const GLubyte *)"glCompressedTextureImage2DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCompressedTextureSubImage2DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCompressedTextureImage3DEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glCompressedTextureSubImage3DEXT") == NULL)
        return fail(14, "proc-address exposure");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL EXT DSA named compressed texture unit test: PASS");
    return 0;
}

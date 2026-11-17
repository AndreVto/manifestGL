#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    GLuint next_framebuffer;
    GLenum last_bind_target;
    GLuint last_bound_texture;
    GLenum last_image_target;
    GLenum last_sub_target;
    GLenum last_mipmap_target;
    unsigned image_calls;
    unsigned sub_calls;
    unsigned mipmap_calls;
    GLenum fbo_textarget;
    GLuint fbo_texture;
    GLint fbo_level;
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
    cap.last_bind_target = target;
    cap.last_bound_texture = name;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
}

static void fake_tex_image_2d(GLenum target, GLint level, GLint internalformat, GLsizei width,
                              GLsizei height, GLint border, GLenum format, GLenum type,
                              const void *pixels) {
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)border;
    (void)format;
    (void)type;
    (void)pixels;
    cap.last_image_target = target;
    ++cap.image_calls;
}

static void fake_tex_sub_image_2d(GLenum target, GLint level, GLint x, GLint y, GLsizei width,
                                  GLsizei height, GLenum format, GLenum type, const void *pixels) {
    (void)level;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)format;
    (void)type;
    (void)pixels;
    cap.last_sub_target = target;
    ++cap.sub_calls;
}

static void fake_generate_mipmap(GLenum target) {
    cap.last_mipmap_target = target;
    ++cap.mipmap_calls;
}

static void fake_tex_parameteri(GLenum target, GLenum pname, GLint value) {
    (void)target;
    (void)pname;
    (void)value;
}

static void fake_gen_framebuffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_framebuffer;
}

static void fake_delete_framebuffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_framebuffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_framebuffer_texture_2d(GLenum target, GLenum attachment, GLenum textarget,
                                        GLuint texture, GLint level) {
    (void)target;
    (void)attachment;
    cap.fbo_textarget = textarget;
    cap.fbo_texture = texture;
    cap.fbo_level = level;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "cube texture failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *cube_obj;
    struct mgl_texture *list_obj;
    GLuint tex2d = 0u, cube = 0u, other_cube = 0u, list_cube = 0u, fbo = 0u, list = 0u;
    GLint value = 0;
    unsigned face;
    unsigned char faces[6][16];
    unsigned char readback[16];
    unsigned char patch[4] = {91u, 92u, 93u, 94u};
    unsigned char list_pixels[16];
    unsigned char list_expected[16];

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesPixelStorei = fake_pixel_store;
    native.glesTexImage2D = fake_tex_image_2d;
    native.glesTexSubImage2D = fake_tex_sub_image_2d;
    native.glesGenerateMipmap = fake_generate_mipmap;
    native.glesTexParameteri = fake_tex_parameteri;
    native.glesGenFramebuffers = fake_gen_framebuffers;
    native.glesDeleteFramebuffers = fake_delete_framebuffers;
    native.glesBindFramebuffer = fake_bind_framebuffer;
    native.glesFramebufferTexture2D = fake_framebuffer_texture_2d;

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
    ctx.next_framebuffer_name = 1u;
    mgl_context_set_current_for_test(&ctx);

    glGenTextures(1, &tex2d);
    glGenTextures(1, &cube);
    glBindTexture(GL_TEXTURE_2D, tex2d);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
    if ((GLuint)value != tex2d)
        return fail(2, "2D binding retained");
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &value);
    if ((GLuint)value != cube)
        return fail(3, "cube binding query");

    for (face = 0u; face < 6u; ++face) {
        size_t i;
        GLenum target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
        for (i = 0u; i < sizeof(faces[face]); ++i)
            faces[face][i] = (unsigned char)(face * 20u + i);
        glTexImage2D(target, 0, (GLint)GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, faces[face]);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.last_image_target != target)
            return fail(4, "per-face native upload target");
    }
    cube_obj = mgl_texture_find(&ctx, cube);
    if (cube_obj == NULL || cube_obj->target != GL_TEXTURE_CUBE_MAP)
        return fail(5, "cube object identity");
    for (face = 0u; face < 6u; ++face) {
        if (!cube_obj->cube_levels[face][0].defined ||
            cube_obj->cube_levels[face][0].data_size != sizeof(faces[face]) ||
            memcmp(cube_obj->cube_levels[face][0].data, faces[face], sizeof(faces[face])) != 0)
            return fail(6, "independent canonical face storage");
    }

    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, 1, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    patch);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        cap.last_sub_target != GL_TEXTURE_CUBE_MAP_NEGATIVE_Y)
        return fail(7, "cube face subimage native target");
    if (memcmp(cube_obj->cube_levels[3][0].data + 4u, patch, sizeof(patch)) != 0 ||
        memcmp(cube_obj->cube_levels[2][0].data, faces[2], sizeof(faces[2])) != 0)
        return fail(8, "cube face subimage isolation");

    memset(readback, 0, sizeof(readback));
    glGetTexImage(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, GL_UNSIGNED_BYTE, readback);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(readback, cube_obj->cube_levels[3][0].data, sizeof(readback)) != 0)
        return fail(9, "cube face readback");
    glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_TEXTURE_WIDTH, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 2)
        return fail(10, "cube face level query");

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.mipmap_calls != 1u ||
        cap.last_mipmap_target != GL_TEXTURE_CUBE_MAP)
        return fail(11, "cube mipmap native target");
    for (face = 0u; face < 6u; ++face) {
        if (!cube_obj->cube_levels[face][1].defined || cube_obj->cube_levels[face][1].width != 1 ||
            cube_obj->cube_levels[face][1].height != 1 || cube_obj->cube_levels[face][1].depth != 1)
            return fail(12, "cube face generated mip");
    }

    glGenTextures(1, &other_cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, other_cube);
    glGenerateTextureMipmap(cube);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || (GLuint)value != other_cube ||
        cap.mipmap_calls != 2u)
        return fail(13, "DSA cube mipmap preserves binding");
    glGetTextureLevelParameteriv(cube, 1, GL_TEXTURE_WIDTH, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 1)
        return fail(14, "named cube level query");

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
                           cube, 0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        cap.fbo_textarget != GL_TEXTURE_CUBE_MAP_NEGATIVE_Z ||
        cap.fbo_texture != cube_obj->native_name || cap.fbo_level != 0)
        return fail(15, "cube face FBO native attachment");
    glGetNamedFramebufferAttachmentParameteriv(
        fbo, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        value != (GLint)GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
        return fail(16, "canonical cube face attachment query");

    glGenTextures(1, &list_cube);
    list = glGenLists(1);
    if (list_cube == 0u || list == 0u)
        return fail(17, "list names");
    for (face = 0u; face < sizeof(list_pixels); ++face)
        list_pixels[face] = (unsigned char)(150u + face);
    memcpy(list_expected, list_pixels, sizeof(list_expected));
    glNewList(list, GL_COMPILE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, list_cube);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, (GLint)GL_RGBA8, 2, 2, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, list_pixels);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(18, "cube list compile");
    list_obj = mgl_texture_find(&ctx, list_cube);
    if (list_obj == NULL || list_obj->cube_levels[0][0].defined)
        return fail(19, "compile-only cube upload executed");
    memset(list_pixels, 0xee, sizeof(list_pixels));
    glCallList(list);
    list_obj = mgl_texture_find(&ctx, list_cube);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || list_obj == NULL ||
        !list_obj->cube_levels[0][0].defined ||
        memcmp(list_obj->cube_levels[0][0].data, list_expected, sizeof(list_expected)) != 0)
        return fail(20, "cube display-list payload ownership");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL cube texture unit test: PASS");
    return 0;
}

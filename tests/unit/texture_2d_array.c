#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_texture;
    GLuint next_buffer;
    GLuint next_framebuffer;
    GLenum active_texture;
    GLuint bound_3d[MGL_TEXTURE_UNITS];
    GLuint bound_array[MGL_TEXTURE_UNITS];
    GLuint bound_cube_array[MGL_TEXTURE_UNITS];
    unsigned tex_image_3d_calls;
    unsigned tex_sub_image_3d_calls;
    unsigned generate_mipmap_calls;
    GLenum last_image_target;
    GLenum last_sub_target;
    GLenum last_mipmap_target;
    GLuint layer_texture;
    GLint layer_level;
    GLint layer_index;
    GLenum copy_source_target;
    GLenum copy_dest_target;
    GLint copy_source_layer;
    GLint copy_dest_layer;
    GLenum view_target;
    GLuint view_first_layer;
    GLuint view_layer_count;
} cap;

static unsigned active_unit(void) {
    if (cap.active_texture < GL_TEXTURE0 || cap.active_texture >= GL_TEXTURE0 + MGL_TEXTURE_UNITS)
        return 0u;
    return (unsigned)(cap.active_texture - GL_TEXTURE0);
}

static void fake_active_texture(GLenum texture) {
    cap.active_texture = texture;
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
    unsigned unit = active_unit();
    if (target == GL_TEXTURE_3D)
        cap.bound_3d[unit] = name;
    if (target == GL_TEXTURE_2D_ARRAY)
        cap.bound_array[unit] = name;
    if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
        cap.bound_cube_array[unit] = name;
}

static void fake_pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
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
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)depth;
    (void)border;
    (void)format;
    (void)type;
    (void)pixels;
    cap.last_image_target = target;
    ++cap.tex_image_3d_calls;
}

static void fake_tex_sub_image_3d(GLenum target, GLint level, GLint x, GLint y, GLint z,
                                  GLsizei width, GLsizei height, GLsizei depth, GLenum format,
                                  GLenum type, const void *pixels) {
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
    cap.last_sub_target = target;
    ++cap.tex_sub_image_3d_calls;
}

static void fake_tex_storage_3d(GLenum target, GLsizei levels, GLenum format,
                                GLsizei width, GLsizei height, GLsizei depth) {
    (void)target;
    (void)levels;
    (void)format;
    (void)width;
    (void)height;
    (void)depth;
}

static void fake_texture_view(GLuint texture, GLenum target, GLuint origtexture,
                              GLenum internalformat, GLuint minlevel, GLuint numlevels,
                              GLuint minlayer, GLuint numlayers) {
    (void)texture;
    (void)origtexture;
    (void)internalformat;
    (void)minlevel;
    (void)numlevels;
    cap.view_target = target;
    cap.view_first_layer = minlayer;
    cap.view_layer_count = numlayers;
}

static void fake_generate_mipmap(GLenum target) {
    cap.last_mipmap_target = target;
    ++cap.generate_mipmap_calls;
}

static void fake_tex_parameteri(GLenum target, GLenum pname, GLint value) {
    (void)target;
    (void)pname;
    (void)value;
}

static void fake_gen_buffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_buffer;
}

static void fake_delete_buffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)target;
    (void)size;
    (void)data;
    (void)usage;
}

static void fake_buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size,
                                 const void *data) {
    (void)target;
    (void)offset;
    (void)size;
    (void)data;
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

static GLenum fake_check_framebuffer(GLenum target) {
    (void)target;
    return GL_FRAMEBUFFER_COMPLETE;
}

static void fake_framebuffer_layer(GLenum target, GLenum attachment, GLuint texture, GLint level,
                                   GLint layer) {
    (void)target;
    (void)attachment;
    cap.layer_texture = texture;
    cap.layer_level = level;
    cap.layer_index = layer;
}

static const GLubyte *fake_get_string(GLenum name) {
    return name == GL_VERSION ? (const GLubyte *)"OpenGL ES 3.2"
           : name == GL_EXTENSIONS ? (const GLubyte *)"GL_EXT_texture_view"
                                  : (const GLubyte *)"";
}

static void fake_copy_image_sub_data(GLuint src, GLenum src_target, GLint src_level,
                                     GLint src_x, GLint src_y, GLint src_z,
                                     GLuint dst, GLenum dst_target, GLint dst_level,
                                     GLint dst_x, GLint dst_y, GLint dst_z,
                                     GLsizei width, GLsizei height, GLsizei depth) {
    (void)src;
    (void)src_level;
    (void)src_x;
    (void)src_y;
    (void)dst;
    (void)dst_level;
    (void)dst_x;
    (void)dst_y;
    (void)width;
    (void)height;
    (void)depth;
    cap.copy_source_target = src_target;
    cap.copy_dest_target = dst_target;
    cap.copy_source_layer = src_z;
    cap.copy_dest_layer = dst_z;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "2D array texture failure %d: %s\n", n, message);
    return n;
}

static int layer_is_constant(const struct mgl_texture_level *level, unsigned layer,
                             unsigned char value) {
    size_t layer_size;
    size_t i;
    const unsigned char *data;
    if (level == NULL || !level->defined || level->data == NULL || layer >= (unsigned)level->depth)
        return 0;
    layer_size = (size_t)level->width * (size_t)level->height * 4u;
    data = level->data + layer_size * layer;
    for (i = 0u; i < layer_size; ++i) {
        if (data[i] != value)
            return 0;
    }
    return 1;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *obj;
    struct mgl_texture *named_obj;
    GLuint tex3d = 0u, array = 0u, array_unit1 = 0u, named = 0u, list_tex = 0u;
    GLuint cube_array = 0u, cube_named = 0u, cube_view = 0u, cube_compressed = 0u;
    GLuint pbo = 0u, fbo = 0u, list = 0u;
    GLint value = 0, prior_array_binding = 0;
    unsigned i;
    const unsigned char padded[15] = {1u, 2u, 3u, 99u, 4u,  5u,  6u, 98u,
                                      7u, 8u, 9u, 97u, 10u, 11u, 12u};
    const unsigned char tight[12] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u};
    const unsigned char patch[3] = {20u, 21u, 22u};
    unsigned char readback[15];
    unsigned char pbo_data[16];
    unsigned char mip_pixels[4u * 2u * 3u * 4u];
    unsigned char named_pixels[2u * 2u * 2u * 4u];
    unsigned char list_pixels[2u * 1u * 2u * 4u];
    unsigned char list_patch[4] = {201u, 202u, 203u, 204u};
    const unsigned char layer_values[3] = {17u, 83u, 149u};
    unsigned char cube_pixels[2u * 2u * 12u * 4u];
    unsigned char cube_readback[2u * 2u * 12u * 4u];
    unsigned char cube_compressed_readback[6u * 8u];

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.active_texture = GL_TEXTURE0;

    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesPixelStorei = fake_pixel_store;
    native.glesTexImage2D = fake_tex_image_2d;
    native.glesTexImage3D = fake_tex_image_3d;
    native.glesTexStorage3D = fake_tex_storage_3d;
    native.glesTextureView = fake_texture_view;
    native.glesTexSubImage3D = fake_tex_sub_image_3d;
    native.glesGenerateMipmap = fake_generate_mipmap;
    native.glesTexParameteri = fake_tex_parameteri;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesBufferSubData = fake_buffer_sub_data;
    native.glesGenFramebuffers = fake_gen_framebuffers;
    native.glesDeleteFramebuffers = fake_delete_framebuffers;
    native.glesBindFramebuffer = fake_bind_framebuffer;
    native.glesCheckFramebufferStatus = fake_check_framebuffer;
    native.glesFramebufferTextureLayer = fake_framebuffer_layer;
    native.glesGetString = fake_get_string;
    native.glesCopyImageSubData = fake_copy_image_sub_data;

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

    glGenTextures(1, &tex3d);
    glGenTextures(1, &array);
    glGenTextures(1, &array_unit1);
    glBindTexture(GL_TEXTURE_3D, tex3d);
    glBindTexture(GL_TEXTURE_2D_ARRAY, array);
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &value);
    if ((GLuint)value != tex3d)
        return fail(2, "independent 3D binding");
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &value);
    if ((GLuint)value != array)
        return fail(3, "array binding query");
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, array_unit1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &value);
    if ((GLuint)value != array_unit1)
        return fail(4, "per-unit array binding");
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &value);
    if ((GLuint)value != array)
        return fail(5, "unit-zero array binding retained");

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, (GLint)GL_RGB8, 1, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 padded);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.last_image_target != GL_TEXTURE_2D_ARRAY)
        return fail(6, "array native upload target");
    obj = mgl_bound_texture(&ctx, GL_TEXTURE_2D_ARRAY);
    if (obj == NULL || obj->target != GL_TEXTURE_2D_ARRAY || obj->levels[0].depth != 2 ||
        obj->levels[0].data_size != sizeof(tight) ||
        memcmp(obj->levels[0].data, tight, sizeof(tight)) != 0)
        return fail(7, "canonical array volume");
    glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_DEPTH, &value);
    if (value != 2)
        return fail(8, "array layer-count query");
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, (GLint)GL_CLAMP_TO_EDGE);
    glGetTexParameteriv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, &value);
    if (value != (GLint)GL_CLAMP_TO_EDGE)
        return fail(9, "array wrap R ownership");

    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, patch);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.last_sub_target != GL_TEXTURE_2D_ARRAY ||
        memcmp(obj->levels[0].data + 6u, patch, sizeof(patch)) != 0)
        return fail(10, "array layer subimage");
    memset(readback, 0xee, sizeof(readback));
    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGB, GL_UNSIGNED_BYTE, readback);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(readback, (const unsigned char[]){1u, 2u, 3u}, 3u) != 0 || readback[3] != 0xeeu ||
        memcmp(readback + 4, (const unsigned char[]){4u, 5u, 6u}, 3u) != 0 ||
        readback[7] != 0xeeu || memcmp(readback + 8, patch, 3u) != 0 || readback[11] != 0xeeu ||
        memcmp(readback + 12, (const unsigned char[]){10u, 11u, 12u}, 3u) != 0)
        return fail(11, "array pack-aligned readback");

    memcpy(pbo_data + 1, padded, sizeof(padded));
    pbo_data[0] = 42u;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, (GLsizeiptr)sizeof(pbo_data), pbo_data, GL_STATIC_DRAW);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 1, (GLint)GL_RGB8, 1, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 (const void *)(uintptr_t)1u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj->levels[1].data_size != sizeof(tight) ||
        memcmp(obj->levels[1].data, tight, sizeof(tight)) != 0)
        return fail(12, "array PBO logical offset");
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0u);

    for (i = 0u; i < 3u; ++i)
        memset(mip_pixels + i * (4u * 2u * 4u), layer_values[i], 4u * 2u * 4u);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, (GLint)GL_RGBA8, 4, 2, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 mip_pixels);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        cap.last_mipmap_target != GL_TEXTURE_2D_ARRAY || !obj->levels[1].defined ||
        obj->levels[1].width != 2 || obj->levels[1].height != 1 || obj->levels[1].depth != 3 ||
        !obj->levels[2].defined || obj->levels[2].width != 1 || obj->levels[2].height != 1 ||
        obj->levels[2].depth != 3)
        return fail(13, "array mip dimensions preserve all layers");
    for (i = 0u; i < 3u; ++i) {
        if (!layer_is_constant(&obj->levels[1], i, layer_values[i]) ||
            !layer_is_constant(&obj->levels[2], i, layer_values[i]))
            return fail(14, "array mip generation mixed or dropped layers");
    }

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &named);
    named_obj = mgl_texture_find(&ctx, named);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || named_obj == NULL ||
        named_obj->target != GL_TEXTURE_2D_ARRAY)
        return fail(15, "DSA array creation");
    memset(named_pixels, 7, sizeof(named_pixels));
    glBindTexture(GL_TEXTURE_2D_ARRAY, named);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, (GLint)GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 named_pixels);
    glBindTexture(GL_TEXTURE_2D_ARRAY, array);
    glTextureSubImage3D(named, 0, 1, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                        (const unsigned char[]){31u, 32u, 33u, 34u});
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || (GLuint)value != array ||
        cap.last_sub_target != GL_TEXTURE_2D_ARRAY ||
        memcmp(named_obj->levels[0].data + ((1u * 2u * 2u + 1u) * 4u),
               (const unsigned char[]){31u, 32u, 33u, 34u}, 4u) != 0)
        return fail(16, "DSA array subimage preserves binding");
    glGenerateTextureMipmap(named);
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || (GLuint)value != array ||
        !named_obj->levels[1].defined || named_obj->levels[1].depth != 2)
        return fail(17, "DSA array mipmap preserves layers and binding");
    glBindTextureUnit(2u, named);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || ctx.texture_2d_array_binding[2] != named ||
        cap.bound_array[2] != named_obj->native_name)
        return fail(18, "DSA texture-unit array binding");

    glBindTexture(GL_TEXTURE_2D_ARRAY, array);
    glPushAttrib(GL_TEXTURE_BIT);
    glBindTexture(GL_TEXTURE_2D_ARRAY, named);
    glPopAttrib();
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || (GLuint)value != array ||
        cap.bound_array[0] != obj->native_name)
        return fail(19, "PushAttrib/PopAttrib restores array binding");

    glBindTexture(GL_TEXTURE_2D_ARRAY, array);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, array, 0, 2);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.layer_texture != obj->native_name ||
        cap.layer_level != 0 || cap.layer_index != 2)
        return fail(20, "array layered native attachment");
    glGetNamedFramebufferAttachmentParameteriv(fbo, GL_COLOR_ATTACHMENT0,
                                               GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 2)
        return fail(21, "canonical array layer query");

    glGenTextures(1, &list_tex);
    list = glGenLists(1);
    for (i = 0u; i < sizeof(list_pixels); ++i)
        list_pixels[i] = (unsigned char)(40u + i);
    glNewList(list, GL_COMPILE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, list_tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, (GLint)GL_RGBA8, 2, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 list_pixels);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 1, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    list_patch);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(22, "array list compilation");
    memset(list_pixels, 0, sizeof(list_pixels));
    memset(list_patch, 0, sizeof(list_patch));
    glCallList(list);
    obj = mgl_texture_find(&ctx, list_tex);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL || !obj->levels[0].defined ||
        obj->levels[0].depth != 2 || obj->levels[0].data[0] != 40u ||
        obj->levels[0].data[8] != 48u ||
        memcmp(obj->levels[0].data + 12u, (const unsigned char[]){201u, 202u, 203u, 204u}, 4u) != 0)
        return fail(23, "array list owns image and subimage payloads");

    glBindTexture(GL_TEXTURE_2D_ARRAY, named);
    glDeleteTextures(1, &named);
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 0 ||
        ctx.texture_2d_array_binding[2] != 0u || cap.bound_array[0] != 0u)
        return fail(24, "array deletion clears bindings");

    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &prior_array_binding);
    glCreateTextures(GL_TEXTURE_CUBE_MAP_ARRAY, 1, &cube_array);
    obj = mgl_texture_find(&ctx, cube_array);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || obj == NULL ||
        obj->target != GL_TEXTURE_CUBE_MAP_ARRAY)
        return fail(25, "cube-map-array object creation");
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, cube_array);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || (GLuint)value != cube_array ||
        ctx.texture_2d_array_binding[0] != (GLuint)prior_array_binding)
        return fail(26, "cube-map-array binding is independent");

    glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RGBA8, 2, 1, 12, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(27, "cube-map-array must be square");
    glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RGBA8, 2, 2, 7, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(28, "cube-map-array layer count must be multiple of six");
    for (i = 0u; i < 12u; ++i)
        memset(cube_pixels + i * 16u, (unsigned char)(i + 1u), 16u);
    glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RGBA8, 2, 2, 12, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, cube_pixels);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        cap.last_image_target != GL_TEXTURE_CUBE_MAP_ARRAY ||
        obj->levels[0].depth != 12 || !layer_is_constant(&obj->levels[0], 7, 8u))
        return fail(29, "cube-map-array image and layer addressing");
    glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_TEXTURE_DEPTH, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 12)
        return fail(30, "cube-map-array texture-level depth");
    glTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 7, 1, 1, 1, GL_RGBA,
                    GL_UNSIGNED_BYTE, (const unsigned char[]){91u, 92u, 93u, 94u});
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        cap.last_sub_target != GL_TEXTURE_CUBE_MAP_ARRAY ||
        memcmp(obj->levels[0].data + 7u * 16u,
               (const unsigned char[]){91u, 92u, 93u, 94u}, 4u) != 0)
        return fail(31, "cube-map-array per-face upload");
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    memset(cube_readback, 0, sizeof(cube_readback));
    glGetTexImage(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, cube_readback);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(cube_readback, obj->levels[0].data, sizeof(cube_readback)) != 0)
        return fail(32, "cube-map-array full image readback");
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP_ARRAY);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        cap.last_mipmap_target != GL_TEXTURE_CUBE_MAP_ARRAY ||
        !obj->levels[1].defined || obj->levels[1].depth != 12 ||
        obj->levels[1].width != 1 || obj->levels[1].height != 1 ||
        !layer_is_constant(&obj->levels[1], 4, 5u))
        return fail(33, "cube-map-array mipmaps retain every face");
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_array, 0, 7);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        cap.layer_texture != obj->native_name || cap.layer_index != 7)
        return fail(34, "cube-map-array framebuffer-layer attachment");
    glGetNamedFramebufferAttachmentParameteriv(fbo, GL_COLOR_ATTACHMENT0,
                                               GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 7)
        return fail(35, "cube-map-array attachment layer query");
    glBindTextureUnit(3, cube_array);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        ctx.texture_cube_array_binding[3] != cube_array ||
        cap.bound_cube_array[3] != obj->native_name)
        return fail(36, "cube-map-array native unit realization");
    glPushAttrib(GL_TEXTURE_BIT);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
    glPopAttrib();
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || (GLuint)value != cube_array)
        return fail(37, "cube-map-array attrib-stack binding restoration");

    glCreateTextures(GL_TEXTURE_CUBE_MAP_ARRAY, 1, &cube_named);
    glTextureStorage3D(cube_named, 2, GL_RGBA8, 2, 2, 12);
    named_obj = mgl_texture_find(&ctx, cube_named);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || named_obj == NULL ||
        !named_obj->immutable_format || named_obj->levels[0].depth != 12 ||
        named_obj->levels[1].depth != 12)
        return fail(38, "cube-map-array immutable named storage");
    glTextureSubImage3D(cube_named, 0, 0, 0, 11, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                        (const unsigned char[]){71u, 72u, 73u, 74u});
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || (GLuint)value != cube_array ||
        memcmp(named_obj->levels[0].data + 11u * 16u,
               (const unsigned char[]){71u, 72u, 73u, 74u}, 4u) != 0)
        return fail(39, "cube-map-array DSA upload preserves bound texture");
    glGenTextures(1, &cube_view);
    glTextureView(cube_view, GL_TEXTURE_2D_ARRAY, cube_named, GL_RGBA8, 0, 2, 6, 6);
    {
        struct mgl_texture *view_obj = mgl_texture_find(&ctx, cube_view);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || view_obj == NULL ||
            view_obj->view_root != named_obj || view_obj->levels[0].depth != 6 ||
            view_obj->levels[0].data != named_obj->levels[0].data + 6u * 16u ||
            cap.view_target != GL_TEXTURE_2D_ARRAY || cap.view_first_layer != 6u ||
            cap.view_layer_count != 6u)
            return fail(43, "cube-map-array to 2D-array texture view layer mapping");
    }
    glTextureSubImage3D(cube_view, 0, 0, 0, 4, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                        (const unsigned char[]){61u, 62u, 63u, 64u});
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(named_obj->levels[0].data + 10u * 16u,
               (const unsigned char[]){61u, 62u, 63u, 64u}, 4u) != 0)
        return fail(44, "texture view updates corresponding cube-map-array layer");
    glDeleteTextures(1, &cube_view);
    glCopyImageSubData(cube_named, GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 11,
                       cube_array, GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 5, 1, 1, 1);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        cap.copy_source_target != GL_TEXTURE_CUBE_MAP_ARRAY ||
        cap.copy_dest_target != GL_TEXTURE_CUBE_MAP_ARRAY ||
        cap.copy_source_layer != 11 || cap.copy_dest_layer != 5 ||
        memcmp(obj->levels[0].data + 5u * 16u,
               (const unsigned char[]){71u, 72u, 73u, 74u}, 4u) != 0)
        return fail(41, "cube-map-array image copying preserves face indices");
    memset(cube_readback, 0, sizeof(cube_readback));
    glGetTextureSubImage(cube_array, 0, 0, 0, 5, 1, 1, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                         8, cube_readback);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        memcmp(cube_readback, (const unsigned char[]){71u, 72u, 73u, 74u}, 4u) != 0 ||
        memcmp(cube_readback + 4, (const unsigned char[]){7u, 7u, 7u, 7u}, 4u) != 0)
        return fail(42, "cube-map-array subimage readback across cube faces");
    glDeleteTextures(1, &cube_array);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, &value);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || value != 0 ||
        ctx.texture_cube_array_binding[3] != 0)
        return fail(40, "cube-map-array deletion clears all bindings");
    glCreateTextures(GL_TEXTURE_CUBE_MAP_ARRAY, 1, &cube_compressed);
    glTextureStorage3D(cube_compressed, 1, GL_COMPRESSED_RED_RGTC1, 4, 4, 6);
    {
        struct mgl_texture *compressed_obj = mgl_texture_find(&ctx, cube_compressed);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || compressed_obj == NULL ||
            !compressed_obj->levels[0].defined || !compressed_obj->levels[0].compressed ||
            compressed_obj->levels[0].data_size != sizeof(cube_compressed_readback) ||
            compressed_obj->levels[0].depth != 6)
            return fail(45, "compressed cube-map-array immutable storage");
        glCompressedTextureSubImage3D(
            cube_compressed, 0, 0, 0, 4, 4, 4, 1, GL_COMPRESSED_RED_RGTC1, 8,
            (const unsigned char[]){17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u});
        memset(cube_compressed_readback, 0, sizeof(cube_compressed_readback));
        glGetCompressedTextureImage(cube_compressed, 0,
                                    (GLsizei)sizeof(cube_compressed_readback),
                                    cube_compressed_readback);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(cube_compressed_readback + 4u * 8u,
                   (const unsigned char[]){17u, 18u, 19u, 20u,
                                           21u, 22u, 23u, 24u}, 8u) != 0)
            return fail(46, "compressed cube-map-array face update and readback");
    }
    glDeleteTextures(1, &cube_compressed);
    glDeleteTextures(1, &cube_named);

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL 2D array texture unit test: PASS");
    return 0;
}

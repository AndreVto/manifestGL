#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    GLuint name;
    unsigned char bytes[128];
    size_t size;
    int immutable;
} native_buffer;

static native_buffer buffers[32];
static GLuint next_name;
static GLuint pack_bound, copy_read_bound, copy_write_bound;
static unsigned copies, subdata_calls;
static GLenum attachment_type = GL_UNSIGNED_NORMALIZED;
static GLuint next_tex_name = 200u;

static void gen_textures(GLsizei count, GLuint *names) {
    GLsizei i;
    for (i = 0; i < count; ++i)
        names[i] = ++next_tex_name;
}

static void bind_texture(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void tex_image_2d(GLenum target, GLint level, GLint internalformat, GLsizei width,
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

static native_buffer *buffer_for(GLuint name) {
    unsigned i;
    for (i = 0; i < 32u; ++i)
        if (buffers[i].name == name && name != 0u)
            return &buffers[i];
    return NULL;
}

static GLuint bound(GLenum target) {
    if (target == GL_PIXEL_PACK_BUFFER)
        return pack_bound;
    if (target == GL_COPY_READ_BUFFER)
        return copy_read_bound;
    if (target == GL_COPY_WRITE_BUFFER)
        return copy_write_bound;
    return 0u;
}

static void gen_buffers(GLsizei count, GLuint *names) {
    GLsizei i;
    for (i = 0; i < count; ++i) {
        GLuint name = ++next_name;
        names[i] = name;
        buffers[name - 1u].name = name;
    }
}

static void delete_buffers(GLsizei count, const GLuint *names) {
    (void)count;
    (void)names;
}

static void bind_buffer(GLenum target, GLuint name) {
    if (target == GL_PIXEL_PACK_BUFFER)
        pack_bound = name;
    else if (target == GL_COPY_READ_BUFFER)
        copy_read_bound = name;
    else if (target == GL_COPY_WRITE_BUFFER)
        copy_write_bound = name;
}

static void buffer_storage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags) {
    native_buffer *dst = buffer_for(bound(target));
    (void)flags;
    if (dst == NULL || size < 0 || (size_t)size > sizeof(dst->bytes))
        return;
    dst->immutable = 1;
    dst->size = (size_t)size;
    if (data != NULL)
        memcpy(dst->bytes, data, dst->size);
}

static void buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    native_buffer *dst = buffer_for(bound(target));
    (void)usage;
    if (dst == NULL || size < 0 || (size_t)size > sizeof(dst->bytes))
        return;
    dst->size = (size_t)size;
    if (data != NULL)
        memcpy(dst->bytes, data, dst->size);
}

static void buffer_sub_data(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    native_buffer *dst = buffer_for(bound(target));
    ++subdata_calls;
    if (dst == NULL || dst->immutable || offset < 0 || size < 0 || (size_t)offset > dst->size ||
        (size_t)size > dst->size - (size_t)offset)
        return;
    memcpy(dst->bytes + offset, data, (size_t)size);
}

static void copy_buffer(GLenum read_target, GLenum write_target, GLintptr read_offset,
                        GLintptr write_offset, GLsizeiptr size) {
    native_buffer *src = buffer_for(bound(read_target));
    native_buffer *dst = buffer_for(bound(write_target));
    if (src == NULL || dst == NULL || read_offset < 0 || write_offset < 0 || size < 0 ||
        (size_t)read_offset > src->size || (size_t)size > src->size - (size_t)read_offset ||
        (size_t)write_offset > dst->size || (size_t)size > dst->size - (size_t)write_offset)
        return;
    ++copies;
    memcpy(dst->bytes + write_offset, src->bytes + read_offset, (size_t)size);
}

static const GLubyte *get_string(GLenum name) {
    return name == GL_EXTENSIONS ? (const GLubyte *)"GL_EXT_buffer_storage GL_NV_read_depth_stencil"
                                 : NULL;
}

static void get_integer(GLenum name, GLint *value) {
    if (name == GL_READ_FRAMEBUFFER_BINDING)
        *value = 0;
}

static void get_attachment(GLenum target, GLenum attachment, GLenum pname, GLint *value) {
    (void)target;
    (void)attachment;
    if (pname == GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE)
        *value = (GLint)attachment_type;
    else
        *value = 8;
}

static void bind_framebuffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void pixel_store(GLenum pname, GLint value) {
    (void)pname;
    (void)value;
}

static GLenum get_error(void) {
    return GL_NO_ERROR;
}

static void read_pixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
                        void *out) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    if (format == GL_DEPTH_STENCIL && type == GL_UNSIGNED_INT_24_8) {
        uint32_t value = 0x80000003u;
        memcpy(out, &value, sizeof(value));
    } else if (format == GL_RGBA_INTEGER && type == GL_UNSIGNED_INT) {
        const uint32_t rgba[4] = {1u, 2u, 3u, 4u};
        memcpy(out, rgba, sizeof(rgba));
    } else if (format == GL_RGBA && type == GL_UNSIGNED_BYTE) {
        const unsigned char rgba[4] = {10u, 20u, 30u, 40u};
        memcpy(out, rgba, sizeof(rgba));
    }
}

static int fail(int code, const char *msg) {
    fprintf(stderr, "immutable pack readback %d: %s\n", code, msg);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint id = 0u, pack_compressed = 0u, texture = 0u, cube = 0u;
    unsigned char initial[32], compressed_initial[96];
    unsigned char block[8] = {0xf0u, 0xf0u, 0x0fu, 0x0fu, 0u, 0u, 0u, 0u};
    unsigned char face_blocks[6][8];
    native_buffer *gpu;
    struct mgl_buffer *obj;
    size_t i;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(buffers, 0, sizeof(buffers));
    memset(initial, 0xa5, sizeof(initial));
    native.glesGenTextures = gen_textures;
    native.glesBindTexture = bind_texture;
    native.glesTexImage2D = tex_image_2d;
    native.glesGenBuffers = gen_buffers;
    native.glesDeleteBuffers = delete_buffers;
    native.glesBindBuffer = bind_buffer;
    native.glesBufferStorageEXT = buffer_storage;
    native.glesBufferData = buffer_data;
    native.glesBufferSubData = buffer_sub_data;
    native.glesCopyBufferSubData = copy_buffer;
    native.glesReadPixels = read_pixels;
    native.glesGetString = get_string;
    native.glesGetIntegerv = get_integer;
    native.glesGetFramebufferAttachmentParameteriv = get_attachment;
    native.glesBindFramebuffer = bind_framebuffer;
    native.glesPixelStorei = pixel_store;
    native.glesGetError = get_error;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 16, 16);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.default_read_buffer = GL_BACK;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);
    glGenBuffers(1, &id);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, id);
    glBufferStorage(GL_PIXEL_PACK_BUFFER, (GLsizeiptr)sizeof(initial), initial, 0u);
    obj = mgl_buffer_find(&ctx, id);
    gpu = obj != NULL ? buffer_for(obj->native_name) : NULL;
    if (gpu == NULL || !obj->native_immutable || !obj->immutable ||
        (obj->storage_flags & GL_DYNAMIC_STORAGE_BIT) != 0u ||
        mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(2, "native immutable pack allocation");
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void *)(uintptr_t)4u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || copies != 1u || subdata_calls != 0u ||
        memcmp(gpu->bytes + 4, (const unsigned char[]){10u, 20u, 30u, 40u}, 4u) != 0)
        return fail(3, "normalized pack transfer through server copy");
    attachment_type = GL_UNSIGNED_INT;
    glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, (void *)(uintptr_t)8u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || copies != 2u || subdata_calls != 0u ||
        memcmp(gpu->bytes + 8, (const unsigned char[]){1u, 2u, 3u, 4u}, 4u) != 0)
        return fail(4, "integer pack transfer through server copy");
    glReadnPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, 3, (void *)(uintptr_t)16u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || copies != 2u ||
        gpu->bytes[16] != 0xa5u)
        return fail(5, "bounded pack transfer guards");
    glReadnPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, 4, (void *)(uintptr_t)16u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || copies != 3u ||
        memcmp(gpu->bytes + 16, (const unsigned char[]){1u, 2u, 3u, 4u}, 4u) != 0)
        return fail(6, "bounded pack transfer through server copy");
    glReadPixels(0, 0, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, (void *)(uintptr_t)24u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || copies != 4u ||
        memcmp(gpu->bytes + 24, (const unsigned char[]){3u, 0u, 0u, 128u}, 4u) != 0)
        return fail(7, "packed depth/stencil transfer through server copy");
    for (i = 0u; i < sizeof(initial); ++i)
        if ((i < 4u || (i >= 12u && i < 16u) || (i >= 20u && i < 24u) || i >= 28u) &&
            gpu->bytes[i] != 0xa5u)
            return fail(8, "unmodified pack bytes preserved");
    if (pack_bound != obj->native_name || copy_read_bound != 0u || copy_write_bound != 0u ||
        memcmp(gpu->bytes, obj->shadow, obj->size) != 0)
        return fail(9, "pack bindings and canonical shadow remain consistent");
    glDeleteBuffers(1, &id);
    memset(compressed_initial, 0xa5, sizeof(compressed_initial));
    glGenBuffers(1, &pack_compressed);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pack_compressed);
    glBufferStorage(GL_PIXEL_PACK_BUFFER, (GLsizeiptr)sizeof(compressed_initial),
                    compressed_initial, 0u);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
                           (GLsizei)sizeof(block), block);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(10, "compressed texture setup");
    glGetCompressedTexImage(GL_TEXTURE_2D, 0, (void *)(uintptr_t)0u);
    glGetCompressedTextureImage(texture, 0, 8, (void *)(uintptr_t)8u);
    glGetCompressedTextureSubImage(texture, 0, 0, 0, 0, 4, 4, 1, 8, (void *)(uintptr_t)16u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(11, "compressed immutable PBO readback");
    obj = mgl_buffer_find(&ctx, pack_compressed);
    gpu = obj != NULL ? buffer_for(obj->native_name) : NULL;
    if (gpu == NULL || copies != 7u || subdata_calls != 0u || memcmp(gpu->bytes, block, 8u) != 0 ||
        memcmp(gpu->bytes + 8u, block, 8u) != 0 || memcmp(gpu->bytes + 16u, block, 8u) != 0 ||
        memcmp(gpu->bytes, obj->shadow, obj->size) != 0)
        return fail(12, "compressed immutable PBO native bytes and shadows");
    glGenTextures(1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    for (i = 0u; i < 6u; ++i) {
        memcpy(face_blocks[i], block, sizeof(block));
        face_blocks[i][0] = (unsigned char)(0xf0u + i);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)i, 0,
                               GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0, 8, face_blocks[i]);
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(13, "compressed cube setup");
    glGetCompressedTextureImage(cube, 0, 48, (void *)(uintptr_t)32u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || copies != 8u || subdata_calls != 0u)
        return fail(14, "compressed cube immutable PBO transfer");
    for (i = 0u; i < 6u; ++i)
        if (memcmp(gpu->bytes + 32u + i * 8u, face_blocks[i], 8u) != 0)
            return fail(15, "compressed cube native PBO contents");
    if (memcmp(gpu->bytes, obj->shadow, obj->size) != 0 || gpu->bytes[24] != 0xa5u ||
        gpu->bytes[80] != 0xa5u || pack_bound != obj->native_name || copy_read_bound != 0u ||
        copy_write_bound != 0u)
        return fail(16, "compressed PBO preservation and bindings");
    glDeleteBuffers(1, &pack_compressed);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

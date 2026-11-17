#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

static struct {
    GLuint next_texture;
    GLuint next_buffer;
    GLuint bound_unpack;
    unsigned sub_calls;
    unsigned sub3d_calls;
    GLenum last_target;
    GLenum last_format;
    GLenum last_type;
    GLint last_x;
    GLint last_y;
    GLint last_z;
    GLint image_height;
    GLint skip_images;
    GLint image_height_during_upload;
    GLint images_during_upload;
    unsigned copy_calls;
    unsigned reads;
    const void *last_pointer;
    GLint row_length;
    GLint skip_rows;
    GLint skip_pixels;
    GLint row_during_upload;
    GLint rows_during_upload;
    GLint pixels_during_upload;
    GLuint pbo_during_upload;
} cap;

static GLenum fake_error(void) {
    return GL_NO_ERROR;
}

static const GLubyte *fake_string(GLenum t) {
    return t == GL_VERSION ? (const GLubyte *)"OpenGL ES 3.2" : (const GLubyte *)"ANGLE Mock";
}

static void fake_gen_tex(GLsizei n, GLuint *a) {
    for (GLsizei i = 0; i < n; ++i)
        a[i] = ++cap.next_texture;
}

static void fake_delete_tex(GLsizei n, const GLuint *a) {
    (void)n;
    (void)a;
}

static void fake_bind_tex(GLenum t, GLuint n) {
    (void)t;
    (void)n;
}

static void fake_active(GLenum u) {
    (void)u;
}

static void fake_image(GLenum t, GLint l, GLint f, GLsizei w, GLsizei h, GLint b, GLenum sf,
                       GLenum st, const void *p) {
    (void)t;
    (void)l;
    (void)f;
    (void)w;
    (void)h;
    (void)b;
    (void)sf;
    (void)st;
    (void)p;
}

static void fake_sub(GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty,
                     const void *p) {
    (void)l;
    (void)w;
    (void)h;
    cap.last_target = t;
    cap.last_format = f;
    cap.last_type = ty;
    cap.last_x = x;
    cap.last_y = y;
    ++cap.sub_calls;
    cap.last_pointer = p;
    cap.row_during_upload = cap.row_length;
    cap.rows_during_upload = cap.skip_rows;
    cap.pixels_during_upload = cap.skip_pixels;
    cap.pbo_during_upload = cap.bound_unpack;
}

static void fake_image3d(GLenum t, GLint l, GLint f, GLsizei w, GLsizei h, GLsizei d, GLint b,
                         GLenum sf, GLenum st, const void *p) {
    (void)t;
    (void)l;
    (void)f;
    (void)w;
    (void)h;
    (void)d;
    (void)b;
    (void)sf;
    (void)st;
    (void)p;
}

static void fake_sub3d(GLenum t, GLint l, GLint x, GLint y, GLint z, GLsizei w, GLsizei h,
                       GLsizei d, GLenum f, GLenum ty, const void *p) {
    (void)l;
    (void)w;
    (void)h;
    (void)d;
    cap.last_target = t;
    cap.last_format = f;
    cap.last_type = ty;
    cap.last_x = x;
    cap.last_y = y;
    cap.last_z = z;
    cap.last_pointer = p;
    cap.image_height_during_upload = cap.image_height;
    cap.images_during_upload = cap.skip_images;
    ++cap.sub3d_calls;
}

static void fake_store(GLenum p, GLint v) {
    if (p == GL_UNPACK_IMAGE_HEIGHT)
        cap.image_height = v;
    if (p == GL_UNPACK_SKIP_IMAGES)
        cap.skip_images = v;
    if (p == GL_UNPACK_ROW_LENGTH)
        cap.row_length = v;
    if (p == GL_UNPACK_SKIP_ROWS)
        cap.skip_rows = v;
    if (p == GL_UNPACK_SKIP_PIXELS)
        cap.skip_pixels = v;
}

static void fake_bind_buffer(GLenum t, GLuint b) {
    if (t == GL_PIXEL_UNPACK_BUFFER)
        cap.bound_unpack = b;
}

static void fake_gen_buffer(GLsizei n, GLuint *a) {
    for (GLsizei i = 0; i < n; ++i)
        a[i] = ++cap.next_buffer;
}

static void fake_delete_buffer(GLsizei n, const GLuint *a) {
    (void)n;
    (void)a;
}

static void fake_buffer_data(GLenum t, GLsizeiptr s, const void *p, GLenum u) {
    (void)t;
    (void)s;
    (void)p;
    (void)u;
}

static void fake_gen_fbo(GLsizei n, GLuint *a) {
    for (GLsizei i = 0; i < n; ++i)
        a[i] = ++cap.next_buffer;
}

static void fake_delete_fbo(GLsizei n, const GLuint *a) {
    (void)n;
    (void)a;
}

static void fake_bind_fbo(GLenum t, GLuint b) {
    (void)t;
    (void)b;
}

static void fake_attach(GLenum t, GLenum a, GLenum tt, GLuint b, GLint l) {
    (void)t;
    (void)a;
    (void)tt;
    (void)b;
    (void)l;
}

static GLenum fake_fbo_status(GLenum t) {
    (void)t;
    return GL_FRAMEBUFFER_COMPLETE;
}

static void fake_read(GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, void *p) {
    (void)x;
    (void)y;
    (void)f;
    (void)t;
    memset(p, 0x76, (size_t)w * (size_t)h * 4u);
    ++cap.reads;
}

static void fake_copy(GLuint sn, GLenum st, GLint sl, GLint sx, GLint sy, GLint sz, GLuint dn,
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
    ++cap.copy_calls;
}

static int fail(int n) {
    fprintf(stderr, "angle_texture_passthrough failure %d\n", n);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch b;
    struct mgl_texture *texture;
    struct mgl_texture *dst;
    struct mgl_buffer *pbo;
    GLuint tex[2];
    GLuint buffer;
    unsigned char initial[16] = {0};
    unsigned char full[16] = {1u, 2u, 3u, 4u};
    unsigned char output[16] = {0};
    unsigned char rgb[27] = {0};
    unsigned char red[16] = {0};
    unsigned char half[64] = {0};
    unsigned char voxels[256] = {0};
    GLuint extra[4];
    memset(&ctx, 0, sizeof(ctx));
    memset(&b, 0, sizeof(b));
    memset(&cap, 0, sizeof(cap));
    b.glesGetError = fake_error;
    b.glesGetString = fake_string;
    b.glesGenTextures = fake_gen_tex;
    b.glesDeleteTextures = fake_delete_tex;
    b.glesBindTexture = fake_bind_tex;
    b.glesActiveTexture = fake_active;
    b.glesTexImage2D = fake_image;
    b.glesTexSubImage2D = fake_sub;
    b.glesTexImage3D = fake_image3d;
    b.glesTexSubImage3D = fake_sub3d;
    b.glesPixelStorei = fake_store;
    b.glesBindBuffer = fake_bind_buffer;
    b.glesGenBuffers = fake_gen_buffer;
    b.glesDeleteBuffers = fake_delete_buffer;
    b.glesBufferData = fake_buffer_data;
    b.glesGenFramebuffers = fake_gen_fbo;
    b.glesDeleteFramebuffers = fake_delete_fbo;
    b.glesBindFramebuffer = fake_bind_fbo;
    b.glesFramebufferTexture2D = fake_attach;
    b.glesCheckFramebufferStatus = fake_fbo_status;
    b.glesReadPixels = fake_read;
    b.glesCopyImageSubData = fake_copy;
    ctx.native_dispatch = &b;
    ctx.angle_direct_texture_passthrough = GL_TRUE;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1);
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);
    glGenTextures(2, tex);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, initial);
    texture = mgl_texture_find(&ctx, tex[0]);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || texture == NULL)
        return fail(2);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, full);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.sub_calls != 1u ||
        cap.last_pointer != full || !texture->levels[0].shadow_stale ||
        !texture->used_as_color_attachment || memcmp(texture->levels[0].data, initial, 16u) != 0)
        return fail(3);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, output);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.reads != 1u || output[0] != 0x76u ||
        output[15] != 0x76u)
        return fail(4);
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 256, NULL, GL_STATIC_DRAW);
    pbo = mgl_buffer_find(&ctx, buffer);
    if (pbo == NULL)
        return fail(5);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 4);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 1);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                    (const void *)(uintptr_t)12u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.sub_calls != 2u ||
        cap.last_pointer != (const void *)(uintptr_t)12u ||
        cap.pbo_during_upload != pbo->native_name || cap.row_during_upload != 4 ||
        cap.rows_during_upload != 1 || cap.pixels_during_upload != 1 || cap.row_length != 0 ||
        cap.skip_rows != 0 || cap.skip_pixels != 0)
        return fail(6);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, tex[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, initial);
    dst = mgl_texture_find(&ctx, tex[1]);
    if (dst == NULL || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(7);
    glCopyImageSubData(tex[0], GL_TEXTURE_2D, 0, 0, 0, 0, tex[1], GL_TEXTURE_2D, 0, 0, 0, 0, 2, 2,
                       1);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.copy_calls != 1u ||
        !dst->levels[0].shadow_stale || !dst->used_as_color_attachment ||
        memcmp(dst->levels[0].data, initial, sizeof(initial)) != 0)
        return fail(8);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, full);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.sub_calls != 3u ||
        !texture->levels[0].shadow_stale)
        return fail(9);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, output);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.reads < 2u)
        return fail(10);
    glGenTextures(4, extra);
    glBindTexture(GL_TEXTURE_2D, extra[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 3, 3, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.sub_calls != 4u ||
        cap.last_pointer != rgb || cap.last_x != 1 || cap.last_y != 1 ||
        cap.last_format != GL_RGB || cap.last_type != GL_UNSIGNED_BYTE ||
        !mgl_texture_find(&ctx, extra[0])->levels[0].shadow_stale)
        return fail(11);
    glBindTexture(GL_TEXTURE_2D, extra[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 4, 4, 0, GL_RED, GL_UNSIGNED_BYTE, red);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 2, 2, 1, GL_RED, GL_UNSIGNED_BYTE, red);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.sub_calls != 5u ||
        cap.last_pointer != red || cap.last_x != 1 || cap.last_y != 2 ||
        !mgl_texture_find(&ctx, extra[1])->levels[0].shadow_stale)
        return fail(12);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE, red);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.sub_calls != 6u)
        return fail(13);
    {
        unsigned old_reads = cap.reads;
        glGenerateMipmap(GL_TEXTURE_2D);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.reads <= old_reads ||
            mgl_texture_find(&ctx, extra[1])->levels[0].shadow_stale)
            return fail(18);
    }
    glBindTexture(GL_TEXTURE_2D, extra[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 2, 2, 0, GL_RGBA, GL_HALF_FLOAT, half);
    texture = mgl_texture_find(&ctx, extra[2]);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || texture == NULL)
        return fail(14);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_HALF_FLOAT, half);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.sub_calls != 7u ||
        cap.last_pointer != half || cap.last_type != GL_HALF_FLOAT ||
        texture->levels[0].shadow_stale)
        return fail(15);
    glBindTexture(GL_TEXTURE_CUBE_MAP, extra[3]);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB8, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 rgb);
    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.sub_calls != 8u ||
        cap.last_target != GL_TEXTURE_CUBE_MAP_POSITIVE_X || cap.last_pointer != rgb ||
        mgl_texture_find(&ctx, extra[3])->cube_levels[0][0].shadow_stale)
        return fail(16);
    glGenTextures(1, &buffer);
    glBindTexture(GL_TEXTURE_3D, buffer);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, voxels);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 3);
    glPixelStorei(GL_UNPACK_SKIP_IMAGES, 1);
    voxels[12] = 0xa5u;
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, voxels);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.sub3d_calls != 1u ||
        cap.last_target != GL_TEXTURE_3D || cap.last_pointer != voxels ||
        cap.image_height_during_upload != 3 || cap.images_during_upload != 1 ||
        cap.image_height != 0 || cap.skip_images != 0 ||
        mgl_texture_find(&ctx, buffer)->levels[0].shadow_stale ||
        mgl_texture_find(&ctx, buffer)->levels[0].data[0] != 0xa5u)
        return fail(17);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
    glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

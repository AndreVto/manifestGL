#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

struct cap_state {
    GLuint next_name;
    GLenum last_target;
    GLuint last_texture;
    GLsizei samples;
    GLsizei width, height, depth;
    GLenum internal_format;
    GLboolean fixed;
    unsigned storage2_calls, storage3_calls;
    GLuint deleted_texture;
    unsigned delete_texture_calls;
    GLuint next_framebuffer;
    GLuint bound_framebuffer;
    GLenum attachment_target;
    GLuint attachment_texture;
    GLint attachment_level;
    GLint attachment_layer;
    unsigned attachment_calls;
    unsigned layer_attachment_calls;
} cap;

static void fake_gen_textures(GLsizei n, GLuint *out) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        out[i] = ++cap.next_name;
}

static void fake_delete_textures(GLsizei n, const GLuint *p) {
    if (n > 0 && p != NULL) {
        cap.deleted_texture = p[0];
        ++cap.delete_texture_calls;
    }
}

static void fake_active_texture(GLenum t) {
    (void)t;
}

static void fake_bind_texture(GLenum target, GLuint tex) {
    cap.last_target = target;
    cap.last_texture = tex;
}

static void fake_storage2(GLenum target, GLsizei samples, GLenum internal, GLsizei w, GLsizei h,
                          GLboolean fixed) {
    cap.last_target = target;
    cap.samples = samples;
    cap.internal_format = internal;
    cap.width = w;
    cap.height = h;
    cap.fixed = fixed;
    ++cap.storage2_calls;
}

static void fake_storage3(GLenum target, GLsizei samples, GLenum internal, GLsizei w, GLsizei h,
                          GLsizei d, GLboolean fixed) {
    cap.last_target = target;
    cap.samples = samples;
    cap.internal_format = internal;
    cap.width = w;
    cap.height = h;
    cap.depth = d;
    cap.fixed = fixed;
    ++cap.storage3_calls;
}

static void fake_gen_framebuffers(GLsizei n, GLuint *out) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        out[i] = ++cap.next_framebuffer;
}

static void fake_delete_framebuffers(GLsizei n, const GLuint *p) {
    (void)n;
    (void)p;
}

static void fake_bind_framebuffer(GLenum target, GLuint fb) {
    (void)target;
    cap.bound_framebuffer = fb;
}

static void fake_framebuffer_texture_2d(GLenum target, GLenum attachment, GLenum textarget,
                                        GLuint texture, GLint level) {
    (void)target;
    (void)attachment;
    cap.attachment_target = textarget;
    cap.attachment_texture = texture;
    cap.attachment_level = level;
    ++cap.attachment_calls;
}

static void fake_framebuffer_texture_layer(GLenum target, GLenum attachment, GLuint texture,
                                           GLint level, GLint layer) {
    (void)target;
    (void)attachment;
    cap.attachment_texture = texture;
    cap.attachment_level = level;
    cap.attachment_layer = layer;
    ++cap.layer_attachment_calls;
}

static GLint fake_gles_minor = 2;

static void fake_gles_version(GLenum pname, GLint *value) {
    *value = pname == GL_MAJOR_VERSION ? 3 : fake_gles_minor;
}

static MGL_EGLBoolean fake_egl_make_current(MGL_EGLDisplay display, MGL_EGLSurface draw,
                                            MGL_EGLSurface read, MGL_EGLContext context) {
    (void)display;
    (void)draw;
    (void)read;
    (void)context;
    return MGL_EGL_TRUE;
}

static int fail(int n, const char *m) {
    fprintf(stderr, "multisample texture failure %d: %s\n", n, m);
    return n;
}

int main(void) {
    struct mgl_context ctx, ctx2;
    struct mgl_native_dispatch native;
    GLuint tex[2], fb = 0u, fb2 = 0u;
    GLint v = 0;
    memset(&ctx, 0, sizeof(ctx));
    memset(&ctx2, 0, sizeof(ctx2));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexStorage2DMultisample = fake_storage2;
    native.glesTexStorage3DMultisample = fake_storage3;
    native.glesGenFramebuffers = fake_gen_framebuffers;
    native.glesDeleteFramebuffers = fake_delete_framebuffers;
    native.glesBindFramebuffer = fake_bind_framebuffer;
    native.glesFramebufferTexture2D = fake_framebuffer_texture_2d;
    native.glesFramebufferTextureLayer = fake_framebuffer_texture_layer;
    native.eglMakeCurrent = fake_egl_make_current;
    native.glesGetIntegerv = fake_gles_version;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (!ctx.share_group)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.compat.active_texture = 0u;
    mgl_context_set_current_for_test(&ctx);
    glGenTextures(2, tex);
    if (!tex[0] || !tex[1])
        return fail(2, "texture names");
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex[0]);
    glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE, &v);
    if ((GLuint)v != tex[0])
        return fail(3, "2d ms binding");
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, 64, 32, GL_TRUE);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.storage2_calls != 1u ||
        cap.samples != 4 || cap.width != 64 || cap.height != 32 || !cap.fixed)
        return fail(4, "2d ms allocation");
    glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_SAMPLES, &v);
    if (v != 4)
        return fail(5, "sample query");
    glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_FIXED_SAMPLE_LOCATIONS, &v);
    if (v != 1)
        return fail(6, "fixed query");
    glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_DEPTH, &v);
    if (v != 1)
        return fail(7, "2d depth query");
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, tex[0],
                           0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.attachment_calls != 1u ||
        cap.attachment_target != GL_TEXTURE_2D_MULTISAMPLE || cap.attachment_texture == 0u ||
        cap.attachment_level != 0)
        return fail(8, "2d multisample framebuffer attachment");
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, tex[0],
                           1);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(9, "multisample framebuffer level validation");
    {
        GLuint old_native = cap.attachment_texture;
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 8, GL_RGBA8, 128, 64, GL_FALSE);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.storage2_calls != 2u ||
            cap.samples != 8 || cap.width != 128 || cap.height != 64 || cap.fixed)
            return fail(10, "2d multisample redefinition");
        if (cap.delete_texture_calls != 1u || cap.deleted_texture != old_native ||
            cap.attachment_calls != 2u || cap.attachment_texture == old_native)
            return fail(11, "2d native recreation and attachment refresh");
        glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_WIDTH, &v);
        if (v != 128)
            return fail(12, "2d redefined width");
        glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE, &v);
        if ((GLuint)v != tex[0])
            return fail(13, "2d interface identity preserved");
    }
    /* A second context has context-local FBO state but shares the texture object.  After
     * a texture native recreation in ctx, making ctx2 current must lazily refresh its
     * attachment to the shared texture's new native name. */
    ctx2.native_dispatch = &native;
    ctx2.share_group = ctx.share_group;
    mgl_share_group_retain(ctx2.share_group);
    mgl_error_init(&ctx2.errors);
    mgl_vertex_array_init(&ctx2.default_vao);
    ctx2.current_vao = &ctx2.default_vao;
    ctx2.compat.active_texture = 0u;
    ctx2.egl_display = (MGL_EGLDisplay)1;
    ctx2.egl_draw = (MGL_EGLSurface)1;
    ctx2.egl_read = (MGL_EGLSurface)1;
    ctx2.egl_context = (MGL_EGLContext)1;
    mgl_context_set_current_for_test(&ctx2);
    glGenFramebuffers(1, &fb2);
    glBindFramebuffer(GL_FRAMEBUFFER, fb2);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, tex[0],
                           0);
    if (mgl_error_take(&ctx2.errors) != GL_NO_ERROR || fb2 == 0u)
        return fail(14, "shared-context framebuffer attachment");
    {
        GLuint shared_old_native = cap.attachment_texture;
        unsigned before_refresh;
        mgl_context_set_current_for_test(&ctx);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex[0]);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 6, GL_RGBA8, 96, 48, GL_TRUE);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.attachment_texture == shared_old_native)
            return fail(15, "shared texture native recreation");
        before_refresh = cap.attachment_calls;
        fake_gles_minor = 1;
        if (mgl_context_make_current(&ctx2))
            return fail(30, "GLES 3.1 native was accepted");
        if (mgl_context_current() != &ctx)
            return fail(31, "previous context was not restored");
        fake_gles_minor = 2;
        if (!mgl_context_make_current(&ctx2))
            return fail(16, "shared context make current");
        if (cap.attachment_calls <= before_refresh || cap.attachment_texture == shared_old_native)
            return fail(17, "shared-context attachment refresh on make-current");
        glDeleteFramebuffers(1, &fb2);
        fb2 = 0u;
        mgl_context_set_current_for_test(&ctx);
    }

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, tex[1]);
    glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY, &v);
    if ((GLuint)v != tex[1])
        return fail(18, "array ms binding");
    glTexImage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 2, GL_DEPTH_COMPONENT24, 16, 8, 6,
                            GL_FALSE);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.storage3_calls != 1u || cap.depth != 6 ||
        cap.samples != 2 || cap.fixed)
        return fail(19, "3d ms allocation");
    glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 0, GL_TEXTURE_DEPTH, &v);
    if (v != 6)
        return fail(20, "array depth query");
    glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 0, GL_TEXTURE_INTERNAL_FORMAT, &v);
    if (v != (GLint)GL_DEPTH_COMPONENT24)
        return fail(21, "array format query");
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, tex[1], 0, 2);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.layer_attachment_calls != 1u ||
        cap.attachment_layer != 2)
        return fail(22, "array multisample layer attachment");
    {
        GLuint old_native = cap.attachment_texture;
        glTexImage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 4, GL_DEPTH_COMPONENT24, 32, 16, 4,
                                GL_TRUE);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.storage3_calls != 2u ||
            cap.samples != 4 || cap.depth != 4 || !cap.fixed)
            return fail(23, "array multisample redefinition");
        if (cap.delete_texture_calls != 3u || cap.deleted_texture != old_native ||
            cap.layer_attachment_calls != 2u || cap.attachment_texture == old_native ||
            cap.attachment_layer != 2)
            return fail(24, "array native recreation and attachment refresh");
        glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 0, GL_TEXTURE_DEPTH, &v);
        if (v != 4)
            return fail(25, "array redefined depth");
    }
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_RGBA8, 1, 1, GL_TRUE);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(26, "sample validation");
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx2.share_group);
    mgl_share_group_release(ctx.share_group);
    puts("mGL multisample texture unit test: PASS");
    return 0;
}

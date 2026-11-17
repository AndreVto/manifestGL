#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"
#include "state/state.h"

#include <stdio.h>
#include <string.h>

static GLuint next_native_name;
static unsigned layered_calls;
static unsigned parameter_calls;
static unsigned mask_calls;
static GLuint last_layered_texture;
static GLuint last_bound_fbo;
static GLuint last_mask;
static GLenum queried_target, queried_attachment, queried_pname;
static unsigned attachment_queries;

static void fake_get_attachment(GLenum target, GLenum attachment, GLenum pname, GLint *value) {
    queried_target = target;
    queried_attachment = attachment;
    queried_pname = pname;
    ++attachment_queries;
    switch (pname) {
        case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
            *value = 8;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
            *value = 8;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
            *value = 8;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
            *value = 8;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
            *value = 24;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            *value = (GLint)GL_RENDERBUFFER;
            break;
        default:
            *value = 0;
            break;
    }
}

static void generate(GLsizei n, GLuint *names) {
    for (GLsizei i = 0; i < n; ++i)
        names[i] = ++next_native_name;
}

static void bind_texture(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void bind_fbo(GLenum target, GLuint name) {
    (void)target;
    last_bound_fbo = name;
}

static void attach_2d(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                      GLint level) {
    (void)target;
    (void)attachment;
    (void)textarget;
    (void)texture;
    (void)level;
}

static void attach_layered(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    (void)target;
    (void)attachment;
    (void)level;
    ++layered_calls;
    last_layered_texture = texture;
}

static void set_parameter(GLenum target, GLenum pname, GLint value) {
    (void)target;
    (void)pname;
    (void)value;
    ++parameter_calls;
}

static void set_mask(GLuint index, GLbitfield mask) {
    (void)index;
    ++mask_calls;
    last_mask = mask;
}

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "native test:%d: %s\n", __LINE__, #expr);                              \
            return __LINE__;                                                                       \
        }                                                                                          \
    } while (0)

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_framebuffer *fb;
    struct mgl_texture *tex;
    GLuint fbo, cube;
    GLint value;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenTextures = generate;
    native.glesBindTexture = bind_texture;
    native.glesGenFramebuffers = generate;
    native.glesBindFramebuffer = bind_fbo;
    native.glesFramebufferTexture2D = attach_2d;
    native.glesFramebufferTexture = attach_layered;
    native.glesFramebufferParameteri = set_parameter;
    native.glesSampleMaski = set_mask;
    native.glesGetFramebufferAttachmentParameteriv = fake_get_attachment;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    CHECK(ctx.share_group != NULL);
    ctx.next_framebuffer_name = 1u;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_state_init(&ctx.canonical_state, 32, 32);
    mgl_context_set_current_for_test(&ctx);
    glGenTextures(1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    fb = mgl_bound_framebuffer(&ctx, GL_FRAMEBUFFER);
    tex = mgl_texture_find(&ctx, cube);
    CHECK(fb != NULL && tex != NULL && glGetError() == GL_NO_ERROR);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube, 0);
    CHECK(glGetError() == GL_NO_ERROR && fb->color_texture == cube &&
          fb->color_texture_layer == -2);
    CHECK(layered_calls == 1 && last_layered_texture == tex->native_name);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_LAYERED, &value);
    CHECK(value == 1 && glGetError() == GL_NO_ERROR);
    CHECK(mgl_texture_refresh_framebuffer_attachments(&ctx, fb));
    CHECK(layered_calls == 2 && last_layered_texture == tex->native_name &&
          last_bound_fbo == fb->native_name);
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, 31);
    CHECK(glGetError() == GL_NO_ERROR && parameter_calls == 1 && fb->default_width == 31);
    glSampleMaski(0, 0x128ab34du);
    CHECK(glGetError() == GL_NO_ERROR && mask_calls == 1 && last_mask == 0x128ab34du);
    glGetIntegeri_v(GL_SAMPLE_MASK_VALUE, 0, &value);
    CHECK((GLuint)value == 0x128ab34du && glGetError() == GL_NO_ERROR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0u);
    ctx.native_default_framebuffer =
        72u; /* Android AHardwareBuffer direct presentation-backed drawable. */
    {
        const GLenum pnames[] = {
            GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
            GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE};
        const GLenum depths[] = {GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE};
        unsigned before = attachment_queries;
        for (unsigned i = 0; i < sizeof(pnames) / sizeof(pnames[0]); ++i) {
            value = -1;
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT, pnames[i], &value);
            CHECK(value == 8 && glGetError() == GL_NO_ERROR && queried_target == GL_FRAMEBUFFER &&
                  queried_attachment == GL_COLOR_ATTACHMENT0 && queried_pname == pnames[i]);
        }
        for (unsigned i = 0; i < sizeof(depths) / sizeof(depths[0]); ++i) {
            value = -1;
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH, depths[i], &value);
            CHECK(value == 24 && glGetError() == GL_NO_ERROR &&
                  queried_attachment == GL_DEPTH_ATTACHMENT && queried_pname == depths[i]);
        }
        CHECK(attachment_queries == before + 5u);
    }
    glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_BACK_LEFT,
                                          GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &value);
    CHECK(value == 8 && glGetError() == GL_NO_ERROR && queried_target == GL_READ_FRAMEBUFFER &&
          queried_attachment == GL_COLOR_ATTACHMENT0);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &value);
    CHECK(value == (GLint)GL_FRAMEBUFFER_DEFAULT && glGetError() == GL_NO_ERROR);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &value);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &value);
    CHECK(glGetError() == GL_INVALID_ENUM);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_FRONT_LEFT,
                                          GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &value);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    ctx.native_default_framebuffer = 0u; /* Native GLES default drawable. */
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT,
                                          GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &value);
    CHECK(value == 8 && glGetError() == GL_NO_ERROR && queried_attachment == GL_BACK);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH,
                                          GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &value);
    CHECK(value == 24 && glGetError() == GL_NO_ERROR && queried_attachment == GL_DEPTH);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE, &value);
    CHECK(value == 8 && glGetError() == GL_NO_ERROR && queried_attachment == GL_COLOR_ATTACHMENT0);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

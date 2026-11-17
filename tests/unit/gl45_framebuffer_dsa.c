#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"
#include "state/state.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "framebuffer DSA:%d: %s\n", __LINE__, #expr);                          \
            return __LINE__;                                                                       \
        }                                                                                          \
    } while (0)

static GLuint next_native;
static GLuint native_draw, native_read;
static GLuint last_attached_texture;
static GLenum last_attachment;
static GLint last_parameter_value;
static unsigned parameter_calls, attach_calls, layer_calls;

static void gen(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i)
        names[i] = ++next_native;
}

static void bind_fb(GLenum target, GLuint name) {
    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER)
        native_draw = name;
    if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER)
        native_read = name;
}

static void bind_tex(GLenum target, GLuint texture) {
    (void)target;
    (void)texture;
}

static void attach_2d(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                      GLint level) {
    (void)textarget;
    (void)level;
    if (target == GL_DRAW_FRAMEBUFFER) {
        last_attachment = attachment;
        last_attached_texture = texture;
        ++attach_calls;
    }
}

static void attach_layer(GLenum target, GLenum attachment, GLuint texture, GLint level,
                         GLint layer) {
    (void)level;
    (void)layer;
    if (target == GL_DRAW_FRAMEBUFFER) {
        last_attachment = attachment;
        last_attached_texture = texture;
        ++layer_calls;
    }
}

static void fb_parameter(GLenum target, GLenum pname, GLint value) {
    (void)pname;
    if (target == GL_DRAW_FRAMEBUFFER) {
        last_parameter_value = value;
        ++parameter_calls;
    }
}

static void get_limit(GLenum pname, GLint *value) {
    if (pname == 0x9315u || pname == 0x9316u)
        *value = 4096;
    else if (pname == 0x9317u)
        *value = 256;
    else if (pname == 0x9318u)
        *value = 8;
    else
        *value = 8;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_framebuffer *fbo_a, *fbo_b;
    struct mgl_texture *texture;
    GLuint framebuffers[2], textures[2];
    GLint value;
    GLenum invalidation = GL_COLOR_ATTACHMENT0;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenFramebuffers = gen;
    native.glesGenTextures = gen;
    native.glesBindFramebuffer = bind_fb;
    native.glesBindTexture = bind_tex;
    native.glesFramebufferTexture2D = attach_2d;
    native.glesFramebufferTextureLayer = attach_layer;
    native.glesFramebufferParameteri = fb_parameter;
    native.glesGetIntegerv = get_limit;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    CHECK(ctx.share_group != NULL);
    ctx.next_framebuffer_name = 1u;
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 32, 32);
    mgl_context_set_current_for_test(&ctx);

    glCreateFramebuffers(2, framebuffers);
    glGenTextures(2, textures);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textures[1]);
    fbo_a = mgl_framebuffer_find(&ctx, framebuffers[0]);
    fbo_b = mgl_framebuffer_find(&ctx, framebuffers[1]);
    texture = mgl_texture_find(&ctx, textures[0]);
    CHECK(fbo_a && fbo_b && texture && glGetError() == GL_NO_ERROR);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[1]);
    CHECK(ctx.draw_framebuffer == framebuffers[1] && ctx.read_framebuffer == framebuffers[1]);

    glNamedFramebufferParameteri(framebuffers[0], GL_FRAMEBUFFER_DEFAULT_WIDTH, 640);
    CHECK(glGetError() == GL_NO_ERROR && fbo_a->default_width == 640);
    CHECK(parameter_calls == 1u && last_parameter_value == 640);
    CHECK(native_draw == fbo_b->native_name && native_read == fbo_b->native_name);
    CHECK(ctx.draw_framebuffer == framebuffers[1] && ctx.read_framebuffer == framebuffers[1]);
    value = -1;
    glGetNamedFramebufferParameteriv(framebuffers[0], GL_FRAMEBUFFER_DEFAULT_WIDTH, &value);
    CHECK(value == 640 && glGetError() == GL_NO_ERROR);
    glNamedFramebufferParameteri(framebuffers[0], GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS, 1);
    glGetNamedFramebufferParameteriv(framebuffers[0], GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS,
                                     &value);
    CHECK(value == 1 && glGetError() == GL_NO_ERROR);
    glNamedFramebufferParameteri(framebuffers[0], GL_FRAMEBUFFER_DEFAULT_WIDTH, -1);
    CHECK(glGetError() == GL_INVALID_VALUE && fbo_a->default_width == 640);
    glNamedFramebufferParameteri(framebuffers[0], GL_FRAMEBUFFER_DEFAULT_WIDTH, 4097);
    CHECK(glGetError() == GL_INVALID_VALUE && fbo_a->default_width == 640);
    glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, 4097);
    CHECK(glGetError() == GL_INVALID_VALUE && fbo_b->default_width == 0);
    glNamedFramebufferParameteri(framebuffers[0], 0xDEAD, 1);
    CHECK(glGetError() == GL_INVALID_ENUM);
    glNamedFramebufferParameteri(0, GL_FRAMEBUFFER_DEFAULT_WIDTH, 4);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    glGetNamedFramebufferParameteriv(0, GL_FRAMEBUFFER_DEFAULT_WIDTH, &value);
    CHECK(glGetError() == GL_INVALID_OPERATION);

    glNamedFramebufferTexture(framebuffers[0], GL_COLOR_ATTACHMENT0, textures[0], 0);
    CHECK(glGetError() == GL_NO_ERROR && fbo_a->color_texture == textures[0]);
    CHECK(attach_calls == 1u && last_attached_texture == texture->native_name &&
          last_attachment == GL_COLOR_ATTACHMENT0);
    CHECK(native_draw == fbo_b->native_name && native_read == fbo_b->native_name);
    CHECK(ctx.draw_framebuffer == framebuffers[1] && ctx.read_framebuffer == framebuffers[1]);
    glNamedFramebufferTextureLayer(framebuffers[0], GL_COLOR_ATTACHMENT0, textures[1], 0, 2);
    CHECK(glGetError() == GL_NO_ERROR && layer_calls == 1u);
    CHECK(fbo_a->color_texture == textures[1] && fbo_a->color_texture_layer == 2);
    CHECK(native_draw == fbo_b->native_name && native_read == fbo_b->native_name);
    glNamedFramebufferTextureLayer(framebuffers[0], GL_COLOR_ATTACHMENT0, textures[1], 0, -1);
    CHECK(glGetError() == GL_INVALID_VALUE && fbo_a->color_texture_layer == 2);
    CHECK(native_draw == fbo_b->native_name && native_read == fbo_b->native_name);
    glNamedFramebufferTexture(0, GL_COLOR_ATTACHMENT0, textures[0], 0);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    glNamedFramebufferTexture(framebuffers[0], GL_COLOR_ATTACHMENT0, 0xFFFFFFu, 0);
    CHECK(glGetError() == GL_INVALID_OPERATION && fbo_a->color_texture == textures[1]);
    glNamedFramebufferTexture(framebuffers[0], GL_COLOR_ATTACHMENT0, 0, 0);
    CHECK(glGetError() == GL_NO_ERROR && fbo_a->color_texture == 0);

    glInvalidateSubFramebuffer(GL_DRAW_FRAMEBUFFER, 1, &invalidation, 2, 3, 4, 5);
    CHECK(glGetError() == GL_NO_ERROR);
    invalidation = GL_COLOR_ATTACHMENT0 + 1u;
    glInvalidateSubFramebuffer(GL_DRAW_FRAMEBUFFER, 1, &invalidation, 2, 3, 4, 5);
    CHECK(glGetError() == GL_NO_ERROR);
    invalidation = GL_COLOR_ATTACHMENT0 + 8u;
    glInvalidateSubFramebuffer(GL_DRAW_FRAMEBUFFER, 1, &invalidation, 0, 0, 4, 5);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    glInvalidateSubFramebuffer(GL_DRAW_FRAMEBUFFER, 1, &invalidation, 0, 0, -1, 5);
    CHECK(glGetError() == GL_INVALID_VALUE);
    glInvalidateSubFramebuffer(0xDEAD, 1, &invalidation, 0, 0, 4, 5);
    CHECK(glGetError() == GL_INVALID_ENUM);
    CHECK(ctx.draw_framebuffer == framebuffers[1] && ctx.read_framebuffer == framebuffers[1]);

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

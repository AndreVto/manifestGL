#include "mgl/gl.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "state/state.h"
#include <string.h>

static GLuint next_name;
static GLuint bound_2d;
static GLuint upload_binding;
static unsigned sub2d_count;
static unsigned char uploaded[64];
static size_t uploaded_bytes;

static void gen(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i)
        names[i] = ++next_name;
}

static void bind_texture(GLenum target, GLuint name) {
    if (target == GL_TEXTURE_2D)
        bound_2d = name;
}

static void image_2d(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height,
                     GLint border, GLenum format, GLenum type, const void *data) {
    (void)target;
    (void)level;
    (void)internal;
    (void)width;
    (void)height;
    (void)border;
    (void)format;
    (void)type;
    (void)data;
}

static void sub_image_2d(GLenum target, GLint level, GLint x, GLint y, GLsizei width,
                         GLsizei height, GLenum format, GLenum type, const void *data) {
    (void)target;
    (void)level;
    (void)x;
    (void)y;
    (void)format;
    (void)type;
    upload_binding = bound_2d;
    if (width == 2 && height == 1 && data != NULL) {
        uploaded_bytes = (size_t)width * (size_t)height * (type == GL_FLOAT ? 16u : 4u);
        memcpy(uploaded, data, uploaded_bytes);
    }
    ++sub2d_count;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint a, b, c;
    const unsigned char original[8] = {0};
    const unsigned char clear[4] = {11, 22, 33, 44};
    const unsigned char expected[8] = {11, 22, 33, 44, 11, 22, 33, 44};
    const float float_clear[4] = {0.25f, 0.5f, 1.5f, -0.125f};
    const float float_original[8] = {0};
    float float_expected[8];
    GLint bound;
    int result = 0;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenTextures = gen;
    native.glesBindTexture = bind_texture;
    native.glesTexImage2D = image_2d;
    native.glesTexSubImage2D = sub_image_2d;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    ctx.unpack_alignment = 4;
    ctx.pack_alignment = 4;
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);
    glGenTextures(1, &a);
    glBindTexture(GL_TEXTURE_2D, a);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, original);
    glGenTextures(1, &b);
    glBindTexture(GL_TEXTURE_2D, b);
    glClearTexImage(a, 0, GL_RGBA, GL_UNSIGNED_BYTE, clear);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    if (glGetError() != GL_NO_ERROR || bound != (GLint)b || bound_2d != b || upload_binding != a ||
        sub2d_count != 1 || memcmp(uploaded, expected, 8) != 0)
        result = 2;
    glClearTexSubImage(a, 0, 2, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, clear);
    if (glGetError() != GL_INVALID_OPERATION || sub2d_count != 1)
        result = 3;
    glGenTextures(1, &c);
    glBindTexture(GL_TEXTURE_2D, c);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 2, 1, 0, GL_RGBA, GL_FLOAT, float_original);
    memcpy(float_expected, float_clear, sizeof(float_clear));
    memcpy(float_expected + 4, float_clear, sizeof(float_clear));
    glClearTexImage(c, 0, GL_RGBA, GL_FLOAT, float_clear);
    if (glGetError() != GL_NO_ERROR || uploaded_bytes != sizeof(float_expected) ||
        memcmp(uploaded, float_expected, sizeof(float_expected)) != 0)
        result = 4;
    glDeleteTextures(1, &a);
    glDeleteTextures(1, &b);
    glDeleteTextures(1, &c);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return result;
}

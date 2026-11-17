#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_texture_native;
    GLuint next_buffer_native;
    unsigned active_unit;
    unsigned tex_buffer_calls;
    unsigned tex_buffer_unit;
    GLenum tex_buffer_format;
    GLuint tex_buffer_name;
} cap;

static const GLubyte *fake_get_string(GLenum name) {
    static const GLubyte extensions[] = "GL_EXT_texture_buffer";
    if (name == GL_EXTENSIONS)
        return extensions;
    return (const GLubyte *)"";
}

static void fake_active_texture(GLenum texture) {
    cap.active_unit = (unsigned)(texture - GL_TEXTURE0);
}

static void fake_gen_textures(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_texture_native;
}

static void fake_delete_textures(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_texture(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_tex_buffer(GLenum target, GLenum internalformat, GLuint buffer) {
    if (target != GL_TEXTURE_BUFFER)
        return;
    ++cap.tex_buffer_calls;
    cap.tex_buffer_unit = cap.active_unit;
    cap.tex_buffer_format = internalformat;
    cap.tex_buffer_name = buffer;
}

static void fake_gen_buffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_buffer_native;
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

static int fail(int code, const char *message) {
    fprintf(stderr, "MultiTexBuffer failure %d: %s\n", code, message);
    return code;
}

static int expect_error(struct mgl_context *ctx, GLenum expected, int code, const char *message) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr, "MultiTexBuffer failure %d: %s (got 0x%04x expected 0x%04x)\n", code,
                message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_texture *tex_a_obj, *tex_b_obj, *tex_c_obj;
    struct mgl_buffer *buffer_a_obj, *buffer_b_obj;
    GLuint textures[3] = {0u, 0u, 0u};
    GLuint buffers[2] = {0u, 0u};
    GLuint list;
    unsigned char data_a[64], data_b[32];
    GLint value = -1;
    unsigned calls_before;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    memset(data_a, 0x3c, sizeof(data_a));
    memset(data_b, 0xa5, sizeof(data_b));

    native.glesGetString = fake_get_string;
    native.glesActiveTexture = fake_active_texture;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexBufferEXT = fake_tex_buffer;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glGenBuffers(2, buffers);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(data_a), data_a, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(data_b), data_b, GL_STATIC_DRAW);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 2, "buffer realization")) != 0)
        return rc;
    buffer_a_obj = mgl_buffer_find(&ctx, buffers[0]);
    buffer_b_obj = mgl_buffer_find(&ctx, buffers[1]);
    if (buffer_a_obj == NULL || buffer_b_obj == NULL || !buffer_a_obj->realized ||
        !buffer_b_obj->realized)
        return fail(3, "canonical buffers");

    glGenTextures(3, textures);
    glBindMultiTexture(GL_TEXTURE2, GL_TEXTURE_BUFFER, textures[0]);
    glBindMultiTexture(GL_TEXTURE3, GL_TEXTURE_BUFFER, textures[1]);
    glBindMultiTexture(GL_TEXTURE2, GL_TEXTURE_BUFFER, textures[2]);
    glBindMultiTexture(GL_TEXTURE2, GL_TEXTURE_BUFFER, textures[0]);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 4, "explicit buffer-texture binds")) != 0)
        return rc;
    tex_a_obj = mgl_texture_find(&ctx, textures[0]);
    tex_b_obj = mgl_texture_find(&ctx, textures[1]);
    tex_c_obj = mgl_texture_find(&ctx, textures[2]);
    if (tex_a_obj == NULL || tex_b_obj == NULL || tex_c_obj == NULL)
        return fail(5, "canonical textures");

    glActiveTexture(GL_TEXTURE1);
    glMultiTexBufferEXT(GL_TEXTURE2, GL_TEXTURE_BUFFER, GL_RGBA8, buffers[0]);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 6, "EXT explicit-unit attach")) != 0)
        return rc;
    if (ctx.compat.active_texture != 1u || cap.active_unit != 1u)
        return fail(7, "active selector was not restored");
    if (tex_a_obj->buffer_texture_buffer != buffer_a_obj ||
        tex_a_obj->buffer_texture_internal_format != GL_RGBA8)
        return fail(8, "unit 2 canonical attachment");
    if (tex_b_obj->buffer_texture_buffer != NULL)
        return fail(9, "unit 3 texture was modified");
    if (cap.tex_buffer_calls != 1u || cap.tex_buffer_unit != 2u ||
        cap.tex_buffer_format != GL_RGBA8 || cap.tex_buffer_name != buffer_a_obj->native_name)
        return fail(10, "native lowering did not run on explicit unit");

    glMultiTexBuffer(GL_TEXTURE3, GL_TEXTURE_BUFFER, GL_R8, buffers[1]);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 11, "unsuffixed explicit-unit attach")) != 0)
        return rc;
    if (ctx.compat.active_texture != 1u || cap.active_unit != 1u ||
        tex_b_obj->buffer_texture_buffer != buffer_b_obj)
        return fail(12, "unit 3 attach/selector preservation");
    glGetMultiTexLevelParameteriv(GL_TEXTURE3, GL_TEXTURE_BUFFER, 0,
                                  GL_TEXTURE_BUFFER_DATA_STORE_BINDING, &value);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 13, "explicit-unit buffer query")) != 0 ||
        (GLuint)value != buffers[1])
        return fail(14, "unit 3 data-store query");
    glGetMultiTexLevelParameteriv(GL_TEXTURE3, GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &value);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 15, "explicit-unit width query")) != 0 ||
        value != (GLint)sizeof(data_b))
        return fail(16, "R8 width query");

    list = glGenLists(1);
    if (list == 0u)
        return fail(17, "display-list allocation");
    calls_before = cap.tex_buffer_calls;
    glNewList(list, GL_COMPILE);
    glMultiTexBuffer(GL_TEXTURE2, GL_TEXTURE_BUFFER, GL_R8, buffers[1]);
    glEndList();
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 18, "display-list compilation")) != 0)
        return rc;
    if (tex_a_obj->buffer_texture_buffer != buffer_a_obj || cap.tex_buffer_calls != calls_before)
        return fail(19, "compile-only list executed MultiTexBuffer");

    glBindMultiTexture(GL_TEXTURE2, GL_TEXTURE_BUFFER, textures[2]);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 20, "late-bound texture selection")) != 0)
        return rc;
    glCallList(list);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 21, "display-list replay")) != 0)
        return rc;
    if (tex_c_obj->buffer_texture_buffer != buffer_b_obj ||
        tex_c_obj->buffer_texture_internal_format != GL_R8)
        return fail(22, "replay did not use current unit-2 binding");
    if (tex_a_obj->buffer_texture_buffer != buffer_a_obj)
        return fail(23, "replay retargeted compile-time texture binding");
    if (ctx.compat.active_texture != 1u || cap.active_unit != 1u)
        return fail(24, "replay did not restore active selector");
    if (cap.tex_buffer_unit != 2u || cap.tex_buffer_name != buffer_b_obj->native_name)
        return fail(25, "replay native lowering used wrong unit/buffer");

    calls_before = cap.tex_buffer_calls;
    glMultiTexBuffer(GL_TEXTURE0 - 1u, GL_TEXTURE_BUFFER, GL_R8, buffers[0]);
    if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 26, "low texunit")) != 0)
        return rc;
    glMultiTexBuffer(GL_TEXTURE0 + MGL_TEXTURE_UNITS, GL_TEXTURE_BUFFER, GL_R8, buffers[0]);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 27, "high texunit")) != 0)
        return rc;
    glMultiTexBuffer(GL_TEXTURE2, GL_TEXTURE_2D, GL_R8, buffers[0]);
    if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 28, "invalid target")) != 0)
        return rc;
    glMultiTexBuffer(GL_TEXTURE2, GL_TEXTURE_BUFFER, GL_RGBA, buffers[0]);
    if ((rc = expect_error(&ctx, GL_INVALID_ENUM, 29, "invalid internal format")) != 0)
        return rc;
    glMultiTexBuffer(GL_TEXTURE2, GL_TEXTURE_BUFFER, GL_R8, 0x7fffffffu);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 30, "unknown buffer")) != 0)
        return rc;
    if (ctx.compat.active_texture != 1u || cap.active_unit != 1u ||
        cap.tex_buffer_calls != calls_before)
        return fail(31, "error path changed selector or native attachment");

    glMultiTexBufferEXT(GL_TEXTURE2, GL_TEXTURE_BUFFER, GL_R8, 0u);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 32, "explicit-unit detach")) != 0)
        return rc;
    if (tex_c_obj->buffer_texture_buffer != NULL || ctx.compat.active_texture != 1u ||
        cap.active_unit != 1u)
        return fail(33, "detach state/selector");

    if (glXGetProcAddress((const GLubyte *)"glMultiTexBuffer") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexBufferEXT") == NULL)
        return fail(34, "proc-address coverage");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL MultiTexBuffer unit test: PASS");
    return 0;
}

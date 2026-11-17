#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static unsigned indexed_binds;
static GLuint next_name = 100u;
static GLuint bound_name;
static GLenum bound_target;

static void fake_bind(GLenum target, GLuint name) {
    bound_target = target;
    bound_name = name;
}

static void fake_gen(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++next_name;
}

static void fake_delete(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_base(GLenum target, GLuint index, GLuint name) {
    (void)target;
    (void)index;
    (void)name;
    ++indexed_binds;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "native buffer cache test %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_share_group *share;
    GLuint name;

    memset(&native, 0, sizeof(native));
    native.glesBindBuffer = fake_bind;
    native.glesGenBuffers = fake_gen;
    native.glesDeleteBuffers = fake_delete;
    native.glesBindBufferBase = fake_bind_base;
    share = mgl_share_group_create();
    if (share == NULL)
        return fail(1, "share group");
    memset(&ctx, 0, sizeof(ctx));
    ctx.native_dispatch = &native;
    ctx.share_group = share;
    ctx.current_vao = &ctx.default_vao;
    mgl_error_init(&ctx.errors);
    mgl_context_set_current_for_test(&ctx);

    glGenBuffers(1, &name);
    if (name == 0u)
        return fail(2, "glGenBuffers");
    glBindBuffer(GL_ARRAY_BUFFER, name);
    if (bound_target != GL_ARRAY_BUFFER || bound_name != 101u)
        return fail(3, "initial binding");
    glBindBuffer(GL_ARRAY_BUFFER, name);
    if (bound_name != 101u || bound_target != GL_ARRAY_BUFFER)
        return fail(4, "repeated interface bind");

    mgl_native_dispatch_bind_buffer(&ctx, GL_ARRAY_BUFFER, 999u);
    glBindBuffer(GL_ARRAY_BUFFER, name);
    if (bound_target != GL_ARRAY_BUFFER || bound_name != 101u)
        return fail(5, "internal bind tracking");

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, name);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, name);
    if (bound_target != GL_ELEMENT_ARRAY_BUFFER || bound_name != 101u)
        return fail(6, "element buffer binding");

    mgl_native_dispatch_bind_buffer(&ctx, GL_UNIFORM_BUFFER, 200u);
    mgl_native_dispatch_bind_buffer(&ctx, GL_UNIFORM_BUFFER, 200u);
    if (bound_target != GL_UNIFORM_BUFFER || bound_name != 200u)
        return fail(7, "internal uniform buffer binding");
    glBindBufferBase(GL_UNIFORM_BUFFER, 0u, name);
    if (indexed_binds != 1u)
        return fail(8, "indexed bind dispatch");
    mgl_native_dispatch_bind_buffer(&ctx, GL_UNIFORM_BUFFER, 200u);
    if (bound_target != GL_UNIFORM_BUFFER || bound_name != 200u)
        return fail(9, "indexed binding state restoration");

    bound_name = 0u;
    mgl_context_set_current_for_test(&ctx);
    glBindBuffer(GL_ARRAY_BUFFER, name);
    if (bound_target != GL_ARRAY_BUFFER || bound_name != 101u)
        return fail(10, "context activation restores binding");

    glDeleteBuffers(1, &name);
    glBindBuffer(GL_ARRAY_BUFFER, name);
    if (bound_name != 102u)
        return fail(12, "name reuse");

    {
        struct mgl_buffer *retained;
        (void)pthread_mutex_lock(&share->lock);
        retained = mgl_buffer_find(&ctx, name);
        if (retained == NULL)
            return fail(13, "existing buffer");
        mgl_buffer_retain_texture(retained);
        mgl_buffer_remove(&ctx, name);
        (void)pthread_mutex_unlock(&share->lock);
        glBindBuffer(GL_ARRAY_BUFFER, name);
        if (bound_name != 103u)
            return fail(14, "shared deletion invalidates interface binding cache");
        (void)pthread_mutex_lock(&share->lock);
        mgl_buffer_release_texture(&ctx, retained);
        (void)pthread_mutex_unlock(&share->lock);
    }

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(share);
    puts("mGL native buffer cache unit test: PASS");
    return 0;
}

#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    int token;
    unsigned fence_calls;
    unsigned delete_calls;
    unsigned client_wait_calls;
    unsigned wait_calls;
    unsigned flush_calls;
    GLbitfield last_flags;
    GLuint64 last_timeout;
} cap;

static GLsync fake_fence(GLenum condition, GLbitfield flags) {
    (void)condition;
    (void)flags;
    ++cap.fence_calls;
    return (GLsync)&cap.token;
}

static GLboolean fake_is_sync(GLsync sync) {
    return sync == (GLsync)&cap.token ? GL_TRUE : GL_FALSE;
}

static void fake_delete(GLsync sync) {
    if (sync == (GLsync)&cap.token)
        ++cap.delete_calls;
}

static GLenum fake_client_wait(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    if (sync != (GLsync)&cap.token)
        return GL_WAIT_FAILED;
    ++cap.client_wait_calls;
    cap.last_flags = flags;
    cap.last_timeout = timeout;
    return GL_CONDITION_SATISFIED;
}

static void fake_wait(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    if (sync == (GLsync)&cap.token) {
        ++cap.wait_calls;
        cap.last_flags = flags;
        cap.last_timeout = timeout;
    }
}

static void fake_get_sync(GLsync sync, GLenum pname, GLsizei size, GLsizei *length, GLint *values) {
    (void)sync;
    (void)pname;
    if (length != NULL)
        *length = size > 0 ? 1 : 0;
    if (size > 0 && values != NULL)
        *values = (GLint)GL_SIGNALED;
}

static void fake_flush(void) {
    ++cap.flush_calls;
}

static int fail(int n, const char *msg) {
    fprintf(stderr, "sync failure %d: %s\n", n, msg);
    return n;
}

static void init_context(struct mgl_context *ctx, struct mgl_native_dispatch *native) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->native_dispatch = native;
    ctx->share_group = mgl_share_group_create();
    mgl_error_init(&ctx->errors);
    mgl_compat_state_init(&ctx->compat);
    mgl_vertex_array_init(&ctx->default_vao);
    ctx->current_vao = &ctx->default_vao;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLsync sync;
    GLint value = 0;
    GLsizei length = 0;
    GLenum wait_result;

    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesFenceSync = fake_fence;
    native.glesIsSync = fake_is_sync;
    native.glesDeleteSync = fake_delete;
    native.glesClientWaitSync = fake_client_wait;
    native.glesWaitSync = fake_wait;
    native.glesGetSynciv = fake_get_sync;
    native.glesFlush = fake_flush;
    init_context(&ctx, &native);
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_context_set_current_for_test(&ctx);

    sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0u);
    if (sync == NULL || cap.fence_calls != 1u || !glIsSync(sync))
        return fail(2, "wrapper/native fence identity");
    if (sync == (GLsync)&cap.token)
        return fail(3, "must not expose native pointer");

    glGetSynciv(sync, GL_OBJECT_TYPE, 1, &length, &value);
    if (length != 1 || value != (GLint)GL_SYNC_FENCE)
        return fail(4, "object type");
    glGetSynciv(sync, GL_SYNC_CONDITION, 1, &length, &value);
    if (value != (GLint)GL_SYNC_GPU_COMMANDS_COMPLETE)
        return fail(5, "condition");
    glGetSynciv(sync, GL_SYNC_STATUS, 1, &length, &value);
    if (value != (GLint)GL_SIGNALED)
        return fail(6, "native status");

    wait_result = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 1234u);
    if (wait_result != GL_CONDITION_SATISFIED || cap.client_wait_calls != 1u ||
        cap.last_flags != GL_SYNC_FLUSH_COMMANDS_BIT || cap.last_timeout != 1234u)
        return fail(7, "client wait forwarding");
    glWaitSync(sync, 0u, GL_TIMEOUT_IGNORED);
    if (cap.wait_calls != 1u || cap.last_timeout != GL_TIMEOUT_IGNORED)
        return fail(8, "server wait forwarding");

    glWaitSync(sync, 1u, GL_TIMEOUT_IGNORED);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(9, "wait flags validation");
    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 1u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(10, "fence flags validation");

    /* Every sync entry point must reject Begin/End use before parameter checks,
     * without creating, deleting or touching a native sync object. */
    {
        unsigned fence_before = cap.fence_calls;
        unsigned delete_before = cap.delete_calls;
        unsigned client_before = cap.client_wait_calls;
        unsigned wait_before = cap.wait_calls;
        unsigned flush_before = cap.flush_calls;
        GLsync rejected;
        GLboolean identified;
        GLint unchanged = -123;
        GLsizei unchanged_length = -9;
        int test_case;
        for (test_case = 0; test_case < 9; ++test_case) {
            glBegin(GL_POINTS);
            switch (test_case) {
                case 0:
                    rejected = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0u);
                    if (rejected != NULL)
                        return fail(15, "illegal fence creation");
                    break;
                case 1:
                    rejected = glFenceSync(0u, 1u);
                    if (rejected != NULL)
                        return fail(16, "fence validation priority");
                    break;
                case 2:
                    identified = glIsSync(sync);
                    if (identified)
                        return fail(17, "illegal sync identification");
                    break;
                case 3:
                    glDeleteSync(sync);
                    break;
                case 4:
                    (void)glClientWaitSync(sync, 2u, 0u);
                    break;
                case 5:
                    glWaitSync(sync, 1u, 0u);
                    break;
                case 6:
                    glGetSynciv(sync, 0u, 1, &unchanged_length, &unchanged);
                    break;
                case 7:
                    glDeleteSync(NULL);
                    break;
                case 8:
                    identified = glIsSync(NULL);
                    if (identified)
                        return fail(18, "null sync identified");
                    break;
            }
            glEnd();
            if (glGetError() != GL_INVALID_OPERATION)
                return fail(19, "Begin/End error priority");
            if (cap.fence_calls != fence_before || cap.delete_calls != delete_before ||
                cap.client_wait_calls != client_before || cap.wait_calls != wait_before ||
                cap.flush_calls != flush_before)
                return fail(20, "Begin/End reached native");
        }
        if (!glIsSync(sync) || unchanged != -123 || unchanged_length != -9)
            return fail(21, "Begin/End mutated sync or query output");
        if (glGetError() != GL_NO_ERROR)
            return fail(22, "unexpected sync query error");
    }

    glDeleteSync(sync);
    if (glIsSync(sync) || cap.delete_calls != 1u)
        return fail(11, "delete wrapper/native");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL sync object unit test: PASS");
    return 0;
}

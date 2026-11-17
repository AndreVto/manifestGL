#include "abi/unimplemented_exports.h"
#include "core/context.h"
#include "objects/nv_fence.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

static unsigned created, deleted, polled, flushed, poll_flushes;
static GLenum poll_result;
static int require_poll_flush;

static GLsync fake_fence(GLenum condition, GLbitfield flags) {
    if (condition != GL_SYNC_GPU_COMMANDS_COMPLETE || flags != 0u)
        return NULL;
    ++created;
    return (GLsync)(uintptr_t)(created + 1u);
}

static void fake_delete(GLsync handle) {
    if (handle != NULL)
        ++deleted;
}

static GLenum fake_wait(GLsync handle, GLbitfield flags, GLuint64 timeout) {
    if (handle == NULL)
        return GL_WAIT_FAILED;
    if (flags == GL_SYNC_FLUSH_COMMANDS_BIT) {
        if (timeout == GL_TIMEOUT_IGNORED)
            ++flushed;
        else if (timeout == 0u)
            ++poll_flushes;
    }
    ++polled;
    if (require_poll_flush && timeout == 0u)
        return flags == GL_SYNC_FLUSH_COMMANDS_BIT ? GL_CONDITION_SATISFIED
                                                  : GL_TIMEOUT_EXPIRED;
    return poll_result;
}

int main(void) {
    struct mgl_context ctx, other;
    struct mgl_native_dispatch native;
    GLuint names[2] = {0u, 0u};
    GLint value = -1;

    memset(&ctx, 0, sizeof(ctx));
    memset(&other, 0, sizeof(other));
    memset(&native, 0, sizeof(native));
    native.glesFenceSync = fake_fence;
    native.glesClientWaitSync = fake_wait;
    native.glesDeleteSync = fake_delete;
    ctx.native_dispatch = &native;
    other.native_dispatch = &native;
    mgl_error_init(&ctx.errors);
    mgl_error_init(&other.errors);
    mgl_context_set_current_for_test(&ctx);

    glGenFencesNV(2, names);
    CHECK(names[0] != 0u && names[0] != names[1] && glGetError() == GL_NO_ERROR);
    CHECK(!glIsFenceNV(names[0]) && glGetError() == GL_NO_ERROR);
    CHECK(glTestFenceNV(names[0]) == GL_TRUE && glGetError() == GL_INVALID_OPERATION);
    glFinishFenceNV(names[0]);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    glSetFenceNV(names[0], GL_ALL_COMPLETED_NV);
    CHECK(glGetError() == GL_NO_ERROR && glIsFenceNV(names[0]) && created == 1u);
    glGetFenceivNV(names[0], GL_FENCE_CONDITION_NV, &value);
    CHECK(glGetError() == GL_NO_ERROR && value == (GLint)GL_ALL_COMPLETED_NV);

    poll_result = GL_TIMEOUT_EXPIRED;
    CHECK(glTestFenceNV(names[0]) == GL_FALSE && polled == 1u && glGetError() == GL_NO_ERROR);
    value = -1;
    glGetFenceivNV(names[0], GL_FENCE_STATUS_NV, &value);
    CHECK(glGetError() == GL_NO_ERROR && value == GL_FALSE);
    poll_result = GL_ALREADY_SIGNALED;
    CHECK(glTestFenceNV(names[0]) == GL_TRUE && glGetError() == GL_NO_ERROR);
    CHECK(glTestFenceNV(names[0]) == GL_TRUE && polled == 3u && poll_flushes == 3u);
    glSetFenceNV(names[0], GL_ALL_COMPLETED_NV);
    CHECK(glGetError() == GL_NO_ERROR && created == 2u && deleted == 1u);
    poll_result = GL_CONDITION_SATISFIED;
    glFinishFenceNV(names[0]);
    CHECK(glGetError() == GL_NO_ERROR && flushed == 1u && polled == 4u);

    glSetFenceNV(names[1], GL_ALL_COMPLETED_NV);
    CHECK(glGetError() == GL_NO_ERROR);
    require_poll_flush = 1;
    poll_result = GL_TIMEOUT_EXPIRED;
    CHECK(glTestFenceNV(names[1]) == GL_TRUE && poll_flushes == 4u &&
          glGetError() == GL_NO_ERROR);
    glSetFenceNV(names[1], GL_ALL_COMPLETED_NV);
    CHECK(glGetError() == GL_NO_ERROR);
    value = -1;
    glGetFenceivNV(names[1], GL_FENCE_STATUS_NV, &value);
    CHECK(value == GL_TRUE && poll_flushes == 5u && glGetError() == GL_NO_ERROR);
    require_poll_flush = 0;

    glSetFenceNV(99u, GL_ALL_COMPLETED_NV);
    CHECK(glGetError() == GL_NO_ERROR && glIsFenceNV(99u));
    glSetFenceNV(100u, 0u);
    CHECK(glGetError() == GL_INVALID_ENUM && !glIsFenceNV(100u));
    mgl_context_set_current_for_test(&other);
    CHECK(!glIsFenceNV(names[0]) && glTestFenceNV(names[0]) == GL_TRUE &&
          glGetError() == GL_INVALID_OPERATION);
    mgl_context_set_current_for_test(&ctx);

    ctx.immediate.active = GL_TRUE;
    glDeleteFencesNV(1, names);
    CHECK(glIsFenceNV(names[0]) == GL_FALSE);
    ctx.immediate.active = GL_FALSE;
    CHECK(glGetError() == GL_INVALID_OPERATION && glIsFenceNV(names[0]) == GL_TRUE);
    glDeleteFencesNV(2, names);
    CHECK(glGetError() == GL_NO_ERROR && !glIsFenceNV(names[0]) && deleted == 4u);
    glDeleteFencesNV(1, names);
    CHECK(glGetError() == GL_NO_ERROR);
    glGenFencesNV(-1, names);
    CHECK(glGetError() == GL_INVALID_VALUE);

    ctx.share_group = mgl_share_group_create();
    CHECK(ctx.share_group != NULL);
    glNewList(8u, GL_COMPILE);
    glSetFenceNV(101u, GL_ALL_COMPLETED_NV);
    glEndList();
    CHECK(glGetError() == GL_NO_ERROR && !glIsFenceNV(101u));
    glCallList(8u);
    CHECK(glGetError() == GL_NO_ERROR && glIsFenceNV(101u));
    glDeleteLists(8u, 1);
    mgl_share_group_release(ctx.share_group);
    ctx.share_group = NULL;
    mgl_nv_fence_free_all(&ctx, 1);
    CHECK(deleted == created);
    mgl_context_set_current_for_test(NULL);
    return 0;
}

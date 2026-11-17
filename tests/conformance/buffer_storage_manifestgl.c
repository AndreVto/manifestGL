#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "core/error.h"
#include "conformance/buffer_storage_cases.h"
#include <string.h>

static GLuint next_name;

static void gen(GLsizei n, GLuint *out) {
    for (GLsizei i = 0; i < n; ++i)
        out[i] = ++next_name;
}

static void bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenBuffers = gen;
    native.glesBindBuffer = bind_buffer;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);
    if (glXGetProcAddress((const GLubyte *)"glBufferStorage") == NULL ||
        glXGetProcAddress((const GLubyte *)"glNamedBufferStorage") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetBufferParameteri64v") == NULL)
        return 2;
    buffer_storage_trace();
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

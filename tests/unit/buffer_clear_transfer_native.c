#include "mgl/gl.h"
#include "core/context.h"
#include "core/error.h"
#include <stdio.h>
#include <string.h>

static struct {
    GLuint next_name;
    GLuint bound;
    unsigned calls;
    GLintptr offset;
    GLsizeiptr length;
    unsigned char bytes[32];
} cap;

static void gen(GLsizei n, GLuint *names) {
    for (GLsizei i = 0; i < n; ++i)
        names[i] = ++cap.next_name;
}

static void bind(GLenum target, GLuint name) {
    if (target == GL_COPY_WRITE_BUFFER)
        cap.bound = name;
}

static void upload(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)target;
    (void)size;
    (void)data;
    (void)usage;
}

static void subdata(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    if (target != GL_COPY_WRITE_BUFFER || cap.bound == 0u || size > (GLsizeiptr)sizeof(cap.bytes))
        return;
    cap.calls++;
    cap.offset = offset;
    cap.length = size;
    if (data != NULL && size > 0)
        memcpy(cap.bytes, data, (size_t)size);
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint buffer = 0u;
    unsigned char init[24], readback[24];
    const GLuint pattern = 0x12345678u;
    unsigned before;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    memset(init, 0x55, sizeof(init));
    native.glesGenBuffers = gen;
    native.glesBindBuffer = bind;
    native.glesBufferData = upload;
    native.glesBufferSubData = subdata;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (!ctx.share_group)
        return 1;
    mgl_error_init(&ctx.errors);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, buffer);
    glBufferData(GL_COPY_WRITE_BUFFER, 24, init, GL_DYNAMIC_DRAW);
    glClearBufferSubData(GL_COPY_WRITE_BUFFER, GL_R32UI, 4, 12, GL_RED_INTEGER, GL_UNSIGNED_INT,
                         &pattern);
    if (glGetError() != GL_NO_ERROR || cap.calls != 1u || cap.offset != 4 || cap.length != 12 ||
        memcmp(cap.bytes, &pattern, 4) || memcmp(cap.bytes + 4, &pattern, 4) ||
        memcmp(cap.bytes + 8, &pattern, 4))
        return 2;
    glGetBufferSubData(GL_COPY_WRITE_BUFFER, 0, 24, readback);
    if (readback[0] != 0x55u || readback[16] != 0x55u || memcmp(readback + 4, cap.bytes, 12))
        return 3;
    before = cap.calls;
    glClearBufferSubData(GL_COPY_WRITE_BUFFER, GL_R32UI, 2, 8, GL_RED_INTEGER, GL_UNSIGNED_INT,
                         &pattern);
    if (glGetError() != GL_INVALID_VALUE || cap.calls != before)
        return 4;
    glClearBufferData(GL_COPY_WRITE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
    if (glGetError() != GL_NO_ERROR || cap.calls != before + 1u || cap.length != 24)
        return 5;
    for (size_t i = 0; i < 24; ++i)
        if (cap.bytes[i] != 0u)
            return 6;
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("buffer clear native: PASS");
    return 0;
}

#include "mgl/gl.h"
#include "abi/proc_address.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_buffer;
    GLenum pixel_store_pname;
    GLint pixel_store_param;
    unsigned pixel_store_calls;
} cap;

static void fake_gen_buffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_buffer;
}

static void fake_delete_buffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_pixel_store(GLenum pname, GLint param) {
    cap.pixel_store_pname = pname;
    cap.pixel_store_param = param;
    ++cap.pixel_store_calls;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "buffer/pixel parity failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint buffer = 0u;
    unsigned char initial[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    unsigned char *mapped;
    void *pointer = (void *)1;
    GLint iv = 0;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesPixelStorei = fake_pixel_store;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 16, 16);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, 8, initial, GL_STATIC_DRAW);
    glGetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER, &pointer);
    if (pointer != NULL || glGetError() != GL_NO_ERROR)
        return fail(2, "unmapped pointer query");

    mapped = glMapBufferRange(GL_ARRAY_BUFFER, 2, 4, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
    if (mapped == NULL)
        return fail(3, "map range");
    glGetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER, &pointer);
    if (pointer != mapped || glGetError() != GL_NO_ERROR)
        return fail(4, "mapped pointer query");
    glGetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &pointer);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(5, "pointer pname validation");
    if (!glUnmapBuffer(GL_ARRAY_BUFFER))
        return fail(6, "unmap");

    glBindBuffer(GL_ARRAY_BUFFER, 0u);
    glGetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER, &pointer);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(7, "unbound pointer validation");
    glGetBufferPointerv(0xdeadbeefu, GL_BUFFER_MAP_POINTER, &pointer);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(8, "buffer target validation");

    glPixelStoref(GL_PACK_ALIGNMENT, 3.6f);
    glGetIntegerv(GL_PACK_ALIGNMENT, &iv);
    if (iv != 4 || cap.pixel_store_calls != 1u || cap.pixel_store_pname != GL_PACK_ALIGNMENT ||
        cap.pixel_store_param != 4)
        return fail(9, "PixelStoref nearest integer realization");

    glPixelStoref(GL_UNPACK_ALIGNMENT, 1.4f);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &iv);
    if (iv != 1 || cap.pixel_store_calls != 2u || cap.pixel_store_param != 1)
        return fail(10, "PixelStoref unpack realization");

    glPixelStoref(GL_PACK_ALIGNMENT, 3.4f);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(11, "PixelStoref rounded invalid alignment");
    glGetIntegerv(GL_PACK_ALIGNMENT, &iv);
    if (iv != 4 || cap.pixel_store_calls != 2u)
        return fail(12, "invalid PixelStoref changed state/native");

    glPixelStoref(0xdeadbeefu, 4.0f);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(13, "PixelStoref pname validation");

    if (mgl_proc_address("glGetBufferPointerv") == NULL ||
        mgl_proc_address("glPixelStoref") == NULL)
        return fail(14, "proc-address coverage");

    glDeleteBuffers(1, &buffer);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL buffer/pixel parity unit test: PASS");
    return 0;
}

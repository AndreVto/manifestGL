#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "objects/query.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "buffer/query DSA:%d: %s\n", __LINE__, #expr);                         \
            return __LINE__;                                                                       \
        }                                                                                          \
    } while (0)

static GLuint next_native = 100u;
static GLuint copy_read, copy_write, array_binding;
static unsigned copies, subdata_calls, invalid_targets;
static GLenum query_available = GL_TRUE;

static void gen_buffers(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i)
        names[i] = ++next_native;
}

static void delete_buffers(GLsizei count, const GLuint *names) {
    (void)count;
    (void)names;
}

static void bind_buffer(GLenum target, GLuint name) {
    if (target == GL_COPY_READ_BUFFER)
        copy_read = name;
    if (target == GL_COPY_WRITE_BUFFER)
        copy_write = name;
    if (target == GL_ARRAY_BUFFER)
        array_binding = name;
    if (target == GL_QUERY_BUFFER)
        ++invalid_targets;
}

static void buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)size;
    (void)data;
    (void)usage;
    if (target == GL_QUERY_BUFFER)
        ++invalid_targets;
}

static void buffer_subdata(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    (void)offset;
    (void)size;
    (void)data;
    ++subdata_calls;
    if (target == GL_QUERY_BUFFER)
        ++invalid_targets;
}

static void copy_buffer(GLenum read_target, GLenum write_target, GLintptr read_offset,
                        GLintptr write_offset, GLsizeiptr size) {
    (void)read_offset;
    (void)write_offset;
    (void)size;
    if (read_target == GL_COPY_READ_BUFFER && write_target == GL_COPY_WRITE_BUFFER)
        ++copies;
}

static void get_query(GLuint id, GLenum pname, GLuint *value) {
    (void)id;
    *value = pname == GL_QUERY_RESULT_AVAILABLE ? query_available : 27u;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_query *query;
    struct mgl_buffer *obj;
    GLuint buffers[3], queries[1];
    const unsigned char initial[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const unsigned char pair[2] = {0xa1, 0xb2};
    unsigned char out[16];
    GLuint out32 = 0;
    GLuint64 out64 = 0;
    GLint signed32;
    GLint64 signed64;
    unsigned previous_copies;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenBuffers = gen_buffers;
    native.glesDeleteBuffers = delete_buffers;
    native.glesBindBuffer = bind_buffer;
    native.glesBufferData = buffer_data;
    native.glesBufferSubData = buffer_subdata;
    native.glesCopyBufferSubData = copy_buffer;
    native.glesGetQueryObjectuiv = get_query;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    CHECK(ctx.share_group != NULL);
    ctx.next_query_name = 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glCreateBuffers(3, buffers);
    CHECK(glGetError() == GL_NO_ERROR);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glNamedBufferData(buffers[0], sizeof(initial), initial, GL_DYNAMIC_DRAW);
    glNamedBufferData(buffers[1], sizeof(initial), initial, GL_DYNAMIC_DRAW);
    glNamedBufferData(buffers[2], sizeof(initial), initial, GL_DYNAMIC_DRAW);
    CHECK(glGetError() == GL_NO_ERROR);

    glClearNamedBufferSubData(buffers[1], GL_RG8, 4, 4, GL_RG, GL_UNSIGNED_BYTE, pair);
    CHECK(glGetError() == GL_NO_ERROR);
    glGetNamedBufferSubData(buffers[1], 0, sizeof(out), out);
    CHECK(memcmp(out, initial, 4) == 0 && out[4] == 0xa1 && out[5] == 0xb2 && out[6] == 0xa1 &&
          out[7] == 0xb2);
    CHECK(memcmp(out + 8, initial + 8, 8) == 0);
    CHECK(ctx.array_buffer == buffers[0] &&
          array_binding == mgl_buffer_find(&ctx, buffers[0])->native_name);
    CHECK(copy_write == 0);

    glClearNamedBufferData(buffers[1], GL_R8, GL_RED, GL_UNSIGNED_BYTE, NULL);
    CHECK(glGetError() == GL_NO_ERROR);
    glGetNamedBufferSubData(buffers[1], 0, sizeof(out), out);
    for (unsigned i = 0; i < sizeof(out); ++i)
        CHECK(out[i] == 0u);

    glClearNamedBufferData(0, GL_R8, GL_RED, GL_UNSIGNED_BYTE, NULL);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    glClearNamedBufferSubData(buffers[1], GL_RG8, 1, 4, GL_RG, GL_UNSIGNED_BYTE, pair);
    CHECK(glGetError() == GL_INVALID_VALUE);
    glClearNamedBufferSubData(buffers[1], GL_RG8, 14, 4, GL_RG, GL_UNSIGNED_BYTE, pair);
    CHECK(glGetError() == GL_INVALID_VALUE);
    glClearNamedBufferData(buffers[1], 0xdeadbeefu, GL_RED, GL_UNSIGNED_BYTE, pair);
    CHECK(glGetError() == GL_INVALID_ENUM);
    glClearNamedBufferData(buffers[1], GL_RG8, GL_RGBA, GL_UNSIGNED_BYTE, pair);
    CHECK(glGetError() == GL_INVALID_OPERATION);

    obj = mgl_buffer_find(&ctx, buffers[1]);
    CHECK(obj != NULL);
    obj->immutable = GL_TRUE;
    obj->native_immutable = GL_TRUE;
    obj->storage_flags = GL_MAP_READ_BIT;
    previous_copies = copies;
    glClearNamedBufferSubData(buffers[1], GL_RG8, 2, 2, GL_RG, GL_UNSIGNED_BYTE, pair);
    CHECK(glGetError() == GL_NO_ERROR);
    CHECK(copies == previous_copies + 1 && copy_read == 0u && copy_write == 0u);

    glCreateQueries(GL_TIMESTAMP, 1, queries);
    CHECK(glGetError() == GL_NO_ERROR);
    query = mgl_query_find(&ctx, queries[0]);
    CHECK(query != NULL);
    query->result = UINT64_C(0x100000002);
    query->available = GL_TRUE;

    glGetQueryBufferObjectui64v(queries[0], buffers[1], GL_QUERY_RESULT, 8);
    CHECK(glGetError() == GL_NO_ERROR);
    glGetNamedBufferSubData(buffers[1], 8, 8, &out64);
    CHECK(out64 == UINT64_C(0x100000002));
    glGetQueryBufferObjectuiv(queries[0], buffers[1], GL_QUERY_RESULT, 0);
    glGetNamedBufferSubData(buffers[1], 0, 4, &out32);
    CHECK(out32 == UINT32_MAX);
    glGetQueryBufferObjectiv(queries[0], buffers[1], GL_QUERY_RESULT, 4);
    glGetNamedBufferSubData(buffers[1], 4, 4, &signed32);
    CHECK(signed32 == INT32_MAX);
    glGetQueryBufferObjecti64v(queries[0], buffers[1], GL_QUERY_RESULT, 8);
    glGetNamedBufferSubData(buffers[1], 8, 8, &signed64);
    CHECK(signed64 == INT64_C(0x100000002));
    glGetQueryBufferObjectuiv(queries[0], buffers[1], GL_QUERY_TARGET, 0);
    glGetNamedBufferSubData(buffers[1], 0, 4, &out32);
    CHECK(out32 == GL_TIMESTAMP);
    CHECK(glGetError() == GL_NO_ERROR);
    CHECK(ctx.array_buffer == buffers[0] && copy_read == 0u && copy_write == 0u);
    glGetQueryObjectuiv(queries[0], GL_QUERY_RESULT, &out32);
    CHECK(out32 == UINT32_MAX);
    glGetQueryObjectiv(queries[0], GL_QUERY_RESULT, &signed32);
    CHECK(signed32 == INT32_MAX);

    {
        GLint binding = 0;
        GLuint query_storage;
        glBindBuffer(GL_QUERY_BUFFER, buffers[1]);
        glGetIntegerv(GL_QUERY_BUFFER_BINDING, &binding);
        CHECK(binding == (GLint)buffers[1] && glGetError() == GL_NO_ERROR);
        glGetQueryObjectuiv(queries[0], GL_QUERY_RESULT, (GLuint *)(uintptr_t)0);
        glGetNamedBufferSubData(buffers[1], 0, 4, &out32);
        CHECK(out32 == UINT32_MAX && glGetError() == GL_NO_ERROR);
        glGetQueryObjectui64v(queries[0], GL_QUERY_RESULT, (GLuint64 *)(uintptr_t)8);
        glGetNamedBufferSubData(buffers[1], 8, 8, &out64);
        CHECK(out64 == UINT64_C(0x100000002) && glGetError() == GL_NO_ERROR);
        glGetQueryObjectiv(queries[0], GL_QUERY_RESULT, (GLint *)(uintptr_t)3);
        CHECK(glGetError() == GL_INVALID_OPERATION);
        glGetQueryObjectui64v(queries[0], GL_QUERY_RESULT, (GLuint64 *)(uintptr_t)12);
        CHECK(glGetError() == GL_INVALID_OPERATION);
        glBindBuffer(GL_QUERY_BUFFER, 0u);
        glGetIntegerv(GL_QUERY_BUFFER_BINDING, &binding);
        CHECK(binding == 0 && glGetError() == GL_NO_ERROR);
        glCreateBuffers(1, &query_storage);
        glBindBuffer(GL_QUERY_BUFFER, query_storage);
        glBufferData(GL_QUERY_BUFFER, sizeof(initial), initial, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_QUERY_BUFFER, 4, sizeof(pair), pair);
        glGetNamedBufferSubData(query_storage, 0, sizeof(out), out);
        CHECK(memcmp(out, initial, 4) == 0 && out[4] == pair[0] && out[5] == pair[1]);
        CHECK(copy_read == 0u && copy_write == 0u && invalid_targets == 0u &&
              glGetError() == GL_NO_ERROR);
        glDeleteBuffers(1, &query_storage);
        glGetIntegerv(GL_QUERY_BUFFER_BINDING, &binding);
        CHECK(binding == 0 && glGetError() == GL_NO_ERROR);
    }

    glBindBuffer(GL_COPY_READ_BUFFER, buffers[0]);
    glBindBuffer(GL_COPY_WRITE_BUFFER, buffers[2]);
    CHECK(copy_read == mgl_buffer_find(&ctx, buffers[0])->native_name);
    CHECK(copy_write == mgl_buffer_find(&ctx, buffers[2])->native_name);
    glGetQueryBufferObjectuiv(queries[0], buffers[1], GL_QUERY_RESULT, 0);
    CHECK(glGetError() == GL_NO_ERROR);
    CHECK(copy_read == mgl_buffer_find(&ctx, buffers[0])->native_name);
    CHECK(copy_write == mgl_buffer_find(&ctx, buffers[2])->native_name);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

    glGetQueryBufferObjectuiv(queries[0], buffers[1], GL_QUERY_RESULT, -1);
    CHECK(glGetError() == GL_INVALID_VALUE);
    glGetQueryBufferObjectuiv(queries[0], buffers[1], GL_QUERY_RESULT, 13);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    glGetQueryBufferObjectuiv(queries[0], 0u, GL_QUERY_RESULT, 0);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    glGetQueryBufferObjectuiv(queries[0], buffers[1], 0x123456u, 0);
    CHECK(glGetError() == GL_INVALID_ENUM);
    glGetQueryBufferObjectuiv(queries[0] + 100u, buffers[1], GL_QUERY_RESULT, 0);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    query->active = GL_TRUE;
    glGetQueryBufferObjectuiv(queries[0], buffers[1], GL_QUERY_RESULT, 0);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    query->active = GL_FALSE;

    obj->mapped = GL_TRUE;
    obj->map_access = GL_MAP_WRITE_BIT;
    glGetQueryBufferObjectuiv(queries[0], buffers[1], GL_QUERY_RESULT, 0);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    obj->mapped = GL_FALSE;
    query->native_managed = GL_TRUE;
    query->native_name = 7u;
    query_available = GL_FALSE;
    previous_copies = copies;
    glGetQueryBufferObjectuiv(queries[0], buffers[1], GL_QUERY_RESULT_NO_WAIT, 0);
    CHECK(glGetError() == GL_NO_ERROR && copies == previous_copies);
    glGetNamedBufferSubData(buffers[1], 0, 4, &out32);
    CHECK(out32 == UINT32_MAX);
    query->native_managed = GL_FALSE;

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

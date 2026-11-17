#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "shader/translator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_buffer;
    unsigned char *last_array_upload;
    size_t last_array_size;
    unsigned char *last_element_upload;
    size_t last_element_size;
    GLenum last_element_type;
    unsigned draw_calls;
} cap;

static void fake_get_integerv(GLenum pname, GLint *value) {
    *value = pname == GL_MAX_VERTEX_ATTRIBS ? 16 : 0;
}

static void fake_gen_buffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_buffer;
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
    unsigned char **dst = NULL;
    size_t *dst_size = NULL;
    (void)usage;
    if (target == GL_ARRAY_BUFFER) {
        dst = &cap.last_array_upload;
        dst_size = &cap.last_array_size;
    } else if (target == GL_ELEMENT_ARRAY_BUFFER) {
        dst = &cap.last_element_upload;
        dst_size = &cap.last_element_size;
    } else
        return;
    free(*dst);
    *dst = NULL;
    *dst_size = 0u;
    if (size > 0 && data != NULL) {
        *dst = malloc((size_t)size);
        if (*dst != NULL) {
            memcpy(*dst, data, (size_t)size);
            *dst_size = (size_t)size;
        }
    }
}

static void fake_buffer_sub_data(GLenum target, GLintptr off, GLsizeiptr size, const void *data) {
    unsigned char **dst;
    size_t *dst_size;
    if (off < 0 || size <= 0 || data == NULL)
        return;
    if (target == GL_ARRAY_BUFFER) {
        dst = &cap.last_array_upload;
        dst_size = &cap.last_array_size;
    } else if (target == GL_ELEMENT_ARRAY_BUFFER) {
        dst = &cap.last_element_upload;
        dst_size = &cap.last_element_size;
    } else
        return;
    free(*dst);
    *dst = malloc((size_t)size);
    *dst_size = *dst != NULL ? (size_t)size : 0u;
    if (*dst != NULL)
        memcpy(*dst, data, (size_t)size);
}

static void fake_vertex_attrib_pointer(GLuint loc, GLint size, GLenum type, GLboolean normalized,
                                       GLsizei stride, const void *p) {
    (void)loc;
    (void)size;
    (void)type;
    (void)normalized;
    (void)stride;
    (void)p;
}

static void fake_enable_attrib(GLuint i) {
    (void)i;
}

static void fake_disable_attrib(GLuint i) {
    (void)i;
}

static void fake_attrib4fv(GLuint i, const GLfloat *v) {
    (void)i;
    (void)v;
}

static void fake_use_program(GLuint p) {
    (void)p;
}

static void fake_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    (void)mode;
    (void)first;
    (void)count;
    ++cap.draw_calls;
}

static void fake_draw_elements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    (void)mode;
    (void)count;
    (void)indices;
    cap.last_element_type = type;
    ++cap.draw_calls;
}

static void init_native(struct mgl_native_dispatch *native) {
    memset(native, 0, sizeof(*native));
    native->glesGetIntegerv = fake_get_integerv;
    native->glesGenBuffers = fake_gen_buffers;
    native->glesDeleteBuffers = fake_delete_buffers;
    native->glesBindBuffer = fake_bind_buffer;
    native->glesBufferData = fake_buffer_data;
    native->glesBufferSubData = fake_buffer_sub_data;
    native->glesVertexAttribPointer = fake_vertex_attrib_pointer;
    native->glesEnableVertexAttribArray = fake_enable_attrib;
    native->glesDisableVertexAttribArray = fake_disable_attrib;
    native->glesVertexAttrib4fv = fake_attrib4fv;
    native->glesUseProgram = fake_use_program;
    native->glesDrawArrays = fake_draw_arrays;
    native->glesDrawElements = fake_draw_elements;
}

static int fail(int n, const char *why) {
    fprintf(stderr, "display-list draw snapshot failure %d: %s\n", n, why);
    return n;
}

static int install_program(struct mgl_context *ctx) {
    struct mgl_program *program = calloc(1, sizeof(*program));
    size_t i;
    if (program == NULL)
        return 0;
    program->name = 19u;
    program->native_name = 119u;
    program->linked = GL_TRUE;
    for (i = 0u; i < MGL_COMPAT_SEMANTIC_COUNT; ++i)
        program->compat_attrib[i] = -1;
    program->compat_attrib[MGL_COMPAT_SEM_SECONDARY] = 9;
    program->compat_uniform_model_view_projection = -1;
    program->compat_uniform_model_view = -1;
    program->compat_uniform_projection = -1;
    program->compat_uniform_normal_matrix = -1;
    program->next = ctx->share_group->programs;
    ctx->share_group->programs = program;
    ctx->current_program = program->name;
    return 1;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLfloat client_data[2][3] = {{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}};
    const GLfloat client_expected[2][3] = {{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}};
    GLfloat vbo_data[2][3] = {{7.0f, 8.0f, 9.0f}, {10.0f, 11.0f, 12.0f}};
    const GLfloat vbo_expected[2][3] = {{7.0f, 8.0f, 9.0f}, {10.0f, 11.0f, 12.0f}};
    GLfloat replacement[2][3] = {{99.0f, 99.0f, 99.0f}, {88.0f, 88.0f, 88.0f}};
    GLfloat *high_vertices = NULL;
    const GLfloat high_expected[3][3] = {
        {13.0f, 14.0f, 15.0f}, {16.0f, 17.0f, 18.0f}, {19.0f, 20.0f, 21.0f}};
    const GLuint high_indices[3] = {65535u, 65536u, 65537u};
    const GLubyte rebased_expected[3] = {0u, 1u, 2u};
    const GLint multi_first[2] = {0, 1};
    const GLsizei multi_count[2] = {1, 1};
    GLuint list_client, list_vbo, list_high, list_multi, vbo;

    memset(&ctx, 0, sizeof(ctx));
    memset(&cap, 0, sizeof(cap));
    init_native(&native);
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.native_max_vertex_attribs = 16;
    mgl_context_set_current_for_test(&ctx);
    if (!install_program(&ctx))
        return fail(2, "program");

    list_client = glGenLists(1);
    glSecondaryColorPointer(3, GL_FLOAT, 0, client_data);
    glEnableClientState(GL_SECONDARY_COLOR_ARRAY);
    glNewList(list_client, GL_COMPILE);
    glDrawArrays(GL_POINTS, 0, 2);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.draw_calls != 0u)
        return fail(3, "client compile");
    memset(client_data, 0, sizeof(client_data));
    glCallList(list_client);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.draw_calls != 1u)
        return fail(4, "client replay");
    if (cap.last_array_size != sizeof(client_expected) ||
        memcmp(cap.last_array_upload, client_expected, sizeof(client_expected)) != 0)
        return fail(5, "client array was not snapshotted at compile time");

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(vbo_data), vbo_data, GL_STATIC_DRAW);
    glSecondaryColorPointer(3, GL_FLOAT, 0, (const void *)(uintptr_t)0u);
    list_vbo = glGenLists(1);
    glNewList(list_vbo, GL_COMPILE);
    glDrawArrays(GL_POINTS, 0, 2);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.draw_calls != 1u)
        return fail(6, "VBO compile");
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(replacement), replacement);
    glCallList(list_vbo);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.draw_calls != 2u)
        return fail(7, "VBO replay");
    if (cap.last_array_size != sizeof(vbo_expected) ||
        memcmp(cap.last_array_upload, vbo_expected, sizeof(vbo_expected)) != 0)
        return fail(8, "VBO list snapshot used post-compile buffer contents");

    high_vertices = calloc(65538u * 3u, sizeof(*high_vertices));
    if (high_vertices == NULL)
        return fail(9, "high vertex allocation");
    memcpy(high_vertices + 65535u * 3u, high_expected, sizeof(high_expected));
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(65538u * 3u * sizeof(*high_vertices)), high_vertices,
                 GL_STATIC_DRAW);
    glSecondaryColorPointer(3, GL_FLOAT, 0, (const void *)(uintptr_t)0u);
    list_high = glGenLists(1);
    glNewList(list_high, GL_COMPILE);
    glDrawElements(GL_POINTS, 3, GL_UNSIGNED_INT, high_indices);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.draw_calls != 2u)
        return fail(10, "high-index compile");
    memset(high_vertices + 65535u * 3u, 0, sizeof(high_expected));
    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(65535u * 3u * sizeof(*high_vertices)),
                    (GLsizeiptr)sizeof(high_expected), high_vertices + 65535u * 3u);
    glCallList(list_high);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.draw_calls != 3u)
        return fail(11, "high-index replay");
    if (cap.last_array_size != sizeof(high_expected) ||
        memcmp(cap.last_array_upload, high_expected, sizeof(high_expected)) != 0)
        return fail(12, "high-index vertex snapshot mismatch");
    if (cap.last_element_type != GL_UNSIGNED_BYTE ||
        cap.last_element_size != sizeof(rebased_expected) ||
        memcmp(cap.last_element_upload, rebased_expected, sizeof(rebased_expected)) != 0)
        return fail(13, "high uint indices were not safely rebased");

    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(vbo_data), vbo_data, GL_STATIC_DRAW);
    list_multi = glGenLists(1);
    glNewList(list_multi, GL_COMPILE);
    glMultiDrawArrays(GL_POINTS, multi_first, multi_count, 2);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.draw_calls != 3u)
        return fail(14, "multidraw compile");
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(replacement), replacement);
    glCallList(list_multi);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.draw_calls != 5u)
        return fail(15, "multidraw replay");
    if (cap.last_array_size != sizeof(vbo_expected[1]) ||
        memcmp(cap.last_array_upload, vbo_expected[1], sizeof(vbo_expected[1])) != 0)
        return fail(16, "multidraw snapshot used post-compile contents");

    glDeleteLists(list_client, 1);
    glDeleteLists(list_vbo, 1);
    glDeleteLists(list_high, 1);
    glDeleteLists(list_multi, 1);
    free(high_vertices);
    free(cap.last_array_upload);
    free(cap.last_element_upload);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL display-list draw snapshot unit test: PASS");
    return 0;
}

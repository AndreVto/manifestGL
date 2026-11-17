#include "mgl/gl.h"
#include "abi/proc_address.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "shader/translator.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct expected_layout {
    GLenum format;
    GLboolean texcoord;
    GLboolean color;
    GLboolean normal;
    GLint texcoord_size;
    GLint color_size;
    GLint vertex_size;
    GLenum color_type;
    size_t color_offset;
    size_t normal_offset;
    size_t vertex_offset;
    GLsizei stride;
};

static int fail(int n, const char *message) {
    fprintf(stderr, "interleaved arrays failure %d: %s\n", n, message);
    return n;
}

static int check_binding(const struct mgl_vertex_binding *binding, GLboolean enabled, GLint size,
                         GLenum type, GLboolean normalized, GLsizei stride, const void *base,
                         size_t offset) {
    if (binding->enabled != enabled)
        return 0;
    if (!enabled)
        return 1;
    return binding->size == size && binding->type == type && binding->normalized == normalized &&
           binding->stride == stride && binding->data.source == MGL_DATA_CLIENT &&
           binding->data.client_ptr == (const void *)((uintptr_t)base + (uintptr_t)offset);
}

int main(void) {
    struct mgl_context ctx;
    unsigned char client[128];
    const size_t f = sizeof(GLfloat);
    const size_t c = ((4u * sizeof(GLubyte) + f - 1u) / f) * f;
    const struct expected_layout layouts[] = {
        {GL_V2F, GL_FALSE, GL_FALSE, GL_FALSE, 0, 0, 2, GL_FLOAT, 0u, 0u, 0u, (GLsizei)(2u * f)},
        {GL_V3F, GL_FALSE, GL_FALSE, GL_FALSE, 0, 0, 3, GL_FLOAT, 0u, 0u, 0u, (GLsizei)(3u * f)},
        {GL_C4UB_V2F, GL_FALSE, GL_TRUE, GL_FALSE, 0, 4, 2, GL_UNSIGNED_BYTE, 0u, 0u, c,
         (GLsizei)(c + 2u * f)},
        {GL_C4UB_V3F, GL_FALSE, GL_TRUE, GL_FALSE, 0, 4, 3, GL_UNSIGNED_BYTE, 0u, 0u, c,
         (GLsizei)(c + 3u * f)},
        {GL_C3F_V3F, GL_FALSE, GL_TRUE, GL_FALSE, 0, 3, 3, GL_FLOAT, 0u, 0u, 3u * f,
         (GLsizei)(6u * f)},
        {GL_N3F_V3F, GL_FALSE, GL_FALSE, GL_TRUE, 0, 0, 3, GL_FLOAT, 0u, 0u, 3u * f,
         (GLsizei)(6u * f)},
        {GL_C4F_N3F_V3F, GL_FALSE, GL_TRUE, GL_TRUE, 0, 4, 3, GL_FLOAT, 0u, 4u * f, 7u * f,
         (GLsizei)(10u * f)},
        {GL_T2F_V3F, GL_TRUE, GL_FALSE, GL_FALSE, 2, 0, 3, GL_FLOAT, 0u, 0u, 2u * f,
         (GLsizei)(5u * f)},
        {GL_T4F_V4F, GL_TRUE, GL_FALSE, GL_FALSE, 4, 0, 4, GL_FLOAT, 0u, 0u, 4u * f,
         (GLsizei)(8u * f)},
        {GL_T2F_C4UB_V3F, GL_TRUE, GL_TRUE, GL_FALSE, 2, 4, 3, GL_UNSIGNED_BYTE, 2u * f, 0u,
         c + 2u * f, (GLsizei)(c + 5u * f)},
        {GL_T2F_C3F_V3F, GL_TRUE, GL_TRUE, GL_FALSE, 2, 3, 3, GL_FLOAT, 2u * f, 0u, 5u * f,
         (GLsizei)(8u * f)},
        {GL_T2F_N3F_V3F, GL_TRUE, GL_FALSE, GL_TRUE, 2, 0, 3, GL_FLOAT, 0u, 2u * f, 5u * f,
         (GLsizei)(8u * f)},
        {GL_T2F_C4F_N3F_V3F, GL_TRUE, GL_TRUE, GL_TRUE, 2, 4, 3, GL_FLOAT, 2u * f, 6u * f, 9u * f,
         (GLsizei)(12u * f)},
        {GL_T4F_C4F_N3F_V4F, GL_TRUE, GL_TRUE, GL_TRUE, 4, 4, 4, GL_FLOAT, 4u * f, 8u * f, 11u * f,
         (GLsizei)(15u * f)}};
    size_t i;

    memset(&ctx, 0, sizeof(ctx));
    memset(client, 0, sizeof(client));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.client_active_texture = 3u;
    mgl_context_set_current_for_test(&ctx);

    for (i = 0u; i < sizeof(layouts) / sizeof(layouts[0]); ++i) {
        const struct expected_layout *e = &layouts[i];
        const struct mgl_vertex_binding *vertex;
        const struct mgl_vertex_binding *normal;
        const struct mgl_vertex_binding *color;
        const struct mgl_vertex_binding *secondary;
        const struct mgl_vertex_binding *texcoord;

        memset(&ctx.default_vao, 0, sizeof(ctx.default_vao));
        mgl_vertex_array_init(&ctx.default_vao);
        ctx.current_vao = &ctx.default_vao;
        ctx.default_vao.compat[MGL_COMPAT_SEM_SECONDARY].enabled = GL_TRUE;
        ctx.default_vao.compat[MGL_COMPAT_SEM_TEXCOORD0].enabled = GL_TRUE;

        glInterleavedArrays(e->format, 0, client);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(2, "valid format error");

        vertex = &ctx.default_vao.compat[MGL_COMPAT_SEM_POSITION];
        normal = &ctx.default_vao.compat[MGL_COMPAT_SEM_NORMAL];
        color = &ctx.default_vao.compat[MGL_COMPAT_SEM_COLOR];
        secondary = &ctx.default_vao.compat[MGL_COMPAT_SEM_SECONDARY];
        texcoord = &ctx.default_vao.compat[MGL_COMPAT_SEM_TEXCOORD0 + 3u];

        if (!check_binding(vertex, GL_TRUE, e->vertex_size, GL_FLOAT, GL_FALSE, e->stride, client,
                           e->vertex_offset))
            return fail(3, "vertex layout");
        if (!check_binding(normal, e->normal, 3, GL_FLOAT, GL_FALSE, e->stride, client,
                           e->normal_offset))
            return fail(4, "normal layout");
        if (!check_binding(color, e->color, e->color_size, e->color_type,
                           e->color_type == GL_UNSIGNED_BYTE ? GL_TRUE : GL_FALSE, e->stride,
                           client, e->color_offset))
            return fail(5, "color layout");
        if (!check_binding(texcoord, e->texcoord, e->texcoord_size, GL_FLOAT, GL_FALSE, e->stride,
                           client, 0u))
            return fail(6, "texcoord layout");
        if (secondary->enabled != GL_FALSE)
            return fail(7, "secondary array not disabled");
        if (ctx.default_vao.compat[MGL_COMPAT_SEM_TEXCOORD0].enabled != GL_TRUE)
            return fail(8, "non-active texture coordinate array changed");
    }

    memset(&ctx.default_vao, 0, sizeof(ctx.default_vao));
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.array_buffer = 77u;
    glInterleavedArrays(GL_C4UB_V3F, 64, (const void *)(uintptr_t)32u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(9, "VBO interleaved error");
    if (ctx.default_vao.compat[MGL_COMPAT_SEM_COLOR].data.source != MGL_DATA_BUFFER ||
        ctx.default_vao.compat[MGL_COMPAT_SEM_COLOR].data.buffer_name != 77u ||
        ctx.default_vao.compat[MGL_COMPAT_SEM_COLOR].data.offset != 32u ||
        ctx.default_vao.compat[MGL_COMPAT_SEM_COLOR].stride != 64)
        return fail(10, "VBO color reference");
    if (ctx.default_vao.compat[MGL_COMPAT_SEM_POSITION].data.source != MGL_DATA_BUFFER ||
        ctx.default_vao.compat[MGL_COMPAT_SEM_POSITION].data.buffer_name != 77u ||
        ctx.default_vao.compat[MGL_COMPAT_SEM_POSITION].data.offset != 32u + c ||
        ctx.default_vao.compat[MGL_COMPAT_SEM_POSITION].stride != 64)
        return fail(11, "VBO vertex reference");

    {
        struct mgl_vertex_binding before[MGL_COMPAT_SEMANTIC_COUNT];
        memcpy(before, ctx.default_vao.compat, sizeof(before));
        glInterleavedArrays(0xDEADu, 0, client);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
            return fail(12, "invalid format error");
        if (memcmp(before, ctx.default_vao.compat, sizeof(before)) != 0)
            return fail(13, "invalid format mutated state");
        glInterleavedArrays(GL_V3F, -1, client);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
            return fail(14, "negative stride error");
        if (memcmp(before, ctx.default_vao.compat, sizeof(before)) != 0)
            return fail(15, "negative stride mutated state");
    }

    if (mgl_proc_address("glInterleavedArrays") == NULL)
        return fail(16, "proc address");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

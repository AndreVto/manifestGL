#include "mgl/gl.h"
#include "compat/selection.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static int fail(int n, const char *msg) {
    fprintf(stderr, "selection failure %d: %s\n", n, msg);
    return n;
}

static void init_context(struct mgl_context *ctx) {
    unsigned i;
    memset(ctx, 0, sizeof(*ctx));
    ctx->share_group = mgl_share_group_create();
    mgl_error_init(&ctx->errors);
    mgl_compat_state_init(&ctx->compat);
    mgl_pixel_transfer_init(&ctx->pixel_transfer);
    mgl_selection_init(&ctx->selection);
    mgl_vertex_array_init(&ctx->default_vao);
    ctx->current_vao = &ctx->default_vao;
    ctx->pack_alignment = 4;
    ctx->unpack_alignment = 4;
    for (i = 0u; i < MGL_MAX_VERTEX_ATTRIBS; ++i)
        ctx->generic_current[i][3] = 1.0f;
    for (i = 0u; i < MGL_COMPAT_SEMANTIC_COUNT; ++i)
        ctx->compat_current[i][3] = 1.0f;
}

int main(void) {
    struct mgl_context ctx;
    GLuint selectbuf[32];
    GLfloat tri0[] = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f};
    GLfloat tri1[] = {-0.25f, -0.25f, 0.5f, 0.25f, -0.25f, 0.5f, 0.0f, 0.25f, 0.5f};
    GLfloat outside[] = {2.0f, 2.0f, 0.0f};
    GLuint tiny[3] = {0u, 0u, 0u};
    GLuint list;
    GLint value = 0;
    GLint hits;

    init_context(&ctx);
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_context_set_current_for_test(&ctx);

    glGetIntegerv(GL_RENDER_MODE, &value);
    if (value != (GLint)GL_RENDER)
        return fail(2, "default render mode");
    glSelectBuffer(32, selectbuf);
    if (glRenderMode(GL_SELECT) != 0 || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(3, "enter select");
    glInitNames();
    glPushName(7u);
    glVertexPointer(3, GL_FLOAT, 0, tri0);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(4, "first selection draw");

    glLoadName(9u);
    glVertexPointer(3, GL_FLOAT, 0, tri1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glVertexPointer(3, GL_FLOAT, 0, outside);
    glDrawArrays(GL_POINTS, 0, 1);
    hits = glRenderMode(GL_RENDER);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || hits != 2)
        return fail(5, "hit count");
    if (selectbuf[0] != 1u || selectbuf[3] != 7u || selectbuf[4] != 1u || selectbuf[7] != 9u)
        return fail(6, "name records");
    if (selectbuf[1] < 0x7ffffff0u || selectbuf[1] > 0x80000010u || selectbuf[2] != selectbuf[1])
        return fail(7, "first depth word");
    if (selectbuf[5] <= selectbuf[1] || selectbuf[6] != selectbuf[5])
        return fail(8, "second depth word");
    glGetIntegerv(GL_NAME_STACK_DEPTH, &value);
    if (value != 1)
        return fail(9, "name stack depth query");

    glRenderMode(GL_SELECT);
    glSelectBuffer(3, tiny);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(10, "SelectBuffer while selecting");
    (void)glRenderMode(GL_RENDER);

    glSelectBuffer(3, tiny);
    glRenderMode(GL_SELECT);
    glInitNames();
    glPushName(1u);
    glVertexPointer(3, GL_FLOAT, 0, tri0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (glRenderMode(GL_RENDER) != -1)
        return fail(11, "selection overflow result");

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glInitNames();
    glPushName(42u);
    glVertexPointer(3, GL_FLOAT, 0, tri0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEndList();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(12, "compile selection list");
    memset(selectbuf, 0, sizeof(selectbuf));
    glSelectBuffer(32, selectbuf);
    glRenderMode(GL_SELECT);
    glCallList(list);
    hits = glRenderMode(GL_RENDER);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || hits != 1 || selectbuf[0] != 1u ||
        selectbuf[3] != 42u)
        return fail(13, "selection list replay");

    glInitNames();
    glPopName();
    if (mgl_error_take(&ctx.errors) != GL_STACK_UNDERFLOW)
        return fail(14, "name stack underflow");
    glLoadName(5u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(15, "load name on empty stack");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL selection mode unit test: PASS");
    return 0;
}

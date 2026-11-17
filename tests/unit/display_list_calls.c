#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "state/state.h"
#include "shader/translator.h"

#include <stdio.h>
#include <string.h>

static int fail(int code, const char *message) {
    fprintf(stderr, "display-list calls failure %d: %s\n", code, message);
    return code;
}

static int color_is(const struct mgl_context *ctx, GLfloat r, GLfloat g, GLfloat b) {
    return ctx->compat_current[MGL_COMPAT_SEM_COLOR][0] == r &&
           ctx->compat_current[MGL_COMPAT_SEM_COLOR][1] == g &&
           ctx->compat_current[MGL_COMPAT_SEM_COLOR][2] == b;
}

static void set_white(struct mgl_context *ctx) {
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][0] = 1.0f;
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][1] = 1.0f;
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][2] = 1.0f;
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][3] = 1.0f;
}

int main(void) {
    struct mgl_context ctx;
    GLuint base;
    GLuint parent;
    GLint value = -1;
    GLubyte ub = 1u;
    GLbyte sb = -1;
    GLushort us = 2u;
    GLshort ss = -1;
    GLuint ui = 1u;
    GLint si = -1;
    GLfloat f = 2.75f;
    GLubyte two_bytes[2] = {0x01u, 0x02u};
    GLubyte three_bytes[3] = {0x00u, 0x00u, 0x02u};
    GLubyte four_bytes[4] = {0x00u, 0x00u, 0x00u, 0x01u};
    GLubyte captured = 1u;

    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 1, 1);
    mgl_compat_state_init(&ctx.compat);
    set_white(&ctx);
    mgl_context_set_current_for_test(&ctx);

    glNewList(1u, GL_COMPILE);
    glEndList();
    if (!glIsList(1u))
        return fail(2, "manual list setup");
    base = glGenLists(260);
    if (base == 0u || base == 1u)
        return fail(3, "glGenLists collision with used name");
    if (!glIsList(base) || !glIsList(base + 259u))
        return fail(4, "glGenLists did not create empty list names");

    glNewList(base + 1u, GL_COMPILE);
    glColor3f(1.0f, 0.0f, 0.0f);
    glEndList();
    glNewList(base + 2u, GL_COMPILE);
    glColor3f(0.0f, 1.0f, 0.0f);
    glEndList();
    glNewList(base + 258u, GL_COMPILE);
    glColor3f(0.0f, 0.0f, 1.0f);
    glEndList();

    value = -1;
    glListBase(base);
    glGetIntegerv(GL_LIST_BASE, &value);
    if (value != (GLint)base)
        return fail(5, "GL_LIST_BASE query");

    glCallLists(1, GL_UNSIGNED_BYTE, &ub);
    if (!color_is(&ctx, 1.0f, 0.0f, 0.0f))
        return fail(6, "GL_UNSIGNED_BYTE offset");
    glCallLists(1, GL_UNSIGNED_SHORT, &us);
    if (!color_is(&ctx, 0.0f, 1.0f, 0.0f))
        return fail(7, "GL_UNSIGNED_SHORT offset");
    glCallLists(1, GL_UNSIGNED_INT, &ui);
    if (!color_is(&ctx, 1.0f, 0.0f, 0.0f))
        return fail(8, "GL_UNSIGNED_INT offset");
    glCallLists(1, GL_FLOAT, &f);
    if (!color_is(&ctx, 0.0f, 1.0f, 0.0f))
        return fail(9, "GL_FLOAT truncation");
    glCallLists(1, GL_2_BYTES, two_bytes);
    if (!color_is(&ctx, 0.0f, 0.0f, 1.0f))
        return fail(10, "GL_2_BYTES big-endian offset");
    glCallLists(1, GL_3_BYTES, three_bytes);
    if (!color_is(&ctx, 0.0f, 1.0f, 0.0f))
        return fail(11, "GL_3_BYTES offset");
    glCallLists(1, GL_4_BYTES, four_bytes);
    if (!color_is(&ctx, 1.0f, 0.0f, 0.0f))
        return fail(12, "GL_4_BYTES offset");

    glListBase(base + 2u);
    glCallLists(1, GL_BYTE, &sb);
    if (!color_is(&ctx, 1.0f, 0.0f, 0.0f))
        return fail(13, "GL_BYTE signed offset");
    glCallLists(1, GL_SHORT, &ss);
    if (!color_is(&ctx, 1.0f, 0.0f, 0.0f))
        return fail(14, "GL_SHORT signed offset");
    glCallLists(1, GL_INT, &si);
    if (!color_is(&ctx, 1.0f, 0.0f, 0.0f))
        return fail(15, "GL_INT signed offset");

    glPushAttrib(GL_LIST_BIT);
    glListBase(base + 20u);
    glPopAttrib();
    glGetIntegerv(GL_LIST_BASE, &value);
    if (value != (GLint)(base + 2u))
        return fail(16, "GL_LIST_BIT restore");

    parent = base + 10u;
    set_white(&ctx);
    glNewList(parent, GL_COMPILE);
    glCallLists(1, GL_UNSIGNED_BYTE, &captured);
    glEndList();
    if (!color_is(&ctx, 1.0f, 1.0f, 1.0f))
        return fail(17, "GL_COMPILE executed glCallLists");
    captured = 2u;
    glListBase(base);
    glCallList(parent);
    if (!color_is(&ctx, 1.0f, 0.0f, 0.0f))
        return fail(18, "glCallLists pointer data was not captured");
    glListBase(base + 1u);
    glCallList(parent);
    if (!color_is(&ctx, 0.0f, 1.0f, 0.0f))
        return fail(19, "glCallLists did not use replay-time list base");

    set_white(&ctx);
    glNewList(base + 11u, GL_COMPILE_AND_EXECUTE);
    glListBase(base);
    captured = 2u;
    glCallLists(1, GL_UNSIGNED_BYTE, &captured);
    glEndList();
    if (!color_is(&ctx, 0.0f, 1.0f, 0.0f))
        return fail(20, "GL_COMPILE_AND_EXECUTE did not execute glCallLists");
    glListBase(base + 50u);
    set_white(&ctx);
    glCallList(base + 11u);
    if (!color_is(&ctx, 0.0f, 1.0f, 0.0f))
        return fail(21, "compiled glListBase/glCallLists replay");
    glGetIntegerv(GL_LIST_BASE, &value);
    if (value != (GLint)base)
        return fail(22, "compiled glListBase state replay");

    set_white(&ctx);
    glCallLists(0, GL_UNSIGNED_BYTE, NULL);
    if (!color_is(&ctx, 1.0f, 1.0f, 1.0f) || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(23, "zero-count glCallLists");
    glCallLists(-1, GL_UNSIGNED_BYTE, &ub);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(24, "negative glCallLists count");
    glCallLists(1, 0xdeadbeefu, &ub);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(25, "invalid glCallLists type");
    glCallList(0u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(26, "glCallList zero name");

    glBegin(GL_POINTS);
    glListBase(base);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(27, "glListBase inside Begin/End");
    if (glGenLists(1) != 0u || mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(28, "glGenLists inside Begin/End");
    if (glIsList(base) != GL_FALSE || mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(29, "glIsList inside Begin/End");
    glDeleteLists(base, 1);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(30, "glDeleteLists inside Begin/End");
    glNewList(base + 300u, GL_COMPILE);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(31, "glNewList inside Begin/End");
    glEnd();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(32, "glEnd after management errors");
    if (!glIsList(base))
        return fail(33, "Begin/End delete guard mutated list");

    if (glXGetProcAddress((const GLubyte *)"glCallLists") == NULL ||
        glXGetProcAddress((const GLubyte *)"glListBase") == NULL)
        return fail(34, "proc-address exposure");

    {
        const GLubyte offsets[2] = {1u, 2u};
        glListBase(base);
        glEnable(GL_TEXTURE);
        glCallLists(2, GL_UNSIGNED_BYTE, offsets);
        if (!color_is(&ctx, 0.0f, 1.0f, 0.0f) ||
            mgl_error_take(&ctx.errors) != GL_INVALID_ENUM ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(35, "call-lists replay continues despite older error");
    }

    glDeleteLists(base, 260);
    glDeleteLists(1u, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL display-list call addressing unit test: PASS");
    return 0;
}

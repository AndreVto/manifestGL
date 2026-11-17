#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static unsigned flush_calls;
static unsigned finish_calls;

static int fail(int n, const char *why) {
    fprintf(stderr, "flush/finish semantics failure %d: %s\n", n, why);
    return n;
}

static void fake_flush(void) {
    ++flush_calls;
}

static void fake_finish(void) {
    ++finish_calls;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint list_compile;
    GLuint list_execute;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesFlush = fake_flush;
    native.glesFinish = fake_finish;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 320, 240);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glFlush();
    glFinish();
    if (flush_calls != 1u || finish_calls != 1u)
        return fail(2, "direct native lowering");
    if (glGetError() != GL_NO_ERROR)
        return fail(3, "direct calls generated error");

    /* Finish and Flush execute immediately while a display list is compiled and are not recorded.
     */
    list_compile = glGenLists(1);
    list_execute = glGenLists(1);
    if (list_compile == 0u || list_execute == 0u)
        return fail(4, "display-list allocation");

    glNewList(list_compile, GL_COMPILE);
    glFlush();
    glFinish();
    glEndList();
    if (flush_calls != 2u || finish_calls != 2u)
        return fail(5, "GL_COMPILE did not execute immediately");
    glCallList(list_compile);
    if (flush_calls != 2u || finish_calls != 2u)
        return fail(6, "GL_COMPILE incorrectly recorded command");

    glNewList(list_execute, GL_COMPILE_AND_EXECUTE);
    glFlush();
    glFinish();
    glEndList();
    if (flush_calls != 3u || finish_calls != 3u)
        return fail(7, "GL_COMPILE_AND_EXECUTE immediate execution");
    glCallList(list_execute);
    if (flush_calls != 3u || finish_calls != 3u)
        return fail(8, "GL_COMPILE_AND_EXECUTE incorrectly recorded command");

    glBegin(GL_POINTS);
    glFlush();
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION || flush_calls != 3u)
        return fail(9, "glFlush Begin/End legality");

    glBegin(GL_POINTS);
    glFinish();
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION || finish_calls != 3u)
        return fail(10, "glFinish Begin/End legality");

    if (glXGetProcAddress((const GLubyte *)"glFlush") == NULL ||
        glXGetProcAddress((const GLubyte *)"glFinish") == NULL)
        return fail(11, "proc-address surface");

    glDeleteLists(list_compile, 1);
    glDeleteLists(list_execute, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL flush/finish semantics unit test: PASS");
    return 0;
}

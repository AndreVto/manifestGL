#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct capture {
    unsigned clear_color_calls;
    unsigned color_mask_calls;
};

static struct capture cap;

static int fail(int n, const char *why) {
    fprintf(stderr, "color-index state failure %d: %s\n", n, why);
    return n;
}

static void fake_clear_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    (void)r;
    (void)g;
    (void)b;
    (void)a;
    ++cap.clear_color_calls;
}

static void fake_color_mask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    (void)r;
    (void)g;
    (void)b;
    (void)a;
    ++cap.color_mask_calls;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint list;
    GLfloat clear_index = -99.0f;
    GLint index_mask = 0;
    unsigned clear_calls_before;
    unsigned mask_calls_before;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesClearColor = fake_clear_color;
    native.glesColorMask = fake_color_mask;
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

    glGetFloatv(GL_INDEX_CLEAR_VALUE, &clear_index);
    if (clear_index != 0.0f)
        return fail(2, "default clear index");
    glGetIntegerv(GL_INDEX_WRITEMASK, &index_mask);
    if ((GLuint)index_mask != UINT32_MAX)
        return fail(3, "default index writemask");

    clear_calls_before = cap.clear_color_calls;
    mask_calls_before = cap.color_mask_calls;
    glClearIndex(7.75f);
    glIndexMask(0x00ff00aau);
    if (glGetError() != GL_NO_ERROR)
        return fail(4, "direct setters generated an error");
    glGetFloatv(GL_INDEX_CLEAR_VALUE, &clear_index);
    glGetIntegerv(GL_INDEX_WRITEMASK, &index_mask);
    if (clear_index != 7.0f)
        return fail(5, "clear index fixed-point integer conversion");
    if ((GLuint)index_mask != 0x00ff00aau)
        return fail(6, "index writemask state");
    if (cap.clear_color_calls != clear_calls_before || cap.color_mask_calls != mask_calls_before)
        return fail(7, "color-index state leaked into RGBA native state");

    glPushAttrib(GL_COLOR_BUFFER_BIT);
    glClearIndex(-2.75f);
    glIndexMask(0x55aa55aau);
    glPopAttrib();
    if (glGetError() != GL_NO_ERROR)
        return fail(8, "color-buffer attribute stack");
    glGetFloatv(GL_INDEX_CLEAR_VALUE, &clear_index);
    glGetIntegerv(GL_INDEX_WRITEMASK, &index_mask);
    if (clear_index != 7.0f || (GLuint)index_mask != 0x00ff00aau)
        return fail(9, "color-buffer attribute restoration");

    list = glGenLists(1);
    if (list == 0u)
        return fail(10, "display-list allocation");
    glNewList(list, GL_COMPILE);
    glClearIndex(11.875f);
    glIndexMask(0xf0f00f0fu);
    glEndList();
    if (glGetError() != GL_NO_ERROR)
        return fail(11, "display-list compilation");
    glGetFloatv(GL_INDEX_CLEAR_VALUE, &clear_index);
    glGetIntegerv(GL_INDEX_WRITEMASK, &index_mask);
    if (clear_index != 7.0f || (GLuint)index_mask != 0x00ff00aau)
        return fail(12, "GL_COMPILE mutated color-index state");

    glCallList(list);
    if (glGetError() != GL_NO_ERROR)
        return fail(13, "display-list replay");
    glGetFloatv(GL_INDEX_CLEAR_VALUE, &clear_index);
    glGetIntegerv(GL_INDEX_WRITEMASK, &index_mask);
    if (clear_index != 11.0f || (GLuint)index_mask != 0xf0f00f0fu)
        return fail(14, "display-list color-index replay");

    glBegin(GL_POINTS);
    glClearIndex(3.5f);
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(15, "glClearIndex Begin/End legality");
    glGetFloatv(GL_INDEX_CLEAR_VALUE, &clear_index);
    if (clear_index != 11.0f)
        return fail(16, "rejected glClearIndex mutated state");

    glBegin(GL_POINTS);
    glIndexMask(0u);
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(17, "glIndexMask Begin/End legality");
    glGetIntegerv(GL_INDEX_WRITEMASK, &index_mask);
    if ((GLuint)index_mask != 0xf0f00f0fu)
        return fail(18, "rejected glIndexMask mutated state");

    if (glXGetProcAddress((const GLubyte *)"glClearIndex") == NULL ||
        glXGetProcAddress((const GLubyte *)"glIndexMask") == NULL)
        return fail(19, "proc-address surface");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL color-index state unit test: PASS");
    return 0;
}

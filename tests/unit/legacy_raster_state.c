#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static int fail(int index, const char *description) {
    fprintf(stderr, "legacy raster %d: %s\n", index, description);
    return index;
}

int main(void) {
    struct mgl_context ctx;
    GLubyte mask[128] = {0};
    GLubyte result[128] = {0};
    GLint value = 0;
    GLboolean flag = GL_FALSE;
    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 32, 32);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.current_edge_flag = GL_TRUE;
    ctx.pack_alignment = ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);
    glGetBooleanv(GL_EDGE_FLAG, &flag);
    if (flag != GL_TRUE)
        return fail(2, "edge flag initial");
    glEdgeFlag(GL_FALSE);
    glGetBooleanv(GL_EDGE_FLAG, &flag);
    if (flag != GL_FALSE)
        return fail(3, "edge flag update");
    glPushAttrib(GL_CURRENT_BIT);
    glEdgeFlag(GL_TRUE);
    glPopAttrib();
    glGetBooleanv(GL_EDGE_FLAG, &flag);
    if (flag != GL_FALSE)
        return fail(4, "edge flag restore");
    glLineStipple(1000, 0x0f0fu);
    glGetIntegerv(GL_LINE_STIPPLE_REPEAT, &value);
    if (value != 256)
        return fail(5, "repeat clamp");
    glGetIntegerv(GL_LINE_STIPPLE_PATTERN, &value);
    if (value != 0x0f0f)
        return fail(6, "pattern");
    glEnable(GL_LINE_STIPPLE);
    if (glIsEnabled(GL_LINE_STIPPLE) != GL_TRUE)
        return fail(7, "line enable");
    glPushAttrib(GL_LINE_BIT | GL_ENABLE_BIT);
    glLineStipple(-10, 0xffffu);
    glDisable(GL_LINE_STIPPLE);
    glPopAttrib();
    glGetIntegerv(GL_LINE_STIPPLE_REPEAT, &value);
    if (value != 256 || glIsEnabled(GL_LINE_STIPPLE) != GL_TRUE)
        return fail(8, "line restore");
    glGetPolygonStipple(result);
    for (unsigned i = 0; i < 128u; ++i)
        if (result[i] != 0xffu)
            return fail(9, "default mask");
    mask[0] = 0x80u;
    glPolygonStipple(mask);
    memset(result, 0, sizeof(result));
    glGetPolygonStipple(result);
    if (memcmp(result, mask, sizeof(mask)) != 0)
        return fail(10, "mask roundtrip");
    glPixelStorei(GL_PACK_LSB_FIRST, GL_TRUE);
    glGetPolygonStipple(result);
    if (result[0] != 0x01u)
        return fail(11, "LSB pack");
    glEnable(GL_POLYGON_STIPPLE);
    if (glIsEnabled(GL_POLYGON_STIPPLE) != GL_TRUE)
        return fail(12, "polygon enable");
    glPushAttrib(GL_POLYGON_STIPPLE_BIT);
    glDisable(GL_POLYGON_STIPPLE);
    glPopAttrib();
    if (glIsEnabled(GL_POLYGON_STIPPLE) != GL_TRUE)
        return fail(13, "polygon restore");
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(14, "unexpected error");
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

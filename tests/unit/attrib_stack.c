#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "shader/translator.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(int n, const char *why) {
    fprintf(stderr, "attrib-stack failure %d: %s\n", n, why);
    return n;
}

static int closef(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 0.0001f;
}

int main(void) {
    struct mgl_context ctx;
    GLfloat client_a[6] = {1, 2, 3, 4, 5, 6};
    GLfloat client_b[6] = {7, 8, 9, 10, 11, 12};
    GLfloat index_a[2] = {2, 4};
    GLfloat index_b[2] = {6, 8};
    GLint iv = 0;
    GLfloat fv = 0.0f;
    GLboolean enabled = GL_FALSE;
    GLuint list;

    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 640, 480);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    {
        unsigned i;
        for (i = 0; i < MGL_COMPAT_SEMANTIC_COUNT; ++i)
            ctx.compat_current[i][3] = 1.0f;
    }
    mgl_context_set_current_for_test(&ctx);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CW);
    glClearStencil(7);
    glPixelTransferi(GL_MAP_COLOR, 1);
    glPixelTransferf(GL_RED_SCALE, 2.5f);
    glColor4f(0.2f, 0.3f, 0.4f, 0.5f);
    glPushAttrib(GL_POLYGON_BIT | GL_STENCIL_BUFFER_BIT | GL_PIXEL_MODE_BIT | GL_CURRENT_BIT);
    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glClearStencil(2);
    glPixelTransferi(GL_MAP_COLOR, 0);
    glPixelTransferf(GL_RED_SCALE, 9.0f);
    glColor4f(1, 1, 1, 1);
    glPopAttrib();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(2, "server pop error");
    if (!glIsEnabled(GL_CULL_FACE))
        return fail(3, "cull enable restore");
    glGetIntegerv(GL_CULL_FACE_MODE, &iv);
    if (iv != (GLint)GL_FRONT)
        return fail(4, "cull mode restore");
    glGetIntegerv(GL_FRONT_FACE, &iv);
    if (iv != (GLint)GL_CW)
        return fail(5, "front face restore");
    glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &iv);
    if (iv != 7)
        return fail(6, "stencil restore");
    glGetIntegerv(GL_MAP_COLOR, &iv);
    if (iv != 1)
        return fail(7, "pixel map restore");
    glGetFloatv(GL_RED_SCALE, &fv);
    if (!closef(fv, 2.5f))
        return fail(8, "pixel scale restore");
    glGetFloatv(GL_CURRENT_COLOR, &fv);
    if (!closef(fv, 0.2f))
        return fail(9, "current color restore");

    glEnable(GL_FOG);
    glEnable(GL_DEPTH_TEST);
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glPopAttrib();
    if (!glIsEnabled(GL_FOG) || !glIsEnabled(GL_DEPTH_TEST))
        return fail(10, "enable group restore");

    if (glIsEnabled(GL_LINE_SMOOTH) || glIsEnabled(GL_FRAMEBUFFER_SRGB) ||
        glIsEnabled(GL_TEXTURE_3D) || glGetError() != GL_NO_ERROR)
        return fail(20, "initial legacy capabilities");
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_TEXTURE_3D);
    if (!glIsEnabled(GL_LINE_SMOOTH) || !glIsEnabled(GL_FRAMEBUFFER_SRGB) ||
        !glIsEnabled(GL_TEXTURE_3D) || glGetError() != GL_NO_ERROR)
        return fail(21, "legacy capability enables");
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_TEXTURE_3D);
    if (glIsEnabled(GL_LINE_SMOOTH) || glIsEnabled(GL_FRAMEBUFFER_SRGB) ||
        glIsEnabled(GL_TEXTURE_3D) || glGetError() != GL_NO_ERROR)
        return fail(22, "legacy capability disables");
    glPopAttrib();
    if (!glIsEnabled(GL_LINE_SMOOTH) || !glIsEnabled(GL_FRAMEBUFFER_SRGB) ||
        !glIsEnabled(GL_TEXTURE_3D) || glGetError() != GL_NO_ERROR)
        return fail(23, "legacy enable mask restore");
    glPushAttrib(GL_LINE_BIT | GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_TEXTURE_3D);
    glPopAttrib();
    if (!glIsEnabled(GL_LINE_SMOOTH) || !glIsEnabled(GL_FRAMEBUFFER_SRGB) ||
        !glIsEnabled(GL_TEXTURE_3D) || glGetError() != GL_NO_ERROR)
        return fail(24, "legacy specific mask restore");
    glGetBooleanv(GL_LINE_SMOOTH, &enabled);
    if (enabled != GL_TRUE || glGetError() != GL_NO_ERROR)
        return fail(25, "legacy boolean query");
    glGetIntegerv(GL_FRAMEBUFFER_SRGB, &iv);
    if (iv != 1 || glGetError() != GL_NO_ERROR)
        return fail(26, "sRGB integer query");
    glActiveTexture(GL_TEXTURE1);
    if (glIsEnabled(GL_TEXTURE_3D) || glGetError() != GL_NO_ERROR)
        return fail(27, "texture unit independent");
    glEnable(GL_TEXTURE_3D);
    glActiveTexture(GL_TEXTURE0);
    if (!glIsEnabled(GL_TEXTURE_3D) || glGetError() != GL_NO_ERROR)
        return fail(28, "texture unit zero preserved");
    glActiveTexture(GL_TEXTURE1);
    glDisable(GL_TEXTURE_3D);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_TEXTURE_3D);
    if (glGetError() != GL_NO_ERROR)
        return fail(29, "legacy final disable");

    /* Desktop-only fixed-function switches are not legal GLES enables: they
     * must be held by the interface and must not raise GL_INVALID_ENUM. */
    if (glIsEnabled(GL_COLOR_SUM) || glIsEnabled(GL_TEXTURE_CUBE_MAP) ||
        glGetError() != GL_NO_ERROR)
        return fail(30, "initial color sum and cube map");
    glDisable(GL_COLOR_SUM);
    glDisable(GL_TEXTURE_CUBE_MAP);
    if (glGetError() != GL_NO_ERROR)
        return fail(31, "valid initially disabled switches");
    glEnable(GL_COLOR_SUM);
    glEnable(GL_TEXTURE_CUBE_MAP);
    if (!glIsEnabled(GL_COLOR_SUM) || !glIsEnabled(GL_TEXTURE_CUBE_MAP) ||
        glGetError() != GL_NO_ERROR)
        return fail(32, "legacy color sum/cube enables");
    glGetBooleanv(GL_COLOR_SUM, &enabled);
    if (enabled != GL_TRUE || glGetError() != GL_NO_ERROR)
        return fail(33, "color sum boolean query");
    glGetIntegerv(GL_TEXTURE_CUBE_MAP, &iv);
    if (iv != 1 || glGetError() != GL_NO_ERROR)
        return fail(34, "cube map integer query");
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_COLOR_SUM);
    glDisable(GL_TEXTURE_CUBE_MAP);
    if (glIsEnabled(GL_COLOR_SUM) || glIsEnabled(GL_TEXTURE_CUBE_MAP) ||
        glGetError() != GL_NO_ERROR)
        return fail(35, "color sum/cube disable");
    glPopAttrib();
    if (!glIsEnabled(GL_COLOR_SUM) || !glIsEnabled(GL_TEXTURE_CUBE_MAP) ||
        glGetError() != GL_NO_ERROR)
        return fail(36, "enable attrib restoration");
    glPushAttrib(GL_TEXTURE_BIT);
    glDisable(GL_TEXTURE_CUBE_MAP);
    glPopAttrib();
    if (!glIsEnabled(GL_TEXTURE_CUBE_MAP) || glGetError() != GL_NO_ERROR)
        return fail(37, "texture attrib cube restoration");
    glActiveTexture(GL_TEXTURE1);
    if (glIsEnabled(GL_TEXTURE_CUBE_MAP) || !glIsEnabled(GL_COLOR_SUM) ||
        glGetError() != GL_NO_ERROR)
        return fail(38, "cube state per unit, sum global");
    glEnableIndexedEXT(GL_TEXTURE_CUBE_MAP, 1);
    if (!glIsEnabled(GL_TEXTURE_CUBE_MAP) || !glIsEnabledIndexedEXT(GL_TEXTURE_CUBE_MAP, 1) ||
        glGetError() != GL_NO_ERROR)
        return fail(39, "indexed cube enable");
    glDisableIndexedEXT(GL_TEXTURE_CUBE_MAP, 1);
    if (glIsEnabled(GL_TEXTURE_CUBE_MAP) || glGetError() != GL_NO_ERROR)
        return fail(40, "indexed cube disable");
    glActiveTexture(GL_TEXTURE0);
    if (!glIsEnabled(GL_TEXTURE_CUBE_MAP) || glGetError() != GL_NO_ERROR)
        return fail(41, "original cube unit remains enabled");
    glDisable(GL_COLOR_SUM);
    glDisable(GL_TEXTURE_CUBE_MAP);
    if (glGetError() != GL_NO_ERROR)
        return fail(42, "final legacy disable");

    /* Desktop compatibility enables must be queryable and preserved across
     * the appropriate attribute groups, not sent as GLES enable enums. */
    if (glIsEnabled(GL_POLYGON_OFFSET_POINT) || glIsEnabled(GL_POLYGON_OFFSET_LINE) ||
        glIsEnabled(GL_TEXTURE_RECTANGLE) || glGetError() != GL_NO_ERROR)
        return fail(50, "new legacy switches initially disabled");
    glDisable(GL_POLYGON_OFFSET_POINT);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glDisable(GL_TEXTURE_RECTANGLE);
    if (glGetError() != GL_NO_ERROR)
        return fail(51, "valid legacy disables");
    glEnable(GL_POLYGON_OFFSET_POINT);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glEnable(GL_TEXTURE_RECTANGLE);
    if (!glIsEnabled(GL_POLYGON_OFFSET_POINT) || !glIsEnabled(GL_POLYGON_OFFSET_LINE) ||
        !glIsEnabled(GL_TEXTURE_RECTANGLE) || glGetError() != GL_NO_ERROR)
        return fail(52, "new legacy enables");
    glGetBooleanv(GL_POLYGON_OFFSET_POINT, &enabled);
    if (enabled != GL_TRUE || glGetError() != GL_NO_ERROR)
        return fail(53, "polygon point boolean query");
    glGetIntegerv(GL_POLYGON_OFFSET_LINE, &iv);
    if (iv != 1 || glGetError() != GL_NO_ERROR)
        return fail(54, "polygon line integer query");
    glGetIntegerv(GL_TEXTURE_RECTANGLE, &iv);
    if (iv != 1 || glGetError() != GL_NO_ERROR)
        return fail(55, "rectangle integer query");
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_POLYGON_OFFSET_POINT);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glDisable(GL_TEXTURE_RECTANGLE);
    if (glIsEnabled(GL_POLYGON_OFFSET_POINT) || glIsEnabled(GL_POLYGON_OFFSET_LINE) ||
        glIsEnabled(GL_TEXTURE_RECTANGLE))
        return fail(56, "enable group disabled");
    glPopAttrib();
    if (!glIsEnabled(GL_POLYGON_OFFSET_POINT) || !glIsEnabled(GL_POLYGON_OFFSET_LINE) ||
        !glIsEnabled(GL_TEXTURE_RECTANGLE) || glGetError() != GL_NO_ERROR)
        return fail(57, "enable group restored");
    glPushAttrib(GL_POLYGON_BIT | GL_TEXTURE_BIT);
    glDisable(GL_POLYGON_OFFSET_POINT);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glDisable(GL_TEXTURE_RECTANGLE);
    glPopAttrib();
    if (!glIsEnabled(GL_POLYGON_OFFSET_POINT) || !glIsEnabled(GL_POLYGON_OFFSET_LINE) ||
        !glIsEnabled(GL_TEXTURE_RECTANGLE) || glGetError() != GL_NO_ERROR)
        return fail(58, "polygon texture groups restored");
    glActiveTexture(GL_TEXTURE1);
    if (glIsEnabled(GL_TEXTURE_RECTANGLE) || !glIsEnabled(GL_POLYGON_OFFSET_POINT) ||
        glGetError() != GL_NO_ERROR)
        return fail(59, "rectangle per unit / polygon global");
    glEnableIndexedEXT(GL_TEXTURE_RECTANGLE, 1);
    if (!glIsEnabled(GL_TEXTURE_RECTANGLE) || !glIsEnabledIndexedEXT(GL_TEXTURE_RECTANGLE, 1) ||
        glGetError() != GL_NO_ERROR)
        return fail(60, "indexed rectangle enable");
    glDisableIndexedEXT(GL_TEXTURE_RECTANGLE, 1);
    if (glIsEnabled(GL_TEXTURE_RECTANGLE) || glGetError() != GL_NO_ERROR)
        return fail(61, "indexed rectangle disable");
    glActiveTexture(GL_TEXTURE0);
    if (!glIsEnabled(GL_TEXTURE_RECTANGLE) || glGetError() != GL_NO_ERROR)
        return fail(62, "rectangle unit zero unchanged");
    glDisable(GL_POLYGON_OFFSET_POINT);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glDisable(GL_TEXTURE_RECTANGLE);
    if (glGetError() != GL_NO_ERROR)
        return fail(63, "final new legacy disables");

    glVertexPointer(3, GL_FLOAT, 0, client_a);
    glEnableClientState(GL_VERTEX_ARRAY);
    glIndexPointer(GL_FLOAT, 0, index_a);
    glEnableClientState(GL_INDEX_ARRAY);
    glPixelStorei(GL_PACK_ALIGNMENT, 8);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT | GL_CLIENT_PIXEL_STORE_BIT);
    glVertexPointer(3, GL_FLOAT, 0, client_b);
    glDisableClientState(GL_VERTEX_ARRAY);
    glIndexPointer(GL_SHORT, 2, index_b);
    glDisableClientState(GL_INDEX_ARRAY);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPopClientAttrib();
    if (ctx.current_vao->compat[MGL_COMPAT_SEM_POSITION].data.client_ptr != client_a ||
        !ctx.current_vao->compat[MGL_COMPAT_SEM_POSITION].enabled)
        return fail(11, "client array restore");
    glGetIntegerv(GL_PACK_ALIGNMENT, &iv);
    if (iv != 8)
        return fail(12, "pack restore");
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &iv);
    if (iv != 2)
        return fail(13, "unpack restore");
    if (ctx.current_vao->color_index.data.client_ptr != index_a ||
        ctx.current_vao->color_index.type != GL_FLOAT || ctx.current_vao->color_index.stride != 0 ||
        !ctx.current_vao->color_index.enabled || glGetError() != GL_NO_ERROR)
        return fail(64, "color-index client array restoration");

    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glIndexPointer(GL_SHORT, 2, index_b);
    glDisableClientState(GL_INDEX_ARRAY);
    glPopClientAttrib();
    if (ctx.current_vao->color_index.data.client_ptr != index_b ||
        ctx.current_vao->color_index.type != GL_SHORT || ctx.current_vao->color_index.stride != 2 ||
        ctx.current_vao->color_index.enabled || glGetError() != GL_NO_ERROR)
        return fail(65, "pixel-store-only client stack changed color-index array");

    glFogf(GL_FOG_DENSITY, 0.25f);
    glPixelTransferf(GL_RED_BIAS, 0.1f);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glPixelTransferf(GL_RED_BIAS, 0.6f);
    glPushAttrib(GL_FOG_BIT);
    glFogf(GL_FOG_DENSITY, 0.75f);
    glPopAttrib();
    glEndList();
    glGetFloatv(GL_FOG_DENSITY, &fv);
    if (!closef(fv, 0.25f))
        return fail(14, "list compile executed push/pop state");
    glGetFloatv(GL_RED_BIAS, &fv);
    if (!closef(fv, 0.1f))
        return fail(15, "list compile executed pixel transfer");
    glCallList(list);
    glGetFloatv(GL_FOG_DENSITY, &fv);
    if (!closef(fv, 0.25f))
        return fail(16, "list push/pop replay restore");
    glGetFloatv(GL_RED_BIAS, &fv);
    if (!closef(fv, 0.6f))
        return fail(17, "list pixel-transfer replay");

    glGetIntegerv(GL_ATTRIB_STACK_DEPTH, &iv);
    if (iv != 0)
        return fail(18, "attrib depth query");
    glPopAttrib();
    if (glGetError() != GL_STACK_UNDERFLOW)
        return fail(19, "attrib underflow");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL attribute stack unit test: PASS");
    return 0;
}

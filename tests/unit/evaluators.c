#include "mgl/gl.h"
#include "compat/evaluator.h"
#include "compat/fixed_function.h"
#include "compat/selection.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "shader/translator.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(int n, const char *message) {
    fprintf(stderr, "evaluator failure %d: %s\n", n, message);
    return n;
}

static int nearf(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 0.0005f;
}

static int neard(GLdouble a, GLdouble b) {
    return fabs(a - b) < 0.000001;
}

int main(void) {
    struct mgl_context ctx;
    GLdouble curve[12] = {0.0, 0.0, 0.0, 1.0, 2.0, 0.0, 2.0, 0.0, 0.0, 3.0, 3.0, 0.0};
    GLdouble queried[32] = {0};
    GLfloat color_map[8] = {1, 0, 0, 1, 0, 0, 1, 1};
    GLdouble surface[12] = {0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1};
    GLuint list;
    GLfloat domain[4] = {0};
    GLint segs[2] = {0};
    GLfloat queried_f[32] = {0};
    GLint queried_i[4] = {0};
    GLuint selectbuf[64] = {0};
    GLdouble coord1d[1] = {0.25};
    GLfloat coord1f[1] = {0.75f};
    GLdouble coord2d[2] = {0.25, 0.75};
    GLfloat coord2f[2] = {0.75f, 0.25f};
    GLint hits;

    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 320, 240);
    mgl_compat_state_init(&ctx.compat);
    mgl_evaluator_init(&ctx.evaluator);
    mgl_selection_init(&ctx.selection);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.compat_current[MGL_COMPAT_SEM_COLOR][0] = ctx.compat_current[MGL_COMPAT_SEM_COLOR][1] =
        ctx.compat_current[MGL_COMPAT_SEM_COLOR][2] = ctx.compat_current[MGL_COMPAT_SEM_COLOR][3] =
            1.0f;
    ctx.compat_current[MGL_COMPAT_SEM_NORMAL][2] = 1.0f;
    mgl_context_set_current_for_test(&ctx);

    glMap1d(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 4, curve);
    glEnable(GL_MAP1_VERTEX_3);
    if (!glIsEnabled(GL_MAP1_VERTEX_3))
        return fail(2, "map1 enable");
    glGetMapdv(GL_MAP1_VERTEX_3, GL_ORDER, queried);
    if (!neard(queried[0], 4.0))
        return fail(3, "map1 order query");
    memset(queried, 0, sizeof(queried));
    glGetMapdv(GL_MAP1_VERTEX_3, GL_COEFF, queried);
    if (!neard(queried[3], 1.0) || !neard(queried[10], 3.0))
        return fail(4, "map1 coefficient ownership/query");

    glBegin(GL_POINTS);
    glEvalCoord1d(0.5);
    if (ctx.immediate.count != 1u)
        return fail(5, "evalcoord1 vertex emission");
    if (!nearf(ctx.immediate.vertices[0].position[0], 1.5f) ||
        !nearf(ctx.immediate.vertices[0].position[1], 1.125f))
        return fail(6, "bezier curve evaluation");
    ctx.immediate.active = GL_FALSE;
    ctx.immediate.count = 0u;

    glBegin(GL_POINTS);
    glEvalCoord1dv(coord1d);
    if (ctx.immediate.count != 1u || !nearf(ctx.immediate.vertices[0].position[0], 0.75f) ||
        !nearf(ctx.immediate.vertices[0].position[1], 0.890625f))
        return fail(16, "evalcoord1dv exact entry");
    ctx.immediate.active = GL_FALSE;
    ctx.immediate.count = 0u;
    glBegin(GL_POINTS);
    glEvalCoord1fv(coord1f);
    if (ctx.immediate.count != 1u || !nearf(ctx.immediate.vertices[0].position[0], 2.25f) ||
        !nearf(ctx.immediate.vertices[0].position[1], 1.546875f))
        return fail(17, "evalcoord1fv exact entry");
    ctx.immediate.active = GL_FALSE;
    ctx.immediate.count = 0u;

    glMap1f(GL_MAP1_COLOR_4, 0.0f, 1.0f, 4, 2, color_map);
    glEnable(GL_MAP1_COLOR_4);
    glBegin(GL_POINTS);
    glEvalCoord1f(0.25f);
    if (!nearf(ctx.immediate.vertices[0].color[0], 0.75f) ||
        !nearf(ctx.immediate.vertices[0].color[2], 0.25f))
        return fail(7, "color evaluator");
    ctx.immediate.active = GL_FALSE;
    ctx.immediate.count = 0u;

    glMap2d(GL_MAP2_VERTEX_3, 0.0, 1.0, 6, 2, 0.0, 1.0, 3, 2, surface);
    glEnable(GL_MAP2_VERTEX_3);
    glEnable(GL_AUTO_NORMAL);
    glBegin(GL_POINTS);
    glEvalCoord2d(0.5, 0.5);
    if (ctx.immediate.count != 1u || !nearf(ctx.immediate.vertices[0].position[2], 0.25f))
        return fail(8, "surface evaluation");
    if (fabsf(ctx.immediate.vertices[0].normal[0]) + fabsf(ctx.immediate.vertices[0].normal[1]) +
            fabsf(ctx.immediate.vertices[0].normal[2]) <
        0.5f)
        return fail(9, "auto normal");
    ctx.immediate.active = GL_FALSE;
    ctx.immediate.count = 0u;

    glBegin(GL_POINTS);
    glEvalCoord2f(0.25f, 0.75f);
    if (ctx.immediate.count != 1u || !nearf(ctx.immediate.vertices[0].position[0], 0.25f) ||
        !nearf(ctx.immediate.vertices[0].position[1], 0.75f) ||
        !nearf(ctx.immediate.vertices[0].position[2], 0.1875f))
        return fail(18, "evalcoord2f exact entry");
    ctx.immediate.active = GL_FALSE;
    ctx.immediate.count = 0u;
    glBegin(GL_POINTS);
    glEvalCoord2dv(coord2d);
    if (ctx.immediate.count != 1u || !nearf(ctx.immediate.vertices[0].position[0], 0.25f) ||
        !nearf(ctx.immediate.vertices[0].position[1], 0.75f) ||
        !nearf(ctx.immediate.vertices[0].position[2], 0.1875f))
        return fail(19, "evalcoord2dv exact entry");
    ctx.immediate.active = GL_FALSE;
    ctx.immediate.count = 0u;
    glBegin(GL_POINTS);
    glEvalCoord2fv(coord2f);
    if (ctx.immediate.count != 1u || !nearf(ctx.immediate.vertices[0].position[0], 0.75f) ||
        !nearf(ctx.immediate.vertices[0].position[1], 0.25f) ||
        !nearf(ctx.immediate.vertices[0].position[2], 0.1875f))
        return fail(20, "evalcoord2fv exact entry");
    ctx.immediate.active = GL_FALSE;
    ctx.immediate.count = 0u;

    {
        GLfloat surface_f[12] = {0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1};
        glMap2f(GL_MAP2_VERTEX_3, 0.0f, 1.0f, 6, 2, 0.0f, 1.0f, 3, 2, surface_f);
        if (glGetError() != GL_NO_ERROR)
            return fail(21, "map2f exact entry");
        glBegin(GL_POINTS);
        glEvalCoord2f(0.5f, 0.5f);
        if (ctx.immediate.count != 1u || !nearf(ctx.immediate.vertices[0].position[2], 0.25f))
            return fail(22, "map2f evaluation");
        ctx.immediate.active = GL_FALSE;
        ctx.immediate.count = 0u;
    }

    memset(queried_f, 0, sizeof(queried_f));
    glGetMapfv(GL_MAP2_VERTEX_3, GL_DOMAIN, queried_f);
    if (glGetError() != GL_NO_ERROR || !nearf(queried_f[0], 0.0f) || !nearf(queried_f[1], 1.0f) ||
        !nearf(queried_f[2], 0.0f) || !nearf(queried_f[3], 1.0f))
        return fail(23, "getmapfv exact entry");
    memset(queried_i, 0, sizeof(queried_i));
    glGetMapiv(GL_MAP2_VERTEX_3, GL_ORDER, queried_i);
    if (glGetError() != GL_NO_ERROR || queried_i[0] != 2 || queried_i[1] != 2)
        return fail(24, "getmapiv exact entry");

    glIndexf(7.25f);
    if (!nearf(ctx.evaluator.current_index, 7.25f))
        return fail(25, "indexf exact entry");
    {
        const GLfloat index_value[1] = {3.5f};
        glIndexfv(index_value);
        if (!nearf(ctx.evaluator.current_index, 3.5f))
            return fail(35, "indexfv exact entry");
    }

    glMapGrid2d(4, -1.0, 1.0, 5, 2.0, 4.0);
    glGetFloatv(GL_MAP2_GRID_DOMAIN, domain);
    glGetIntegerv(GL_MAP2_GRID_SEGMENTS, segs);
    if (!nearf(domain[0], -1.0f) || !nearf(domain[3], 4.0f) || segs[0] != 4 || segs[1] != 5)
        return fail(10, "grid queries");

    /* EvalPoint must use the grid that is current when a display list replays. */
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glEvalPoint2(2, 2);
    glEndList();
    glMapGrid2d(4, 0.0, 1.0, 4, 0.0, 1.0);
    glBegin(GL_POINTS);
    glCallList(list);
    if (ctx.immediate.count != 1u || !nearf(ctx.immediate.vertices[0].position[0], 0.5f) ||
        !nearf(ctx.immediate.vertices[0].position[1], 0.5f))
        return fail(11, "late grid evalpoint replay");
    ctx.immediate.active = GL_FALSE;
    ctx.immediate.count = 0u;

    /* A map definition in GL_COMPILE owns its control points and executes only on replay. */
    {
        GLdouble owned[6] = {10, 0, 0, 20, 0, 0};
        GLuint map_list = glGenLists(1);
        glNewList(map_list, GL_COMPILE);
        glMap1d(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 2, owned);
        glEndList();
        owned[0] = 99.0;
        owned[3] = 99.0;
        glCallList(map_list);
        memset(queried, 0, sizeof(queried));
        glGetMapdv(GL_MAP1_VERTEX_3, GL_COEFF, queried);
        if (!neard(queried[0], 10.0) || !neard(queried[3], 20.0))
            return fail(12, "display-list map payload ownership");
        glDeleteLists(map_list, 1);
    }

    glMapGrid1f(8, 0.0f, 1.0f);
    glGetIntegerv(GL_MAP1_GRID_SEGMENTS, segs);
    if (segs[0] != 8)
        return fail(13, "grid1 segments");

    glMap1d(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 4, curve);
    glMapGrid1d(4, 0.0, 1.0);
    glGetIntegerv(GL_MAP1_GRID_SEGMENTS, segs);
    glGetFloatv(GL_MAP1_GRID_DOMAIN, domain);
    if (glGetError() != GL_NO_ERROR || segs[0] != 4 || !nearf(domain[0], 0.0f) ||
        !nearf(domain[1], 1.0f))
        return fail(26, "mapgrid1d exact entry");
    glBegin(GL_POINTS);
    glEvalPoint1(2);
    if (ctx.immediate.count != 1u || !nearf(ctx.immediate.vertices[0].position[0], 1.5f) ||
        !nearf(ctx.immediate.vertices[0].position[1], 1.125f))
        return fail(27, "evalpoint1 exact entry");
    ctx.immediate.active = GL_FALSE;
    ctx.immediate.count = 0u;

    {
        GLdouble mesh_curve[6] = {-0.5, 0.0, 0.0, 0.5, 0.0, 0.0};
        GLdouble mesh_surface[12] = {-0.5, -0.5, 0.0, -0.5, 0.5, 0.0,
                                     0.5,  -0.5, 0.0, 0.5,  0.5, 0.0};
        glMap1d(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 2, mesh_curve);
        glMapGrid1d(2, 0.0, 1.0);
        glSelectBuffer(64, selectbuf);
        glRenderMode(GL_SELECT);
        glInitNames();
        glPushName(1u);
        glEvalMesh1(GL_POINT, 0, 2);
        hits = glRenderMode(GL_RENDER);
        if (glGetError() != GL_NO_ERROR || hits != 1)
            return fail(28, "evalmesh1 point execution");
        memset(selectbuf, 0, sizeof(selectbuf));
        glSelectBuffer(64, selectbuf);
        glRenderMode(GL_SELECT);
        glInitNames();
        glPushName(2u);
        glEvalMesh1(GL_LINE, 0, 2);
        hits = glRenderMode(GL_RENDER);
        if (glGetError() != GL_NO_ERROR || hits != 1)
            return fail(29, "evalmesh1 line execution");
        glEvalMesh1(GL_FILL, 0, 2);
        if (glGetError() != GL_INVALID_ENUM)
            return fail(30, "evalmesh1 mode validation");

        glMap2d(GL_MAP2_VERTEX_3, 0.0, 1.0, 6, 2, 0.0, 1.0, 3, 2, mesh_surface);
        glMapGrid2d(1, 0.0, 1.0, 1, 0.0, 1.0);
        glSelectBuffer(64, selectbuf);
        glRenderMode(GL_SELECT);
        glInitNames();
        glPushName(3u);
        glEvalMesh2(GL_POINT, 0, 1, 0, 1);
        hits = glRenderMode(GL_RENDER);
        if (glGetError() != GL_NO_ERROR || hits != 1)
            return fail(31, "evalmesh2 point execution");
        memset(selectbuf, 0, sizeof(selectbuf));
        glSelectBuffer(64, selectbuf);
        glRenderMode(GL_SELECT);
        glInitNames();
        glPushName(4u);
        glEvalMesh2(GL_LINE, 0, 1, 0, 1);
        hits = glRenderMode(GL_RENDER);
        if (glGetError() != GL_NO_ERROR || hits != 1)
            return fail(32, "evalmesh2 line execution");
        memset(selectbuf, 0, sizeof(selectbuf));
        glSelectBuffer(64, selectbuf);
        glRenderMode(GL_SELECT);
        glInitNames();
        glPushName(5u);
        glEvalMesh2(GL_FILL, 0, 1, 0, 1);
        hits = glRenderMode(GL_RENDER);
        if (glGetError() != GL_NO_ERROR || hits != 1)
            return fail(33, "evalmesh2 fill execution");
        glEvalMesh2(0xDEADu, 0, 1, 0, 1);
        if (glGetError() != GL_INVALID_ENUM)
            return fail(34, "evalmesh2 mode validation");
    }

    glMap1f(GL_MAP1_VERTEX_3, 0.0f, 1.0f, 2, 2, (GLfloat[]){0, 0, 1, 1});
    if (glGetError() != GL_INVALID_VALUE)
        return fail(15, "invalid evaluator stride");

    glDeleteLists(list, 1);
    mgl_evaluator_destroy(&ctx.evaluator);
    free(ctx.immediate.vertices);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL evaluator unit test: PASS");
    return 0;
}

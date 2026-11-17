#include "compat/evaluator.h"

#include "core/context.h"
#include "list/display_list.h"
#include "mgl/gl.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int map_info(GLenum target, int dimension, unsigned *slot, unsigned *components) {
    static const unsigned widths[MGL_EVAL_MAP_COUNT] = {4u, 1u, 3u, 1u, 2u, 3u, 4u, 3u, 4u};
    GLenum base = dimension == 1 ? GL_MAP1_COLOR_4 : GL_MAP2_COLOR_4;
    if (dimension != 1 && dimension != 2)
        return 0;
    if (target < base || target >= base + MGL_EVAL_MAP_COUNT)
        return 0;
    *slot = (unsigned)(target - base);
    *components = widths[*slot];
    return 1;
}

void mgl_evaluator_init(struct mgl_evaluator_state *state) {
    memset(state, 0, sizeof(*state));
    state->grid1_segments = 1;
    state->grid1_u1 = 0.0;
    state->grid1_u2 = 1.0;
    state->grid2_u_segments = 1;
    state->grid2_v_segments = 1;
    state->grid2_u1 = 0.0;
    state->grid2_u2 = 1.0;
    state->grid2_v1 = 0.0;
    state->grid2_v2 = 1.0;
    state->current_index = 1.0f;
}

void mgl_evaluator_destroy(struct mgl_evaluator_state *state) {
    unsigned i;
    if (state == NULL)
        return;
    for (i = 0u; i < MGL_EVAL_MAP_COUNT; ++i) {
        free(state->map1[i].coeff);
        free(state->map2[i].coeff);
    }
    memset(state, 0, sizeof(*state));
}

GLboolean *mgl_evaluator_enable_ptr(struct mgl_context *ctx, GLenum cap) {
    unsigned slot, components;
    if (ctx == NULL)
        return NULL;
    if (cap == GL_AUTO_NORMAL)
        return &ctx->evaluator.auto_normal;
    if (map_info(cap, 1, &slot, &components))
        return &ctx->evaluator.map1[slot].enabled;
    if (map_info(cap, 2, &slot, &components))
        return &ctx->evaluator.map2[slot].enabled;
    return NULL;
}

static int validate_map_common(struct mgl_context *ctx, GLenum target, int dimension, GLdouble u1,
                               GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2,
                               GLint vstride, GLint vorder, const void *points, unsigned *slot,
                               unsigned *components) {
    if (ctx->immediate.active && (ctx->compiling_list == NULL || ctx->list_replaying)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (!map_info(target, dimension, slot, components)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (points == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (u1 == u2 || ustride < (GLint)*components || uorder < 1 ||
        uorder > (GLint)MGL_MAX_EVAL_ORDER) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (dimension == 2 && (v1 == v2 || vstride < (GLint)*components || vorder < 1 ||
                           vorder > (GLint)MGL_MAX_EVAL_ORDER)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    return 1;
}

static void install_map(struct mgl_eval_map *map, unsigned components, GLint uorder, GLint vorder,
                        GLdouble u1, GLdouble u2, GLdouble v1, GLdouble v2, GLdouble *coeff) {
    free(map->coeff);
    map->defined = GL_TRUE;
    map->components = components;
    map->uorder = uorder;
    map->vorder = vorder;
    map->u1 = u1;
    map->u2 = u2;
    map->v1 = v1;
    map->v2 = v2;
    map->coeff = coeff;
}

static void map1_double(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order,
                        const GLdouble *points) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned slot, components, i, c;
    GLdouble *coeff;
    size_t count;
    if (ctx == NULL)
        return;
    if (!validate_map_common(ctx, target, 1, u1, u2, stride, order, 0.0, 1.0, 1, 1, points, &slot,
                             &components))
        return;
    count = (size_t)order * components;
    coeff = malloc(count * sizeof(*coeff));
    if (coeff == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    for (i = 0u; i < (unsigned)order; ++i)
        for (c = 0u; c < components; ++c)
            coeff[(size_t)i * components + c] = points[(size_t)i * (size_t)stride + c];
    if (mgl_list_record_map1(ctx, target, u1, u2, order, components, coeff)) {
        free(coeff);
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        free(coeff);
        return;
    }
    install_map(&ctx->evaluator.map1[slot], components, order, 1, u1, u2, 0.0, 1.0, coeff);
}

static void map1_float(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order,
                       const GLfloat *points) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned slot, components, i, c;
    GLdouble *coeff;
    size_t count;
    if (ctx == NULL)
        return;
    if (!validate_map_common(ctx, target, 1, (GLdouble)u1, (GLdouble)u2, stride, order, 0.0, 1.0, 1,
                             1, points, &slot, &components))
        return;
    count = (size_t)order * components;
    coeff = malloc(count * sizeof(*coeff));
    if (coeff == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    for (i = 0u; i < (unsigned)order; ++i)
        for (c = 0u; c < components; ++c)
            coeff[(size_t)i * components + c] = (GLdouble)points[(size_t)i * (size_t)stride + c];
    if (mgl_list_record_map1(ctx, target, (GLdouble)u1, (GLdouble)u2, order, components, coeff)) {
        free(coeff);
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        free(coeff);
        return;
    }
    install_map(&ctx->evaluator.map1[slot], components, order, 1, (GLdouble)u1, (GLdouble)u2, 0.0,
                1.0, coeff);
}

void glMap1d(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order,
             const GLdouble *points) {
    map1_double(target, u1, u2, stride, order, points);
}

void glMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order,
             const GLfloat *points) {
    map1_float(target, u1, u2, stride, order, points);
}

static void map2_double(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder,
                        GLdouble v1, GLdouble v2, GLint vstride, GLint vorder,
                        const GLdouble *points) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned slot, components, u, v, c;
    GLdouble *coeff;
    size_t count;
    if (ctx == NULL)
        return;
    if (!validate_map_common(ctx, target, 2, u1, u2, ustride, uorder, v1, v2, vstride, vorder,
                             points, &slot, &components))
        return;
    count = (size_t)uorder * (size_t)vorder * components;
    coeff = malloc(count * sizeof(*coeff));
    if (coeff == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    for (u = 0u; u < (unsigned)uorder; ++u)
        for (v = 0u; v < (unsigned)vorder; ++v)
            for (c = 0u; c < components; ++c)
                coeff[((size_t)u * (size_t)vorder + v) * components + c] =
                    points[(size_t)u * (size_t)ustride + (size_t)v * (size_t)vstride + c];
    if (mgl_list_record_map2(ctx, target, u1, u2, v1, v2, uorder, vorder, components, coeff)) {
        free(coeff);
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        free(coeff);
        return;
    }
    install_map(&ctx->evaluator.map2[slot], components, uorder, vorder, u1, u2, v1, v2, coeff);
}

static void map2_float(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder,
                       GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned slot, components, u, v, c;
    GLdouble *coeff;
    size_t count;
    if (ctx == NULL)
        return;
    if (!validate_map_common(ctx, target, 2, (GLdouble)u1, (GLdouble)u2, ustride, uorder,
                             (GLdouble)v1, (GLdouble)v2, vstride, vorder, points, &slot,
                             &components))
        return;
    count = (size_t)uorder * (size_t)vorder * components;
    coeff = malloc(count * sizeof(*coeff));
    if (coeff == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    for (u = 0u; u < (unsigned)uorder; ++u)
        for (v = 0u; v < (unsigned)vorder; ++v)
            for (c = 0u; c < components; ++c)
                coeff[((size_t)u * (size_t)vorder + v) * components + c] =
                    (GLdouble)points[(size_t)u * (size_t)ustride + (size_t)v * (size_t)vstride + c];
    if (mgl_list_record_map2(ctx, target, (GLdouble)u1, (GLdouble)u2, (GLdouble)v1, (GLdouble)v2,
                             uorder, vorder, components, coeff)) {
        free(coeff);
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        free(coeff);
        return;
    }
    install_map(&ctx->evaluator.map2[slot], components, uorder, vorder, (GLdouble)u1, (GLdouble)u2,
                (GLdouble)v1, (GLdouble)v2, coeff);
}

void glMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1,
             GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points) {
    map2_double(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

void glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1,
             GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points) {
    map2_float(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

static void eval_curve(const GLdouble *coeff, GLint order, unsigned components, GLdouble t,
                       GLdouble out[4]) {
    GLdouble work[MGL_MAX_EVAL_ORDER][4];
    GLint i, level;
    unsigned c;
    memset(out, 0, 4u * sizeof(*out));
    for (i = 0; i < order; ++i)
        for (c = 0u; c < components; ++c)
            work[i][c] = coeff[(size_t)i * components + c];
    for (level = 1; level < order; ++level)
        for (i = 0; i < order - level; ++i)
            for (c = 0u; c < components; ++c)
                work[i][c] = (1.0 - t) * work[i][c] + t * work[i + 1][c];
    for (c = 0u; c < components; ++c)
        out[c] = work[0][c];
}

static void eval_curve_derivative(const GLdouble *coeff, GLint order, unsigned components,
                                  GLdouble t, GLdouble scale, GLdouble out[4]) {
    GLdouble derivative[MGL_MAX_EVAL_ORDER * 4u];
    GLint i;
    unsigned c;
    if (order <= 1) {
        memset(out, 0, 4u * sizeof(*out));
        return;
    }
    for (i = 0; i < order - 1; ++i)
        for (c = 0u; c < components; ++c)
            derivative[(size_t)i * components + c] =
                (GLdouble)(order - 1) *
                (coeff[(size_t)(i + 1) * components + c] - coeff[(size_t)i * components + c]) *
                scale;
    eval_curve(derivative, order - 1, components, t, out);
}

static void eval_surface(const struct mgl_eval_map *map, GLdouble u, GLdouble v, GLdouble out[4]) {
    GLdouble rows[MGL_MAX_EVAL_ORDER * 4u];
    GLdouble tu = (u - map->u1) / (map->u2 - map->u1);
    GLdouble tv = (v - map->v1) / (map->v2 - map->v1);
    GLint i;
    for (i = 0; i < map->uorder; ++i)
        eval_curve(map->coeff + (size_t)i * (size_t)map->vorder * map->components, map->vorder,
                   map->components, tv, rows + (size_t)i * map->components);
    eval_curve(rows, map->uorder, map->components, tu, out);
}

static void eval_surface_derivatives(const struct mgl_eval_map *map, GLdouble u, GLdouble v,
                                     GLdouble du[4], GLdouble dv[4]) {
    GLdouble rows[MGL_MAX_EVAL_ORDER * 4u];
    GLdouble drows[MGL_MAX_EVAL_ORDER * 4u];
    GLdouble tu = (u - map->u1) / (map->u2 - map->u1);
    GLdouble tv = (v - map->v1) / (map->v2 - map->v1);
    GLint i;
    for (i = 0; i < map->uorder; ++i) {
        const GLdouble *row = map->coeff + (size_t)i * (size_t)map->vorder * map->components;
        eval_curve(row, map->vorder, map->components, tv, rows + (size_t)i * map->components);
        eval_curve_derivative(row, map->vorder, map->components, tv, 1.0 / (map->v2 - map->v1),
                              drows + (size_t)i * map->components);
    }
    eval_curve_derivative(rows, map->uorder, map->components, tu, 1.0 / (map->u2 - map->u1), du);
    eval_curve(drows, map->uorder, map->components, tu, dv);
}

static void apply_eval_value(unsigned slot, const GLdouble value[4]) {
    switch (slot) {
        case 0u:
            glColor4f((GLfloat)value[0], (GLfloat)value[1], (GLfloat)value[2], (GLfloat)value[3]);
            break;
        case 1u: {
            struct mgl_context *ctx = mgl_context_current();
            if (ctx != NULL)
                ctx->evaluator.current_index = (GLfloat)value[0];
            break;
        }
        case 2u:
            glNormal3f((GLfloat)value[0], (GLfloat)value[1], (GLfloat)value[2]);
            break;
        case 3u:
            glTexCoord4f((GLfloat)value[0], 0.0f, 0.0f, 1.0f);
            break;
        case 4u:
            glTexCoord4f((GLfloat)value[0], (GLfloat)value[1], 0.0f, 1.0f);
            break;
        case 5u:
            glTexCoord4f((GLfloat)value[0], (GLfloat)value[1], (GLfloat)value[2], 1.0f);
            break;
        case 6u:
            glTexCoord4f((GLfloat)value[0], (GLfloat)value[1], (GLfloat)value[2],
                         (GLfloat)value[3]);
            break;
        default:
            break;
    }
}

static void emit_vertex(unsigned slot, const GLdouble value[4]) {
    if (slot == 7u)
        glVertex3d(value[0], value[1], value[2]);
    else if (slot == 8u)
        glVertex4d(value[0], value[1], value[2], value[3]);
}

static void auto_normal_for_map(const struct mgl_eval_map *map, GLdouble u, GLdouble v) {
    GLdouble du[4], dv[4];
    GLdouble nx, ny, nz, length;
    eval_surface_derivatives(map, u, v, du, dv);
    nx = du[1] * dv[2] - du[2] * dv[1];
    ny = du[2] * dv[0] - du[0] * dv[2];
    nz = du[0] * dv[1] - du[1] * dv[0];
    length = sqrt(nx * nx + ny * ny + nz * nz);
    if (length > 0.0)
        glNormal3f((GLfloat)(nx / length), (GLfloat)(ny / length), (GLfloat)(nz / length));
}

void glEvalCoord1d(GLdouble u) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned slot;
    if (ctx == NULL)
        return;
    if (mgl_list_record_eval_coord1(ctx, u))
        return;
    for (slot = 0u; slot < MGL_EVAL_MAP_COUNT; ++slot) {
        const struct mgl_eval_map *map = &ctx->evaluator.map1[slot];
        GLdouble value[4];
        if (!map->enabled || !map->defined)
            continue;
        eval_curve(map->coeff, map->uorder, map->components, (u - map->u1) / (map->u2 - map->u1),
                   value);
        if (slot < 7u)
            apply_eval_value(slot, value);
    }
    for (slot = 7u; slot < MGL_EVAL_MAP_COUNT; ++slot) {
        const struct mgl_eval_map *map = &ctx->evaluator.map1[slot];
        GLdouble value[4];
        if (!map->enabled || !map->defined)
            continue;
        eval_curve(map->coeff, map->uorder, map->components, (u - map->u1) / (map->u2 - map->u1),
                   value);
        emit_vertex(slot, value);
    }
}

void glEvalCoord1f(GLfloat u) {
    glEvalCoord1d((GLdouble)u);
}

void glEvalCoord1dv(const GLdouble *u) {
    if (u != NULL)
        glEvalCoord1d(*u);
}

void glEvalCoord1fv(const GLfloat *u) {
    if (u != NULL)
        glEvalCoord1f(*u);
}

void glEvalCoord2d(GLdouble u, GLdouble v) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned slot;
    if (ctx == NULL)
        return;
    if (mgl_list_record_eval_coord2(ctx, u, v))
        return;
    for (slot = 0u; slot < 7u; ++slot) {
        const struct mgl_eval_map *map = &ctx->evaluator.map2[slot];
        GLdouble value[4];
        if (!map->enabled || !map->defined)
            continue;
        eval_surface(map, u, v, value);
        apply_eval_value(slot, value);
    }
    if (ctx->evaluator.auto_normal) {
        const struct mgl_eval_map *vertex =
            ctx->evaluator.map2[8].enabled && ctx->evaluator.map2[8].defined
                ? &ctx->evaluator.map2[8]
                : &ctx->evaluator.map2[7];
        if (vertex->enabled && vertex->defined)
            auto_normal_for_map(vertex, u, v);
    }
    for (slot = 7u; slot < MGL_EVAL_MAP_COUNT; ++slot) {
        const struct mgl_eval_map *map = &ctx->evaluator.map2[slot];
        GLdouble value[4];
        if (!map->enabled || !map->defined)
            continue;
        eval_surface(map, u, v, value);
        emit_vertex(slot, value);
    }
}

void glEvalCoord2f(GLfloat u, GLfloat v) {
    glEvalCoord2d((GLdouble)u, (GLdouble)v);
}

void glEvalCoord2dv(const GLdouble *uv) {
    if (uv != NULL)
        glEvalCoord2d(uv[0], uv[1]);
}

void glEvalCoord2fv(const GLfloat *uv) {
    if (uv != NULL)
        glEvalCoord2f(uv[0], uv[1]);
}

void glMapGrid1d(GLint un, GLdouble u1, GLdouble u2) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->compiling_list != NULL && !ctx->list_replaying &&
        ctx->list_compile_mode == GL_COMPILE) {
        if (mgl_list_record_map_grid1(ctx, un, u1, u2))
            return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (un < 1 || u1 == u2) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_map_grid1(ctx, un, u1, u2))
        return;
    ctx->evaluator.grid1_segments = un;
    ctx->evaluator.grid1_u1 = u1;
    ctx->evaluator.grid1_u2 = u2;
}

void glMapGrid1f(GLint un, GLfloat u1, GLfloat u2) {
    glMapGrid1d(un, (GLdouble)u1, (GLdouble)u2);
}

void glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->compiling_list != NULL && !ctx->list_replaying &&
        ctx->list_compile_mode == GL_COMPILE) {
        if (mgl_list_record_map_grid2(ctx, un, u1, u2, vn, v1, v2))
            return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (un < 1 || vn < 1 || u1 == u2 || v1 == v2) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_map_grid2(ctx, un, u1, u2, vn, v1, v2))
        return;
    ctx->evaluator.grid2_u_segments = un;
    ctx->evaluator.grid2_v_segments = vn;
    ctx->evaluator.grid2_u1 = u1;
    ctx->evaluator.grid2_u2 = u2;
    ctx->evaluator.grid2_v1 = v1;
    ctx->evaluator.grid2_v2 = v2;
}

void glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2) {
    glMapGrid2d(un, (GLdouble)u1, (GLdouble)u2, vn, (GLdouble)v1, (GLdouble)v2);
}

void glEvalPoint1(GLint i) {
    struct mgl_context *ctx = mgl_context_current();
    GLdouble u;
    if (ctx == NULL)
        return;
    if (mgl_list_record_eval_point1(ctx, i))
        return;
    u = ctx->evaluator.grid1_u1 + (ctx->evaluator.grid1_u2 - ctx->evaluator.grid1_u1) *
                                      (GLdouble)i / (GLdouble)ctx->evaluator.grid1_segments;
    glEvalCoord1d(u);
}

void glEvalPoint2(GLint i, GLint j) {
    struct mgl_context *ctx = mgl_context_current();
    GLdouble u, v;
    if (ctx == NULL)
        return;
    if (mgl_list_record_eval_point2(ctx, i, j))
        return;
    u = ctx->evaluator.grid2_u1 + (ctx->evaluator.grid2_u2 - ctx->evaluator.grid2_u1) *
                                      (GLdouble)i / (GLdouble)ctx->evaluator.grid2_u_segments;
    v = ctx->evaluator.grid2_v1 + (ctx->evaluator.grid2_v2 - ctx->evaluator.grid2_v1) *
                                      (GLdouble)j / (GLdouble)ctx->evaluator.grid2_v_segments;
    glEvalCoord2d(u, v);
}

void glEvalMesh1(GLenum mode, GLint i1, GLint i2) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glEvalMesh1",
                        (const struct mgl_error_arg[]){{"mode", (uint64_t)(uintptr_t)mode},
                                                       {"i1", (uint64_t)(uintptr_t)i1},
                                                       {"i2", (uint64_t)(uintptr_t)i2}},
                        3u);
    GLint i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mode != GL_POINT && mode != GL_LINE) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_eval_mesh1(ctx, mode, i1, i2))
        return;
    glBegin(mode == GL_POINT ? GL_POINTS : GL_LINE_STRIP);
    for (i = i1; i <= i2; ++i)
        glEvalPoint1(i);
    glEnd();
}

void glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glEvalMesh2",
                        (const struct mgl_error_arg[]){{"mode", (uint64_t)(uintptr_t)mode},
                                                       {"i1", (uint64_t)(uintptr_t)i1},
                                                       {"i2", (uint64_t)(uintptr_t)i2},
                                                       {"j1", (uint64_t)(uintptr_t)j1},
                                                       {"j2", (uint64_t)(uintptr_t)j2}},
                        5u);
    GLint i, j;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mode != GL_POINT && mode != GL_LINE && mode != GL_FILL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (mgl_list_record_eval_mesh2(ctx, mode, i1, i2, j1, j2))
        return;
    if (mode == GL_POINT) {
        glBegin(GL_POINTS);
        for (j = j1; j <= j2; ++j)
            for (i = i1; i <= i2; ++i)
                glEvalPoint2(i, j);
        glEnd();
    } else if (mode == GL_LINE) {
        for (j = j1; j <= j2; ++j) {
            glBegin(GL_LINE_STRIP);
            for (i = i1; i <= i2; ++i)
                glEvalPoint2(i, j);
            glEnd();
        }
        for (i = i1; i <= i2; ++i) {
            glBegin(GL_LINE_STRIP);
            for (j = j1; j <= j2; ++j)
                glEvalPoint2(i, j);
            glEnd();
        }
    } else {
        for (j = j1; j < j2; ++j) {
            glBegin(GL_QUAD_STRIP);
            for (i = i1; i <= i2; ++i) {
                glEvalPoint2(i, j);
                glEvalPoint2(i, j + 1);
            }
            glEnd();
        }
    }
}

static const struct mgl_eval_map *get_map(struct mgl_context *ctx, GLenum target,
                                          unsigned *components) {
    unsigned slot;
    if (map_info(target, 1, &slot, components))
        return &ctx->evaluator.map1[slot];
    if (map_info(target, 2, &slot, components))
        return &ctx->evaluator.map2[slot];
    return NULL;
}

void glGetMapdv(GLenum target, GLenum query, GLdouble *v) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetMapdv",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"query", (uint64_t)(uintptr_t)query},
                                                       {"v", (uint64_t)(uintptr_t)v}},
                        3u);
    const struct mgl_eval_map *map;
    unsigned components;
    size_t count, i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (v == NULL)
        return;
    map = get_map(ctx, target, &components);
    if (map == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (query == GL_COEFF) {
        if (!map->defined)
            return;
        count = (size_t)map->uorder * (size_t)map->vorder * components;
        for (i = 0u; i < count; ++i)
            v[i] = map->coeff[i];
    } else if (query == GL_ORDER) {
        v[0] = (GLdouble)map->uorder;
        if (target >= GL_MAP2_COLOR_4)
            v[1] = (GLdouble)map->vorder;
    } else if (query == GL_DOMAIN) {
        v[0] = map->u1;
        v[1] = map->u2;
        if (target >= GL_MAP2_COLOR_4) {
            v[2] = map->v1;
            v[3] = map->v2;
        }
    } else
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
}

void glGetMapfv(GLenum target, GLenum query, GLfloat *v) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetMapfv",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"query", (uint64_t)(uintptr_t)query},
                                                       {"v", (uint64_t)(uintptr_t)v}},
                        3u);
    const struct mgl_eval_map *map;
    unsigned components;
    size_t count, i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (v == NULL)
        return;
    map = get_map(ctx, target, &components);
    if (map == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (query == GL_COEFF) {
        if (!map->defined)
            return;
        count = (size_t)map->uorder * (size_t)map->vorder * components;
        for (i = 0u; i < count; ++i)
            v[i] = (GLfloat)map->coeff[i];
    } else if (query == GL_ORDER) {
        v[0] = (GLfloat)map->uorder;
        if (target >= GL_MAP2_COLOR_4)
            v[1] = (GLfloat)map->vorder;
    } else if (query == GL_DOMAIN) {
        v[0] = (GLfloat)map->u1;
        v[1] = (GLfloat)map->u2;
        if (target >= GL_MAP2_COLOR_4) {
            v[2] = (GLfloat)map->v1;
            v[3] = (GLfloat)map->v2;
        }
    } else
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
}

void glGetMapiv(GLenum target, GLenum query, GLint *v) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetMapiv",
                        (const struct mgl_error_arg[]){{"target", (uint64_t)(uintptr_t)target},
                                                       {"query", (uint64_t)(uintptr_t)query},
                                                       {"v", (uint64_t)(uintptr_t)v}},
                        3u);
    const struct mgl_eval_map *map;
    unsigned components;
    size_t count, i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (v == NULL)
        return;
    map = get_map(ctx, target, &components);
    if (map == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (query == GL_COEFF) {
        if (!map->defined)
            return;
        count = (size_t)map->uorder * (size_t)map->vorder * components;
        for (i = 0u; i < count; ++i)
            v[i] = (GLint)map->coeff[i];
    } else if (query == GL_ORDER) {
        v[0] = map->uorder;
        if (target >= GL_MAP2_COLOR_4)
            v[1] = map->vorder;
    } else if (query == GL_DOMAIN) {
        v[0] = (GLint)map->u1;
        v[1] = (GLint)map->u2;
        if (target >= GL_MAP2_COLOR_4) {
            v[2] = (GLint)map->v1;
            v[3] = (GLint)map->v2;
        }
    } else
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
}

static int getn_map_size(struct mgl_context *ctx, GLenum target, GLenum query,
                         GLsizei bufSize, size_t element_size) {
    const struct mgl_eval_map *map;
    unsigned components;
    size_t count;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    map = get_map(ctx, target, &components);
    if (map == NULL || (query != GL_COEFF && query != GL_ORDER && query != GL_DOMAIN)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (query == GL_COEFF)
        count = map->defined ? (size_t)map->uorder * (size_t)map->vorder * components : 0u;
    else if (query == GL_ORDER)
        count = target >= GL_MAP2_COLOR_4 ? 2u : 1u;
    else
        count = target >= GL_MAP2_COLOR_4 ? 4u : 2u;
    if (count * element_size > (size_t)bufSize) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    return 1;
}

void glGetnMapdv(GLenum target, GLenum query, GLsizei bufSize, GLdouble *v) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx != NULL && getn_map_size(ctx, target, query, bufSize, sizeof(*v)))
        glGetMapdv(target, query, v);
}

void glGetnMapfv(GLenum target, GLenum query, GLsizei bufSize, GLfloat *v) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx != NULL && getn_map_size(ctx, target, query, bufSize, sizeof(*v)))
        glGetMapfv(target, query, v);
}

void glGetnMapiv(GLenum target, GLenum query, GLsizei bufSize, GLint *v) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx != NULL && getn_map_size(ctx, target, query, bufSize, sizeof(*v)))
        glGetMapiv(target, query, v);
}

void glIndexf(GLfloat c) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx != NULL)
        ctx->evaluator.current_index = c;
}

void glIndexfv(const GLfloat *c) {
    if (c != NULL)
        glIndexf(*c);
}

void glIndexd(GLdouble c) {
    glIndexf((GLfloat)c);
}

void glIndexi(GLint c) {
    glIndexf((GLfloat)c);
}

void glIndexs(GLshort c) {
    glIndexf((GLfloat)c);
}

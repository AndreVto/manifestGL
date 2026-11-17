#ifndef MGL_EVALUATOR_H
#define MGL_EVALUATOR_H

#include "mgl/types.h"

#define MGL_EVAL_MAP_COUNT 9u
#define MGL_MAX_EVAL_ORDER 64u

struct mgl_eval_map {
    GLboolean defined;
    GLboolean enabled;
    unsigned components;
    GLint uorder;
    GLint vorder;
    GLdouble u1;
    GLdouble u2;
    GLdouble v1;
    GLdouble v2;
    GLdouble *coeff;
};

struct mgl_evaluator_state {
    struct mgl_eval_map map1[MGL_EVAL_MAP_COUNT];
    struct mgl_eval_map map2[MGL_EVAL_MAP_COUNT];
    GLboolean auto_normal;
    GLint grid1_segments;
    GLdouble grid1_u1;
    GLdouble grid1_u2;
    GLint grid2_u_segments;
    GLint grid2_v_segments;
    GLdouble grid2_u1;
    GLdouble grid2_u2;
    GLdouble grid2_v1;
    GLdouble grid2_v2;
    GLfloat current_index;
};

struct mgl_context;

void mgl_evaluator_init(struct mgl_evaluator_state *state);
void mgl_evaluator_destroy(struct mgl_evaluator_state *state);
GLboolean *mgl_evaluator_enable_ptr(struct mgl_context *ctx, GLenum cap);

#endif

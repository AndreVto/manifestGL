#ifndef MGL_IMMEDIATE_H
#define MGL_IMMEDIATE_H

#include "mgl/types.h"
#include "compat/fixed_function.h"

struct mgl_immediate_vertex {
    GLfloat position[4];
    GLfloat normal[4];
    GLfloat color[4];
    GLfloat secondary[4];
    GLfloat fog;
    GLboolean edge_flag;
    GLfloat texcoord[MGL_TEXTURE_UNITS][4];
};

struct mgl_immediate_state {
    GLboolean active;
    GLenum mode;
    struct mgl_immediate_vertex *vertices;
    size_t count;
    size_t capacity;
};

struct mgl_context;
void mgl_immediate_destroy(struct mgl_immediate_state *state);

#endif

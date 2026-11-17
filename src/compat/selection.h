#ifndef MGL_SELECTION_H
#define MGL_SELECTION_H

#include "mgl/types.h"

#define MGL_NAME_STACK_MAX_DEPTH 64u

struct mgl_context;

struct mgl_selection_state {
    GLenum render_mode;
    GLuint *buffer;
    GLsizei buffer_size;
    GLsizei buffer_pos;
    GLint hit_count;
    GLboolean overflow;
    GLfloat *feedback_buffer;
    GLsizei feedback_size;
    GLsizei feedback_pos;
    GLenum feedback_type;
    GLboolean feedback_configured;
    GLboolean feedback_overflow;
    GLuint names[MGL_NAME_STACK_MAX_DEPTH];
    unsigned name_depth;
    GLboolean pending_hit;
    GLfloat min_depth;
    GLfloat max_depth;
};

void mgl_selection_init(struct mgl_selection_state *state);
int mgl_selection_active(const struct mgl_context *ctx);
void mgl_selection_draw_arrays(struct mgl_context *ctx, GLenum mode, GLint first, GLsizei count);
void mgl_selection_draw_elements(struct mgl_context *ctx, GLenum mode, GLsizei count, GLenum type,
                                 const void *indices);

#endif

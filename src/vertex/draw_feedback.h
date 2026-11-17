#ifndef MGL_DRAW_FEEDBACK_H
#define MGL_DRAW_FEEDBACK_H

#include "mgl/types.h"

struct mgl_context;

void mgl_draw_transform_feedback_native(struct mgl_context *ctx, GLenum mode, GLuint native_tf,
                                        GLsizei instancecount);

#endif

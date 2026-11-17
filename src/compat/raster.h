#ifndef MGL_COMPAT_RASTER_H
#define MGL_COMPAT_RASTER_H

#include "mgl/types.h"
#include <stddef.h>

struct mgl_context;

struct mgl_compat_raster_state {
    GLboolean valid;
    GLfloat position[4];
    GLfloat color[4];
    GLfloat secondary[4];
    GLfloat texcoord[4];
};

void mgl_compat_raster_init(struct mgl_compat_raster_state *state);
int mgl_raster_draw_pixels_tight(struct mgl_context *ctx, GLsizei width, GLsizei height,
                                 GLenum format, GLenum type, const void *data, size_t data_size);
int mgl_raster_bitmap_tight(struct mgl_context *ctx, GLsizei width, GLsizei height, GLfloat xorig,
                            GLfloat yorig, GLfloat xmove, GLfloat ymove, const void *data,
                            size_t data_size);

#endif

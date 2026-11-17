#ifndef MGL_PIXEL_LAYOUT_H
#define MGL_PIXEL_LAYOUT_H

#include <stddef.h>
#include "mgl/types.h"

struct mgl_pixel_layout {
    size_t row_bytes;
    size_t row_stride;
    size_t image_stride;
    size_t start_offset;
    size_t span_bytes;
};

int mgl_pixel_layout_2d(GLsizei width, GLsizei height, size_t bytes_per_pixel, GLint alignment,
                        GLint row_length, GLint skip_pixels, GLint skip_rows,
                        struct mgl_pixel_layout *layout);
int mgl_pixel_layout_3d(GLsizei width, GLsizei height, GLsizei depth, size_t bytes_per_pixel,
                        GLint alignment, GLint row_length, GLint image_height, GLint skip_pixels,
                        GLint skip_rows, GLint skip_images, struct mgl_pixel_layout *layout);

#endif

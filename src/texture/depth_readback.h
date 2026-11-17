#ifndef MGL_DEPTH_READBACK_H
#define MGL_DEPTH_READBACK_H

#include "mgl/types.h"

struct mgl_context;
struct mgl_texture;

/* Read realized depth textures through the GLES color-readable helper path. */
int mgl_depth_texture_readback(struct mgl_context *ctx, struct mgl_texture *texture, GLint level,
                               GLint x, GLint y, GLsizei width, GLsizei height, GLfloat *out_depth);

int mgl_depth_copy_framebuffer_to_texture(struct mgl_context *ctx, struct mgl_texture *texture,
                                          GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                          GLint x, GLint y, GLsizei width, GLsizei height);
void mgl_depth_copy_cleanup(struct mgl_context *ctx);

#endif

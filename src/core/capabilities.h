#ifndef MGL_CAPABILITIES_H
#define MGL_CAPABILITIES_H

#include "mgl/types.h"

struct mgl_context;

GLuint mgl_capability_extension_count(const struct mgl_context *ctx);
const GLubyte *mgl_capability_extension(const struct mgl_context *ctx, GLuint index);
const GLubyte *mgl_capability_extension_string(const struct mgl_context *ctx);
GLuint mgl_capability_compressed_texture_format_count(void);
GLenum mgl_capability_compressed_texture_format(GLuint index);

#endif

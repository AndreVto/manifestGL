#ifndef MGL_SAMPLER_H
#define MGL_SAMPLER_H

#include "core/context.h"

struct mgl_sampler *mgl_sampler_find(struct mgl_context *ctx, GLuint name);
struct mgl_sampler *mgl_sampler_create(struct mgl_context *ctx, GLuint name);
void mgl_sampler_remove(struct mgl_context *ctx, GLuint name);
void mgl_sampler_realize_unit(struct mgl_context *ctx, GLuint unit,
                              const struct mgl_texture *texture);

#endif

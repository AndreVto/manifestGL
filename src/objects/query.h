#ifndef MGL_QUERY_H
#define MGL_QUERY_H

#include "mgl/types.h"

struct mgl_context;

struct mgl_query {
    GLuint name;
    GLuint native_name;
    GLenum target;
    GLuint index;
    GLboolean native_indexed;
    GLboolean active;
    GLboolean available;
    GLboolean native_managed;
    GLuint64 result;
    struct mgl_query *next;
};

struct mgl_query *mgl_query_find(struct mgl_context *ctx, GLuint name);
int mgl_query_conditional_result(struct mgl_context *ctx, GLuint name, int wait,
                                 GLboolean *available, GLboolean *passed);
void mgl_query_free_all(struct mgl_context *ctx);

#endif

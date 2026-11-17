#ifndef MGL_DEBUG_H
#define MGL_DEBUG_H

#include "mgl/types.h"

#define MGL_DEBUG_QUEUE_CAPACITY 64u
#define MGL_DEBUG_TEXT_CAPACITY 1024u
#define MGL_DEBUG_FILTER_CAPACITY 32u
#define MGL_DEBUG_GROUP_MAX_DEPTH 64u

struct mgl_debug_message {
    GLenum source, type, severity;
    GLuint id;
    GLsizei length;
    char text[MGL_DEBUG_TEXT_CAPACITY];
};

struct mgl_debug_filter {
    GLenum source, type, severity;
    GLuint id;
    GLboolean use_id;
    GLboolean enabled;
};

struct mgl_debug_ptr_label {
    const void *ptr;
    char text[256];
    struct mgl_debug_ptr_label *next;
};

struct mgl_debug_state {
    GLboolean output_enabled;
    GLboolean synchronous;
    GLDEBUGPROC callback;
    const void *callback_user;
    struct mgl_debug_message queue[MGL_DEBUG_QUEUE_CAPACITY];
    unsigned queue_head;
    unsigned queue_count;
    struct mgl_debug_filter filters[MGL_DEBUG_FILTER_CAPACITY];
    unsigned filter_count;
    unsigned group_depth;
    struct mgl_debug_ptr_label *ptr_labels;
};

struct mgl_object_label {
    GLenum identifier;
    GLuint name;
    char text[256];
    struct mgl_object_label *next;
};

struct mgl_context;
void mgl_debug_init(struct mgl_debug_state *debug);
void mgl_debug_destroy(struct mgl_debug_state *debug);

#endif

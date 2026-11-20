#include "mgl/gl.h"

#include "core/context.h"
#include "shader/shader_store.h"
#include "objects/object_store.h"
#include "vertex/draw_feedback.h"

#include <stdlib.h>
#include <string.h>

/* Check Begin/End legality before examining transform-feedback names or arguments. */
static int reject_during_begin_end(struct mgl_context *ctx) {
    if (ctx == NULL || !ctx->immediate.active)
        return 0;
    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    return 1;
}

static struct mgl_transform_feedback *find_tf(struct mgl_context *ctx, GLuint name) {
    struct mgl_transform_feedback *it;
    if (ctx == NULL || name == 0u)
        return NULL;
    for (it = ctx->transform_feedbacks; it != NULL; it = it->next)
        if (it->name == name)
            return it;
    return NULL;
}

static GLuint next_tf_name(struct mgl_context *ctx) {
    GLuint name = ctx->next_transform_feedback_name;
    if (name == 0u)
        name = 1u;
    while (find_tf(ctx, name) != NULL)
        ++name;
    ctx->next_transform_feedback_name = name + 1u;
    return name;
}

static struct mgl_transform_feedback *reserve_tf(struct mgl_context *ctx, GLuint name) {
    struct mgl_transform_feedback *tf;
    if (ctx == NULL || name == 0u)
        return NULL;
    tf = find_tf(ctx, name);
    if (tf != NULL)
        return tf;
    tf = calloc(1, sizeof(*tf));
    if (tf == NULL)
        return NULL;
    tf->name = name;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGenTransformFeedbacks != NULL)
        ctx->native_dispatch->glesGenTransformFeedbacks(1, &tf->native_name);
    tf->next = ctx->transform_feedbacks;
    ctx->transform_feedbacks = tf;
    if (ctx->next_transform_feedback_name <= name)
        ctx->next_transform_feedback_name = name + 1u;
    return tf;
}

static void remove_tf(struct mgl_context *ctx, GLuint name) {
    struct mgl_transform_feedback **link;
    if (ctx == NULL || name == 0u)
        return;
    link = &ctx->transform_feedbacks;
    while (*link != NULL) {
        if ((*link)->name == name) {
            struct mgl_transform_feedback *victim = *link;
            *link = victim->next;
            if (ctx->transform_feedback_binding == name) {
                ctx->transform_feedback_binding = 0u;
                if (ctx->native_dispatch != NULL &&
                    ctx->native_dispatch->glesBindTransformFeedback != NULL)
                    ctx->native_dispatch->glesBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0u);
            }
            if (victim->native_name != 0u && ctx->native_dispatch != NULL &&
                ctx->native_dispatch->glesDeleteTransformFeedbacks != NULL)
                ctx->native_dispatch->glesDeleteTransformFeedbacks(1, &victim->native_name);
            free(victim);
            return;
        }
        link = &(*link)->next;
    }
}

void glGenTransformFeedbacks(GLsizei n, GLuint *ids) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0 || ids == NULL)
        return;
    for (i = 0; i < n; ++i) {
        GLuint name = next_tf_name(ctx);
        struct mgl_transform_feedback *tf = reserve_tf(ctx, name);
        ids[i] = tf != NULL ? name : 0u;
        if (tf == NULL)
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
    }
}

void glCreateTransformFeedbacks(GLsizei n, GLuint *ids) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint old;
    GLsizei i;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0 || ids == NULL)
        return;
    old = ctx->transform_feedback_binding;
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGenTransformFeedbacks(n, ids);
    if (ctx->errors.sequence != error_sequence_before)
        return;
    for (i = 0; i < n; ++i)
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, ids[i]);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, old);
}

void glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0 || ids == NULL)
        return;
    if (ctx->transform_feedback_active && !ctx->transform_feedback_paused) {
        for (i = 0; i < n; ++i)
            if (ids[i] == ctx->transform_feedback_binding && ids[i] != 0u) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
    }
    for (i = 0; i < n; ++i)
        remove_tf(ctx, ids[i]);
}

GLboolean glIsTransformFeedback(GLuint id) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_transform_feedback *tf;
    if (ctx == NULL)
        return GL_FALSE;
    if (reject_during_begin_end(ctx))
        return GL_FALSE;
    if (id == 0u)
        return GL_FALSE;
    tf = find_tf(ctx, id);
    return tf != NULL && tf->realized ? GL_TRUE : GL_FALSE;
}

void glBindTransformFeedback(GLenum target, GLuint id) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_transform_feedback *tf = NULL;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (target != GL_TRANSFORM_FEEDBACK) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (ctx->transform_feedback_active && !ctx->transform_feedback_paused) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (id != 0u) {
        tf = find_tf(ctx, id);
        if (tf == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        tf->realized = GL_TRUE;
    }
    ctx->transform_feedback_binding = id;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindTransformFeedback != NULL)
        ctx->native_dispatch->glesBindTransformFeedback(target, tf != NULL ? tf->native_name : 0u);
}

static struct mgl_transform_feedback *dsa_tf(struct mgl_context *ctx, GLuint name) {
    struct mgl_transform_feedback *tf = find_tf(ctx, name);
    if (name == 0u || tf == NULL || !tf->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    return tf;
}

static GLuint native_tf_buffer(struct mgl_context *ctx, GLuint name) {
    struct mgl_buffer *obj = mgl_buffer_find(ctx, name);
    return obj != NULL ? obj->native_name : 0u;
}

static void named_tf_buffer(GLuint xfb, GLuint index, GLuint buffer, GLintptr offset,
                            GLsizeiptr size, int ranged) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_transform_feedback *tf, *old_tf;
    struct mgl_buffer *obj = NULL;
    struct mgl_indexed_buffer_binding *slot;
    GLint limit = 4;
    if (ctx == NULL || reject_during_begin_end(ctx))
        return;
    tf = dsa_tf(ctx, xfb);
    if (tf == NULL)
        return;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegerv != NULL) {
        ctx->native_dispatch->glesGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &limit);
        if (limit < 4)
            limit = 4;
    }
    if (index >= MGL_MAX_INDEXED_BUFFER_BINDINGS || index >= (GLuint)limit) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (tf->active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (buffer != 0u) {
        obj = mgl_buffer_find(ctx, buffer);
        if (obj == NULL || !obj->realized) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
    }
    if (ranged) {
        if (offset < 0 || (buffer != 0u && (size <= 0 || (offset & 3) != 0 || (size & 3) != 0))) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
        if (obj != NULL &&
            ((size_t)offset > obj->size || (size_t)size > obj->size - (size_t)offset)) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
    }
    slot = &tf->buffer_bindings[index];
    slot->buffer = buffer;
    slot->offset = ranged && buffer != 0u ? offset : 0;
    slot->size = obj != NULL ? (ranged ? size : (GLsizeiptr)obj->size) : 0;
    slot->ranged = ranged && buffer != 0u ? GL_TRUE : GL_FALSE;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesBindTransformFeedback == NULL)
        return;
    old_tf = find_tf(ctx, ctx->transform_feedback_binding);
    ctx->native_dispatch->glesBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf->native_name);
    if (ranged && obj != NULL)
        ctx->native_dispatch->glesBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, index,
                                                  obj->native_name, offset, size);
    else
        ctx->native_dispatch->glesBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, index,
                                                 obj != NULL ? obj->native_name : 0u);
    ctx->native_dispatch->glesBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
                                                    old_tf != NULL ? old_tf->native_name : 0u);
    mgl_native_dispatch_buffer_binding_invalidate(ctx, GL_TRANSFORM_FEEDBACK_BUFFER);
    mgl_native_dispatch_bind_buffer(ctx, GL_TRANSFORM_FEEDBACK_BUFFER,
                                    native_tf_buffer(ctx, ctx->transform_feedback_buffer));
}

void glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer) {
    named_tf_buffer(xfb, index, buffer, 0, 0, 0);
}

void glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer, GLintptr offset,
                                    GLsizeiptr size) {
    named_tf_buffer(xfb, index, buffer, offset, size, 1);
}

static const struct mgl_indexed_buffer_binding *tf_query_slot(struct mgl_context *ctx, GLuint xfb,
                                                              GLuint index) {
    struct mgl_transform_feedback *tf;
    GLint limit = 4;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegerv != NULL) {
        ctx->native_dispatch->glesGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &limit);
        if (limit < 4)
            limit = 4;
    }
    if (index >= MGL_MAX_INDEXED_BUFFER_BINDINGS || index >= (GLuint)limit) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return NULL;
    }
    if (xfb == 0u)
        return &ctx->transform_feedback_buffer_bindings[index];
    tf = dsa_tf(ctx, xfb);
    return tf != NULL ? &tf->buffer_bindings[index] : NULL;
}

void glGetTransformFeedbackiv(GLuint xfb, GLenum pname, GLint *param) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_transform_feedback *tf;
    if (ctx == NULL || reject_during_begin_end(ctx))
        return;
    if (pname != GL_TRANSFORM_FEEDBACK_ACTIVE && pname != GL_TRANSFORM_FEEDBACK_PAUSED) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    tf = xfb != 0u ? dsa_tf(ctx, xfb) : NULL;
    if (xfb != 0u && tf == NULL)
        return;
    if (param != NULL) {
        if (pname == GL_TRANSFORM_FEEDBACK_ACTIVE)
            *param = tf != NULL
                         ? tf->active
                         : (ctx->transform_feedback_binding == 0u ? ctx->transform_feedback_active
                                                                  : GL_FALSE);
        else
            *param = tf != NULL
                         ? tf->paused
                         : (ctx->transform_feedback_binding == 0u ? ctx->transform_feedback_paused
                                                                  : GL_FALSE);
    }
}

void glGetTransformFeedbacki_v(GLuint xfb, GLenum pname, GLuint index, GLint *param) {
    struct mgl_context *ctx = mgl_context_current();
    const struct mgl_indexed_buffer_binding *slot;
    if (ctx == NULL || reject_during_begin_end(ctx))
        return;
    if (pname != GL_TRANSFORM_FEEDBACK_BUFFER_BINDING) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    slot = tf_query_slot(ctx, xfb, index);
    if (slot != NULL && param != NULL)
        *param = (GLint)slot->buffer;
}

void glGetTransformFeedbacki64_v(GLuint xfb, GLenum pname, GLuint index, GLint64 *param) {
    struct mgl_context *ctx = mgl_context_current();
    const struct mgl_indexed_buffer_binding *slot;
    struct mgl_buffer *buffer;
    if (ctx == NULL || reject_during_begin_end(ctx))
        return;
    if (pname != GL_TRANSFORM_FEEDBACK_BUFFER_START && pname != GL_TRANSFORM_FEEDBACK_BUFFER_SIZE) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    slot = tf_query_slot(ctx, xfb, index);
    if (slot == NULL || param == NULL)
        return;
    if (pname == GL_TRANSFORM_FEEDBACK_BUFFER_START) {
        *param = (GLint64)slot->offset;
        return;
    }
    buffer = mgl_buffer_find(ctx, slot->buffer);
    *param = buffer == NULL ? 0 : slot->ranged ? (GLint64)slot->size : (GLint64)buffer->size;
}

static int valid_tf_primitive(GLenum mode) {
    return mode == GL_POINTS || mode == GL_LINES || mode == GL_TRIANGLES;
}

void glBeginTransformFeedback(GLenum primitiveMode) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (!valid_tf_primitive(primitiveMode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (ctx->transform_feedback_active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesBeginTransformFeedback == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesBeginTransformFeedback(primitiveMode);
    ctx->transform_feedback_active = GL_TRUE;
    ctx->transform_feedback_paused = GL_FALSE;
    {
        struct mgl_transform_feedback *tf = find_tf(ctx, ctx->transform_feedback_binding);
        if (tf != NULL) {
            tf->active = GL_TRUE;
            tf->paused = GL_FALSE;
        }
    }
}

void glEndTransformFeedback(void) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (!ctx->transform_feedback_active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesEndTransformFeedback == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesEndTransformFeedback();
    ctx->transform_feedback_active = GL_FALSE;
    ctx->transform_feedback_paused = GL_FALSE;
    {
        struct mgl_transform_feedback *tf = find_tf(ctx, ctx->transform_feedback_binding);
        if (tf != NULL) {
            tf->active = GL_FALSE;
            tf->paused = GL_FALSE;
            tf->ever_ended = GL_TRUE;
        }
    }
}

void glPauseTransformFeedback(void) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (!ctx->transform_feedback_active || ctx->transform_feedback_paused) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesPauseTransformFeedback == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesPauseTransformFeedback();
    ctx->transform_feedback_paused = GL_TRUE;
    {
        struct mgl_transform_feedback *tf = find_tf(ctx, ctx->transform_feedback_binding);
        if (tf != NULL)
            tf->paused = GL_TRUE;
    }
}

void glResumeTransformFeedback(void) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (!ctx->transform_feedback_active || !ctx->transform_feedback_paused) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesResumeTransformFeedback == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesResumeTransformFeedback();
    ctx->transform_feedback_paused = GL_FALSE;
    {
        struct mgl_transform_feedback *tf = find_tf(ctx, ctx->transform_feedback_binding);
        if (tf != NULL)
            tf->paused = GL_FALSE;
    }
}

static int feedback_draw_mode(GLenum mode) {
    return mode == GL_POINTS || mode == GL_LINES || mode == GL_LINE_STRIP || mode == GL_LINE_LOOP ||
           mode == GL_TRIANGLES || mode == GL_TRIANGLE_STRIP || mode == GL_TRIANGLE_FAN ||
           mode == GL_PATCHES;
}

static void draw_feedback(GLenum mode, GLuint id, GLuint stream, GLsizei instancecount) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_transform_feedback *tf;
    if (ctx == NULL || reject_during_begin_end(ctx))
        return;
    if (!feedback_draw_mode(mode)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (instancecount < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    tf = find_tf(ctx, id);
    if (tf == NULL || !tf->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!tf->ever_ended) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (stream != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch == NULL ||
        (ctx->native_dispatch->glesDrawTransformFeedbackEXT == NULL &&
         ctx->native_dispatch->glesDrawTransformFeedbackInstancedEXT == NULL) ||
        (instancecount != 1 &&
         ctx->native_dispatch->glesDrawTransformFeedbackInstancedEXT == NULL) ||
        !mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                                "GL_EXT_draw_transform_feedback")) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    mgl_draw_transform_feedback_native(ctx, mode, tf->native_name, instancecount);
}

void glDrawTransformFeedback(GLenum mode, GLuint id) {
    draw_feedback(mode, id, 0u, 1);
}

void glDrawTransformFeedbackInstanced(GLenum mode, GLuint id, GLsizei instancecount) {
    draw_feedback(mode, id, 0u, instancecount);
}

void glDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream) {
    draw_feedback(mode, id, stream, 1);
}

void glDrawTransformFeedbackStreamInstanced(GLenum mode, GLuint id, GLuint stream,
                                            GLsizei instancecount) {
    draw_feedback(mode, id, stream, instancecount);
}

void glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar *const *varyings,
                                 GLenum bufferMode) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *p;
    char **copy = NULL;
    GLsizei i;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (count < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (bufferMode != GL_INTERLEAVED_ATTRIBS && bufferMode != GL_SEPARATE_ATTRIBS) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (count > 0 && varyings == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    p = mgl_program_find(ctx, program);
    if (p == NULL) {
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (count > 0) {
        copy = calloc((size_t)count, sizeof(*copy));
        if (copy == NULL) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        for (i = 0; i < count; ++i) {
            size_t n;
            if (varyings[i] == NULL) {
                GLsizei j;
                for (j = 0; j < i; ++j)
                    free(copy[j]);
                free(copy);
                (void)pthread_mutex_unlock(&ctx->share_group->lock);
                mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
                return;
            }
            n = strlen(varyings[i]);
            copy[i] = malloc(n + 1u);
            if (copy[i] == NULL) {
                GLsizei j;
                for (j = 0; j < i; ++j)
                    free(copy[j]);
                free(copy);
                (void)pthread_mutex_unlock(&ctx->share_group->lock);
                mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
                return;
            }
            memcpy(copy[i], varyings[i], n + 1u);
        }
    }
    if (p->transform_feedback_varyings != NULL) {
        for (i = 0; i < p->transform_feedback_varying_count; ++i)
            free(p->transform_feedback_varyings[i]);
        free(p->transform_feedback_varyings);
    }
    p->transform_feedback_varyings = copy;
    p->transform_feedback_varying_count = count;
    p->transform_feedback_buffer_mode = bufferMode;
    p->linked = GL_FALSE;
    p->validated = GL_FALSE;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length,
                                   GLsizei *size, GLenum *type, GLchar *name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_program *p;
    if (ctx == NULL)
        return;
    if (reject_during_begin_end(ctx))
        return;
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    p = mgl_program_find(ctx, program);
    if (p == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesGetTransformFeedbackVarying == NULL || p->native_name == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesGetTransformFeedbackVarying(p->native_name, index, bufSize, length,
                                                          size, type, name);
}

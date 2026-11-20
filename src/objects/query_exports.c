#include "mgl/gl.h"

#include "core/context.h"
#include "objects/query.h"
#include "objects/object_store.h"
#include "native/dispatch.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static int valid_target(GLenum target, int allow_timestamp) {
    switch (target) {
        case GL_SAMPLES_PASSED:
        case GL_ANY_SAMPLES_PASSED:
        case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        case GL_PRIMITIVES_GENERATED:
        case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        case GL_TIME_ELAPSED:
            return 1;
        case GL_TIMESTAMP:
            return allow_timestamp;
        default:
            return 0;
    }
}

static GLenum native_target(GLenum target) {
    if (target == GL_SAMPLES_PASSED)
        return GL_ANY_SAMPLES_PASSED_CONSERVATIVE;
    if (target == GL_ANY_SAMPLES_PASSED || target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE ||
        target == GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN)
        return target;
    return 0u;
}

static GLuint64 query_result_value(GLenum target, GLuint value) {
    return target == GL_SAMPLES_PASSED && value != 0u ? 4096u : (GLuint64)value;
}

struct mgl_query *mgl_query_find(struct mgl_context *ctx, GLuint name) {
    struct mgl_query *it;
    if (ctx == NULL || name == 0u)
        return NULL;
    for (it = ctx->queries; it != NULL; it = it->next)
        if (it->name == name)
            return it;
    return NULL;
}

static GLuint next_query_name(struct mgl_context *ctx) {
    GLuint name = ctx->next_query_name;
    while (name == 0u || mgl_query_find(ctx, name) != NULL) {
        ++name;
        if (name == ctx->next_query_name)
            return 0u;
    }
    ctx->next_query_name = name + 1u;
    return name;
}

static int boolean_occlusion_target(GLenum target) {
    return target == GL_ANY_SAMPLES_PASSED || target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE;
}

static int query_target_matches(GLenum previous, GLenum target) {
    return previous == 0u || previous == target;
}

static int native_primitive_supported(struct mgl_context *ctx, GLenum target) {
    struct mgl_native_dispatch *native = ctx->native_dispatch;
    if (native == NULL || native->glesGenQueries == NULL || native->glesGetQueryObjectuiv == NULL)
        return 0;
    if (target == GL_PRIMITIVES_GENERATED)
        return native->glesBeginQueryIndexed != NULL && native->glesEndQueryIndexed != NULL;
    if (target == GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN)
        return (native->glesBeginQuery != NULL && native->glesEndQuery != NULL) ||
               (native->glesBeginQueryIndexed != NULL && native->glesEndQueryIndexed != NULL);
    return 0;
}

static struct mgl_query *ensure_query(struct mgl_context *ctx, GLuint name, GLenum target) {
    struct mgl_query *q = mgl_query_find(ctx, name);
    if (q != NULL)
        return q;
    q = calloc(1, sizeof(*q));
    if (q == NULL)
        return NULL;
    q->name = name;
    q->target = target;
    q->available = GL_TRUE;
    q->next = ctx->queries;
    ctx->queries = q;
    return q;
}

static struct mgl_query *active_for_target(struct mgl_context *ctx, GLenum target, GLuint index) {
    struct mgl_query *q;
    for (q = ctx->queries; q != NULL; q = q->next)
        if (q->active && q->target == target && q->index == index)
            return q;
    return NULL;
}

static int native_query_supported(struct mgl_context *ctx, GLenum target) {
    return native_target(target) != 0u && ctx->native_dispatch != NULL &&
           ctx->native_dispatch->glesGenQueries != NULL &&
           ctx->native_dispatch->glesBeginQuery != NULL &&
           ctx->native_dispatch->glesEndQuery != NULL &&
           ctx->native_dispatch->glesGetQueryObjectuiv != NULL;
}

static int native_timer_supported(struct mgl_context *ctx, int timestamp) {
    struct mgl_native_dispatch *native = ctx->native_dispatch;
    return native != NULL && native->glesGenQueries != NULL && native->glesDeleteQueries != NULL &&
           native->glesBeginQuery != NULL && native->glesEndQuery != NULL &&
           native->glesGetQueryObjectuiv != NULL && native->glesGetQueryObjectui64vEXT != NULL &&
           (!timestamp || native->glesQueryCounterEXT != NULL) &&
           mgl_native_dispatch_has_gles_extension(native, "GL_EXT_disjoint_timer_query");
}

static int realize_native_timer(struct mgl_context *ctx, struct mgl_query *q, int timestamp) {
    if (!native_timer_supported(ctx, timestamp))
        return 0;
    if (q->native_name == 0u)
        ctx->native_dispatch->glesGenQueries(1, &q->native_name);
    q->native_managed = q->native_name != 0u ? GL_TRUE : GL_FALSE;
    return q->native_managed;
}

static int realize_native_query(struct mgl_context *ctx, struct mgl_query *q) {
    if (!native_query_supported(ctx, q->target))
        return 0;
    if (q->native_name == 0u)
        ctx->native_dispatch->glesGenQueries(1, &q->native_name);
    q->native_managed = q->native_name != 0u ? GL_TRUE : GL_FALSE;
    return q->native_managed;
}

void glGenQueries(GLsizei n, GLuint *ids) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0)
        return;
    if (ids == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < n; ++i) {
        ids[i] = next_query_name(ctx);
        if (ids[i] == 0u || ensure_query(ctx, ids[i], 0u) == NULL) {
            ids[i] = 0u;
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
    }
}

void glCreateQueries(GLenum target, GLsizei n, GLuint *ids) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_target(target, 1)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0)
        return;
    if (ids == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < n; ++i) {
        struct mgl_query *q;
        ids[i] = next_query_name(ctx);
        if (ids[i] == 0u) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        q = ensure_query(ctx, ids[i], target);
        if (q == NULL) {
            ids[i] = 0u;
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
    }
}

GLboolean glIsQuery(GLuint id) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return GL_FALSE;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    struct mgl_query *q = mgl_query_find(ctx, id);
    return q != NULL && q->target != 0u ? GL_TRUE : GL_FALSE;
}

void glDeleteQueries(GLsizei n, const GLuint *ids) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0)
        return;
    if (ids == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    for (i = 0; i < n; ++i) {
        struct mgl_query **link = &ctx->queries;
        while (*link != NULL) {
            if ((*link)->name == ids[i]) {
                struct mgl_query *victim = *link;
                if (victim->active) {
                    if (victim->target == GL_TIME_ELAPSED)
                        ctx->native_dispatch->glesEndQuery(GL_TIME_ELAPSED);
                    else if (victim->native_indexed)
                        ctx->native_dispatch->glesEndQueryIndexed(victim->target, victim->index);
                    else if (victim->native_managed)
                        ctx->native_dispatch->glesEndQuery(native_target(victim->target));
                    victim->active = GL_FALSE;
                    if (ctx->active_occlusion_query == victim)
                        ctx->active_occlusion_query = NULL;
                }
                *link = victim->next;
                if (victim->native_name != 0u && ctx->native_dispatch != NULL &&
                    ctx->native_dispatch->glesDeleteQueries != NULL)
                    ctx->native_dispatch->glesDeleteQueries(1, &victim->native_name);
                free(victim);
                break;
            }
            link = &(*link)->next;
        }
    }
}

static int valid_query_index(struct mgl_context *ctx, GLenum target, GLuint index,
                             int allow_timestamp) {
    GLint max_streams = 1;
    if (!valid_target(target, allow_timestamp)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    if (index == 0u)
        return 1;
    if (target != GL_PRIMITIVES_GENERATED && target != GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (ctx->native_dispatch != NULL &&
        mgl_native_dispatch_has_gles_extension(ctx->native_dispatch,
                                               "GL_ARB_transform_feedback3") &&
        ctx->native_dispatch->glesBeginQueryIndexed != NULL &&
        ctx->native_dispatch->glesEndQueryIndexed != NULL &&
        ctx->native_dispatch->glesGetIntegerv != NULL)
        ctx->native_dispatch->glesGetIntegerv(GL_MAX_VERTEX_STREAMS, &max_streams);
    if (max_streams < 1)
        max_streams = 1;
    if (index >= (GLuint)max_streams) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    return 1;
}

void glBeginQueryIndexed(GLenum target, GLuint index, GLuint id) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_query *q;
    int use_indexed;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_query_index(ctx, target, index, 0))
        return;
    use_indexed =
        (target == GL_PRIMITIVES_GENERATED || target == GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN) &&
        ctx->native_dispatch != NULL && ctx->native_dispatch->glesBeginQueryIndexed != NULL &&
        ctx->native_dispatch->glesEndQueryIndexed != NULL &&
        ctx->native_dispatch->glesGenQueries != NULL &&
        ctx->native_dispatch->glesGetQueryObjectuiv != NULL;
    if (index != 0u && !use_indexed) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((target == GL_TIME_ELAPSED && !native_timer_supported(ctx, 0)) ||
        ((target == GL_PRIMITIVES_GENERATED ||
          target == GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN) &&
         !native_primitive_supported(ctx, target))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (id == 0u || active_for_target(ctx, target, index) != NULL ||
        ((target == GL_SAMPLES_PASSED || target == GL_ANY_SAMPLES_PASSED ||
          target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE) &&
         ctx->active_occlusion_query != NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    q = mgl_query_find(ctx, id);
    if (q == NULL && (ctx->profile_mask & GL_CONTEXT_CORE_PROFILE_BIT) != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (q != NULL && (q->active || !query_target_matches(q->target, target))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (q == NULL)
        q = ensure_query(ctx, id, target);
    if (q == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    if (target == GL_TIME_ELAPSED && !realize_native_timer(ctx, q, 0)) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    if (q->target != 0u && q->target != target && q->native_name != 0u &&
        ctx->native_dispatch != NULL && ctx->native_dispatch->glesDeleteQueries != NULL) {
        ctx->native_dispatch->glesDeleteQueries(1, &q->native_name);
        q->native_name = 0u;
    }
    q->target = target;
    q->index = index;
    q->active = GL_TRUE;
    if (target == GL_SAMPLES_PASSED || target == GL_ANY_SAMPLES_PASSED ||
        target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE)
        ctx->active_occlusion_query = q;
    q->available = GL_FALSE;
    q->result = 0u;
    if (target != GL_TIME_ELAPSED)
        q->native_managed = GL_FALSE;
    q->native_indexed = GL_FALSE;
    if (target == GL_TIME_ELAPSED) {
        ctx->native_dispatch->glesBeginQuery(GL_TIME_ELAPSED, q->native_name);
    } else if (use_indexed) {
        if (q->native_name == 0u)
            ctx->native_dispatch->glesGenQueries(1, &q->native_name);
        if (q->native_name != 0u) {
            q->native_managed = GL_TRUE;
            q->native_indexed = GL_TRUE;
            ctx->native_dispatch->glesBeginQueryIndexed(target, index, q->native_name);
        }
    } else if (realize_native_query(ctx, q))
        ctx->native_dispatch->glesBeginQuery(native_target(target), q->native_name);
}

void glBeginQuery(GLenum target, GLuint id) {
    glBeginQueryIndexed(target, 0u, id);
}

void glEndQueryIndexed(GLenum target, GLuint index) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_query *q;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_query_index(ctx, target, index, 0))
        return;
    q = active_for_target(ctx, target, index);
    if (q == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (q->target == GL_TIME_ELAPSED) {
        ctx->native_dispatch->glesEndQuery(GL_TIME_ELAPSED);
    } else if (q->native_indexed) {
        ctx->native_dispatch->glesEndQueryIndexed(target, index);
    } else if (q->native_managed) {
        ctx->native_dispatch->glesEndQuery(native_target(target));
    } else {
        q->available = GL_TRUE;
    }
    q->active = GL_FALSE;
    if (ctx->active_occlusion_query == q)
        ctx->active_occlusion_query = NULL;
}

void glEndQuery(GLenum target) {
    glEndQueryIndexed(target, 0u);
}

void glQueryCounter(GLuint id, GLenum target) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_query *q;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_TIMESTAMP) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!native_timer_supported(ctx, 1)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (id == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    q = mgl_query_find(ctx, id);
    if (q == NULL && (ctx->profile_mask & GL_CONTEXT_CORE_PROFILE_BIT) != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (q != NULL && (q->active || !query_target_matches(q->target, target))) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (q == NULL)
        q = ensure_query(ctx, id, target);
    if (q == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    if (!realize_native_timer(ctx, q, 1)) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    q->target = target;
    q->available = GL_FALSE;
    ctx->native_dispatch->glesQueryCounterEXT(q->native_name, GL_TIMESTAMP);
}

void glGetQueryIndexediv(GLenum target, GLuint index, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_query *q;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_query_index(ctx, target, index, 1))
        return;
    if (pname == GL_CURRENT_QUERY) {
        q = active_for_target(ctx, target, index);
        if (params != NULL)
            *params = q != NULL ? (GLint)q->name : 0;
    } else if (pname == GL_QUERY_COUNTER_BITS) {
        if (params != NULL) {
            if ((target == GL_TIME_ELAPSED || target == GL_TIMESTAMP) &&
                native_timer_supported(ctx, target == GL_TIMESTAMP)) {
                if (ctx->native_dispatch->glesGetQueryivEXT != NULL)
                    ctx->native_dispatch->glesGetQueryivEXT(target, pname, params);
                else if (ctx->native_dispatch->glesGetQueryiv != NULL)
                    ctx->native_dispatch->glesGetQueryiv(target, pname, params);
                else
                    *params = 0;
            } else if (target == GL_PRIMITIVES_GENERATED ||
                       target == GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN) {
                *params = native_primitive_supported(ctx, target) ? 32 : 0;
            } else if (boolean_occlusion_target(target)) {
                *params = 1;
            } else {
                *params = (target == GL_TIME_ELAPSED || target == GL_TIMESTAMP) ? 0 : 32;
            }
        }
    } else {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
    }
}

void glGetQueryiv(GLenum target, GLenum pname, GLint *params) {
    glGetQueryIndexediv(target, 0u, pname, params);
}

static int refresh_native_result(struct mgl_context *ctx, struct mgl_query *q, int block) {
    GLuint available = 1u;
    GLuint value = 0u;
    if (!q->native_managed)
        return 1;
    if (q->active)
        return 0;
    if (!block) {
        ctx->native_dispatch->glesGetQueryObjectuiv(q->native_name, GL_QUERY_RESULT_AVAILABLE,
                                                    &available);
        if (!available) {
            q->available = GL_FALSE;
            return 1;
        }
    }
    if (q->target == GL_TIME_ELAPSED || q->target == GL_TIMESTAMP)
        ctx->native_dispatch->glesGetQueryObjectui64vEXT(q->native_name, GL_QUERY_RESULT,
                                                         &q->result);
    else {
        ctx->native_dispatch->glesGetQueryObjectuiv(q->native_name, GL_QUERY_RESULT, &value);
        q->result = query_result_value(q->target, value);
    }
    q->available = GL_TRUE;
    return 1;
}

static int query_value(struct mgl_context *ctx, GLuint id, GLenum pname, GLuint64 *out) {
    struct mgl_query *q = mgl_query_find(ctx, id);
    if (q == NULL || q->target == 0u || q->active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (pname == GL_QUERY_TARGET) {
        *out = q->target;
        return 1;
    }
    if (pname == GL_QUERY_RESULT_AVAILABLE) {
        (void)refresh_native_result(ctx, q, 0);
        *out = q->available ? 1u : 0u;
        return 1;
    }
    if (pname == GL_QUERY_RESULT || pname == GL_QUERY_RESULT_NO_WAIT) {
        if (q->native_managed) {
            int block = pname == GL_QUERY_RESULT;
            if (!refresh_native_result(ctx, q, block))
                return 0;
            if (!q->available && !block)
                return 2;
        }
        *out = q->result;
        return 1;
    }
    mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
    return 0;
}

int mgl_query_conditional_result(struct mgl_context *ctx, GLuint name, int wait,
                                 GLboolean *available, GLboolean *passed) {
    struct mgl_query *q;
    if (ctx == NULL || available == NULL || passed == NULL)
        return 0;
    q = mgl_query_find(ctx, name);
    if (q == NULL || q->active)
        return 0;
    if (q->target != GL_SAMPLES_PASSED && q->target != GL_ANY_SAMPLES_PASSED &&
        q->target != GL_ANY_SAMPLES_PASSED_CONSERVATIVE)
        return 0;
    if (q->native_managed) {
        if (!refresh_native_result(ctx, q, wait != 0))
            return 0;
    }
    *available = q->available;
    *passed = q->result != 0u ? GL_TRUE : GL_FALSE;
    return 1;
}

static void query_buffer_result(GLuint id, GLuint buffer, GLenum pname, GLintptr offset, int width,
                                int is_signed) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_buffer *obj;
    GLuint64 result = 0u;
    unsigned char bytes[sizeof(GLuint64)];
    size_t count = width == 64 ? sizeof(GLuint64) : sizeof(GLuint);
    int status;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = mgl_buffer_find(ctx, buffer);
    if (obj == NULL || !obj->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (offset < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (((size_t)offset % count) != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((size_t)offset > obj->size || count > obj->size - (size_t)offset) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (obj->mapped && (obj->map_access & GL_MAP_PERSISTENT_BIT) == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    status = query_value(ctx, id, pname, &result);
    if (status != 1)
        return;
    if (width == 64) {
        if (is_signed) {
            GLint64 value = result > (GLuint64)INT64_MAX ? INT64_MAX : (GLint64)result;
            memcpy(bytes, &value, count);
        } else
            memcpy(bytes, &result, count);
    } else {
        if (is_signed) {
            GLint value = result > (GLuint64)INT32_MAX ? INT32_MAX : (GLint)result;
            memcpy(bytes, &value, count);
        } else {
            GLuint value = result > UINT32_MAX ? UINT32_MAX : (GLuint)result;
            memcpy(bytes, &value, count);
        }
    }
    (void)mgl_buffer_write_bytes(ctx, obj, (size_t)offset, count, bytes,
                                 obj->native_immutable &&
                                     (obj->storage_flags & GL_DYNAMIC_STORAGE_BIT) == 0u);
}

static void query_object_result(GLuint id, GLenum pname, void *params, int width, int is_signed) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint64 result = 0u;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->query_buffer != 0u) {
        query_buffer_result(id, ctx->query_buffer, pname, (GLintptr)(uintptr_t)params, width,
                            is_signed);
        return;
    }
    if (params == NULL || query_value(ctx, id, pname, &result) != 1)
        return;
    if (width == 64) {
        if (is_signed)
            *(GLint64 *)params = result > (GLuint64)INT64_MAX ? INT64_MAX : (GLint64)result;
        else
            *(GLuint64 *)params = result;
    } else {
        if (is_signed)
            *(GLint *)params = result > (GLuint64)INT32_MAX ? INT32_MAX : (GLint)result;
        else
            *(GLuint *)params = result > UINT32_MAX ? UINT32_MAX : (GLuint)result;
    }
}

void glGetQueryObjectiv(GLuint id, GLenum pname, GLint *params) {
    query_object_result(id, pname, params, 32, 1);
}

void glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params) {
    query_object_result(id, pname, params, 32, 0);
}

void glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params) {
    query_object_result(id, pname, params, 64, 1);
}

void glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params) {
    query_object_result(id, pname, params, 64, 0);
}

void glGetQueryBufferObjectiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    query_buffer_result(id, buffer, pname, offset, 32, 1);
}

void glGetQueryBufferObjectuiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    query_buffer_result(id, buffer, pname, offset, 32, 0);
}

void glGetQueryBufferObjecti64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    query_buffer_result(id, buffer, pname, offset, 64, 1);
}

void glGetQueryBufferObjectui64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    query_buffer_result(id, buffer, pname, offset, 64, 0);
}

void glGenQueriesARB(GLsizei n, GLuint *ids) {
    glGenQueries(n, ids);
}

void glDeleteQueriesARB(GLsizei n, const GLuint *ids) {
    glDeleteQueries(n, ids);
}

GLboolean glIsQueryARB(GLuint id) {
    return glIsQuery(id);
}

void glBeginQueryARB(GLenum target, GLuint id) {
    glBeginQuery(target, id);
}

void glEndQueryARB(GLenum target) {
    glEndQuery(target);
}

void glGetQueryivARB(GLenum target, GLenum pname, GLint *params) {
    glGetQueryiv(target, pname, params);
}

void glGetQueryObjectivARB(GLuint id, GLenum pname, GLint *params) {
    glGetQueryObjectiv(id, pname, params);
}

void glGetQueryObjectuivARB(GLuint id, GLenum pname, GLuint *params) {
    glGetQueryObjectuiv(id, pname, params);
}

void mgl_query_free_all(struct mgl_context *ctx) {
    struct mgl_query *q;
    if (ctx == NULL)
        return;
    q = ctx->queries;
    while (q != NULL) {
        struct mgl_query *next = q->next;
        if (q->native_name != 0u && mgl_context_current() == ctx && ctx->native_dispatch != NULL &&
            ctx->native_dispatch->glesDeleteQueries != NULL)
            ctx->native_dispatch->glesDeleteQueries(1, &q->native_name);
        free(q);
        q = next;
    }
    ctx->queries = NULL;
    ctx->active_occlusion_query = NULL;
}

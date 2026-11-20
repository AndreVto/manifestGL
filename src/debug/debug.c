#include "mgl/gl.h"
#include "core/context.h"
#include "core/render_diag.h"
#include "debug/debug.h"
#include "abi/unimplemented_exports.h"
#include "shader/translator.h"

#include <stdlib.h>
#include <string.h>

static int valid_source(GLenum value, int allow_dont_care) {
    return (allow_dont_care && value == GL_DONT_CARE) || value == GL_DEBUG_SOURCE_API ||
           value == GL_DEBUG_SOURCE_WINDOW_SYSTEM || value == GL_DEBUG_SOURCE_SHADER_COMPILER ||
           value == GL_DEBUG_SOURCE_THIRD_PARTY || value == GL_DEBUG_SOURCE_APPLICATION ||
           value == GL_DEBUG_SOURCE_OTHER;
}

static int valid_type(GLenum value, int allow_dont_care) {
    return (allow_dont_care && value == GL_DONT_CARE) || value == GL_DEBUG_TYPE_ERROR ||
           value == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR ||
           value == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR || value == GL_DEBUG_TYPE_PORTABILITY ||
           value == GL_DEBUG_TYPE_PERFORMANCE || value == GL_DEBUG_TYPE_OTHER ||
           value == GL_DEBUG_TYPE_MARKER || value == GL_DEBUG_TYPE_PUSH_GROUP ||
           value == GL_DEBUG_TYPE_POP_GROUP;
}

static int valid_severity(GLenum value, int allow_dont_care) {
    return (allow_dont_care && value == GL_DONT_CARE) || value == GL_DEBUG_SEVERITY_HIGH ||
           value == GL_DEBUG_SEVERITY_MEDIUM || value == GL_DEBUG_SEVERITY_LOW ||
           value == GL_DEBUG_SEVERITY_NOTIFICATION;
}

static int copy_text(char *dst, size_t capacity, GLsizei length, const GLchar *src,
                     GLsizei *out_length) {
    size_t count;

    if (src == NULL) {
        return 0;
    }

    count = length < 0 ? strlen(src) : (size_t)length;
    if (count >= capacity) {
        return 0;
    }

    memcpy(dst, src, count);
    dst[count] = '\0';
    if (out_length != NULL) {
        *out_length = (GLsizei)count;
    }
    return 1;
}

void mgl_debug_init(struct mgl_debug_state *debug) {
    if (debug != NULL) {
        memset(debug, 0, sizeof(*debug));
    }
}

void mgl_debug_destroy(struct mgl_debug_state *debug) {
    struct mgl_debug_ptr_label *label;

    if (debug == NULL) {
        return;
    }

    label = debug->ptr_labels;
    while (label != NULL) {
        struct mgl_debug_ptr_label *next = label->next;
        free(label);
        label = next;
    }
    memset(debug, 0, sizeof(*debug));
}

static int filter_match(const struct mgl_debug_filter *filter, GLenum source, GLenum type,
                        GLuint id, GLenum severity) {
    if (filter->source != GL_DONT_CARE && filter->source != source) {
        return 0;
    }
    if (filter->type != GL_DONT_CARE && filter->type != type) {
        return 0;
    }
    if (filter->severity != GL_DONT_CARE && filter->severity != severity) {
        return 0;
    }
    if (filter->use_id && filter->id != id) {
        return 0;
    }
    return 1;
}

static int message_enabled(struct mgl_debug_state *debug, GLenum source, GLenum type, GLuint id,
                           GLenum severity) {
    int enabled = 1;
    unsigned i;

    for (i = 0; i < debug->filter_count; ++i) {
        if (filter_match(&debug->filters[i], source, type, id, severity)) {
            enabled = debug->filters[i].enabled ? 1 : 0;
        }
    }
    return enabled;
}

static void push_message(struct mgl_context *ctx, GLenum source, GLenum type, GLuint id,
                         GLenum severity, GLsizei length, const GLchar *text) {
    struct mgl_debug_state *debug = &ctx->debug;
    struct mgl_debug_message message;
    unsigned index;

    if (!message_enabled(debug, source, type, id, severity)) {
        return;
    }

    memset(&message, 0, sizeof(message));
    message.source = source;
    message.type = type;
    message.id = id;
    message.severity = severity;

    if (!copy_text(message.text, sizeof(message.text), length, text, &message.length)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }

    if (debug->queue_count == MGL_DEBUG_QUEUE_CAPACITY) {
        debug->queue_head = (debug->queue_head + 1u) % MGL_DEBUG_QUEUE_CAPACITY;
        --debug->queue_count;
    }

    index = (debug->queue_head + debug->queue_count) % MGL_DEBUG_QUEUE_CAPACITY;
    debug->queue[index] = message;
    ++debug->queue_count;

    if (debug->output_enabled && debug->callback != NULL) {
        debug->callback(source, type, id, severity, message.length, message.text,
                        debug->callback_user);
    }
}

void glDebugMessageCallback(GLDEBUGPROC callback, const void *userParam) {
    struct mgl_context *ctx = mgl_context_current();

    if (ctx == NULL) {
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->debug.callback = callback;
    ctx->debug.callback_user = userParam;
    {
        uintptr_t callback_bits = 0u;
        size_t bytes =
            sizeof(callback_bits) < sizeof(callback) ? sizeof(callback_bits) : sizeof(callback);
        memcpy(&callback_bits, &callback, bytes);
        mgl_render_diag_detail(ctx, "glDebugMessageCallback", "registered_callback_user",
                               callback_bits, (uintptr_t)userParam);
    }
}

void glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count,
                           const GLuint *ids, GLboolean enabled) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;

    if (ctx == NULL) {
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_source(source, 1) || !valid_type(type, 1) || !valid_severity(severity, 1)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (count < 0 || (count > 0 && ids == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (count > 0 && (source == GL_DONT_CARE || type == GL_DONT_CARE || severity != GL_DONT_CARE)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }

    if (count == 0) {
        if (ctx->debug.filter_count == MGL_DEBUG_FILTER_CAPACITY) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        ctx->debug.filters[ctx->debug.filter_count++] = (struct mgl_debug_filter){
            source, type, severity, 0u, GL_FALSE, enabled ? GL_TRUE : GL_FALSE};
        return;
    }

    for (i = 0; i < count; ++i) {
        if (ctx->debug.filter_count == MGL_DEBUG_FILTER_CAPACITY) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        ctx->debug.filters[ctx->debug.filter_count++] = (struct mgl_debug_filter){
            source, type, GL_DONT_CARE, ids[i], GL_TRUE, enabled ? GL_TRUE : GL_FALSE};
    }
}

void glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                          const GLchar *buf) {
    struct mgl_context *ctx = mgl_context_current();

    if (ctx == NULL) {
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((source != GL_DEBUG_SOURCE_APPLICATION && source != GL_DEBUG_SOURCE_THIRD_PARTY) ||
        !valid_type(type, 0) || !valid_severity(severity, 0)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (buf == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }

    push_message(ctx, source, type, id, severity, length, buf);
}

GLuint glGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types,
                            GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint returned = 0;
    GLsizei used = 0;

    if (ctx == NULL) {
        return 0u;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0u;
    }
    if (count == 0u)
        return 0u;
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0u;
    }

    while (returned < count && ctx->debug.queue_count != 0u) {
        struct mgl_debug_message *message = &ctx->debug.queue[ctx->debug.queue_head];
        GLsizei required = message->length + 1;

        if (messageLog != NULL && used + required > bufSize) {
            break;
        }
        if (sources != NULL) {
            sources[returned] = message->source;
        }
        if (types != NULL) {
            types[returned] = message->type;
        }
        if (ids != NULL) {
            ids[returned] = message->id;
        }
        if (severities != NULL) {
            severities[returned] = message->severity;
        }
        if (lengths != NULL) {
            lengths[returned] = required;
        }
        if (messageLog != NULL) {
            memcpy(messageLog + used, message->text, (size_t)required);
            used += required;
        }

        ctx->debug.queue_head = (ctx->debug.queue_head + 1u) % MGL_DEBUG_QUEUE_CAPACITY;
        --ctx->debug.queue_count;
        ++returned;
    }

    return returned;
}

void glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message) {
    struct mgl_context *ctx = mgl_context_current();

    if (ctx == NULL) {
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (source != GL_DEBUG_SOURCE_APPLICATION && source != GL_DEBUG_SOURCE_THIRD_PARTY) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (ctx->debug.group_depth >= MGL_DEBUG_GROUP_MAX_DEPTH) {
        mgl_error_set(&ctx->errors, GL_STACK_OVERFLOW);
        return;
    }
    if (message == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }

    ++ctx->debug.group_depth;
    push_message(ctx, source, GL_DEBUG_TYPE_PUSH_GROUP, id, GL_DEBUG_SEVERITY_NOTIFICATION, length,
                 message);
}

void glPopDebugGroup(void) {
    struct mgl_context *ctx = mgl_context_current();

    if (ctx == NULL) {
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->debug.group_depth == 0u) {
        mgl_error_set(&ctx->errors, GL_STACK_UNDERFLOW);
        return;
    }

    --ctx->debug.group_depth;
    push_message(ctx, GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_POP_GROUP, 0u,
                 GL_DEBUG_SEVERITY_NOTIFICATION, -1, "mGL debug group pop");
}

static struct mgl_object_label **label_link(struct mgl_context *ctx, GLenum identifier,
                                            GLuint name) {
    struct mgl_object_label **link = &ctx->share_group->object_labels;

    while (*link != NULL && ((*link)->identifier != identifier || (*link)->name != name)) {
        link = &(*link)->next;
    }
    return link;
}

void glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_object_label **link;
    struct mgl_object_label *object;

    if (ctx == NULL) {
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (name == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }

    link = label_link(ctx, identifier, name);
    if (label == NULL) {
        if (*link != NULL) {
            object = *link;
            *link = object->next;
            free(object);
        }
        return;
    }

    if (*link == NULL) {
        *link = calloc(1u, sizeof(**link));
        if (*link == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        (*link)->identifier = identifier;
        (*link)->name = name;
    }

    if (!copy_text((*link)->text, sizeof((*link)->text), length, label, NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
    }
}

void glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length,
                      GLchar *label) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_object_label **link;
    size_t source_length;

    if (ctx == NULL) {
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }

    link = label_link(ctx, identifier, name);
    source_length = *link != NULL ? strlen((*link)->text) : 0u;
    if (length != NULL) {
        *length = (GLsizei)source_length;
    }
    if (label != NULL && bufSize > 0) {
        size_t copy_length =
            source_length < (size_t)(bufSize - 1) ? source_length : (size_t)(bufSize - 1);
        if (*link != NULL) {
            memcpy(label, (*link)->text, copy_length);
        }
        label[copy_length] = '\0';
    }
}

static struct mgl_debug_ptr_label **ptr_label_link(struct mgl_context *ctx, const void *ptr) {
    struct mgl_debug_ptr_label **link = &ctx->debug.ptr_labels;

    while (*link != NULL && (*link)->ptr != ptr) {
        link = &(*link)->next;
    }
    return link;
}

void glObjectPtrLabel(const void *ptr, GLsizei length, const GLchar *label) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_debug_ptr_label **link;
    struct mgl_debug_ptr_label *object;

    if (ctx == NULL) {
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ptr == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }

    link = ptr_label_link(ctx, ptr);
    if (label == NULL) {
        if (*link != NULL) {
            object = *link;
            *link = object->next;
            free(object);
        }
        return;
    }

    if (*link == NULL) {
        *link = calloc(1u, sizeof(**link));
        if (*link == NULL) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        (*link)->ptr = ptr;
    }

    if (!copy_text((*link)->text, sizeof((*link)->text), length, label, NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
    }
}

void glGetObjectPtrLabel(const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_debug_ptr_label **link;
    size_t source_length;

    if (ctx == NULL) {
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (bufSize < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }

    link = ptr_label_link(ctx, ptr);
    source_length = *link != NULL ? strlen((*link)->text) : 0u;
    if (length != NULL) {
        *length = (GLsizei)source_length;
    }
    if (label != NULL && bufSize > 0) {
        size_t copy_length =
            source_length < (size_t)(bufSize - 1) ? source_length : (size_t)(bufSize - 1);
        if (*link != NULL) {
            memcpy(label, (*link)->text, copy_length);
        }
        label[copy_length] = '\0';
    }
}

void glGetPointerv(GLenum pname, void **params) {
    struct mgl_context *ctx = mgl_context_current();

    if (ctx == NULL || params == NULL) {
        return;
    }
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }

    if (pname == GL_DEBUG_CALLBACK_FUNCTION) {
        /* Copy the callback representation with memcpy to support function-pointer and
         * data-pointer representations with distinct types. */
        size_t copy_size = sizeof(ctx->debug.callback) < sizeof(*params)
                               ? sizeof(ctx->debug.callback)
                               : sizeof(*params);
        memset(params, 0, sizeof(*params));
        memcpy(params, &ctx->debug.callback, copy_size);
        {
            uintptr_t bits = 0u;
            size_t bytes = sizeof(bits) < sizeof(*params) ? sizeof(bits) : sizeof(*params);
            memcpy(&bits, params, bytes);
            mgl_render_diag_detail(ctx, "glGetPointerv", "returned_callback", bits,
                                   (uintptr_t)ctx->debug.callback_user);
        }
        return;
    }
    if (pname == GL_DEBUG_CALLBACK_USER_PARAM) {
        *params = (void *)ctx->debug.callback_user;
        return;
    }
    if (pname == GL_FEEDBACK_BUFFER_POINTER) {
        *params = ctx->selection.feedback_buffer;
        return;
    }

    {
        const struct mgl_vertex_binding *binding = NULL;
        switch (pname) {
            case GL_VERTEX_ARRAY_POINTER:
                binding = &ctx->current_vao->compat[MGL_COMPAT_SEM_POSITION];
                break;
            case GL_EDGE_FLAG_ARRAY_POINTER:
                binding = &ctx->current_vao->edge_flag;
                break;
            case GL_INDEX_ARRAY_POINTER:
                binding = &ctx->current_vao->color_index;
                break;
            case GL_NORMAL_ARRAY_POINTER:
                binding = &ctx->current_vao->compat[MGL_COMPAT_SEM_NORMAL];
                break;
            case GL_COLOR_ARRAY_POINTER:
                binding = &ctx->current_vao->compat[MGL_COMPAT_SEM_COLOR];
                break;
            case GL_SECONDARY_COLOR_ARRAY_POINTER:
                binding = &ctx->current_vao->compat[MGL_COMPAT_SEM_SECONDARY];
                break;
            case GL_FOG_COORD_ARRAY_POINTER:
                binding = &ctx->current_vao->compat[MGL_COMPAT_SEM_FOG];
                break;
            case GL_TEXTURE_COORD_ARRAY_POINTER:
                if (ctx->client_active_texture < MGL_TEXTURE_UNITS)
                    binding = &ctx->current_vao
                                   ->compat[MGL_COMPAT_SEM_TEXCOORD0 + ctx->client_active_texture];
                break;
            default:
                break;
        }
        if (binding != NULL) {
            *params = binding->data.source == MGL_DATA_BUFFER
                          ? (void *)(uintptr_t)binding->data.offset
                          : (void *)binding->data.client_ptr;
            return;
        }
    }

    mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
}

void glDebugMessageCallbackARB(GLDEBUGPROC callback, const void *userParam) {
    glDebugMessageCallback(callback, userParam);
}

void glDebugMessageControlARB(GLenum source, GLenum type, GLenum severity, GLsizei count,
                              const GLuint *ids, GLboolean enabled) {
    glDebugMessageControl(source, type, severity, count, ids, enabled);
}

void glDebugMessageInsertARB(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                             const GLchar *buf) {
    glDebugMessageInsert(source, type, id, severity, length, buf);
}

GLuint glGetDebugMessageLogARB(GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types,
                               GLuint *ids, GLenum *severities, GLsizei *lengths,
                               GLchar *messageLog) {
    return glGetDebugMessageLog(count, bufSize, sources, types, ids, severities, lengths,
                                messageLog);
}

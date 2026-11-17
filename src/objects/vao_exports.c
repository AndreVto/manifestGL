#include "mgl/gl.h"

#include "core/context.h"
#include "objects/object_store.h"

#include <stdlib.h>

void glGenVertexArrays(GLsizei n, GLuint *arrays) {
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
    if (n == 0 || arrays == NULL)
        return;
    for (i = 0; i < n; ++i) {
        GLuint name = ctx->next_vao_name++;
        struct mgl_vertex_array *obj;
        if (name == 0u)
            name = ctx->next_vao_name++;
        obj = mgl_vao_create_reserved(ctx, name);
        if (obj == NULL) {
            arrays[i] = 0u;
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        } else {
            arrays[i] = name;
        }
    }
}

void glCreateVertexArrays(GLsizei n, GLuint *arrays) {
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
    if (n == 0 || arrays == NULL)
        return;
    glGenVertexArrays(n, arrays);
    for (i = 0; i < n; ++i) {
        struct mgl_vertex_array *vao = mgl_vao_find(ctx, arrays[i]);
        if (vao != NULL && arrays[i] != 0u)
            vao->realized = GL_TRUE;
    }
}

void glDeleteVertexArrays(GLsizei n, const GLuint *arrays) {
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
    if (n == 0 || arrays == NULL)
        return;
    for (i = 0; i < n; ++i)
        mgl_vao_remove(ctx, arrays[i]);
    ctx->draw_cached_vao = NULL;
    ctx->draw_enabled_mask_valid = GL_FALSE;
    ctx->draw_native_pointer_valid = 0u;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindVertexArray != NULL &&
        ctx->current_vao == &ctx->default_vao) {
        ctx->native_dispatch->glesBindVertexArray(0u);
    }
}

void glBindVertexArray(GLuint array) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_array *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (array == 0u) {
        ctx->current_vao = &ctx->default_vao;
        ctx->draw_native_pointer_valid = 0u;
        ctx->draw_enabled_mask_valid = GL_FALSE;
        if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindVertexArray != NULL)
            ctx->native_dispatch->glesBindVertexArray(0u);
        return;
    }
    obj = mgl_vao_find(ctx, array);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj->realized = GL_TRUE;
    ctx->current_vao = obj;
    ctx->draw_native_pointer_valid = 0u;
    ctx->draw_enabled_mask_valid = GL_FALSE;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindVertexArray != NULL)
        ctx->native_dispatch->glesBindVertexArray(obj->native_name);
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindBuffer != NULL) {
        struct mgl_buffer *element = mgl_buffer_find(ctx, obj->element_array_buffer);
        mgl_native_dispatch_bind_buffer(ctx, GL_ELEMENT_ARRAY_BUFFER,
                                        element != NULL ? element->native_name : 0u);
    }
}

GLboolean glIsVertexArray(GLuint array) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_array *obj;
    if (ctx == NULL)
        return GL_FALSE;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    if (array == 0u)
        return GL_FALSE;
    obj = mgl_vao_find(ctx, array);
    return obj != NULL ? obj->realized : GL_FALSE;
}

static int mgl_named_vao_attrib_index_valid(struct mgl_context *ctx, GLuint index) {
    GLint max_attribs = 0;
    if (ctx == NULL || index >= MGL_MAX_VERTEX_ATTRIBS)
        return 0;
    if (ctx->native_max_vertex_attribs <= 0 && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesGetIntegerv != NULL) {
        ctx->native_dispatch->glesGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_attribs);
        if (max_attribs <= 0 || max_attribs > (GLint)MGL_MAX_VERTEX_ATTRIBS)
            max_attribs = (GLint)MGL_MAX_VERTEX_ATTRIBS;
        ctx->native_max_vertex_attribs = max_attribs;
    }
    if (ctx->native_max_vertex_attribs <= 0)
        ctx->native_max_vertex_attribs = (GLint)MGL_MAX_VERTEX_ATTRIBS;
    return index < (GLuint)ctx->native_max_vertex_attribs;
}

static void mgl_set_named_vao_attrib_enabled(GLuint vaobj, GLuint index, GLboolean enabled) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_array *vao;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!mgl_named_vao_attrib_index_valid(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (vaobj == 0u || (vao = mgl_vao_find(ctx, vaobj)) == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    vao->realized = GL_TRUE;
    vao->generic[index].enabled = enabled;
}

void glEnableVertexArrayAttrib(GLuint vaobj, GLuint index) {
    mgl_set_named_vao_attrib_enabled(vaobj, index, GL_TRUE);
}

void glDisableVertexArrayAttrib(GLuint vaobj, GLuint index) {
    mgl_set_named_vao_attrib_enabled(vaobj, index, GL_FALSE);
}

void glEnableVertexArrayAttribEXT(GLuint vaobj, GLuint index) {
    glEnableVertexArrayAttrib(vaobj, index);
}

void glDisableVertexArrayAttribEXT(GLuint vaobj, GLuint index) {
    glDisableVertexArrayAttrib(vaobj, index);
}

static struct mgl_vertex_array *mgl_query_vertex_array(struct mgl_context *ctx, GLuint vaobj) {
    struct mgl_vertex_array *vao = vaobj ? mgl_vao_find(ctx, vaobj) : NULL;
    if (vao == NULL || !vao->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    return vao;
}

void glGetVertexArrayiv(GLuint vaobj, GLenum pname, GLint *param) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_array *vao;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((vao = mgl_query_vertex_array(ctx, vaobj)) == NULL)
        return;
    if (pname != GL_ELEMENT_ARRAY_BUFFER_BINDING) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (param != NULL)
        *param = (GLint)vao->element_array_buffer;
}

void glGetVertexArrayIndexediv(GLuint vaobj, GLuint index, GLenum pname, GLint *param) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_array *vao;
    const struct mgl_vertex_binding *attr;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((vao = mgl_query_vertex_array(ctx, vaobj)) == NULL)
        return;
    if (pname == GL_VERTEX_BINDING_BUFFER || pname == GL_VERTEX_BINDING_STRIDE ||
        pname == GL_VERTEX_BINDING_DIVISOR || pname == GL_VERTEX_BINDING_OFFSET) {
        if (index >= mgl_context_vertex_binding_limit(ctx)) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return;
        }
    } else if (!mgl_named_vao_attrib_index_valid(ctx, index)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    attr = &vao->generic[index];
    if (param == NULL)
        return;
    switch (pname) {
        case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
            *param = attr->enabled ? 1 : 0;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_SIZE:
            *param = attr->size;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
            *param = attr->separate_stride ? attr->stride : attr->attrib_array_stride;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_TYPE:
            *param = (GLint)attr->type;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
            *param = attr->normalized ? 1 : 0;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
            *param = attr->integer ? 1 : 0;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_LONG:
            *param = 0;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
            *param = (GLint)attr->data.buffer_name;
            break;
        case GL_VERTEX_ATTRIB_BINDING:
            *param = (GLint)vao->vertex_attrib_binding[index];
            break;
        case GL_VERTEX_BINDING_BUFFER:
            *param = (GLint)vao->vertex_buffers[index].data.buffer_name;
            break;
        case GL_VERTEX_BINDING_STRIDE:
            *param = vao->vertex_buffers[index].stride;
            break;
        case GL_VERTEX_BINDING_DIVISOR:
            *param = (GLint)vao->vertex_buffers[index].divisor;
            break;
        case GL_VERTEX_BINDING_OFFSET:
            *param = (GLint)vao->vertex_buffers[index].data.offset;
            break;
        case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
            *param = (GLint)attr->divisor;
            break;
        case GL_VERTEX_ATTRIB_RELATIVE_OFFSET:
            *param = (GLint)vao->vertex_attrib_relative_offset[index];
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
}

void glGetVertexArrayIndexed64iv(GLuint vaobj, GLuint index, GLenum pname, GLint64 *param) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_vertex_array *vao;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((vao = mgl_query_vertex_array(ctx, vaobj)) == NULL)
        return;
    if (pname != GL_VERTEX_BINDING_OFFSET) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (index >= mgl_context_vertex_binding_limit(ctx)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (param != NULL)
        *param = (GLint64)vao->vertex_buffers[index].data.offset;
}

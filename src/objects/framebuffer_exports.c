#include "mgl/gl.h"

#include "core/context.h"
#include "core/render_diag.h"
#include "objects/object_store.h"
#include "list/display_list.h"
#include "texture/texture_store.h"
#include "texture/renderable_formats.h"

#include <stdatomic.h>

static int valid_framebuffer_target(GLenum target) {
    return target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER ||
           target == GL_READ_FRAMEBUFFER;
}

static int color_attachment_index(GLenum buffer) {
    if (buffer >= GL_COLOR_ATTACHMENT0 && buffer < GL_COLOR_ATTACHMENT0 + MGL_MAX_DRAW_BUFFERS)
        return (int)(buffer - GL_COLOR_ATTACHMENT0);
    return -1;
}

static GLint framebuffer_limit(const struct mgl_context *ctx, GLenum pname) {
    GLint limit = MGL_MAX_DRAW_BUFFERS;
    if (ctx != NULL && ctx->native_dispatch != NULL &&
        ctx->native_dispatch->glesGetIntegerv != NULL) {
        GLint value = 0;
        ctx->native_dispatch->glesGetIntegerv(pname, &value);
        if (value > 0 && value < limit)
            limit = value;
    }
    return limit;
}

static int default_buffer_enum(GLenum buffer) {
    switch (buffer) {
        case GL_NONE:
        case GL_FRONT_LEFT:
        case GL_BACK_LEFT:
        case GL_FRONT:
        case GL_BACK:
        case GL_LEFT:
        case GL_FRONT_AND_BACK:
            return 1;
        case GL_FRONT_RIGHT:
        case GL_BACK_RIGHT:
        case GL_RIGHT:
            return 2;
        default:
            if (buffer >= GL_AUX0 && buffer < GL_AUX0 + 4u)
                return 2;
            return 0;
    }
}

static GLenum *current_draw_buffers(struct mgl_context *ctx) {
    struct mgl_framebuffer *fb;
    if (ctx == NULL)
        return NULL;
    if (ctx->draw_framebuffer == 0u)
        return ctx->default_draw_buffers;
    fb = mgl_framebuffer_find(ctx, ctx->draw_framebuffer);
    return fb != NULL ? fb->draw_buffers : NULL;
}

static GLenum *current_read_buffer(struct mgl_context *ctx) {
    struct mgl_framebuffer *fb;
    if (ctx == NULL)
        return NULL;
    if (ctx->read_framebuffer == 0u)
        return &ctx->default_read_buffer;
    fb = mgl_framebuffer_find(ctx, ctx->read_framebuffer);
    return fb != NULL ? &fb->read_buffer : NULL;
}

static int default_buffer_is_back_left(GLenum buffer) {
    return buffer == GL_BACK || buffer == GL_BACK_LEFT;
}

static int default_draw_buffers_value(GLenum buffer) {
    return buffer == GL_NONE || buffer == GL_FRONT_LEFT || buffer == GL_FRONT_RIGHT ||
           buffer == GL_BACK_LEFT || buffer == GL_BACK_RIGHT || buffer == GL_BACK;
}

static void lower_draw_buffers_for_framebuffer(struct mgl_context *ctx, int default_framebuffer,
                                               GLsizei n, const GLenum *buffers) {
    GLenum translated;
    if (ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesDrawBuffers == NULL)
        return;
    if (default_framebuffer) {
        if (n == 0) {
            ctx->native_dispatch->glesDrawBuffers(0, NULL);
            return;
        }
        /* The native default framebuffer provides one GLES back-buffer selection. Retain desktop
         * front/back-left selections in canonical state and restrict native dispatch to valid
         * GLES targets. */
        if (n != 1 || (buffers[0] != GL_NONE && !default_buffer_is_back_left(buffers[0]))) {
            mgl_render_diag_skip(ctx, "glDrawBuffers", "default_selection", (uint64_t)n,
                                 n > 0 ? (uint64_t)buffers[0] : 0u);
            return;
        }
        translated = buffers[0] == GL_NONE
                         ? GL_NONE
                         : (ctx->native_default_framebuffer ? GL_COLOR_ATTACHMENT0 : GL_BACK);
        ctx->native_dispatch->glesDrawBuffers(1, &translated);
        return;
    }
    {
        GLsizei i;
        for (i = 0; i < n; ++i) {
            if (buffers[i] != GL_NONE && buffers[i] != GL_COLOR_ATTACHMENT0 + (GLenum)i)
                return;
        }
    }
    ctx->native_dispatch->glesDrawBuffers(n, buffers);
}

static int routed_attach(struct mgl_context *ctx, GLenum attachment, GLuint texture,
                         GLuint renderbuffer, GLint level, GLint layer, GLenum target) {
    struct mgl_texture *tex;
    struct mgl_renderbuffer *rb;
    struct mgl_native_dispatch *b = ctx->native_dispatch;
    GLuint native_texture;
    if (texture != 0u) {
        tex = mgl_texture_find(ctx, texture);
        if (tex == NULL || !tex->realized)
            return 0;
        native_texture = tex->native_name;
        if (layer == -2) {
            if (b->glesFramebufferTexture == NULL)
                return 0;
            b->glesFramebufferTexture(GL_DRAW_FRAMEBUFFER, attachment, native_texture, level);
        } else if (layer >= 0) {
            if (b->glesFramebufferTextureLayer == NULL)
                return 0;
            b->glesFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, attachment, native_texture, level,
                                           layer);
        } else {
            if (b->glesFramebufferTexture2D == NULL)
                return 0;
            b->glesFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attachment,
                                        mgl_texture_native_target(target), native_texture, level);
        }
        return 1;
    }
    if (b->glesFramebufferRenderbuffer == NULL)
        return 0;
    if (renderbuffer != 0u) {
        rb = mgl_renderbuffer_find(ctx, renderbuffer);
        if (rb == NULL || !rb->realized)
            return 0;
        b->glesFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, attachment, GL_RENDERBUFFER,
                                       rb->native_name);
    } else {
        b->glesFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, attachment, GL_RENDERBUFFER, 0u);
    }
    return 1;
}

int mgl_framebuffer_draw_route_begin_for(struct mgl_context *ctx, struct mgl_framebuffer *fb) {
    struct mgl_native_dispatch *b;
    GLenum native_buffers[MGL_MAX_DRAW_BUFFERS];
    unsigned i, n = 0u;
    int needs_routing = 0;
    if (ctx == NULL || fb == NULL)
        return 0;
    b = ctx->native_dispatch;
    for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i) {
        if (fb->draw_buffers[i] != GL_NONE && fb->draw_buffers[i] != GL_COLOR_ATTACHMENT0 + i)
            needs_routing = 1;
    }
    if (!needs_routing)
        return 0;
    if (b == NULL || b->glesGenFramebuffers == NULL || b->glesBindFramebuffer == NULL ||
        b->glesDrawBuffers == NULL || b->glesFramebufferRenderbuffer == NULL)
        goto failed;
    if (fb->routed_draw_native_name == 0u)
        b->glesGenFramebuffers(1, &fb->routed_draw_native_name);
    if (fb->routed_draw_native_name == 0u)
        goto failed;
    b->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb->routed_draw_native_name);
    for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i) {
        unsigned source;
        GLuint texture, renderbuffer;
        GLint level, layer;
        GLenum target;
        if (fb->draw_buffers[i] == GL_NONE) {
            native_buffers[i] = GL_NONE;
            if (!routed_attach(ctx, GL_COLOR_ATTACHMENT0 + i, 0u, 0u, 0, -1, 0u))
                goto failed_restore;
            continue;
        }
        source = (unsigned)(fb->draw_buffers[i] - GL_COLOR_ATTACHMENT0);
        if (source >= MGL_MAX_DRAW_BUFFERS)
            goto failed_restore;
        texture = source == 0u ? fb->color_texture : fb->extra_color_textures[source];
        renderbuffer =
            source == 0u ? fb->color_renderbuffer : fb->extra_color_renderbuffers[source];
        level = source == 0u ? fb->color_texture_level : fb->extra_color_levels[source];
        layer = source == 0u ? fb->color_texture_layer : fb->extra_color_layers[source];
        target = source == 0u ? fb->color_texture_target : fb->extra_color_targets[source];
        if (!routed_attach(ctx, GL_COLOR_ATTACHMENT0 + i, texture, renderbuffer, level, layer,
                           target))
            goto failed_restore;
        native_buffers[i] = GL_COLOR_ATTACHMENT0 + i;
        n = i + 1u;
    }
    if (!routed_attach(ctx, GL_DEPTH_ATTACHMENT, fb->depth_texture, fb->depth_renderbuffer,
                       fb->depth_texture_level, fb->depth_texture_layer,
                       fb->depth_texture_target) ||
        !routed_attach(ctx, GL_STENCIL_ATTACHMENT, fb->stencil_texture, fb->stencil_renderbuffer,
                       fb->stencil_texture_level, fb->stencil_texture_layer,
                       fb->stencil_texture_target))
        goto failed_restore;
    if (n == 0u) {
        native_buffers[0] = GL_NONE;
        n = 1u;
    }
    b->glesDrawBuffers((GLsizei)n, native_buffers);
    return 1;
failed_restore:
    b->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb->native_name);
failed:
    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    return -1;
}

int mgl_framebuffer_draw_route_begin(struct mgl_context *ctx) {
    if (ctx == NULL || ctx->draw_framebuffer == 0u)
        return 0;
    return mgl_framebuffer_draw_route_begin_for(ctx,
                                                mgl_framebuffer_find(ctx, ctx->draw_framebuffer));
}

void mgl_framebuffer_draw_route_end(struct mgl_context *ctx) {
    struct mgl_framebuffer *fb;
    if (ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesBindFramebuffer == NULL)
        return;
    fb = mgl_framebuffer_find(ctx, ctx->draw_framebuffer);
    ctx->native_dispatch->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                              fb != NULL ? fb->native_name : 0u);
}

static void lower_read_buffer_for_framebuffer(struct mgl_context *ctx, int default_framebuffer,
                                              GLenum buffer) {
    if (ctx == NULL || ctx->native_dispatch == NULL || ctx->native_dispatch->glesReadBuffer == NULL)
        return;
    if (default_framebuffer) {
        if (buffer != GL_NONE && !default_buffer_is_back_left(buffer)) {
            mgl_render_diag_skip(ctx, "glReadBuffer", "default_selection", (uint64_t)buffer, 0u);
            return;
        }
        buffer = buffer == GL_NONE
                     ? GL_NONE
                     : (ctx->native_default_framebuffer ? GL_COLOR_ATTACHMENT0 : GL_BACK);
    }
    ctx->native_dispatch->glesReadBuffer(buffer);
}

static int validate_draw_buffer_selection(struct mgl_context *ctx, int default_framebuffer,
                                          GLenum buffer) {
    int kind;
    if (default_framebuffer) {
        kind = default_buffer_enum(buffer);
        if (kind == 0) {
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return 0;
        }
        if (kind == 2) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        return 1;
    }
    if (buffer == GL_NONE ||
        (color_attachment_index(buffer) >= 0 &&
         color_attachment_index(buffer) < framebuffer_limit(ctx, GL_MAX_COLOR_ATTACHMENTS)))
        return 1;
    if ((buffer >= GL_COLOR_ATTACHMENT0 && buffer <= GL_COLOR_ATTACHMENT0 + 31u) ||
        default_buffer_enum(buffer))
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    else
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
    return 0;
}

static int validate_draw_buffers_selection(struct mgl_context *ctx, int default_framebuffer,
                                           GLsizei n, const GLenum *buffers) {
    GLsizei i, j;
    if (n < 0 || n > framebuffer_limit(ctx, GL_MAX_DRAW_BUFFERS) || (n > 0 && buffers == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    for (i = 0; i < n; ++i) {
        if (default_framebuffer) {
            if (!default_draw_buffers_value(buffers[i])) {
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return 0;
            }
            if (buffers[i] == GL_FRONT_RIGHT || buffers[i] == GL_BACK_RIGHT) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return 0;
            }
            if (buffers[i] == GL_BACK && n != 1) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return 0;
            }
        } else {
            if (buffers[i] != GL_NONE && (color_attachment_index(buffers[i]) < 0 ||
                                          color_attachment_index(buffers[i]) >=
                                              framebuffer_limit(ctx, GL_MAX_COLOR_ATTACHMENTS))) {
                if (buffers[i] >= GL_COLOR_ATTACHMENT0 && buffers[i] <= GL_COLOR_ATTACHMENT0 + 31u)
                    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                else
                    mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return 0;
            }
        }
        if (buffers[i] != GL_NONE) {
            for (j = 0; j < i; ++j) {
                if (buffers[j] == buffers[i]) {
                    mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                    return 0;
                }
            }
        }
    }
    return 1;
}

static int validate_read_buffer_selection(struct mgl_context *ctx, int default_framebuffer,
                                          GLenum buffer) {
    int kind;
    if (default_framebuffer) {
        kind = default_buffer_enum(buffer);
        if (kind == 0) {
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return 0;
        }
        if (kind == 2) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        return 1;
    }
    if (buffer == GL_NONE ||
        (color_attachment_index(buffer) >= 0 &&
         color_attachment_index(buffer) < framebuffer_limit(ctx, GL_MAX_COLOR_ATTACHMENTS)))
        return 1;
    if ((buffer >= GL_COLOR_ATTACHMENT0 && buffer <= GL_COLOR_ATTACHMENT0 + 31u) ||
        default_buffer_enum(buffer))
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    else
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
    return 0;
}

static GLenum renderbuffer_storage_format(GLenum format) {
    switch (format) {
        case GL_DEPTH_COMPONENT:
            return GL_DEPTH_COMPONENT24;
        case GL_DEPTH_COMPONENT32:
            return GL_DEPTH_COMPONENT32F;
        case GL_RGB5:
            return GL_RGB565;
        case GL_RGB16F:
            return GL_RGBA16F;
        case GL_RGB32F_ARB:
            return GL_RGBA32F;
        case GL_STENCIL_INDEX:
            return GL_STENCIL_INDEX8;
        default:
            return format;
    }
}

static GLint renderbuffer_depth_size(GLenum format) {
    switch (format) {
        case GL_DEPTH_COMPONENT16:
            return 16;
        case GL_DEPTH_COMPONENT24:
        case GL_DEPTH24_STENCIL8:
            return 24;
        case GL_DEPTH_COMPONENT32F:
        case GL_DEPTH32F_STENCIL8:
            return 32;
        default:
            return 0;
    }
}

static void mgl_restore_native_framebuffers(struct mgl_context *ctx);

static GLint *framebuffer_default_parameter(struct mgl_framebuffer *fb, GLenum pname) {
    switch (pname) {
        case GL_FRAMEBUFFER_DEFAULT_WIDTH:
            return &fb->default_width;
        case GL_FRAMEBUFFER_DEFAULT_HEIGHT:
            return &fb->default_height;
        case GL_FRAMEBUFFER_DEFAULT_LAYERS:
            return &fb->default_layers;
        case GL_FRAMEBUFFER_DEFAULT_SAMPLES:
            return &fb->default_samples;
        default:
            return NULL;
    }
}

static int named_framebuffer_default_parameter_valid(struct mgl_context *ctx, GLenum pname,
                                                     GLint param) {
    GLenum limit = 0u;
    GLint maximum = 0;
    switch (pname) {
        case GL_FRAMEBUFFER_DEFAULT_WIDTH:
            limit = 0x9315u;
            break;
        case GL_FRAMEBUFFER_DEFAULT_HEIGHT:
            limit = 0x9316u;
            break;
        case GL_FRAMEBUFFER_DEFAULT_LAYERS:
            limit = 0x9317u;
            break;
        case GL_FRAMEBUFFER_DEFAULT_SAMPLES:
            limit = 0x9318u;
            break;
        case GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS:
            if (param != 0 && param != 1) {
                mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
                return 0;
            }
            return 1;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return 0;
    }
    if (param < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetIntegerv != NULL) {
        ctx->native_dispatch->glesGetIntegerv(limit, &maximum);
        if (maximum > 0 && param > maximum) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return 0;
        }
    }
    return 1;
}

void glFramebufferParameteri(GLenum target, GLenum pname, GLint param) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *fb;
    GLint *slot;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_framebuffer_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    fb = mgl_bound_framebuffer(ctx, target);
    if (fb == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!named_framebuffer_default_parameter_valid(ctx, pname, param))
        return;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesFramebufferParameteri == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesFramebufferParameteri(target, pname, param);
    slot = framebuffer_default_parameter(fb, pname);
    if (slot != NULL)
        *slot = param;
    else
        fb->default_fixed_sample_locations = param ? GL_TRUE : GL_FALSE;
}

void glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *fb;
    GLint *slot;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_framebuffer_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (pname != GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS &&
        pname != GL_FRAMEBUFFER_DEFAULT_WIDTH && pname != GL_FRAMEBUFFER_DEFAULT_HEIGHT &&
        pname != GL_FRAMEBUFFER_DEFAULT_LAYERS && pname != GL_FRAMEBUFFER_DEFAULT_SAMPLES) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    fb = mgl_bound_framebuffer(ctx, target);
    if (fb == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (params == NULL)
        return;
    slot = framebuffer_default_parameter(fb, pname);
    *params = slot != NULL ? *slot : (GLint)fb->default_fixed_sample_locations;
}

void glNamedFramebufferParameteri(GLuint framebuffer, GLenum pname, GLint param) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *fb;
    GLint *slot;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    fb = mgl_framebuffer_find(ctx, framebuffer);
    if (fb == NULL || !fb->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!named_framebuffer_default_parameter_valid(ctx, pname, param))
        return;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesBindFramebuffer == NULL ||
        ctx->native_dispatch->glesFramebufferParameteri == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->native_dispatch->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb->native_name);
    ctx->native_dispatch->glesFramebufferParameteri(GL_DRAW_FRAMEBUFFER, pname, param);
    mgl_restore_native_framebuffers(ctx);
    slot = framebuffer_default_parameter(fb, pname);
    if (slot != NULL)
        *slot = param;
    else
        fb->default_fixed_sample_locations = param ? GL_TRUE : GL_FALSE;
}

void glGetNamedFramebufferParameteriv(GLuint framebuffer, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *fb;
    GLint *slot;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    fb = mgl_framebuffer_find(ctx, framebuffer);
    if (fb == NULL || !fb->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    slot = framebuffer_default_parameter(fb, pname);
    if (slot == NULL && pname != GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (params != NULL)
        *params = slot != NULL ? *slot : (GLint)fb->default_fixed_sample_locations;
}

void glGenFramebuffers(GLsizei n, GLuint *framebuffers) {
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
    if (n == 0 || framebuffers == NULL)
        return;
    for (i = 0; i < n; ++i) {
        GLuint name = ctx->next_framebuffer_name++;
        struct mgl_framebuffer *obj;
        if (name == 0u)
            name = ctx->next_framebuffer_name++;
        obj = mgl_framebuffer_ensure(ctx, name, 1);
        if (obj == NULL || obj->native_name == 0u) {
            framebuffers[i] = 0u;
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        } else
            framebuffers[i] = name;
    }
}

void glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers) {
    struct mgl_context *ctx = mgl_context_current();
    GLsizei i;
    int rebind_draw = 0, rebind_read = 0;
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
    if (n == 0 || framebuffers == NULL)
        return;
    for (i = 0; i < n; ++i) {
        if (ctx->draw_framebuffer == framebuffers[i])
            rebind_draw = 1;
        if (ctx->read_framebuffer == framebuffers[i])
            rebind_read = 1;
        mgl_framebuffer_remove(ctx, framebuffers[i]);
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindFramebuffer != NULL) {
        if (rebind_draw && rebind_read)
            ctx->native_dispatch->glesBindFramebuffer(GL_FRAMEBUFFER, 0u);
        else {
            if (rebind_draw)
                ctx->native_dispatch->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0u);
            if (rebind_read)
                ctx->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER, 0u);
        }
    }
}

void glBindFramebuffer(GLenum target, GLuint framebuffer) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_FRAMEBUFFER);
    struct mgl_framebuffer *obj = NULL;
    GLuint native_name = 0u;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_framebuffer_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (framebuffer != 0u) {
        obj = mgl_framebuffer_find(ctx, framebuffer);
        if (obj == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (obj->native_name == 0u) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
        obj->realized = GL_TRUE;
        native_name = obj->native_name;
    }
    if (target == GL_FRAMEBUFFER) {
        ctx->draw_framebuffer = framebuffer;
        ctx->read_framebuffer = framebuffer;
    } else if (target == GL_DRAW_FRAMEBUFFER)
        ctx->draw_framebuffer = framebuffer;
    else
        ctx->read_framebuffer = framebuffer;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindFramebuffer != NULL) {
        ctx->native_dispatch->glesBindFramebuffer(target, native_name);
        if (obj != NULL)
            (void)mgl_texture_refresh_framebuffer_attachments(ctx, obj);
    }
}

GLboolean glIsFramebuffer(GLuint framebuffer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *obj;
    if (ctx == NULL)
        return GL_FALSE;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    if (framebuffer == 0u)
        return GL_FALSE;
    obj = mgl_framebuffer_find(ctx, framebuffer);
    return obj != NULL ? obj->realized : GL_FALSE;
}

static void note_framebuffer_color_status(struct mgl_context *ctx, struct mgl_framebuffer *fb,
                                          GLenum status) {
    GLuint names[MGL_MAX_DRAW_BUFFERS];
    unsigned i;
    if (fb == NULL)
        return;
    names[0] = fb->color_texture;
    for (i = 1u; i < MGL_MAX_DRAW_BUFFERS; ++i)
        names[i] = fb->extra_color_textures[i];
    for (i = 0u; i < MGL_MAX_DRAW_BUFFERS; ++i) {
        struct mgl_texture *tex = mgl_texture_find(ctx, names[i]);
        if (tex == NULL)
            continue;
        if (status == GL_FRAMEBUFFER_COMPLETE)
            tex->color_attachment_complete = GL_TRUE;
        else if (!tex->color_attachment_complete)
            tex->color_attachment_incomplete = GL_TRUE;
    }
}

GLenum glCheckFramebufferStatus(GLenum target) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_FRAMEBUFFER);
    GLenum status;
    GLuint name;
    if (ctx == NULL)
        return GL_FRAMEBUFFER_UNDEFINED;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0u;
    }
    if (!valid_framebuffer_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0u;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesCheckFramebufferStatus != NULL) {
        int route = 0;
        mgl_texture_refresh_current_framebuffers(ctx);
        if (target != GL_READ_FRAMEBUFFER)
            route = mgl_framebuffer_draw_route_begin(ctx);
        if (route < 0)
            return GL_FRAMEBUFFER_UNSUPPORTED;
        status = ctx->native_dispatch->glesCheckFramebufferStatus(route > 0 ? GL_DRAW_FRAMEBUFFER
                                                                            : target);
        if (route > 0)
            mgl_framebuffer_draw_route_end(ctx);
        name = target == GL_READ_FRAMEBUFFER ? ctx->read_framebuffer : ctx->draw_framebuffer;
        if (name != 0u)
            note_framebuffer_color_status(ctx, mgl_framebuffer_find(ctx, name), status);
        mgl_render_diag_framebuffer(ctx, "glCheckFramebufferStatus", target, name, status);
        return status;
    }
    return GL_FRAMEBUFFER_UNSUPPORTED;
}

void glDrawBuffer(GLenum buf) {
    struct mgl_context *ctx = mgl_context_current();
    GLenum *state;
    unsigned i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!validate_draw_buffer_selection(ctx, ctx->draw_framebuffer == 0u, buf))
        return;
    if (mgl_list_record_draw_buffer(ctx, buf))
        return;
    state = current_draw_buffers(ctx);
    if (state == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    state[0] = buf;
    for (i = 1u; i < MGL_MAX_DRAW_BUFFERS; ++i)
        state[i] = GL_NONE;
    lower_draw_buffers_for_framebuffer(ctx, ctx->draw_framebuffer == 0u, 1, state);
}

void glDrawBuffers(GLsizei n, const GLenum *bufs) {
    struct mgl_context *ctx = mgl_context_current();
    GLenum *state;
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!validate_draw_buffers_selection(ctx, ctx->draw_framebuffer == 0u, n, bufs))
        return;
    if (mgl_list_record_draw_buffers(ctx, n, bufs))
        return;
    state = current_draw_buffers(ctx);
    if (state == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    for (i = 0; i < n; ++i)
        state[i] = bufs[i];
    for (; i < (GLsizei)MGL_MAX_DRAW_BUFFERS; ++i)
        state[i] = GL_NONE;
    lower_draw_buffers_for_framebuffer(ctx, ctx->draw_framebuffer == 0u, n, state);
}

void glReadBuffer(GLenum src) {
    struct mgl_context *ctx = mgl_context_current();
    GLenum *state;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!validate_read_buffer_selection(ctx, ctx->read_framebuffer == 0u, src))
        return;
    if (mgl_list_record_read_buffer(ctx, src))
        return;
    state = current_read_buffer(ctx);
    if (state == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    *state = src;
    lower_read_buffer_for_framebuffer(ctx, ctx->read_framebuffer == 0u, src);
}

void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget,
                               GLuint renderbuffer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *fb;
    struct mgl_renderbuffer *rb = NULL;
    GLuint native_rb = 0u;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_framebuffer_target(target) || renderbuffertarget != GL_RENDERBUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if ((color_attachment_index(attachment) < 0 && attachment != GL_DEPTH_ATTACHMENT &&
         attachment != GL_STENCIL_ATTACHMENT && attachment != GL_DEPTH_STENCIL_ATTACHMENT)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    fb = mgl_bound_framebuffer(ctx, target);
    if (fb == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (renderbuffer != 0u) {
        rb = mgl_renderbuffer_find(ctx, renderbuffer);
        if (rb == NULL || !rb->realized) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        native_rb = rb->native_name;
    }
    if (attachment == GL_COLOR_ATTACHMENT0) {
        fb->color_renderbuffer = renderbuffer;
        fb->color_texture = 0u;
        fb->color_texture_layer = -1;
        fb->color_texture_target = 0u;
    } else if (color_attachment_index(attachment) > 0) {
        unsigned idx = (unsigned)color_attachment_index(attachment);
        fb->extra_color_renderbuffers[idx] = renderbuffer;
        fb->extra_color_textures[idx] = 0u;
        fb->extra_color_layers[idx] = -1;
        fb->extra_color_targets[idx] = 0u;
    } else if (attachment == GL_DEPTH_ATTACHMENT) {
        fb->depth_renderbuffer = renderbuffer;
        fb->depth_texture = 0u;
        fb->depth_texture_layer = -1;
        fb->depth_texture_target = 0u;
    } else if (attachment == GL_STENCIL_ATTACHMENT) {
        fb->stencil_renderbuffer = renderbuffer;
        fb->stencil_texture = 0u;
        fb->stencil_texture_layer = -1;
        fb->stencil_texture_target = 0u;
    } else {
        fb->depth_renderbuffer = renderbuffer;
        fb->stencil_renderbuffer = renderbuffer;
        fb->depth_texture = 0u;
        fb->stencil_texture = 0u;
        fb->depth_texture_layer = -1;
        fb->stencil_texture_layer = -1;
        fb->depth_texture_target = 0u;
        fb->stencil_texture_target = 0u;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesFramebufferRenderbuffer != NULL) {
        ctx->native_dispatch->glesFramebufferRenderbuffer(target, attachment, GL_RENDERBUFFER,
                                                          native_rb);
    }
}

void glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0,
                       GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    struct mgl_context *ctx = mgl_context_current();
    const GLbitfield allowed = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (filter != GL_NEAREST && filter != GL_LINEAR) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if ((mask & ~allowed) != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if ((mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0u && filter != GL_NEAREST) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesBlitFramebuffer == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    mgl_texture_refresh_current_framebuffers(ctx);
    {
        int route = mgl_framebuffer_draw_route_begin(ctx);
        if (route >= 0)
            ctx->native_dispatch->glesBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                                      dstX1, dstY1, mask, filter);
        if (route > 0)
            mgl_framebuffer_draw_route_end(ctx);
    }
}

static GLuint next_renderbuffer_name(struct mgl_share_group *group) {
    GLuint name;
    do {
        name = (GLuint)atomic_fetch_add_explicit(&group->next_renderbuffer_name, 1u,
                                                 memory_order_relaxed);
    } while (name == 0u);
    return name;
}

void glGenRenderbuffers(GLsizei n, GLuint *renderbuffers) {
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
    if (n == 0 || renderbuffers == NULL)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < n; ++i) {
        GLuint name = next_renderbuffer_name(ctx->share_group);
        struct mgl_renderbuffer *obj = mgl_renderbuffer_ensure(ctx, name, 1);
        if (obj == NULL || obj->native_name == 0u) {
            renderbuffers[i] = 0u;
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        } else
            renderbuffers[i] = name;
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers) {
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
    if (n == 0 || renderbuffers == NULL)
        return;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < n; ++i)
        mgl_renderbuffer_remove(ctx, renderbuffers[i]);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
}

void glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_renderbuffer *obj = NULL;
    GLuint native_name = 0u;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_RENDERBUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (renderbuffer != 0u) {
        (void)pthread_mutex_lock(&ctx->share_group->lock);
        obj = mgl_renderbuffer_find(ctx, renderbuffer);
        if (obj != NULL) {
            obj->realized = GL_TRUE;
            native_name = obj->native_name;
        }
        (void)pthread_mutex_unlock(&ctx->share_group->lock);
        if (obj == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (native_name == 0u) {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
            return;
        }
    }
    ctx->renderbuffer = renderbuffer;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindRenderbuffer != NULL)
        ctx->native_dispatch->glesBindRenderbuffer(target, native_name);
}

GLboolean glIsRenderbuffer(GLuint renderbuffer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_renderbuffer *obj;
    GLboolean out = GL_FALSE;
    if (ctx == NULL)
        return GL_FALSE;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    if (renderbuffer == 0u)
        return GL_FALSE;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    obj = mgl_renderbuffer_find(ctx, renderbuffer);
    if (obj != NULL)
        out = obj->realized;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return out;
}

void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_renderbuffer *obj;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_RENDERBUFFER ||
        !mgl_renderable_storage_format(ctx, renderbuffer_storage_format(internalformat))) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    obj = mgl_renderbuffer_find(ctx, ctx->renderbuffer);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj->internal_format = renderbuffer_storage_format(internalformat);
    obj->width = width;
    obj->height = height;
    obj->samples = 0;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesRenderbufferStorage != NULL)
        ctx->native_dispatch->glesRenderbufferStorage(
            target, renderbuffer_storage_format(internalformat), width, height);
}

void glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                      GLsizei width, GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_renderbuffer *obj;
    GLint actual_samples = samples;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_RENDERBUFFER ||
        !mgl_renderable_storage_format(ctx, renderbuffer_storage_format(internalformat))) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (samples < 0 || width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    obj = mgl_renderbuffer_find(ctx, ctx->renderbuffer);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (samples > 0 && (ctx->native_dispatch == NULL ||
                        ctx->native_dispatch->glesRenderbufferStorageMultisample == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch != NULL) {
        if (samples > 0)
            ctx->native_dispatch->glesRenderbufferStorageMultisample(
                target, samples, renderbuffer_storage_format(internalformat), width, height);
        else if (ctx->native_dispatch->glesRenderbufferStorage != NULL)
            ctx->native_dispatch->glesRenderbufferStorage(
                target, renderbuffer_storage_format(internalformat), width, height);
        if (ctx->native_dispatch->glesGetRenderbufferParameteriv != NULL)
            ctx->native_dispatch->glesGetRenderbufferParameteriv(target, GL_RENDERBUFFER_SAMPLES,
                                                                 &actual_samples);
    }
    obj->internal_format = renderbuffer_storage_format(internalformat);
    obj->width = width;
    obj->height = height;
    obj->samples = actual_samples;
}

void glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_renderbuffer *obj;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (target != GL_RENDERBUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    obj = mgl_renderbuffer_find(ctx, ctx->renderbuffer);
    if (obj == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    switch (pname) {
        case GL_RENDERBUFFER_WIDTH:
            *params = obj->width;
            break;
        case GL_RENDERBUFFER_HEIGHT:
            *params = obj->height;
            break;
        case GL_RENDERBUFFER_INTERNAL_FORMAT:
            *params = (GLint)obj->internal_format;
            break;
        case GL_RENDERBUFFER_SAMPLES:
            *params = obj->samples;
            break;
        case GL_RENDERBUFFER_RED_SIZE:
        case GL_RENDERBUFFER_GREEN_SIZE:
        case GL_RENDERBUFFER_BLUE_SIZE:
        case GL_RENDERBUFFER_ALPHA_SIZE:
        case GL_RENDERBUFFER_STENCIL_SIZE:
        case GL_RENDERBUFFER_DEPTH_SIZE:
            *params = pname == GL_RENDERBUFFER_DEPTH_SIZE
                          ? renderbuffer_depth_size(obj->internal_format)
                          : 0;
            if (ctx->native_dispatch != NULL &&
                ctx->native_dispatch->glesGetRenderbufferParameteriv != NULL)
                ctx->native_dispatch->glesGetRenderbufferParameteriv(GL_RENDERBUFFER, pname,
                                                                     params);
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
}

static GLuint mgl_native_dispatch_framebuffer_name(struct mgl_context *ctx, GLuint name) {
    struct mgl_framebuffer *obj;
    if (ctx == NULL || name == 0u)
        return 0u;
    obj = mgl_framebuffer_find(ctx, name);
    return obj != NULL && obj->realized ? obj->native_name : 0u;
}

static GLuint mgl_native_dispatch_renderbuffer_name(struct mgl_context *ctx, GLuint name) {
    struct mgl_renderbuffer *obj;
    if (ctx == NULL || name == 0u)
        return 0u;
    obj = mgl_renderbuffer_find(ctx, name);
    return obj != NULL && obj->realized ? obj->native_name : 0u;
}

static void mgl_restore_native_framebuffers(struct mgl_context *ctx) {
    if (ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesBindFramebuffer == NULL)
        return;
    ctx->native_dispatch->glesBindFramebuffer(
        GL_DRAW_FRAMEBUFFER, mgl_native_dispatch_framebuffer_name(ctx, ctx->draw_framebuffer));
    ctx->native_dispatch->glesBindFramebuffer(
        GL_READ_FRAMEBUFFER, mgl_native_dispatch_framebuffer_name(ctx, ctx->read_framebuffer));
}

static int mgl_validate_clear_drawbuffer(struct mgl_context *ctx, GLenum buffer, GLint drawbuffer) {
    if (buffer == GL_COLOR) {
        if (drawbuffer < 0 || drawbuffer >= (GLint)MGL_MAX_DRAW_BUFFERS) {
            mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
            return 0;
        }
        return 1;
    }
    if (drawbuffer != 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    return 1;
}

static int mgl_current_draw_framebuffer_complete(struct mgl_context *ctx) {
    GLenum status;
    if (ctx->draw_framebuffer == 0u)
        return 1;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesCheckFramebufferStatus == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    mgl_texture_refresh_current_framebuffers(ctx);
    {
        int route = mgl_framebuffer_draw_route_begin(ctx);
        if (route < 0)
            return 0;
        status = ctx->native_dispatch->glesCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        if (route > 0)
            mgl_framebuffer_draw_route_end(ctx);
    }
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        mgl_error_set(&ctx->errors, GL_INVALID_FRAMEBUFFER_OPERATION);
        return 0;
    }
    return 1;
}

void glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned count;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (buffer == GL_COLOR)
        count = 4u;
    else if (buffer == GL_STENCIL)
        count = 1u;
    else {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!mgl_validate_clear_drawbuffer(ctx, buffer, drawbuffer))
        return;
    if (value == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_clear_buffer_iv(ctx, buffer, drawbuffer, value, count))
        return;
    if (!mgl_current_draw_framebuffer_complete(ctx))
        return;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesClearBufferiv == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    {
        int route = mgl_framebuffer_draw_route_begin(ctx);
        if (route >= 0)
            ctx->native_dispatch->glesClearBufferiv(buffer, drawbuffer, value);
        if (route > 0)
            mgl_framebuffer_draw_route_end(ctx);
    }
}

void glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (buffer != GL_COLOR) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!mgl_validate_clear_drawbuffer(ctx, buffer, drawbuffer))
        return;
    if (value == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_clear_buffer_uiv(ctx, buffer, drawbuffer, value, 4u))
        return;
    if (!mgl_current_draw_framebuffer_complete(ctx))
        return;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesClearBufferuiv == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    {
        int route = mgl_framebuffer_draw_route_begin(ctx);
        if (route >= 0)
            ctx->native_dispatch->glesClearBufferuiv(buffer, drawbuffer, value);
        if (route > 0)
            mgl_framebuffer_draw_route_end(ctx);
    }
}

void glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value) {
    struct mgl_context *ctx = mgl_context_current();
    unsigned count;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (buffer == GL_COLOR)
        count = 4u;
    else if (buffer == GL_DEPTH)
        count = 1u;
    else {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!mgl_validate_clear_drawbuffer(ctx, buffer, drawbuffer))
        return;
    if (value == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_clear_buffer_fv(ctx, buffer, drawbuffer, value, count))
        return;
    if (!mgl_current_draw_framebuffer_complete(ctx))
        return;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesClearBufferfv == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    {
        int route = mgl_framebuffer_draw_route_begin(ctx);
        if (route >= 0)
            ctx->native_dispatch->glesClearBufferfv(buffer, drawbuffer, value);
        if (route > 0)
            mgl_framebuffer_draw_route_end(ctx);
    }
}

void glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (buffer != GL_DEPTH_STENCIL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!mgl_validate_clear_drawbuffer(ctx, buffer, drawbuffer))
        return;
    if (mgl_list_record_clear_buffer_fi(ctx, buffer, drawbuffer, depth, stencil))
        return;
    if (!mgl_current_draw_framebuffer_complete(ctx))
        return;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesClearBufferfi == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    {
        int route = mgl_framebuffer_draw_route_begin(ctx);
        if (route >= 0)
            ctx->native_dispatch->glesClearBufferfi(buffer, drawbuffer, depth, stencil);
        if (route > 0)
            mgl_framebuffer_draw_route_end(ctx);
    }
}

static int mgl_named_clear_begin(struct mgl_context *ctx, GLuint framebuffer,
                                 GLuint *target_native) {
    struct mgl_framebuffer *fb = NULL;
    if (framebuffer != 0u) {
        fb = mgl_framebuffer_find(ctx, framebuffer);
        if (fb == NULL || !fb->realized) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return 0;
        }
        *target_native = fb->native_name;
    } else
        *target_native = 0u;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesBindFramebuffer == NULL ||
        ctx->native_dispatch->glesCheckFramebufferStatus == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0;
    }
    if (fb != NULL)
        (void)mgl_texture_refresh_framebuffer_attachments(ctx, fb);
    ctx->native_dispatch->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER, *target_native);
    if (framebuffer != 0u) {
        int route = mgl_framebuffer_draw_route_begin_for(ctx, fb);
        if (route < 0) {
            mgl_render_diag_detail(ctx, "glClearNamedFramebuffer", "draw_route_failed", framebuffer,
                                   fb->native_name);
            mgl_restore_native_framebuffers(ctx);
            return 0;
        }
        {
            GLenum status = ctx->native_dispatch->glesCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                mgl_render_diag_framebuffer(ctx, "glClearNamedFramebuffer", GL_DRAW_FRAMEBUFFER,
                                            framebuffer, status);
                mgl_render_diag_detail(ctx, "glClearNamedFramebuffer", "native_framebuffer",
                                       fb->native_name, fb->routed_draw_native_name);
                mgl_render_diag_detail(ctx, "glClearNamedFramebuffer", "color_texture_renderbuffer",
                                       fb->color_texture, fb->color_renderbuffer);
                mgl_render_diag_detail(ctx, "glClearNamedFramebuffer",
                                       "depth_stencil_renderbuffers", fb->depth_renderbuffer,
                                       fb->stencil_renderbuffer);
                mgl_error_set(&ctx->errors, GL_INVALID_FRAMEBUFFER_OPERATION);
                mgl_restore_native_framebuffers(ctx);
                return 0;
            }
        }
    }
    return 1;
}

void glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                               const GLint *value) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint native_name;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (buffer != GL_COLOR && buffer != GL_STENCIL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!mgl_validate_clear_drawbuffer(ctx, buffer, drawbuffer))
        return;
    if (value == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!mgl_named_clear_begin(ctx, framebuffer, &native_name))
        return;
    (void)native_name;
    if (ctx->native_dispatch->glesClearBufferiv == NULL)
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    else {
        ctx->native_dispatch->glesClearBufferiv(buffer, drawbuffer, value);
    }
    mgl_restore_native_framebuffers(ctx);
}

void glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                const GLuint *value) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint native_name;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (buffer != GL_COLOR) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!mgl_validate_clear_drawbuffer(ctx, buffer, drawbuffer))
        return;
    if (value == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!mgl_named_clear_begin(ctx, framebuffer, &native_name))
        return;
    (void)native_name;
    if (ctx->native_dispatch->glesClearBufferuiv == NULL)
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    else {
        ctx->native_dispatch->glesClearBufferuiv(buffer, drawbuffer, value);
    }
    mgl_restore_native_framebuffers(ctx);
}

void glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                               const GLfloat *value) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint native_name;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (buffer != GL_COLOR && buffer != GL_DEPTH) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!mgl_validate_clear_drawbuffer(ctx, buffer, drawbuffer))
        return;
    if (value == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (!mgl_named_clear_begin(ctx, framebuffer, &native_name))
        return;
    (void)native_name;
    if (ctx->native_dispatch->glesClearBufferfv == NULL)
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    else {
        ctx->native_dispatch->glesClearBufferfv(buffer, drawbuffer, value);
    }
    mgl_restore_native_framebuffers(ctx);
}

void glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth,
                               GLint stencil) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint native_name;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (buffer != GL_DEPTH_STENCIL) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (!mgl_validate_clear_drawbuffer(ctx, buffer, drawbuffer))
        return;
    if (!mgl_named_clear_begin(ctx, framebuffer, &native_name))
        return;
    (void)native_name;
    if (ctx->native_dispatch->glesClearBufferfi == NULL)
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    else {
        ctx->native_dispatch->glesClearBufferfi(buffer, drawbuffer, depth, stencil);
    }
    mgl_restore_native_framebuffers(ctx);
}

void glClearNamedFramebufferivEXT(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                  const GLint *value) {
    glClearNamedFramebufferiv(framebuffer, buffer, drawbuffer, value);
}

void glClearNamedFramebufferuivEXT(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                   const GLuint *value) {
    glClearNamedFramebufferuiv(framebuffer, buffer, drawbuffer, value);
}

void glClearNamedFramebufferfvEXT(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                  const GLfloat *value) {
    glClearNamedFramebufferfv(framebuffer, buffer, drawbuffer, value);
}

void glClearNamedFramebufferfiEXT(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                  GLfloat depth, GLint stencil) {
    glClearNamedFramebufferfi(framebuffer, buffer, drawbuffer, depth, stencil);
}

void glCreateFramebuffers(GLsizei n, GLuint *framebuffers) {
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
    if (n == 0 || framebuffers == NULL)
        return;
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGenFramebuffers(n, framebuffers);
    if (ctx->errors.sequence != error_sequence_before)
        return;
    for (i = 0; i < n; ++i) {
        struct mgl_framebuffer *obj = mgl_framebuffer_find(ctx, framebuffers[i]);
        if (obj == NULL || obj->native_name == 0u) {
            framebuffers[i] = 0u;
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        } else
            obj->realized = GL_TRUE;
    }
}

GLenum glCheckNamedFramebufferStatus(GLuint framebuffer, GLenum target) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *obj;
    GLenum status;
    if (ctx == NULL)
        return GL_FRAMEBUFFER_UNDEFINED;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0u;
    }
    if (!valid_framebuffer_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0u;
    }
    obj = mgl_framebuffer_find(ctx, framebuffer);
    if (obj == NULL || !obj->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0u;
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesBindFramebuffer == NULL ||
        ctx->native_dispatch->glesCheckFramebufferStatus == NULL)
        return GL_FRAMEBUFFER_UNSUPPORTED;
    (void)mgl_texture_refresh_framebuffer_attachments(ctx, obj);
    ctx->native_dispatch->glesBindFramebuffer(target, obj->native_name);
    if (target != GL_READ_FRAMEBUFFER) {
        int route = mgl_framebuffer_draw_route_begin_for(ctx, obj);
        if (route < 0) {
            mgl_restore_native_framebuffers(ctx);
            return GL_FRAMEBUFFER_UNSUPPORTED;
        }
        status = ctx->native_dispatch->glesCheckFramebufferStatus(route > 0 ? GL_DRAW_FRAMEBUFFER
                                                                            : target);
    } else
        status = ctx->native_dispatch->glesCheckFramebufferStatus(target);
    note_framebuffer_color_status(ctx, obj, status);
    mgl_restore_native_framebuffers(ctx);
    return status;
}

static struct mgl_framebuffer *mgl_validate_named_framebuffer(struct mgl_context *ctx,
                                                              GLuint framebuffer) {
    struct mgl_framebuffer *fb;
    if (framebuffer == 0u)
        return NULL;
    fb = mgl_framebuffer_find(ctx, framebuffer);
    if (fb == NULL || !fb->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    return fb;
}

static void lower_named_draw_buffers(struct mgl_context *ctx, struct mgl_framebuffer *fb, GLsizei n,
                                     const GLenum *buffers) {
    if (ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesBindFramebuffer == NULL)
        return;
    ctx->native_dispatch->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                              fb != NULL ? fb->native_name : 0u);
    lower_draw_buffers_for_framebuffer(ctx, fb == NULL, n, buffers);
    mgl_restore_native_framebuffers(ctx);
}

static void lower_named_read_buffer(struct mgl_context *ctx, struct mgl_framebuffer *fb,
                                    GLenum buffer) {
    if (ctx == NULL || ctx->native_dispatch == NULL ||
        ctx->native_dispatch->glesBindFramebuffer == NULL)
        return;
    ctx->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER,
                                              fb != NULL ? fb->native_name : 0u);
    lower_read_buffer_for_framebuffer(ctx, fb == NULL, buffer);
    mgl_restore_native_framebuffers(ctx);
}

void glNamedFramebufferDrawBuffer(GLuint framebuffer, GLenum buf) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *fb;
    GLenum *state;
    unsigned i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    fb = mgl_validate_named_framebuffer(ctx, framebuffer);
    if (framebuffer != 0u && fb == NULL)
        return;
    if (!validate_draw_buffer_selection(ctx, framebuffer == 0u, buf))
        return;
    state = framebuffer == 0u ? ctx->default_draw_buffers : fb->draw_buffers;
    state[0] = buf;
    for (i = 1u; i < MGL_MAX_DRAW_BUFFERS; ++i)
        state[i] = GL_NONE;
    lower_named_draw_buffers(ctx, fb, 1, state);
}

void glNamedFramebufferDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum *bufs) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *fb;
    GLenum *state;
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    fb = mgl_validate_named_framebuffer(ctx, framebuffer);
    if (framebuffer != 0u && fb == NULL)
        return;
    if (!validate_draw_buffers_selection(ctx, framebuffer == 0u, n, bufs))
        return;
    state = framebuffer == 0u ? ctx->default_draw_buffers : fb->draw_buffers;
    for (i = 0; i < n; ++i)
        state[i] = bufs[i];
    for (; i < (GLsizei)MGL_MAX_DRAW_BUFFERS; ++i)
        state[i] = GL_NONE;
    lower_named_draw_buffers(ctx, fb, n, state);
}

void glNamedFramebufferReadBuffer(GLuint framebuffer, GLenum src) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *fb;
    GLenum *state;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    fb = mgl_validate_named_framebuffer(ctx, framebuffer);
    if (framebuffer != 0u && fb == NULL)
        return;
    if (!validate_read_buffer_selection(ctx, framebuffer == 0u, src))
        return;
    state = framebuffer == 0u ? &ctx->default_read_buffer : &fb->read_buffer;
    *state = src;
    lower_named_read_buffer(ctx, fb, src);
}

void glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0,
                            GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0,
                            GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *read_fb = NULL, *draw_fb = NULL;
    const GLbitfield allowed = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (filter != GL_NEAREST && filter != GL_LINEAR) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if ((mask & ~allowed) != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if ((mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0u && filter != GL_NEAREST) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (readFramebuffer != 0u) {
        read_fb = mgl_framebuffer_find(ctx, readFramebuffer);
        if (read_fb == NULL || !read_fb->realized) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
    }
    if (drawFramebuffer != 0u) {
        draw_fb = mgl_framebuffer_find(ctx, drawFramebuffer);
        if (draw_fb == NULL || !draw_fb->realized) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
    }
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesBindFramebuffer == NULL ||
        ctx->native_dispatch->glesCheckFramebufferStatus == NULL ||
        ctx->native_dispatch->glesBlitFramebuffer == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (read_fb != NULL)
        (void)mgl_texture_refresh_framebuffer_attachments(ctx, read_fb);
    if (draw_fb != NULL && draw_fb != read_fb)
        (void)mgl_texture_refresh_framebuffer_attachments(ctx, draw_fb);
    ctx->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER,
                                              read_fb != NULL ? read_fb->native_name : 0u);
    ctx->native_dispatch->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                              draw_fb != NULL ? draw_fb->native_name : 0u);
    if ((readFramebuffer != 0u && ctx->native_dispatch->glesCheckFramebufferStatus(
                                      GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) ||
        (drawFramebuffer != 0u && ctx->native_dispatch->glesCheckFramebufferStatus(
                                      GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)) {
        mgl_error_set(&ctx->errors, GL_INVALID_FRAMEBUFFER_OPERATION);
        mgl_restore_native_framebuffers(ctx);
        return;
    }
    {
        int route = mgl_framebuffer_draw_route_begin_for(ctx, draw_fb);
        if (route >= 0)
            ctx->native_dispatch->glesBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                                      dstX1, dstY1, mask, filter);
    }
    mgl_restore_native_framebuffers(ctx);
}

static int mgl_validate_invalidate_attachments(struct mgl_context *ctx,
                                               GLboolean default_framebuffer,
                                               GLsizei numAttachments, const GLenum *attachments) {
    GLsizei i;
    if (numAttachments < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    if (numAttachments > 0 && attachments == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0;
    }
    for (i = 0; i < numAttachments; ++i) {
        const GLenum attachment = attachments[i];
        if (default_framebuffer) {
            if (attachment != GL_FRONT_LEFT && attachment != GL_FRONT_RIGHT &&
                attachment != GL_BACK_LEFT && attachment != GL_BACK_RIGHT &&
                attachment != GL_COLOR && attachment != GL_DEPTH && attachment != GL_STENCIL) {
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return 0;
            }
        } else if (attachment != GL_DEPTH_ATTACHMENT && attachment != GL_STENCIL_ATTACHMENT &&
                   attachment != GL_DEPTH_STENCIL_ATTACHMENT) {
            int index = color_attachment_index(attachment);
            if (index < 0) {
                mgl_error_set(&ctx->errors, attachment >= GL_COLOR_ATTACHMENT0 &&
                                                    attachment <= GL_COLOR_ATTACHMENT0 + 31u
                                                ? GL_INVALID_OPERATION
                                                : GL_INVALID_ENUM);
                return 0;
            }
            if (index >= framebuffer_limit(ctx, GL_MAX_COLOR_ATTACHMENTS)) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return 0;
            }
        }
    }
    return 1;
}

void glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint bound;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_framebuffer_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (target == GL_READ_FRAMEBUFFER)
        bound = ctx->read_framebuffer;
    else
        bound = ctx->draw_framebuffer;
    if (!mgl_validate_invalidate_attachments(ctx, bound == 0u ? GL_TRUE : GL_FALSE, numAttachments,
                                             attachments))
        return;
    /* Framebuffer invalidation permits content retention; this path leaves
     * native attachment storage unchanged. */
}

void glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments,
                                GLint x, GLint y, GLsizei width, GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint bound;
    (void)x;
    (void)y;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_framebuffer_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    bound = target == GL_READ_FRAMEBUFFER ? ctx->read_framebuffer : ctx->draw_framebuffer;
    (void)mgl_validate_invalidate_attachments(ctx, bound == 0u ? GL_TRUE : GL_FALSE, numAttachments,
                                              attachments);
}

void glInvalidateNamedFramebufferData(GLuint framebuffer, GLsizei numAttachments,
                                      const GLenum *attachments) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *fb = NULL;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (framebuffer != 0u) {
        fb = mgl_framebuffer_find(ctx, framebuffer);
        if (fb == NULL || !fb->realized) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
    }
    (void)fb;
    if (!mgl_validate_invalidate_attachments(ctx, framebuffer == 0u ? GL_TRUE : GL_FALSE,
                                             numAttachments, attachments))
        return;
}

void glInvalidateNamedFramebufferSubData(GLuint framebuffer, GLsizei numAttachments,
                                         const GLenum *attachments, GLint x, GLint y, GLsizei width,
                                         GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *fb = NULL;
    (void)x;
    (void)y;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (framebuffer != 0u) {
        fb = mgl_framebuffer_find(ctx, framebuffer);
        if (fb == NULL || !fb->realized) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
    }
    (void)fb;
    if (!mgl_validate_invalidate_attachments(ctx, framebuffer == 0u ? GL_TRUE : GL_FALSE,
                                             numAttachments, attachments))
        return;
}

void glNamedFramebufferRenderbuffer(GLuint framebuffer, GLenum attachment,
                                    GLenum renderbuffertarget, GLuint renderbuffer) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *fb;
    struct mgl_renderbuffer *rb = NULL;
    GLuint native_rb = 0u;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (renderbuffertarget != GL_RENDERBUFFER) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if ((color_attachment_index(attachment) < 0 && attachment != GL_DEPTH_ATTACHMENT &&
         attachment != GL_STENCIL_ATTACHMENT && attachment != GL_DEPTH_STENCIL_ATTACHMENT)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    fb = mgl_framebuffer_find(ctx, framebuffer);
    if (fb == NULL || !fb->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (renderbuffer != 0u) {
        rb = mgl_renderbuffer_find(ctx, renderbuffer);
        if (rb == NULL || !rb->realized) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        native_rb = rb->native_name;
    }
    if (attachment == GL_COLOR_ATTACHMENT0) {
        fb->color_renderbuffer = renderbuffer;
        fb->color_texture = 0u;
    } else if (color_attachment_index(attachment) > 0) {
        unsigned idx = (unsigned)color_attachment_index(attachment);
        fb->extra_color_renderbuffers[idx] = renderbuffer;
        fb->extra_color_textures[idx] = 0u;
        fb->extra_color_layers[idx] = -1;
        fb->extra_color_targets[idx] = 0u;
    } else if (attachment == GL_DEPTH_ATTACHMENT) {
        fb->depth_renderbuffer = renderbuffer;
        fb->depth_texture = 0u;
    } else if (attachment == GL_STENCIL_ATTACHMENT) {
        fb->stencil_renderbuffer = renderbuffer;
        fb->stencil_texture = 0u;
    } else {
        fb->depth_renderbuffer = renderbuffer;
        fb->stencil_renderbuffer = renderbuffer;
        fb->depth_texture = 0u;
        fb->stencil_texture = 0u;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindFramebuffer != NULL &&
        ctx->native_dispatch->glesFramebufferRenderbuffer != NULL) {
        ctx->native_dispatch->glesBindFramebuffer(GL_FRAMEBUFFER, fb->native_name);
        ctx->native_dispatch->glesFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment,
                                                          GL_RENDERBUFFER, native_rb);
        mgl_restore_native_framebuffers(ctx);
    }
}

static void mgl_get_framebuffer_attachment_parameter(struct mgl_context *ctx,
                                                     struct mgl_framebuffer *fb, GLenum attachment,
                                                     GLenum pname, GLint *params) {
    GLuint rb = 0u, tex = 0u;
    GLint level = 0, layer = -1;
    GLenum tex_target = 0u;
    if (ctx == NULL || fb == NULL || params == NULL)
        return;
    if (attachment == GL_COLOR_ATTACHMENT0) {
        rb = fb->color_renderbuffer;
        tex = fb->color_texture;
        level = fb->color_texture_level;
        layer = fb->color_texture_layer;
        tex_target = fb->color_texture_target;
    } else if (color_attachment_index(attachment) > 0) {
        unsigned idx = (unsigned)color_attachment_index(attachment);
        rb = fb->extra_color_renderbuffers[idx];
        tex = fb->extra_color_textures[idx];
        level = fb->extra_color_levels[idx];
        layer = fb->extra_color_layers[idx];
        tex_target = fb->extra_color_targets[idx];
    } else if (attachment == GL_DEPTH_ATTACHMENT) {
        rb = fb->depth_renderbuffer;
        tex = fb->depth_texture;
        level = fb->depth_texture_level;
        layer = fb->depth_texture_layer;
        tex_target = fb->depth_texture_target;
    } else if (attachment == GL_STENCIL_ATTACHMENT) {
        rb = fb->stencil_renderbuffer;
        tex = fb->stencil_texture;
        level = fb->stencil_texture_level;
        layer = fb->stencil_texture_layer;
        tex_target = fb->stencil_texture_target;
    } else if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
        rb = fb->depth_renderbuffer;
        tex = fb->depth_texture;
        level = fb->depth_texture_level;
        layer = fb->depth_texture_layer;
        tex_target = fb->depth_texture_target;
    } else {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    switch (pname) {
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            *params = rb != 0u ? (GLint)GL_RENDERBUFFER
                               : (tex != 0u ? (GLint)GL_TEXTURE : (GLint)GL_NONE);
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            *params = (GLint)(rb != 0u ? rb : tex);
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
            if (tex == 0u) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            *params = level;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
            if (tex == 0u) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            *params = layer >= 0 ? layer : 0;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_LAYERED:
            if (tex == 0u) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            *params = layer == -2 ? 1 : 0;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
            if (tex == 0u) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            *params = tex_target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X &&
                              tex_target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
                          ? (GLint)tex_target
                          : 0;
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
}

/* Query default-framebuffer component sizes from the current GLES backing image. Android
 * AHardwareBuffer presentation uses the attachments of its backing FBO. */
static int mgl_default_attachment_native_enum(const struct mgl_context *ctx, GLenum attachment,
                                              GLenum *native_attachment) {
    const GLboolean redirected = ctx->native_default_framebuffer != 0u;
    switch (attachment) {
        case GL_BACK_LEFT:
            *native_attachment = redirected ? GL_COLOR_ATTACHMENT0 : GL_BACK;
            return 1;
        case GL_DEPTH:
            *native_attachment = redirected ? GL_DEPTH_ATTACHMENT : GL_DEPTH;
            return 1;
        case GL_STENCIL:
            *native_attachment = redirected ? GL_STENCIL_ATTACHMENT : GL_STENCIL;
            return 1;
        default:
            return 0;
    }
}

static int mgl_framebuffer_attachment_size_pname(GLenum pname) {
    switch (pname) {
        case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
            return 1;
        default:
            return 0;
    }
}

void glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname,
                                           GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *fb;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!valid_framebuffer_target(target)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    fb = mgl_bound_framebuffer(ctx, target);
    if (fb == NULL) {
        GLenum native_attachment = 0u;
        GLuint bound =
            target == GL_READ_FRAMEBUFFER ? ctx->read_framebuffer : ctx->draw_framebuffer;
        if (bound != 0u) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        if (!mgl_default_attachment_native_enum(ctx, attachment, &native_attachment)) {
            /* The GLES default surface has no separate front-left, front-right, or back-right
             * images. */
            if (attachment == GL_FRONT_LEFT || attachment == GL_FRONT_RIGHT ||
                attachment == GL_BACK_RIGHT)
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            else
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
        }
        if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE) {
            GLint native_type = (GLint)GL_NONE;
            if (ctx->native_dispatch == NULL ||
                ctx->native_dispatch->glesGetFramebufferAttachmentParameteriv == NULL) {
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
                return;
            }
            ctx->native_dispatch->glesGetFramebufferAttachmentParameteriv(target, native_attachment,
                                                                          pname, &native_type);
            *params =
                native_type == (GLint)GL_NONE ? (GLint)GL_NONE : (GLint)GL_FRAMEBUFFER_DEFAULT;
            return;
        }
        if (pname != GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING &&
            pname != GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE &&
            !mgl_framebuffer_attachment_size_pname(pname)) {
            mgl_error_set(&ctx->errors, pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME
                                            ? GL_INVALID_OPERATION
                                            : GL_INVALID_ENUM);
            return;
        }
        if (ctx->native_dispatch == NULL ||
            ctx->native_dispatch->glesGetFramebufferAttachmentParameteriv == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        ctx->native_dispatch->glesGetFramebufferAttachmentParameteriv(target, native_attachment,
                                                                      pname, params);
        return;
    }
    if (mgl_framebuffer_attachment_size_pname(pname) ||
        pname == GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING ||
        pname == GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE) {
        if (color_attachment_index(attachment) < 0 && attachment != GL_DEPTH_ATTACHMENT &&
            attachment != GL_STENCIL_ATTACHMENT && attachment != GL_DEPTH_STENCIL_ATTACHMENT) {
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return;
        }
        if (ctx->native_dispatch == NULL ||
            ctx->native_dispatch->glesGetFramebufferAttachmentParameteriv == NULL) {
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return;
        }
        ctx->native_dispatch->glesGetFramebufferAttachmentParameteriv(target, attachment, pname,
                                                                      params);
        return;
    }
    mgl_get_framebuffer_attachment_parameter(ctx, fb, attachment, pname, params);
}

void glGetNamedFramebufferAttachmentParameteriv(GLuint framebuffer, GLenum attachment, GLenum pname,
                                                GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_framebuffer *fb;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    fb = mgl_framebuffer_find(ctx, framebuffer);
    if (fb == NULL || !fb->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    mgl_get_framebuffer_attachment_parameter(ctx, fb, attachment, pname, params);
}

void glCreateRenderbuffers(GLsizei n, GLuint *renderbuffers) {
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
    if (n == 0 || renderbuffers == NULL)
        return;
    uint64_t error_sequence_before = ctx->errors.sequence;
    glGenRenderbuffers(n, renderbuffers);
    if (ctx->errors.sequence != error_sequence_before)
        return;
    for (i = 0; i < n; ++i) {
        struct mgl_renderbuffer *obj = mgl_renderbuffer_find(ctx, renderbuffers[i]);
        if (obj == NULL || obj->native_name == 0u) {
            renderbuffers[i] = 0u;
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        } else
            obj->realized = GL_TRUE;
    }
}

void glNamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat, GLsizei width,
                                GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_renderbuffer *obj;
    GLuint restore;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!mgl_renderable_storage_format(ctx, renderbuffer_storage_format(internalformat))) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    obj = mgl_renderbuffer_find(ctx, renderbuffer);
    if (obj == NULL || !obj->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj->internal_format = renderbuffer_storage_format(internalformat);
    obj->width = width;
    obj->height = height;
    obj->samples = 0;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindRenderbuffer != NULL &&
        ctx->native_dispatch->glesRenderbufferStorage != NULL) {
        restore = mgl_native_dispatch_renderbuffer_name(ctx, ctx->renderbuffer);
        ctx->native_dispatch->glesBindRenderbuffer(GL_RENDERBUFFER, obj->native_name);
        ctx->native_dispatch->glesRenderbufferStorage(
            GL_RENDERBUFFER, renderbuffer_storage_format(internalformat), width, height);
        ctx->native_dispatch->glesBindRenderbuffer(GL_RENDERBUFFER, restore);
    }
}

void glNamedRenderbufferStorageMultisample(GLuint renderbuffer, GLsizei samples,
                                           GLenum internalformat, GLsizei width, GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_renderbuffer *obj;
    GLuint restore;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (!mgl_renderable_storage_format(ctx, renderbuffer_storage_format(internalformat))) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (samples < 0 || width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    obj = mgl_renderbuffer_find(ctx, renderbuffer);
    if (obj == NULL || !obj->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (samples > 0 && (ctx->native_dispatch == NULL ||
                        ctx->native_dispatch->glesRenderbufferStorageMultisample == NULL)) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj->internal_format = renderbuffer_storage_format(internalformat);
    obj->width = width;
    obj->height = height;
    obj->samples = samples;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesBindRenderbuffer != NULL) {
        restore = mgl_native_dispatch_renderbuffer_name(ctx, ctx->renderbuffer);
        ctx->native_dispatch->glesBindRenderbuffer(GL_RENDERBUFFER, obj->native_name);
        if (samples > 0)
            ctx->native_dispatch->glesRenderbufferStorageMultisample(
                GL_RENDERBUFFER, samples, renderbuffer_storage_format(internalformat), width,
                height);
        else if (ctx->native_dispatch->glesRenderbufferStorage != NULL)
            ctx->native_dispatch->glesRenderbufferStorage(
                GL_RENDERBUFFER, renderbuffer_storage_format(internalformat), width, height);
        if (ctx->native_dispatch->glesGetRenderbufferParameteriv != NULL) {
            GLint actual_samples = samples;
            ctx->native_dispatch->glesGetRenderbufferParameteriv(
                GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &actual_samples);
            obj->samples = actual_samples;
        }
        ctx->native_dispatch->glesBindRenderbuffer(GL_RENDERBUFFER, restore);
    }
}

void glGetNamedRenderbufferParameteriv(GLuint renderbuffer, GLenum pname, GLint *params) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_renderbuffer *obj;
    if (ctx == NULL || params == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    obj = mgl_renderbuffer_find(ctx, renderbuffer);
    if (obj == NULL || !obj->realized) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    switch (pname) {
        case GL_RENDERBUFFER_WIDTH:
            *params = obj->width;
            break;
        case GL_RENDERBUFFER_HEIGHT:
            *params = obj->height;
            break;
        case GL_RENDERBUFFER_INTERNAL_FORMAT:
            *params = (GLint)obj->internal_format;
            break;
        case GL_RENDERBUFFER_SAMPLES:
            *params = obj->samples;
            break;
        case GL_RENDERBUFFER_RED_SIZE:
        case GL_RENDERBUFFER_GREEN_SIZE:
        case GL_RENDERBUFFER_BLUE_SIZE:
        case GL_RENDERBUFFER_ALPHA_SIZE:
        case GL_RENDERBUFFER_STENCIL_SIZE:
        case GL_RENDERBUFFER_DEPTH_SIZE:
            *params = pname == GL_RENDERBUFFER_DEPTH_SIZE
                          ? renderbuffer_depth_size(obj->internal_format)
                          : 0;
            if (ctx->native_dispatch != NULL &&
                ctx->native_dispatch->glesBindRenderbuffer != NULL &&
                ctx->native_dispatch->glesGetRenderbufferParameteriv != NULL) {
                GLuint restore = mgl_native_dispatch_renderbuffer_name(ctx, ctx->renderbuffer);
                ctx->native_dispatch->glesBindRenderbuffer(GL_RENDERBUFFER, obj->native_name);
                ctx->native_dispatch->glesGetRenderbufferParameteriv(GL_RENDERBUFFER, pname,
                                                                     params);
                ctx->native_dispatch->glesBindRenderbuffer(GL_RENDERBUFFER, restore);
            }
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
}

void glGenFramebuffersEXT(GLsizei n, GLuint *framebuffers) {
    glGenFramebuffers(n, framebuffers);
}

void glDeleteFramebuffersEXT(GLsizei n, const GLuint *framebuffers) {
    glDeleteFramebuffers(n, framebuffers);
}

void glBindFramebufferEXT(GLenum target, GLuint framebuffer) {
    glBindFramebuffer(target, framebuffer);
}

GLboolean glIsFramebufferEXT(GLuint framebuffer) {
    return glIsFramebuffer(framebuffer);
}

GLenum glCheckFramebufferStatusEXT(GLenum target) {
    return glCheckFramebufferStatus(target);
}

void glFramebufferRenderbufferEXT(GLenum target, GLenum attachment, GLenum renderbuffertarget,
                                  GLuint renderbuffer) {
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void glBlitFramebufferEXT(GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint dx0, GLint dy0,
                          GLint dx1, GLint dy1, GLbitfield mask, GLenum filter) {
    glBlitFramebuffer(sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, mask, filter);
}

void glGetFramebufferAttachmentParameterivEXT(GLenum target, GLenum attachment, GLenum pname,
                                              GLint *params) {
    glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

void glGenRenderbuffersEXT(GLsizei n, GLuint *renderbuffers) {
    glGenRenderbuffers(n, renderbuffers);
}

void glDeleteRenderbuffersEXT(GLsizei n, const GLuint *renderbuffers) {
    glDeleteRenderbuffers(n, renderbuffers);
}

void glBindRenderbufferEXT(GLenum target, GLuint renderbuffer) {
    glBindRenderbuffer(target, renderbuffer);
}

GLboolean glIsRenderbufferEXT(GLuint renderbuffer) {
    return glIsRenderbuffer(renderbuffer);
}

void glRenderbufferStorageEXT(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    glRenderbufferStorage(target, internalformat, width, height);
}

void glRenderbufferStorageMultisampleEXT(GLenum target, GLsizei samples, GLenum internalformat,
                                         GLsizei width, GLsizei height) {
    glRenderbufferStorageMultisample(target, samples, internalformat, width, height);
}

void glGetRenderbufferParameterivEXT(GLenum target, GLenum pname, GLint *params) {
    glGetRenderbufferParameteriv(target, pname, params);
}

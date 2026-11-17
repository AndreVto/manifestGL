#include "mgl/gl.h"

#include "core/context.h"
#include "objects/object_store.h"
#include "core/render_diag.h"
#include "core/capabilities.h"
#include "core/version.h"
#include "list/display_list.h"

#include <math.h>

static const GLubyte mgl_version[] = MGL_OPENGL_VERSION_IDENTITY;
static const GLubyte mgl_glsl_version[] = MGL_GLSL_VERSION_IDENTITY;

GLenum glGetError(void) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_GET_ERROR);
    if (ctx == NULL)
        return GL_NO_ERROR;
    GLenum interface_error;
    GLenum native_error;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_NO_ERROR;
    }
    interface_error = mgl_error_take(&ctx->errors);
    if (interface_error != GL_NO_ERROR) {
        mgl_render_diag_gl_error(ctx, "interface", interface_error);
        return interface_error;
    }
    native_error = mgl_error_native_take(&ctx->errors);
    if (native_error != GL_NO_ERROR) {
        mgl_render_diag_gl_error(ctx, "gles_queued", native_error);
        return native_error;
    }
    {
        uint64_t deep_started = mgl_perf_diag_now();
        native_error = ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetError != NULL
                           ? ctx->native_dispatch->glesGetError()
                           : GL_NO_ERROR;
        mgl_deep_record(ctx, MGL_DEEP_GLES_GET_ERROR, deep_started);
    }
    mgl_render_diag_gl_error(ctx, "gles", native_error);
    return native_error;
}

const GLubyte *glGetString(GLenum name) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetString",
                        (const struct mgl_error_arg[]){{"name", (uint64_t)(uintptr_t)name}}, 1u);
    if (ctx == NULL)
        return NULL;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    switch (name) {
        case GL_VENDOR:
        case GL_RENDERER: {
            const GLubyte *identity =
                ctx->native_dispatch != NULL && ctx->native_dispatch->glesGetString != NULL
                    ? ctx->native_dispatch->glesGetString(name)
                    : NULL;
            if (identity == NULL)
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            return identity;
        }
        case GL_VERSION:
            return mgl_version;
        case GL_SHADING_LANGUAGE_VERSION:
            return mgl_glsl_version;
        case GL_EXTENSIONS:
            if (ctx->profile_mask == GL_CONTEXT_CORE_PROFILE_BIT) {
                mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
                return NULL;
            }
            return mgl_capability_extension_string(ctx);
        case GL_PROGRAM_ERROR_STRING_ARB:
            return (const GLubyte *)ctx->arb_program_error_string;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            return NULL;
    }
}

const GLubyte *glGetStringi(GLenum name, GLuint index) {
    struct mgl_context *ctx = mgl_context_current();
    mgl_error_trace_api(ctx != NULL ? &ctx->errors : NULL, "glGetStringi",
                        (const struct mgl_error_arg[]){{"name", (uint64_t)(uintptr_t)name},
                                                       {"index", (uint64_t)(uintptr_t)index}},
                        2u);
    const GLubyte *extension;
    if (ctx == NULL)
        return NULL;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return NULL;
    }
    if (name == GL_SHADING_LANGUAGE_VERSION) {
        if (index == 0u)
            return mgl_glsl_version;
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return NULL;
    }
    if (name == GL_SPIR_V_EXTENSIONS) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return NULL;
    }
    if (name != GL_EXTENSIONS) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return NULL;
    }
    extension = mgl_capability_extension(ctx, index);
    if (extension == NULL)
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
    return extension;
}

static GLfloat clamp01f(GLfloat value) {
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

void glClearIndex(GLfloat index) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    index = truncf(index);
    if (mgl_list_record_clear_index(ctx, index))
        return;
    ctx->canonical_state.clear_index = index;
}

void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_clear_color(ctx, red, green, blue, alpha))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    red = clamp01f(red);
    green = clamp01f(green);
    blue = clamp01f(blue);
    alpha = clamp01f(alpha);
    if (mgl_list_record_clear_color(ctx, red, green, blue, alpha))
        return;
    ctx->canonical_state.clear_color[0] = red;
    ctx->canonical_state.clear_color[1] = green;
    ctx->canonical_state.clear_color[2] = blue;
    ctx->canonical_state.clear_color[3] = alpha;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesClearColor != NULL)
        ctx->native_dispatch->glesClearColor(red, green, blue, alpha);
}

void glClearDepth(GLdouble depth) {
    struct mgl_context *ctx = mgl_context_current();
    GLfloat clamped;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_clear_depth(ctx, (GLfloat)depth))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (depth < 0.0)
        depth = 0.0;
    if (depth > 1.0)
        depth = 1.0;
    clamped = (GLfloat)depth;
    if (mgl_list_record_clear_depth(ctx, clamped))
        return;
    ctx->canonical_state.clear_depth = clamped;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesClearDepthf != NULL)
        ctx->native_dispatch->glesClearDepthf(clamped);
}

void glClearDepthf(GLfloat depth) {
    glClearDepth((GLdouble)depth);
}

void glClearStencil(GLint s) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_clear_stencil(ctx, s))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (mgl_list_record_clear_stencil(ctx, s))
        return;
    ctx->canonical_state.clear_stencil = s;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesClearStencil != NULL)
        ctx->native_dispatch->glesClearStencil(s);
}

void glClear(GLbitfield mask) {
    struct mgl_context *ctx = mgl_context_current();
    const GLbitfield allowed =
        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying && mgl_list_record_clear(ctx, mask))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if ((mask & ~allowed) != 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_clear(ctx, mask))
        return;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesClear != NULL) {
        uint64_t started = mgl_perf_diag_now();
        int route = mgl_framebuffer_draw_route_begin(ctx);
        if (route >= 0 && (mask & ~GL_ACCUM_BUFFER_BIT) != 0u) {
            MGL_DEEP_SCOPE(ctx, MGL_DEEP_GLES_CLEAR);
            ctx->native_dispatch->glesClear(mask & ~GL_ACCUM_BUFFER_BIT);
        }
        if (route > 0)
            mgl_framebuffer_draw_route_end(ctx);
        mgl_perf_diag_extra(ctx, MGL_PERF_CLEAR, started);
    }
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_ENABLE_STATE);
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        if (ctx->compiling_list != NULL && !ctx->list_replaying &&
            mgl_list_record_viewport(ctx, x, y, width, height))
            return;
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (width < 0 || height < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mgl_list_record_viewport(ctx, x, y, width, height))
        return;
    ctx->canonical_state.viewport[0] = x;
    ctx->canonical_state.viewport[1] = y;
    ctx->canonical_state.viewport[2] = width;
    ctx->canonical_state.viewport[3] = height;
    ctx->canonical_state.indexed_viewports[0][0] = (GLfloat)x;
    ctx->canonical_state.indexed_viewports[0][1] = (GLfloat)y;
    ctx->canonical_state.indexed_viewports[0][2] = (GLfloat)width;
    ctx->canonical_state.indexed_viewports[0][3] = (GLfloat)height;
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesViewport != NULL)
        ctx->native_dispatch->glesViewport(x, y, width, height);
}

void glFlush(void) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesFlush != NULL) {
        uint64_t started = mgl_perf_diag_now();
        ctx->native_dispatch->glesFlush();
        mgl_perf_diag_extra(ctx, MGL_PERF_FLUSH, started);
    }
}

void glFinish(void) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->native_dispatch != NULL && ctx->native_dispatch->glesFinish != NULL) {
        uint64_t started = mgl_perf_diag_now();
        ctx->native_dispatch->glesFinish();
        mgl_perf_diag_extra(ctx, MGL_PERF_FINISH, started);
    }
}

void glCompressedTexImage1DARB(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                               GLint border, GLsizei imageSize, const void *data) {
    glCompressedTexImage1D(target, level, internalformat, width, border, imageSize, data);
}

void glCompressedTexSubImage1DARB(GLenum target, GLint level, GLint xoffset, GLsizei width,
                                  GLenum format, GLsizei imageSize, const void *data) {
    glCompressedTexSubImage1D(target, level, xoffset, width, format, imageSize, data);
}

void glCompressedTexImage2DARB(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                               GLsizei height, GLint border, GLsizei imageSize, const void *data) {
    glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}

void glCompressedTexImage3DARB(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                               GLsizei height, GLsizei depth, GLint border, GLsizei imageSize,
                               const void *data) {
    glCompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize,
                           data);
}

void glCompressedTexSubImage2DARB(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                  GLsizei width, GLsizei height, GLenum format, GLsizei imageSize,
                                  const void *data) {
    glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize,
                              data);
}

void glCompressedTexSubImage3DARB(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                  GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                  GLenum format, GLsizei imageSize, const void *data) {
    glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth,
                              format, imageSize, data);
}

void glGetCompressedTexImageARB(GLenum target, GLint level, void *img) {
    glGetCompressedTexImage(target, level, img);
}

void glMultiTexCoord1fARB(GLenum texture, GLfloat s) {
    glMultiTexCoord1f(texture, s);
}

void glMultiTexCoord1fvARB(GLenum texture, const GLfloat *v) {
    glMultiTexCoord1fv(texture, v);
}

void glMultiTexCoord2fARB(GLenum texture, GLfloat s, GLfloat t) {
    glMultiTexCoord2f(texture, s, t);
}

void glMultiTexCoord2fvARB(GLenum texture, const GLfloat *v) {
    glMultiTexCoord2fv(texture, v);
}

void glMultiTexCoord3fARB(GLenum texture, GLfloat s, GLfloat t, GLfloat r) {
    glMultiTexCoord3f(texture, s, t, r);
}

void glMultiTexCoord3fvARB(GLenum texture, const GLfloat *v) {
    glMultiTexCoord3fv(texture, v);
}

void glMultiTexCoord4fARB(GLenum texture, GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    glMultiTexCoord4f(texture, s, t, r, q);
}

void glMultiTexCoord4fvARB(GLenum texture, const GLfloat *v) {
    glMultiTexCoord4fv(texture, v);
}

void glMultiTexCoord1dARB(GLenum texture, GLdouble s) {
    glMultiTexCoord1d(texture, s);
}

void glMultiTexCoord1dvARB(GLenum texture, const GLdouble *v) {
    glMultiTexCoord1dv(texture, v);
}

void glMultiTexCoord1iARB(GLenum texture, GLint s) {
    glMultiTexCoord1i(texture, s);
}

void glMultiTexCoord1ivARB(GLenum texture, const GLint *v) {
    glMultiTexCoord1iv(texture, v);
}

void glMultiTexCoord1sARB(GLenum texture, GLshort s) {
    glMultiTexCoord1s(texture, s);
}

void glMultiTexCoord1svARB(GLenum texture, const GLshort *v) {
    glMultiTexCoord1sv(texture, v);
}

void glMultiTexCoord2dARB(GLenum texture, GLdouble s, GLdouble t) {
    glMultiTexCoord2d(texture, s, t);
}

void glMultiTexCoord2dvARB(GLenum texture, const GLdouble *v) {
    glMultiTexCoord2dv(texture, v);
}

void glMultiTexCoord2iARB(GLenum texture, GLint s, GLint t) {
    glMultiTexCoord2i(texture, s, t);
}

void glMultiTexCoord2ivARB(GLenum texture, const GLint *v) {
    glMultiTexCoord2iv(texture, v);
}

void glMultiTexCoord2sARB(GLenum texture, GLshort s, GLshort t) {
    glMultiTexCoord2s(texture, s, t);
}

void glMultiTexCoord3dARB(GLenum texture, GLdouble s, GLdouble t, GLdouble r) {
    glMultiTexCoord3d(texture, s, t, r);
}

void glMultiTexCoord3dvARB(GLenum texture, const GLdouble *v) {
    glMultiTexCoord3dv(texture, v);
}

void glMultiTexCoord3iARB(GLenum texture, GLint s, GLint t, GLint r) {
    glMultiTexCoord3i(texture, s, t, r);
}

void glMultiTexCoord3ivARB(GLenum texture, const GLint *v) {
    glMultiTexCoord3iv(texture, v);
}

void glMultiTexCoord3sARB(GLenum texture, GLshort s, GLshort t, GLshort r) {
    glMultiTexCoord3s(texture, s, t, r);
}

void glMultiTexCoord3svARB(GLenum texture, const GLshort *v) {
    glMultiTexCoord3sv(texture, v);
}

void glMultiTexCoord4dARB(GLenum texture, GLdouble s, GLdouble t, GLdouble r, GLdouble q) {
    glMultiTexCoord4d(texture, s, t, r, q);
}

void glMultiTexCoord4dvARB(GLenum texture, const GLdouble *v) {
    glMultiTexCoord4dv(texture, v);
}

void glMultiTexCoord4iARB(GLenum texture, GLint s, GLint t, GLint r, GLint q) {
    glMultiTexCoord4i(texture, s, t, r, q);
}

void glMultiTexCoord4ivARB(GLenum texture, const GLint *v) {
    glMultiTexCoord4iv(texture, v);
}

void glMultiTexCoord4sARB(GLenum texture, GLshort s, GLshort t, GLshort r, GLshort q) {
    glMultiTexCoord4s(texture, s, t, r, q);
}

void glVertexAttribI1iEXT(GLuint index, GLint x) {
    glVertexAttribI1i(index, x);
}

void glVertexAttribI2iEXT(GLuint index, GLint x, GLint y) {
    glVertexAttribI2i(index, x, y);
}

void glVertexAttribI3iEXT(GLuint index, GLint x, GLint y, GLint z) {
    glVertexAttribI3i(index, x, y, z);
}

void glVertexAttribI4iEXT(GLuint index, GLint x, GLint y, GLint z, GLint w) {
    glVertexAttribI4i(index, x, y, z, w);
}

void glVertexAttribI1uiEXT(GLuint index, GLuint x) {
    glVertexAttribI1ui(index, x);
}

void glVertexAttribI2uiEXT(GLuint index, GLuint x, GLuint y) {
    glVertexAttribI2ui(index, x, y);
}

void glVertexAttribI3uiEXT(GLuint index, GLuint x, GLuint y, GLuint z) {
    glVertexAttribI3ui(index, x, y, z);
}

void glVertexAttribI4uiEXT(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    glVertexAttribI4ui(index, x, y, z, w);
}

void glVertexAttribI1ivEXT(GLuint index, const GLint *v) {
    glVertexAttribI1iv(index, v);
}

void glVertexAttribI2ivEXT(GLuint index, const GLint *v) {
    glVertexAttribI2iv(index, v);
}

void glVertexAttribI3ivEXT(GLuint index, const GLint *v) {
    glVertexAttribI3iv(index, v);
}

void glVertexAttribI4ivEXT(GLuint index, const GLint *v) {
    glVertexAttribI4iv(index, v);
}

void glVertexAttribI1uivEXT(GLuint index, const GLuint *v) {
    glVertexAttribI1uiv(index, v);
}

void glVertexAttribI2uivEXT(GLuint index, const GLuint *v) {
    glVertexAttribI2uiv(index, v);
}

void glVertexAttribI3uivEXT(GLuint index, const GLuint *v) {
    glVertexAttribI3uiv(index, v);
}

void glVertexAttribI4uivEXT(GLuint index, const GLuint *v) {
    glVertexAttribI4uiv(index, v);
}

void glVertexAttribIPointerEXT(GLuint index, GLint size, GLenum type, GLsizei stride,
                               const void *pointer) {
    glVertexAttribIPointer(index, size, type, stride, pointer);
}

void glGetVertexAttribIivEXT(GLuint index, GLenum pname, GLint *params) {
    glGetVertexAttribIiv(index, pname, params);
}

void glGetVertexAttribIuivEXT(GLuint index, GLenum pname, GLuint *params) {
    glGetVertexAttribIuiv(index, pname, params);
}

void glFogCoordfEXT(GLfloat coord) {
    glFogCoordf(coord);
}

void glFogCoordfvEXT(const GLfloat *coord) {
    glFogCoordfv(coord);
}

void glFogCoorddEXT(GLdouble coord) {
    glFogCoordd(coord);
}

void glFogCoorddvEXT(const GLdouble *coord) {
    glFogCoorddv(coord);
}

void glFogCoordPointerEXT(GLenum type, GLsizei stride, const void *pointer) {
    glFogCoordPointer(type, stride, pointer);
}

void glSecondaryColor3fEXT(GLfloat r, GLfloat g, GLfloat b) {
    glSecondaryColor3f(r, g, b);
}

void glSecondaryColor3fvEXT(const GLfloat *v) {
    glSecondaryColor3fv(v);
}

void glSecondaryColorPointerEXT(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    glSecondaryColorPointer(size, type, stride, pointer);
}

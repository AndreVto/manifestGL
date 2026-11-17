#include "mgl/gl.h"
#include "core/context.h"

static GLfloat clamp_accum(GLfloat value) {
    if (value < -1.0f)
        return -1.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

void glClearAccum(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    ctx->canonical_state.clear_accum[0] = clamp_accum(red);
    ctx->canonical_state.clear_accum[1] = clamp_accum(green);
    ctx->canonical_state.clear_accum[2] = clamp_accum(blue);
    ctx->canonical_state.clear_accum[3] = clamp_accum(alpha);
}

void glAccum(GLenum op, GLfloat value) {
    struct mgl_context *ctx = mgl_context_current();
    (void)value;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    switch (op) {
        case GL_ACCUM:
        case GL_LOAD:
        case GL_ADD:
        case GL_MULT:
        case GL_RETURN:
            mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            break;
        default:
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
            break;
    }
}

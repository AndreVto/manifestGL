#include "mgl/gl.h"
#include "core/context.h"

static int unpack_packed(GLenum type, GLuint packed, GLboolean normalized, GLfloat out[4]) {
    static const unsigned shifts[4] = {0u, 10u, 20u, 30u};
    static const unsigned widths[4] = {10u, 10u, 10u, 2u};
    struct mgl_context *ctx = mgl_context_current();
    unsigned i;
    if (type != GL_INT_2_10_10_10_REV && type != GL_UNSIGNED_INT_2_10_10_10_REV) {
        if (ctx != NULL)
            mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return 0;
    }
    for (i = 0u; i < 4u; ++i) {
        unsigned width = widths[i];
        unsigned bits = (packed >> shifts[i]) & ((1u << width) - 1u);
        if (type == GL_INT_2_10_10_10_REV) {
            int value =
                (bits & (1u << (width - 1u))) != 0u ? (int)bits - (int)(1u << width) : (int)bits;
            if (normalized) {
                out[i] = (GLfloat)(2 * value + 1) / (GLfloat)((1u << width) - 1u);
            } else {
                out[i] = (GLfloat)value;
            }
        } else {
            out[i] = normalized ? (GLfloat)bits / (GLfloat)((1u << width) - 1u) : (GLfloat)bits;
        }
    }
    return 1;
}

void glVertexAttribP1ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, value, normalized, v))
        return;
    glVertexAttrib1f(index, v[0]);
}

void glVertexAttribP1uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint *value) {
    if (value != NULL)
        glVertexAttribP1ui(index, type, normalized, value[0]);
}

void glVertexAttribP2ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, value, normalized, v))
        return;
    glVertexAttrib2f(index, v[0], v[1]);
}

void glVertexAttribP2uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint *value) {
    if (value != NULL)
        glVertexAttribP2ui(index, type, normalized, value[0]);
}

void glVertexAttribP3ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, value, normalized, v))
        return;
    glVertexAttrib3f(index, v[0], v[1], v[2]);
}

void glVertexAttribP3uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint *value) {
    if (value != NULL)
        glVertexAttribP3ui(index, type, normalized, value[0]);
}

void glVertexAttribP4ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, value, normalized, v))
        return;
    glVertexAttrib4f(index, v[0], v[1], v[2], v[3]);
}

void glVertexAttribP4uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint *value) {
    if (value != NULL)
        glVertexAttribP4ui(index, type, normalized, value[0]);
}

void glColorP3ui(GLenum type, GLuint color) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, color, GL_TRUE, v))
        return;
    glColor3f(v[0], v[1], v[2]);
}

void glColorP3uiv(GLenum type, const GLuint *color) {
    if (color != NULL)
        glColorP3ui(type, color[0]);
}

void glColorP4ui(GLenum type, GLuint color) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, color, GL_TRUE, v))
        return;
    glColor4f(v[0], v[1], v[2], v[3]);
}

void glColorP4uiv(GLenum type, const GLuint *color) {
    if (color != NULL)
        glColorP4ui(type, color[0]);
}

void glNormalP3ui(GLenum type, GLuint coords) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, coords, GL_TRUE, v))
        return;
    glNormal3f(v[0], v[1], v[2]);
}

void glNormalP3uiv(GLenum type, const GLuint *coords) {
    if (coords != NULL)
        glNormalP3ui(type, coords[0]);
}

void glSecondaryColorP3ui(GLenum type, GLuint color) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, color, GL_TRUE, v))
        return;
    glSecondaryColor3f(v[0], v[1], v[2]);
}

void glSecondaryColorP3uiv(GLenum type, const GLuint *color) {
    if (color != NULL)
        glSecondaryColorP3ui(type, color[0]);
}

void glTexCoordP1ui(GLenum type, GLuint coords) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, coords, GL_FALSE, v))
        return;
    glTexCoord1f(v[0]);
}

void glTexCoordP1uiv(GLenum type, const GLuint *coords) {
    if (coords != NULL)
        glTexCoordP1ui(type, coords[0]);
}

void glMultiTexCoordP1ui(GLenum texture, GLenum type, GLuint coords) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, coords, GL_FALSE, v))
        return;
    glMultiTexCoord1f(texture, v[0]);
}

void glMultiTexCoordP1uiv(GLenum texture, GLenum type, const GLuint *coords) {
    if (coords != NULL)
        glMultiTexCoordP1ui(texture, type, coords[0]);
}

void glTexCoordP2ui(GLenum type, GLuint coords) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, coords, GL_FALSE, v))
        return;
    glTexCoord2f(v[0], v[1]);
}

void glTexCoordP2uiv(GLenum type, const GLuint *coords) {
    if (coords != NULL)
        glTexCoordP2ui(type, coords[0]);
}

void glMultiTexCoordP2ui(GLenum texture, GLenum type, GLuint coords) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, coords, GL_FALSE, v))
        return;
    glMultiTexCoord2f(texture, v[0], v[1]);
}

void glMultiTexCoordP2uiv(GLenum texture, GLenum type, const GLuint *coords) {
    if (coords != NULL)
        glMultiTexCoordP2ui(texture, type, coords[0]);
}

void glTexCoordP3ui(GLenum type, GLuint coords) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, coords, GL_FALSE, v))
        return;
    glTexCoord3f(v[0], v[1], v[2]);
}

void glTexCoordP3uiv(GLenum type, const GLuint *coords) {
    if (coords != NULL)
        glTexCoordP3ui(type, coords[0]);
}

void glMultiTexCoordP3ui(GLenum texture, GLenum type, GLuint coords) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, coords, GL_FALSE, v))
        return;
    glMultiTexCoord3f(texture, v[0], v[1], v[2]);
}

void glMultiTexCoordP3uiv(GLenum texture, GLenum type, const GLuint *coords) {
    if (coords != NULL)
        glMultiTexCoordP3ui(texture, type, coords[0]);
}

void glTexCoordP4ui(GLenum type, GLuint coords) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, coords, GL_FALSE, v))
        return;
    glTexCoord4f(v[0], v[1], v[2], v[3]);
}

void glTexCoordP4uiv(GLenum type, const GLuint *coords) {
    if (coords != NULL)
        glTexCoordP4ui(type, coords[0]);
}

void glMultiTexCoordP4ui(GLenum texture, GLenum type, GLuint coords) {
    GLfloat v[4];
    if (mgl_context_current() == NULL || !unpack_packed(type, coords, GL_FALSE, v))
        return;
    glMultiTexCoord4f(texture, v[0], v[1], v[2], v[3]);
}

void glMultiTexCoordP4uiv(GLenum texture, GLenum type, const GLuint *coords) {
    if (coords != NULL)
        glMultiTexCoordP4ui(texture, type, coords[0]);
}

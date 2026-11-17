#include "mgl/gl.h"
#include "core/context.h"

void glRasterPos2i(GLint x, GLint y) {
    glRasterPos2d((GLdouble)x, (GLdouble)y);
}

void glRasterPos2iv(const GLint *v) {
    if (v != NULL)
        glRasterPos2i(v[0], v[1]);
}

void glRasterPos2s(GLshort x, GLshort y) {
    glRasterPos2d((GLdouble)x, (GLdouble)y);
}

void glRasterPos2sv(const GLshort *v) {
    if (v != NULL)
        glRasterPos2s(v[0], v[1]);
}

void glRasterPos3i(GLint x, GLint y, GLint z) {
    glRasterPos3d((GLdouble)x, (GLdouble)y, (GLdouble)z);
}

void glRasterPos3iv(const GLint *v) {
    if (v != NULL)
        glRasterPos3i(v[0], v[1], v[2]);
}

void glRasterPos3s(GLshort x, GLshort y, GLshort z) {
    glRasterPos3d((GLdouble)x, (GLdouble)y, (GLdouble)z);
}

void glRasterPos3sv(const GLshort *v) {
    if (v != NULL)
        glRasterPos3s(v[0], v[1], v[2]);
}

void glRasterPos4i(GLint x, GLint y, GLint z, GLint w) {
    glRasterPos4d((GLdouble)x, (GLdouble)y, (GLdouble)z, (GLdouble)w);
}

void glRasterPos4iv(const GLint *v) {
    if (v != NULL)
        glRasterPos4i(v[0], v[1], v[2], v[3]);
}

void glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w) {
    glRasterPos4d((GLdouble)x, (GLdouble)y, (GLdouble)z, (GLdouble)w);
}

void glRasterPos4sv(const GLshort *v) {
    if (v != NULL)
        glRasterPos4s(v[0], v[1], v[2], v[3]);
}

static void rectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2) {
    struct mgl_context *ctx = mgl_context_current();
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    glBegin(GL_QUADS);
    glVertex2d(x1, y1);
    glVertex2d(x2, y1);
    glVertex2d(x2, y2);
    glVertex2d(x1, y2);
    glEnd();
}

void glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2) {
    rectd((GLdouble)x1, (GLdouble)y1, (GLdouble)x2, (GLdouble)y2);
}

void glRectdv(const GLdouble *v1, const GLdouble *v2) {
    if (v1 != NULL && v2 != NULL)
        glRectd(v1[0], v1[1], v2[0], v2[1]);
}

void glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {
    rectd((GLdouble)x1, (GLdouble)y1, (GLdouble)x2, (GLdouble)y2);
}

void glRectfv(const GLfloat *v1, const GLfloat *v2) {
    if (v1 != NULL && v2 != NULL)
        glRectf(v1[0], v1[1], v2[0], v2[1]);
}

void glRecti(GLint x1, GLint y1, GLint x2, GLint y2) {
    rectd((GLdouble)x1, (GLdouble)y1, (GLdouble)x2, (GLdouble)y2);
}

void glRectiv(const GLint *v1, const GLint *v2) {
    if (v1 != NULL && v2 != NULL)
        glRecti(v1[0], v1[1], v2[0], v2[1]);
}

void glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2) {
    rectd((GLdouble)x1, (GLdouble)y1, (GLdouble)x2, (GLdouble)y2);
}

void glRectsv(const GLshort *v1, const GLshort *v2) {
    if (v1 != NULL && v2 != NULL)
        glRects(v1[0], v1[1], v2[0], v2[1]);
}

static GLfloat snorm_b(GLbyte v) {
    return (2.0f * (GLfloat)v + 1.0f) * (1.0f / 255.0f);
}

static GLfloat snorm_s(GLshort v) {
    return (2.0f * (GLfloat)v + 1.0f) * (1.0f / 65535.0f);
}

static GLfloat snorm_i(GLint v) {
    return (2.0f * (GLfloat)v + 1.0f) * (1.0f / 4294967294.0f);
}

static GLfloat snorm_ub(GLubyte v) {
    return (GLfloat)v * (1.0f / 255.0f);
}

static GLfloat snorm_us(GLushort v) {
    return (GLfloat)v * (1.0f / 65535.0f);
}

static GLfloat snorm_ui(GLuint v) {
    return (GLfloat)((double)v / 4294967295.0);
}

void glSecondaryColor3b(GLbyte r, GLbyte g, GLbyte b) {
    glSecondaryColor3f(snorm_b(r), snorm_b(g), snorm_b(b));
}

void glSecondaryColor3bv(const GLbyte *v) {
    if (v != NULL)
        glSecondaryColor3b(v[0], v[1], v[2]);
}

void glSecondaryColor3d(GLdouble r, GLdouble g, GLdouble b) {
    glSecondaryColor3f((GLfloat)r, (GLfloat)g, (GLfloat)b);
}

void glSecondaryColor3dv(const GLdouble *v) {
    if (v != NULL)
        glSecondaryColor3d(v[0], v[1], v[2]);
}

void glSecondaryColor3i(GLint r, GLint g, GLint b) {
    glSecondaryColor3f(snorm_i(r), snorm_i(g), snorm_i(b));
}

void glSecondaryColor3iv(const GLint *v) {
    if (v != NULL)
        glSecondaryColor3i(v[0], v[1], v[2]);
}

void glSecondaryColor3s(GLshort r, GLshort g, GLshort b) {
    glSecondaryColor3f(snorm_s(r), snorm_s(g), snorm_s(b));
}

void glSecondaryColor3sv(const GLshort *v) {
    if (v != NULL)
        glSecondaryColor3s(v[0], v[1], v[2]);
}

void glSecondaryColor3ub(GLubyte r, GLubyte g, GLubyte b) {
    glSecondaryColor3f(snorm_ub(r), snorm_ub(g), snorm_ub(b));
}

void glSecondaryColor3ubv(const GLubyte *v) {
    if (v != NULL)
        glSecondaryColor3ub(v[0], v[1], v[2]);
}

void glSecondaryColor3ui(GLuint r, GLuint g, GLuint b) {
    glSecondaryColor3f(snorm_ui(r), snorm_ui(g), snorm_ui(b));
}

void glSecondaryColor3uiv(const GLuint *v) {
    if (v != NULL)
        glSecondaryColor3ui(v[0], v[1], v[2]);
}

void glSecondaryColor3us(GLushort r, GLushort g, GLushort b) {
    glSecondaryColor3f(snorm_us(r), snorm_us(g), snorm_us(b));
}

void glSecondaryColor3usv(const GLushort *v) {
    if (v != NULL)
        glSecondaryColor3us(v[0], v[1], v[2]);
}

void glSecondaryColor3ubEXT(GLubyte r, GLubyte g, GLubyte b) {
    glSecondaryColor3ub(r, g, b);
}

void glSecondaryColor3ubvEXT(const GLubyte *v) {
    glSecondaryColor3ubv(v);
}

void glVertexAttribI4bv(GLuint index, const GLbyte *v) {
    if (v != NULL)
        glVertexAttribI4i(index, (GLint)v[0], (GLint)v[1], (GLint)v[2], (GLint)v[3]);
}

void glVertexAttribI4bvEXT(GLuint index, const GLbyte *v) {
    glVertexAttribI4bv(index, v);
}

void glVertexAttribI4sv(GLuint index, const GLshort *v) {
    if (v != NULL)
        glVertexAttribI4i(index, (GLint)v[0], (GLint)v[1], (GLint)v[2], (GLint)v[3]);
}

void glVertexAttribI4svEXT(GLuint index, const GLshort *v) {
    glVertexAttribI4sv(index, v);
}

void glVertexAttribI4ubv(GLuint index, const GLubyte *v) {
    if (v != NULL)
        glVertexAttribI4ui(index, (GLuint)v[0], (GLuint)v[1], (GLuint)v[2], (GLuint)v[3]);
}

void glVertexAttribI4ubvEXT(GLuint index, const GLubyte *v) {
    glVertexAttribI4ubv(index, v);
}

void glVertexAttribI4usv(GLuint index, const GLushort *v) {
    if (v != NULL)
        glVertexAttribI4ui(index, (GLuint)v[0], (GLuint)v[1], (GLuint)v[2], (GLuint)v[3]);
}

void glVertexAttribI4usvEXT(GLuint index, const GLushort *v) {
    glVertexAttribI4usv(index, v);
}

void glIndexdv(const GLdouble *v) {
    if (v != NULL)
        glIndexd(v[0]);
}

void glIndexiv(const GLint *v) {
    if (v != NULL)
        glIndexi(v[0]);
}

void glIndexsv(const GLshort *v) {
    if (v != NULL)
        glIndexs(v[0]);
}

void glIndexubv(const GLubyte *v) {
    if (v != NULL)
        glIndexub(v[0]);
}

void glIndexub(GLubyte value) {
    glIndexf((GLfloat)value);
}

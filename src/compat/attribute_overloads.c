#include "mgl/gl.h"
#include <stdint.h>

/* Compatibility-profile signed colors and normals use the full integer range. */
static GLfloat norm_b(GLbyte v) {
    return (2.0f * (GLfloat)v + 1.0f) * (1.0f / 255.0f);
}

static GLfloat norm_s(GLshort v) {
    return (2.0f * (GLfloat)v + 1.0f) * (1.0f / 65535.0f);
}

static GLfloat norm_i(GLint v) {
    return (2.0f * (GLfloat)v + 1.0f) * (1.0f / 4294967294.0f);
}

static GLfloat norm_ub(GLubyte v) {
    return (GLfloat)v / (GLfloat)UINT8_MAX;
}

static GLfloat norm_us(GLushort v) {
    return (GLfloat)v / (GLfloat)UINT16_MAX;
}

static GLfloat norm_ui(GLuint v) {
    return (GLfloat)((double)v / (double)UINT32_MAX);
}

void glColor3b(GLbyte red, GLbyte green, GLbyte blue) {
    glColor4f(norm_b(red), norm_b(green), norm_b(blue), 1.0f);
}

void glColor3bv(const GLbyte *v) {
    if (v != NULL)
        glColor4f(norm_b(v[0]), norm_b(v[1]), norm_b(v[2]), 1.0f);
}

void glColor3d(GLdouble red, GLdouble green, GLdouble blue) {
    glColor4f((GLfloat)red, (GLfloat)green, (GLfloat)blue, 1.0f);
}

void glColor3dv(const GLdouble *v) {
    if (v != NULL)
        glColor4f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], 1.0f);
}

void glColor3i(GLint red, GLint green, GLint blue) {
    glColor4f(norm_i(red), norm_i(green), norm_i(blue), 1.0f);
}

void glColor3iv(const GLint *v) {
    if (v != NULL)
        glColor4f(norm_i(v[0]), norm_i(v[1]), norm_i(v[2]), 1.0f);
}

void glColor3s(GLshort red, GLshort green, GLshort blue) {
    glColor4f(norm_s(red), norm_s(green), norm_s(blue), 1.0f);
}

void glColor3sv(const GLshort *v) {
    if (v != NULL)
        glColor4f(norm_s(v[0]), norm_s(v[1]), norm_s(v[2]), 1.0f);
}

void glColor3ub(GLubyte red, GLubyte green, GLubyte blue) {
    glColor4f(norm_ub(red), norm_ub(green), norm_ub(blue), 1.0f);
}

void glColor3ui(GLuint red, GLuint green, GLuint blue) {
    glColor4f(norm_ui(red), norm_ui(green), norm_ui(blue), 1.0f);
}

void glColor3uiv(const GLuint *v) {
    if (v != NULL)
        glColor4f(norm_ui(v[0]), norm_ui(v[1]), norm_ui(v[2]), 1.0f);
}

void glColor3us(GLushort red, GLushort green, GLushort blue) {
    glColor4f(norm_us(red), norm_us(green), norm_us(blue), 1.0f);
}

void glColor3usv(const GLushort *v) {
    if (v != NULL)
        glColor4f(norm_us(v[0]), norm_us(v[1]), norm_us(v[2]), 1.0f);
}

void glColor4b(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha) {
    glColor4f(norm_b(red), norm_b(green), norm_b(blue), norm_b(alpha));
}

void glColor4bv(const GLbyte *v) {
    if (v != NULL)
        glColor4f(norm_b(v[0]), norm_b(v[1]), norm_b(v[2]), norm_b(v[3]));
}

void glColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha) {
    glColor4f((GLfloat)red, (GLfloat)green, (GLfloat)blue, (GLfloat)alpha);
}

void glColor4dv(const GLdouble *v) {
    if (v != NULL)
        glColor4f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

void glColor4i(GLint red, GLint green, GLint blue, GLint alpha) {
    glColor4f(norm_i(red), norm_i(green), norm_i(blue), norm_i(alpha));
}

void glColor4iv(const GLint *v) {
    if (v != NULL)
        glColor4f(norm_i(v[0]), norm_i(v[1]), norm_i(v[2]), norm_i(v[3]));
}

void glColor4s(GLshort red, GLshort green, GLshort blue, GLshort alpha) {
    glColor4f(norm_s(red), norm_s(green), norm_s(blue), norm_s(alpha));
}

void glColor4sv(const GLshort *v) {
    if (v != NULL)
        glColor4f(norm_s(v[0]), norm_s(v[1]), norm_s(v[2]), norm_s(v[3]));
}

void glColor4ui(GLuint red, GLuint green, GLuint blue, GLuint alpha) {
    glColor4f(norm_ui(red), norm_ui(green), norm_ui(blue), norm_ui(alpha));
}

void glColor4uiv(const GLuint *v) {
    if (v != NULL)
        glColor4f(norm_ui(v[0]), norm_ui(v[1]), norm_ui(v[2]), norm_ui(v[3]));
}

void glColor4us(GLushort red, GLushort green, GLushort blue, GLushort alpha) {
    glColor4f(norm_us(red), norm_us(green), norm_us(blue), norm_us(alpha));
}

void glColor4usv(const GLushort *v) {
    if (v != NULL)
        glColor4f(norm_us(v[0]), norm_us(v[1]), norm_us(v[2]), norm_us(v[3]));
}

void glMultiTexCoord1d(GLenum target, GLdouble s) {
    glMultiTexCoord4f(target, (GLfloat)s, 0.0f, 0.0f, 1.0f);
}

void glMultiTexCoord1dv(GLenum target, const GLdouble *v) {
    if (v != NULL)
        glMultiTexCoord4f(target, (GLfloat)v[0], 0.0f, 0.0f, 1.0f);
}

void glMultiTexCoord1i(GLenum target, GLint s) {
    glMultiTexCoord4f(target, (GLfloat)s, 0.0f, 0.0f, 1.0f);
}

void glMultiTexCoord1iv(GLenum target, const GLint *v) {
    if (v != NULL)
        glMultiTexCoord4f(target, (GLfloat)v[0], 0.0f, 0.0f, 1.0f);
}

void glMultiTexCoord1s(GLenum target, GLshort s) {
    glMultiTexCoord4f(target, (GLfloat)s, 0.0f, 0.0f, 1.0f);
}

void glMultiTexCoord1sv(GLenum target, const GLshort *v) {
    if (v != NULL)
        glMultiTexCoord4f(target, (GLfloat)v[0], 0.0f, 0.0f, 1.0f);
}

void glMultiTexCoord2d(GLenum target, GLdouble s, GLdouble t) {
    glMultiTexCoord4f(target, (GLfloat)s, (GLfloat)t, 0.0f, 1.0f);
}

void glMultiTexCoord2dv(GLenum target, const GLdouble *v) {
    if (v != NULL)
        glMultiTexCoord4f(target, (GLfloat)v[0], (GLfloat)v[1], 0.0f, 1.0f);
}

void glMultiTexCoord2i(GLenum target, GLint s, GLint t) {
    glMultiTexCoord4f(target, (GLfloat)s, (GLfloat)t, 0.0f, 1.0f);
}

void glMultiTexCoord2iv(GLenum target, const GLint *v) {
    if (v != NULL)
        glMultiTexCoord4f(target, (GLfloat)v[0], (GLfloat)v[1], 0.0f, 1.0f);
}

void glMultiTexCoord2s(GLenum target, GLshort s, GLshort t) {
    glMultiTexCoord4f(target, (GLfloat)s, (GLfloat)t, 0.0f, 1.0f);
}

void glMultiTexCoord2sv(GLenum target, const GLshort *v) {
    if (v != NULL)
        glMultiTexCoord4f(target, (GLfloat)v[0], (GLfloat)v[1], 0.0f, 1.0f);
}

void glMultiTexCoord2svARB(GLenum target, const GLshort *v) {
    if (v != NULL)
        glMultiTexCoord2sv(target, v);
}

void glMultiTexCoord3d(GLenum target, GLdouble s, GLdouble t, GLdouble r) {
    glMultiTexCoord4f(target, (GLfloat)s, (GLfloat)t, (GLfloat)r, 1.0f);
}

void glMultiTexCoord3dv(GLenum target, const GLdouble *v) {
    if (v != NULL)
        glMultiTexCoord4f(target, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], 1.0f);
}

void glMultiTexCoord3i(GLenum target, GLint s, GLint t, GLint r) {
    glMultiTexCoord4f(target, (GLfloat)s, (GLfloat)t, (GLfloat)r, 1.0f);
}

void glMultiTexCoord3iv(GLenum target, const GLint *v) {
    if (v != NULL)
        glMultiTexCoord4f(target, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], 1.0f);
}

void glMultiTexCoord3s(GLenum target, GLshort s, GLshort t, GLshort r) {
    glMultiTexCoord4f(target, (GLfloat)s, (GLfloat)t, (GLfloat)r, 1.0f);
}

void glMultiTexCoord3sv(GLenum target, const GLshort *v) {
    if (v != NULL)
        glMultiTexCoord4f(target, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], 1.0f);
}

void glMultiTexCoord4d(GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q) {
    glMultiTexCoord4f(target, (GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q);
}

void glMultiTexCoord4dv(GLenum target, const GLdouble *v) {
    if (v != NULL)
        glMultiTexCoord4f(target, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

void glMultiTexCoord4i(GLenum target, GLint s, GLint t, GLint r, GLint q) {
    glMultiTexCoord4f(target, (GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q);
}

void glMultiTexCoord4iv(GLenum target, const GLint *v) {
    if (v != NULL)
        glMultiTexCoord4f(target, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

void glMultiTexCoord4s(GLenum target, GLshort s, GLshort t, GLshort r, GLshort q) {
    glMultiTexCoord4f(target, (GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q);
}

void glMultiTexCoord4sv(GLenum target, const GLshort *v) {
    if (v != NULL)
        glMultiTexCoord4f(target, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]);
}

void glMultiTexCoord4svARB(GLenum target, const GLshort *v) {
    if (v != NULL)
        glMultiTexCoord4sv(target, v);
}

void glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz) {
    glNormal3f(norm_b(nx), norm_b(ny), norm_b(nz));
}

void glNormal3bv(const GLbyte *v) {
    if (v != NULL)
        glNormal3f(norm_b(v[0]), norm_b(v[1]), norm_b(v[2]));
}

void glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz) {
    glNormal3f((GLfloat)nx, (GLfloat)ny, (GLfloat)nz);
}

void glNormal3dv(const GLdouble *v) {
    if (v != NULL)
        glNormal3f((GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2]);
}

void glNormal3i(GLint nx, GLint ny, GLint nz) {
    glNormal3f(norm_i(nx), norm_i(ny), norm_i(nz));
}

void glNormal3iv(const GLint *v) {
    if (v != NULL)
        glNormal3f(norm_i(v[0]), norm_i(v[1]), norm_i(v[2]));
}

void glNormal3s(GLshort nx, GLshort ny, GLshort nz) {
    glNormal3f(norm_s(nx), norm_s(ny), norm_s(nz));
}

void glNormal3sv(const GLshort *v) {
    if (v != NULL)
        glNormal3f(norm_s(v[0]), norm_s(v[1]), norm_s(v[2]));
}

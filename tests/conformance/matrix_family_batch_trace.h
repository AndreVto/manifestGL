#ifndef MGL_MATRIX_FAMILY_BATCH_TRACE_H
#define MGL_MATRIX_FAMILY_BATCH_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

/* Differential comparison: 41 matrix entry points, shared by native GLX and mGL.
 * Each legal call runs from a known matrix and records observable matrix
 * values, error, and active-mode preservation. Begin/End probes run
 * the same calls while already in a primitive and inspect state afterward.
 * No pointer contents are read after an illegal call. */
static void mb_emit(const char *name, const char *phase, GLenum observed, GLenum matrix) {
    GLfloat m[16] = {0};
    GLint mode = 0;
    glGetFloatv(matrix, m);
    glGetIntegerv(GL_MATRIX_MODE, &mode);

    printf("mb_%s_%s 0x%x %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f 0x%x 0x%x\n", name, phase,
           (unsigned)observed, (double)m[0], (double)m[1], (double)m[4], (double)m[5],
           (double)m[10], (double)m[12], (double)m[13], (double)m[14], (unsigned)mode,
           (unsigned)glGetError());
}

static void mb_reset(void) {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    (void)glGetError();
}

#define MB_LEGAL(label, statement, mode)                                                           \
    do {                                                                                           \
        mb_reset();                                                                                \
        statement;                                                                                 \
        mb_emit(label, "valid", glGetError(), mode);                                               \
    } while (0)
#define MB_ILLEGAL(label, statement, mode)                                                         \
    do {                                                                                           \
        mb_reset();                                                                                \
        glBegin(GL_POINTS);                                                                        \
        statement;                                                                                 \
        glEnd();                                                                                   \
        mb_emit(label, "begin", glGetError(), mode);                                               \
    } while (0)

static void matrix_family_batch_trace(void) {
    const GLfloat mbf[16] = {2.f, 1.f, 0.f, 0.f, 3.f, 4.f, 0.f, 0.f,
                             0.f, 0.f, 5.f, 0.f, 6.f, 7.f, 8.f, 1.f};
    const GLdouble mbd[16] = {2., 1., 0., 0., 3., 4., 0., 0., 0., 0., 5., 0., 6., 7., 8., 1.};
    MB_LEGAL("glMatrixMode", glMatrixMode(GL_PROJECTION), GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixMode", glMatrixMode(GL_PROJECTION), GL_PROJECTION_MATRIX);
    MB_LEGAL("glLoadIdentity", glLoadIdentity(), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glLoadIdentity", glLoadIdentity(), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glLoadMatrixf", glLoadMatrixf(mbf), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glLoadMatrixf", glLoadMatrixf(mbf), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glLoadMatrixd", glLoadMatrixd(mbd), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glLoadMatrixd", glLoadMatrixd(mbd), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glMultMatrixf", glMultMatrixf(mbf), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glMultMatrixf", glMultMatrixf(mbf), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glMultMatrixd", glMultMatrixd(mbd), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glMultMatrixd", glMultMatrixd(mbd), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glLoadTransposeMatrixf", glLoadTransposeMatrixf(mbf), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glLoadTransposeMatrixf", glLoadTransposeMatrixf(mbf), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glLoadTransposeMatrixd", glLoadTransposeMatrixd(mbd), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glLoadTransposeMatrixd", glLoadTransposeMatrixd(mbd), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glMultTransposeMatrixf", glMultTransposeMatrixf(mbf), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glMultTransposeMatrixf", glMultTransposeMatrixf(mbf), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glMultTransposeMatrixd", glMultTransposeMatrixd(mbd), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glMultTransposeMatrixd", glMultTransposeMatrixd(mbd), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glTranslatef", glTranslatef(2.f, 3.f, 4.f), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glTranslatef", glTranslatef(2.f, 3.f, 4.f), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glTranslated", glTranslated(2., 3., 4.), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glTranslated", glTranslated(2., 3., 4.), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glScalef", glScalef(2.f, 3.f, 4.f), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glScalef", glScalef(2.f, 3.f, 4.f), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glScaled", glScaled(2., 3., 4.), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glScaled", glScaled(2., 3., 4.), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glRotatef", glRotatef(90.f, 0.f, 0.f, 1.f), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glRotatef", glRotatef(90.f, 0.f, 0.f, 1.f), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glRotated", glRotated(90., 0., 0., 1.), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glRotated", glRotated(90., 0., 0., 1.), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glOrtho", glOrtho(-1., 1., -1., 1., 1., 9.), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glOrtho", glOrtho(-1., 1., -1., 1., 1., 9.), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glOrthof", glOrthof(-1.f, 1.f, -1.f, 1.f, 1.f, 9.f), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glOrthof", glOrthof(-1.f, 1.f, -1.f, 1.f, 1.f, 9.f), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glFrustum", glFrustum(-1., 1., -1., 1., 1., 9.), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glFrustum", glFrustum(-1., 1., -1., 1., 1., 9.), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glFrustumf", glFrustumf(-1.f, 1.f, -1.f, 1.f, 1.f, 9.f), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glFrustumf", glFrustumf(-1.f, 1.f, -1.f, 1.f, 1.f, 9.f), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glMatrixLoadfEXT", glMatrixLoadfEXT(GL_PROJECTION, mbf), GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixLoadfEXT", glMatrixLoadfEXT(GL_PROJECTION, mbf), GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixLoaddEXT", glMatrixLoaddEXT(GL_PROJECTION, mbd), GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixLoaddEXT", glMatrixLoaddEXT(GL_PROJECTION, mbd), GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixMultfEXT", glMatrixMultfEXT(GL_PROJECTION, mbf), GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixMultfEXT", glMatrixMultfEXT(GL_PROJECTION, mbf), GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixMultdEXT", glMatrixMultdEXT(GL_PROJECTION, mbd), GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixMultdEXT", glMatrixMultdEXT(GL_PROJECTION, mbd), GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixLoadIdentityEXT", glMatrixLoadIdentityEXT(GL_PROJECTION),
             GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixLoadIdentityEXT", glMatrixLoadIdentityEXT(GL_PROJECTION),
               GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixLoadTransposefEXT", glMatrixLoadTransposefEXT(GL_PROJECTION, mbf),
             GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixLoadTransposefEXT", glMatrixLoadTransposefEXT(GL_PROJECTION, mbf),
               GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixLoadTransposedEXT", glMatrixLoadTransposedEXT(GL_PROJECTION, mbd),
             GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixLoadTransposedEXT", glMatrixLoadTransposedEXT(GL_PROJECTION, mbd),
               GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixMultTransposefEXT", glMatrixMultTransposefEXT(GL_PROJECTION, mbf),
             GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixMultTransposefEXT", glMatrixMultTransposefEXT(GL_PROJECTION, mbf),
               GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixMultTransposedEXT", glMatrixMultTransposedEXT(GL_PROJECTION, mbd),
             GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixMultTransposedEXT", glMatrixMultTransposedEXT(GL_PROJECTION, mbd),
               GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixTranslatefEXT", glMatrixTranslatefEXT(GL_PROJECTION, 2.f, 3.f, 4.f),
             GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixTranslatefEXT", glMatrixTranslatefEXT(GL_PROJECTION, 2.f, 3.f, 4.f),
               GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixTranslatedEXT", glMatrixTranslatedEXT(GL_PROJECTION, 2., 3., 4.),
             GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixTranslatedEXT", glMatrixTranslatedEXT(GL_PROJECTION, 2., 3., 4.),
               GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixScalefEXT", glMatrixScalefEXT(GL_PROJECTION, 2.f, 3.f, 4.f),
             GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixScalefEXT", glMatrixScalefEXT(GL_PROJECTION, 2.f, 3.f, 4.f),
               GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixScaledEXT", glMatrixScaledEXT(GL_PROJECTION, 2., 3., 4.),
             GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixScaledEXT", glMatrixScaledEXT(GL_PROJECTION, 2., 3., 4.),
               GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixRotatefEXT", glMatrixRotatefEXT(GL_PROJECTION, 90.f, 0.f, 0.f, 1.f),
             GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixRotatefEXT", glMatrixRotatefEXT(GL_PROJECTION, 90.f, 0.f, 0.f, 1.f),
               GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixRotatedEXT", glMatrixRotatedEXT(GL_PROJECTION, 90., 0., 0., 1.),
             GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixRotatedEXT", glMatrixRotatedEXT(GL_PROJECTION, 90., 0., 0., 1.),
               GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixOrthoEXT", glMatrixOrthoEXT(GL_PROJECTION, -1., 1., -1., 1., 1., 9.),
             GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixOrthoEXT", glMatrixOrthoEXT(GL_PROJECTION, -1., 1., -1., 1., 1., 9.),
               GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixFrustumEXT", glMatrixFrustumEXT(GL_PROJECTION, -1., 1., -1., 1., 1., 9.),
             GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixFrustumEXT", glMatrixFrustumEXT(GL_PROJECTION, -1., 1., -1., 1., 1., 9.),
               GL_PROJECTION_MATRIX);
    MB_LEGAL("glPushMatrix", glPushMatrix(), GL_MODELVIEW_MATRIX);
    glPopMatrix();
    (void)glGetError();
    MB_ILLEGAL("glPushMatrix", glPushMatrix(), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glPopMatrix", glPopMatrix(), GL_MODELVIEW_MATRIX);
    MB_ILLEGAL("glPopMatrix", glPopMatrix(), GL_MODELVIEW_MATRIX);
    MB_LEGAL("glMatrixPushEXT", glMatrixPushEXT(GL_PROJECTION), GL_PROJECTION_MATRIX);
    glMatrixPopEXT(GL_PROJECTION);
    (void)glGetError();
    MB_ILLEGAL("glMatrixPushEXT", glMatrixPushEXT(GL_PROJECTION), GL_PROJECTION_MATRIX);
    MB_LEGAL("glMatrixPopEXT", glMatrixPopEXT(GL_PROJECTION), GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixPopEXT", glMatrixPopEXT(GL_PROJECTION), GL_PROJECTION_MATRIX);
    mb_reset();
    glMatrixMode(0xDEADu);
    mb_emit("glMatrixMode", "bad_enum", glGetError(), GL_MODELVIEW_MATRIX);
    mb_reset();
    glFrustum(0., 0., -1., 1., 1., 9.);
    mb_emit("glFrustum", "bad_range", glGetError(), GL_MODELVIEW_MATRIX);
    mb_reset();
    glOrtho(0., 0., -1., 1., 1., 9.);
    mb_emit("glOrtho", "bad_range", glGetError(), GL_MODELVIEW_MATRIX);
    mb_reset();
    glMatrixFrustumEXT(GL_PROJECTION, 0., 0., -1., 1., 1., 9.);
    mb_emit("glMatrixFrustumEXT", "bad_range", glGetError(), GL_PROJECTION_MATRIX);
    mb_reset();
    glMatrixOrthoEXT(GL_PROJECTION, 0., 0., -1., 1., 1., 9.);
    mb_emit("glMatrixOrthoEXT", "bad_range", glGetError(), GL_PROJECTION_MATRIX);
    mb_reset();
    glMatrixLoadIdentityEXT(0xDEADu);
    mb_emit("glMatrixLoadIdentityEXT", "bad_enum", glGetError(), GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixLoadfEXT_bad_enum", glMatrixLoadfEXT(0xDEADu, mbf), GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixRotatefEXT_zero_axis",
               glMatrixRotatefEXT(GL_PROJECTION, 90.f, 0.f, 0.f, 0.f), GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixFrustumEXT_bad_range",
               glMatrixFrustumEXT(GL_PROJECTION, 0., 0., -1., 1., 1., 9.), GL_PROJECTION_MATRIX);
    MB_ILLEGAL("glMatrixOrthoEXT_bad_range",
               glMatrixOrthoEXT(GL_PROJECTION, 0., 0., -1., 1., 1., 9.), GL_PROJECTION_MATRIX);
    mb_reset();
    glPopMatrix();
    mb_emit("glPopMatrix", "underflow", glGetError(), GL_MODELVIEW_MATRIX);
    mb_reset();
    glMatrixPopEXT(GL_PROJECTION);
    mb_emit("glMatrixPopEXT", "underflow", glGetError(), GL_PROJECTION_MATRIX);
    mb_reset();
}

#undef MB_LEGAL
#undef MB_ILLEGAL
#endif

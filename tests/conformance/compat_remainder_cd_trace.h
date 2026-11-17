#ifndef MGL_COMPAT_REMAINDER_CD_TRACE_H
#define MGL_COMPAT_REMAINDER_CD_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

/* Mapped to native EXT symbols only on the reference runner, where the
 * ManifestGL unsuffixed compatibility aliases are not exported. */
#ifdef MGL_MESA_REFERENCE_CD_ALIASES
#define glLockArrays glLockArraysEXT
#define glUnlockArrays glUnlockArraysEXT
#define glEnableIndexed glEnableIndexedEXT
#define glDisableIndexed glDisableIndexedEXT
#define glIsEnabledIndexed glIsEnabledIndexedEXT
#define glBindMultiTexture glBindMultiTextureEXT
#endif

#define CD_CHECK(name, operation)                                                                  \
    do {                                                                                           \
        operation;                                                                                 \
        printf("cd_%s 0x%04x\n", name, (unsigned)glGetError());                                    \
    } while (0)
#define CD_BEGIN(name, operation)                                                                  \
    do {                                                                                           \
        glBegin(GL_POINTS);                                                                        \
        operation;                                                                                 \
        glEnd();                                                                                   \
        printf("cd_%s_inside 0x%04x\n", name, (unsigned)glGetError());                             \
    } while (0)

static void compat_remainder_cd_trace(void) {
    const GLfloat ce[4] = {0.25f, 0.f, 0.f, 1.f};
    const GLfixed cx[4] = {16384, 0, 0, 65536};
    const GLfloat raster2[2] = {0.25f, 0.5f};
    const GLdouble ev1[1] = {0.375};
    const GLfloat ev2[2] = {0.25f, 0.75f};
    const GLdouble ed2[2] = {0.25, 0.75};
    const GLfloat evf1[1] = {0.25f};
    const GLint vbo_offset = 16;
    GLfloat clip[4] = {0};
    GLfixed clipx[4] = {0};
    GLfloat pos[4] = {0};
    GLboolean enabled = 0;
    GLuint tex = 0, buf = 0;
    GLint bound = 0;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    (void)glGetError();
    CD_CHECK("clip_f_set", glClipPlanef(GL_CLIP_PLANE0, ce));
    CD_CHECK("clip_x_set", glClipPlanex(GL_CLIP_PLANE0, cx));
    CD_BEGIN("clip_f_set", glClipPlanef(GL_CLIP_PLANE0, ce));
    CD_BEGIN("clip_x_set", glClipPlanex(GL_CLIP_PLANE0, cx));
    clip[0] = -11.f;
    CD_BEGIN("clip_f_get", glGetClipPlanef(GL_CLIP_PLANE0, clip));
    printf("cd_clip_f_reject_output %.0f\n", (double)clip[0]);
    clipx[0] = -11;
    CD_BEGIN("clip_x_get", glGetClipPlanex(GL_CLIP_PLANE0, clipx));
    printf("cd_clip_x_reject_output %d\n", (int)clipx[0]);
    CD_CHECK("clip_f_bad_plane", glClipPlanef(0xDEADu, ce));
    CD_CHECK("clip_x_bad_plane", glClipPlanex(0xDEADu, cx));

    CD_CHECK("eval1d", glEvalCoord1d(0.25));
    CD_CHECK("eval1dv", glEvalCoord1dv(ev1));
    CD_CHECK("eval1fv", glEvalCoord1fv(evf1));
    CD_CHECK("eval2f", glEvalCoord2f(0.25f, 0.5f));
    CD_CHECK("eval2dv", glEvalCoord2dv(ed2));
    CD_CHECK("eval2fv", glEvalCoord2fv(ev2));
    CD_BEGIN("eval1d", glEvalCoord1d(0.25));
    CD_BEGIN("eval1dv", glEvalCoord1dv(ev1));
    CD_BEGIN("eval1fv", glEvalCoord1fv(evf1));
    CD_BEGIN("eval2f", glEvalCoord2f(0.25f, 0.5f));
    CD_BEGIN("eval2dv", glEvalCoord2dv(ed2));
    CD_BEGIN("eval2fv", glEvalCoord2fv(ev2));

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glViewport(0, 0, 64, 64);
    glDepthRange(0., 1.);
    (void)glGetError();
    CD_CHECK("raster2f", glRasterPos2f(0.f, 0.f));
    glGetFloatv(GL_CURRENT_RASTER_POSITION, pos);
    printf("cd_raster2f_pos %.1f %.1f %.1f 0x%04x\n", (double)pos[0], (double)pos[1],
           (double)pos[2], (unsigned)glGetError());
    CD_CHECK("raster3f", glRasterPos3f(0.f, 0.f, 0.f));
    glGetFloatv(GL_CURRENT_RASTER_POSITION, pos);
    printf("cd_raster3f_pos %.1f %.1f %.1f 0x%04x\n", (double)pos[0], (double)pos[1],
           (double)pos[2], (unsigned)glGetError());
    CD_CHECK("window2fv", glWindowPos2fv(raster2));
    glGetFloatv(GL_CURRENT_RASTER_POSITION, pos);
    printf("cd_window2fv_pos %.2f %.2f 0x%04x\n", (double)pos[0], (double)pos[1],
           (unsigned)glGetError());
    CD_BEGIN("raster2f", glRasterPos2f(1.f, 1.f));
    CD_BEGIN("raster3f", glRasterPos3f(1.f, 1.f, 1.f));
    CD_BEGIN("window2fv", glWindowPos2fv(raster2));

    CD_CHECK("lock_arrays", glLockArrays(0, 4));
    CD_CHECK("unlock_arrays", glUnlockArrays());
    CD_CHECK("lock_bad_first", glLockArrays(-1, 4));
    CD_CHECK("lock_bad_count", glLockArrays(0, -1));
    CD_BEGIN("lock_arrays", glLockArrays(0, 4));
    CD_BEGIN("unlock_arrays", glUnlockArrays());

    CD_CHECK("indexed_enable", glEnableIndexed(GL_BLEND, 0));
    enabled = glIsEnabledIndexed(GL_BLEND, 0);
    printf("cd_indexed_read %d 0x%04x\n", (int)enabled, (unsigned)glGetError());
    CD_CHECK("indexed_disable", glDisableIndexed(GL_BLEND, 0));
    enabled = glIsEnabledIndexed(GL_BLEND, 0);
    printf("cd_indexed_off %d 0x%04x\n", (int)enabled, (unsigned)glGetError());
    CD_BEGIN("indexed_enable", glEnableIndexed(GL_BLEND, 0));
    CD_BEGIN("indexed_disable", glDisableIndexed(GL_BLEND, 0));
    glBegin(GL_POINTS);
    enabled = glIsEnabledIndexed(GL_BLEND, 0);
    glEnd();
    printf("cd_indexed_get_inside %d 0x%04x\n", (int)enabled, (unsigned)glGetError());

    glGenTextures(1, &tex);
    CD_CHECK("multitex_bind", glBindMultiTexture(GL_TEXTURE0, GL_TEXTURE_2D, tex));
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    printf("cd_multitex_bound %d 0x%04x\n", bound == ((GLint)tex), (unsigned)glGetError());
    CD_BEGIN("multitex_bind", glBindMultiTexture(GL_TEXTURE0, GL_TEXTURE_2D, tex));
    CD_BEGIN("multitex_bad_target", glBindMultiTexture(GL_TEXTURE0, 0xDEADu, tex));
    glNewList(9934u, GL_COMPILE);
    glBegin(GL_POINTS);
    glBindMultiTexture(GL_TEXTURE0, GL_TEXTURE_2D, 0);
    glEnd();
    glEndList();
    printf("cd_multitex_list_compile 0x%04x\n", (unsigned)glGetError());
    glCallList(9934u);
    printf("cd_multitex_list_replay 0x%04x\n", (unsigned)glGetError());
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    printf("cd_multitex_list_binding %d 0x%04x\n", bound == (GLint)tex, (unsigned)glGetError());
    glDeleteLists(9934u, 1);
    glDeleteTextures(1, &tex);
    (void)glGetError();

    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);
    CD_CHECK("arb_pointer_valid", glVertexAttribPointerARB(2, 3, GL_FLOAT, GL_FALSE, 12,
                                                           (const void *)(size_t)vbo_offset));
    CD_CHECK("arb_attrib_enable", glEnableVertexAttribArrayARB(2));
    CD_CHECK("arb_attrib_disable", glDisableVertexAttribArrayARB(2));
    CD_BEGIN("arb_pointer", glVertexAttribPointerARB(2, 3, GL_FLOAT, GL_FALSE, 12,
                                                     (const void *)(size_t)vbo_offset));
    CD_BEGIN("arb_attrib_enable", glEnableVertexAttribArrayARB(2));
    CD_BEGIN("arb_attrib_disable", glDisableVertexAttribArrayARB(2));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &buf);
    (void)glGetError();
}

#undef CD_CHECK
#undef CD_BEGIN
#ifdef MGL_MESA_REFERENCE_CD_ALIASES
#undef glLockArrays
#undef glUnlockArrays
#undef glEnableIndexed
#undef glDisableIndexed
#undef glIsEnabledIndexed
#undef glBindMultiTexture
#endif
#endif

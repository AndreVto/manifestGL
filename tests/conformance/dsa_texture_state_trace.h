#ifndef MGL_DSA_TEXTURE_STATE_TRACE_H
#define MGL_DSA_TEXTURE_STATE_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void dsa_texture_state_trace(void) {
    GLuint tex = 0;
    GLuint names[1];
    GLfloat floats[4];
    GLint ints[4];
    GLuint uints[4];
    GLint binding = 0;
    const GLint wrap[1] = {GL_CLAMP_TO_EDGE};
    const GLfloat lod[1] = {2.75f};
    const GLint border_i[4] = {12, -34, 56, 78};
    const GLuint border_ui[4] = {3u, 4u, 5u, 6u};
    while (glGetError() != GL_NO_ERROR) {}
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    printf("dsa_create %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(tex));
    glBindTextureUnit(1, tex);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    printf("dsa_bind_unit %04x %u\n", (unsigned)glGetError(), (unsigned)(binding == (GLint)tex));
    glActiveTexture(GL_TEXTURE0);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteriv(tex, GL_TEXTURE_WRAP_S, wrap);
    glGetTextureParameteriv(tex, GL_TEXTURE_WRAP_S, ints);
    printf("dsa_wrap %04x %04x\n", (unsigned)glGetError(), (unsigned)ints[0]);
    glTextureParameterf(tex, GL_TEXTURE_MIN_LOD, 2.75f);
    glTextureParameterfv(tex, GL_TEXTURE_MIN_LOD, lod);
    glGetTextureParameteriv(tex, GL_TEXTURE_MIN_LOD, ints);
    glGetTextureParameterfv(tex, GL_TEXTURE_MIN_LOD, floats);
    printf("dsa_lod %04x %d %.2f\n", (unsigned)glGetError(), ints[0], (double)floats[0]);
    glTextureParameterf(tex, GL_TEXTURE_MAX_LOD, -2.5f);
    glGetTextureParameteriv(tex, GL_TEXTURE_MAX_LOD, ints);
    printf("dsa_negative_lod %04x %d\n", (unsigned)glGetError(), ints[0]);
    glTextureParameterIiv(tex, GL_TEXTURE_BORDER_COLOR, border_i);
    glGetTextureParameterIiv(tex, GL_TEXTURE_BORDER_COLOR, ints);
    printf("dsa_border_i %04x %d %d\n", (unsigned)glGetError(), ints[0], ints[1]);
    glTextureParameterIuiv(tex, GL_TEXTURE_BORDER_COLOR, border_ui);
    glGetTextureParameterIuiv(tex, GL_TEXTURE_BORDER_COLOR, uints);
    printf("dsa_border_u %04x %u %u\n", (unsigned)glGetError(), uints[0], uints[1]);
    glBegin(GL_POINTS);
    glCreateTextures((GLenum)0xDEAD, -1, names);
    glEnd();
    printf("dsa_create_begin %04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glBindTextureUnit(9999u, tex);
    glEnd();
    printf("dsa_bind_begin %04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_S, (GLint)0xDEAD);
    glEnd();
    printf("dsa_param_i_begin %04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glTextureParameterf(tex, GL_TEXTURE_MIN_LOD, 9.0f);
    glEnd();
    printf("dsa_param_f_begin %04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glTextureParameteriv(tex, GL_TEXTURE_WRAP_S, wrap);
    glEnd();
    printf("dsa_param_iv_begin %04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glTextureParameterfv(tex, GL_TEXTURE_MIN_LOD, lod);
    glEnd();
    printf("dsa_param_fv_begin %04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glTextureParameterIiv(tex, GL_TEXTURE_BORDER_COLOR, border_i);
    glEnd();
    printf("dsa_param_Iiv_begin %04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glTextureParameterIuiv(tex, GL_TEXTURE_BORDER_COLOR, border_ui);
    glEnd();
    printf("dsa_param_Iuiv_begin %04x\n", (unsigned)glGetError());
    ints[0] = -717;
    glBegin(GL_POINTS);
    glGetTextureParameteriv(tex, (GLenum)0xDEAD, ints);
    glEnd();
    printf("dsa_get_iv_begin %04x %d\n", (unsigned)glGetError(), ints[0]);
    floats[0] = -717.f;
    glBegin(GL_POINTS);
    glGetTextureParameterfv(tex, GL_TEXTURE_MIN_LOD, floats);
    glEnd();
    printf("dsa_get_fv_begin %04x %.0f\n", (unsigned)glGetError(), (double)floats[0]);
    ints[0] = -717;
    glBegin(GL_POINTS);
    glGetTextureParameterIiv(tex, GL_TEXTURE_BORDER_COLOR, ints);
    glEnd();
    printf("dsa_get_Iiv_begin %04x %d\n", (unsigned)glGetError(), ints[0]);
    uints[0] = 717u;
    glBegin(GL_POINTS);
    glGetTextureParameterIuiv(tex, GL_TEXTURE_BORDER_COLOR, uints);
    glEnd();
    printf("dsa_get_Iuiv_begin %04x %u\n", (unsigned)glGetError(), uints[0]);
    glGetTextureParameteriv(tex, GL_TEXTURE_WRAP_S, ints);
    glGetTextureParameterfv(tex, GL_TEXTURE_MIN_LOD, floats);
    printf("dsa_preserved %04x %04x %.2f\n", (unsigned)glGetError(), (unsigned)ints[0],
           (double)floats[0]);
    {
        GLboolean residency[1] = {GL_FALSE};
        GLfloat priority = 0.6f;
        GLfloat pval = -1.f;
        GLboolean is_resident = glAreTexturesResident(1, &tex, residency);
        printf("dsa_resident %04x %u\n", (unsigned)glGetError(), (unsigned)is_resident);
        glPrioritizeTextures(1, &tex, &priority);
        glGetTextureParameterfv(tex, GL_TEXTURE_PRIORITY, &pval);
        printf("dsa_priority %04x %.2f\n", (unsigned)glGetError(), (double)pval);
        residency[0] = 7;
        glBegin(GL_POINTS);
        (void)glAreTexturesResident(-1, &tex, residency);
        glEnd();
        printf("dsa_resident_begin %04x %u\n", (unsigned)glGetError(), (unsigned)residency[0]);
        priority = 0.1f;
        glBegin(GL_POINTS);
        glPrioritizeTextures(1, &tex, &priority);
        glEnd();
        glGetTextureParameterfv(tex, GL_TEXTURE_PRIORITY, &pval);
        printf("dsa_priority_begin %04x %.2f\n", (unsigned)glGetError(), (double)pval);
    }
    glDeleteTextures(1, &tex);
}
#endif

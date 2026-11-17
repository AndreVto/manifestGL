#ifndef MGL_FOG_STATE_TRACE_H
#define MGL_FOG_STATE_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void fog_state_trace(void) {
    GLfloat color[4] = {0.125f, 0.25f, 0.5f, 0.75f};
    GLfloat value = -99.f, read_color[4] = {0.f};
    GLint mode = -1, source = -1;
    GLint icolor[4] = {0, 0, 0, 0};

    glFogf(GL_FOG_DENSITY, 0.75f);
    glGetFloatv(GL_FOG_DENSITY, &value);
    printf("fog_density_init %.3f 0x%04x\n", value, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glFogf(GL_FOG_DENSITY, 0.25f);
    glEnd();
    glGetFloatv(GL_FOG_DENSITY, &value);
    printf("fog_f_inside %.3f 0x%04x\n", value, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glFogf(0xDEAD, -2.f);
    glEnd();
    printf("fog_f_invalid_inside 0x%04x\n", (unsigned)glGetError());
    glFogf(GL_FOG_DENSITY, -1.f);
    printf("fog_density_negative 0x%04x\n", (unsigned)glGetError());
    glFogf(0xDEAD, 1.f);
    printf("fog_f_invalid_enum 0x%04x\n", (unsigned)glGetError());

    glFogi(GL_FOG_MODE, GL_EXP2);
    glGetIntegerv(GL_FOG_MODE, &mode);
    printf("fog_mode_init %d 0x%04x\n", mode == (GLint)GL_EXP2, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glEnd();
    glGetIntegerv(GL_FOG_MODE, &mode);
    printf("fog_i_inside %d 0x%04x\n", mode == (GLint)GL_EXP2, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glFogi(0xDEAD, 0);
    glEnd();
    printf("fog_i_invalid_inside 0x%04x\n", (unsigned)glGetError());

    glFogfv(GL_FOG_COLOR, color);
    glGetFloatv(GL_FOG_COLOR, read_color);
    printf("fog_color_init %.3f %.3f 0x%04x\n", read_color[0], read_color[2],
           (unsigned)glGetError());
    color[0] = 0.875f;
    glBegin(GL_POINTS);
    glFogfv(GL_FOG_COLOR, color);
    glEnd();
    glGetFloatv(GL_FOG_COLOR, read_color);
    printf("fog_fv_inside %.3f 0x%04x\n", read_color[0], (unsigned)glGetError());
    glBegin(GL_POINTS);
    glFogfv(0xDEAD, color);
    glEnd();
    printf("fog_fv_invalid_inside 0x%04x\n", (unsigned)glGetError());

    icolor[0] = 0;
    icolor[1] = 1073741824;
    icolor[2] = -1073741824;
    icolor[3] = 2147483647;
    glFogiv(GL_FOG_COLOR, icolor);
    glGetFloatv(GL_FOG_COLOR, read_color);
    printf("fog_color_integer %.3f %.3f %.3f 0x%04x\n", read_color[0], read_color[1], read_color[2],
           (unsigned)glGetError());
    glBegin(GL_POINTS);
    glFogiv(GL_FOG_COLOR, icolor);
    glEnd();
    printf("fog_iv_inside 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glFogiv(0xDEAD, icolor);
    glEnd();
    printf("fog_iv_invalid_inside 0x%04x\n", (unsigned)glGetError());
    glFogi(GL_FOG_COORD_SRC, GL_FOG_COORD);
    glGetIntegerv(GL_FOG_COORD_SRC, &source);
    printf("fog_coord_src %d 0x%04x\n", source == (GLint)GL_FOG_COORD, (unsigned)glGetError());

    glFogf(GL_FOG_DENSITY, 0.75f);
    glNewList(9271, GL_COMPILE);
    glBegin(GL_POINTS);
    glFogf(GL_FOG_DENSITY, 0.25f);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, color);
    glFogiv(GL_FOG_COLOR, icolor);
    glEnd();
    glEndList();
    printf("fog_compile 0x%04x\n", (unsigned)glGetError());
    glGetFloatv(GL_FOG_DENSITY, &value);
    printf("fog_compile_preserved %.3f 0x%04x\n", value, (unsigned)glGetError());
    glCallList(9271);
    printf("fog_replay 0x%04x\n", (unsigned)glGetError());
    glGetFloatv(GL_FOG_DENSITY, &value);
    printf("fog_replay_preserved %.3f 0x%04x\n", value, (unsigned)glGetError());
    glDeleteLists(9271, 1);
    glNewList(9272, GL_COMPILE_AND_EXECUTE);
    glBegin(GL_POINTS);
    glFogf(GL_FOG_DENSITY, 0.5f);
    glEnd();
    glEndList();
    printf("fog_compile_execute 0x%04x\n", (unsigned)glGetError());
    glDeleteLists(9272, 1);
    glFogf(GL_FOG_DENSITY, 0.625f);
    glGetFloatv(GL_FOG_DENSITY, &value);
    printf("fog_recovery %.3f 0x%04x\n", value, (unsigned)glGetError());
    {
        GLfloat snapshot[4] = {0.25f, 0.50f, 0.75f, 1.00f};
        glNewList(9275, GL_COMPILE);
        glFogfv(GL_FOG_COLOR, snapshot);
        glFogf(GL_FOG_DENSITY, 0.375f);
        glEndList();
        printf("fog_valid_compile 0x%04x\n", (unsigned)glGetError());
        snapshot[0] = 0.875f;
        glCallList(9275);
        glGetFloatv(GL_FOG_COLOR, read_color);
        printf("fog_list_snapshot %.3f 0x%04x\n", read_color[0], (unsigned)glGetError());
        glGetFloatv(GL_FOG_DENSITY, &value);
        printf("fog_list_apply %.3f 0x%04x\n", value, (unsigned)glGetError());
        glDeleteLists(9275, 1);
    }
    {
        GLfloat saturated[4] = {-3.f, 3.f, 0.25f, 1.f};
        glFogfv(GL_FOG_COLOR, saturated);
        glGetFloatv(GL_FOG_COLOR, read_color);
        printf("fog_color_clamp %.1f %.1f 0x%04x\n", read_color[0], read_color[1],
               (unsigned)glGetError());
    }
}
#endif

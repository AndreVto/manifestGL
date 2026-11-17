#ifndef MGL_LIGHT_STATE_TRACE_H
#define MGL_LIGHT_STATE_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void light_state_trace(void) {
    GLfloat ambient[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    GLfloat read[4] = {0.f};
    GLfloat scalar = 0.f;
    GLint int_color[4] = {0, 1073741824, -1073741824, 2147483647};
    GLint integer = -5;
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glGetLightfv(GL_LIGHT0, GL_AMBIENT, read);
    printf("light_ambient %.2f %.2f 0x%04x\n", read[0], read[2], (unsigned)glGetError());
    glBegin(GL_POINTS);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glEnd();
    printf("light_fv_begin 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 4.f);
    glEnd();
    printf("light_f_begin 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glLighti(GL_LIGHT0, GL_SPOT_CUTOFF, 45);
    glEnd();
    printf("light_i_begin 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glLightiv(GL_LIGHT0, GL_AMBIENT, int_color);
    glEnd();
    printf("light_iv_begin 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glLightfv(0xDEAD, GL_AMBIENT, ambient);
    glEnd();
    printf("light_bad_inside 0x%04x\n", (unsigned)glGetError());
    read[0] = -9.f;
    glBegin(GL_POINTS);
    glGetLightfv(GL_LIGHT0, GL_AMBIENT, read);
    glEnd();
    printf("light_get_fv_begin %.1f 0x%04x\n", read[0], (unsigned)glGetError());
    glBegin(GL_POINTS);
    glGetLightiv(GL_LIGHT0, GL_SPOT_CUTOFF, &integer);
    glEnd();
    printf("light_get_iv_begin %d 0x%04x\n", integer, (unsigned)glGetError());
    glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 9.5f);
    glGetLightfv(GL_LIGHT0, GL_SPOT_EXPONENT, &scalar);
    printf("light_exp %.1f 0x%04x\n", scalar, (unsigned)glGetError());
    glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, -1.f);
    printf("light_exp_invalid 0x%04x\n", (unsigned)glGetError());
    glLighti(GL_LIGHT0, GL_SPOT_CUTOFF, 90);
    glGetLightiv(GL_LIGHT0, GL_SPOT_CUTOFF, &integer);
    printf("light_cutoff %d 0x%04x\n", integer, (unsigned)glGetError());
    glLightiv(GL_LIGHT0, GL_AMBIENT, int_color);
    glGetLightfv(GL_LIGHT0, GL_AMBIENT, read);
    printf("light_integer_color %.3f %.3f 0x%04x\n", read[1], read[2], (unsigned)glGetError());
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);

    glNewList(9273, GL_COMPILE);
    glBegin(GL_POINTS);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 17.f);
    glLighti(GL_LIGHT0, GL_SPOT_CUTOFF, 60);
    glLightiv(GL_LIGHT0, GL_AMBIENT, int_color);
    glEnd();
    glEndList();
    printf("light_compile 0x%04x\n", (unsigned)glGetError());
    glCallList(9273);
    printf("light_replay 0x%04x\n", (unsigned)glGetError());
    glGetLightfv(GL_LIGHT0, GL_SPOT_EXPONENT, &scalar);
    printf("light_replay_preserve %.1f 0x%04x\n", scalar, (unsigned)glGetError());
    glDeleteLists(9273, 1);
    glNewList(9274, GL_COMPILE_AND_EXECUTE);
    glBegin(GL_POINTS);
    glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 3.f);
    glEnd();
    glEndList();
    printf("light_compile_execute 0x%04x\n", (unsigned)glGetError());
    glDeleteLists(9274, 1);
    {
        GLfloat snapshot[4] = {0.1f, 0.2f, 0.3f, 1.f};
        glNewList(9276, GL_COMPILE);
        glLightfv(GL_LIGHT0, GL_AMBIENT, snapshot);
        glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 12.f);
        glEndList();
        printf("light_valid_compile 0x%04x\n", (unsigned)glGetError());
        snapshot[0] = 0.9f;
        glCallList(9276);
        glGetLightfv(GL_LIGHT0, GL_AMBIENT, read);
        printf("light_list_snapshot %.1f 0x%04x\n", read[0], (unsigned)glGetError());
        glGetLightfv(GL_LIGHT0, GL_SPOT_EXPONENT, &scalar);
        printf("light_list_apply %.1f 0x%04x\n", scalar, (unsigned)glGetError());
        glDeleteLists(9276, 1);
    }
    glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, -1.f);
    printf("light_invalid_value 0x%04x\n", (unsigned)glGetError());
    glLightfv(0xDEAD, GL_AMBIENT, ambient);
    printf("light_invalid_name 0x%04x\n", (unsigned)glGetError());
}
#endif

#ifndef MGL_POINT_PARAMETER_TRACE_H
#define MGL_POINT_PARAMETER_TRACE_H

#include "mgl/gl.h"
#include <stdio.h>

static void point_parameter_trace(void) {
    GLint value;
    GLfloat float_value;
    GLdouble double_value;
    const GLint upper = GL_UPPER_LEFT;

    glGetIntegerv(GL_POINT_SPRITE_COORD_ORIGIN, &value);
    printf("default %d %x\n", value, glGetError());

    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT);
    glGetIntegerv(GL_POINT_SPRITE_COORD_ORIGIN, &value);
    glGetFloatv(GL_POINT_SPRITE_COORD_ORIGIN, &float_value);
    glGetDoublev(GL_POINT_SPRITE_COORD_ORIGIN, &double_value);
    printf("lower %d %.0f %.0f %x\n", value, float_value, double_value, glGetError());

    glPointParameteriv(GL_POINT_SPRITE_COORD_ORIGIN, &upper);
    glGetIntegerv(GL_POINT_SPRITE_COORD_ORIGIN, &value);
    printf("upper-v %d %x\n", value, glGetError());

    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, -1);
    glGetIntegerv(GL_POINT_SPRITE_COORD_ORIGIN, &value);
    printf("bad-value %d %x\n", value, glGetError());

    glPointParameteriv(0xDEADu, &upper);
    glGetIntegerv(GL_POINT_SPRITE_COORD_ORIGIN, &value);
    printf("bad-name %d %x\n", value, glGetError());

    glPushAttrib(GL_POINT_BIT);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT);
    glPopAttrib();
    glGetIntegerv(GL_POINT_SPRITE_COORD_ORIGIN, &value);
    printf("stack %d %x\n", value, glGetError());

    glBegin(GL_POINTS);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT);
    glEnd();
    glGetIntegerv(GL_POINT_SPRITE_COORD_ORIGIN, &value);
    printf("begin-end %d %x\n", value, glGetError());

    printf("sprite-default %d %x\n", glIsEnabled(GL_POINT_SPRITE), glGetError());
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT);
    glEnable(GL_POINT_SPRITE);
    printf("sprite-enabled %d %x\n", glIsEnabled(GL_POINT_SPRITE), glGetError());
    glPushAttrib(GL_POINT_BIT);
    glDisable(GL_POINT_SPRITE);
    glPopAttrib();
    printf("sprite-point-stack %d %x\n", glIsEnabled(GL_POINT_SPRITE), glGetError());
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_POINT_SPRITE);
    glPopAttrib();
    printf("sprite-enable-stack %d %x\n", glIsEnabled(GL_POINT_SPRITE), glGetError());
}

#endif

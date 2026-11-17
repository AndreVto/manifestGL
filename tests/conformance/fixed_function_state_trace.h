#ifndef MGL_FIXED_FUNCTION_STATE_TRACE_H
#define MGL_FIXED_FUNCTION_STATE_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void fixed_function_state_trace(void) {
    GLint env = -1, shade = -1;
    GLfloat ambient[4] = {-9.f, -9.f, -9.f, -9.f};
    GLfloat diffuse[4] = {-9.f, -9.f, -9.f, -9.f};
    GLfloat emission[4] = {0.125f, 0.25f, 0.375f, 0.5f};
    GLint emission_i[4] = {0, 1073741824, 0, 0};
    GLfloat model[4] = {0.1f, 0.2f, 0.3f, 1.f};
    GLint model_i[4] = {0, 0, 0, 1};
    GLfloat material_shininess = -1.f;
    GLint color_mask = -1;
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &env);
    printf("ff_env_setup %d 0x%04x\n", env == (GLint)GL_REPLACE, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glTexEnvi(0xDEADu, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnd();
    glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &env);
    printf("ff_env_i_bad %d 0x%04x\n", env == (GLint)GL_REPLACE, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (GLfloat)GL_MODULATE);
    glEnd();
    printf("ff_env_f_begin 0x%04x\n", (unsigned)glGetError());
    {
        GLint v = GL_MODULATE;
        glBegin(GL_POINTS);
        glTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &v);
        glEnd();
    }
    printf("ff_env_iv_begin 0x%04x\n", (unsigned)glGetError());
    {
        GLfloat v = (GLfloat)GL_MODULATE;
        glBegin(GL_POINTS);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &v);
        glEnd();
    }
    printf("ff_env_fv_begin 0x%04x\n", (unsigned)glGetError());
    env = -1;
    glBegin(GL_POINTS);
    glGetTexEnviv(0xDEADu, GL_TEXTURE_ENV_MODE, &env);
    glEnd();
    printf("ff_env_get_i_begin %d 0x%04x\n", env, (unsigned)glGetError());
    {
        GLfloat v = -9.f;
        glBegin(GL_POINTS);
        glGetTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &v);
        glEnd();
        printf("ff_env_get_f_begin %.1f 0x%04x\n", v, (unsigned)glGetError());
    }

    glShadeModel(GL_SMOOTH);
    glBegin(GL_POINTS);
    glShadeModel(0xDEADu);
    glEnd();
    glGetIntegerv(GL_SHADE_MODEL, &shade);
    printf("ff_shade_begin %d 0x%04x\n", shade == (GLint)GL_SMOOTH, (unsigned)glGetError());
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glBegin(GL_POINTS);
    glColorMaterial(0xDEADu, GL_AMBIENT);
    glEnd();
    glGetIntegerv(GL_COLOR_MATERIAL_FACE, &color_mask);
    printf("ff_color_material_begin %d 0x%04x\n", color_mask == (GLint)GL_FRONT_AND_BACK,
           (unsigned)glGetError());

    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, model);
    glBegin(GL_POINTS);
    glLightModelfv(0xDEADu, model);
    glEnd();
    glGetFloatv(GL_LIGHT_MODEL_AMBIENT, ambient);
    printf("ff_light_model_fv %.2f 0x%04x\n", ambient[0], (unsigned)glGetError());
    glBegin(GL_POINTS);
    glLightModelf(GL_LIGHT_MODEL_LOCAL_VIEWER, 1.f);
    glEnd();
    printf("ff_light_model_f 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);
    glEnd();
    printf("ff_light_model_i 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glLightModeliv(GL_LIGHT_MODEL_AMBIENT, model_i);
    glEnd();
    printf("ff_light_model_iv 0x%04x\n", (unsigned)glGetError());

    glMaterialfv(GL_FRONT, GL_EMISSION, emission);
    glBegin(GL_POINTS);
    glMaterialfv(GL_FRONT, GL_SHININESS, emission);
    glEnd();
    glGetMaterialfv(GL_FRONT, GL_EMISSION, diffuse);
    printf("ff_material_fv %.3f 0x%04x\n", diffuse[0], (unsigned)glGetError());
    glBegin(GL_POINTS);
    glMaterialf(GL_FRONT, GL_SHININESS, -1.f);
    glEnd();
    printf("ff_material_f 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glMateriali(GL_FRONT, GL_SHININESS, -1);
    glEnd();
    printf("ff_material_i 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glMaterialiv(GL_FRONT, GL_EMISSION, emission_i);
    glEnd();
    printf("ff_material_iv 0x%04x\n", (unsigned)glGetError());
    diffuse[0] = -9.f;
    glBegin(GL_POINTS);
    glGetMaterialfv(GL_FRONT, GL_EMISSION, diffuse);
    glEnd();
    printf("ff_get_material_fv %.1f 0x%04x\n", diffuse[0], (unsigned)glGetError());
    {
        GLint v = -9;
        glBegin(GL_POINTS);
        glGetMaterialiv(GL_FRONT, GL_SHININESS, &v);
        glEnd();
        printf("ff_get_material_iv %d 0x%04x\n", v, (unsigned)glGetError());
    }

    glMaterialf(GL_FRONT, GL_SHININESS, 24.f);
    glGetMaterialfv(GL_FRONT, GL_SHININESS, &material_shininess);
    printf("ff_shininess_setup %.1f 0x%04x\n", material_shininess, (unsigned)glGetError());
    glNewList(9250u, GL_COMPILE);
    glBegin(GL_POINTS);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glShadeModel(GL_FLAT);
    glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, 1.f);
    glEnd();
    glEndList();
    printf("ff_list_compile 0x%04x\n", (unsigned)glGetError());
    glCallList(9250u);
    printf("ff_list_replay 0x%04x\n", (unsigned)glGetError());
    glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &env);
    glGetIntegerv(GL_SHADE_MODEL, &shade);
    glGetMaterialfv(GL_FRONT, GL_SHININESS, &material_shininess);
    printf("ff_list_preserve %d %d %.1f 0x%04x\n", env == (GLint)GL_REPLACE,
           shade == (GLint)GL_SMOOTH, material_shininess, (unsigned)glGetError());
    glDeleteLists(9250u, 1);

    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    {
        GLint mode = -1;
        glGetTexGeniv(GL_S, GL_TEXTURE_GEN_MODE, &mode);
        printf("ff_texgen_setup %d 0x%04x\n", mode == (GLint)GL_OBJECT_LINEAR,
               (unsigned)glGetError());
    }
    glBegin(GL_POINTS);
    glTexGenf(GL_S, GL_TEXTURE_GEN_MODE, (GLfloat)GL_EYE_LINEAR);
    glEnd();
    printf("ff_texgen_f 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glTexGeni(0xDEADu, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
    glEnd();
    printf("ff_texgen_i_invalid 0x%04x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glTexGend(GL_S, GL_TEXTURE_GEN_MODE, (GLdouble)GL_EYE_LINEAR);
    glEnd();
    printf("ff_texgen_d 0x%04x\n", (unsigned)glGetError());
    {
        GLfloat plane[4] = {1.f, 0.f, 0.f, 0.f};
        glBegin(GL_POINTS);
        glTexGenfv(GL_S, GL_OBJECT_PLANE, plane);
        glEnd();
        printf("ff_texgen_fv 0x%04x\n", (unsigned)glGetError());
    }
    {
        GLint plane[4] = {1, 0, 0, 0};
        glBegin(GL_POINTS);
        glTexGeniv(GL_S, GL_OBJECT_PLANE, plane);
        glEnd();
        printf("ff_texgen_iv 0x%04x\n", (unsigned)glGetError());
    }
    {
        GLdouble plane[4] = {1., 0., 0., 0.};
        glBegin(GL_POINTS);
        glTexGendv(GL_S, GL_OBJECT_PLANE, plane);
        glEnd();
        printf("ff_texgen_dv 0x%04x\n", (unsigned)glGetError());
    }
    {
        GLfloat v = -9.f;
        glBegin(GL_POINTS);
        glGetTexGenfv(GL_S, GL_TEXTURE_GEN_MODE, &v);
        glEnd();
        printf("ff_texgen_get_fv %.1f 0x%04x\n", v, (unsigned)glGetError());
    }
    {
        GLint v = -9;
        glBegin(GL_POINTS);
        glGetTexGeniv(GL_S, GL_TEXTURE_GEN_MODE, &v);
        glEnd();
        printf("ff_texgen_get_iv %d 0x%04x\n", v, (unsigned)glGetError());
    }
    {
        GLdouble v = -9.;
        glBegin(GL_POINTS);
        glGetTexGendv(GL_S, GL_TEXTURE_GEN_MODE, &v);
        glEnd();
        printf("ff_texgen_get_dv %.1f 0x%04x\n", v, (unsigned)glGetError());
    }
    {
        GLint mode = -1;
        glGetTexGeniv(GL_S, GL_TEXTURE_GEN_MODE, &mode);
        printf("ff_texgen_preserved %d 0x%04x\n", mode == (GLint)GL_OBJECT_LINEAR,
               (unsigned)glGetError());
    }

    {
        GLdouble eq[4] = {1., 0., 0., 0.};
        glClipPlane(GL_CLIP_PLANE0, eq);
        printf("ff_clip_setup 0x%04x\n", (unsigned)glGetError());
        glBegin(GL_POINTS);
        glClipPlane(0xDEADu, eq);
        glEnd();
        printf("ff_clip_begin 0x%04x\n", (unsigned)glGetError());
        eq[0] = -9.;
        glBegin(GL_POINTS);
        glGetClipPlane(GL_CLIP_PLANE0, eq);
        glEnd();
        printf("ff_clip_get %.1f 0x%04x\n", eq[0], (unsigned)glGetError());
        glGetClipPlane(GL_CLIP_PLANE0, eq);
        printf("ff_clip_preserve %.1f 0x%04x\n", eq[0], (unsigned)glGetError());
    }
}
#endif

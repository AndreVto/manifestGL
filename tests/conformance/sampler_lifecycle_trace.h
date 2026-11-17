#ifndef MGL_SAMPLER_LIFECYCLE_TRACE_H
#define MGL_SAMPLER_LIFECYCLE_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void sampler_trace_error(const char *name) {
    printf("sam_%s 0x%x\n", name, (unsigned)glGetError());
}

static void sampler_trace_binding(const char *name) {
    GLint v = -1;
    glGetIntegerv(GL_SAMPLER_BINDING, &v);
    printf("sam_%s %d 0x%x\n", name, v != 0, (unsigned)glGetError());
}

static void sampler_trace_float(const char *name, GLuint sampler, GLenum pname) {
    GLfloat v = -678.0f;
    glGetSamplerParameterfv(sampler, pname, &v);
    printf("sam_%s %.3f 0x%x\n", name, (double)v, (unsigned)glGetError());
}

static void sampler_lifecycle_trace(void) {
    GLuint samples[2] = {0, 0}, extra = 0, invalid = 0xabcdu;
    GLint v = -1729;
    GLfloat values[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    GLint ivec[4] = {5, 6, 7, 8};
    GLuint uvec[4] = {11u, 12u, 13u, 14u};
    glGenSamplers(2, samples);
    printf("sam_gen %d %d 0x%x\n",
           (int)(samples[0] != 0 && samples[1] != 0 && samples[0] != samples[1]),
           (int)glIsSampler(samples[0]), (unsigned)glGetError());
    glBindSampler(0, samples[0]);
    sampler_trace_binding("bound");
    glSamplerParameterf(samples[0], GL_TEXTURE_MIN_LOD, 3.25f);
    sampler_trace_float("set_float", samples[0], GL_TEXTURE_MIN_LOD);

    glSamplerParameterf(samples[0], GL_TEXTURE_MAX_LOD, 2.75f);
    glGetSamplerParameteriv(samples[0], GL_TEXTURE_MAX_LOD, &v);
    printf("sam_lod_round_positive %d 0x%x\n", v, (unsigned)glGetError());
    glSamplerParameterf(samples[0], GL_TEXTURE_MAX_LOD, -2.5f);
    glGetSamplerParameteriv(samples[0], GL_TEXTURE_MAX_LOD, &v);
    printf("sam_lod_round_negative %d 0x%x\n", v, (unsigned)glGetError());
    glSamplerParameterf(samples[0], GL_TEXTURE_MAX_LOD, 3.5f);
    glGetSamplerParameteriv(samples[0], GL_TEXTURE_MAX_LOD, &v);
    printf("sam_lod_round_halfway %d 0x%x\n", v, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glBindSampler(0, 0);
    glEnd();
    sampler_trace_binding("illegal_bind");
    glBegin(GL_POINTS);
    glBindSampler(0xffffffffu, 0);
    glEnd();
    sampler_trace_error("illegal_bind_precedence");
    glBegin(GL_POINTS);
    printf("sam_illegal_is %d ", (int)glIsSampler(samples[0]));
    glEnd();
    printf("0x%x\n", (unsigned)glGetError());
    glBegin(GL_POINTS);
    glGenSamplers(-1, &invalid);
    glEnd();
    sampler_trace_error("illegal_gen_precedence");
    glBegin(GL_POINTS);
    glGenSamplers(1, &invalid);
    glEnd();
    printf("sam_illegal_gen %d 0x%x\n", invalid == 0xabcdu, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glCreateSamplers(1, &invalid);
    glEnd();
    printf("sam_illegal_create %d 0x%x\n", invalid == 0xabcdu, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glDeleteSamplers(1, samples);
    glEnd();
    printf("sam_illegal_delete %d 0x%x\n", (int)glIsSampler(samples[0]), (unsigned)glGetError());
    glBegin(GL_POINTS);
    glDeleteSamplers(-1, NULL);
    glEnd();
    sampler_trace_error("illegal_delete_precedence");
    glBegin(GL_POINTS);
    glSamplerParameteri(samples[0], 0, 0);
    glEnd();
    sampler_trace_error("illegal_set_precedence");
    glBegin(GL_POINTS);
    glSamplerParameterf(samples[0], GL_TEXTURE_MIN_LOD, 8.0f);
    glEnd();
    sampler_trace_float("illegal_float", samples[0], GL_TEXTURE_MIN_LOD);
    glBegin(GL_POINTS);
    glSamplerParameteri(samples[0], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glEnd();
    glGetSamplerParameteriv(samples[0], GL_TEXTURE_MIN_FILTER, &v);
    printf("sam_illegal_integer %d 0x%x\n", v == (GLint)GL_NEAREST_MIPMAP_LINEAR,
           (unsigned)glGetError());
    glBegin(GL_POINTS);
    glSamplerParameterfv(samples[0], GL_TEXTURE_BORDER_COLOR, values);
    glEnd();
    sampler_trace_error("illegal_float_vector");
    glBegin(GL_POINTS);
    glSamplerParameteriv(samples[0], GL_TEXTURE_WRAP_S, ivec);
    glEnd();
    sampler_trace_error("illegal_int_vector");
    glBegin(GL_POINTS);
    glSamplerParameterIiv(samples[0], GL_TEXTURE_BORDER_COLOR, ivec);
    glEnd();
    sampler_trace_error("illegal_signed_border");
    glBegin(GL_POINTS);
    glSamplerParameterIuiv(samples[0], GL_TEXTURE_BORDER_COLOR, uvec);
    glEnd();
    sampler_trace_error("illegal_unsigned_border");
    glBegin(GL_POINTS);
    v = -1729;
    glGetSamplerParameteriv(samples[0], GL_TEXTURE_MIN_FILTER, &v);
    glEnd();
    printf("sam_illegal_get_int %d 0x%x\n", v == -1729, (unsigned)glGetError());
    glBegin(GL_POINTS);
    values[0] = -678.0f;
    glGetSamplerParameterfv(samples[0], GL_TEXTURE_MIN_LOD, values);
    glEnd();
    printf("sam_illegal_get_float %d 0x%x\n", values[0] == -678.0f, (unsigned)glGetError());
    glBegin(GL_POINTS);
    ivec[0] = -1729;
    glGetSamplerParameterIiv(samples[0], GL_TEXTURE_BORDER_COLOR, ivec);
    glEnd();
    printf("sam_illegal_get_signed %d 0x%x\n", ivec[0] == -1729, (unsigned)glGetError());
    glBegin(GL_POINTS);
    uvec[0] = 0xabcdefu;
    glGetSamplerParameterIuiv(samples[0], GL_TEXTURE_BORDER_COLOR, uvec);
    glEnd();
    printf("sam_illegal_get_unsigned %d 0x%x\n", uvec[0] == 0xabcdefu, (unsigned)glGetError());
    glBegin(GL_POINTS);
    v = -1729;
    glGetSamplerParameteriv(0, 0, &v);
    glEnd();
    printf("sam_illegal_get_precedence %d 0x%x\n", v == -1729, (unsigned)glGetError());
    glBegin(GL_POINTS);
    glBindSamplers(0xffffffffu, -1, NULL);
    glEnd();
    sampler_trace_error("illegal_multibind_precedence");
    glBegin(GL_POINTS);
    glBindSamplers(0, 1, samples);
    glEnd();
    sampler_trace_error("illegal_multibind");
    glBindSamplers(1, 1, &samples[1]);
    glActiveTexture(GL_TEXTURE1);
    sampler_trace_binding("multi_bound_unit1");
    glBindSamplers(1, 1, NULL);
    sampler_trace_binding("multi_unbound_unit1");
    glActiveTexture(GL_TEXTURE0);
    glBindSampler(0, samples[0]);
    sampler_trace_binding("retained_unit0");
    glDeleteSamplers(1, samples);
    sampler_trace_binding("deleted_binding");
    printf("sam_deleted_object %d 0x%x\n", (int)glIsSampler(samples[0]), (unsigned)glGetError());
    glDeleteSamplers(1, &samples[1]);
    glGenSamplers(1, &extra);
    printf("sam_recreate %d 0x%x\n", (int)glIsSampler(extra), (unsigned)glGetError());
    glDeleteSamplers(1, &extra);
    sampler_trace_error("teardown");
}
#endif

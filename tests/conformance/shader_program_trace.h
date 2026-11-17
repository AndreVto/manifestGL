#ifndef MGL_SHADER_PROGRAM_TRACE_H
#define MGL_SHADER_PROGRAM_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void shader_program_row(const char *name, int observed) {
    printf("sp_%s 0x%04x %d\n", name, (unsigned)glGetError(), observed);
}

static void shader_program_trace_trace(void) {
    GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint program = glCreateProgram();
    GLint attached = 91, precision = 91, range[2] = {91, 91};
    GLint value = 91;
    GLfloat float_value = 91.0f;
    GLuint uint_value = 91u;
    shader_program_row("objects", shader != 0u && program != 0u);
    glGetProgramiv(program, GL_ATTACHED_SHADERS, &attached);
    shader_program_row("initial_attached", attached == 0);

    glBegin(GL_POINTS);
    glAttachShader(program, shader);
    glEnd();
    shader_program_row("attach_begin", 0);
    glGetProgramiv(program, GL_ATTACHED_SHADERS, &attached);
    shader_program_row("attach_preserved", attached == 0);
    glBegin(GL_POINTS);
    glAttachShader(0u, 0u);
    glEnd();
    shader_program_row("attach_begin_bad_names", 0);
    glBegin(GL_POINTS);
    glCompileShader(shader);
    glEnd();
    shader_program_row("compile_begin", 0);
    glBegin(GL_POINTS);
    glBindAttribLocation(program, 0u, "position");
    glEnd();
    shader_program_row("bind_attrib_begin", 0);
    glBegin(GL_POINTS);
    glBindFragDataLocation(program, 0u, "pixel");
    glEnd();
    shader_program_row("bind_fragment_begin", 0);
    glBegin(GL_POINTS);
    glBindFragDataLocationIndexed(program, 0u, 0u, "pixel");
    glEnd();
    shader_program_row("bind_fragment_indexed_begin", 0);
    glBegin(GL_POINTS);
    glLinkProgram(program);
    glEnd();
    shader_program_row("link_begin", 0);
    glBegin(GL_POINTS);
    glValidateProgram(program);
    glEnd();
    shader_program_row("validate_begin", 0);
    glBegin(GL_POINTS);
    glUseProgram(program);
    glEnd();
    shader_program_row("use_begin", 0);
    value = 91;
    glBegin(GL_POINTS);
    glGetProgramiv(program, GL_ATTACHED_SHADERS, &value);
    glEnd();
    shader_program_row("get_program_begin", value == 91);
    value = 91;
    glBegin(GL_POINTS);
    glGetProgramiv(program, 0u, &value);
    glEnd();
    shader_program_row("get_program_begin_bad_pname", value == 91);
    glBegin(GL_POINTS);
    (void)glGetAttribLocation(program, "position");
    glEnd();
    shader_program_row("get_attrib_begin", 0);
    glBegin(GL_POINTS);
    (void)glGetUniformLocation(program, "color");
    glEnd();
    shader_program_row("get_uniform_location_begin", 0);
    glBegin(GL_POINTS);
    (void)glGetFragDataLocation(program, "pixel");
    glEnd();
    shader_program_row("get_fragment_location_begin", 0);
    glBegin(GL_POINTS);
    (void)glGetFragDataIndex(program, "pixel");
    glEnd();
    shader_program_row("get_fragment_index_begin", 0);
    glBegin(GL_POINTS);
    glProgramParameteri(program, GL_PROGRAM_SEPARABLE, GL_TRUE);
    glEnd();
    shader_program_row("program_parameter_begin", 0);
    glBegin(GL_POINTS);
    glProgramParameteri(program, 0u, 7);
    glEnd();
    shader_program_row("program_parameter_begin_bad_enum", 0);
    glBegin(GL_POINTS);
    glGetProgramBinary(program, -1, NULL, NULL, NULL);
    glEnd();
    shader_program_row("get_binary_begin", 0);
    glBegin(GL_POINTS);
    glProgramBinary(program, 0u, NULL, -1);
    glEnd();
    shader_program_row("binary_begin", 0);
    glBegin(GL_POINTS);
    glShaderBinary(-1, NULL, 0u, NULL, -1);
    glEnd();
    shader_program_row("shader_binary_begin", 0);
    glBegin(GL_POINTS);
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, 0u, range, &precision);
    glEnd();
    shader_program_row("precision_begin", range[0] == 91 && range[1] == 91 && precision == 91);
    float_value = 91.0f;
    glBegin(GL_POINTS);
    glGetUniformfv(program, 0, &float_value);
    glEnd();
    shader_program_row("get_uniformfv_begin", float_value == 91.0f);
    value = 91;
    glBegin(GL_POINTS);
    glGetUniformiv(program, 0, &value);
    glEnd();
    shader_program_row("get_uniformiv_begin", value == 91);
    uint_value = 91u;
    glBegin(GL_POINTS);
    glGetUniformuiv(program, 0, &uint_value);
    glEnd();
    shader_program_row("get_uniformuiv_begin", uint_value == 91u);

    glAttachShader(program, shader);
    shader_program_row("attach_legal", 0);
    glGetProgramiv(program, GL_ATTACHED_SHADERS, &attached);
    shader_program_row("attached_after_legal", attached == 1);
    glDetachShader(program, shader);
    shader_program_row("detach_legal", 0);
    glGetProgramiv(program, GL_ATTACHED_SHADERS, &attached);
    shader_program_row("detached_after_legal", attached == 0);
    glDeleteShader(shader);
    glDeleteProgram(program);
    shader_program_row("cleanup", 0);
}
#endif

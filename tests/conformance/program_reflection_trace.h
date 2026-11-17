#ifndef MGL_PROGRAM_REFLECTION_TRACE_H
#define MGL_PROGRAM_REFLECTION_TRACE_H

#include "mgl/gl.h"
#include <stdio.h>
#include <string.h>

/* Both runners use an independently created linked program and the same
 * observable GL calls. Values after a GL error are checked only where
 * the API promises that the output is unchanged. */
static GLuint matrix_trace_prepare(void);
static void matrix_trace_finish(GLuint program);
static unsigned reflection_failures;

static void reflection_row(const char *label, GLenum expected, int preserved) {
    GLenum error = glGetError();
    if (error != expected || !preserved) {
        fprintf(stderr, "reflection reference mismatch %s: error=0x%x expected=0x%x unchanged=%d\n",
                label, (unsigned)error, (unsigned)expected, preserved);
        ++reflection_failures;
    }
    printf("ref_%s 0x%04x %d\n", label, (unsigned)error, preserved);
}

#define REFLECT_IN_BEGIN(label, command, valid)                                                    \
    do {                                                                                           \
        glBegin(GL_POINTS);                                                                        \
        command;                                                                                   \
        glEnd();                                                                                   \
        reflection_row(label, GL_INVALID_OPERATION, (valid));                                      \
    } while (0)

static int reflection_trace(void) {
    GLuint program = matrix_trace_prepare();
    GLint size = -443, length = -443, values[2] = {-443, -443};
    GLenum type = 0xdead;
    GLuint indices[2] = {0xdead, 0xdead};
    GLchar name[64] = "unchanged";
    const GLchar *names[] = {"u23"};
    const GLenum props[] = {GL_TYPE};
    reflection_row("setup", GL_NO_ERROR, program != 0u);
    if (program == 0u)
        return 0;
    REFLECT_IN_BEGIN(
        "active_attrib", glGetActiveAttrib(program, 0, 64, &length, &size, &type, name),
        length == -443 && size == -443 && type == 0xdead && !strcmp(name, "unchanged"));
    REFLECT_IN_BEGIN(
        "active_uniform", glGetActiveUniform(program, 0, 64, &length, &size, &type, name),
        length == -443 && size == -443 && type == 0xdead && !strcmp(name, "unchanged"));
    REFLECT_IN_BEGIN("uniform_indices", glGetUniformIndices(program, 1, names, indices),
                     indices[0] == 0xdead);
    REFLECT_IN_BEGIN("active_uniformsiv",
                     glGetActiveUniformsiv(program, 1, indices, GL_TYPE, values),
                     values[0] == -443);
    REFLECT_IN_BEGIN("active_uniform_name", glGetActiveUniformName(program, 0, 64, &length, name),
                     length == -443 && !strcmp(name, "unchanged"));
    REFLECT_IN_BEGIN("block_index", (void)glGetUniformBlockIndex(program, "NoBlock"), 1);
    REFLECT_IN_BEGIN("block_parameters",
                     glGetActiveUniformBlockiv(program, 0, GL_UNIFORM_BLOCK_BINDING, &size),
                     size == -443);
    REFLECT_IN_BEGIN("block_name", glGetActiveUniformBlockName(program, 0, 64, &length, name),
                     length == -443 && !strcmp(name, "unchanged"));
    REFLECT_IN_BEGIN("block_binding", glUniformBlockBinding(program, 0, 0), 1);
    REFLECT_IN_BEGIN("storage_binding", glShaderStorageBlockBinding(program, 0, 0), 1);
    REFLECT_IN_BEGIN("program_interface",
                     glGetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &size),
                     size == -443);
    REFLECT_IN_BEGIN("resource_index", (void)glGetProgramResourceIndex(program, GL_UNIFORM, "u23"),
                     1);
    REFLECT_IN_BEGIN("resource_name",
                     glGetProgramResourceName(program, GL_UNIFORM, 0, 64, &length, name),
                     length == -443 && !strcmp(name, "unchanged"));
    REFLECT_IN_BEGIN("resource_properties",
                     glGetProgramResourceiv(program, GL_UNIFORM, 0, 1, props, 2, &length, values),
                     length == -443 && values[0] == -443 && values[1] == -443);
    REFLECT_IN_BEGIN("resource_location",
                     (void)glGetProgramResourceLocation(program, GL_UNIFORM, "u23"), 1);
    REFLECT_IN_BEGIN(
        "resource_location_index",
        (void)glGetProgramResourceLocationIndex(program, GL_PROGRAM_OUTPUT, "outColor"), 1);
    /* Bad arguments must not override Begin/End's GL_INVALID_OPERATION. */
    REFLECT_IN_BEGIN("bad_buffer_precedence",
                     glGetActiveUniform(program, 0, -1, &length, &size, &type, name),
                     length == -443);
    REFLECT_IN_BEGIN("bad_count_precedence", glGetUniformIndices(program, -1, NULL, NULL), 1);
    REFLECT_IN_BEGIN("bad_resource_precedence",
                     glGetProgramResourceiv(program, GL_UNIFORM, 0, -1, NULL, -1, &length, values),
                     length == -443 && values[0] == -443);

    glGetActiveUniform(program, 0, 64, &length, &size, &type, name);
    reflection_row("valid_active_uniform", GL_NO_ERROR, length > 0 && size > 0 && name[0] != '\0');
    {
        GLint alias_length = -443;
        GLchar alias[64] = {0};
        glGetActiveUniformName(program, 0, 64, &alias_length, alias);
        reflection_row("valid_uniform_name", GL_NO_ERROR,
                       alias_length == length && strcmp(alias, name) == 0);
    }
    matrix_trace_finish(program);
    reflection_row("cleanup", GL_NO_ERROR, 1);
    return reflection_failures == 0u;
}

#undef REFLECT_IN_BEGIN
#endif

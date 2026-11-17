#ifndef MGL_SHADER_OBJECT_ALIAS_TRACE_H
#define MGL_SHADER_OBJECT_ALIAS_TRACE_H

#include "mgl/gl.h"
#include <stdio.h>
#include <string.h>

static void shader_object_alias_trace(void) {
    static const GLcharARB *vs =
        "#version 120\nattribute vec4 a; uniform float scale; void main(){gl_Position=a*scale;}";
    static const GLcharARB *fs = "#version 120\nvoid main(){gl_FragColor=vec4(1.0);}";
    GLhandleARB vertex, fragment, program;
    GLint compiled = 0, linked = 0, location;
    GLfloat actual = -1.0f;
    GLsizei source_len = -1;
    GLcharARB source[128];
    GLcharARB uniform_name[32] = {0};
    GLint uniform_size = 0;
    GLenum uniform_type = 0;
    GLsizei uniform_len = 0;

    vertex = glCreateShaderObjectARB(GL_VERTEX_SHADER);
    fragment = glCreateShaderObjectARB(GL_FRAGMENT_SHADER);
    program = glCreateProgramObjectARB();
    printf("create %d %04x\n", (vertex != 0 && fragment != 0 && program != 0), glGetError());
    glShaderSourceARB(vertex, 1, &vs, NULL);
    glCompileShaderARB(vertex);
    glGetObjectParameterivARB(vertex, GL_OBJECT_COMPILE_STATUS_ARB, &compiled);
    printf("vertex_compile %d %04x\n", compiled, glGetError());
    glShaderSourceARB(fragment, 1, &fs, NULL);
    glCompileShaderARB(fragment);
    glGetObjectParameterivARB(fragment, GL_OBJECT_COMPILE_STATUS_ARB, &compiled);
    printf("fragment_compile %d %04x\n", compiled, glGetError());
    glGetShaderSourceARB(vertex, (GLsizei)sizeof(source), &source_len, source);
    printf("source %d %04x\n", source_len == (GLsizei)strlen(vs), glGetError());
    glBindAttribLocationARB(program, 2, "a");
    glAttachObjectARB(program, vertex);
    glAttachObjectARB(program, fragment);
    glLinkProgramARB(program);
    glGetObjectParameterivARB(program, GL_OBJECT_LINK_STATUS_ARB, &linked);
    printf("link %d %04x\n", linked, glGetError());
    printf("attrib %d %04x\n", glGetAttribLocationARB(program, "a") == 2, glGetError());
    glGetActiveUniformARB(program, 0, (GLsizei)sizeof(uniform_name), &uniform_len, &uniform_size,
                          &uniform_type, uniform_name);
    printf("active_uniform %d %d %d %04x\n", uniform_len == 5 && uniform_name[0] == 's',
           uniform_size, uniform_type == GL_FLOAT, glGetError());
    location = glGetUniformLocationARB(program, "scale");
    printf("location %d %04x\n", location >= 0, glGetError());
    glUseProgramObjectARB(program);
    glUniform1fARB(location, 0.75f);
    glGetUniformfvARB(program, location, &actual);
    printf("uniform %.2f %04x\n", (double)actual, glGetError());
    glUniform1uiEXT(location, 3u);
    printf("uint_wrong_type %04x\n", glGetError());
    glUniform1fvARB(location, 1, &actual);
    printf("uniform_vec %04x\n", glGetError());
    glUniform2fvARB(location, -1, &actual);
    printf("uniform_bad_count %04x\n", glGetError());
    glUniform1fARB(-1, 9.0f);
    printf("uniform_inactive %04x\n", glGetError());
    glVertexAttribDivisorARB(0, 3u);
    printf("attrib_divisor %04x\n", glGetError());
    {
        GLenum buffer = GL_BACK;
        glDrawBuffersARB(1, &buffer);
        printf("draw_buffer %04x\n", glGetError());
    }
    glUseProgramObjectARB(0);
    glUniform1fARB(location, 1.0f);
    printf("uniform_no_program %04x\n", glGetError());
    glGetUniformfvARB(program, location, &actual);
    printf("uniform_retained %.2f %04x\n", (double)actual, glGetError());
    glDeleteObjectARB(program);
    glDeleteObjectARB(vertex);
    glDeleteObjectARB(fragment);
    printf("cleanup %04x\n", glGetError());
}

#endif

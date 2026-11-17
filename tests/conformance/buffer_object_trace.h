#ifndef MGL_BUFFER_OBJECT_TRACE_H
#define MGL_BUFFER_OBJECT_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void bot_error(const char *step) {
    printf("bot_%s 0x%x\n", step, (unsigned)glGetError());
}

static void bot_binding(const char *step) {
    GLint first = -1, second = -1;
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &first);
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 1, &second);
    printf("bot_%s %d %d 0x%x\n", step, first != 0, second != 0, (unsigned)glGetError());
}

static void bot_size(const char *step, GLuint name) {
    GLint size = -1;
    glGetNamedBufferParameteriv(name, GL_BUFFER_SIZE, &size);
    printf("bot_%s %d 0x%x\n", step, size, (unsigned)glGetError());
}

static void buffer_object_trace(void) {
    GLuint names[2] = {0, 0};
    GLuint selected[2];
    GLintptr offsets[2] = {0, 0};
    GLsizeiptr sizes[2] = {64, 64};
    GLubyte contents[128] = {0};
    GLubyte query[2] = {0xff, 0xff};
    glGenBuffers(2, names);
    glBindBuffer(GL_UNIFORM_BUFFER, names[0]);
    glBufferData(GL_UNIFORM_BUFFER, 128, contents, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, names[1]);
    glBufferData(GL_UNIFORM_BUFFER, 128, contents, GL_STATIC_DRAW);
    bot_error("setup");
    selected[0] = names[0];
    selected[1] = names[1];
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, names[0]);
    bot_binding("base_one");
    glBindBufferRange(GL_UNIFORM_BUFFER, 1, names[1], 0, 64);
    bot_binding("range_one");
    glBindBuffersBase(GL_UNIFORM_BUFFER, 0, 2, selected);
    bot_binding("base_many");
    glBindBuffersRange(GL_UNIFORM_BUFFER, 0, 2, selected, offsets, sizes);
    bot_binding("range_many");
    glBegin(GL_POINTS);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
    glEnd();
    bot_binding("illegal_base");
    glBegin(GL_POINTS);
    glBindBufferRange(GL_UNIFORM_BUFFER, 1, 0, -1, -1);
    glEnd();
    bot_binding("illegal_range");
    glBegin(GL_POINTS);
    glBindBuffersBase(GL_UNIFORM_BUFFER, 0, 2, NULL);
    glEnd();
    bot_binding("illegal_multi_base");
    glBegin(GL_POINTS);
    glBindBuffersRange(GL_UNIFORM_BUFFER, 0, 2, NULL, NULL, NULL);
    glEnd();
    bot_binding("illegal_multi_range");
    glBegin(GL_POINTS);
    glBindBufferBase(0, 0xffffffffu, 0);
    glEnd();
    bot_error("illegal_base_precedence");
    glBegin(GL_POINTS);
    glBindBuffersRange(0, 0, -1, NULL, NULL, NULL);
    glEnd();
    bot_error("illegal_multi_precedence");
    bot_size("named_initial", names[0]);
    glNamedBufferData(names[0], 128, contents, GL_DYNAMIC_DRAW);
    bot_size("named_data", names[0]);
    glNamedBufferSubData(names[0], 3, 2, (const GLubyte[]){17, 29});
    glGetNamedBufferSubData(names[0], 3, 2, query);
    printf("bot_named_rw %u %u 0x%x\n", (unsigned)query[0], (unsigned)query[1],
           (unsigned)glGetError());
    glBegin(GL_POINTS);
    glNamedBufferData(names[0], 32, NULL, GL_STATIC_DRAW);
    glEnd();
    bot_size("illegal_named_data", names[0]);
    glBegin(GL_POINTS);
    glNamedBufferSubData(names[0], 3, 2, (const GLubyte[]){44, 55});
    glEnd();
    bot_error("illegal_named_subdata");
    query[0] = 0xee;
    query[1] = 0xee;
    glBegin(GL_POINTS);
    glGetNamedBufferSubData(names[0], 3, 2, query);
    glEnd();
    printf("bot_illegal_named_read %d 0x%x\n", query[0] == 0xee && query[1] == 0xee,
           (unsigned)glGetError());
    glBegin(GL_POINTS);
    glGetNamedBufferParameteriv(names[0], GL_BUFFER_SIZE, (GLint *)contents);
    glEnd();
    bot_error("illegal_named_query");
    glGetNamedBufferSubData(names[0], 3, 2, query);
    printf("bot_retained_named_rw %u %u 0x%x\n", (unsigned)query[0], (unsigned)query[1],
           (unsigned)glGetError());
    {
        GLint64 queried_size = -91;
        void *pointer = NULL;
        void *unchanged = contents;
        GLboolean result;
        glBegin(GL_POINTS);
        glGetNamedBufferParameteri64v(names[0], GL_BUFFER_SIZE, &queried_size);
        glEnd();
        printf("bot_illegal_named_query64 %d 0x%x\n", queried_size == -91, (unsigned)glGetError());
        pointer = glMapNamedBufferRange(names[0], 0, 8, GL_MAP_READ_BIT);
        printf("bot_map_range %d 0x%x\n", pointer != NULL, (unsigned)glGetError());
        glBegin(GL_POINTS);
        result = glUnmapNamedBuffer(names[0]);
        glEnd();
        printf("bot_illegal_unmap %d 0x%x\n", (int)result, (unsigned)glGetError());
        {
            GLint mapped = -1;
            glGetNamedBufferParameteriv(names[0], GL_BUFFER_MAPPED, &mapped);
            printf("bot_map_retained %d 0x%x\n", mapped, (unsigned)glGetError());
        }
        glBegin(GL_POINTS);
        pointer = glMapNamedBufferRange(names[1], 0, 8, GL_MAP_READ_BIT);
        glEnd();
        printf("bot_illegal_map_range %d 0x%x\n", pointer == NULL, (unsigned)glGetError());
        glBegin(GL_POINTS);
        pointer = glMapNamedBuffer(names[1], GL_READ_ONLY);
        glEnd();
        printf("bot_illegal_map %d 0x%x\n", pointer == NULL, (unsigned)glGetError());
        glBegin(GL_POINTS);
        glFlushMappedNamedBufferRange(names[0], 0, 1);
        glEnd();
        bot_error("illegal_flush");
        glBegin(GL_POINTS);
        pointer = unchanged;
        glGetNamedBufferPointerv(names[0], GL_BUFFER_MAP_POINTER, &pointer);
        glEnd();
        printf("bot_illegal_pointer %d 0x%x\n", pointer == unchanged, (unsigned)glGetError());
        result = glUnmapNamedBuffer(names[0]);
        printf("bot_unmap %d 0x%x\n", (int)result, (unsigned)glGetError());
        pointer = glMapNamedBuffer(names[0], GL_READ_ONLY);
        printf("bot_map_all %d 0x%x\n", pointer != NULL, (unsigned)glGetError());
        result = glUnmapNamedBuffer(names[0]);
        printf("bot_unmap_all %d 0x%x\n", (int)result, (unsigned)glGetError());
        glBegin(GL_POINTS);
        glCopyNamedBufferSubData(names[0], names[1], 3, 3, 2);
        glEnd();
        bot_error("illegal_copy");
        query[0] = 0xee;
        query[1] = 0xee;
        glGetNamedBufferSubData(names[1], 3, 2, query);
        printf("bot_copy_retained %u %u 0x%x\n", (unsigned)query[0], (unsigned)query[1],
               (unsigned)glGetError());
        glCopyNamedBufferSubData(names[0], names[1], 3, 3, 2);
        glGetNamedBufferSubData(names[1], 3, 2, query);
        printf("bot_copy_valid %u %u 0x%x\n", (unsigned)query[0], (unsigned)query[1],
               (unsigned)glGetError());
    }
    glBindBuffersBase(GL_UNIFORM_BUFFER, 0, 2, NULL);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glDeleteBuffers(2, names);
    bot_error("teardown");
}
#endif

#ifndef MGL_BUFFER_STORAGE_CASES_H
#define MGL_BUFFER_STORAGE_CASES_H
#include "mgl/gl.h"
#include <stdio.h>

static void buffer_storage_trace(void) {
    GLuint a = 0, b = 0, c = 0;
    GLint x = -1;
    GLint64 x64 = -1;
    unsigned char init[8] = {1, 2, 3, 4, 5, 6, 7, 8}, update[2] = {42, 43};
    unsigned char result[8] = {0};
    unsigned char *ptr;
    glGenBuffers(1, &a);
    glBindBuffer(GL_ARRAY_BUFFER, a);
    glBufferData(GL_ARRAY_BUFFER, 8, init, GL_DYNAMIC_DRAW);
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_IMMUTABLE_STORAGE, &x);
    printf("mutable %d %04x\n", x, glGetError());
    ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, 4, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    printf("mutable_persist_map %d %04x\n", (int)(ptr != NULL), glGetError());
    glBufferStorage(GL_ARRAY_BUFFER, 0, NULL, 0);
    printf("zero %04x\n", glGetError());
    glBufferStorage(GL_ARRAY_BUFFER, 8, init, 0x80000000u);
    printf("unknown %04x\n", glGetError());
    glBufferStorage(GL_ARRAY_BUFFER, 8, init, GL_MAP_COHERENT_BIT);
    printf("coherent_only %04x\n", glGetError());
    glBufferStorage(GL_ARRAY_BUFFER, 8, init, GL_MAP_PERSISTENT_BIT);
    printf("persistent_only %04x\n", glGetError());
    glBufferStorage(GL_ARRAY_BUFFER, 8, init,
                    GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
    printf("immutable %04x\n", glGetError());
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_IMMUTABLE_STORAGE, &x);
    glGetBufferParameteri64v(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &x64);
    printf("state %d %lld %04x\n", x, (long long)x64, glGetError());
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_STORAGE_FLAGS, &x);
    printf("flags %04x %04x\n", x, glGetError());
    glBufferData(GL_ARRAY_BUFFER, 4, update, GL_STATIC_DRAW);
    printf("redata %04x\n", glGetError());
    glBufferStorage(GL_ARRAY_BUFFER, 8, init, 0);
    printf("restore %04x\n", glGetError());
    glBufferSubData(GL_ARRAY_BUFFER, 2, 2, update);
    printf("subdata %04x\n", glGetError());
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, 8, result);
    printf("contents %u %u %u %u %04x\n", result[0], result[2], result[3], result[7], glGetError());
    ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, 8, GL_MAP_READ_BIT);
    printf("read_map %d %u %04x\n", (int)(ptr != NULL), ptr ? ptr[2] : 0, glGetError());
    printf("unmap %d %04x\n", (int)glUnmapBuffer(GL_ARRAY_BUFFER), glGetError());
    ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, 8, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT);
    printf("forbidden_map %d %04x\n", (int)(ptr != NULL), glGetError());
    glCreateBuffers(1, &b);
    glNamedBufferStorage(b, 8, init, GL_MAP_READ_BIT);
    printf("named_storage %04x\n", glGetError());
    glGetNamedBufferParameteriv(b, GL_BUFFER_STORAGE_FLAGS, &x);
    printf("named_flags %04x %04x\n", x, glGetError());
    glNamedBufferSubData(b, 0, 2, update);
    printf("named_subdata_denied %04x\n", glGetError());
    glNamedBufferData(b, 8, init, GL_STATIC_DRAW);
    printf("named_redata_denied %04x\n", glGetError());
    glGetNamedBufferParameteri64v(b, GL_BUFFER_SIZE, &x64);
    printf("named_size %lld %04x\n", (long long)x64, glGetError());
    glBindBuffer(GL_COPY_READ_BUFFER, b);
    ptr = glMapBufferRange(GL_COPY_READ_BUFFER, 0, 8, GL_MAP_WRITE_BIT);
    printf("denied_write_map %d %04x\n", (int)(ptr != NULL), glGetError());
    glGenBuffers(1, &c);
    glBindBuffer(GL_COPY_WRITE_BUFFER, c);
    glBufferStorage(GL_COPY_WRITE_BUFFER, 8, init,
                    GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT |
                        GL_MAP_COHERENT_BIT);
    printf("persist_create %04x\n", glGetError());
    ptr = glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, 8,
                           GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT |
                               GL_MAP_COHERENT_BIT);
    printf("persist_map %d %u %04x\n", (int)(ptr != NULL), ptr ? ptr[4] : 0, glGetError());
    if (ptr)
        ptr[4] = 22;
    glGetBufferSubData(GL_COPY_WRITE_BUFFER, 4, 1, result);
    printf("persist_mapped_read %u %04x\n", result[0], glGetError());
    printf("persist_unmap %d %04x\n", (int)glUnmapBuffer(GL_COPY_WRITE_BUFFER), glGetError());
    glGetBufferSubData(GL_COPY_WRITE_BUFFER, 4, 1, result);
    printf("persist_contents %u %04x\n", result[0], glGetError());
    glDeleteBuffers(1, &a);
    glDeleteBuffers(1, &b);
    glDeleteBuffers(1, &c);
}
#endif

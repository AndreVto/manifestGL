#ifndef MGL_BUFFER_CLEAR_TRANSFER_CASES_H
#define MGL_BUFFER_CLEAR_TRANSFER_CASES_H
#include "mgl/gl.h"
#include <stdio.h>
#include <string.h>

static void buffer_clear_transfer_trace(void) {
    GLuint buffer = 0u;
    unsigned char init[32], readback[32];
    const unsigned char rgba[4] = {9, 21, 37, 53};
    GLuint uint_value = 0x12345678u;
    GLint int_value = -123456;
    unsigned i;
    memset(init, 0x5a, sizeof(init));
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, buffer);
    glBufferData(GL_COPY_WRITE_BUFFER, 32, init, GL_DYNAMIC_DRAW);
    printf("init %04x\n", (unsigned)glGetError());
    glClearBufferSubData(GL_COPY_WRITE_BUFFER, GL_RGBA8, 4, 8, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glGetBufferSubData(GL_COPY_WRITE_BUFFER, 0, 32, readback);
    printf("rgba %02x %02x %02x %02x %02x %02x %02x %02x %04x\n", readback[0], readback[4],
           readback[5], readback[6], readback[7], readback[8], readback[11], readback[12],
           (unsigned)glGetError());
    {
        const GLfloat linear[4] = {0.25f, 0.5f, 1.0f, 1.5f};
        glClearBufferSubData(GL_COPY_WRITE_BUFFER, GL_RGBA8, 0, 4, GL_RGBA, GL_FLOAT, linear);
        glGetBufferSubData(GL_COPY_WRITE_BUFFER, 0, 4, readback);
        printf("normalized %u %u %u %u %04x\n", readback[0], readback[1], readback[2], readback[3],
               (unsigned)glGetError());
    }
    glClearBufferData(GL_COPY_WRITE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &uint_value);
    glGetBufferSubData(GL_COPY_WRITE_BUFFER, 0, 32, readback);
    printf("uint %d %d %04x\n", memcmp(readback, &uint_value, 4) == 0,
           memcmp(readback + 28, &uint_value, 4) == 0, (unsigned)glGetError());
    glClearBufferSubData(GL_COPY_WRITE_BUFFER, GL_R32I, 8, 8, GL_RED_INTEGER, GL_INT, &int_value);
    glGetBufferSubData(GL_COPY_WRITE_BUFFER, 8, 8, readback);
    printf("int %d %d %04x\n", memcmp(readback, &int_value, 4) == 0,
           memcmp(readback + 4, &int_value, 4) == 0, (unsigned)glGetError());
    glClearBufferSubData(GL_COPY_WRITE_BUFFER, GL_RGBA8, 0, 32, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGetBufferSubData(GL_COPY_WRITE_BUFFER, 0, 32, readback);
    for (i = 0; i < 32; ++i)
        if (readback[i])
            break;
    printf("null %d %04x\n", (int)(i == 32), (unsigned)glGetError());
    glClearBufferSubData(GL_COPY_WRITE_BUFFER, GL_R32UI, 2, 8, GL_RED_INTEGER, GL_UNSIGNED_INT,
                         &uint_value);
    printf("unaligned %04x\n", (unsigned)glGetError());
    glClearBufferSubData(GL_COPY_WRITE_BUFFER, GL_R32UI, 28, 8, GL_RED_INTEGER, GL_UNSIGNED_INT,
                         &uint_value);
    printf("outofrange %04x\n", (unsigned)glGetError());
    glClearBufferSubData(GL_COPY_WRITE_BUFFER, GL_R32UI, 0, -4, GL_RED_INTEGER, GL_UNSIGNED_INT,
                         &uint_value);
    printf("negative %04x\n", (unsigned)glGetError());
    glClearBufferData(GL_COPY_WRITE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_INT, &int_value);
    printf("mismatch %04x\n", (unsigned)glGetError());
    glClearBufferData(GL_COPY_WRITE_BUFFER, 0xdead, GL_RED_INTEGER, GL_UNSIGNED_INT, &uint_value);
    printf("badformat %04x\n", (unsigned)glGetError());
    {
        GLuint immutable = 0u;
        glGenBuffers(1, &immutable);
        glBindBuffer(GL_COPY_READ_BUFFER, immutable);
        glBufferStorage(GL_COPY_READ_BUFFER, 16, NULL, GL_MAP_READ_BIT);
        glClearBufferData(GL_COPY_READ_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT,
                          &uint_value);
        printf("immutable_nodynamic %04x\n", (unsigned)glGetError());
        glDeleteBuffers(1, &immutable);
    }
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    glClearBufferData(GL_COPY_WRITE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &uint_value);
    printf("unbound %04x\n", (unsigned)glGetError());
    glBindBuffer(GL_COPY_WRITE_BUFFER, buffer);
    glDeleteBuffers(1, &buffer);
}
#endif

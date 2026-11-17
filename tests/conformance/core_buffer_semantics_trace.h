#ifndef MGL_CORE_BUFFER_SEMANTICS_TRACE_H
#define MGL_CORE_BUFFER_SEMANTICS_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>
#include <string.h>

static void cbs_error(const char *name) {
    printf("cbs_%s 0x%x\n", name, (unsigned)glGetError());
}

static void cbs_size(const char *name) {
    GLint size = -33;
    glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &size);
    printf("cbs_%s %d 0x%x\n", name, size, (unsigned)glGetError());
}

static void cbs_content(const char *name) {
    unsigned char result[8] = {0};
    glGetBufferSubData(GL_COPY_READ_BUFFER, 0, 8, result);
    printf("cbs_%s %u %u %u %u 0x%x\n", name, (unsigned)result[0], (unsigned)result[1],
           (unsigned)result[4], (unsigned)result[7], (unsigned)glGetError());
}

static void core_buffer_semantics_trace(void) {
    const unsigned char init[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    const unsigned char replace[2] = {31, 32};
    unsigned char retained[2] = {91, 92};
    GLuint names[2] = {0, 0};
    GLint flag = -1;
    void *ptr;
    GLboolean ok;
    glGenBuffers(2, names);
    glBindBuffer(GL_COPY_READ_BUFFER, names[0]);
    glBufferData(GL_COPY_READ_BUFFER, 8, init, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_COPY_WRITE_BUFFER, names[1]);
    glBufferData(GL_COPY_WRITE_BUFFER, 8, init, GL_DYNAMIC_DRAW);
    cbs_error("setup");
    cbs_content("initial");
    glBufferSubData(GL_COPY_READ_BUFFER, 4, 2, replace);
    cbs_content("subdata");
    glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_USAGE, &flag);
    printf("cbs_usage %d 0x%x\n", flag == (GLint)GL_DYNAMIC_DRAW, (unsigned)glGetError());
    glBufferSubData(GL_COPY_READ_BUFFER, -1, 1, replace);
    cbs_error("negative_offset");
    glBufferSubData(GL_COPY_READ_BUFFER, 7, 2, replace);
    cbs_error("overrun");
    glBufferData(GL_COPY_READ_BUFFER, -1, init, GL_DYNAMIC_DRAW);
    cbs_error("negative_size");
    glBufferData(GL_COPY_READ_BUFFER, 8, init, (GLenum)0xdead);
    cbs_error("invalid_usage");
    cbs_content("retained");
    glBegin(GL_POINTS);
    glBufferData(GL_COPY_READ_BUFFER, -1, init, (GLenum)0xdead);
    glEnd();
    cbs_error("illegal_data_precedence");
    glBegin(GL_POINTS);
    glBufferSubData(GL_COPY_READ_BUFFER, 4, 2, replace);
    glEnd();
    cbs_error("illegal_subdata");
    retained[0] = 91;
    retained[1] = 92;
    glBegin(GL_POINTS);
    glGetBufferSubData(GL_COPY_READ_BUFFER, 0, 2, retained);
    glEnd();
    printf("cbs_illegal_get %d 0x%x\n", retained[0] == 91 && retained[1] == 92,
           (unsigned)glGetError());
    glBegin(GL_POINTS);
    glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &flag);
    glEnd();
    cbs_error("illegal_get_param");
    cbs_content("retained_after_begin");
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 4, 0, 2);
    cbs_error("copy_valid");
    glBindBuffer(GL_COPY_READ_BUFFER, names[1]);
    cbs_content("copy_result");
    glBindBuffer(GL_COPY_READ_BUFFER, names[0]);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_READ_BUFFER, 0, 1, 4);
    cbs_error("copy_overlap");
    glBegin(GL_POINTS);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, -1, -1, -1);
    glEnd();
    cbs_error("illegal_copy_precedence");
    ptr = glMapBufferRange(GL_COPY_READ_BUFFER, 0, 8, GL_MAP_READ_BIT);
    printf("cbs_map_read %d 0x%x\n", ptr != NULL, (unsigned)glGetError());
    glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_MAPPED, &flag);
    printf("cbs_mapped %d 0x%x\n", flag, (unsigned)glGetError());
    ptr = glMapBufferRange(GL_COPY_READ_BUFFER, 0, 4, GL_MAP_READ_BIT);
    printf("cbs_double_map %d 0x%x\n", ptr == NULL, (unsigned)glGetError());
    glBegin(GL_POINTS);
    ok = glUnmapBuffer(GL_COPY_READ_BUFFER);
    glEnd();
    printf("cbs_illegal_unmap %d 0x%x\n", ok == GL_FALSE, (unsigned)glGetError());
    glGetBufferPointerv(GL_COPY_READ_BUFFER, GL_BUFFER_MAP_POINTER, &ptr);
    printf("cbs_map_pointer %d 0x%x\n", ptr != NULL, (unsigned)glGetError());
    ok = glUnmapBuffer(GL_COPY_READ_BUFFER);
    printf("cbs_unmap %d 0x%x\n", ok == GL_TRUE, (unsigned)glGetError());
    ptr = glMapBuffer(GL_COPY_READ_BUFFER, GL_READ_WRITE);
    printf("cbs_map_all %d 0x%x\n", ptr != NULL, (unsigned)glGetError());
    ok = glUnmapBuffer(GL_COPY_READ_BUFFER);
    printf("cbs_unmap_all %d 0x%x\n", ok == GL_TRUE, (unsigned)glGetError());
    ptr = glMapBufferRange(GL_COPY_READ_BUFFER, 0, 0, GL_MAP_READ_BIT);
    printf("cbs_zero_length_map %d 0x%x\n", ptr == NULL, (unsigned)glGetError());
    glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_MAPPED, &flag);
    printf("cbs_zero_length_unmapped %d 0x%x\n", flag == GL_FALSE, (unsigned)glGetError());
    ptr = glMapNamedBufferRange(names[0], 0, 0, GL_MAP_READ_BIT);
    printf("cbs_zero_length_named %d 0x%x\n", ptr == NULL, (unsigned)glGetError());
    ptr = glMapBufferRange(GL_COPY_READ_BUFFER, 0, 2, GL_MAP_READ_BIT);
    printf("cbs_map_for_write_checks %d 0x%x\n", ptr != NULL, (unsigned)glGetError());
    glBufferSubData(GL_COPY_READ_BUFFER, 5, 2, replace);
    cbs_error("subdata_nonoverlap_mapped");
    glBufferData(GL_COPY_READ_BUFFER, 4, init, GL_STATIC_DRAW);
    cbs_error("data_while_mapped");
    cbs_size("size_while_mapped");
    glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_MAPPED, &flag);
    printf("cbs_still_mapped %d 0x%x\n", flag, (unsigned)glGetError());
    glNamedBufferData(names[0], 4, init, GL_STATIC_DRAW);
    cbs_error("named_data_while_mapped");
    ok = glUnmapBuffer(GL_COPY_READ_BUFFER);
    printf("cbs_unmap_after_invalid_data %d 0x%x\n", ok == GL_TRUE, (unsigned)glGetError());
    cbs_content("after_mapped_rejections");
    glBufferData(GL_COPY_READ_BUFFER, 0, NULL, GL_STATIC_DRAW);
    cbs_error("zero_storage");
    ptr = glMapBuffer(GL_COPY_READ_BUFFER, GL_READ_ONLY);
    printf("cbs_zero_storage_map %d 0x%x\n", ptr == NULL, (unsigned)glGetError());
    glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_MAPPED, &flag);
    printf("cbs_zero_storage_unmapped %d 0x%x\n", flag == GL_FALSE, (unsigned)glGetError());
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    glDeleteBuffers(2, names);
    cbs_error("teardown");
}
#endif

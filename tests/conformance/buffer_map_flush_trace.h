#ifndef MGL_BUFFER_MAP_FLUSH_TRACE_H
#define MGL_BUFFER_MAP_FLUSH_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void bmf_error(const char *step) {
    printf("bmf_%s 0x%x\n", step, (unsigned)glGetError());
}

static void bmf_bound_query(const char *step, GLenum pname, GLint expected) {
    GLint result = -777;
    glGetBufferParameteriv(GL_COPY_READ_BUFFER, pname, &result);
    printf("bmf_%s %d 0x%x\n", step, result == expected, (unsigned)glGetError());
}

static void bmf_named_query(const char *step, GLuint buffer, GLenum pname, GLint expected) {
    GLint result = -777;
    glGetNamedBufferParameteriv(buffer, pname, &result);
    printf("bmf_%s %d 0x%x\n", step, result == expected, (unsigned)glGetError());
}

static void buffer_map_flush_trace(void) {
    const GLubyte initial[12] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    GLubyte returned[2] = {0, 0};
    GLuint buffer = 0;
    GLubyte *mapped;
    GLint value;
    GLint64 wide;
    void *pointer;
    GLboolean result;
    const GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_COPY_READ_BUFFER, buffer);
    glBufferData(GL_COPY_READ_BUFFER, sizeof(initial), initial, GL_DYNAMIC_DRAW);
    bmf_error("setup");
    bmf_bound_query("initial_flags", GL_BUFFER_ACCESS_FLAGS, 0);
    bmf_named_query("initial_length", buffer, GL_BUFFER_MAP_LENGTH, 0);
    mapped = glMapBufferRange(GL_COPY_READ_BUFFER, 2, 6, flags);
    printf("bmf_range_map %d 0x%x\n", mapped != NULL, (unsigned)glGetError());
    bmf_bound_query("range_flags", GL_BUFFER_ACCESS_FLAGS, (GLint)flags);
    bmf_bound_query("range_offset", GL_BUFFER_MAP_OFFSET, 2);
    bmf_bound_query("range_length", GL_BUFFER_MAP_LENGTH, 6);
    bmf_named_query("range_named_mapped", buffer, GL_BUFFER_MAPPED, GL_TRUE);
    pointer = NULL;
    glGetBufferPointerv(GL_COPY_READ_BUFFER, GL_BUFFER_MAP_POINTER, &pointer);
    printf("bmf_pointer %d 0x%x\n", pointer == mapped && pointer != NULL, (unsigned)glGetError());
    if (mapped != NULL) {
        mapped[0] = 61;
        mapped[1] = 67;
        mapped[3] = 71;
    }
    glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 0, 2);
    bmf_error("flush_at_mapping_start");
    glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 5, 2);
    bmf_error("flush_beyond_mapping");
    glFlushMappedBufferRange(GL_COPY_READ_BUFFER, -1, 1);
    bmf_error("flush_negative_offset");
    glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 0, -1);
    bmf_error("flush_negative_length");
    glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 6, 0);
    bmf_error("flush_empty_end");
    glBegin(GL_POINTS);
    glFlushMappedBufferRange(GL_COPY_READ_BUFFER, -1, -1);
    glEnd();
    bmf_error("illegal_flush_precedence");
    result = glUnmapBuffer(GL_COPY_READ_BUFFER);
    printf("bmf_unmap %d 0x%x\n", result == GL_TRUE, (unsigned)glGetError());
    glGetBufferSubData(GL_COPY_READ_BUFFER, 2, 2, returned);
    printf("bmf_flushed_bytes %d 0x%x\n", returned[0] == 61 && returned[1] == 67,
           (unsigned)glGetError());
    bmf_bound_query("unmapped_flags", GL_BUFFER_ACCESS_FLAGS, 0);
    pointer = (void *)&buffer;
    glGetNamedBufferPointerv(buffer, GL_BUFFER_MAP_POINTER, &pointer);
    printf("bmf_unmapped_pointer %d 0x%x\n", pointer == NULL, (unsigned)glGetError());
    glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 0, 1);
    bmf_error("flush_without_map");
    glFlushMappedBufferRange(GL_COPY_READ_BUFFER, -1, 1);
    bmf_error("unmapped_flush_negative_offset");
    glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 0, -1);
    bmf_error("unmapped_flush_negative_length");
    mapped = glMapBufferRange(GL_COPY_READ_BUFFER, 0, 2, GL_MAP_READ_BIT);
    printf("bmf_read_map %d 0x%x\n", mapped != NULL, (unsigned)glGetError());
    glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 0, 1);
    bmf_error("flush_without_explicit_flag");
    glFlushMappedBufferRange(GL_COPY_READ_BUFFER, -1, 1);
    bmf_error("no_explicit_flush_negative_offset");
    glUnmapBuffer(GL_COPY_READ_BUFFER);
    bmf_error("read_unmap");
    mapped = glMapBufferRange(GL_COPY_READ_BUFFER, 0, 2,
                              GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_READ_BIT);
    printf("bmf_read_write_explicit %d 0x%x\n", mapped != NULL, (unsigned)glGetError());
    glUnmapBuffer(GL_COPY_READ_BUFFER);
    bmf_error("read_write_unmap");
    mapped =
        glMapBufferRange(GL_COPY_READ_BUFFER, 0, 2, GL_MAP_READ_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    printf("bmf_read_only_flush_flag %d 0x%x\n", mapped == NULL, (unsigned)glGetError());
    mapped =
        glMapBufferRange(GL_COPY_READ_BUFFER, 0, 2, GL_MAP_READ_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
    printf("bmf_read_unsynchronized %d 0x%x\n", mapped == NULL, (unsigned)glGetError());
    mapped = glMapBufferRange(GL_COPY_READ_BUFFER, 0, 2, GL_MAP_WRITE_BIT | 0x80000000u);
    printf("bmf_unknown_bit %d 0x%x\n", mapped == NULL, (unsigned)glGetError());
    mapped = glMapBufferRange(GL_COPY_READ_BUFFER, 0, 2, 0);
    printf("bmf_no_access %d 0x%x\n", mapped == NULL, (unsigned)glGetError());
    mapped = glMapNamedBufferRange(buffer, 3, 4, flags);
    printf("bmf_named_map %d 0x%x\n", mapped != NULL, (unsigned)glGetError());
    wide = -1;
    glGetNamedBufferParameteri64v(buffer, GL_BUFFER_MAP_OFFSET, &wide);
    printf("bmf_named_wide_offset %d 0x%x\n", wide == 3, (unsigned)glGetError());
    wide = -1;
    glGetNamedBufferParameteri64v(buffer, GL_BUFFER_MAP_LENGTH, &wide);
    printf("bmf_named_wide_length %d 0x%x\n", wide == 4, (unsigned)glGetError());
    if (mapped != NULL) {
        mapped[0] = 79;
        mapped[1] = 83;
    }
    glFlushMappedNamedBufferRange(buffer, 0, 2);
    bmf_error("named_flush_start");
    glFlushMappedNamedBufferRange(buffer, 3, 2);
    bmf_error("named_flush_overrun");
    glBegin(GL_POINTS);
    glFlushMappedNamedBufferRange(buffer, -1, -1);
    glEnd();
    bmf_error("named_illegal_flush_precedence");
    value = -99;
    glBegin(GL_POINTS);
    glGetNamedBufferParameteriv(buffer, GL_BUFFER_MAP_LENGTH, &value);
    glEnd();
    printf("bmf_named_illegal_query %d 0x%x\n", value == -99, (unsigned)glGetError());
    result = glUnmapNamedBuffer(buffer);
    printf("bmf_named_unmap %d 0x%x\n", result == GL_TRUE, (unsigned)glGetError());
    glGetNamedBufferSubData(buffer, 3, 2, returned);
    printf("bmf_named_flushed_bytes %d 0x%x\n", returned[0] == 79 && returned[1] == 83,
           (unsigned)glGetError());
    bmf_named_query("final_flags", buffer, GL_BUFFER_ACCESS_FLAGS, 0);
    glFlushMappedNamedBufferRange(buffer, 0, 2);
    bmf_error("named_flush_unmapped");
    glFlushMappedNamedBufferRange(buffer, -1, 1);
    bmf_error("named_unmapped_negative_offset");
    glFlushMappedNamedBufferRange(buffer, 0, -1);
    bmf_error("named_unmapped_negative_length");
    /* Named subdata and bound subdata should use the same mapped-range rules. */
    mapped = glMapBufferRange(GL_COPY_READ_BUFFER, 0, 2, GL_MAP_READ_BIT);
    printf("bmf_map_for_nonoverlap %d 0x%x\n", mapped != NULL, (unsigned)glGetError());
    glNamedBufferSubData(buffer, 9, 2, (const GLubyte[]){101, 103});
    bmf_error("named_nonoverlap_subdata");
    glBufferSubData(GL_COPY_READ_BUFFER, 9, 2, (const GLubyte[]){107, 109});
    bmf_error("bound_nonoverlap_subdata");
    glNamedBufferSubData(buffer, 1, 2, (const GLubyte[]){111, 113});
    bmf_error("named_overlap_subdata");
    glBufferSubData(GL_COPY_READ_BUFFER, 1, 2, (const GLubyte[]){117, 119});
    bmf_error("bound_overlap_subdata");
    glUnmapBuffer(GL_COPY_READ_BUFFER);
    bmf_error("nonoverlap_unmap");
    returned[0] = 0;
    returned[1] = 0;
    glGetBufferSubData(GL_COPY_READ_BUFFER, 9, 2, returned);
    printf("bmf_nonoverlap_values %d 0x%x\n", returned[0] == 107 && returned[1] == 109,
           (unsigned)glGetError());
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glDeleteBuffers(1, &buffer);
    bmf_error("cleanup");
}
#endif

#ifndef MGL_SYNC_OBJECT_TRACE_H
#define MGL_SYNC_OBJECT_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void sync_trace_row(const char *name, int observation) {
    printf("sync_%s 0x%04x %d\n", name, (unsigned)glGetError(), observation);
}

static void sync_object_trace(void) {
    GLsync fence, rejected;
    GLsizei length;
    GLint value;
    GLboolean exists;
    GLenum result;

    fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    sync_trace_row("create", fence != NULL);
    sync_trace_row("is_valid", glIsSync(fence));
    length = -1;
    value = -1;
    glGetSynciv(fence, GL_OBJECT_TYPE, 1, &length, &value);
    sync_trace_row("object_type", value == (GLint)GL_SYNC_FENCE && length == 1);
    glGetSynciv(fence, GL_SYNC_CONDITION, 1, &length, &value);
    sync_trace_row("condition", value == (GLint)GL_SYNC_GPU_COMMANDS_COMPLETE);
    glGetSynciv(fence, GL_SYNC_FLAGS, 1, &length, &value);
    sync_trace_row("flags", value == 0);

    rejected = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 1);
    sync_trace_row("invalid_flags", rejected == NULL);
    rejected = glFenceSync(0, 0);
    sync_trace_row("invalid_condition", rejected == NULL);
    glBegin(GL_POINTS);
    rejected = glFenceSync(0, 1);
    glEnd();
    sync_trace_row("fence_begin_bad_args", rejected == NULL);
    glBegin(GL_POINTS);
    rejected = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glEnd();
    sync_trace_row("fence_begin_valid", rejected == NULL);
    glBegin(GL_POINTS);
    exists = glIsSync(fence);
    glEnd();
    sync_trace_row("is_begin", exists);
    glBegin(GL_POINTS);
    glDeleteSync(fence);
    glEnd();
    sync_trace_row("delete_begin", glIsSync(fence));
    result = glClientWaitSync(fence, 2, 0);
    sync_trace_row("client_wait_bad_flags", result == GL_WAIT_FAILED);
    /* For rejected Begin/End sync calls, compare the GL error and verify that the host issues no
     * native wait. */
    glBegin(GL_POINTS);
    result = glClientWaitSync(fence, 2, 0);
    glEnd();
    sync_trace_row("client_wait_begin_bad_flags", 0);
    glBegin(GL_POINTS);
    result = glClientWaitSync(fence, 0, 0);
    glEnd();
    sync_trace_row("client_wait_begin_valid", 0);
    glWaitSync(fence, 1, GL_TIMEOUT_IGNORED);
    sync_trace_row("wait_bad_flags", 0);
    glWaitSync(fence, 0, 0);
    sync_trace_row("wait_bad_timeout", 0);
    glBegin(GL_POINTS);
    glWaitSync(fence, 1, 0);
    glEnd();
    sync_trace_row("wait_begin_bad_args", glIsSync(fence));
    glBegin(GL_POINTS);
    glWaitSync(fence, 0, GL_TIMEOUT_IGNORED);
    glEnd();
    sync_trace_row("wait_begin_valid", glIsSync(fence));
    glGetSynciv(fence, 0, 1, &length, &value);
    sync_trace_row("get_invalid_pname", 0);
    glGetSynciv(fence, GL_OBJECT_TYPE, -1, &length, &value);
    sync_trace_row("get_invalid_size", 0);
    glBegin(GL_POINTS);
    glGetSynciv(fence, 0, -1, &length, &value);
    glEnd();
    sync_trace_row("get_begin_bad_args", glIsSync(fence));
    glBegin(GL_POINTS);
    glGetSynciv(fence, GL_OBJECT_TYPE, 1, &length, &value);
    glEnd();
    sync_trace_row("get_begin_valid", glIsSync(fence));
    glBegin(GL_POINTS);
    exists = glIsSync(NULL);
    glEnd();
    sync_trace_row("is_null_begin", exists);
    glBegin(GL_POINTS);
    glDeleteSync(NULL);
    glEnd();
    sync_trace_row("delete_null_begin", glIsSync(fence));
    glBegin(GL_POINTS);
    glGetSynciv(NULL, GL_OBJECT_TYPE, 1, &length, &value);
    glEnd();
    sync_trace_row("get_null_begin", glIsSync(fence));
    glBegin(GL_POINTS);
    glFlush();
    glEnd();
    sync_trace_row("flush_begin", glIsSync(fence));
    glBegin(GL_POINTS);
    glFinish();
    glEnd();
    sync_trace_row("finish_begin", glIsSync(fence));
    glFlush();
    sync_trace_row("flush_legal", glIsSync(fence));
    glFinish();
    sync_trace_row("finish_legal", glIsSync(fence));
    glDeleteSync(fence);
    sync_trace_row("delete_valid", glIsSync(fence));
}
#endif

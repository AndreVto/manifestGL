#ifndef MGL_FRAMEBUFFER_RENDERBUFFER_TRACE_H
#define MGL_FRAMEBUFFER_RENDERBUFFER_TRACE_H

#include "mgl/gl.h"
#include <stdio.h>

static void framebuffer_trace_emit(const char *step, GLint value) {
    GLenum error = glGetError();
    printf("fbo_%s %d 0x%04x\n", step, (int)value, (unsigned)error);
}

static void framebuffer_trace_trace(void) {
    GLuint rb = 0, fb = 0, created = 0;
    GLint value = -1;
    GLenum status;
    const GLenum none = GL_NONE;

    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 3, 2);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &value);
    framebuffer_trace_emit("initial_storage", value);

    glBegin(GL_POINTS);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 7, 6);
    glEnd();
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &value);
    framebuffer_trace_emit("storage_begin", value);

    glBegin(GL_POINTS);
    glRenderbufferStorage(0u, GL_RGBA4, 7, 6);
    glEnd();
    framebuffer_trace_emit("storage_invalid_target_begin", 0);

    glBegin(GL_POINTS);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 0, GL_RGBA4, 9, 8);
    glEnd();
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &value);
    framebuffer_trace_emit("multisample_begin", value);

    value = 12345;
    glBegin(GL_POINTS);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &value);
    glEnd();
    framebuffer_trace_emit("get_storage_begin", value);

    glBegin(GL_POINTS);
    glDeleteRenderbuffers(1, &rb);
    glEnd();
    framebuffer_trace_emit("delete_begin", glIsRenderbuffer(rb));

    glBegin(GL_POINTS);
    glCreateRenderbuffers(1, &created);
    glEnd();
    framebuffer_trace_emit("create_begin", 0);

    glBegin(GL_POINTS);
    glNamedRenderbufferStorage(rb, GL_RGBA4, 11, 10);
    glEnd();
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &value);
    framebuffer_trace_emit("named_storage_begin", value);

    glBegin(GL_POINTS);
    glNamedRenderbufferStorageMultisample(rb, 0, GL_RGBA4, 13, 12);
    glEnd();
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &value);
    framebuffer_trace_emit("named_multisample_begin", value);

    value = 12345;
    glBegin(GL_POINTS);
    glGetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_WIDTH, &value);
    glEnd();
    framebuffer_trace_emit("named_get_begin", value);

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &value);
    framebuffer_trace_emit("initial_attachment", value);

    glBegin(GL_POINTS);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
    glEnd();
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &value);
    framebuffer_trace_emit("attachment_begin", value);

    glBegin(GL_POINTS);
    glFramebufferRenderbuffer(0, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
    glEnd();
    framebuffer_trace_emit("attachment_invalid_target_begin", 0);

    glBegin(GL_POINTS);
    glNamedFramebufferRenderbuffer(fb, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
    glEnd();
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &value);
    framebuffer_trace_emit("named_attachment_begin", value);

    glBegin(GL_POINTS);
    glDrawBuffer(GL_NONE);
    glEnd();
    glGetIntegerv(GL_DRAW_BUFFER, &value);
    framebuffer_trace_emit("draw_buffer_begin", value);

    glBegin(GL_POINTS);
    glDrawBuffers(1, &none);
    glEnd();
    glGetIntegerv(GL_DRAW_BUFFER, &value);
    framebuffer_trace_emit("draw_buffers_begin", value);

    glBegin(GL_POINTS);
    glReadBuffer(GL_NONE);
    glEnd();
    glGetIntegerv(GL_READ_BUFFER, &value);
    framebuffer_trace_emit("read_buffer_begin", value);

    glBegin(GL_POINTS);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glEnd();
    (void)status;
    framebuffer_trace_emit("check_status_begin", 0);

    glBegin(GL_POINTS);
    status = glCheckNamedFramebufferStatus(fb, GL_FRAMEBUFFER);
    glEnd();
    (void)status;
    framebuffer_trace_emit("named_check_status_begin", 0);

    glBegin(GL_POINTS);
    glBlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glEnd();
    framebuffer_trace_emit("blit_begin", 0);

    value = 12345;
    glBegin(GL_POINTS);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &value);
    glEnd();
    framebuffer_trace_emit("get_attachment_begin", value);

    value = 12345;
    glBegin(GL_POINTS);
    glGetNamedFramebufferAttachmentParameteriv(fb, GL_COLOR_ATTACHMENT0,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &value);
    glEnd();
    framebuffer_trace_emit("named_get_attachment_begin", value);

    glBegin(GL_POINTS);
    glNamedFramebufferDrawBuffer(fb, GL_NONE);
    glEnd();
    glGetIntegerv(GL_DRAW_BUFFER, &value);
    framebuffer_trace_emit("named_draw_begin", value);

    glBegin(GL_POINTS);
    glNamedFramebufferDrawBuffers(fb, 1, &none);
    glEnd();
    glGetIntegerv(GL_DRAW_BUFFER, &value);
    framebuffer_trace_emit("named_draws_begin", value);

    glBegin(GL_POINTS);
    glNamedFramebufferReadBuffer(fb, GL_NONE);
    glEnd();
    glGetIntegerv(GL_READ_BUFFER, &value);
    framebuffer_trace_emit("named_read_begin", value);

    glBegin(GL_POINTS);
    glCreateFramebuffers(1, &created);
    glEnd();
    framebuffer_trace_emit("create_framebuffer_begin", 0);

    glBegin(GL_POINTS);
    glBlitNamedFramebuffer(fb, fb, 0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glEnd();
    framebuffer_trace_emit("named_blit_begin", 0);

    {
        const GLint int_value[] = {1, 2, 3, 4};
        const GLuint uint_value[] = {1u, 2u, 3u, 4u};
        const GLfloat float_value[] = {0.25f, 0.5f, 0.75f, 1.0f};
        glBegin(GL_POINTS);
        glClearBufferiv(GL_COLOR, 0, int_value);
        glEnd();
        framebuffer_trace_emit("clear_int_begin", 0);

        glBegin(GL_POINTS);
        glClearBufferuiv(GL_COLOR, 0, uint_value);
        glEnd();
        framebuffer_trace_emit("clear_uint_begin", 0);

        glBegin(GL_POINTS);
        glClearBufferfv(GL_COLOR, 0, float_value);
        glEnd();
        framebuffer_trace_emit("clear_float_begin", 0);

        glBegin(GL_POINTS);
        glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
        glEnd();
        framebuffer_trace_emit("clear_depth_stencil_begin", 0);

        glBegin(GL_POINTS);
        glClearNamedFramebufferiv(fb, GL_COLOR, 0, int_value);
        glEnd();
        framebuffer_trace_emit("named_clear_int_begin", 0);

        glBegin(GL_POINTS);
        glClearNamedFramebufferuiv(fb, GL_COLOR, 0, uint_value);
        glEnd();
        framebuffer_trace_emit("named_clear_uint_begin", 0);

        glBegin(GL_POINTS);
        glClearNamedFramebufferfv(fb, GL_COLOR, 0, float_value);
        glEnd();
        framebuffer_trace_emit("named_clear_float_begin", 0);

        glBegin(GL_POINTS);
        glClearNamedFramebufferfi(fb, GL_DEPTH_STENCIL, 0, 1.0f, 0);
        glEnd();
        framebuffer_trace_emit("named_clear_depth_stencil_begin", 0);
    }

    glNamedRenderbufferStorage(rb, GL_RGBA4, 5, 4);
    glGetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_WIDTH, &value);
    framebuffer_trace_emit("named_storage_after", value);
    glDrawBuffer(GL_NONE);
    glGetIntegerv(GL_DRAW_BUFFER, &value);
    framebuffer_trace_emit("draw_buffer_after", value);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
    glDeleteRenderbuffers(1, &rb);
    framebuffer_trace_emit("cleanup", 0);
}

#endif

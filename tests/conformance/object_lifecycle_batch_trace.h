#ifndef MGL_OBJECT_LIFECYCLE_BATCH_TRACE_H
#define MGL_OBJECT_LIFECYCLE_BATCH_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void olb_emit(const char *step, int ok) {
    printf("olb_%s %d 0x%x\n", step, ok, (unsigned)glGetError());
}

static void object_lifecycle_batch_trace(void) {
    GLuint b = 0, bc = 0, t = 0, tc = 0, va = 0, f = 0, fc = 0, r = 0, rc = 0;
    glGenBuffers(1, &b);
    olb_emit("gen_buffer", b != 0 && !glIsBuffer(b));
    glBindBuffer(GL_ARRAY_BUFFER, b);
    olb_emit("realized_buffer", glIsBuffer(b));
    glCreateBuffers(1, &bc);
    olb_emit("created_buffer", bc != 0 && glIsBuffer(bc));
    glDeleteBuffers(1, &bc);
    olb_emit("delete_created_buffer", !glIsBuffer(bc));
    glDeleteBuffers(1, &b);
    olb_emit("delete_bound_buffer", !glIsBuffer(b));
    glGenTextures(1, &t);
    olb_emit("gen_texture", t != 0 && !glIsTexture(t));
    glBindTexture(GL_TEXTURE_2D, t);
    olb_emit("realized_texture", glIsTexture(t));
    glCreateTextures(GL_TEXTURE_2D, 1, &tc);
    olb_emit("created_texture", tc != 0 && glIsTexture(tc));
    glDeleteTextures(1, &tc);
    olb_emit("delete_created_texture", !glIsTexture(tc));
    glDeleteTextures(1, &t);
    olb_emit("delete_bound_texture", !glIsTexture(t));
    glGenVertexArrays(1, &va);
    olb_emit("gen_vao", va != 0 && !glIsVertexArray(va));
    glBindVertexArray(va);
    olb_emit("realized_vao", glIsVertexArray(va));
    glDeleteVertexArrays(1, &va);
    olb_emit("delete_vao", !glIsVertexArray(va));
    glGenFramebuffers(1, &f);
    olb_emit("gen_framebuffer", f != 0 && !glIsFramebuffer(f));
    glBindFramebuffer(GL_FRAMEBUFFER, f);
    olb_emit("realized_framebuffer", glIsFramebuffer(f));
    glCreateFramebuffers(1, &fc);
    olb_emit("created_framebuffer", fc != 0 && glIsFramebuffer(fc));
    glDeleteFramebuffers(1, &fc);
    olb_emit("delete_created_framebuffer", !glIsFramebuffer(fc));
    glDeleteFramebuffers(1, &f);
    olb_emit("delete_framebuffer", !glIsFramebuffer(f));
    glGenRenderbuffers(1, &r);
    olb_emit("gen_renderbuffer", r != 0 && !glIsRenderbuffer(r));
    glBindRenderbuffer(GL_RENDERBUFFER, r);
    olb_emit("realized_renderbuffer", glIsRenderbuffer(r));
    glCreateRenderbuffers(1, &rc);
    olb_emit("created_renderbuffer", rc != 0 && glIsRenderbuffer(rc));
    glDeleteRenderbuffers(1, &rc);
    olb_emit("delete_created_renderbuffer", !glIsRenderbuffer(rc));
    glDeleteRenderbuffers(1, &r);
    olb_emit("delete_renderbuffer", !glIsRenderbuffer(r));
    /* Errors take precedence inside glBegin/glEnd; no objects are destroyed. */
    glCreateBuffers(1, &bc);
    olb_emit("setup_illegal_delete", glIsBuffer(bc));
    glBegin(GL_POINTS);
    glDeleteBuffers(1, &bc);
    glEnd();
    olb_emit("illegal_delete", glIsBuffer(bc));
    glBegin(GL_POINTS);
    glIsBuffer(bc);
    glEnd();
    olb_emit("illegal_is_buffer", glIsBuffer(bc));
    glBegin(GL_POINTS);
    glCreateFramebuffers(1, &fc);
    glEnd();
    olb_emit("illegal_create_framebuffer", glIsBuffer(bc));
    glBegin(GL_POINTS);
    glGenVertexArrays(-1, &va);
    glEnd();
    olb_emit("illegal_gen_vao_precedence", glIsBuffer(bc));
    glDeleteBuffers(1, &bc);
    olb_emit("cleanup", glGetError() == GL_NO_ERROR);
}
#endif

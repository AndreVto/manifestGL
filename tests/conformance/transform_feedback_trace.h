#ifndef MGL_TRANSFORM_FEEDBACK_TRACE_H
#define MGL_TRANSFORM_FEEDBACK_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void tf_trace_row(const char *label, int observed) {
    printf("tf_%s 0x%04x %d\n", label, (unsigned)glGetError(), observed);
}

static void transform_feedback_trace_trace(void) {
    GLuint tf = 0, created = 0, output;
    GLboolean exists;
    GLint binding = -1;
    GLsizei length = 73, size = 73;
    GLenum type = 0;
    GLchar name[8] = "marker";

    glGenTransformFeedbacks(-1, &output);
    tf_trace_row("gen_negative", 0);
    glGenTransformFeedbacks(1, &tf);
    tf_trace_row("gen_name", tf != 0u);
    tf_trace_row("not_realized", glIsTransformFeedback(tf));
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf);
    tf_trace_row("bind", glIsTransformFeedback(tf));

    output = 777u;
    glBegin(GL_POINTS);
    glGenTransformFeedbacks(-1, &output);
    glEnd();
    tf_trace_row("gen_begin_negative", output == 777u);
    output = 777u;
    glBegin(GL_POINTS);
    glGenTransformFeedbacks(1, &output);
    glEnd();
    tf_trace_row("gen_begin", output == 777u);
    output = 777u;
    glBegin(GL_POINTS);
    glCreateTransformFeedbacks(1, &output);
    glEnd();
    tf_trace_row("create_begin", output == 777u);
    glBegin(GL_POINTS);
    exists = glIsTransformFeedback(tf);
    glEnd();
    (void)exists; /* Return value after GL_INVALID_OPERATION is unspecified. */
    tf_trace_row("is_begin", 0);
    glBegin(GL_POINTS);
    glBindTransformFeedback(0u, tf);
    glEnd();
    tf_trace_row("bind_begin_bad_target", 0);
    glBegin(GL_POINTS);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0u);
    glEnd();
    tf_trace_row("bind_begin", 0);
    glGetIntegerv(GL_TRANSFORM_FEEDBACK_BINDING, &binding);
    tf_trace_row("binding_preserved", (GLuint)binding == tf);
    glBegin(GL_POINTS);
    glDeleteTransformFeedbacks(1, &tf);
    glEnd();
    tf_trace_row("delete_begin", glIsTransformFeedback(tf));
    glBegin(GL_POINTS);
    glBeginTransformFeedback(0u);
    glEnd();
    tf_trace_row("begin_tf_begin_bad_mode", 0);
    glBegin(GL_POINTS);
    glEndTransformFeedback();
    glEnd();
    tf_trace_row("end_tf_begin", 0);
    glBegin(GL_POINTS);
    glPauseTransformFeedback();
    glEnd();
    tf_trace_row("pause_tf_begin", 0);
    glBegin(GL_POINTS);
    glResumeTransformFeedback();
    glEnd();
    tf_trace_row("resume_tf_begin", 0);
    glBegin(GL_POINTS);
    glTransformFeedbackVaryings(0u, -1, NULL, 0u);
    glEnd();
    tf_trace_row("varyings_begin_bad_args", 0);
    glBegin(GL_POINTS);
    glGetTransformFeedbackVarying(0u, 0u, -1, &length, &size, &type, name);
    glEnd();
    tf_trace_row("get_varying_begin_bad_args", 0);
    glDeleteTransformFeedbacks(1, &tf);
    tf_trace_row("delete_valid", !glIsTransformFeedback(tf));
    glCreateTransformFeedbacks(1, &created);
    tf_trace_row("create_valid", created != 0u);
    tf_trace_row("create_realized", glIsTransformFeedback(created));
    glDeleteTransformFeedbacks(1, &created);
    tf_trace_row("delete_created", !glIsTransformFeedback(created));
}
#endif

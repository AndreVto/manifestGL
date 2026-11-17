#ifndef MGL_QUERY_OBJECT_TRACE_H
#define MGL_QUERY_OBJECT_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void query_trace_result(const char *case_name, GLuint id, GLuint before, GLint observed) {
    GLenum err = glGetError();
    printf("query_%s 0x%04x %u %u %d\n", case_name, (unsigned)err, (unsigned)id, (unsigned)before,
           (int)observed);
}

static void query_object_trace(void) {
    GLuint q = 0, spare = 0, created = 0;
    GLint out = -123;
    GLuint uout = 31337u;
    GLint64 i64 = -123;
    GLuint64 ui64 = 31337u;
    GLboolean exists;
    glGenQueries(1, &q);
    query_trace_result("gen", q != 0u ? 1u : 0u, 0u, glIsQuery(q));
    glBeginQuery(GL_SAMPLES_PASSED, q);
    query_trace_result("begin", q != 0u ? 1u : 0u, 0u, glIsQuery(q));
    glEndQuery(GL_SAMPLES_PASSED);
    query_trace_result("end", q != 0u ? 1u : 0u, 0u, glIsQuery(q));
    glGetQueryiv(GL_SAMPLES_PASSED, GL_CURRENT_QUERY, &out);
    query_trace_result("current", q != 0u ? 1u : 0u, 0u, out);
    glBegin(GL_POINTS);
    glGenQueries(1, &spare);
    glEnd();
    query_trace_result("gen_illegal", spare, 0u, 0);
    glBegin(GL_POINTS);
    glGenQueries(-1, &spare);
    glEnd();
    query_trace_result("gen_bad_count_precedence", spare, 0u, 0);
    glBegin(GL_POINTS);
    glCreateQueries(GL_SAMPLES_PASSED, 1, &created);
    glEnd();
    query_trace_result("create_illegal", created, 0u, 0);
    glBegin(GL_POINTS);
    glCreateQueries(0u, -1, &created);
    glEnd();
    query_trace_result("create_bad_args_precedence", created, 0u, 0);
    glBegin(GL_POINTS);
    exists = glIsQuery(q);
    glEnd();
    query_trace_result("is_illegal", exists, 0u, 0);
    glBegin(GL_POINTS);
    glDeleteQueries(1, &q);
    glEnd();
    query_trace_result("delete_illegal", glIsQuery(q), 0u, 0);
    glBegin(GL_POINTS);
    glBeginQuery(0u, q);
    glEnd();
    query_trace_result("begin_bad_target_precedence", glIsQuery(q), 0u, 0);
    glBegin(GL_POINTS);
    glBeginQuery(GL_SAMPLES_PASSED, q);
    glEnd();
    query_trace_result("begin_illegal", glIsQuery(q), 0u, 0);
    glBegin(GL_POINTS);
    glEndQuery(0u);
    glEnd();
    query_trace_result("end_bad_target_precedence", glIsQuery(q), 0u, 0);
    glBegin(GL_POINTS);
    glEndQuery(GL_SAMPLES_PASSED);
    glEnd();
    query_trace_result("end_illegal", glIsQuery(q), 0u, 0);
    glBegin(GL_POINTS);
    glQueryCounter(q, GL_TIMESTAMP);
    glEnd();
    query_trace_result("counter_illegal", glIsQuery(q), 0u, 0);
    out = -123;
    glBegin(GL_POINTS);
    glGetQueryiv(GL_SAMPLES_PASSED, GL_CURRENT_QUERY, &out);
    glEnd();
    query_trace_result("queryiv_illegal", 0u, 0u, out);
    out = -123;
    glBegin(GL_POINTS);
    glGetQueryiv(0u, 0u, &out);
    glEnd();
    query_trace_result("queryiv_bad_args_precedence", 0u, 0u, out);
    out = -123;
    glBegin(GL_POINTS);
    glGetQueryObjectiv(q, GL_QUERY_RESULT_AVAILABLE, &out);
    glEnd();
    query_trace_result("getiv_illegal", 0u, 0u, out);
    uout = 31337u;
    glBegin(GL_POINTS);
    glGetQueryObjectuiv(q, GL_QUERY_RESULT_AVAILABLE, &uout);
    glEnd();
    query_trace_result("getuiv_illegal", 0u, 0u, (GLint)uout);
    i64 = -123;
    glBegin(GL_POINTS);
    glGetQueryObjecti64v(q, GL_QUERY_RESULT_AVAILABLE, &i64);
    glEnd();
    query_trace_result("geti64_illegal", 0u, 0u, (GLint)i64);
    ui64 = 31337u;
    glBegin(GL_POINTS);
    glGetQueryObjectui64v(q, GL_QUERY_RESULT_AVAILABLE, &ui64);
    glEnd();
    query_trace_result("getui64_illegal", 0u, 0u, (GLint)ui64);
    spare = 0;
    glGenQueriesARB(1, &spare);
    query_trace_result("arb_gen", spare != 0u, 0u, glIsQueryARB(spare));
    glBegin(GL_POINTS);
    glDeleteQueriesARB(1, &q);
    glEnd();
    query_trace_result("arb_delete_illegal", glIsQueryARB(q), 0u, 0);
    glDeleteQueries(1, &q);
    query_trace_result("delete_legal", glIsQuery(q), 0u, 0);
}
#endif

#include "mgl/gl.h"
#include "core/context.h"
#include "abi/unimplemented_exports.h"
#include "objects/object_store.h"
#include <stdio.h>
#include <string.h>

static unsigned callback_count;
static GLuint callback_id;
static char callback_text[128];

static void cb(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
               const GLchar *message, const void *user) {
    (void)source;
    (void)type;
    (void)severity;
    (void)user;
    callback_count++;
    callback_id = id;
    if (length > 0) {
        size_t n = (size_t)length < sizeof(callback_text) - 1u ? (size_t)length
                                                               : sizeof(callback_text) - 1u;
        memcpy(callback_text, message, n);
        callback_text[n] = '\0';
    }
}

static int fail(int n, const char *m) {
    fprintf(stderr, "debug output failure %d: %s\n", n, m);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    char log[256] = {0};
    GLenum src = 0, type = 0, sev = 0;
    GLuint id = 0;
    GLsizei len = 0;
    GLint iv = 0;
    char label[64];
    GLsizei labellen = 0;
    void *ptr = NULL;
    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (!ctx.share_group)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_debug_init(&ctx.debug);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);
    glDebugMessageCallback(cb, (const void *)0x1234);
    glGetPointerv(GL_DEBUG_CALLBACK_USER_PARAM, &ptr);
    if (ptr != (void *)0x1234)
        return fail(2, "callback user pointer");
    glEnable(GL_DEBUG_OUTPUT);
    if (!glIsEnabled(GL_DEBUG_OUTPUT))
        return fail(3, "debug enable");
    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 77u,
                         GL_DEBUG_SEVERITY_LOW, -1, "hello debug");
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || callback_count != 1u || callback_id != 77u ||
        strcmp(callback_text, "hello debug") != 0)
        return fail(4, "callback delivery");
    glGetIntegerv(GL_DEBUG_LOGGED_MESSAGES, &iv);
    if (iv != 1)
        return fail(5, "logged count");
    if (glGetDebugMessageLog(1u, (GLsizei)sizeof(log), &src, &type, &id, &sev, &len, log) != 1u ||
        id != 77u || strcmp(log, "hello debug") != 0)
        return fail(6, "message log");
    glDebugMessageControl(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, GL_DONT_CARE, 0, NULL,
                          GL_FALSE);
    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 78u,
                         GL_DEBUG_SEVERITY_LOW, -1, "filtered");
    if (callback_count != 1u)
        return fail(7, "filter");
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 9u, -1, "group");
    glGetIntegerv(GL_DEBUG_GROUP_STACK_DEPTH, &iv);
    if (iv != 1)
        return fail(8, "group push");
    glPopDebugGroup();
    glGetIntegerv(GL_DEBUG_GROUP_STACK_DEPTH, &iv);
    if (iv != 0)
        return fail(9, "group pop");
    glObjectLabel(GL_BUFFER, 5u, -1, "vertex-data");
    memset(label, 0, sizeof(label));
    glGetObjectLabel(GL_BUFFER, 5u, (GLsizei)sizeof(label), &labellen, label);
    if (labellen != 11 || strcmp(label, "vertex-data") != 0)
        return fail(10, "object label");
    glObjectPtrLabel((const void *)0xfeedu, -1, "sync-tag");
    memset(label, 0, sizeof(label));
    glGetObjectPtrLabel((const void *)0xfeedu, (GLsizei)sizeof(label), &labellen, label);
    if (strcmp(label, "sync-tag") != 0)
        return fail(11, "ptr label");
    (void)glGetDebugMessageLog(8u, (GLsizei)sizeof(log), NULL, NULL, NULL, NULL, NULL, log);
    glDebugMessageCallbackARB(cb, (const void *)0x4321);
    glDebugMessageControlARB(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 0,
                             NULL, GL_TRUE);
    glDebugMessageInsertARB(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 79u,
                            GL_DEBUG_SEVERITY_HIGH, -1, "ARB debug");
    memset(log, 0, sizeof(log));
    if (glGetDebugMessageLogARB(1u, (GLsizei)sizeof(log), &src, &type, &id, &sev, &len, log) !=
            1u ||
        id != 79u || strcmp(log, "ARB debug") != 0 || callback_id != 79u)
        return fail(12, "ARB debug query");
    glPopDebugGroup();
    if (mgl_error_take(&ctx.errors) != GL_STACK_UNDERFLOW)
        return fail(13, "group underflow");
    mgl_context_set_current_for_test(NULL);
    mgl_debug_destroy(&ctx.debug);
    mgl_share_group_release(ctx.share_group);
    puts("mGL debug output unit test: PASS");
    return 0;
}

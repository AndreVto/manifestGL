#include "mgl/gl.h"
#include "core/context.h"
#include "compat/fixed_function.h"
#include "shader/translator.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int fail(int code, const char *message) {
    fprintf(stderr, "display-list transaction failure %d: %s\n", code, message);
    return code;
}

static void init_context(struct mgl_context *ctx, struct mgl_share_group *group) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->share_group = group;
    mgl_error_init(&ctx->errors);
    mgl_compat_state_init(&ctx->compat);
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][0] = 1.0f;
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][1] = 1.0f;
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][2] = 1.0f;
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][3] = 1.0f;
}

static void set_white(struct mgl_context *ctx) {
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][0] = 1.0f;
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][1] = 1.0f;
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][2] = 1.0f;
    ctx->compat_current[MGL_COMPAT_SEM_COLOR][3] = 1.0f;
}

static int color_is(const struct mgl_context *ctx, GLfloat r, GLfloat g, GLfloat b) {
    return ctx->compat_current[MGL_COMPAT_SEM_COLOR][0] == r &&
           ctx->compat_current[MGL_COMPAT_SEM_COLOR][1] == g &&
           ctx->compat_current[MGL_COMPAT_SEM_COLOR][2] == b;
}

struct replay_blocker {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int armed;
    int entered;
    int released;
};

struct replay_thread_args {
    struct mgl_context *ctx;
    GLuint list;
    GLenum error;
    GLfloat color[3];
};

static struct replay_blocker blocker = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0,
                                        0};

static void fake_clear_buffer_fv(GLenum buffer, GLint drawbuffer, const GLfloat *value) {
    (void)buffer;
    (void)drawbuffer;
    (void)value;
    (void)pthread_mutex_lock(&blocker.mutex);
    if (blocker.armed) {
        blocker.entered = 1;
        (void)pthread_cond_broadcast(&blocker.cond);
        while (!blocker.released)
            (void)pthread_cond_wait(&blocker.cond, &blocker.mutex);
    }
    (void)pthread_mutex_unlock(&blocker.mutex);
}

static void arm_blocker(void) {
    (void)pthread_mutex_lock(&blocker.mutex);
    blocker.armed = 1;
    blocker.entered = 0;
    blocker.released = 0;
    (void)pthread_mutex_unlock(&blocker.mutex);
}

static int wait_for_blocker(void) {
    struct timespec deadline;
    int entered;
    (void)timespec_get(&deadline, TIME_UTC);
    deadline.tv_sec += 2;
    (void)pthread_mutex_lock(&blocker.mutex);
    while (!blocker.entered) {
        if (pthread_cond_timedwait(&blocker.cond, &blocker.mutex, &deadline) != 0)
            break;
    }
    entered = blocker.entered;
    (void)pthread_mutex_unlock(&blocker.mutex);
    return entered;
}

static void release_blocker(void) {
    (void)pthread_mutex_lock(&blocker.mutex);
    blocker.released = 1;
    blocker.armed = 0;
    (void)pthread_cond_broadcast(&blocker.cond);
    (void)pthread_mutex_unlock(&blocker.mutex);
}

static void *replay_thread_main(void *opaque) {
    struct replay_thread_args *args = (struct replay_thread_args *)opaque;
    mgl_context_set_current_for_test(args->ctx);
    set_white(args->ctx);
    glCallList(args->list);
    args->error = mgl_error_take(&args->ctx->errors);
    args->color[0] = args->ctx->compat_current[MGL_COMPAT_SEM_COLOR][0];
    args->color[1] = args->ctx->compat_current[MGL_COMPAT_SEM_COLOR][1];
    args->color[2] = args->ctx->compat_current[MGL_COMPAT_SEM_COLOR][2];
    mgl_context_set_current_for_test(NULL);
    return NULL;
}

int main(void) {
    struct mgl_context a;
    struct mgl_context b;
    struct mgl_native_dispatch native;
    struct replay_thread_args replay_args;
    pthread_t replay_thread;
    struct mgl_share_group *group = mgl_share_group_create();
    const GLuint fresh = 0x1000u;
    const GLuint replace = 0x1001u;
    const GLuint victim = 0x1002u;
    const GLuint holder = 0x1003u;
    const GLuint inflight_replace = 0x1004u;
    const GLuint inflight_delete = 0x1005u;
    const GLfloat clear_value[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    if (group == NULL)
        return fail(1, "share group");
    init_context(&a, group);
    init_context(&b, group);
    memset(&native, 0, sizeof(native));
    native.glesClearBufferfv = fake_clear_buffer_fv;
    b.native_dispatch = &native;

    /* A list definition enters the shared namespace at EndList. */
    mgl_context_set_current_for_test(&a);
    glNewList(fresh, GL_COMPILE);
    glColor3f(1.0f, 0.0f, 0.0f);
    mgl_context_set_current_for_test(&b);
    if (glIsList(fresh) != GL_FALSE)
        return fail(2, "fresh list became visible before EndList");
    mgl_context_set_current_for_test(&a);
    glEndList();
    mgl_context_set_current_for_test(&b);
    if (glIsList(fresh) != GL_TRUE)
        return fail(3, "fresh list not associated at EndList");
    set_white(&b);
    glCallList(fresh);
    if (!color_is(&b, 1.0f, 0.0f, 0.0f))
        return fail(4, "fresh list replay");

    /* Re-definition is staged: another sharing context keeps observing the old definition. */
    mgl_context_set_current_for_test(&a);
    glNewList(replace, GL_COMPILE);
    glColor3f(1.0f, 0.0f, 0.0f);
    glEndList();
    glNewList(replace, GL_COMPILE);
    glColor3f(0.0f, 0.0f, 1.0f);

    mgl_context_set_current_for_test(&b);
    set_white(&b);
    glCallList(replace);
    if (!color_is(&b, 1.0f, 0.0f, 0.0f))
        return fail(5, "old definition not retained during replacement");

    mgl_context_set_current_for_test(&a);
    glEndList();
    mgl_context_set_current_for_test(&b);
    set_white(&b);
    glCallList(replace);
    if (!color_is(&b, 0.0f, 0.0f, 1.0f))
        return fail(6, "replacement not committed at EndList");

    /* DeleteLists is an immediate command while compiling and must not be retained in holder. */
    mgl_context_set_current_for_test(&a);
    glNewList(victim, GL_COMPILE);
    glColor3f(0.0f, 1.0f, 0.0f);
    glEndList();
    glNewList(holder, GL_COMPILE);
    glDeleteLists(victim, 1);
    if (mgl_error_take(&a.errors) != GL_NO_ERROR)
        return fail(7, "DeleteLists rejected during compilation");
    if (glIsList(victim) != GL_FALSE)
        return fail(8, "DeleteLists was not immediate during compilation");
    glEndList();

    glNewList(victim, GL_COMPILE);
    glColor3f(1.0f, 1.0f, 0.0f);
    glEndList();
    glCallList(holder);
    if (glIsList(victim) != GL_TRUE)
        return fail(9, "DeleteLists was incorrectly retained in display list");

    /* Deleting the old definition of the list under construction does not cancel the staged
     * replacement. */
    glNewList(replace, GL_COMPILE);
    glColor3f(0.0f, 1.0f, 1.0f);
    glDeleteLists(replace, 1);
    if (glIsList(replace) != GL_FALSE)
        return fail(10, "old definition survived immediate delete");
    glEndList();
    if (glIsList(replace) != GL_TRUE)
        return fail(11, "staged replacement lost after delete");
    set_white(&a);
    glCallList(replace);
    if (!color_is(&a, 0.0f, 1.0f, 1.0f))
        return fail(12, "staged replacement contents after delete");

    /* A sharing context may replace a name while another thread is executing the old definition. */
    glNewList(inflight_replace, GL_COMPILE);
    glClearBufferfv(GL_COLOR, 0, clear_value);
    glColor3f(1.0f, 0.0f, 0.0f);
    glEndList();
    memset(&replay_args, 0, sizeof(replay_args));
    replay_args.ctx = &b;
    replay_args.list = inflight_replace;
    arm_blocker();
    if (pthread_create(&replay_thread, NULL, replay_thread_main, &replay_args) != 0)
        return fail(13, "replacement replay thread");
    if (!wait_for_blocker()) {
        release_blocker();
        (void)pthread_join(replay_thread, NULL);
        return fail(14, "replacement replay did not enter native");
    }
    glNewList(inflight_replace, GL_COMPILE);
    glColor3f(0.0f, 0.0f, 1.0f);
    glEndList();
    release_blocker();
    (void)pthread_join(replay_thread, NULL);
    if (replay_args.error != GL_NO_ERROR || replay_args.color[0] != 1.0f ||
        replay_args.color[1] != 0.0f || replay_args.color[2] != 0.0f)
        return fail(15, "in-flight replacement did not retain old definition");
    set_white(&a);
    glCallList(inflight_replace);
    if (!color_is(&a, 0.0f, 0.0f, 1.0f))
        return fail(16, "replacement definition not visible after commit");

    /* Deletion removes the name immediately but an already executing definition remains alive to
     * completion. */
    glNewList(inflight_delete, GL_COMPILE);
    glClearBufferfv(GL_COLOR, 0, clear_value);
    glColor3f(0.0f, 1.0f, 0.0f);
    glEndList();
    memset(&replay_args, 0, sizeof(replay_args));
    replay_args.ctx = &b;
    replay_args.list = inflight_delete;
    arm_blocker();
    if (pthread_create(&replay_thread, NULL, replay_thread_main, &replay_args) != 0)
        return fail(17, "delete replay thread");
    if (!wait_for_blocker()) {
        release_blocker();
        (void)pthread_join(replay_thread, NULL);
        return fail(18, "delete replay did not enter native");
    }
    glDeleteLists(inflight_delete, 1);
    if (glIsList(inflight_delete) != GL_FALSE) {
        release_blocker();
        (void)pthread_join(replay_thread, NULL);
        return fail(19, "deleted in-flight name remained visible");
    }
    release_blocker();
    (void)pthread_join(replay_thread, NULL);
    if (replay_args.error != GL_NO_ERROR || replay_args.color[0] != 0.0f ||
        replay_args.color[1] != 1.0f || replay_args.color[2] != 0.0f)
        return fail(20, "in-flight delete did not retain executing definition");

    if (mgl_error_take(&a.errors) != GL_NO_ERROR || mgl_error_take(&b.errors) != GL_NO_ERROR)
        return fail(21, "unexpected GL error");

    glDeleteLists(fresh, 1);
    glDeleteLists(replace, 1);
    glDeleteLists(victim, 1);
    glDeleteLists(holder, 1);
    glDeleteLists(inflight_replace, 1);
    glDeleteLists(inflight_delete, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(group);
    puts("mGL display-list transaction unit test: PASS");
    return 0;
}

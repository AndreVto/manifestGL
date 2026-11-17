#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "abi/proc_address.h"
#include "compat/fixed_function.h"

#include <stdio.h>
#include <string.h>

static int fail(int n, const char *message) {
    fprintf(stderr, "ARB program parameter semantics failure %d: %s\n", n, message);
    return n;
}

static int same4f(const GLfloat *a, const GLfloat *b) {
    return memcmp(a, b, 4u * sizeof(*a)) == 0;
}

static int same4d(const GLdouble *a, const GLdouble *b) {
    return memcmp(a, b, 4u * sizeof(*a)) == 0;
}

int main(void) {
    struct mgl_context ctx;
    GLuint named = 0u;
    GLuint env_list;
    GLuint local_list;
    GLuint execute_list;
    GLfloat gotf[4];
    GLdouble gotd[4];
    const GLfloat zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const GLfloat env0[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    const GLdouble env1d[4] = {5.0, 6.0, 7.0, 8.0};
    const GLfloat env1f[4] = {5.0f, 6.0f, 7.0f, 8.0f};
    const GLdouble env2d[4] = {9.0, 10.0, 11.0, 12.0};
    const GLfloat env2f[4] = {9.0f, 10.0f, 11.0f, 12.0f};
    const GLfloat default_local_v[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    const GLdouble default_local_f_d[4] = {1.5, 2.5, 3.5, 4.5};
    const GLfloat default_local_f[4] = {1.5f, 2.5f, 3.5f, 4.5f};
    GLfloat bulk_env[8] = {21.0f, 22.0f, 23.0f, 24.0f, 25.0f, 26.0f, 27.0f, 28.0f};
    GLfloat bulk_local[8] = {31.0f, 32.0f, 33.0f, 34.0f, 35.0f, 36.0f, 37.0f, 38.0f};
    const GLfloat named_before[4] = {41.0f, 42.0f, 43.0f, 44.0f};
    const GLfloat default_before[4] = {51.0f, 52.0f, 53.0f, 54.0f};
    const GLfloat list_local0[4] = {61.0f, 62.0f, 63.0f, 64.0f};
    const GLfloat execute_value[4] = {71.0f, 72.0f, 73.0f, 74.0f};
    const GLfloat changed_value[4] = {81.0f, 82.0f, 83.0f, 84.0f};
    const char *proc_names[] = {"glProgramEnvParameter4fARB",    "glProgramEnvParameter4dARB",
                                "glProgramEnvParameter4dvARB",   "glProgramLocalParameter4fARB",
                                "glProgramLocalParameter4dARB",  "glProgramLocalParameter4dvARB",
                                "glGetProgramEnvParameterdvARB", "glGetProgramLocalParameterdvARB",
                                "glProgramEnvParameters4fvEXT",  "glProgramLocalParameters4fvEXT"};
    size_t i;

    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.arb_program_error_position = -1;
    mgl_context_set_current_for_test(&ctx);

    /* Scalar/vector and float/double forms share canonical environment state. */
    glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 0u, env0[0], env0[1], env0[2], env0[3]);
    glGetProgramEnvParameterdvARB(GL_VERTEX_PROGRAM_ARB, 0u, gotd);
    if (gotd[0] != 1.0 || gotd[1] != 2.0 || gotd[2] != 3.0 || gotd[3] != 4.0)
        return fail(2, "scalar float environment round trip through double getter");

    glProgramEnvParameter4dvARB(GL_VERTEX_PROGRAM_ARB, 1u, env1d);
    glGetProgramEnvParameterfvARB(GL_VERTEX_PROGRAM_ARB, 1u, gotf);
    if (!same4f(gotf, env1f))
        return fail(3, "vector double environment conversion");

    glProgramEnvParameter4dARB(GL_FRAGMENT_PROGRAM_ARB, 2u, env2d[0], env2d[1], env2d[2], env2d[3]);
    glGetProgramEnvParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, 2u, gotf);
    if (!same4f(gotf, env2f))
        return fail(4, "scalar double environment conversion");

    /* Program name zero is a real target-specific default object with local parameters. */
    glProgramLocalParameter4fvARB(GL_VERTEX_PROGRAM_ARB, 3u, default_local_v);
    glGetProgramLocalParameterfvARB(GL_VERTEX_PROGRAM_ARB, 3u, gotf);
    if (!same4f(gotf, default_local_v) || glGetError() != GL_NO_ERROR)
        return fail(5, "default vertex-program local state");

    glProgramLocalParameter4dvARB(GL_FRAGMENT_PROGRAM_ARB, 4u, default_local_f_d);
    glGetProgramLocalParameterdvARB(GL_FRAGMENT_PROGRAM_ARB, 4u, gotd);
    if (!same4d(gotd, default_local_f_d) || glGetError() != GL_NO_ERROR)
        return fail(6, "default fragment-program local double state");
    glGetProgramLocalParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, 4u, gotf);
    if (!same4f(gotf, default_local_f))
        return fail(7, "default fragment-program local float query");

    /* Parameter commands are illegal inside Begin/End, ahead of ordinary argument validation. */
    glBegin(GL_POINTS);
    glProgramEnvParameter4fARB(GL_ARRAY_BUFFER, 999u, 1.0f, 2.0f, 3.0f, 4.0f);
    if (glGetError() != GL_NO_ERROR)
        return fail(8, "environment GetError during Begin");
    glProgramLocalParameter4dvARB(GL_ARRAY_BUFFER, 999u, default_local_f_d);
    if (glGetError() != GL_NO_ERROR)
        return fail(9, "local GetError during Begin");
    gotd[0] = gotd[1] = gotd[2] = gotd[3] = -99.0;
    glGetProgramEnvParameterdvARB(GL_ARRAY_BUFFER, 999u, gotd);
    if (glGetError() != GL_NO_ERROR)
        return fail(10, "environment getter GetError during Begin");
    glGetProgramLocalParameterdvARB(GL_ARRAY_BUFFER, 999u, gotd);
    if (glGetError() != GL_NO_ERROR)
        return fail(11, "local getter GetError during Begin");
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION || glGetError() != GL_NO_ERROR)
        return fail(12, "End after parameter legality checks");

    /* GL_COMPILE retains setters without mutating state; array operands are copied by value. */
    env_list = glGenLists(1);
    if (env_list == 0u)
        return fail(13, "environment list allocation");
    glGetProgramEnvParameterfvARB(GL_VERTEX_PROGRAM_ARB, 10u, gotf);
    if (!same4f(gotf, zero))
        return fail(14, "environment list baseline");
    glNewList(env_list, GL_COMPILE);
    glProgramEnvParameter4dARB(GL_VERTEX_PROGRAM_ARB, 10u, 11.0, 12.0, 13.0, 14.0);
    glProgramEnvParameters4fvEXT(GL_VERTEX_PROGRAM_ARB, 11u, 2, bulk_env);
    glEndList();
    if (glGetError() != GL_NO_ERROR)
        return fail(15, "environment list compile");
    glGetProgramEnvParameterfvARB(GL_VERTEX_PROGRAM_ARB, 10u, gotf);
    if (!same4f(gotf, zero))
        return fail(16, "environment list executed during GL_COMPILE");
    for (i = 0u; i < 8u; ++i)
        bulk_env[i] = -1.0f;
    glCallList(env_list);
    if (glGetError() != GL_NO_ERROR)
        return fail(17, "environment list replay");
    {
        const GLfloat expected10[4] = {11.0f, 12.0f, 13.0f, 14.0f};
        const GLfloat expected11[4] = {21.0f, 22.0f, 23.0f, 24.0f};
        const GLfloat expected12[4] = {25.0f, 26.0f, 27.0f, 28.0f};
        glGetProgramEnvParameterfvARB(GL_VERTEX_PROGRAM_ARB, 10u, gotf);
        if (!same4f(gotf, expected10))
            return fail(18, "environment scalar list replay");
        glGetProgramEnvParameterfvARB(GL_VERTEX_PROGRAM_ARB, 11u, gotf);
        if (!same4f(gotf, expected11))
            return fail(19, "environment bulk list capture first");
        glGetProgramEnvParameterfvARB(GL_VERTEX_PROGRAM_ARB, 12u, gotf);
        if (!same4f(gotf, expected12))
            return fail(20, "environment bulk list capture second");
    }

    /* Local-parameter list replay resolves the program binding at replay time. */
    glGenProgramsARB(1, &named);
    if (named == 0u)
        return fail(21, "named program allocation");
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, named);
    glProgramLocalParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, 20u, named_before);
    local_list = glGenLists(1);
    if (local_list == 0u)
        return fail(22, "local list allocation");
    glNewList(local_list, GL_COMPILE);
    glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 20u, list_local0[0], list_local0[1],
                                 list_local0[2], list_local0[3]);
    glProgramLocalParameters4fvEXT(GL_FRAGMENT_PROGRAM_ARB, 21u, 2, bulk_local);
    glEndList();
    if (glGetError() != GL_NO_ERROR)
        return fail(23, "local list compile");
    glGetProgramLocalParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, 20u, gotf);
    if (!same4f(gotf, named_before))
        return fail(24, "local list executed during GL_COMPILE");
    for (i = 0u; i < 8u; ++i)
        bulk_local[i] = -2.0f;

    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0u);
    glProgramLocalParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, 20u, default_before);
    glCallList(local_list);
    if (glGetError() != GL_NO_ERROR)
        return fail(25, "local list replay on default object");
    glGetProgramLocalParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, 20u, gotf);
    if (!same4f(gotf, list_local0))
        return fail(26, "late-bound local scalar replay");
    {
        const GLfloat expected21[4] = {31.0f, 32.0f, 33.0f, 34.0f};
        const GLfloat expected22[4] = {35.0f, 36.0f, 37.0f, 38.0f};
        glGetProgramLocalParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, 21u, gotf);
        if (!same4f(gotf, expected21))
            return fail(27, "late-bound local bulk replay first");
        glGetProgramLocalParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, 22u, gotf);
        if (!same4f(gotf, expected22))
            return fail(28, "late-bound local bulk replay second");
    }
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, named);
    glGetProgramLocalParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, 20u, gotf);
    if (!same4f(gotf, named_before))
        return fail(29, "compile-time local binding was captured incorrectly");

    /* GL_COMPILE_AND_EXECUTE updates state immediately and records the command for replay. */
    execute_list = glGenLists(1);
    if (execute_list == 0u)
        return fail(30, "execute list allocation");
    glNewList(execute_list, GL_COMPILE_AND_EXECUTE);
    glProgramLocalParameter4dARB(GL_FRAGMENT_PROGRAM_ARB, 24u, execute_value[0], execute_value[1],
                                 execute_value[2], execute_value[3]);
    glEndList();
    glGetProgramLocalParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, 24u, gotf);
    if (!same4f(gotf, execute_value))
        return fail(31, "compile-and-execute immediate local update");
    glProgramLocalParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, 24u, changed_value);
    glCallList(execute_list);
    glGetProgramLocalParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, 24u, gotf);
    if (!same4f(gotf, execute_value))
        return fail(32, "compile-and-execute retained local update");

    glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, MGL_ARB_MAX_PROGRAM_PARAMETERS, 1.0f, 2.0f,
                               3.0f, 4.0f);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(33, "scalar environment index validation");
    glProgramLocalParameter4dARB(GL_FRAGMENT_PROGRAM_ARB, MGL_ARB_MAX_PROGRAM_PARAMETERS, 1.0, 2.0,
                                 3.0, 4.0);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(34, "scalar local index validation");
    glGetProgramEnvParameterdvARB(GL_ARRAY_BUFFER, 0u, gotd);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(35, "double environment getter target validation");
    glGetProgramLocalParameterdvARB(GL_ARRAY_BUFFER, 0u, gotd);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(36, "double local getter target validation");

    for (i = 0u; i < sizeof(proc_names) / sizeof(proc_names[0]); ++i) {
        if (mgl_proc_address(proc_names[i]) == NULL)
            return fail(37, "proc-address coverage");
    }

    glDeleteLists(env_list, 1);
    glDeleteLists(local_list, 1);
    glDeleteLists(execute_list, 1);
    glDeleteProgramsARB(1, &named);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL ARB program parameter semantics test: PASS");
    return 0;
}

#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "compat/arb_program.h"
#include "compat/fixed_function.h"
#include "shader/shader_store.h"
#include "abi/proc_address.h"

#include <stdio.h>
#include <string.h>

static int fail(int n, const char *message) {
    fprintf(stderr, "ARB program string semantics failure %d: %s\n", n, message);
    return n;
}

static int query_string(GLenum target, char *out, size_t capacity, GLint *length) {
    GLint n = -1;
    memset(out, 0x5a, capacity);
    glGetProgramivARB(target, GL_PROGRAM_LENGTH_ARB, &n);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    if (n < 0 || (size_t)n > capacity)
        return 0;
    glGetProgramStringARB(target, GL_PROGRAM_STRING_ARB, out);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    if (length != NULL)
        *length = n;
    return 1;
}

int main(void) {
    struct mgl_context ctx;
    GLuint named = 0u;
    GLuint compile_list;
    GLuint execute_list;
    GLint length = -1;
    GLint binding = -1;
    char got[512];
    char compile_source[256] = "!!ARBvp1.0\n"
                               "MOV result.position, vertex.position;\n"
                               "MOV result.color, vertex.color.primary;\n"
                               "END";
    const char default_source[] = "!!ARBvp1.0\n"
                                  "MOV result.position, vertex.position;\n"
                                  "END";
    const char named_source[] = "!!ARBvp1.0\n"
                                "ADD result.position, vertex.position, program.env[0];\n"
                                "END";
    const char execute_source[] = "!!ARBvp1.0\n"
                                  "MOV result.position, vertex.position;\n"
                                  "MOV result.color.secondary, vertex.color.secondary;\n"
                                  "END";
    const size_t default_len = strlen(default_source);
    const size_t named_len = strlen(named_source);
    const size_t compile_len = strlen(compile_source);
    const size_t execute_len = strlen(execute_source);

    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.arb_program_error_position = -1;
    mgl_program_init_metadata(&ctx.arb_linked_program);
    mgl_context_set_current_for_test(&ctx);

    /* Name zero is the initially bound, initially empty default object. */
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_BINDING_ARB, &binding);
    if (binding != 0 || glGetError() != GL_NO_ERROR)
        return fail(2, "initial default binding");
    if (!query_string(GL_VERTEX_PROGRAM_ARB, got, sizeof(got), &length) || length != 0)
        return fail(3, "initial default program query");

    glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)default_len,
                       default_source);
    if (glGetError() != GL_NO_ERROR)
        return fail(4, "direct default program load");
    if (!query_string(GL_VERTEX_PROGRAM_ARB, got, sizeof(got), &length) ||
        length != (GLint)default_len || memcmp(got, default_source, default_len) != 0)
        return fail(5, "direct default program query");

    glGenProgramsARB(1, &named);
    if (named == 0u)
        return fail(6, "named program allocation");
    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, named);
    glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)named_len,
                       named_source);
    if (glGetError() != GL_NO_ERROR)
        return fail(7, "named baseline load");

    /* ProgramString is retained: compile does not execute, payload is copied,
     * and the current program is resolved when the list is replayed. */
    compile_list = glGenLists(1);
    if (compile_list == 0u)
        return fail(8, "compile list allocation");
    glNewList(compile_list, GL_COMPILE);
    glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)compile_len,
                       compile_source);
    glEndList();
    if (glGetError() != GL_NO_ERROR)
        return fail(9, "program string list compile");
    if (!query_string(GL_VERTEX_PROGRAM_ARB, got, sizeof(got), &length) ||
        length != (GLint)named_len || memcmp(got, named_source, named_len) != 0)
        return fail(10, "GL_COMPILE executed ProgramStringARB");
    memset(compile_source, 'X', compile_len);

    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0u);
    glCallList(compile_list);
    if (glGetError() != GL_NO_ERROR)
        return fail(11, "program string list replay");
    {
        const char expected[] = "!!ARBvp1.0\n"
                                "MOV result.position, vertex.position;\n"
                                "MOV result.color, vertex.color.primary;\n"
                                "END";
        const size_t expected_len = strlen(expected);
        if (!query_string(GL_VERTEX_PROGRAM_ARB, got, sizeof(got), &length) ||
            length != (GLint)expected_len || memcmp(got, expected, expected_len) != 0)
            return fail(12, "copied late-bound program string replay");
    }
    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, named);
    if (!query_string(GL_VERTEX_PROGRAM_ARB, got, sizeof(got), &length) ||
        length != (GLint)named_len || memcmp(got, named_source, named_len) != 0)
        return fail(13, "compile-time named object was overwritten");

    execute_list = glGenLists(1);
    if (execute_list == 0u)
        return fail(14, "execute list allocation");
    glNewList(execute_list, GL_COMPILE_AND_EXECUTE);
    glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)execute_len,
                       execute_source);
    glEndList();
    if (!query_string(GL_VERTEX_PROGRAM_ARB, got, sizeof(got), &length) ||
        length != (GLint)execute_len || memcmp(got, execute_source, execute_len) != 0)
        return fail(15, "compile-and-execute immediate load");
    glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)named_len,
                       named_source);
    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0u);
    glCallList(execute_list);
    if (!query_string(GL_VERTEX_PROGRAM_ARB, got, sizeof(got), &length) ||
        length != (GLint)execute_len || memcmp(got, execute_source, execute_len) != 0)
        return fail(16, "compile-and-execute retained replay");

    /* Begin/End legality precedes ordinary argument validation. */
    glBegin(GL_POINTS);
    glBindProgramARB(GL_ARRAY_BUFFER, named);
    if (glGetError() != GL_NO_ERROR)
        return fail(17, "BindProgram GetError during Begin");
    glProgramStringARB(GL_ARRAY_BUFFER, 0u, -1, NULL);
    if (glGetError() != GL_NO_ERROR)
        return fail(18, "ProgramString GetError during Begin");
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION || glGetError() != GL_NO_ERROR)
        return fail(19, "End after ARB program rejection");

    if (mgl_proc_address("glProgramStringARB") == NULL ||
        mgl_proc_address("glGetProgramivARB") == NULL ||
        mgl_proc_address("glGetProgramStringARB") == NULL)
        return fail(20, "proc-address coverage");

    glDeleteLists(compile_list, 1);
    glDeleteLists(execute_list, 1);
    glDeleteProgramsARB(1, &named);
    mgl_context_set_current_for_test(NULL);
    mgl_arb_context_cleanup(&ctx);
    mgl_share_group_release(ctx.share_group);
    puts("mGL ARB program string semantics test: PASS");
    return 0;
}

#include "mgl/gl.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "shader/shader_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(condition)                                                                          \
    do {                                                                                            \
        if (!(condition)) {                                                                         \
            fprintf(stderr, "atomic counter reflection: %s:%d: %s\n", __FILE__, __LINE__,        \
                    #condition);                                                                    \
            abort();                                                                                \
        }                                                                                           \
    } while (0)

static unsigned native_get_program_calls;
static unsigned native_resource_calls;
static GLuint expected_native_program = 77u;
static GLint active_buffers = 1;

static void get_program(GLuint program, GLenum pname, GLint *params) {
    REQUIRE(program == expected_native_program && pname == GL_ACTIVE_ATOMIC_COUNTER_BUFFERS);
    ++native_get_program_calls;
    *params = active_buffers;
}

static void get_resource(GLuint program, GLenum interface, GLuint index, GLsizei prop_count,
                         const GLenum *props, GLsizei buf_size, GLsizei *length, GLint *params) {
    REQUIRE(program == expected_native_program && interface == GL_ATOMIC_COUNTER_BUFFER);
    REQUIRE(index == 0u && prop_count == 1 && props != NULL && params != NULL);
    ++native_resource_calls;
    if (props[0] == GL_ACTIVE_VARIABLES) {
        REQUIRE(buf_size == 3);
        params[0] = 2;
        params[1] = 5;
        params[2] = 9;
        if (length != NULL)
            *length = 3;
        return;
    }
    REQUIRE(buf_size == 1);
    if (length != NULL)
        *length = 1;
    switch (props[0]) {
        case GL_BUFFER_BINDING:
            params[0] = 4;
            break;
        case GL_BUFFER_DATA_SIZE:
            params[0] = 24;
            break;
        case GL_NUM_ACTIVE_VARIABLES:
            params[0] = 3;
            break;
        case GL_REFERENCED_BY_VERTEX_SHADER:
            params[0] = 0;
            break;
        case GL_REFERENCED_BY_TESS_CONTROL_SHADER:
            params[0] = 0;
            break;
        case GL_REFERENCED_BY_TESS_EVALUATION_SHADER:
            params[0] = 1;
            break;
        case GL_REFERENCED_BY_GEOMETRY_SHADER:
            params[0] = 0;
            break;
        case GL_REFERENCED_BY_FRAGMENT_SHADER:
            params[0] = 1;
            break;
        case GL_REFERENCED_BY_COMPUTE_SHADER:
            params[0] = 1;
            break;
        default:
            REQUIRE(0);
            break;
    }
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;

    static const struct {
        GLenum pname;
        GLint expected;
    } cases[] = {{GL_ATOMIC_COUNTER_BUFFER_BINDING, 4},
                 {GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE, 24},
                 {GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTERS, 3},
                 {GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER, 0},
                 {GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_CONTROL_SHADER, 0},
                 {GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_EVALUATION_SHADER, 1},
                 {GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_GEOMETRY_SHADER, 0},
                 {GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_FRAGMENT_SHADER, 1},
                 {GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_COMPUTE_SHADER, 1}};

    GLint values[4];
    size_t i;
    unsigned before;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetProgramiv = get_program;
    native.glesGetProgramResourceiv = get_resource;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    REQUIRE(ctx.share_group != NULL);
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    program = calloc(1u, sizeof(*program));
    REQUIRE(program != NULL);
    program->name = 9u;
    program->native_name = expected_native_program;
    program->linked = GL_TRUE;
    ctx.share_group->programs = program;

    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        values[0] = -17;
        glGetActiveAtomicCounterBufferiv(9u, 0u, cases[i].pname, values);
        REQUIRE(glGetError() == GL_NO_ERROR && values[0] == cases[i].expected);
    }
    values[0] = values[1] = values[2] = values[3] = -17;
    glGetActiveAtomicCounterBufferiv(9u, 0u, GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTER_INDICES,
                                     values);
    REQUIRE(glGetError() == GL_NO_ERROR);
    REQUIRE(values[0] == 2 && values[1] == 5 && values[2] == 9 && values[3] == -17);
    REQUIRE(native_resource_calls == 11u && native_get_program_calls == 10u);

    before = native_resource_calls;
    values[0] = -17;
    glGetActiveAtomicCounterBufferiv(9u, 1u, GL_ATOMIC_COUNTER_BUFFER_BINDING, values);
    REQUIRE(glGetError() == GL_INVALID_VALUE && values[0] == -17 && native_resource_calls == before);
    glGetActiveAtomicCounterBufferiv(9u, 0u, 0xdeadu, values);
    REQUIRE(glGetError() == GL_INVALID_ENUM && values[0] == -17 && native_resource_calls == before);
    glGetActiveAtomicCounterBufferiv(32767u, 0u, GL_ATOMIC_COUNTER_BUFFER_BINDING, values);
    REQUIRE(glGetError() == GL_INVALID_VALUE && values[0] == -17 && native_resource_calls == before);

    program->linked = GL_FALSE;
    glGetActiveAtomicCounterBufferiv(9u, 0u, GL_ATOMIC_COUNTER_BUFFER_BINDING, values);
    REQUIRE(glGetError() == GL_INVALID_VALUE && values[0] == -17 && native_resource_calls == before);
    program->linked = GL_TRUE;

    active_buffers = 0;
    glGetActiveAtomicCounterBufferiv(9u, 0u, GL_ATOMIC_COUNTER_BUFFER_BINDING, values);
    REQUIRE(glGetError() == GL_INVALID_VALUE && values[0] == -17 && native_resource_calls == before);

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL atomic counter reflection: PASS");
    return 0;
}

#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "shader/shader_store.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint next_tf = 100u;
static GLuint native_tf_binding;
static GLuint last_draw_tf;
static GLenum last_draw_mode;
static GLsizei last_draw_instances;
static unsigned draw_calls;
static int has_extension = 1;

static void native_gen_tf(GLsizei n, GLuint *ids) {
    for (GLsizei i = 0; i < n; ++i)
        ids[i] = ++next_tf;
}

static void native_bind_tf(GLenum target, GLuint id) {
    (void)target;
    native_tf_binding = id;
}

static void native_begin_tf(GLenum mode) {
    (void)mode;
}

static void native_end_tf(void) {
}

static void native_draw_tf(GLenum mode, GLuint id) {
    last_draw_mode = mode;
    last_draw_tf = id;
    last_draw_instances = 1;
    ++draw_calls;
}

static void native_draw_tf_instanced(GLenum mode, GLuint id, GLsizei instances) {
    last_draw_mode = mode;
    last_draw_tf = id;
    last_draw_instances = instances;
    ++draw_calls;
}

static const GLubyte *native_get_string(GLenum pname) {
    (void)pname;
    return (const GLubyte *)(has_extension ? "GL_EXT_draw_transform_feedback" : "GL_EXT_other");
}

static void native_use_program(GLuint id) {
    (void)id;
}

static void native_buffer_data(GLenum target, GLsizeiptr size, const void *ptr, GLenum usage) {
    (void)target;
    (void)size;
    (void)ptr;
    (void)usage;
}

static void native_attrib_pointer(GLuint i, GLint n, GLenum t, GLboolean normalized, GLsizei stride,
                                  const void *ptr) {
    (void)i;
    (void)n;
    (void)t;
    (void)normalized;
    (void)stride;
    (void)ptr;
}

static void native_disable_attrib(GLuint index) {
    (void)index;
}

static int check(int result, int line) {
    if (!result)
        fprintf(stderr, "TF replay/stage test failure: %d\n", line);
    return result;
}

#define REQUIRE(x)                                                                                 \
    do {                                                                                           \
        if (!check(!!(x), __LINE__))                                                               \
            return 1;                                                                              \
    } while (0)
#define ERROR_IS(x) REQUIRE(glGetError() == (x))

int main(void) {
    struct mgl_context ctx = {0};
    struct mgl_native_dispatch native = {0};
    struct mgl_program *program;
    GLuint id, unfinished;
    GLint value = -1;
    native.glesGenTransformFeedbacks = native_gen_tf;
    native.glesBindTransformFeedback = native_bind_tf;
    native.glesBeginTransformFeedback = native_begin_tf;
    native.glesEndTransformFeedback = native_end_tf;
    native.glesDrawTransformFeedbackEXT = native_draw_tf;
    native.glesDrawTransformFeedbackInstancedEXT = native_draw_tf_instanced;
    native.glesGetString = native_get_string;
    native.glesUseProgram = native_use_program;
    native.glesBufferData = native_buffer_data;
    native.glesVertexAttribPointer = native_attrib_pointer;
    native.glesDisableVertexAttribArray = native_disable_attrib;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    REQUIRE(ctx.share_group != NULL);
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.native_max_vertex_attribs = 4;
    mgl_context_set_current_for_test(&ctx);

    program = calloc(1u, sizeof(*program));
    REQUIRE(program != NULL);
    program->name = 50u;
    program->native_name = 51u;
    mgl_program_init_metadata(program);
    program->linked = GL_TRUE;
    ctx.share_group->programs = program;
    ctx.current_program = program->name;

    const GLenum stages[] = {GL_VERTEX_SHADER,       GL_FRAGMENT_SHADER,        GL_GEOMETRY_SHADER,
                             GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_COMPUTE_SHADER};
    const GLenum pnames[] = {GL_ACTIVE_SUBROUTINES, GL_ACTIVE_SUBROUTINE_UNIFORMS,
                             GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS,
                             GL_ACTIVE_SUBROUTINE_MAX_LENGTH,
                             GL_ACTIVE_SUBROUTINE_UNIFORM_MAX_LENGTH};
    for (unsigned i = 0; i < sizeof(stages) / sizeof(stages[0]); ++i) {
        for (unsigned j = 0; j < sizeof(pnames) / sizeof(pnames[0]); ++j) {
            value = -123;
            glGetProgramStageiv(program->name, stages[i], pnames[j], &value);
            REQUIRE(value == 0);
            ERROR_IS(GL_NO_ERROR);
        }
    }
    value = 22;
    glGetProgramStageiv(999u, GL_VERTEX_SHADER, GL_ACTIVE_SUBROUTINES, &value);
    ERROR_IS(GL_INVALID_VALUE);
    REQUIRE(value == 22);
    glGetProgramStageiv(program->name, 0xeeeeu, GL_ACTIVE_SUBROUTINES, &value);
    ERROR_IS(GL_INVALID_ENUM);
    glGetProgramStageiv(program->name, GL_VERTEX_SHADER, 0xeeeeu, &value);
    ERROR_IS(GL_INVALID_ENUM);

    glGenTransformFeedbacks(1, &id);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, id);
    unfinished = id;
    REQUIRE(native_tf_binding != 0u);
    glDrawTransformFeedback(GL_POINTS, unfinished);
    ERROR_IS(GL_INVALID_OPERATION);
    glBeginTransformFeedback(GL_POINTS);
    glEndTransformFeedback();

    glDrawTransformFeedback(GL_POINTS, id);
    ERROR_IS(GL_NO_ERROR);
    REQUIRE(draw_calls == 1 && last_draw_tf == native_tf_binding && last_draw_mode == GL_POINTS &&
            last_draw_instances == 1);
    glDrawTransformFeedbackInstanced(GL_TRIANGLES, id, 4);
    ERROR_IS(GL_NO_ERROR);
    REQUIRE(draw_calls == 2 && last_draw_instances == 4 && last_draw_mode == GL_TRIANGLES);
    glDrawTransformFeedbackStream(GL_LINES, id, 0u);
    ERROR_IS(GL_NO_ERROR);
    REQUIRE(draw_calls == 3 && last_draw_mode == GL_LINES);
    glDrawTransformFeedbackStreamInstanced(GL_TRIANGLES, id, 0u, 2);
    ERROR_IS(GL_NO_ERROR);
    REQUIRE(draw_calls == 4 && last_draw_instances == 2);

    glDrawTransformFeedbackStream(GL_POINTS, id, 1u);
    ERROR_IS(GL_INVALID_OPERATION);
    glDrawTransformFeedbackInstanced(GL_POINTS, id, -1);
    ERROR_IS(GL_INVALID_VALUE);
    glDrawTransformFeedback(GL_POINTS, 999u);
    ERROR_IS(GL_INVALID_VALUE);
    has_extension = 0;
    glDrawTransformFeedback(GL_POINTS, id);
    ERROR_IS(GL_INVALID_OPERATION);
    REQUIRE(draw_calls == 4);

    REQUIRE(glXGetProcAddress((const GLubyte *)"glDrawTransformFeedback") != NULL);
    REQUIRE(glXGetProcAddress((const GLubyte *)"glDrawTransformFeedbackInstanced") != NULL);
    REQUIRE(glXGetProcAddress((const GLubyte *)"glDrawTransformFeedbackStreamInstanced") != NULL);
    REQUIRE(glXGetProcAddress((const GLubyte *)"glGetProgramStageiv") != NULL);

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL TF replay/stage query test: PASS");
    return 0;
}

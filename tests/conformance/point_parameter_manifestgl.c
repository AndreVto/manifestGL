#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "core/error.h"
#include "state/state.h"
#include "native/dispatch.h"
#include "shader/shader_store.h"
#include "compat/fixed_function.h"
#include "conformance/point_parameter_cases.h"

#include <stdio.h>
#include <string.h>

static GLint recorded_location;
static GLint recorded_flip;
static unsigned uniform_calls;
static GLint point_size_location;
static GLfloat point_size_value;
static unsigned point_size_calls;

static void record_point_size(GLint location, GLfloat value) {
    point_size_location = location;
    point_size_value = value;
    ++point_size_calls;
}

static void record_uniform(GLint location, GLint value) {
    recorded_location = location;
    recorded_flip = value;
    ++uniform_calls;
}

static int check(int ok, int line) {
    if (!ok)
        fprintf(stderr, "point_parameter:%d\n", line);
    return ok;
}

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!check((x), __LINE__))                                                                 \
            return __LINE__;                                                                       \
    } while (0)

int main(void) {
    struct mgl_context a, b;
    struct mgl_native_dispatch native;
    struct mgl_program program;
    GLint value;
    GLfloat float_value;
    GLdouble double_value;
    GLint upper = (GLint)GL_UPPER_LEFT;
    GLint invalid = -1;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    mgl_error_init(&a.errors);
    mgl_error_init(&b.errors);
    mgl_state_init(&a.canonical_state, 200, 100);
    mgl_state_init(&b.canonical_state, 200, 100);
    mgl_context_set_current_for_test(&a);

    point_parameter_trace();
    mgl_error_init(&a.errors);
    mgl_state_init(&a.canonical_state, 200, 100);

    CHECK(glXGetProcAddress((const GLubyte *)"glPointParameteri") != NULL);
    CHECK(glXGetProcAddress((const GLubyte *)"glPointParameteriv") != NULL);
    glGetIntegerv(GL_POINT_SPRITE_COORD_ORIGIN, &value);
    CHECK(value == (GLint)GL_UPPER_LEFT && glGetError() == GL_NO_ERROR);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, (GLint)GL_LOWER_LEFT);
    CHECK(glGetError() == GL_NO_ERROR);
    glGetIntegerv(GL_POINT_SPRITE_COORD_ORIGIN, &value);
    glGetFloatv(GL_POINT_SPRITE_COORD_ORIGIN, &float_value);
    glGetDoublev(GL_POINT_SPRITE_COORD_ORIGIN, &double_value);
    CHECK(value == (GLint)GL_LOWER_LEFT && float_value == (GLfloat)GL_LOWER_LEFT &&
          double_value == (GLdouble)GL_LOWER_LEFT);

    glPointParameteriv(GL_POINT_SPRITE_COORD_ORIGIN, &upper);
    CHECK(glGetError() == GL_NO_ERROR &&
          a.canonical_state.point_sprite_coord_origin == GL_UPPER_LEFT);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, invalid);
    CHECK(glGetError() == GL_INVALID_VALUE &&
          a.canonical_state.point_sprite_coord_origin == GL_UPPER_LEFT);
    glPointParameteriv(0xDEADu, &upper);
    CHECK(glGetError() == GL_INVALID_ENUM &&
          a.canonical_state.point_sprite_coord_origin == GL_UPPER_LEFT);

    glPushAttrib(GL_POINT_BIT);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, (GLint)GL_LOWER_LEFT);
    glPopAttrib();
    CHECK(glGetError() == GL_NO_ERROR &&
          a.canonical_state.point_sprite_coord_origin == GL_UPPER_LEFT);
    glBegin(GL_POINTS);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, (GLint)GL_LOWER_LEFT);
    glEnd();
    CHECK(glGetError() == GL_INVALID_OPERATION &&
          a.canonical_state.point_sprite_coord_origin == GL_UPPER_LEFT);

    memset(&native, 0, sizeof(native));
    memset(&program, 0, sizeof(program));
    native.glesUniform1i = record_uniform;
    native.glesUniform1f = record_point_size;
    a.native_dispatch = &native;
    mgl_program_init_metadata(&program);
    program.compat_uniform_point_coord_flip = 7;
    uniform_calls = 0u;
    program.compat_uniform_point_size = 9;
    point_size_calls = 0u;
    glPointSize(5.5f);
    CHECK(glGetError() == GL_NO_ERROR);
    mgl_compat_realize_program_uniforms(&a, &program);
    CHECK(uniform_calls == 1u && recorded_location == 7 && recorded_flip == 0);
    CHECK(point_size_calls == 1u && point_size_location == 9 && point_size_value == 5.5f);
    glEnable(GL_PROGRAM_POINT_SIZE);
    CHECK(glIsEnabled(GL_PROGRAM_POINT_SIZE) == GL_TRUE && glGetError() == GL_NO_ERROR);
    glDisable(GL_PROGRAM_POINT_SIZE);
    CHECK(glIsEnabled(GL_PROGRAM_POINT_SIZE) == GL_FALSE && glGetError() == GL_NO_ERROR);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT);
    mgl_compat_realize_program_uniforms(&a, &program);
    CHECK(uniform_calls == 2u && recorded_location == 7 && recorded_flip == 0);
    glEnable(GL_POINT_SPRITE);
    CHECK(glIsEnabled(GL_POINT_SPRITE) && glGetError() == GL_NO_ERROR);
    mgl_compat_realize_program_uniforms(&a, &program);
    CHECK(uniform_calls == 3u && recorded_flip == 1);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, -1);
    CHECK(glGetError() == GL_INVALID_VALUE);
    mgl_compat_realize_program_uniforms(&a, &program);
    CHECK(uniform_calls == 3u && recorded_flip == 1);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_UPPER_LEFT);
    mgl_compat_realize_program_uniforms(&a, &program);
    CHECK(uniform_calls == 4u && recorded_flip == 0);
    glDisable(GL_POINT_SPRITE);
    CHECK(!glIsEnabled(GL_POINT_SPRITE));
    glPushAttrib(GL_POINT_BIT);
    glEnable(GL_POINT_SPRITE);
    glPopAttrib();
    CHECK(!glIsEnabled(GL_POINT_SPRITE) && glGetError() == GL_NO_ERROR);
    program.compat_uniform_point_coord_flip = -1;
    program.compat_uniform_program_point_size = 11;
    uniform_calls = 0u;
    glEnable(GL_PROGRAM_POINT_SIZE);
    mgl_compat_realize_program_uniforms(&a, &program);
    CHECK(uniform_calls == 1u && recorded_location == 11 && recorded_flip == 1);
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_PROGRAM_POINT_SIZE);
    mgl_compat_realize_program_uniforms(&a, &program);
    CHECK(uniform_calls == 2u && recorded_flip == 0);
    glPopAttrib();
    CHECK(glIsEnabled(GL_PROGRAM_POINT_SIZE) == GL_TRUE);
    mgl_compat_realize_program_uniforms(&a, &program);
    CHECK(uniform_calls == 3u && recorded_flip == 1);
    glDisable(GL_PROGRAM_POINT_SIZE);
    a.native_dispatch = NULL;

    mgl_context_set_current_for_test(&b);
    glGetIntegerv(GL_POINT_SPRITE_COORD_ORIGIN, &value);
    CHECK(value == (GLint)GL_UPPER_LEFT && glGetError() == GL_NO_ERROR);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, (GLint)GL_LOWER_LEFT);
    mgl_context_set_current_for_test(&a);
    CHECK(a.canonical_state.point_sprite_coord_origin == GL_UPPER_LEFT &&
          b.canonical_state.point_sprite_coord_origin == GL_LOWER_LEFT);
    mgl_context_set_current_for_test(NULL);
    return 0;
}

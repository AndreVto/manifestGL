#include "mgl/gl.h"
#include "core/context.h"
#include "shader/shader_store.h"

#include <stdio.h>
#include <string.h>

static unsigned delete_calls;
static GLuint last_program;
static GLint link_status = GL_TRUE;
static GLint last_uniform_location = -1;
static GLint last_uniform_value = -1;

static void fake_uniform1i(GLint location, GLint value) {
    last_uniform_location = location;
    last_uniform_value = value;
}

static GLuint fake_create_program(void) {
    return 9001u;
}

static void fake_use_program(GLuint name) {
    last_program = name;
}

static void fake_delete_program(GLuint name) {
    (void)name;
    ++delete_calls;
}

static void fake_link_program(GLuint name) {
    (void)name;
}

static void fake_program_binary(GLuint name, GLenum format, const void *data, GLsizei size) {
    (void)name;
    (void)format;
    (void)data;
    (void)size;
}

static GLint fake_get_uniform_location(GLuint name, const GLchar *text) {
    (void)name;
    (void)text;
    return -1;
}

static GLint fake_get_attrib_location(GLuint name, const GLchar *text) {
    (void)name;
    (void)text;
    return -1;
}

static void fake_get_programiv(GLuint name, GLenum pname, GLint *result) {
    (void)name;
    if (pname == GL_LINK_STATUS)
        *result = link_status;
    else
        *result = 0;
}

static int check(int condition, int step) {
    if (!condition)
        fprintf(stderr, "program-state fast-path failure %d\n", step);
    return condition ? 0 : step;
}

int main(void) {
    struct mgl_context first;
    struct mgl_context second;
    struct mgl_native_dispatch native;
    struct mgl_program *stored;
    const unsigned char binary = 42u;
    GLuint program;
    GLint current = -1;
    int rc;

    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&native, 0, sizeof(native));
    native.glesCreateProgram = fake_create_program;
    native.glesUseProgram = fake_use_program;
    native.glesDeleteProgram = fake_delete_program;
    native.glesProgramBinary = fake_program_binary;
    native.glesLinkProgram = fake_link_program;
    native.glesGetProgramiv = fake_get_programiv;
    native.glesGetUniformLocation = fake_get_uniform_location;
    native.glesUniform1i = fake_uniform1i;
    native.glesGetAttribLocation = fake_get_attrib_location;
    first.native_dispatch = &native;
    first.share_group = mgl_share_group_create();
    if (first.share_group == NULL)
        return check(0, 1);
    mgl_error_init(&first.errors);
    second.native_dispatch = &native;
    second.share_group = first.share_group;
    mgl_share_group_retain(second.share_group);
    mgl_error_init(&second.errors);
    mgl_context_set_current_for_test(&first);

    glUseProgram(0u);
    if ((rc = check(glGetError() == GL_NO_ERROR, 2)))
        return rc;
    program = glCreateProgram();
    if ((rc = check(program != 0u, 3)))
        return rc;
    stored = mgl_program_find(&first, program);
    if ((rc = check(stored != NULL, 4)))
        return rc;
    stored->linked = GL_TRUE;

    glUseProgram(program);
    glGetIntegerv(GL_CURRENT_PROGRAM, &current);
    if ((rc = check(glGetError() == GL_NO_ERROR && current == (GLint)program &&
                        last_program == 9001u, 5)))
        return rc;
    glUseProgram(program);
    if ((rc = check(glGetError() == GL_NO_ERROR, 6)))
        return rc;

    first.immediate.active = GL_TRUE;
    glUseProgram(program);
    first.immediate.active = GL_FALSE;
    if ((rc = check(glGetError() == GL_INVALID_OPERATION, 7)))
        return rc;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current);
    if ((rc = check(current == (GLint)program, 8)))
        return rc;

    mgl_context_set_current_for_test(&second);
    link_status = GL_FALSE;
    glProgramBinary(program, 1u, &binary, 1);
    if ((rc = check(glGetError() == GL_NO_ERROR && !stored->linked, 9)))
        return rc;
    mgl_context_set_current_for_test(&first);
    glUseProgram(program);
    if ((rc = check(glGetError() == GL_INVALID_OPERATION, 10)))
        return rc;

    mgl_context_set_current_for_test(&second);
    link_status = GL_TRUE;
    glProgramBinary(program, 1u, &binary, 1);
    if ((rc = check(glGetError() == GL_NO_ERROR && stored->linked, 11)))
        return rc;
    mgl_context_set_current_for_test(&first);
    glUseProgram(program);
    if ((rc = check(glGetError() == GL_NO_ERROR, 12)))
        return rc;

    stored->compat_attrib[0] = 12;
    stored->active_generic_mask = 4u;
    stored->compat_uniform_sampler0 = 77;
    link_status = GL_FALSE;
    glLinkProgram(program);
    if ((rc = check(glGetError() == GL_NO_ERROR && !stored->linked &&
                    first.current_program_executable, 13)))
        return rc;
    if ((rc = check(stored->compat_attrib[0] == 12 &&
                    stored->active_generic_mask == 4u &&
                    stored->compat_uniform_sampler0 == 77, 23)))
        return rc;
    glUniform1i(3, 42);
    if ((rc = check(glGetError() == GL_NO_ERROR &&
                    last_uniform_location == 3 && last_uniform_value == 42, 24)))
        return rc;
    glUseProgram(program);
    if ((rc = check(glGetError() == GL_INVALID_OPERATION &&
                    first.current_program_executable, 14)))
        return rc;
    link_status = GL_TRUE;
    glLinkProgram(program);
    if ((rc = check(glGetError() == GL_NO_ERROR && stored->linked, 15)))
        return rc;
    glUseProgram(program);
    if ((rc = check(glGetError() == GL_NO_ERROR, 16)))
        return rc;

    glUseProgramObjectARB(0u);
    glGetIntegerv(GL_CURRENT_PROGRAM, &current);
    if ((rc = check(glGetError() == GL_NO_ERROR && current == 0 && last_program == 0u, 17)))
        return rc;
    glUseProgram(program);
    if ((rc = check(glGetError() == GL_NO_ERROR && last_program == 9001u, 18)))
        return rc;

    glDeleteProgram(program);
    if ((rc = check(glGetError() == GL_NO_ERROR && stored->delete_pending &&
                        delete_calls == 0u, 19)))
        return rc;
    glUseProgram(program);
    if ((rc = check(glGetError() == GL_NO_ERROR, 20)))
        return rc;
    glUseProgram(0u);
    if ((rc = check(glGetError() == GL_NO_ERROR && delete_calls == 1u &&
                        mgl_program_find(&first, program) == NULL, 21)))
        return rc;
    glUseProgram(program);
    if ((rc = check(glGetError() == GL_INVALID_VALUE, 22)))
        return rc;

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(second.share_group);
    mgl_share_group_release(first.share_group);
    return 0;
}

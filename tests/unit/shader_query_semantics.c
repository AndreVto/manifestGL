#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "shader/shader_store.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint next_shader_native = 501u;
static GLuint next_program_native = 601u;
static unsigned create_shader_calls;
static unsigned create_program_calls;
static unsigned attach_calls;
static unsigned detach_calls;
static unsigned delete_shader_calls;
static unsigned delete_program_calls;
static unsigned use_program_calls;
static GLuint last_used_program;

static GLuint fake_create_shader(GLenum type) {
    (void)type;
    ++create_shader_calls;
    return next_shader_native++;
}

static GLuint fake_create_program(void) {
    ++create_program_calls;
    return next_program_native++;
}

static void fake_attach_shader(GLuint program, GLuint shader) {
    (void)program;
    (void)shader;
    ++attach_calls;
}

static void fake_detach_shader(GLuint program, GLuint shader) {
    (void)program;
    (void)shader;
    ++detach_calls;
}

static void fake_delete_shader(GLuint shader) {
    (void)shader;
    ++delete_shader_calls;
}

static void fake_delete_program(GLuint program) {
    (void)program;
    ++delete_program_calls;
}

static void fake_use_program(GLuint program) {
    ++use_program_calls;
    last_used_program = program;
}

static char *dup_text(const char *text) {
    size_t n = strlen(text);
    char *out = malloc(n + 1u);
    if (out != NULL)
        memcpy(out, text, n + 1u);
    return out;
}

static int fail(int n, const char *why) {
    fprintf(stderr, "shader query semantics failure %d: %s\n", n, why);
    return n;
}

static int expect_error(GLenum expected, int n, const char *why) {
    GLenum got = glGetError();
    if (got != expected) {
        fprintf(stderr, "shader query semantics failure %d: %s (got 0x%x expected 0x%x)\n", n, why,
                (unsigned)got, (unsigned)expected);
        return n;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_shader *shader_obj;
    struct mgl_program *program_obj;
    GLuint shader, program, list, list_program;
    GLuint lifecycle_list, list_delete_shader, list_delete_program;
    GLuint detach_shader, detach_program;
    const GLchar *parts[2] = {"abc", "DEFghi"};
    GLint lengths[2] = {3, 3};
    const GLchar *list_source = "list-source";
    const GLchar *after_source = "after-list";
    GLint value;
    GLsizei written;
    GLchar text[64];
    GLuint attached = 0u;
    unsigned before_create_shader, before_create_program;
    unsigned before_delete_shader, before_delete_program, before_detach;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesCreateShader = fake_create_shader;
    native.glesCreateProgram = fake_create_program;
    native.glesAttachShader = fake_attach_shader;
    native.glesDetachShader = fake_detach_shader;
    native.glesDeleteShader = fake_delete_shader;
    native.glesDeleteProgram = fake_delete_program;
    native.glesUseProgram = fake_use_program;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 320, 240);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    {
        GLuint geometry = glCreateShader(GL_GEOMETRY_SHADER);
        if (geometry == 0u || (rc = expect_error(GL_NO_ERROR, 51, "geometry shader creation")) != 0)
            return fail(51, "geometry shader creation");
        GLint stage = 0;
        glGetShaderiv(geometry, GL_SHADER_TYPE, &stage);
        if (stage != (GLint)GL_GEOMETRY_SHADER ||
            (rc = expect_error(GL_NO_ERROR, 52, "geometry type query")) != 0)
            return fail(52, "geometry shader type query");
        glDeleteShader(geometry);
    }

    shader = glCreateShader(GL_VERTEX_SHADER);
    program = glCreateProgram();
    if (shader == 0u || program == 0u ||
        (rc = expect_error(GL_NO_ERROR, 2, "object creation")) != 0)
        return fail(2, "object creation");

    value = -1;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &value);
    if ((rc = expect_error(GL_NO_ERROR, 3, "initial shader log length")) != 0 || value != 0)
        return fail(3, "shader without log must report length zero");
    value = -1;
    glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &value);
    if (glGetError() != GL_NO_ERROR || value != 0)
        return fail(4, "initial shader source length");
    value = -1;
    glGetShaderiv(shader, GL_SHADER_TYPE, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)GL_VERTEX_SHADER)
        return fail(5, "shader type query");
    value = -1;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
    if (glGetError() != GL_NO_ERROR || value != GL_FALSE)
        return fail(6, "initial compile status");
    value = -1;
    glGetShaderiv(shader, GL_DELETE_STATUS, &value);
    if (glGetError() != GL_NO_ERROR || value != GL_FALSE)
        return fail(7, "initial delete status");
    value = -1;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &value);
    if (glGetError() != GL_NO_ERROR || value != 0)
        return fail(8, "program without log must report length zero");

    glShaderSource(shader, 2, parts, lengths);
    if ((rc = expect_error(GL_NO_ERROR, 9, "ShaderSource explicit lengths")) != 0)
        return rc;
    value = -1;
    glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &value);
    if (glGetError() != GL_NO_ERROR || value != 7)
        return fail(10, "shader source length includes terminator");
    memset(text, 'x', sizeof(text));
    written = -1;
    glGetShaderSource(shader, 5, &written, text);
    if (glGetError() != GL_NO_ERROR || written != 4 || strcmp(text, "abcD") != 0)
        return fail(11, "GetShaderSource truncation/length");
    memset(text, 0, sizeof(text));
    written = -1;
    glGetShaderSource(shader, (GLsizei)sizeof(text), &written, text);
    if (glGetError() != GL_NO_ERROR || written != 6 || strcmp(text, "abcDEF") != 0)
        return fail(12, "GetShaderSource full copy");

    written = -1;
    strcpy(text, "sentinel");
    glGetShaderInfoLog(shader, (GLsizei)sizeof(text), &written, text);
    if (glGetError() != GL_NO_ERROR || written != 0 || strcmp(text, "") != 0)
        return fail(13, "empty shader info log");
    written = -1;
    strcpy(text, "sentinel");
    glGetProgramInfoLog(program, (GLsizei)sizeof(text), &written, text);
    if (glGetError() != GL_NO_ERROR || written != 0 || strcmp(text, "") != 0)
        return fail(14, "empty program info log");

    (void)pthread_mutex_lock(&ctx.share_group->lock);
    shader_obj = mgl_shader_find(&ctx, shader);
    program_obj = mgl_program_find(&ctx, program);
    if (shader_obj == NULL || program_obj == NULL) {
        (void)pthread_mutex_unlock(&ctx.share_group->lock);
        return fail(15, "object lookup");
    }
    shader_obj->info_log = dup_text("shader-log");
    program_obj->info_log = dup_text("program-log");
    if (shader_obj->info_log == NULL || program_obj->info_log == NULL) {
        (void)pthread_mutex_unlock(&ctx.share_group->lock);
        return fail(16, "log allocation");
    }
    (void)pthread_mutex_unlock(&ctx.share_group->lock);

    value = -1;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &value);
    if (glGetError() != GL_NO_ERROR || value != 11)
        return fail(17, "shader info-log length");
    value = -1;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &value);
    if (glGetError() != GL_NO_ERROR || value != 12)
        return fail(18, "program info-log length");
    memset(text, 0, sizeof(text));
    written = -1;
    glGetShaderInfoLog(shader, 7, &written, text);
    if (glGetError() != GL_NO_ERROR || written != 6 || strcmp(text, "shader") != 0)
        return fail(19, "shader log truncation");
    memset(text, 0, sizeof(text));
    written = -1;
    glGetProgramInfoLog(program, 8, &written, text);
    if (glGetError() != GL_NO_ERROR || written != 7 || strcmp(text, "program") != 0)
        return fail(20, "program log truncation");

    glAttachShader(program, shader);
    if ((rc = expect_error(GL_NO_ERROR, 21, "AttachShader")) != 0 || attach_calls != 1u)
        return fail(21, "attach");
    written = -1;
    attached = 0u;
    glGetAttachedShaders(program, 1, &written, &attached);
    if (glGetError() != GL_NO_ERROR || written != 1 || attached != shader)
        return fail(22, "attached-shader interface name");
    written = -1;
    attached = 0xabcdefu;
    glGetAttachedShaders(program, 0, &written, &attached);
    if (glGetError() != GL_NO_ERROR || written != 0 || attached != 0xabcdefu)
        return fail(23, "zero maxCount attached query");

    /* Begin/End legality must win before ordinary validation and preserve outputs/state. */
    before_create_program = create_program_calls;
    glBegin(GL_POINTS);
    list_program = glCreateProgram();
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 24, "CreateProgram Begin/End")) != 0 ||
        list_program != 0u || create_program_calls != before_create_program)
        return fail(24, "CreateProgram mutated in Begin/End");
    before_create_shader = create_shader_calls;
    glBegin(GL_POINTS);
    list_program = glCreateShader(0xdeadbeefu);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 25, "CreateShader Begin/End ordering")) != 0 ||
        list_program != 0u || create_shader_calls != before_create_shader)
        return fail(25, "CreateShader validation won inside Begin/End");

    glBegin(GL_POINTS);
    glShaderSource(shader, -1, NULL, NULL);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 26, "ShaderSource Begin/End ordering")) != 0)
        return rc;
    value = 0x12345678;
    glBegin(GL_POINTS);
    glGetShaderiv(shader, 0xdeadbeefu, &value);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 27, "GetShaderiv Begin/End ordering")) != 0 ||
        value != 0x12345678)
        return fail(27, "GetShaderiv output mutation in Begin/End");
    written = 77;
    strcpy(text, "unchanged");
    glBegin(GL_POINTS);
    glGetShaderInfoLog(shader, -1, &written, text);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 28, "GetShaderInfoLog Begin/End ordering")) != 0 ||
        written != 77 || strcmp(text, "unchanged") != 0)
        return fail(28, "GetShaderInfoLog output mutation in Begin/End");
    written = 78;
    strcpy(text, "unchanged");
    glBegin(GL_POINTS);
    glGetShaderSource(shader, -1, &written, text);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 29, "GetShaderSource Begin/End ordering")) != 0 ||
        written != 78 || strcmp(text, "unchanged") != 0)
        return fail(29, "GetShaderSource output mutation in Begin/End");
    written = 79;
    strcpy(text, "unchanged");
    glBegin(GL_POINTS);
    glGetProgramInfoLog(program, -1, &written, text);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 30, "GetProgramInfoLog Begin/End ordering")) !=
            0 ||
        written != 79 || strcmp(text, "unchanged") != 0)
        return fail(30, "GetProgramInfoLog output mutation in Begin/End");
    written = 80;
    attached = 0x1234u;
    glBegin(GL_POINTS);
    glGetAttachedShaders(program, -1, &written, &attached);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 31, "GetAttachedShaders Begin/End ordering")) !=
            0 ||
        written != 80 || attached != 0x1234u)
        return fail(31, "GetAttachedShaders output mutation in Begin/End");

    /* Shader/program management and Get queries execute immediately and are not retained in lists.
     */
    list = glGenLists(1);
    if (list == 0u)
        return fail(32, "display-list allocation");
    before_create_program = create_program_calls;
    glNewList(list, GL_COMPILE);
    glShaderSource(shader, 1, &list_source, NULL);
    list_program = glCreateProgram();
    memset(text, 0, sizeof(text));
    written = -1;
    glGetShaderSource(shader, (GLsizei)sizeof(text), &written, text);
    glEndList();
    if ((rc = expect_error(GL_NO_ERROR, 33, "display-list compile")) != 0 ||
        strcmp(text, "list-source") != 0 || written != (GLsizei)strlen("list-source") ||
        list_program == 0u || create_program_calls != before_create_program + 1u)
        return fail(33, "shader/program commands did not execute during list compile");
    glShaderSource(shader, 1, &after_source, NULL);
    if ((rc = expect_error(GL_NO_ERROR, 34, "source after list compile")) != 0)
        return rc;
    before_create_program = create_program_calls;
    glCallList(list);
    if ((rc = expect_error(GL_NO_ERROR, 35, "display-list replay")) != 0 ||
        create_program_calls != before_create_program)
        return fail(35, "CreateProgram was incorrectly retained");
    memset(text, 0, sizeof(text));
    written = -1;
    glGetShaderSource(shader, (GLsizei)sizeof(text), &written, text);
    if (glGetError() != GL_NO_ERROR || strcmp(text, "after-list") != 0)
        return fail(36, "ShaderSource was incorrectly retained");

    if (glXGetProcAddress((const GLubyte *)"glCreateProgram") == NULL ||
        glXGetProcAddress((const GLubyte *)"glShaderSource") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetShaderiv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetShaderInfoLog") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetShaderSource") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetProgramInfoLog") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetAttachedShaders") == NULL ||
        glXGetProcAddress((const GLubyte *)"glDeleteShader") == NULL ||
        glXGetProcAddress((const GLubyte *)"glDetachShader") == NULL ||
        glXGetProcAddress((const GLubyte *)"glDeleteProgram") == NULL)
        return fail(37, "proc-address surface");

    /* Lifecycle commands are illegal in Begin/End, even when ordinary validation would otherwise
     * no-op/fail. */
    glBegin(GL_POINTS);
    glDeleteShader(0u);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 38, "DeleteShader Begin/End ordering")) != 0)
        return rc;
    glBegin(GL_POINTS);
    glDeleteProgram(0u);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 39, "DeleteProgram Begin/End ordering")) != 0)
        return rc;
    glBegin(GL_POINTS);
    glDetachShader(999991u, 999992u);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 40, "DetachShader Begin/End ordering")) != 0)
        return rc;

    /* Delete/detach execute immediately while compiling a display list and are never retained for
     * replay. */
    list_delete_shader = glCreateShader(GL_VERTEX_SHADER);
    list_delete_program = glCreateProgram();
    detach_shader = glCreateShader(GL_FRAGMENT_SHADER);
    detach_program = glCreateProgram();
    if (list_delete_shader == 0u || list_delete_program == 0u || detach_shader == 0u ||
        detach_program == 0u)
        return fail(41, "lifecycle object creation");
    glAttachShader(detach_program, detach_shader);
    if ((rc = expect_error(GL_NO_ERROR, 42, "lifecycle attach")) != 0)
        return rc;
    lifecycle_list = glGenLists(1);
    if (lifecycle_list == 0u)
        return fail(43, "lifecycle list allocation");
    before_delete_shader = delete_shader_calls;
    before_delete_program = delete_program_calls;
    before_detach = detach_calls;
    glNewList(lifecycle_list, GL_COMPILE);
    glDeleteShader(list_delete_shader);
    glDeleteProgram(list_delete_program);
    glDetachShader(detach_program, detach_shader);
    glEndList();
    if ((rc = expect_error(GL_NO_ERROR, 44, "lifecycle list compile")) != 0 ||
        glIsShader(list_delete_shader) != GL_FALSE ||
        glIsProgram(list_delete_program) != GL_FALSE ||
        delete_shader_calls != before_delete_shader + 1u ||
        delete_program_calls != before_delete_program + 1u || detach_calls != before_detach + 1u)
        return fail(44, "lifecycle commands did not execute during list compile");
    value = -1;
    glGetProgramiv(detach_program, GL_ATTACHED_SHADERS, &value);
    if (glGetError() != GL_NO_ERROR || value != 0)
        return fail(45, "DetachShader list compile state");
    glAttachShader(detach_program, detach_shader);
    if ((rc = expect_error(GL_NO_ERROR, 46, "reattach after list compile")) != 0)
        return rc;
    before_delete_shader = delete_shader_calls;
    before_delete_program = delete_program_calls;
    before_detach = detach_calls;
    glCallList(lifecycle_list);
    if ((rc = expect_error(GL_NO_ERROR, 47, "lifecycle list replay")) != 0 ||
        delete_shader_calls != before_delete_shader ||
        delete_program_calls != before_delete_program || detach_calls != before_detach)
        return fail(47, "lifecycle commands were incorrectly retained");
    value = -1;
    glGetProgramiv(detach_program, GL_ATTACHED_SHADERS, &value);
    if (glGetError() != GL_NO_ERROR || value != 1)
        return fail(48, "DetachShader replay changed attachment");
    glDeleteProgram(detach_program);
    glDeleteShader(detach_shader);
    glDeleteLists(lifecycle_list, 1);
    if ((rc = expect_error(GL_NO_ERROR, 49, "lifecycle list cleanup")) != 0)
        return rc;

    before_delete_shader = delete_shader_calls;
    glDeleteShader(shader);
    if ((rc = expect_error(GL_NO_ERROR, 50, "DeleteShader while attached")) != 0 ||
        delete_shader_calls != before_delete_shader)
        return fail(50, "DeleteShader destroyed an attached shader too early");
    value = GL_FALSE;
    glGetShaderiv(shader, GL_DELETE_STATUS, &value);
    if (glGetError() != GL_NO_ERROR || value != GL_TRUE || glIsShader(shader) != GL_TRUE ||
        glGetError() != GL_NO_ERROR || delete_shader_calls != before_delete_shader)
        return fail(51, "delete-pending attached shader visibility");
    written = -1;
    attached = 0u;
    glGetAttachedShaders(program, 1, &written, &attached);
    if (glGetError() != GL_NO_ERROR || written != 1 || attached != shader)
        return fail(52, "delete-pending shader attachment visibility");
    before_detach = detach_calls;
    glDetachShader(program, shader);
    if ((rc = expect_error(GL_NO_ERROR, 53, "DetachShader")) != 0 ||
        detach_calls != before_detach + 1u || glIsShader(shader) != GL_FALSE ||
        delete_shader_calls != before_delete_shader + 1u)
        return fail(53, "deferred shader destruction after detach");

    /* A current program remains a program after DeleteProgram until it is no longer current. */
    (void)pthread_mutex_lock(&ctx.share_group->lock);
    program_obj = mgl_program_find(&ctx, program);
    if (program_obj == NULL) {
        (void)pthread_mutex_unlock(&ctx.share_group->lock);
        return fail(54, "program lookup before current delete");
    }
    program_obj->linked = GL_TRUE;
    (void)pthread_mutex_unlock(&ctx.share_group->lock);
    glUseProgram(program);
    if ((rc = expect_error(GL_NO_ERROR, 55, "UseProgram before delete")) != 0 ||
        use_program_calls == 0u || last_used_program == 0u)
        return fail(55, "current program native binding");
    before_delete_program = delete_program_calls;
    glDeleteProgram(program);
    if ((rc = expect_error(GL_NO_ERROR, 56, "DeleteProgram current")) != 0 ||
        delete_program_calls != before_delete_program)
        return fail(56, "current program destroyed too early");
    value = GL_FALSE;
    glGetProgramiv(program, GL_DELETE_STATUS, &value);
    if (glGetError() != GL_NO_ERROR || value != GL_TRUE || glIsProgram(program) != GL_TRUE ||
        glGetError() != GL_NO_ERROR)
        return fail(57, "delete-pending current program visibility");
    glUseProgram(0u);
    if ((rc = expect_error(GL_NO_ERROR, 58, "release deleted current program")) != 0 ||
        last_used_program != 0u || delete_program_calls != before_delete_program + 1u ||
        glIsProgram(program) != GL_FALSE || glGetError() != GL_NO_ERROR)
        return fail(58, "deferred current program destruction");

    glDeleteProgram(list_program);
    glDeleteLists(list, 1);
    if ((rc = expect_error(GL_NO_ERROR, 59, "cleanup")) != 0)
        return rc;

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL shader query semantics unit test: PASS");
    return 0;
}

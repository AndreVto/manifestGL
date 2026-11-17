#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "shader/shader_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_program;
    GLuint next_texture;
    GLuint next_buffer;
    unsigned draw_calls;
    unsigned tex_buffer_calls;
    unsigned get_uniform_calls;
    unsigned use_program_calls;
    GLint sampler_unit;
    int texture_buffer_extension;
} cap;

static const GLubyte *fake_get_string(GLenum name) {
    static const GLubyte with_ext[] = "GL_EXT_texture_buffer";
    static const GLubyte without_ext[] = "";
    if (name == GL_EXTENSIONS)
        return cap.texture_buffer_extension ? with_ext : without_ext;
    return (const GLubyte *)"";
}

static GLuint fake_create_program(void) {
    return ++cap.next_program;
}

static void fake_delete_program(GLuint program) {
    (void)program;
}

static void fake_link_program(GLuint program) {
    (void)program;
}

static void fake_use_program(GLuint program) {
    (void)program;
    ++cap.use_program_calls;
}

static void fake_get_programiv(GLuint program, GLenum pname, GLint *value) {
    (void)program;
    switch (pname) {
        case GL_LINK_STATUS:
            *value = GL_TRUE;
            break;
        case GL_INFO_LOG_LENGTH:
            *value = 1;
            break;
        case GL_ACTIVE_UNIFORMS:
            *value = 1;
            break;
        case GL_ACTIVE_UNIFORM_MAX_LENGTH:
            *value = 8;
            break;
        case GL_ACTIVE_ATTRIBUTES:
            *value = 0;
            break;
        case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
            *value = 0;
            break;
        default:
            *value = 0;
            break;
    }
}

static void fake_get_active_uniform(GLuint program, GLuint index, GLsizei buf_size, GLsizei *length,
                                    GLint *size, GLenum *type, GLchar *name) {
    static const char uniform_name[] = "tb";
    size_t n = sizeof(uniform_name) - 1u;
    (void)program;
    if (index != 0u || buf_size <= 0) {
        if (length != NULL)
            *length = 0;
        if (size != NULL)
            *size = 0;
        if (type != NULL)
            *type = 0u;
        if (buf_size > 0 && name != NULL)
            name[0] = '\0';
        return;
    }
    if (n >= (size_t)buf_size)
        n = (size_t)buf_size - 1u;
    memcpy(name, uniform_name, n);
    name[n] = '\0';
    if (length != NULL)
        *length = (GLsizei)n;
    if (size != NULL)
        *size = 1;
    if (type != NULL)
        *type = GL_SAMPLER_BUFFER;
}

static GLint fake_get_uniform_location(GLuint program, const GLchar *name) {
    (void)program;
    return name != NULL && strcmp(name, "tb") == 0 ? 4 : -1;
}

static GLint fake_get_attrib_location(GLuint program, const GLchar *name) {
    (void)program;
    (void)name;
    return -1;
}

static void fake_get_uniformiv(GLuint program, GLint location, GLint *value) {
    (void)program;
    ++cap.get_uniform_calls;
    *value = location == 4 ? cap.sampler_unit : 0;
}

static void fake_gen_textures(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_texture;
}

static void fake_delete_textures(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_active_texture(GLenum texture) {
    (void)texture;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    (void)target;
    (void)texture;
}

static void fake_tex_buffer(GLenum target, GLenum internalformat, GLuint buffer) {
    (void)target;
    (void)internalformat;
    (void)buffer;
    ++cap.tex_buffer_calls;
}

static void fake_gen_buffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_buffer;
}

static void fake_delete_buffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_buffer(GLenum target, GLuint buffer) {
    (void)target;
    (void)buffer;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)target;
    (void)size;
    (void)data;
    (void)usage;
}

static void fake_vertex_attrib_pointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                       GLsizei stride, const void *pointer) {
    (void)index;
    (void)size;
    (void)type;
    (void)normalized;
    (void)stride;
    (void)pointer;
}

static void fake_disable_vertex_attrib(GLuint index) {
    (void)index;
}

static void fake_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    (void)mode;
    (void)first;
    (void)count;
    ++cap.draw_calls;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "texture buffer runtime failure %d: %s\n", code, message);
    return code;
}

static int expect_error(struct mgl_context *ctx, GLenum expected, int code, const char *message) {
    GLenum actual = mgl_error_take(&ctx->errors);
    if (actual != expected) {
        fprintf(stderr, "texture buffer runtime failure %d: %s (got 0x%04x expected 0x%04x)\n",
                code, message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLuint program_name;
    GLuint texture;
    GLuint buffer;
    unsigned char storage[64];
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    memset(storage, 0x5a, sizeof(storage));

    cap.texture_buffer_extension = 0;
    cap.sampler_unit = 0;

    native.glesGetString = fake_get_string;
    native.glesCreateProgram = fake_create_program;
    native.glesDeleteProgram = fake_delete_program;
    native.glesLinkProgram = fake_link_program;
    native.glesUseProgram = fake_use_program;
    native.glesGetProgramiv = fake_get_programiv;
    native.glesGetActiveUniform = fake_get_active_uniform;
    native.glesGetUniformLocation = fake_get_uniform_location;
    native.glesGetAttribLocation = fake_get_attrib_location;
    native.glesGetUniformiv = fake_get_uniformiv;
    native.glesGenTextures = fake_gen_textures;
    native.glesDeleteTextures = fake_delete_textures;
    native.glesActiveTexture = fake_active_texture;
    native.glesBindTexture = fake_bind_texture;
    native.glesTexBufferEXT = fake_tex_buffer;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesVertexAttribPointer = fake_vertex_attrib_pointer;
    native.glesDisableVertexAttribArray = fake_disable_vertex_attrib;
    native.glesDrawArrays = fake_draw_arrays;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.native_max_vertex_attribs = 1;
    mgl_context_set_current_for_test(&ctx);

    program_name = glCreateProgram();
    if (program_name == 0u)
        return fail(2, "program creation");
    glLinkProgram(program_name);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 3, "program link")) != 0)
        return rc;
    program = mgl_program_find(&ctx, program_name);
    if (program == NULL || !program->linked)
        return fail(4, "linked program state");
    if (program->buffer_sampler_reflection_failed || program->buffer_sampler_uniform_count != 1u)
        return fail(5, "buffer sampler reflection count");
    if (program->buffer_sampler_uniforms[0].location != 4 ||
        program->buffer_sampler_uniforms[0].type != GL_SAMPLER_BUFFER)
        return fail(6, "buffer sampler reflected metadata");

    glUseProgram(program_name);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 7, "use program")) != 0)
        return rc;

    glGenBuffers(1, &buffer);
    glBindBuffer(GL_TEXTURE_BUFFER, buffer);
    glBufferData(GL_TEXTURE_BUFFER, (GLsizeiptr)sizeof(storage), storage, GL_STATIC_DRAW);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 8, "buffer storage")) != 0)
        return rc;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_BUFFER, texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R16, buffer);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 9, "desktop R16 attachment")) != 0)
        return rc;
    if (cap.tex_buffer_calls != 0u)
        return fail(10, "desktop-only R16 was forwarded to GLES EXT");

    glDrawArrays(GL_POINTS, 0, 1);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 11, "R16 draw realization rejection")) != 0)
        return rc;
    if (cap.draw_calls != 0u)
        return fail(12, "rejected draw reached native");
    if (cap.get_uniform_calls != 1u)
        return fail(13, "buffer sampler unit was not queried");

    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, buffer);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 14, "native RGBA8 attachment")) != 0)
        return rc;
    if (cap.tex_buffer_calls != 1u)
        return fail(15, "native texture buffer attachment was not lowered");
    glDrawArrays(GL_POINTS, 0, 1);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 16, "native TBO draw")) != 0)
        return rc;
    if (cap.draw_calls != 1u)
        return fail(17, "native TBO draw did not reach native");
    if (cap.get_uniform_calls != 2u)
        return fail(18, "native draw did not resolve sampler unit");

    cap.texture_buffer_extension = 0;
    glDrawArrays(GL_POINTS, 0, 1);
    if ((rc = expect_error(&ctx, GL_NO_ERROR, 21,
                           "GLES core texture-buffer draw without extension string")) != 0)
        return rc;
    if (cap.draw_calls != 2u)
        return fail(22, "core texture-buffer draw not forwarded");
    native.glesTexBufferEXT = NULL;
    glDrawArrays(GL_POINTS, 0, 1);
    if ((rc = expect_error(&ctx, GL_INVALID_OPERATION, 19,
                           "missing native texture-buffer entry point draw rejection")) != 0)
        return rc;
    if (cap.draw_calls != 2u)
        return fail(20, "unsupported texture-buffer draw reached native");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL texture buffer runtime test: PASS");
    return 0;
}

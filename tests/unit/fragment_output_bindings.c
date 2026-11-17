#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/objects.h"
#include "objects/object_store.h"
#include "compiler/spirv_toolchain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct output_binding_capture {
    char name[32];
    GLuint color;
    GLuint index;
};

struct capture_state {
    struct output_binding_capture bindings[8];
    unsigned binding_count;
    int has_output_extension;
    unsigned link_calls;
    unsigned create_shader_calls;
    unsigned shader_compile_calls;
    unsigned detach_calls;
    unsigned attach_calls;
    unsigned delete_shader_calls;
    unsigned native_index_calls;
    char shader_source[2048];
} cap;

static void store_binding(GLuint color, GLuint index, const GLchar *name) {
    unsigned i;
    for (i = 0u; i < cap.binding_count; ++i) {
        if (strcmp(cap.bindings[i].name, name) == 0) {
            cap.bindings[i].color = color;
            cap.bindings[i].index = index;
            return;
        }
    }
    if (cap.binding_count < sizeof(cap.bindings) / sizeof(cap.bindings[0])) {
        struct output_binding_capture *binding = &cap.bindings[cap.binding_count++];
        (void)snprintf(binding->name, sizeof(binding->name), "%s", name);
        binding->color = color;
        binding->index = index;
    }
}

static const GLubyte *fake_get_string(GLenum name) {
    if (name != GL_EXTENSIONS)
        return NULL;
    return (const GLubyte *)(cap.has_output_extension ? "GL_EXT_blend_func_extended" : "");
}

static void fake_bind_frag_data_location(GLuint program, GLuint color, const GLchar *name) {
    (void)program;
    store_binding(color, 0u, name);
}

static void fake_bind_frag_data_location_indexed(GLuint program, GLuint color, GLuint index,
                                                 const GLchar *name) {
    (void)program;
    store_binding(color, index, name);
}

static void fake_link_program(GLuint program) {
    (void)program;
    ++cap.link_calls;
}

static GLuint fake_create_shader(GLenum type) {
    if (type != GL_FRAGMENT_SHADER)
        return 0u;
    ++cap.create_shader_calls;
    return 999u;
}

static void fake_shader_source(GLuint shader, GLsizei count, const GLchar *const *strings,
                               const GLint *lengths) {
    (void)lengths;
    if (shader == 999u && count == 1 && strings != NULL && strings[0] != NULL)
        (void)snprintf(cap.shader_source, sizeof(cap.shader_source), "%s", strings[0]);
}

static void fake_compile_shader(GLuint shader) {
    if (shader == 999u)
        ++cap.shader_compile_calls;
}

static void fake_get_shaderiv(GLuint shader, GLenum pname, GLint *value) {
    if (shader == 999u && pname == GL_COMPILE_STATUS)
        *value = GL_TRUE;
    else
        *value = 0;
}

static void fake_detach_shader(GLuint program, GLuint shader) {
    if (program == 80u && (shader == 800u || shader == 999u))
        ++cap.detach_calls;
}

static void fake_attach_shader(GLuint program, GLuint shader) {
    if (program == 80u && (shader == 800u || shader == 999u))
        ++cap.attach_calls;
}

static void fake_delete_shader(GLuint shader) {
    if (shader == 999u)
        ++cap.delete_shader_calls;
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

static void fake_get_program_info_log(GLuint program, GLsizei size, GLsizei *length, GLchar *log) {
    (void)program;
    if (length != NULL)
        *length = 0;
    if (size > 0 && log != NULL)
        log[0] = '\0';
}

static GLint fake_get_attrib_location(GLuint program, const GLchar *name) {
    (void)program;
    (void)name;
    return -1;
}

static GLint fake_get_uniform_location(GLuint program, const GLchar *name) {
    (void)program;
    (void)name;
    return -1;
}

static GLint fake_get_frag_data_location(GLuint program, const GLchar *name) {
    unsigned i;
    (void)program;
    for (i = 0u; i < cap.binding_count; ++i)
        if (strcmp(cap.bindings[i].name, name) == 0)
            return (GLint)cap.bindings[i].color;
    return -1;
}

static void fake_get_output_interface(GLuint program, GLenum interface, GLenum pname,
                                      GLint *params) {
    (void)program;
    if (interface == GL_PROGRAM_OUTPUT && pname == GL_ACTIVE_RESOURCES)
        *params = 2;
}

static void fake_get_output_resourceiv(GLuint program, GLenum interface, GLuint index,
                                       GLsizei prop_count, const GLenum *props,
                                       GLsizei buf_size, GLsizei *length, GLint *params) {
    const char *name = index == 0u ? "primary" : "secondary";
    (void)program;
    if (interface == GL_PROGRAM_OUTPUT && prop_count == 1 && props[0] == GL_NAME_LENGTH &&
        buf_size == 1) {
        *params = (GLint)strlen(name) + 1;
        if (length != NULL)
            *length = 1;
    }
}

static void fake_get_output_resource_name(GLuint program, GLenum interface, GLuint index,
                                          GLsizei buf_size, GLsizei *length, GLchar *name) {
    (void)program;
    if (interface == GL_PROGRAM_OUTPUT) {
        const char *source = index == 0u ? "primary" : "secondary";
        size_t n = strlen(source);
        if (buf_size > 0 && name != NULL) {
            size_t count = n < (size_t)buf_size ? n : (size_t)buf_size - 1u;
            memcpy(name, source, count);
            name[count] = '\0';
            if (length != NULL)
                *length = (GLsizei)count;
        }
    }
}

static GLint fake_get_resource_location_index(GLuint program, GLenum interface,
                                              const GLchar *name) {
    unsigned i;
    ++cap.native_index_calls;
    (void)program;
    if (interface != GL_PROGRAM_OUTPUT)
        return -1;
    for (i = 0u; i < cap.binding_count; ++i)
        if (strcmp(cap.bindings[i].name, name) == 0)
            return (GLint)cap.bindings[i].index;
    return -1;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "fragment-output binding failure %d: %s\n", code, message);
    return code;
}

static int expect_error(GLenum expected, int code, const char *message) {
    GLenum actual = glGetError();
    if (actual != expected) {
        fprintf(stderr, "fragment-output binding failure %d: %s (got 0x%04x expected 0x%04x)\n",
                code, message, (unsigned)actual, (unsigned)expected);
        return code;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    struct mgl_program *fallback_program;
    struct mgl_shader fallback_fragment;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.has_output_extension = 1;
    native.glesGetString = fake_get_string;
    native.glesBindFragDataLocationEXT = fake_bind_frag_data_location;
    native.glesBindFragDataLocationIndexedEXT = fake_bind_frag_data_location_indexed;
    native.glesLinkProgram = fake_link_program;
    native.glesGetProgramiv = fake_get_programiv;
    native.glesGetProgramInfoLog = fake_get_program_info_log;
    native.glesGetAttribLocation = fake_get_attrib_location;
    native.glesGetUniformLocation = fake_get_uniform_location;
    native.glesGetFragDataLocation = fake_get_frag_data_location;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    program = calloc(1u, sizeof(*program));
    if (program == NULL)
        return fail(2, "program allocation");
    program->name = 7u;
    program->native_name = 70u;
    program->next = ctx.share_group->programs;
    ctx.share_group->programs = program;

    glBindFragDataLocation(program->name, 2u, "primary");
    glBindFragDataLocationIndexedEXT(program->name, 0u, 1u, "secondary");
    if ((rc = expect_error(GL_NO_ERROR, 3, "initial binding")) != 0)
        return rc;
    if (cap.binding_count != 0u)
        return fail(4, "binding must be deferred until link");

    glLinkProgram(program->name);
    if ((rc = expect_error(GL_NO_ERROR, 5, "link")) != 0)
        return rc;
    if (!program->linked || cap.link_calls != 1u || cap.binding_count != 2u)
        return fail(6, "link-time binding application");
    if (glGetFragDataLocation(program->name, "primary") != 2)
        return fail(7, "primary location query");
    if (glGetFragDataIndexEXT(program->name, "secondary") != 1)
        return fail(8, "tracked secondary index fallback");

    native.glesGetProgramInterfaceiv = fake_get_output_interface;
    native.glesGetProgramResourceiv = fake_get_output_resourceiv;
    native.glesGetProgramResourceName = fake_get_output_resource_name;
    cap.has_output_extension = 0;
    {
        const GLenum property = GL_LOCATION_INDEX;
        GLint result = -1;
        GLsizei written = 0;
        glGetProgramResourceiv(program->name, GL_PROGRAM_OUTPUT, 1u, 1,
                               &property, 1, &written, &result);
        if (written != 1 || result != 1 ||
            (rc = expect_error(GL_NO_ERROR, 26, "indexed output resource fallback")) != 0)
            return rc != 0 ? rc : 26;
    }
    cap.has_output_extension = 1;

    glBindFragDataLocationEXT(program->name, 3u, "primary");
    if (glGetFragDataLocation(program->name, "primary") != 2)
        return fail(9, "post-link bind changed native too early");
    if (glGetFragDataIndex(program->name, "primary") != 0)
        return fail(10, "last-linked index changed before relink");

    native.glesGetProgramResourceLocationIndex = fake_get_resource_location_index;
    cap.has_output_extension = 0;
    if (glGetProgramResourceLocationIndexEXT(program->name, GL_PROGRAM_OUTPUT, "secondary") != 1 ||
        cap.native_index_calls != 0u || glGetFragDataIndex(program->name, "secondary") != 1 ||
        cap.native_index_calls != 0u)
        return fail(40, "native index symbol without extension must use fallback");
    cap.has_output_extension = 1;
    if (glGetProgramResourceLocationIndexEXT(program->name, GL_PROGRAM_OUTPUT, "secondary") != 1 ||
        cap.native_index_calls != 1u)
        return fail(41, "native index query requires advertised extension");
    if (glGetProgramResourceLocationIndexEXT(program->name, GL_PROGRAM_OUTPUT, "secondary") != 1)
        return fail(11, "EXT resource-location-index alias");
    if (glGetFragDataIndex(program->name, "secondary") != 1)
        return fail(12, "resource-index query path");
    native.glesGetProgramResourceLocationIndex = NULL;

    glLinkProgram(program->name);
    if ((rc = expect_error(GL_NO_ERROR, 13, "relink after pending rebind")) != 0)
        return rc;
    if (!program->linked || cap.link_calls != 2u ||
        glGetFragDataLocation(program->name, "primary") != 3)
        return fail(14, "relink did not commit pending fragment-output binding");

    glBindFragDataLocation(program->name, MGL_MAX_DRAW_BUFFERS, "badColor");
    if ((rc = expect_error(GL_INVALID_VALUE, 16, "draw-buffer limit validation")) != 0)
        return rc;
    glBindFragDataLocationIndexed(program->name, MGL_MAX_DUAL_SOURCE_DRAW_BUFFERS, 1u,
                                  "badDualColor");
    if ((rc = expect_error(GL_INVALID_VALUE, 17, "dual-source color limit validation")) != 0)
        return rc;
    glBindFragDataLocationIndexed(program->name, 0u, 2u, "badIndex");
    if ((rc = expect_error(GL_INVALID_VALUE, 18, "index validation")) != 0)
        return rc;
    glBindFragDataLocation(program->name, 0u, "gl_reserved");
    if ((rc = expect_error(GL_INVALID_OPERATION, 19, "reserved-name validation")) != 0)
        return rc;
    glBindFragDataLocation(999u, 0u, "missing");
    if ((rc = expect_error(GL_INVALID_VALUE, 20, "program validation")) != 0)
        return rc;

    native.glesBindFragDataLocationIndexedEXT = NULL;
    glLinkProgram(program->name);
    if (program->linked || cap.link_calls != 2u)
        return fail(21, "missing indexed native support must fail link before native link");
    if (program->info_log == NULL || strstr(program->info_log, "fragment-output binding") == NULL)
        return fail(22, "missing-native link log");
    if (glGetFragDataLocation(program->name, "primary") != -1 ||
        (rc = expect_error(GL_INVALID_OPERATION, 23, "query after failed link")) != 0)
        return rc != 0 ? rc : 23;

    fallback_program = calloc(1u, sizeof(*fallback_program));
    if (fallback_program == NULL)
        return fail(25, "fallback program allocation");
    fallback_program->name = 8u;
    fallback_program->native_name = 80u;
    fallback_program->next = ctx.share_group->programs;
    ctx.share_group->programs = fallback_program;
    memset(&fallback_fragment, 0, sizeof(fallback_fragment));
    fallback_fragment.type = GL_FRAGMENT_SHADER;
    fallback_fragment.native_name = 800u;
    fallback_fragment.compiled = GL_TRUE;
    fallback_fragment.translated_source =
        "#version 320 es\nprecision mediump float;\nout vec4 fragment_color;\n"
        "out vec4 secondary_color;\n"
        "void main(){fragment_color=vec4(1.0);secondary_color=vec4(0.5); }\n";
    fallback_program->attached[0] = &fallback_fragment;
    fallback_program->attached_count = 1u;
    /* A native function pointer is unusable unless its extension is advertised. */
    cap.has_output_extension = 0;
    native.glesBindFragDataLocationEXT = fake_bind_frag_data_location;
    native.glesCreateShader = fake_create_shader;
    native.glesShaderSource = fake_shader_source;
    native.glesCompileShader = fake_compile_shader;
    native.glesGetShaderiv = fake_get_shaderiv;
    native.glesDetachShader = fake_detach_shader;
    native.glesAttachShader = fake_attach_shader;
    native.glesDeleteShader = fake_delete_shader;

    glBindFragDataLocation(fallback_program->name, 2u, "fragment_color");
    if ((rc = expect_error(GL_NO_ERROR, 26, "output binding")) != 0)
        return rc;
    glLinkProgram(fallback_program->name);
    if ((rc = expect_error(GL_NO_ERROR, 27, "output link")) != 0)
        return rc;
    if (!fallback_program->linked ||
        !mgl_validate_gles_glsl(GL_FRAGMENT_SHADER, cap.shader_source) ||
        strstr(cap.shader_source, "layout(location = 2) out mediump vec4 fragment_color;") == NULL ||
        strstr(cap.shader_source, "layout(location = 0) out mediump vec4 secondary_color;") == NULL ||
        cap.create_shader_calls != 1u || cap.shader_compile_calls != 1u || cap.attach_calls != 2u ||
        cap.detach_calls != 2u || cap.delete_shader_calls != 1u ||
        fallback_program->attached[0] != &fallback_fragment ||
        strstr(fallback_fragment.translated_source, "layout(location") != NULL)
        return fail(28, "explicit-output link and shader restoration");
    if (cap.link_calls != 3u)
        return fail(29, "program was not passed to native link");

    cap.has_output_extension = 1;
    native.glesBindFragDataLocationIndexedEXT = fake_bind_frag_data_location_indexed;
    glBindFragDataLocationIndexed(fallback_program->name, 0u, 1u, "secondary_color");
    if ((rc = expect_error(GL_NO_ERROR, 30, "dual-source output binding")) != 0)
        return rc;
    glLinkProgram(fallback_program->name);
    if ((rc = expect_error(GL_NO_ERROR, 31, "dual-source output link")) != 0)
        return rc;
    if (!fallback_program->linked || cap.link_calls != 4u ||
        !mgl_validate_gles_glsl(GL_FRAGMENT_SHADER, cap.shader_source) ||
        strstr(cap.shader_source, "#extension GL_EXT_blend_func_extended : require") == NULL ||
        strstr(cap.shader_source, "layout(location = 2) out mediump vec4 fragment_color;") == NULL ||
        strstr(cap.shader_source,
               "layout(location = 0, index = 1) out mediump vec4 secondary_color;") == NULL ||
        cap.shader_compile_calls != 2u || cap.attach_calls != 4u || cap.detach_calls != 4u ||
        cap.delete_shader_calls != 2u ||
        fallback_program->attached[0] != &fallback_fragment)
        return fail(32, "dual-source output location/index and shader restoration");

    if (glXGetProcAddress((const GLubyte *)"glBindFragDataLocation") == NULL ||
        glXGetProcAddress((const GLubyte *)"glBindFragDataLocationIndexed") == NULL ||
        glXGetProcAddress((const GLubyte *)"glBindFragDataLocationEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glBindFragDataLocationIndexedEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetFragDataLocation") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetFragDataIndex") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetFragDataIndexEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetProgramResourceLocationIndexEXT") == NULL)
        return fail(24, "proc-address coverage");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL fragment-output binding unit test: PASS");
    return 0;
}

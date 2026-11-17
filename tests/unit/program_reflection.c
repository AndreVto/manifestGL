#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/objects.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint last_program;
    GLenum last_program_pname;
    GLuint uniform_binding_block;
    GLuint uniform_binding_point;
    GLuint storage_binding_block;
    GLuint storage_binding_point;
    unsigned native_resource_calls;
    unsigned native_location_index_property_calls;
    unsigned native_offset_property_calls;
    int has_blend_extension;
} cap;

static void copy_name(const char *src, GLsizei buf_size, GLsizei *length, GLchar *dst) {
    size_t n = strlen(src);
    size_t c = n;
    if (buf_size > 0 && c >= (size_t)buf_size)
        c = (size_t)buf_size - 1u;
    if (dst != NULL && buf_size > 0) {
        memcpy(dst, src, c);
        dst[c] = '\0';
    }
    if (length != NULL)
        *length = (GLsizei)c;
}

static void fake_get_programiv(GLuint program, GLenum pname, GLint *params) {
    cap.last_program = program;
    cap.last_program_pname = pname;
    switch (pname) {
        case GL_ACTIVE_UNIFORM_BLOCKS:
            *params = 3;
            break;
        case GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH:
            *params = 16;
            break;
        case GL_ACTIVE_ATOMIC_COUNTER_BUFFERS:
            *params = 2;
            break;
        case GL_COMPUTE_WORK_GROUP_SIZE:
            params[0] = 4;
            params[1] = 2;
            params[2] = 1;
            break;
        case GL_GEOMETRY_VERTICES_OUT:
            *params = 8;
            break;
        case GL_GEOMETRY_INPUT_TYPE:
            *params = GL_LINES;
            break;
        case GL_GEOMETRY_OUTPUT_TYPE:
            *params = GL_TRIANGLE_STRIP;
            break;
        case GL_GEOMETRY_SHADER_INVOCATIONS:
            *params = 3;
            break;
        case GL_TESS_CONTROL_OUTPUT_VERTICES:
            *params = 4;
            break;
        case GL_TESS_GEN_MODE:
            *params = GL_TRIANGLES;
            break;
        case GL_TESS_GEN_SPACING:
            *params = GL_EQUAL;
            break;
        case GL_TESS_GEN_VERTEX_ORDER:
            *params = GL_CCW;
            break;
        case GL_TESS_GEN_POINT_MODE:
            *params = GL_TRUE;
            break;
    }
}

static void fake_active_attrib(GLuint program, GLuint index, GLsizei buf_size, GLsizei *length,
                               GLint *size, GLenum *type, GLchar *name) {
    (void)index;
    cap.last_program = program;
    if (size)
        *size = 1;
    if (type)
        *type = GL_FLOAT;
    copy_name("position", buf_size, length, name);
}

static void fake_active_uniform(GLuint program, GLuint index, GLsizei buf_size, GLsizei *length,
                                GLint *size, GLenum *type, GLchar *name) {
    (void)index;
    cap.last_program = program;
    if (size)
        *size = 1;
    if (type)
        *type = GL_FLOAT;
    copy_name("scale", buf_size, length, name);
}

static void fake_uniform_indices(GLuint program, GLsizei count, const GLchar *const *names,
                                 GLuint *indices) {
    GLsizei i;
    cap.last_program = program;
    for (i = 0; i < count; ++i)
        indices[i] = strcmp(names[i], "scale") == 0 ? 3u : GL_INVALID_INDEX;
}

static void fake_active_uniforms(GLuint program, GLsizei count, const GLuint *indices, GLenum pname,
                                 GLint *params) {
    GLsizei i;
    (void)indices;
    cap.last_program = program;
    for (i = 0; i < count; ++i)
        params[i] = pname == GL_UNIFORM_OFFSET ? 64 : 1;
}

static GLuint fake_uniform_block_index(GLuint program, const GLchar *name) {
    cap.last_program = program;
    return strcmp(name, "Globals") == 0 ? 2u : GL_INVALID_INDEX;
}

static void fake_uniform_block_iv(GLuint program, GLuint index, GLenum pname, GLint *params) {
    (void)index;
    cap.last_program = program;
    *params = pname == GL_UNIFORM_BLOCK_DATA_SIZE ? 128 : 4;
}

static void fake_uniform_block_name(GLuint program, GLuint index, GLsizei buf_size, GLsizei *length,
                                    GLchar *name) {
    (void)index;
    cap.last_program = program;
    copy_name("Globals", buf_size, length, name);
}

static void fake_uniform_block_binding(GLuint program, GLuint block, GLuint binding) {
    cap.last_program = program;
    cap.uniform_binding_block = block;
    cap.uniform_binding_point = binding;
}

static void fake_storage_block_binding(GLuint program, GLuint block, GLuint binding) {
    cap.last_program = program;
    cap.storage_binding_block = block;
    cap.storage_binding_point = binding;
}

static const GLubyte *fake_extensions(GLenum name) {
    if (name != GL_EXTENSIONS)
        return NULL;
    return (const GLubyte *)(cap.has_blend_extension ? "GL_EXT_blend_func_extended" : "");
}

static void fake_program_interface(GLuint program, GLenum interface, GLenum pname, GLint *params) {
    cap.last_program = program;
    (void)interface;
    *params = pname == GL_ACTIVE_RESOURCES ? 4 : 16;
}

static GLuint fake_resource_index(GLuint program, GLenum interface, const GLchar *name) {
    cap.last_program = program;
    (void)interface;
    return strcmp(name, "values") == 0 ? 1u : GL_INVALID_INDEX;
}

static void fake_resource_name(GLuint program, GLenum interface, GLuint index, GLsizei buf_size,
                               GLsizei *length, GLchar *name) {
    cap.last_program = program;
    (void)interface;
    (void)index;
    copy_name(interface == GL_PROGRAM_OUTPUT ? "color" : "values", buf_size, length, name);
}

static void fake_resource_iv(GLuint program, GLenum interface, GLuint index, GLsizei prop_count,
                             const GLenum *props, GLsizei buf_size, GLsizei *length,
                             GLint *params) {
    GLsizei i, n = prop_count < buf_size ? prop_count : buf_size;
    cap.last_program = program;
    ++cap.native_resource_calls;
    (void)index;
    for (i = 0; i < n; ++i) {
        if (props[i] == GL_LOCATION_INDEX)
            ++cap.native_location_index_property_calls;
        if (props[i] == GL_OFFSET)
            ++cap.native_offset_property_calls;
        params[i] = props[i] == GL_LOCATION ? 7
                    : props[i] == GL_TYPE ? (GLint)GL_FLOAT
                    : props[i] == GL_NAME_LENGTH ? (interface == GL_PROGRAM_OUTPUT ? 6 : 7)
                    : 1;
    }
    if (length)
        *length = n;
}

static GLint fake_resource_location(GLuint program, GLenum interface, const GLchar *name) {
    cap.last_program = program;
    (void)interface;
    return strcmp(name, "value") == 0 ? 7 : (strcmp(name, "color") == 0 ? 4 : -1);
}

static GLint fake_resource_location_index(GLuint program, GLenum interface, const GLchar *name) {
    cap.last_program = program;
    (void)interface;
    return strcmp(name, "color") == 0 ? 1 : -1;
}

static int fail(int code, const char *message) {
    fprintf(stderr, "program reflection failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLchar name[32];
    GLsizei length = 0;
    GLint size = 0, value = 0, props_out[2] = {0, 0};
    GLenum type = 0u;
    GLuint index = 0u;
    const GLchar *uniform_names[1] = {"scale"};
    GLenum props[2] = {GL_LOCATION, GL_TYPE};
    const struct {
        GLenum pname;
        GLint expected;
    } queries[] = {
        {GL_ACTIVE_ATOMIC_COUNTER_BUFFERS, 2},
        {GL_GEOMETRY_VERTICES_OUT, 8},
        {GL_GEOMETRY_INPUT_TYPE, GL_LINES},
        {GL_GEOMETRY_OUTPUT_TYPE, GL_TRIANGLE_STRIP},
        {GL_GEOMETRY_SHADER_INVOCATIONS, 3},
        {GL_TESS_CONTROL_OUTPUT_VERTICES, 4},
        {GL_TESS_GEN_MODE, GL_TRIANGLES},
        {GL_TESS_GEN_SPACING, GL_EQUAL},
        {GL_TESS_GEN_VERTEX_ORDER, GL_CCW},
        {GL_TESS_GEN_POINT_MODE, GL_TRUE},
    };
    size_t i;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGetProgramiv = fake_get_programiv;
    native.glesGetString = fake_extensions;
    native.glesGetActiveAttrib = fake_active_attrib;
    native.glesGetActiveUniform = fake_active_uniform;
    native.glesGetUniformIndices = fake_uniform_indices;
    native.glesGetActiveUniformsiv = fake_active_uniforms;
    native.glesGetUniformBlockIndex = fake_uniform_block_index;
    native.glesGetActiveUniformBlockiv = fake_uniform_block_iv;
    native.glesGetActiveUniformBlockName = fake_uniform_block_name;
    native.glesUniformBlockBinding = fake_uniform_block_binding;
    native.glesShaderStorageBlockBinding = fake_storage_block_binding;
    native.glesGetProgramInterfaceiv = fake_program_interface;
    native.glesGetProgramResourceIndex = fake_resource_index;
    native.glesGetProgramResourceName = fake_resource_name;
    native.glesGetProgramResourceiv = fake_resource_iv;
    native.glesGetProgramResourceLocation = fake_resource_location;
    native.glesGetProgramResourceLocationIndex = fake_resource_location_index;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (!ctx.share_group)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);
    program = calloc(1u, sizeof(*program));
    if (!program)
        return fail(2, "program allocation");
    program->name = 9u;
    program->native_name = 77u;
    program->linked = GL_TRUE;
    program->next = ctx.share_group->programs;
    ctx.share_group->programs = program;

    glGetActiveAttrib(9u, 0u, (GLsizei)sizeof(name), &length, &size, &type, name);
    if (cap.last_program != 77u || strcmp(name, "position") != 0 || size != 1 || type != GL_FLOAT)
        return fail(3, "active attrib mapping");
    glGetActiveUniform(9u, 0u, (GLsizei)sizeof(name), &length, &size, &type, name);
    if (strcmp(name, "scale") != 0)
        return fail(4, "active uniform");
    glGetUniformIndices(9u, 1, uniform_names, &index);
    if (index != 3u)
        return fail(5, "uniform indices");
    glGetActiveUniformsiv(9u, 1, &index, GL_UNIFORM_OFFSET, &value);
    if (value != 64)
        return fail(6, "uniform metadata");
    glGetActiveUniformName(9u, 0u, (GLsizei)sizeof(name), &length, name);
    if (strcmp(name, "scale") != 0)
        return fail(7, "uniform name");
    glGetProgramiv(9u, GL_ACTIVE_UNIFORM_BLOCKS, &value);
    if (value != 3 || cap.last_program != 77u || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(21, "active uniform block count");
    glGetProgramiv(9u, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &value);
    if (value != 16 || cap.last_program != 77u || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(22, "active uniform block name length");
    for (i = 0u; i < sizeof(queries) / sizeof(queries[0]); ++i) {
        value = -1;
        glGetProgramiv(9u, queries[i].pname, &value);
        if (value != queries[i].expected || cap.last_program != 77u ||
            cap.last_program_pname != queries[i].pname ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(23, "GLES 3.2 program parameter query");
    }
    {
        GLint work_group[4] = {-1, -1, -1, -1};
        glGetProgramiv(9u, GL_COMPUTE_WORK_GROUP_SIZE, work_group);
        if (cap.last_program != 77u || cap.last_program_pname != GL_COMPUTE_WORK_GROUP_SIZE ||
            work_group[0] != 4 || work_group[1] != 2 || work_group[2] != 1 ||
            work_group[3] != -1 || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(24, "compute work group dimensions");
    }
    if (glGetUniformBlockIndex(9u, "Globals") != 2u)
        return fail(8, "uniform block index");
    glGetActiveUniformBlockiv(9u, 2u, GL_UNIFORM_BLOCK_DATA_SIZE, &value);
    if (value != 128)
        return fail(9, "uniform block metadata");
    glGetActiveUniformBlockName(9u, 2u, (GLsizei)sizeof(name), &length, name);
    if (strcmp(name, "Globals") != 0)
        return fail(10, "uniform block name");
    glUniformBlockBinding(9u, 2u, 5u);
    if (cap.uniform_binding_block != 2u || cap.uniform_binding_point != 5u)
        return fail(11, "uniform block binding");
    glShaderStorageBlockBinding(9u, 1u, 6u);
    if (cap.storage_binding_block != 1u || cap.storage_binding_point != 6u)
        return fail(12, "storage block binding");
    glGetProgramInterfaceiv(9u, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &value);
    if (value != 4)
        return fail(13, "program interface query");
    if (glGetProgramResourceIndex(9u, GL_BUFFER_VARIABLE, "values") != 1u)
        return fail(14, "resource index");
    glGetProgramResourceName(9u, GL_BUFFER_VARIABLE, 1u, (GLsizei)sizeof(name), &length, name);
    if (strcmp(name, "values") != 0)
        return fail(15, "resource name");
    glGetProgramResourceiv(9u, GL_BUFFER_VARIABLE, 1u, 2, props, 2, &length, props_out);
    if (length != 2 || props_out[0] != 7 || props_out[1] != (GLint)GL_FLOAT)
        return fail(16, "resource properties");
    {
        const GLenum indexed_props[] = {GL_TYPE, GL_LOCATION_INDEX, GL_NAME_LENGTH};
        GLint indexed_results[4] = {-7, -7, -7, -7};
        unsigned before = cap.native_resource_calls;
        length = -1;
        glGetProgramResourceiv(9u, GL_PROGRAM_OUTPUT, 0u, 3, indexed_props, 4,
                               &length, indexed_results);
        if (length != 3 || indexed_results[0] != (GLint)GL_FLOAT ||
            indexed_results[1] != 0 || indexed_results[2] != 6 ||
            indexed_results[3] != -7 || cap.native_location_index_property_calls != 0 ||
            cap.native_resource_calls <= before || glGetError() != GL_NO_ERROR)
            return fail(27, "mixed desktop location index reflection");
        length = -1;
        indexed_results[0] = -7;
        indexed_results[1] = -7;
        glGetProgramResourceiv(9u, GL_PROGRAM_OUTPUT, 0u, 3, indexed_props, 1,
                               &length, indexed_results);
        if (length != 1 || indexed_results[0] != (GLint)GL_FLOAT ||
            indexed_results[1] != -7 || glGetError() != GL_NO_ERROR)
            return fail(28, "truncated resource property buffer");
        {
            const GLenum extra_props[] = {GL_LOCATION_INDEX, GL_OFFSET};
            unsigned before_offset = cap.native_offset_property_calls;
            glGetProgramResourceiv(9u, GL_PROGRAM_OUTPUT, 0u, 2, extra_props, 1,
                                   &length, indexed_results);
            if (length != 1 || cap.native_offset_property_calls != before_offset + 1u ||
                glGetError() != GL_NO_ERROR)
                return fail(37, "truncated resource properties still validated");
        }
        length = -1;
        glGetProgramResourceiv(9u, GL_PROGRAM_OUTPUT, 0u, 3, indexed_props, 0, &length, NULL);
        if (length != 0 || glGetError() != GL_NO_ERROR)
            return fail(29, "zero-size resource output buffer");
        before = cap.native_resource_calls;
        glGetProgramResourceiv(9u, GL_PROGRAM_OUTPUT, 0u, 0, indexed_props, 1,
                               &length, indexed_results);
        if (glGetError() != GL_INVALID_VALUE || cap.native_resource_calls != before)
            return fail(30, "zero-property validation");
        glGetProgramResourceiv(9u, GL_UNIFORM, 0u, 1, indexed_props + 1, 1,
                               &length, indexed_results);
        if (glGetError() != GL_INVALID_OPERATION)
            return fail(31, "location index invalid interface");
        glGetProgramResourceiv(9u, GL_PROGRAM_OUTPUT, 4u, 1, indexed_props + 1, 1,
                               &length, indexed_results);
        if (glGetError() != GL_INVALID_VALUE)
            return fail(32, "location index invalid resource index");
        cap.has_blend_extension = 1;
        before = cap.native_location_index_property_calls;
        glGetProgramResourceiv(9u, GL_PROGRAM_OUTPUT, 0u, 1, indexed_props + 1, 1,
                               &length, indexed_results);
        if (length != 1 || indexed_results[0] != 1 ||
            cap.native_location_index_property_calls != before + 1u ||
            glGetError() != GL_NO_ERROR)
            return fail(36, "native extension location index query");
        cap.has_blend_extension = 0;
    }
    if (glGetProgramResourceLocation(9u, GL_UNIFORM, "value") != 7)
        return fail(17, "resource location");
    if (glGetProgramResourceLocationIndex(9u, GL_PROGRAM_OUTPUT, "color") != 0)
        return fail(18, "resource location index fallback");
    cap.has_blend_extension = 1;
    if (glGetProgramResourceLocationIndex(9u, GL_PROGRAM_OUTPUT, "color") != 1)
        return fail(40, "native resource location index with extension");
    cap.has_blend_extension = 0;
    native.glesGetProgramResourceLocationIndex = NULL;
    if (glGetProgramResourceLocationIndex(9u, GL_PROGRAM_OUTPUT, "color") != 0 ||
        glGetProgramResourceLocationIndex(9u, GL_PROGRAM_OUTPUT, "missing") != -1 ||
        glGetError() != GL_NO_ERROR)
        return fail(25, "resource location index without native extension");
    if (glGetProgramResourceLocationIndex(9u, GL_UNIFORM, "value") != -1 ||
        glGetError() != GL_INVALID_ENUM)
        return fail(26, "resource location index interface validation");
    native.glesGetProgramResourceLocationIndex = fake_resource_location_index;
    if (glXGetProcAddress((const GLubyte *)"glGetProgramInterfaceiv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glUniformBlockBinding") == NULL)
        return fail(19, "proc address coverage");

    glGetProgramInterfaceiv(12345u, GL_UNIFORM, GL_ACTIVE_RESOURCES, &value);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(20, "invalid program validation");
    {
        struct mgl_shader *shader = calloc(1u, sizeof(*shader));
        if (shader == NULL)
            return fail(33, "shader allocation");
        shader->name = 45u;
        shader->next = ctx.share_group->shaders;
        ctx.share_group->shaders = shader;
        glGetProgramInterfaceiv(45u, GL_UNIFORM, GL_ACTIVE_RESOURCES, &value);
        if (glGetError() != GL_INVALID_OPERATION)
            return fail(34, "shader name as program interface");
        glGetProgramResourceiv(45u, GL_UNIFORM, 0u, 1, props, 1, &length, &value);
        if (glGetError() != GL_INVALID_OPERATION)
            return fail(35, "shader name as program resource");
    }

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL program reflection unit test: PASS");
    return 0;
}

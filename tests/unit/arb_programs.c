#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "shader/shader_store.h"
#include "abi/proc_address.h"
#include "compat/arb_program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_buffer;
    GLuint next_shader;
    GLuint next_program;
    GLuint used_program;
    unsigned compile_calls;
    unsigned link_calls;
    unsigned draw_calls;
    unsigned vp_env_uploads;
    unsigned fp_local_uploads;
    unsigned sampler_upload_mask;
    unsigned sampler_upload_calls;
    unsigned model_view_uploads;
    unsigned projection_uploads;
    GLfloat model_view_matrix[16];
    GLfloat projection_matrix[16];
    GLfloat vp_env0[4];
    GLfloat fp_local0[4];
    GLuint position_location;
    GLuint color_location;
    size_t position_offset;
    size_t color_offset;
    char vertex_source[4096];
    char fragment_source[4096];
} cap;

static GLenum shader_type[32];

static void fake_get_integerv(GLenum pname, GLint *value) {
    if (pname == GL_MAX_VERTEX_ATTRIBS)
        *value = 16;
    else
        *value = 0;
}

static void fake_gen_buffers(GLsizei n, GLuint *out) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        out[i] = ++cap.next_buffer;
}

static void fake_delete_buffers(GLsizei n, const GLuint *ids) {
    (void)n;
    (void)ids;
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

static GLuint fake_create_shader(GLenum type) {
    GLuint id = ++cap.next_shader;
    if (id < 32u)
        shader_type[id] = type;
    return id;
}

static void fake_shader_source(GLuint shader, GLsizei count, const GLchar *const *strings,
                               const GLint *lengths) {
    char *dst = shader_type[shader] == GL_VERTEX_SHADER ? cap.vertex_source : cap.fragment_source;
    size_t capacity = shader_type[shader] == GL_VERTEX_SHADER ? sizeof(cap.vertex_source)
                                                              : sizeof(cap.fragment_source);
    size_t used = 0u;
    GLsizei i;
    dst[0] = '\0';
    for (i = 0; i < count && used + 1u < capacity; ++i) {
        size_t n = lengths != NULL && lengths[i] >= 0 ? (size_t)lengths[i] : strlen(strings[i]);
        if (n > capacity - used - 1u)
            n = capacity - used - 1u;
        memcpy(dst + used, strings[i], n);
        used += n;
        dst[used] = '\0';
    }
}

static void fake_compile_shader(GLuint shader) {
    (void)shader;
    ++cap.compile_calls;
}

static void fake_get_shaderiv(GLuint shader, GLenum pname, GLint *value) {
    (void)shader;
    if (pname == GL_COMPILE_STATUS)
        *value = GL_TRUE;
    else if (pname == GL_INFO_LOG_LENGTH)
        *value = 1;
    else
        *value = 0;
}

static void fake_delete_shader(GLuint shader) {
    (void)shader;
}

static GLuint fake_create_program(void) {
    return ++cap.next_program + 100u;
}

static void fake_attach_shader(GLuint program, GLuint shader) {
    (void)program;
    (void)shader;
}

static void fake_link_program(GLuint program) {
    (void)program;
    ++cap.link_calls;
}

static void fake_get_programiv(GLuint program, GLenum pname, GLint *value) {
    (void)program;
    if (pname == GL_LINK_STATUS)
        *value = GL_TRUE;
    else if (pname == GL_INFO_LOG_LENGTH)
        *value = 1;
    else
        *value = 0;
}

static void fake_delete_program(GLuint program) {
    (void)program;
}

static GLint fake_get_attrib_location(GLuint program, const GLchar *name) {
    (void)program;
    if (strcmp(name, "mgl_a_position") == 0)
        return 2;
    if (strcmp(name, "mgl_a_color") == 0)
        return 5;
    return -1;
}

static GLint fake_get_uniform_location(GLuint program, const GLchar *name) {
    (void)program;
    if (strcmp(name, "mgl_arb_vp_env[0]") == 0)
        return 10;
    if (strcmp(name, "mgl_arb_fp_local[0]") == 0)
        return 11;
    if (strcmp(name, "mgl_u_model_view") == 0)
        return 30;
    if (strcmp(name, "mgl_u_projection") == 0)
        return 31;
    for (unsigned unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit) {
        char sampler_name[40];
        (void)snprintf(sampler_name, sizeof(sampler_name), "mgl_arb_texture%u", unit);
        if (strcmp(name, sampler_name) == 0)
            return (GLint)(20u + unit);
    }
    return -1;
}

static void fake_use_program(GLuint program) {
    cap.used_program = program;
}

static void fake_uniform4fv(GLint location, GLsizei count, const GLfloat *values) {
    if (location == 10 && count >= 1) {
        memcpy(cap.vp_env0, values, sizeof(cap.vp_env0));
        ++cap.vp_env_uploads;
    }
    if (location == 11 && count >= 1) {
        memcpy(cap.fp_local0, values, sizeof(cap.fp_local0));
        ++cap.fp_local_uploads;
    }
}

static void fake_uniform1i(GLint location, GLint value) {
    ++cap.sampler_upload_calls;
    if (location >= 20 && location < (GLint)(20u + MGL_TEXTURE_UNITS) && value == location - 20)
        cap.sampler_upload_mask |= 1u << (unsigned)value;
}

static void fake_uniform_matrix4fv(GLint location, GLsizei count, GLboolean transpose,
                                   const GLfloat *value) {
    if (count != 1 || transpose != GL_FALSE || value == NULL)
        return;
    if (location == 30) {
        memcpy(cap.model_view_matrix, value, sizeof(cap.model_view_matrix));
        ++cap.model_view_uploads;
    } else if (location == 31) {
        memcpy(cap.projection_matrix, value, sizeof(cap.projection_matrix));
        ++cap.projection_uploads;
    }
}

static void fake_disable_attrib(GLuint index) {
    (void)index;
}

static void fake_enable_attrib(GLuint index) {
    (void)index;
}

static void fake_attrib_current(GLuint index, const GLfloat *value) {
    (void)index;
    (void)value;
}

static void fake_attrib_pointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                GLsizei stride, const void *ptr) {
    (void)size;
    (void)type;
    (void)normalized;
    (void)stride;
    if (index == 2u) {
        cap.position_location = index;
        cap.position_offset = (size_t)(uintptr_t)ptr;
    }
    if (index == 5u) {
        cap.color_location = index;
        cap.color_offset = (size_t)(uintptr_t)ptr;
    }
}

static void fake_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    (void)mode;
    (void)first;
    (void)count;
    ++cap.draw_calls;
}

static void init_native(struct mgl_native_dispatch *native) {
    memset(native, 0, sizeof(*native));
    native->glesGetIntegerv = fake_get_integerv;
    native->glesGenBuffers = fake_gen_buffers;
    native->glesDeleteBuffers = fake_delete_buffers;
    native->glesBindBuffer = fake_bind_buffer;
    native->glesBufferData = fake_buffer_data;
    native->glesCreateShader = fake_create_shader;
    native->glesShaderSource = fake_shader_source;
    native->glesCompileShader = fake_compile_shader;
    native->glesGetShaderiv = fake_get_shaderiv;
    native->glesDeleteShader = fake_delete_shader;
    native->glesCreateProgram = fake_create_program;
    native->glesAttachShader = fake_attach_shader;
    native->glesLinkProgram = fake_link_program;
    native->glesGetProgramiv = fake_get_programiv;
    native->glesDeleteProgram = fake_delete_program;
    native->glesGetAttribLocation = fake_get_attrib_location;
    native->glesGetUniformLocation = fake_get_uniform_location;
    native->glesUseProgram = fake_use_program;
    native->glesUniform4fv = fake_uniform4fv;
    native->glesUniform1i = fake_uniform1i;
    native->glesUniformMatrix4fv = fake_uniform_matrix4fv;
    native->glesDisableVertexAttribArray = fake_disable_attrib;
    native->glesEnableVertexAttribArray = fake_enable_attrib;
    native->glesVertexAttrib4fv = fake_attrib_current;
    native->glesVertexAttribPointer = fake_attrib_pointer;
    native->glesDrawArrays = fake_draw_arrays;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "ARB program failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint ids[2] = {0u, 0u};
    char vp[] = "!!ARBvp1.0\n"
                "ATTRIB pos = vertex.position;\n"
                "ATTRIB col = vertex.color.primary;\n"
                "PARAM offset = program.env[0];\n"
                "OUTPUT oPos = result.position;\n"
                "OUTPUT oCol = result.color;\n"
                "ADD oPos, pos, offset;\n"
                "MOV oCol, col;\n"
                "END";
    char fp[] = "!!ARBfp1.0\n"
                "PARAM tint = program.local[0];\n"
                "MUL result.color, fragment.color.primary, tint;\n"
                "END";
    char bad[] = "!!ARBvp1.0\nBOGUS result.position, vertex.position;\nEND";
    char copied[512];
    GLfloat env[4] = {0.25f, 0.0f, 0.0f, 0.0f};
    GLfloat local[4] = {0.5f, 0.75f, 1.0f, 1.0f};
    GLfloat got[4];
    GLfloat positions[2][4] = {{0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}};
    GLfloat colors[2][4] = {{1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}};
    GLint value = 0;
    size_t vp_len = strlen(vp);
    size_t fp_len = strlen(fp);

    memset(&ctx, 0, sizeof(ctx));
    memset(&cap, 0, sizeof(cap));
    memset(shader_type, 0, sizeof(shader_type));
    init_native(&native);
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.native_max_vertex_attribs = 16;
    ctx.arb_program_error_position = -1;
    ctx.arb_uniform_vp_env = ctx.arb_uniform_vp_local = -1;
    ctx.arb_uniform_fp_env = ctx.arb_uniform_fp_local = -1;
    for (unsigned unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit)
        ctx.arb_uniform_textures[unit] = -1;
    mgl_program_init_metadata(&ctx.arb_linked_program);
    mgl_context_set_current_for_test(&ctx);

    glGenProgramsARB(2, ids);
    if (ids[0] == 0u || ids[1] == 0u || ids[0] == ids[1] || glIsProgramARB(ids[0]) ||
        glIsProgramARB(ids[1]))
        return fail(2, "generated names must not be realized before binding");

    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, ids[0]);
    if (!glIsProgramARB(ids[0]) || glIsProgramARB(ids[1]))
        return fail(2, "binding realizes only the selected program");
    glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)vp_len, vp);
    if (glGetError() != GL_NO_ERROR)
        return fail(3, "vertex program load");
    memset(vp, 'X', vp_len);
    memset(copied, 0, sizeof(copied));
    glGetProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_STRING_ARB, copied);
    if (strncmp(copied, "!!ARBvp1.0", 10u) != 0)
        return fail(4, "program source ownership");

    glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, 0, env);
    glGetProgramEnvParameterfvARB(GL_VERTEX_PROGRAM_ARB, 0, got);
    if (memcmp(got, env, sizeof(env)) != 0)
        return fail(5, "environment parameter");

    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ids[1]);
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)fp_len, fp);
    if (glGetError() != GL_NO_ERROR)
        return fail(6, "fragment program load");
    glProgramLocalParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, 0, local);
    glGetProgramLocalParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, 0, got);
    if (memcmp(got, local, sizeof(local)) != 0)
        return fail(7, "local parameter");

    {
        const GLfloat env_bulk[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
        const GLfloat local_bulk[8] = {9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};
        GLfloat before[4];

        glProgramEnvParameters4fvEXT(GL_VERTEX_PROGRAM_ARB, 3u, 2, env_bulk);
        glGetProgramEnvParameterfvARB(GL_VERTEX_PROGRAM_ARB, 3u, got);
        if (memcmp(got, env_bulk, sizeof(got)) != 0)
            return fail(23, "bulk environment parameter first");
        glGetProgramEnvParameterfvARB(GL_VERTEX_PROGRAM_ARB, 4u, got);
        if (memcmp(got, env_bulk + 4, sizeof(got)) != 0)
            return fail(24, "bulk environment parameter second");

        glProgramLocalParameters4fvEXT(GL_FRAGMENT_PROGRAM_ARB, 3u, 2, local_bulk);
        glGetProgramLocalParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, 3u, got);
        if (memcmp(got, local_bulk, sizeof(got)) != 0)
            return fail(25, "bulk local parameter first");
        glGetProgramLocalParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, 4u, got);
        if (memcmp(got, local_bulk + 4, sizeof(got)) != 0)
            return fail(26, "bulk local parameter second");

        glProgramEnvParameters4fvEXT(GL_VERTEX_PROGRAM_ARB, 96u, 0, NULL);
        glProgramLocalParameters4fvEXT(GL_FRAGMENT_PROGRAM_ARB, 96u, 0, NULL);
        if (glGetError() != GL_NO_ERROR)
            return fail(27, "zero-count bulk parameters");

        glGetProgramEnvParameterfvARB(GL_VERTEX_PROGRAM_ARB, 95u, before);
        glProgramEnvParameters4fvEXT(GL_VERTEX_PROGRAM_ARB, 95u, 2, env_bulk);
        if (glGetError() != GL_INVALID_VALUE)
            return fail(28, "bulk environment range error");
        glGetProgramEnvParameterfvARB(GL_VERTEX_PROGRAM_ARB, 95u, got);
        if (memcmp(got, before, sizeof(got)) != 0)
            return fail(29, "bulk environment range atomicity");

        glGetProgramLocalParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, 95u, before);
        glProgramLocalParameters4fvEXT(GL_FRAGMENT_PROGRAM_ARB, 95u, 2, local_bulk);
        if (glGetError() != GL_INVALID_VALUE)
            return fail(30, "bulk local range error");
        glGetProgramLocalParameterfvARB(GL_FRAGMENT_PROGRAM_ARB, 95u, got);
        if (memcmp(got, before, sizeof(got)) != 0)
            return fail(31, "bulk local range atomicity");

        glProgramEnvParameters4fvEXT(GL_VERTEX_PROGRAM_ARB, 0u, -1, env_bulk);
        if (glGetError() != GL_INVALID_VALUE)
            return fail(32, "negative bulk environment count");
        glProgramLocalParameters4fvEXT(GL_FRAGMENT_PROGRAM_ARB, 0u, -1, local_bulk);
        if (glGetError() != GL_INVALID_VALUE)
            return fail(33, "negative bulk local count");
        glProgramEnvParameters4fvEXT(GL_ARRAY_BUFFER, 0u, 1, env_bulk);
        if (glGetError() != GL_INVALID_ENUM)
            return fail(34, "bulk environment invalid target");
        glProgramLocalParameters4fvEXT(GL_ARRAY_BUFFER, 0u, 1, local_bulk);
        if (glGetError() != GL_INVALID_ENUM)
            return fail(35, "bulk local invalid target");
        if (mgl_proc_address("glProgramEnvParameters4fvEXT") == NULL ||
            mgl_proc_address("glProgramLocalParameters4fvEXT") == NULL)
            return fail(36, "bulk parameter proc-address coverage");
    }

    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, ids[0]);
    glPushAttrib(GL_ENABLE_BIT);
    glEnable(GL_VERTEX_PROGRAM_ARB);
    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glPopAttrib();
    if (glIsEnabled(GL_VERTEX_PROGRAM_ARB) || glIsEnabled(GL_FRAGMENT_PROGRAM_ARB))
        return fail(8, "GL_ENABLE_BIT restore");
    glEnable(GL_VERTEX_PROGRAM_ARB);
    glEnable(GL_FRAGMENT_PROGRAM_ARB);

    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_LENGTH_ARB, &value);
    if (value != (GLint)vp_len)
        return fail(9, "program length");

    glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(bad),
                       bad);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(10, "parser error code");
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &value);
    if (value < 0 ||
        strstr((const char *)glGetString(GL_PROGRAM_ERROR_STRING_ARB), "BOGUS") == NULL)
        return fail(11, "parser error diagnostics");

    glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, 0, env);

    glVertexPointer(4, GL_FLOAT, 0, positions);
    glEnableClientState(GL_VERTEX_ARRAY);
    glColorPointer(4, GL_FLOAT, 0, colors);
    glEnableClientState(GL_COLOR_ARRAY);
    glDrawArrays(GL_POINTS, 0, 2);
    if (glGetError() != GL_NO_ERROR)
        return fail(12, "ARB draw interface error");
    if (cap.compile_calls != 2u || cap.link_calls != 1u || cap.draw_calls != 1u ||
        cap.used_program == 0u)
        return fail(13, "native compile/link/draw");
    if (cap.position_location != 2u || cap.color_location != 5u ||
        cap.position_offset == cap.color_offset)
        return fail(14, "reflected compatibility inputs");
    if (cap.vp_env_uploads == 0u || cap.fp_local_uploads == 0u ||
        memcmp(cap.vp_env0, env, sizeof(env)) != 0 ||
        memcmp(cap.fp_local0, local, sizeof(local)) != 0)
        return fail(15, "ARB parameter realization");
    {
        /* Native Dispatch uniforms belong to the GLES linked pair. Repeated draws
         * and unchanged interface parameter setters must not resubmit them. */
        unsigned vp_calls = cap.vp_env_uploads, fp_calls = cap.fp_local_uploads;
        unsigned samplers = cap.sampler_upload_calls;
        GLfloat changed_env[4] = {0.75f, 0.f, 0.f, 0.f};
        GLfloat changed_local[4] = {0.25f, 0.5f, 1.f, 1.f};
        glDrawArrays(GL_POINTS, 0, 2);
        glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, 0, env);
        glProgramLocalParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, 0, local);
        glDrawArrays(GL_POINTS, 0, 2);
        if (glGetError() != GL_NO_ERROR || cap.vp_env_uploads != vp_calls ||
            cap.fp_local_uploads != fp_calls || cap.sampler_upload_calls != samplers)
            return fail(91, "unchanged ARB uniforms must be reused across draws and setters");
        glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, 0, changed_env);
        glProgramLocalParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, 0, changed_local);
        glDrawArrays(GL_POINTS, 0, 2);
        if (glGetError() != GL_NO_ERROR || cap.vp_env_uploads != vp_calls + 1u ||
            cap.fp_local_uploads != fp_calls + 1u ||
            memcmp(cap.vp_env0, changed_env, sizeof(changed_env)) != 0 ||
            memcmp(cap.fp_local0, changed_local, sizeof(changed_local)) != 0 ||
            cap.sampler_upload_calls != samplers)
            return fail(92, "changed ARB parameter values must reach the native");
        glDrawArrays(GL_POINTS, 0, 2);
        if (cap.vp_env_uploads != vp_calls + 1u || cap.fp_local_uploads != fp_calls + 1u)
            return fail(93, "second draw after modification must reuse uniforms");
        glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, 0, env);
        glProgramLocalParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, 0, local);
        glDrawArrays(GL_POINTS, 0, 2);
        if (glGetError() != GL_NO_ERROR || cap.vp_env_uploads != vp_calls + 2u ||
            cap.fp_local_uploads != fp_calls + 2u)
            return fail(94, "restoring ARB parameters must propagate the new revision");
    }

    if (strstr(cap.vertex_source, "mgl_arb_vp_env[1]") == NULL ||
        strstr(cap.fragment_source, "mgl_arb_fp_local[1]") == NULL)
        return fail(16, "translated shader parameter declarations");

    if (mgl_proc_address("glProgramStringARB") == NULL ||
        mgl_proc_address("glBindProgramARB") == NULL)
        return fail(17, "proc-address coverage");

    {
        const char matrix_vp[] = "!!ARBvp1.0\n"
                                 "PARAM mvp[4] = { state.matrix.mvp };\n"
                                 "DP4 result.position.x, mvp[0], vertex.position;\n"
                                 "DP4 result.position.y, mvp[1], vertex.position;\n"
                                 "DP4 result.position.z, mvp[2], vertex.position;\n"
                                 "DP4 result.position.w, mvp[3], vertex.position;\n"
                                 "MOV result.color, vertex.color.primary;\n"
                                 "END";
        const char texture_fp[] = "!!ARBfp1.0\n"
                                  "TEMP tex;\n"
                                  "TEX tex, fragment.texcoord[0], texture[0], 2D;\n"
                                  "MUL result.color, tex, program.local[1];\n"
                                  "END";
        struct mgl_arb_translation translated;
        GLint pos = -1;
        char text[256];
        memset(&translated, 0, sizeof(translated));
        if (!mgl_arb_translate(GL_VERTEX_PROGRAM_ARB, matrix_vp, &translated, &pos, text,
                               sizeof(text)))
            return fail(19, "matrix parameter translation");
        if (strstr(translated.source, "vec4 mvp[4]") == NULL ||
            strstr(translated.source, "gl_Position.x=(vec4(dot") == NULL)
            return fail(20, "matrix row/write-mask translation");
        mgl_arb_translation_destroy(&translated);
        memset(&translated, 0, sizeof(translated));
        if (!mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, texture_fp, &translated, &pos, text,
                               sizeof(text)))
            return fail(21, "texture instruction translation");
        if (!translated.uses_texture0 || translated.max_local_index != 1u ||
            strstr(translated.source, "texture2D(mgl_arb_texture0") == NULL ||
            strstr(translated.source, "mgl_arb_alpha_pass") == NULL)
            return fail(22, "texture/alpha translation metadata");
        mgl_arb_translation_destroy(&translated);
    }

    {
        static const char position_invariant_vp[] =
            "!!ARBvp1.0\nOPTION ARB_position_invariant;\nTEMP R;\n"
            "MOV R, vertex.position;\nEND";
        static const char arl_vp[] = "!!ARBvp1.0\nADDRESS A0;\nARL A0.x, vertex.position.x;\nEND";
        static const char precision_fp[] = "!!ARBfp1.0\nOPTION ARB_precision_hint_fastest;\n"
                                           "MOV result.color, fragment.color.primary;\nEND";
        static const char fog_fp[] = "!!ARBfp1.0\nOPTION ARB_fog_linear;\n"
                                     "MOV result.color, fragment.color.primary;\nEND";
        static const char conflict_fp[] = "!!ARBfp1.0\nOPTION ARB_precision_hint_fastest;\n"
                                          "OPTION ARB_precision_hint_nicest;\nEND";
        static const char conflict_fog_fp[] =
            "!!ARBfp1.0\nOPTION ARB_fog_exp;\nOPTION ARB_fog_linear;\nEND";
        static const char invalid_position_vp[] = "!!ARBvp1.0\nOPTION ARB_position_invariant;\n"
                                                  "MOV result.position, vertex.position;\nEND";
        struct mgl_arb_translation translated;
        GLint pos = -1;
        char text[256];
        memset(&translated, 0, sizeof(translated));
        if (!mgl_arb_translate(GL_VERTEX_PROGRAM_ARB, position_invariant_vp, &translated, &pos,
                               text, sizeof(text)))
            return fail(23, "position invariant option acceptance");
        if (strstr(translated.source, "gl_Position=mgl_u_model_view_projection*mgl_a_position;") ==
            NULL)
            return fail(24, "position invariant transformation");
        mgl_arb_translation_destroy(&translated);
        if (!mgl_arb_translate(GL_VERTEX_PROGRAM_ARB, arl_vp, &translated, &pos, text,
                               sizeof(text)))
            return fail(25, "ARL vertex address operation acceptance");
        if (strstr(translated.source, "A0.x=floor((mgl_a_position.xxxx).x);") == NULL)
            return fail(26, "ARL floor semantics");
        mgl_arb_translation_destroy(&translated);
        if (!mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, precision_fp, &translated, &pos, text,
                               sizeof(text)))
            return fail(27, "precision hint acceptance");
        mgl_arb_translation_destroy(&translated);
        if (!mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, fog_fp, &translated, &pos, text,
                               sizeof(text)))
            return fail(28, "fog option acceptance");
        mgl_arb_translation_destroy(&translated);
        if (mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, conflict_fp, &translated, &pos, text,
                              sizeof(text)) ||
            pos < 0)
            return fail(29, "conflicting precision hints must fail");
        if (mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, conflict_fog_fp, &translated, &pos, text,
                              sizeof(text)) ||
            pos < 0)
            return fail(30, "conflicting fog options must fail");
        if (mgl_arb_translate(GL_VERTEX_PROGRAM_ARB, invalid_position_vp, &translated, &pos, text,
                              sizeof(text)) ||
            pos < 0)
            return fail(31, "position invariant must forbid writing position");
    }

    {
        static const char tab_vertex[] = "!!ARBvp1.0\n"
                                         "OPTION\tARB_position_invariant;\n"
                                         "ATTRIB\tlocalPos = vertex.position;\n"
                                         "PARAM\tdefaultTexCoord = { 0, 0.5, 0, 1 };\n"
                                         "TEMP\tR0,\tR1;\n"
                                         "ADDRESS\tA0;\n"
                                         "ARL A0.x, localPos.x;\n"
                                         "MOV R0, defaultTexCoord;\n"
                                         "ADD R1, R0, localPos;\n"
                                         "END";
        static const char tab_fragment[] = "!!ARBfp1.0\n"
                                           "OPTION\tARB_precision_hint_fastest;\n"
                                           "PARAM\ttint = { 1, 0.5, 0.25, 1 };\n"
                                           "TEMP\ttoLight,\tlocalNormal;\n"
                                           "MOV toLight, fragment.color.primary;\n"
                                           "MUL localNormal, toLight, tint;\n"
                                           "MOV result.color, localNormal;\n"
                                           "END";
        static const char invalid_keyword[] = "!!ARBvp1.0\nTEMPORARY R0;\nEND";
        struct mgl_arb_translation translated;
        GLint pos = -1;
        char text[256];
        memset(&translated, 0, sizeof(translated));
        if (!mgl_arb_translate(GL_VERTEX_PROGRAM_ARB, tab_vertex, &translated, &pos, text,
                               sizeof(text)))
            return fail(32, "tab-separated vertex declarations");
        if (strstr(translated.source, "vec4 R0=vec4(0.0);") == NULL ||
            strstr(translated.source, "vec4 R1=vec4(0.0);") == NULL ||
            strstr(translated.source, "#define defaultTexCoord (vec4( 0, 0.5, 0, 1 ))") == NULL ||
            strstr(translated.source, "A0.x=floor(") == NULL)
            return fail(33, "tab-separated vertex declaration semantics");
        mgl_arb_translation_destroy(&translated);
        if (!mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, tab_fragment, &translated, &pos, text,
                               sizeof(text)))
            return fail(34, "tab-separated fragment declarations");
        if (strstr(translated.source, "vec4 localNormal=vec4(0.0);") == NULL ||
            strstr(translated.source, "#define tint (vec4( 1, 0.5, 0.25, 1 ))") == NULL)
            return fail(35, "tab-separated fragment declaration semantics");
        mgl_arb_translation_destroy(&translated);
        if (mgl_arb_translate(GL_VERTEX_PROGRAM_ARB, invalid_keyword, &translated, &pos, text,
                              sizeof(text)) ||
            pos < 0)
            return fail(36, "invalid declaration keyword should remain rejected");
    }

    {
        static const char multitexture_fp[] =
            "!!ARBfp1.0\n"
            "TEMP normal, falloff, light, diffuse, specular;\n"
            "TEX normal, fragment.texcoord[0], texture[1], 2D;\n"
            "TEX falloff, fragment.texcoord[1], texture[2], 2D;\n"
            "TEX light, fragment.texcoord[2], texture[3], 2D;\n"
            "TEX diffuse, fragment.texcoord[3], texture[4], 2D;\n"
            "TXB specular, fragment.texcoord[4], texture[5], 2D;\n"
            "MUL result.color, normal, diffuse;\nEND";
        static const char cube_fp[] = "!!ARBfp1.0\nTEMP env;\n"
                                      "TEX env, fragment.texcoord[0], texture[6], CUBE;\n"
                                      "MOV result.color, env;\nEND";
        static const char projected_fp[] = "!!ARBfp1.0\nTEMP projected;\n"
                                           "TXP projected, fragment.texcoord[0], texture[3], 2D;\n"
                                           "MOV result.color, projected;\nEND";
        static const char invalid_unit_fp[] =
            "!!ARBfp1.0\nTEX result.color, fragment.texcoord[0], texture[8], 2D;\nEND";
        static const char conflicting_unit_fp[] =
            "!!ARBfp1.0\nTEMP t;\n"
            "TEX t, fragment.texcoord[0], texture[2], 2D;\n"
            "TEX result.color, fragment.texcoord[1], texture[2], CUBE;\nEND";
        static const char unsupported_target_fp[] =
            "!!ARBfp1.0\nTEX result.color, fragment.texcoord[0], texture[2], 3D;\nEND";
        struct mgl_arb_translation translated;
        GLint pos = -1;
        char text[256];
        memset(&translated, 0, sizeof(translated));
        if (!mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, multitexture_fp, &translated, &pos, text,
                               sizeof(text)))
            return fail(37, "multi-unit 2D fragment sampling");
        if (translated.uses_texture0 || translated.texture_targets[1] != GL_TEXTURE_2D ||
            translated.texture_targets[5] != GL_TEXTURE_2D ||
            strstr(translated.source, "uniform sampler2D mgl_arb_texture5;") == NULL ||
            strstr(translated.source, "texture2D(mgl_arb_texture5") == NULL ||
            strstr(translated.source, ",(") == NULL)
            return fail(38, "multi-unit sampler and TXB bias translation");
        mgl_arb_translation_destroy(&translated);
        if (!mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, cube_fp, &translated, &pos, text,
                               sizeof(text)))
            return fail(39, "cube sampler translation");
        if (translated.texture_targets[6] != GL_TEXTURE_CUBE_MAP ||
            strstr(translated.source, "uniform samplerCube mgl_arb_texture6;") == NULL ||
            strstr(translated.source, "textureCube(mgl_arb_texture6") == NULL)
            return fail(40, "cube sampler semantics");
        mgl_arb_translation_destroy(&translated);
        if (!mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, projected_fp, &translated, &pos, text,
                               sizeof(text)))
            return fail(41, "projective sampler translation");
        if (strstr(translated.source, "/(mgl_arb_v_texcoord0).w") == NULL)
            return fail(42, "projective divide semantics");
        mgl_arb_translation_destroy(&translated);
        if (mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, invalid_unit_fp, &translated, &pos, text,
                              sizeof(text)) ||
            pos < 0)
            return fail(43, "out of range sampler unit rejection");
        if (mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, conflicting_unit_fp, &translated, &pos, text,
                              sizeof(text)) ||
            pos < 0)
            return fail(44, "conflicting sampler targets must fail");
        if (mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, unsupported_target_fp, &translated, &pos,
                              text, sizeof(text)) ||
            pos < 0)
            return fail(45, "unsupported sampler target must fail");
        glBindProgramARB(GL_VERTEX_PROGRAM_ARB, ids[0]);
        {
            static const char valid_vertex[] =
                "!!ARBvp1.0\nMOV result.position, vertex.position;\nEND";
            glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                               (GLsizei)strlen(valid_vertex), valid_vertex);
        }
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ids[1]);
        glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                           (GLsizei)strlen(multitexture_fp), multitexture_fp);
        if (glGetError() != GL_NO_ERROR)
            return fail(46, "multi-unit program upload");
        cap.sampler_upload_mask = 0u;
        glDrawArrays(GL_POINTS, 0, 2);
        if (glGetError() != GL_NO_ERROR || (cap.sampler_upload_mask & 0x3eu) != 0x3eu)
            return fail(47, "texture sampler units must match GL texture indices");
    }

    {
        static const char vertex_primary_color[] = "!!ARBvp1.0\n"
                                                   "ATTRIB color = vertex.color;\n"
                                                   "MOV result.position, vertex.position;\n"
                                                   "MOV result.color, color;\n"
                                                   "MOV result.texcoord[0], vertex.color.xyzw;\n"
                                                   "END";
        static const char fragment_primary_color[] = "!!ARBfp1.0\n"
                                                     "ATTRIB color = fragment.color;\n"
                                                     "MOV result.color, color;\n"
                                                     "END";
        static const char invalid_vertex_color[] = "!!ARBvp1.0\n"
                                                   "ATTRIB color = vertex.color.invalid;\n"
                                                   "END";
        struct mgl_arb_translation translated;
        GLint pos = -1;
        char text[256];
        memset(&translated, 0, sizeof(translated));
        if (!mgl_arb_translate(GL_VERTEX_PROGRAM_ARB, vertex_primary_color, &translated, &pos, text,
                               sizeof(text)))
            return fail(48, "vertex primary color alias acceptance");
        if (strstr(translated.source, "#define color (mgl_a_color)") == NULL ||
            strstr(translated.source, "mgl_a_color.xyzw") == NULL ||
            strstr(translated.source, "vertex.color") != NULL)
            return fail(49, "vertex color alias GLSL mapping");
        mgl_arb_translation_destroy(&translated);
        if (!mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, fragment_primary_color, &translated, &pos,
                               text, sizeof(text)))
            return fail(50, "fragment primary color alias acceptance");
        if (strstr(translated.source, "#define color (mgl_arb_v_color)") == NULL ||
            strstr(translated.source, "fragment.color") != NULL)
            return fail(51, "fragment color alias GLSL mapping");
        mgl_arb_translation_destroy(&translated);
        if (mgl_arb_translate(GL_VERTEX_PROGRAM_ARB, invalid_vertex_color, &translated, &pos, text,
                              sizeof(text)) ||
            pos < 0)
            return fail(52, "invalid primary color suffix must fail");
    }

    {
        static const char fragment_window_position[] =
            "!!ARBfp1.0\n"
            "TEMP R0;\n"
            "ATTRIB window = fragment.position;\n"
            "MUL R0, fragment.position, program.env[1];\n"
            "ADD R0.xy, R0, fragment.position.xy;\n"
            "MOV R0.z, fragment.position.z;\n"
            "MOV result.color, window;\n"
            "END";
        static const char invalid_fragment_position[] =
            "!!ARBfp1.0\n"
            "MOV result.color, fragment.position.invalid;\n"
            "END";
        static const char invalid_vertex_fragment_position[] =
            "!!ARBvp1.0\n"
            "MOV result.position, fragment.position;\n"
            "END";
        struct mgl_arb_translation translated = {0};
        GLint pos = -1;
        char text[256];
        if (!mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, fragment_window_position, &translated, &pos,
                               text, sizeof(text)))
            return fail(64, "fragment window-coordinate attribute translation");
        if (strstr(translated.source, "R0=(gl_FragCoord*mgl_arb_fp_env[1]);") == NULL ||
            strstr(translated.source, "#define window (gl_FragCoord)") == NULL ||
            strstr(translated.source, "gl_FragCoord.xy") == NULL ||
            strstr(translated.source, "gl_FragCoord.zzzz") == NULL ||
            strstr(translated.source, "fragment.position") != NULL)
            return fail(65, "fragment window-coordinate GLSL mapping");
        mgl_arb_translation_destroy(&translated);
        if (mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, invalid_fragment_position, &translated, &pos,
                              text, sizeof(text)) ||
            pos < 0)
            return fail(66, "invalid fragment position swizzle rejection");
        if (mgl_arb_translate(GL_VERTEX_PROGRAM_ARB, invalid_vertex_fragment_position, &translated,
                              &pos, text, sizeof(text)) ||
            pos < 0)
            return fail(67, "fragment input in vertex program rejection");
    }

    {
        static const char fragment_color_swizzle[] =
            "!!ARBfp1.0\n"
            "TEMP localNormal;\n"
            "MOV localNormal, fragment.color;\n"
            "MOV localNormal.x, localNormal.a;\n"
            "MOV localNormal.y, localNormal.r;\n"
            "MOV localNormal.z, localNormal.g;\n"
            "MOV localNormal.w, localNormal.b;\n"
            "MUL result.color, localNormal.a, localNormal.g;\n"
            "END";
        struct mgl_arb_translation translated = {0};
        GLint pos = -1;
        char text[256];
        if (!mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, fragment_color_swizzle, &translated, &pos,
                               text, sizeof(text)))
            return fail(62, "fragment source component swizzles");
        if (strstr(translated.source, "localNormal.x=(localNormal.aaaa).x;") == NULL ||
            strstr(translated.source, "localNormal.y=(localNormal.rrrr).y;") == NULL ||
            strstr(translated.source, "localNormal.z=(localNormal.gggg).z;") == NULL ||
            strstr(translated.source, "localNormal.w=(localNormal.bbbb).w;") == NULL ||
            strstr(translated.source, "(localNormal.aaaa*localNormal.gggg)") == NULL ||
            strstr(translated.source, "(localNormal.a).x") != NULL)
            return fail(63, "fragment scalar swizzle vector replication");
        mgl_arb_translation_destroy(&translated);
    }

    {
        static const char matrix_state_vp[] =
            "!!ARBvp1.0\n"
            "DP4 result.position.x, state.matrix.modelview.row[0], vertex.position;\n"
            "DP4 result.position.y, state.matrix.projection.row[1], vertex.position;\n"
            "MOV result.position.zw, vertex.position.zw;\n"
            "END";
        static const char matrix_param_vp[] = "!!ARBvp1.0\n"
                                              "PARAM mv[4] = { state.matrix.modelview };\n"
                                              "PARAM pr[4] = { state.matrix.projection };\n"
                                              "DP4 result.position.x, mv[0], vertex.position;\n"
                                              "DP4 result.position.y, pr[1], vertex.position;\n"
                                              "MOV result.position.zw, vertex.position.zw;\n"
                                              "END";
        static const char bad_matrix_row[] =
            "!!ARBvp1.0\n"
            "DP4 result.position.x, state.matrix.modelview.row[4], vertex.position;\n"
            "END";
        static const char scalar_fp[] = "!!ARBfp1.0\n"
                                        "RCP result.color, 1.0;\n"
                                        "END";
        GLfloat model_view[16] = {2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f,
                                  0.0f, 0.0f, 4.0f, 0.0f, 1.0f, 2.0f, 3.0f, 1.0f};
        GLfloat projection[16] = {5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 6.0f, 0.0f, 0.0f,
                                  0.0f, 0.0f, 7.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
        struct mgl_arb_translation translated = {0};
        GLint pos = -1;
        char text[256];
        if (!mgl_arb_translate(GL_VERTEX_PROGRAM_ARB, matrix_state_vp, &translated, &pos, text,
                               sizeof(text)))
            return fail(53, "matrix state row translation");
        if (strstr(translated.source, "mgl_u_model_view[0][0]") == NULL ||
            strstr(translated.source, "mgl_u_projection[0][1]") == NULL ||
            strstr(translated.source, "state.matrix.") != NULL)
            return fail(54, "matrix row GLSL mapping");
        mgl_arb_translation_destroy(&translated);
        if (!mgl_arb_translate(GL_VERTEX_PROGRAM_ARB, matrix_param_vp, &translated, &pos, text,
                               sizeof(text)))
            return fail(55, "matrix parameter array translation");
        if (strstr(translated.source, "mv[0]=vec4(mgl_u_model_view[0][0]") == NULL ||
            strstr(translated.source, "pr[1]=vec4(mgl_u_projection[0][1]") == NULL)
            return fail(56, "matrix parameter array GLSL mapping");
        mgl_arb_translation_destroy(&translated);
        if (mgl_arb_translate(GL_VERTEX_PROGRAM_ARB, bad_matrix_row, &translated, &pos, text,
                              sizeof(text)))
            return fail(57, "invalid matrix row rejection");
        if (!mgl_arb_translate(GL_FRAGMENT_PROGRAM_ARB, scalar_fp, &translated, &pos, text,
                               sizeof(text)))
            return fail(58, "scalar constant ARB operand");
        if (strstr(translated.source, "vec4(float(1.0))") == NULL)
            return fail(59, "scalar constant vector expansion");
        mgl_arb_translation_destroy(&translated);
        glBindProgramARB(GL_VERTEX_PROGRAM_ARB, ids[0]);
        glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                           (GLsizei)strlen(matrix_state_vp), matrix_state_vp);
        if (glGetError() != GL_NO_ERROR)
            return fail(60, "matrix state program upload");
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(model_view);
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(projection);
        glDrawArrays(GL_POINTS, 0, 2);
        if (glGetError() != GL_NO_ERROR || cap.model_view_uploads == 0u ||
            cap.projection_uploads == 0u ||
            memcmp(cap.model_view_matrix, model_view, sizeof(model_view)) != 0 ||
            memcmp(cap.projection_matrix, projection, sizeof(projection)) != 0)
            return fail(61, "ARB matrix uniforms must reflect compatibility state");
    }

    {
        /* An unrelated shared ARB object update invalidates pair lookup revision. The current
         * linked shader pair remains reusable after validation. */
        GLuint unrelated = 0u;
        GLuint prior_compiles = cap.compile_calls;
        GLuint prior_links = cap.link_calls;
        uint64_t previous_revision = ctx.share_group->arb_programs_revision;
        struct mgl_program *linked = mgl_arb_current_linked_program(&ctx);
        if (linked == NULL || !ctx.arb_native_ready ||
            ctx.arb_resolved_revision != previous_revision || ctx.arb_cached_stage_mask != 3u)
            return fail(71, "ARB linked-program cache initialized");
        glGenProgramsARB(1, &unrelated);
        if (unrelated == 0u || ctx.share_group->arb_programs_revision == previous_revision)
            return fail(72, "shared ARB program allocation revision");
        if (mgl_arb_current_linked_program(&ctx) != linked ||
            ctx.arb_resolved_revision != ctx.share_group->arb_programs_revision ||
            cap.compile_calls != prior_compiles || cap.link_calls != prior_links)
            return fail(73, "unrelated ARB program must not recompile current pair");
        if (mgl_arb_current_linked_program(&ctx) != linked ||
            ctx.arb_resolved_revision != ctx.share_group->arb_programs_revision)
            return fail(74, "stable ARB program lookup revision");
        /* Loading a new source into the bound object must still cause shader
         * recompilation; a lookup cache must never hide source replacement. */
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ids[1]);
        {
            static const char changed_fragment[] =
                "!!ARBfp1.0\nMOV result.color, fragment.color.primary;\nEND";
            glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                               (GLsizei)strlen(changed_fragment), changed_fragment);
        }
        if (glGetError() != GL_NO_ERROR || mgl_arb_current_linked_program(&ctx) == NULL ||
            cap.compile_calls <= prior_compiles || cap.link_calls <= prior_links)
            return fail(75, "bound ARB source replacement forces a new linked pair");
        glDeleteProgramsARB(1, &unrelated);
    }

    {
        /* Peer contexts share ARB program objects and maintain independent native shader caches.
         * Source replacement invalidates cached pairs. */
        static const char peer_fragment[] = "!!ARBfp1.0\nMOV result.color, fragment.position;\nEND";
        struct mgl_context peer;
        uint64_t rev = ctx.share_group->arb_programs_revision;
        unsigned previous_links = cap.link_calls;
        GLenum peer_error;
        memset(&peer, 0, sizeof(peer));
        peer.native_dispatch = &native;
        peer.share_group = ctx.share_group;
        mgl_error_init(&peer.errors);
        mgl_context_set_current_for_test(&peer);
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ids[1]);
        glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                           (GLsizei)strlen(peer_fragment), peer_fragment);
        peer_error = glGetError();
        mgl_context_set_current_for_test(&ctx);
        if (peer_error != GL_NO_ERROR || ctx.share_group->arb_programs_revision == rev)
            return fail(76, "peer context source update revision");
        if (mgl_arb_current_linked_program(&ctx) == NULL || cap.link_calls <= previous_links ||
            ctx.arb_resolved_revision != ctx.share_group->arb_programs_revision)
            return fail(77, "shared ARB source update invalidates draw cache");
    }

    {
        /* Alternating two ARB pairs must retain both GLES programs.  A
         * repeated bind and returning to an old pair must never relink. */
        static const char alternative_fragment[] =
            "!!ARBfp1.0\nMOV result.color, fragment.color.primary;\nEND";
        static const char replaced_fragment[] =
            "!!ARBfp1.0\nMOV result.color, fragment.position;\nEND";
        GLuint second_fragment = 0u;
        GLuint initial_native;
        GLuint secondary_native;
        unsigned initial_links;
        glGenProgramsARB(1, &second_fragment);
        if (second_fragment == 0u)
            return fail(78, "pair cache test program creation");
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ids[1]);
        if (mgl_arb_current_linked_program(&ctx) == NULL)
            return fail(79, "pair cache initial program");
        initial_native = ctx.arb_native_program;
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, second_fragment);
        glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                           (GLsizei)strlen(alternative_fragment), alternative_fragment);
        if (glGetError() != GL_NO_ERROR || mgl_arb_current_linked_program(&ctx) == NULL)
            return fail(80, "pair cache alternate program");
        secondary_native = ctx.arb_native_program;
        if (secondary_native == initial_native)
            return fail(81, "distinct ARB pairs");
        initial_links = cap.link_calls;
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ids[1]);
        if (mgl_arb_current_linked_program(&ctx) == NULL ||
            ctx.arb_native_program != initial_native || cap.link_calls != initial_links)
            return fail(82, "restore original ARB pair without relinking");
        glDrawArrays(GL_POINTS, 0, 2);
        if (glGetError() != GL_NO_ERROR || cap.used_program != initial_native)
            return fail(88, "restored ARB pair must bind its original GLES program");
        {
            unsigned last_vp = cap.vp_env_uploads;
            unsigned last_samplers = cap.sampler_upload_calls;
            glDrawArrays(GL_POINTS, 0, 2);
            if (cap.vp_env_uploads != last_vp || cap.sampler_upload_calls != last_samplers)
                return fail(95, "restoring a cached ARB pair must retain its GLES uniform values");
        }
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, second_fragment);
        if (mgl_arb_current_linked_program(&ctx) == NULL ||
            ctx.arb_native_program != secondary_native || cap.link_calls != initial_links)
            return fail(83, "restore alternate ARB pair without relinking");
        glDrawArrays(GL_POINTS, 0, 2);
        if (glGetError() != GL_NO_ERROR || cap.used_program != secondary_native)
            return fail(89, "restored alternate pair must bind its own GLES program");
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, second_fragment);
        if (mgl_arb_current_linked_program(&ctx) == NULL || cap.link_calls != initial_links)
            return fail(84, "redundant binding must not recompile");
        glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                           (GLsizei)strlen(replaced_fragment), replaced_fragment);
        if (glGetError() != GL_NO_ERROR || mgl_arb_current_linked_program(&ctx) == NULL ||
            cap.link_calls <= initial_links)
            return fail(85, "source replacement must recompile inactive cached pair");
        initial_links = cap.link_calls;
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ids[1]);
        if (mgl_arb_current_linked_program(&ctx) == NULL ||
            ctx.arb_native_program != initial_native || cap.link_calls != initial_links)
            return fail(86, "unchanged pair survives another source replacement");
        glDeleteProgramsARB(1, &second_fragment);
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, second_fragment);
        glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                           (GLsizei)strlen(alternative_fragment), alternative_fragment);
        if (glGetError() != GL_NO_ERROR || mgl_arb_current_linked_program(&ctx) == NULL ||
            cap.link_calls <= initial_links)
            return fail(87, "deleted program name cannot restore a stale pair");
        glDeleteProgramsARB(1, &second_fragment);
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ids[1]);
    }

    glDeleteProgramsARB(2, ids);
    if (glIsProgramARB(ids[0]) || glIsProgramARB(ids[1]))
        return fail(18, "program deletion");

    mgl_arb_destroy_native_program(&ctx);
    if (ctx.arb_native_program != 0u || ctx.arb_pair_cache != NULL ||
        ctx.arb_pair_cache_count != 0u)
        return fail(90, "all native pair entries must be released at context destruction");
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL ARB program unit test: PASS");
    return 0;
}

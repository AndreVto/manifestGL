#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_buffer;
    GLuint next_tf;
    GLuint next_program;
    GLuint last_tf_binding;
    GLuint last_buffer_native;
    GLuint last_buffer_index;
    GLintptr last_buffer_offset;
    GLsizeiptr last_buffer_size;
    GLenum last_buffer_target;
    GLenum begin_mode;
    unsigned begin_calls;
    unsigned end_calls;
    unsigned pause_calls;
    unsigned resume_calls;
    unsigned varying_calls;
    unsigned gen_tf_calls;
    unsigned delete_tf_calls;
    unsigned bind_tf_calls;
    unsigned get_varying_calls;
    unsigned link_calls;
    GLsizei varying_count;
    GLenum varying_mode;
    char varying0[64];
    char varying1[64];
} cap;

static void fake_gen_buffers(GLsizei n, GLuint *names) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_buffer;
}

static void fake_delete_buffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)target;
    (void)size;
    (void)data;
    (void)usage;
}

static void fake_bind_buffer_base(GLenum target, GLuint index, GLuint buffer) {
    cap.last_buffer_target = target;
    cap.last_buffer_index = index;
    cap.last_buffer_native = buffer;
    cap.last_buffer_offset = 0;
    cap.last_buffer_size = 0;
}

static void fake_bind_buffer_range(GLenum target, GLuint index, GLuint buffer, GLintptr offset,
                                   GLsizeiptr size) {
    cap.last_buffer_target = target;
    cap.last_buffer_index = index;
    cap.last_buffer_native = buffer;
    cap.last_buffer_offset = offset;
    cap.last_buffer_size = size;
}

static void fake_gen_tf(GLsizei n, GLuint *names) {
    GLsizei i;
    ++cap.gen_tf_calls;
    for (i = 0; i < n; ++i)
        names[i] = ++cap.next_tf;
}

static void fake_delete_tf(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
    ++cap.delete_tf_calls;
}

static void fake_bind_tf(GLenum target, GLuint name) {
    (void)target;
    cap.last_tf_binding = name;
    ++cap.bind_tf_calls;
}

static void fake_begin_tf(GLenum mode) {
    cap.begin_mode = mode;
    ++cap.begin_calls;
}

static void fake_end_tf(void) {
    ++cap.end_calls;
}

static void fake_pause_tf(void) {
    ++cap.pause_calls;
}

static void fake_resume_tf(void) {
    ++cap.resume_calls;
}

static GLuint fake_create_program(void) {
    return ++cap.next_program;
}

static void fake_delete_program(GLuint program) {
    (void)program;
}

static void fake_tf_varyings(GLuint program, GLsizei count, const GLchar *const *varyings,
                             GLenum mode) {
    (void)program;
    ++cap.varying_calls;
    cap.varying_count = count;
    cap.varying_mode = mode;
    cap.varying0[0] = '\0';
    cap.varying1[0] = '\0';
    if (count > 0)
        (void)snprintf(cap.varying0, sizeof(cap.varying0), "%s", varyings[0]);
    if (count > 1)
        (void)snprintf(cap.varying1, sizeof(cap.varying1), "%s", varyings[1]);
}

static void fake_link_program(GLuint program) {
    (void)program;
    ++cap.link_calls;
}

static void fake_get_program_iv(GLuint program, GLenum pname, GLint *params) {
    (void)program;
    if (pname == GL_LINK_STATUS)
        *params = GL_TRUE;
    else if (pname == GL_INFO_LOG_LENGTH)
        *params = 1;
    else if (pname == GL_ACTIVE_ATTRIBUTES || pname == GL_ACTIVE_ATTRIBUTE_MAX_LENGTH)
        *params = 0;
    else
        *params = 0;
}

static void fake_get_program_log(GLuint program, GLsizei size, GLsizei *length, GLchar *log) {
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

static void fake_get_tf_varying(GLuint program, GLuint index, GLsizei buf_size, GLsizei *length,
                                GLsizei *size, GLenum *type, GLchar *name) {
    const char *src = index == 0u ? "position_out" : "color_out";
    size_t n = strlen(src);
    (void)program;
    ++cap.get_varying_calls;
    if (size != NULL)
        *size = 1;
    if (type != NULL)
        *type = GL_FLOAT;
    if (buf_size > 0 && name != NULL) {
        size_t copy = n < (size_t)(buf_size - 1) ? n : (size_t)(buf_size - 1);
        memcpy(name, src, copy);
        name[copy] = '\0';
        if (length != NULL)
            *length = (GLsizei)copy;
    } else if (length != NULL)
        *length = 0;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "transform feedback failure %d: %s\n", n, message);
    return n;
}

static void init_context(struct mgl_context *ctx, struct mgl_native_dispatch *native) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->native_dispatch = native;
    ctx->share_group = mgl_share_group_create();
    mgl_error_init(&ctx->errors);
    mgl_compat_state_init(&ctx->compat);
    mgl_vertex_array_init(&ctx->default_vao);
    ctx->current_vao = &ctx->default_vao;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint tf[2] = {0u, 0u};
    GLuint buffers[2] = {0u, 0u};
    GLint iv = -1;
    GLint64 i64 = -1;
    GLuint program;
    char varying_a[32] = "position_out";
    char varying_b[32] = "color_out";
    const GLchar *varyings[2] = {varying_a, varying_b};
    GLchar name[32];
    GLsizei length = 0, size = 0;
    GLenum type = 0u;

    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.next_buffer = 100u;
    cap.next_tf = 200u;
    cap.next_program = 300u;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesBindBufferBase = fake_bind_buffer_base;
    native.glesBindBufferRange = fake_bind_buffer_range;
    native.glesGenTransformFeedbacks = fake_gen_tf;
    native.glesDeleteTransformFeedbacks = fake_delete_tf;
    native.glesBindTransformFeedback = fake_bind_tf;
    native.glesBeginTransformFeedback = fake_begin_tf;
    native.glesEndTransformFeedback = fake_end_tf;
    native.glesPauseTransformFeedback = fake_pause_tf;
    native.glesResumeTransformFeedback = fake_resume_tf;
    native.glesCreateProgram = fake_create_program;
    native.glesDeleteProgram = fake_delete_program;
    native.glesTransformFeedbackVaryings = fake_tf_varyings;
    native.glesLinkProgram = fake_link_program;
    native.glesGetProgramiv = fake_get_program_iv;
    native.glesGetProgramInfoLog = fake_get_program_log;
    native.glesGetAttribLocation = fake_get_attrib_location;
    native.glesGetUniformLocation = fake_get_uniform_location;
    native.glesGetTransformFeedbackVarying = fake_get_tf_varying;

    init_context(&ctx, &native);
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_context_set_current_for_test(&ctx);

    glGenTransformFeedbacks(2, tf);
    if (tf[0] == 0u || tf[1] == 0u || tf[0] == tf[1])
        return fail(2, "generation");
    if (glIsTransformFeedback(tf[0]))
        return fail(3, "generated name should not be object until bound");
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf[0]);
    if (!glIsTransformFeedback(tf[0]) || cap.last_tf_binding == 0u)
        return fail(4, "bind/realize");
    /* The same public GL path must reject every transform-feedback API while
     * immediate geometry is open, without affecting objects or the native. */
    {
        const unsigned gen_before = cap.gen_tf_calls;
        const unsigned delete_before = cap.delete_tf_calls;
        const unsigned bind_before = cap.bind_tf_calls;
        const unsigned varying_before = cap.varying_calls;
        const unsigned get_before = cap.get_varying_calls;
        GLuint untouched = 777u;
        GLsizei untouched_length = 43;
        GLsizei untouched_size = 43;
        GLenum untouched_type = 0xeeeeu;
        GLchar untouched_name[8] = "marker";
        glBegin(GL_POINTS);
        glGenTransformFeedbacks(1, &untouched);
        glCreateTransformFeedbacks(1, &untouched);
        glDeleteTransformFeedbacks(1, &tf[0]);
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0u);
        (void)glIsTransformFeedback(tf[0]);
        glBeginTransformFeedback(GL_POINTS);
        glEndTransformFeedback();
        glPauseTransformFeedback();
        glResumeTransformFeedback();
        glTransformFeedbackVaryings(0u, -1, NULL, 0u);
        glGetTransformFeedbackVarying(0u, 0u, -1, &untouched_length, &untouched_size,
                                      &untouched_type, untouched_name);
        glEnd();
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || untouched != 777u ||
            untouched_length != 43 || untouched_size != 43 || untouched_type != 0xeeeeu ||
            strcmp(untouched_name, "marker") != 0 || ctx.transform_feedback_binding != tf[0] ||
            !glIsTransformFeedback(tf[0]) || cap.gen_tf_calls != gen_before ||
            cap.delete_tf_calls != delete_before || cap.bind_tf_calls != bind_before ||
            cap.varying_calls != varying_before || cap.get_varying_calls != get_before ||
            cap.begin_calls != 0u || cap.end_calls != 0u || cap.pause_calls != 0u ||
            cap.resume_calls != 0u)
            return fail(23, "illegal Begin/End changed transform-feedback state or GLES callbacks");
    }
    glGetIntegerv(GL_TRANSFORM_FEEDBACK_BINDING, &iv);
    if ((GLuint)iv != tf[0])
        return fail(5, "binding query");

    glGenBuffers(2, buffers);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffers[0]);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 64, NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0u, buffers[0]);
    glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0u, &iv);
    if ((GLuint)iv != buffers[0] || cap.last_buffer_target != GL_TRANSFORM_FEEDBACK_BUFFER ||
        cap.last_buffer_index != 0u)
        return fail(6, "object-local base binding");

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf[1]);
    glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0u, &iv);
    if (iv != 0)
        return fail(7, "second object must have independent indexed bindings");
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffers[1]);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 64, NULL, GL_DYNAMIC_DRAW);
    glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0u, buffers[1], 8, 16);
    glGetInteger64i_v(GL_TRANSFORM_FEEDBACK_BUFFER_START, 0u, &i64);
    if (i64 != 8)
        return fail(8, "range start query");
    glGetInteger64i_v(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, 0u, &i64);
    if (i64 != 16 || cap.last_buffer_offset != 8 || cap.last_buffer_size != 16)
        return fail(9, "range size/native");

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf[0]);
    glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0u, &iv);
    if ((GLuint)iv != buffers[0])
        return fail(10, "restored object-local binding");

    glTransformFeedbackBufferBase(tf[1], 1u, buffers[0]);
    if (glGetError() != GL_NO_ERROR || ctx.transform_feedback_binding != tf[0] ||
        cap.last_tf_binding != cap.next_tf - 1u)
        return fail(24, "named base changes current binding");
    glGetTransformFeedbacki_v(tf[1], GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 1u, &iv);
    if (glGetError() != GL_NO_ERROR || (GLuint)iv != buffers[0])
        return fail(25, "named base query");
    glTransformFeedbackBufferRange(tf[1], 1u, buffers[1], 8, 16);
    glGetTransformFeedbacki64_v(tf[1], GL_TRANSFORM_FEEDBACK_BUFFER_START, 1u, &i64);
    if (glGetError() != GL_NO_ERROR || i64 != 8)
        return fail(26, "named range start");
    glGetTransformFeedbacki64_v(tf[1], GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, 1u, &i64);
    if (glGetError() != GL_NO_ERROR || i64 != 16)
        return fail(27, "named range size");
    glGetTransformFeedbackiv(tf[1], GL_TRANSFORM_FEEDBACK_ACTIVE, &iv);
    if (glGetError() != GL_NO_ERROR || iv != GL_FALSE)
        return fail(28, "named inactive query");
    glGetTransformFeedbackiv(tf[1], GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &iv);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(29, "named query selector");
    glTransformFeedbackBufferRange(tf[1], 1u, buffers[1], 61, 4);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(30, "named out of range");
    glTransformFeedbackBufferBase(tf[1], 1u, 0xffffffffu);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(31, "named unknown buffer");
    glTransformFeedbackBufferBase(tf[1], 15u, buffers[0]);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(32, "named buffer index");
    glTransformFeedbackBufferBase(0xffffffffu, 1u, buffers[0]);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(33, "named unknown tf");

    glBeginTransformFeedback(GL_TRIANGLES);
    glGetIntegerv(GL_TRANSFORM_FEEDBACK_ACTIVE, &iv);
    if (!iv || cap.begin_calls != 1u || cap.begin_mode != GL_TRIANGLES)
        return fail(11, "begin/active");
    glGetTransformFeedbackiv(tf[0], GL_TRANSFORM_FEEDBACK_ACTIVE, &iv);
    if (glGetError() != GL_NO_ERROR || !iv)
        return fail(34, "active named query");
    glTransformFeedbackBufferBase(tf[0], 2u, buffers[0]);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(35, "mutate active tf");
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf[1]);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(12, "bind while active");
    glPauseTransformFeedback();
    glGetIntegerv(GL_TRANSFORM_FEEDBACK_PAUSED, &iv);
    if (!iv || cap.pause_calls != 1u)
        return fail(13, "pause");
    glResumeTransformFeedback();
    if (cap.resume_calls != 1u)
        return fail(14, "resume");
    glEndTransformFeedback();
    if (cap.end_calls != 1u)
        return fail(15, "end");

    program = glCreateProgram();
    if (program == 0u)
        return fail(16, "program creation");
    glTransformFeedbackVaryings(program, 2, varyings, GL_SEPARATE_ATTRIBS);
    varying_a[0] = 'X';
    varying_b[0] = 'Y';
    glGetProgramiv(program, GL_TRANSFORM_FEEDBACK_VARYINGS, &iv);
    if (iv != 2)
        return fail(17, "varying count query");
    glGetProgramiv(program, GL_TRANSFORM_FEEDBACK_BUFFER_MODE, &iv);
    if (iv != (GLint)GL_SEPARATE_ATTRIBS)
        return fail(18, "buffer mode query");
    glLinkProgram(program);
    if (cap.varying_calls != 1u || cap.link_calls != 1u ||
        strcmp(cap.varying0, "position_out") != 0 || strcmp(cap.varying1, "color_out") != 0)
        return fail(19, "varyings retained and applied at link");
    memset(name, 0, sizeof(name));
    glGetTransformFeedbackVarying(program, 1u, (GLsizei)sizeof(name), &length, &size, &type, name);
    if (strcmp(name, "color_out") != 0 || length != 9 || size != 1 || type != GL_FLOAT)
        return fail(20, "varying reflection");

    glDeleteBuffers(1, &buffers[0]);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf[0]);
    glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0u, &iv);
    if (iv != 0)
        return fail(21, "buffer deletion clears tf-object binding");

    glDeleteProgram(program);
    glDeleteTransformFeedbacks(2, tf);
    glDeleteBuffers(1, &buffers[1]);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL transform feedback unit test: PASS");
    return 0;
}

#include "mgl/gl.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "shader/translator.h"
#include "state/state.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_name;
    unsigned char *array_data;
    size_t array_size;
    unsigned char *element_data;
    size_t element_size;
    GLenum draw_mode;
    GLenum draw_type;
    GLsizei draw_count;
    unsigned draw_calls;
} cap;

static void fake_get_integerv(GLenum pname, GLint *value) {
    *value = pname == GL_MAX_VERTEX_ATTRIBS ? 16 : 0;
}

static void fake_gen_buffers(GLsizei n, GLuint *out) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        out[i] = ++cap.next_name;
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
    unsigned char **dst;
    size_t *dst_size;
    (void)usage;
    if (target == GL_ARRAY_BUFFER) {
        dst = &cap.array_data;
        dst_size = &cap.array_size;
    } else if (target == GL_ELEMENT_ARRAY_BUFFER) {
        dst = &cap.element_data;
        dst_size = &cap.element_size;
    } else
        return;
    free(*dst);
    *dst = NULL;
    *dst_size = 0u;
    if (size > 0 && data != NULL) {
        *dst = malloc((size_t)size);
        if (*dst != NULL) {
            memcpy(*dst, data, (size_t)size);
            *dst_size = (size_t)size;
        }
    }
}

static void fake_vertex_attrib_pointer(GLuint i, GLint s, GLenum t, GLboolean n, GLsizei stride,
                                       const void *p) {
    (void)i;
    (void)s;
    (void)t;
    (void)n;
    (void)stride;
    (void)p;
}

static void fake_enable_attrib(GLuint i) {
    (void)i;
}

static void fake_disable_attrib(GLuint i) {
    (void)i;
}

static void fake_attrib4fv(GLuint i, const GLfloat *v) {
    (void)i;
    (void)v;
}

static void fake_use_program(GLuint p) {
    (void)p;
}

static void fake_enable(GLenum cap) {
    (void)cap;
}

static void fake_disable(GLenum cap) {
    (void)cap;
}

static void fake_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    (void)first;
    cap.draw_mode = mode;
    cap.draw_type = 0u;
    cap.draw_count = count;
    ++cap.draw_calls;
}

static void fake_draw_elements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    (void)indices;
    cap.draw_mode = mode;
    cap.draw_type = type;
    cap.draw_count = count;
    ++cap.draw_calls;
}

static void init_native(struct mgl_native_dispatch *native) {
    memset(native, 0, sizeof(*native));
    native->glesGetIntegerv = fake_get_integerv;
    native->glesGenBuffers = fake_gen_buffers;
    native->glesDeleteBuffers = fake_delete_buffers;
    native->glesBindBuffer = fake_bind_buffer;
    native->glesBufferData = fake_buffer_data;
    native->glesVertexAttribPointer = fake_vertex_attrib_pointer;
    native->glesEnableVertexAttribArray = fake_enable_attrib;
    native->glesDisableVertexAttribArray = fake_disable_attrib;
    native->glesVertexAttrib4fv = fake_attrib4fv;
    native->glesUseProgram = fake_use_program;
    native->glesEnable = fake_enable;
    native->glesDisable = fake_disable;
    native->glesDrawArrays = fake_draw_arrays;
    native->glesDrawElements = fake_draw_elements;
}

static int fail(int n, const char *why) {
    fprintf(stderr, "display-list geometry failure %d: %s\n", n, why);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLuint list;
    size_t i;

    memset(&ctx, 0, sizeof(ctx));
    memset(&cap, 0, sizeof(cap));
    init_native(&native);
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_state_init(&ctx.canonical_state, 1, 1);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.native_max_vertex_attribs = 16;
    ctx.compat_current[MGL_COMPAT_SEM_COLOR][0] = 1.0f;
    ctx.compat_current[MGL_COMPAT_SEM_COLOR][1] = 1.0f;
    ctx.compat_current[MGL_COMPAT_SEM_COLOR][2] = 1.0f;
    ctx.compat_current[MGL_COMPAT_SEM_COLOR][3] = 1.0f;
    mgl_context_set_current_for_test(&ctx);

    program = calloc(1, sizeof(*program));
    if (program == NULL)
        return fail(2, "program");
    program->name = 5u;
    program->native_name = 50u;
    program->linked = GL_TRUE;
    for (i = 0; i < MGL_COMPAT_SEMANTIC_COUNT; ++i)
        program->compat_attrib[i] = -1;
    program->compat_attrib[MGL_COMPAT_SEM_POSITION] = 0;
    program->compat_attrib[MGL_COMPAT_SEM_COLOR] = 1;
    program->compat_uniform_model_view_projection = -1;
    program->compat_uniform_model_view = -1;
    program->compat_uniform_projection = -1;
    program->compat_uniform_normal_matrix = -1;
    program->next = ctx.share_group->programs;
    ctx.share_group->programs = program;
    ctx.current_program = program->name;

    list = glGenLists(1);
    if (list == 0u)
        return fail(3, "list name");
    glNewList(list, GL_COMPILE);
    glEnable(GL_FOG);
    glBegin(GL_QUADS);
    glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    glVertex2f(-1.0f, -1.0f);
    glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
    glVertex2f(1.0f, -1.0f);
    glColor4f(0.0f, 0.0f, 1.0f, 1.0f);
    glVertex2f(1.0f, 1.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);
    glEnd();
    glEndList();

    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(4, "compile error");
    if (cap.draw_calls != 0u)
        return fail(5, "GL_COMPILE rendered geometry");
    if (ctx.compat.fog_enabled)
        return fail(6, "GL_COMPILE executed enable");
    if (ctx.compat_current[MGL_COMPAT_SEM_COLOR][0] != 1.0f ||
        ctx.compat_current[MGL_COMPAT_SEM_COLOR][1] != 1.0f ||
        ctx.compat_current[MGL_COMPAT_SEM_COLOR][2] != 1.0f)
        return fail(7, "GL_COMPILE changed current color");

    glCallList(list);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(8, "replay error");
    if (!ctx.compat.fog_enabled)
        return fail(9, "replay did not enable fog");
    if (cap.draw_calls != 1u || cap.draw_mode != GL_TRIANGLES || cap.draw_count != 6 ||
        cap.draw_type != GL_UNSIGNED_BYTE)
        return fail(10, "replayed quad geometry");
    if (ctx.compat_current[MGL_COMPAT_SEM_COLOR][0] != 1.0f ||
        ctx.compat_current[MGL_COMPAT_SEM_COLOR][1] != 1.0f ||
        ctx.compat_current[MGL_COMPAT_SEM_COLOR][2] != 1.0f)
        return fail(11, "final replay current color");

    glDeleteLists(list, 1);
    free(cap.array_data);
    free(cap.element_data);
    free(ctx.immediate.vertices);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL display-list geometry unit test: PASS");
    return 0;
}

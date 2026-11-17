#include "mgl/gl.h"
#include "core/context.h"
#include "compat/fixed_function.h"
#include "objects/object_store.h"
#include "shader/translator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERTICES 4096

struct fake_buffer {
    unsigned char *data;
    size_t size;
};

static struct {
    GLuint next_name;
    GLuint array_binding;
    GLuint element_binding;
    struct fake_buffer array;
    struct fake_buffer element;
    size_t attrib_offset;
    GLint attrib_size;
    unsigned draws;
    unsigned indexed_draws;
    unsigned failures;
    float expected;
} capture;

static void gen_buffers(GLsizei count, GLuint *names) {
    GLsizei i;
    for (i = 0; i < count; ++i)
        names[i] = ++capture.next_name;
}

static void bind_buffer(GLenum target, GLuint name) {
    if (target == GL_ARRAY_BUFFER)
        capture.array_binding = name;
    if (target == GL_ELEMENT_ARRAY_BUFFER)
        capture.element_binding = name;
}

static void buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    struct fake_buffer *buffer = target == GL_ARRAY_BUFFER ? &capture.array : &capture.element;
    (void)usage;
    if (size < 0) {
        ++capture.failures;
        return;
    }
    free(buffer->data);
    buffer->data = malloc((size_t)size);
    buffer->size = buffer->data == NULL ? 0u : (size_t)size;
    if (data != NULL && buffer->data != NULL)
        memcpy(buffer->data, data, (size_t)size);
}

static void buffer_subdata(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    struct fake_buffer *buffer = target == GL_ARRAY_BUFFER ? &capture.array : &capture.element;
    if (offset < 0 || size < 0 || (size_t)offset > buffer->size ||
        (size_t)size > buffer->size - (size_t)offset) {
        ++capture.failures;
        return;
    }
    memcpy(buffer->data + offset, data, (size_t)size);
}

static void vertex_pointer(GLuint location, GLint size, GLenum type, GLboolean normalized,
                           GLsizei stride, const void *pointer) {
    (void)normalized;
    (void)stride;
    if (location == 2u) {
        if ((size != 3 && size != 4) || type != GL_FLOAT)
            ++capture.failures;
        capture.attrib_size = size;
        capture.attrib_offset = (size_t)(uintptr_t)pointer;
    }
}

static void enable_attrib(GLuint location) {
    (void)location;
}

static void disable_attrib(GLuint location) {
    (void)location;
}

static void attrib4fv(GLuint location, const GLfloat *value) {
    (void)location;
    (void)value;
}

static void use_program(GLuint name) {
    (void)name;
}

static void check_attribute(GLint index) {
    const size_t offset =
        capture.attrib_offset + (size_t)index * (size_t)capture.attrib_size * sizeof(GLfloat);
    GLfloat value;
    if (offset > capture.array.size || sizeof(value) > capture.array.size - offset) {
        ++capture.failures;
        return;
    }
    memcpy(&value, capture.array.data + offset, sizeof(value));
    if (value != capture.expected + (float)index)
        ++capture.failures;
}

static void draw_arrays(GLenum mode, GLint first, GLsizei count) {
    if (first != 0)
        ++capture.failures;
    if (mode == GL_POINTS && count == VERTICES) {
        check_attribute(0);
        check_attribute(VERTICES - 1);
    } else if (mode == GL_TRIANGLE_FAN && count == 4) {
        check_attribute(0);
        check_attribute(3);
    } else
        ++capture.failures;
    ++capture.draws;
}

static void draw_elements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    size_t offset = (size_t)(uintptr_t)indices;
    GLuint first;
    if (mode != GL_TRIANGLES || count != 3 || type != GL_UNSIGNED_BYTE)
        ++capture.failures;
    if (offset > capture.element.size || sizeof(GLubyte) > capture.element.size - offset) {
        ++capture.failures;
        return;
    }
    first = capture.element.data[offset];
    if (first != 0u)
        ++capture.failures;
    check_attribute(0);
    ++capture.indexed_draws;
}

static int fail(int number, const char *description) {
    fprintf(stderr, "scratch upload reuse test %d: %s\n", number, description);
    return number;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLfloat positions[VERTICES][3];
    const GLushort indices[3] = {0u, 1u, 2u};
    unsigned i, frame;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&capture, 0, sizeof(capture));
    native.glesGenBuffers = gen_buffers;
    native.glesBindBuffer = bind_buffer;
    native.glesBufferData = buffer_data;
    native.glesBufferSubData = buffer_subdata;
    native.glesVertexAttribPointer = vertex_pointer;
    native.glesEnableVertexAttribArray = enable_attrib;
    native.glesDisableVertexAttribArray = disable_attrib;
    native.glesVertexAttrib4fv = attrib4fv;
    native.glesUseProgram = use_program;
    native.glesDrawArrays = draw_arrays;
    native.glesDrawElements = draw_elements;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 1, 1);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.native_max_vertex_attribs = 16;
    mgl_context_set_current_for_test(&ctx);
    program = calloc(1u, sizeof(*program));
    if (program == NULL)
        return fail(2, "program");
    program->name = 23u;
    program->linked = GL_TRUE;
    for (i = 0u; i < MGL_COMPAT_SEMANTIC_COUNT; ++i)
        program->compat_attrib[i] = -1;
    program->compat_attrib[MGL_COMPAT_SEM_POSITION] = 2;
    program->next = ctx.share_group->programs;
    ctx.share_group->programs = program;
    ctx.current_program = program->name;
    glVertexPointer(3, GL_FLOAT, 0, positions);
    glEnableClientState(GL_VERTEX_ARRAY);
    for (frame = 0u; frame < 100u; ++frame) {
        capture.expected = (float)(frame * 10000u);
        for (i = 0u; i < VERTICES; ++i) {
            positions[i][0] = capture.expected + (float)i;
            positions[i][1] = 12.0f;
            positions[i][2] = -12.0f;
        }
        glDrawArrays(GL_POINTS, 0, VERTICES);
        if (capture.failures != 0u || glGetError() != GL_NO_ERROR)
            return fail(3, "client vertex array contents");
    }
    if (capture.draws != 100u)
        return fail(4, "draw count");
    capture.expected = 7777.0f;
    for (i = 0u; i < VERTICES; ++i)
        positions[i][0] = capture.expected + (float)i;
    for (i = 0u; i < 40u; ++i) {
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, indices);
        if (capture.failures != 0u || glGetError() != GL_NO_ERROR)
            return fail(5, "indexed data and attributes");
    }
    if (capture.indexed_draws != 40u)
        return fail(6, "indexed draw count");
    capture.expected = 101.0f;
    glBegin(GL_POLYGON);
    glVertex3f(101.0f, 0.0f, 0.0f);
    glVertex3f(102.0f, 0.0f, 0.0f);
    glVertex3f(103.0f, 0.0f, 0.0f);
    glVertex3f(104.0f, 0.0f, 0.0f);
    glEnd();
    if (capture.failures != 0u || glGetError() != GL_NO_ERROR || capture.draws != 101u)
        return fail(7, "polygon fan without intermediate index upload");
    free(ctx.scratch_cpu_storage);
    free(capture.array.data);
    free(capture.element.data);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL scratch upload reuse unit test: PASS");
    return 0;
}

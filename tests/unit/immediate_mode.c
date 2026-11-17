#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "shader/translator.h"

#include <math.h>
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
} cap;

static void fake_get_integerv(GLenum pname, GLint *value) {
    if (pname == GL_MAX_VERTEX_ATTRIBS)
        *value = 16;
    else
        *value = 0;
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
    } else {
        return;
    }
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

static void fake_vertex_attrib_pointer(GLuint location, GLint size, GLenum type,
                                       GLboolean normalized, GLsizei stride, const void *pointer) {
    (void)location;
    (void)size;
    (void)type;
    (void)normalized;
    (void)stride;
    (void)pointer;
}

static void fake_enable_vertex_attrib(GLuint index) {
    (void)index;
}

static void fake_disable_vertex_attrib(GLuint index) {
    (void)index;
}

static void fake_vertex_attrib4fv(GLuint index, const GLfloat *v) {
    (void)index;
    (void)v;
}

static void fake_use_program(GLuint program) {
    (void)program;
}

static void fake_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    (void)first;
    cap.draw_mode = mode;
    cap.draw_count = count;
    cap.draw_type = 0u;
}

static void fake_draw_elements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    (void)indices;
    cap.draw_mode = mode;
    cap.draw_count = count;
    cap.draw_type = type;
}

static void init_native(struct mgl_native_dispatch *native) {
    memset(native, 0, sizeof(*native));
    native->glesGetIntegerv = fake_get_integerv;
    native->glesGenBuffers = fake_gen_buffers;
    native->glesDeleteBuffers = fake_delete_buffers;
    native->glesBindBuffer = fake_bind_buffer;
    native->glesBufferData = fake_buffer_data;
    native->glesVertexAttribPointer = fake_vertex_attrib_pointer;
    native->glesEnableVertexAttribArray = fake_enable_vertex_attrib;
    native->glesDisableVertexAttribArray = fake_disable_vertex_attrib;
    native->glesVertexAttrib4fv = fake_vertex_attrib4fv;
    native->glesUseProgram = fake_use_program;
    native->glesDrawArrays = fake_draw_arrays;
    native->glesDrawElements = fake_draw_elements;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "immediate-mode failure %d: %s\n", n, message);
    return n;
}

static int nearf(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 0.0001f;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    const GLfloat expected_positions[4][4] = {{-1.0f, -1.0f, 0.0f, 1.0f},
                                              {1.0f, -1.0f, 0.0f, 1.0f},
                                              {1.0f, 1.0f, 0.0f, 1.0f},
                                              {-1.0f, 1.0f, 0.0f, 1.0f}};
    const GLfloat expected_colors[4][4] = {{1.0f, 0.0f, 0.0f, 1.0f},
                                           {0.0f, 1.0f, 0.0f, 1.0f},
                                           {0.0f, 0.0f, 1.0f, 1.0f},
                                           {1.0f, 1.0f, 1.0f, 1.0f}};
    const GLubyte expected_indices[6] = {0u, 1u, 2u, 0u, 2u, 3u};
    const GLfloat *packed;
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
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.native_max_vertex_attribs = 16;
    mgl_context_set_current_for_test(&ctx);

    program = calloc(1, sizeof(*program));
    if (program == NULL)
        return fail(2, "program allocation");
    program->name = 11u;
    program->native_name = 111u;
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

    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(3, "interface error");
    if (cap.draw_mode != GL_TRIANGLES || cap.draw_count != 6 || cap.draw_type != GL_UNSIGNED_BYTE)
        return fail(4, "quad conversion did not emit six triangle indices");
    if (cap.element_size != sizeof(expected_indices) || cap.element_data == NULL ||
        memcmp(cap.element_data, expected_indices, sizeof(expected_indices)) != 0)
        return fail(5, "quad index order");

    if (cap.array_size != sizeof(expected_positions) + sizeof(expected_colors) ||
        cap.array_data == NULL)
        return fail(6, "packed immediate vertex size");
    packed = (const GLfloat *)cap.array_data;
    for (i = 0; i < 16u; ++i) {
        if (!nearf(packed[i], ((const GLfloat *)expected_positions)[i]))
            return fail(7, "position snapshot");
    }
    packed += 16;
    for (i = 0; i < 16u; ++i) {
        if (!nearf(packed[i], ((const GLfloat *)expected_colors)[i]))
            return fail(8, "current color was not captured per vertex");
    }

    {
        size_t previous_count = ctx.immediate.count;
        glVertex3f(0.0f, 0.0f, 0.0f);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || ctx.immediate.count != previous_count)
            return fail(9, "glVertex outside glBegin/glEnd must be ignored");
    }
    glBegin(GL_TRIANGLES);
    glBegin(GL_TRIANGLES);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(10, "nested glBegin");
    glEnd();

    {
        const GLuint unsigned_p4 = 5u | (6u << 10) | (7u << 20) | (3u << 30);
        const GLuint signed_p4 = 0x3ffu | (2u << 10) | (0x200u << 20) | (3u << 30);
        const GLuint unsigned_p3 = 9u | (10u << 10) | (11u << 20);
        const GLuint signed_p2 = 0x3feu | (3u << 10);
        const struct mgl_immediate_vertex *v;

        glBegin(GL_POINTS);
        glVertexP4ui(GL_UNSIGNED_INT_2_10_10_10_REV, unsigned_p4);
        glVertexP4uiv(GL_INT_2_10_10_10_REV, &signed_p4);
        glVertexP3uiv(GL_UNSIGNED_INT_2_10_10_10_REV, &unsigned_p3);
        glVertexP2uiv(GL_INT_2_10_10_10_REV, &signed_p2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || ctx.immediate.count != 4u)
            return fail(11, "packed vertex append");
        v = ctx.immediate.vertices;
        if (!nearf(v[0].position[0], 5.0f) || !nearf(v[0].position[1], 6.0f) ||
            !nearf(v[0].position[2], 7.0f) || !nearf(v[0].position[3], 3.0f))
            return fail(12, "unsigned packed vertex conversion");
        if (!nearf(v[1].position[0], -1.0f) || !nearf(v[1].position[1], 2.0f) ||
            !nearf(v[1].position[2], -512.0f) || !nearf(v[1].position[3], -1.0f))
            return fail(13, "signed packed vertex conversion");
        if (!nearf(v[2].position[0], 9.0f) || !nearf(v[2].position[1], 10.0f) ||
            !nearf(v[2].position[2], 11.0f) || !nearf(v[2].position[3], 1.0f))
            return fail(14, "packed vertex3 defaults");
        if (!nearf(v[3].position[0], -2.0f) || !nearf(v[3].position[1], 3.0f) ||
            !nearf(v[3].position[2], 0.0f) || !nearf(v[3].position[3], 1.0f))
            return fail(15, "packed vertex2 defaults");
        glEnd();
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(16, "packed point draw");

        glBegin(GL_POINTS);
        glVertexP2ui(GL_FLOAT, 0u);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM || ctx.immediate.count != 0u)
            return fail(17, "packed vertex type validation");
        glEnd();

        if (glXGetProcAddress((const GLubyte *)"glVertexP2ui") == NULL ||
            glXGetProcAddress((const GLubyte *)"glVertexP2uiv") == NULL ||
            glXGetProcAddress((const GLubyte *)"glVertexP3ui") == NULL ||
            glXGetProcAddress((const GLubyte *)"glVertexP3uiv") == NULL ||
            glXGetProcAddress((const GLubyte *)"glVertexP4ui") == NULL ||
            glXGetProcAddress((const GLubyte *)"glVertexP4uiv") == NULL)
            return fail(18, "packed vertex proc surface");
    }

    {
        const GLfloat positions[2][3] = {{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}};
        const GLubyte colors[2][4] = {{1u, 2u, 3u, 4u}, {64u, 128u, 255u, 32u}};
        const GLshort normals[2][3] = {{0, 0, 32767}, {16384, -32768, 32767}};
        const GLubyte secondary[2][3] = {{0u, 0u, 0u}, {255u, 128u, 0u}};
        const GLfloat texcoords[2][2] = {{0.0f, 0.0f}, {0.25f, 0.75f}};
        const struct mgl_immediate_vertex *v;
        GLuint vbo = 0u;
        void *mapped;

        glBindBuffer(GL_ARRAY_BUFFER, 0u);
        glVertexPointer(3, GL_FLOAT, 0, positions);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
        glNormalPointer(GL_SHORT, 0, normals);
        glSecondaryColorPointer(3, GL_UNSIGNED_BYTE, 0, secondary);
        glClientActiveTexture(GL_TEXTURE1);
        glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_SECONDARY_COLOR_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        glBegin(GL_POINTS);
        glArrayElement(1);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || ctx.immediate.count != 1u)
            return fail(19, "array element append");
        v = &ctx.immediate.vertices[0];
        if (!nearf(v->position[0], 4.0f) || !nearf(v->position[1], 5.0f) ||
            !nearf(v->position[2], 6.0f) || !nearf(v->position[3], 1.0f))
            return fail(20, "array element position");
        if (!nearf(v->color[0], 64.0f / 255.0f) || !nearf(v->color[1], 128.0f / 255.0f) ||
            !nearf(v->color[2], 1.0f) || !nearf(v->color[3], 32.0f / 255.0f))
            return fail(21, "array element normalized color");
        if (!nearf(v->normal[0], 16384.0f / 32767.0f) || !nearf(v->normal[1], -1.0f) ||
            !nearf(v->normal[2], 1.0f))
            return fail(22, "array element normalized normal");
        if (!nearf(v->secondary[0], 1.0f) || !nearf(v->secondary[1], 128.0f / 255.0f) ||
            !nearf(v->secondary[2], 0.0f))
            return fail(23, "array element secondary color");
        if (!nearf(v->texcoord[1][0], 0.25f) || !nearf(v->texcoord[1][1], 0.75f) ||
            !nearf(v->texcoord[1][2], 0.0f) || !nearf(v->texcoord[1][3], 1.0f))
            return fail(24, "array element texture coordinate");
        glEnd();
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(25, "array element draw");

        glDisableClientState(GL_VERTEX_ARRAY);
        glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
        glArrayElement(0);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            !nearf(ctx.compat_current[MGL_COMPAT_SEM_COLOR][0], 1.0f / 255.0f) ||
            !nearf(ctx.compat_current[MGL_COMPAT_SEM_COLOR][3], 4.0f / 255.0f))
            return fail(26, "array element attributes without vertex array");

        glArrayElement(-1);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
            return fail(27, "array element negative index");

        glEnableClientState(GL_VERTEX_ARRAY);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(positions), positions, GL_STATIC_DRAW);
        glVertexPointer(3, GL_FLOAT, 0, (const void *)0);
        mapped = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
        if (mapped == NULL || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(28, "array element mapped-buffer setup");
        glBegin(GL_POINTS);
        glArrayElement(0);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION || ctx.immediate.count != 0u)
            return fail(29, "array element mapped-buffer rejection");
        glEnd();
        if (!glUnmapBuffer(GL_ARRAY_BUFFER) || mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(30, "array element mapped-buffer cleanup");
        glDeleteBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, 0u);

        {
            GLfloat list_positions[1][3] = {{7.0f, 8.0f, 9.0f}};
            GLubyte list_colors[1][4] = {{10u, 20u, 30u, 40u}};
            GLuint list;
            const GLfloat *list_upload;
            glVertexPointer(3, GL_FLOAT, 0, list_positions);
            glColorPointer(4, GL_UNSIGNED_BYTE, 0, list_colors);
            glDisableClientState(GL_NORMAL_ARRAY);
            glDisableClientState(GL_SECONDARY_COLOR_ARRAY);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            list = glGenLists(1);
            if (list == 0u)
                return fail(31, "array element display-list allocation");
            glNewList(list, GL_COMPILE);
            glBegin(GL_POINTS);
            glArrayElement(0);
            glEnd();
            glEndList();
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
                return fail(32, "array element display-list compile");
            list_positions[0][0] = 70.0f;
            list_colors[0][0] = 200u;
            glCallList(list);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.array_data == NULL ||
                cap.array_size < 8u * sizeof(GLfloat))
                return fail(33, "array element display-list replay");
            list_upload = (const GLfloat *)cap.array_data;
            if (!nearf(list_upload[0], 7.0f) || !nearf(list_upload[1], 8.0f) ||
                !nearf(list_upload[2], 9.0f) || !nearf(list_upload[4], 10.0f / 255.0f))
                return fail(34, "array element display-list captured source data");
            glDeleteLists(list, 1);
        }

        if (glXGetProcAddress((const GLubyte *)"glArrayElement") == NULL)
            return fail(35, "array element proc surface");
        glClientActiveTexture(GL_TEXTURE0);
    }

    free(cap.array_data);
    free(cap.element_data);
    free(ctx.immediate.vertices);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL immediate-mode semantic unit test: PASS");
    return 0;
}

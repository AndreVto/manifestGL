#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_name;
    GLuint bound_array;
    GLuint bound_element;
    GLsizei arrays_instances;
    GLsizei elements_instances;
    unsigned arrays_instanced_calls;
    unsigned elements_instanced_calls;
    GLuint divisor_location;
    GLuint divisor_value;
    size_t attrib_offset[16];
    unsigned char array_upload[512];
    size_t array_upload_size;
} cap;

static void fake_get_integerv(GLenum pname, GLint *v) {
    *v = pname == GL_MAX_VERTEX_ATTRIBS ? 16 : 0;
}

static void fake_gen_buffers(GLsizei n, GLuint *out) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        out[i] = ++cap.next_name;
}

static void fake_delete_buffers(GLsizei n, const GLuint *p) {
    (void)n;
    (void)p;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    if (target == GL_ARRAY_BUFFER)
        cap.bound_array = name;
    if (target == GL_ELEMENT_ARRAY_BUFFER)
        cap.bound_element = name;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)usage;
    if (target == GL_ARRAY_BUFFER && size >= 0 && (size_t)size <= sizeof(cap.array_upload)) {
        cap.array_upload_size = (size_t)size;
        if (size != 0 && data != NULL)
            memcpy(cap.array_upload, data, (size_t)size);
    }
}

static void fake_vertex_attrib_pointer(GLuint i, GLint s, GLenum t, GLboolean n, GLsizei st,
                                       const void *p) {
    (void)s;
    (void)t;
    (void)n;
    (void)st;
    if (i < 16u)
        cap.attrib_offset[i] = (size_t)(uintptr_t)p;
}

static void fake_vertex_attrib_divisor(GLuint i, GLuint d) {
    if (d) {
        cap.divisor_location = i;
        cap.divisor_value = d;
    }
}

static void fake_enable(GLuint i) {
    (void)i;
}

static void fake_disable(GLuint i) {
    (void)i;
}

static void fake_current(GLuint i, const GLfloat *v) {
    (void)i;
    (void)v;
}

static void fake_use_program(GLuint p) {
    (void)p;
}

static void fake_draw_arrays(GLenum m, GLint f, GLsizei c) {
    (void)m;
    (void)f;
    (void)c;
}

static void fake_draw_elements(GLenum m, GLsizei c, GLenum t, const void *p) {
    (void)m;
    (void)c;
    (void)t;
    (void)p;
}

static void fake_draw_arrays_instanced(GLenum m, GLint f, GLsizei c, GLsizei instances) {
    (void)m;
    (void)f;
    (void)c;
    cap.arrays_instances = instances;
    ++cap.arrays_instanced_calls;
}

static void fake_draw_elements_instanced(GLenum m, GLsizei c, GLenum t, const void *p,
                                         GLsizei instances) {
    (void)m;
    (void)c;
    (void)t;
    (void)p;
    cap.elements_instances = instances;
    ++cap.elements_instanced_calls;
}

static int fail(int n, const char *m) {
    fprintf(stderr, "extended draw failure %d: %s\n", n, m);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLfloat verts[8][4] = {{0}};
    GLfloat per_instance[3][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};
    GLuint element_data[3] = {0u, 1u, 2u};
    GLuint buffers[2];
    GLint q = 0;

    struct {
        GLuint count, instance_count, first, base_instance;
    } acmd = {2u, 4u, 1u, 0u};

    struct {
        GLuint count, instance_count, first_index;
        GLint base_vertex;
        GLuint base_instance;
    } ecmd = {2u, 3u, 0u, 2, 0u};

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGetIntegerv = fake_get_integerv;
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesVertexAttribPointer = fake_vertex_attrib_pointer;
    native.glesVertexAttribDivisor = fake_vertex_attrib_divisor;
    native.glesEnableVertexAttribArray = fake_enable;
    native.glesDisableVertexAttribArray = fake_disable;
    native.glesVertexAttrib4fv = fake_current;
    native.glesUseProgram = fake_use_program;
    native.glesDrawArrays = fake_draw_arrays;
    native.glesDrawElements = fake_draw_elements;
    native.glesDrawArraysInstanced = fake_draw_arrays_instanced;
    native.glesDrawElementsInstanced = fake_draw_elements_instanced;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (!ctx.share_group)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.native_max_vertex_attribs = 16;
    mgl_context_set_current_for_test(&ctx);
    program = calloc(1, sizeof(*program));
    if (!program)
        return fail(2, "program");
    program->name = 3u;
    program->linked = GL_TRUE;
    program->active_generic_mask = (1u << 0) | (1u << 5);
    for (size_t i = 0; i < MGL_COMPAT_SEMANTIC_COUNT; ++i) {
        program->compat_attrib[i] = -1;
    }
    program->next = ctx.share_group->programs;
    ctx.share_group->programs = program;
    ctx.current_program = 3u;
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 0, per_instance);
    glEnableVertexAttribArray(5);
    glVertexAttribDivisor(5, 2u);
    glGetVertexAttribiv(5, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &q);
    if (q != 2)
        return fail(3, "divisor query");
    glDrawArraysInstanced(GL_POINTS, 0, 2, 5);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.arrays_instanced_calls != 1u ||
        cap.arrays_instances != 5)
        return fail(4, "arrays instanced");
    if (cap.divisor_location != 5u || cap.divisor_value != 2u)
        return fail(5, "divisor native");
    glDrawElementsInstancedBaseVertex(GL_POINTS, 2, GL_UNSIGNED_INT, element_data, 3, 2);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.elements_instanced_calls != 1u ||
        cap.elements_instances != 3)
        return fail(6, "elements instanced base vertex");

    program->native_name = 300u;
    program->linked = GL_FALSE;
    ctx.current_program_executable = GL_TRUE;
    ctx.draw_native_program = program->native_name;
    ctx.draw_native_program_valid = GL_TRUE;
    glDrawArraysInstanced(GL_POINTS, 0, 2, 5);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.arrays_instanced_calls != 2u)
        return fail(30, "installed executable survives failed relink");
    ctx.current_program_executable = GL_FALSE;
    glDrawArraysInstanced(GL_POINTS, 0, 2, 5);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION ||
        cap.arrays_instanced_calls != 2u)
        return fail(31, "unbound failed link has no executable");
    program->linked = GL_TRUE;
    ctx.current_program_executable = GL_TRUE;
    cap.arrays_instanced_calls = 1u;

    glGenBuffers(2, buffers);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)sizeof(element_data), element_data,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffers[1]);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, (GLsizeiptr)sizeof(acmd), &acmd, GL_STATIC_DRAW);
    glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &q);
    if ((GLuint)q != buffers[1])
        return fail(7, "indirect binding query");
    glDrawArraysIndirect(GL_POINTS, (const void *)(uintptr_t)0u);
    {
        GLenum e = mgl_error_take(&ctx.errors);
        if (e != GL_NO_ERROR || cap.arrays_instanced_calls != 2u || cap.arrays_instances != 4) {
            fprintf(stderr, "indirect e=%x calls=%u inst=%d bind=%u\n", e,
                    cap.arrays_instanced_calls, cap.arrays_instances, ctx.draw_indirect_buffer);
            return fail(8, "arrays indirect");
        }
    }
    glBufferData(GL_DRAW_INDIRECT_BUFFER, (GLsizeiptr)sizeof(ecmd), &ecmd, GL_STATIC_DRAW);
    glDrawElementsIndirect(GL_POINTS, GL_UNSIGNED_INT, (const void *)(uintptr_t)0u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.elements_instanced_calls != 2u ||
        cap.elements_instances != 3)
        return fail(9, "elements indirect");
    acmd.base_instance = 1u;
    glBufferData(GL_DRAW_INDIRECT_BUFFER, (GLsizeiptr)sizeof(acmd), &acmd, GL_STATIC_DRAW);
    glDrawArraysIndirect(GL_POINTS, 0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.arrays_instanced_calls != 3u ||
        cap.arrays_instances != 4)
        return fail(10, "arrays indirect base instance");
    if (cap.attrib_offset[5] + 2u * sizeof(per_instance[0]) > cap.array_upload_size)
        return fail(11, "arrays base instance packed range");
    if (memcmp(cap.array_upload + cap.attrib_offset[5], per_instance[1], sizeof(per_instance[0])) !=
            0 ||
        memcmp(cap.array_upload + cap.attrib_offset[5] + sizeof(per_instance[0]), per_instance[2],
               sizeof(per_instance[0])) != 0)
        return fail(12, "arrays base instance attribute selection");

    ecmd.base_instance = 1u;
    glBufferData(GL_DRAW_INDIRECT_BUFFER, (GLsizeiptr)sizeof(ecmd), &ecmd, GL_STATIC_DRAW);
    glDrawElementsIndirect(GL_POINTS, GL_UNSIGNED_INT, 0);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.elements_instanced_calls != 3u ||
        cap.elements_instances != 3)
        return fail(13, "elements indirect base instance");
    if (cap.attrib_offset[5] + 2u * sizeof(per_instance[0]) > cap.array_upload_size)
        return fail(14, "elements base instance packed range");
    if (memcmp(cap.array_upload + cap.attrib_offset[5], per_instance[1], sizeof(per_instance[0])) !=
            0 ||
        memcmp(cap.array_upload + cap.attrib_offset[5] + sizeof(per_instance[0]), per_instance[2],
               sizeof(per_instance[0])) != 0)
        return fail(15, "elements base instance attribute selection");
    glDrawArraysInstancedBaseInstance(GL_POINTS, 0, 2, 4, 1u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.arrays_instanced_calls != 4u ||
        cap.arrays_instances != 4)
        return fail(16, "direct base-instance arrays");
    if (cap.attrib_offset[5] + 2u * sizeof(per_instance[0]) > cap.array_upload_size ||
        memcmp(cap.array_upload + cap.attrib_offset[5], per_instance[1], sizeof(per_instance[0])) !=
            0 ||
        memcmp(cap.array_upload + cap.attrib_offset[5] + sizeof(per_instance[0]), per_instance[2],
               sizeof(per_instance[0])) != 0)
        return fail(17, "direct base-instance array attribute fetch");
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);
    glDrawElementsInstancedBaseVertexBaseInstance(GL_POINTS, 2, GL_UNSIGNED_INT, element_data, 3, 2,
                                                  1u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.elements_instanced_calls != 4u ||
        cap.elements_instances != 3)
        return fail(18, "direct base-instance elements");
    if (cap.attrib_offset[5] + 2u * sizeof(per_instance[0]) > cap.array_upload_size ||
        memcmp(cap.array_upload + cap.attrib_offset[5], per_instance[1], sizeof(per_instance[0])) !=
            0 ||
        memcmp(cap.array_upload + cap.attrib_offset[5] + sizeof(per_instance[0]), per_instance[2],
               sizeof(per_instance[0])) != 0)
        return fail(19, "direct base-instance element attribute fetch");
    glDrawElementsInstancedBaseInstance(GL_POINTS, 2, GL_UNSIGNED_INT, element_data, 3, 1u);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.elements_instanced_calls != 5u ||
        cap.elements_instances != 3)
        return fail(21, "direct base-instance indexed elements");
    if (cap.attrib_offset[5] + 2u * sizeof(per_instance[0]) > cap.array_upload_size ||
        memcmp(cap.array_upload + cap.attrib_offset[5], per_instance[1], sizeof(per_instance[0])) !=
            0 ||
        memcmp(cap.array_upload + cap.attrib_offset[5] + sizeof(per_instance[0]), per_instance[2],
               sizeof(per_instance[0])) != 0)
        return fail(22, "indexed base-instance attribute fetch");
    glDrawElementsInstancedBaseInstance(GL_POINTS, -1, GL_UNSIGNED_INT, element_data, 3, 1u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE || cap.elements_instanced_calls != 5u)
        return fail(23, "invalid indexed base-instance count");
    glDrawArraysInstancedBaseInstance(GL_POINTS, 0, -1, 2, 1u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE || cap.arrays_instanced_calls != 4u)
        return fail(20, "invalid direct base-instance count");
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL extended draw unit test: PASS");
    return 0;
}

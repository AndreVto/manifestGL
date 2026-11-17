#define _POSIX_C_SOURCE 200809L
#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "objects/query.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct capture_state {
    GLuint next_name;
    GLuint bound_array;
    GLuint bound_element;
    unsigned array_bind_calls;
    unsigned element_bind_calls;
    unsigned char *array_data;
    size_t array_size;
    const void *array_upload_source;
    unsigned char *element_data;
    size_t element_size;
    unsigned element_upload_calls;
    GLuint pointer_location;
    GLint pointer_size;
    GLenum pointer_type;
    size_t pointer_offset;
    GLenum draw_elements_type;
    GLsizei draw_elements_count;
    const void *draw_elements_indices;
    GLenum draw_arrays_mode;
    GLint draw_arrays_first;
    GLsizei draw_arrays_count;
    unsigned draw_arrays_calls;
    unsigned pointer_calls9;
    unsigned constant_calls5;
    GLfloat constant_value5[4];
    unsigned draw_elements_calls;
    GLuint divisor_location;
    GLuint divisor_value;
    unsigned divisor_calls;
    GLsizei draw_arrays_instancecount;
    GLsizei draw_elements_instancecount;
    unsigned draw_arrays_instanced_calls;
    unsigned draw_elements_instanced_calls;
    size_t pointer_offset5;
    GLenum provoking_mode;
    unsigned provoking_calls;
    GLuint gpu_vbo[16];
    size_t gpu_offset[16];
    GLint gpu_size[16];
    GLenum gpu_type[16];
    GLint gpu_stride[16];
    GLint gpu_normalized[16];
    GLint gpu_enabled[16];
    int fake_offset_mismatch;
    int fake_readback_corrupt;
    unsigned map_calls;
    unsigned unmap_calls;
    unsigned char *readback_copy;
} cap;

static const GLubyte *fake_get_string(GLenum name) {
    static const GLubyte extensions[] = "GL_ANGLE_provoking_vertex GL_EXT_mock";
    return name == GL_EXTENSIONS ? extensions : (const GLubyte *)"";
}

static void fake_get_integerv(GLenum pname, GLint *value) {
    if (pname == GL_MAX_VERTEX_ATTRIBS)
        *value = 16;
    else if (pname == GL_ARRAY_BUFFER_BINDING)
        *value = (GLint)cap.bound_array;
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
    if (target == GL_ARRAY_BUFFER) {
        cap.bound_array = name;
        ++cap.array_bind_calls;
    }
    if (target == GL_ELEMENT_ARRAY_BUFFER) {
        cap.bound_element = name;
        ++cap.element_bind_calls;
    }
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    unsigned char **dst = target == GL_ARRAY_BUFFER ? &cap.array_data : &cap.element_data;
    size_t *dst_size = target == GL_ARRAY_BUFFER ? &cap.array_size : &cap.element_size;
    (void)usage;
    if (target == GL_ARRAY_BUFFER)
        cap.array_upload_source = data;
    else if (target == GL_ELEMENT_ARRAY_BUFFER)
        ++cap.element_upload_calls;
    free(*dst);
    *dst = NULL;
    *dst_size = 0u;
    if (size > 0) {
        *dst = malloc((size_t)size);
        if (*dst != NULL && data != NULL)
            memcpy(*dst, data, (size_t)size);
        *dst_size = *dst != NULL ? (size_t)size : 0u;
    }
}

static void *fake_map_buffer_range(GLenum target, GLintptr offset, GLsizeiptr length,
                                   GLbitfield flags) {
    ++cap.map_calls;
    if (target != GL_ARRAY_BUFFER || flags != GL_MAP_READ_BIT || offset < 0 || length <= 0 ||
        (size_t)offset > cap.array_size || (size_t)length > cap.array_size - (size_t)offset ||
        cap.readback_copy != NULL)
        return NULL;
    cap.readback_copy = malloc((size_t)length);
    if (cap.readback_copy == NULL)
        return NULL;
    memcpy(cap.readback_copy, cap.array_data + (size_t)offset, (size_t)length);
    if (cap.fake_readback_corrupt)
        cap.readback_copy[0] ^= 0xffu;
    return cap.readback_copy;
}

static GLboolean fake_unmap_buffer(GLenum target) {
    if (target != GL_ARRAY_BUFFER || cap.readback_copy == NULL)
        return GL_FALSE;
    ++cap.unmap_calls;
    free(cap.readback_copy);
    cap.readback_copy = NULL;
    return GL_TRUE;
}

static void fake_vertex_attrib_pointer(GLuint location, GLint size, GLenum type,
                                       GLboolean normalized, GLsizei stride, const void *pointer) {
    if (location < 16u) {
        cap.gpu_vbo[location] = cap.bound_array;
        cap.gpu_offset[location] = (size_t)(uintptr_t)pointer;
        cap.gpu_size[location] = size;
        cap.gpu_type[location] = type;
        cap.gpu_stride[location] = stride;
        cap.gpu_normalized[location] = normalized != 0u;
    }
    if (location == 9u) {
        ++cap.pointer_calls9;
        cap.pointer_location = location;
        cap.pointer_size = size;
        cap.pointer_type = type;
        cap.pointer_offset = (size_t)(uintptr_t)pointer;
    }
    if (location == 5u)
        cap.pointer_offset5 = (size_t)(uintptr_t)pointer;
}

static void fake_enable_vertex_attrib(GLuint index) {
    if (index < 16u)
        cap.gpu_enabled[index] = 1;
}

static void fake_disable_vertex_attrib(GLuint index) {
    if (index < 16u)
        cap.gpu_enabled[index] = 0;
}

static void fake_get_vertex_attrib_iv(GLuint loc, GLenum pname, GLint *out) {
    if (loc >= 16u)
        return;
    switch (pname) {
        case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
            *out = cap.gpu_enabled[loc];
            break;
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
            *out = (GLint)cap.gpu_vbo[loc];
            break;
        case GL_VERTEX_ATTRIB_ARRAY_SIZE:
            *out = cap.gpu_size[loc];
            break;
        case GL_VERTEX_ATTRIB_ARRAY_TYPE:
            *out = (GLint)cap.gpu_type[loc];
            break;
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
            *out = cap.gpu_stride[loc];
            break;
        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
            *out = cap.gpu_normalized[loc];
            break;
        case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
            *out = 0;
            break;
        default:
            *out = -1;
            break;
    }
}

static void fake_get_vertex_attrib_pointer(GLuint loc, GLenum pname, void **out) {
    if (loc >= 16u || pname != GL_VERTEX_ATTRIB_ARRAY_POINTER)
        return;
    *out = (void *)(uintptr_t)(cap.gpu_offset[loc] + (cap.fake_offset_mismatch ? 4u : 0u));
}

static void fake_vertex_attrib4fv(GLuint index, const GLfloat *v) {
    if (index == 5u) {
        ++cap.constant_calls5;
        memcpy(cap.constant_value5, v, sizeof(cap.constant_value5));
    }
}

static void fake_vertex_attrib_divisor(GLuint index, GLuint divisor) {
    if (divisor != 0u) {
        cap.divisor_location = index;
        cap.divisor_value = divisor;
    }
    ++cap.divisor_calls;
}

static void fake_use_program(GLuint program) {
    (void)program;
}

static void fake_provoking_vertex(GLenum mode) {
    cap.provoking_mode = mode;
    ++cap.provoking_calls;
}

static void fake_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    cap.draw_arrays_mode = mode;
    cap.draw_arrays_first = first;
    cap.draw_arrays_count = count;
    ++cap.draw_arrays_calls;
}

static void fake_draw_elements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    (void)mode;
    (void)indices;
    cap.draw_elements_count = count;
    cap.draw_elements_type = type;
    cap.draw_elements_indices = indices;
    ++cap.draw_elements_calls;
}

static void fake_draw_arrays_instanced(GLenum mode, GLint first, GLsizei count, GLsizei instances) {
    (void)mode;
    (void)first;
    (void)count;
    cap.draw_arrays_instancecount = instances;
    ++cap.draw_arrays_instanced_calls;
}

static void fake_draw_elements_instanced(GLenum mode, GLsizei count, GLenum type,
                                         const void *indices, GLsizei instances) {
    (void)mode;
    (void)count;
    (void)type;
    (void)indices;
    cap.draw_elements_instancecount = instances;
    ++cap.draw_elements_instanced_calls;
}

static void init_native(struct mgl_native_dispatch *native) {
    memset(native, 0, sizeof(*native));
    native->glesGetString = fake_get_string;
    native->glesGetIntegerv = fake_get_integerv;
    native->glesGenBuffers = fake_gen_buffers;
    native->glesDeleteBuffers = fake_delete_buffers;
    native->glesBindBuffer = fake_bind_buffer;
    native->glesBufferData = fake_buffer_data;
    native->glesMapBufferRange = fake_map_buffer_range;
    native->glesUnmapBuffer = fake_unmap_buffer;
    native->glesGetVertexAttribiv = fake_get_vertex_attrib_iv;
    native->glesGetVertexAttribPointerv = fake_get_vertex_attrib_pointer;
    native->glesVertexAttribPointer = fake_vertex_attrib_pointer;
    native->glesVertexAttribDivisor = fake_vertex_attrib_divisor;
    native->glesEnableVertexAttribArray = fake_enable_vertex_attrib;
    native->glesDisableVertexAttribArray = fake_disable_vertex_attrib;
    native->glesVertexAttrib4fv = fake_vertex_attrib4fv;
    native->glesUseProgram = fake_use_program;
    native->glesDrawArrays = fake_draw_arrays;
    native->glesDrawElements = fake_draw_elements;
    native->glesDrawArraysInstanced = fake_draw_arrays_instanced;
    native->glesDrawElementsInstanced = fake_draw_elements_instanced;
    native->glesProvokingVertexANGLE = fake_provoking_vertex;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "vertex draw semantic failure %d: %s\n", n, message);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLfloat secondary[2][3] = {{0.10f, 0.20f, 0.30f}, {0.40f, 0.50f, 0.60f}};
    GLfloat generic9[3][4] = {
        {9.0f, 9.0f, 9.0f, 9.0f}, {8.0f, 8.0f, 8.0f, 8.0f}, {7.0f, 7.0f, 7.0f, 7.0f}};
    GLfloat *large_secondary;
    GLuint high_indices[3] = {65535u, 65536u, 65537u};
    size_t i;

    memset(&ctx, 0, sizeof(ctx));
    memset(&cap, 0, sizeof(cap));
    init_native(&native);
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.native_max_vertex_attribs = 16;
    mgl_context_set_current_for_test(&ctx);

    program = calloc(1, sizeof(*program));
    if (program == NULL)
        return fail(2, "program allocation");
    program->name = 7u;
    program->linked = GL_TRUE;
    for (i = 0; i < MGL_COMPAT_SEMANTIC_COUNT; ++i)
        program->compat_attrib[i] = -1;
    program->compat_attrib[3] = 9;
    program->active_generic_mask = (UINT32_C(1) << 9) | (UINT32_C(1) << 5);
    program->next = ctx.share_group->programs;
    ctx.share_group->programs = program;
    ctx.current_program = program->name;

    glSecondaryColorPointer(3, GL_FLOAT, 0, secondary);
    glEnableClientState(GL_SECONDARY_COLOR_ARRAY);
    glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, 0, generic9);
    glEnableVertexAttribArray(9);
    cap.array_bind_calls = 0u;
    glDrawArrays(GL_TRIANGLES, 0, 2);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(3, "draw arrays interface error");
    if (cap.array_bind_calls != 2u)
        return fail(100, "scratch array buffer rebound for each attribute");
    if (cap.pointer_location != 9u || cap.pointer_size != 3 || cap.pointer_type != GL_FLOAT) {
        return fail(4, "compat semantic did not own reflected native location");
    }
    if (cap.array_size != sizeof(secondary) || cap.pointer_offset != 0u ||
        memcmp(cap.array_data, secondary, sizeof(secondary)) != 0) {
        return fail(5, "location 9 used generic data instead of secondary-color semantic data");
    }
    if (cap.draw_arrays_first != 0 || cap.draw_arrays_count != 2)
        return fail(6, "draw arrays rebase");
    {
        unsigned previous_draws = cap.draw_arrays_calls;
        glDrawArrays(GL_PATCHES, 0, 3);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.draw_arrays_calls != previous_draws + 1u || cap.draw_arrays_mode != GL_PATCHES ||
            cap.draw_arrays_count != 3)
            return fail(460, "tessellation patch draw did not reach native GLES");
    }

    {
        unsigned previous_draws = cap.draw_arrays_calls;
        glDrawArrays(GL_TRIANGLES, 0, 2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.draw_arrays_calls != previous_draws + 1u)
            return fail(101, "scratch draw submitted");
    }

    {
        GLuint ebo = 0u;
        GLuint native_name;
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        native_name = mgl_buffer_find(&ctx, ebo)->native_name;
        glDrawArrays(GL_POINTS, 0, 2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.bound_element != native_name)
            return fail(328, "scratch draw arrays preserved element binding");
        glDrawArraysInstanced(GL_POINTS, 0, 2, 1);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.bound_element != native_name)
            return fail(329, "instanced scratch draw arrays preserved element binding");
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);
        glDeleteBuffers(1, &ebo);
    }

    /* GLES generic current attributes persist across programs and VAOs.
     * Reuse unchanged values across draws; resubmit after a relevant state
     * change or context reactivation. */
    {
        static const GLfloat updated[4] = {0.75f, -0.25f, 0.5f, 1.0f};
        static const GLfloat reset[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        unsigned before = cap.constant_calls5;
        if (before == 0u)
            return fail(314, "disabled generic attribute was not initialized");
        glDrawArrays(GL_POINTS, 0, 2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(315, "generic attribute draw");
        glVertexAttrib4fv(5u, updated);
        glDrawArrays(GL_POINTS, 0, 2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.constant_calls5 != before + 1u ||
            memcmp(cap.constant_value5, updated, sizeof(updated)) != 0)
            return fail(316, "changed generic constant not realized");
        glDrawArrays(GL_POINTS, 0, 2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            memcmp(cap.constant_value5, updated, sizeof(updated)) != 0)
            return fail(317, "updated generic constant persisted across draws");
        mgl_context_set_current_for_test(&ctx);
        glDrawArrays(GL_POINTS, 0, 2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.constant_calls5 != before + 2u)
            return fail(318, "context reactivation did not invalidate native current value");
        glVertexAttrib4fv(5u, reset);
    }

    {
        unsigned previous_draws = cap.draw_arrays_calls;
        glDrawArrays(GL_POINTS, 0, 2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.draw_arrays_calls != previous_draws + 1u)
            return fail(302, "vertex drawing changed after removing diagnostics");
    }

    large_secondary = calloc((size_t)65538u * 3u, sizeof(*large_secondary));
    if (large_secondary == NULL)
        return fail(7, "large client array allocation");
    for (i = 65535u; i <= 65537u; ++i) {
        large_secondary[i * 3u + 0u] = (GLfloat)i;
        large_secondary[i * 3u + 1u] = (GLfloat)i + 0.25f;
        large_secondary[i * 3u + 2u] = (GLfloat)i + 0.5f;
    }
    glSecondaryColorPointer(3, GL_FLOAT, 0, large_secondary);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, high_indices);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(8, "high-index draw interface error");
    if (cap.draw_elements_count != 3 || cap.draw_elements_type != GL_UNSIGNED_BYTE) {
        return fail(9, "high indices were not semantically rebased to a safe native stream");
    }
    if (cap.element_size != 3u || cap.element_data == NULL || cap.element_data[0] != 0u ||
        cap.element_data[1] != 1u || cap.element_data[2] != 2u) {
        return fail(10, "rebased index contents");
    }
    if (cap.array_size != 3u * 3u * sizeof(GLfloat))
        return fail(11, "high-index vertex range packing");
    if (memcmp(cap.array_data, large_secondary + (size_t)65535u * 3u, cap.array_size) != 0) {
        return fail(12, "high-index logical vertex source");
    }

    {
        const unsigned char expected_quads[12] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
        GLuint quad_indices[4] = {65535u, 65536u, 65537u, 65535u};
        const unsigned char expected_indexed[6] = {0, 1, 2, 0, 2, 0};
        GLuint polygon_indices[4] = {65535u, 65536u, 65537u, 65535u};
        const unsigned char expected_polygon[6] = {0, 1, 2, 0, 2, 0};
        unsigned previous_draws = cap.draw_elements_calls;
        glDrawArrays(GL_QUADS, 65530, 8);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.draw_elements_calls != previous_draws + 1u || cap.draw_elements_count != 12 ||
            cap.element_size != sizeof(expected_quads) ||
            memcmp(cap.element_data, expected_quads, sizeof(expected_quads)) != 0)
            return fail(102, "quad arrays triangulation");
        previous_draws = cap.draw_elements_calls;
        glDrawArrays(GL_QUAD_STRIP, 65534, 4);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.draw_elements_calls != previous_draws + 1u || cap.draw_elements_count != 6 ||
            cap.element_size != 6u ||
            memcmp(cap.element_data, (const unsigned char[]){0, 1, 3, 0, 3, 2}, 6u) != 0)
            return fail(103, "quad strip arrays triangulation");
        previous_draws = cap.draw_elements_calls;
        glDrawElements(GL_QUADS, 4, GL_UNSIGNED_INT, quad_indices);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.draw_elements_calls != previous_draws + 1u || cap.draw_elements_count != 6 ||
            cap.element_size != sizeof(expected_indexed) ||
            memcmp(cap.element_data, expected_indexed, sizeof(expected_indexed)) != 0)
            return fail(104, "indexed quad triangulation");
        previous_draws = cap.draw_elements_calls;
        glDrawElements(GL_POLYGON, 4, GL_UNSIGNED_INT, polygon_indices);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.draw_elements_calls != previous_draws + 1u || cap.draw_elements_count != 6 ||
            cap.element_size != sizeof(expected_polygon) ||
            memcmp(cap.element_data, expected_polygon, sizeof(expected_polygon)) != 0)
            return fail(105, "indexed polygon triangulation");
    }

    {
        GLint firsts[3] = {65535, 65536, 65537};
        GLsizei counts[3] = {1, 0, 1};
        unsigned before = cap.draw_arrays_calls;
        glMultiDrawArrays(GL_POINTS, firsts, counts, 3);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(13, "multi draw arrays interface error");
        if (cap.draw_arrays_calls != before + 2u)
            return fail(14, "multi draw arrays nonzero subdraw count");
    }

    {
        GLint firsts[2] = {65535, 65537};
        GLsizei counts[2] = {1, 1};
        unsigned before = cap.draw_arrays_calls;
        glEnable(GL_TEXTURE);
        glMultiDrawArrays(GL_POINTS, firsts, counts, 2);
        if (cap.draw_arrays_calls != before + 2u ||
            mgl_error_take(&ctx.errors) != GL_INVALID_ENUM ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(115, "multi draw continues despite older error");
    }

    {
        GLuint index0 = 65535u;
        GLuint index1 = 65537u;
        const void *indices_list[2] = {&index0, &index1};
        GLsizei counts[2] = {1, 1};
        unsigned before = cap.draw_elements_calls;
        glMultiDrawElements(GL_POINTS, counts, GL_UNSIGNED_INT, indices_list, 2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(15, "multi draw elements interface error");
        if (cap.draw_elements_calls != before + 2u)
            return fail(16, "multi draw elements subdraw count");
    }

    {
        GLuint one = 65536u;
        unsigned before = cap.draw_elements_calls;
        glDrawRangeElements(GL_POINTS, 65535u, 65537u, 1, GL_UNSIGNED_INT, &one);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(17, "draw range interface error");
        if (cap.draw_elements_calls != before + 1u)
            return fail(18, "draw range delegated semantic draw");
    }

    {
        struct mgl_query conditional;
        unsigned before = cap.draw_arrays_calls;
        memset(&conditional, 0, sizeof(conditional));
        conditional.name = 91u;
        conditional.target = GL_SAMPLES_PASSED;
        conditional.available = GL_TRUE;
        conditional.result = 0u;
        ctx.queries = &conditional;
        glBeginConditionalRender(conditional.name, GL_QUERY_WAIT);
        glDrawArrays(GL_POINTS, 0, 1);
        glEndConditionalRender();
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(19, "conditional render zero-result state");
        if (cap.draw_arrays_calls != before)
            return fail(20, "zero-result conditional query did not suppress draw");
        conditional.result = 1u;
        glBeginConditionalRender(conditional.name, GL_QUERY_NO_WAIT);
        glDrawArrays(GL_POINTS, 0, 1);
        glEndConditionalRender();
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.draw_arrays_calls != before + 1u)
            return fail(21, "passing conditional query did not allow draw");
        ctx.queries = NULL;
    }

    {
        GLuint restart_indices[3] = {65535u, 0xffffffffu, 65537u};
        unsigned before = cap.draw_elements_calls;
        GLint value = 0;
        glPrimitiveRestartIndex(0xffffffffu);
        glEnable(GL_PRIMITIVE_RESTART);
        glDrawElements(GL_LINE_STRIP, 3, GL_UNSIGNED_INT, restart_indices);
        glDisable(GL_PRIMITIVE_RESTART);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(22, "primitive restart draw error");
        if (cap.draw_elements_calls != before + 2u || cap.draw_elements_count != 1)
            return fail(23, "primitive restart did not split logical stream");
        glGetIntegerv(GL_PRIMITIVE_RESTART_INDEX, &value);
        if ((GLuint)value != 0xffffffffu)
            return fail(24, "primitive restart index query");
    }

    {
        GLint value = 0;
        glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
        glGetIntegerv(GL_PROVOKING_VERTEX, &value);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            value != (GLint)GL_FIRST_VERTEX_CONVENTION)
            return fail(25, "provoking vertex state/query");
        if (cap.provoking_calls != 1u || cap.provoking_mode != GL_FIRST_VERTEX_CONVENTION)
            return fail(260, "ANGLE provoking-vertex lowering");
        glProvokingVertex(GL_LAST_VERTEX_CONVENTION);
        if (cap.provoking_calls != 2u || cap.provoking_mode != GL_LAST_VERTEX_CONVENTION)
            return fail(261, "ANGLE provoking-vertex restore");
        glProvokingVertex(0xdeadbeefu);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
            return fail(26, "provoking vertex validation");
        if (cap.provoking_calls != 2u)
            return fail(262, "invalid provoking mode reached native");
    }

    {
        GLuint base_indices[2] = {0u, 2u};
        unsigned before = cap.draw_elements_calls;
        glDrawElementsBaseVertex(GL_POINTS, 2, GL_UNSIGNED_INT, base_indices, 65535);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(27, "base-vertex interface error");
        if (cap.draw_elements_calls != before + 1u || cap.element_size != 2u ||
            cap.element_data[0] != 0u || cap.element_data[1] != 2u)
            return fail(28, "base-vertex semantic rebase");
        if (cap.array_size < 3u * 3u * sizeof(GLfloat) ||
            memcmp(cap.array_data, large_secondary + (size_t)65535u * 3u,
                   3u * 3u * sizeof(GLfloat)) != 0)
            return fail(29, "base-vertex logical source");
    }

    {
        GLuint a = 0u, b = 1u;
        const void *ilist[2] = {&a, &b};
        GLsizei counts[2] = {1, 1};
        GLint bases[2] = {65535, 65536};
        unsigned before = cap.draw_elements_calls;
        glMultiDrawElementsBaseVertex(GL_POINTS, counts, GL_UNSIGNED_INT, ilist, 2, bases);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.draw_elements_calls != before + 2u)
            return fail(30, "multi draw base-vertex");
    }

    {
        GLfloat instanced5[3][4] = {
            {1.f, 2.f, 3.f, 4.f}, {5.f, 6.f, 7.f, 8.f}, {9.f, 10.f, 11.f, 12.f}};
        GLuint idx[2] = {0u, 1u};
        GLint divisor = 0;
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 0, instanced5);
        glEnableVertexAttribArray(5);
        glVertexAttribDivisor(5, 2u);
        glGetVertexAttribiv(5, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &divisor);
        if (divisor != 2)
            return fail(31, "divisor query");
        glDrawArraysInstanced(GL_POINTS, 0, 2, 5);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.draw_arrays_instanced_calls == 0u ||
            cap.draw_arrays_instancecount != 5)
            return fail(32, "draw arrays instanced");
        if (cap.divisor_location != 5u || cap.divisor_value != 2u)
            return fail(33, "native divisor realization");
        if (cap.array_size < cap.pointer_offset5 + sizeof(instanced5) ||
            memcmp(cap.array_data + cap.pointer_offset5, instanced5, sizeof(instanced5)) != 0)
            return fail(34, "instanced attribute packing");
        glDrawElementsInstancedBaseVertex(GL_POINTS, 2, GL_UNSIGNED_INT, idx, 3, 65535);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.draw_elements_instanced_calls == 0u ||
            cap.draw_elements_instancecount != 3)
            return fail(35, "instanced base-vertex draw");
    }
    /* Client-side VBO offsets refer to the CPU shadow, not realized GLES storage. */
    {
        GLuint logical_vbo = 0u;
        GLfloat force_scratch[2][4] = {{1.f, 1.f, 1.f, 1.f}, {2.f, 2.f, 2.f, 2.f}};
        GLfloat interleaved[2][12] = {
            {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 0.25f, 0.50f, 0.75f},
            {11.f, 12.f, 13.f, 14.f, 15.f, 16.f, 17.f, 18.f, 19.f, 0.35f, 0.60f, 0.85f}};
        glGenBuffers(1, &logical_vbo);
        if (logical_vbo == 0u)
            return fail(306, "create VBO");
        glBindBuffer(GL_ARRAY_BUFFER, logical_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(interleaved), interleaved, GL_STATIC_DRAW);
        glSecondaryColorPointer(3, GL_FLOAT, 12 * (GLsizei)sizeof(GLfloat),
                                (const void *)(uintptr_t)0x24u);
        glBindBuffer(GL_ARRAY_BUFFER, 0u);
        glVertexAttribPointer(5u, 4, GL_FLOAT, GL_FALSE, 0, force_scratch);
        glEnableVertexAttribArray(5u);
        glVertexAttribDivisor(5u, 0u);
        glDrawArrays(GL_POINTS, 0, 2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.array_size != 2u * (3u + 4u) * sizeof(GLfloat))
            return fail(307, "nonzero VBO offset and scratch vertex draw");
        if (memcmp(cap.array_data, &interleaved[0][9], 3u * sizeof(GLfloat)) != 0 ||
            memcmp(cap.array_data + 3u * sizeof(GLfloat), &interleaved[1][9],
                   3u * sizeof(GLfloat)) != 0)
            return fail(308, "strided VBO shadow data was not packed correctly");
        {
            unsigned previous_draws = cap.draw_arrays_calls;
            glBindBuffer(GL_ARRAY_BUFFER, logical_vbo);
            glSecondaryColorPointer(
                3, GL_FLOAT, 12 * (GLsizei)sizeof(GLfloat),
                (const void *)(uintptr_t)(sizeof(interleaved) - sizeof(GLfloat)));
            glBindBuffer(GL_ARRAY_BUFFER, 0u);
            glDrawArrays(GL_POINTS, 0, 2);
            if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION ||
                cap.draw_arrays_calls != previous_draws)
                return fail(310, "out-of-range strided VBO must reject before native draw");
        }
        glDeleteBuffers(1, &logical_vbo);
        /* The next test draws again. Do not leave a live attribute pointing
         * into this block's expired force_scratch stack array. */
        glVertexAttribPointer(5u, 4, GL_FLOAT, GL_FALSE, 0, generic9);
    }
    /* A nonzero first vertex and a nontrivial stride must select the exact
     * source elements, without changing the existing client-array semantics. */
    {
        const GLfloat client[3][5] = {{0.f, 1.f, 2.f, 3.f, 4.f},
                                      {10.f, 11.f, 12.f, 13.f, 14.f},
                                      {20.f, 21.f, 22.f, 23.f, 24.f}};
        glSecondaryColorPointer(3, GL_FLOAT, 5 * (GLsizei)sizeof(GLfloat), &client[0][1]);
        glDrawArrays(GL_POINTS, 1, 2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.array_size != 2u * (3u + 4u) * sizeof(GLfloat) ||
            memcmp(cap.array_data, &client[1][1], 3u * sizeof(GLfloat)) != 0 ||
            memcmp(cap.array_data + 3u * sizeof(GLfloat), &client[2][1], 3u * sizeof(GLfloat)) != 0)
            return fail(309, "strided client-array range with nonzero first vertex");
    }
    /* Indexed draws can use GPU-resident vertices even when the index list
     * is client memory. Only the index list should be staged for GLES. */
    {
        GLuint vbo = 0u;
        const GLfloat gpu_vertices[2][7] = {{1.f, 2.f, 3.f, 10.f, 11.f, 12.f, 13.f},
                                            {4.f, 5.f, 6.f, 20.f, 21.f, 22.f, 23.f}};
        const GLuint indices[3] = {1u, 0u, 1u};
        const GLfloat client_generic[2][4] = {{1.f, 1.f, 1.f, 1.f}, {2.f, 2.f, 2.f, 2.f}};
        GLuint native_name;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(gpu_vertices), gpu_vertices, GL_STATIC_DRAW);
        glSecondaryColorPointer(3, GL_FLOAT, 7 * (GLsizei)sizeof(GLfloat),
                                (const void *)(uintptr_t)0u);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 7 * (GLsizei)sizeof(GLfloat),
                              (const void *)(uintptr_t)(3u * sizeof(GLfloat)));
        glEnableVertexAttribArray(5);
        glBindBuffer(GL_ARRAY_BUFFER, 0u);
        native_name = mgl_buffer_find(&ctx, vbo)->native_name;
        /* Mixed draw preparation binds the GLES array buffer for attribute realization and
         * restores the canonical array-buffer binding after setup. */
        cap.array_bind_calls = 0u;
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, indices);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.gpu_vbo[9] != native_name ||
            cap.gpu_vbo[5] != native_name || cap.draw_elements_type != GL_UNSIGNED_INT ||
            cap.element_size != sizeof(indices) ||
            memcmp(cap.element_data, indices, sizeof(indices)) != 0 ||
            cap.array_size != sizeof(gpu_vertices) ||
            memcmp(cap.array_data, gpu_vertices, sizeof(gpu_vertices)) != 0 ||
            cap.bound_array != 0u || cap.bound_element != 0u)
            return fail(311,
                        "mixed VBO vertices and client indices must avoid scratch vertex packing");
        {
            /* Client-memory indices use scratch EBO uploads on repeated draws. */
            unsigned previous_uploads = cap.element_upload_calls;
            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, indices);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                cap.element_upload_calls != previous_uploads + 1u ||
                cap.element_size != sizeof(indices) ||
                memcmp(cap.element_data, indices, sizeof(indices)) != 0 || cap.bound_element != 0u)
                return fail(327, "repeated client indices must upload to the scratch EBO");
        }
        {
            unsigned previous_uploads = cap.element_upload_calls;
            ctx.angle_client_arrays = GL_TRUE;
            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, indices);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                cap.element_upload_calls != previous_uploads ||
                cap.draw_elements_indices != indices || cap.draw_elements_type != GL_UNSIGNED_INT ||
                cap.bound_element != 0u || cap.gpu_vbo[9] != native_name ||
                cap.gpu_vbo[5] != native_name)
                return fail(350, "ANGLE client indices must bypass scratch upload");
            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, indices);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                cap.element_upload_calls != previous_uploads ||
                cap.draw_elements_indices != indices || cap.bound_element != 0u)
                return fail(351, "repeated ANGLE client indices must remain direct");
            ctx.angle_client_arrays = GL_FALSE;
            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, indices);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                cap.element_upload_calls != previous_uploads + 1u ||
                cap.draw_elements_indices != (const void *)0 || cap.bound_element != 0u)
                return fail(352, "scratch index fallback must remain available");
            ctx.angle_client_arrays = GL_TRUE;
        }
        {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glSecondaryColorPointer(3, GL_FLOAT, 7 * (GLsizei)sizeof(GLfloat),
                                    (const void *)(uintptr_t)sizeof(GLfloat));
            glBindBuffer(GL_ARRAY_BUFFER, 0u);
            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, indices);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                cap.gpu_offset[9] != sizeof(GLfloat))
                return fail(319, "pointer-only update preserved offset");
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glSecondaryColorPointer(3, GL_FLOAT, 7 * (GLsizei)sizeof(GLfloat),
                                    (const void *)(uintptr_t)0u);
            glBindBuffer(GL_ARRAY_BUFFER, 0u);
            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, indices);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.gpu_offset[9] != 0u)
                return fail(320, "restored pointer offset");
        }
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 0, client_generic);
        {
            unsigned previous_uploads = cap.element_upload_calls;
            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, indices);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.gpu_vbo[5] != 0u ||
                cap.gpu_offset[5] != (size_t)(uintptr_t)client_generic ||
                cap.gpu_vbo[9] != native_name || cap.draw_elements_indices != indices ||
                cap.draw_elements_type != GL_UNSIGNED_INT ||
                cap.element_upload_calls != previous_uploads)
                return fail(312, "ANGLE mixed client/VBO vertex attributes must bypass staging");
            ctx.angle_client_arrays = GL_FALSE;
            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, indices);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                cap.gpu_vbo[5] != ctx.scratch_array_buffer_native ||
                cap.draw_elements_type != GL_UNSIGNED_BYTE || cap.element_size != 3u)
                return fail(353, "disabled ANGLE client arrays must retain staged vertex fallback");
        }
        glDeleteBuffers(1, &vbo);
    }
    /* A single enabled, tightly packed client array is already in upload
     * layout, so the staging copy is unnecessary. Disabled attributes retain
     * their constant values, and nonzero first still refers to the same data. */
    {
        unsigned previous_draws = cap.draw_arrays_calls;
        glDisableVertexAttribArray(5);
        glSecondaryColorPointer(3, GL_FLOAT, 0, secondary);
        glDrawArrays(GL_POINTS, 0, 2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.draw_arrays_calls != previous_draws + 1u || cap.array_upload_source != secondary ||
            cap.array_size != sizeof(secondary) ||
            memcmp(cap.array_data, secondary, sizeof(secondary)) != 0)
            return fail(313, "direct upload of a tightly packed single client attribute");
        glDrawArrays(GL_POINTS, 1, 1);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
            cap.array_upload_source != &secondary[1][0] || cap.array_size != sizeof(secondary[1]) ||
            memcmp(cap.array_data, secondary[1], sizeof(secondary[1])) != 0)
            return fail(314, "direct single-attribute upload must respect first vertex");
        {
            size_t previous_upload_size = cap.array_size;
            unsigned previous_draws = cap.draw_arrays_calls;
            ctx.angle_client_arrays = GL_TRUE;
            glDrawArrays(GL_POINTS, 1, 1);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
                cap.draw_arrays_calls != previous_draws + 1u || cap.draw_arrays_first != 1 ||
                cap.gpu_vbo[9] != 0u || cap.gpu_offset[9] != (size_t)(uintptr_t)secondary ||
                cap.array_size != previous_upload_size)
                return fail(354, "ANGLE client vertex arrays must retain pointer and draw first");
            ctx.angle_client_arrays = GL_FALSE;
        }
    }
    {
        const GLfloat vertices[4][3] = {
            {0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {1.f, 1.f, 0.f}, {0.f, 1.f, 0.f}};
        const GLuint indexed[6] = {0u, 1u, 2u, 3u, 0u, 1u};
        const GLuint restarted[6] = {0u, 1u, 0xffffffffu, 2u, 3u, 0u};
        const GLenum modes[] = {GL_QUADS, GL_QUAD_STRIP, GL_POLYGON};
        GLuint ebo = 0u;
        GLuint native_ebo;
        GLuint list;
        size_t mode_index;
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexed), indexed, GL_STATIC_DRAW);
        native_ebo = mgl_buffer_find(&ctx, ebo)->native_name;
        glSecondaryColorPointer(3, GL_FLOAT, 0, vertices);
        glEnableClientState(GL_SECONDARY_COLOR_ARRAY);
        for (mode_index = 0u; mode_index < sizeof(modes) / sizeof(modes[0]); ++mode_index) {
            glDrawArrays(modes[mode_index], 0, 4);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.bound_element != native_ebo ||
                ctx.current_vao->element_array_buffer != ebo)
                return fail(330, "legacy array draw must restore native element binding");
            glDrawElements(modes[mode_index], 4, GL_UNSIGNED_INT, (const void *)0);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.bound_element != native_ebo ||
                ctx.current_vao->element_array_buffer != ebo)
                return fail(331, "legacy indexed draw must restore native element binding");
        }
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(restarted), restarted, GL_STATIC_DRAW);
        glPrimitiveRestartIndex(0xffffffffu);
        glEnable(GL_PRIMITIVE_RESTART);
        glDrawElements(GL_POINTS, 6, GL_UNSIGNED_INT, (const void *)0);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.bound_element != native_ebo ||
            ctx.current_vao->element_array_buffer != ebo)
            return fail(332, "primitive restart must restore native element binding");
        glDisable(GL_PRIMITIVE_RESTART);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexed), indexed, GL_STATIC_DRAW);
        glDrawElementsBaseVertex(GL_POINTS, 2, GL_UNSIGNED_INT, (const void *)0, 1);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.bound_element != native_ebo ||
            ctx.current_vao->element_array_buffer != ebo)
            return fail(333, "base-vertex fallback must restore native element binding");
        glDrawElementsInstancedBaseVertex(GL_POINTS, 2, GL_UNSIGNED_INT, (const void *)0, 1, 1);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.bound_element != native_ebo ||
            ctx.current_vao->element_array_buffer != ebo)
            return fail(334, "instanced base-vertex fallback must restore native element binding");
        glBegin(GL_QUADS);
        glVertex3f(0.f, 0.f, 0.f);
        glVertex3f(1.f, 0.f, 0.f);
        glVertex3f(1.f, 1.f, 0.f);
        glVertex3f(0.f, 1.f, 0.f);
        glEnd();
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.bound_element != native_ebo ||
            ctx.current_vao->element_array_buffer != ebo)
            return fail(335, "immediate quads must restore native element binding");
        list = glGenLists(1);
        if (list == 0u)
            return fail(336, "indexed display-list allocation");
        glNewList(list, GL_COMPILE);
        glDrawElements(GL_POINTS, 2, GL_UNSIGNED_INT, (const void *)0);
        glEndList();
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(337, "indexed display-list compile");
        glCallList(list);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.bound_element != native_ebo ||
            ctx.current_vao->element_array_buffer != ebo)
            return fail(338, "indexed display-list replay must restore native element binding");
        glDeleteLists(list, 1);
        for (mode_index = 0u; mode_index < sizeof(modes) / sizeof(modes[0]); ++mode_index) {
            list = glGenLists(1);
            if (list == 0u)
                return fail(340, "legacy array display-list allocation");
            glNewList(list, GL_COMPILE);
            glDrawArrays(modes[mode_index], 0, 4);
            glEndList();
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
                return fail(341, "legacy array display-list compile");
            glCallList(list);
            if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.bound_element != native_ebo ||
                ctx.current_vao->element_array_buffer != ebo)
                return fail(342,
                            "legacy array display-list replay must restore native element binding");
            glDeleteLists(list, 1);
        }
        glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);
        glPopClientAttrib();
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.bound_element != native_ebo ||
            ctx.current_vao->element_array_buffer != ebo)
            return fail(339, "client attribute pop must restore native element binding");
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);
        glDeleteBuffers(1, &ebo);
    }
    {
        const GLuint packed[2] = {0x3ff00200u, 0xc00003ffu};
        const GLint fixed[2][3] = {{65536, 0, -65536}, {0, 65536, 32768}};
        glBindBuffer(GL_ARRAY_BUFFER, 0u);
        glDisableClientState(GL_SECONDARY_COLOR_ARRAY);
        glDisableVertexAttribArray(9u);
        glEnableVertexAttribArray(5u);
        ctx.angle_client_arrays = GL_FALSE;
        glVertexAttribPointer(5u, 4, GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, 0, packed);
        glDrawArrays(GL_POINTS, 0, 2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.gpu_type[5] !=
                GL_UNSIGNED_INT_2_10_10_10_REV || cap.gpu_stride[5] != (GLint)sizeof(GLuint) ||
            cap.array_size != sizeof(packed) || memcmp(cap.array_data, packed, sizeof(packed)) != 0)
            return fail(470, "packed unsigned vertex data must retain native layout");
        glVertexAttribPointer(5u, 4, GL_INT_2_10_10_10_REV, GL_FALSE, 0, packed);
        glDrawArrays(GL_POINTS, 0, 2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.gpu_type[5] !=
                GL_INT_2_10_10_10_REV || cap.gpu_stride[5] != (GLint)sizeof(GLuint) ||
            cap.array_size != sizeof(packed) || memcmp(cap.array_data, packed, sizeof(packed)) != 0)
            return fail(471, "packed signed vertex data must retain native layout");
        glVertexAttribPointer(5u, 3, GL_FIXED, GL_FALSE, 0, fixed);
        glDrawArrays(GL_POINTS, 0, 2);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.gpu_type[5] != GL_FIXED ||
            cap.gpu_stride[5] != 3 * (GLint)sizeof(GLint) || cap.array_size != sizeof(fixed) ||
            memcmp(cap.array_data, fixed, sizeof(fixed)) != 0)
            return fail(472, "fixed-point attribute data must retain native layout");
    }
    free(ctx.immediate.vertices);
    free(large_secondary);
    free(cap.array_data);
    free(cap.element_data);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL vertex/draw semantic unit test: PASS");
    return 0;
}

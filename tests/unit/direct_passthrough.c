#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    GLuint next_buffer;
    GLuint next_texture;
    unsigned draw_arrays;
    unsigned draw_elements;
    unsigned draw_basevertex;
    unsigned draw_arrays_indirect;
    unsigned draw_elements_indirect;
    unsigned vertex_half;
    unsigned vertex_divisor;
    unsigned enable_calls;
    unsigned disable_calls;
    unsigned enable_indexed;
    unsigned disable_indexed;
    unsigned tex_images;
    unsigned tex_sub_images;
    unsigned tex_images_3d;
    unsigned tex_sub_images_3d;
    const void *pixel_pointer;
    GLint basevertex;
    GLuint divisor;
} cap;

static void gen_buffer(GLsizei count, GLuint *names) {
    GLsizei i;
    for (i = 0; i < count; ++i)
        names[i] = ++cap.next_buffer;
}

static void bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)target;
    (void)size;
    (void)data;
    (void)usage;
}

static void gen_texture(GLsizei count, GLuint *names) {
    GLsizei i;
    for (i = 0; i < count; ++i)
        names[i] = ++cap.next_texture;
}

static void bind_texture(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void active_texture(GLenum unit) {
    (void)unit;
}

static void pixel_store(GLenum pname, GLint param) {
    (void)pname;
    (void)param;
}

static GLenum get_error(void) {
    return GL_NO_ERROR;
}

static void vertex_pointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                           GLsizei stride, const void *pointer) {
    (void)size;
    (void)normalized;
    (void)stride;
    (void)pointer;
    if (index == 0u && type == GL_HALF_FLOAT)
        ++cap.vertex_half;
}

static void vertex_divisor(GLuint index, GLuint divisor) {
    if (index == 1u) {
        ++cap.vertex_divisor;
        cap.divisor = divisor;
    }
}

static void attrib_enable(GLuint index) {
    (void)index;
}

static void attrib_disable(GLuint index) {
    (void)index;
}

static void attrib_constant(GLuint index, const GLfloat *value) {
    (void)index;
    (void)value;
}

static void program_use(GLuint name) {
    (void)name;
}

static void draw_arrays(GLenum mode, GLint first, GLsizei count) {
    (void)mode;
    (void)first;
    (void)count;
    ++cap.draw_arrays;
}

static void draw_elements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    (void)mode;
    (void)count;
    (void)type;
    (void)indices;
    ++cap.draw_elements;
}

static void draw_arrays_instanced(GLenum mode, GLint first, GLsizei count, GLsizei instance_count) {
    (void)mode;
    (void)first;
    (void)count;
    (void)instance_count;
}

static void draw_elements_instanced(GLenum mode, GLsizei count, GLenum type, const void *indices,
                                    GLsizei instance_count) {
    (void)mode;
    (void)count;
    (void)type;
    (void)indices;
    (void)instance_count;
}

static void draw_basevertex(GLenum mode, GLsizei count, GLenum type, const void *indices,
                            GLint basevertex) {
    (void)mode;
    (void)count;
    (void)type;
    (void)indices;
    ++cap.draw_basevertex;
    cap.basevertex = basevertex;
}

static void draw_arrays_indirect(GLenum mode, const void *offset) {
    (void)mode;
    (void)offset;
    ++cap.draw_arrays_indirect;
}

static void draw_elements_indirect(GLenum mode, GLenum type, const void *offset) {
    (void)mode;
    (void)type;
    (void)offset;
    ++cap.draw_elements_indirect;
}

static void enable(GLenum cap_name) {
    (void)cap_name;
    ++cap.enable_calls;
}

static void disable(GLenum cap_name) {
    (void)cap_name;
    ++cap.disable_calls;
}

static void enablei(GLenum cap_name, GLuint index) {
    (void)cap_name;
    (void)index;
    ++cap.enable_indexed;
}

static void disablei(GLenum cap_name, GLuint index) {
    (void)cap_name;
    (void)index;
    ++cap.disable_indexed;
}

static void image2d(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height,
                    GLint border, GLenum format, GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)internal;
    (void)width;
    (void)height;
    (void)border;
    (void)format;
    (void)type;
    cap.pixel_pointer = pixels;
    ++cap.tex_images;
}

static void sub2d(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
                  GLenum format, GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)format;
    (void)type;
    cap.pixel_pointer = pixels;
    ++cap.tex_sub_images;
}

static void image3d(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height,
                    GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)internal;
    (void)width;
    (void)height;
    (void)depth;
    (void)border;
    (void)format;
    (void)type;
    cap.pixel_pointer = pixels;
    ++cap.tex_images_3d;
}

static void sub3d(GLenum target, GLint level, GLint x, GLint y, GLint z, GLsizei width,
                  GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)x;
    (void)y;
    (void)z;
    (void)width;
    (void)height;
    (void)depth;
    (void)format;
    (void)type;
    cap.pixel_pointer = pixels;
    ++cap.tex_sub_images_3d;
}

static int fail(int id, const char *message) {
    fprintf(stderr, "direct passthrough %d: %s (gl_error=%x)\n", id, message, glGetError());
    return id;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLuint buf[3], tex[3];
    GLushort verts_half[12] = {0u};
    GLfloat instances[8] = {0.0f};
    GLushort indices[3] = {0u, 1u, 2u};

    struct {
        GLuint count, instance_count, first, base_instance;
    } arrays = {3u, 2u, 0u, 0u};

    struct {
        GLuint count, instance_count, first_index;
        GLint base_vertex;
        GLuint base_instance;
    } elements = {3u, 2u, 0u, 4, 0u};

    unsigned char rgba[16] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u};
    unsigned char volume[32] = {0u};
    size_t i;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGenBuffers = gen_buffer;
    native.glesBindBuffer = bind_buffer;
    native.glesBufferData = buffer_data;
    native.glesGenTextures = gen_texture;
    native.glesBindTexture = bind_texture;
    native.glesActiveTexture = active_texture;
    native.glesPixelStorei = pixel_store;
    native.glesGetError = get_error;
    native.glesVertexAttribPointer = vertex_pointer;
    native.glesVertexAttribDivisor = vertex_divisor;
    native.glesEnableVertexAttribArray = attrib_enable;
    native.glesDisableVertexAttribArray = attrib_disable;
    native.glesVertexAttrib4fv = attrib_constant;
    native.glesUseProgram = program_use;
    native.glesDrawArrays = draw_arrays;
    native.glesDrawElements = draw_elements;
    native.glesDrawArraysInstanced = draw_arrays_instanced;
    native.glesDrawElementsInstanced = draw_elements_instanced;
    native.glesDrawElementsBaseVertex = draw_basevertex;
    native.glesDrawArraysIndirect = draw_arrays_indirect;
    native.glesDrawElementsIndirect = draw_elements_indirect;
    native.glesEnable = enable;
    native.glesDisable = disable;
    native.glesEnablei = enablei;
    native.glesDisablei = disablei;
    native.glesTexImage2D = image2d;
    native.glesTexSubImage2D = sub2d;
    native.glesTexImage3D = image3d;
    native.glesTexSubImage3D = sub3d;
    ctx.native_dispatch = &native;
    ctx.native_draw_elements_base_vertex = draw_basevertex;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 1, 1);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.native_max_vertex_attribs = 16;
    ctx.unpack_alignment = 4;
    ctx.pack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);
    program = calloc(1u, sizeof(*program));
    if (program == NULL)
        return fail(2, "program");
    program->name = 19u;
    program->linked = GL_TRUE;
    program->active_generic_mask = 3u;
    for (i = 0u; i < MGL_COMPAT_SEMANTIC_COUNT; ++i)
        program->compat_attrib[i] = -1;
    ctx.share_group->programs = program;
    ctx.current_program = 19u;
    glGenBuffers(3, buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts_half), verts_half, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_HALF_FLOAT, GL_FALSE, 0, (const void *)(uintptr_t)0u);
    glEnableVertexAttribArray(0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(instances), instances, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (const void *)(uintptr_t)0u);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 2u);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (glGetError() != GL_NO_ERROR || cap.draw_arrays != 1u || cap.vertex_half == 0u ||
        cap.vertex_divisor == 0u || cap.divisor != 2u)
        return fail(3, "native half-float and divisor attributes");
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glDrawElementsBaseVertex(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (const void *)(uintptr_t)0u, 4);
    if (glGetError() != GL_NO_ERROR || cap.draw_basevertex != 1u || cap.basevertex != 4)
        return fail(4, "native base-vertex without index rewriting");
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buf[2]);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(arrays), &arrays, GL_STATIC_DRAW);
    glDrawArraysIndirect(GL_TRIANGLES, (const void *)(uintptr_t)0u);
    if (glGetError() != GL_NO_ERROR || cap.draw_arrays_indirect != 1u)
        return fail(5, "native arrays indirect");
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(elements), &elements, GL_STATIC_DRAW);
    glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, (const void *)(uintptr_t)0u);
    if (glGetError() != GL_NO_ERROR || cap.draw_elements_indirect != 1u)
        return fail(6, "native indexed indirect");
    {
        struct {
            GLuint count, instance_count, first, base_instance;
        } commands[2] = {{3u, 2u, 0u, 0u}, {3u, 2u, 0u, 0u}};

        unsigned before = cap.draw_arrays_indirect;
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(commands), commands, GL_STATIC_DRAW);
        glMultiDrawArraysIndirect(GL_TRIANGLES, NULL, 2, 0);
        if (glGetError() != GL_NO_ERROR || cap.draw_arrays_indirect != before + 2u)
            return fail(16, "multi arrays tight stride");
        before = cap.draw_arrays_indirect;
        glMultiDrawArraysIndirect(GL_TRIANGLES, NULL, 2, 2);
        if (glGetError() != GL_INVALID_VALUE || cap.draw_arrays_indirect != before)
            return fail(17, "multi arrays invalid stride");
        glMultiDrawArraysIndirect(GL_TRIANGLES, NULL, -1, 0);
        if (glGetError() != GL_INVALID_VALUE)
            return fail(18, "multi arrays negative drawcount");
        glMultiDrawArraysIndirect(GL_TRIANGLES, NULL, 3, 0);
        if (glGetError() != GL_INVALID_OPERATION || cap.draw_arrays_indirect != before)
            return fail(19, "multi arrays range check before drawing");
        glMultiDrawArraysIndirect(GL_TRIANGLES, NULL, 0, 0);
        if (glGetError() != GL_NO_ERROR || cap.draw_arrays_indirect != before)
            return fail(20, "multi arrays empty draw");
    }
    {
        struct {
            GLuint count, instance_count, first_index;
            GLint base_vertex;
            GLuint base_instance;
        } commands[2] = {{3u, 2u, 0u, 0, 0u}, {3u, 2u, 0u, 0, 0u}};

        unsigned before = cap.draw_elements_indirect;
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(commands), commands, GL_STATIC_DRAW);
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, NULL, 2, 0);
        if (glGetError() != GL_NO_ERROR || cap.draw_elements_indirect != before + 2u)
            return fail(21, "multi elements tight stride");
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, NULL, 2, 22);
        if (glGetError() != GL_INVALID_VALUE)
            return fail(22, "multi elements invalid stride");
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, NULL, 1, 0);
        if (glGetError() != GL_INVALID_OPERATION)
            return fail(23, "multi elements needs index buffer");
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);
    }
    arrays.base_instance = 1u;
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(arrays), &arrays, GL_STATIC_DRAW);
    glDrawArraysIndirect(GL_TRIANGLES, (const void *)(uintptr_t)0u);
    if (glGetError() != GL_NO_ERROR || cap.draw_arrays_indirect != 3u)
        return fail(7, "base-instance fallback");
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);
    if (glGetError() != GL_NO_ERROR || cap.enable_calls != 1u || cap.disable_calls != 1u)
        return fail(8, "state-change coalescing");
    glEnablei(GL_BLEND, 1u);
    glEnablei(GL_BLEND, 1u);
    glDisablei(GL_BLEND, 1u);
    glDisablei(GL_BLEND, 1u);
    if (glGetError() != GL_NO_ERROR || cap.enable_indexed != 1u || cap.disable_indexed != 1u)
        return fail(9, "indexed blend coalescing");
    glGenTextures(3, tex);
    glBindTexture(GL_TEXTURE_1D, tex[0]);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    if (glGetError() != GL_NO_ERROR || cap.pixel_pointer != rgba)
        return fail(10, "1D upload borrow");
    glBindTexture(GL_TEXTURE_2D, tex[1]);
    glMultiTexImage2D(GL_TEXTURE0, GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      rgba);
    if (glGetError() != GL_NO_ERROR || cap.pixel_pointer != rgba)
        return fail(11, "multitexture upload borrow");
    glTextureImage2DEXT(tex[1], GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        rgba);
    if (glGetError() != GL_NO_ERROR || cap.pixel_pointer != rgba)
        return fail(12, "EXT DSA upload borrow");
    glTextureSubImage2D(tex[1], 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    if (glGetError() != GL_NO_ERROR || cap.pixel_pointer != rgba)
        return fail(13, "core DSA subimage borrow");
    glBindTexture(GL_TEXTURE_3D, tex[2]);
    glMultiTexImage3D(GL_TEXTURE0, GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA,
                      GL_UNSIGNED_BYTE, volume);
    if (glGetError() != GL_NO_ERROR || cap.pixel_pointer != volume)
        return fail(14, "3D multitexture borrow");
    glTextureSubImage3D(tex[2], 0, 0, 0, 0, 2, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, volume);
    if (glGetError() != GL_NO_ERROR || cap.pixel_pointer != volume)
        return fail(15, "3D core DSA borrow");
    {
        GLuint parameter = 0u;
        GLint requested = 2;
        GLint binding = 0;
        unsigned before;

        struct {
            GLuint count, instances, first, baseinstance;
        } commands[2] = {{3u, 2u, 0u, 0u}, {3u, 2u, 0u, 0u}};

        glGenBuffers(1, &parameter);
        glBindBuffer(GL_PARAMETER_BUFFER, parameter);
        glBufferData(GL_PARAMETER_BUFFER, sizeof(requested), &requested, GL_DYNAMIC_DRAW);
        glGetIntegerv(GL_PARAMETER_BUFFER_BINDING, &binding);
        if (glGetError() != GL_NO_ERROR || (GLuint)binding != parameter)
            return fail(24, "parameter-buffer binding and allocation");
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(commands), commands, GL_STATIC_DRAW);
        before = cap.draw_arrays_indirect;
        glMultiDrawArraysIndirectCount(GL_TRIANGLES, NULL, 0, 2, 0);
        if (glGetError() != GL_NO_ERROR || cap.draw_arrays_indirect != before + 2u)
            return fail(25, "indirect count both draws");
        requested = 1;
        glBufferSubData(GL_PARAMETER_BUFFER, 0, sizeof(requested), &requested);
        before = cap.draw_arrays_indirect;
        glMultiDrawArraysIndirectCount(GL_TRIANGLES, NULL, 0, 2, 0);
        if (glGetError() != GL_NO_ERROR || cap.draw_arrays_indirect != before + 1u)
            return fail(26, "indirect count from updated buffer");
        {
            GLint *mapped = (GLint *)glMapBufferRange(GL_PARAMETER_BUFFER, 0, sizeof(requested),
                                                      GL_MAP_WRITE_BIT);
            if (mapped == NULL || glGetError() != GL_NO_ERROR)
                return fail(31, "map parameter buffer");
            *mapped = 2;
            if (!glUnmapBuffer(GL_PARAMETER_BUFFER) || glGetError() != GL_NO_ERROR)
                return fail(32, "unmap parameter buffer");
            before = cap.draw_arrays_indirect;
            glMultiDrawArraysIndirectCount(GL_TRIANGLES, NULL, 0, 2, 0);
            if (glGetError() != GL_NO_ERROR || cap.draw_arrays_indirect != before + 2u)
                return fail(33, "mapped parameter-buffer count");
            requested = 1;
            glBufferSubData(GL_PARAMETER_BUFFER, 0, sizeof(requested), &requested);
        }
        {
            struct {
                GLuint count, instances, first_index;
                GLint base_vertex;
                GLuint base_instance;
            } indexed[2] = {{3u, 2u, 0u, 0, 0u}, {3u, 2u, 0u, 0, 0u}};

            glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(indexed), indexed, GL_STATIC_DRAW);
            before = cap.draw_elements_indirect;
            glMultiDrawElementsIndirectCount(GL_TRIANGLES, GL_UNSIGNED_SHORT, NULL, 0, 2, 0);
            if (glGetError() != GL_NO_ERROR || cap.draw_elements_indirect != before + 1u)
                return fail(34, "indexed indirect count");
            glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(commands), commands, GL_STATIC_DRAW);
        }
        before = cap.draw_arrays_indirect;
        glMultiDrawArraysIndirectCount(GL_TRIANGLES, NULL, 1, 2, 0);
        if (glGetError() != GL_INVALID_OPERATION || cap.draw_arrays_indirect != before)
            return fail(27, "unaligned parameter offset");
        glMultiDrawArraysIndirectCount(GL_TRIANGLES, NULL, 4, 2, 0);
        if (glGetError() != GL_INVALID_OPERATION || cap.draw_arrays_indirect != before)
            return fail(28, "out of bounds parameter offset");
        glMultiDrawArraysIndirectCount(GL_TRIANGLES, NULL, 0, -1, 0);
        if (glGetError() != GL_INVALID_VALUE)
            return fail(29, "negative max count");
        glBindBuffer(GL_PARAMETER_BUFFER, 0u);
        glMultiDrawArraysIndirectCount(GL_TRIANGLES, NULL, 0, 2, 0);
        if (glGetError() != GL_INVALID_OPERATION)
            return fail(30, "parameter binding required");
        glDeleteBuffers(1, &parameter);
    }
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL direct passthrough test: PASS");
    return 0;
}

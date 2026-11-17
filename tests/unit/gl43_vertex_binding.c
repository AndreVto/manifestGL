#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLint binding_limit = 16;
static GLuint next_native;
static GLuint bound_slot_buffer;
static GLintptr bound_slot_offset;
static GLsizei bound_slot_stride;
static GLuint formatted_location;
static GLuint bound_attrib_location;
static unsigned native_zero_stride_calls;
static unsigned native_draw_calls;
static unsigned staged_uploads;
static GLint staged_vertices[4];

static void data_buffer(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)usage;
    if (target == GL_ARRAY_BUFFER && size >= (GLsizeiptr)sizeof(staged_vertices) && data != NULL) {
        memcpy(staged_vertices, data, sizeof(staged_vertices));
        ++staged_uploads;
    }
}

static void use_program(GLuint name) {
    (void)name;
}

static void enable_attrib(GLuint index) {
    (void)index;
}

static void disable_attrib(GLuint index) {
    (void)index;
}

static void vertex_pointer(GLuint i, GLint n, GLenum t, GLboolean norm, GLsizei stride,
                           const void *ptr) {
    (void)i;
    (void)n;
    (void)t;
    (void)norm;
    (void)stride;
    (void)ptr;
}

static void vertex_ipointer(GLuint i, GLint n, GLenum t, GLsizei stride, const void *ptr) {
    (void)i;
    (void)n;
    (void)t;
    (void)stride;
    (void)ptr;
}

static void vertex_divisor(GLuint index, GLuint divisor) {
    (void)index;
    (void)divisor;
}

static void bind_vertex_slot(GLuint index, GLuint buffer, GLintptr offset, GLsizei stride) {
    (void)index;
    bound_slot_buffer = buffer;
    bound_slot_offset = offset;
    bound_slot_stride = stride;
    ++native_zero_stride_calls;
}

static void format_integer(GLuint index, GLint size, GLenum type, GLuint relativeoffset) {
    (void)size;
    (void)type;
    (void)relativeoffset;
    formatted_location = index;
}

static void bind_vertex_attrib(GLuint attr, GLuint binding) {
    (void)attr;
    bound_attrib_location = binding;
}

static void draw_arrays(GLenum mode, GLint first, GLsizei count) {
    (void)mode;
    (void)first;
    (void)count;
    ++native_draw_calls;
}

static void gen_buffer(GLsizei n, GLuint *out) {
    for (GLsizei i = 0; i < n; ++i)
        out[i] = ++next_native;
}

static void bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void gen_vao(GLsizei n, GLuint *out) {
    gen_buffer(n, out);
}

static void bind_vao(GLuint name) {
    (void)name;
}

static void get_limit(GLenum pname, GLint *value) {
    *value = pname == GL_MAX_VERTEX_ATTRIBS ? 16
             : pname == GL_MAX_VERTEX_ATTRIB_BINDINGS ? binding_limit : 0;
}

static int fail(int n, const char *why) {
    fprintf(stderr, "GL 4.3 vertex binding failure %d: %s\n", n, why);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_vertex_array *named;
    GLuint buf[2], vaos[2], fake = 123456u;
    GLint value = -1;
    GLintptr offsets[3] = {0, 10, 20};
    GLsizei strides[3] = {12, 16, 24};
    GLuint buffers[3];
    struct mgl_program *program;
    GLint vertex_data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenBuffers = gen_buffer;
    native.glesGenVertexArrays = gen_vao;
    native.glesBindBuffer = bind_buffer;
    native.glesBindVertexArray = bind_vao;
    native.glesGetIntegerv = get_limit;
    native.glesBufferData = data_buffer;
    native.glesUseProgram = use_program;
    native.glesEnableVertexAttribArray = enable_attrib;
    native.glesDisableVertexAttribArray = disable_attrib;
    native.glesVertexAttribPointer = vertex_pointer;
    native.glesVertexAttribIPointer = vertex_ipointer;
    native.glesVertexAttribDivisor = vertex_divisor;
    native.glesDrawArrays = draw_arrays;
    native.glesBindVertexBuffer = bind_vertex_slot;
    native.glesVertexAttribIFormat = format_integer;
    native.glesVertexAttribBinding = bind_vertex_attrib;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.native_max_vertex_attribs = 16;
    mgl_context_set_current_for_test(&ctx);

    glGenBuffers(2, buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
    glBindBuffer(GL_ARRAY_BUFFER, buf[1]);
    if (glGetError() != GL_NO_ERROR)
        return fail(2, "buffer setup");

    glBindVertexBuffer(3u, buf[0], 16, 24);
    glVertexAttribFormat(1u, 3, GL_FLOAT, GL_FALSE, 8u);
    glVertexAttribBinding(1u, 3u);
    glEnableVertexAttribArray(1u);
    if (glGetError() != GL_NO_ERROR || ctx.current_vao->generic[1].data.buffer_name != buf[0] ||
        ctx.current_vao->generic[1].data.offset != 24u || ctx.current_vao->generic[1].stride != 24)
        return fail(3, "effective separate attribute input");
    glGetVertexAttribiv(1u, GL_VERTEX_ATTRIB_BINDING, &value);
    if (value != 3)
        return fail(4, "binding query");
    glGetVertexAttribiv(1u, GL_VERTEX_ATTRIB_RELATIVE_OFFSET, &value);
    if (value != 8)
        return fail(5, "relative offset query");
    glGetIntegeri_v(GL_VERTEX_BINDING_BUFFER, 3u, &value);
    if (value != (GLint)buf[0])
        return fail(6, "indexed binding query");
    glGetInteger64i_v(GL_VERTEX_BINDING_OFFSET, 3u, (GLint64 *)&offsets[0]);
    if (offsets[0] != 16)
        return fail(7, "64 bit offset query");
    glVertexBindingDivisor(3u, 4u);
    if (ctx.current_vao->generic[1].divisor != 4u)
        return fail(8, "slot divisor");
    glVertexAttribBinding(2u, 3u);
    if (ctx.current_vao->generic[2].divisor != 4u ||
        ctx.current_vao->generic[2].data.buffer_name != buf[0])
        return fail(9, "shared slot");
    glBindVertexBuffer(3u, buf[1], 64, 16);
    if (ctx.current_vao->generic[1].data.offset != 72u ||
        ctx.current_vao->generic[2].data.offset != 64u)
        return fail(10, "shared buffer change");

    glVertexAttribPointer(1u, 4, GL_FLOAT, GL_FALSE, 0, (void *)(uintptr_t)32u);
    if (glGetError() != GL_NO_ERROR || ctx.current_vao->vertex_attrib_binding[1] != 1u ||
        ctx.current_vao->vertex_attrib_relative_offset[1] != 0u ||
        ctx.current_vao->generic[1].data.buffer_name != buf[1] ||
        ctx.current_vao->generic[1].data.offset != 32u)
        return fail(11, "legacy pointer resets separate binding");
    glGetVertexAttribiv(1u, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &value);
    if (value != 0)
        return fail(28, "legacy attribute stride query");
    glGetIntegeri_v(GL_VERTEX_BINDING_STRIDE, 1u, &value);
    if (value != 16)
        return fail(29, "legacy computed binding stride");
    glBindVertexBuffer(1u, buf[1], 0, 32);
    glGetVertexAttribiv(1u, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &value);
    if (value != 0)
        return fail(30, "legacy stride query after binding change");
    glGetIntegeri_v(GL_VERTEX_BINDING_STRIDE, 1u, &value);
    if (value != 32)
        return fail(31, "independent binding stride query");

    glCreateVertexArrays(2, vaos);
    named = mgl_vao_find(&ctx, vaos[0]);
    if (named == NULL || ctx.current_vao != &ctx.default_vao)
        return fail(12, "named VAO setup");
    glVertexArrayVertexBuffer(vaos[0], 5u, buf[0], 96, 32);
    glVertexArrayAttribIFormat(vaos[0], 4u, 2, GL_INT, 12u);
    glVertexArrayAttribBinding(vaos[0], 4u, 5u);
    glVertexArrayBindingDivisor(vaos[0], 5u, 2u);
    glEnableVertexArrayAttrib(vaos[0], 4u);
    glVertexArrayElementBuffer(vaos[0], buf[1]);
    if (glGetError() != GL_NO_ERROR || ctx.current_vao != &ctx.default_vao ||
        named->generic[4].data.offset != 108u || named->generic[4].data.buffer_name != buf[0] ||
        named->generic[4].integer != GL_TRUE || named->generic[4].divisor != 2u ||
        named->element_array_buffer != buf[1])
        return fail(13, "DSA update isolated");

    glGetVertexArrayiv(vaos[0], GL_ELEMENT_ARRAY_BUFFER_BINDING, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)buf[1])
        return fail(35, "named element buffer query");
    glGetVertexArrayIndexediv(vaos[0], 4u, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &value);
    if (glGetError() != GL_NO_ERROR || value != 1)
        return fail(36, "named enabled query");
    glGetVertexArrayIndexediv(vaos[0], 4u, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &value);
    if (glGetError() != GL_NO_ERROR || value != 1)
        return fail(37, "named integer query");
    glGetVertexArrayIndexediv(vaos[0], 4u, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &value);
    if (glGetError() != GL_NO_ERROR || value != 32)
        return fail(38, "named stride query");
    glGetVertexArrayIndexediv(vaos[0], 4u, GL_VERTEX_ATTRIB_RELATIVE_OFFSET, &value);
    if (glGetError() != GL_NO_ERROR || value != 12)
        return fail(39, "named relative offset query");
    {
        GLint64 offset = -1;
        glGetVertexArrayIndexed64iv(vaos[0], 5u, GL_VERTEX_BINDING_OFFSET, &offset);
        if (glGetError() != GL_NO_ERROR || offset != 96)
            return fail(40, "named 64 bit binding offset");
    }
    value = 742;
    glGetVertexArrayIndexediv(vaos[0], 16u, GL_VERTEX_ATTRIB_ARRAY_SIZE, &value);
    if (glGetError() != GL_INVALID_VALUE || value != 742)
        return fail(41, "out of range named query");
    glGetVertexArrayiv(vaos[0], GL_VERTEX_BINDING_BUFFER, &value);
    if (glGetError() != GL_INVALID_ENUM || value != 742)
        return fail(42, "invalid named VAO selector");
    glGetVertexArrayIndexediv(vaos[0], 5u, GL_VERTEX_BINDING_BUFFER, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)buf[0])
        return fail(43, "named buffer slot query");
    glGetVertexArrayIndexediv(vaos[0], 5u, GL_VERTEX_BINDING_STRIDE, &value);
    if (glGetError() != GL_NO_ERROR || value != 32)
        return fail(48, "named binding stride query");
    glGetVertexArrayIndexediv(vaos[0], 4u, GL_VERTEX_ATTRIB_BINDING, &value);
    if (glGetError() != GL_NO_ERROR || value != 5)
        return fail(49, "named attribute binding query");
    glGetVertexArrayIndexediv(vaos[0], 4u, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)buf[0])
        return fail(50, "named attribute buffer query");
    value = 742;
    glGetVertexArrayIndexediv(vaos[0], 4u, GL_TEXTURE_BINDING_2D, &value);
    if (glGetError() != GL_INVALID_ENUM || value != 742)
        return fail(51, "invalid indexed selector");
    {
        GLint64 offset = -1;
        glGetVertexArrayIndexed64iv(vaos[0], 5u, GL_VERTEX_BINDING_STRIDE, &offset);
        if (glGetError() != GL_INVALID_ENUM || offset != -1)
            return fail(44, "invalid 64 bit selector");
    }
    glGetVertexArrayiv(fake, GL_ELEMENT_ARRAY_BUFFER_BINDING, &value);
    if (glGetError() != GL_INVALID_OPERATION || value != 742)
        return fail(45, "unknown named VAO query");
    glGetVertexArrayiv(0u, GL_ELEMENT_ARRAY_BUFFER_BINDING, &value);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(46, "default VAO rejected by core DSA");
    {
        GLuint reserved;
        glGenVertexArrays(1, &reserved);
        glGetVertexArrayiv(reserved, GL_ELEMENT_ARRAY_BUFFER_BINDING, &value);
        if (glGetError() != GL_INVALID_OPERATION)
            return fail(47, "unrealized VAO query");
        glDeleteVertexArrays(1, &reserved);
    }
    buffers[0] = buf[0];
    buffers[1] = fake;
    buffers[2] = buf[1];
    glVertexArrayVertexBuffers(vaos[0], 0u, 3, buffers, offsets, strides);
    if (glGetError() != GL_INVALID_OPERATION ||
        named->vertex_buffers[0].data.buffer_name != buf[0] ||
        named->vertex_buffers[1].data.buffer_name != 0u ||
        named->vertex_buffers[2].data.buffer_name != buf[1])
        return fail(14, "DSA multibind partial errors");
    glBindVertexBuffers(2u, 3, buffers, offsets, strides);
    if (glGetError() != GL_INVALID_OPERATION ||
        ctx.current_vao->vertex_buffers[2].data.buffer_name != buf[0] ||
        ctx.current_vao->vertex_buffers[3].data.buffer_name != buf[1])
        return fail(15, "multibind partial errors");
    glBindVertexBuffers(2u, 3, NULL, NULL, NULL);
    if (glGetError() != GL_NO_ERROR || ctx.current_vao->vertex_buffers[2].data.buffer_name != 0u ||
        ctx.current_vao->vertex_buffers[3].data.buffer_name != 0u ||
        ctx.current_vao->vertex_buffers[2].stride != 16)
        return fail(16, "multibind reset");
    glBindVertexBuffer(2u, 0, 8, 48);
    glGetIntegeri_v(GL_VERTEX_BINDING_STRIDE, 2u, &value);
    if (glGetError() != GL_NO_ERROR || value != 48)
        return fail(32, "unbound binding stride");
    glBindVertexBuffers(15u, 2, NULL, NULL, NULL);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(33, "multibind native limit");
    {
        GLfloat fv = -1.0f;
        GLdouble dv = -1.0;
        glGetFloati_v(GL_VERTEX_BINDING_STRIDE, 2u, &fv);
        glGetDoublei_v(GL_VERTEX_BINDING_STRIDE, 2u, &dv);
        if (glGetError() != GL_NO_ERROR || fv != 48.0f || dv != 48.0)
            return fail(34, "indexed floating binding queries");
    }
    glVertexAttribFormat(0u, 4, GL_FLOAT, GL_FALSE, 2048u);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(17, "relative offset range");
    glBindVertexBuffer(3u, buf[0], -1, 16);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(18, "negative offset");
    glBindVertexBuffer(3u, buf[0], 0, 2049);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(19, "maximum stride");
    glVertexArrayAttribFormat(999u, 0, 4, GL_FLOAT, GL_FALSE, 0u);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(20, "unknown VAO");
    glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &value);
    if (value != 16)
        return fail(21, "max bindings");
    glBindVertexArray(vaos[0]);
    if (glGetError() != GL_NO_ERROR || ctx.current_vao != named ||
        ctx.current_vao->generic[4].data.buffer_name != buf[0])
        return fail(22, "bound DSA state");
    glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
    glVertexArrayVertexBuffer(vaos[0], 5u, buf[0], 0, 0);
    if (glGetError() != GL_NO_ERROR || !named->generic[4].separate_stride ||
        named->generic[4].stride != 0)
        return fail(23, "explicit zero stride retained");
    program = calloc(1, sizeof(*program));
    if (program == NULL)
        return fail(24, "program allocation");
    program->name = 7u;
    program->native_name = 42u;
    program->linked = GL_TRUE;
    program->active_generic_mask = 1u << 4;
    for (size_t i = 0u; i < MGL_COMPAT_SEMANTIC_COUNT; ++i)
        program->compat_attrib[i] = -1;
    ctx.share_group->programs = program;
    ctx.current_program = 7u;
    glVertexArrayBindingDivisor(vaos[0], 5u, 0u);
    glDrawArrays(GL_POINTS, 0, 2);
    {
        GLenum draw_error = glGetError();
        if (draw_error != GL_NO_ERROR || native_draw_calls != 1u ||
            native_zero_stride_calls == 0u || bound_slot_offset != 12 || bound_slot_stride != 0 ||
            formatted_location != 4u || bound_attrib_location != 4u ||
            bound_slot_buffer != mgl_buffer_find(&ctx, buf[0])->native_name) {
            fprintf(
                stderr,
                "draw_error=%x calls=%u bind=%u off=%ld stride=%d fmt=%u attrib=%u expected=%u\n",
                draw_error, native_draw_calls, native_zero_stride_calls, (long)bound_slot_offset,
                bound_slot_stride, formatted_location, bound_attrib_location,
                mgl_buffer_find(&ctx, buf[0])->native_name);
            return fail(25, "GLES 3.1 zero stride vertex binding");
        }
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribIPointer(4u, 2, GL_INT, 0, vertex_data);
    glEnableVertexAttribArray(4u);
    memset(staged_vertices, 0, sizeof(staged_vertices));
    staged_uploads = 0u;
    native_draw_calls = 0u;
    glDrawArrays(GL_POINTS, 0, 2);
    {
        GLenum err = glGetError();
        if (err != GL_NO_ERROR || native_draw_calls != 1u || staged_uploads == 0u ||
            staged_vertices[0] != 1 || staged_vertices[1] != 2 || staged_vertices[2] != 3 ||
            staged_vertices[3] != 4)
            return fail(27, "client vertex array staged translation");
    }
    binding_limit = 8;
    ctx.native_max_vertex_bindings = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &value);
    if (value != 8 || glGetError() != GL_NO_ERROR)
        return fail(44, "native vertex binding limit query");
    glBindVertexBuffer(7u, buf[0], 0, 16);
    if (glGetError() != GL_NO_ERROR)
        return fail(45, "last valid binding slot");
    glBindVertexBuffer(8u, buf[0], 0, 16);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(46, "binding beyond native limit");
    glVertexAttribBinding(4u, 8u);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(47, "attrib binding beyond native limit");
    glGetIntegeri_v(GL_VERTEX_BINDING_BUFFER, 8u, &value);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(48, "indexed binding beyond native limit");
    glGetVertexArrayIndexediv(vaos[0], 8u, GL_VERTEX_BINDING_STRIDE, &value);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(49, "named indexed binding beyond native limit");
    glBindVertexBuffers(7u, 2, NULL, NULL, NULL);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(50, "multibind beyond native limit");
    glVertexAttribFormat(9u, 4, GL_FLOAT, GL_FALSE, 0u);
    if (glGetError() != GL_NO_ERROR)
        return fail(51, "attribute indices have separate limit");
    {
        GLint current[4] = {-7, -7, -7, -7};
        glVertexAttrib4f(2u, 1.0f, 2.0f, 3.0f, 4.0f);
        glGetVertexAttribiv(2u, GL_CURRENT_VERTEX_ATTRIB, current);
        if (glGetError() != GL_NO_ERROR || current[0] != 1 || current[1] != 2 ||
            current[2] != 3 || current[3] != 4)
            return fail(52, "current vertex attribute integer query");
        glVertexAttribI4i(2u, -4, 3, 2, 1);
        glGetVertexAttribiv(2u, GL_CURRENT_VERTEX_ATTRIB, current);
        if (glGetError() != GL_NO_ERROR || current[0] != -4 || current[1] != 3 ||
            current[2] != 2 || current[3] != 1)
            return fail(53, "current integer attribute query");
        glVertexAttribPointer(2u, 4, GL_INT_2_10_10_10_REV, GL_TRUE, 0, (void *)0);
        glGetVertexAttribiv(2u, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &value);
        if (glGetError() != GL_NO_ERROR || value != 0 ||
            ctx.current_vao->vertex_buffers[2].stride != 4)
            return fail(54, "packed vertex pointer stride");
        glVertexAttribPointer(2u, 3, GL_INT_2_10_10_10_REV, GL_TRUE, 0, (void *)0);
        if (glGetError() != GL_INVALID_OPERATION)
            return fail(55, "packed pointer size validation");
        glVertexAttribFormat(2u, 4, GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, 0u);
        if (glGetError() != GL_NO_ERROR)
            return fail(56, "packed separate attribute format");
        glVertexAttribFormat(2u, 3, GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, 0u);
        if (glGetError() != GL_INVALID_OPERATION)
            return fail(57, "packed separate format size validation");
        glVertexAttribPointer(2u, 3, GL_FIXED, GL_FALSE, 0, (void *)0);
        if (glGetError() != GL_NO_ERROR)
            return fail(58, "fixed-point generic attribute pointer");
        glVertexAttribFormat(2u, 3, GL_FIXED, GL_FALSE, 0u);
        if (glGetError() != GL_NO_ERROR)
            return fail(59, "fixed-point separate attribute format");
    }
    binding_limit = 16;
    ctx.native_max_vertex_bindings = 0;
    glDeleteBuffers(1, &buf[0]);
    if (glGetError() != GL_NO_ERROR || named->vertex_buffers[5].data.buffer_name != 0u ||
        named->generic[4].data.buffer_name != 0u)
        return fail(26, "buffer deletion invalidates binding slots");
    mgl_context_set_current_for_test(NULL);
    puts("mGL GL 4.3 vertex binding and GL 4.5 DSA: PASS");
    return 0;
}

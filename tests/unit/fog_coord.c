#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "shader/translator.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_state {
    GLuint next_buffer;
    GLuint bound_array;
    unsigned char *array_data;
    size_t array_size;
    GLuint pointer_location;
    GLint pointer_size;
    GLenum pointer_type;
    unsigned draw_calls;
} cap;

static void fake_gen_buffers(GLsizei n, GLuint *out) {
    GLsizei i;
    for (i = 0; i < n; ++i)
        out[i] = ++cap.next_buffer;
}

static void fake_delete_buffers(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    if (target == GL_ARRAY_BUFFER)
        cap.bound_array = name;
}

static void fake_buffer_data(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)usage;
    if (target != GL_ARRAY_BUFFER)
        return;
    free(cap.array_data);
    cap.array_data = NULL;
    cap.array_size = 0u;
    if (size > 0) {
        cap.array_data = malloc((size_t)size);
        if (cap.array_data != NULL && data != NULL)
            memcpy(cap.array_data, data, (size_t)size);
        if (cap.array_data != NULL)
            cap.array_size = (size_t)size;
    }
}

static void fake_vertex_attrib_pointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                       GLsizei stride, const void *pointer) {
    (void)normalized;
    (void)stride;
    (void)pointer;
    if (index == 4u) {
        cap.pointer_location = index;
        cap.pointer_size = size;
        cap.pointer_type = type;
    }
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

static void fake_use_program(GLuint p) {
    (void)p;
}

static void fake_draw_arrays(GLenum mode, GLint first, GLsizei count) {
    (void)mode;
    (void)first;
    (void)count;
    ++cap.draw_calls;
}

static int fail(int n, const char *message) {
    fprintf(stderr, "fog-coordinate failure %d: %s\n", n, message);
    return n;
}

static int nearf(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 0.0001f;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_program *program;
    GLfloat f = -1.0f, fv = 2.25f;
    GLdouble d = 0.0, dv = -3.5;
    GLint iv = 0;
    void *ptr = NULL;
    GLfloat client_fog[2] = {1.25f, 2.5f};
    GLushort half_fog[2] = {0x3c00u, 0xc000u};
    GLdouble double_fog[2] = {4.5, -6.25};
    GLenum old_type;
    const void *old_ptr;
    unsigned i;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGenBuffers = fake_gen_buffers;
    native.glesDeleteBuffers = fake_delete_buffers;
    native.glesBindBuffer = fake_bind_buffer;
    native.glesBufferData = fake_buffer_data;
    native.glesVertexAttribPointer = fake_vertex_attrib_pointer;
    native.glesEnableVertexAttribArray = fake_enable_vertex_attrib;
    native.glesDisableVertexAttribArray = fake_disable_vertex_attrib;
    native.glesVertexAttrib4fv = fake_vertex_attrib4fv;
    native.glesUseProgram = fake_use_program;
    native.glesDrawArrays = fake_draw_arrays;
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

    glGetFloatv(GL_CURRENT_FOG_COORD, &f);
    if (!nearf(f, 0.0f))
        return fail(2, "default current fog coordinate");
    glFogCoordf(1.5f);
    glGetFloatv(GL_CURRENT_FOG_COORD, &f);
    if (!nearf(f, 1.5f))
        return fail(3, "glFogCoordf");
    glFogCoordfv(&fv);
    glGetFloatv(GL_CURRENT_FOG_COORD, &f);
    if (!nearf(f, fv))
        return fail(4, "glFogCoordfv");
    glFogCoordd(7.75);
    glGetDoublev(GL_CURRENT_FOG_COORD, &d);
    if (fabs(d - 7.75) > 0.0001)
        return fail(5, "glFogCoordd/query double");
    glFogCoorddv(&dv);
    glGetFloatv(GL_CURRENT_FOG_COORD, &f);
    if (!nearf(f, -3.5f))
        return fail(6, "glFogCoorddv");

    glFogCoordPointer(GL_FLOAT, 0, client_fog);
    glEnableClientState(GL_FOG_COORD_ARRAY);
    glGetIntegerv(GL_FOG_COORD_ARRAY, &iv);
    if (iv != 1)
        return fail(7, "fog array enable query");
    glGetIntegerv(GL_FOG_COORD_ARRAY_TYPE, &iv);
    if (iv != (GLint)GL_FLOAT)
        return fail(8, "fog array type query");
    glGetIntegerv(GL_FOG_COORD_ARRAY_STRIDE, &iv);
    if (iv != 0)
        return fail(9, "fog array stride query");
    glGetPointerv(GL_FOG_COORD_ARRAY_POINTER, &ptr);
    if (ptr != client_fog)
        return fail(10, "fog array pointer query");
    glGetIntegerv(GL_FOG_COORD_ARRAY_BUFFER_BINDING, &iv);
    if (iv != 0)
        return fail(11, "client fog array buffer query");

    old_type = ctx.current_vao->compat[MGL_COMPAT_SEM_FOG].type;
    old_ptr = ctx.current_vao->compat[MGL_COMPAT_SEM_FOG].data.client_ptr;
    glFogCoordPointer(GL_INT, 0, double_fog);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(12, "invalid fog pointer type error");
    if (ctx.current_vao->compat[MGL_COMPAT_SEM_FOG].type != old_type ||
        ctx.current_vao->compat[MGL_COMPAT_SEM_FOG].data.client_ptr != old_ptr)
        return fail(13, "invalid fog pointer type mutation");
    glFogCoordPointer(GL_DOUBLE, -1, double_fog);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(14, "negative fog stride error");
    if (ctx.current_vao->compat[MGL_COMPAT_SEM_FOG].type != old_type ||
        ctx.current_vao->compat[MGL_COMPAT_SEM_FOG].data.client_ptr != old_ptr)
        return fail(15, "invalid fog stride mutation");

    glFogCoordPointer(GL_HALF_FLOAT, 0, half_fog);
    glArrayElement(1);
    glGetFloatv(GL_CURRENT_FOG_COORD, &f);
    if (!nearf(f, -2.0f))
        return fail(16, "half fog array through glArrayElement");
    glFogCoordPointer(GL_DOUBLE, 0, double_fog);
    glArrayElement(0);
    glGetFloatv(GL_CURRENT_FOG_COORD, &f);
    if (!nearf(f, 4.5f))
        return fail(17, "double fog array through glArrayElement");

    glNewList(1u, GL_COMPILE);
    glFogCoordf(9.0f);
    glEndList();
    glFogCoordf(-1.0f);
    glCallList(1u);
    glGetFloatv(GL_CURRENT_FOG_COORD, &f);
    if (!nearf(f, 9.0f))
        return fail(18, "display-list fog coordinate replay");

    program = calloc(1, sizeof(*program));
    if (program == NULL)
        return fail(19, "program allocation");
    program->name = 7u;
    program->linked = GL_TRUE;
    for (i = 0u; i < MGL_COMPAT_SEMANTIC_COUNT; ++i)
        program->compat_attrib[i] = -1;
    program->compat_attrib[MGL_COMPAT_SEM_FOG] = 4;
    program->next = ctx.share_group->programs;
    ctx.share_group->programs = program;
    ctx.current_program = program->name;

    glFogCoordPointer(GL_DOUBLE, 0, double_fog);
    glEnableClientState(GL_FOG_COORD_ARRAY);
    glDrawArrays(GL_POINTS, 0, 2);
    if (glGetError() != GL_NO_ERROR || cap.draw_calls != 1u)
        return fail(20, "double fog draw");
    if (cap.pointer_location != 4u || cap.pointer_size != 1 || cap.pointer_type != GL_FLOAT)
        return fail(21, "double fog realized as float attribute");
    if (cap.array_size != 2u * sizeof(GLfloat))
        return fail(22, "double fog packed size");
    if (!nearf(((const GLfloat *)cap.array_data)[0], 4.5f) ||
        !nearf(((const GLfloat *)cap.array_data)[1], -6.25f))
        return fail(23, "double fog packed values");

    glFogCoordPointer(GL_HALF_FLOAT, 0, half_fog);
    glDrawArrays(GL_POINTS, 0, 2);
    if (glGetError() != GL_NO_ERROR || cap.draw_calls != 2u)
        return fail(24, "half fog draw");
    if (cap.pointer_type != GL_FLOAT || cap.array_size != 2u * sizeof(GLfloat))
        return fail(25, "half fog realized as float attribute");
    if (!nearf(((const GLfloat *)cap.array_data)[0], 1.0f) ||
        !nearf(((const GLfloat *)cap.array_data)[1], -2.0f))
        return fail(26, "half fog packed values");

    if (glXGetProcAddress((const GLubyte *)"glFogCoordf") == NULL ||
        glXGetProcAddress((const GLubyte *)"glFogCoordfv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glFogCoordd") == NULL ||
        glXGetProcAddress((const GLubyte *)"glFogCoorddv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glFogCoordPointer") == NULL)
        return fail(27, "proc-address exposure");

    free(cap.array_data);
    glDeleteLists(1u, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL fog-coordinate unit test: PASS");
    return 0;
}

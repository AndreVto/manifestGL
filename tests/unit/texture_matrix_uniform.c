#include "compat/fixed_function.h"
#include "native/dispatch.h"
#include "core/context.h"
#include "shader/shader_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int upload_count;
static GLint uploaded_location;
static GLfloat uploaded_matrix[16];

static void record_matrix(GLint location, GLsizei count, GLboolean transpose,
                          const GLfloat *values) {
    if (count != 1 || transpose != GL_FALSE)
        abort();
    ++upload_count;
    uploaded_location = location;
    memcpy(uploaded_matrix, values, sizeof(uploaded_matrix));
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_program program;
    struct mgl_native_dispatch native;
    int i;

    memset(&ctx, 0, sizeof(ctx));
    memset(&program, 0, sizeof(program));
    memset(&native, 0, sizeof(native));
    mgl_program_init_metadata(&program);
    mgl_compat_state_init(&ctx.compat);
    native.glesUniformMatrix4fv = record_matrix;
    ctx.native_dispatch = &native;
    program.compat_uniform_texture_matrix[2] = 72;

    mgl_compat_realize_program_uniforms(&ctx, &program);
    if (upload_count != 1 || uploaded_location != 72 || uploaded_matrix[0] != 1.0f ||
        uploaded_matrix[5] != 1.0f || uploaded_matrix[10] != 1.0f || uploaded_matrix[15] != 1.0f)
        return 1;
    for (i = 0; i < 16; ++i)
        if (i != 0 && i != 5 && i != 10 && i != 15 && uploaded_matrix[i] != 0.0f)
            return 2;

    mgl_compat_realize_program_uniforms(&ctx, &program);
    if (upload_count != 1)
        return 3;

    ctx.compat.texture[2][0][12] = 9.0f;
    mgl_compat_realize_program_uniforms(&ctx, &program);
    if (upload_count != 2 || uploaded_matrix[12] != 9.0f)
        return 4;

    ctx.compat.texture[1][0][12] = 3.0f;
    mgl_compat_realize_program_uniforms(&ctx, &program);
    if (upload_count != 2)
        return 5;

    free(program.compat_uniform_cache);
    puts("Texture matrix uniform: PASS");
    return 0;
}

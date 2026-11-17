#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"

#include <stdio.h>
#include <string.h>

static unsigned precision_calls;
static GLenum last_shader_type;
static GLenum last_precision_type;

static void fake_precision(GLenum shadertype, GLenum precisiontype, GLint *range,
                           GLint *precision) {
    ++precision_calls;
    last_shader_type = shadertype;
    last_precision_type = precisiontype;
    if (precisiontype == GL_HIGH_FLOAT) {
        range[0] = 127;
        range[1] = 127;
        *precision = 23;
    } else {
        range[0] = 31;
        range[1] = 30;
        *precision = 0;
    }
}

static int fail(int n, const char *msg) {
    fprintf(stderr, "shader ES2 compatibility failure %d: %s\n", n, msg);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLint range[2] = {-1, -1};
    GLint precision = -1;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGetShaderPrecisionFormat = fake_precision;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_context_set_current_for_test(&ctx);

    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_HIGH_FLOAT, range, &precision);
    if (glGetError() != GL_NO_ERROR || precision_calls != 1u ||
        last_shader_type != GL_VERTEX_SHADER || last_precision_type != GL_HIGH_FLOAT ||
        range[0] != 127 || range[1] != 127 || precision != 23)
        return fail(2, "valid float precision forwarding");

    range[0] = 99;
    range[1] = 98;
    precision = 97;
    glGetShaderPrecisionFormat(GL_COMPUTE_SHADER, GL_HIGH_FLOAT, range, &precision);
    if (glGetError() != GL_INVALID_ENUM || precision_calls != 1u || range[0] != 99 ||
        range[1] != 98 || precision != 97)
        return fail(3, "invalid shader type mutated output/native");

    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, 0xdeadbeefu, range, &precision);
    if (glGetError() != GL_INVALID_ENUM || precision_calls != 1u || range[0] != 99 ||
        range[1] != 98 || precision != 97)
        return fail(4, "invalid precision type mutated output/native");

    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_INT, range, &precision);
    if (glGetError() != GL_NO_ERROR || precision_calls != 2u ||
        last_shader_type != GL_FRAGMENT_SHADER || last_precision_type != GL_HIGH_INT ||
        range[0] != 31 || range[1] != 30 || precision != 0)
        return fail(5, "valid integer precision forwarding");

    glReleaseShaderCompiler();
    if (glGetError() != GL_NO_ERROR)
        return fail(6, "release compiler hint generated error");

    if (glXGetProcAddress((const GLubyte *)"glGetShaderPrecisionFormat") == NULL ||
        glXGetProcAddress((const GLubyte *)"glReleaseShaderCompiler") == NULL)
        return fail(7, "proc-address exposure");

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

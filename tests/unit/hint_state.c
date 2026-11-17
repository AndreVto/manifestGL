#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static int fail(int code, const char *message) {
    fprintf(stderr, "hint state failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    GLint value = 0;
    GLfloat fvalue = 0.0f;
    GLuint list;
    GLenum targets[] = {GL_PERSPECTIVE_CORRECTION_HINT,
                        GL_POINT_SMOOTH_HINT,
                        GL_LINE_SMOOTH_HINT,
                        GL_POLYGON_SMOOTH_HINT,
                        GL_FOG_HINT,
                        GL_GENERATE_MIPMAP_HINT,
                        GL_TEXTURE_COMPRESSION_HINT,
                        GL_FRAGMENT_SHADER_DERIVATIVE_HINT};
    unsigned i;

    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 320, 240);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    for (i = 0u; i < sizeof(targets) / sizeof(targets[0]); ++i) {
        value = 0;
        glGetIntegerv(targets[i], &value);
        if (value != (GLint)GL_DONT_CARE)
            return fail(2, "initial hint state");
    }

    glHint(GL_FOG_HINT, GL_NICEST);
    glHint(GL_TEXTURE_COMPRESSION_HINT, GL_FASTEST);
    glGetIntegerv(GL_FOG_HINT, &value);
    if (value != (GLint)GL_NICEST)
        return fail(3, "integer query");
    glGetFloatv(GL_TEXTURE_COMPRESSION_HINT, &fvalue);
    if ((GLenum)(GLint)fvalue != GL_FASTEST)
        return fail(4, "float query");

    glHint(0xDEADBEEFu, GL_NICEST);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(5, "invalid target");
    glHint(GL_FOG_HINT, 0xDEADBEEFu);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(6, "invalid mode");
    glGetIntegerv(GL_FOG_HINT, &value);
    if (value != (GLint)GL_NICEST)
        return fail(7, "invalid mode mutated state");

    glPushAttrib(GL_HINT_BIT);
    glHint(GL_FOG_HINT, GL_FASTEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glPopAttrib();
    glGetIntegerv(GL_FOG_HINT, &value);
    if (value != (GLint)GL_NICEST)
        return fail(8, "hint attrib restore");
    glGetIntegerv(GL_LINE_SMOOTH_HINT, &value);
    if (value != (GLint)GL_DONT_CARE)
        return fail(9, "second hint attrib restore");

    list = glGenLists(1);
    if (list == 0u)
        return fail(10, "list allocation");
    glHint(GL_POINT_SMOOTH_HINT, GL_DONT_CARE);
    glNewList(list, GL_COMPILE);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glEndList();
    glGetIntegerv(GL_POINT_SMOOTH_HINT, &value);
    if (value != (GLint)GL_DONT_CARE)
        return fail(11, "GL_COMPILE mutated hint");
    glCallList(list);
    glGetIntegerv(GL_POINT_SMOOTH_HINT, &value);
    if (value != (GLint)GL_NICEST)
        return fail(12, "list hint replay");

    if (glXGetProcAddress((const GLubyte *)"glHint") == NULL)
        return fail(13, "proc-address exposure");

    glDeleteLists(list, 1);
    if (glGetError() != GL_NO_ERROR)
        return fail(14, "unexpected GL error");
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL hint state unit test: PASS");
    return 0;
}

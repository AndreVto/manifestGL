#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int fail(int n, const char *why) {
    fprintf(stderr, "indexed-state failure %d: %s\n", n, why);
    return n;
}

static int get_i(GLenum pname, GLuint index) {
    GLint value = -1;
    glGetIntegeri_v(pname, index, &value);
    return value;
}

int main(void) {
    struct mgl_context ctx;
    GLboolean mask[4] = {0, 0, 0, 0};
    GLuint list;
    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 1, 1);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    if (glIsEnabledi(GL_BLEND, 3) != GL_FALSE)
        return fail(2, "initial indexed blend");
    glEnablei(GL_BLEND, 3);
    if (glIsEnabledi(GL_BLEND, 3) != GL_TRUE || glIsEnabledi(GL_BLEND, 2) != GL_FALSE)
        return fail(3, "indexed blend independence");
    glDisablei(GL_BLEND, 3);
    if (glIsEnabledi(GL_BLEND, 3) != GL_FALSE)
        return fail(4, "indexed disable");

    glColorMaski(2, GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
    glGetBooleani_v(GL_COLOR_WRITEMASK, 2, mask);
    if (!mask[0] || mask[1] || !mask[2] || mask[3])
        return fail(5, "indexed color mask");
    glBlendFuncSeparatei(2, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
    glBlendEquationSeparatei(2, GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT);
    if (get_i(GL_BLEND_SRC_RGB, 2) != (GLint)GL_SRC_ALPHA ||
        get_i(GL_BLEND_DST_ALPHA, 2) != (GLint)GL_ZERO)
        return fail(6, "indexed factors");
    if (get_i(GL_BLEND_EQUATION_RGB, 2) != (GLint)GL_FUNC_REVERSE_SUBTRACT ||
        get_i(GL_BLEND_EQUATION_ALPHA, 2) != (GLint)GL_FUNC_SUBTRACT)
        return fail(7, "indexed equations");

    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
    glGetBooleani_v(GL_COLOR_WRITEMASK, 6, mask);
    if (mask[0] || !mask[1] || mask[2] || !mask[3])
        return fail(8, "global color mask propagation");
    glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (get_i(GL_BLEND_SRC_RGB, 6) != (GLint)GL_ONE ||
        get_i(GL_BLEND_DST_ALPHA, 6) != (GLint)GL_ONE_MINUS_SRC_ALPHA)
        return fail(9, "global blend propagation");

    glActiveTexture(GL_TEXTURE0);
    glEnableIndexedEXT(GL_TEXTURE_2D, 3);
    if (ctx.compat.active_texture != 0u || !glIsEnabledIndexedEXT(GL_TEXTURE_2D, 3) ||
        glIsEnabledIndexedEXT(GL_TEXTURE_2D, 2))
        return fail(10, "EXT texture-unit enable");
    glEnableIndexed(GL_TEXTURE_GEN_S, 4);
    if (ctx.compat.active_texture != 0u || !glIsEnabledIndexed(GL_TEXTURE_GEN_S, 4))
        return fail(11, "unsuffixed indexed texgen");

    glClientActiveTexture(GL_TEXTURE0 + 5u);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glClientActiveTexture(GL_TEXTURE0);
    /* Mesa's EXT indexed capability query rejects GL_TEXTURE_COORD_ARRAY;
     * client-array enable state remains queryable via the selected client unit. */
    if (glIsEnabledIndexedEXT(GL_TEXTURE_COORD_ARRAY, 5) != GL_FALSE ||
        glGetError() != GL_INVALID_ENUM || ctx.client_active_texture != 0u)
        return fail(12, "indexed client-array query legality");
    glClientActiveTexture(GL_TEXTURE0 + 5u);
    if (!glIsEnabled(GL_TEXTURE_COORD_ARRAY) || glGetError() != GL_NO_ERROR)
        return fail(12, "indexed client-array state preserved");
    glClientActiveTexture(GL_TEXTURE0);

    glEnablei(GL_TEXTURE_2D, 0);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(13, "core Enablei must not use texture-unit semantics");
    glEnablei(GL_BLEND, MGL_MAX_DRAW_BUFFERS);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(14, "blend index range");
    glEnableIndexedEXT(GL_TEXTURE_2D, 8);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(15, "texture unit range");

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glEnablei(GL_BLEND, 5);
    glColorMaski(5, GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
    glBlendFuncSeparatei(5, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
    glBlendEquationSeparatei(5, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT);
    glEnableIndexedEXT(GL_TEXTURE_2D, 6);
    glEndList();
    if (glIsEnabledi(GL_BLEND, 5) || glIsEnabledIndexedEXT(GL_TEXTURE_2D, 6))
        return fail(16, "list executed while compiling");
    glCallList(list);
    if (!glIsEnabledi(GL_BLEND, 5) || !glIsEnabledIndexedEXT(GL_TEXTURE_2D, 6))
        return fail(17, "indexed list replay");
    glGetBooleani_v(GL_COLOR_WRITEMASK, 5, mask);
    if (!mask[0] || mask[1] || mask[2] || !mask[3])
        return fail(18, "list color mask replay");
    if (get_i(GL_BLEND_EQUATION_RGB, 5) != (GLint)GL_FUNC_SUBTRACT)
        return fail(19, "list equation replay");

    if (glXGetProcAddress((const GLubyte *)"glEnablei") == NULL ||
        glXGetProcAddress((const GLubyte *)"glEnableIndexed") == NULL ||
        glXGetProcAddress((const GLubyte *)"glEnableIndexedEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glColorMaskIndexedEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glBlendFuncSeparateiARB") == NULL ||
        glXGetProcAddress((const GLubyte *)"glBlendEquationSeparateiEXT") == NULL)
        return fail(20, "proc surface");

    {
        GLfloat fvalues[4] = {-9.0f, -9.0f, -9.0f, -9.0f};
        GLdouble dvalues[4] = {-9.0, -9.0, -9.0, -9.0};
        GLboolean boolean = GL_FALSE;
        GLint64 i64 = 0;
        const GLintptr large_offset = (GLintptr)(UINT64_C(1) << 34u) + 1024;

        glGetFloati_v(GL_BLEND_SRC_RGB, 2u, fvalues);
        glGetDoublei_v(GL_BLEND_DST_ALPHA, 2u, dvalues);
        if (fvalues[0] != (GLfloat)GL_ONE || dvalues[0] != (GLdouble)GL_ONE_MINUS_SRC_ALPHA ||
            glGetError() != GL_NO_ERROR)
            return fail(21, "indexed float/double blend parameters");
        glColorMaski(2u, GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
        glGetFloati_v(GL_COLOR_WRITEMASK, 2u, fvalues);
        glGetDoublei_v(GL_COLOR_WRITEMASK, 2u, dvalues);
        if (fvalues[0] != 1.0f || fvalues[1] != 0.0f || fvalues[2] != 1.0f ||
            fvalues[3] != 0.0f || dvalues[0] != 1.0 || dvalues[1] != 0.0 ||
            dvalues[2] != 1.0 || dvalues[3] != 0.0 || glGetError() != GL_NO_ERROR)
            return fail(22, "indexed float/double color masks");
        glGetInteger64i_v(GL_SAMPLE_MASK_VALUE, 0u, &i64);
        glGetDoublei_v(GL_SAMPLE_MASK_VALUE, 0u, dvalues);
        if (i64 != -1 || dvalues[0] != -1.0 ||
            glGetError() != GL_NO_ERROR)
            return fail(23, "indexed sample-mask type variants");
        glGetFloati_v(GL_SAMPLE_MASK_VALUE, 1u, fvalues);
        if (glGetError() != GL_INVALID_VALUE)
            return fail(24, "sample-mask index validation");

        if (sizeof(GLintptr) >= 8u) {
            glBindVertexBuffer(3u, 0u, large_offset, 16);
            glGetDoublei_v(GL_VERTEX_BINDING_OFFSET, 3u, dvalues);
            if (dvalues[0] != (GLdouble)large_offset || glGetError() != GL_NO_ERROR)
                return fail(25, "indexed double preserves 64-bit vertex offset");
            glGetDoubleIndexedv(GL_VERTEX_BINDING_OFFSET, 3u, dvalues);
            if (dvalues[0] != (GLdouble)large_offset || glGetError() != GL_NO_ERROR)
                return fail(26, "EXT indexed double preserves 64-bit vertex offset");
            glGetBooleani_v(GL_VERTEX_BINDING_OFFSET, 3u, &boolean);
            if (boolean != GL_TRUE || glGetError() != GL_NO_ERROR)
                return fail(27, "indexed boolean sees nonzero 64-bit offset");
        }
        glGetFloati_v(GL_UNIFORM_BUFFER_BINDING, 0u, fvalues);
        glGetDoublei_v(GL_UNIFORM_BUFFER_BINDING, 0u, dvalues);
        if (fvalues[0] != 0.0f || dvalues[0] != 0.0 || glGetError() != GL_NO_ERROR)
            return fail(28, "indexed buffer binding variants");

        glEnablei(GL_TEXTURE_2D, 0u);
        glGetFloati_v(GL_BLEND_SRC_RGB, 2u, fvalues);
        glGetDoublei_v(GL_BLEND_DST_ALPHA, 2u, dvalues);
        glGetBooleani_v(GL_VERTEX_BINDING_OFFSET, 3u, &boolean);
        glGetInteger64i_v(GL_SAMPLE_MASK_VALUE, 0u, &i64);
        if (fvalues[0] != (GLfloat)GL_ONE || dvalues[0] != (GLdouble)GL_ONE_MINUS_SRC_ALPHA ||
            boolean != GL_TRUE || i64 != -1 ||
            glGetError() != GL_INVALID_ENUM || glGetError() != GL_NO_ERROR)
            return fail(29, "valid indexed queries preserve older GL error");
    }

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL indexed state unit test: PASS");
    return 0;
}

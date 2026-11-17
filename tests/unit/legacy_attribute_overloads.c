#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "compat/fixed_function.h"
#include "shader/translator.h"
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check4(const GLfloat *actual, const GLfloat *expected) {
    for (int i = 0; i < 4; ++i) {
        if (fabsf(actual[i] - expected[i]) > 0.0000001f)
            return 0;
    }
    return 1;
}

static int fail(int id, const char *name) {
    fprintf(stderr, "legacy attribute failure %d: %s\n", id, name);
    return id;
}

int main(void) {
    struct mgl_context ctx;
    GLuint list;
    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (!ctx.share_group)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.native_max_vertex_attribs = 16;
    mgl_context_set_current_for_test(&ctx);

    const GLbyte b_values[4] = {INT8_MIN, 0, INT8_MAX, 64};
    const GLdouble d_values[4] = {-2.5, 0.25, 3.5, 0.75};
    const GLint i_values[4] = {INT32_MIN, 0, INT32_MAX, INT32_C(1073741824)};
    const GLshort s_values[4] = {INT16_MIN, 0, INT16_MAX, INT16_C(16384)};
    const GLubyte ub_values[4] = {0, UINT8_MAX, 128u, 64u};
    const GLuint ui_values[4] = {0u, UINT32_MAX, UINT32_C(2147483648), UINT32_C(1073741824)};
    const GLushort us_values[4] = {0, UINT16_MAX, 32768u, 16384u};
    glColor3b(b_values[0], b_values[1], b_values[2]);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 255.0f), 1.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(2, "glColor3b");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(102, "glColor3b GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor3b"))
        return fail(202, "glColor3b proc");
    glColor3bv(b_values);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 255.0f), 1.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(3, "glColor3bv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(103, "glColor3bv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor3bv"))
        return fail(203, "glColor3bv proc");
    glColor3d(d_values[0], d_values[1], d_values[2]);
    {
        const GLfloat want[4] = {-2.5f, 0.25f, 3.5f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(4, "glColor3d");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(104, "glColor3d GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor3d"))
        return fail(204, "glColor3d proc");
    glColor3dv(d_values);
    {
        const GLfloat want[4] = {-2.5f, 0.25f, 3.5f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(5, "glColor3dv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(105, "glColor3dv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor3dv"))
        return fail(205, "glColor3dv proc");
    glColor3i(i_values[0], i_values[1], i_values[2]);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 4294967294.0f), 1.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(6, "glColor3i");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(106, "glColor3i GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor3i"))
        return fail(206, "glColor3i proc");
    glColor3iv(i_values);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 4294967294.0f), 1.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(7, "glColor3iv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(107, "glColor3iv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor3iv"))
        return fail(207, "glColor3iv proc");
    glColor3s(s_values[0], s_values[1], s_values[2]);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 65535.0f), 1.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(8, "glColor3s");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(108, "glColor3s GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor3s"))
        return fail(208, "glColor3s proc");
    glColor3sv(s_values);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 65535.0f), 1.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(9, "glColor3sv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(109, "glColor3sv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor3sv"))
        return fail(209, "glColor3sv proc");
    glColor3ub(ub_values[0], ub_values[1], ub_values[2]);
    {
        const GLfloat want[4] = {0.0f, 1.0f, (128.0f / 255.0f), 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(10, "glColor3ub");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(110, "glColor3ub GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor3ub"))
        return fail(210, "glColor3ub proc");
    glColor3ui(ui_values[0], ui_values[1], ui_values[2]);
    {
        const GLfloat want[4] = {0.0f, 1.0f, 0.5f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(11, "glColor3ui");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(111, "glColor3ui GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor3ui"))
        return fail(211, "glColor3ui proc");
    glColor3uiv(ui_values);
    {
        const GLfloat want[4] = {0.0f, 1.0f, 0.5f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(12, "glColor3uiv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(112, "glColor3uiv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor3uiv"))
        return fail(212, "glColor3uiv proc");
    glColor3us(us_values[0], us_values[1], us_values[2]);
    {
        const GLfloat want[4] = {0.0f, 1.0f, (32768.0f / 65535.0f), 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(13, "glColor3us");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(113, "glColor3us GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor3us"))
        return fail(213, "glColor3us proc");
    glColor3usv(us_values);
    {
        const GLfloat want[4] = {0.0f, 1.0f, (32768.0f / 65535.0f), 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(14, "glColor3usv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(114, "glColor3usv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor3usv"))
        return fail(214, "glColor3usv proc");
    glColor4b(b_values[0], b_values[1], b_values[2], b_values[3]);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 255.0f), 1.0f, (129.0f / 255.0f)};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(15, "glColor4b");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(115, "glColor4b GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor4b"))
        return fail(215, "glColor4b proc");
    glColor4bv(b_values);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 255.0f), 1.0f, (129.0f / 255.0f)};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(16, "glColor4bv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(116, "glColor4bv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor4bv"))
        return fail(216, "glColor4bv proc");
    glColor4d(d_values[0], d_values[1], d_values[2], d_values[3]);
    {
        const GLfloat want[4] = {-2.5f, 0.25f, 3.5f, 0.75f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(17, "glColor4d");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(117, "glColor4d GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor4d"))
        return fail(217, "glColor4d proc");
    glColor4dv(d_values);
    {
        const GLfloat want[4] = {-2.5f, 0.25f, 3.5f, 0.75f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(18, "glColor4dv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(118, "glColor4dv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor4dv"))
        return fail(218, "glColor4dv proc");
    glColor4i(i_values[0], i_values[1], i_values[2], i_values[3]);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 4294967294.0f), 1.0f, 0.5f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(19, "glColor4i");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(119, "glColor4i GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor4i"))
        return fail(219, "glColor4i proc");
    glColor4iv(i_values);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 4294967294.0f), 1.0f, 0.5f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(20, "glColor4iv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(120, "glColor4iv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor4iv"))
        return fail(220, "glColor4iv proc");
    glColor4s(s_values[0], s_values[1], s_values[2], s_values[3]);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 65535.0f), 1.0f, (32769.0f / 65535.0f)};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(21, "glColor4s");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(121, "glColor4s GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor4s"))
        return fail(221, "glColor4s proc");
    glColor4sv(s_values);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 65535.0f), 1.0f, (32769.0f / 65535.0f)};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(22, "glColor4sv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(122, "glColor4sv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor4sv"))
        return fail(222, "glColor4sv proc");
    glColor4ui(ui_values[0], ui_values[1], ui_values[2], ui_values[3]);
    {
        const GLfloat want[4] = {0.0f, 1.0f, 0.5f, 0.25f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(23, "glColor4ui");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(123, "glColor4ui GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor4ui"))
        return fail(223, "glColor4ui proc");
    glColor4uiv(ui_values);
    {
        const GLfloat want[4] = {0.0f, 1.0f, 0.5f, 0.25f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(24, "glColor4uiv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(124, "glColor4uiv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor4uiv"))
        return fail(224, "glColor4uiv proc");
    glColor4us(us_values[0], us_values[1], us_values[2], us_values[3]);
    {
        const GLfloat want[4] = {0.0f, 1.0f, (32768.0f / 65535.0f), (16384.0f / 65535.0f)};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(25, "glColor4us");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(125, "glColor4us GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor4us"))
        return fail(225, "glColor4us proc");
    glColor4usv(us_values);
    {
        const GLfloat want[4] = {0.0f, 1.0f, (32768.0f / 65535.0f), (16384.0f / 65535.0f)};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], want))
            return fail(26, "glColor4usv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(126, "glColor4usv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glColor4usv"))
        return fail(226, "glColor4usv proc");
    glMultiTexCoord1d(GL_TEXTURE3, d_values[0]);
    {
        const GLfloat want[4] = {(GLfloat)d_values[0], 0.0f, 0.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 3u], want))
            return fail(27, "glMultiTexCoord1d");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(127, "glMultiTexCoord1d GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord1d"))
        return fail(227, "glMultiTexCoord1d proc");
    glMultiTexCoord1dv(GL_TEXTURE4, d_values);
    {
        const GLfloat want[4] = {(GLfloat)d_values[0], 0.0f, 0.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 4u], want))
            return fail(28, "glMultiTexCoord1dv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(128, "glMultiTexCoord1dv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord1dv"))
        return fail(228, "glMultiTexCoord1dv proc");
    glMultiTexCoord1i(GL_TEXTURE5, i_values[0]);
    {
        const GLfloat want[4] = {(GLfloat)i_values[0], 0.0f, 0.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 5u], want))
            return fail(29, "glMultiTexCoord1i");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(129, "glMultiTexCoord1i GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord1i"))
        return fail(229, "glMultiTexCoord1i proc");
    glMultiTexCoord1iv(GL_TEXTURE6, i_values);
    {
        const GLfloat want[4] = {(GLfloat)i_values[0], 0.0f, 0.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 6u], want))
            return fail(30, "glMultiTexCoord1iv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(130, "glMultiTexCoord1iv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord1iv"))
        return fail(230, "glMultiTexCoord1iv proc");
    glMultiTexCoord1s(GL_TEXTURE7, s_values[0]);
    {
        const GLfloat want[4] = {(GLfloat)s_values[0], 0.0f, 0.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 7u], want))
            return fail(31, "glMultiTexCoord1s");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(131, "glMultiTexCoord1s GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord1s"))
        return fail(231, "glMultiTexCoord1s proc");
    glMultiTexCoord1sv(GL_TEXTURE0, s_values);
    {
        const GLfloat want[4] = {(GLfloat)s_values[0], 0.0f, 0.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 0u], want))
            return fail(32, "glMultiTexCoord1sv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(132, "glMultiTexCoord1sv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord1sv"))
        return fail(232, "glMultiTexCoord1sv proc");
    glMultiTexCoord2d(GL_TEXTURE1, d_values[0], d_values[1]);
    {
        const GLfloat want[4] = {(GLfloat)d_values[0], (GLfloat)d_values[1], 0.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 1u], want))
            return fail(33, "glMultiTexCoord2d");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(133, "glMultiTexCoord2d GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord2d"))
        return fail(233, "glMultiTexCoord2d proc");
    glMultiTexCoord2dv(GL_TEXTURE2, d_values);
    {
        const GLfloat want[4] = {(GLfloat)d_values[0], (GLfloat)d_values[1], 0.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 2u], want))
            return fail(34, "glMultiTexCoord2dv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(134, "glMultiTexCoord2dv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord2dv"))
        return fail(234, "glMultiTexCoord2dv proc");
    glMultiTexCoord2i(GL_TEXTURE3, i_values[0], i_values[1]);
    {
        const GLfloat want[4] = {(GLfloat)i_values[0], (GLfloat)i_values[1], 0.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 3u], want))
            return fail(35, "glMultiTexCoord2i");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(135, "glMultiTexCoord2i GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord2i"))
        return fail(235, "glMultiTexCoord2i proc");
    glMultiTexCoord2iv(GL_TEXTURE4, i_values);
    {
        const GLfloat want[4] = {(GLfloat)i_values[0], (GLfloat)i_values[1], 0.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 4u], want))
            return fail(36, "glMultiTexCoord2iv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(136, "glMultiTexCoord2iv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord2iv"))
        return fail(236, "glMultiTexCoord2iv proc");
    glMultiTexCoord2s(GL_TEXTURE5, s_values[0], s_values[1]);
    {
        const GLfloat want[4] = {(GLfloat)s_values[0], (GLfloat)s_values[1], 0.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 5u], want))
            return fail(37, "glMultiTexCoord2s");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(137, "glMultiTexCoord2s GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord2s"))
        return fail(237, "glMultiTexCoord2s proc");
    glMultiTexCoord2sv(GL_TEXTURE6, s_values);
    {
        const GLfloat want[4] = {(GLfloat)s_values[0], (GLfloat)s_values[1], 0.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 6u], want))
            return fail(38, "glMultiTexCoord2sv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(138, "glMultiTexCoord2sv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord2sv"))
        return fail(238, "glMultiTexCoord2sv proc");
    glMultiTexCoord2svARB(GL_TEXTURE7, s_values);
    {
        const GLfloat want[4] = {(GLfloat)s_values[0], (GLfloat)s_values[1], 0.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 7u], want))
            return fail(39, "glMultiTexCoord2svARB");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(139, "glMultiTexCoord2svARB GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord2svARB"))
        return fail(239, "glMultiTexCoord2svARB proc");
    glMultiTexCoord3d(GL_TEXTURE0, d_values[0], d_values[1], d_values[2]);
    {
        const GLfloat want[4] = {(GLfloat)d_values[0], (GLfloat)d_values[1], (GLfloat)d_values[2],
                                 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 0u], want))
            return fail(40, "glMultiTexCoord3d");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(140, "glMultiTexCoord3d GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord3d"))
        return fail(240, "glMultiTexCoord3d proc");
    glMultiTexCoord3dv(GL_TEXTURE1, d_values);
    {
        const GLfloat want[4] = {(GLfloat)d_values[0], (GLfloat)d_values[1], (GLfloat)d_values[2],
                                 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 1u], want))
            return fail(41, "glMultiTexCoord3dv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(141, "glMultiTexCoord3dv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord3dv"))
        return fail(241, "glMultiTexCoord3dv proc");
    glMultiTexCoord3i(GL_TEXTURE2, i_values[0], i_values[1], i_values[2]);
    {
        const GLfloat want[4] = {(GLfloat)i_values[0], (GLfloat)i_values[1], (GLfloat)i_values[2],
                                 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 2u], want))
            return fail(42, "glMultiTexCoord3i");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(142, "glMultiTexCoord3i GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord3i"))
        return fail(242, "glMultiTexCoord3i proc");
    glMultiTexCoord3iv(GL_TEXTURE3, i_values);
    {
        const GLfloat want[4] = {(GLfloat)i_values[0], (GLfloat)i_values[1], (GLfloat)i_values[2],
                                 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 3u], want))
            return fail(43, "glMultiTexCoord3iv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(143, "glMultiTexCoord3iv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord3iv"))
        return fail(243, "glMultiTexCoord3iv proc");
    glMultiTexCoord3s(GL_TEXTURE4, s_values[0], s_values[1], s_values[2]);
    {
        const GLfloat want[4] = {(GLfloat)s_values[0], (GLfloat)s_values[1], (GLfloat)s_values[2],
                                 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 4u], want))
            return fail(44, "glMultiTexCoord3s");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(144, "glMultiTexCoord3s GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord3s"))
        return fail(244, "glMultiTexCoord3s proc");
    glMultiTexCoord3sv(GL_TEXTURE5, s_values);
    {
        const GLfloat want[4] = {(GLfloat)s_values[0], (GLfloat)s_values[1], (GLfloat)s_values[2],
                                 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 5u], want))
            return fail(45, "glMultiTexCoord3sv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(145, "glMultiTexCoord3sv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord3sv"))
        return fail(245, "glMultiTexCoord3sv proc");
    glMultiTexCoord4d(GL_TEXTURE6, d_values[0], d_values[1], d_values[2], d_values[3]);
    {
        const GLfloat want[4] = {(GLfloat)d_values[0], (GLfloat)d_values[1], (GLfloat)d_values[2],
                                 (GLfloat)d_values[3]};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 6u], want))
            return fail(46, "glMultiTexCoord4d");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(146, "glMultiTexCoord4d GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord4d"))
        return fail(246, "glMultiTexCoord4d proc");
    glMultiTexCoord4dv(GL_TEXTURE7, d_values);
    {
        const GLfloat want[4] = {(GLfloat)d_values[0], (GLfloat)d_values[1], (GLfloat)d_values[2],
                                 (GLfloat)d_values[3]};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 7u], want))
            return fail(47, "glMultiTexCoord4dv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(147, "glMultiTexCoord4dv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord4dv"))
        return fail(247, "glMultiTexCoord4dv proc");
    glMultiTexCoord4i(GL_TEXTURE0, i_values[0], i_values[1], i_values[2], i_values[3]);
    {
        const GLfloat want[4] = {(GLfloat)i_values[0], (GLfloat)i_values[1], (GLfloat)i_values[2],
                                 (GLfloat)i_values[3]};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 0u], want))
            return fail(48, "glMultiTexCoord4i");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(148, "glMultiTexCoord4i GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord4i"))
        return fail(248, "glMultiTexCoord4i proc");
    glMultiTexCoord4iv(GL_TEXTURE1, i_values);
    {
        const GLfloat want[4] = {(GLfloat)i_values[0], (GLfloat)i_values[1], (GLfloat)i_values[2],
                                 (GLfloat)i_values[3]};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 1u], want))
            return fail(49, "glMultiTexCoord4iv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(149, "glMultiTexCoord4iv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord4iv"))
        return fail(249, "glMultiTexCoord4iv proc");
    glMultiTexCoord4s(GL_TEXTURE2, s_values[0], s_values[1], s_values[2], s_values[3]);
    {
        const GLfloat want[4] = {(GLfloat)s_values[0], (GLfloat)s_values[1], (GLfloat)s_values[2],
                                 (GLfloat)s_values[3]};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 2u], want))
            return fail(50, "glMultiTexCoord4s");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(150, "glMultiTexCoord4s GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord4s"))
        return fail(250, "glMultiTexCoord4s proc");
    glMultiTexCoord4sv(GL_TEXTURE3, s_values);
    {
        const GLfloat want[4] = {(GLfloat)s_values[0], (GLfloat)s_values[1], (GLfloat)s_values[2],
                                 (GLfloat)s_values[3]};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 3u], want))
            return fail(51, "glMultiTexCoord4sv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(151, "glMultiTexCoord4sv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord4sv"))
        return fail(251, "glMultiTexCoord4sv proc");
    glMultiTexCoord4svARB(GL_TEXTURE4, s_values);
    {
        const GLfloat want[4] = {(GLfloat)s_values[0], (GLfloat)s_values[1], (GLfloat)s_values[2],
                                 (GLfloat)s_values[3]};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 4u], want))
            return fail(52, "glMultiTexCoord4svARB");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(152, "glMultiTexCoord4svARB GL error");
    if (!glXGetProcAddress((const GLubyte *)"glMultiTexCoord4svARB"))
        return fail(252, "glMultiTexCoord4svARB proc");
    glNormal3b(b_values[0], b_values[1], b_values[2]);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 255.0f), 1.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_NORMAL], want))
            return fail(53, "glNormal3b");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(153, "glNormal3b GL error");
    if (!glXGetProcAddress((const GLubyte *)"glNormal3b"))
        return fail(253, "glNormal3b proc");
    glNormal3bv(b_values);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 255.0f), 1.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_NORMAL], want))
            return fail(54, "glNormal3bv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(154, "glNormal3bv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glNormal3bv"))
        return fail(254, "glNormal3bv proc");
    glNormal3d(d_values[0], d_values[1], d_values[2]);
    {
        const GLfloat want[4] = {-2.5f, 0.25f, 3.5f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_NORMAL], want))
            return fail(55, "glNormal3d");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(155, "glNormal3d GL error");
    if (!glXGetProcAddress((const GLubyte *)"glNormal3d"))
        return fail(255, "glNormal3d proc");
    glNormal3dv(d_values);
    {
        const GLfloat want[4] = {-2.5f, 0.25f, 3.5f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_NORMAL], want))
            return fail(56, "glNormal3dv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(156, "glNormal3dv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glNormal3dv"))
        return fail(256, "glNormal3dv proc");
    glNormal3i(i_values[0], i_values[1], i_values[2]);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 4294967294.0f), 1.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_NORMAL], want))
            return fail(57, "glNormal3i");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(157, "glNormal3i GL error");
    if (!glXGetProcAddress((const GLubyte *)"glNormal3i"))
        return fail(257, "glNormal3i proc");
    glNormal3iv(i_values);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 4294967294.0f), 1.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_NORMAL], want))
            return fail(58, "glNormal3iv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(158, "glNormal3iv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glNormal3iv"))
        return fail(258, "glNormal3iv proc");
    glNormal3s(s_values[0], s_values[1], s_values[2]);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 65535.0f), 1.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_NORMAL], want))
            return fail(59, "glNormal3s");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(159, "glNormal3s GL error");
    if (!glXGetProcAddress((const GLubyte *)"glNormal3s"))
        return fail(259, "glNormal3s proc");
    glNormal3sv(s_values);
    {
        const GLfloat want[4] = {-1.0f, (1.0f / 65535.0f), 1.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_NORMAL], want))
            return fail(60, "glNormal3sv");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(160, "glNormal3sv GL error");
    if (!glXGetProcAddress((const GLubyte *)"glNormal3sv"))
        return fail(260, "glNormal3sv proc");

    {
        GLfloat before[4];
        memcpy(before, ctx.compat_current[MGL_COMPAT_SEM_COLOR], sizeof(before));
        glColor4iv(NULL);
        if (!check4(before, ctx.compat_current[MGL_COMPAT_SEM_COLOR]))
            return fail(410, "NULL color vector");
        glNormal3dv(NULL);
        glMultiTexCoord2sv(GL_TEXTURE2, NULL);
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(411, "NULL vector error");
    }
    glBegin(GL_POINTS);
    glColor3ub(255u, 128u, 0u);
    glNormal3s(INT16_MIN, 0, INT16_MAX);
    glMultiTexCoord2i(GL_TEXTURE2, 5, -6);
    glVertex3i(1, 2, 3);
    if (ctx.immediate.count != 1u)
        return fail(420, "immediate vertex count");
    {
        const GLfloat color[4] = {1.0f, 128.0f / 255.0f, 0.0f, 1.0f};
        const GLfloat normal[4] = {-1.0f, (1.0f / 65535.0f), 1.0f, 1.0f};
        const GLfloat texcoord[4] = {5.0f, -6.0f, 0.0f, 1.0f};
        if (!check4(ctx.immediate.vertices[0].color, color) ||
            !check4(ctx.immediate.vertices[0].normal, normal) ||
            !check4(ctx.immediate.vertices[0].texcoord[2], texcoord))
            return fail(421, "immediate vertex attributes");
    }
    glEnd();
    (void)mgl_error_take(&ctx.errors);
    list = glGenLists(1);
    if (!list)
        return fail(430, "display list allocate");
    glNewList(list, GL_COMPILE);
    glColor4us(65535u, 0u, 32768u, 16384u);
    glNormal3b(INT8_MIN, 0, INT8_MAX);
    glMultiTexCoord3d(GL_TEXTURE4, 1.25, -2.5, 3.75);
    glEndList();
    glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
    glNormal3f(0.0f, 0.0f, 0.0f);
    glMultiTexCoord4f(GL_TEXTURE4, 0.0f, 0.0f, 0.0f, 0.0f);
    glCallList(list);
    {
        const GLfloat color[4] = {1.0f, 0.0f, 32768.0f / 65535.0f, 16384.0f / 65535.0f};
        const GLfloat normal[4] = {-1.0f, (1.0f / 255.0f), 1.0f, 1.0f};
        const GLfloat texcoord[4] = {1.25f, -2.5f, 3.75f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], color) ||
            !check4(ctx.compat_current[MGL_COMPAT_SEM_NORMAL], normal) ||
            !check4(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 4u], texcoord))
            return fail(431, "display list attributes");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(432, "display list errors");

    /* The signed integer zero maps above zero for current colors and normals. */
    glColor4i(0, -1, INT32_MIN, INT32_MAX);
    {
        const GLfloat *color = ctx.compat_current[MGL_COMPAT_SEM_COLOR];
        if (color[0] != (1.0f / 4294967294.0f) || color[1] != -(1.0f / 4294967294.0f) ||
            color[2] != -1.0f || color[3] != 1.0f)
            return fail(440, "signed int color mapping");
    }
    glNormal3i(0, -1, INT32_MAX);
    {
        const GLfloat *normal = ctx.compat_current[MGL_COMPAT_SEM_NORMAL];
        if (normal[0] != (1.0f / 4294967294.0f) || normal[1] != -(1.0f / 4294967294.0f) ||
            normal[2] != 1.0f)
            return fail(441, "signed int normal mapping");
    }
    glColor3b(0, -1, 64);
    {
        const GLfloat expected[4] = {1.0f / 255.0f, -1.0f / 255.0f, 129.0f / 255.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_COLOR], expected))
            return fail(442, "signed byte color mapping");
    }
    glNormal3s(0, -1, INT16_MIN);
    {
        const GLfloat expected[4] = {1.0f / 65535.0f, -1.0f / 65535.0f, -1.0f, 1.0f};
        if (!check4(ctx.compat_current[MGL_COMPAT_SEM_NORMAL], expected))
            return fail(443, "signed short normal mapping");
    }
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(444, "signed conversion errors");
    glDeleteLists(list, 1);
    free(ctx.immediate.vertices);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("legacy attribute overloads verified");
    return 0;
}

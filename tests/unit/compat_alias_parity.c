#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "compat/fixed_function.h"
#include "shader/translator.h"

#include <math.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(int n, const char *message) {
    fprintf(stderr, "compat alias parity failure %d: %s\n", n, message);
    return n;
}

static int nearf(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 0.0001f;
}

static int neard(GLdouble a, GLdouble b) {
    return fabs(a - b) < 0.0001;
}

static int check4f(const GLfloat *v, GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
    return nearf(v[0], a) && nearf(v[1], b) && nearf(v[2], c) && nearf(v[3], d);
}

int main(void) {
    struct mgl_context ctx;
    GLfloat color3[3] = {0.25f, 0.5f, 0.75f};
    GLfloat color4[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    GLfloat normal[3] = {-1.0f, 0.5f, 2.0f};
    GLfloat secondary[3] = {0.6f, 0.7f, 0.8f};
    GLfloat tc1[1] = {0.125f};
    GLfloat tc2[2] = {0.2f, 0.3f};
    GLfloat tc3[3] = {0.4f, 0.5f, 0.6f};
    GLfloat tc4[4] = {0.7f, 0.8f, 0.9f, 0.25f};
    GLfloat pos2[2] = {1.0f, 2.0f};
    GLfloat pos3[3] = {3.0f, 4.0f, 5.0f};
    GLfloat pos4[4] = {6.0f, 7.0f, 8.0f, 2.0f};
    GLfloat attr4[4] = {-3.5f, 2.25f, 8.0f, 0.5f};
    GLfloat qf[4] = {0};
    GLdouble qd[4] = {0};
    GLint qi = 0;
    GLfloat client_data[16] = {0};
    void *ptr = NULL;
    GLuint list;

    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.native_max_vertex_attribs = 16;
    mgl_context_set_current_for_test(&ctx);

    /* Exact scalar compatibility entries share the same canonical current-state owners as
     * their vector forms; exercise the scalar ABI directly before the vector-alias checks. */
    glColor3f(0.9f, 0.8f, 0.7f);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_COLOR], 0.9f, 0.8f, 0.7f, 1.0f))
        return fail(80, "glColor3f state");
    glColor4f(0.6f, 0.5f, 0.4f, 0.3f);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_COLOR], 0.6f, 0.5f, 0.4f, 0.3f))
        return fail(81, "glColor4f state");
    glNormal3f(0.25f, -0.5f, 0.75f);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_NORMAL], 0.25f, -0.5f, 0.75f, 1.0f))
        return fail(82, "glNormal3f state");
    glSecondaryColor3f(0.11f, 0.22f, 0.33f);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_SECONDARY], 0.11f, 0.22f, 0.33f, 1.0f))
        return fail(83, "glSecondaryColor3f state");
    glTexCoord2f(0.31f, 0.41f);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], 0.31f, 0.41f, 0.0f, 1.0f))
        return fail(84, "glTexCoord2f state");
    glTexCoord4f(0.51f, 0.61f, 0.71f, 0.81f);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], 0.51f, 0.61f, 0.71f, 0.81f))
        return fail(85, "glTexCoord4f state");
    {
        const GLint tc2i[2] = {-3, 7};
        const GLdouble tc2d[2] = {1.25, -2.5};
        const GLshort tc3s[3] = {2, -4, 8};
        glTexCoord2i(12, -16);
        if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], 12.0f, -16.0f, 0.0f, 1.0f))
            return fail(90, "glTexCoord2i canonical state");
        glTexCoord2iv(tc2i);
        if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], -3.0f, 7.0f, 0.0f, 1.0f))
            return fail(91, "glTexCoord2iv canonical state");
        glTexCoord2s(-5, 6);
        if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], -5.0f, 6.0f, 0.0f, 1.0f))
            return fail(92, "glTexCoord2s canonical state");
        glTexCoord2dv(tc2d);
        if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], 1.25f, -2.5f, 0.0f, 1.0f))
            return fail(93, "glTexCoord2dv canonical state");
        glTexCoord3sv(tc3s);
        if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], 2.0f, -4.0f, 8.0f, 1.0f))
            return fail(94, "glTexCoord3sv canonical state");
        glTexCoord4i(2, 3, 4, 5);
        if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], 2.0f, 3.0f, 4.0f, 5.0f))
            return fail(95, "glTexCoord4i canonical state");
        glTexCoord1i(3);
        if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], 3.0f, 0.0f, 0.0f, 1.0f))
            return fail(96, "glTexCoord1i canonical state");
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(97, "legacy texcoord errors");
    }
    glMultiTexCoord2f(GL_TEXTURE3, 0.12f, 0.34f);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 3u], 0.12f, 0.34f, 0.0f, 1.0f))
        return fail(86, "glMultiTexCoord2f state");
    glMultiTexCoord4f(GL_TEXTURE5, 0.21f, 0.43f, 0.65f, 0.87f);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 5u], 0.21f, 0.43f, 0.65f, 0.87f))
        return fail(87, "glMultiTexCoord4f state");

    glColor3fv(color3);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_COLOR], 0.25f, 0.5f, 0.75f, 1.0f))
        return fail(2, "glColor3fv state");
    glColor4fv(color4);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_COLOR], 0.1f, 0.2f, 0.3f, 0.4f))
        return fail(3, "glColor4fv state");
    glNormal3fv(normal);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_NORMAL], -1.0f, 0.5f, 2.0f, 1.0f))
        return fail(4, "glNormal3fv state");
    glSecondaryColor3fv(secondary);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_SECONDARY], 0.6f, 0.7f, 0.8f, 1.0f))
        return fail(5, "glSecondaryColor3fv state");

    glTexCoord1fv(tc1);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], 0.125f, 0.0f, 0.0f, 1.0f))
        return fail(6, "glTexCoord1fv defaults");
    glTexCoord2fv(tc2);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], 0.2f, 0.3f, 0.0f, 1.0f))
        return fail(7, "glTexCoord2fv defaults");
    glTexCoord3fv(tc3);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], 0.4f, 0.5f, 0.6f, 1.0f))
        return fail(8, "glTexCoord3fv defaults");
    glTexCoord4fv(tc4);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], 0.7f, 0.8f, 0.9f, 0.25f))
        return fail(9, "glTexCoord4fv state");

    glMultiTexCoord1fv(GL_TEXTURE2, tc1);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 2u], 0.125f, 0.0f, 0.0f, 1.0f))
        return fail(10, "glMultiTexCoord1fv state");
    glMultiTexCoord2fv(GL_TEXTURE3, tc2);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 3u], 0.2f, 0.3f, 0.0f, 1.0f))
        return fail(11, "glMultiTexCoord2fv state");
    glMultiTexCoord3fv(GL_TEXTURE4, tc3);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 4u], 0.4f, 0.5f, 0.6f, 1.0f))
        return fail(12, "glMultiTexCoord3fv state");
    glMultiTexCoord4fv(GL_TEXTURE5, tc4);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 5u], 0.7f, 0.8f, 0.9f, 0.25f))
        return fail(13, "glMultiTexCoord4fv state");
    /* Native Mesa indexes eight current texture coordinate slots by the low
     * three selector bits, including selectors outside GL_TEXTURE0..7. */
    glMultiTexCoord2f(GL_TEXTURE0 + 99u, 0.0f, 0.0f);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR ||
        !check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0 + 3u], 0.f, 0.f, 0.f, 1.f))
        return fail(14, "multitex selector low-bit target");

    list = glGenLists(1);
    if (list == 0u)
        return fail(15, "display list allocation");
    glNewList(list, GL_COMPILE);
    glColor4fv(color4);
    glTexCoord3fv(tc3);
    glEndList();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexCoord4f(0.0f, 0.0f, 0.0f, 1.0f);
    glCallList(list);
    if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_COLOR], 0.1f, 0.2f, 0.3f, 0.4f) ||
        !check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], 0.4f, 0.5f, 0.6f, 1.0f))
        return fail(16, "vector list replay");

    glBegin(GL_POINTS);
    glVertex2fv(pos2);
    glVertex3fv(pos3);
    glVertex4fv(pos4);
    if (ctx.immediate.count != 3u)
        return fail(17, "vector immediate count");
    if (!check4f(ctx.immediate.vertices[0].position, 1.0f, 2.0f, 0.0f, 1.0f) ||
        !check4f(ctx.immediate.vertices[1].position, 3.0f, 4.0f, 5.0f, 1.0f) ||
        !check4f(ctx.immediate.vertices[2].position, 6.0f, 7.0f, 8.0f, 2.0f))
        return fail(18, "vector immediate positions");
    ctx.immediate.count = 0u;
    glEnd();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(19, "vector immediate end");

    glBegin(GL_POINTS);
    glVertex2f(10.0f, 11.0f);
    glVertex3f(12.0f, 13.0f, 14.0f);
    glVertex4f(15.0f, 16.0f, 17.0f, 18.0f);
    if (ctx.immediate.count != 3u ||
        !check4f(ctx.immediate.vertices[0].position, 10.0f, 11.0f, 0.0f, 1.0f) ||
        !check4f(ctx.immediate.vertices[1].position, 12.0f, 13.0f, 14.0f, 1.0f) ||
        !check4f(ctx.immediate.vertices[2].position, 15.0f, 16.0f, 17.0f, 18.0f))
        return fail(88, "scalar immediate positions");
    ctx.immediate.count = 0u;
    glEnd();
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(89, "scalar immediate end");

    {
        const GLint vertex3i[3] = {-7, 13, 19};
        const GLshort vertex4s[4] = {1, 2, 3, 4};
        const GLdouble vertex2d[2] = {4.5, -3.25};
        glBegin(GL_POINTS);
        glTexCoord2i(5, 9);
        glVertex3i(10, 20, -30);
        glVertex3iv(vertex3i);
        glVertex3s(4, 5, 6);
        glVertex4sv(vertex4s);
        glVertex2dv(vertex2d);
        if (ctx.immediate.count != 5u ||
            !check4f(ctx.immediate.vertices[0].position, 10.0f, 20.0f, -30.0f, 1.0f) ||
            !check4f(ctx.immediate.vertices[1].position, -7.0f, 13.0f, 19.0f, 1.0f) ||
            !check4f(ctx.immediate.vertices[2].position, 4.0f, 5.0f, 6.0f, 1.0f) ||
            !check4f(ctx.immediate.vertices[3].position, 1.0f, 2.0f, 3.0f, 4.0f) ||
            !check4f(ctx.immediate.vertices[4].position, 4.5f, -3.25f, 0.0f, 1.0f) ||
            !check4f(ctx.immediate.vertices[0].texcoord[0], 5.0f, 9.0f, 0.0f, 1.0f))
            return fail(98, "integer/double vertex canonical batches");
        ctx.immediate.count = 0u;
        glEnd();
        if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(99, "legacy vertex errors");
        list = glGenLists(1);
        if (list == 0u)
            return fail(100, "legacy list allocation");
        glNewList(list, GL_COMPILE);
        glTexCoord2i(-8, 12);
        glEndList();
        glTexCoord2f(0.0f, 0.0f);
        glCallList(list);
        if (!check4f(ctx.compat_current[MGL_COMPAT_SEM_TEXCOORD0], -8.0f, 12.0f, 0.0f, 1.0f) ||
            mgl_error_take(&ctx.errors) != GL_NO_ERROR)
            return fail(101, "integer aliases display list replay");
        glDeleteLists(list, 1);
    }
    if (glXGetProcAddress((const GLubyte *)"glTexCoord2i") == NULL ||
        glXGetProcAddress((const GLubyte *)"glVertex3i") == NULL)
        return fail(102, "integer aliases proc address");
    glVertexAttrib1fARB(3u, 1.5f);
    glGetVertexAttribfvARB(3u, GL_CURRENT_VERTEX_ATTRIB, qf);
    if (!check4f(qf, 1.5f, 0.0f, 0.0f, 1.0f))
        return fail(20, "ARB attrib1 current");
    glVertexAttrib2fvARB(3u, tc2);
    glGetVertexAttribdvARB(3u, GL_CURRENT_VERTEX_ATTRIB, qd);
    if (!neard(qd[0], 0.2) || !neard(qd[1], 0.3) || !neard(qd[2], 0.0) || !neard(qd[3], 1.0))
        return fail(21, "ARB attrib2 double query");
    glVertexAttrib3fARB(3u, 0.4f, 0.5f, 0.6f);
    glVertexAttrib4fvARB(3u, attr4);
    glGetVertexAttribdv(3u, GL_CURRENT_VERTEX_ATTRIB, qd);
    if (!neard(qd[0], -3.5) || !neard(qd[1], 2.25) || !neard(qd[2], 8.0) || !neard(qd[3], 0.5))
        return fail(22, "core double current query");

    glVertexAttribPointerARB(3u, 4, GL_FLOAT, GL_FALSE, 16, client_data);
    glEnableVertexAttribArrayARB(3u);
    glGetVertexAttribivARB(3u, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &qi);
    if (qi != 1)
        return fail(23, "ARB enable query");
    glGetVertexAttribivARB(3u, GL_VERTEX_ATTRIB_ARRAY_SIZE, &qi);
    if (qi != 4)
        return fail(24, "ARB size query");
    glGetVertexAttribPointervARB(3u, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);
    if (ptr != client_data)
        return fail(25, "ARB pointer query");
    glDisableVertexAttribArrayARB(3u);
    glGetVertexAttribiv(3u, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &qi);
    if (qi != 0)
        return fail(26, "ARB disable aliases core state");

    glVertexAttribPointerARB(99u, 4, GL_FLOAT, GL_FALSE, 0, client_data);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(27, "ARB pointer error forwarding");

    {
        GLdouble dv1[1] = {1.125}, dv2[2] = {2.25, -3.5}, dv3[3] = {4.0, -5.0, 6.5};
        GLdouble dv4[4] = {1.25, -2.5, 3.75, 0.5};
        GLshort sv1[1] = {-2}, sv2[2] = {3, -4}, sv3[3] = {5, -6, 7};
        GLshort sv4[4] = {-7, 8, -9, 10};
        GLbyte bv4[4] = {-1, 2, -3, 4};
        GLint iv4[4] = {-11, 12, -13, 14};
        GLubyte ubv4[4] = {15u, 16u, 17u, 18u};
        GLushort usv4[4] = {19u, 20u, 21u, 22u};
        GLuint uiv4[4] = {23u, 24u, 25u, 26u};
        GLbyte nbv[4] = {SCHAR_MIN, -64, 0, SCHAR_MAX};
        GLshort nsv[4] = {SHRT_MIN, -16384, 0, SHRT_MAX};
        GLint niv[4] = {INT_MIN, INT_MIN / 2, 0, INT_MAX};
        GLubyte nubv[4] = {0u, 64u, 128u, UCHAR_MAX};
        GLushort nusv[4] = {0u, 16384u, 32768u, USHRT_MAX};
        GLuint nuiv[4] = {0u, UINT_MAX / 4u, UINT_MAX / 2u, UINT_MAX};

        glVertexAttrib1d(4u, dv1[0]);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 1.125f, 0.0f, 0.0f, 1.0f))
            return fail(29, "core attrib1d conversion");
        glVertexAttrib2dv(4u, dv2);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 2.25f, -3.5f, 0.0f, 1.0f))
            return fail(30, "core attrib2dv conversion");
        glVertexAttrib3dARB(4u, dv3[0], dv3[1], dv3[2]);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 4.0f, -5.0f, 6.5f, 1.0f))
            return fail(31, "ARB attrib3d conversion");
        glVertexAttrib4dv(4u, dv4);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 1.25f, -2.5f, 3.75f, 0.5f))
            return fail(32, "core double attrib conversion");

        glVertexAttrib1sARB(4u, sv1[0]);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, -2.0f, 0.0f, 0.0f, 1.0f))
            return fail(33, "ARB attrib1s conversion");
        glVertexAttrib2svARB(4u, sv2);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 3.0f, -4.0f, 0.0f, 1.0f))
            return fail(34, "ARB attrib2sv conversion");
        glVertexAttrib3svARB(4u, sv3);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 5.0f, -6.0f, 7.0f, 1.0f))
            return fail(35, "ARB attrib3sv conversion");
        glVertexAttrib4svARB(4u, sv4);
        glGetVertexAttribfvARB(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, -7.0f, 8.0f, -9.0f, 10.0f))
            return fail(36, "ARB short attrib conversion");
        glVertexAttrib1dARB(4u, dv1[0]);
        glVertexAttrib1dvARB(4u, dv1);
        glVertexAttrib2dARB(4u, dv2[0], dv2[1]);
        glVertexAttrib2dvARB(4u, dv2);
        glVertexAttrib3dvARB(4u, dv3);
        glVertexAttrib4dARB(4u, dv4[0], dv4[1], dv4[2], dv4[3]);
        glVertexAttrib4dvARB(4u, dv4);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 1.25f, -2.5f, 3.75f, 0.5f))
            return fail(51, "ARB double alias family");
        glVertexAttrib1svARB(4u, sv1);
        glVertexAttrib2sARB(4u, sv2[0], sv2[1]);
        glVertexAttrib3sARB(4u, sv3[0], sv3[1], sv3[2]);
        glVertexAttrib4sARB(4u, sv4[0], sv4[1], sv4[2], sv4[3]);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, -7.0f, 8.0f, -9.0f, 10.0f))
            return fail(52, "ARB short alias family");
        glVertexAttrib4bvARB(4u, bv4);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, -1.0f, 2.0f, -3.0f, 4.0f))
            return fail(37, "ARB byte vector conversion");
        glVertexAttrib4ivARB(4u, iv4);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, -11.0f, 12.0f, -13.0f, 14.0f))
            return fail(38, "ARB int vector conversion");
        glVertexAttrib4ubvARB(4u, ubv4);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 15.0f, 16.0f, 17.0f, 18.0f))
            return fail(39, "ARB ubyte vector conversion");
        glVertexAttrib4usvARB(4u, usv4);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 19.0f, 20.0f, 21.0f, 22.0f))
            return fail(40, "ARB ushort vector conversion");
        glVertexAttrib4uivARB(4u, uiv4);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 23.0f, 24.0f, 25.0f, 26.0f))
            return fail(41, "ARB uint vector conversion");

        glVertexAttrib4NbvARB(4u, nbv);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!nearf(qf[0], -1.0f) || !nearf(qf[1], (2.0f * -64.0f + 1.0f) / 255.0f) ||
            !nearf(qf[2], 1.0f / 255.0f) || !nearf(qf[3], 1.0f))
            return fail(42, "normalized byte attrib");

        glVertexAttrib4NsvARB(4u, nsv);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!nearf(qf[0], -1.0f) || !nearf(qf[1], (2.0f * -16384.0f + 1.0f) / 65535.0f) ||
            !nearf(qf[2], 1.0f / 65535.0f) || !nearf(qf[3], 1.0f))
            return fail(43, "normalized short attrib");

        glVertexAttrib4NivARB(4u, niv);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!nearf(qf[0], -1.0f) || !(qf[1] < -0.49f && qf[1] > -0.51f) ||
            !(qf[2] > 0.0f && qf[2] < 1.0e-8f) || !nearf(qf[3], 1.0f))
            return fail(44, "normalized int attrib");

        glVertexAttrib4NubvARB(4u, nubv);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!nearf(qf[0], 0.0f) || !nearf(qf[1], 64.0f / 255.0f) ||
            !nearf(qf[2], 128.0f / 255.0f) || !nearf(qf[3], 1.0f))
            return fail(45, "normalized ubyte attrib");

        glVertexAttrib4NusvARB(4u, nusv);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!nearf(qf[0], 0.0f) || !nearf(qf[1], 16384.0f / 65535.0f) ||
            !nearf(qf[2], 32768.0f / 65535.0f) || !nearf(qf[3], 1.0f))
            return fail(46, "normalized ushort attrib");

        glVertexAttrib4NuivARB(4u, nuiv);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!nearf(qf[0], 0.0f) || !(qf[1] > 0.24f && qf[1] < 0.26f) ||
            !(qf[2] > 0.49f && qf[2] < 0.51f) || !nearf(qf[3], 1.0f))
            return fail(47, "normalized uint attrib");

        glVertexAttrib4NubARB(4u, 0u, 85u, 170u, 255u);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!nearf(qf[0], 0.0f) || !nearf(qf[1], 1.0f / 3.0f) || !nearf(qf[2], 2.0f / 3.0f) ||
            !nearf(qf[3], 1.0f))
            return fail(48, "normalized ubyte scalar attrib");
        glVertexAttrib4Nub(4u, 255u, 170u, 85u, 0u);
        glGetVertexAttribfv(4u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!nearf(qf[0], 1.0f) || !nearf(qf[1], 2.0f / 3.0f) || !nearf(qf[2], 1.0f / 3.0f) ||
            !nearf(qf[3], 0.0f))
            return fail(53, "core normalized ubyte scalar attrib");

        /* Test core fixed-point and double current-attribute entry points through their
         * observable canonical state. */
        glVertexAttrib1dv(5u, dv1);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 1.125f, 0.0f, 0.0f, 1.0f))
            return fail(54, "core attrib1dv conversion");
        glVertexAttrib1s(5u, sv1[0]);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, -2.0f, 0.0f, 0.0f, 1.0f))
            return fail(55, "core attrib1s conversion");
        glVertexAttrib1sv(5u, sv1);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, -2.0f, 0.0f, 0.0f, 1.0f))
            return fail(56, "core attrib1sv conversion");
        glVertexAttrib2d(5u, dv2[0], dv2[1]);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 2.25f, -3.5f, 0.0f, 1.0f))
            return fail(57, "core attrib2d conversion");
        glVertexAttrib2s(5u, sv2[0], sv2[1]);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 3.0f, -4.0f, 0.0f, 1.0f))
            return fail(58, "core attrib2s conversion");
        glVertexAttrib2sv(5u, sv2);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 3.0f, -4.0f, 0.0f, 1.0f))
            return fail(59, "core attrib2sv conversion");
        glVertexAttrib3d(5u, dv3[0], dv3[1], dv3[2]);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 4.0f, -5.0f, 6.5f, 1.0f))
            return fail(60, "core attrib3d conversion");
        glVertexAttrib3dv(5u, dv3);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 4.0f, -5.0f, 6.5f, 1.0f))
            return fail(61, "core attrib3dv conversion");
        glVertexAttrib3s(5u, sv3[0], sv3[1], sv3[2]);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 5.0f, -6.0f, 7.0f, 1.0f))
            return fail(62, "core attrib3s conversion");
        glVertexAttrib3sv(5u, sv3);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 5.0f, -6.0f, 7.0f, 1.0f))
            return fail(63, "core attrib3sv conversion");
        glVertexAttrib4d(5u, dv4[0], dv4[1], dv4[2], dv4[3]);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 1.25f, -2.5f, 3.75f, 0.5f))
            return fail(64, "core attrib4d conversion");
        glVertexAttrib4s(5u, sv4[0], sv4[1], sv4[2], sv4[3]);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, -7.0f, 8.0f, -9.0f, 10.0f))
            return fail(65, "core attrib4s conversion");
        glVertexAttrib4sv(5u, sv4);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, -7.0f, 8.0f, -9.0f, 10.0f))
            return fail(66, "core attrib4sv conversion");
        glVertexAttrib4bv(5u, bv4);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, -1.0f, 2.0f, -3.0f, 4.0f))
            return fail(67, "core attrib4bv conversion");
        glVertexAttrib4iv(5u, iv4);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, -11.0f, 12.0f, -13.0f, 14.0f))
            return fail(68, "core attrib4iv conversion");
        glVertexAttrib4ubv(5u, ubv4);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 15.0f, 16.0f, 17.0f, 18.0f))
            return fail(69, "core attrib4ubv conversion");
        glVertexAttrib4usv(5u, usv4);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 19.0f, 20.0f, 21.0f, 22.0f))
            return fail(70, "core attrib4usv conversion");
        glVertexAttrib4uiv(5u, uiv4);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!check4f(qf, 23.0f, 24.0f, 25.0f, 26.0f))
            return fail(71, "core attrib4uiv conversion");

        glVertexAttrib4Nbv(5u, nbv);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!nearf(qf[0], -1.0f) || !nearf(qf[1], (2.0f * -64.0f + 1.0f) / 255.0f) ||
            !nearf(qf[2], 1.0f / 255.0f) || !nearf(qf[3], 1.0f))
            return fail(72, "core normalized byte attrib");
        glVertexAttrib4Nsv(5u, nsv);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!nearf(qf[0], -1.0f) || !nearf(qf[1], (2.0f * -16384.0f + 1.0f) / 65535.0f) ||
            !nearf(qf[2], 1.0f / 65535.0f) || !nearf(qf[3], 1.0f))
            return fail(73, "core normalized short attrib");
        glVertexAttrib4Niv(5u, niv);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!nearf(qf[0], -1.0f) || !(qf[1] < -0.49f && qf[1] > -0.51f) ||
            !(qf[2] > 0.0f && qf[2] < 1.0e-8f) || !nearf(qf[3], 1.0f))
            return fail(74, "core normalized int attrib");
        glVertexAttrib4Nubv(5u, nubv);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!nearf(qf[0], 0.0f) || !nearf(qf[1], 64.0f / 255.0f) ||
            !nearf(qf[2], 128.0f / 255.0f) || !nearf(qf[3], 1.0f))
            return fail(75, "core normalized ubyte attrib");
        glVertexAttrib4Nusv(5u, nusv);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!nearf(qf[0], 0.0f) || !nearf(qf[1], 16384.0f / 65535.0f) ||
            !nearf(qf[2], 32768.0f / 65535.0f) || !nearf(qf[3], 1.0f))
            return fail(76, "core normalized ushort attrib");
        glVertexAttrib4Nuiv(5u, nuiv);
        glGetVertexAttribfv(5u, GL_CURRENT_VERTEX_ATTRIB, qf);
        if (!nearf(qf[0], 0.0f) || !(qf[1] > 0.24f && qf[1] < 0.26f) ||
            !(qf[2] > 0.49f && qf[2] < 0.51f) || !nearf(qf[3], 1.0f))
            return fail(77, "core normalized uint attrib");

        glVertexAttrib1dARB(99u, 1.0);
        if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
            return fail(49, "new ARB alias error forwarding");
    }

    if (glXGetProcAddress((const GLubyte *)"glColor3f") == NULL ||
        glXGetProcAddress((const GLubyte *)"glColor4f") == NULL ||
        glXGetProcAddress((const GLubyte *)"glNormal3f") == NULL ||
        glXGetProcAddress((const GLubyte *)"glSecondaryColor3f") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTexCoord2f") == NULL ||
        glXGetProcAddress((const GLubyte *)"glTexCoord4f") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexCoord2f") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexCoord4f") == NULL ||
        glXGetProcAddress((const GLubyte *)"glVertex2f") == NULL ||
        glXGetProcAddress((const GLubyte *)"glVertex3f") == NULL ||
        glXGetProcAddress((const GLubyte *)"glVertex4f") == NULL ||
        glXGetProcAddress((const GLubyte *)"glColor3fv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMultiTexCoord4fv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glVertex4fv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glVertexAttribPointerARB") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetVertexAttribdv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetVertexAttribdvARB") == NULL ||
        glXGetProcAddress((const GLubyte *)"glVertexAttrib4Nub") == NULL ||
        glXGetProcAddress((const GLubyte *)"glVertexAttrib4NbvARB") == NULL ||
        glXGetProcAddress((const GLubyte *)"glVertexAttrib4NuivARB") == NULL)
        return fail(50, "proc-address coverage");

    glDeleteLists(list, 1);
    free(ctx.immediate.vertices);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("compat/vector/ARB alias parity passed");
    return 0;
}

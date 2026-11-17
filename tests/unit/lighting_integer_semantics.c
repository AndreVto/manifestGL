#include "mgl/gl.h"
#include "mgl/glx.h"
#include "compat/fixed_function.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "state/state.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(int code, const char *message) {
    fprintf(stderr, "lighting integer semantics failure %d: %s\n", code, message);
    return code;
}

static int closef(GLfloat a, GLfloat b) {
    return fabsf(a - b) < 1.0e-6f;
}

static int same4i(const GLint a[4], const GLint b[4]) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    const GLint colors[4] = {INT_MIN, 0, INT_MAX, INT_MAX / 2};
    const GLint position[4] = {2, -3, 4, 1};
    const GLint sentinel[4] = {111, 222, 333, 444};
    GLfloat fv[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GLint iv[4] = {0, 0, 0, 0};
    GLuint list;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 64, 64);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    glLightiv(GL_LIGHT0, GL_AMBIENT, colors);
    glGetLightfv(GL_LIGHT0, GL_AMBIENT, fv);
    if (!closef(fv[0], -1.0f) || fabsf(fv[1]) > 1.0e-6f || !closef(fv[2], 1.0f) ||
        !closef(fv[3], 0.5f) || glGetError() != GL_NO_ERROR)
        return fail(2, "glLightiv normalized signed-color conversion");
    glGetLightiv(GL_LIGHT0, GL_AMBIENT, iv);
    if (!same4i(iv, colors) || glGetError() != GL_NO_ERROR)
        return fail(3, "glGetLightiv inverse signed-color conversion");

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glLightiv(GL_LIGHT0, GL_POSITION, position);
    glGetLightfv(GL_LIGHT0, GL_POSITION, fv);
    if (!closef(fv[0], 2.0f) || !closef(fv[1], -3.0f) || !closef(fv[2], 4.0f) ||
        !closef(fv[3], 1.0f))
        return fail(4, "non-color integer light parameters must map directly");

    glLighti(GL_LIGHT0, GL_SPOT_EXPONENT, 12);
    glGetLightfv(GL_LIGHT0, GL_SPOT_EXPONENT, fv);
    if (!closef(fv[0], 12.0f))
        return fail(5, "glLighti scalar mapping");
    glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 12.6f);
    glGetLightiv(GL_LIGHT0, GL_SPOT_EXPONENT, iv);
    if (iv[0] != 13)
        return fail(6, "glGetLightiv nearest-integer query conversion");

    glMaterialiv(GL_FRONT, GL_DIFFUSE, colors);
    glGetMaterialfv(GL_FRONT, GL_DIFFUSE, fv);
    if (!closef(fv[0], -1.0f) || fabsf(fv[1]) > 1.0e-6f || !closef(fv[2], 1.0f) ||
        !closef(fv[3], 0.5f))
        return fail(7, "glMaterialiv normalized signed-color conversion");
    glGetMaterialiv(GL_FRONT, GL_DIFFUSE, iv);
    if (!same4i(iv, colors))
        return fail(8, "glGetMaterialiv inverse signed-color conversion");

    glMateriali(GL_FRONT, GL_SHININESS, 33);
    glGetMaterialiv(GL_FRONT, GL_SHININESS, iv);
    if (iv[0] != 33)
        return fail(9, "glMateriali scalar mapping");
    glMaterialf(GL_FRONT, GL_SHININESS, 31.6f);
    glGetMaterialiv(GL_FRONT, GL_SHININESS, iv);
    if (iv[0] != 32)
        return fail(10, "glGetMaterialiv nearest-integer query conversion");

    glBegin(GL_POINTS);
    glMateriali(GL_FRONT, GL_SHININESS, 17);
    glEnd();
    if (glGetError() != GL_NO_ERROR)
        return fail(11, "glMateriali must remain legal inside Begin/End");
    glGetMaterialiv(GL_FRONT, GL_SHININESS, iv);
    if (iv[0] != 17)
        return fail(12, "Begin/End material mutation");

    glBegin(GL_POINTS);
    glLighti(GL_LIGHT0, GL_AMBIENT, 1);
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(13, "glLighti Begin/End error must precede pname validation");

    memcpy(iv, sentinel, sizeof(iv));
    glBegin(GL_POINTS);
    glGetLightiv(GL_LIGHT0, GL_AMBIENT, iv);
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION || !same4i(iv, sentinel))
        return fail(14, "glGetLightiv Begin/End rejection/output preservation");

    memcpy(iv, sentinel, sizeof(iv));
    glBegin(GL_POINTS);
    glGetMaterialiv(GL_FRONT, GL_DIFFUSE, iv);
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION || !same4i(iv, sentinel))
        return fail(15, "glGetMaterialiv Begin/End rejection/output preservation");

    memcpy(iv, sentinel, sizeof(iv));
    glGetLightiv(GL_LIGHT0, GL_AMBIENT_AND_DIFFUSE, iv);
    if (glGetError() != GL_INVALID_ENUM || !same4i(iv, sentinel))
        return fail(16, "glGetLightiv invalid pname output preservation");
    memcpy(iv, sentinel, sizeof(iv));
    glGetMaterialiv(GL_FRONT_AND_BACK, GL_DIFFUSE, iv);
    if (glGetError() != GL_INVALID_ENUM || !same4i(iv, sentinel))
        return fail(17, "glGetMaterialiv invalid face output preservation");

    glLighti(GL_LIGHT0, GL_SPOT_EXPONENT, 3);
    list = glGenLists(1);
    if (list == 0u)
        return fail(18, "display-list allocation");
    glNewList(list, GL_COMPILE);
    glLighti(GL_LIGHT0, GL_SPOT_EXPONENT, 129);
    glEndList();
    if (glGetError() != GL_INVALID_VALUE)
        return fail(19, "invalid light value must fail during list compilation");
    glCallList(list);
    glGetLightfv(GL_LIGHT0, GL_SPOT_EXPONENT, fv);
    if (!closef(fv[0], 3.0f))
        return fail(20, "invalid light command must not be retained");

    glLightfv(GL_LIGHT1, GL_AMBIENT, (const GLfloat[4]){0.0f, 0.0f, 0.0f, 0.0f});
    glMaterialfv(GL_BACK, GL_EMISSION, (const GLfloat[4]){0.0f, 0.0f, 0.0f, 0.0f});
    glNewList(list, GL_COMPILE);
    glLightiv(GL_LIGHT1, GL_AMBIENT, colors);
    glMaterialiv(GL_BACK, GL_EMISSION, colors);
    glEndList();
    glGetLightfv(GL_LIGHT1, GL_AMBIENT, fv);
    if (!closef(fv[0], 0.0f) || !closef(fv[2], 0.0f))
        return fail(21, "GL_COMPILE executed integer light state");
    glGetMaterialfv(GL_BACK, GL_EMISSION, fv);
    if (!closef(fv[0], 0.0f) || !closef(fv[2], 0.0f))
        return fail(22, "GL_COMPILE executed integer material state");
    glCallList(list);
    glGetLightiv(GL_LIGHT1, GL_AMBIENT, iv);
    if (!same4i(iv, colors))
        return fail(23, "integer light display-list replay");
    glGetMaterialiv(GL_BACK, GL_EMISSION, iv);
    if (!same4i(iv, colors))
        return fail(24, "integer material display-list replay");

    glLightfv(GL_LIGHT0, GL_AMBIENT, (const GLfloat[4]){1.0f, 1.0f, 1.0f, 1.0f});
    glLightf(GL_LIGHT0, GL_AMBIENT, 1.0f);
    glGetLightiv(GL_LIGHT0, GL_AMBIENT, iv);
    if (iv[0] != INT_MAX || glGetError() != GL_INVALID_ENUM)
        return fail(25, "valid integer query must not consume a pre-existing error");

    if (glXGetProcAddress((const GLubyte *)"glLighti") == NULL ||
        glXGetProcAddress((const GLubyte *)"glLightiv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetLightiv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMateriali") == NULL ||
        glXGetProcAddress((const GLubyte *)"glMaterialiv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetMaterialiv") == NULL)
        return fail(26, "proc-address exposure");

    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL lighting integer semantics unit test: PASS");
    return 0;
}

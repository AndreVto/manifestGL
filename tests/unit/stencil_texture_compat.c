#include "mgl/gl.h"
#include "core/context.h"
#include "core/capabilities.h"
#include "abi/proc_address.h"
#include <stdio.h>
#include <string.h>

static struct {
    GLint max_texture_size;
    unsigned native_binds;
    unsigned native_enable_calls;
    unsigned native_disable_calls;
    struct mgl_stencil_face_state faces[2];
} cap;

static void fake_get_integer(GLenum pname, GLint *value) {
    *value = pname == GL_MAX_TEXTURE_SIZE ? cap.max_texture_size : 0;
}

static void fake_bind_texture(GLenum target, GLuint texture) {
    (void)target;
    (void)texture;
    ++cap.native_binds;
}

static void fake_enable(GLenum capname) {
    (void)capname;
    ++cap.native_enable_calls;
}

static void fake_disable(GLenum capname) {
    (void)capname;
    ++cap.native_disable_calls;
}

static void fake_stencil_func(GLenum face, GLenum func, GLint ref, GLuint mask) {
    int i;
    for (i = 0; i != 2; ++i)
        if (face == GL_FRONT_AND_BACK || face == (i == 0 ? GL_FRONT : GL_BACK)) {
            cap.faces[i].func = func;
            cap.faces[i].ref = ref;
            cap.faces[i].value_mask = mask;
        }
}

static void fake_stencil_mask(GLenum face, GLuint mask) {
    int i;
    for (i = 0; i != 2; ++i)
        if (face == GL_FRONT_AND_BACK || face == (i == 0 ? GL_FRONT : GL_BACK))
            cap.faces[i].write_mask = mask;
}

static void fake_stencil_op(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    int i;
    for (i = 0; i != 2; ++i)
        if (face == GL_FRONT_AND_BACK || face == (i == 0 ? GL_FRONT : GL_BACK)) {
            cap.faces[i].sfail = sfail;
            cap.faces[i].dpfail = dpfail;
            cap.faces[i].dppass = dppass;
        }
}

static int fail(int code, const char *message) {
    fprintf(stderr, "stencil/texture compatibility failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLint value = -1;
    unsigned binds;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    cap.max_texture_size = 1024;
    native.glesGetIntegerv = fake_get_integer;
    native.glesBindTexture = fake_bind_texture;
    native.glesEnable = fake_enable;
    native.glesDisable = fake_disable;
    native.glesStencilFuncSeparate = fake_stencil_func;
    native.glesStencilMaskSeparate = fake_stencil_mask;
    native.glesStencilOpSeparate = fake_stencil_op;
    ctx.native_dispatch = &native;
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 800, 600);
    mgl_context_set_current_for_test(&ctx);

    if (strstr((const char *)mgl_capability_extension_string(&ctx), "GL_EXT_stencil_two_side") == NULL)
        return fail(1, "two-sided stencil extension missing");
    if (glIsEnabled(GL_STENCIL_TEST_TWO_SIDE_EXT))
        return fail(2, "two-sided stencil initial state");
    glDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
    if (glGetError() != GL_NO_ERROR || cap.native_disable_calls)
        return fail(3, "desktop-only disable reaches GLES");

    glStencilFuncSeparate(GL_BACK, GL_NOTEQUAL, 4, 0x4u);
    glStencilMaskSeparate(GL_BACK, 0x40u);
    glStencilOpSeparate(GL_BACK, GL_DECR, GL_KEEP, GL_REPLACE);
    glActiveStencilFaceEXT(GL_BACK);
    glStencilFunc(GL_GREATER, 7, 0x0fu);
    glStencilOp(GL_REPLACE, GL_DECR, GL_KEEP);
    glStencilMask(0x0fu);
    glGetIntegerv(GL_STENCIL_FUNC, &value);
    if (glGetError() != GL_NO_ERROR || value != GL_GREATER || cap.faces[1].func != GL_NOTEQUAL ||
        ctx.canonical_state.stencil_back.func != GL_NOTEQUAL ||
        ctx.canonical_state.stencil_back_ext.func != GL_GREATER)
        return fail(4, "extension back state updated without enabling extension");
    glGetIntegerv(GL_STENCIL_BACK_FUNC, &value);
    if (glGetError() != GL_NO_ERROR || value != GL_NOTEQUAL)
        return fail(5, "core back query independent of extension state");

    glEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);
    if (glGetError() != GL_NO_ERROR || !glIsEnabled(GL_STENCIL_TEST_TWO_SIDE_EXT) ||
        cap.native_enable_calls || cap.faces[1].func != GL_GREATER || cap.faces[1].ref != 7 ||
        cap.faces[1].sfail != GL_REPLACE || cap.faces[1].write_mask != 0x0fu)
        return fail(6, "extension back state realizes on enable");
    glStencilFuncSeparate(GL_BACK, GL_EQUAL, 9, 0xf0u);
    glStencilMaskSeparate(GL_BACK, 0xffu);
    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_INCR);
    if (cap.faces[1].func != GL_GREATER || cap.faces[1].write_mask != 0x0fu ||
        ctx.canonical_state.stencil_back.func != GL_EQUAL ||
        ctx.canonical_state.stencil_back_ext.func != GL_GREATER)
        return fail(7, "separate core operations do not overwrite extension back state");

    glActiveStencilFaceEXT(GL_FRONT);
    glStencilFunc(GL_LESS, 3, 0xffu);
    glStencilOp(GL_KEEP, GL_INCR, GL_REPLACE);
    glStencilMask(0xf0u);
    if (cap.faces[0].func != GL_LESS || cap.faces[1].func != GL_GREATER ||
        ctx.canonical_state.stencil_back.func != GL_LESS ||
        ctx.canonical_state.stencil_back_ext.func != GL_GREATER)
        return fail(8, "front active operations maintain both core faces");
    glGetIntegerv(GL_STENCIL_FUNC, &value);
    if (glGetError() != GL_NO_ERROR || value != GL_LESS)
        return fail(9, "front query");
    glActiveStencilFaceEXT(GL_BACK);
    glGetIntegerv(GL_STENCIL_FUNC, &value);
    if (glGetError() != GL_NO_ERROR || value != GL_GREATER)
        return fail(10, "extension back query");
    glActiveStencilFaceEXT(GL_FRONT_AND_BACK);
    if (glGetError() != GL_INVALID_ENUM)
        return fail(11, "active stencil face invalid enumeration");
    glDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
    if (glGetError() != GL_NO_ERROR || cap.faces[1].func != GL_LESS ||
        cap.faces[1].write_mask != 0xf0u || ctx.canonical_state.stencil_back_ext.func != GL_GREATER)
        return fail(12, "core back state restored without deleting extension state");
    glEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);
    if (glGetError() != GL_NO_ERROR || cap.faces[1].func != GL_GREATER)
        return fail(13, "extension state reused on reenable");
    glStencilFuncSeparate(GL_FRONT_AND_BACK, GL_ALWAYS, 2, 0x7fu);
    if (cap.faces[0].func != GL_ALWAYS || cap.faces[1].func != GL_GREATER ||
        ctx.canonical_state.stencil_back.func != GL_ALWAYS)
        return fail(14, "core face selection while extension enabled");

    glPushAttrib(GL_STENCIL_BUFFER_BIT);
    glStencilFunc(GL_NEVER, 1, 1u);
    glPopAttrib();
    if (glGetError() != GL_NO_ERROR || ctx.canonical_state.stencil_back_ext.func != GL_GREATER ||
        cap.faces[1].func != GL_GREATER)
        return fail(15, "extension back state restored by attribute stack");

    binds = cap.native_binds;
    glTexImage1D(GL_PROXY_TEXTURE_1D, 1, GL_RGB, 128, 0, GL_BGR, GL_UNSIGNED_BYTE, NULL);
    glGetTexLevelParameteriv(GL_PROXY_TEXTURE_1D, 1, GL_TEXTURE_WIDTH, &value);
    if (glGetError() != GL_NO_ERROR || value != 128 || cap.native_binds != binds)
        return fail(16, "proxy texture 1D query");
    glGetTexLevelParameteriv(GL_PROXY_TEXTURE_1D, 1, GL_TEXTURE_HEIGHT, &value);
    if (glGetError() != GL_NO_ERROR || value != 1)
        return fail(17, "proxy 1D height");
    glGetTexLevelParameteriv(GL_PROXY_TEXTURE_1D, 1, GL_TEXTURE_INTERNAL_FORMAT, &value);
    if (glGetError() != GL_NO_ERROR || value != GL_RGB)
        return fail(18, "proxy 1D internal format");
    glTexImage1D(GL_PROXY_TEXTURE_1D, 1, GL_RGB, 1024, 0, GL_BGR, GL_UNSIGNED_BYTE, NULL);
    glGetTexLevelParameteriv(GL_PROXY_TEXTURE_1D, 1, GL_TEXTURE_WIDTH, &value);
    if (glGetError() != GL_NO_ERROR || value != 0)
        return fail(19, "oversized proxy 1D width");
    value = -1;
    glGetTexLevelParameteriv(GL_PROXY_TEXTURE_1D, 1, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &value);
    if (glGetError() != GL_INVALID_OPERATION || value != -1)
        return fail(20, "compressed proxy query error");
    if (mgl_proc_address("glCompressedTexImage1DARB") == NULL ||
        mgl_proc_address("glCompressedTexSubImage1DARB") == NULL)
        return fail(21, "compressed 1D ARB procedure aliases");
    mgl_context_set_current_for_test(NULL);
    return 0;
}

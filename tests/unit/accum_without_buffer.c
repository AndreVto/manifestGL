#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int check(int ok, const char *name) {
    if (!ok)
        fprintf(stderr, "accumulation state: %s\n", name);
    return ok;
}

int main(void) {
    struct mgl_context ctx;
    GLfloat values[4] = {0};
    GLint bits = -1;
    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 16, 16);
    mgl_context_set_current_for_test(&ctx);

    glGetIntegerv(GL_ACCUM_RED_BITS, &bits);
    if (!check(bits == 0, "no accumulation buffer"))
        return 2;
    glClearAccum(2.0f, -2.0f, 0.5f, -0.5f);
    glGetFloatv(GL_ACCUM_CLEAR_VALUE, values);
    if (!check(values[0] == 1.0f && values[1] == -1.0f && values[2] == 0.5f && values[3] == -0.5f,
               "clear state clamp"))
        return 3;
    glPushAttrib(GL_ACCUM_BUFFER_BIT);
    glClearAccum(0.0f, 0.0f, 0.0f, 0.0f);
    glPopAttrib();
    glGetFloatv(GL_ACCUM_CLEAR_VALUE, values);
    if (!check(values[0] == 1.0f && values[1] == -1.0f, "attribute stack"))
        return 4;
    glClear(GL_ACCUM_BUFFER_BIT);
    if (!check(mgl_error_take(&ctx.errors) == GL_NO_ERROR, "clear without accum buffer"))
        return 5;
    glAccum(GL_LOAD, 1.0f);
    if (!check(mgl_error_take(&ctx.errors) == GL_INVALID_OPERATION, "accum without buffer"))
        return 6;
    glAccum(GL_RGBA, 1.0f);
    if (!check(mgl_error_take(&ctx.errors) == GL_INVALID_ENUM, "invalid operation"))
        return 7;
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

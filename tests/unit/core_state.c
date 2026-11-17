#include "core/context.h"
#include "core/error.h"
#include "mgl/gl.h"
#include <string.h>

int main(void) {
    struct mgl_context ctx;
    struct mgl_share_group *group;
    memset(&ctx, 0, sizeof(ctx));
    mgl_error_init(&ctx.errors);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return 1;
    mgl_error_set(&ctx.errors, GL_INVALID_ENUM);
    mgl_error_set(&ctx.errors, GL_INVALID_VALUE);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return 2;
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return 3;
    mgl_context_set_current_for_test(&ctx);
    if (mgl_context_current() != &ctx)
        return 4;
    glDisable(GL_LINE_STIPPLE);
    if (glGetError() != GL_NO_ERROR)
        return 7;
    if (glIsEnabled(GL_LINE_STIPPLE) != GL_FALSE || glGetError() != GL_NO_ERROR)
        return 8;
    {
        GLboolean enabled = GL_TRUE;
        GLint value = -1;
        glGetBooleanv(GL_LINE_STIPPLE, &enabled);
        if (enabled != GL_FALSE || glGetError() != GL_NO_ERROR)
            return 9;
        glGetIntegerv(GL_LINE_STIPPLE, &value);
        if (value != 0 || glGetError() != GL_NO_ERROR)
            return 10;
    }
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    if (glGetError() != GL_NO_ERROR || glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS) != GL_TRUE)
        return 11;
    {
        GLboolean enabled = GL_FALSE;
        GLint value = 0;
        glGetBooleanv(GL_TEXTURE_CUBE_MAP_SEAMLESS, &enabled);
        glGetIntegerv(GL_TEXTURE_CUBE_MAP_SEAMLESS, &value);
        if (enabled != GL_TRUE || value != 1 || glGetError() != GL_NO_ERROR)
            return 12;
    }
    glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    if (glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS) || glGetError() != GL_NO_ERROR)
        return 13;
    mgl_context_set_current_for_test(NULL);
    if (mgl_context_current() != NULL)
        return 5;
    group = mgl_share_group_create();
    if (group == NULL)
        return 6;
    mgl_share_group_retain(group);
    mgl_share_group_release(group);
    mgl_share_group_release(group);
    return 0;
}

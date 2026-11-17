#include "mgl/gl.h"
#include "core/context.h"
#include "native/dispatch.h"

#include <stdio.h>
#include <string.h>

static const GLubyte *provider_one(GLenum name) {
    if (name == GL_VENDOR)
        return (const GLubyte *)"Vendor One";
    if (name == GL_RENDERER)
        return (const GLubyte *)"Renderer One";
    return NULL;
}

static const GLubyte *provider_two(GLenum name) {
    if (name == GL_VENDOR)
        return (const GLubyte *)"Vendor Two";
    if (name == GL_RENDERER)
        return (const GLubyte *)"Renderer Two";
    return NULL;
}

static int failure(int id) {
    fprintf(stderr, "GPU identity failure %d\n", id);
    return id;
}

int main(void) {
    struct mgl_native_dispatch first_native = {0}, second_native = {0};
    struct mgl_context first = {0}, second = {0};
    first_native.glesGetString = provider_one;
    second_native.glesGetString = provider_two;
    first.native_dispatch = &first_native;
    second.native_dispatch = &second_native;
    mgl_error_init(&first.errors);
    mgl_error_init(&second.errors);

    if (glGetString(GL_RENDERER) != NULL)
        return failure(1);
    mgl_context_set_current_for_test(&first);
    if (strcmp((const char *)glGetString(GL_VENDOR), "Vendor One") != 0)
        return failure(2);
    if (strcmp((const char *)glGetString(GL_RENDERER), "Renderer One") != 0)
        return failure(3);
    if (glGetError() != GL_NO_ERROR)
        return failure(6);

    mgl_context_set_current_for_test(&second);
    if (strcmp((const char *)glGetString(GL_VENDOR), "Vendor Two") != 0)
        return failure(7);
    if (strcmp((const char *)glGetString(GL_RENDERER), "Renderer Two") != 0)
        return failure(8);
    if (glGetError() != GL_NO_ERROR)
        return failure(9);

    second.native_dispatch = NULL;
    if (glGetString(GL_VENDOR) != NULL)
        return failure(10);
    if (glGetError() != GL_INVALID_OPERATION)
        return failure(11);
    if (glGetString(GL_RENDERER) != NULL)
        return failure(12);
    if (glGetError() != GL_INVALID_OPERATION)
        return failure(13);
    mgl_context_set_current_for_test(NULL);
    puts("GPU identity forwarding passed");
    return 0;
}

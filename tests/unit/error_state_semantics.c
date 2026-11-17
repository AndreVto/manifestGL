#include "core/error.h"

#include <stdio.h>

static int fail(const char *reason) {
    fprintf(stderr, "error state: %s\n", reason);
    return 1;
}

int main(void) {
    struct mgl_error_state state;
    mgl_error_init(&state);
    if (mgl_error_peek(&state) != GL_NO_ERROR || mgl_error_take(&state) != GL_NO_ERROR)
        return fail("initial state");

    mgl_error_set(&state, GL_INVALID_ENUM);
    mgl_error_set(&state, GL_INVALID_OPERATION);
    if (state.sequence != 2u)
        return fail("new errors are tracked while one is pending");
    if (mgl_error_peek(&state) != GL_INVALID_ENUM || mgl_error_take(&state) != GL_INVALID_ENUM ||
        mgl_error_take(&state) != GL_NO_ERROR)
        return fail("first error is retained and consumed once");

    state.suppress = GL_TRUE;
    mgl_error_set(&state, GL_INVALID_OPERATION);
    state.suppress = GL_FALSE;
    if (state.sequence != 2u)
        return fail("suppressed errors do not advance the sequence");
    if (mgl_error_peek(&state) != GL_NO_ERROR)
        return fail("suppressed error was retained");

    mgl_error_set(&state, GL_INVALID_OPERATION);
    mgl_error_set(&state, GL_INVALID_ENUM);
    if (mgl_error_take(&state) != GL_INVALID_OPERATION || mgl_error_take(&state) != GL_NO_ERROR)
        return fail("pending error was overwritten");

    mgl_error_set(&state, GL_NO_ERROR);
    if (mgl_error_peek(&state) != GL_NO_ERROR)
        return fail("GL_NO_ERROR changed the pending state");

    puts("error state semantics: PASS");
    return 0;
}

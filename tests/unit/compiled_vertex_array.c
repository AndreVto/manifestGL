#include "mgl/gl.h"
#include "abi/proc_address.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "shader/translator.h"

#include <stdio.h>
#include <string.h>

static int fail(int n, const char *message) {
    fprintf(stderr, "compiled vertex array failure %d: %s\n", n, message);
    return n;
}

static int query_lock(GLint *first, GLint *count) {
    glGetIntegerv(GL_ARRAY_ELEMENT_LOCK_FIRST_EXT, first);
    glGetIntegerv(GL_ARRAY_ELEMENT_LOCK_COUNT_EXT, count);
    return glGetError() == GL_NO_ERROR;
}

int main(void) {
    struct mgl_context ctx;
    GLint first = -1;
    GLint count = -1;

    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);

    if (!query_lock(&first, &count) || first != 0 || count != 0)
        return fail(2, "initial query state");

    glLockArrays(3, 9);
    if (glGetError() != GL_NO_ERROR)
        return fail(3, "valid unsuffixed lock");
    if (!query_lock(&first, &count) || first != 3 || count != 9)
        return fail(4, "locked query state");

    glLockArraysEXT(1, 2);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(5, "nested lock error");
    if (!query_lock(&first, &count) || first != 3 || count != 9)
        return fail(6, "nested lock mutated state");

    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glUnlockArraysEXT();
    if (glGetError() != GL_NO_ERROR)
        return fail(7, "EXT unlock");
    if (!query_lock(&first, &count) || first != 0 || count != 0)
        return fail(8, "unlock query state");
    glPopClientAttrib();
    if (glGetError() != GL_NO_ERROR)
        return fail(9, "client attrib pop");
    if (!query_lock(&first, &count) || first != 3 || count != 9)
        return fail(10, "client attrib restore");

    glClientAttribDefaultEXT(GL_CLIENT_VERTEX_ARRAY_BIT);
    if (glGetError() != GL_NO_ERROR)
        return fail(11, "client attrib default");
    if (!query_lock(&first, &count) || first != 0 || count != 0)
        return fail(12, "client default lock state");

    glUnlockArrays();
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(13, "unlock without lock");
    glLockArrays(-1, 1);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(14, "negative first");
    glLockArrays(0, 0);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(15, "zero count");
    glLockArrays(0, -1);
    if (glGetError() != GL_INVALID_VALUE)
        return fail(16, "negative count");
    if (!query_lock(&first, &count) || first != 0 || count != 0)
        return fail(17, "invalid call mutated state");

    glBegin(GL_POINTS);
    glLockArrays(0, 1);
    if (glGetError() != GL_NO_ERROR)
        return fail(18, "GetError inside Begin returns zero");
    glUnlockArrays();
    if (glGetError() != GL_NO_ERROR)
        return fail(19, "GetError inside Begin returns zero");
    glEnd();
    if (glGetError() != GL_INVALID_OPERATION || glGetError() != GL_NO_ERROR)
        return fail(20, "Begin/End errors remain pending");

    glNewList(1u, GL_COMPILE);
    if (glGetError() != GL_NO_ERROR)
        return fail(21, "new list");
    glLockArraysEXT(5, 7);
    if (glGetError() != GL_NO_ERROR)
        return fail(22, "lock executes during compile");
    if (!query_lock(&first, &count) || first != 5 || count != 7)
        return fail(23, "compile-time lock state");
    glUnlockArraysEXT();
    if (glGetError() != GL_NO_ERROR)
        return fail(24, "unlock executes during compile");
    glEndList();
    if (glGetError() != GL_NO_ERROR)
        return fail(25, "end list");
    glLockArrays(2, 4);
    if (glGetError() != GL_NO_ERROR)
        return fail(26, "pre-replay lock");
    glCallList(1u);
    if (glGetError() != GL_NO_ERROR)
        return fail(27, "call list");
    if (!query_lock(&first, &count) || first != 2 || count != 4)
        return fail(28, "lock commands compiled into list");
    glUnlockArrays();
    if (glGetError() != GL_NO_ERROR)
        return fail(29, "final unlock");

    if (mgl_proc_address("glLockArrays") == NULL || mgl_proc_address("glUnlockArrays") == NULL ||
        mgl_proc_address("glLockArraysEXT") == NULL ||
        mgl_proc_address("glUnlockArraysEXT") == NULL)
        return fail(30, "proc addresses");

    {
        GLboolean edge_flags[3] = {GL_TRUE, GL_FALSE, GL_TRUE};
        GLboolean enabled = GL_TRUE;
        GLint value = -1;
        void *pointer = (void *)(uintptr_t)1u;
        GLuint other_vao = 0u;

        if (glIsEnabled(GL_EDGE_FLAG_ARRAY) != GL_FALSE || glGetError() != GL_NO_ERROR)
            return fail(31, "edge flag array default enable");
        glDisableClientState(GL_EDGE_FLAG_ARRAY);
        if (glGetError() != GL_NO_ERROR)
            return fail(32, "disable edge flags");
        glEdgeFlagPointer(2, edge_flags);
        glGetPointerv(GL_EDGE_FLAG_ARRAY_POINTER, &pointer);
        if (glGetError() != GL_NO_ERROR || pointer != edge_flags)
            return fail(33, "edge pointer query");
        glGetIntegerv(GL_EDGE_FLAG_ARRAY_STRIDE, &value);
        if (glGetError() != GL_NO_ERROR || value != 2)
            return fail(34, "edge pointer stride query");
        glEnableClientState(GL_EDGE_FLAG_ARRAY);
        glGetBooleanv(GL_EDGE_FLAG_ARRAY, &enabled);
        if (glGetError() != GL_NO_ERROR || !enabled || !glIsEnabled(GL_EDGE_FLAG_ARRAY))
            return fail(35, "edge enable query");
        glGetIntegerv(GL_EDGE_FLAG_ARRAY, &value);
        if (glGetError() != GL_NO_ERROR || value != 1)
            return fail(36, "edge integer enable query");

        glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
        glDisableClientState(GL_EDGE_FLAG_ARRAY);
        glEdgeFlagPointer(0, NULL);
        glPopClientAttrib();
        glGetPointerv(GL_EDGE_FLAG_ARRAY_POINTER, &pointer);
        glGetIntegerv(GL_EDGE_FLAG_ARRAY_STRIDE, &value);
        if (glGetError() != GL_NO_ERROR || !glIsEnabled(GL_EDGE_FLAG_ARRAY) ||
            pointer != edge_flags || value != 2)
            return fail(37, "edge client attribute stack restore");

        glPushClientAttribDefaultEXT(GL_CLIENT_VERTEX_ARRAY_BIT);
        glGetPointerv(GL_EDGE_FLAG_ARRAY_POINTER, &pointer);
        if (glGetError() != GL_NO_ERROR || glIsEnabled(GL_EDGE_FLAG_ARRAY) || pointer != NULL)
            return fail(38, "edge client attribute defaults");
        glPopClientAttrib();
        glGetPointerv(GL_EDGE_FLAG_ARRAY_POINTER, &pointer);
        if (glGetError() != GL_NO_ERROR || !glIsEnabled(GL_EDGE_FLAG_ARRAY) ||
            pointer != edge_flags)
            return fail(39, "edge client attribute default restore");

        glEdgeFlagPointer(-1, edge_flags);
        glGetPointerv(GL_EDGE_FLAG_ARRAY_POINTER, &pointer);
        if (glGetError() != GL_INVALID_VALUE || pointer != edge_flags)
            return fail(40, "invalid edge pointer stride should preserve state");
        if (glGetError() != GL_NO_ERROR)
            return fail(41, "edge pointer error consumed");

        glGenVertexArrays(1, &other_vao);
        if (glGetError() != GL_NO_ERROR || other_vao == 0u)
            return fail(42, "edge test alternate VAO");
        glBindVertexArray(other_vao);
        if (glGetError() != GL_NO_ERROR || glIsEnabled(GL_EDGE_FLAG_ARRAY))
            return fail(43, "edge flag state VAO isolation");
        glEnableVertexArray(other_vao, GL_EDGE_FLAG_ARRAY);
        if (glGetError() != GL_NO_ERROR || !glIsEnabled(GL_EDGE_FLAG_ARRAY))
            return fail(44, "named VAO edge enable");
        glBindVertexArray(0u);
        glGetPointerv(GL_EDGE_FLAG_ARRAY_POINTER, &pointer);
        if (glGetError() != GL_NO_ERROR || !glIsEnabled(GL_EDGE_FLAG_ARRAY) ||
            pointer != edge_flags)
            return fail(45, "default VAO retains edge pointer and enable");
        glDeleteVertexArrays(1, &other_vao);
        if (glGetError() != GL_NO_ERROR)
            return fail(46, "delete edge test VAO");
        glDisableClientState(GL_EDGE_FLAG_ARRAY);
        glGetBooleanv(GL_EDGE_FLAG_ARRAY, &enabled);
        if (glGetError() != GL_NO_ERROR || enabled)
            return fail(47, "edge state disable query");
    }

    /* Mesa's color-index array is separate from element indices and stays
     * VAO-local, even when the compatibility GLES renderer does not use it. */
    {
        GLshort indices[3] = {1, 2, 3};
        GLboolean enabled = GL_TRUE;
        GLint value = -1;
        void *pointer = (void *)(uintptr_t)1u;
        GLuint other_vao = 0u;

        if (glIsEnabled(GL_INDEX_ARRAY) || glGetError() != GL_NO_ERROR)
            return fail(48, "color-index array default disabled");
        glGetIntegerv(GL_INDEX_ARRAY_TYPE, &value);
        if (glGetError() != GL_NO_ERROR || value != (GLint)GL_FLOAT)
            return fail(49, "color-index default type");
        glDisableClientState(GL_INDEX_ARRAY);
        if (glGetError() != GL_NO_ERROR)
            return fail(50, "disable color-index array");
        glIndexPointer(GL_SHORT, sizeof(GLshort), indices);
        glGetPointerv(GL_INDEX_ARRAY_POINTER, &pointer);
        glGetIntegerv(GL_INDEX_ARRAY_TYPE, &value);
        if (glGetError() != GL_NO_ERROR || pointer != indices || value != (GLint)GL_SHORT)
            return fail(51, "color-index pointer and type");
        glGetIntegerv(GL_INDEX_ARRAY_STRIDE, &value);
        if (glGetError() != GL_NO_ERROR || value != (GLint)sizeof(GLshort))
            return fail(52, "color-index stride");

        glEnableClientState(GL_INDEX_ARRAY);
        glGetBooleanv(GL_INDEX_ARRAY, &enabled);
        glGetIntegerv(GL_INDEX_ARRAY, &value);
        if (glGetError() != GL_NO_ERROR || !enabled || value != 1 || !glIsEnabled(GL_INDEX_ARRAY))
            return fail(53, "color-index enable queries");
        glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
        glDisableClientState(GL_INDEX_ARRAY);
        glIndexPointer(GL_FLOAT, 0, NULL);
        glPopClientAttrib();
        glGetPointerv(GL_INDEX_ARRAY_POINTER, &pointer);
        if (glGetError() != GL_NO_ERROR || !glIsEnabled(GL_INDEX_ARRAY) || pointer != indices)
            return fail(54, "color-index stack restore");
        glPushClientAttribDefaultEXT(GL_CLIENT_VERTEX_ARRAY_BIT);
        glGetPointerv(GL_INDEX_ARRAY_POINTER, &pointer);
        glGetIntegerv(GL_INDEX_ARRAY_TYPE, &value);
        if (glGetError() != GL_NO_ERROR || glIsEnabled(GL_INDEX_ARRAY) || pointer != NULL ||
            value != (GLint)GL_FLOAT)
            return fail(55, "color-index client defaults");
        glPopClientAttrib();
        if (glGetError() != GL_NO_ERROR || !glIsEnabled(GL_INDEX_ARRAY))
            return fail(56, "color-index client defaults restored");

        glIndexPointer(GL_UNSIGNED_INT, 0, indices);
        if (glGetError() != GL_INVALID_ENUM)
            return fail(57, "color-index invalid pointer type");
        glIndexPointer(GL_SHORT, -1, indices);
        if (glGetError() != GL_INVALID_VALUE)
            return fail(58, "color-index negative pointer stride");
        glGetPointerv(GL_INDEX_ARRAY_POINTER, &pointer);
        if (glGetError() != GL_NO_ERROR || pointer != indices)
            return fail(59, "invalid index pointers preserve state");

        glGenVertexArrays(1, &other_vao);
        glBindVertexArray(other_vao);
        if (glGetError() != GL_NO_ERROR || glIsEnabled(GL_INDEX_ARRAY))
            return fail(60, "color-index VAO isolation");
        glEnableVertexArray(other_vao, GL_INDEX_ARRAY);
        if (glGetError() != GL_NO_ERROR || !glIsEnabled(GL_INDEX_ARRAY))
            return fail(61, "color-index named VAO state");
        glBindVertexArray(0u);
        if (glGetError() != GL_NO_ERROR || !glIsEnabled(GL_INDEX_ARRAY))
            return fail(62, "color-index default VAO retained");
        glDeleteVertexArrays(1, &other_vao);
        if (glGetError() != GL_NO_ERROR)
            return fail(63, "color-index VAO cleanup");

        /* This unit context has no GLES native for glGenBuffers; set the
         * existing array-buffer binding directly to check captured VBO state. */
        ctx.array_buffer = 73u;
        glIndexPointer(GL_FLOAT, 0, (const void *)(uintptr_t)8u);
        glGetIntegerv(GL_INDEX_ARRAY_BUFFER_BINDING, &value);
        glGetPointerv(GL_INDEX_ARRAY_POINTER, &pointer);
        if (glGetError() != GL_NO_ERROR || value != 73 || pointer != (void *)(uintptr_t)8u)
            return fail(64, "color-index VBO binding and offset");
        ctx.array_buffer = 0u;
        glDisableClientState(GL_INDEX_ARRAY);
        if (glGetError() != GL_NO_ERROR)
            return fail(65, "color-index cleanup");
    }

    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("compiled vertex array semantics passed");
    return 0;
}

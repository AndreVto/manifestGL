#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failure(int index, const char *name) {
    fprintf(stderr, "feedback %d: %s\n", index, name);
    return index;
}

int main(void) {
    struct mgl_context ctx;
    GLfloat buffer[64] = {0};
    GLfloat positions[3] = {0.0f, 0.0f, 0.0f};
    GLint query = 0;
    void *pointer = NULL;
    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 100, 100);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_selection_init(&ctx.selection);
    mgl_context_set_current_for_test(&ctx);
    if (glRenderMode(GL_FEEDBACK) != 0 || mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return failure(2, "enter before buffer");
    glFeedbackBuffer(32, GL_3D, buffer);
    glGetPointerv(GL_FEEDBACK_BUFFER_POINTER, &pointer);
    if (pointer != buffer)
        return failure(11, "pointer query");
    glGetIntegerv(GL_FEEDBACK_BUFFER_TYPE, &query);
    if (query != (GLint)GL_3D)
        return failure(3, "type query");
    glGetIntegerv(GL_FEEDBACK_BUFFER_SIZE, &query);
    if (query != 32)
        return failure(4, "size query");
    if (glRenderMode(GL_FEEDBACK) != 0)
        return failure(5, "enter feedback");
    glPassThrough(18.75f);
    glVertexPointer(3, GL_FLOAT, 0, positions);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDrawArrays(GL_POINTS, 0, 1);
    if (glRenderMode(GL_RENDER) != 6)
        return failure(6, "feedback item count");
    if (buffer[0] != (GLfloat)GL_PASS_THROUGH_TOKEN || buffer[1] != 18.75f ||
        buffer[2] != (GLfloat)GL_POINT_TOKEN || buffer[3] != 50.0f || buffer[4] != 50.0f ||
        buffer[5] != 0.5f)
        return failure(7, "feedback record");
    glFeedbackBuffer(1, GL_2D, buffer);
    glRenderMode(GL_FEEDBACK);
    glPassThrough(4.0f);
    if (glRenderMode(GL_RENDER) != -1)
        return failure(8, "overflow");
    glFeedbackBuffer(-1, GL_2D, buffer);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return failure(9, "size validation");
    glFeedbackBuffer(4, GL_RGBA, buffer);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return failure(10, "type validation");
    glFeedbackBuffer(2, GL_2D, NULL);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return failure(12, "null buffer validation");
    if (glRenderMode(GL_FEEDBACK) != 0 || mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return failure(13, "null buffer rejects feedback mode");
    glFeedbackBuffer(0, GL_2D, buffer);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return failure(25, "zero-sized buffer accepted");
    if (glRenderMode(GL_FEEDBACK) != 0 || mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return failure(26, "zero-sized buffer rejects feedback mode");
    {
        GLfloat line[6] = {-2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        glVertexPointer(3, GL_FLOAT, 0, line);
        glFeedbackBuffer(64, GL_2D, buffer);
        glRenderMode(GL_FEEDBACK);
        glDrawArrays(GL_LINES, 0, 2);
        if (glRenderMode(GL_RENDER) != 5)
            return failure(14, "clipped line length");
        if (buffer[0] != (GLfloat)GL_LINE_RESET_TOKEN || fabsf(buffer[1]) > 0.001f ||
            fabsf(buffer[2] - 50.0f) > 0.001f || fabsf(buffer[3] - 50.0f) > 0.001f ||
            fabsf(buffer[4] - 50.0f) > 0.001f)
            return failure(15, "clipped line coordinates");
    }
    {
        GLfloat tri[9] = {-2.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, -0.5f, 0.0f};
        int i;
        glVertexPointer(3, GL_FLOAT, 0, tri);
        glFeedbackBuffer(64, GL_2D, buffer);
        glRenderMode(GL_FEEDBACK);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        if (glRenderMode(GL_RENDER) != 16)
            return failure(16, "clipped polygon length");
        if (buffer[0] != (GLfloat)GL_POLYGON_TOKEN || buffer[1] != 3.0f ||
            buffer[8] != (GLfloat)GL_POLYGON_TOKEN || buffer[9] != 3.0f)
            return failure(17, "clipped polygon records");
        for (i = 0; i < 16; ++i) {
            if (i == 0 || i == 1 || i == 8 || i == 9)
                continue;
            if (buffer[i] < -0.001f || buffer[i] > 75.001f)
                return failure(18, "clipped polygon coordinates");
        }
    }
    {
        GLfloat points[6] = {-0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f};
        GLfloat colors[8] = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
        glVertexPointer(3, GL_FLOAT, 0, points);
        glColorPointer(4, GL_FLOAT, 0, colors);
        glEnableClientState(GL_COLOR_ARRAY);
        glFeedbackBuffer(64, GL_3D_COLOR, buffer);
        glRenderMode(GL_FEEDBACK);
        glDrawArrays(GL_POINTS, 0, 2);
        if (glRenderMode(GL_RENDER) != 16)
            return failure(19, "per-vertex color record length");
        if (buffer[4] != 1.0f || buffer[5] != 0.0f || buffer[6] != 0.0f || buffer[7] != 1.0f ||
            buffer[12] != 0.0f || buffer[13] != 1.0f || buffer[14] != 0.0f || buffer[15] != 1.0f)
            return failure(20, "per-vertex color values");
        glDisableClientState(GL_COLOR_ARRAY);
    }
    {
        GLfloat line[6] = {-2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        GLfloat colors[8] = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
        glVertexPointer(3, GL_FLOAT, 0, line);
        glColorPointer(4, GL_FLOAT, 0, colors);
        glEnableClientState(GL_COLOR_ARRAY);
        glFeedbackBuffer(64, GL_3D_COLOR, buffer);
        glRenderMode(GL_FEEDBACK);
        glDrawArrays(GL_LINES, 0, 2);
        if (glRenderMode(GL_RENDER) != 15)
            return failure(21, "clipped line color record length");
        if (fabsf(buffer[4] - 0.5f) > 0.001f || fabsf(buffer[5] - 0.5f) > 0.001f ||
            fabsf(buffer[6]) > 0.001f || fabsf(buffer[7] - 1.0f) > 0.001f)
            return failure(22, "clipped line color interpolation");
        glDisableClientState(GL_COLOR_ARRAY);
    }
    {
        GLfloat point[3] = {0.0f, 0.0f, 0.0f};
        GLfloat texcoord[2] = {0.25f, 0.5f};
        GLubyte color[4] = {128u, 255u, 0u, 255u};
        glVertexPointer(3, GL_FLOAT, 0, point);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, color);
        glTexCoordPointer(2, GL_FLOAT, 0, texcoord);
        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glMatrixMode(GL_TEXTURE);
        glTranslatef(0.25f, 0.0f, 0.0f);
        glMatrixMode(GL_MODELVIEW);
        glFeedbackBuffer(64, GL_3D_COLOR_TEXTURE, buffer);
        glRenderMode(GL_FEEDBACK);
        glDrawArrays(GL_POINTS, 0, 1);
        if (glRenderMode(GL_RENDER) != 12)
            return failure(23, "textured feedback record length");
        if (fabsf(buffer[4] - 128.0f / 255.0f) > 0.001f || fabsf(buffer[5] - 1.0f) > 0.001f ||
            fabsf(buffer[8] - 0.5f) > 0.001f || fabsf(buffer[9] - 0.5f) > 0.001f ||
            fabsf(buffer[11] - 1.0f) > 0.001f)
            return failure(24, "vertex attribute conversion");
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

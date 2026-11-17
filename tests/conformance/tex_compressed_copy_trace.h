#ifndef MGL_TEX_COMPRESSED_COPY_TRACE_H
#define MGL_TEX_COMPRESSED_COPY_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static void cc_error(const char *name) {
    printf("cc_%s 0x%04x\n", name, (unsigned)glGetError());
}

static void cc_level(const char *name, GLenum target) {
    GLint width = -1;
    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &width);
    printf("cc_%s %d 0x%04x\n", name, width, (unsigned)glGetError());
}

static void tex_compressed_copy_trace(void) {
    const GLubyte block[16] = {30, 60, 0, 0, 0, 0, 0, 0, 90, 100, 0, 0, 0, 0, 0, 0};
    const GLubyte rgba[8] = {10, 20, 30, 255, 40, 50, 60, 255};
    GLuint list;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 9501);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8, block);
    cc_error("compressed_2d_define");
    cc_level("compressed_2d_initial", GL_TEXTURE_2D);
    glBegin(GL_POINTS);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 8, 4, 0, 16, block);
    glEnd();
    cc_error("compressed_2d_begin_image");
    cc_level("compressed_2d_after_image", GL_TEXTURE_2D);
    glBegin(GL_POINTS);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_COMPRESSED_RED_RGTC1, 8, block);
    glEnd();
    cc_error("compressed_2d_begin_sub");
    glBegin(GL_POINTS);
    glCompressedTexImage2D(0, -1, 0, -1, -1, -1, -1, NULL);
    glEnd();
    cc_error("compressed_2d_begin_precedence");

    glBindTexture(GL_TEXTURE_2D_ARRAY, 9502);
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 2, 0, 16, block);
    cc_error("compressed_3d_define");
    cc_level("compressed_3d_initial", GL_TEXTURE_2D_ARRAY);
    glBegin(GL_POINTS);
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 8, 4, 2, 0, 32, block);
    glEnd();
    cc_error("compressed_3d_begin_image");
    cc_level("compressed_3d_after_image", GL_TEXTURE_2D_ARRAY);
    glBegin(GL_POINTS);
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 4, 4, 1, GL_COMPRESSED_RED_RGTC1, 8,
                              block);
    glEnd();
    cc_error("compressed_3d_begin_sub");
    glBegin(GL_POINTS);
    glCompressedTexSubImage3D(0, -1, -1, -1, -1, -1, -1, -1, 0, -1, NULL);
    glEnd();
    cc_error("compressed_3d_begin_precedence");

    glBindTexture(GL_TEXTURE_2D, 9503);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    cc_error("copy_2d_define");
    glBegin(GL_POINTS);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 1, 1, 0);
    glEnd();
    cc_error("copy_2d_begin_image");
    cc_level("copy_2d_after_image", GL_TEXTURE_2D);
    glBegin(GL_POINTS);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
    glEnd();
    cc_error("copy_2d_begin_sub");
    glBegin(GL_POINTS);
    glCopyTexImage2D(0, -1, 0, 0, 0, -1, -1, -1);
    glEnd();
    cc_error("copy_2d_begin_precedence");

    glBindTexture(GL_TEXTURE_3D, 9504);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    cc_error("copy_3d_define");
    glBegin(GL_POINTS);
    glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 1, 1);
    glEnd();
    cc_error("copy_3d_begin_sub");
    glBegin(GL_POINTS);
    glCopyTexSubImage3D(0, -1, -1, -1, -1, 0, 0, -1, -1);
    glEnd();
    cc_error("copy_3d_begin_precedence");

    /* A list contains all seven illegal operations, with GL_COMPILE deferring errors to replay. */
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBegin(GL_POINTS);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8, block);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_COMPRESSED_RED_RGTC1, 8, block);
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 2, 0, 16, block);
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 4, 4, 1, GL_COMPRESSED_RED_RGTC1, 8,
                              block);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 1, 1, 0);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
    glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 1, 1);
    glEnd();
    glEndList();
    cc_error("list_compile");
    glCallList(list);
    cc_error("list_replay");
    glCallList(list);
    cc_error("list_second_replay");
    glDeleteLists(list, 1);
    /* Replay must restore Begin/End state even after an error. */
    glActiveTexture(GL_TEXTURE1);
    cc_error("after_replay");
    glActiveTexture(GL_TEXTURE0);

    /* GL_COMPILE_AND_EXECUTE must preserve
     * compressed texture metadata and prevent framebuffer copy execution. */
    glBindTexture(GL_TEXTURE_2D, 9501);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE_AND_EXECUTE);
    glBegin(GL_POINTS);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 8, 4, 0, 16, block);
    glEnd();
    glEndList();
    cc_error("compressed_compile_execute");
    cc_level("compressed_after_compile_execute", GL_TEXTURE_2D);
    glCallList(list);
    cc_error("compressed_compile_execute_replay");
    cc_level("compressed_after_replay", GL_TEXTURE_2D);
    glDeleteLists(list, 1);

    glBindTexture(GL_TEXTURE_2D, 9503);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE_AND_EXECUTE);
    glBegin(GL_POINTS);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 1, 1, 0);
    glEnd();
    glEndList();
    cc_error("copy_compile_execute");
    cc_level("copy_after_compile_execute", GL_TEXTURE_2D);
    glCallList(list);
    cc_error("copy_compile_execute_replay");
    cc_level("copy_after_replay", GL_TEXTURE_2D);
    glDeleteLists(list, 1);
}
#endif

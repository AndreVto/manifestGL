#ifndef MGL_TEX_UPLOAD_TRACE_H
#define MGL_TEX_UPLOAD_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>
#include <string.h>

static void upload_error(const char *tag) {
    printf("upload_%s 0x%04x\n", tag, (unsigned)glGetError());
}

static void upload_pixel_state(const char *tag) {
    GLint unpack = -1, pack = -1;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack);
    glGetIntegerv(GL_PACK_ALIGNMENT, &pack);
    printf("upload_%s %d %d 0x%04x\n", tag, unpack, pack, (unsigned)glGetError());
}

static void upload_2d_state(const char *tag) {
    GLubyte rgba[8] = {0};
    GLint width = -1;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    printf("upload_%s %d %u %u %u %u %u %u %u %u 0x%04x\n", tag, width, (unsigned)rgba[0],
           (unsigned)rgba[1], (unsigned)rgba[2], (unsigned)rgba[3], (unsigned)rgba[4],
           (unsigned)rgba[5], (unsigned)rgba[6], (unsigned)rgba[7], (unsigned)glGetError());
}

static void upload_3d_state(const char *tag) {
    GLubyte rgba[8] = {0};
    GLint depth = -1;
    glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_DEPTH, &depth);
    glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    printf("upload_%s %d %u %u %u %u %u %u %u %u 0x%04x\n", tag, depth, (unsigned)rgba[0],
           (unsigned)rgba[1], (unsigned)rgba[2], (unsigned)rgba[3], (unsigned)rgba[4],
           (unsigned)rgba[5], (unsigned)rgba[6], (unsigned)rgba[7], (unsigned)glGetError());
}

static void tex_upload_trace(void) {
    const GLubyte initial[8] = {11, 12, 13, 14, 21, 22, 23, 24};
    const GLubyte replacement[4] = {31, 32, 33, 34};
    const GLubyte volume[8] = {41, 42, 43, 44, 51, 52, 53, 54};
    const GLubyte changed[4] = {61, 62, 63, 64};
    GLuint list;
    glActiveTexture(GL_TEXTURE0);
    upload_error("select_unit");
    upload_pixel_state("initial_pixel_state");
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    upload_pixel_state("pixel_i_valid");
    glPixelStoref(GL_UNPACK_ALIGNMENT, 4.0f);
    upload_pixel_state("pixel_f_valid");
    glBegin(GL_POINTS);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
    glPixelStoref(GL_PACK_ALIGNMENT, 2.0f);
    glEnd();
    upload_pixel_state("pixel_begin_reject");
    glBegin(GL_POINTS);
    glPixelStorei(0u, -1);
    glEnd();
    upload_pixel_state("pixel_begin_bad_pname");
    glPixelStoref(GL_PACK_ALIGNMENT, 0.0f);
    upload_error("pixel_invalid_alignment");
    upload_pixel_state("pixel_after_invalid");

    glBindTexture(GL_TEXTURE_2D, 8401u);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, initial);
    upload_2d_state("2d_initial");
    glBegin(GL_POINTS);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, replacement);
    glEnd();
    upload_2d_state("2d_begin_image");
    glBegin(GL_POINTS);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, replacement);
    glEnd();
    upload_2d_state("2d_begin_sub");
    glBegin(GL_POINTS);
    glTexImage2D(0u, -1, 0, -1, -1, -1, 0u, 0u, NULL);
    glEnd();
    upload_error("2d_begin_bad_args");
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, replacement);
    upload_2d_state("2d_valid_sub");

    glBindTexture(GL_TEXTURE_3D, 8402u);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, volume);
    upload_3d_state("3d_initial");
    glBegin(GL_POINTS);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, changed);
    glEnd();
    upload_3d_state("3d_begin_image");
    glBegin(GL_POINTS);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, changed);
    glEnd();
    upload_3d_state("3d_begin_sub");
    glBegin(GL_POINTS);
    glTexSubImage3D(0u, -1, -1, -1, -1, -1, -1, -1, 0u, 0u, NULL);
    glEnd();
    upload_error("3d_begin_bad_args");
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, changed);
    upload_3d_state("3d_valid_sub");

    glBindTexture(GL_TEXTURE_2D, 8401u);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, initial);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, changed);
    glEndList();
    upload_2d_state("2d_compile_deferred");
    glCallList(list);
    upload_2d_state("2d_compile_replay");
    glDeleteLists(list, 1);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBegin(GL_POINTS);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, replacement);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, replacement);
    glEnd();
    glEndList();
    upload_2d_state("2d_illegal_list_compile");
    glCallList(list);
    upload_2d_state("2d_illegal_list_replay");
    glDeleteLists(list, 1);

    glBindTexture(GL_TEXTURE_3D, 8402u);
    list = glGenLists(1);
    {
        GLubyte source[8] = {71, 72, 73, 74, 81, 82, 83, 84};
        glNewList(list, GL_COMPILE);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, source);
        glEndList();
        memset(source, 0, sizeof(source));
    }
    upload_3d_state("3d_compile_deferred");
    glCallList(list);
    upload_3d_state("3d_compile_owned_payload");
    glDeleteLists(list, 1);

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBegin(GL_POINTS);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, changed);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, changed);
    glEnd();
    glEndList();
    upload_3d_state("3d_illegal_list_compile");
    glCallList(list);
    upload_3d_state("3d_illegal_list_replay");
    glDeleteLists(list, 1);

    glBindTexture(GL_TEXTURE_2D, 8401u);
    list = glGenLists(1);
    glNewList(list, GL_COMPILE_AND_EXECUTE);
    glBegin(GL_POINTS);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, replacement);
    glEnd();
    glEndList();
    upload_2d_state("2d_compile_execute_illegal");
    glCallList(list);
    upload_2d_state("2d_compile_execute_replay");
    glDeleteLists(list, 1);
}
#endif

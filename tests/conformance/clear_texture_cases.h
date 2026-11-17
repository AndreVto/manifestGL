#ifndef MGL_CLEAR_TEXTURE_CASES_H
#define MGL_CLEAR_TEXTURE_CASES_H
#include "mgl/gl.h"
#include <stdio.h>

static void clear_texture_trace(void) {
    GLuint a, b, c;
    unsigned char rgba[16] = {0};
    unsigned char color[4] = {12, 44, 99, 211};
    unsigned char green[4] = {3, 200, 5, 250};
    unsigned char rgb[6] = {0};
    unsigned char shade[3] = {9, 8, 7};
    unsigned char voxels[8] = {0};
    glGenTextures(1, &a);
    glBindTexture(GL_TEXTURE_2D, a);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    printf("create2d %04x\n", glGetError());
    glClearTexImage(a, 0, GL_RGBA, GL_UNSIGNED_BYTE, color);
    printf("clear2d %04x\n", glGetError());
    glClearTexSubImage(a, 0, 1, 0, 0, 1, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, green);
    printf("sub2d %04x\n", glGetError());
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    printf("read2d %u,%u,%u,%u|%u,%u,%u,%u|%u,%u,%u,%u|%u,%u,%u,%u %04x\n", rgba[0], rgba[1],
           rgba[2], rgba[3], rgba[4], rgba[5], rgba[6], rgba[7], rgba[8], rgba[9], rgba[10],
           rgba[11], rgba[12], rgba[13], rgba[14], rgba[15], glGetError());
    glClearTexSubImage(a, 0, 2, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, green);
    printf("range %04x\n", glGetError());
    glClearTexImage(a, -1, GL_RGBA, GL_UNSIGNED_BYTE, color);
    printf("level %04x\n", glGetError());
    glClearTexImage(123456u, 0, GL_RGBA, GL_UNSIGNED_BYTE, color);
    printf("unknown %04x\n", glGetError());
    {
        const unsigned char blue[3] = {5, 6, 70};
        glClearTexImage(a, 0, GL_RGB, GL_UNSIGNED_BYTE, blue);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        printf("convert_rgb %u,%u,%u,%u %04x\n", rgba[0], rgba[1], rgba[2], rgba[3], glGetError());
    }
    {
        const GLfloat floats[4] = {1.0f, 0.5f, 0.0f, 1.0f};
        glClearTexImage(a, 0, GL_RGBA, GL_FLOAT, floats);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        printf("convert_float %u,%u,%u,%u %04x\n", rgba[0], rgba[1], rgba[2], rgba[3],
               glGetError());
        const unsigned char bgra[4] = {31, 21, 11, 255};
        glClearTexImage(a, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, bgra);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        printf("convert_bgra %u,%u,%u,%u %04x\n", rgba[0], rgba[1], rgba[2], rgba[3], glGetError());
    }
    glClearTexImage(a, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    printf("zero %u,%u,%u,%u %04x\n", rgba[0], rgba[1], rgba[2], rgba[3], glGetError());
    {
        GLuint unpack;
        unsigned char source[4] = {71, 72, 73, 74};
        glGenBuffers(1, &unpack);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 4, source, GL_STATIC_DRAW);
        glClearTexImage(a, 0, GL_RGBA, GL_UNSIGNED_BYTE, (const void *)0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        printf("unpack %u,%u,%u,%u %04x\n", rgba[0], rgba[1], rgba[2], rgba[3], glGetError());
        glDeleteBuffers(1, &unpack);
    }
    glGenTextures(1, &b);
    glBindTexture(GL_TEXTURE_1D, b);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB8, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    printf("create1d %04x\n", glGetError());
    glClearTexImage(b, 0, GL_RGB, GL_UNSIGNED_BYTE, shade);
    glGetTexImage(GL_TEXTURE_1D, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    printf("clear1d %u,%u,%u|%u,%u,%u %04x\n", rgb[0], rgb[1], rgb[2], rgb[3], rgb[4], rgb[5],
           glGetError());
    {
        GLuint cube;
        unsigned char facepixels[4] = {0};
        unsigned char shade4[4] = {20, 30, 40, 50};
        glGenTextures(1, &cube);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
        for (GLenum f = GL_TEXTURE_CUBE_MAP_POSITIVE_X; f <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; ++f)
            glTexImage2D(f, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, facepixels);
        printf("cube_image %04x\n", glGetError());
        glClearTexImage(cube, 0, GL_RGBA, GL_UNSIGNED_BYTE, shade4);
        printf("cube_clear %04x\n", glGetError());
        glGetTexImage(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, GL_UNSIGNED_BYTE, facepixels);
        printf("cube_read %u,%u,%u,%u %04x\n", facepixels[0], facepixels[1], facepixels[2],
               facepixels[3], glGetError());
        glDeleteTextures(1, &cube);
    }
    glGenTextures(1, &c);
    glBindTexture(GL_TEXTURE_3D, c);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, voxels);
    printf("create3d %04x\n", glGetError());
    glClearTexSubImage(c, 0, 0, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color);
    glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_UNSIGNED_BYTE, voxels);
    printf("sub3d %u,%u,%u,%u|%u,%u,%u,%u %04x\n", voxels[0], voxels[1], voxels[2], voxels[3],
           voxels[4], voxels[5], voxels[6], voxels[7], glGetError());
    glClearTexImage(c, 0, GL_RGBA, GL_UNSIGNED_BYTE, color);
    printf("post3d %04x\n", glGetError());
    glInvalidateTexImage(a, 0);
    printf("inv_tex %04x\n", glGetError());
    glInvalidateTexImage(54321u, 0);
    printf("inv_tex_bad %04x\n", glGetError());
    glInvalidateTexSubImage(a, 0, 0, 0, 0, 1, 1, 1);
    printf("inv_tex_sub %04x\n", glGetError());
    glInvalidateTexSubImage(a, 0, -1, 0, 0, 1, 1, 1);
    printf("inv_tex_sub_bad %04x\n", glGetError());
    {
        GLuint buf;
        unsigned char contents[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        unsigned char read[8] = {0};
        glGenBuffers(1, &buf);
        glBindBuffer(GL_ARRAY_BUFFER, buf);
        glBufferData(GL_ARRAY_BUFFER, 8, contents, GL_STATIC_DRAW);
        printf("createbuf %04x\n", glGetError());
        glInvalidateBufferData(buf);
        printf("inv_buf %04x\n", glGetError());
        glInvalidateBufferSubData(buf, 2, 3);
        printf("inv_buf_sub %04x\n", glGetError());
        glInvalidateBufferSubData(buf, 7, 3);
        printf("inv_buf_range %04x\n", glGetError());
        glInvalidateBufferData(98765u);
        printf("inv_buf_bad %04x\n", glGetError());
        glGetBufferSubData(GL_ARRAY_BUFFER, 0, 8, read);
        printf("buf %u,%u,%u,%u,%u,%u,%u,%u %04x\n", read[0], read[1], read[2], read[3], read[4],
               read[5], read[6], read[7], glGetError());
        glDeleteBuffers(1, &buf);
    }
    glDeleteTextures(1, &a);
    glDeleteTextures(1, &b);
    glDeleteTextures(1, &c);
}
#endif

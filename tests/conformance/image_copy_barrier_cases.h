#ifndef MGL_IMAGE_COPY_BARRIER_CASES_H
#define MGL_IMAGE_COPY_BARRIER_CASES_H
#include "mgl/gl.h"
#include <stdio.h>
#include <string.h>

static void image_copy_barrier_trace(void) {
    GLuint src, dst;
    unsigned char source[32], result[32];
    unsigned i;
    for (i = 0; i < 32; ++i)
        source[i] = (unsigned char)(i + 1u);
    glGenTextures(1, &src);
    glGenTextures(1, &dst);
    glBindTexture(GL_TEXTURE_2D, src);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, source);
    glBindTexture(GL_TEXTURE_2D, dst);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glCopyImageSubData(src, GL_TEXTURE_2D, 0, 0, 0, 0, dst, GL_TEXTURE_2D, 0, 0, 0, 0, 1, 1, 1);
    printf("incomplete %04x\n", (unsigned)glGetError());
    glBindTexture(GL_TEXTURE_2D, src);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, dst);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    printf("setup %04x\n", (unsigned)glGetError());
    glCopyImageSubData(src, GL_TEXTURE_2D, 0, 1, 0, 0, dst, GL_TEXTURE_2D, 0, 0, 1, 0, 2, 1, 1);
    printf("copy %04x\n", (unsigned)glGetError());
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, result);
    printf("data %02x %02x %02x %02x %02x %02x %04x\n", result[16], result[17], result[18],
           result[19], result[20], result[23], (unsigned)glGetError());
    glCopyImageSubData(src, GL_TEXTURE_2D, 0, 3, 0, 0, dst, GL_TEXTURE_2D, 0, 0, 0, 0, 2, 1, 1);
    printf("src_range %04x\n", (unsigned)glGetError());
    glCopyImageSubData(src, GL_TEXTURE_2D, 0, 0, 0, 0, dst, GL_TEXTURE_2D, 0, -1, 0, 0, 1, 1, 1);
    printf("dst_range %04x\n", (unsigned)glGetError());
    glCopyImageSubData(src, GL_TEXTURE_2D, 0, 0, 0, 0, dst, GL_TEXTURE_2D, 0, 0, 0, 0, -1, 1, 1);
    printf("negative %04x\n", (unsigned)glGetError());
    glTextureBarrier();
    printf("barrier %04x\n", (unsigned)glGetError());
    glDeleteTextures(1, &src);
    glDeleteTextures(1, &dst);
}

static void texture_view_trace(void) {
    GLuint original, view, child, bad;
    GLint levels = -1, minlevel = -1, original_wrap = -1;
    unsigned char pixels[16], output[16];
    unsigned i;
    for (i = 0; i < 16; ++i)
        pixels[i] = (unsigned char)(i + 3u);
    glGenTextures(1, &original);
    glGenTextures(1, &view);
    glGenTextures(1, &child);
    glGenTextures(1, &bad);
    glBindTexture(GL_TEXTURE_2D, original);
    glTexStorage2D(GL_TEXTURE_2D, 2, GL_RGBA8, 4, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("view_storage %04x\n", (unsigned)glGetError());
    glTextureView(view, GL_TEXTURE_2D, original, GL_RGBA8, 1, 1, 0, 1);
    printf("view_create %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(view));
    glBindTexture(GL_TEXTURE_2D, view);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT, &levels);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_VIEW_MIN_LEVEL, &minlevel);
    printf("view_parameters %d %d %04x\n", levels, minlevel, (unsigned)glGetError());
    memset(output, 0, sizeof(output));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, output);
    printf("view_pixels %u %u %u %04x\n", output[0], output[7], output[15], (unsigned)glGetError());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels + 4);
    printf("view_update %04x\n", (unsigned)glGetError());
    glBindTexture(GL_TEXTURE_2D, original);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &original_wrap);
    memset(output, 0, sizeof(output));
    glGetTexImage(GL_TEXTURE_2D, 1, GL_RGBA, GL_UNSIGNED_BYTE, output);
    printf("original_pixels %d %u %u %04x\n", original_wrap, output[0], output[3],
           (unsigned)glGetError());
    glTextureView(view, GL_TEXTURE_2D, original, GL_RGBA8, 0, 1, 0, 1);
    printf("view_recreate %04x\n", (unsigned)glGetError());
    glTextureView(bad, GL_TEXTURE_2D, original, GL_RGBA8, 2, 1, 0, 1);
    printf("view_bad_range %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(bad));
    glTextureView(child, GL_TEXTURE_2D, view, GL_RGBA8, 0, 1, 0, 1);
    printf("view_nested %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(child));
    glDeleteTextures(1, &original);
    glBindTexture(GL_TEXTURE_2D, view);
    memset(output, 0, sizeof(output));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, output);
    printf("view_survives %u %u %04x\n", output[0], output[3], (unsigned)glGetError());
    glDeleteTextures(1, &view);
    glBindTexture(GL_TEXTURE_2D, child);
    memset(output, 0, sizeof(output));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, output);
    printf("nested_survives %u %u %04x\n", output[0], output[3], (unsigned)glGetError());
    glDeleteTextures(1, &child);
    glDeleteTextures(1, &bad);
}

static void texture_layer_view_trace(void) {
    GLuint array, view, nested, cube, cube_view, cube_slice;
    GLint start = -1, count = -1;
    unsigned char source[64], update[16], pixels[64];
    unsigned i;
    for (i = 0; i < 64; ++i)
        source[i] = (unsigned char)(i + 4u);
    memset(update, 161, sizeof(update));
    glGenTextures(1, &array);
    glGenTextures(1, &view);
    glGenTextures(1, &nested);
    glBindTexture(GL_TEXTURE_2D_ARRAY, array);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 2, 2, 4);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 2, 2, 4, GL_RGBA, GL_UNSIGNED_BYTE, source);
    printf("array_storage %04x\n", (unsigned)glGetError());
    glTextureView(view, GL_TEXTURE_2D_ARRAY, array, GL_RGBA8, 0, 1, 1, 2);
    printf("array_view %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(view));
    glBindTexture(GL_TEXTURE_2D_ARRAY, view);
    glGetTexParameteriv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_VIEW_MIN_LAYER, &start);
    glGetTexParameteriv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_VIEW_NUM_LAYERS, &count);
    memset(pixels, 0, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("array_view_pixels %d %d %u %u %04x\n", start, count, pixels[0], pixels[16],
           (unsigned)glGetError());
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, update);
    printf("array_update %04x\n", (unsigned)glGetError());
    glBindTexture(GL_TEXTURE_2D_ARRAY, array);
    memset(pixels, 0, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("array_shared %u %u %u %04x\n", pixels[0], pixels[16], pixels[32],
           (unsigned)glGetError());
    glTextureView(nested, GL_TEXTURE_2D_ARRAY, view, GL_RGBA8, 0, 1, 1, 1);
    printf("array_nested %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(nested));
    glDeleteTextures(1, &array);
    glDeleteTextures(1, &view);
    glBindTexture(GL_TEXTURE_2D_ARRAY, nested);
    glGetTexParameteriv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_VIEW_MIN_LAYER, &start);
    memset(pixels, 0, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("array_nested_survives %d %u %04x\n", start, pixels[0], (unsigned)glGetError());
    glDeleteTextures(1, &nested);

    glGenTextures(1, &cube);
    glGenTextures(1, &cube_view);
    glGenTextures(1, &cube_slice);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGBA8, 2, 2);
    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                    source);
    printf("cube_storage %04x\n", (unsigned)glGetError());
    glTextureView(cube_view, GL_TEXTURE_CUBE_MAP, cube, GL_RGBA8, 0, 1, 0, 6);
    printf("cube_view %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(cube_view));
    glTextureView(cube_slice, GL_TEXTURE_2D, cube, GL_RGBA8, 0, 1, 0, 1);
    printf("cube_to_2d %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(cube_slice));
    glDeleteTextures(1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube_view);
    glGetTexParameteriv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_VIEW_NUM_LAYERS, &count);
    memset(pixels, 0, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("cube_pixels %d %u %u %04x\n", count, pixels[0], pixels[15], (unsigned)glGetError());
    glDeleteTextures(1, &cube_view);
    glBindTexture(GL_TEXTURE_2D, cube_slice);
    memset(pixels, 0, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("cube_to_2d_survives %u %u %04x\n", pixels[0], pixels[15], (unsigned)glGetError());
    glDeleteTextures(1, &cube_slice);
}

static void texture_cross_target_view_trace(void) {
    GLuint array, slice, nested, cube, face_view;
    unsigned char data[96], update[16], pixels[96];
    GLint minlayer = -1, layers = -1;
    unsigned i;
    for (i = 0; i < sizeof(data); ++i)
        data[i] = (unsigned char)(i + 1u);
    memset(update, 199, sizeof(update));
    glGenTextures(1, &array);
    glGenTextures(1, &slice);
    glGenTextures(1, &nested);
    glGenTextures(1, &cube);
    glGenTextures(1, &face_view);
    glBindTexture(GL_TEXTURE_2D_ARRAY, array);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 2, 2, 6);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 2, 2, 6, GL_RGBA, GL_UNSIGNED_BYTE, data);
    printf("cross_storage %04x\n", (unsigned)glGetError());
    glTextureView(slice, GL_TEXTURE_2D, array, GL_RGBA8, 0, 1, 2, 1);
    printf("cross_2d %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(slice));
    glBindTexture(GL_TEXTURE_2D, slice);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_VIEW_MIN_LAYER, &minlayer);
    memset(pixels, 0, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("cross_2d_data %d %u %u %04x\n", minlayer, pixels[0], pixels[15],
           (unsigned)glGetError());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, update);
    printf("cross_2d_write %04x\n", (unsigned)glGetError());
    glTextureView(nested, GL_TEXTURE_2D, slice, GL_RGBA8, 0, 1, 0, 1);
    printf("cross_2d_nested %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(nested));
    glTextureView(cube, GL_TEXTURE_CUBE_MAP, array, GL_RGBA8, 0, 1, 0, 6);
    printf("cross_cube %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(cube));
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    glGetTexParameteriv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_VIEW_NUM_LAYERS, &layers);
    memset(pixels, 0, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("cross_cube_face %d %u %u %04x\n", layers, pixels[0], pixels[15],
           (unsigned)glGetError());
    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    update);
    printf("cross_cube_write %04x\n", (unsigned)glGetError());
    glBindTexture(GL_TEXTURE_2D_ARRAY, array);
    memset(pixels, 0, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("cross_cube_shared %u %u %u %04x\n", pixels[64], pixels[67], pixels[80],
           (unsigned)glGetError());
    glTextureView(face_view, GL_TEXTURE_2D, cube, GL_RGBA8, 0, 1, 4, 1);
    printf("cross_cube_to_2d %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(face_view));
    glBindTexture(GL_TEXTURE_2D, face_view);
    memset(pixels, 0, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("cross_cube_to_2d_data %u %u %04x\n", pixels[0], pixels[15], (unsigned)glGetError());
    glDeleteTextures(1, &array);
    glDeleteTextures(1, &slice);
    glBindTexture(GL_TEXTURE_2D, nested);
    memset(pixels, 0, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("cross_nested_survives %u %u %04x\n", pixels[0], pixels[15], (unsigned)glGetError());
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    memset(pixels, 0, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("cross_cube_survives %u %u %04x\n", pixels[0], pixels[15], (unsigned)glGetError());
    glDeleteTextures(1, &nested);
    glDeleteTextures(1, &cube);
    glBindTexture(GL_TEXTURE_2D, face_view);
    memset(pixels, 0, sizeof(pixels));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("cross_face_survives %u %u %04x\n", pixels[0], pixels[15], (unsigned)glGetError());
    glDeleteTextures(1, &face_view);
}

static void texture_format_view_trace(void) {
    GLuint source, view, nested, second, reverse, rejected;
    GLint internal = -1;
    unsigned char pixels[16], update[4] = {221u, 81u, 42u, 255u}, readback[16];
    unsigned i;
    for (i = 0; i < sizeof(pixels); ++i)
        pixels[i] = (unsigned char)(i + 10u);
    glGenTextures(1, &source);
    glGenTextures(1, &view);
    glGenTextures(1, &nested);
    glGenTextures(1, &second);
    glGenTextures(1, &reverse);
    glGenTextures(1, &rejected);
    glBindTexture(GL_TEXTURE_2D, source);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("format_storage %04x\n", (unsigned)glGetError());
    glTextureView(view, GL_TEXTURE_2D, source, GL_SRGB8_ALPHA8, 0, 1, 0, 1);
    printf("format_view %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(view));
    glBindTexture(GL_TEXTURE_2D, view);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal);
    memset(readback, 0, sizeof(readback));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, readback);
    printf("format_view_pixels %x %u %u %u %04x\n", (unsigned)internal, readback[0], readback[3],
           readback[15], (unsigned)glGetError());
    glTextureView(nested, GL_TEXTURE_2D, view, GL_RGBA8, 0, 1, 0, 1);
    printf("format_nested %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(nested));
    glBindTexture(GL_TEXTURE_2D, nested);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, update);
    printf("format_write %04x\n", (unsigned)glGetError());
    glBindTexture(GL_TEXTURE_2D, source);
    memset(readback, 0, sizeof(readback));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, readback);
    printf("format_shared %u %u %u %04x\n", readback[0], readback[3], readback[4],
           (unsigned)glGetError());
    glTextureView(rejected, GL_TEXTURE_2D, source, GL_RGB8, 0, 1, 0, 1);
    printf("format_incompatible %04x %u\n", (unsigned)glGetError(),
           (unsigned)glIsTexture(rejected));
    glBindTexture(GL_TEXTURE_2D, second);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, 2, 2);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf("format_reverse_storage %04x\n", (unsigned)glGetError());
    glTextureView(reverse, GL_TEXTURE_2D, second, GL_RGBA8, 0, 1, 0, 1);
    printf("format_reverse %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(reverse));
    glBindTexture(GL_TEXTURE_2D, reverse);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal);
    memset(readback, 0, sizeof(readback));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, readback);
    printf("format_reverse_pixels %x %u %u %04x\n", (unsigned)internal, readback[0], readback[15],
           (unsigned)glGetError());
    glDeleteTextures(1, &source);
    glDeleteTextures(1, &view);
    glDeleteTextures(1, &second);
    glBindTexture(GL_TEXTURE_2D, nested);
    memset(readback, 0, sizeof(readback));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, readback);
    printf("format_nested_survives %u %u %04x\n", readback[0], readback[3], (unsigned)glGetError());
    glDeleteTextures(1, &nested);
    glDeleteTextures(1, &reverse);
    glDeleteTextures(1, &rejected);
    {
        GLuint rgb, srgb, bad;
        unsigned char rgb_pixels[12], rgb_output[12];
        GLint rgb_internal = -1;
        for (i = 0; i < sizeof(rgb_pixels); ++i)
            rgb_pixels[i] = (unsigned char)(i + 30u);
        glGenTextures(1, &rgb);
        glGenTextures(1, &srgb);
        glGenTextures(1, &bad);
        glBindTexture(GL_TEXTURE_2D, rgb);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, 2, 2);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGB, GL_UNSIGNED_BYTE, rgb_pixels);
        printf("format_rgb_storage %04x\n", (unsigned)glGetError());
        glTextureView(srgb, GL_TEXTURE_2D, rgb, GL_SRGB8, 0, 1, 0, 1);
        printf("format_rgb_view %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(srgb));
        glBindTexture(GL_TEXTURE_2D, srgb);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &rgb_internal);
        memset(rgb_output, 0, sizeof(rgb_output));
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_output);
        printf("format_rgb_pixels %x %u %u %04x\n", (unsigned)rgb_internal, rgb_output[0],
               rgb_output[11], (unsigned)glGetError());
        glTextureView(bad, GL_TEXTURE_2D, rgb, GL_RGBA8, 0, 1, 0, 1);
        printf("format_rgb_invalid %04x %u\n", (unsigned)glGetError(), (unsigned)glIsTexture(bad));
        glDeleteTextures(1, &rgb);
        glDeleteTextures(1, &srgb);
        glDeleteTextures(1, &bad);
    }
}

#endif

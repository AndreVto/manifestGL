#include "shader/translator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const legacy_love_vertex =
    "#version 120\n"
    "#define VERTEX VERTEX\n"
    "#define Image sampler2D\n"
    "#define sample_image(s, uv) \\\n"
    " Texel(s, uv)\n"
    "#if __VERSION__ >= 130 && !defined(LOVE_GLSL1_ON_GLSL3)\n"
    " #define Texel texture\n"
    "#else\n"
    " #define Texel(s, uv) texture2D(s, uv)\n"
    "#endif\n"
    "#if defined(GL_ES)\n"
    " #error Desktop GLSL must be preprocessed before GLES lowering\n"
    "#endif\n"
    "uniform Image diffuse;\n"
    "vec4 effect(Image texture, vec2 uv){return sample_image(texture,uv);}\n"
    "void main(){gl_Position=effect(diffuse,vec2(0.2));}\n";

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    char *buffer;
    long length;
    if (file == NULL)
        return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    length = ftell(file);
    if (length < 0 || length > 1024L * 1024L || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    buffer = malloc((size_t)length + 1u);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }
    if (fread(buffer, 1u, (size_t)length, file) != (size_t)length) {
        free(buffer);
        fclose(file);
        return NULL;
    }
    buffer[length] = '\0';
    fclose(file);
    return buffer;
}

int main(int argc, char **argv) {
    char *input = NULL;
    char *out;
    char *log = NULL;
    int is_fixture = argc == 1;
    if (!is_fixture) {
        input = read_file(argv[1]);
        if (input == NULL)
            return 2;
    }
    out = mgl_translate_glsl(GL_VERTEX_SHADER, is_fixture ? legacy_love_vertex : input, &log);
    free(input);
    if (out == NULL) {
        fprintf(stderr, "GLSL translation failed: %s\n", log != NULL ? log : "unknown");
        free(log);
        return 3;
    }
    free(log);
    if (is_fixture &&
        (strstr(out, "#define Texel") != NULL || strstr(out, "#define sample_image") != NULL ||
         strstr(out, "#if __VERSION__") != NULL || strstr(out, "Texel(texture") != NULL ||
         strstr(out, "texture(texture") != NULL ||
         strstr(out, "mgl_legacy_texture2D(texture, uv)") == NULL)) {
        fprintf(stderr, "Original-version macro preprocessing regression:\n%s\n", out);
        free(out);
        return 4;
    }
    if (argc > 2) {
        FILE *file = fopen(argv[2], "wb");
        if (file == NULL) {
            free(out);
            return 5;
        }
        if (fputs(out, file) == EOF) {
            fclose(file);
            free(out);
            return 5;
        }
        if (fclose(file) != 0) {
            free(out);
            return 5;
        }
    }
    free(out);
    return 0;
}

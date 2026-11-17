#include "compiler/spirv_toolchain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int require_contains(const char *text, const char *needle, const char *what) {
    if (text == NULL || strstr(text, needle) == NULL) {
        fprintf(stderr, "%s: missing '%s'\n", what, needle);
        if (text != NULL)
            fprintf(stderr, "output:\n%s\n", text);
        return 0;
    }
    return 1;
}

static int require_not_contains(const char *text, const char *needle, const char *what) {
    if (text != NULL && strstr(text, needle) != NULL) {
        fprintf(stderr, "%s: unexpectedly contains '%s'\noutput:\n%s\n", what, needle, text);
        return 0;
    }
    return 1;
}

int main(void) {
    static const char vertex_source[] =
        "#version 330 core\n"
        "layout(location=0) in vec3 pos;\n"
        "uniform mat4 transform;\n"
        "void main(){ gl_Position = transform * vec4(pos, 1.0); }\n";
    static const char fragment_source[] = "#version 330 core\n"
                                          "out vec4 color;\n"
                                          "void main(){ color = vec4(0.25, 0.5, 0.75, 1.0); }\n";
    static const char dual_source_fragment[] =
        "#version 330 core\n"
        "layout(location=0, index=1) out vec4 secondary;\n"
        "void main(){ secondary = vec4(0.1, 0.2, 0.3, 0.4); }\n";
    static const char buffer_sampler_fragment[] = "#version 330 core\n"
                                                  "uniform samplerBuffer values;\n"
                                                  "out vec4 color;\n"
                                                  "void main(){ color = texelFetch(values, 0); }\n";
    static const char compute_source[] =
        "#version 430 core\n"
        "layout(local_size_x=4, local_size_y=2, local_size_z=1) in;\n"
        "layout(std430, binding=0) buffer Values { uint values[]; };\n"
        "void main(){ values[gl_GlobalInvocationID.x] += 1u; }\n";
    static const char geometry_vertex_source[] =
        "#version 150 core\n"
        "in vec3 position; out vec4 vertexColor;\n"
        "void main(){ gl_Position=vec4(position,1.0); vertexColor=vec4(0.5); }\n";
    static const char geometry_fragment_source[] = "#version 150 core\n"
                                                   "in vec4 Color; out vec4 pixel;\n"
                                                   "void main(){pixel=Color;}\n";
    static const char geometry_source[] =
        "#version 150 core\n"
        "layout(triangles) in; layout(triangle_strip, max_vertices=3) out;\n"
        "in vec4 vertexColor[]; out vec4 Color;\n"
        "void main() { for(int i=0;i<3;++i) {\n"
        "gl_Position=gl_in[i].gl_Position; Color=vertexColor[i]; EmitVertex();\n"
        "} EndPrimitive(); }\n";
    char *out = NULL;
    char *log = NULL;

    if (!mgl_spirv_translate_glsl(GL_VERTEX_SHADER, geometry_vertex_source, 320u, &out, &log)) {
        fprintf(stderr, "geometry input vertex failed: %s\n", log != NULL ? log : "no log");
        free(log);
        return 1;
    }
    if (!require_contains(out, "#version 320 es", "geometry input vertex version") ||
        !require_contains(out, "vertexColor", "geometry input vertex varying") ||
        !mgl_validate_gles_glsl(GL_VERTEX_SHADER, out)) {
        free(out);
        free(log);
        return 1;
    }
    free(out);
    free(log);
    out = log = NULL;

    if (!mgl_spirv_translate_glsl(GL_GEOMETRY_SHADER, geometry_source, 320u, &out, &log)) {
        fprintf(stderr, "geometry toolchain failed: %s\n", log != NULL ? log : "no log");
        free(log);
        return 1;
    }
    if (!require_contains(out, "#version 320 es", "geometry version") ||
        !require_contains(out, "EmitVertex", "geometry emit") ||
        !require_contains(out, "Color", "geometry varying") ||
        !mgl_validate_gles_glsl(GL_GEOMETRY_SHADER, out)) {
        fprintf(stderr, "geometry output invalid:\n%s\n", out != NULL ? out : "(null)");
        free(out);
        free(log);
        return 1;
    }
    free(out);
    free(log);
    out = log = NULL;

    if (!mgl_spirv_translate_glsl(GL_FRAGMENT_SHADER, geometry_fragment_source, 320u, &out, &log)) {
        fprintf(stderr, "geometry output fragment failed: %s\n", log != NULL ? log : "no log");
        free(log);
        return 1;
    }
    if (!require_contains(out, "#version 320 es", "geometry output fragment version") ||
        !require_contains(out, "Color", "geometry output fragment varying") ||
        !mgl_validate_gles_glsl(GL_FRAGMENT_SHADER, out)) {
        free(out);
        free(log);
        return 1;
    }
    free(out);
    free(log);
    out = log = NULL;

    if (!mgl_spirv_translate_glsl(GL_VERTEX_SHADER, vertex_source, 300u, &out, &log)) {
        fprintf(stderr, "vertex toolchain failed: %s\n", log != NULL ? log : "no log");
        free(log);
        return 1;
    }
    if (!require_contains(out, "#version 300 es", "vertex") ||
        !require_contains(out, "gl_Position", "vertex")) {
        free(out);
        free(log);
        return 1;
    }
    free(out);
    free(log);
    out = log = NULL;

    if (!mgl_spirv_translate_glsl(GL_FRAGMENT_SHADER, fragment_source, 300u, &out, &log)) {
        fprintf(stderr, "fragment toolchain failed: %s\n", log != NULL ? log : "no log");
        free(log);
        return 1;
    }
    if (!require_contains(out, "#version 300 es", "fragment") ||
        !require_not_contains(out, "GL_EXT_blend_func_extended", "ordinary fragment")) {
        free(out);
        free(log);
        return 1;
    }
    free(out);
    free(log);
    out = log = NULL;

    {
        static const char *const direct_versions[] = {
            "#version 130\nout vec4 pixel; uniform float opacity; "
            "void main(){pixel=vec4(opacity);if(pixel.a<=0)discard;}\n",
            "#version 140\nout vec4 pixel; uniform float opacity; "
            "void main(){pixel=vec4(opacity);if(pixel.a<=0)discard;}\n",
            "#version 150 core\n#define LIMIT 2\n#if __VERSION__ == 150\n"
            "out vec4 pixel;\n#else\nthis must not compile\n#endif\n"
            "uniform float opacity;uniform int flags;"
            "void main(){pixel=vec4(opacity);if(pixel.a<=LIMIT)discard;"
            "if((flags&1)==0)discard;}\n"};
        size_t i;
        for (i = 0u; i < sizeof(direct_versions) / sizeof(direct_versions[0]); ++i) {
            if (!mgl_spirv_translate_glsl(GL_FRAGMENT_SHADER, direct_versions[i], 300u, &out,
                                          &log)) {
                fprintf(stderr, "direct desktop GLSL test %zu failed: %s\n", i,
                        log != NULL ? log : "no log");
                free(log);
                return 1;
            }
            if (!require_contains(out, "#version 300 es", "direct desktop version") ||
                !require_contains(out, "<=", "floating-point comparison") ||
                !mgl_validate_gles_glsl(GL_FRAGMENT_SHADER, out)) {
                fprintf(stderr, "direct desktop GLSL test %zu invalid:\n%s\n", i, out);
                free(out);
                free(log);
                return 1;
            }
            if (i == 2u && (!require_contains(out, "2.0", "float conversion") ||
                            !require_contains(out, "& 1", "integer bitmask"))) {
                free(out);
                free(log);
                return 1;
            }
            free(out);
            free(log);
            out = log = NULL;
        }
    }

    if (!mgl_spirv_translate_glsl(GL_FRAGMENT_SHADER, dual_source_fragment, 300u, &out, &log)) {
        fprintf(stderr, "dual-source fragment toolchain failed: %s\n",
                log != NULL ? log : "no log");
        free(log);
        return 1;
    }
    if (!require_contains(out, "#extension GL_EXT_blend_func_extended : require",
                          "dual-source fragment") ||
        !require_contains(out, "index = 1", "dual-source fragment")) {
        free(out);
        free(log);
        return 1;
    }
    free(out);
    free(log);
    out = log = NULL;

    if (!mgl_spirv_translate_glsl(GL_FRAGMENT_SHADER, buffer_sampler_fragment, 300u, &out, &log)) {
        fprintf(stderr, "buffer-sampler fragment toolchain failed: %s\n",
                log != NULL ? log : "no log");
        free(log);
        return 1;
    }
    if (!require_contains(out, "#extension GL_EXT_texture_buffer : require",
                          "buffer-sampler fragment") ||
        !require_contains(out, "samplerBuffer", "buffer-sampler fragment") ||
        !require_contains(out, "texelFetch", "buffer-sampler fragment")) {
        free(out);
        free(log);
        return 1;
    }
    free(out);
    free(log);
    out = log = NULL;

    if (!mgl_spirv_translate_glsl(GL_COMPUTE_SHADER, compute_source, 310u, &out, &log)) {
        fprintf(stderr, "compute toolchain failed: %s\n", log != NULL ? log : "no log");
        free(log);
        return 1;
    }
    if (!require_contains(out, "#version 310 es", "compute") ||
        !require_contains(out, "local_size_x", "compute") ||
        !require_contains(out, "gl_GlobalInvocationID", "compute")) {
        free(out);
        free(log);
        return 1;
    }
    free(out);
    free(log);
    return 0;
}

#include "shader/translator.h"
#include "compiler/spirv_toolchain.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* glslang's original-version preprocessing canonicalizes source whitespace.
 * Keep the unit assertions about identifier/GLSL semantics, not formatting. */
static char *compact_space(const char *src) {
    const size_t n = strlen(src);
    char *dst = malloc(n + 1u);
    size_t i, j = 0;
    if (dst == NULL)
        return NULL;
    for (i = 0; i < n; ++i)
        if (!isspace((unsigned char)src[i]))
            dst[j++] = src[i];
    dst[j] = '\0';
    return dst;
}

static int validate_target_glsl(GLenum stage, const char *source) {
    return mgl_validate_gles_glsl(stage, source);
}

static int require_contains(const char *text, const char *needle, int code) {
    int found = text != NULL && strstr(text, needle) != NULL;
    if (!found && text != NULL) {
        char *a = compact_space(text);
        char *b = compact_space(needle);
        if (a != NULL && b != NULL && strstr(a, b) != NULL)
            found = 1;
        free(a);
        free(b);
    }
    if (!found) {
        fprintf(stderr, "translation test %d missing: %s\n%s\n", code, needle,
                text != NULL ? text : "(null)");
        return code;
    }
    return 0;
}

static int require_not_contains(const char *text, const char *needle, int code) {
    if (text != NULL && strstr(text, needle) != NULL) {
        fprintf(stderr, "translation test %d unexpectedly contains: %s\n%s\n", code, needle, text);
        return code;
    }
    return 0;
}

static int require_before(const char *text, const char *earlier, const char *later, int code) {
    const char *a = text != NULL ? strstr(text, earlier) : NULL;
    const char *b = text != NULL ? strstr(text, later) : NULL;
    if (a == NULL || b == NULL || a >= b) {
        fprintf(stderr, "translation order test %d failed: %s before %s\n%s\n", code, earlier,
                later, text != NULL ? text : "(null)");
        return code;
    }
    return 0;
}

int main(void) {
    const char *legacy_vs = "#version 120\n"
                            "varying vec3 v;\n"
                            "void main(){ gl_Position=gl_Vertex; v=vec3(gl_Normal.z, "
                            "gl_MultiTexCoord0.x, gl_SecondaryColor.r); }\n";
    const char *legacy_fs = "#version 120\n"
                            "varying float v; void main(){ gl_FragColor=vec4(v,0,0,1); }\n";
    const char *modern_vs =
        "#version 330 core\n"
        "layout(location=0) in vec2 p; void main(){ gl_Position=vec4(p,0,1); }\n";
    const char *modern_fs = "#version 330 core\n"
                            "layout(location=0) out vec4 color; void main(){ color=vec4(1.0); }\n";
    const char *dual_source_fs =
        "#version 330 core\n"
        "layout(location=0, index=1) out vec4 secondary; void main(){ secondary=vec4(1.0); }\n";
    const char *buffer_sampler_fs =
        "#version 330 core\n"
        "uniform samplerBuffer values; layout(location=0) out vec4 color; "
        "void main(){ color=texelFetch(values, 0); }\n";
    const char *buffer_sampler_dual_fs =
        "#version 330 core\n"
        "uniform isamplerBuffer values; layout(location=0, index=1) out vec4 secondary; "
        "void main(){ secondary=vec4(texelFetch(values, 0)); }\n";
    const char *ftransform_vs = "#version 120\nvoid main(){ gl_Position=ftransform(); }\n";
    char *log = NULL;
    char *out;
    int rc;

    {
        const char *desktop =
            "#version 150\n"
            "uniform sampler2D s_texture_0;\n"
            "in vec4 s_texcoord;\n"
            "in vec4 s_color;\n"
            "out vec4 s_frag_data_0;\n"
            "out vec4 s_frag_data_1;\n"
            "void main() {\n"
            "s_frag_data_0 = texture(s_texture_0,s_texcoord.xy) * s_color;\n"
            "s_frag_data_1 = s_frag_data_0;\n"
            "}\n";
        char *located;
        out = mgl_translate_glsl(GL_FRAGMENT_SHADER, desktop, &log);
        located = out != NULL ? mgl_fragment_output_locations(out, 0u, NULL) : NULL;
        if (located == NULL || !validate_target_glsl(GL_FRAGMENT_SHADER, located) ||
            (rc = require_contains(located, "layout(location = 0) out vec4 s_frag_data_0;", 470)) != 0 ||
            (rc = require_contains(located, "layout(location = 1) out vec4 s_frag_data_1;", 471)) != 0) {
            fprintf(stderr, "dual-output translation failed:\n%s\n%s\n",
                    located != NULL ? located : "(null)", log != NULL ? log : "");
            free(located);
            free(out);
            free(log);
            return rc != 0 ? rc : 472;
        }
        free(located);
        free(out);
        free(log);
        log = NULL;
    }

    {
        const char *source =
            "#version 320 es\n"
            "precision highp float;\n"
            "layout(location = 3) out vec4 fixedColor;\n"
            "out highp vec4\n"
            "  freeColor;\n"
            "void main() { fixedColor = vec4(1.0); freeColor = vec4(0.5); }\n";
        struct mgl_fragment_output_location binding = {"freeColor", 2u, 0u};
        out = mgl_fragment_output_locations(source, 1u, &binding);
        if (out == NULL || !validate_target_glsl(GL_FRAGMENT_SHADER, out) ||
            (rc = require_contains(out, "layout(location = 3)", 473)) != 0 ||
            (rc = require_contains(out, "layout(location = 2)", 474)) != 0) {
            free(out);
            return rc != 0 ? rc : 475;
        }
        free(out);
        out = mgl_fragment_output_locations(source, 0u, NULL);
        if (out == NULL || !validate_target_glsl(GL_FRAGMENT_SHADER, out) ||
            (rc = require_contains(out, "layout(location = 3)", 476)) != 0 ||
            (rc = require_contains(out, "layout(location = 0)", 477)) != 0) {
            free(out);
            return rc != 0 ? rc : 478;
        }
        free(out);
    }


    {
        const char *source =
            "#version 320 es\nprecision highp float;\n"
            "void main() { gl_FragDepth = 0.5; }\n";
        struct mgl_fragment_output_location unused = {"absentOutput", 2u, 0u};
        out = mgl_fragment_output_locations(source, 1u, &unused);
        if (out == NULL || strcmp(out, source) != 0 ||
            !validate_target_glsl(GL_FRAGMENT_SHADER, out)) {
            free(out);
            return 479;
        }
        free(out);
    }

    {
        const char *source =
            "#version 320 es\nprecision highp float;\n"
            "out vec4 unusedOutput;\n"
            "out vec4 primaryOutput;\n"
            "void main() { primaryOutput = vec4(1.0); }\n";
        const struct mgl_fragment_output_location bindings[] = {
            {"primaryOutput", 0u, 0u}, {"unusedOutput", 0u, 0u}
        };
        out = mgl_fragment_output_locations(source, 2u, bindings);
        if (out == NULL || !validate_target_glsl(GL_FRAGMENT_SHADER, out)) {
            free(out);
            return 480;
        }
        if ((rc = require_contains(out, "layout(location = 0) out vec4 primaryOutput;", 481)) != 0 ||
            (rc = require_contains(out, "layout(location = 1) out vec4 unusedOutput;", 482)) != 0) {
            free(out);
            return rc;
        }
        free(out);
    }

    {
        const char *source =
            "#version 320 es\nprecision highp float;\n"
            "out vec4 primaryOutput;\n"
            "out vec4 secondaryOutput;\n"
            "void main() { primaryOutput = vec4(1.0); secondaryOutput = vec4(0.5); }\n";
        const struct mgl_fragment_output_location bindings[] = {
            {"primaryOutput", 0u, 0u}, {"secondaryOutput", 0u, 0u}
        };
        out = mgl_fragment_output_locations(source, 2u, bindings);
        if (out != NULL && validate_target_glsl(GL_FRAGMENT_SHADER, out)) {
            free(out);
            return 483;
        }
        free(out);
    }

    {
        static const char *const inputs[] = {
            "// header\n/* block */\n#version 120\nvoid main(){gl_Position=gl_Vertex;}\n",
            "/* header\n block */\n// line\n#version 120\nvoid main(){gl_FragColor=vec4(1.0);}\n"};
        static const GLenum stages[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
        size_t i;
        for (i = 0; i < 2u; ++i) {
            out = mgl_translate_glsl(stages[i], inputs[i], &log);
            if (out == NULL || strstr(out, "#version 320 es\n") != out ||
                strstr(out, "#version 120") != NULL || !validate_target_glsl(stages[i], out)) {
                fprintf(stderr, "comment-prefixed version translation %zu failed: %s\n", i,
                        out != NULL   ? out
                        : log != NULL ? log
                                      : "(null)");
                free(out);
                free(log);
                return 466;
            }
            free(out);
            free(log);
            log = NULL;
        }
    }

    {
        static const struct {
            GLenum stage;
            const char *source;
            const char *varying;
        } chain[] = {{GL_VERTEX_SHADER,
                      "#version 150 core\nin vec3 position; out vec4 vertexColor; "
                      "void main(){gl_Position=vec4(position,1.0);vertexColor=vec4(1.0);}",
                      "vertexColor"},
                     {GL_GEOMETRY_SHADER,
                      "#version 150 core\nlayout(triangles) in; "
                      "layout(triangle_strip,max_vertices=3) out; "
                      "in vec4 vertexColor[]; out vec4 Color; "
                      "void main(){for(int i=0;i<3;++i){gl_Position=gl_in[i].gl_Position; "
                      "Color=vertexColor[i];EmitVertex();}EndPrimitive();}",
                      "Color"},
                     {GL_FRAGMENT_SHADER,
                      "#version 150 core\nin vec4 Color; out vec4 pixel; "
                      "void main(){pixel=Color;}",
                      "Color"}};

        size_t i;
        for (i = 0u; i < sizeof(chain) / sizeof(chain[0]); ++i) {
            out = mgl_translate_glsl(chain[i].stage, chain[i].source, &log);
            if (out == NULL) {
                fprintf(stderr, "linked-stage translation %zu failed: %s\n", i,
                        log ? log : "no log");
                free(log);
                return 450;
            }
            if ((rc = require_contains(out, "#version 320 es", 451)) != 0 ||
                (rc = require_contains(out, chain[i].varying, 452)) != 0 ||
                !validate_target_glsl(chain[i].stage, out)) {
                fprintf(stderr, "linked-stage translation %zu invalid:\n%s\n", i, out);
                free(out);
                free(log);
                return rc != 0 ? rc : 453;
            }
            free(out);
            free(log);
            log = NULL;
        }
    }

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 150 core\n"
                             "out vec4 FragColor; void main() { int d = 1; "
                             "if ((d & 1) == 0) discard; FragColor = vec4(1.0); "
                             "if (FragColor.a <= 0) discard; }\n",
                             &log);
    if (out == NULL)
        return 410;
    if ((rc = require_contains(out, "<= 0.0", 411)) != 0)
        return rc;
    if ((rc = require_contains(out, "(d & 1) == 0", 412)) != 0)
        return rc;
    if (!validate_target_glsl(GL_FRAGMENT_SHADER, out)) {
        fprintf(stderr, "GLSL 150 typed comparison did not validate as ESSL:\n%s\n", out);
        return 413;
    }
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 150 core\n"
                             "#define LIMIT 2\n"
                             "#if __VERSION__ == 150\n"
                             "out vec4 pixel;\n"
                             "#else\n"
                             "this would not compile\n"
                             "#endif\n"
                             "uniform float opacity;\n"
                             "void main(){float x=opacity; if(x >= LIMIT) discard; "
                             "int i=4; if(i>=2) discard; pixel=vec4(1.0); }\n",
                             &log);
    if (out == NULL) {
        fprintf(stderr, "typed GLSL 150 translation failed: %s\n", log ? log : "(null)");
        return 414;
    }
    if ((rc = require_contains(out, ">= 2.0", 415)) != 0)
        return rc;
    if ((rc = require_contains(out, ">= 2", 416)) != 0)
        return rc;
    if (!validate_target_glsl(GL_FRAGMENT_SHADER, out))
        return 417;
    free(out);
    free(log);
    log = NULL;

    /* GLSL ES 3.00 implements texture arrays directly. Legacy extension directives are removed
     * after desktop GLSL preprocessing. */
    out =
        mgl_translate_glsl(GL_FRAGMENT_SHADER,
                           "#version 120\n"
                           "#extension GL_EXT_texture_array : enable\n"
                           "uniform sampler2DArray pages;\n"
                           "void main(){ gl_FragColor=texture2DArray(pages,vec3(0.2,0.3,0.0)); }\n",
                           &log);
    if (out == NULL) {
        fprintf(stderr, "texture array extension translation failed: %s\n",
                log != NULL ? log : "(null)");
        return 310;
    }
    if ((rc = require_not_contains(out, "#extension GL_EXT_texture_array", 311)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_legacy_texture2DArray", 312)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(
        GL_VERTEX_SHADER,
        "#version 120\n"
        "uniform sampler2D diffuse;\n"
        "vec4 sample_image(sampler2D texture, vec2 uv) { return texture2D(texture, uv); }\n"
        "void main() { gl_Position=sample_image(diffuse,vec2(0.25)); }\n",
        &log);
    if ((rc = require_contains(out, "vec4 mgl_legacy_texture2D(sampler2D samp, vec2 uv)", 300)) !=
        0)
        return rc;
    if ((rc = require_contains(out, "mgl_legacy_texture2D(texture, uv)", 301)) != 0)
        return rc;
    if ((rc = require_not_contains(out, "return texture(texture, uv)", 302)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n"
                             "#define Texel texture2D\n"
                             "layout(location=0) out vec4 love_PixelColor;\n"
                             "uniform sampler2D diffuse;\n"
                             "vec4 effect(sampler2D texture, vec2 uv){return Texel(texture,uv);}\n"
                             "void main(){love_PixelColor=effect(diffuse,vec2(0.2));}\n",
                             &log);
    /* The original desktop macros must have expanded before GLES lowering. */
    if ((rc = require_not_contains(out, "#define Texel", 303)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_legacy_texture2D(texture, uv)", 305)) != 0)
        return rc;
    if ((rc = require_contains(out, "vec4 mgl_legacy_texture2D(sampler2D samp, vec2 uv)", 304)) !=
        0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(
        GL_VERTEX_SHADER,
        "#version 110\n"
        "void main(){ gl_FrontColor=gl_Color; gl_Position=gl_TextureMatrix[2]*gl_Vertex; }\n",
        &log);
    if ((rc = require_contains(out, "uniform mat4 mgl_u_texture_matrix[8];", 401)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_u_texture_matrix[2]", 402)) != 0)
        return rc;
    if ((rc = require_contains(out, "out vec4 mgl_v_front_color;", 403)) != 0)
        return rc;
    if ((rc = require_contains(out, "out vec4 mgl_v_back_color;", 407)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_v_back_color = mgl_v_front_color;", 408)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 110\n"
                             "void main(){ gl_FragColor=vec4(gl_Color.rgb,1.0); }\n",
                             &log);
    if ((rc = require_contains(out, "in vec4 mgl_v_front_color;", 404)) != 0)
        return rc;
    if ((rc = require_contains(out, "in vec4 mgl_v_back_color;", 409)) != 0)
        return rc;
    if ((rc = require_contains(out, "(gl_FrontFacing ? mgl_v_front_color : mgl_v_back_color).rgb",
                               405)) != 0)
        return rc;
    if ((rc = require_not_contains(out, "mgl_a_color", 406)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_VERTEX_SHADER,
                             "#version 110\n"
                             "void main(){ gl_Position=gl_Vertex; gl_FrontColor=vec4(1.,0.,0.,1.); "
                             "gl_BackColor=vec4(0.,1.,0.,1.); }\n",
                             &log);
    if ((rc = require_contains(out, "out vec4 mgl_v_front_color;", 410)) != 0)
        return rc;
    if ((rc = require_contains(out, "out vec4 mgl_v_back_color;", 411)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_v_back_color=vec4(0.", 412)) != 0)
        return rc;
    if ((rc = require_not_contains(out, "mgl_v_back_color = mgl_v_front_color;", 413)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    /* Explicit gl_FrontColor bypasses the facing selector, gl_Color does not. */
    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 110\n"
                             "void main(){ gl_FragColor=gl_FrontColor+gl_BackColor+gl_Color; }\n",
                             &log);
    if ((rc = require_contains(out, "mgl_v_front_color+mgl_v_back_color+", 414)) != 0)
        return rc;
    if ((rc = require_contains(out, "(gl_FrontFacing ? mgl_v_front_color : mgl_v_back_color)",
                               415)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    /* Modern shaders must not inherit compatibility varyings or selectors. */
    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 330 core\nlayout(location=0) out vec4 c; "
                             "void main(){ c=vec4(gl_FrontFacing ? 1. : 0.); }\n",
                             &log);
    if ((rc = require_not_contains(out, "mgl_v_back_color", 416)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_VERTEX_SHADER, legacy_vs, &log);
    if ((rc = require_contains(out, "#version 320 es", 1)) != 0)
        return rc;
    if ((rc = require_contains(out, "in vec4 mgl_a_position;", 2)) != 0)
        return rc;
    if ((rc = require_contains(out, "in vec3 mgl_a_normal;", 3)) != 0)
        return rc;
    if ((rc = require_contains(out, "in vec4 mgl_a_texcoord0;", 4)) != 0)
        return rc;
    if ((rc = require_contains(out, "in vec4 mgl_a_secondary;", 5)) != 0)
        return rc;
    if ((rc = require_contains(out, "gl_Position=mgl_a_position", 6)) != 0)
        return rc;
    if ((rc = require_contains(out, "uniform float mgl_u_point_size;", 30)) != 0)
        return rc;
    if ((rc = require_contains(out, "gl_PointSize=mgl_u_point_size", 31)) != 0)
        return rc;
    if (strstr(out, "gl_MultiTexCoord0") != NULL)
        return 7;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER, legacy_fs, &log);
    if ((rc = require_contains(out, "precision highp float;", 8)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_frag_color", 9)) != 0)
        return rc;
    if ((rc = require_contains(out, "uniform int mgl_u_alpha_test_enabled;", 14)) != 0)
        return rc;
    if ((rc = require_contains(out, "#define main mgl_user_main", 15)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_alpha_test_pass(mgl_frag_color.a)", 16)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_VERTEX_SHADER, modern_vs, &log);
    if ((rc = require_contains(out, "#version 320 es", 10)) != 0)
        return rc;
    if ((rc = require_contains(out, "layout(location", 11)) != 0)
        return rc;
    if ((rc = require_contains(out, "in vec2 p;", 17)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_u_program_point_size==0", 32)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_VERTEX_SHADER, ftransform_vs, &log);
    if ((rc = require_contains(out, "uniform mat4 mgl_u_model_view_projection;", 12)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_u_model_view_projection * mgl_a_position", 13)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n#extension GL_ARB_texture_rectangle : enable\n"
                             "uniform sampler2DRect frame; varying vec2 texcoord; "
                             "void main(){ gl_FragColor=texture2DRect(frame,texcoord); }\n",
                             &log);
    if (out == NULL || strstr(out, "vec4 mgl_legacy_texture2DRect(sampler2D samp") == NULL ||
        strstr(out, "textureSize(samp, 0)") == NULL)
        return 333;
    free(out);
    free(log);
    log = NULL;
    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n#extension GL_ARB_texture_rectangle : enable\n"
                             "uniform sampler2DRect diffuseMap; uniform sampler1D convMap;\n"
                             "void main(){ vec3 color=texture2DRect(diffuseMap,vec2(10.0)).rgb; "
                             "gl_FragColor=vec4(texture1D(convMap,color.r).x); }\n",
                             &log);
    if (out == NULL || strstr(out, "vec4 mgl_legacy_texture1D(sampler2D samp") == NULL ||
        strstr(out, "uniform sampler2D convMap") == NULL ||
        strstr(out, "#extension GL_ARB_texture_rectangle") != NULL ||
        !validate_target_glsl(GL_FRAGMENT_SHADER, out))
        return 334;
    free(out);
    free(log);
    log = NULL;
    out = mgl_translate_glsl(GL_FRAGMENT_SHADER, modern_fs, &log);
    if ((rc = require_not_contains(out, "GL_EXT_blend_func_extended", 18)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER, dual_source_fs, &log);
    if ((rc = require_contains(out, "#extension GL_EXT_blend_func_extended : require", 19)) != 0)
        return rc;
    if ((rc = require_contains(out, "index=1", 20)) != 0 &&
        (rc = require_contains(out, "index = 1", 20)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER, buffer_sampler_fs, &log);
    if ((rc = require_not_contains(out, "#extension GL_EXT_texture_buffer : require", 21)) != 0)
        return rc;
    if ((rc = require_contains(out, "samplerBuffer", 22)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER, buffer_sampler_dual_fs, &log);
    if ((rc = require_not_contains(out, "#extension GL_EXT_texture_buffer : require", 23)) != 0)
        return rc;
    if ((rc = require_contains(out, "#extension GL_EXT_blend_func_extended : require", 24)) != 0)
        return rc;
    free(out);
    free(log);

    out = mgl_translate_glsl(
        GL_FRAGMENT_SHADER,
        "#version 120\nvoid main(){ gl_FragColor=vec4(gl_PointCoord,0.0,1.0); }\n", &log);
    if ((rc = require_contains(out, "uniform int mgl_u_point_coord_flip;", 25)) != 0)
        return rc;
    if ((rc = require_contains(out, "vec4(mgl_point_coord(),0.0,1.0)", 26)) != 0)
        return rc;
    if ((rc = require_contains(out, "vec2 c=gl_PointCoord;", 27)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 330 core\nlayout(location=0) out vec4 color; void main(){ "
                             "color=vec4(gl_PointCoord,0.0,1.0); }\n",
                             &log);
    if ((rc = require_contains(out, "uniform int mgl_u_point_coord_flip;", 28)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_point_coord()", 29)) != 0)
        return rc;
    free(out);
    free(log);

    out = mgl_translate_glsl(GL_VERTEX_SHADER,
                             "#version 120\n"
                             "// before extension\n"
                             "#extension GL_OES_standard_derivatives : enable\n"
                             "#define USE_3D 1\n"
                             "attribute vec4 vert; varying vec3 v; uniform sampler3D volume;\n"
                             "void main(){v=texture3D(volume,vec3(0.2)).rgb; gl_Position=vert;}\n",
                             &log);
    if ((rc = require_contains(out, "#version 320 es", 33)) != 0)
        return rc;
    if ((rc = require_contains(out, "uniform sampler3D volume", 34)) != 0)
        return rc;
    if ((rc = require_contains(out, "precision highp sampler3D;", 45)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_legacy_texture3D(volume", 35)) != 0)
        return rc;
    if ((rc = require_contains(out, "in vec4 vert; out vec3 v;", 36)) != 0)
        return rc;
    if ((rc = require_before(out, "#extension GL_OES_standard_derivatives", "in vec4 vert", 37)) !=
        0)
        return rc;
    if ((rc = require_not_contains(out, "#define USE_3D", 38)) != 0)
        return rc;
    if ((rc = require_not_contains(out, "#version 100", 39)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n"
                             "#extension GL_OES_standard_derivatives : enable\n"
                             "varying vec3 v; uniform sampler3D volume;\n"
                             "void main(){gl_FragColor=texture3D(volume,v);}\n",
                             &log);
    if ((rc = require_before(out, "#extension GL_OES_standard_derivatives", "precision highp float",
                             40)) != 0)
        return rc;
    if ((rc = require_contains(out, "in vec3 v", 41)) != 0)
        return rc;
    if ((rc = require_contains(out, "out vec4 mgl_frag_color", 42)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_frag_color=mgl_legacy_texture3D(volume,v)", 43)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 330 core\n"
                             "#extension GL_OES_standard_derivatives : enable\n"
                             "layout(location=0) out vec4 color; void main(){color=vec4(1.0);}\n",
                             &log);
    if ((rc = require_contains(out, "#version 320 es", 44)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n"
                             "#extension GL_OES_standard_derivatives : enable\n"
                             "layout(location=0) out vec4 love_PixelColor;\n"
                             "varying vec2 texcoord; uniform sampler2D tex;\n"
                             "void main(){ love_PixelColor = texture2D(tex,texcoord); }\n",
                             &log);
    if ((rc = require_not_contains(out, "out vec4 mgl_frag_color", 46)) != 0)
        return rc;
    if ((rc = require_contains(out, "out vec4 love_PixelColor", 47)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_alpha_test_pass(love_PixelColor.a)", 48)) != 0)
        return rc;
    if ((rc = require_before(out, "#extension GL_OES_standard_derivatives", "precision highp float",
                             49)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n"
                             "out highp vec4 love_PixelColor;\n"
                             "void main(){ love_PixelColor=vec4(1.0); }\n",
                             &log);
    if ((rc = require_not_contains(out, "out vec4 mgl_frag_color", 50)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_alpha_test_pass(love_PixelColor.a)", 51)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n"
                             "layout(location=1) out vec4 bloom;\n"
                             "layout(location=0) out vec4 love_PixelColor;\n"
                             "void main(){bloom=vec4(0.0); love_PixelColor=vec4(1.0);}\n",
                             &log);
    if ((rc = require_not_contains(out, "out vec4 mgl_frag_color", 52)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_alpha_test_pass(love_PixelColor.a)", 53)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n"
                             "// out vec4 bogus;\n"
                             "void helper(out vec4 val){val=vec4(0.5);}\n"
                             "void main(){gl_FragColor=vec4(1.0);}\n",
                             &log);
    if ((rc = require_contains(out, "layout(location=0) out vec4 mgl_frag_color", 54)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_alpha_test_pass(mgl_frag_color.a)", 55)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n"
                             "out vec4 love_PixelColor;\n"
                             "void main(){ gl_FragColor=vec4(0.8); }\n",
                             &log);
    if ((rc = require_contains(out, "#define mgl_frag_color love_PixelColor", 56)) != 0)
        return rc;
    if ((rc = require_not_contains(out, "out vec4 mgl_frag_color", 57)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n"
                             "void main(){gl_FragData[0]=vec4(1.0);gl_FragData[1]=vec4(0.0);}\n",
                             &log);
    if ((rc = require_contains(out, "layout(location=0) out vec4 mgl_frag_data[4]", 58)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_frag_data[1]", 59)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_alpha_test_pass(mgl_frag_data[0].a)", 60)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n"
                             "void main(){gl_FragDepth=0.5;}\n",
                             &log);
    if ((rc = require_not_contains(out, "layout(location=0) out vec4 mgl_frag_color", 61)) != 0)
        return rc;
    if ((rc = require_not_contains(out, "#define main mgl_user_main", 62)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;
    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n"
                             "#define PIXEL_COLOR gl_FragColor\n"
                             "void main(){PIXEL_COLOR=vec4(1.0);}\n",
                             &log);
    if ((rc = require_contains(out, "mgl_frag_color = vec4(1.0)", 63)) != 0)
        return rc;
    if ((rc = require_contains(out, "layout(location=0) out vec4 mgl_frag_color", 64)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n"
                             "layout(location=0) out vec4 color[2];\n"
                             "void main(){color[0]=vec4(1.0);color[1]=vec4(0.0);}\n",
                             &log);
    if ((rc = require_contains(out, "mgl_alpha_test_pass(color[0].a)", 65)) != 0)
        return rc;
    if ((rc = require_not_contains(out, "out vec4 mgl_frag_color", 66)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n"
                             "// gl_FragColor and out vec4 fakeOutput should not count\n"
                             "void main(){gl_FragDepth=0.5;}\n",
                             &log);
    if ((rc = require_not_contains(out, "layout(location=0) out vec4 mgl_frag_color", 67)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\nlayout(location=0) out vec4 love_PixelColor;\n"
                             "void main(){gl_FragData[0]=vec4(1.0); }\n",
                             &log);
    if ((rc = require_not_contains(out, "out vec4 mgl_frag_data[4]", 68)) != 0)
        return rc;
    if ((rc = require_not_contains(out, "out vec4 mgl_frag_data_0", 69)) != 0)
        return rc;
    if ((rc = require_contains(out, "love_PixelColor=vec4(1.0)", 70)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_alpha_test_pass(love_PixelColor.a)", 71)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n#define OUTPIX gl_FragData[0]\n"
                             "layout(location=0) out vec4 love_PixelColor;\n"
                             "void main(){OUTPIX=vec4(1.0); }\n",
                             &log);
    if ((rc = require_contains(out, "love_PixelColor = vec4(1.0)", 72)) != 0)
        return rc;
    if ((rc = require_not_contains(out, "out vec4 mgl_frag_data[4]", 73)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\nlayout(location=0) out vec4 love_PixelColor;\n"
                             "layout(location=1) out vec4 love_Bloom;\n"
                             "void main(){gl_FragData[0]=vec4(1.0); gl_FragData[1]=vec4(0.0);}\n",
                             &log);
    if ((rc = require_contains(out, "love_PixelColor=vec4(1.0)", 74)) != 0)
        return rc;
    if ((rc = require_contains(out, "love_Bloom=vec4(0.0)", 75)) != 0)
        return rc;
    if ((rc = require_not_contains(out, "out vec4 mgl_frag_data[4]", 76)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\nlayout(location=0) out vec4 love_PixelColor;\n"
                             "void main(){gl_FragData[0]=vec4(1.0); gl_FragData[1]=vec4(0.0);}\n",
                             &log);
    if ((rc = require_contains(out, "layout(location=1) out vec4 mgl_frag_data_1", 77)) != 0)
        return rc;
    if ((rc = require_not_contains(out, "out vec4 mgl_frag_data[4]", 78)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\nlayout(location=0) out vec4 love_Colors[2];\n"
                             "void main(){gl_FragData[0]=vec4(1.0); gl_FragData[1]=vec4(0.0); "
                             "gl_FragData[2]=vec4(0.5);}\n",
                             &log);
    if ((rc = require_contains(out, "love_Colors[0]=vec4(1.0)", 79)) != 0)
        return rc;
    if ((rc = require_contains(out, "love_Colors[1]=vec4(0.0)", 80)) != 0)
        return rc;
    if ((rc = require_contains(out, "layout(location=2) out vec4 mgl_frag_data_2", 81)) != 0)
        return rc;
    if ((rc = require_not_contains(out, "layout(location=1) out vec4 mgl_frag_data_1", 82)) != 0)
        return rc;
    free(out);
    free(log);
    log = NULL;

    /* Preserve explicit vertex texture LOD sampling during GLSL 1.20 conversion. */
    out = mgl_translate_glsl(
        GL_VERTEX_SHADER,
        "#version 120\n"
        "#define USE_GLES_OVER_GL\n"
        "uniform sampler2D tex;\n"
        "vec4 texel2DFetch(sampler2D texture, ivec2 size, ivec2 coord) {\n"
        "  vec2 uv = (vec2(coord) + vec2(0.5)) / vec2(size);\n"
        "  return texture2DLod(texture, uv, 0.0);\n"
        "}\n"
        "void main() { gl_Position = texel2DFetch(tex, ivec2(16), ivec2(0)); }\n",
        &log);
    if ((rc = require_contains(out, "mgl_legacy_texture2DLod(sampler2D samp, vec2 uv, float lod)",
                               310)) != 0)
        return rc;
    if ((rc = require_contains(out, "return textureLod(samp, uv, lod)", 311)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_legacy_texture2DLod(texture, uv, 0.0)", 312)) != 0)
        return rc;
    if ((rc = require_not_contains(out, "return texture2DLod(texture", 313)) != 0)
        return rc;
    if (!validate_target_glsl(GL_VERTEX_SHADER, out)) {
        fprintf(stderr, "Legacy vertex texture sample failed GLSL ES compiler validation:\n%s\n",
                out);
        return 314;
    }
    free(out);
    free(log);
    log = NULL;

    /* Desktop GLSL shadow samplers return vec4, while GLES shadow lookups return float. */
    out = mgl_translate_glsl(GL_FRAGMENT_SHADER,
                             "#version 120\n"
                             "uniform sampler2DShadow shadowMap;\n"
                             "float lookup(vec4 p) { return shadow2DProj(shadowMap, p).x; }\n"
                             "void main() { float s = shadow2D(shadowMap, vec3(0.5)).x; "
                             "gl_FragColor=vec4(lookup(vec4(0.5)) + s); }\n",
                             &log);
    if ((rc = require_contains(out, "vec4 mgl_legacy_shadow2DProj(sampler2DShadow samp, vec4 uv)",
                               335)) != 0)
        return rc;
    if ((rc = require_contains(out, "vec4(vec3(texture(samp, uv)), 1.0)", 453)) != 0)
        return rc;
    if ((rc = require_contains(out, "vec4(vec3(textureProj(samp, uv)), 1.0)", 454)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_legacy_shadow2DProj(shadowMap, p).x", 337)) != 0)
        return rc;
    if ((rc = require_contains(out, "mgl_legacy_shadow2D(shadowMap, vec3(0.5)).x", 338)) != 0)
        return rc;
    if (!validate_target_glsl(GL_FRAGMENT_SHADER, out))
        return 339;
    free(out);
    free(log);
    log = NULL;

    /* Scene-lighting source also relies on desktop GLSL scalar promotions. */
    out =
        mgl_translate_glsl(GL_FRAGMENT_SHADER,
                           "#version 120\n"
                           "uniform sampler2DShadow shadowMap;\n"
                           "void main() { float s = 0; s += shadow2DProj(shadowMap, vec4(0.5)).x; "
                           "s /= 4; gl_FragColor = vec4(s); }\n",
                           &log);
    if (out == NULL)
        return 340;
    if (!validate_target_glsl(GL_FRAGMENT_SHADER, out)) {
        fprintf(stderr,
                "Legacy shadow sampling and scalar conversion failed GLES validation:\n%s\n", out);
        return 341;
    }
    free(out);
    free(log);
    log = NULL;

    {
        static const char *const sources[] = {
            "#version 400 core\n"
            "layout(vertices=3) out;\n"
            "void main(){ gl_out[gl_InvocationID].gl_Position = "
            "gl_in[gl_InvocationID].gl_Position; "
            "gl_TessLevelOuter[0]=1.0; gl_TessLevelOuter[1]=1.0; gl_TessLevelOuter[2]=1.0; "
            "gl_TessLevelInner[0]=1.0; }\n",
            "#version 400 core\n"
            "layout(triangles, equal_spacing, cw) in;\n"
            "void main(){ gl_Position = gl_TessCoord.x * gl_in[0].gl_Position + "
            "gl_TessCoord.y * gl_in[1].gl_Position + "
            "gl_TessCoord.z * gl_in[2].gl_Position; }\n"};
        static const GLenum stages[] = {GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER};
        for (unsigned i = 0; i < 2u; ++i) {
            out = mgl_translate_glsl(stages[i], sources[i], &log);
            if (out == NULL || strstr(out, "#version 320 es") != out ||
                !validate_target_glsl(stages[i], out)) {
                fprintf(stderr, "tessellation translation %u failed: %s\n", i,
                        out != NULL   ? out
                        : log != NULL ? log
                                      : "(null)");
                free(out);
                free(log);
                return 487 + (int)i;
            }
            free(out);
            free(log);
            log = NULL;
        }
    }

    puts("mGL shader translation unit test: PASS");
    return 0;
}

#include "mgl/gl.h"

#include "compat/fixed_function.h"
#include "core/context.h"
#include "shader/shader_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Single-stage desktop programs receive a temporary implicit counterpart during GLES linking.
 * Desktop attachment queries and relinks observe only application-attached shaders. */
static int replace_fixed_shader_text(char *source, size_t capacity, const char *before,
                                     const char *after) {
    char *match = strstr(source, before);
    size_t head, before_length, after_length, tail_length;
    if (match == NULL)
        return 0;
    head = (size_t)(match - source);
    before_length = strlen(before);
    after_length = strlen(after);
    tail_length = strlen(match + before_length);
    if (head + after_length + tail_length + 1u > capacity)
        return 0;
    memmove(match + after_length, match + before_length, tail_length + 1u);
    memcpy(match, after, after_length);
    return 1;
}

GLuint mgl_compile_implicit_compat_stage(struct mgl_context *ctx, GLenum type,
                                         const struct mgl_program *program, const GLenum *targets) {
    static const char fixed_vertex_0[] =
        "#version 320 es\n"
        "precision highp float;\n"
        "in vec4 mgl_a_position;\n"
        "in vec4 mgl_a_normal;\n"
        "in vec4 mgl_a_color;\n"
        "in vec4 mgl_a_secondary;\n"
        "in float mgl_a_fog;\n"
        "in vec4 mgl_a_texcoord0;\n"
        "in vec4 mgl_a_texcoord1;\n"
        "in vec4 mgl_a_texcoord2;\n"
        "in vec4 mgl_a_texcoord3;\n"
        "in vec4 mgl_a_texcoord4;\n"
        "in vec4 mgl_a_texcoord5;\n"
        "in vec4 mgl_a_texcoord6;\n"
        "in vec4 mgl_a_texcoord7;\n"
        "uniform mat4 mgl_u_model_view_projection;\n"
        "uniform mat4 mgl_u_model_view;\n"
        "uniform mat3 mgl_u_normal_matrix;\n"
        "uniform mat4 mgl_u_texture_matrix[8];\n"
        "uniform float mgl_u_point_size;\n"
        "uniform int mgl_u_fog_coord_explicit;\n"
        "uniform int mgl_u_lighting_enabled;\n"
        "uniform int mgl_u_normalize_enabled;\n"
        "uniform int mgl_u_light_enabled[8];\n"
        "uniform vec4 mgl_u_light_ambient[8];\n"
        "uniform vec4 mgl_u_light_diffuse[8];\n"
        "uniform vec4 mgl_u_light_specular[8];\n"
        "uniform vec4 mgl_u_light_position[8];\n"
        "uniform vec3 mgl_u_light_spot_direction[8];\n"
        "uniform float mgl_u_light_spot_exponent[8];\n"
        "uniform float mgl_u_light_spot_cutoff[8];\n"
        "uniform float mgl_u_light_constant_attenuation[8];\n"
        "uniform float mgl_u_light_linear_attenuation[8];\n"
        "uniform float mgl_u_light_quadratic_attenuation[8];\n"
        "uniform vec4 mgl_u_light_model_ambient;\n"
        "uniform int mgl_u_light_model_local_viewer;\n"
        "uniform int mgl_u_light_model_two_side;\n"
        "uniform vec4 mgl_u_material_ambient;\n"
        "uniform vec4 mgl_u_material_diffuse;\n"
        "uniform vec4 mgl_u_material_specular;\n"
        "uniform vec4 mgl_u_material_emission;\n"
        "uniform float mgl_u_material_shininess;\n"
        "uniform vec4 mgl_u_material_back_ambient;\n"
        "uniform vec4 mgl_u_material_back_diffuse;\n"
        "uniform vec4 mgl_u_material_back_specular;\n"
        "uniform vec4 mgl_u_material_back_emission;\n"
        "uniform float mgl_u_material_back_shininess;\n"
        "uniform int mgl_u_color_material_enabled;\n"
        "uniform int mgl_u_color_material_mode;\n"
        "uniform int mgl_u_color_material_face;\n"
        "uniform int mgl_u_implicit_texgen_enabled[32];\n"
        "uniform int mgl_u_implicit_texgen_mode[32];\n"
        "uniform vec4 mgl_u_implicit_texgen_object_plane[32];\n"
        "uniform vec4 mgl_u_implicit_texgen_eye_plane[32];\n"
        "out vec4 mgl_v_eye_position;\n"
        "out vec4 mgl_v_front_color;\n"
        "out vec4 mgl_v_back_color;\n"
        "out vec4 mgl_v_front_secondary;\n"
        "out vec4 mgl_v_back_secondary;\n"
        "out vec4 mgl_v_texcoord[8];\n"
        "out float mgl_v_fog_coord;\n"
        "vec4 mgl_lit(vec4 eye, vec3 normal, bool back) {\n"
        "    vec3 n = mgl_u_normal_matrix * normal;\n"
        "    if (mgl_u_normalize_enabled != 0) n = normalize(n);\n"
        "    vec4 ma = back ? mgl_u_material_back_ambient : mgl_u_material_ambient;\n"
        "    vec4 md = back ? mgl_u_material_back_diffuse : mgl_u_material_diffuse;\n"
        "    vec4 ms = back ? mgl_u_material_back_specular : mgl_u_material_specular;\n"
        "    vec4 me = back ? mgl_u_material_back_emission : mgl_u_material_emission;\n"
        "    float shininess = back ? mgl_u_material_back_shininess : mgl_u_material_shininess;\n"
        "    if (mgl_u_color_material_enabled != 0 &&\n"
        "        (mgl_u_color_material_face == 1032 || (back ? mgl_u_color_material_face == 1029 : "
        "mgl_u_color_material_face == 1028))) {\n"
        "        int mode = mgl_u_color_material_mode;\n"
        "        if (mode == 4608 || mode == 5634) ma = mgl_a_color;\n"
        "        if (mode == 4609 || mode == 5634) md = mgl_a_color;\n";
    static const char fixed_vertex_1[] =
        "        if (mode == 4610) ms = mgl_a_color;\n"
        "        if (mode == 5632) me = mgl_a_color;\n"
        "    }\n"
        "    vec3 view = mgl_u_light_model_local_viewer != 0 ? normalize(-eye.xyz) : "
        "vec3(0.0,0.0,1.0);\n"
        "    vec4 result = me + mgl_u_light_model_ambient * ma;\n"
        "    for (int i=0; i<8; ++i) {\n"
        "        if (mgl_u_light_enabled[i] == 0) continue;\n"
        "        vec3 light_dir;\n"
        "        float attenuation = 1.0;\n"
        "        if (mgl_u_light_position[i].w == 0.0) {\n"
        "            light_dir = normalize(mgl_u_light_position[i].xyz);\n"
        "        } else {\n"
        "            vec3 d = mgl_u_light_position[i].xyz - eye.xyz;\n"
        "            float distance_to_light = length(d);\n"
        "            light_dir = distance_to_light > 0.0 ? d/distance_to_light : "
        "vec3(0.0,0.0,1.0);\n"
        "            attenuation = 1.0 / max(mgl_u_light_constant_attenuation[i] +\n"
        "                mgl_u_light_linear_attenuation[i]*distance_to_light +\n"
        "                mgl_u_light_quadratic_attenuation[i]*distance_to_light*distance_to_light, "
        "0.000001);\n"
        "            if (mgl_u_light_spot_cutoff[i] != 180.0) {\n"
        "                float sd = dot(normalize(mgl_u_light_spot_direction[i]), -light_dir);\n"
        "                float cutoff = cos(radians(mgl_u_light_spot_cutoff[i]));\n"
        "                if (sd < cutoff) attenuation = 0.0;\n"
        "                else attenuation *= pow(max(sd,0.0),mgl_u_light_spot_exponent[i]);\n"
        "            }\n"
        "        }\n"
        "        float ndotl = max(dot(n,light_dir),0.0);\n"
        "        float spec = 0.0;\n"
        "        if (ndotl > 0.0) {\n"
        "            float ndoth = max(dot(n,normalize(light_dir + view)),0.0);\n"
        "            spec = shininess == 0.0 ? 1.0 : pow(ndoth,shininess);\n"
        "        }\n"
        "        result += attenuation * (mgl_u_light_ambient[i]*ma +\n"
        "            ndotl*mgl_u_light_diffuse[i]*md + spec*mgl_u_light_specular[i]*ms);\n"
        "    }\n"
        "    result.a = md.a;\n"
        "    return clamp(result,0.0,1.0);\n"
        "}\n"
        "vec4 mgl_texgen_unit(int unit, vec4 obj, vec4 eye, vec3 normal, vec4 tex) {\n"
        "    vec4 tc = tex;\n"
        "    vec3 n = normalize(mgl_u_normal_matrix * normal);\n"
        "    vec3 u = normalize(eye.xyz);\n"
        "    vec3 reflection = reflect(u,n);\n"
        "    float m = 2.0 * sqrt(max(reflection.x*reflection.x + reflection.y*reflection.y +\n"
        "        (reflection.z+1.0)*(reflection.z+1.0),0.000001));\n"
        "    float sphere_s = reflection.x/m + 0.5;\n"
        "    float sphere_t = reflection.y/m + 0.5;\n";
    static const char fixed_vertex_2[] =
        "    for (int i=0; i<4; ++i) {\n"
        "        int index = unit*4 + i;\n"
        "        if (mgl_u_implicit_texgen_enabled[index] == 0) continue;\n"
        "        int mode = mgl_u_implicit_texgen_mode[index];\n"
        "        float value = 0.0;\n"
        "        if (mode == 9217) value = dot(obj,mgl_u_implicit_texgen_object_plane[index]);\n"
        "        else if (mode == 9216) value = dot(eye,mgl_u_implicit_texgen_eye_plane[index]);\n"
        "        else if (mode == 9218) value = i == 0 ? sphere_s : sphere_t;\n"
        "        else if (mode == 34065) value = i == 0 ? n.x : (i == 1 ? n.y : n.z);\n"
        "        else if (mode == 34066) value = i == 0 ? reflection.x : (i == 1 ? reflection.y : "
        "reflection.z);\n"
        "        if (i == 0) tc.x = value;\n"
        "        else if (i == 1) tc.y = value;\n"
        "        else if (i == 2) tc.z = value;\n"
        "        else tc.w = value;\n"
        "    }\n"
        "    return tc;\n"
        "}\n"
        "void main() {\n"
        "    vec4 eye = mgl_u_model_view * mgl_a_position;\n"
        "    mgl_v_eye_position = eye;\n"
        "    gl_Position = mgl_u_model_view_projection * mgl_a_position;\n"
        "    gl_PointSize = mgl_u_point_size;\n"
        "    vec4 primary = mgl_u_lighting_enabled != 0 ? mgl_lit(eye,mgl_a_normal.xyz,false) : "
        "mgl_a_color;\n"
        "    mgl_v_front_color = primary;\n"
        "    mgl_v_back_color = (mgl_u_lighting_enabled != 0 && mgl_u_light_model_two_side != 0)\n"
        "        ? mgl_lit(eye,-mgl_a_normal.xyz,true) : primary;\n"
        "    mgl_v_front_secondary = mgl_a_secondary;\n"
        "    mgl_v_back_secondary = mgl_a_secondary;\n"
        "    mgl_v_fog_coord = mgl_u_fog_coord_explicit != 0 ? mgl_a_fog : abs(eye.z);\n"
        "    mgl_v_texcoord[0] = mgl_u_texture_matrix[0] * "
        "mgl_texgen_unit(0,mgl_a_position,eye,mgl_a_normal.xyz,mgl_a_texcoord0);\n"
        "    mgl_v_texcoord[1] = mgl_u_texture_matrix[1] * "
        "mgl_texgen_unit(1,mgl_a_position,eye,mgl_a_normal.xyz,mgl_a_texcoord1);\n"
        "    mgl_v_texcoord[2] = mgl_u_texture_matrix[2] * "
        "mgl_texgen_unit(2,mgl_a_position,eye,mgl_a_normal.xyz,mgl_a_texcoord2);\n"
        "    mgl_v_texcoord[3] = mgl_u_texture_matrix[3] * "
        "mgl_texgen_unit(3,mgl_a_position,eye,mgl_a_normal.xyz,mgl_a_texcoord3);\n"
        "    mgl_v_texcoord[4] = mgl_u_texture_matrix[4] * "
        "mgl_texgen_unit(4,mgl_a_position,eye,mgl_a_normal.xyz,mgl_a_texcoord4);\n"
        "    mgl_v_texcoord[5] = mgl_u_texture_matrix[5] * "
        "mgl_texgen_unit(5,mgl_a_position,eye,mgl_a_normal.xyz,mgl_a_texcoord5);\n"
        "    mgl_v_texcoord[6] = mgl_u_texture_matrix[6] * "
        "mgl_texgen_unit(6,mgl_a_position,eye,mgl_a_normal.xyz,mgl_a_texcoord6);\n"
        "    mgl_v_texcoord[7] = mgl_u_texture_matrix[7] * "
        "mgl_texgen_unit(7,mgl_a_position,eye,mgl_a_normal.xyz,mgl_a_texcoord7);\n"
        "}\n";
    char fixed_fragment[16000];
    const char *source = NULL;
    const char *original = program->attached[0]->translated_source;
    GLuint shader;
    GLint compiled = GL_FALSE;
    if (type == GL_FRAGMENT_SHADER) {
        const int color = original != NULL && strstr(original, "mgl_v_front_color") != NULL;
        const int back_color = original != NULL && strstr(original, "mgl_v_back_color") != NULL;
        const int texcoord = original != NULL && strstr(original, "mgl_v_texcoord") != NULL;
        const int fogcoord = original != NULL && strstr(original, "mgl_v_fog_coord") != NULL;
        const int clipcoord = original != NULL && strstr(original, "mgl_v_eye_position") != NULL;
        static const char env_shader[] =
            "uniform sampler2D mgl_u_texture0;\n"
            "uniform sampler2D mgl_u_texture1;\n"
            "uniform sampler2D mgl_u_texture2;\n"
            "uniform sampler2D mgl_u_texture3;\n"
            "uniform sampler2D mgl_u_texture4;\n"
            "uniform sampler2D mgl_u_texture5;\n"
            "uniform sampler2D mgl_u_texture6;\n"
            "uniform sampler2D mgl_u_texture7;\n"
            "uniform vec2 mgl_u_rect_scale[8];\n"
            "uniform int mgl_u_env_enabled[8];\n"
            "uniform int mgl_u_env_mode[8];\n"
            "uniform vec4 mgl_u_env_color[8];\n"
            "uniform int mgl_u_env_rgb_mode[8];\n"
            "uniform int mgl_u_env_alpha_mode[8];\n"
            "uniform int mgl_u_env_rgb_source[24];\n"
            "uniform int mgl_u_env_alpha_source[24];\n"
            "uniform int mgl_u_env_rgb_operand[24];\n"
            "uniform int mgl_u_env_alpha_operand[24];\n"
            "uniform float mgl_u_env_rgb_scale[8];\n"
            "uniform float mgl_u_env_alpha_scale[8];\n"
            "uniform float mgl_u_env_lod_bias[8];\n"
            "uniform int mgl_u_clip_enabled[6];\n"
            "uniform vec4 mgl_u_clip_plane[6];\n"
            "uniform int mgl_u_alpha_test_enabled;\n"
            "uniform int mgl_u_alpha_test_func;\n"
            "uniform float mgl_u_alpha_test_ref;\n"
            "uniform int mgl_u_fog_enabled;\n"
            "uniform int mgl_u_fog_mode;\n"
            "uniform float mgl_u_fog_density;\n"
            "uniform float mgl_u_fog_start;\n"
            "uniform float mgl_u_fog_end;\n"
            "uniform vec4 mgl_u_fog_color;\n"
            "bool mgl_clip_reject(vec4 eye){for(int i=0;i<6;++i){"
            "if(mgl_u_clip_enabled[i]!=0&&dot(eye,mgl_u_clip_plane[i])<0.0)return true;}return "
            "false;}\n"
            "out vec4 mgl_ff_frag_color;\n"
            "bool mgl_ff_alpha(float a){int f=mgl_u_alpha_test_func;float r=mgl_u_alpha_test_ref;\n"
            "if(f==512)return false;if(f==513)return a<r;if(f==514)return a==r;\n"
            "if(f==515)return a<=r;if(f==516)return a>r;if(f==517)return a!=r;\n"
            "if(f==518)return a>=r;return true;}\n"
            "vec4 mgl_ff_source(int source,int unit,vec4 texels[8],vec4 primary,vec4 previous){\n"
            "if(source==5890)return texels[unit];\n"
            "if(source==34166)return mgl_u_env_color[unit];\n"
            "if(source==34167)return primary;\n"
            "if(source==34168)return previous;\n"
            "if(source>=33984&&source<33992)return texels[source-33984];\n"
            "return vec4(0.0);}\n"
            "vec3 mgl_ff_rgb_op(vec4 source,int operand){\n"
            "if(operand==769)return vec3(1.0)-source.rgb;\n"
            "if(operand==770)return vec3(source.a);\n"
            "if(operand==771)return vec3(1.0-source.a);\n"
            "return source.rgb;}\n"
            "float mgl_ff_alpha_op(vec4 source,int operand){\n"
            "return operand==771?1.0-source.a:source.a;}\n"
            "vec4 mgl_ff_combine(int unit,vec4 primary,vec4 previous,vec4 texels[8]){\n"
            "int p=unit*3;\n"
            "vec3 "
            "r0=mgl_ff_rgb_op(mgl_ff_source(mgl_u_env_rgb_source[p],unit,texels,primary,previous),"
            "mgl_u_env_rgb_operand[p]);\n"
            "vec3 "
            "r1=mgl_ff_rgb_op(mgl_ff_source(mgl_u_env_rgb_source[p+1],unit,texels,primary,previous)"
            ",mgl_u_env_rgb_operand[p+1]);\n"
            "vec3 "
            "r2=mgl_ff_rgb_op(mgl_ff_source(mgl_u_env_rgb_source[p+2],unit,texels,primary,previous)"
            ",mgl_u_env_rgb_operand[p+2]);\n"
            "float "
            "a0=mgl_ff_alpha_op(mgl_ff_source(mgl_u_env_alpha_source[p],unit,texels,primary,"
            "previous),mgl_u_env_alpha_operand[p]);\n"
            "float "
            "a1=mgl_ff_alpha_op(mgl_ff_source(mgl_u_env_alpha_source[p+1],unit,texels,primary,"
            "previous),mgl_u_env_alpha_operand[p+1]);\n"
            "float "
            "a2=mgl_ff_alpha_op(mgl_ff_source(mgl_u_env_alpha_source[p+2],unit,texels,primary,"
            "previous),mgl_u_env_alpha_operand[p+2]);\n"
            "int rgb=mgl_u_env_rgb_mode[unit];\n"
            "int alpha=mgl_u_env_alpha_mode[unit];\n"
            "vec3 c=r0;\n"
            "if(rgb==8448)c=r0*r1;\n"
            "else if(rgb==260)c=r0+r1;\n"
            "else if(rgb==34164)c=r0+r1-vec3(0.5);\n"
            "else if(rgb==34165)c=mix(r1,r0,r2);\n"
            "else if(rgb==34023)c=r0-r1;\n"
            "else if(rgb==34478||rgb==34479)c=vec3(4.0*dot(r0-vec3(0.5),r1-vec3(0.5)));\n"
            "float a=a0;\n"
            "if(alpha==8448)a=a0*a1;\n"
            "else if(alpha==260)a=a0+a1;\n"
            "else if(alpha==34164)a=a0+a1-0.5;\n"
            "else if(alpha==34165)a=mix(a1,a0,a2);\n"
            "else if(alpha==34023)a=a0-a1;\n"
            "if(rgb!=34478&&rgb!=34479)c*=mgl_u_env_rgb_scale[unit];\n"
            "a*=mgl_u_env_alpha_scale[unit];\n"
            "vec4 result=clamp(vec4(c,a),0.0,1.0);\n"
            "if(rgb==34479)result.a=result.r;\n"
            "return result;}\n"
            "void mgl_ff_apply(int unit,vec4 primary,inout vec4 c,vec4 texels[8]){\n"
            "if(mgl_u_env_enabled[unit]==0)return;\n"
            "vec4 t=texels[unit]; int mode=mgl_u_env_mode[unit];\n"
            "if(mode==7681)c=t;\n"
            "else if(mode==8449)c=vec4(mix(c.rgb,t.rgb,t.a),c.a);\n"
            "else if(mode==3042)c=vec4(mix(c.rgb,mgl_u_env_color[unit].rgb,t.rgb),c.a*t.a);\n"
            "else if(mode==260)c=vec4(min(c.rgb+t.rgb,vec3(1.0)),c.a*t.a);\n"
            "else if(mode==34160)c=mgl_ff_combine(unit,primary,c,texels);\n"
            "else c*=t;}\n";
        size_t length;
        int written = snprintf(
            fixed_fragment, sizeof(fixed_fragment),
            "#version 320 es\nprecision highp float;\nprecision highp int;\n"
            "%s%s%s%s%s%s"
            "void main(){%svec4 primary=%s;vec4 c=primary;vec4 texels[8];\n"
            "texels[0]=mgl_u_env_enabled[0]!=0?texture(mgl_u_texture0,(%s)*mgl_u_rect_scale[0],mgl_"
            "u_env_lod_bias[0]):vec4(1.0);\n"
            "texels[1]=mgl_u_env_enabled[1]!=0?texture(mgl_u_texture1,(%s)*mgl_u_rect_scale[1],mgl_"
            "u_env_lod_bias[1]):vec4(1.0);\n"
            "texels[2]=mgl_u_env_enabled[2]!=0?texture(mgl_u_texture2,(%s)*mgl_u_rect_scale[2],mgl_"
            "u_env_lod_bias[2]):vec4(1.0);\n"
            "texels[3]=mgl_u_env_enabled[3]!=0?texture(mgl_u_texture3,(%s)*mgl_u_rect_scale[3],mgl_"
            "u_env_lod_bias[3]):vec4(1.0);\n"
            "texels[4]=mgl_u_env_enabled[4]!=0?texture(mgl_u_texture4,(%s)*mgl_u_rect_scale[4],mgl_"
            "u_env_lod_bias[4]):vec4(1.0);\n"
            "texels[5]=mgl_u_env_enabled[5]!=0?texture(mgl_u_texture5,(%s)*mgl_u_rect_scale[5],mgl_"
            "u_env_lod_bias[5]):vec4(1.0);\n"
            "texels[6]=mgl_u_env_enabled[6]!=0?texture(mgl_u_texture6,(%s)*mgl_u_rect_scale[6],mgl_"
            "u_env_lod_bias[6]):vec4(1.0);\n"
            "texels[7]=mgl_u_env_enabled[7]!=0?texture(mgl_u_texture7,(%s)*mgl_u_rect_scale[7],mgl_"
            "u_env_lod_bias[7]):vec4(1.0);\n"
            "for(int unit=0;unit<8;++unit)mgl_ff_apply(unit,primary,c,texels);\n"
            "if(mgl_u_fog_enabled!=0){float z=%s;float f;int mode=mgl_u_fog_mode;"
            "if(mode==2048)f=exp(-mgl_u_fog_density*z);"
            "else if(mode==2049){float d=mgl_u_fog_density*z;f=exp(-d*d);}"
            "else f=(mgl_u_fog_end-z)/max(mgl_u_fog_end-mgl_u_fog_start,0.000001);"
            "c.rgb=mix(mgl_u_fog_color.rgb,c.rgb,clamp(f,0.0,1.0));}\n"
            "if(mgl_u_alpha_test_enabled!=0&&!mgl_ff_alpha(c.a))discard;"
            "mgl_ff_frag_color=c;}\n",
            color ? "in vec4 mgl_v_front_color;\n" : "",
            back_color ? "in vec4 mgl_v_back_color;\n" : "",
            texcoord ? "in vec4 mgl_v_texcoord[8];\n" : "",
            fogcoord ? "in float mgl_v_fog_coord;\n" : "",
            clipcoord ? "in vec4 mgl_v_eye_position;\n" : "", env_shader,
            clipcoord ? "if(mgl_clip_reject(mgl_v_eye_position))discard;" : "",
            color && back_color
                ? "(gl_FrontFacing ? mgl_v_front_color : mgl_v_back_color)"
                : (color ? "mgl_v_front_color" : (back_color ? "mgl_v_back_color" : "vec4(1.0)")),
            texcoord ? "mgl_v_texcoord[0].xy" : "vec2(0.0)",
            texcoord ? "mgl_v_texcoord[1].xy" : "vec2(0.0)",
            texcoord ? "mgl_v_texcoord[2].xy" : "vec2(0.0)",
            texcoord ? "mgl_v_texcoord[3].xy" : "vec2(0.0)",
            texcoord ? "mgl_v_texcoord[4].xy" : "vec2(0.0)",
            texcoord ? "mgl_v_texcoord[5].xy" : "vec2(0.0)",
            texcoord ? "mgl_v_texcoord[6].xy" : "vec2(0.0)",
            texcoord ? "mgl_v_texcoord[7].xy" : "vec2(0.0)",
            fogcoord ? "abs(mgl_v_fog_coord)" : "gl_FragCoord.z");
        if (written < 0 || (size_t)written >= sizeof(fixed_fragment))
            return 0u;
        if (targets != NULL && texcoord) {
            unsigned unit;
            for (unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit) {
                GLenum target = targets[unit];
                char old_line[256], replacement[256], declaration[80], typed_declaration[80];
                char *line, *end;
                const char *sampler = target == GL_TEXTURE_3D         ? "sampler3D"
                                      : target == GL_TEXTURE_CUBE_MAP ? "samplerCube"
                                                                      : "sampler2D";
                if (target != GL_TEXTURE_1D && target != GL_TEXTURE_3D &&
                    target != GL_TEXTURE_CUBE_MAP)
                    continue;
                (void)snprintf(declaration, sizeof(declaration),
                               "uniform sampler2D mgl_u_texture%u;", unit);
                (void)snprintf(typed_declaration, sizeof(typed_declaration),
                               "uniform %s mgl_u_texture%u;", sampler, unit);
                if (!replace_fixed_shader_text(fixed_fragment, sizeof(fixed_fragment), declaration,
                                               typed_declaration))
                    return 0u;
                (void)snprintf(declaration, sizeof(declaration), "texels[%u]=", unit);
                line = strstr(fixed_fragment, declaration);
                if (line == NULL || (end = strchr(line, '\n')) == NULL ||
                    (size_t)(end + 1 - line) >= sizeof(old_line))
                    return 0u;
                memcpy(old_line, line, (size_t)(end + 1 - line));
                old_line[end + 1 - line] = '\0';
                if (target == GL_TEXTURE_1D)
                    (void)snprintf(replacement, sizeof(replacement),
                                   "texels[%u]=mgl_u_env_enabled[%u]!=0?texture(mgl_u_texture%u,"
                                   "vec2(mgl_v_texcoord[%u].x/"
                                   "mgl_v_texcoord[%u].w,0.5),mgl_u_env_lod_bias[%u]):vec4(1.0);\n",
                                   unit, unit, unit, unit, unit, unit);
                else
                    (void)snprintf(replacement, sizeof(replacement),
                                   "texels[%u]=mgl_u_env_enabled[%u]!=0?texture(mgl_u_texture%u,"
                                   "mgl_v_texcoord[%u].xyz/"
                                   "mgl_v_texcoord[%u].w,mgl_u_env_lod_bias[%u]):vec4(1.0);\n",
                                   unit, unit, unit, unit, unit, unit);
                if (!replace_fixed_shader_text(fixed_fragment, sizeof(fixed_fragment), old_line,
                                               replacement))
                    return 0u;
            }
        }
        length = strlen(fixed_fragment);
        (void)length;
        source = fixed_fragment;
    }
    if (ctx->native_dispatch->glesCreateShader == NULL ||
        ctx->native_dispatch->glesShaderSource == NULL ||
        ctx->native_dispatch->glesCompileShader == NULL ||
        ctx->native_dispatch->glesGetShaderiv == NULL ||
        ctx->native_dispatch->glesDeleteShader == NULL)
        return 0u;
    shader = ctx->native_dispatch->glesCreateShader(type);
    if (shader == 0u)
        return 0u;
    if (type == GL_VERTEX_SHADER) {
        const GLchar *parts[] = {fixed_vertex_0, fixed_vertex_1, fixed_vertex_2};
        ctx->native_dispatch->glesShaderSource(shader, 3, parts, NULL);
    } else {
        ctx->native_dispatch->glesShaderSource(shader, 1, (const GLchar *const *)&source, NULL);
    }
    ctx->native_dispatch->glesCompileShader(shader);
    ctx->native_dispatch->glesGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char *log = mgl_native_shader_log(ctx, shader);
        (void)fprintf(stderr,
                      "mGL: error: event=implicit_stage_compile type=0x%x status=FAIL log=%s\n",
                      (unsigned)type, log != NULL ? log : "");
        free(log);
        ctx->native_dispatch->glesDeleteShader(shader);
        return 0u;
    }
    return shader;
}

/* Synthetic reflection must not appear in application shader queries. */
GLuint mgl_compile_full_fixed_function_stage(struct mgl_context *ctx, GLenum type) {
    struct mgl_shader synthetic_shader = {0};
    struct mgl_program synthetic_program = {0};
    if (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER)
        return 0u;
    synthetic_shader.translated_source =
        "mgl_v_front_color mgl_v_back_color mgl_v_texcoord mgl_v_fog_coord mgl_v_eye_position";
    synthetic_program.attached[0] = &synthetic_shader;
    synthetic_program.attached_count = 1u;
    GLenum targets[MGL_TEXTURE_UNITS];
    unsigned unit;
    for (unit = 0u; unit < MGL_TEXTURE_UNITS; ++unit)
        targets[unit] = mgl_compat_selected_texture_target(&ctx->compat, unit);
    return mgl_compile_implicit_compat_stage(ctx, type, &synthetic_program, targets);
}

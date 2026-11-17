#include "mgl/glx.h"
#include "active_texture_trace.h"
#include "bind_texture_trace.h"

#include "tex_parameter_trace.h"
#include "tex_parameter_variants_trace.h"

#include "tex_parameter_integer_trace.h"

#include "tex_parameter_getters_trace.h"

#include "tex_level_query_trace.h"
#include "tex_image_readback_trace.h"
#include "tex_upload_trace.h"
#include "tex_compressed_copy_trace.h"
#include "tex_storage_mipmap_readback_trace.h"
#include "tex_buffer_attachment_trace.h"
#include "sampler_lifecycle_trace.h"
#include "buffer_object_trace.h"
#include "core_buffer_semantics_trace.h"
#include "buffer_map_flush_trace.h"
#include "framebuffer_renderbuffer_trace.h"
#include "enable_state_trace.h"
#include "query_object_trace.h"
#include "sync_object_trace.h"
#include "transform_feedback_trace.h"
#include "shader_program_trace.h"
#include "program_uniform_matrix_trace.h"
#include "program_reflection_trace.h"
#include "uniform_matrix_trace.h"
#include "vertex_attrib_state_trace.h"
#include "vertex_current_batch_trace.h"
#include "object_lifecycle_batch_trace.h"
#include "matrix_family_batch_trace.h"
#include "scalar_uniform_batch_trace.h"
#include "immediate_current_batch_trace.h"
#include "extension_multitex_batch_trace.h"
#include "fixed_point_batch_trace.h"
#include "texture_dsa_parameter_batch_trace.h"
#include "indexed_family_batch_trace.h"
#include "arb_program_compat_batch_trace.h"
#include "client_array_state_trace.h"
#include "draw_call_trace.h"
#include "modern_state_trace.h"
#include "indexed_state_getters_trace.h"
#include "legacy_state_trace.h"
#include "fixed_function_state_trace.h"
#include "fog_state_trace.h"
#include "light_state_trace.h"
#include "evaluator_state_trace.h"
#include "selection_state_trace.h"
#include "raster_operation_trace.h"
#include "dsa_texture_state_trace.h"
#include "depth_stencil_state_trace.h"
#include "dsa_texture_transfer_trace.h"
#include "object_list_batch_trace.h"
#include "framebuffer_family_batch_trace.h"
#include "texture_transfer_bx_trace.h"
#include "debug_and_compat_by_trace.h"
#include "indexed_output_vao_bz_trace.h"
#include "core_texture_dsa_ca_trace.h"
#define MGL_MESA_REFERENCE_EXT_ALIASES
#include "multitex_alias_cb_trace.h"
#include "vao_ownership_cc_trace.h"
#define MGL_MESA_REFERENCE_CD_ALIASES
#include "compat_remainder_cd_trace.h"
#undef MGL_MESA_REFERENCE_CD_ALIASES
#undef MGL_MESA_REFERENCE_EXT_ALIASES
#include <stdio.h>
#include <string.h>

static GLuint matrix_ref_program;
static GLint matrix_ref_locations[10];

static int matrix_ref_compile(GLuint shader, const char *source) {
    GLint ok = 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    return ok != 0;
}

static GLuint matrix_trace_prepare(void) {
    const char *vs =
        "#version 130\n"
        "uniform mat2x3 u23; uniform mat4x3 u43[2]; uniform mat4 u44;\n"
        "uniform mat2 u22; uniform mat3 u33; uniform mat3x2 u32;\n"
        "uniform mat2x4 u24; uniform mat4x2 u42; uniform mat3x4 u34;\n"
        "void main() { vec3 x = u23 * vec2(1.0) + u43[0] * vec4(1.0) + "
        "u43[1] * vec4(1.0) + (u44 * vec4(1.0)).xyz; "
        "float y = u22[0][0] + u33[0][0] + u32[0][0] + u24[0][0] + u42[0][0] + u34[0][0]; "
        "gl_Position = vec4(x + vec3(y),1.0); }\n";
    const char *fs = "#version 130\nvoid main() { gl_FragColor=vec4(1.0); }\n";
    GLuint vert = glCreateShader(GL_VERTEX_SHADER), frag = glCreateShader(GL_FRAGMENT_SHADER);
    GLint linked = 0;
    if (!matrix_ref_compile(vert, vs) || !matrix_ref_compile(frag, fs)) {
        fputs("native matrix reference: GLSL shader compile failed\n", stderr);
        return 0;
    }
    matrix_ref_program = glCreateProgram();
    glAttachShader(matrix_ref_program, vert);
    glAttachShader(matrix_ref_program, frag);
    glLinkProgram(matrix_ref_program);
    glGetProgramiv(matrix_ref_program, GL_LINK_STATUS, &linked);
    glDeleteShader(vert);
    glDeleteShader(frag);
    if (!linked) {
        fputs("native matrix reference: GLSL link failed\n", stderr);
        return 0;
    }
    matrix_ref_locations[0] = glGetUniformLocation(matrix_ref_program, "u23");
    matrix_ref_locations[1] = glGetUniformLocation(matrix_ref_program, "u43[0]");
    matrix_ref_locations[2] = glGetUniformLocation(matrix_ref_program, "u43[1]");
    matrix_ref_locations[3] = glGetUniformLocation(matrix_ref_program, "u44");
    matrix_ref_locations[4] = glGetUniformLocation(matrix_ref_program, "u22");
    matrix_ref_locations[5] = glGetUniformLocation(matrix_ref_program, "u33");
    matrix_ref_locations[6] = glGetUniformLocation(matrix_ref_program, "u32");
    matrix_ref_locations[7] = glGetUniformLocation(matrix_ref_program, "u24");
    matrix_ref_locations[8] = glGetUniformLocation(matrix_ref_program, "u42");
    matrix_ref_locations[9] = glGetUniformLocation(matrix_ref_program, "u34");
    return matrix_ref_program;
}

static GLint matrix_trace_location(int index) {
    return matrix_ref_locations[index];
}

static void matrix_trace_fetch(int index, GLfloat *values) {
    glGetUniformfv(matrix_ref_program, matrix_ref_locations[index], values);
}

static void matrix_trace_finish(GLuint program) {
    if (program != 0u)
        glDeleteProgram(program);
}

int main(void) {
    Display *display = XOpenDisplay(NULL);
    XVisualInfo *visual;
    GLXContext context;
    Colormap map;
    XSetWindowAttributes attributes;
    Window window;
    int screen;
    int visual_attributes[] = {GLX_RGBA, GLX_DEPTH_SIZE, 16, None};
    if (display == NULL) {
        fputs("native reference: XOpenDisplay failed\n", stderr);
        return 2;
    }
    screen = DefaultScreen(display);
    visual = glXChooseVisual(display, screen, visual_attributes);
    if (visual == NULL) {
        fputs("native reference: no compatible GLX visual\n", stderr);
        XCloseDisplay(display);
        return 2;
    }
    map = XCreateColormap(display, RootWindow(display, screen), visual->visual, AllocNone);
    attributes.colormap = map;
    attributes.border_pixel = 0;
    window = XCreateWindow(display, RootWindow(display, screen), 0, 0, 64, 64, 0, visual->depth,
                           InputOutput, visual->visual, CWColormap | CWBorderPixel, &attributes);
    context = glXCreateContext(display, visual, NULL, True);
    if (context == NULL || !glXMakeCurrent(display, window, context)) {
        fputs("native reference: no GLX context\n", stderr);
        if (context != NULL)
            glXDestroyContext(display, context);
        XDestroyWindow(display, window);
        XFreeColormap(display, map);
        XFree(visual);
        XCloseDisplay(display);
        return 2;
    }
    /* Guard against accidentally testing mGL as its own reference. */
    {
        const GLubyte *vendor = glGetString(GL_VENDOR);
        const GLubyte *renderer = glGetString(GL_RENDERER);
        if (vendor == NULL || renderer == NULL ||
            strstr((const char *)glGetString(GL_VERSION), "ManifestGL") != NULL) {
            fputs("native reference: reference renderer is unavailable or resolves to mGL\n",
                  stderr);
            return 3;
        }
        fprintf(stderr, "Native OpenGL reference: vendor=%s renderer=%s\n", vendor, renderer);
    }
    active_texture_trace();
    bind_texture_trace();
    tex_parameter_trace();
    tex_parameter_variants_trace();
    tex_parameter_integer_trace();
    tex_parameter_getters_trace();
    tex_level_query_trace();
    tex_image_readback_trace();
    tex_upload_trace();
    tex_compressed_copy_trace();
    tex_storage_mipmap_readback_trace();
    tex_buffer_attachment_trace();
    sampler_lifecycle_trace();
    buffer_object_trace();
    core_buffer_semantics_trace();
    buffer_map_flush_trace();
    framebuffer_trace_trace();
    enable_state_trace();
    query_object_trace();
    sync_object_trace();
    transform_feedback_trace_trace();
    shader_program_trace_trace();
    if (!matrix_trace_trace())
        return 98;
    if (!reflection_trace())
        return 99;
    if (!uniform_matrix_trace_trace())
        return 100;
    vertex_attrib_state_trace();
    vertex_current_batch_trace();
    client_array_state_trace();
    draw_call_trace_trace();
    modern_state_trace();
    indexed_state_getters_trace();
    legacy_state_trace();
    fixed_function_state_trace();
    fog_state_trace();
    light_state_trace();
    evaluator_state_trace();
    selection_state_trace();
    raster_operation_trace();
    dsa_texture_state_trace();
    depth_stencil_state_trace();
    dsa_texture_transfer_trace();
    object_lifecycle_batch_trace();
    matrix_family_batch_trace();
    scalar_uniform_batch_trace();
    immediate_current_batch_trace();
    extension_multitex_batch_trace();
    fixed_point_batch_trace();
    texture_dsa_parameter_batch_trace();
    indexed_family_batch_trace();
    arb_program_compat_batch_trace();
    object_list_batch_trace();
    fbf_trace();
    texture_transfer_bx_trace();
    by_trace();
    indexed_output_vao_bz_trace();
    core_texture_dsa_ca_trace();
    multitex_alias_cb_trace();
    vao_ownership_cc_trace();
    compat_remainder_cd_trace();
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, context);
    XDestroyWindow(display, window);
    XFreeColormap(display, map);
    XFree(visual);
    XCloseDisplay(display);
    return 0;
}

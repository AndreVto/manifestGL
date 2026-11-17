#include "mgl/gl.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "state/state.h"
#include "conformance/legacy_entrypoint_aliases_cases.h"
#include "abi/proc_address.h"
#include "shader/shader_store.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define REQUIRE(condition)                                                                          \
    do {                                                                                            \
        if (!(condition)) {                                                                         \
            fprintf(stderr, "legacy entrypoint aliases: %s:%d: %s\n", __FILE__, __LINE__,        \
                    #condition);                                                                    \
            abort();                                                                                \
        }                                                                                           \
    } while (0)

static int clip_calls, bound_calls, patch_calls, patch_fv_calls, shading_calls, offset_calls,
    multisample_calls;
static int viewport_array_calls, scissor_array_calls, depth_array_calls, polygon_mode_calls;
static GLint native_samples;
static GLuint query_next_name;
static unsigned query_begin_indexed_calls, query_end_indexed_calls;
static GLdouble received_min, received_max;

static void enable_state(GLenum cap) {
    (void)cap;
}

static void disable_state(GLenum cap) {
    (void)cap;
}

static void accept_restored_cap(GLenum cap) {
    (void)cap;
}

static const GLubyte *runtime_extensions(GLenum pname) {
    (void)pname;
    return (const GLubyte *)"GL_EXT_clip_control GL_EXT_depth_bounds_test GL_OES_sample_shading "
                            "GL_EXT_polygon_offset_clamp GL_OES_viewport_array GL_NV_polygon_mode GL_ARB_transform_feedback3";
}

static void clip_control(GLenum origin, GLenum depth) {
    REQUIRE(origin == GL_UPPER_LEFT && depth == GL_ZERO_TO_ONE);
    ++clip_calls;
}

static void patch_parameterfv(GLenum pname, const GLfloat *v) {
    REQUIRE(pname == GL_PATCH_DEFAULT_INNER_LEVEL || pname == GL_PATCH_DEFAULT_OUTER_LEVEL);
    REQUIRE(v != NULL);
    ++patch_fv_calls;
}

static void min_sample_shading(GLfloat v) {
    REQUIRE(v >= 0.0f && v <= 1.0f);
    ++shading_calls;
}

static void polygon_mode(GLenum face, GLenum mode) {
    REQUIRE(face == GL_FRONT_AND_BACK);
    REQUIRE(mode == GL_LINE || mode == GL_POINT || mode == GL_FILL);
    ++polygon_mode_calls;
}

static void polygon_offset_clamp(GLfloat factor, GLfloat units, GLfloat clamp) {
    (void)factor;
    (void)units;
    (void)clamp;
    ++offset_calls;
}

static void depth_bounds(GLclampd min, GLclampd max) {
    received_min = min;
    received_max = max;
    ++bound_calls;
}

static void patch_parameter(GLenum pname, GLint value) {
    REQUIRE(pname == GL_PATCH_VERTICES && value == 4);
    ++patch_calls;
}

static void gen_queries(GLsizei n, GLuint *ids) {
    for (GLsizei i = 0; i < n; ++i)
        ids[i] = ++query_next_name;
}

static void begin_query_indexed(GLenum target, GLuint index, GLuint name) {
    REQUIRE(target == GL_PRIMITIVES_GENERATED && index < 4u && name != 0u);
    ++query_begin_indexed_calls;
}

static void end_query_indexed(GLenum target, GLuint index) {
    REQUIRE(target == GL_PRIMITIVES_GENERATED && index < 4u);
    ++query_end_indexed_calls;
}

static void get_query_result(GLuint name, GLenum pname, GLuint *value) {
    REQUIRE(name != 0u);
    (void)pname;
    *value = 1u;
}

static void runtime_limit(GLenum pname, GLint *result) {
    if (pname == GL_MAX_PATCH_VERTICES)
        *result = 32;
    if (pname == GL_SAMPLES)
        *result = native_samples;
    if (pname == GL_MAX_VIEWPORTS)
        *result = 16;
    if (pname == GL_MAX_VERTEX_STREAMS)
        *result = 4;
}

static void multisample_query(GLenum pname, GLuint index, GLfloat *out) {
    REQUIRE(pname == GL_SAMPLE_POSITION && index < (GLuint)native_samples);
    out[0] = 0.25f;
    out[1] = 0.75f;
    ++multisample_calls;
}

static void viewport_array(GLuint first, GLsizei count, const GLfloat *v) {
    REQUIRE(first < 16u && count > 0 && v != NULL);
    ++viewport_array_calls;
}

static void scissor_array(GLuint first, GLsizei count, const GLint *v) {
    REQUIRE(first < 16u && count > 0 && v != NULL);
    ++scissor_array_calls;
}

static void depth_range_array(GLuint first, GLsizei count, const GLfloat *v) {
    REQUIRE(first < 16u && count > 0 && v != NULL);
    ++depth_array_calls;
}

static GLuint fake_program(void) {
    return 77u;
}

static void fake_delete_program(GLuint program) {
    REQUIRE(program == 77u);
}

static unsigned atomic_query_calls;

static void fake_programiv(GLuint program, GLenum pname, GLint *value) {
    REQUIRE(program == 77u && pname == GL_ACTIVE_ATOMIC_COUNTER_BUFFERS);
    *value = 1;
}

static void fake_atomic_query(GLuint program, GLenum interface, GLuint index, GLsizei prop_count,
                              const GLenum *props, GLsizei buf_size, GLsizei *length,
                              GLint *values) {
    REQUIRE(program == 77u && interface == GL_ATOMIC_COUNTER_BUFFER && index == 0u);
    REQUIRE(prop_count == 1 && buf_size == 1 && props != NULL && values != NULL);
    ++atomic_query_calls;
    if (length != NULL)
        *length = 1;
    switch (props[0]) {
        case GL_BUFFER_BINDING:
            values[0] = 3;
            break;
        case GL_BUFFER_DATA_SIZE:
            values[0] = 4;
            break;
        case GL_NUM_ACTIVE_VARIABLES:
            values[0] = 1;
            break;
        case GL_REFERENCED_BY_VERTEX_SHADER:
            values[0] = 1;
            break;
        case GL_REFERENCED_BY_FRAGMENT_SHADER:
        case GL_ACTIVE_VARIABLES:
            values[0] = 0;
            break;
        default:
            REQUIRE(0);
            break;
    }
}

static void sample_mask(GLuint index, GLbitfield mask) {
    (void)index;
    (void)mask;
}

static void integer64_query(GLenum pname, GLint64 *result) {
    REQUIRE(pname == GL_TIMESTAMP || pname == GL_MAX_SERVER_WAIT_TIMEOUT);
    *result = INT64_C(0x100000123);
}

static void internalformat_query(GLenum target, GLenum format, GLenum pname, GLsizei count,
                                 GLint *values) {
    if (count == 0 || values == NULL)
        return;
    values[0] = format == GL_RGBA8 ? (pname == GL_NUM_SAMPLE_COUNTS ? 1 : 4) : 0;
    (void)target;
}

static GLuint legacy_atomic_program(void) {
    struct mgl_context *ctx = mgl_context_current();
    GLuint program = glCreateProgram();
    struct mgl_program *obj = mgl_program_find(ctx, program);
    REQUIRE(obj != NULL);
    obj->linked = GL_TRUE;
    return program;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesCreateProgram = fake_program;
    native.glesDeleteProgram = fake_delete_program;
    native.glesGetProgramiv = fake_programiv;
    native.glesGetProgramResourceiv = fake_atomic_query;
    native.glesSampleMaski = sample_mask;
    native.glesGenQueries = gen_queries;
    native.glesBeginQueryIndexed = begin_query_indexed;
    native.glesEndQueryIndexed = end_query_indexed;
    native.glesGetQueryObjectuiv = get_query_result;
    native.glesViewportArrayvOES = viewport_array;
    native.glesScissorArrayvOES = scissor_array;
    native.glesDepthRangeArrayfvOES = depth_range_array;
    native.glesGetString = runtime_extensions;
    native.glesEnable = enable_state;
    native.glesDisable = disable_state;
    native.glesClipControlEXT = clip_control;
    native.glesDepthBoundsEXT = depth_bounds;
    native.glesPatchParameteri = patch_parameter;
    native.glesPatchParameterfv = patch_parameterfv;
    native.glesMinSampleShading = min_sample_shading;
    native.glesPolygonOffsetClamp = polygon_offset_clamp;
    native.glesPolygonModeNV = polygon_mode;
    native.glesGetIntegerv = runtime_limit;
    native.glesGetInternalformativ = internalformat_query;
    native.glesGetInteger64v = integer64_query;
    native.glesGetMultisamplefv = multisample_query;
    ctx.native_dispatch = &native;
    ctx.next_query_name = 1u;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_state_init(&ctx.canonical_state, 64, 64);
    mgl_context_set_current_for_test(&ctx);
    {
        const char *const names[] = {"glPolygonOffsetClamp",
                                     "glPolygonOffsetClampEXT",
                                     "glMinSampleShading",
                                     "glMinSampleShadingARB",
                                     "glPatchParameterfv",
                                     "glGetMultisamplefv",
                                     "glViewportArrayv",
                                     "glViewportIndexedf",
                                     "glViewportIndexedfv",
                                     "glScissorArrayv",
                                     "glScissorIndexed",
                                     "glScissorIndexedv",
                                     "glDepthRangeArrayv",
                                     "glDepthRangeIndexed",
                                     "glGetDoublei_v",
                                     "glGetFloati_v",
                                     "glPolygonMode",
                                     "glBeginQueryIndexed",
                                     "glEndQueryIndexed",
                                     "glGetQueryIndexediv",
                                     "glGetInteger64v",
                                     "glGetInternalformati64v",
                                     "glGetActiveAtomicCounterBufferiv",
                                     "glGetFragDataLocationEXT",
                                     "glColor3hNV",
                                     "glColor3hvNV",
                                     "glColor4hNV",
                                     "glColor4hvNV",
                                     "glNormal3hNV",
                                     "glNormal3hvNV",
                                     "glSecondaryColor3hNV",
                                     "glSecondaryColor3hvNV",
                                     "glFogCoordhNV",
                                     "glFogCoordhvNV",
                                     "glTexCoord1hNV",
                                     "glTexCoord1hvNV",
                                     "glTexCoord2hNV",
                                     "glTexCoord2hvNV",
                                     "glTexCoord3hNV",
                                     "glTexCoord3hvNV",
                                     "glTexCoord4hNV",
                                     "glTexCoord4hvNV",
                                     "glMultiTexCoord1hNV",
                                     "glMultiTexCoord1hvNV",
                                     "glMultiTexCoord2hNV",
                                     "glMultiTexCoord2hvNV",
                                     "glMultiTexCoord3hNV",
                                     "glMultiTexCoord3hvNV",
                                     "glMultiTexCoord4hNV",
                                     "glMultiTexCoord4hvNV",
                                     "glVertex2hNV",
                                     "glVertex2hvNV",
                                     "glVertex3hNV",
                                     "glVertex3hvNV",
                                     "glVertex4hNV",
                                     "glVertex4hvNV",
                                     "glVertexAttrib1hNV",
                                     "glVertexAttrib1hvNV",
                                     "glVertexAttrib2hNV",
                                     "glVertexAttrib3hNV",
                                     "glVertexAttrib3hvNV",
                                     "glVertexAttrib4hNV"};
        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
            REQUIRE(mgl_proc_address(names[i]) != NULL);
    }
    legacy_entrypoint_aliases_trace();
    {
        GLint64 value = -1;
        native.glesGetInteger64v = NULL;
        glGetInteger64v(GL_TIMESTAMP, &value);
        REQUIRE(glGetError() == GL_INVALID_OPERATION && value == -1);
        native.glesGetInteger64v = integer64_query;
        glGetInteger64v(GL_MAX_SERVER_WAIT_TIMEOUT, &value);
        REQUIRE(glGetError() == GL_NO_ERROR && value == INT64_C(0x100000123));
        glGetInternalformati64v(GL_RENDERBUFFER, GL_RGBA8, GL_SAMPLES, 1, &value);
        REQUIRE(glGetError() == GL_NO_ERROR && value == 4);
        value = -1;
        glGetInternalformati64v(GL_RENDERBUFFER, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, -1, &value);
        REQUIRE(glGetError() == GL_INVALID_VALUE && value == -1);
    }
    {
        GLuint program = glCreateProgram();
        struct mgl_program *record = mgl_program_find(&ctx, program);
        GLint value = -1;
        REQUIRE(record != NULL && record->native_name == 77u);
        record->linked = GL_TRUE;
        glGetActiveAtomicCounterBufferiv(program, 0u, GL_ATOMIC_COUNTER_BUFFER_BINDING, &value);
        REQUIRE(glGetError() == GL_NO_ERROR && value == 3);
        glGetActiveAtomicCounterBufferiv(program, 0u, GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE, &value);
        REQUIRE(glGetError() == GL_NO_ERROR && value == 4);
        glGetActiveAtomicCounterBufferiv(program, 0u,
                                         GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTERS, &value);
        REQUIRE(glGetError() == GL_NO_ERROR && value == 1 && atomic_query_calls == 10);
        glGetActiveAtomicCounterBufferiv(program, 1u, GL_ATOMIC_COUNTER_BUFFER_BINDING, &value);
        REQUIRE(glGetError() == GL_INVALID_VALUE && value == 1 && atomic_query_calls == 10);
        glGetActiveAtomicCounterBufferiv(program, 0u, 0xdeadu, &value);
        REQUIRE(glGetError() == GL_INVALID_ENUM && value == 1 && atomic_query_calls == 10);
        native.glesGetProgramResourceiv = NULL;
        glGetActiveAtomicCounterBufferiv(program, 0u, GL_ATOMIC_COUNTER_BUFFER_BINDING, &value);
        REQUIRE(glGetError() == GL_INVALID_OPERATION && atomic_query_calls == 10);
        native.glesGetProgramResourceiv = fake_atomic_query;
        glDeleteProgram(program);
    }
    REQUIRE(polygon_mode_calls == 4);
    REQUIRE(query_begin_indexed_calls == 2 && query_end_indexed_calls == 2);
    {
        GLint modes[2] = {0};
        native.glesPolygonModeNV = NULL;
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        REQUIRE(glGetError() == GL_INVALID_OPERATION);
        glGetIntegerv(GL_POLYGON_MODE, modes);
        REQUIRE(modes[0] == (GLint)GL_FILL && modes[1] == (GLint)GL_FILL);
        REQUIRE(glGetError() == GL_NO_ERROR);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        REQUIRE(glGetError() == GL_NO_ERROR);
        native.glesPolygonModeNV = polygon_mode;
    }
    REQUIRE(viewport_array_calls == 4 && scissor_array_calls == 3 && depth_array_calls == 2);
    {
        const GLfloat invalid[8] = {2.f, 3.f, 10.f, 10.f, 4.f, 5.f, -1.f, 6.f};
        GLfloat viewport[4] = {0};
        GLdouble depth[2] = {0};
        GLint scissor[4] = {0};
        GLint viewport_calls = viewport_array_calls;
        GLint scissor_calls = scissor_array_calls;
        GLint depth_calls = depth_array_calls;
        glViewportArrayv(1, 2, invalid);
        REQUIRE(glGetError() == GL_INVALID_VALUE && viewport_array_calls == viewport_calls);
        glViewportArrayv(1, -1, invalid);
        REQUIRE(glGetError() == GL_INVALID_VALUE && viewport_array_calls == viewport_calls);
        glGetFloati_v(GL_VIEWPORT, 1, viewport);
        REQUIRE(glGetError() == GL_NO_ERROR && viewport[0] == 1.f && viewport[2] == 30.f);
        glGetDoubleIndexedv(GL_DEPTH_RANGE, 1, depth);
        REQUIRE(glGetError() == GL_NO_ERROR && depth[0] == 0.125 && depth[1] == 0.5);
        glPushAttrib(GL_VIEWPORT_BIT | GL_SCISSOR_BIT);
        REQUIRE(glGetError() == GL_NO_ERROR);
        glViewportIndexedf(1, 9.f, 10.f, 11.f, 12.f);
        glScissorIndexed(1, 9, 10, 11, 12);
        glDepthRangeIndexed(1, 0.5, 0.75);
        REQUIRE(glGetError() == GL_NO_ERROR);
        native.glesEnable = accept_restored_cap;
        native.glesDisable = accept_restored_cap;
        glPopAttrib();
        native.glesEnable = enable_state;
        native.glesDisable = disable_state;
        REQUIRE(glGetError() == GL_NO_ERROR);
        glGetFloati_v(GL_VIEWPORT, 1, viewport);
        glGetIntegeri_v(GL_SCISSOR_BOX, 1, scissor);
        glGetDoublei_v(GL_DEPTH_RANGE, 1, depth);
        REQUIRE(glGetError() == GL_NO_ERROR && viewport[0] == 1.f && viewport[2] == 30.f);
        REQUIRE(scissor[0] == 3 && scissor[1] == 4 && scissor[2] == 25);
        REQUIRE(depth[0] == 0.125 && depth[1] == 0.5);
        REQUIRE(viewport_array_calls > viewport_calls && scissor_array_calls > scissor_calls &&
               depth_array_calls > depth_calls);
    }

    REQUIRE(clip_calls == 1 && patch_calls == 1);
    REQUIRE(patch_fv_calls == 2 && shading_calls == 3 && offset_calls == 4);
    REQUIRE(bound_calls == 0);
    glDepthBoundsEXT(0.25, 0.75);
    REQUIRE(glGetError() == GL_NO_ERROR && bound_calls == 1);
    REQUIRE(received_min == 0.25 && received_max == 0.75);
    {
        GLdouble limits[2];
        glGetDoublev(GL_DEPTH_BOUNDS_EXT, limits);
        REQUIRE(glGetError() == GL_NO_ERROR);
        REQUIRE(limits[0] == 0.25 && limits[1] == 0.75);
    }
    glEnable(GL_DEPTH_BOUNDS_TEST_EXT);
    REQUIRE(glGetError() == GL_NO_ERROR && glIsEnabled(GL_DEPTH_BOUNDS_TEST_EXT));
    glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
    REQUIRE(glGetError() == GL_NO_ERROR && !glIsEnabled(GL_DEPTH_BOUNDS_TEST_EXT));
    {
        GLfloat position[2] = {0.f, 0.f};
        native_samples = 4;
        glGetMultisamplefv(GL_SAMPLE_POSITION, 2, position);
        REQUIRE(glGetError() == GL_NO_ERROR && multisample_calls == 1);
        REQUIRE(position[0] == 0.25f && position[1] == 0.75f);
        glGetMultisamplefv(GL_SAMPLE_POSITION, 4, position);
        REQUIRE(glGetError() == GL_INVALID_VALUE && multisample_calls == 1);
        native.glesGetMultisamplefv = NULL;
        glGetMultisamplefv(GL_SAMPLE_POSITION, 1, position);
        REQUIRE(glGetError() == GL_INVALID_OPERATION && multisample_calls == 1);
    }
    native.glesPatchParameterfv = NULL;
    {
        const GLfloat levels[2] = {7.0f, 9.0f};
        GLfloat observed[2] = {0};
        glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, levels);
        REQUIRE(glGetError() == GL_INVALID_OPERATION);
        glGetFloatv(GL_PATCH_DEFAULT_INNER_LEVEL, observed);
        REQUIRE(observed[0] == 1.5f && observed[1] == 2.5f);
        glGetError();
    }
    native.glesPolygonOffsetClamp = NULL;
    glPolygonOffsetClamp(1.f, 2.f, 3.f);
    REQUIRE(glGetError() == GL_INVALID_OPERATION && offset_calls == 4);
    native.glesMinSampleShading = NULL;
    glMinSampleShading(0.75f);
    REQUIRE(glGetError() == GL_INVALID_OPERATION && shading_calls == 3);
    glEnable(GL_SAMPLE_SHADING);
    REQUIRE(glGetError() == GL_INVALID_ENUM && !ctx.canonical_state.sample_shading);
    native.glesDepthBoundsEXT = NULL;
    glDepthBoundsEXT(0.3, 0.7);
    REQUIRE(glGetError() == GL_INVALID_OPERATION && bound_calls == 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    return 0;
}

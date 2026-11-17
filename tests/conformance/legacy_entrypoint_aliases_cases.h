#ifndef LEGACY_ENTRYPOINT_ALIASES_TRACE_H
#define LEGACY_ENTRYPOINT_ALIASES_TRACE_H
#include "mgl/gl.h"
#include <stdio.h>

static GLuint legacy_atomic_program(void);

static void legacy_entrypoint_aliases_trace(void) {
    GLint front, back, provoking;
    GLfloat coords[4];
    const GLshort tex2[2] = {2, 3};
    const GLshort tex4[4] = {4, 5, 6, 7};
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
    glStencilFuncSeparate(GL_FRONT, GL_LESS, 3, 0xff);
    glStencilFuncSeparate(GL_BACK, GL_GREATER, 3, 0xff);
#else
    glStencilFuncSeparateATI(GL_LESS, GL_GREATER, 3, 0xff);
#endif
    printf("stencil_func %04x ", glGetError());
    glGetIntegerv(GL_STENCIL_FUNC, &front);
    glGetIntegerv(GL_STENCIL_BACK_FUNC, &back);
    printf("%04x %04x %04x\n", front, back, glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_REPLACE, GL_INCR);
#else
    glStencilOpSeparateATI(GL_BACK, GL_KEEP, GL_REPLACE, GL_INCR);
#endif
    printf("stencil_op %04x\n", glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
    glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
#else
    glProvokingVertexEXT(GL_FIRST_VERTEX_CONVENTION);
#endif
    glGetIntegerv(GL_PROVOKING_VERTEX, &provoking);
    printf("provoking %04x %04x\n", provoking, glGetError());
    {
        GLfloat size = 0.0f;
        glGetFloatv(GL_POINT_SIZE, &size);
        printf("point_size_default %.1f %04x\n", size, glGetError());
        glPointSize(4.0f);
        glGetFloatv(GL_POINT_SIZE, &size);
        printf("point_size_set %.1f %04x\n", size, glGetError());
        glPointSize(0.0f);
        printf("point_size_zero %04x\n", glGetError());
        glPointSize(-1.0f);
        printf("point_size_negative %04x\n", glGetError());
        glGetFloatv(GL_POINT_SIZE, &size);
        printf("point_size_still %.1f %04x\n", size, glGetError());
        glPushAttrib(GL_POINT_BIT);
        glPointSize(2.0f);
        glPopAttrib();
        glGetFloatv(GL_POINT_SIZE, &size);
        printf("point_size_restored %.1f %04x\n", size, glGetError());
        glEnable(GL_PROGRAM_POINT_SIZE);
        printf("program_point_enable %d %04x\n", glIsEnabled(GL_PROGRAM_POINT_SIZE), glGetError());
        glDisable(GL_PROGRAM_POINT_SIZE);
        printf("program_point_disable %d %04x\n", glIsEnabled(GL_PROGRAM_POINT_SIZE), glGetError());
        glPointSize(1.0f);
    }
    glTexCoord2sv(tex2);
    glGetFloatv(GL_CURRENT_TEXTURE_COORDS, coords);
    printf("tex2 %.1f %.1f %.1f %.1f %04x\n", coords[0], coords[1], coords[2], coords[3],
           glGetError());
    glTexCoord4sv(tex4);
    glGetFloatv(GL_CURRENT_TEXTURE_COORDS, coords);
    printf("tex4 %.1f %.1f %.1f %.1f %04x\n", coords[0], coords[1], coords[2], coords[3],
           glGetError());
    {
        GLint vertex[4] = {0, 0, 0, 0};
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glVertexAttribI4i(3, 5, -7, 11, 13);
        glGetVertexAttribIiv(3, GL_CURRENT_VERTEX_ATTRIB, vertex);
#else
        glVertexAttribI4iEXT(3, 5, -7, 11, 13);
        glGetVertexAttribIivEXT(3, GL_CURRENT_VERTEX_ATTRIB, vertex);
#endif
        printf("int_attrib %d %d %d %d %04x\n", vertex[0], vertex[1], vertex[2], vertex[3],
               glGetError());
    }
    {
        GLfloat coord[4] = {0};
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glMultiTexCoord2f(GL_TEXTURE0, 0.25f, 0.75f);
#else
        glMultiTexCoord2fARB(GL_TEXTURE0, 0.25f, 0.75f);
#endif
        glGetFloatv(GL_CURRENT_TEXTURE_COORDS, coord);
        printf("multi_tex %.2f %.2f %.1f %.1f %04x\n", coord[0], coord[1], coord[2], coord[3],
               glGetError());
    }
    {
        GLfloat fog = 0.0f, secondary[4] = {0};
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glFogCoordf(2.5f);
        glSecondaryColor3f(0.25f, 0.50f, 0.75f);
#else
        glFogCoordfEXT(2.5f);
        glSecondaryColor3fEXT(0.25f, 0.50f, 0.75f);
#endif
        glGetFloatv(GL_CURRENT_FOG_COORD, &fog);
        glGetFloatv(GL_CURRENT_SECONDARY_COLOR, secondary);
        printf("legacy_attrib %.2f %.2f %.2f %.2f %04x\n", fog, secondary[0], secondary[1],
               secondary[2], glGetError());
    }
    {
        GLint count = -23, samples = -23;
        glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 1, &count);
        printf("rb_sample_count %d %04x\n", count, glGetError());
        glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_SAMPLES, 1, &samples);
        printf("rb_samples %d %04x\n", samples, glGetError());
        glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 1, &count);
        printf("tex2d_sample_count %d %04x\n", count, glGetError());
        glGetInternalformativ(0xdead, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 1, &count);
        printf("bad_internal_target %04x\n", glGetError());
        glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, 0xdead, 1, &count);
        printf("bad_internal_pname %04x\n", glGetError());
        glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, -1, &count);
        printf("bad_internal_size %04x\n", glGetError());
    }
    {
        GLint origin = -1, depth = -1, patch = -1, max_patch = -1;
        glGetIntegerv(GL_CLIP_ORIGIN, &origin);
        glGetIntegerv(GL_CLIP_DEPTH_MODE, &depth);
        glGetIntegerv(GL_PATCH_VERTICES, &patch);
        glGetIntegerv(GL_MAX_PATCH_VERTICES, &max_patch);
        printf("patch_max %d %04x\n", max_patch, glGetError());
        printf("clip_defaults %04x %04x %d %04x\n", origin, depth, patch, glGetError());
        glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
        glGetIntegerv(GL_CLIP_ORIGIN, &origin);
        glGetIntegerv(GL_CLIP_DEPTH_MODE, &depth);
        printf("clip_set %04x %04x %04x\n", origin, depth, glGetError());
        glClipControl(0xdead, GL_ZERO_TO_ONE);
        printf("clip_bad_origin %04x\n", glGetError());
        glClipControl(GL_LOWER_LEFT, 0xdead);
        printf("clip_bad_depth %04x\n", glGetError());
        glGetIntegerv(GL_CLIP_ORIGIN, &origin);
        printf("clip_unchanged %04x %04x\n", origin, glGetError());
        glDepthBoundsEXT(0.75, 0.25);
        printf("bounds_bad_range %04x\n", glGetError());
        glPatchParameteri(GL_PATCH_VERTICES, 4);
        glGetIntegerv(GL_PATCH_VERTICES, &patch);
        printf("patch_set %d %04x\n", patch, glGetError());
        glPatchParameteri(GL_PATCH_VERTICES, 0);
        printf("patch_zero %04x\n", glGetError());
        glPatchParameteri(GL_PATCH_VERTICES, 9999);
        printf("patch_too_large %04x\n", glGetError());
        glPatchParameteri(0xdead, 4);
        printf("patch_bad_pname %04x\n", glGetError());
        glGetIntegerv(GL_PATCH_VERTICES, &patch);
        printf("patch_unchanged %d %04x\n", patch, glGetError());
    }
    {
        GLint mask_value = 0;
        glGetIntegeri_v(GL_SAMPLE_MASK_VALUE, 0, &mask_value);
        printf("mask_default %08x %04x\n", (unsigned)mask_value, glGetError());
        glSampleMaski(0, 0x1234abcd);
        glGetIntegeri_v(GL_SAMPLE_MASK_VALUE, 0, &mask_value);
        printf("mask_set %08x %04x\n", (unsigned)mask_value, glGetError());
        glSampleMaski(1, 0);
        printf("mask_bad_index %04x\n", glGetError());
        glGetIntegeri_v(GL_SAMPLE_MASK_VALUE, 0, &mask_value);
        printf("mask_unchanged %08x %04x\n", (unsigned)mask_value, glGetError());
    }
    {
        GLfloat minimum = -1.0f, clamp = -1.0f, factor = -1.0f, units = -1.0f;
        glGetFloatv(GL_MIN_SAMPLE_SHADING_VALUE, &minimum);
        printf("shading_default %.2f %04x\n", minimum, glGetError());
        glMinSampleShadingARB(0.625f);
        glGetFloatv(GL_MIN_SAMPLE_SHADING_VALUE, &minimum);
        printf("shading_set %.3f %04x\n", minimum, glGetError());
        glMinSampleShading(-2.0f);
        glGetFloatv(GL_MIN_SAMPLE_SHADING_VALUE, &minimum);
        printf("shading_lower %.2f %04x\n", minimum, glGetError());
        glMinSampleShading(2.0f);
        glGetFloatv(GL_MIN_SAMPLE_SHADING_VALUE, &minimum);
        printf("shading_upper %.2f %04x\n", minimum, glGetError());
        glEnable(GL_SAMPLE_SHADING);
        printf("shading_enabled %d %04x\n", glIsEnabled(GL_SAMPLE_SHADING), glGetError());
        glDisable(GL_SAMPLE_SHADING);
        printf("shading_disabled %d %04x\n", glIsEnabled(GL_SAMPLE_SHADING), glGetError());
        glPolygonOffsetClampEXT(2.0f, 3.0f, 0.125f);
        glGetFloatv(GL_POLYGON_OFFSET_CLAMP, &clamp);
        glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &factor);
        glGetFloatv(GL_POLYGON_OFFSET_UNITS, &units);
        printf("offset_clamp %.3f %.1f %.1f %04x\n", clamp, factor, units, glGetError());
        glPolygonOffsetClamp(-4.0f, 5.0f, -0.25f);
        glGetFloatv(GL_POLYGON_OFFSET_CLAMP, &clamp);
        printf("offset_clamp_negative %.2f %04x\n", clamp, glGetError());
        glPolygonOffset(1.0f, 2.0f);
        glGetFloatv(GL_POLYGON_OFFSET_CLAMP, &clamp);
        printf("offset_clamp_after_regular %.2f %04x\n", clamp, glGetError());
    }
    {
        GLint samples = -1;
        GLfloat xy[2] = {-3.0f, -4.0f};
        glGetIntegerv(GL_SAMPLES, &samples);
        printf("sample_position_samples %d %04x\n", samples, glGetError());
        glGetMultisamplefv(GL_SAMPLE_POSITION, 0, xy);
        printf("sample_position_0 %.1f %.1f %04x\n", xy[0], xy[1], glGetError());
        glGetMultisamplefv(0xdead, 0, xy);
        printf("sample_position_bad_name %04x\n", glGetError());
    }
    {
        const GLfloat vp[8] = {1.f, 2.f, 30.f, 40.f, 5.f, 6.f, 70.f, 80.f};
        const GLint sc[8] = {3, 4, 25, 35, 7, 8, 60, 50};
        const GLdouble dr[4] = {0.25, 0.75, -1.0, 2.0};
        GLint viewport[4] = {0}, scissor[4] = {0};
        GLdouble depths[2] = {0};
        GLint max_viewports = 0;
        glGetIntegerv(GL_MAX_VIEWPORTS, &max_viewports);
        printf("viewport_max %d %04x\n", max_viewports, glGetError());
        glViewportArrayv(0, 2, vp);
        glScissorArrayv(0, 2, sc);
        glDepthRangeArrayv(0, 2, dr);
        printf("viewport_arrays %04x\n", glGetError());
        glGetIntegeri_v(GL_VIEWPORT, 1, viewport);
        glGetIntegeri_v(GL_SCISSOR_BOX, 1, scissor);
        glGetDoublei_v(GL_DEPTH_RANGE, 1, depths);
        printf("viewport_index1 %d %d %d %d %d %d %.2f %.2f %04x\n", viewport[0], viewport[1],
               viewport[2], viewport[3], scissor[0], scissor[2], depths[0], depths[1],
               glGetError());
        glViewportIndexedf(1, 8.f, 9.f, 20.f, 21.f);
        glScissorIndexed(1, 4, 5, 8, 9);
        glDepthRangeIndexed(1, 0.125, 0.5);
        glGetIntegeri_v(GL_VIEWPORT, 1, viewport);
        glGetDoublei_v(GL_DEPTH_RANGE, 1, depths);
        printf("viewport_indexed %d %d %d %d %.3f %.3f %04x\n", viewport[0], viewport[1],
               viewport[2], viewport[3], depths[0], depths[1], glGetError());
        glViewportIndexedfv(1, vp);
        glScissorIndexedv(1, sc);
        glGetIntegeri_v(GL_SCISSOR_BOX, 1, scissor);
        printf("viewport_indexed_vectors %d %d %d %04x\n", scissor[0], scissor[1], scissor[2],
               glGetError());
        {
            GLfloat floats[4] = {0};
            GLdouble double_depth[2] = {0};
            GLboolean boolean_depth[2] = {0};
            glGetFloati_v(GL_VIEWPORT, 1, floats);
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
            glGetDoublei_v(GL_DEPTH_RANGE, 1, double_depth);
#else
            glGetDoubleIndexedv(GL_DEPTH_RANGE, 1, double_depth);
#endif
            glGetBooleani_v(GL_DEPTH_RANGE, 1, boolean_depth);
            printf("viewport_typed_queries %.1f %.1f %.3f %.3f %d %d %04x\n", floats[0], floats[2],
                   double_depth[0], double_depth[1], boolean_depth[0], boolean_depth[1],
                   glGetError());
        }
        {
            GLfloat viewport0[4] = {0};
            glViewportIndexedf(0, 2.25f, 3.5f, 4.5f, 5.5f);
            glGetFloatv(GL_VIEWPORT, viewport0);
            printf("viewport_subpixels %.2f %.2f %.2f %.2f %04x\n", viewport0[0], viewport0[1],
                   viewport0[2], viewport0[3], glGetError());
        }
        glViewportArrayv(1, 1, (const GLfloat[]){0.f, 0.f, -1.f, 1.f});
        printf("viewport_bad_size %04x\n", glGetError());
        glScissorIndexed(1, 0, 0, -1, 1);
        printf("scissor_bad_size %04x\n", glGetError());
        glViewportIndexedf((GLuint)max_viewports, 1.f, 1.f, 1.f, 1.f);
        printf("viewport_bad_index %04x\n", glGetError());
        glGetDoublei_v(GL_DEPTH_RANGE, (GLuint)max_viewports, depths);
        printf("depth_bad_index %04x\n", glGetError());
    }
    {
        GLint mode[2] = {-1, -1};
        glGetIntegerv(GL_POLYGON_MODE, mode);
        printf("polygon_default %04x %04x %04x\n", mode[0], mode[1], glGetError());
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glGetIntegerv(GL_POLYGON_MODE, mode);
        printf("polygon_line %04x %04x %04x\n", mode[0], mode[1], glGetError());
        {
            GLfloat fmodes[2] = {0};
            GLdouble dmodes[2] = {0};
            glGetFloatv(GL_POLYGON_MODE, fmodes);
            glGetDoublev(GL_POLYGON_MODE, dmodes);
            printf("polygon_typed %.0f %.0f %.0f %.0f %04x\n", fmodes[0], fmodes[1], dmodes[0],
                   dmodes[1], glGetError());
        }
        glPushAttrib(GL_POLYGON_BIT);
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glPopAttrib();
        glGetIntegerv(GL_POLYGON_MODE, mode);
        printf("polygon_pop %04x %04x %04x\n", mode[0], mode[1], glGetError());
        glPolygonMode(0xdead, GL_FILL);
        printf("polygon_bad_face %04x\n", glGetError());
        glPolygonMode(GL_FRONT_AND_BACK, 0xdead);
        printf("polygon_bad_mode %04x\n", glGetError());
        glBegin(GL_POINTS);
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glEnd();
        printf("polygon_begin %04x\n", glGetError());
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glGetIntegerv(GL_POLYGON_MODE, mode);
        printf("polygon_fill %04x %04x %04x\n", mode[0], mode[1], glGetError());
    }
    {
        GLuint queries[3] = {0};
        GLint current = -1;
        glGenQueries(3, queries);
        glBeginQueryIndexed(GL_PRIMITIVES_GENERATED, 0, queries[0]);
        glBeginQueryIndexed(GL_PRIMITIVES_GENERATED, 1, queries[1]);
        glGetQueryIndexediv(GL_PRIMITIVES_GENERATED, 1, GL_CURRENT_QUERY, &current);
        printf("stream_queries_active %d %04x\n", current == (GLint)queries[1], glGetError());
        glEndQuery(GL_PRIMITIVES_GENERATED);
        glGetQueryIndexediv(GL_PRIMITIVES_GENERATED, 0, GL_CURRENT_QUERY, &current);
        printf("stream_query_end0 %d %04x\n", current, glGetError());
        glEndQueryIndexed(GL_PRIMITIVES_GENERATED, 1);
        glGetQueryIndexediv(GL_PRIMITIVES_GENERATED, 1, GL_CURRENT_QUERY, &current);
        printf("stream_query_end1 %d %04x\n", current, glGetError());
        glBeginQueryIndexed(GL_SAMPLES_PASSED, 1, queries[2]);
        printf("stream_nonstream_index %04x\n", glGetError());
        glBeginQueryIndexed(GL_PRIMITIVES_GENERATED, 999, queries[2]);
        printf("stream_bad_index %04x\n", glGetError());
        glEndQueryIndexed(GL_PRIMITIVES_GENERATED, 1);
        printf("stream_inactive_end %04x\n", glGetError());
        glGetQueryIndexediv(GL_PRIMITIVES_GENERATED, 999, GL_CURRENT_QUERY, &current);
        printf("stream_invalid_get %04x\n", glGetError());
        glDeleteQueries(3, queries);
    }
    {
        GLint64 modes[2] = {0};
        GLint64 count = -1, timeout = -1;
        GLint64 invalid_value = 12345;
        glGetInteger64v(GL_POLYGON_MODE, modes);
        printf("query64_polygon %04llx %04llx %04x\n", (long long)modes[0], (long long)modes[1],
               glGetError());
        glGetInteger64v(GL_MAX_SERVER_WAIT_TIMEOUT, &timeout);
        printf("query64_timeout %d %04x\n", timeout > 2147483647LL, glGetError());
        glGetInteger64v(0xdeadu, &invalid_value);
        printf("query64_invalid %lld %04x\n", (long long)invalid_value, glGetError());
        glGetInternalformati64v(GL_TEXTURE_2D, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 1, &count);
        printf("format64_2d_count %lld %04x\n", (long long)count, glGetError());
        glGetInternalformati64v(GL_RENDERBUFFER, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 1, &count);
        printf("format64_renderbuffer %d %04x\n", count > 0, glGetError());
        glGetInternalformati64v(0xdeadu, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 1, &count);
        printf("format64_bad_target %04x\n", glGetError());
        glGetInternalformati64v(GL_RENDERBUFFER, GL_RGBA8, 0xdeadu, 1, &count);
        printf("format64_bad_name %04x\n", glGetError());
    }
    {
        GLint value = -37;
        GLuint program = glCreateProgram();
        glGetActiveAtomicCounterBufferiv(program, 0, GL_ATOMIC_COUNTER_BUFFER_BINDING, &value);
        printf("atomic_unlinked %d %04x\n", value, glGetError());
        glGetActiveAtomicCounterBufferiv(0, 0, GL_ATOMIC_COUNTER_BUFFER_BINDING, &value);
        printf("atomic_bad_program %d %04x\n", value, glGetError());
        glDeleteProgram(program);
        value = glGetFragDataLocationEXT(0, "col");
        printf("frag_location_ext_bad %d %04x\n", value, glGetError());
        program = legacy_atomic_program();
        if (program) {
            GLint binding = -1, bytes = -1, active = -1, vertex = -1, fragment = -1, indices = -1;
            glGetActiveAtomicCounterBufferiv(program, 0, GL_ATOMIC_COUNTER_BUFFER_BINDING,
                                             &binding);
            glGetActiveAtomicCounterBufferiv(program, 0, GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE,
                                             &bytes);
            glGetActiveAtomicCounterBufferiv(
                program, 0, GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTERS, &active);
            glGetActiveAtomicCounterBufferiv(
                program, 0, GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER, &vertex);
            glGetActiveAtomicCounterBufferiv(
                program, 0, GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_FRAGMENT_SHADER, &fragment);
            glGetActiveAtomicCounterBufferiv(
                program, 0, GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTER_INDICES, &indices);
            printf("atomic_linked %d %d %d %d %d %d %04x\n", binding, bytes, active, vertex,
                   fragment, indices, glGetError());
            glGetActiveAtomicCounterBufferiv(program, 1, GL_ATOMIC_COUNTER_BUFFER_BINDING, &value);
            printf("atomic_bad_index %d %04x\n", value, glGetError());
            glGetActiveAtomicCounterBufferiv(program, 0, 0xdeadu, &value);
            printf("atomic_bad_pname %d %04x\n", value, glGetError());
            glDeleteProgram(program);
        }
    }
    {
        GLint vertex = -1, fragment = -1, read = -1;
        glGetIntegerv(GL_CLAMP_VERTEX_COLOR, &vertex);
        glGetIntegerv(GL_CLAMP_FRAGMENT_COLOR, &fragment);
        glGetIntegerv(GL_CLAMP_READ_COLOR, &read);
        printf("clamp_defaults %d %d %d %04x\n", vertex, fragment, read, glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
#else
        glClampColorARB(GL_CLAMP_READ_COLOR, GL_FALSE);
#endif
        glGetIntegerv(GL_CLAMP_READ_COLOR, &read);
        printf("clamp_set %d %04x\n", read, glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glClampColor(0xdeadu, GL_TRUE);
#else
        glClampColorARB(0xdeadu, GL_TRUE);
#endif
        printf("clamp_bad_target %04x\n", glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glClampColor(GL_CLAMP_VERTEX_COLOR, 0xdeadu);
#else
        glClampColorARB(GL_CLAMP_VERTEX_COLOR, 0xdeadu);
#endif
        printf("clamp_bad_mode %04x\n", glGetError());
        glBegin(GL_POINTS);
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glClampColor(GL_CLAMP_READ_COLOR, GL_TRUE);
#else
        glClampColorARB(GL_CLAMP_READ_COLOR, GL_TRUE);
#endif
        glEnd();
        printf("clamp_in_begin %04x\n", glGetError());
        glPushAttrib(GL_COLOR_BUFFER_BIT);
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glClampColor(GL_CLAMP_READ_COLOR, GL_TRUE);
        glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);
#else
        glClampColorARB(GL_CLAMP_READ_COLOR, GL_TRUE);
        glClampColorARB(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);
#endif
        glPopAttrib();
        glGetIntegerv(GL_CLAMP_READ_COLOR, &read);
        glGetIntegerv(GL_CLAMP_FRAGMENT_COLOR, &fragment);
        printf("clamp_color_stack %d %d %04x\n", read, fragment, glGetError());
        glPushAttrib(GL_PIXEL_MODE_BIT);
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glClampColor(GL_CLAMP_READ_COLOR, GL_TRUE);
#else
        glClampColorARB(GL_CLAMP_READ_COLOR, GL_TRUE);
#endif
        glPopAttrib();
        glGetIntegerv(GL_CLAMP_READ_COLOR, &read);
        printf("clamp_pixel_stack %d %04x\n", read, glGetError());
        glPushAttrib(GL_LIGHTING_BIT);
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glClampColor(GL_CLAMP_VERTEX_COLOR, GL_FALSE);
#else
        glClampColorARB(GL_CLAMP_VERTEX_COLOR, GL_FALSE);
#endif
        glPopAttrib();
        glGetIntegerv(GL_CLAMP_VERTEX_COLOR, &vertex);
        printf("clamp_lighting_stack %d %04x\n", vertex, glGetError());
    }
    {
        const GLushort half2[2] = {0x3800, 0xb400};
        const GLushort half4[4] = {0x0001, 0x7c00, 0x8000, 0x3c00};
        GLfloat value[4] = {0};
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glVertexAttrib2f(3, 0.5f, -0.25f);
#else
        glVertexAttrib2hvNV(3, half2);
#endif
        glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, value);
        printf("half2 %.9g %.9g %.9g %.9g %04x\n", value[0], value[1], value[2], value[3],
               glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glVertexAttrib4f(3, 0x1p-24f, 1.0f / 0.0f, -0.0f, 1.0f);
#else
        glVertexAttrib4hvNV(3, half4);
#endif
        glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, value);
        printf("half4 %.9g %.9g %.9g %.9g %04x\n", value[0], value[1], value[2], value[3],
               glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glVertexAttrib2f(999, 0.5f, -0.25f);
#else
        glVertexAttrib2hvNV(999, half2);
#endif
        printf("half_bad_index %04x\n", glGetError());
    }
    {
        GLfloat inner[2] = {0}, outer[4] = {0};
        const GLfloat new_inner[2] = {1.5f, 2.5f}, new_outer[4] = {2.0f, 3.0f, 4.0f, 5.0f};
        glGetFloatv(GL_PATCH_DEFAULT_INNER_LEVEL, inner);
        glGetFloatv(GL_PATCH_DEFAULT_OUTER_LEVEL, outer);
        printf("patch_levels_default %.1f %.1f %.1f %.1f %04x\n", inner[0], inner[1], outer[0],
               outer[3], glGetError());
        glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, new_inner);
        glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, new_outer);
        glGetFloatv(GL_PATCH_DEFAULT_INNER_LEVEL, inner);
        glGetFloatv(GL_PATCH_DEFAULT_OUTER_LEVEL, outer);
        printf("patch_levels_set %.1f %.1f %.1f %.1f %04x\n", inner[0], inner[1], outer[0],
               outer[3], glGetError());
        glPatchParameterfv(0xdead, new_inner);
        printf("patch_levels_bad_name %04x\n", glGetError());
        {
            GLint value[4] = {0};
            glGetIntegerv(GL_PATCH_DEFAULT_OUTER_LEVEL, value);
            printf("patch_levels_int %d %d %04x\n", value[0], value[3], glGetError());
        }
    }

    {
        const GLushort h[4] = {0x3800u, 0xb400u, 0x0001u, 0x8000u};
        GLfloat value[4] = {0};
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glColor3f(0.5f, -0.25f, 0x1p-24f);
#else
        glColor3hvNV(h);
#endif
        glGetFloatv(GL_CURRENT_COLOR, value);
        printf("half_color3 %.9g %.9g %.9g %.9g %04x\n", value[0], value[1], value[2], value[3],
               glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glColor4f(0.5f, -0.25f, 0x1p-24f, -0.0f);
#else
        glColor4hvNV(h);
#endif
        glGetFloatv(GL_CURRENT_COLOR, value);
        printf("half_color4 %.9g %.9g %.9g %.9g %04x\n", value[0], value[1], value[2], value[3],
               glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glNormal3f(0.5f, -0.25f, 0x1p-24f);
#else
        glNormal3hvNV(h);
#endif
        glGetFloatv(GL_CURRENT_NORMAL, value);
        printf("half_normal %.9g %.9g %.9g %04x\n", value[0], value[1], value[2], glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glSecondaryColor3f(0.5f, -0.25f, 0x1p-24f);
#else
        glSecondaryColor3hvNV(h);
#endif
        glGetFloatv(GL_CURRENT_SECONDARY_COLOR, value);
        printf("half_secondary %.9g %.9g %.9g %04x\n", value[0], value[1], value[2], glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glFogCoordf(0x1p-24f);
#else
        glFogCoordhNV(h[2]);
#endif
        glGetFloatv(GL_CURRENT_FOG_COORD, value);
        printf("half_fog %.9g %04x\n", value[0], glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glTexCoord4f(0.5f, -0.25f, 0x1p-24f, -0.0f);
#else
        glTexCoord4hvNV(h);
#endif
        glGetFloatv(GL_CURRENT_TEXTURE_COORDS, value);
        printf("half_tex4 %.9g %.9g %.9g %.9g %04x\n", value[0], value[1], value[2], value[3],
               glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glMultiTexCoord3f(GL_TEXTURE0, 0.5f, -0.25f, 0x1p-24f);
#else
        glMultiTexCoord3hvNV(GL_TEXTURE0, h);
#endif
        glGetFloatv(GL_CURRENT_TEXTURE_COORDS, value);
        printf("half_multi3 %.9g %.9g %.9g %.9g %04x\n", value[0], value[1], value[2], value[3],
               glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glVertexAttrib1f(3, 0.5f);
#else
        glVertexAttrib1hNV(3, h[0]);
#endif
        glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, value);
        printf("half_attrib1 %.9g %.9g %.9g %.9g %04x\n", value[0], value[1], value[2], value[3],
               glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glVertexAttrib3f(3, 0.5f, -0.25f, 0x1p-24f);
#else
        glVertexAttrib3hvNV(3, h);
#endif
        glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, value);
        printf("half_attrib3 %.9g %.9g %.9g %.9g %04x\n", value[0], value[1], value[2], value[3],
               glGetError());

#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glVertex3f(0.5f, -0.25f, 0x1p-24f);
#else
        glVertex3hvNV(h);
#endif
        printf("half_vertex_outside3 %04x\n", glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glVertex2f(0.5f, -0.25f);
#else
        glVertex2hNV(h[0], h[1]);
#endif
        printf("half_vertex_outside %04x\n", glGetError());
#ifdef MGL_CORE_ENTRYPOINT_REFERENCE
        glVertexAttrib3f(999, 0.5f, -0.25f, 0x1p-24f);
#else
        glVertexAttrib3hvNV(999, h);
#endif
        printf("half_attrib_bad_index %04x\n", glGetError());
    }
}
#endif

#include "native/dispatch.h"
#include "core/context.h"
#include "native/gles_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const egl_candidates[] = {
#if defined(__ANDROID__) && defined(__LP64__)
    "/system/lib64/libEGL.so",
#elif defined(__ANDROID__)
    "/system/lib/libEGL.so",
#endif
    "libEGL.so", "libEGL.so.1"};

static const char *const gles_candidates[] = {
#if defined(__ANDROID__) && defined(__LP64__)
    "/system/lib64/libGLESv3.so", "/system/lib64/libGLESv2.so",
#elif defined(__ANDROID__)
    "/system/lib/libGLESv3.so", "/system/lib/libGLESv2.so",
#endif
    "libGLESv2.so", "libGLESv2.so.2"};

static int load_egl(struct mgl_native_dispatch *b) {
#define LOAD_EGL(member, symbol)                                                                   \
    do {                                                                                           \
        *(void **)(&b->member) = mgl_library_symbol(&b->egl_lib, symbol);                          \
        if (b->member == NULL)                                                                     \
            return 0;                                                                              \
    } while (0)
    LOAD_EGL(eglGetDisplay, "eglGetDisplay");
    LOAD_EGL(eglInitialize, "eglInitialize");
    LOAD_EGL(eglTerminate, "eglTerminate");
    LOAD_EGL(eglChooseConfig, "eglChooseConfig");
    LOAD_EGL(eglBindAPI, "eglBindAPI");
    LOAD_EGL(eglCreateContext, "eglCreateContext");
    LOAD_EGL(eglDestroyContext, "eglDestroyContext");
    LOAD_EGL(eglCreatePbufferSurface, "eglCreatePbufferSurface");
    *(void **)(&b->eglCreateWindowSurface) =
        mgl_library_symbol(&b->egl_lib, "eglCreateWindowSurface");
    LOAD_EGL(eglDestroySurface, "eglDestroySurface");
    LOAD_EGL(eglMakeCurrent, "eglMakeCurrent");
    LOAD_EGL(eglSwapBuffers, "eglSwapBuffers");
    LOAD_EGL(eglSwapInterval, "eglSwapInterval");
    LOAD_EGL(eglGetError, "eglGetError");
    LOAD_EGL(eglQueryString, "eglQueryString");
    LOAD_EGL(eglGetProcAddress, "eglGetProcAddress");
#undef LOAD_EGL
    return 1;
}

int mgl_native_dispatch_has_gles_extension(const struct mgl_native_dispatch *native,
                                           const char *name) {
    const GLubyte *extensions;
    const char *p;
    size_t name_len;
    if (native == NULL || native->glesGetString == NULL || name == NULL || *name == '\0')
        return 0;
    if (native->glesGetStringi != NULL && native->glesGetIntegerv != NULL) {
        GLint count = 0;
        native->glesGetIntegerv(GL_NUM_EXTENSIONS, &count);
        if (count >= 0 && count <= 65536) {
            GLint i;
            for (i = 0; i < count; ++i) {
                const GLubyte *extension = native->glesGetStringi(GL_EXTENSIONS, (GLuint)i);
                if (extension != NULL && strcmp((const char *)extension, name) == 0)
                    return 1;
            }
            return 0;
        }
    }
    extensions = native->glesGetString(GL_EXTENSIONS);
    if (extensions == NULL)
        return 0;
    name_len = strlen(name);
    p = (const char *)extensions;
    while ((p = strstr(p, name)) != NULL) {
        const char before = p == (const char *)extensions ? ' ' : p[-1];
        const char after = p[name_len];
        if ((before == ' ' || before == '\t' || before == '\r' || before == '\n') &&
            (after == '\0' || after == ' ' || after == '\t' || after == '\r' || after == '\n'))
            return 1;
        p += name_len;
    }
    return 0;
}

void *mgl_native_dispatch_resolve_gles(struct mgl_native_dispatch *native, const char *name) {
    void *p;
    if (native == NULL || name == NULL)
        return NULL;
    p = mgl_library_symbol(&native->gles_lib, name);
    if (p == NULL && native->eglGetProcAddress != NULL) {
        mgl_egl_proc_t proc = native->eglGetProcAddress(name);
        memset(&p, 0, sizeof(p));
        memcpy(&p, &proc, sizeof(p) < sizeof(proc) ? sizeof(p) : sizeof(proc));
    }
    return p;
}

/* Canonical framebuffer zero maps to the GLES backing FBO, including during internal binding
 * restoration. */
static struct mgl_native_dispatch *mgl_bind_native;

static const struct mgl_native_dispatch *mgl_trace_current_raw(struct mgl_context *ctx) {
    struct mgl_native_dispatch *native = ctx != NULL ? ctx->native_dispatch : mgl_bind_native;
    return native != NULL ? native->trace_raw : NULL;
}

static GLenum mgl_trace_glesGetError(void) {
    struct mgl_context *ctx = mgl_context_current();
    return mgl_gles_trace_get_error(ctx, mgl_trace_current_raw(ctx));
}

static void mgl_bind_default_framebuffer(GLenum target, GLuint name) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_native_dispatch *native;
    native = mgl_bind_native;
    if (ctx != NULL && ctx->native_dispatch != NULL)
        native = ctx->native_dispatch;
    if (native == NULL || native->glesBindFramebufferRaw == NULL)
        return;
    if (name == 0u && ctx != NULL && ctx->native_default_framebuffer != 0u) {
        GLenum color;
        name = ctx->native_default_framebuffer;
        native->glesBindFramebufferRaw(target, name);
        if ((target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) &&
            native->glesDrawBuffers) {
            color = ctx->default_draw_buffers[0] == GL_NONE ? GL_NONE : GL_COLOR_ATTACHMENT0;
            native->glesDrawBuffers(1, &color);
        }
        if ((target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER) && native->glesReadBuffer) {
            color = ctx->default_read_buffer == GL_NONE ? GL_NONE : GL_COLOR_ATTACHMENT0;
            native->glesReadBuffer(color);
        }
        return;
    }
    native->glesBindFramebufferRaw(target, name);
}

static int load_gles(struct mgl_native_dispatch *b) {
#define LOAD_REQUIRED(member, symbol)                                                              \
    do {                                                                                           \
        *(void **)(&b->member) = mgl_native_dispatch_resolve_gles(b, symbol);                      \
        if (b->member == NULL) {                                                                   \
            fprintf(stderr, "ManifestGL: required GLES entry point missing: %s\n", symbol);        \
            return 0;                                                                              \
        }                                                                                          \
    } while (0)
#define LOAD_OPTIONAL(member, symbol)                                                              \
    do {                                                                                           \
        *(void **)(&b->member) = mgl_native_dispatch_resolve_gles(b, symbol);                      \
    } while (0)
    LOAD_REQUIRED(glesGetString, "glGetString");
    LOAD_REQUIRED(glesGetStringi, "glGetStringi");
    LOAD_REQUIRED(glesGetError, "glGetError");
    LOAD_REQUIRED(glesGetGraphicsResetStatus, "glGetGraphicsResetStatus");
    LOAD_REQUIRED(glesGetIntegerv, "glGetIntegerv");
    LOAD_REQUIRED(glesGetTexParameteriv, "glGetTexParameteriv");
    LOAD_REQUIRED(glesGetSamplerParameteriv, "glGetSamplerParameteriv");
    LOAD_REQUIRED(glesGetFloatv, "glGetFloatv");
    LOAD_REQUIRED(glesGetInternalformativ, "glGetInternalformativ");
    LOAD_REQUIRED(glesGetFramebufferAttachmentParameteriv, "glGetFramebufferAttachmentParameteriv");
    LOAD_REQUIRED(glesGetInteger64v, "glGetInteger64v");
    LOAD_REQUIRED(glesGetIntegeri_v, "glGetIntegeri_v");
    LOAD_OPTIONAL(glesBeginQueryIndexed, "glBeginQueryIndexed");
    if (b->glesBeginQueryIndexed == NULL)
        LOAD_OPTIONAL(glesBeginQueryIndexed, "glBeginQueryIndexedEXT");
    LOAD_OPTIONAL(glesEndQueryIndexed, "glEndQueryIndexed");
    if (b->glesEndQueryIndexed == NULL)
        LOAD_OPTIONAL(glesEndQueryIndexed, "glEndQueryIndexedEXT");
    LOAD_REQUIRED(glesGetMultisamplefv, "glGetMultisamplefv");
    LOAD_OPTIONAL(glesLogicOpANGLE, "glLogicOpANGLE");
    LOAD_REQUIRED(glesEnable, "glEnable");
    LOAD_REQUIRED(glesDisable, "glDisable");
    LOAD_REQUIRED(glesEnablei, "glEnablei");
    LOAD_REQUIRED(glesDisablei, "glDisablei");
    LOAD_REQUIRED(glesClearColor, "glClearColor");
    LOAD_REQUIRED(glesClearDepthf, "glClearDepthf");
    LOAD_REQUIRED(glesClearStencil, "glClearStencil");
    LOAD_REQUIRED(glesClear, "glClear");
    LOAD_REQUIRED(glesClearBufferiv, "glClearBufferiv");
    LOAD_REQUIRED(glesClearBufferuiv, "glClearBufferuiv");
    LOAD_REQUIRED(glesClearBufferfv, "glClearBufferfv");
    LOAD_REQUIRED(glesClearBufferfi, "glClearBufferfi");
    LOAD_REQUIRED(glesViewport, "glViewport");
    LOAD_OPTIONAL(glesViewportArrayvOES, "glViewportArrayvOES");
    LOAD_OPTIONAL(glesScissorArrayvOES, "glScissorArrayvOES");
    LOAD_OPTIONAL(glesDepthRangeArrayfvOES, "glDepthRangeArrayfvOES");
    LOAD_OPTIONAL(glesClipControlEXT, "glClipControlEXT");
    LOAD_OPTIONAL(glesDepthBoundsEXT, "glDepthBoundsEXT");
    LOAD_REQUIRED(glesPatchParameteri, "glPatchParameteri");
    LOAD_OPTIONAL(glesPatchParameterfv, "glPatchParameterfv");
    LOAD_REQUIRED(glesDepthRangef, "glDepthRangef");
    LOAD_REQUIRED(glesBlendColor, "glBlendColor");
    LOAD_REQUIRED(glesLineWidth, "glLineWidth");
    LOAD_OPTIONAL(glesPolygonModeNV, "glPolygonModeNV");
    LOAD_REQUIRED(glesPolygonOffset, "glPolygonOffset");
    LOAD_OPTIONAL(glesPolygonOffsetClamp, "glPolygonOffsetClamp");
    if (b->glesPolygonOffsetClamp == NULL)
        LOAD_OPTIONAL(glesPolygonOffsetClamp, "glPolygonOffsetClampEXT");
    LOAD_REQUIRED(glesMinSampleShading, "glMinSampleShading");
    LOAD_REQUIRED(glesSampleCoverage, "glSampleCoverage");
    LOAD_REQUIRED(glesSampleMaski, "glSampleMaski");
    LOAD_REQUIRED(glesScissor, "glScissor");
    LOAD_REQUIRED(glesDepthFunc, "glDepthFunc");
    LOAD_REQUIRED(glesDepthMask, "glDepthMask");
    LOAD_REQUIRED(glesStencilFuncSeparate, "glStencilFuncSeparate");
    LOAD_REQUIRED(glesStencilMaskSeparate, "glStencilMaskSeparate");
    LOAD_REQUIRED(glesStencilOpSeparate, "glStencilOpSeparate");
    LOAD_REQUIRED(glesColorMask, "glColorMask");
    LOAD_REQUIRED(glesColorMaski, "glColorMaski");
    LOAD_REQUIRED(glesCullFace, "glCullFace");
    LOAD_REQUIRED(glesFrontFace, "glFrontFace");
    LOAD_REQUIRED(glesBlendFunc, "glBlendFunc");
    LOAD_REQUIRED(glesBlendFuncSeparate, "glBlendFuncSeparate");
    LOAD_REQUIRED(glesBlendFuncSeparatei, "glBlendFuncSeparatei");
    LOAD_REQUIRED(glesBlendBarrier, "glBlendBarrier");
    LOAD_REQUIRED(glesPrimitiveBoundingBox, "glPrimitiveBoundingBox");
    LOAD_REQUIRED(glesBlendEquationi, "glBlendEquationi");
    LOAD_REQUIRED(glesBlendEquation, "glBlendEquation");
    LOAD_REQUIRED(glesBlendEquationSeparate, "glBlendEquationSeparate");
    LOAD_REQUIRED(glesBlendEquationSeparatei, "glBlendEquationSeparatei");
    LOAD_REQUIRED(glesFlush, "glFlush");
    LOAD_REQUIRED(glesFinish, "glFinish");
    LOAD_REQUIRED(glesReadPixels, "glReadPixels");
    LOAD_REQUIRED(glesActiveTexture, "glActiveTexture");
    LOAD_REQUIRED(glesGenTextures, "glGenTextures");
    LOAD_REQUIRED(glesDeleteTextures, "glDeleteTextures");
    LOAD_REQUIRED(glesBindTexture, "glBindTexture");
    LOAD_REQUIRED(glesTexImage2D, "glTexImage2D");
    LOAD_REQUIRED(glesCopyTexImage2D, "glCopyTexImage2D");
    LOAD_REQUIRED(glesCopyTexSubImage2D, "glCopyTexSubImage2D");
    LOAD_REQUIRED(glesTexImage3D, "glTexImage3D");
    LOAD_REQUIRED(glesTexStorage2D, "glTexStorage2D");
    LOAD_OPTIONAL(glesTextureView, "glTextureViewEXT");
    if (b->glesTextureView == NULL)
        LOAD_OPTIONAL(glesTextureView, "glTextureViewOES");
    LOAD_REQUIRED(glesTexStorage3D, "glTexStorage3D");
    LOAD_REQUIRED(glesTexStorage2DMultisample, "glTexStorage2DMultisample");
    LOAD_REQUIRED(glesTexStorage3DMultisample, "glTexStorage3DMultisample");
    LOAD_REQUIRED(glesTexSubImage2D, "glTexSubImage2D");
    LOAD_REQUIRED(glesTexSubImage3D, "glTexSubImage3D");
    LOAD_REQUIRED(glesCompressedTexImage2D, "glCompressedTexImage2D");
    LOAD_REQUIRED(glesCompressedTexImage3D, "glCompressedTexImage3D");
    LOAD_REQUIRED(glesCompressedTexSubImage2D, "glCompressedTexSubImage2D");
    LOAD_REQUIRED(glesCompressedTexSubImage3D, "glCompressedTexSubImage3D");
    LOAD_REQUIRED(glesTexBufferEXT, "glTexBuffer");
    LOAD_REQUIRED(glesTexBufferRangeEXT, "glTexBufferRange");
    LOAD_REQUIRED(glesTexParameteri, "glTexParameteri");
    LOAD_REQUIRED(glesTexParameterf, "glTexParameterf");
    LOAD_REQUIRED(glesTexParameterfv, "glTexParameterfv");
    LOAD_REQUIRED(glesTexParameterIiv, "glTexParameterIiv");
    LOAD_REQUIRED(glesTexParameterIuiv, "glTexParameterIuiv");
    LOAD_REQUIRED(glesPixelStorei, "glPixelStorei");
    LOAD_REQUIRED(glesGenerateMipmap, "glGenerateMipmap");
    LOAD_REQUIRED(glesCopyImageSubData, "glCopyImageSubData");
    LOAD_OPTIONAL(glesTextureBarrier, "glTextureBarrier");
    if (b->glesTextureBarrier == NULL)
        LOAD_OPTIONAL(glesTextureBarrier, "glTextureBarrierNV");
    LOAD_REQUIRED(glesGenSamplers, "glGenSamplers");
    LOAD_REQUIRED(glesDeleteSamplers, "glDeleteSamplers");
    LOAD_REQUIRED(glesBindSampler, "glBindSampler");
    LOAD_REQUIRED(glesSamplerParameteri, "glSamplerParameteri");
    LOAD_REQUIRED(glesSamplerParameterf, "glSamplerParameterf");
    LOAD_REQUIRED(glesSamplerParameterfv, "glSamplerParameterfv");
    LOAD_REQUIRED(glesSamplerParameteriv, "glSamplerParameteriv");
    LOAD_REQUIRED(glesSamplerParameterIiv, "glSamplerParameterIiv");
    LOAD_REQUIRED(glesSamplerParameterIuiv, "glSamplerParameterIuiv");

    LOAD_REQUIRED(glesGenBuffers, "glGenBuffers");
    LOAD_REQUIRED(glesDeleteBuffers, "glDeleteBuffers");
    LOAD_REQUIRED(glesBindBuffer, "glBindBuffer");
    LOAD_REQUIRED(glesBufferData, "glBufferData");
    LOAD_OPTIONAL(glesBufferStorageEXT, "glBufferStorageEXT");
    LOAD_REQUIRED(glesMapBufferRange, "glMapBufferRange");
    LOAD_REQUIRED(glesUnmapBuffer, "glUnmapBuffer");
    LOAD_REQUIRED(glesFlushMappedBufferRange, "glFlushMappedBufferRange");
    LOAD_REQUIRED(glesBufferSubData, "glBufferSubData");
    LOAD_REQUIRED(glesCopyBufferSubData, "glCopyBufferSubData");
    LOAD_REQUIRED(glesBindBufferBase, "glBindBufferBase");
    LOAD_REQUIRED(glesBindBufferRange, "glBindBufferRange");

    LOAD_REQUIRED(glesGenVertexArrays, "glGenVertexArrays");
    LOAD_REQUIRED(glesDeleteVertexArrays, "glDeleteVertexArrays");
    LOAD_REQUIRED(glesBindVertexArray, "glBindVertexArray");

    LOAD_REQUIRED(glesGenFramebuffers, "glGenFramebuffers");
    LOAD_REQUIRED(glesDeleteFramebuffers, "glDeleteFramebuffers");
    LOAD_REQUIRED(glesBindFramebuffer, "glBindFramebuffer");
    LOAD_REQUIRED(glesCheckFramebufferStatus, "glCheckFramebufferStatus");
    LOAD_REQUIRED(glesFramebufferRenderbuffer, "glFramebufferRenderbuffer");
    LOAD_REQUIRED(glesFramebufferTexture2D, "glFramebufferTexture2D");
    LOAD_REQUIRED(glesFramebufferTextureLayer, "glFramebufferTextureLayer");
    LOAD_REQUIRED(glesFramebufferTexture, "glFramebufferTexture");
    LOAD_REQUIRED(glesFramebufferParameteri, "glFramebufferParameteri");
    LOAD_REQUIRED(glesDrawBuffers, "glDrawBuffers");
    LOAD_REQUIRED(glesReadBuffer, "glReadBuffer");
    LOAD_REQUIRED(glesBlitFramebuffer, "glBlitFramebuffer");

    LOAD_REQUIRED(glesGenRenderbuffers, "glGenRenderbuffers");
    LOAD_REQUIRED(glesDeleteRenderbuffers, "glDeleteRenderbuffers");
    LOAD_REQUIRED(glesBindRenderbuffer, "glBindRenderbuffer");
    LOAD_REQUIRED(glesRenderbufferStorage, "glRenderbufferStorage");
    LOAD_REQUIRED(glesRenderbufferStorageMultisample, "glRenderbufferStorageMultisample");
    LOAD_REQUIRED(glesGetRenderbufferParameteriv, "glGetRenderbufferParameteriv");

    LOAD_REQUIRED(glesCreateShader, "glCreateShader");
    LOAD_REQUIRED(glesShaderSource, "glShaderSource");
    LOAD_REQUIRED(glesCompileShader, "glCompileShader");
    LOAD_REQUIRED(glesShaderBinary, "glShaderBinary");
    LOAD_REQUIRED(glesGetShaderPrecisionFormat, "glGetShaderPrecisionFormat");
    LOAD_REQUIRED(glesGetShaderiv, "glGetShaderiv");
    LOAD_REQUIRED(glesGetShaderInfoLog, "glGetShaderInfoLog");
    LOAD_REQUIRED(glesDeleteShader, "glDeleteShader");
    LOAD_REQUIRED(glesCreateProgram, "glCreateProgram");
    LOAD_REQUIRED(glesAttachShader, "glAttachShader");
    LOAD_REQUIRED(glesDetachShader, "glDetachShader");
    LOAD_REQUIRED(glesBindAttribLocation, "glBindAttribLocation");
    LOAD_OPTIONAL(glesBindFragDataLocationEXT, "glBindFragDataLocationEXT");
    LOAD_OPTIONAL(glesBindFragDataLocationIndexedEXT, "glBindFragDataLocationIndexedEXT");
    LOAD_REQUIRED(glesLinkProgram, "glLinkProgram");
    LOAD_REQUIRED(glesValidateProgram, "glValidateProgram");
    LOAD_REQUIRED(glesUseProgram, "glUseProgram");
    LOAD_REQUIRED(glesGenProgramPipelines, "glGenProgramPipelines");
    LOAD_REQUIRED(glesDeleteProgramPipelines, "glDeleteProgramPipelines");
    LOAD_REQUIRED(glesBindProgramPipeline, "glBindProgramPipeline");
    LOAD_REQUIRED(glesIsProgramPipeline, "glIsProgramPipeline");
    LOAD_REQUIRED(glesUseProgramStages, "glUseProgramStages");
    LOAD_REQUIRED(glesActiveShaderProgram, "glActiveShaderProgram");
    LOAD_REQUIRED(glesValidateProgramPipeline, "glValidateProgramPipeline");
    LOAD_REQUIRED(glesGetProgramPipelineiv, "glGetProgramPipelineiv");
    LOAD_REQUIRED(glesGetProgramPipelineInfoLog, "glGetProgramPipelineInfoLog");
    LOAD_REQUIRED(glesGetProgramiv, "glGetProgramiv");
    LOAD_REQUIRED(glesGetProgramInfoLog, "glGetProgramInfoLog");
    LOAD_REQUIRED(glesProgramParameteri, "glProgramParameteri");
    LOAD_REQUIRED(glesGetProgramBinary, "glGetProgramBinary");
    LOAD_REQUIRED(glesProgramBinary, "glProgramBinary");
    LOAD_REQUIRED(glesGetActiveAttrib, "glGetActiveAttrib");
    LOAD_REQUIRED(glesGetActiveUniform, "glGetActiveUniform");
    LOAD_REQUIRED(glesGetUniformIndices, "glGetUniformIndices");
    LOAD_REQUIRED(glesGetActiveUniformsiv, "glGetActiveUniformsiv");
    LOAD_REQUIRED(glesGetUniformBlockIndex, "glGetUniformBlockIndex");
    LOAD_REQUIRED(glesGetActiveUniformBlockiv, "glGetActiveUniformBlockiv");
    LOAD_REQUIRED(glesGetActiveUniformBlockName, "glGetActiveUniformBlockName");
    LOAD_REQUIRED(glesUniformBlockBinding, "glUniformBlockBinding");
    LOAD_OPTIONAL(glesShaderStorageBlockBinding, "glShaderStorageBlockBinding");
    LOAD_REQUIRED(glesGetProgramInterfaceiv, "glGetProgramInterfaceiv");
    LOAD_REQUIRED(glesGetProgramResourceIndex, "glGetProgramResourceIndex");
    LOAD_REQUIRED(glesGetProgramResourceName, "glGetProgramResourceName");
    LOAD_REQUIRED(glesGetProgramResourceiv, "glGetProgramResourceiv");
    LOAD_REQUIRED(glesGetProgramResourceLocation, "glGetProgramResourceLocation");
    LOAD_OPTIONAL(glesGetProgramResourceLocationIndex, "glGetProgramResourceLocationIndex");
    if (b->glesGetProgramResourceLocationIndex == NULL)
        LOAD_OPTIONAL(glesGetProgramResourceLocationIndex, "glGetProgramResourceLocationIndexEXT");
    LOAD_REQUIRED(glesDeleteProgram, "glDeleteProgram");
    LOAD_REQUIRED(glesGetAttribLocation, "glGetAttribLocation");
    LOAD_REQUIRED(glesGetUniformLocation, "glGetUniformLocation");
    LOAD_REQUIRED(glesGetFragDataLocation, "glGetFragDataLocation");
    LOAD_OPTIONAL(glesGetFragDataIndexEXT, "glGetFragDataIndexEXT");
    LOAD_REQUIRED(glesUniform1f, "glUniform1f");
    LOAD_REQUIRED(glesUniform2f, "glUniform2f");
    LOAD_REQUIRED(glesUniform3f, "glUniform3f");
    LOAD_REQUIRED(glesUniform4f, "glUniform4f");
    LOAD_REQUIRED(glesUniform1i, "glUniform1i");
    LOAD_REQUIRED(glesUniform2i, "glUniform2i");
    LOAD_REQUIRED(glesUniform3i, "glUniform3i");
    LOAD_REQUIRED(glesUniform4i, "glUniform4i");
    LOAD_REQUIRED(glesUniform1ui, "glUniform1ui");
    LOAD_REQUIRED(glesUniform2ui, "glUniform2ui");
    LOAD_REQUIRED(glesUniform3ui, "glUniform3ui");
    LOAD_REQUIRED(glesUniform4ui, "glUniform4ui");
    LOAD_REQUIRED(glesUniform1fv, "glUniform1fv");
    LOAD_REQUIRED(glesUniform2fv, "glUniform2fv");
    LOAD_REQUIRED(glesUniform3fv, "glUniform3fv");
    LOAD_REQUIRED(glesUniform4fv, "glUniform4fv");
    LOAD_REQUIRED(glesUniform1iv, "glUniform1iv");
    LOAD_REQUIRED(glesUniform2iv, "glUniform2iv");
    LOAD_REQUIRED(glesUniform3iv, "glUniform3iv");
    LOAD_REQUIRED(glesUniform4iv, "glUniform4iv");
    LOAD_REQUIRED(glesUniform1uiv, "glUniform1uiv");
    LOAD_REQUIRED(glesUniform2uiv, "glUniform2uiv");
    LOAD_REQUIRED(glesUniform3uiv, "glUniform3uiv");
    LOAD_REQUIRED(glesUniform4uiv, "glUniform4uiv");
    LOAD_REQUIRED(glesUniformMatrix2fv, "glUniformMatrix2fv");
    LOAD_REQUIRED(glesUniformMatrix3fv, "glUniformMatrix3fv");
    LOAD_REQUIRED(glesUniformMatrix4fv, "glUniformMatrix4fv");
    LOAD_REQUIRED(glesUniformMatrix2x3fv, "glUniformMatrix2x3fv");
    LOAD_REQUIRED(glesUniformMatrix3x2fv, "glUniformMatrix3x2fv");
    LOAD_REQUIRED(glesUniformMatrix2x4fv, "glUniformMatrix2x4fv");
    LOAD_REQUIRED(glesUniformMatrix4x2fv, "glUniformMatrix4x2fv");
    LOAD_REQUIRED(glesUniformMatrix3x4fv, "glUniformMatrix3x4fv");
    LOAD_REQUIRED(glesUniformMatrix4x3fv, "glUniformMatrix4x3fv");
    LOAD_REQUIRED(glesGetUniformfv, "glGetUniformfv");
    LOAD_REQUIRED(glesGetUniformiv, "glGetUniformiv");
    LOAD_REQUIRED(glesGetUniformuiv, "glGetUniformuiv");
    LOAD_REQUIRED(glesProgramUniform1f, "glProgramUniform1f");
    LOAD_REQUIRED(glesProgramUniform2f, "glProgramUniform2f");
    LOAD_REQUIRED(glesProgramUniform3f, "glProgramUniform3f");
    LOAD_REQUIRED(glesProgramUniform4f, "glProgramUniform4f");
    LOAD_REQUIRED(glesProgramUniform1i, "glProgramUniform1i");
    LOAD_REQUIRED(glesProgramUniform2i, "glProgramUniform2i");
    LOAD_REQUIRED(glesProgramUniform3i, "glProgramUniform3i");
    LOAD_REQUIRED(glesProgramUniform4i, "glProgramUniform4i");
    LOAD_REQUIRED(glesProgramUniform1ui, "glProgramUniform1ui");
    LOAD_REQUIRED(glesProgramUniform2ui, "glProgramUniform2ui");
    LOAD_REQUIRED(glesProgramUniform3ui, "glProgramUniform3ui");
    LOAD_REQUIRED(glesProgramUniform4ui, "glProgramUniform4ui");
    LOAD_REQUIRED(glesProgramUniform1fv, "glProgramUniform1fv");
    LOAD_REQUIRED(glesProgramUniform2fv, "glProgramUniform2fv");
    LOAD_REQUIRED(glesProgramUniform3fv, "glProgramUniform3fv");
    LOAD_REQUIRED(glesProgramUniform4fv, "glProgramUniform4fv");
    LOAD_REQUIRED(glesProgramUniform1iv, "glProgramUniform1iv");
    LOAD_REQUIRED(glesProgramUniform2iv, "glProgramUniform2iv");
    LOAD_REQUIRED(glesProgramUniform3iv, "glProgramUniform3iv");
    LOAD_REQUIRED(glesProgramUniform4iv, "glProgramUniform4iv");
    LOAD_REQUIRED(glesProgramUniform1uiv, "glProgramUniform1uiv");
    LOAD_REQUIRED(glesProgramUniform2uiv, "glProgramUniform2uiv");
    LOAD_REQUIRED(glesProgramUniform3uiv, "glProgramUniform3uiv");
    LOAD_REQUIRED(glesProgramUniform4uiv, "glProgramUniform4uiv");
    LOAD_REQUIRED(glesProgramUniformMatrix2fv, "glProgramUniformMatrix2fv");
    LOAD_REQUIRED(glesProgramUniformMatrix3fv, "glProgramUniformMatrix3fv");
    LOAD_REQUIRED(glesProgramUniformMatrix4fv, "glProgramUniformMatrix4fv");
    LOAD_REQUIRED(glesProgramUniformMatrix2x3fv, "glProgramUniformMatrix2x3fv");
    LOAD_REQUIRED(glesProgramUniformMatrix3x2fv, "glProgramUniformMatrix3x2fv");
    LOAD_REQUIRED(glesProgramUniformMatrix2x4fv, "glProgramUniformMatrix2x4fv");
    LOAD_REQUIRED(glesProgramUniformMatrix4x2fv, "glProgramUniformMatrix4x2fv");
    LOAD_REQUIRED(glesProgramUniformMatrix3x4fv, "glProgramUniformMatrix3x4fv");
    LOAD_REQUIRED(glesProgramUniformMatrix4x3fv, "glProgramUniformMatrix4x3fv");
    LOAD_REQUIRED(glesGetVertexAttribiv, "glGetVertexAttribiv");
    LOAD_REQUIRED(glesGetVertexAttribPointerv, "glGetVertexAttribPointerv");
    LOAD_REQUIRED(glesGetVertexAttribfv, "glGetVertexAttribfv");
    LOAD_REQUIRED(glesVertexAttribPointer, "glVertexAttribPointer");
    LOAD_REQUIRED(glesVertexAttribIPointer, "glVertexAttribIPointer");
    LOAD_REQUIRED(glesVertexAttribDivisor, "glVertexAttribDivisor");
    LOAD_REQUIRED(glesBindVertexBuffer, "glBindVertexBuffer");
    LOAD_REQUIRED(glesVertexAttribFormat, "glVertexAttribFormat");
    LOAD_REQUIRED(glesVertexAttribIFormat, "glVertexAttribIFormat");
    LOAD_REQUIRED(glesVertexAttribBinding, "glVertexAttribBinding");
    LOAD_REQUIRED(glesEnableVertexAttribArray, "glEnableVertexAttribArray");
    LOAD_REQUIRED(glesDisableVertexAttribArray, "glDisableVertexAttribArray");
    LOAD_REQUIRED(glesVertexAttrib4fv, "glVertexAttrib4fv");
    LOAD_REQUIRED(glesVertexAttribI4iv, "glVertexAttribI4iv");
    LOAD_REQUIRED(glesVertexAttribI4uiv, "glVertexAttribI4uiv");
    LOAD_REQUIRED(glesDrawArrays, "glDrawArrays");
    LOAD_REQUIRED(glesDrawElements, "glDrawElements");
    LOAD_REQUIRED(glesDrawElementsBaseVertex, "glDrawElementsBaseVertex");
    LOAD_OPTIONAL(glesDrawElementsBaseVertexOES, "glDrawElementsBaseVertexOES");
    LOAD_OPTIONAL(glesDrawElementsBaseVertexEXT, "glDrawElementsBaseVertexEXT");
    LOAD_REQUIRED(glesDrawArraysIndirect, "glDrawArraysIndirect");
    LOAD_REQUIRED(glesDrawElementsIndirect, "glDrawElementsIndirect");
    LOAD_OPTIONAL(glesDrawTransformFeedbackEXT, "glDrawTransformFeedbackEXT");
    LOAD_OPTIONAL(glesDrawTransformFeedbackInstancedEXT, "glDrawTransformFeedbackInstancedEXT");
    LOAD_REQUIRED(glesDrawArraysInstanced, "glDrawArraysInstanced");
    LOAD_REQUIRED(glesDrawElementsInstanced, "glDrawElementsInstanced");
    LOAD_OPTIONAL(glesProvokingVertexANGLE, "glProvokingVertexANGLE");
    LOAD_REQUIRED(glesDispatchCompute, "glDispatchCompute");
    LOAD_REQUIRED(glesDispatchComputeIndirect, "glDispatchComputeIndirect");
    LOAD_REQUIRED(glesBindImageTexture, "glBindImageTexture");
    LOAD_REQUIRED(glesMemoryBarrier, "glMemoryBarrier");
    LOAD_REQUIRED(glesMemoryBarrierByRegion, "glMemoryBarrierByRegion");
    LOAD_REQUIRED(glesGenQueries, "glGenQueries");
    LOAD_REQUIRED(glesDeleteQueries, "glDeleteQueries");
    LOAD_REQUIRED(glesBeginQuery, "glBeginQuery");
    LOAD_REQUIRED(glesEndQuery, "glEndQuery");
    LOAD_REQUIRED(glesGetQueryiv, "glGetQueryiv");
    LOAD_REQUIRED(glesGetQueryObjectuiv, "glGetQueryObjectuiv");
    LOAD_OPTIONAL(glesQueryCounterEXT, "glQueryCounterEXT");
    LOAD_OPTIONAL(glesGetQueryObjectui64vEXT, "glGetQueryObjectui64vEXT");
    LOAD_OPTIONAL(glesGetQueryivEXT, "glGetQueryivEXT");
    LOAD_REQUIRED(glesFenceSync, "glFenceSync");
    LOAD_REQUIRED(glesIsSync, "glIsSync");
    LOAD_REQUIRED(glesDeleteSync, "glDeleteSync");
    LOAD_REQUIRED(glesClientWaitSync, "glClientWaitSync");
    LOAD_REQUIRED(glesWaitSync, "glWaitSync");
    LOAD_REQUIRED(glesGetSynciv, "glGetSynciv");
    LOAD_REQUIRED(glesGenTransformFeedbacks, "glGenTransformFeedbacks");
    LOAD_REQUIRED(glesDeleteTransformFeedbacks, "glDeleteTransformFeedbacks");
    LOAD_REQUIRED(glesBindTransformFeedback, "glBindTransformFeedback");
    LOAD_REQUIRED(glesBeginTransformFeedback, "glBeginTransformFeedback");
    LOAD_REQUIRED(glesEndTransformFeedback, "glEndTransformFeedback");
    LOAD_REQUIRED(glesPauseTransformFeedback, "glPauseTransformFeedback");
    LOAD_REQUIRED(glesResumeTransformFeedback, "glResumeTransformFeedback");
    LOAD_REQUIRED(glesTransformFeedbackVaryings, "glTransformFeedbackVaryings");
    LOAD_REQUIRED(glesGetTransformFeedbackVarying, "glGetTransformFeedbackVarying");
#undef LOAD_OPTIONAL
#undef LOAD_REQUIRED
    return 1;
}

static void preload_android_vulkan(struct mgl_native_dispatch *b) {
#if defined(__ANDROID__)
#if defined(__LP64__)
    const char *path = "/system/lib64/libvulkan.so";
#else
    const char *path = "/system/lib/libvulkan.so";
#endif
    if (!mgl_library_preload_system_vulkan(&b->system_vulkan_lib, path)) {
        fprintf(stderr,
                "ManifestGL: system Vulkan preload failed for %s; "
                "ANGLE may select another Vulkan loader\n",
                path);
    } else if (mgl_library_system_vulkan_preempted(&b->system_vulkan_lib)) {
        fprintf(stderr, "ManifestGL: Vulkan preemption: another libvulkan is already "
                        "globally visible; ANGLE may select that loader\n");
    }
#else
    (void)b;
#endif
}

#include "native/dispatch_trace.generated.inc"

int mgl_native_dispatch_init(struct mgl_native_dispatch *b) {
    if (b == NULL)
        return 0;
    if (b->initialized)
        return b->available;
    b->initialized = 1;
    preload_android_vulkan(b);

    if (!mgl_library_open_filtered(&b->egl_lib, "MGL_EGL_LIBRARY", egl_candidates,
                                   sizeof(egl_candidates) / sizeof(egl_candidates[0]),
                                   "mgl_public_egl_interface")) {
        mgl_library_close(&b->system_vulkan_lib);
        return 0;
    }
    if (!mgl_library_open(&b->gles_lib, "MGL_GLES_LIBRARY", gles_candidates,
                          sizeof(gles_candidates) / sizeof(gles_candidates[0]))) {
        mgl_library_close(&b->egl_lib);
        mgl_library_close(&b->system_vulkan_lib);
        return 0;
    }
    if (!load_egl(b) || !load_gles(b)) {
        mgl_native_dispatch_shutdown(b);
        b->initialized = 1;
        return 0;
    }
    mgl_bind_native = b;
    b->glesBindFramebufferRaw = b->glesBindFramebuffer;
    b->glesBindFramebuffer = mgl_bind_default_framebuffer;
    if (mgl_gles_trace_enabled()) {
        if (!mgl_gles_trace_install(b)) {
            mgl_native_dispatch_shutdown(b);
            b->initialized = 1;
            return 0;
        }
        b->glesGetError = mgl_trace_glesGetError;
    }
    b->available = 1;
    return 1;
}

void mgl_native_dispatch_shutdown(struct mgl_native_dispatch *b) {
    if (b == NULL)
        return;
    if (mgl_bind_native == b)
        mgl_bind_native = NULL;
    free(b->trace_raw);
    mgl_library_close(&b->gles_lib);
    mgl_library_close(&b->egl_lib);
    mgl_library_close(&b->system_vulkan_lib);
    memset(b, 0, sizeof(*b));
}

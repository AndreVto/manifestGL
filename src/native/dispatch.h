#ifndef MGL_NATIVE_DISPATCH_H
#define MGL_NATIVE_DISPATCH_H

#include "native/egl_abi.h"
#include "native/loader.h"
#include "mgl/types.h"

struct mgl_native_dispatch {
    struct mgl_library egl_lib;
    struct mgl_library gles_lib;
    struct mgl_library system_vulkan_lib;
    int initialized;
    int available;

    MGL_EGLDisplay (*eglGetDisplay)(MGL_EGLNativeDisplayType);
    MGL_EGLBoolean (*eglInitialize)(MGL_EGLDisplay, MGL_EGLint *, MGL_EGLint *);
    MGL_EGLBoolean (*eglTerminate)(MGL_EGLDisplay);
    MGL_EGLBoolean (*eglChooseConfig)(MGL_EGLDisplay, const MGL_EGLint *, MGL_EGLConfig *,
                                      MGL_EGLint, MGL_EGLint *);
    MGL_EGLBoolean (*eglBindAPI)(MGL_EGLint);
    MGL_EGLContext (*eglCreateContext)(MGL_EGLDisplay, MGL_EGLConfig, MGL_EGLContext,
                                       const MGL_EGLint *);
    MGL_EGLBoolean (*eglDestroyContext)(MGL_EGLDisplay, MGL_EGLContext);
    MGL_EGLSurface (*eglCreatePbufferSurface)(MGL_EGLDisplay, MGL_EGLConfig, const MGL_EGLint *);
    MGL_EGLSurface (*eglCreateWindowSurface)(MGL_EGLDisplay, MGL_EGLConfig, MGL_EGLNativeWindowType,
                                             const MGL_EGLint *);
    MGL_EGLBoolean (*eglDestroySurface)(MGL_EGLDisplay, MGL_EGLSurface);
    MGL_EGLBoolean (*eglMakeCurrent)(MGL_EGLDisplay, MGL_EGLSurface, MGL_EGLSurface,
                                     MGL_EGLContext);
    MGL_EGLBoolean (*eglSwapBuffers)(MGL_EGLDisplay, MGL_EGLSurface);
    MGL_EGLBoolean (*eglSwapInterval)(MGL_EGLDisplay, MGL_EGLint);
    MGL_EGLint (*eglGetError)(void);
    const char *(*eglQueryString)(MGL_EGLDisplay, MGL_EGLint);
    mgl_egl_proc_t (*eglGetProcAddress)(const char *);

    const GLubyte *(*glesGetString)(GLenum);
    const GLubyte *(*glesGetStringi)(GLenum, GLuint);
    GLenum (*glesGetError)(void);
    GLenum (*glesGetGraphicsResetStatus)(void);
    void (*glesGetIntegerv)(GLenum, GLint *);
    void (*glesGetFloatv)(GLenum, GLfloat *);
    void (*glesGetInternalformativ)(GLenum, GLenum, GLenum, GLsizei, GLint *);
    void (*glesGetFramebufferAttachmentParameteriv)(GLenum, GLenum, GLenum, GLint *);
    void (*glesGetInteger64v)(GLenum, GLint64 *);
    void (*glesGetIntegeri_v)(GLenum, GLuint, GLint *);
    void (*glesBeginQueryIndexed)(GLenum, GLuint, GLuint);
    void (*glesEndQueryIndexed)(GLenum, GLuint);
    void (*glesGetMultisamplefv)(GLenum, GLuint, GLfloat *);
    void (*glesLogicOpANGLE)(GLenum);
    void (*glesEnable)(GLenum);
    void (*glesDisable)(GLenum);
    void (*glesEnablei)(GLenum, GLuint);
    void (*glesDisablei)(GLenum, GLuint);
    void (*glesClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (*glesClearDepthf)(GLfloat);
    void (*glesClearStencil)(GLint);
    void (*glesClear)(GLbitfield);
    void (*glesClearBufferiv)(GLenum, GLint, const GLint *);
    void (*glesClearBufferuiv)(GLenum, GLint, const GLuint *);
    void (*glesClearBufferfv)(GLenum, GLint, const GLfloat *);
    void (*glesClearBufferfi)(GLenum, GLint, GLfloat, GLint);
    void (*glesViewport)(GLint, GLint, GLsizei, GLsizei);
    void (*glesViewportArrayvOES)(GLuint, GLsizei, const GLfloat *);
    void (*glesScissorArrayvOES)(GLuint, GLsizei, const GLint *);
    void (*glesDepthRangeArrayfvOES)(GLuint, GLsizei, const GLfloat *);
    void (*glesClipControlEXT)(GLenum, GLenum);
    void (*glesDepthBoundsEXT)(GLclampd, GLclampd);
    void (*glesPatchParameteri)(GLenum, GLint);
    void (*glesPatchParameterfv)(GLenum, const GLfloat *);
    void (*glesDepthRangef)(GLfloat, GLfloat);
    void (*glesBlendColor)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (*glesLineWidth)(GLfloat);
    void (*glesPolygonModeNV)(GLenum, GLenum);
    void (*glesPolygonOffset)(GLfloat, GLfloat);
    void (*glesPolygonOffsetClamp)(GLfloat, GLfloat, GLfloat);
    void (*glesMinSampleShading)(GLfloat);
    void (*glesSampleCoverage)(GLfloat, GLboolean);
    void (*glesSampleMaski)(GLuint, GLbitfield);
    void (*glesScissor)(GLint, GLint, GLsizei, GLsizei);
    void (*glesDepthFunc)(GLenum);
    void (*glesDepthMask)(GLboolean);
    void (*glesStencilFuncSeparate)(GLenum, GLenum, GLint, GLuint);
    void (*glesStencilMaskSeparate)(GLenum, GLuint);
    void (*glesStencilOpSeparate)(GLenum, GLenum, GLenum, GLenum);
    void (*glesColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
    void (*glesColorMaski)(GLuint, GLboolean, GLboolean, GLboolean, GLboolean);
    void (*glesCullFace)(GLenum);
    void (*glesFrontFace)(GLenum);
    void (*glesBlendFunc)(GLenum, GLenum);
    void (*glesBlendFuncSeparate)(GLenum, GLenum, GLenum, GLenum);
    void (*glesBlendFuncSeparatei)(GLuint, GLenum, GLenum, GLenum, GLenum);
    void (*glesBlendBarrier)(void);
    void (*glesPrimitiveBoundingBox)(GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat,
                                     GLfloat, GLfloat);
    void (*glesBlendEquationi)(GLuint, GLenum);
    void (*glesBlendEquation)(GLenum);
    void (*glesBlendEquationSeparate)(GLenum, GLenum);
    void (*glesBlendEquationSeparatei)(GLuint, GLenum, GLenum);
    void (*glesFlush)(void);
    void (*glesFinish)(void);
    void (*glesReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
    void (*glesActiveTexture)(GLenum);
    void (*glesGenTextures)(GLsizei, GLuint *);
    void (*glesDeleteTextures)(GLsizei, const GLuint *);
    void (*glesBindTexture)(GLenum, GLuint);
    void (*glesTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
                           const void *);
    void (*glesCopyTexImage2D)(GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint);
    void (*glesCopyTexSubImage2D)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
    void (*glesTexImage3D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum,
                           const void *);
    void (*glesTexStorage2D)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
    void (*glesTextureView)(GLuint, GLenum, GLuint, GLenum, GLuint, GLuint, GLuint, GLuint);
    void (*glesTexStorage3D)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei);
    void (*glesTexStorage2DMultisample)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean);
    void (*glesTexStorage3DMultisample)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei,
                                        GLboolean);
    void (*glesTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum,
                              const void *);
    void (*glesTexSubImage3D)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum,
                              GLenum, const void *);
    void (*glesCompressedTexImage2D)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei,
                                     const void *);
    void (*glesCompressedTexImage3D)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint,
                                     GLsizei, const void *);
    void (*glesCompressedTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum,
                                        GLsizei, const void *);
    void (*glesCompressedTexSubImage3D)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei,
                                        GLsizei, GLenum, GLsizei, const void *);
    void (*glesTexBufferEXT)(GLenum, GLenum, GLuint);
    void (*glesTexBufferRangeEXT)(GLenum, GLenum, GLuint, GLintptr, GLsizeiptr);
    void (*glesGetTexParameteriv)(GLenum, GLenum, GLint *);
    void (*glesTexParameteri)(GLenum, GLenum, GLint);
    void (*glesTexParameterf)(GLenum, GLenum, GLfloat);
    void (*glesTexParameterfv)(GLenum, GLenum, const GLfloat *);
    void (*glesTexParameterIiv)(GLenum, GLenum, const GLint *);
    void (*glesTexParameterIuiv)(GLenum, GLenum, const GLuint *);
    void (*glesPixelStorei)(GLenum, GLint);
    void (*glesGenerateMipmap)(GLenum);
    void (*glesCopyImageSubData)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint,
                                 GLint, GLint, GLint, GLsizei, GLsizei, GLsizei);
    void (*glesTextureBarrier)(void);
    void (*glesGenSamplers)(GLsizei, GLuint *);
    void (*glesDeleteSamplers)(GLsizei, const GLuint *);
    void (*glesBindSampler)(GLuint, GLuint);
    void (*glesGetSamplerParameteriv)(GLuint, GLenum, GLint *);
    void (*glesSamplerParameteri)(GLuint, GLenum, GLint);
    void (*glesSamplerParameterf)(GLuint, GLenum, GLfloat);
    void (*glesSamplerParameterfv)(GLuint, GLenum, const GLfloat *);
    void (*glesSamplerParameteriv)(GLuint, GLenum, const GLint *);
    void (*glesSamplerParameterIiv)(GLuint, GLenum, const GLint *);
    void (*glesSamplerParameterIuiv)(GLuint, GLenum, const GLuint *);

    void (*glesGenBuffers)(GLsizei, GLuint *);
    void (*glesDeleteBuffers)(GLsizei, const GLuint *);
    void (*glesBindBuffer)(GLenum, GLuint);
    void (*glesBufferData)(GLenum, GLsizeiptr, const void *, GLenum);
    void (*glesBufferStorageEXT)(GLenum, GLsizeiptr, const void *, GLbitfield);
    void *(*glesMapBufferRange)(GLenum, GLintptr, GLsizeiptr, GLbitfield);
    GLboolean (*glesUnmapBuffer)(GLenum);
    void (*glesFlushMappedBufferRange)(GLenum, GLintptr, GLsizeiptr);
    void (*glesBufferSubData)(GLenum, GLintptr, GLsizeiptr, const void *);
    void (*glesCopyBufferSubData)(GLenum, GLenum, GLintptr, GLintptr, GLsizeiptr);
    void (*glesBindBufferBase)(GLenum, GLuint, GLuint);
    void (*glesBindBufferRange)(GLenum, GLuint, GLuint, GLintptr, GLsizeiptr);

    void (*glesGenVertexArrays)(GLsizei, GLuint *);
    void (*glesDeleteVertexArrays)(GLsizei, const GLuint *);
    void (*glesBindVertexArray)(GLuint);

    void (*glesGenFramebuffers)(GLsizei, GLuint *);
    void (*glesDeleteFramebuffers)(GLsizei, const GLuint *);
    void (*glesBindFramebuffer)(GLenum, GLuint);

    void (*glesBindFramebufferRaw)(GLenum, GLuint);
    GLenum (*glesCheckFramebufferStatus)(GLenum);
    void (*glesFramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint);
    void (*glesFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
    void (*glesFramebufferTexture)(GLenum, GLenum, GLuint, GLint);
    void (*glesFramebufferParameteri)(GLenum, GLenum, GLint);
    void (*glesFramebufferTextureLayer)(GLenum, GLenum, GLuint, GLint, GLint);
    void (*glesDrawBuffers)(GLsizei, const GLenum *);
    void (*glesReadBuffer)(GLenum);
    void (*glesBlitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield,
                                GLenum);

    void (*glesGenRenderbuffers)(GLsizei, GLuint *);
    void (*glesDeleteRenderbuffers)(GLsizei, const GLuint *);
    void (*glesBindRenderbuffer)(GLenum, GLuint);
    void (*glesRenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei);
    void (*glesRenderbufferStorageMultisample)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
    void (*glesGetRenderbufferParameteriv)(GLenum, GLenum, GLint *);

    GLuint (*glesCreateShader)(GLenum);
    void (*glesShaderSource)(GLuint, GLsizei, const GLchar *const *, const GLint *);
    void (*glesCompileShader)(GLuint);
    void (*glesShaderBinary)(GLsizei, const GLuint *, GLenum, const void *, GLsizei);
    void (*glesGetShaderPrecisionFormat)(GLenum, GLenum, GLint *, GLint *);
    void (*glesGetShaderiv)(GLuint, GLenum, GLint *);
    void (*glesGetShaderInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
    void (*glesDeleteShader)(GLuint);

    GLuint (*glesCreateProgram)(void);
    void (*glesAttachShader)(GLuint, GLuint);
    void (*glesDetachShader)(GLuint, GLuint);
    void (*glesBindAttribLocation)(GLuint, GLuint, const GLchar *);
    void (*glesBindFragDataLocationEXT)(GLuint, GLuint, const GLchar *);
    void (*glesBindFragDataLocationIndexedEXT)(GLuint, GLuint, GLuint, const GLchar *);
    void (*glesLinkProgram)(GLuint);
    void (*glesValidateProgram)(GLuint);
    void (*glesUseProgram)(GLuint);
    void (*glesGenProgramPipelines)(GLsizei, GLuint *);
    void (*glesDeleteProgramPipelines)(GLsizei, const GLuint *);
    void (*glesBindProgramPipeline)(GLuint);
    GLboolean (*glesIsProgramPipeline)(GLuint);
    void (*glesUseProgramStages)(GLuint, GLbitfield, GLuint);
    void (*glesActiveShaderProgram)(GLuint, GLuint);
    void (*glesValidateProgramPipeline)(GLuint);
    void (*glesGetProgramPipelineiv)(GLuint, GLenum, GLint *);
    void (*glesGetProgramPipelineInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
    void (*glesGetProgramiv)(GLuint, GLenum, GLint *);
    void (*glesGetProgramInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
    void (*glesProgramParameteri)(GLuint, GLenum, GLint);
    void (*glesGetProgramBinary)(GLuint, GLsizei, GLsizei *, GLenum *, void *);
    void (*glesProgramBinary)(GLuint, GLenum, const void *, GLsizei);
    void (*glesGetActiveAttrib)(GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLchar *);
    void (*glesGetActiveUniform)(GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLchar *);
    void (*glesGetUniformIndices)(GLuint, GLsizei, const GLchar *const *, GLuint *);
    void (*glesGetActiveUniformsiv)(GLuint, GLsizei, const GLuint *, GLenum, GLint *);
    GLuint (*glesGetUniformBlockIndex)(GLuint, const GLchar *);
    void (*glesGetActiveUniformBlockiv)(GLuint, GLuint, GLenum, GLint *);
    void (*glesGetActiveUniformBlockName)(GLuint, GLuint, GLsizei, GLsizei *, GLchar *);
    void (*glesUniformBlockBinding)(GLuint, GLuint, GLuint);
    void (*glesShaderStorageBlockBinding)(GLuint, GLuint, GLuint);
    void (*glesGetProgramInterfaceiv)(GLuint, GLenum, GLenum, GLint *);
    GLuint (*glesGetProgramResourceIndex)(GLuint, GLenum, const GLchar *);
    void (*glesGetProgramResourceName)(GLuint, GLenum, GLuint, GLsizei, GLsizei *, GLchar *);
    void (*glesGetProgramResourceiv)(GLuint, GLenum, GLuint, GLsizei, const GLenum *, GLsizei,
                                     GLsizei *, GLint *);
    GLint (*glesGetProgramResourceLocation)(GLuint, GLenum, const GLchar *);
    GLint (*glesGetProgramResourceLocationIndex)(GLuint, GLenum, const GLchar *);
    void (*glesDeleteProgram)(GLuint);
    GLint (*glesGetAttribLocation)(GLuint, const GLchar *);
    GLint (*glesGetUniformLocation)(GLuint, const GLchar *);
    GLint (*glesGetFragDataLocation)(GLuint, const GLchar *);
    GLint (*glesGetFragDataIndexEXT)(GLuint, const GLchar *);
    void (*glesUniform1f)(GLint, GLfloat);
    void (*glesUniform2f)(GLint, GLfloat, GLfloat);
    void (*glesUniform3f)(GLint, GLfloat, GLfloat, GLfloat);
    void (*glesUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
    void (*glesUniform1i)(GLint, GLint);
    void (*glesUniform2i)(GLint, GLint, GLint);
    void (*glesUniform3i)(GLint, GLint, GLint, GLint);
    void (*glesUniform4i)(GLint, GLint, GLint, GLint, GLint);
    void (*glesUniform1ui)(GLint, GLuint);
    void (*glesUniform2ui)(GLint, GLuint, GLuint);
    void (*glesUniform3ui)(GLint, GLuint, GLuint, GLuint);
    void (*glesUniform4ui)(GLint, GLuint, GLuint, GLuint, GLuint);
    void (*glesUniform1fv)(GLint, GLsizei, const GLfloat *);
    void (*glesUniform2fv)(GLint, GLsizei, const GLfloat *);
    void (*glesUniform3fv)(GLint, GLsizei, const GLfloat *);
    void (*glesUniform4fv)(GLint, GLsizei, const GLfloat *);
    void (*glesUniform1iv)(GLint, GLsizei, const GLint *);
    void (*glesUniform2iv)(GLint, GLsizei, const GLint *);
    void (*glesUniform3iv)(GLint, GLsizei, const GLint *);
    void (*glesUniform4iv)(GLint, GLsizei, const GLint *);
    void (*glesUniform1uiv)(GLint, GLsizei, const GLuint *);
    void (*glesUniform2uiv)(GLint, GLsizei, const GLuint *);
    void (*glesUniform3uiv)(GLint, GLsizei, const GLuint *);
    void (*glesUniform4uiv)(GLint, GLsizei, const GLuint *);
    void (*glesUniformMatrix2fv)(GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesUniformMatrix3fv)(GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesUniformMatrix2x3fv)(GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesUniformMatrix3x2fv)(GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesUniformMatrix2x4fv)(GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesUniformMatrix4x2fv)(GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesUniformMatrix3x4fv)(GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesUniformMatrix4x3fv)(GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesGetUniformfv)(GLuint, GLint, GLfloat *);
    void (*glesGetUniformiv)(GLuint, GLint, GLint *);
    void (*glesGetUniformuiv)(GLuint, GLint, GLuint *);
    void (*glesProgramUniform1f)(GLuint, GLint, GLfloat);
    void (*glesProgramUniform2f)(GLuint, GLint, GLfloat, GLfloat);
    void (*glesProgramUniform3f)(GLuint, GLint, GLfloat, GLfloat, GLfloat);
    void (*glesProgramUniform4f)(GLuint, GLint, GLfloat, GLfloat, GLfloat, GLfloat);
    void (*glesProgramUniform1i)(GLuint, GLint, GLint);
    void (*glesProgramUniform2i)(GLuint, GLint, GLint, GLint);
    void (*glesProgramUniform3i)(GLuint, GLint, GLint, GLint, GLint);
    void (*glesProgramUniform4i)(GLuint, GLint, GLint, GLint, GLint, GLint);
    void (*glesProgramUniform1ui)(GLuint, GLint, GLuint);
    void (*glesProgramUniform2ui)(GLuint, GLint, GLuint, GLuint);
    void (*glesProgramUniform3ui)(GLuint, GLint, GLuint, GLuint, GLuint);
    void (*glesProgramUniform4ui)(GLuint, GLint, GLuint, GLuint, GLuint, GLuint);
    void (*glesProgramUniform1fv)(GLuint, GLint, GLsizei, const GLfloat *);
    void (*glesProgramUniform2fv)(GLuint, GLint, GLsizei, const GLfloat *);
    void (*glesProgramUniform3fv)(GLuint, GLint, GLsizei, const GLfloat *);
    void (*glesProgramUniform4fv)(GLuint, GLint, GLsizei, const GLfloat *);
    void (*glesProgramUniform1iv)(GLuint, GLint, GLsizei, const GLint *);
    void (*glesProgramUniform2iv)(GLuint, GLint, GLsizei, const GLint *);
    void (*glesProgramUniform3iv)(GLuint, GLint, GLsizei, const GLint *);
    void (*glesProgramUniform4iv)(GLuint, GLint, GLsizei, const GLint *);
    void (*glesProgramUniform1uiv)(GLuint, GLint, GLsizei, const GLuint *);
    void (*glesProgramUniform2uiv)(GLuint, GLint, GLsizei, const GLuint *);
    void (*glesProgramUniform3uiv)(GLuint, GLint, GLsizei, const GLuint *);
    void (*glesProgramUniform4uiv)(GLuint, GLint, GLsizei, const GLuint *);
    void (*glesProgramUniformMatrix2fv)(GLuint, GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesProgramUniformMatrix3fv)(GLuint, GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesProgramUniformMatrix4fv)(GLuint, GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesProgramUniformMatrix2x3fv)(GLuint, GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesProgramUniformMatrix3x2fv)(GLuint, GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesProgramUniformMatrix2x4fv)(GLuint, GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesProgramUniformMatrix4x2fv)(GLuint, GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesProgramUniformMatrix3x4fv)(GLuint, GLint, GLsizei, GLboolean, const GLfloat *);
    void (*glesProgramUniformMatrix4x3fv)(GLuint, GLint, GLsizei, GLboolean, const GLfloat *);

    void (*glesGetVertexAttribiv)(GLuint, GLenum, GLint *);
    void (*glesGetVertexAttribPointerv)(GLuint, GLenum, void **);
    void (*glesGetVertexAttribfv)(GLuint, GLenum, GLfloat *);
    void (*glesVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
    void (*glesVertexAttribIPointer)(GLuint, GLint, GLenum, GLsizei, const void *);
    void (*glesVertexAttribDivisor)(GLuint, GLuint);
    void (*glesBindVertexBuffer)(GLuint, GLuint, GLintptr, GLsizei);
    void (*glesVertexAttribFormat)(GLuint, GLint, GLenum, GLboolean, GLuint);
    void (*glesVertexAttribIFormat)(GLuint, GLint, GLenum, GLuint);
    void (*glesVertexAttribBinding)(GLuint, GLuint);
    void (*glesEnableVertexAttribArray)(GLuint);
    void (*glesDisableVertexAttribArray)(GLuint);
    void (*glesVertexAttrib4fv)(GLuint, const GLfloat *);
    void (*glesVertexAttribI4iv)(GLuint, const GLint *);
    void (*glesVertexAttribI4uiv)(GLuint, const GLuint *);
    void (*glesDrawArrays)(GLenum, GLint, GLsizei);
    void (*glesDrawElements)(GLenum, GLsizei, GLenum, const void *);
    void (*glesDrawElementsBaseVertex)(GLenum, GLsizei, GLenum, const void *, GLint);
    void (*glesDrawElementsBaseVertexOES)(GLenum, GLsizei, GLenum, const void *, GLint);
    void (*glesDrawElementsBaseVertexEXT)(GLenum, GLsizei, GLenum, const void *, GLint);
    void (*glesDrawArraysIndirect)(GLenum, const void *);
    void (*glesDrawElementsIndirect)(GLenum, GLenum, const void *);
    void (*glesDrawTransformFeedbackEXT)(GLenum, GLuint);
    void (*glesDrawTransformFeedbackInstancedEXT)(GLenum, GLuint, GLsizei);
    void (*glesDrawArraysInstanced)(GLenum, GLint, GLsizei, GLsizei);
    void (*glesDrawElementsInstanced)(GLenum, GLsizei, GLenum, const void *, GLsizei);
    void (*glesProvokingVertexANGLE)(GLenum);
    void (*glesDispatchCompute)(GLuint, GLuint, GLuint);
    void (*glesDispatchComputeIndirect)(GLintptr);
    void (*glesBindImageTexture)(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum);
    void (*glesMemoryBarrier)(GLbitfield);
    void (*glesMemoryBarrierByRegion)(GLbitfield);

    void (*glesGenQueries)(GLsizei, GLuint *);
    void (*glesDeleteQueries)(GLsizei, const GLuint *);
    void (*glesBeginQuery)(GLenum, GLuint);
    void (*glesEndQuery)(GLenum);
    void (*glesGetQueryiv)(GLenum, GLenum, GLint *);
    void (*glesGetQueryObjectuiv)(GLuint, GLenum, GLuint *);
    void (*glesQueryCounterEXT)(GLuint, GLenum);
    void (*glesGetQueryObjectui64vEXT)(GLuint, GLenum, GLuint64 *);
    void (*glesGetQueryivEXT)(GLenum, GLenum, GLint *);
    GLsync (*glesFenceSync)(GLenum, GLbitfield);
    GLboolean (*glesIsSync)(GLsync);
    void (*glesDeleteSync)(GLsync);
    GLenum (*glesClientWaitSync)(GLsync, GLbitfield, GLuint64);
    void (*glesWaitSync)(GLsync, GLbitfield, GLuint64);
    void (*glesGetSynciv)(GLsync, GLenum, GLsizei, GLsizei *, GLint *);
    void (*glesGenTransformFeedbacks)(GLsizei, GLuint *);
    void (*glesDeleteTransformFeedbacks)(GLsizei, const GLuint *);
    void (*glesBindTransformFeedback)(GLenum, GLuint);
    void (*glesBeginTransformFeedback)(GLenum);
    void (*glesEndTransformFeedback)(void);
    void (*glesPauseTransformFeedback)(void);
    void (*glesResumeTransformFeedback)(void);
    void (*glesTransformFeedbackVaryings)(GLuint, GLsizei, const GLchar *const *, GLenum);
    void (*glesGetTransformFeedbackVarying)(GLuint, GLuint, GLsizei, GLsizei *, GLsizei *, GLenum *,
                                            GLchar *);
};

int mgl_native_dispatch_init(struct mgl_native_dispatch *native);
void mgl_native_dispatch_shutdown(struct mgl_native_dispatch *native);
void *mgl_native_dispatch_resolve_gles(struct mgl_native_dispatch *native, const char *name);
int mgl_native_dispatch_has_gles_extension(const struct mgl_native_dispatch *native,
                                           const char *name);

#endif

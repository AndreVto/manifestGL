#ifndef MGL_UNIMPLEMENTED_EXPORTS_H
#define MGL_UNIMPLEMENTED_EXPORTS_H

#include "mgl/gl.h"

typedef GLushort GLhalfNV;
typedef GLDEBUGPROC GLDEBUGPROCARB;

MGL_EXPORT void glAccum(GLenum op, GLfloat value);
MGL_EXPORT void glAlphaFragmentOp1ATI(GLenum op, GLuint dst, GLuint dstMod, GLuint arg1,
                                      GLuint arg1Rep, GLuint arg1Mod);
MGL_EXPORT void glAlphaFragmentOp2ATI(GLenum op, GLuint dst, GLuint dstMod, GLuint arg1,
                                      GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep,
                                      GLuint arg2Mod);
MGL_EXPORT void glAlphaFragmentOp3ATI(GLenum op, GLuint dst, GLuint dstMod, GLuint arg1,
                                      GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep,
                                      GLuint arg2Mod, GLuint arg3, GLuint arg3Rep, GLuint arg3Mod);
MGL_EXPORT void glBeginFragmentShaderATI(void);
MGL_EXPORT void glBindFragmentShaderATI(GLuint id);
MGL_EXPORT void glBufferParameteriAPPLE(GLenum target, GLenum pname, GLint param);
MGL_EXPORT void glClearAccum(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
MGL_EXPORT void glColorFragmentOp1ATI(GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod,
                                      GLuint arg1, GLuint arg1Rep, GLuint arg1Mod);
MGL_EXPORT void glColorFragmentOp2ATI(GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod,
                                      GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2,
                                      GLuint arg2Rep, GLuint arg2Mod);
MGL_EXPORT void glColorFragmentOp3ATI(GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod,
                                      GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2,
                                      GLuint arg2Rep, GLuint arg2Mod, GLuint arg3, GLuint arg3Rep,
                                      GLuint arg3Mod);
MGL_EXPORT void glCombinerInputNV(GLenum stage, GLenum portion, GLenum variable, GLenum input,
                                  GLenum mapping, GLenum componentUsage);
MGL_EXPORT void glCombinerOutputNV(GLenum stage, GLenum portion, GLenum abOutput, GLenum cdOutput,
                                   GLenum sumOutput, GLenum scale, GLenum bias,
                                   GLboolean abDotProduct, GLboolean cdDotProduct,
                                   GLboolean muxSum);
MGL_EXPORT void glCombinerParameterfNV(GLenum pname, GLfloat param);
MGL_EXPORT void glCombinerParameterfvNV(GLenum pname, const GLfloat *params);
MGL_EXPORT void glCombinerParameteriNV(GLenum pname, GLint param);
MGL_EXPORT void glCombinerParameterivNV(GLenum pname, const GLint *params);
MGL_EXPORT void glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x,
                                 GLint y, GLsizei width, GLint border);
MGL_EXPORT void glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y,
                                    GLsizei width);
MGL_EXPORT void glDebugMessageCallbackARB(GLDEBUGPROCARB callback, const void *userParam);
MGL_EXPORT void glDebugMessageControlARB(GLenum source, GLenum type, GLenum severity, GLsizei count,
                                         const GLuint *ids, GLboolean enabled);
MGL_EXPORT void glDebugMessageInsertARB(GLenum source, GLenum type, GLuint id, GLenum severity,
                                        GLsizei length, const GLchar *buf);
MGL_EXPORT void glDeleteFencesAPPLE(GLsizei n, const GLuint *fences);
MGL_EXPORT void glDeleteFragmentShaderATI(GLuint id);
MGL_EXPORT void glEdgeFlag(GLboolean flag);
MGL_EXPORT void glEdgeFlagPointer(GLsizei stride, const void *pointer);
MGL_EXPORT void glEdgeFlagv(const GLboolean *flag);
MGL_EXPORT void glEndFragmentShaderATI(void);
MGL_EXPORT void glFeedbackBuffer(GLsizei size, GLenum type, GLfloat *buffer);
MGL_EXPORT void glFinalCombinerInputNV(GLenum variable, GLenum input, GLenum mapping,
                                       GLenum componentUsage);
MGL_EXPORT void glFinishFenceAPPLE(GLuint fence);
MGL_EXPORT void glFinishObjectAPPLE(GLenum object, GLint name);
MGL_EXPORT void glFlushMappedBufferRangeAPPLE(GLenum target, GLintptr offset, GLsizeiptr size);
MGL_EXPORT void glGenFencesAPPLE(GLsizei n, GLuint *fences);
MGL_EXPORT GLuint glGenFragmentShadersATI(GLuint range);
MGL_EXPORT GLuint glGetDebugMessageLogARB(GLuint count, GLsizei bufSize, GLenum *sources,
                                          GLenum *types, GLuint *ids, GLenum *severities,
                                          GLsizei *lengths, GLchar *messageLog);
MGL_EXPORT void glGetPolygonStipple(GLubyte *mask);
MGL_EXPORT void glGetUnsignedBytei_vEXT(GLenum target, GLuint index, GLubyte *data);
MGL_EXPORT void glGetUnsignedBytevEXT(GLenum pname, GLubyte *data);
MGL_EXPORT void glImportMemoryFdEXT(GLuint memory, GLuint64 size, GLenum handleType, GLint fd);
MGL_EXPORT void glImportSemaphoreFdEXT(GLuint semaphore, GLenum handleType, GLint fd);
MGL_EXPORT void glIndexPointer(GLenum type, GLsizei stride, const void *pointer);
MGL_EXPORT GLboolean glIsFenceAPPLE(GLuint fence);
MGL_EXPORT void glLineStipple(GLint factor, GLushort pattern);
MGL_EXPORT void glLogicOp(GLenum opcode);
MGL_EXPORT void glPassTexCoordATI(GLuint dst, GLuint coord, GLenum swizzle);
MGL_EXPORT void glPassThrough(GLfloat token);
MGL_EXPORT void glPointParameterf(GLenum pname, GLfloat param);
MGL_EXPORT void glPointParameterfARB(GLenum pname, GLfloat param);
MGL_EXPORT void glPointParameterfEXT(GLenum pname, GLfloat param);
MGL_EXPORT void glPointParameterfv(GLenum pname, const GLfloat *params);
MGL_EXPORT void glPointParameterfvARB(GLenum pname, const GLfloat *params);
MGL_EXPORT void glPointParameterfvEXT(GLenum pname, const GLfloat *params);
MGL_EXPORT void glPointParameteriNV(GLenum pname, GLint param);
MGL_EXPORT void glPointParameterivNV(GLenum pname, const GLint *params);
MGL_EXPORT void glPolygonStipple(const GLubyte *mask);
MGL_EXPORT void glSampleMapATI(GLuint dst, GLuint interp, GLenum swizzle);
MGL_EXPORT void glSetFenceAPPLE(GLuint fence);
MGL_EXPORT void glSetFragmentShaderConstantATI(GLuint dst, const GLfloat *value);
MGL_EXPORT GLboolean glTestFenceAPPLE(GLuint fence);
MGL_EXPORT GLboolean glTestObjectAPPLE(GLenum object, GLuint name);
MGL_EXPORT void glVertexWeighthNV(GLhalfNV weight);
MGL_EXPORT void glVertexWeighthvNV(const GLhalfNV *weight);

#endif

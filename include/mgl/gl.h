#ifndef MGL_GL_H
#define MGL_GL_H

#include "mgl/export.h"
#include "mgl/types.h"

#ifdef __cplusplus
extern "C" {
#endif

MGL_EXPORT void glCompressedTexImage1DARB(GLenum target, GLint level, GLenum internalformat,
                                          GLsizei width, GLint border, GLsizei imageSize,
                                          const void *data);
MGL_EXPORT void glCompressedTexSubImage1DARB(GLenum target, GLint level, GLint xoffset,
                                             GLsizei width, GLenum format, GLsizei imageSize,
                                             const void *data);
MGL_EXPORT void glCompressedTexImage2DARB(GLenum target, GLint level, GLenum internalformat,
                                          GLsizei width, GLsizei height, GLint border,
                                          GLsizei imageSize, const void *data);
MGL_EXPORT void glCompressedTexImage3DARB(GLenum target, GLint level, GLenum internalformat,
                                          GLsizei width, GLsizei height, GLsizei depth,
                                          GLint border, GLsizei imageSize, const void *data);
MGL_EXPORT void glCompressedTexSubImage2DARB(GLenum target, GLint level, GLint xoffset,
                                             GLint yoffset, GLsizei width, GLsizei height,
                                             GLenum format, GLsizei imageSize, const void *data);
MGL_EXPORT void glCompressedTexSubImage3DARB(GLenum target, GLint level, GLint xoffset,
                                             GLint yoffset, GLint zoffset, GLsizei width,
                                             GLsizei height, GLsizei depth, GLenum format,
                                             GLsizei imageSize, const void *data);
MGL_EXPORT void glGetCompressedTexImageARB(GLenum target, GLint level, void *img);
MGL_EXPORT void glMultiTexCoord1fARB(GLenum texture, GLfloat s);
MGL_EXPORT void glMultiTexCoord1fvARB(GLenum texture, const GLfloat *v);
MGL_EXPORT void glMultiTexCoord2fARB(GLenum texture, GLfloat s, GLfloat t);
MGL_EXPORT void glMultiTexCoord2fvARB(GLenum texture, const GLfloat *v);
MGL_EXPORT void glMultiTexCoord3fARB(GLenum texture, GLfloat s, GLfloat t, GLfloat r);
MGL_EXPORT void glMultiTexCoord3fvARB(GLenum texture, const GLfloat *v);
MGL_EXPORT void glMultiTexCoord4fARB(GLenum texture, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
MGL_EXPORT void glMultiTexCoord4fvARB(GLenum texture, const GLfloat *v);
MGL_EXPORT void glVertexAttribI1iEXT(GLuint index, GLint x);
MGL_EXPORT void glVertexAttribI2iEXT(GLuint index, GLint x, GLint y);
MGL_EXPORT void glVertexAttribI3iEXT(GLuint index, GLint x, GLint y, GLint z);
MGL_EXPORT void glVertexAttribI4iEXT(GLuint index, GLint x, GLint y, GLint z, GLint w);
MGL_EXPORT void glVertexAttribI1uiEXT(GLuint index, GLuint x);
MGL_EXPORT void glVertexAttribI2uiEXT(GLuint index, GLuint x, GLuint y);
MGL_EXPORT void glVertexAttribI3uiEXT(GLuint index, GLuint x, GLuint y, GLuint z);
MGL_EXPORT void glVertexAttribI4uiEXT(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
MGL_EXPORT void glVertexAttribI1ivEXT(GLuint index, const GLint *v);
MGL_EXPORT void glVertexAttribI2ivEXT(GLuint index, const GLint *v);
MGL_EXPORT void glVertexAttribI3ivEXT(GLuint index, const GLint *v);
MGL_EXPORT void glVertexAttribI4ivEXT(GLuint index, const GLint *v);
MGL_EXPORT void glVertexAttribI1uivEXT(GLuint index, const GLuint *v);
MGL_EXPORT void glVertexAttribI2uivEXT(GLuint index, const GLuint *v);
MGL_EXPORT void glVertexAttribI3uivEXT(GLuint index, const GLuint *v);
MGL_EXPORT void glVertexAttribI4uivEXT(GLuint index, const GLuint *v);
MGL_EXPORT void glVertexAttribIPointerEXT(GLuint index, GLint size, GLenum type, GLsizei stride,
                                          const void *pointer);
MGL_EXPORT void glGetVertexAttribIivEXT(GLuint index, GLenum pname, GLint *params);
MGL_EXPORT void glGetVertexAttribIuivEXT(GLuint index, GLenum pname, GLuint *params);
MGL_EXPORT void glFogCoordfEXT(GLfloat coord);
MGL_EXPORT void glFogCoordfvEXT(const GLfloat *coord);
MGL_EXPORT void glFogCoorddEXT(GLdouble coord);
MGL_EXPORT void glFogCoorddvEXT(const GLdouble *coord);
MGL_EXPORT void glFogCoordPointerEXT(GLenum type, GLsizei stride, const void *pointer);
MGL_EXPORT void glSecondaryColor3fEXT(GLfloat r, GLfloat g, GLfloat b);
MGL_EXPORT void glSecondaryColor3fvEXT(const GLfloat *v);
MGL_EXPORT void glSecondaryColorPointerEXT(GLint size, GLenum type, GLsizei stride,
                                           const void *pointer);
MGL_EXPORT GLenum glGetError(void);
MGL_EXPORT GLenum glGetGraphicsResetStatus(void);
MGL_EXPORT const GLubyte *glGetString(GLenum name);
MGL_EXPORT const GLubyte *glGetStringi(GLenum name, GLuint index);
MGL_EXPORT void glGetBooleanv(GLenum pname, GLboolean *data);
MGL_EXPORT void glGetBooleani_v(GLenum target, GLuint index, GLboolean *data);
MGL_EXPORT void glGetBooleanIndexedv(GLenum target, GLuint index, GLboolean *data);
MGL_EXPORT void glGetBooleanIndexedvEXT(GLenum target, GLuint index, GLboolean *data);
MGL_EXPORT void glGetIntegerv(GLenum pname, GLint *data);
MGL_EXPORT void glClipControl(GLenum origin, GLenum depth);
MGL_EXPORT void glDepthBoundsEXT(GLclampd zmin, GLclampd zmax);
MGL_EXPORT void glPatchParameteri(GLenum pname, GLint value);
MGL_EXPORT void glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname,
                                      GLsizei bufSize, GLint *params);
MGL_EXPORT void glGetInternalformati64v(GLenum target, GLenum internalformat, GLenum pname,
                                        GLsizei bufSize, GLint64 *params);
MGL_EXPORT void glGetInteger64v(GLenum pname, GLint64 *params);
MGL_EXPORT void glGetIntegeri_v(GLenum target, GLuint index, GLint *data);
MGL_EXPORT void glGetIntegerIndexedv(GLenum target, GLuint index, GLint *data);
MGL_EXPORT void glGetIntegerIndexedvEXT(GLenum target, GLuint index, GLint *data);
MGL_EXPORT void glGetInteger64i_v(GLenum target, GLuint index, GLint64 *data);
MGL_EXPORT void glGetFloatv(GLenum pname, GLfloat *data);
MGL_EXPORT void glGetFixedv(GLenum pname, GLfixed *data);
MGL_EXPORT void glGetDoublev(GLenum pname, GLdouble *data);
MGL_EXPORT void glGetFloatIndexedv(GLenum target, GLuint index, GLfloat *data);
MGL_EXPORT void glGetFloatIndexedvEXT(GLenum target, GLuint index, GLfloat *data);
MGL_EXPORT void glGetDoubleIndexedv(GLenum target, GLuint index, GLdouble *data);
MGL_EXPORT void glGetDoubleIndexedvEXT(GLenum target, GLuint index, GLdouble *data);
MGL_EXPORT GLboolean glIsEnabled(GLenum cap);
MGL_EXPORT void glEnable(GLenum cap);
MGL_EXPORT void glDisable(GLenum cap);
MGL_EXPORT GLboolean glIsEnabledi(GLenum cap, GLuint index);
MGL_EXPORT void glEnablei(GLenum cap, GLuint index);
MGL_EXPORT void glDisablei(GLenum cap, GLuint index);
MGL_EXPORT GLboolean glIsEnabledIndexedEXT(GLenum cap, GLuint index);
MGL_EXPORT void glEnableIndexed(GLenum cap, GLuint index);
MGL_EXPORT void glDisableIndexed(GLenum cap, GLuint index);
MGL_EXPORT GLboolean glIsEnabledIndexed(GLenum cap, GLuint index);
MGL_EXPORT void glEnableIndexedEXT(GLenum cap, GLuint index);
MGL_EXPORT void glDisableIndexedEXT(GLenum cap, GLuint index);
MGL_EXPORT void glPushAttrib(GLbitfield mask);
MGL_EXPORT void glPopAttrib(void);
MGL_EXPORT void glPushClientAttrib(GLbitfield mask);
MGL_EXPORT void glPopClientAttrib(void);
MGL_EXPORT void glClientAttribDefaultEXT(GLbitfield mask);
MGL_EXPORT void glPushClientAttribDefaultEXT(GLbitfield mask);
MGL_EXPORT void glGetPointerv(GLenum pname, void **params);
MGL_EXPORT void glEdgeFlagPointer(GLsizei stride, const void *pointer);
MGL_EXPORT void glIndexPointer(GLenum type, GLsizei stride, const void *pointer);
MGL_EXPORT void glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count,
                                      const GLuint *ids, GLboolean enabled);
MGL_EXPORT void glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity,
                                     GLsizei length, const GLchar *buf);
MGL_EXPORT void glDebugMessageCallback(GLDEBUGPROC callback, const void *userParam);
MGL_EXPORT GLuint glGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum *sources,
                                       GLenum *types, GLuint *ids, GLenum *severities,
                                       GLsizei *lengths, GLchar *messageLog);
MGL_EXPORT void glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message);
MGL_EXPORT void glPopDebugGroup(void);
MGL_EXPORT void glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label);
MGL_EXPORT void glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length,
                                 GLchar *label);
MGL_EXPORT void glObjectPtrLabel(const void *ptr, GLsizei length, const GLchar *label);
MGL_EXPORT void glGetObjectPtrLabel(const void *ptr, GLsizei bufSize, GLsizei *length,
                                    GLchar *label);

MGL_EXPORT void glClearIndex(GLfloat index);
MGL_EXPORT void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
MGL_EXPORT void glClearColorx(GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha);
MGL_EXPORT void glClearDepth(GLdouble depth);
MGL_EXPORT void glClearDepthf(GLfloat depth);
MGL_EXPORT void glClearDepthx(GLclampx depth);
MGL_EXPORT void glClearStencil(GLint s);
MGL_EXPORT void glAccum(GLenum op, GLfloat value);
MGL_EXPORT void glFeedbackBuffer(GLsizei size, GLenum type, GLfloat *buffer);
MGL_EXPORT void glPassThrough(GLfloat token);
MGL_EXPORT void glEdgeFlag(GLboolean flag);
MGL_EXPORT void glEdgeFlagv(const GLboolean *flag);
MGL_EXPORT void glLineStipple(GLint factor, GLushort pattern);
MGL_EXPORT void glPolygonStipple(const GLubyte *mask);
MGL_EXPORT void glGetPolygonStipple(GLubyte *mask);
MGL_EXPORT void glClearAccum(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
MGL_EXPORT void glClear(GLbitfield mask);
MGL_EXPORT void glDrawBuffer(GLenum buf);
MGL_EXPORT void glDrawBuffers(GLsizei n, const GLenum *bufs);
MGL_EXPORT void glReadBuffer(GLenum src);
MGL_EXPORT void glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value);
MGL_EXPORT void glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value);
MGL_EXPORT void glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value);
MGL_EXPORT void glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
MGL_EXPORT void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
MGL_EXPORT void glViewportArrayv(GLuint first, GLsizei count, const GLfloat *v);
MGL_EXPORT void glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h);
MGL_EXPORT void glViewportIndexedfv(GLuint index, const GLfloat *v);
MGL_EXPORT void glScissorArrayv(GLuint first, GLsizei count, const GLint *v);
MGL_EXPORT void glScissorIndexed(GLuint index, GLint x, GLint y, GLsizei w, GLsizei h);
MGL_EXPORT void glScissorIndexedv(GLuint index, const GLint *v);
MGL_EXPORT void glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble *v);
MGL_EXPORT void glDepthRangeIndexed(GLuint index, GLdouble n, GLdouble f);
MGL_EXPORT void glGetFloati_v(GLenum target, GLuint index, GLfloat *data);
MGL_EXPORT void glGetDoublei_v(GLenum target, GLuint index, GLdouble *data);
MGL_EXPORT void glDepthRange(GLdouble nearVal, GLdouble farVal);
MGL_EXPORT void glDepthRangef(GLfloat nearVal, GLfloat farVal);
MGL_EXPORT void glDepthRangex(GLclampx nearVal, GLclampx farVal);
MGL_EXPORT void glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
MGL_EXPORT void glPointSize(GLfloat size);
MGL_EXPORT void glLineWidth(GLfloat width);
MGL_EXPORT void glLogicOp(GLenum opcode);
MGL_EXPORT void glPointParameterf(GLenum pname, GLfloat param);
MGL_EXPORT void glPointParameterfv(GLenum pname, const GLfloat *params);
MGL_EXPORT void glPointParameterfARB(GLenum pname, GLfloat param);
MGL_EXPORT void glPointParameterfvARB(GLenum pname, const GLfloat *params);
MGL_EXPORT void glPointParameterfEXT(GLenum pname, GLfloat param);
MGL_EXPORT void glPointParameterfvEXT(GLenum pname, const GLfloat *params);
MGL_EXPORT void glPointParameteri(GLenum pname, GLint param);
MGL_EXPORT void glPointParameteriv(GLenum pname, const GLint *params);
MGL_EXPORT void glLineWidthx(GLfixed width);
MGL_EXPORT void glClampColor(GLenum target, GLenum clamp);
MGL_EXPORT void glClampColorARB(GLenum target, GLenum clamp);
MGL_EXPORT void glPolygonMode(GLenum face, GLenum mode);
MGL_EXPORT void glPolygonOffset(GLfloat factor, GLfloat units);
MGL_EXPORT void glPolygonOffsetClamp(GLfloat factor, GLfloat units, GLfloat clamp);
MGL_EXPORT void glPolygonOffsetClampEXT(GLfloat factor, GLfloat units, GLfloat clamp);
MGL_EXPORT void glGetMultisamplefv(GLenum pname, GLuint index, GLfloat *val);
MGL_EXPORT void glBlendBarrier(void);
MGL_EXPORT void glBlendBarrierKHR(void);
MGL_EXPORT void glPrimitiveBoundingBox(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
                                       GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW);
MGL_EXPORT void glPrimitiveBoundingBoxARB(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
                                          GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW);
MGL_EXPORT void glPrimitiveBoundingBoxEXT(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
                                          GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW);
MGL_EXPORT void glPrimitiveBoundingBoxOES(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
                                          GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW);
MGL_EXPORT void glMinSampleShading(GLfloat value);
MGL_EXPORT void glMinSampleShadingARB(GLfloat value);
MGL_EXPORT void glPatchParameterfv(GLenum pname, const GLfloat *values);
MGL_EXPORT void glPolygonOffsetx(GLfixed factor, GLfixed units);
MGL_EXPORT void glSampleMaski(GLuint maskNumber, GLbitfield mask);
MGL_EXPORT void glSampleCoverage(GLfloat value, GLboolean invert);
MGL_EXPORT void glSampleCoveragex(GLclampx value, GLboolean invert);
MGL_EXPORT void glHint(GLenum target, GLenum mode);
MGL_EXPORT void glAlphaFunc(GLenum func, GLclampf ref);
MGL_EXPORT void glAlphaFuncx(GLenum func, GLclampx ref);
MGL_EXPORT void glFogf(GLenum pname, GLfloat param);
MGL_EXPORT void glFogi(GLenum pname, GLint param);
MGL_EXPORT void glFogfv(GLenum pname, const GLfloat *params);
MGL_EXPORT void glFogiv(GLenum pname, const GLint *params);
MGL_EXPORT void glFogCoordf(GLfloat coord);
MGL_EXPORT void glFogCoordfv(const GLfloat *coord);
MGL_EXPORT void glFogCoordd(GLdouble coord);
MGL_EXPORT void glFogCoorddv(const GLdouble *coord);
MGL_EXPORT void glFogx(GLenum pname, GLfixed param);
MGL_EXPORT void glFogxv(GLenum pname, const GLfixed *params);
MGL_EXPORT void glLightf(GLenum light, GLenum pname, GLfloat param);
MGL_EXPORT void glLighti(GLenum light, GLenum pname, GLint param);
MGL_EXPORT void glLightfv(GLenum light, GLenum pname, const GLfloat *params);
MGL_EXPORT void glLightiv(GLenum light, GLenum pname, const GLint *params);
MGL_EXPORT void glGetLightfv(GLenum light, GLenum pname, GLfloat *params);
MGL_EXPORT void glGetLightiv(GLenum light, GLenum pname, GLint *params);
MGL_EXPORT void glLightx(GLenum light, GLenum pname, GLfixed param);
MGL_EXPORT void glLightxv(GLenum light, GLenum pname, const GLfixed *params);
MGL_EXPORT void glGetLightxv(GLenum light, GLenum pname, GLfixed *params);
MGL_EXPORT void glLightModelf(GLenum pname, GLfloat param);
MGL_EXPORT void glLightModeli(GLenum pname, GLint param);
MGL_EXPORT void glLightModelfv(GLenum pname, const GLfloat *params);
MGL_EXPORT void glLightModeliv(GLenum pname, const GLint *params);
MGL_EXPORT void glLightModelx(GLenum pname, GLfixed param);
MGL_EXPORT void glLightModelxv(GLenum pname, const GLfixed *params);
MGL_EXPORT void glMaterialf(GLenum face, GLenum pname, GLfloat param);
MGL_EXPORT void glMateriali(GLenum face, GLenum pname, GLint param);
MGL_EXPORT void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params);
MGL_EXPORT void glMaterialiv(GLenum face, GLenum pname, const GLint *params);
MGL_EXPORT void glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params);
MGL_EXPORT void glGetMaterialiv(GLenum face, GLenum pname, GLint *params);
MGL_EXPORT void glMaterialx(GLenum face, GLenum pname, GLfixed param);
MGL_EXPORT void glMaterialxv(GLenum face, GLenum pname, const GLfixed *params);
MGL_EXPORT void glGetMaterialxv(GLenum face, GLenum pname, GLfixed *params);
MGL_EXPORT void glColorMaterial(GLenum face, GLenum mode);
MGL_EXPORT void glShadeModel(GLenum mode);
MGL_EXPORT void glTexGenf(GLenum coord, GLenum pname, GLfloat param);
MGL_EXPORT void glTexGeni(GLenum coord, GLenum pname, GLint param);
MGL_EXPORT void glTexGend(GLenum coord, GLenum pname, GLdouble param);
MGL_EXPORT void glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params);
MGL_EXPORT void glTexGeniv(GLenum coord, GLenum pname, const GLint *params);
MGL_EXPORT void glTexGendv(GLenum coord, GLenum pname, const GLdouble *params);
MGL_EXPORT void glGetTexGenfv(GLenum coord, GLenum pname, GLfloat *params);
MGL_EXPORT void glGetTexGeniv(GLenum coord, GLenum pname, GLint *params);
MGL_EXPORT void glGetTexGendv(GLenum coord, GLenum pname, GLdouble *params);
MGL_EXPORT void glMultiTexEnvfEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat param);
MGL_EXPORT void glMultiTexEnvfvEXT(GLenum texunit, GLenum target, GLenum pname,
                                   const GLfloat *params);
MGL_EXPORT void glMultiTexEnviEXT(GLenum texunit, GLenum target, GLenum pname, GLint param);
MGL_EXPORT void glMultiTexEnvivEXT(GLenum texunit, GLenum target, GLenum pname,
                                   const GLint *params);
MGL_EXPORT void glGetMultiTexEnvfvEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat *params);
MGL_EXPORT void glGetMultiTexEnvivEXT(GLenum texunit, GLenum target, GLenum pname, GLint *params);
MGL_EXPORT void glMultiTexGendEXT(GLenum texunit, GLenum coord, GLenum pname, GLdouble param);
MGL_EXPORT void glMultiTexGendvEXT(GLenum texunit, GLenum coord, GLenum pname,
                                   const GLdouble *params);
MGL_EXPORT void glMultiTexGenfEXT(GLenum texunit, GLenum coord, GLenum pname, GLfloat param);
MGL_EXPORT void glMultiTexGenfvEXT(GLenum texunit, GLenum coord, GLenum pname,
                                   const GLfloat *params);
MGL_EXPORT void glMultiTexGeniEXT(GLenum texunit, GLenum coord, GLenum pname, GLint param);
MGL_EXPORT void glMultiTexGenivEXT(GLenum texunit, GLenum coord, GLenum pname, const GLint *params);
MGL_EXPORT void glGetMultiTexGendvEXT(GLenum texunit, GLenum coord, GLenum pname, GLdouble *params);
MGL_EXPORT void glGetMultiTexGenfvEXT(GLenum texunit, GLenum coord, GLenum pname, GLfloat *params);
MGL_EXPORT void glGetMultiTexGenivEXT(GLenum texunit, GLenum coord, GLenum pname, GLint *params);
MGL_EXPORT void glClipPlane(GLenum plane, const GLdouble *equation);
MGL_EXPORT void glClipPlanef(GLenum plane, const GLfloat *equation);
MGL_EXPORT void glClipPlanex(GLenum plane, const GLfixed *equation);
MGL_EXPORT void glGetClipPlane(GLenum plane, GLdouble *equation);
MGL_EXPORT void glGetClipPlanef(GLenum plane, GLfloat *equation);
MGL_EXPORT void glGetClipPlanex(GLenum plane, GLfixed *equation);
MGL_EXPORT void glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
MGL_EXPORT void glDepthFunc(GLenum func);
MGL_EXPORT void glDepthMask(GLboolean flag);
MGL_EXPORT void glActiveStencilFaceEXT(GLenum face);
MGL_EXPORT void glStencilFunc(GLenum func, GLint ref, GLuint mask);
MGL_EXPORT void glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
MGL_EXPORT void glStencilMask(GLuint mask);
MGL_EXPORT void glStencilMaskSeparate(GLenum face, GLuint mask);
MGL_EXPORT void glStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass);
MGL_EXPORT void glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
MGL_EXPORT void glStencilOpSeparateATI(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
MGL_EXPORT void glStencilFuncSeparateATI(GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask);
MGL_EXPORT void glIndexMask(GLuint mask);
MGL_EXPORT void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
MGL_EXPORT void glColorMaski(GLuint index, GLboolean red, GLboolean green, GLboolean blue,
                             GLboolean alpha);
MGL_EXPORT void glColorMaskIndexedEXT(GLuint index, GLboolean red, GLboolean green, GLboolean blue,
                                      GLboolean alpha);
MGL_EXPORT void glCullFace(GLenum mode);
MGL_EXPORT void glFrontFace(GLenum mode);
MGL_EXPORT void glBlendFunc(GLenum sfactor, GLenum dfactor);
MGL_EXPORT void glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
MGL_EXPORT void glBlendFunci(GLuint index, GLenum src, GLenum dst);
MGL_EXPORT void glBlendFuncSeparatei(GLuint index, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
                                     GLenum dstAlpha);
MGL_EXPORT void glBlendFunciARB(GLuint index, GLenum src, GLenum dst);
MGL_EXPORT void glBlendFuncSeparateiARB(GLuint index, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
                                        GLenum dstAlpha);
MGL_EXPORT void glBlendFunciEXT(GLuint index, GLenum src, GLenum dst);
MGL_EXPORT void glBlendFuncSeparateiEXT(GLuint index, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
                                        GLenum dstAlpha);
MGL_EXPORT void glBlendEquation(GLenum mode);
MGL_EXPORT void glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
MGL_EXPORT void glBlendEquationi(GLuint index, GLenum mode);
MGL_EXPORT void glBlendEquationSeparatei(GLuint index, GLenum modeRGB, GLenum modeAlpha);
MGL_EXPORT void glBlendEquationiARB(GLuint index, GLenum mode);
MGL_EXPORT void glBlendEquationSeparateiARB(GLuint index, GLenum modeRGB, GLenum modeAlpha);
MGL_EXPORT void glBlendEquationiEXT(GLuint index, GLenum mode);
MGL_EXPORT void glBlendEquationSeparateiEXT(GLuint index, GLenum modeRGB, GLenum modeAlpha);
MGL_EXPORT void glFlush(void);
MGL_EXPORT void glFinish(void);

MGL_EXPORT void glGenBuffers(GLsizei n, GLuint *buffers);
MGL_EXPORT void glDeleteBuffers(GLsizei n, const GLuint *buffers);
MGL_EXPORT void glBindBuffer(GLenum target, GLuint buffer);
MGL_EXPORT GLboolean glIsBuffer(GLuint buffer);
MGL_EXPORT void glBindBufferBase(GLenum target, GLuint index, GLuint buffer);
MGL_EXPORT void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset,
                                  GLsizeiptr size);
MGL_EXPORT void glBindBuffersBase(GLenum target, GLuint first, GLsizei count,
                                  const GLuint *buffers);
MGL_EXPORT void glBindBuffersRange(GLenum target, GLuint first, GLsizei count,
                                   const GLuint *buffers, const GLintptr *offsets,
                                   const GLsizeiptr *sizes);
MGL_EXPORT void glBindBufferBaseARB(GLenum target, GLuint index, GLuint buffer);
MGL_EXPORT void glBindBufferRangeARB(GLenum target, GLuint index, GLuint buffer, GLintptr offset,
                                     GLsizeiptr size);
MGL_EXPORT void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
MGL_EXPORT void glBufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags);
MGL_EXPORT void glBufferStorageEXT(GLenum target, GLsizeiptr size, const void *data,
                                   GLbitfield flags);
MGL_EXPORT void glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *params);
MGL_EXPORT void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
MGL_EXPORT void glClearBufferData(GLenum target, GLenum internalformat, GLenum format, GLenum type,
                                  const void *data);
MGL_EXPORT void glClearNamedBufferData(GLuint buffer, GLenum internalformat, GLenum format,
                                       GLenum type, const void *data);
MGL_EXPORT void glClearNamedBufferSubData(GLuint buffer, GLenum internalformat, GLintptr offset,
                                          GLsizeiptr size, GLenum format, GLenum type,
                                          const void *data);
MGL_EXPORT void glClearBufferSubData(GLenum target, GLenum internalformat, GLintptr offset,
                                     GLsizeiptr size, GLenum format, GLenum type, const void *data);
MGL_EXPORT void glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data);
MGL_EXPORT void *glMapBuffer(GLenum target, GLenum access);
MGL_EXPORT void *glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
                                  GLbitfield access);
MGL_EXPORT void glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length);
MGL_EXPORT GLboolean glUnmapBuffer(GLenum target);
MGL_EXPORT void glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params);
MGL_EXPORT void glGetBufferPointerv(GLenum target, GLenum pname, void **params);
MGL_EXPORT void glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset,
                                    GLintptr writeOffset, GLsizeiptr size);
MGL_EXPORT void glCreateBuffers(GLsizei n, GLuint *buffers);
MGL_EXPORT void glNamedBufferData(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage);
MGL_EXPORT void glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void *data,
                                     GLbitfield flags);
MGL_EXPORT void glNamedBufferStorageEXT(GLuint buffer, GLsizeiptr size, const void *data,
                                        GLbitfield flags);
MGL_EXPORT void glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                     const void *data);
MGL_EXPORT void glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                        void *data);
MGL_EXPORT void *glMapNamedBuffer(GLuint buffer, GLenum access);
MGL_EXPORT void *glMapNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length,
                                       GLbitfield access);
MGL_EXPORT void glFlushMappedNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length);
MGL_EXPORT GLboolean glUnmapNamedBuffer(GLuint buffer);
MGL_EXPORT void glGetNamedBufferParameteriv(GLuint buffer, GLenum pname, GLint *params);
MGL_EXPORT void glGetNamedBufferParameteri64v(GLuint buffer, GLenum pname, GLint64 *params);
MGL_EXPORT void glGetNamedBufferPointerv(GLuint buffer, GLenum pname, void **params);
MGL_EXPORT void glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset,
                                         GLintptr writeOffset, GLsizeiptr size);
MGL_EXPORT void glNamedBufferDataEXT(GLuint buffer, GLsizeiptr size, const void *data,
                                     GLenum usage);
MGL_EXPORT void glNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                        const void *data);
MGL_EXPORT void glGetNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                           void *data);
MGL_EXPORT void *glMapNamedBufferEXT(GLuint buffer, GLenum access);
MGL_EXPORT void *glMapNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length,
                                          GLbitfield access);
MGL_EXPORT void glFlushMappedNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length);
MGL_EXPORT GLboolean glUnmapNamedBufferEXT(GLuint buffer);
MGL_EXPORT void glGetNamedBufferParameterivEXT(GLuint buffer, GLenum pname, GLint *params);
MGL_EXPORT void glGetNamedBufferPointervEXT(GLuint buffer, GLenum pname, void **params);

MGL_EXPORT void glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset,
                                   GLsizei stride);
MGL_EXPORT void glBindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers,
                                    const GLintptr *offsets, const GLsizei *strides);
MGL_EXPORT void glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type,
                                     GLboolean normalized, GLuint relativeoffset);
MGL_EXPORT void glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type,
                                      GLuint relativeoffset);
MGL_EXPORT void glVertexAttribBinding(GLuint attribindex, GLuint bindingindex);
MGL_EXPORT void glVertexBindingDivisor(GLuint bindingindex, GLuint divisor);
MGL_EXPORT void glVertexArrayVertexBuffer(GLuint vaobj, GLuint bindingindex, GLuint buffer,
                                          GLintptr offset, GLsizei stride);
MGL_EXPORT void glVertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count,
                                           const GLuint *buffers, const GLintptr *offsets,
                                           const GLsizei *strides);
MGL_EXPORT void glVertexArrayAttribFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type,
                                          GLboolean normalized, GLuint relativeoffset);
MGL_EXPORT void glVertexArrayAttribIFormat(GLuint vaobj, GLuint attribindex, GLint size,
                                           GLenum type, GLuint relativeoffset);
MGL_EXPORT void glVertexArrayAttribBinding(GLuint vaobj, GLuint attribindex, GLuint bindingindex);
MGL_EXPORT void glVertexArrayBindingDivisor(GLuint vaobj, GLuint bindingindex, GLuint divisor);
MGL_EXPORT void glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer);
MGL_EXPORT void glGetVertexArrayiv(GLuint vaobj, GLenum pname, GLint *param);
MGL_EXPORT void glGetVertexArrayIndexediv(GLuint vaobj, GLuint index, GLenum pname, GLint *param);
MGL_EXPORT void glGetVertexArrayIndexed64iv(GLuint vaobj, GLuint index, GLenum pname,
                                            GLint64 *param);
MGL_EXPORT void glGenVertexArrays(GLsizei n, GLuint *arrays);
MGL_EXPORT void glCreateVertexArrays(GLsizei n, GLuint *arrays);
MGL_EXPORT void glDeleteVertexArrays(GLsizei n, const GLuint *arrays);
MGL_EXPORT void glBindVertexArray(GLuint array);
MGL_EXPORT GLboolean glIsVertexArray(GLuint array);
MGL_EXPORT void glEnableVertexArrayAttrib(GLuint vaobj, GLuint index);
MGL_EXPORT void glDisableVertexArrayAttrib(GLuint vaobj, GLuint index);
MGL_EXPORT void glEnableVertexArrayAttribEXT(GLuint vaobj, GLuint index);
MGL_EXPORT void glDisableVertexArrayAttribEXT(GLuint vaobj, GLuint index);

MGL_EXPORT void glGenFramebuffers(GLsizei n, GLuint *framebuffers);
MGL_EXPORT void glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers);
MGL_EXPORT void glBindFramebuffer(GLenum target, GLuint framebuffer);
MGL_EXPORT GLboolean glIsFramebuffer(GLuint framebuffer);
MGL_EXPORT GLenum glCheckFramebufferStatus(GLenum target);
MGL_EXPORT void glFramebufferRenderbuffer(GLenum target, GLenum attachment,
                                          GLenum renderbuffertarget, GLuint renderbuffer);
MGL_EXPORT void glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment,
                                                      GLenum pname, GLint *params);
MGL_EXPORT void glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
                                  GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask,
                                  GLenum filter);
MGL_EXPORT void glCreateFramebuffers(GLsizei n, GLuint *framebuffers);
MGL_EXPORT void glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                          const GLint *value);
MGL_EXPORT void glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                           const GLuint *value);
MGL_EXPORT void glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                          const GLfloat *value);
MGL_EXPORT void glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                          GLfloat depth, GLint stencil);
MGL_EXPORT void glClearNamedFramebufferivEXT(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                             const GLint *value);
MGL_EXPORT void glClearNamedFramebufferuivEXT(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                              const GLuint *value);
MGL_EXPORT void glClearNamedFramebufferfvEXT(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                             const GLfloat *value);
MGL_EXPORT void glClearNamedFramebufferfiEXT(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                             GLfloat depth, GLint stencil);
MGL_EXPORT GLenum glCheckNamedFramebufferStatus(GLuint framebuffer, GLenum target);
MGL_EXPORT void glNamedFramebufferDrawBuffer(GLuint framebuffer, GLenum buf);
MGL_EXPORT void glNamedFramebufferDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum *bufs);
MGL_EXPORT void glNamedFramebufferReadBuffer(GLuint framebuffer, GLenum src);
MGL_EXPORT void glNamedFramebufferParameteri(GLuint framebuffer, GLenum pname, GLint param);
MGL_EXPORT void glGetNamedFramebufferParameteriv(GLuint framebuffer, GLenum pname, GLint *params);
MGL_EXPORT void glNamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture,
                                          GLint level);
MGL_EXPORT void glNamedFramebufferTextureLayer(GLuint framebuffer, GLenum attachment,
                                               GLuint texture, GLint level, GLint layer);
MGL_EXPORT void glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0,
                                       GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
                                       GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask,
                                       GLenum filter);
MGL_EXPORT void glInvalidateFramebuffer(GLenum target, GLsizei numAttachments,
                                        const GLenum *attachments);
MGL_EXPORT void glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments,
                                           const GLenum *attachments, GLint x, GLint y,
                                           GLsizei width, GLsizei height);
MGL_EXPORT void glInvalidateNamedFramebufferData(GLuint framebuffer, GLsizei numAttachments,
                                                 const GLenum *attachments);
MGL_EXPORT void glInvalidateNamedFramebufferSubData(GLuint framebuffer, GLsizei numAttachments,
                                                    const GLenum *attachments, GLint x, GLint y,
                                                    GLsizei width, GLsizei height);
MGL_EXPORT void glNamedFramebufferRenderbuffer(GLuint framebuffer, GLenum attachment,
                                               GLenum renderbuffertarget, GLuint renderbuffer);
MGL_EXPORT void glGetNamedFramebufferAttachmentParameteriv(GLuint framebuffer, GLenum attachment,
                                                           GLenum pname, GLint *params);
MGL_EXPORT void glCreateRenderbuffers(GLsizei n, GLuint *renderbuffers);
MGL_EXPORT void glNamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat,
                                           GLsizei width, GLsizei height);
MGL_EXPORT void glNamedRenderbufferStorageMultisample(GLuint renderbuffer, GLsizei samples,
                                                      GLenum internalformat, GLsizei width,
                                                      GLsizei height);
MGL_EXPORT void glGetNamedRenderbufferParameteriv(GLuint renderbuffer, GLenum pname, GLint *params);

MGL_EXPORT void glGenRenderbuffers(GLsizei n, GLuint *renderbuffers);
MGL_EXPORT void glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers);
MGL_EXPORT void glBindRenderbuffer(GLenum target, GLuint renderbuffer);
MGL_EXPORT GLboolean glIsRenderbuffer(GLuint renderbuffer);
MGL_EXPORT void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width,
                                      GLsizei height);
MGL_EXPORT void glRenderbufferStorageMultisample(GLenum target, GLsizei samples,
                                                 GLenum internalformat, GLsizei width,
                                                 GLsizei height);
MGL_EXPORT void glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params);

MGL_EXPORT GLuint glCreateShader(GLenum type);
MGL_EXPORT void glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string,
                               const GLint *length);
MGL_EXPORT void glCompileShader(GLuint shader);
MGL_EXPORT void glShaderBinary(GLsizei count, const GLuint *shaders, GLenum binaryFormat,
                               const void *binary, GLsizei length);
MGL_EXPORT void glSpecializeShader(GLuint shader, const GLchar *entryPoint,
                                   GLuint numSpecializationConstants, const GLuint *constantIndex,
                                   const GLuint *constantValue);
MGL_EXPORT void glReleaseShaderCompiler(void);
MGL_EXPORT void glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint *range,
                                           GLint *precision);
MGL_EXPORT void glGetShaderiv(GLuint shader, GLenum pname, GLint *params);
MGL_EXPORT void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length,
                                   GLchar *infoLog);
MGL_EXPORT void glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source);
MGL_EXPORT void glDeleteShader(GLuint shader);
MGL_EXPORT GLboolean glIsShader(GLuint shader);

MGL_EXPORT void glShaderSourceARB(GLhandleARB shader, GLsizei count,
                                  const GLcharARB *const *strings, const GLint *lengths);
MGL_EXPORT void glCompileShaderARB(GLhandleARB shader);
MGL_EXPORT void glLinkProgramARB(GLhandleARB program);
MGL_EXPORT void glValidateProgramARB(GLhandleARB program);
MGL_EXPORT void glBindAttribLocationARB(GLhandleARB program, GLuint index, const GLcharARB *name);
MGL_EXPORT void glGetActiveUniformARB(GLhandleARB program, GLuint index, GLsizei bufSize,
                                      GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
MGL_EXPORT GLint glGetAttribLocationARB(GLhandleARB program, const GLcharARB *name);
MGL_EXPORT GLint glGetUniformLocationARB(GLhandleARB program, const GLcharARB *name);
MGL_EXPORT void glGetUniformfvARB(GLhandleARB program, GLint location, GLfloat *params);
MGL_EXPORT void glGetUniformivARB(GLhandleARB program, GLint location, GLint *params);
MGL_EXPORT void glGetShaderSourceARB(GLhandleARB shader, GLsizei bufSize, GLsizei *length,
                                     GLcharARB *source);
MGL_EXPORT void glGetUniformuivEXT(GLuint program, GLint location, GLuint *params);
MGL_EXPORT void glUniform1fARB(GLint location, GLfloat v0);
MGL_EXPORT void glUniform2fARB(GLint location, GLfloat v0, GLfloat v1);
MGL_EXPORT void glUniform3fARB(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
MGL_EXPORT void glUniform4fARB(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
MGL_EXPORT void glUniform1iARB(GLint location, GLint v0);
MGL_EXPORT void glUniform2iARB(GLint location, GLint v0, GLint v1);
MGL_EXPORT void glUniform3iARB(GLint location, GLint v0, GLint v1, GLint v2);
MGL_EXPORT void glUniform4iARB(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
MGL_EXPORT void glUniform1uiEXT(GLint location, GLuint v0);
MGL_EXPORT void glUniform2uiEXT(GLint location, GLuint v0, GLuint v1);
MGL_EXPORT void glUniform3uiEXT(GLint location, GLuint v0, GLuint v1, GLuint v2);
MGL_EXPORT void glUniform4uiEXT(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
MGL_EXPORT void glUniform1fvARB(GLint location, GLsizei count, const GLfloat *values);
MGL_EXPORT void glUniform2fvARB(GLint location, GLsizei count, const GLfloat *values);
MGL_EXPORT void glUniform3fvARB(GLint location, GLsizei count, const GLfloat *values);
MGL_EXPORT void glUniform4fvARB(GLint location, GLsizei count, const GLfloat *values);
MGL_EXPORT void glUniform1ivARB(GLint location, GLsizei count, const GLint *values);
MGL_EXPORT void glUniform2ivARB(GLint location, GLsizei count, const GLint *values);
MGL_EXPORT void glUniform3ivARB(GLint location, GLsizei count, const GLint *values);
MGL_EXPORT void glUniform4ivARB(GLint location, GLsizei count, const GLint *values);
MGL_EXPORT void glUniform1uivEXT(GLint location, GLsizei count, const GLuint *values);
MGL_EXPORT void glUniform2uivEXT(GLint location, GLsizei count, const GLuint *values);
MGL_EXPORT void glUniform3uivEXT(GLint location, GLsizei count, const GLuint *values);
MGL_EXPORT void glUniform4uivEXT(GLint location, GLsizei count, const GLuint *values);
MGL_EXPORT void glUniformMatrix2fvARB(GLint location, GLsizei count, GLboolean transpose,
                                      const GLfloat *values);
MGL_EXPORT void glUniformMatrix3fvARB(GLint location, GLsizei count, GLboolean transpose,
                                      const GLfloat *values);
MGL_EXPORT void glUniformMatrix4fvARB(GLint location, GLsizei count, GLboolean transpose,
                                      const GLfloat *values);
MGL_EXPORT void glVertexAttribDivisorARB(GLuint index, GLuint divisor);
MGL_EXPORT void glDrawArraysInstancedARB(GLenum mode, GLint first, GLsizei count,
                                         GLsizei primcount);
MGL_EXPORT void glDrawElementsInstancedARB(GLenum mode, GLsizei count, GLenum type,
                                           const void *indices, GLsizei primcount);
MGL_EXPORT void glDrawBuffersARB(GLsizei count, const GLenum *buffers);
MGL_EXPORT void glTexBufferARB(GLenum target, GLenum internalformat, GLuint buffer);
MGL_EXPORT void glProgramParameteriARB(GLuint program, GLenum pname, GLint value);
MGL_EXPORT GLhandleARB glCreateShaderObjectARB(GLenum shaderType);
MGL_EXPORT GLhandleARB glCreateProgramObjectARB(void);
MGL_EXPORT void glDeleteObjectARB(GLhandleARB obj);
MGL_EXPORT void glAttachObjectARB(GLhandleARB containerObj, GLhandleARB obj);
MGL_EXPORT void glDetachObjectARB(GLhandleARB containerObj, GLhandleARB attachedObj);
MGL_EXPORT void glUseProgramObjectARB(GLhandleARB programObj);
MGL_EXPORT GLhandleARB glGetHandleARB(GLenum pname);
MGL_EXPORT void glGetObjectParameterfvARB(GLhandleARB obj, GLenum pname, GLfloat *params);
MGL_EXPORT void glGetObjectParameterivARB(GLhandleARB obj, GLenum pname, GLint *params);
MGL_EXPORT void glGetInfoLogARB(GLhandleARB obj, GLsizei maxLength, GLsizei *length,
                                GLcharARB *infoLog);
MGL_EXPORT void glGetAttachedObjectsARB(GLhandleARB containerObj, GLsizei maxCount, GLsizei *count,
                                        GLhandleARB *obj);

MGL_EXPORT GLuint glCreateProgram(void);
MGL_EXPORT void glAttachShader(GLuint program, GLuint shader);
MGL_EXPORT void glDetachShader(GLuint program, GLuint shader);
MGL_EXPORT void glBindAttribLocation(GLuint program, GLuint index, const GLchar *name);
MGL_EXPORT void glBindFragDataLocation(GLuint program, GLuint color, const GLchar *name);
MGL_EXPORT void glBindFragDataLocationIndexed(GLuint program, GLuint color, GLuint index,
                                              const GLchar *name);
MGL_EXPORT void glBindFragDataLocationEXT(GLuint program, GLuint color, const GLchar *name);
MGL_EXPORT void glBindFragDataLocationIndexedEXT(GLuint program, GLuint color, GLuint index,
                                                 const GLchar *name);
MGL_EXPORT void glProgramParameteri(GLuint program, GLenum pname, GLint value);
MGL_EXPORT void glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei *length,
                                   GLenum *binaryFormat, void *binary);
MGL_EXPORT void glProgramBinary(GLuint program, GLenum binaryFormat, const void *binary,
                                GLsizei length);
MGL_EXPORT void glLinkProgram(GLuint program);
MGL_EXPORT void glValidateProgram(GLuint program);
MGL_EXPORT void glUseProgram(GLuint program);
MGL_EXPORT void glGetProgramiv(GLuint program, GLenum pname, GLint *params);
MGL_EXPORT void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length,
                                    GLchar *infoLog);
MGL_EXPORT void glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count,
                                     GLuint *shaders);
MGL_EXPORT void glDeleteProgram(GLuint program);
MGL_EXPORT GLboolean glIsProgram(GLuint program);
MGL_EXPORT GLint glGetAttribLocation(GLuint program, const GLchar *name);
MGL_EXPORT GLint glGetUniformLocation(GLuint program, const GLchar *name);
MGL_EXPORT GLint glGetFragDataLocation(GLuint program, const GLchar *name);
MGL_EXPORT GLint glGetFragDataLocationEXT(GLuint program, const GLchar *name);
MGL_EXPORT GLint glGetFragDataIndex(GLuint program, const GLchar *name);
MGL_EXPORT GLint glGetFragDataIndexEXT(GLuint program, const GLchar *name);
MGL_EXPORT void glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length,
                                  GLint *size, GLenum *type, GLchar *name);
MGL_EXPORT void glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length,
                                   GLint *size, GLenum *type, GLchar *name);
MGL_EXPORT void glGetUniformIndices(GLuint program, GLsizei uniformCount,
                                    const GLchar *const *uniformNames, GLuint *uniformIndices);
MGL_EXPORT void glGetActiveUniformsiv(GLuint program, GLsizei uniformCount,
                                      const GLuint *uniformIndices, GLenum pname, GLint *params);
MGL_EXPORT void glGetActiveUniformName(GLuint program, GLuint uniformIndex, GLsizei bufSize,
                                       GLsizei *length, GLchar *uniformName);
MGL_EXPORT GLuint glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName);
MGL_EXPORT void glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname,
                                          GLint *params);
MGL_EXPORT void glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex,
                                            GLsizei bufSize, GLsizei *length,
                                            GLchar *uniformBlockName);
MGL_EXPORT void glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex,
                                      GLuint uniformBlockBinding);
MGL_EXPORT void glShaderStorageBlockBinding(GLuint program, GLuint storageBlockIndex,
                                            GLuint storageBlockBinding);
MGL_EXPORT void glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname,
                                        GLint *params);
MGL_EXPORT GLuint glGetProgramResourceIndex(GLuint program, GLenum programInterface,
                                            const GLchar *name);
MGL_EXPORT void glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index,
                                         GLsizei bufSize, GLsizei *length, GLchar *name);
MGL_EXPORT void glGetActiveAtomicCounterBufferiv(GLuint program, GLuint bufferIndex, GLenum pname,
                                                 GLint *params);
MGL_EXPORT void glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index,
                                       GLsizei propCount, const GLenum *props, GLsizei bufSize,
                                       GLsizei *length, GLint *params);
MGL_EXPORT GLint glGetProgramResourceLocation(GLuint program, GLenum programInterface,
                                              const GLchar *name);
MGL_EXPORT GLint glGetProgramResourceLocationIndex(GLuint program, GLenum programInterface,
                                                   const GLchar *name);
MGL_EXPORT GLint glGetProgramResourceLocationIndexEXT(GLuint program, GLenum programInterface,
                                                      const GLchar *name);
MGL_EXPORT void glUniform1f(GLint location, GLfloat v0);
MGL_EXPORT void glUniform2f(GLint location, GLfloat v0, GLfloat v1);
MGL_EXPORT void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
MGL_EXPORT void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
MGL_EXPORT void glUniform1i(GLint location, GLint v0);
MGL_EXPORT void glUniform2i(GLint location, GLint v0, GLint v1);
MGL_EXPORT void glUniform3i(GLint location, GLint v0, GLint v1, GLint v2);
MGL_EXPORT void glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
MGL_EXPORT void glUniform1ui(GLint location, GLuint v0);
MGL_EXPORT void glUniform2ui(GLint location, GLuint v0, GLuint v1);
MGL_EXPORT void glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2);
MGL_EXPORT void glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
MGL_EXPORT void glUniform1fv(GLint location, GLsizei count, const GLfloat *value);
MGL_EXPORT void glUniform2fv(GLint location, GLsizei count, const GLfloat *value);
MGL_EXPORT void glUniform3fv(GLint location, GLsizei count, const GLfloat *value);
MGL_EXPORT void glUniform4fv(GLint location, GLsizei count, const GLfloat *value);
MGL_EXPORT void glUniform1iv(GLint location, GLsizei count, const GLint *value);
MGL_EXPORT void glUniform2iv(GLint location, GLsizei count, const GLint *value);
MGL_EXPORT void glUniform3iv(GLint location, GLsizei count, const GLint *value);
MGL_EXPORT void glUniform4iv(GLint location, GLsizei count, const GLint *value);
MGL_EXPORT void glUniform1uiv(GLint location, GLsizei count, const GLuint *value);
MGL_EXPORT void glUniform2uiv(GLint location, GLsizei count, const GLuint *value);
MGL_EXPORT void glUniform3uiv(GLint location, GLsizei count, const GLuint *value);
MGL_EXPORT void glUniform4uiv(GLint location, GLsizei count, const GLuint *value);
MGL_EXPORT void glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose,
                                   const GLfloat *value);
MGL_EXPORT void glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose,
                                   const GLfloat *value);
MGL_EXPORT void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
                                   const GLfloat *value);
MGL_EXPORT void glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLfloat *value);
MGL_EXPORT void glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLfloat *value);
MGL_EXPORT void glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLfloat *value);
MGL_EXPORT void glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLfloat *value);
MGL_EXPORT void glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLfloat *value);
MGL_EXPORT void glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLfloat *value);
MGL_EXPORT void glGetUniformfv(GLuint program, GLint location, GLfloat *params);
MGL_EXPORT void glGetUniformdv(GLuint program, GLint location, GLdouble *params);
MGL_EXPORT void glGetUniformiv(GLuint program, GLint location, GLint *params);
MGL_EXPORT void glGetUniformuiv(GLuint program, GLint location, GLuint *params);
MGL_EXPORT void glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params);
MGL_EXPORT void glGetnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint *params);
MGL_EXPORT void glGetnUniformuiv(GLuint program, GLint location, GLsizei bufSize, GLuint *params);
MGL_EXPORT void glGetnUniformdv(GLuint program, GLint location, GLsizei bufSize, GLdouble *params);
MGL_EXPORT void glProgramUniform1f(GLuint program, GLint location, GLfloat v0);
MGL_EXPORT void glProgramUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1);
MGL_EXPORT void glProgramUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1,
                                   GLfloat v2);
MGL_EXPORT void glProgramUniform4f(GLuint program, GLint location, GLfloat v0, GLfloat v1,
                                   GLfloat v2, GLfloat v3);
MGL_EXPORT void glProgramUniform1i(GLuint program, GLint location, GLint v0);
MGL_EXPORT void glProgramUniform2i(GLuint program, GLint location, GLint v0, GLint v1);
MGL_EXPORT void glProgramUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2);
MGL_EXPORT void glProgramUniform4i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2,
                                   GLint v3);
MGL_EXPORT void glProgramUniform1ui(GLuint program, GLint location, GLuint v0);
MGL_EXPORT void glProgramUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1);
MGL_EXPORT void glProgramUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1,
                                    GLuint v2);
MGL_EXPORT void glProgramUniform4ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2,
                                    GLuint v3);
MGL_EXPORT void glProgramUniform1fv(GLuint program, GLint location, GLsizei count,
                                    const GLfloat *value);
MGL_EXPORT void glProgramUniform2fv(GLuint program, GLint location, GLsizei count,
                                    const GLfloat *value);
MGL_EXPORT void glProgramUniform3fv(GLuint program, GLint location, GLsizei count,
                                    const GLfloat *value);
MGL_EXPORT void glProgramUniform4fv(GLuint program, GLint location, GLsizei count,
                                    const GLfloat *value);
MGL_EXPORT void glProgramUniform1iv(GLuint program, GLint location, GLsizei count,
                                    const GLint *value);
MGL_EXPORT void glProgramUniform2iv(GLuint program, GLint location, GLsizei count,
                                    const GLint *value);
MGL_EXPORT void glProgramUniform3iv(GLuint program, GLint location, GLsizei count,
                                    const GLint *value);
MGL_EXPORT void glProgramUniform4iv(GLuint program, GLint location, GLsizei count,
                                    const GLint *value);
MGL_EXPORT void glProgramUniform1uiv(GLuint program, GLint location, GLsizei count,
                                     const GLuint *value);
MGL_EXPORT void glProgramUniform2uiv(GLuint program, GLint location, GLsizei count,
                                     const GLuint *value);
MGL_EXPORT void glProgramUniform3uiv(GLuint program, GLint location, GLsizei count,
                                     const GLuint *value);
MGL_EXPORT void glProgramUniform4uiv(GLuint program, GLint location, GLsizei count,
                                     const GLuint *value);
MGL_EXPORT void glProgramUniformMatrix2fv(GLuint program, GLint location, GLsizei count,
                                          GLboolean transpose, const GLfloat *value);
MGL_EXPORT void glProgramUniformMatrix3fv(GLuint program, GLint location, GLsizei count,
                                          GLboolean transpose, const GLfloat *value);
MGL_EXPORT void glProgramUniformMatrix4fv(GLuint program, GLint location, GLsizei count,
                                          GLboolean transpose, const GLfloat *value);
MGL_EXPORT void glProgramUniformMatrix2x3fv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLfloat *value);
MGL_EXPORT void glProgramUniformMatrix3x2fv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLfloat *value);
MGL_EXPORT void glProgramUniformMatrix2x4fv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLfloat *value);
MGL_EXPORT void glProgramUniformMatrix4x2fv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLfloat *value);
MGL_EXPORT void glProgramUniformMatrix3x4fv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLfloat *value);
MGL_EXPORT void glProgramUniformMatrix4x3fv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLfloat *value);
MGL_EXPORT void glProgramUniform1fEXT(GLuint program, GLint location, GLfloat v0);
MGL_EXPORT void glProgramUniform2fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1);
MGL_EXPORT void glProgramUniform3fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1,
                                      GLfloat v2);
MGL_EXPORT void glProgramUniform4fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1,
                                      GLfloat v2, GLfloat v3);
MGL_EXPORT void glProgramUniform1iEXT(GLuint program, GLint location, GLint v0);
MGL_EXPORT void glProgramUniform2iEXT(GLuint program, GLint location, GLint v0, GLint v1);
MGL_EXPORT void glProgramUniform3iEXT(GLuint program, GLint location, GLint v0, GLint v1, GLint v2);
MGL_EXPORT void glProgramUniform4iEXT(GLuint program, GLint location, GLint v0, GLint v1, GLint v2,
                                      GLint v3);
MGL_EXPORT void glProgramUniform1fvEXT(GLuint program, GLint location, GLsizei count,
                                       const GLfloat *value);
MGL_EXPORT void glProgramUniform2fvEXT(GLuint program, GLint location, GLsizei count,
                                       const GLfloat *value);
MGL_EXPORT void glProgramUniform3fvEXT(GLuint program, GLint location, GLsizei count,
                                       const GLfloat *value);
MGL_EXPORT void glProgramUniform4fvEXT(GLuint program, GLint location, GLsizei count,
                                       const GLfloat *value);
MGL_EXPORT void glProgramUniform1ivEXT(GLuint program, GLint location, GLsizei count,
                                       const GLint *value);
MGL_EXPORT void glProgramUniform2ivEXT(GLuint program, GLint location, GLsizei count,
                                       const GLint *value);
MGL_EXPORT void glProgramUniform3ivEXT(GLuint program, GLint location, GLsizei count,
                                       const GLint *value);
MGL_EXPORT void glProgramUniform4ivEXT(GLuint program, GLint location, GLsizei count,
                                       const GLint *value);
MGL_EXPORT void glProgramUniformMatrix2fvEXT(GLuint program, GLint location, GLsizei count,
                                             GLboolean transpose, const GLfloat *value);
MGL_EXPORT void glProgramUniformMatrix3fvEXT(GLuint program, GLint location, GLsizei count,
                                             GLboolean transpose, const GLfloat *value);
MGL_EXPORT void glProgramUniformMatrix4fvEXT(GLuint program, GLint location, GLsizei count,
                                             GLboolean transpose, const GLfloat *value);

MGL_EXPORT void glActiveTexture(GLenum texture);
MGL_EXPORT void glGenTextures(GLsizei n, GLuint *textures);
MGL_EXPORT void glDeleteTextures(GLsizei n, const GLuint *textures);
MGL_EXPORT void glBindTexture(GLenum target, GLuint texture);
MGL_EXPORT GLboolean glIsTexture(GLuint texture);
MGL_EXPORT GLboolean glAreTexturesResident(GLsizei n, const GLuint *textures,
                                           GLboolean *residences);
MGL_EXPORT void glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLfloat *priorities);
MGL_EXPORT void glCreateTextures(GLenum target, GLsizei n, GLuint *textures);
MGL_EXPORT void glBindTextureUnit(GLuint unit, GLuint texture);
MGL_EXPORT void glBindTextures(GLuint first, GLsizei count, const GLuint *textures);
MGL_EXPORT void glBindMultiTexture(GLenum texunit, GLenum target, GLuint texture);
MGL_EXPORT void glBindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture);
MGL_EXPORT void glMultiTexBuffer(GLenum texunit, GLenum target, GLenum internalformat,
                                 GLuint buffer);
MGL_EXPORT void glMultiTexBufferEXT(GLenum texunit, GLenum target, GLenum internalformat,
                                    GLuint buffer);
MGL_EXPORT void glTextureParameteri(GLuint texture, GLenum pname, GLint param);
MGL_EXPORT void glTextureParameterf(GLuint texture, GLenum pname, GLfloat param);
MGL_EXPORT void glTextureParameteriv(GLuint texture, GLenum pname, const GLint *params);
MGL_EXPORT void glTextureParameterfv(GLuint texture, GLenum pname, const GLfloat *params);
MGL_EXPORT void glTextureParameterIiv(GLuint texture, GLenum pname, const GLint *params);
MGL_EXPORT void glTextureParameterIuiv(GLuint texture, GLenum pname, const GLuint *params);
MGL_EXPORT void glGetTextureParameteriv(GLuint texture, GLenum pname, GLint *params);
MGL_EXPORT void glGetTextureParameterfv(GLuint texture, GLenum pname, GLfloat *params);
MGL_EXPORT void glGetTextureParameterIiv(GLuint texture, GLenum pname, GLint *params);
MGL_EXPORT void glGetTextureParameterIuiv(GLuint texture, GLenum pname, GLuint *params);
MGL_EXPORT void glTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width,
                                    GLenum format, GLenum type, const void *pixels);
MGL_EXPORT void glCompressedTextureSubImage1D(GLuint texture, GLint level, GLint xoffset,
                                              GLsizei width, GLenum format, GLsizei imageSize,
                                              const void *data);
MGL_EXPORT void glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x,
                                        GLint y, GLsizei width);
MGL_EXPORT void glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                    GLsizei width, GLsizei height, GLenum format, GLenum type,
                                    const void *pixels);
MGL_EXPORT void glTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                    GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                    GLenum format, GLenum type, const void *pixels);
MGL_EXPORT void glCompressedTextureSubImage2D(GLuint texture, GLint level, GLint xoffset,
                                              GLint yoffset, GLsizei width, GLsizei height,
                                              GLenum format, GLsizei imageSize, const void *data);
MGL_EXPORT void glCompressedTextureSubImage3D(GLuint texture, GLint level, GLint xoffset,
                                              GLint yoffset, GLint zoffset, GLsizei width,
                                              GLsizei height, GLsizei depth, GLenum format,
                                              GLsizei imageSize, const void *data);
MGL_EXPORT void glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                        GLint x, GLint y, GLsizei width, GLsizei height);
MGL_EXPORT void glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                        GLint zoffset, GLint x, GLint y, GLsizei width,
                                        GLsizei height);
MGL_EXPORT void glCompressedTextureImage2DEXT(GLuint texture, GLenum target, GLint level,
                                              GLenum internalformat, GLsizei width, GLsizei height,
                                              GLint border, GLsizei imageSize, const void *data);
MGL_EXPORT void glCompressedTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level,
                                                 GLint xoffset, GLint yoffset, GLsizei width,
                                                 GLsizei height, GLenum format, GLsizei imageSize,
                                                 const void *data);
MGL_EXPORT void glCompressedTextureImage3DEXT(GLuint texture, GLenum target, GLint level,
                                              GLenum internalformat, GLsizei width, GLsizei height,
                                              GLsizei depth, GLint border, GLsizei imageSize,
                                              const void *data);
MGL_EXPORT void glCompressedTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level,
                                                 GLint xoffset, GLint yoffset, GLint zoffset,
                                                 GLsizei width, GLsizei height, GLsizei depth,
                                                 GLenum format, GLsizei imageSize,
                                                 const void *data);
MGL_EXPORT void glTextureImage2DEXT(GLuint texture, GLenum target, GLint level,
                                    GLint internalformat, GLsizei width, GLsizei height,
                                    GLint border, GLenum format, GLenum type, const void *pixels);
MGL_EXPORT void glTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                       GLint yoffset, GLsizei width, GLsizei height, GLenum format,
                                       GLenum type, const void *pixels);
MGL_EXPORT void glTextureImage3DEXT(GLuint texture, GLenum target, GLint level,
                                    GLint internalformat, GLsizei width, GLsizei height,
                                    GLsizei depth, GLint border, GLenum format, GLenum type,
                                    const void *pixels);
MGL_EXPORT void glTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                       GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                                       GLsizei depth, GLenum format, GLenum type,
                                       const void *pixels);
MGL_EXPORT void glCopyTextureImage2DEXT(GLuint texture, GLenum target, GLint level,
                                        GLenum internalformat, GLint x, GLint y, GLsizei width,
                                        GLsizei height, GLint border);
MGL_EXPORT void glCopyTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level,
                                           GLint xoffset, GLint yoffset, GLint x, GLint y,
                                           GLsizei width, GLsizei height);
MGL_EXPORT void glCopyTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level,
                                           GLint xoffset, GLint yoffset, GLint zoffset, GLint x,
                                           GLint y, GLsizei width, GLsizei height);
MGL_EXPORT void glGenerateTextureMipmap(GLuint texture);
MGL_EXPORT void glGenerateTextureMipmapEXT(GLuint texture, GLenum target);
MGL_EXPORT void glGenerateMultiTexMipmap(GLenum texunit, GLenum target);
MGL_EXPORT void glGenerateMultiTexMipmapEXT(GLenum texunit, GLenum target);
MGL_EXPORT void glGetTextureLevelParameteriv(GLuint texture, GLint level, GLenum pname,
                                             GLint *params);
MGL_EXPORT void glGetTextureLevelParameterfv(GLuint texture, GLint level, GLenum pname,
                                             GLfloat *params);
MGL_EXPORT void glGetTextureImage(GLuint texture, GLint level, GLenum format, GLenum type,
                                  GLsizei bufSize, void *pixels);
MGL_EXPORT void glGetTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                     GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                     GLenum format, GLenum type, GLsizei bufSize, void *pixels);
MGL_EXPORT void glGetCompressedTextureSubImage(GLuint texture, GLint level, GLint xoffset,
                                               GLint yoffset, GLint zoffset, GLsizei width,
                                               GLsizei height, GLsizei depth, GLsizei bufSize,
                                               void *pixels);
MGL_EXPORT void glGetCompressedTextureImage(GLuint texture, GLint level, GLsizei bufSize,
                                            void *pixels);
MGL_EXPORT void glTextureParameteriEXT(GLuint texture, GLenum target, GLenum pname, GLint param);
MGL_EXPORT void glTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname,
                                        const GLint *params);
MGL_EXPORT void glTextureParameterfEXT(GLuint texture, GLenum target, GLenum pname, GLfloat param);
MGL_EXPORT void glTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname,
                                        const GLfloat *params);
MGL_EXPORT void glTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname,
                                         const GLint *params);
MGL_EXPORT void glTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname,
                                          const GLuint *params);
MGL_EXPORT void glGetTextureImageEXT(GLuint texture, GLenum target, GLint level, GLenum format,
                                     GLenum type, void *pixels);
MGL_EXPORT void glGetCompressedTextureImageEXT(GLuint texture, GLenum target, GLint level,
                                               void *img);
MGL_EXPORT void glGetTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname,
                                           GLint *params);
MGL_EXPORT void glGetTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname,
                                           GLfloat *params);
MGL_EXPORT void glGetTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname,
                                            GLint *params);
MGL_EXPORT void glGetTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname,
                                             GLuint *params);
MGL_EXPORT void glGetTextureLevelParameterivEXT(GLuint texture, GLenum target, GLint level,
                                                GLenum pname, GLint *params);
MGL_EXPORT void glGetTextureLevelParameterfvEXT(GLuint texture, GLenum target, GLint level,
                                                GLenum pname, GLfloat *params);
MGL_EXPORT void glPixelStorei(GLenum pname, GLint param);
MGL_EXPORT void glPixelStoref(GLenum pname, GLfloat param);
MGL_EXPORT void glPixelTransferf(GLenum pname, GLfloat param);
MGL_EXPORT void glPixelTransferi(GLenum pname, GLint param);
MGL_EXPORT void glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat,
                                       GLsizei width, GLint border, GLsizei imageSize,
                                       const void *data);
MGL_EXPORT void glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width,
                                          GLenum format, GLsizei imageSize, const void *data);
MGL_EXPORT void glInvalidateTexImage(GLuint texture, GLint level);
MGL_EXPORT void glInvalidateTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                        GLint zoffset, GLsizei width, GLsizei height,
                                        GLsizei depth);
MGL_EXPORT void glInvalidateBufferData(GLuint buffer);
MGL_EXPORT void glInvalidateBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr length);
MGL_EXPORT void glClearTexImage(GLuint texture, GLint level, GLenum format, GLenum type,
                                const void *data);
MGL_EXPORT void glClearTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                   GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                   GLenum format, GLenum type, const void *data);
MGL_EXPORT void glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                             GLint border, GLenum format, GLenum type, const void *pixels);
MGL_EXPORT void glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width,
                                GLenum format, GLenum type, const void *pixels);
MGL_EXPORT void glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX,
                                   GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget,
                                   GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ,
                                   GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
MGL_EXPORT void glTextureBarrier(void);
MGL_EXPORT void glTextureBarrierNV(void);
MGL_EXPORT void glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
MGL_EXPORT void glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level);
MGL_EXPORT void glFramebufferParameteri(GLenum target, GLenum pname, GLint param);
MGL_EXPORT void glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint *params);
MGL_EXPORT void glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget,
                                       GLuint texture, GLint level);
MGL_EXPORT void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                             GLsizei height, GLint border, GLenum format, GLenum type,
                             const void *pixels);
MGL_EXPORT void glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                             GLsizei height, GLsizei depth, GLint border, GLenum format,
                             GLenum type, const void *pixels);
MGL_EXPORT void glTextureView(GLuint texture, GLenum target, GLuint origtexture,
                              GLenum internalformat, GLuint minlevel, GLuint numlevels,
                              GLuint minlayer, GLuint numlayers);
MGL_EXPORT void glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                               GLsizei height);
MGL_EXPORT void glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                               GLsizei height, GLsizei depth);
MGL_EXPORT void glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat,
                                   GLsizei width);
MGL_EXPORT void glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat,
                                   GLsizei width, GLsizei height);
MGL_EXPORT void glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat,
                                   GLsizei width, GLsizei height, GLsizei depth);
MGL_EXPORT void glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                        GLsizei width, GLsizei height,
                                        GLboolean fixedsamplelocations);
MGL_EXPORT void glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                        GLsizei width, GLsizei height, GLsizei depth,
                                        GLboolean fixedsamplelocations);
MGL_EXPORT void glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                          GLsizei width, GLsizei height,
                                          GLboolean fixedsamplelocations);
MGL_EXPORT void glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                          GLsizei width, GLsizei height, GLsizei depth,
                                          GLboolean fixedsamplelocations);
MGL_EXPORT void glTextureStorage2DMultisample(GLuint texture, GLsizei samples,
                                              GLenum internalformat, GLsizei width, GLsizei height,
                                              GLboolean fixedsamplelocations);
MGL_EXPORT void glTextureStorage3DMultisample(GLuint texture, GLsizei samples,
                                              GLenum internalformat, GLsizei width, GLsizei height,
                                              GLsizei depth, GLboolean fixedsamplelocations);
MGL_EXPORT void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                GLsizei width, GLsizei height, GLenum format, GLenum type,
                                const void *pixels);
MGL_EXPORT void glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                GLenum format, GLenum type, const void *pixels);
MGL_EXPORT void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat,
                                       GLsizei width, GLsizei height, GLint border,
                                       GLsizei imageSize, const void *data);
MGL_EXPORT void glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat,
                                       GLsizei width, GLsizei height, GLsizei depth, GLint border,
                                       GLsizei imageSize, const void *data);
MGL_EXPORT void glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                          GLsizei width, GLsizei height, GLenum format,
                                          GLsizei imageSize, const void *data);
MGL_EXPORT void glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                          GLint zoffset, GLsizei width, GLsizei height,
                                          GLsizei depth, GLenum format, GLsizei imageSize,
                                          const void *data);
MGL_EXPORT void glGetCompressedTexImage(GLenum target, GLint level, void *img);
MGL_EXPORT void glGetnTexImage(GLenum target, GLint level, GLenum format, GLenum type,
                               GLsizei bufSize, void *pixels);
MGL_EXPORT void glGetnCompressedTexImage(GLenum target, GLint level, GLsizei bufSize, void *pixels);
MGL_EXPORT void glGetnColorTable(GLenum target, GLenum format, GLenum type, GLsizei bufSize,
                                 void *table);
MGL_EXPORT void glGetnConvolutionFilter(GLenum target, GLenum format, GLenum type, GLsizei bufSize,
                                        void *image);
MGL_EXPORT void glGetnHistogram(GLenum target, GLboolean reset, GLenum format, GLenum type,
                                GLsizei bufSize, void *values);
MGL_EXPORT void glGetnMapdv(GLenum target, GLenum query, GLsizei bufSize, GLdouble *v);
MGL_EXPORT void glGetnMapfv(GLenum target, GLenum query, GLsizei bufSize, GLfloat *v);
MGL_EXPORT void glGetnMapiv(GLenum target, GLenum query, GLsizei bufSize, GLint *v);
MGL_EXPORT void glGetnMinmax(GLenum target, GLboolean reset, GLenum format, GLenum type,
                             GLsizei bufSize, void *values);
MGL_EXPORT void glGetnPixelMapfv(GLenum map, GLsizei bufSize, GLfloat *values);
MGL_EXPORT void glGetnPixelMapuiv(GLenum map, GLsizei bufSize, GLuint *values);
MGL_EXPORT void glGetnPixelMapusv(GLenum map, GLsizei bufSize, GLushort *values);
MGL_EXPORT void glGetnPolygonStipple(GLsizei bufSize, GLubyte *pattern);
MGL_EXPORT void glGetnSeparableFilter(GLenum target, GLenum format, GLenum type, GLsizei rowBufSize,
                                      void *row, GLsizei columnBufSize, void *column, void *span);
MGL_EXPORT void glGenerateMipmap(GLenum target);
MGL_EXPORT void glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x,
                                 GLint y, GLsizei width, GLint border);
MGL_EXPORT void glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y,
                                    GLsizei width);
MGL_EXPORT void glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x,
                                 GLint y, GLsizei width, GLsizei height, GLint border);
MGL_EXPORT void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                    GLint x, GLint y, GLsizei width, GLsizei height);
MGL_EXPORT void glCopyTexSubImage3DEXT(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                       GLint zoffset, GLint x, GLint y, GLsizei width,
                                       GLsizei height);
MGL_EXPORT void glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                    GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
MGL_EXPORT void glTexParameteri(GLenum target, GLenum pname, GLint param);
MGL_EXPORT void glTexParameterf(GLenum target, GLenum pname, GLfloat param);
MGL_EXPORT void glTexParameteriv(GLenum target, GLenum pname, const GLint *params);
MGL_EXPORT void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params);
MGL_EXPORT void glTexParameterIiv(GLenum target, GLenum pname, const GLint *params);
MGL_EXPORT void glTexParameterIuiv(GLenum target, GLenum pname, const GLuint *params);
MGL_EXPORT void glGetTexParameteriv(GLenum target, GLenum pname, GLint *params);
MGL_EXPORT void glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params);
MGL_EXPORT void glGetTexParameterIiv(GLenum target, GLenum pname, GLint *params);
MGL_EXPORT void glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint *params);
MGL_EXPORT void glTexParameterx(GLenum target, GLenum pname, GLfixed param);
MGL_EXPORT void glTexParameterxv(GLenum target, GLenum pname, const GLfixed *params);
MGL_EXPORT void glGetTexParameterxv(GLenum target, GLenum pname, GLfixed *params);
MGL_EXPORT void glMultiTexParameteri(GLenum texunit, GLenum target, GLenum pname, GLint param);
MGL_EXPORT void glMultiTexParameteriv(GLenum texunit, GLenum target, GLenum pname,
                                      const GLint *params);
MGL_EXPORT void glMultiTexParameterf(GLenum texunit, GLenum target, GLenum pname, GLfloat param);
MGL_EXPORT void glMultiTexParameterfv(GLenum texunit, GLenum target, GLenum pname,
                                      const GLfloat *params);
MGL_EXPORT void glGetMultiTexParameteriv(GLenum texunit, GLenum target, GLenum pname,
                                         GLint *params);
MGL_EXPORT void glGetMultiTexParameterfv(GLenum texunit, GLenum target, GLenum pname,
                                         GLfloat *params);
MGL_EXPORT void glMultiTexImage2D(GLenum texunit, GLenum target, GLint level, GLint internalformat,
                                  GLsizei width, GLsizei height, GLint border, GLenum format,
                                  GLenum type, const void *pixels);
MGL_EXPORT void glMultiTexSubImage2D(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                                     GLint yoffset, GLsizei width, GLsizei height, GLenum format,
                                     GLenum type, const void *pixels);
MGL_EXPORT void glMultiTexImage3D(GLenum texunit, GLenum target, GLint level, GLint internalformat,
                                  GLsizei width, GLsizei height, GLsizei depth, GLint border,
                                  GLenum format, GLenum type, const void *pixels);
MGL_EXPORT void glMultiTexSubImage3D(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                                     GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                                     GLsizei depth, GLenum format, GLenum type, const void *pixels);
MGL_EXPORT void glCopyMultiTexImage2D(GLenum texunit, GLenum target, GLint level,
                                      GLenum internalformat, GLint x, GLint y, GLsizei width,
                                      GLsizei height, GLint border);
MGL_EXPORT void glCopyMultiTexSubImage2D(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                                         GLint yoffset, GLint x, GLint y, GLsizei width,
                                         GLsizei height);
MGL_EXPORT void glCopyMultiTexSubImage3D(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                                         GLint yoffset, GLint zoffset, GLint x, GLint y,
                                         GLsizei width, GLsizei height);
MGL_EXPORT void glCompressedMultiTexImage2D(GLenum texunit, GLenum target, GLint level,
                                            GLenum internalformat, GLsizei width, GLsizei height,
                                            GLint border, GLsizei imageSize, const void *data);
MGL_EXPORT void glCompressedMultiTexSubImage2D(GLenum texunit, GLenum target, GLint level,
                                               GLint xoffset, GLint yoffset, GLsizei width,
                                               GLsizei height, GLenum format, GLsizei imageSize,
                                               const void *data);
MGL_EXPORT void glCompressedMultiTexImage3D(GLenum texunit, GLenum target, GLint level,
                                            GLenum internalformat, GLsizei width, GLsizei height,
                                            GLsizei depth, GLint border, GLsizei imageSize,
                                            const void *data);
MGL_EXPORT void glCompressedMultiTexSubImage3D(GLenum texunit, GLenum target, GLint level,
                                               GLint xoffset, GLint yoffset, GLint zoffset,
                                               GLsizei width, GLsizei height, GLsizei depth,
                                               GLenum format, GLsizei imageSize, const void *data);
MGL_EXPORT void glGetMultiTexImage(GLenum texunit, GLenum target, GLint level, GLenum format,
                                   GLenum type, void *pixels);
MGL_EXPORT void glGetMultiTexLevelParameteriv(GLenum texunit, GLenum target, GLint level,
                                              GLenum pname, GLint *params);
MGL_EXPORT void glGetMultiTexLevelParameterfv(GLenum texunit, GLenum target, GLint level,
                                              GLenum pname, GLfloat *params);
MGL_EXPORT void glGetCompressedMultiTexImage(GLenum texunit, GLenum target, GLint level, void *img);
MGL_EXPORT void glMultiTexParameteriEXT(GLenum texunit, GLenum target, GLenum pname, GLint param);
MGL_EXPORT void glMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname,
                                         const GLint *params);
MGL_EXPORT void glMultiTexParameterfEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat param);
MGL_EXPORT void glMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname,
                                         const GLfloat *params);
MGL_EXPORT void glGetMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname,
                                            GLint *params);
MGL_EXPORT void glGetMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname,
                                            GLfloat *params);
MGL_EXPORT void glMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname,
                                          const GLint *params);
MGL_EXPORT void glMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname,
                                           const GLuint *params);
MGL_EXPORT void glGetMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname,
                                             GLint *params);
MGL_EXPORT void glGetMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname,
                                              GLuint *params);
MGL_EXPORT void glMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level,
                                     GLint internalformat, GLsizei width, GLsizei height,
                                     GLint border, GLenum format, GLenum type, const void *pixels);
MGL_EXPORT void glMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                                        GLint yoffset, GLsizei width, GLsizei height, GLenum format,
                                        GLenum type, const void *pixels);
MGL_EXPORT void glMultiTexImage3DEXT(GLenum texunit, GLenum target, GLint level,
                                     GLint internalformat, GLsizei width, GLsizei height,
                                     GLsizei depth, GLint border, GLenum format, GLenum type,
                                     const void *pixels);
MGL_EXPORT void glMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                                        GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                                        GLsizei depth, GLenum format, GLenum type,
                                        const void *pixels);
MGL_EXPORT void glCopyMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level,
                                         GLenum internalformat, GLint x, GLint y, GLsizei width,
                                         GLsizei height, GLint border);
MGL_EXPORT void glCopyMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level,
                                            GLint xoffset, GLint yoffset, GLint x, GLint y,
                                            GLsizei width, GLsizei height);
MGL_EXPORT void glCopyMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level,
                                            GLint xoffset, GLint yoffset, GLint zoffset, GLint x,
                                            GLint y, GLsizei width, GLsizei height);
MGL_EXPORT void glCompressedMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level,
                                               GLenum internalformat, GLsizei width, GLsizei height,
                                               GLint border, GLsizei imageSize, const void *data);
MGL_EXPORT void glCompressedMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level,
                                                  GLint xoffset, GLint yoffset, GLsizei width,
                                                  GLsizei height, GLenum format, GLsizei imageSize,
                                                  const void *data);
MGL_EXPORT void glCompressedMultiTexImage3DEXT(GLenum texunit, GLenum target, GLint level,
                                               GLenum internalformat, GLsizei width, GLsizei height,
                                               GLsizei depth, GLint border, GLsizei imageSize,
                                               const void *data);
MGL_EXPORT void glCompressedMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level,
                                                  GLint xoffset, GLint yoffset, GLint zoffset,
                                                  GLsizei width, GLsizei height, GLsizei depth,
                                                  GLenum format, GLsizei imageSize,
                                                  const void *data);
MGL_EXPORT void glGetMultiTexImageEXT(GLenum texunit, GLenum target, GLint level, GLenum format,
                                      GLenum type, void *pixels);
MGL_EXPORT void glGetMultiTexLevelParameterivEXT(GLenum texunit, GLenum target, GLint level,
                                                 GLenum pname, GLint *params);
MGL_EXPORT void glGetMultiTexLevelParameterfvEXT(GLenum texunit, GLenum target, GLint level,
                                                 GLenum pname, GLfloat *params);
MGL_EXPORT void glGetCompressedMultiTexImageEXT(GLenum texunit, GLenum target, GLint level,
                                                void *img);
MGL_EXPORT void glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void *pixels);
MGL_EXPORT void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params);
MGL_EXPORT void glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params);
MGL_EXPORT void glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer);
MGL_EXPORT void glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer,
                                 GLintptr offset, GLsizeiptr size);
MGL_EXPORT void glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer);
MGL_EXPORT void glTextureBufferEXT(GLuint texture, GLenum target, GLenum internalformat,
                                   GLuint buffer);
MGL_EXPORT void glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer,
                                     GLintptr offset, GLsizeiptr size);
MGL_EXPORT void glTextureBufferRangeEXT(GLuint texture, GLenum target, GLenum internalformat,
                                        GLuint buffer, GLintptr offset, GLsizeiptr size);
MGL_EXPORT void glTexEnvf(GLenum target, GLenum pname, GLfloat param);
MGL_EXPORT void glTexEnvi(GLenum target, GLenum pname, GLint param);
MGL_EXPORT void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params);
MGL_EXPORT void glTexEnviv(GLenum target, GLenum pname, const GLint *params);
MGL_EXPORT void glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params);
MGL_EXPORT void glGetTexEnviv(GLenum target, GLenum pname, GLint *params);
MGL_EXPORT void glTexEnvx(GLenum target, GLenum pname, GLfixed param);
MGL_EXPORT void glTexEnvxv(GLenum target, GLenum pname, const GLfixed *params);
MGL_EXPORT void glGetTexEnvxv(GLenum target, GLenum pname, GLfixed *params);
MGL_EXPORT void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget,
                                       GLuint texture, GLint level);
MGL_EXPORT void glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget,
                                       GLuint texture, GLint level, GLint zoffset);
MGL_EXPORT void glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture,
                                          GLint level, GLint layer);
MGL_EXPORT void glMatrixMode(GLenum mode);
MGL_EXPORT void glLoadIdentity(void);
MGL_EXPORT void glLoadMatrixf(const GLfloat *m);
MGL_EXPORT void glLoadMatrixx(const GLfixed *m);
MGL_EXPORT void glLoadMatrixd(const GLdouble *m);
MGL_EXPORT void glMultMatrixf(const GLfloat *m);
MGL_EXPORT void glMultMatrixx(const GLfixed *m);
MGL_EXPORT void glMultMatrixd(const GLdouble *m);
MGL_EXPORT void glLoadTransposeMatrixf(const GLfloat *m);
MGL_EXPORT void glLoadTransposeMatrixd(const GLdouble *m);
MGL_EXPORT void glMultTransposeMatrixf(const GLfloat *m);
MGL_EXPORT void glMultTransposeMatrixd(const GLdouble *m);
MGL_EXPORT void glPushMatrix(void);
MGL_EXPORT void glPopMatrix(void);
MGL_EXPORT void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
MGL_EXPORT void glTranslatex(GLfixed x, GLfixed y, GLfixed z);
MGL_EXPORT void glTranslated(GLdouble x, GLdouble y, GLdouble z);
MGL_EXPORT void glScalef(GLfloat x, GLfloat y, GLfloat z);
MGL_EXPORT void glScalex(GLfixed x, GLfixed y, GLfixed z);
MGL_EXPORT void glScaled(GLdouble x, GLdouble y, GLdouble z);
MGL_EXPORT void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
MGL_EXPORT void glRotatex(GLfixed angle, GLfixed x, GLfixed y, GLfixed z);
MGL_EXPORT void glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
MGL_EXPORT void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
                        GLdouble near_val, GLdouble far_val);
MGL_EXPORT void glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near_val,
                         GLfloat far_val);
MGL_EXPORT void glOrthox(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed near_val,
                         GLfixed far_val);
MGL_EXPORT void glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
                          GLdouble near_val, GLdouble far_val);
MGL_EXPORT void glFrustumf(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
                           GLfloat near_val, GLfloat far_val);
MGL_EXPORT void glFrustumx(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top,
                           GLfixed near_val, GLfixed far_val);
MGL_EXPORT void glMatrixLoadfEXT(GLenum matrixMode, const GLfloat *m);
MGL_EXPORT void glMatrixLoaddEXT(GLenum matrixMode, const GLdouble *m);
MGL_EXPORT void glMatrixMultfEXT(GLenum matrixMode, const GLfloat *m);
MGL_EXPORT void glMatrixMultdEXT(GLenum matrixMode, const GLdouble *m);
MGL_EXPORT void glMatrixLoadTransposefEXT(GLenum matrixMode, const GLfloat *m);
MGL_EXPORT void glMatrixLoadTransposedEXT(GLenum matrixMode, const GLdouble *m);
MGL_EXPORT void glMatrixMultTransposefEXT(GLenum matrixMode, const GLfloat *m);
MGL_EXPORT void glMatrixMultTransposedEXT(GLenum matrixMode, const GLdouble *m);
MGL_EXPORT void glMatrixLoadIdentityEXT(GLenum matrixMode);
MGL_EXPORT void glMatrixPushEXT(GLenum matrixMode);
MGL_EXPORT void glMatrixPopEXT(GLenum matrixMode);
MGL_EXPORT void glMatrixTranslatefEXT(GLenum matrixMode, GLfloat x, GLfloat y, GLfloat z);
MGL_EXPORT void glMatrixTranslatedEXT(GLenum matrixMode, GLdouble x, GLdouble y, GLdouble z);
MGL_EXPORT void glMatrixScalefEXT(GLenum matrixMode, GLfloat x, GLfloat y, GLfloat z);
MGL_EXPORT void glMatrixScaledEXT(GLenum matrixMode, GLdouble x, GLdouble y, GLdouble z);
MGL_EXPORT void glMatrixRotatefEXT(GLenum matrixMode, GLfloat angle, GLfloat x, GLfloat y,
                                   GLfloat z);
MGL_EXPORT void glMatrixRotatedEXT(GLenum matrixMode, GLdouble angle, GLdouble x, GLdouble y,
                                   GLdouble z);
MGL_EXPORT void glMatrixOrthoEXT(GLenum matrixMode, GLdouble left, GLdouble right, GLdouble bottom,
                                 GLdouble top, GLdouble near_val, GLdouble far_val);
MGL_EXPORT void glMatrixFrustumEXT(GLenum matrixMode, GLdouble left, GLdouble right,
                                   GLdouble bottom, GLdouble top, GLdouble near_val,
                                   GLdouble far_val);
MGL_EXPORT void glColor3b(GLbyte red, GLbyte green, GLbyte blue);
MGL_EXPORT void glColor3bv(const GLbyte *v);
MGL_EXPORT void glColor3d(GLdouble red, GLdouble green, GLdouble blue);
MGL_EXPORT void glColor3dv(const GLdouble *v);
MGL_EXPORT void glColor3i(GLint red, GLint green, GLint blue);
MGL_EXPORT void glColor3iv(const GLint *v);
MGL_EXPORT void glColor3s(GLshort red, GLshort green, GLshort blue);
MGL_EXPORT void glColor3sv(const GLshort *v);
MGL_EXPORT void glColor3ub(GLubyte red, GLubyte green, GLubyte blue);
MGL_EXPORT void glColor3ui(GLuint red, GLuint green, GLuint blue);
MGL_EXPORT void glColor3uiv(const GLuint *v);
MGL_EXPORT void glColor3us(GLushort red, GLushort green, GLushort blue);
MGL_EXPORT void glColor3usv(const GLushort *v);
MGL_EXPORT void glColor4b(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha);
MGL_EXPORT void glColor4bv(const GLbyte *v);
MGL_EXPORT void glColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
MGL_EXPORT void glColor4dv(const GLdouble *v);
MGL_EXPORT void glColor4i(GLint red, GLint green, GLint blue, GLint alpha);
MGL_EXPORT void glColor4iv(const GLint *v);
MGL_EXPORT void glColor4s(GLshort red, GLshort green, GLshort blue, GLshort alpha);
MGL_EXPORT void glColor4sv(const GLshort *v);
MGL_EXPORT void glColor4ui(GLuint red, GLuint green, GLuint blue, GLuint alpha);
MGL_EXPORT void glColor4uiv(const GLuint *v);
MGL_EXPORT void glColor4us(GLushort red, GLushort green, GLushort blue, GLushort alpha);
MGL_EXPORT void glColor4usv(const GLushort *v);
MGL_EXPORT void glColor3f(GLfloat red, GLfloat green, GLfloat blue);
MGL_EXPORT void glColor3fv(const GLfloat *v);
MGL_EXPORT void glColor3ubv(const GLubyte *v);
MGL_EXPORT void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
MGL_EXPORT void glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
MGL_EXPORT void glColor4ubv(const GLubyte *v);
MGL_EXPORT void glColor4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha);
MGL_EXPORT void glColor4fv(const GLfloat *v);
MGL_EXPORT void glSecondaryColor3f(GLfloat red, GLfloat green, GLfloat blue);
MGL_EXPORT void glSecondaryColor3fv(const GLfloat *v);
MGL_EXPORT void glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz);
MGL_EXPORT void glNormal3bv(const GLbyte *v);
MGL_EXPORT void glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz);
MGL_EXPORT void glNormal3dv(const GLdouble *v);
MGL_EXPORT void glNormal3i(GLint nx, GLint ny, GLint nz);
MGL_EXPORT void glNormal3iv(const GLint *v);
MGL_EXPORT void glNormal3s(GLshort nx, GLshort ny, GLshort nz);
MGL_EXPORT void glNormal3sv(const GLshort *v);
MGL_EXPORT void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz);
MGL_EXPORT void glNormal3x(GLfixed nx, GLfixed ny, GLfixed nz);
MGL_EXPORT void glNormal3fv(const GLfloat *v);
MGL_EXPORT void glTexCoord1f(GLfloat s);
MGL_EXPORT void glTexCoord1fv(const GLfloat *v);
MGL_EXPORT void glTexCoord2f(GLfloat s, GLfloat t);
MGL_EXPORT void glTexCoord2fv(const GLfloat *v);
MGL_EXPORT void glTexCoord2sv(const GLshort *v);
MGL_EXPORT void glTexCoord3f(GLfloat s, GLfloat t, GLfloat r);
MGL_EXPORT void glTexCoord3fv(const GLfloat *v);
MGL_EXPORT void glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q);
MGL_EXPORT void glTexCoord4fv(const GLfloat *v);
MGL_EXPORT void glTexCoord4sv(const GLshort *v);
MGL_EXPORT void glTexCoord1d(GLdouble s);
MGL_EXPORT void glTexCoord1dv(const GLdouble *v);
MGL_EXPORT void glTexCoord1i(GLint s);
MGL_EXPORT void glTexCoord1iv(const GLint *v);
MGL_EXPORT void glTexCoord1s(GLshort s);
MGL_EXPORT void glTexCoord1sv(const GLshort *v);
MGL_EXPORT void glTexCoord2d(GLdouble s, GLdouble t);
MGL_EXPORT void glTexCoord2dv(const GLdouble *v);
MGL_EXPORT void glTexCoord2i(GLint s, GLint t);
MGL_EXPORT void glTexCoord2iv(const GLint *v);
MGL_EXPORT void glTexCoord2s(GLshort s, GLshort t);
MGL_EXPORT void glTexCoord3d(GLdouble s, GLdouble t, GLdouble r);
MGL_EXPORT void glTexCoord3dv(const GLdouble *v);
MGL_EXPORT void glTexCoord3i(GLint s, GLint t, GLint r);
MGL_EXPORT void glTexCoord3iv(const GLint *v);
MGL_EXPORT void glTexCoord3s(GLshort s, GLshort t, GLshort r);
MGL_EXPORT void glTexCoord3sv(const GLshort *v);
MGL_EXPORT void glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q);
MGL_EXPORT void glTexCoord4dv(const GLdouble *v);
MGL_EXPORT void glTexCoord4i(GLint s, GLint t, GLint r, GLint q);
MGL_EXPORT void glTexCoord4iv(const GLint *v);
MGL_EXPORT void glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q);
MGL_EXPORT void glMultiTexCoord1dARB(GLenum texture, GLdouble s);
MGL_EXPORT void glMultiTexCoord1dvARB(GLenum texture, const GLdouble *v);
MGL_EXPORT void glMultiTexCoord1iARB(GLenum texture, GLint s);
MGL_EXPORT void glMultiTexCoord1ivARB(GLenum texture, const GLint *v);
MGL_EXPORT void glMultiTexCoord1sARB(GLenum texture, GLshort s);
MGL_EXPORT void glMultiTexCoord1svARB(GLenum texture, const GLshort *v);
MGL_EXPORT void glMultiTexCoord2dARB(GLenum texture, GLdouble s, GLdouble t);
MGL_EXPORT void glMultiTexCoord2dvARB(GLenum texture, const GLdouble *v);
MGL_EXPORT void glMultiTexCoord2iARB(GLenum texture, GLint s, GLint t);
MGL_EXPORT void glMultiTexCoord2ivARB(GLenum texture, const GLint *v);
MGL_EXPORT void glMultiTexCoord2sARB(GLenum texture, GLshort s, GLshort t);
MGL_EXPORT void glMultiTexCoord3dARB(GLenum texture, GLdouble s, GLdouble t, GLdouble r);
MGL_EXPORT void glMultiTexCoord3dvARB(GLenum texture, const GLdouble *v);
MGL_EXPORT void glMultiTexCoord3iARB(GLenum texture, GLint s, GLint t, GLint r);
MGL_EXPORT void glMultiTexCoord3ivARB(GLenum texture, const GLint *v);
MGL_EXPORT void glMultiTexCoord3sARB(GLenum texture, GLshort s, GLshort t, GLshort r);
MGL_EXPORT void glMultiTexCoord3svARB(GLenum texture, const GLshort *v);
MGL_EXPORT void glMultiTexCoord4dARB(GLenum texture, GLdouble s, GLdouble t, GLdouble r,
                                     GLdouble q);
MGL_EXPORT void glMultiTexCoord4dvARB(GLenum texture, const GLdouble *v);
MGL_EXPORT void glMultiTexCoord4iARB(GLenum texture, GLint s, GLint t, GLint r, GLint q);
MGL_EXPORT void glMultiTexCoord4ivARB(GLenum texture, const GLint *v);
MGL_EXPORT void glMultiTexCoord4sARB(GLenum texture, GLshort s, GLshort t, GLshort r, GLshort q);
MGL_EXPORT void glMultiTexCoord1d(GLenum target, GLdouble s);
MGL_EXPORT void glMultiTexCoord1dv(GLenum target, const GLdouble *v);
MGL_EXPORT void glMultiTexCoord1i(GLenum target, GLint s);
MGL_EXPORT void glMultiTexCoord1iv(GLenum target, const GLint *v);
MGL_EXPORT void glMultiTexCoord1s(GLenum target, GLshort s);
MGL_EXPORT void glMultiTexCoord1sv(GLenum target, const GLshort *v);
MGL_EXPORT void glMultiTexCoord2d(GLenum target, GLdouble s, GLdouble t);
MGL_EXPORT void glMultiTexCoord2dv(GLenum target, const GLdouble *v);
MGL_EXPORT void glMultiTexCoord2i(GLenum target, GLint s, GLint t);
MGL_EXPORT void glMultiTexCoord2iv(GLenum target, const GLint *v);
MGL_EXPORT void glMultiTexCoord2s(GLenum target, GLshort s, GLshort t);
MGL_EXPORT void glMultiTexCoord2sv(GLenum target, const GLshort *v);
MGL_EXPORT void glMultiTexCoord2svARB(GLenum target, const GLshort *v);
MGL_EXPORT void glMultiTexCoord3d(GLenum target, GLdouble s, GLdouble t, GLdouble r);
MGL_EXPORT void glMultiTexCoord3dv(GLenum target, const GLdouble *v);
MGL_EXPORT void glMultiTexCoord3i(GLenum target, GLint s, GLint t, GLint r);
MGL_EXPORT void glMultiTexCoord3iv(GLenum target, const GLint *v);
MGL_EXPORT void glMultiTexCoord3s(GLenum target, GLshort s, GLshort t, GLshort r);
MGL_EXPORT void glMultiTexCoord3sv(GLenum target, const GLshort *v);
MGL_EXPORT void glMultiTexCoord4d(GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
MGL_EXPORT void glMultiTexCoord4dv(GLenum target, const GLdouble *v);
MGL_EXPORT void glMultiTexCoord4i(GLenum target, GLint s, GLint t, GLint r, GLint q);
MGL_EXPORT void glMultiTexCoord4iv(GLenum target, const GLint *v);
MGL_EXPORT void glMultiTexCoord4s(GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
MGL_EXPORT void glMultiTexCoord4sv(GLenum target, const GLshort *v);
MGL_EXPORT void glMultiTexCoord4svARB(GLenum target, const GLshort *v);
MGL_EXPORT void glMultiTexCoord1f(GLenum texture, GLfloat s);
MGL_EXPORT void glMultiTexCoord1fv(GLenum texture, const GLfloat *v);
MGL_EXPORT void glMultiTexCoord2f(GLenum texture, GLfloat s, GLfloat t);
MGL_EXPORT void glMultiTexCoord2fv(GLenum texture, const GLfloat *v);
MGL_EXPORT void glMultiTexCoord3f(GLenum texture, GLfloat s, GLfloat t, GLfloat r);
MGL_EXPORT void glMultiTexCoord3fv(GLenum texture, const GLfloat *v);
MGL_EXPORT void glMultiTexCoord4f(GLenum texture, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
MGL_EXPORT void glMultiTexCoord4x(GLenum texture, GLfixed s, GLfixed t, GLfixed r, GLfixed q);
MGL_EXPORT void glMultiTexCoord4fv(GLenum texture, const GLfloat *v);
MGL_EXPORT void glRasterPos2f(GLfloat x, GLfloat y);
MGL_EXPORT void glRasterPos3f(GLfloat x, GLfloat y, GLfloat z);
MGL_EXPORT void glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
MGL_EXPORT void glRasterPos2d(GLdouble x, GLdouble y);
MGL_EXPORT void glRasterPos3d(GLdouble x, GLdouble y, GLdouble z);
MGL_EXPORT void glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
MGL_EXPORT void glRasterPos2fv(const GLfloat *v);
MGL_EXPORT void glRasterPos3fv(const GLfloat *v);
MGL_EXPORT void glRasterPos4fv(const GLfloat *v);
MGL_EXPORT void glRasterPos2dv(const GLdouble *v);
MGL_EXPORT void glRasterPos3dv(const GLdouble *v);
MGL_EXPORT void glRasterPos4dv(const GLdouble *v);
MGL_EXPORT void glWindowPos2f(GLfloat x, GLfloat y);
MGL_EXPORT void glWindowPos3f(GLfloat x, GLfloat y, GLfloat z);
MGL_EXPORT void glWindowPos2d(GLdouble x, GLdouble y);
MGL_EXPORT void glWindowPos3d(GLdouble x, GLdouble y, GLdouble z);
MGL_EXPORT void glWindowPos2i(GLint x, GLint y);
MGL_EXPORT void glWindowPos3i(GLint x, GLint y, GLint z);
MGL_EXPORT void glWindowPos2s(GLshort x, GLshort y);
MGL_EXPORT void glWindowPos3s(GLshort x, GLshort y, GLshort z);
MGL_EXPORT void glWindowPos2fv(const GLfloat *v);
MGL_EXPORT void glWindowPos3fv(const GLfloat *v);
MGL_EXPORT void glWindowPos2dv(const GLdouble *v);
MGL_EXPORT void glWindowPos3dv(const GLdouble *v);
MGL_EXPORT void glWindowPos2iv(const GLint *v);
MGL_EXPORT void glWindowPos3iv(const GLint *v);
MGL_EXPORT void glWindowPos2sv(const GLshort *v);
MGL_EXPORT void glWindowPos3sv(const GLshort *v);
MGL_EXPORT void glPixelZoom(GLfloat xfactor, GLfloat yfactor);
MGL_EXPORT void glPixelMapfv(GLenum map, GLsizei mapsize, const GLfloat *values);
MGL_EXPORT void glPixelMapuiv(GLenum map, GLsizei mapsize, const GLuint *values);
MGL_EXPORT void glPixelMapusv(GLenum map, GLsizei mapsize, const GLushort *values);
MGL_EXPORT void glGetPixelMapfv(GLenum map, GLfloat *values);
MGL_EXPORT void glGetPixelMapuiv(GLenum map, GLuint *values);
MGL_EXPORT void glGetPixelMapusv(GLenum map, GLushort *values);
MGL_EXPORT void glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type,
                             const void *pixels);
MGL_EXPORT void glCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
MGL_EXPORT void glBitmap(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove,
                         GLfloat ymove, const GLubyte *bitmap);
MGL_EXPORT void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                             GLenum type, void *pixels);
MGL_EXPORT void glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                              GLenum type, GLsizei bufSize, void *data);
MGL_EXPORT void glMap1d(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order,
                        const GLdouble *points);
MGL_EXPORT void glMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order,
                        const GLfloat *points);
MGL_EXPORT void glMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder,
                        GLdouble v1, GLdouble v2, GLint vstride, GLint vorder,
                        const GLdouble *points);
MGL_EXPORT void glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder,
                        GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points);
MGL_EXPORT void glEvalCoord1d(GLdouble u);
MGL_EXPORT void glEvalCoord1f(GLfloat u);
MGL_EXPORT void glEvalCoord1dv(const GLdouble *u);
MGL_EXPORT void glEvalCoord1fv(const GLfloat *u);
MGL_EXPORT void glEvalCoord2d(GLdouble u, GLdouble v);
MGL_EXPORT void glEvalCoord2f(GLfloat u, GLfloat v);
MGL_EXPORT void glEvalCoord2dv(const GLdouble *uv);
MGL_EXPORT void glEvalCoord2fv(const GLfloat *uv);
MGL_EXPORT void glMapGrid1d(GLint un, GLdouble u1, GLdouble u2);
MGL_EXPORT void glMapGrid1f(GLint un, GLfloat u1, GLfloat u2);
MGL_EXPORT void glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2);
MGL_EXPORT void glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2);
MGL_EXPORT void glEvalPoint1(GLint i);
MGL_EXPORT void glEvalPoint2(GLint i, GLint j);
MGL_EXPORT void glEvalMesh1(GLenum mode, GLint i1, GLint i2);
MGL_EXPORT void glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
MGL_EXPORT void glGetMapdv(GLenum target, GLenum query, GLdouble *v);
MGL_EXPORT void glGetMapfv(GLenum target, GLenum query, GLfloat *v);
MGL_EXPORT void glGetMapiv(GLenum target, GLenum query, GLint *v);
MGL_EXPORT void glIndexf(GLfloat c);
MGL_EXPORT void glIndexfv(const GLfloat *c);
MGL_EXPORT void glIndexd(GLdouble c);
MGL_EXPORT void glIndexi(GLint c);
MGL_EXPORT void glIndexs(GLshort c);
MGL_EXPORT void glBegin(GLenum mode);
MGL_EXPORT void glArrayElement(GLint i);
MGL_EXPORT void glEnd(void);
MGL_EXPORT void glVertex2f(GLfloat x, GLfloat y);
MGL_EXPORT void glVertex2fv(const GLfloat *v);
MGL_EXPORT void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
MGL_EXPORT void glVertex3fv(const GLfloat *v);
MGL_EXPORT void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
MGL_EXPORT void glVertex4fv(const GLfloat *v);
MGL_EXPORT void glVertex2dv(const GLdouble *v);
MGL_EXPORT void glVertex2i(GLint x, GLint y);
MGL_EXPORT void glVertex2iv(const GLint *v);
MGL_EXPORT void glVertex2s(GLshort x, GLshort y);
MGL_EXPORT void glVertex2sv(const GLshort *v);
MGL_EXPORT void glVertex3dv(const GLdouble *v);
MGL_EXPORT void glVertex3i(GLint x, GLint y, GLint z);
MGL_EXPORT void glVertex3iv(const GLint *v);
MGL_EXPORT void glVertex3s(GLshort x, GLshort y, GLshort z);
MGL_EXPORT void glVertex3sv(const GLshort *v);
MGL_EXPORT void glVertex4dv(const GLdouble *v);
MGL_EXPORT void glVertex4i(GLint x, GLint y, GLint z, GLint w);
MGL_EXPORT void glVertex4iv(const GLint *v);
MGL_EXPORT void glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w);
MGL_EXPORT void glVertex4sv(const GLshort *v);
MGL_EXPORT void glVertexP2ui(GLenum type, GLuint value);
MGL_EXPORT void glVertexP2uiv(GLenum type, const GLuint *value);
MGL_EXPORT void glVertexP3ui(GLenum type, GLuint value);
MGL_EXPORT void glVertexP3uiv(GLenum type, const GLuint *value);
MGL_EXPORT void glVertexP4ui(GLenum type, GLuint value);
MGL_EXPORT void glVertexP4uiv(GLenum type, const GLuint *value);
MGL_EXPORT void glVertex2d(GLdouble x, GLdouble y);
MGL_EXPORT void glVertex3d(GLdouble x, GLdouble y, GLdouble z);
MGL_EXPORT void glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
MGL_EXPORT GLuint glGenLists(GLsizei range);
MGL_EXPORT void glNewList(GLuint list, GLenum mode);
MGL_EXPORT void glEndList(void);
MGL_EXPORT void glCallList(GLuint list);
MGL_EXPORT void glCallLists(GLsizei n, GLenum type, const void *lists);
MGL_EXPORT void glListBase(GLuint base);
MGL_EXPORT void glDeleteLists(GLuint list, GLsizei range);
MGL_EXPORT GLboolean glIsList(GLuint list);

MGL_EXPORT void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                      GLsizei stride, const void *pointer);
MGL_EXPORT void glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride,
                                       const void *pointer);
MGL_EXPORT void glEnableVertexAttribArray(GLuint index);
MGL_EXPORT void glDisableVertexAttribArray(GLuint index);
MGL_EXPORT void glVertexAttrib1f(GLuint index, GLfloat x);
MGL_EXPORT void glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y);
MGL_EXPORT void glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z);
MGL_EXPORT void glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
MGL_EXPORT void glColor3hNV(GLushort x, GLushort y, GLushort z);
MGL_EXPORT void glColor3hvNV(const GLushort *v);
MGL_EXPORT void glColor4hNV(GLushort x, GLushort y, GLushort z, GLushort w);
MGL_EXPORT void glColor4hvNV(const GLushort *v);
MGL_EXPORT void glNormal3hNV(GLushort x, GLushort y, GLushort z);
MGL_EXPORT void glNormal3hvNV(const GLushort *v);
MGL_EXPORT void glSecondaryColor3hNV(GLushort x, GLushort y, GLushort z);
MGL_EXPORT void glSecondaryColor3hvNV(const GLushort *v);
MGL_EXPORT void glFogCoordhNV(GLushort x);
MGL_EXPORT void glFogCoordhvNV(const GLushort *v);
MGL_EXPORT void glTexCoord1hNV(GLushort x);
MGL_EXPORT void glTexCoord1hvNV(const GLushort *v);
MGL_EXPORT void glTexCoord2hNV(GLushort x, GLushort y);
MGL_EXPORT void glTexCoord2hvNV(const GLushort *v);
MGL_EXPORT void glTexCoord3hNV(GLushort x, GLushort y, GLushort z);
MGL_EXPORT void glTexCoord3hvNV(const GLushort *v);
MGL_EXPORT void glTexCoord4hNV(GLushort x, GLushort y, GLushort z, GLushort w);
MGL_EXPORT void glTexCoord4hvNV(const GLushort *v);
MGL_EXPORT void glMultiTexCoord1hNV(GLenum texture, GLushort x);
MGL_EXPORT void glMultiTexCoord1hvNV(GLenum texture, const GLushort *v);
MGL_EXPORT void glMultiTexCoord2hNV(GLenum texture, GLushort x, GLushort y);
MGL_EXPORT void glMultiTexCoord2hvNV(GLenum texture, const GLushort *v);
MGL_EXPORT void glMultiTexCoord3hNV(GLenum texture, GLushort x, GLushort y, GLushort z);
MGL_EXPORT void glMultiTexCoord3hvNV(GLenum texture, const GLushort *v);
MGL_EXPORT void glMultiTexCoord4hNV(GLenum texture, GLushort x, GLushort y, GLushort z, GLushort w);
MGL_EXPORT void glMultiTexCoord4hvNV(GLenum texture, const GLushort *v);
MGL_EXPORT void glVertex2hNV(GLushort x, GLushort y);
MGL_EXPORT void glVertex2hvNV(const GLushort *v);
MGL_EXPORT void glVertex3hNV(GLushort x, GLushort y, GLushort z);
MGL_EXPORT void glVertex3hvNV(const GLushort *v);
MGL_EXPORT void glVertex4hNV(GLushort x, GLushort y, GLushort z, GLushort w);
MGL_EXPORT void glVertex4hvNV(const GLushort *v);
MGL_EXPORT void glVertexAttribs1hvNV(GLuint index, GLsizei n, const GLushort *v);
MGL_EXPORT void glVertexAttribs2hvNV(GLuint index, GLsizei n, const GLushort *v);
MGL_EXPORT void glVertexAttribs3hvNV(GLuint index, GLsizei n, const GLushort *v);
MGL_EXPORT void glVertexAttribs4hvNV(GLuint index, GLsizei n, const GLushort *v);
MGL_EXPORT void glVertexAttrib1hNV(GLuint index, GLushort x);
MGL_EXPORT void glVertexAttrib1hvNV(GLuint index, const GLushort *v);
MGL_EXPORT void glVertexAttrib2hNV(GLuint index, GLushort x, GLushort y);
MGL_EXPORT void glVertexAttrib3hNV(GLuint index, GLushort x, GLushort y, GLushort z);
MGL_EXPORT void glVertexAttrib3hvNV(GLuint index, const GLushort *v);
MGL_EXPORT void glVertexAttrib4hNV(GLuint index, GLushort x, GLushort y, GLushort z, GLushort w);
MGL_EXPORT void glVertexAttrib1fv(GLuint index, const GLfloat *v);
MGL_EXPORT void glVertexAttrib2fv(GLuint index, const GLfloat *v);
MGL_EXPORT void glVertexAttrib2hvNV(GLuint index, const GLushort *v);
MGL_EXPORT void glVertexAttrib4hvNV(GLuint index, const GLushort *v);
MGL_EXPORT void glVertexAttrib3fv(GLuint index, const GLfloat *v);
MGL_EXPORT void glVertexAttrib4fv(GLuint index, const GLfloat *v);
MGL_EXPORT void glVertexAttrib1d(GLuint index, GLdouble x);
MGL_EXPORT void glVertexAttrib2d(GLuint index, GLdouble x, GLdouble y);
MGL_EXPORT void glVertexAttrib3d(GLuint index, GLdouble x, GLdouble y, GLdouble z);
MGL_EXPORT void glVertexAttrib4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
MGL_EXPORT void glVertexAttrib1dv(GLuint index, const GLdouble *v);
MGL_EXPORT void glVertexAttrib2dv(GLuint index, const GLdouble *v);
MGL_EXPORT void glVertexAttrib3dv(GLuint index, const GLdouble *v);
MGL_EXPORT void glVertexAttrib4dv(GLuint index, const GLdouble *v);
MGL_EXPORT void glVertexAttrib1s(GLuint index, GLshort x);
MGL_EXPORT void glVertexAttrib2s(GLuint index, GLshort x, GLshort y);
MGL_EXPORT void glVertexAttrib3s(GLuint index, GLshort x, GLshort y, GLshort z);
MGL_EXPORT void glVertexAttrib4s(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
MGL_EXPORT void glVertexAttrib1sv(GLuint index, const GLshort *v);
MGL_EXPORT void glVertexAttrib2sv(GLuint index, const GLshort *v);
MGL_EXPORT void glVertexAttrib3sv(GLuint index, const GLshort *v);
MGL_EXPORT void glVertexAttrib4sv(GLuint index, const GLshort *v);
MGL_EXPORT void glVertexAttrib4bv(GLuint index, const GLbyte *v);
MGL_EXPORT void glVertexAttrib4iv(GLuint index, const GLint *v);
MGL_EXPORT void glVertexAttrib4ubv(GLuint index, const GLubyte *v);
MGL_EXPORT void glVertexAttrib4usv(GLuint index, const GLushort *v);
MGL_EXPORT void glVertexAttrib4uiv(GLuint index, const GLuint *v);
MGL_EXPORT void glVertexAttrib4Nub(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
MGL_EXPORT void glVertexAttrib4Nbv(GLuint index, const GLbyte *v);
MGL_EXPORT void glVertexAttrib4Nsv(GLuint index, const GLshort *v);
MGL_EXPORT void glVertexAttrib4Niv(GLuint index, const GLint *v);
MGL_EXPORT void glVertexAttrib4Nubv(GLuint index, const GLubyte *v);
MGL_EXPORT void glVertexAttrib4Nusv(GLuint index, const GLushort *v);
MGL_EXPORT void glVertexAttrib4Nuiv(GLuint index, const GLuint *v);
MGL_EXPORT void glVertexAttribI1i(GLuint index, GLint x);
MGL_EXPORT void glVertexAttribI2i(GLuint index, GLint x, GLint y);
MGL_EXPORT void glVertexAttribI3i(GLuint index, GLint x, GLint y, GLint z);
MGL_EXPORT void glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w);
MGL_EXPORT void glVertexAttribI1ui(GLuint index, GLuint x);
MGL_EXPORT void glVertexAttribI2ui(GLuint index, GLuint x, GLuint y);
MGL_EXPORT void glVertexAttribI3ui(GLuint index, GLuint x, GLuint y, GLuint z);
MGL_EXPORT void glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
MGL_EXPORT void glVertexAttribI1iv(GLuint index, const GLint *v);
MGL_EXPORT void glVertexAttribI2iv(GLuint index, const GLint *v);
MGL_EXPORT void glVertexAttribI3iv(GLuint index, const GLint *v);
MGL_EXPORT void glVertexAttribI4iv(GLuint index, const GLint *v);
MGL_EXPORT void glVertexAttribI1uiv(GLuint index, const GLuint *v);
MGL_EXPORT void glVertexAttribI2uiv(GLuint index, const GLuint *v);
MGL_EXPORT void glVertexAttribI3uiv(GLuint index, const GLuint *v);
MGL_EXPORT void glVertexAttribI4uiv(GLuint index, const GLuint *v);
MGL_EXPORT void glVertexAttribDivisor(GLuint index, GLuint divisor);
MGL_EXPORT void glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params);
MGL_EXPORT void glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params);
MGL_EXPORT void glGetVertexAttribdv(GLuint index, GLenum pname, GLdouble *params);
MGL_EXPORT void glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params);
MGL_EXPORT void glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params);
MGL_EXPORT void glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer);
MGL_EXPORT void glVertexAttribPointerARB(GLuint index, GLint size, GLenum type,
                                         GLboolean normalized, GLsizei stride, const void *pointer);
MGL_EXPORT void glEnableVertexAttribArrayARB(GLuint index);
MGL_EXPORT void glDisableVertexAttribArrayARB(GLuint index);
MGL_EXPORT void glVertexAttrib1fARB(GLuint index, GLfloat x);
MGL_EXPORT void glVertexAttrib2fARB(GLuint index, GLfloat x, GLfloat y);
MGL_EXPORT void glVertexAttrib3fARB(GLuint index, GLfloat x, GLfloat y, GLfloat z);
MGL_EXPORT void glVertexAttrib4fARB(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
MGL_EXPORT void glVertexAttrib1fvARB(GLuint index, const GLfloat *v);
MGL_EXPORT void glVertexAttrib2fvARB(GLuint index, const GLfloat *v);
MGL_EXPORT void glVertexAttrib3fvARB(GLuint index, const GLfloat *v);
MGL_EXPORT void glVertexAttrib4fvARB(GLuint index, const GLfloat *v);
MGL_EXPORT void glVertexAttrib1dARB(GLuint index, GLdouble x);
MGL_EXPORT void glVertexAttrib2dARB(GLuint index, GLdouble x, GLdouble y);
MGL_EXPORT void glVertexAttrib3dARB(GLuint index, GLdouble x, GLdouble y, GLdouble z);
MGL_EXPORT void glVertexAttrib4dARB(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
MGL_EXPORT void glVertexAttrib1dvARB(GLuint index, const GLdouble *v);
MGL_EXPORT void glVertexAttrib2dvARB(GLuint index, const GLdouble *v);
MGL_EXPORT void glVertexAttrib3dvARB(GLuint index, const GLdouble *v);
MGL_EXPORT void glVertexAttrib4dvARB(GLuint index, const GLdouble *v);
MGL_EXPORT void glVertexAttrib1sARB(GLuint index, GLshort x);
MGL_EXPORT void glVertexAttrib2sARB(GLuint index, GLshort x, GLshort y);
MGL_EXPORT void glVertexAttrib3sARB(GLuint index, GLshort x, GLshort y, GLshort z);
MGL_EXPORT void glVertexAttrib4sARB(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
MGL_EXPORT void glVertexAttrib1svARB(GLuint index, const GLshort *v);
MGL_EXPORT void glVertexAttrib2svARB(GLuint index, const GLshort *v);
MGL_EXPORT void glVertexAttrib3svARB(GLuint index, const GLshort *v);
MGL_EXPORT void glVertexAttrib4svARB(GLuint index, const GLshort *v);
MGL_EXPORT void glVertexAttrib4bvARB(GLuint index, const GLbyte *v);
MGL_EXPORT void glVertexAttrib4ivARB(GLuint index, const GLint *v);
MGL_EXPORT void glVertexAttrib4ubvARB(GLuint index, const GLubyte *v);
MGL_EXPORT void glVertexAttrib4usvARB(GLuint index, const GLushort *v);
MGL_EXPORT void glVertexAttrib4uivARB(GLuint index, const GLuint *v);
MGL_EXPORT void glVertexAttrib4NubARB(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
MGL_EXPORT void glVertexAttrib4NbvARB(GLuint index, const GLbyte *v);
MGL_EXPORT void glVertexAttrib4NsvARB(GLuint index, const GLshort *v);
MGL_EXPORT void glVertexAttrib4NivARB(GLuint index, const GLint *v);
MGL_EXPORT void glVertexAttrib4NubvARB(GLuint index, const GLubyte *v);
MGL_EXPORT void glVertexAttrib4NusvARB(GLuint index, const GLushort *v);
MGL_EXPORT void glVertexAttrib4NuivARB(GLuint index, const GLuint *v);
MGL_EXPORT void glGetVertexAttribivARB(GLuint index, GLenum pname, GLint *params);
MGL_EXPORT void glGetVertexAttribfvARB(GLuint index, GLenum pname, GLfloat *params);
MGL_EXPORT void glGetVertexAttribdvARB(GLuint index, GLenum pname, GLdouble *params);
MGL_EXPORT void glGetVertexAttribPointervARB(GLuint index, GLenum pname, void **pointer);

MGL_EXPORT void glClientActiveTexture(GLenum texture);
MGL_EXPORT void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void *pointer);
MGL_EXPORT void glNormalPointer(GLenum type, GLsizei stride, const void *pointer);
MGL_EXPORT void glColorPointer(GLint size, GLenum type, GLsizei stride, const void *pointer);
MGL_EXPORT void glSecondaryColorPointer(GLint size, GLenum type, GLsizei stride,
                                        const void *pointer);
MGL_EXPORT void glFogCoordPointer(GLenum type, GLsizei stride, const void *pointer);
MGL_EXPORT void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *pointer);
MGL_EXPORT void glMultiTexCoordPointerEXT(GLenum texunit, GLint size, GLenum type, GLsizei stride,
                                          const void *pointer);
MGL_EXPORT void glInterleavedArrays(GLenum format, GLsizei stride, const void *pointer);
MGL_EXPORT void glLockArrays(GLint first, GLsizei count);
MGL_EXPORT void glUnlockArrays(void);
MGL_EXPORT void glLockArraysEXT(GLint first, GLsizei count);
MGL_EXPORT void glUnlockArraysEXT(void);
MGL_EXPORT void glEnableClientState(GLenum array);
MGL_EXPORT void glDisableClientState(GLenum array);
MGL_EXPORT void glEnableClientStateIndexed(GLenum array, GLuint index);
MGL_EXPORT void glDisableClientStateIndexed(GLenum array, GLuint index);
MGL_EXPORT void glEnableClientStateIndexedEXT(GLenum array, GLuint index);
MGL_EXPORT void glDisableClientStateIndexedEXT(GLenum array, GLuint index);
MGL_EXPORT void glEnableClientStatei(GLenum array, GLuint index);
MGL_EXPORT void glDisableClientStatei(GLenum array, GLuint index);
MGL_EXPORT void glEnableClientStateiEXT(GLenum array, GLuint index);
MGL_EXPORT void glDisableClientStateiEXT(GLenum array, GLuint index);
MGL_EXPORT void glEnableVertexArray(GLuint vaobj, GLenum array);
MGL_EXPORT void glDisableVertexArray(GLuint vaobj, GLenum array);
MGL_EXPORT void glEnableVertexArrayEXT(GLuint vaobj, GLenum array);
MGL_EXPORT void glDisableVertexArrayEXT(GLuint vaobj, GLenum array);
MGL_EXPORT void glGetPointerIndexedv(GLenum pname, GLuint index, void **params);
MGL_EXPORT void glGetPointerIndexedvEXT(GLenum pname, GLuint index, void **params);

MGL_EXPORT void glGenFencesNV(GLsizei n, GLuint *fences);
MGL_EXPORT void glDeleteFencesNV(GLsizei n, const GLuint *fences);
MGL_EXPORT void glSetFenceNV(GLuint fence, GLenum condition);
MGL_EXPORT GLboolean glTestFenceNV(GLuint fence);
MGL_EXPORT void glFinishFenceNV(GLuint fence);
MGL_EXPORT GLboolean glIsFenceNV(GLuint fence);
MGL_EXPORT void glGetFenceivNV(GLuint fence, GLenum pname, GLint *params);
MGL_EXPORT GLsync glFenceSync(GLenum condition, GLbitfield flags);
MGL_EXPORT GLboolean glIsSync(GLsync sync);
MGL_EXPORT void glDeleteSync(GLsync sync);
MGL_EXPORT GLenum glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout);
MGL_EXPORT void glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout);
MGL_EXPORT void glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length,
                            GLint *values);

MGL_EXPORT void glGenTransformFeedbacks(GLsizei n, GLuint *ids);
MGL_EXPORT void glCreateTransformFeedbacks(GLsizei n, GLuint *ids);
MGL_EXPORT void glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids);
MGL_EXPORT GLboolean glIsTransformFeedback(GLuint id);
MGL_EXPORT void glBindTransformFeedback(GLenum target, GLuint id);
MGL_EXPORT void glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer);
MGL_EXPORT void glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer,
                                               GLintptr offset, GLsizeiptr size);
MGL_EXPORT void glGetTransformFeedbackiv(GLuint xfb, GLenum pname, GLint *param);
MGL_EXPORT void glGetTransformFeedbacki_v(GLuint xfb, GLenum pname, GLuint index, GLint *param);
MGL_EXPORT void glGetTransformFeedbacki64_v(GLuint xfb, GLenum pname, GLuint index, GLint64 *param);
MGL_EXPORT void glBeginTransformFeedback(GLenum primitiveMode);
MGL_EXPORT void glEndTransformFeedback(void);
MGL_EXPORT void glPauseTransformFeedback(void);
MGL_EXPORT void glResumeTransformFeedback(void);
MGL_EXPORT void glTransformFeedbackVaryings(GLuint program, GLsizei count,
                                            const GLchar *const *varyings, GLenum bufferMode);
MGL_EXPORT void glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize,
                                              GLsizei *length, GLsizei *size, GLenum *type,
                                              GLchar *name);

MGL_EXPORT void glGenSamplers(GLsizei count, GLuint *samplers);
MGL_EXPORT void glCreateSamplers(GLsizei count, GLuint *samplers);
MGL_EXPORT void glDeleteSamplers(GLsizei count, const GLuint *samplers);
MGL_EXPORT GLboolean glIsSampler(GLuint sampler);
MGL_EXPORT void glBindSampler(GLuint unit, GLuint sampler);
MGL_EXPORT void glBindSamplers(GLuint first, GLsizei count, const GLuint *samplers);
MGL_EXPORT void glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param);
MGL_EXPORT void glSamplerParameteri(GLuint sampler, GLenum pname, GLint param);
MGL_EXPORT void glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *params);
MGL_EXPORT void glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *params);
MGL_EXPORT void glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint *params);
MGL_EXPORT void glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint *params);
MGL_EXPORT void glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params);
MGL_EXPORT void glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params);
MGL_EXPORT void glGetSamplerParameterIiv(GLuint sampler, GLenum pname, GLint *params);
MGL_EXPORT void glGetSamplerParameterIuiv(GLuint sampler, GLenum pname, GLuint *params);

MGL_EXPORT void glGenQueries(GLsizei n, GLuint *ids);
MGL_EXPORT void glCreateQueries(GLenum target, GLsizei n, GLuint *ids);
MGL_EXPORT void glDeleteQueries(GLsizei n, const GLuint *ids);
MGL_EXPORT GLboolean glIsQuery(GLuint id);
MGL_EXPORT void glBeginQuery(GLenum target, GLuint id);
MGL_EXPORT void glBeginQueryIndexed(GLenum target, GLuint index, GLuint id);
MGL_EXPORT void glEndQuery(GLenum target);
MGL_EXPORT void glEndQueryIndexed(GLenum target, GLuint index);
MGL_EXPORT void glGetQueryiv(GLenum target, GLenum pname, GLint *params);
MGL_EXPORT void glGetQueryIndexediv(GLenum target, GLuint index, GLenum pname, GLint *params);
MGL_EXPORT void glGetQueryObjectiv(GLuint id, GLenum pname, GLint *params);
MGL_EXPORT void glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params);
MGL_EXPORT void glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params);
MGL_EXPORT void glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params);
MGL_EXPORT void glGetQueryBufferObjectiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset);
MGL_EXPORT void glGetQueryBufferObjectuiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset);
MGL_EXPORT void glGetQueryBufferObjecti64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset);
MGL_EXPORT void glGetQueryBufferObjectui64v(GLuint id, GLuint buffer, GLenum pname,
                                            GLintptr offset);
MGL_EXPORT void glQueryCounter(GLuint id, GLenum target);
MGL_EXPORT void glGenQueriesARB(GLsizei n, GLuint *ids);
MGL_EXPORT void glDeleteQueriesARB(GLsizei n, const GLuint *ids);
MGL_EXPORT GLboolean glIsQueryARB(GLuint id);
MGL_EXPORT void glBeginQueryARB(GLenum target, GLuint id);
MGL_EXPORT void glEndQueryARB(GLenum target);
MGL_EXPORT void glGetQueryivARB(GLenum target, GLenum pname, GLint *params);
MGL_EXPORT void glGetQueryObjectivARB(GLuint id, GLenum pname, GLint *params);
MGL_EXPORT void glGetQueryObjectuivARB(GLuint id, GLenum pname, GLuint *params);

MGL_EXPORT void glSelectBuffer(GLsizei size, GLuint *buffer);
MGL_EXPORT GLint glRenderMode(GLenum mode);
MGL_EXPORT void glInitNames(void);
MGL_EXPORT void glPushName(GLuint name);
MGL_EXPORT void glPopName(void);
MGL_EXPORT void glLoadName(GLuint name);

MGL_EXPORT void glDrawTransformFeedback(GLenum mode, GLuint id);
MGL_EXPORT void glDrawTransformFeedbackInstanced(GLenum mode, GLuint id, GLsizei instancecount);
MGL_EXPORT void glDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream);
MGL_EXPORT void glDrawTransformFeedbackStreamInstanced(GLenum mode, GLuint id, GLuint stream,
                                                       GLsizei instancecount);
MGL_EXPORT void glGetProgramStageiv(GLuint program, GLenum shadertype, GLenum pname, GLint *values);
MGL_EXPORT void glDrawArrays(GLenum mode, GLint first, GLsizei count);
MGL_EXPORT void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);
MGL_EXPORT void glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                         const void *indices, GLint basevertex);
MGL_EXPORT void glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count,
                                              GLenum type, const void *indices, GLint basevertex);
MGL_EXPORT void glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type,
                                              const void *const *indices, GLsizei drawcount,
                                              const GLint *basevertex);
MGL_EXPORT void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                                      GLsizei instancecount);
MGL_EXPORT void glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count,
                                                  GLsizei instancecount, GLuint baseinstance);
MGL_EXPORT void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                                        const void *indices, GLsizei instancecount);
MGL_EXPORT void glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type,
                                                    const void *indices, GLsizei instancecount,
                                                    GLuint baseinstance);
MGL_EXPORT void glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                                  const void *indices, GLsizei instancecount,
                                                  GLint basevertex);
MGL_EXPORT void glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count,
                                                              GLenum type, const void *indices,
                                                              GLsizei instancecount,
                                                              GLint basevertex,
                                                              GLuint baseinstance);
MGL_EXPORT void glDrawArraysIndirect(GLenum mode, const void *indirect);
MGL_EXPORT void glDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect);
MGL_EXPORT void glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount,
                                          GLsizei stride);
MGL_EXPORT void glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect,
                                            GLsizei drawcount, GLsizei stride);
MGL_EXPORT void glMultiDrawArraysIndirectCount(GLenum mode, const void *indirect,
                                               GLintptr drawcount, GLsizei maxdrawcount,
                                               GLsizei stride);
MGL_EXPORT void glMultiDrawElementsIndirectCount(GLenum mode, GLenum type, const void *indirect,
                                                 GLintptr drawcount, GLsizei maxdrawcount,
                                                 GLsizei stride);
MGL_EXPORT void glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
                                    GLenum type, const void *indices);
MGL_EXPORT void glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count,
                                  GLsizei drawcount);
MGL_EXPORT void glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type,
                                    const void *const *indices, GLsizei drawcount);
MGL_EXPORT void glBeginConditionalRender(GLuint id, GLenum mode);
MGL_EXPORT void glEndConditionalRender(void);
MGL_EXPORT void glPrimitiveRestartIndex(GLuint index);
MGL_EXPORT void glProvokingVertex(GLenum mode);
MGL_EXPORT void glProvokingVertexEXT(GLenum mode);

MGL_EXPORT void glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
MGL_EXPORT void glDispatchComputeIndirect(GLintptr indirect);
MGL_EXPORT void glMemoryBarrier(GLbitfield barriers);
MGL_EXPORT void glMemoryBarrierByRegion(GLbitfield barriers);
MGL_EXPORT void glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered,
                                   GLint layer, GLenum access, GLenum format);
MGL_EXPORT void glBindImageTextures(GLuint first, GLsizei count, const GLuint *textures);

MGL_EXPORT void glGenProgramsARB(GLsizei n, GLuint *programs);
MGL_EXPORT void glDeleteProgramsARB(GLsizei n, const GLuint *programs);
MGL_EXPORT void glBindProgramARB(GLenum target, GLuint program);
MGL_EXPORT GLboolean glIsProgramARB(GLuint program);
MGL_EXPORT void glProgramStringARB(GLenum target, GLenum format, GLsizei len, const void *string);
MGL_EXPORT void glProgramEnvParameter4fARB(GLenum target, GLuint index, GLfloat x, GLfloat y,
                                           GLfloat z, GLfloat w);
MGL_EXPORT void glProgramEnvParameter4fvARB(GLenum target, GLuint index, const GLfloat *params);
MGL_EXPORT void glProgramEnvParameter4dARB(GLenum target, GLuint index, GLdouble x, GLdouble y,
                                           GLdouble z, GLdouble w);
MGL_EXPORT void glProgramEnvParameter4dvARB(GLenum target, GLuint index, const GLdouble *params);
MGL_EXPORT void glProgramEnvParameters4fvEXT(GLenum target, GLuint index, GLsizei count,
                                             const GLfloat *params);
MGL_EXPORT void glProgramLocalParameter4fARB(GLenum target, GLuint index, GLfloat x, GLfloat y,
                                             GLfloat z, GLfloat w);
MGL_EXPORT void glProgramLocalParameter4fvARB(GLenum target, GLuint index, const GLfloat *params);
MGL_EXPORT void glProgramLocalParameter4dARB(GLenum target, GLuint index, GLdouble x, GLdouble y,
                                             GLdouble z, GLdouble w);
MGL_EXPORT void glProgramLocalParameter4dvARB(GLenum target, GLuint index, const GLdouble *params);
MGL_EXPORT void glProgramLocalParameters4fvEXT(GLenum target, GLuint index, GLsizei count,
                                               const GLfloat *params);
MGL_EXPORT void glGetProgramEnvParameterfvARB(GLenum target, GLuint index, GLfloat *params);
MGL_EXPORT void glGetProgramEnvParameterdvARB(GLenum target, GLuint index, GLdouble *params);
MGL_EXPORT void glGetProgramLocalParameterfvARB(GLenum target, GLuint index, GLfloat *params);
MGL_EXPORT void glGetProgramLocalParameterdvARB(GLenum target, GLuint index, GLdouble *params);
MGL_EXPORT void glGetProgramivARB(GLenum target, GLenum pname, GLint *params);
MGL_EXPORT void glGetProgramStringARB(GLenum target, GLenum pname, void *string);

MGL_EXPORT void glGenBuffersARB(GLsizei n, GLuint *buffers);
MGL_EXPORT void glDeleteBuffersARB(GLsizei n, const GLuint *buffers);
MGL_EXPORT void glBindBufferARB(GLenum target, GLuint buffer);
MGL_EXPORT GLboolean glIsBufferARB(GLuint buffer);
MGL_EXPORT void glBufferDataARB(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
MGL_EXPORT void glBufferSubDataARB(GLenum target, GLintptr offset, GLsizeiptr size,
                                   const void *data);
MGL_EXPORT void glGetBufferSubDataARB(GLenum target, GLintptr offset, GLsizeiptr size, void *data);
MGL_EXPORT void *glMapBufferARB(GLenum target, GLenum access);
MGL_EXPORT GLboolean glUnmapBufferARB(GLenum target);
MGL_EXPORT void glGetBufferParameterivARB(GLenum target, GLenum pname, GLint *params);
MGL_EXPORT void glGetBufferPointervARB(GLenum target, GLenum pname, void **params);

MGL_EXPORT void glGenFramebuffersEXT(GLsizei n, GLuint *framebuffers);
MGL_EXPORT void glDeleteFramebuffersEXT(GLsizei n, const GLuint *framebuffers);
MGL_EXPORT void glBindFramebufferEXT(GLenum target, GLuint framebuffer);
MGL_EXPORT GLboolean glIsFramebufferEXT(GLuint framebuffer);
MGL_EXPORT GLenum glCheckFramebufferStatusEXT(GLenum target);
MGL_EXPORT void glFramebufferRenderbufferEXT(GLenum target, GLenum attachment,
                                             GLenum renderbuffertarget, GLuint renderbuffer);
MGL_EXPORT void glBlitFramebufferEXT(GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint dx0,
                                     GLint dy0, GLint dx1, GLint dy1, GLbitfield mask,
                                     GLenum filter);
MGL_EXPORT void glGetFramebufferAttachmentParameterivEXT(GLenum target, GLenum attachment,
                                                         GLenum pname, GLint *params);
MGL_EXPORT void glGenRenderbuffersEXT(GLsizei n, GLuint *renderbuffers);
MGL_EXPORT void glDeleteRenderbuffersEXT(GLsizei n, const GLuint *renderbuffers);
MGL_EXPORT void glBindRenderbufferEXT(GLenum target, GLuint renderbuffer);
MGL_EXPORT GLboolean glIsRenderbufferEXT(GLuint renderbuffer);
MGL_EXPORT void glRenderbufferStorageEXT(GLenum target, GLenum internalformat, GLsizei width,
                                         GLsizei height);
MGL_EXPORT void glRenderbufferStorageMultisampleEXT(GLenum target, GLsizei samples,
                                                    GLenum internalformat, GLsizei width,
                                                    GLsizei height);
MGL_EXPORT void glGetRenderbufferParameterivEXT(GLenum target, GLenum pname, GLint *params);

MGL_EXPORT void glFramebufferTexture1DEXT(GLenum target, GLenum attachment, GLenum textarget,
                                          GLuint texture, GLint level);
MGL_EXPORT void glFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget,
                                          GLuint texture, GLint level);
MGL_EXPORT void glFramebufferTexture3DEXT(GLenum target, GLenum attachment, GLenum textarget,
                                          GLuint texture, GLint level, GLint zoffset);
MGL_EXPORT void glFramebufferTextureARB(GLenum target, GLenum attachment, GLuint texture,
                                        GLint level);
MGL_EXPORT void glFramebufferTextureLayerARB(GLenum target, GLenum attachment, GLuint texture,
                                             GLint level, GLint layer);
MGL_EXPORT void glFramebufferTextureFaceARB(GLenum target, GLenum attachment, GLuint texture,
                                            GLint level, GLenum face);
MGL_EXPORT void glGenerateMipmapEXT(GLenum target);
MGL_EXPORT void glTexImage3DEXT(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                GLsizei height, GLsizei depth, GLint border, GLenum format,
                                GLenum type, const void *pixels);
MGL_EXPORT void glTexSubImage3DEXT(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                   GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                   GLenum format, GLenum type, const void *pixels);

MGL_EXPORT void glActiveTextureARB(GLenum texture);
MGL_EXPORT void glClientActiveTextureARB(GLenum texture);
MGL_EXPORT void glBlendColorEXT(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
MGL_EXPORT void glBlendEquationEXT(GLenum mode);
MGL_EXPORT void glBlendEquationSeparateEXT(GLenum modeRGB, GLenum modeAlpha);
MGL_EXPORT void glBlendFuncSeparateEXT(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
                                       GLenum dstAlpha);
MGL_EXPORT void glSampleCoverageARB(GLfloat value, GLboolean invert);

MGL_EXPORT void glColorP3ui(GLenum type, GLuint color);
MGL_EXPORT void glColorP3uiv(GLenum type, const GLuint *color);
MGL_EXPORT void glColorP4ui(GLenum type, GLuint color);
MGL_EXPORT void glColorP4uiv(GLenum type, const GLuint *color);
MGL_EXPORT void glIndexdv(const GLdouble *c);
MGL_EXPORT void glIndexiv(const GLint *c);
MGL_EXPORT void glIndexsv(const GLshort *c);
MGL_EXPORT void glIndexub(GLubyte c);
MGL_EXPORT void glIndexubv(const GLubyte *c);
MGL_EXPORT void glMultiTexCoordP1ui(GLenum texture, GLenum type, GLuint coords);
MGL_EXPORT void glMultiTexCoordP1uiv(GLenum texture, GLenum type, const GLuint *coords);
MGL_EXPORT void glMultiTexCoordP2ui(GLenum texture, GLenum type, GLuint coords);
MGL_EXPORT void glMultiTexCoordP2uiv(GLenum texture, GLenum type, const GLuint *coords);
MGL_EXPORT void glMultiTexCoordP3ui(GLenum texture, GLenum type, GLuint coords);
MGL_EXPORT void glMultiTexCoordP3uiv(GLenum texture, GLenum type, const GLuint *coords);
MGL_EXPORT void glMultiTexCoordP4ui(GLenum texture, GLenum type, GLuint coords);
MGL_EXPORT void glMultiTexCoordP4uiv(GLenum texture, GLenum type, const GLuint *coords);
MGL_EXPORT void glNormalP3ui(GLenum type, GLuint coords);
MGL_EXPORT void glNormalP3uiv(GLenum type, const GLuint *coords);
MGL_EXPORT void glRasterPos2i(GLint x, GLint y);
MGL_EXPORT void glRasterPos2iv(const GLint *v);
MGL_EXPORT void glRasterPos2s(GLshort x, GLshort y);
MGL_EXPORT void glRasterPos2sv(const GLshort *v);
MGL_EXPORT void glRasterPos3i(GLint x, GLint y, GLint z);
MGL_EXPORT void glRasterPos3iv(const GLint *v);
MGL_EXPORT void glRasterPos3s(GLshort x, GLshort y, GLshort z);
MGL_EXPORT void glRasterPos3sv(const GLshort *v);
MGL_EXPORT void glRasterPos4i(GLint x, GLint y, GLint z, GLint w);
MGL_EXPORT void glRasterPos4iv(const GLint *v);
MGL_EXPORT void glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w);
MGL_EXPORT void glRasterPos4sv(const GLshort *v);
MGL_EXPORT void glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);
MGL_EXPORT void glRectdv(const GLdouble *v1, const GLdouble *v2);
MGL_EXPORT void glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
MGL_EXPORT void glRectfv(const GLfloat *v1, const GLfloat *v2);
MGL_EXPORT void glRecti(GLint x1, GLint y1, GLint x2, GLint y2);
MGL_EXPORT void glRectiv(const GLint *v1, const GLint *v2);
MGL_EXPORT void glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2);
MGL_EXPORT void glRectsv(const GLshort *v1, const GLshort *v2);
MGL_EXPORT void glSecondaryColor3b(GLbyte red, GLbyte green, GLbyte blue);
MGL_EXPORT void glSecondaryColor3bv(const GLbyte *v);
MGL_EXPORT void glSecondaryColor3d(GLdouble red, GLdouble green, GLdouble blue);
MGL_EXPORT void glSecondaryColor3dv(const GLdouble *v);
MGL_EXPORT void glSecondaryColor3i(GLint red, GLint green, GLint blue);
MGL_EXPORT void glSecondaryColor3iv(const GLint *v);
MGL_EXPORT void glSecondaryColor3s(GLshort red, GLshort green, GLshort blue);
MGL_EXPORT void glSecondaryColor3sv(const GLshort *v);
MGL_EXPORT void glSecondaryColor3ub(GLubyte red, GLubyte green, GLubyte blue);
MGL_EXPORT void glSecondaryColor3ubEXT(GLubyte red, GLubyte green, GLubyte blue);
MGL_EXPORT void glSecondaryColor3ubv(const GLubyte *v);
MGL_EXPORT void glSecondaryColor3ubvEXT(const GLubyte *v);
MGL_EXPORT void glSecondaryColor3ui(GLuint red, GLuint green, GLuint blue);
MGL_EXPORT void glSecondaryColor3uiv(const GLuint *v);
MGL_EXPORT void glSecondaryColor3us(GLushort red, GLushort green, GLushort blue);
MGL_EXPORT void glSecondaryColor3usv(const GLushort *v);
MGL_EXPORT void glSecondaryColorP3ui(GLenum type, GLuint color);
MGL_EXPORT void glSecondaryColorP3uiv(GLenum type, const GLuint *color);
MGL_EXPORT void glTexCoordP1ui(GLenum type, GLuint coords);
MGL_EXPORT void glTexCoordP1uiv(GLenum type, const GLuint *coords);
MGL_EXPORT void glTexCoordP2ui(GLenum type, GLuint coords);
MGL_EXPORT void glTexCoordP2uiv(GLenum type, const GLuint *coords);
MGL_EXPORT void glTexCoordP3ui(GLenum type, GLuint coords);
MGL_EXPORT void glTexCoordP3uiv(GLenum type, const GLuint *coords);
MGL_EXPORT void glTexCoordP4ui(GLenum type, GLuint coords);
MGL_EXPORT void glTexCoordP4uiv(GLenum type, const GLuint *coords);
MGL_EXPORT void glVertexAttribI4bv(GLuint index, const GLbyte *v);
MGL_EXPORT void glVertexAttribI4bvEXT(GLuint index, const GLbyte *v);
MGL_EXPORT void glVertexAttribI4sv(GLuint index, const GLshort *v);
MGL_EXPORT void glVertexAttribI4svEXT(GLuint index, const GLshort *v);
MGL_EXPORT void glVertexAttribI4ubv(GLuint index, const GLubyte *v);
MGL_EXPORT void glVertexAttribI4ubvEXT(GLuint index, const GLubyte *v);
MGL_EXPORT void glVertexAttribI4usv(GLuint index, const GLushort *v);
MGL_EXPORT void glVertexAttribI4usvEXT(GLuint index, const GLushort *v);
MGL_EXPORT void glVertexAttribP1ui(GLuint index, GLenum type, GLboolean normalized, GLuint value);
MGL_EXPORT void glVertexAttribP1uiv(GLuint index, GLenum type, GLboolean normalized,
                                    const GLuint *value);
MGL_EXPORT void glVertexAttribP2ui(GLuint index, GLenum type, GLboolean normalized, GLuint value);
MGL_EXPORT void glVertexAttribP2uiv(GLuint index, GLenum type, GLboolean normalized,
                                    const GLuint *value);
MGL_EXPORT void glVertexAttribP3ui(GLuint index, GLenum type, GLboolean normalized, GLuint value);
MGL_EXPORT void glVertexAttribP3uiv(GLuint index, GLenum type, GLboolean normalized,
                                    const GLuint *value);
MGL_EXPORT void glVertexAttribP4ui(GLuint index, GLenum type, GLboolean normalized, GLuint value);
MGL_EXPORT void glVertexAttribP4uiv(GLuint index, GLenum type, GLboolean normalized,
                                    const GLuint *value);

MGL_EXPORT void glGenProgramPipelines(GLsizei n, GLuint *pipelines);
MGL_EXPORT void glDeleteProgramPipelines(GLsizei n, const GLuint *pipelines);
MGL_EXPORT void glBindProgramPipeline(GLuint pipeline);
MGL_EXPORT GLboolean glIsProgramPipeline(GLuint pipeline);
MGL_EXPORT void glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program);
MGL_EXPORT void glActiveShaderProgram(GLuint pipeline, GLuint program);
MGL_EXPORT void glValidateProgramPipeline(GLuint pipeline);
MGL_EXPORT void glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params);
MGL_EXPORT void glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei *length,
                                            GLchar *infoLog);
MGL_EXPORT void glCreateProgramPipelines(GLsizei n, GLuint *pipelines);
MGL_EXPORT GLuint glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar *const *strings);

MGL_EXPORT void glGetActiveSubroutineName(GLuint program, GLenum shadertype, GLuint index,
                                          GLsizei bufSize, GLsizei *length, GLchar *name);
MGL_EXPORT void glGetActiveSubroutineUniformName(GLuint program, GLenum shadertype, GLuint index,
                                                 GLsizei bufSize, GLsizei *length, GLchar *name);
MGL_EXPORT void glGetActiveSubroutineUniformiv(GLuint program, GLenum shadertype, GLuint index,
                                               GLenum pname, GLint *values);
MGL_EXPORT GLuint glGetSubroutineIndex(GLuint program, GLenum shadertype, const GLchar *name);
MGL_EXPORT GLint glGetSubroutineUniformLocation(GLuint program, GLenum shadertype,
                                                const GLchar *name);
MGL_EXPORT void glGetUniformSubroutineuiv(GLenum shadertype, GLint location, GLuint *params);
MGL_EXPORT void glGetVertexAttribLdv(GLuint index, GLenum pname, GLdouble *params);
MGL_EXPORT void glProgramUniform1d(GLuint program, GLint location, GLdouble v0);
MGL_EXPORT void glProgramUniform1dv(GLuint program, GLint location, GLsizei count,
                                    const GLdouble *value);
MGL_EXPORT void glProgramUniform2d(GLuint program, GLint location, GLdouble v0, GLdouble v1);
MGL_EXPORT void glProgramUniform2dv(GLuint program, GLint location, GLsizei count,
                                    const GLdouble *value);
MGL_EXPORT void glProgramUniform3d(GLuint program, GLint location, GLdouble v0, GLdouble v1,
                                   GLdouble v2);
MGL_EXPORT void glProgramUniform3dv(GLuint program, GLint location, GLsizei count,
                                    const GLdouble *value);
MGL_EXPORT void glProgramUniform4d(GLuint program, GLint location, GLdouble v0, GLdouble v1,
                                   GLdouble v2, GLdouble v3);
MGL_EXPORT void glProgramUniform4dv(GLuint program, GLint location, GLsizei count,
                                    const GLdouble *value);
MGL_EXPORT void glProgramUniformMatrix2dv(GLuint program, GLint location, GLsizei count,
                                          GLboolean transpose, const GLdouble *value);
MGL_EXPORT void glProgramUniformMatrix2x3dv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLdouble *value);
MGL_EXPORT void glProgramUniformMatrix2x4dv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLdouble *value);
MGL_EXPORT void glProgramUniformMatrix3dv(GLuint program, GLint location, GLsizei count,
                                          GLboolean transpose, const GLdouble *value);
MGL_EXPORT void glProgramUniformMatrix3x2dv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLdouble *value);
MGL_EXPORT void glProgramUniformMatrix3x4dv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLdouble *value);
MGL_EXPORT void glProgramUniformMatrix4dv(GLuint program, GLint location, GLsizei count,
                                          GLboolean transpose, const GLdouble *value);
MGL_EXPORT void glProgramUniformMatrix4x2dv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLdouble *value);
MGL_EXPORT void glProgramUniformMatrix4x3dv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLdouble *value);
MGL_EXPORT void glUniform1d(GLint location, GLdouble x);
MGL_EXPORT void glUniform1dv(GLint location, GLsizei count, const GLdouble *value);
MGL_EXPORT void glUniform2d(GLint location, GLdouble x, GLdouble y);
MGL_EXPORT void glUniform2dv(GLint location, GLsizei count, const GLdouble *value);
MGL_EXPORT void glUniform3d(GLint location, GLdouble x, GLdouble y, GLdouble z);
MGL_EXPORT void glUniform3dv(GLint location, GLsizei count, const GLdouble *value);
MGL_EXPORT void glUniform4d(GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
MGL_EXPORT void glUniform4dv(GLint location, GLsizei count, const GLdouble *value);
MGL_EXPORT void glUniformMatrix2dv(GLint location, GLsizei count, GLboolean transpose,
                                   const GLdouble *value);
MGL_EXPORT void glUniformMatrix2x3dv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLdouble *value);
MGL_EXPORT void glUniformMatrix2x4dv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLdouble *value);
MGL_EXPORT void glUniformMatrix3dv(GLint location, GLsizei count, GLboolean transpose,
                                   const GLdouble *value);
MGL_EXPORT void glUniformMatrix3x2dv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLdouble *value);
MGL_EXPORT void glUniformMatrix3x4dv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLdouble *value);
MGL_EXPORT void glUniformMatrix4dv(GLint location, GLsizei count, GLboolean transpose,
                                   const GLdouble *value);
MGL_EXPORT void glUniformMatrix4x2dv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLdouble *value);
MGL_EXPORT void glUniformMatrix4x3dv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLdouble *value);
MGL_EXPORT void glUniformSubroutinesuiv(GLenum shadertype, GLsizei count, const GLuint *indices);
MGL_EXPORT void glVertexArrayAttribLFormat(GLuint vaobj, GLuint attribindex, GLint size,
                                           GLenum type, GLuint relativeoffset);
MGL_EXPORT void glVertexAttribL1d(GLuint index, GLdouble x);
MGL_EXPORT void glVertexAttribL1dv(GLuint index, const GLdouble *v);
MGL_EXPORT void glVertexAttribL2d(GLuint index, GLdouble x, GLdouble y);
MGL_EXPORT void glVertexAttribL2dv(GLuint index, const GLdouble *v);
MGL_EXPORT void glVertexAttribL3d(GLuint index, GLdouble x, GLdouble y, GLdouble z);
MGL_EXPORT void glVertexAttribL3dv(GLuint index, const GLdouble *v);
MGL_EXPORT void glVertexAttribL4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
MGL_EXPORT void glVertexAttribL4dv(GLuint index, const GLdouble *v);
MGL_EXPORT void glVertexAttribLFormat(GLuint attribindex, GLint size, GLenum type,
                                      GLuint relativeoffset);
MGL_EXPORT void glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride,
                                       const void *pointer);

#ifdef __cplusplus
}
#endif
#endif

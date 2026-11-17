#include "abi/unimplemented_exports.h"
#include "core/context.h"
#include "core/error.h"

#include <stdatomic.h>
#include <stdio.h>
#if defined(__ANDROID__)
#include <android/log.h>
#endif

static void mgl_unimplemented_call(const char *name, _Atomic unsigned long *counter) {
    unsigned long count = atomic_fetch_add_explicit(counter, 1ul, memory_order_relaxed) + 1ul;
    struct mgl_context *ctx = mgl_context_current();
    if ((count & (count - 1ul)) == 0ul) {
        fprintf(stderr, "mGL: unimplemented %s call=%lu\n", name, count);
        fflush(stderr);
#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_ERROR, "ManifestGL", "mGL: unimplemented %s call=%lu", name,
                            count);
#endif
    }
    if (ctx != NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
    }
}

MGL_EXPORT void glGetnColorTable(GLenum target, GLenum format, GLenum type, GLsizei bufSize,
                                 void *table) {
    static _Atomic unsigned long calls = 0ul;
    (void)target;
    (void)format;
    (void)type;
    (void)bufSize;
    (void)table;
    mgl_unimplemented_call("glGetnColorTable", &calls);
}

MGL_EXPORT void glGetnConvolutionFilter(GLenum target, GLenum format, GLenum type, GLsizei bufSize,
                                        void *image) {
    static _Atomic unsigned long calls = 0ul;
    (void)target;
    (void)format;
    (void)type;
    (void)bufSize;
    (void)image;
    mgl_unimplemented_call("glGetnConvolutionFilter", &calls);
}

MGL_EXPORT void glGetnHistogram(GLenum target, GLboolean reset, GLenum format, GLenum type,
                                GLsizei bufSize, void *values) {
    static _Atomic unsigned long calls = 0ul;
    (void)target;
    (void)reset;
    (void)format;
    (void)type;
    (void)bufSize;
    (void)values;
    mgl_unimplemented_call("glGetnHistogram", &calls);
}

MGL_EXPORT void glGetnMinmax(GLenum target, GLboolean reset, GLenum format, GLenum type,
                             GLsizei bufSize, void *values) {
    static _Atomic unsigned long calls = 0ul;
    (void)target;
    (void)reset;
    (void)format;
    (void)type;
    (void)bufSize;
    (void)values;
    mgl_unimplemented_call("glGetnMinmax", &calls);
}

MGL_EXPORT void glGetnSeparableFilter(GLenum target, GLenum format, GLenum type, GLsizei rowBufSize,
                                      void *row, GLsizei columnBufSize, void *column, void *span) {
    static _Atomic unsigned long calls = 0ul;
    (void)target;
    (void)format;
    (void)type;
    (void)rowBufSize;
    (void)row;
    (void)columnBufSize;
    (void)column;
    (void)span;
    mgl_unimplemented_call("glGetnSeparableFilter", &calls);
}

MGL_EXPORT void glAlphaFragmentOp1ATI(GLenum op, GLuint dst, GLuint dstMod, GLuint arg1,
                                      GLuint arg1Rep, GLuint arg1Mod) {
    static _Atomic unsigned long calls = 0ul;
    (void)op;
    (void)dst;
    (void)dstMod;
    (void)arg1;
    (void)arg1Rep;
    (void)arg1Mod;
    mgl_unimplemented_call("glAlphaFragmentOp1ATI", &calls);
}

MGL_EXPORT void glAlphaFragmentOp2ATI(GLenum op, GLuint dst, GLuint dstMod, GLuint arg1,
                                      GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep,
                                      GLuint arg2Mod) {
    static _Atomic unsigned long calls = 0ul;
    (void)op;
    (void)dst;
    (void)dstMod;
    (void)arg1;
    (void)arg1Rep;
    (void)arg1Mod;
    (void)arg2;
    (void)arg2Rep;
    (void)arg2Mod;
    mgl_unimplemented_call("glAlphaFragmentOp2ATI", &calls);
}

MGL_EXPORT void glAlphaFragmentOp3ATI(GLenum op, GLuint dst, GLuint dstMod, GLuint arg1,
                                      GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep,
                                      GLuint arg2Mod, GLuint arg3, GLuint arg3Rep, GLuint arg3Mod) {
    static _Atomic unsigned long calls = 0ul;
    (void)op;
    (void)dst;
    (void)dstMod;
    (void)arg1;
    (void)arg1Rep;
    (void)arg1Mod;
    (void)arg2;
    (void)arg2Rep;
    (void)arg2Mod;
    (void)arg3;
    (void)arg3Rep;
    (void)arg3Mod;
    mgl_unimplemented_call("glAlphaFragmentOp3ATI", &calls);
}

MGL_EXPORT void glBeginFragmentShaderATI(void) {
    static _Atomic unsigned long calls = 0ul;
    mgl_unimplemented_call("glBeginFragmentShaderATI", &calls);
}

MGL_EXPORT void glBindFragmentShaderATI(GLuint id) {
    static _Atomic unsigned long calls = 0ul;
    (void)id;
    mgl_unimplemented_call("glBindFragmentShaderATI", &calls);
}

MGL_EXPORT void glBufferParameteriAPPLE(GLenum target, GLenum pname, GLint param) {
    static _Atomic unsigned long calls = 0ul;
    (void)target;
    (void)pname;
    (void)param;
    mgl_unimplemented_call("glBufferParameteriAPPLE", &calls);
}

MGL_EXPORT void glColorFragmentOp1ATI(GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod,
                                      GLuint arg1, GLuint arg1Rep, GLuint arg1Mod) {
    static _Atomic unsigned long calls = 0ul;
    (void)op;
    (void)dst;
    (void)dstMask;
    (void)dstMod;
    (void)arg1;
    (void)arg1Rep;
    (void)arg1Mod;
    mgl_unimplemented_call("glColorFragmentOp1ATI", &calls);
}

MGL_EXPORT void glColorFragmentOp2ATI(GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod,
                                      GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2,
                                      GLuint arg2Rep, GLuint arg2Mod) {
    static _Atomic unsigned long calls = 0ul;
    (void)op;
    (void)dst;
    (void)dstMask;
    (void)dstMod;
    (void)arg1;
    (void)arg1Rep;
    (void)arg1Mod;
    (void)arg2;
    (void)arg2Rep;
    (void)arg2Mod;
    mgl_unimplemented_call("glColorFragmentOp2ATI", &calls);
}

MGL_EXPORT void glColorFragmentOp3ATI(GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod,
                                      GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2,
                                      GLuint arg2Rep, GLuint arg2Mod, GLuint arg3, GLuint arg3Rep,
                                      GLuint arg3Mod) {
    static _Atomic unsigned long calls = 0ul;
    (void)op;
    (void)dst;
    (void)dstMask;
    (void)dstMod;
    (void)arg1;
    (void)arg1Rep;
    (void)arg1Mod;
    (void)arg2;
    (void)arg2Rep;
    (void)arg2Mod;
    (void)arg3;
    (void)arg3Rep;
    (void)arg3Mod;
    mgl_unimplemented_call("glColorFragmentOp3ATI", &calls);
}

MGL_EXPORT void glCombinerInputNV(GLenum stage, GLenum portion, GLenum variable, GLenum input,
                                  GLenum mapping, GLenum componentUsage) {
    static _Atomic unsigned long calls = 0ul;
    (void)stage;
    (void)portion;
    (void)variable;
    (void)input;
    (void)mapping;
    (void)componentUsage;
    mgl_unimplemented_call("glCombinerInputNV", &calls);
}

MGL_EXPORT void glCombinerOutputNV(GLenum stage, GLenum portion, GLenum abOutput, GLenum cdOutput,
                                   GLenum sumOutput, GLenum scale, GLenum bias,
                                   GLboolean abDotProduct, GLboolean cdDotProduct,
                                   GLboolean muxSum) {
    static _Atomic unsigned long calls = 0ul;
    (void)stage;
    (void)portion;
    (void)abOutput;
    (void)cdOutput;
    (void)sumOutput;
    (void)scale;
    (void)bias;
    (void)abDotProduct;
    (void)cdDotProduct;
    (void)muxSum;
    mgl_unimplemented_call("glCombinerOutputNV", &calls);
}

MGL_EXPORT void glCombinerParameterfNV(GLenum pname, GLfloat param) {
    static _Atomic unsigned long calls = 0ul;
    (void)pname;
    (void)param;
    mgl_unimplemented_call("glCombinerParameterfNV", &calls);
}

MGL_EXPORT void glCombinerParameterfvNV(GLenum pname, const GLfloat *params) {
    static _Atomic unsigned long calls = 0ul;
    (void)pname;
    (void)params;
    mgl_unimplemented_call("glCombinerParameterfvNV", &calls);
}

MGL_EXPORT void glCombinerParameteriNV(GLenum pname, GLint param) {
    static _Atomic unsigned long calls = 0ul;
    (void)pname;
    (void)param;
    mgl_unimplemented_call("glCombinerParameteriNV", &calls);
}

MGL_EXPORT void glCombinerParameterivNV(GLenum pname, const GLint *params) {
    static _Atomic unsigned long calls = 0ul;
    (void)pname;
    (void)params;
    mgl_unimplemented_call("glCombinerParameterivNV", &calls);
}

MGL_EXPORT void glDeleteFencesAPPLE(GLsizei n, const GLuint *fences) {
    static _Atomic unsigned long calls = 0ul;
    (void)n;
    (void)fences;
    mgl_unimplemented_call("glDeleteFencesAPPLE", &calls);
}

MGL_EXPORT void glDeleteFragmentShaderATI(GLuint id) {
    static _Atomic unsigned long calls = 0ul;
    (void)id;
    mgl_unimplemented_call("glDeleteFragmentShaderATI", &calls);
}

MGL_EXPORT void glEndFragmentShaderATI(void) {
    static _Atomic unsigned long calls = 0ul;
    mgl_unimplemented_call("glEndFragmentShaderATI", &calls);
}

MGL_EXPORT void glFinalCombinerInputNV(GLenum variable, GLenum input, GLenum mapping,
                                       GLenum componentUsage) {
    static _Atomic unsigned long calls = 0ul;
    (void)variable;
    (void)input;
    (void)mapping;
    (void)componentUsage;
    mgl_unimplemented_call("glFinalCombinerInputNV", &calls);
}

MGL_EXPORT void glFinishFenceAPPLE(GLuint fence) {
    static _Atomic unsigned long calls = 0ul;
    (void)fence;
    mgl_unimplemented_call("glFinishFenceAPPLE", &calls);
}

MGL_EXPORT void glFinishObjectAPPLE(GLenum object, GLint name) {
    static _Atomic unsigned long calls = 0ul;
    (void)object;
    (void)name;
    mgl_unimplemented_call("glFinishObjectAPPLE", &calls);
}

MGL_EXPORT void glFlushMappedBufferRangeAPPLE(GLenum target, GLintptr offset, GLsizeiptr size) {
    static _Atomic unsigned long calls = 0ul;
    (void)target;
    (void)offset;
    (void)size;
    mgl_unimplemented_call("glFlushMappedBufferRangeAPPLE", &calls);
}

MGL_EXPORT void glGenFencesAPPLE(GLsizei n, GLuint *fences) {
    static _Atomic unsigned long calls = 0ul;
    (void)n;
    (void)fences;
    mgl_unimplemented_call("glGenFencesAPPLE", &calls);
}

MGL_EXPORT GLuint glGenFragmentShadersATI(GLuint range) {
    static _Atomic unsigned long calls = 0ul;
    (void)range;
    mgl_unimplemented_call("glGenFragmentShadersATI", &calls);
    return (GLuint)0;
}

MGL_EXPORT void glGetUnsignedBytei_vEXT(GLenum target, GLuint index, GLubyte *data) {
    static _Atomic unsigned long calls = 0ul;
    (void)target;
    (void)index;
    (void)data;
    mgl_unimplemented_call("glGetUnsignedBytei_vEXT", &calls);
}

MGL_EXPORT void glGetUnsignedBytevEXT(GLenum pname, GLubyte *data) {
    static _Atomic unsigned long calls = 0ul;
    (void)pname;
    (void)data;
    mgl_unimplemented_call("glGetUnsignedBytevEXT", &calls);
}

MGL_EXPORT void glImportMemoryFdEXT(GLuint memory, GLuint64 size, GLenum handleType, GLint fd) {
    static _Atomic unsigned long calls = 0ul;
    (void)memory;
    (void)size;
    (void)handleType;
    (void)fd;
    mgl_unimplemented_call("glImportMemoryFdEXT", &calls);
}

MGL_EXPORT void glImportSemaphoreFdEXT(GLuint semaphore, GLenum handleType, GLint fd) {
    static _Atomic unsigned long calls = 0ul;
    (void)semaphore;
    (void)handleType;
    (void)fd;
    mgl_unimplemented_call("glImportSemaphoreFdEXT", &calls);
}

MGL_EXPORT GLboolean glIsFenceAPPLE(GLuint fence) {
    static _Atomic unsigned long calls = 0ul;
    (void)fence;
    mgl_unimplemented_call("glIsFenceAPPLE", &calls);
    return (GLboolean)0;
}

MGL_EXPORT void glPassTexCoordATI(GLuint dst, GLuint coord, GLenum swizzle) {
    static _Atomic unsigned long calls = 0ul;
    (void)dst;
    (void)coord;
    (void)swizzle;
    mgl_unimplemented_call("glPassTexCoordATI", &calls);
}

MGL_EXPORT void glPointParameteriNV(GLenum pname, GLint param) {
    static _Atomic unsigned long calls = 0ul;
    (void)pname;
    (void)param;
    mgl_unimplemented_call("glPointParameteriNV", &calls);
}

MGL_EXPORT void glPointParameterivNV(GLenum pname, const GLint *params) {
    static _Atomic unsigned long calls = 0ul;
    (void)pname;
    (void)params;
    mgl_unimplemented_call("glPointParameterivNV", &calls);
}

MGL_EXPORT void glSampleMapATI(GLuint dst, GLuint interp, GLenum swizzle) {
    static _Atomic unsigned long calls = 0ul;
    (void)dst;
    (void)interp;
    (void)swizzle;
    mgl_unimplemented_call("glSampleMapATI", &calls);
}

MGL_EXPORT void glSetFenceAPPLE(GLuint fence) {
    static _Atomic unsigned long calls = 0ul;
    (void)fence;
    mgl_unimplemented_call("glSetFenceAPPLE", &calls);
}

MGL_EXPORT void glSetFragmentShaderConstantATI(GLuint dst, const GLfloat *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)dst;
    (void)value;
    mgl_unimplemented_call("glSetFragmentShaderConstantATI", &calls);
}

MGL_EXPORT GLboolean glTestFenceAPPLE(GLuint fence) {
    static _Atomic unsigned long calls = 0ul;
    (void)fence;
    mgl_unimplemented_call("glTestFenceAPPLE", &calls);
    return (GLboolean)0;
}

MGL_EXPORT GLboolean glTestObjectAPPLE(GLenum object, GLuint name) {
    static _Atomic unsigned long calls = 0ul;
    (void)object;
    (void)name;
    mgl_unimplemented_call("glTestObjectAPPLE", &calls);
    return (GLboolean)0;
}

MGL_EXPORT void glVertexWeighthNV(GLhalfNV weight) {
    static _Atomic unsigned long calls = 0ul;
    (void)weight;
    mgl_unimplemented_call("glVertexWeighthNV", &calls);
}

MGL_EXPORT void glVertexWeighthvNV(const GLhalfNV *weight) {
    static _Atomic unsigned long calls = 0ul;
    (void)weight;
    mgl_unimplemented_call("glVertexWeighthvNV", &calls);
}

MGL_EXPORT void glGetActiveSubroutineName(GLuint program, GLenum shadertype, GLuint index,
                                          GLsizei bufSize, GLsizei *length, GLchar *name) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)shadertype;
    (void)index;
    (void)bufSize;
    (void)length;
    (void)name;
    mgl_unimplemented_call("glGetActiveSubroutineName", &calls);
}

MGL_EXPORT void glGetActiveSubroutineUniformName(GLuint program, GLenum shadertype, GLuint index,
                                                 GLsizei bufSize, GLsizei *length, GLchar *name) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)shadertype;
    (void)index;
    (void)bufSize;
    (void)length;
    (void)name;
    mgl_unimplemented_call("glGetActiveSubroutineUniformName", &calls);
}

MGL_EXPORT void glGetActiveSubroutineUniformiv(GLuint program, GLenum shadertype, GLuint index,
                                               GLenum pname, GLint *values) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)shadertype;
    (void)index;
    (void)pname;
    (void)values;
    mgl_unimplemented_call("glGetActiveSubroutineUniformiv", &calls);
}

MGL_EXPORT GLuint glGetSubroutineIndex(GLuint program, GLenum shadertype, const GLchar *name) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)shadertype;
    (void)name;
    mgl_unimplemented_call("glGetSubroutineIndex", &calls);
    return (GLuint)~0u;
}

MGL_EXPORT GLint glGetSubroutineUniformLocation(GLuint program, GLenum shadertype,
                                                const GLchar *name) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)shadertype;
    (void)name;
    mgl_unimplemented_call("glGetSubroutineUniformLocation", &calls);
    return -1;
}

MGL_EXPORT void glGetUniformSubroutineuiv(GLenum shadertype, GLint location, GLuint *params) {
    static _Atomic unsigned long calls = 0ul;
    (void)shadertype;
    (void)location;
    (void)params;
    mgl_unimplemented_call("glGetUniformSubroutineuiv", &calls);
}

MGL_EXPORT void glGetVertexAttribLdv(GLuint index, GLenum pname, GLdouble *params) {
    static _Atomic unsigned long calls = 0ul;
    (void)index;
    (void)pname;
    (void)params;
    mgl_unimplemented_call("glGetVertexAttribLdv", &calls);
}

MGL_EXPORT void glProgramUniform1d(GLuint program, GLint location, GLdouble v0) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)v0;
    mgl_unimplemented_call("glProgramUniform1d", &calls);
}

MGL_EXPORT void glProgramUniform1dv(GLuint program, GLint location, GLsizei count,
                                    const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)count;
    (void)value;
    mgl_unimplemented_call("glProgramUniform1dv", &calls);
}

MGL_EXPORT void glProgramUniform2d(GLuint program, GLint location, GLdouble v0, GLdouble v1) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)v0;
    (void)v1;
    mgl_unimplemented_call("glProgramUniform2d", &calls);
}

MGL_EXPORT void glProgramUniform2dv(GLuint program, GLint location, GLsizei count,
                                    const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)count;
    (void)value;
    mgl_unimplemented_call("glProgramUniform2dv", &calls);
}

MGL_EXPORT void glProgramUniform3d(GLuint program, GLint location, GLdouble v0, GLdouble v1,
                                   GLdouble v2) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)v0;
    (void)v1;
    (void)v2;
    mgl_unimplemented_call("glProgramUniform3d", &calls);
}

MGL_EXPORT void glProgramUniform3dv(GLuint program, GLint location, GLsizei count,
                                    const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)count;
    (void)value;
    mgl_unimplemented_call("glProgramUniform3dv", &calls);
}

MGL_EXPORT void glProgramUniform4d(GLuint program, GLint location, GLdouble v0, GLdouble v1,
                                   GLdouble v2, GLdouble v3) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)v0;
    (void)v1;
    (void)v2;
    (void)v3;
    mgl_unimplemented_call("glProgramUniform4d", &calls);
}

MGL_EXPORT void glProgramUniform4dv(GLuint program, GLint location, GLsizei count,
                                    const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)count;
    (void)value;
    mgl_unimplemented_call("glProgramUniform4dv", &calls);
}

MGL_EXPORT void glProgramUniformMatrix2dv(GLuint program, GLint location, GLsizei count,
                                          GLboolean transpose, const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glProgramUniformMatrix2dv", &calls);
}

MGL_EXPORT void glProgramUniformMatrix2x3dv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glProgramUniformMatrix2x3dv", &calls);
}

MGL_EXPORT void glProgramUniformMatrix2x4dv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glProgramUniformMatrix2x4dv", &calls);
}

MGL_EXPORT void glProgramUniformMatrix3dv(GLuint program, GLint location, GLsizei count,
                                          GLboolean transpose, const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glProgramUniformMatrix3dv", &calls);
}

MGL_EXPORT void glProgramUniformMatrix3x2dv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glProgramUniformMatrix3x2dv", &calls);
}

MGL_EXPORT void glProgramUniformMatrix3x4dv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glProgramUniformMatrix3x4dv", &calls);
}

MGL_EXPORT void glProgramUniformMatrix4dv(GLuint program, GLint location, GLsizei count,
                                          GLboolean transpose, const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glProgramUniformMatrix4dv", &calls);
}

MGL_EXPORT void glProgramUniformMatrix4x2dv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glProgramUniformMatrix4x2dv", &calls);
}

MGL_EXPORT void glProgramUniformMatrix4x3dv(GLuint program, GLint location, GLsizei count,
                                            GLboolean transpose, const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)program;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glProgramUniformMatrix4x3dv", &calls);
}

MGL_EXPORT void glUniform1d(GLint location, GLdouble x) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)x;
    mgl_unimplemented_call("glUniform1d", &calls);
}

MGL_EXPORT void glUniform1dv(GLint location, GLsizei count, const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)count;
    (void)value;
    mgl_unimplemented_call("glUniform1dv", &calls);
}

MGL_EXPORT void glUniform2d(GLint location, GLdouble x, GLdouble y) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)x;
    (void)y;
    mgl_unimplemented_call("glUniform2d", &calls);
}

MGL_EXPORT void glUniform2dv(GLint location, GLsizei count, const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)count;
    (void)value;
    mgl_unimplemented_call("glUniform2dv", &calls);
}

MGL_EXPORT void glUniform3d(GLint location, GLdouble x, GLdouble y, GLdouble z) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)x;
    (void)y;
    (void)z;
    mgl_unimplemented_call("glUniform3d", &calls);
}

MGL_EXPORT void glUniform3dv(GLint location, GLsizei count, const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)count;
    (void)value;
    mgl_unimplemented_call("glUniform3dv", &calls);
}

MGL_EXPORT void glUniform4d(GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)x;
    (void)y;
    (void)z;
    (void)w;
    mgl_unimplemented_call("glUniform4d", &calls);
}

MGL_EXPORT void glUniform4dv(GLint location, GLsizei count, const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)count;
    (void)value;
    mgl_unimplemented_call("glUniform4dv", &calls);
}

MGL_EXPORT void glUniformMatrix2dv(GLint location, GLsizei count, GLboolean transpose,
                                   const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glUniformMatrix2dv", &calls);
}

MGL_EXPORT void glUniformMatrix2x3dv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glUniformMatrix2x3dv", &calls);
}

MGL_EXPORT void glUniformMatrix2x4dv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glUniformMatrix2x4dv", &calls);
}

MGL_EXPORT void glUniformMatrix3dv(GLint location, GLsizei count, GLboolean transpose,
                                   const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glUniformMatrix3dv", &calls);
}

MGL_EXPORT void glUniformMatrix3x2dv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glUniformMatrix3x2dv", &calls);
}

MGL_EXPORT void glUniformMatrix3x4dv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glUniformMatrix3x4dv", &calls);
}

MGL_EXPORT void glUniformMatrix4dv(GLint location, GLsizei count, GLboolean transpose,
                                   const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glUniformMatrix4dv", &calls);
}

MGL_EXPORT void glUniformMatrix4x2dv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glUniformMatrix4x2dv", &calls);
}

MGL_EXPORT void glUniformMatrix4x3dv(GLint location, GLsizei count, GLboolean transpose,
                                     const GLdouble *value) {
    static _Atomic unsigned long calls = 0ul;
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    mgl_unimplemented_call("glUniformMatrix4x3dv", &calls);
}

MGL_EXPORT void glUniformSubroutinesuiv(GLenum shadertype, GLsizei count, const GLuint *indices) {
    static _Atomic unsigned long calls = 0ul;
    (void)shadertype;
    (void)count;
    (void)indices;
    mgl_unimplemented_call("glUniformSubroutinesuiv", &calls);
}

MGL_EXPORT void glVertexArrayAttribLFormat(GLuint vaobj, GLuint attribindex, GLint size,
                                           GLenum type, GLuint relativeoffset) {
    static _Atomic unsigned long calls = 0ul;
    (void)vaobj;
    (void)attribindex;
    (void)size;
    (void)type;
    (void)relativeoffset;
    mgl_unimplemented_call("glVertexArrayAttribLFormat", &calls);
}

MGL_EXPORT void glVertexAttribL1d(GLuint index, GLdouble x) {
    static _Atomic unsigned long calls = 0ul;
    (void)index;
    (void)x;
    mgl_unimplemented_call("glVertexAttribL1d", &calls);
}

MGL_EXPORT void glVertexAttribL1dv(GLuint index, const GLdouble *v) {
    static _Atomic unsigned long calls = 0ul;
    (void)index;
    (void)v;
    mgl_unimplemented_call("glVertexAttribL1dv", &calls);
}

MGL_EXPORT void glVertexAttribL2d(GLuint index, GLdouble x, GLdouble y) {
    static _Atomic unsigned long calls = 0ul;
    (void)index;
    (void)x;
    (void)y;
    mgl_unimplemented_call("glVertexAttribL2d", &calls);
}

MGL_EXPORT void glVertexAttribL2dv(GLuint index, const GLdouble *v) {
    static _Atomic unsigned long calls = 0ul;
    (void)index;
    (void)v;
    mgl_unimplemented_call("glVertexAttribL2dv", &calls);
}

MGL_EXPORT void glVertexAttribL3d(GLuint index, GLdouble x, GLdouble y, GLdouble z) {
    static _Atomic unsigned long calls = 0ul;
    (void)index;
    (void)x;
    (void)y;
    (void)z;
    mgl_unimplemented_call("glVertexAttribL3d", &calls);
}

MGL_EXPORT void glVertexAttribL3dv(GLuint index, const GLdouble *v) {
    static _Atomic unsigned long calls = 0ul;
    (void)index;
    (void)v;
    mgl_unimplemented_call("glVertexAttribL3dv", &calls);
}

MGL_EXPORT void glVertexAttribL4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    static _Atomic unsigned long calls = 0ul;
    (void)index;
    (void)x;
    (void)y;
    (void)z;
    (void)w;
    mgl_unimplemented_call("glVertexAttribL4d", &calls);
}

MGL_EXPORT void glVertexAttribL4dv(GLuint index, const GLdouble *v) {
    static _Atomic unsigned long calls = 0ul;
    (void)index;
    (void)v;
    mgl_unimplemented_call("glVertexAttribL4dv", &calls);
}

MGL_EXPORT void glVertexAttribLFormat(GLuint attribindex, GLint size, GLenum type,
                                      GLuint relativeoffset) {
    static _Atomic unsigned long calls = 0ul;
    (void)attribindex;
    (void)size;
    (void)type;
    (void)relativeoffset;
    mgl_unimplemented_call("glVertexAttribLFormat", &calls);
}

MGL_EXPORT void glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride,
                                       const void *pointer) {
    static _Atomic unsigned long calls = 0ul;
    (void)index;
    (void)size;
    (void)type;
    (void)stride;
    (void)pointer;
    mgl_unimplemented_call("glVertexAttribLPointer", &calls);
}

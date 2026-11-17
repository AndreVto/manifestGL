#ifndef MGL_SPIRV_GLSLANG_COMPILE_H
#define MGL_SPIRV_GLSLANG_COMPILE_H

#include <stddef.h>
#include "mgl/types.h"
#include "../shader/translator.h"

#ifdef __cplusplus
extern "C" {
#endif

int mgl_glslang_compile_spirv(GLenum shader_type, const char *source, unsigned version,
                              unsigned int **words, size_t *count, char **compiler_log);

struct mgl_glslang_fragment_output {
    char *name;
    GLuint location;
    GLuint index;
};

int mgl_glslang_fragment_output_spirv(const char *source, unsigned version,
                                       size_t binding_count,
                                       const struct mgl_fragment_output_location *bindings,
                                       unsigned int **words, size_t *count,
                                       int *rewrite, struct mgl_glslang_fragment_output **outputs,
                                       size_t *output_count, char **compiler_log);

#ifdef __cplusplus
}
#endif

#endif

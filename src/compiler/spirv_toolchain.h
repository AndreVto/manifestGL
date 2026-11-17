#ifndef MGL_SPIRV_TOOLCHAIN_H
#define MGL_SPIRV_TOOLCHAIN_H

#include "mgl/types.h"
#include <stddef.h>
#include <stdint.h>

/* Isolate compiler types; returned strings are caller-owned. */
/* Preprocess using the original desktop GLSL version before GLES lowering; errors must not select
 * different macro branches. */
int mgl_preprocess_desktop_glsl(GLenum shader_type, const char *source, unsigned desktop_version,
                                char **expanded_source, char **compiler_log);

int mgl_validate_gles_glsl(GLenum shader_type, const char *source);

int mgl_spirv_specialize_glsl(GLenum shader_type, const uint32_t *words, size_t word_count,
                              const char *entry_point, GLuint num_constants,
                              const GLuint *constant_ids, const GLuint *constant_values,
                              char **gles_source, char **compiler_log, GLenum *error);

int mgl_spirv_validate_module(const uint32_t *words, size_t word_count);

int mgl_spirv_translate_glsl(GLenum shader_type, const char *desktop_source, unsigned gles_version,
                             char **gles_source, char **compiler_log);

#endif

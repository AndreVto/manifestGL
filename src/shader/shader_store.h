#ifndef MGL_SHADER_STORE_H
#define MGL_SHADER_STORE_H

#include "core/context.h"

struct mgl_shader *mgl_shader_find(struct mgl_context *ctx, GLuint name);
struct mgl_program *mgl_program_find(struct mgl_context *ctx, GLuint name);
GLint mgl_frag_data_index_locked(struct mgl_context *ctx, struct mgl_program *program,
                                const GLchar *name);
struct mgl_shader *mgl_shader_create(struct mgl_context *ctx, GLenum type);
struct mgl_program *mgl_program_create(struct mgl_context *ctx);
void mgl_program_init_metadata(struct mgl_program *program);
GLuint mgl_compile_full_fixed_function_stage(struct mgl_context *ctx, GLenum type);
GLuint mgl_compile_implicit_compat_stage(struct mgl_context *ctx, GLenum type,
                                         const struct mgl_program *program, const GLenum *targets);
char *mgl_native_shader_log(struct mgl_context *ctx, GLuint native_shader);
struct mgl_spirv_module *mgl_spirv_module_create(const void *binary, size_t length);
void mgl_spirv_module_release(struct mgl_spirv_module *module);
void mgl_shader_set_spirv_module(struct mgl_shader *shader, struct mgl_spirv_module *module);
void mgl_shader_maybe_destroy(struct mgl_context *ctx, struct mgl_shader *shader);
void mgl_program_maybe_destroy(struct mgl_context *ctx, struct mgl_program *program);

#endif

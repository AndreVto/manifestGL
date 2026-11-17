#ifndef MGL_ARB_PROGRAM_H
#define MGL_ARB_PROGRAM_H

#include "core/context.h"

struct mgl_arb_translation {
    char *source;
    GLuint generic_mask;
    unsigned max_env_index;
    unsigned max_local_index;
    GLuint instruction_count;
    GLuint temporary_count;
    GLuint parameter_count;
    GLuint attrib_count;
    GLuint address_count;
    GLuint texture_count;
    GLuint texture_indirections;
    GLboolean uses_env;
    GLboolean uses_local;
    GLboolean uses_texture0;
    GLenum texture_targets[MGL_TEXTURE_UNITS];
};

struct mgl_arb_program *mgl_arb_program_find(struct mgl_context *ctx, GLuint name);
void mgl_arb_program_free_all(struct mgl_share_group *group);
void mgl_arb_context_cleanup(struct mgl_context *ctx);
int mgl_arb_translate(GLenum target, const char *source, struct mgl_arb_translation *out,
                      GLint *error_position, char *error_text, size_t error_text_size);
void mgl_arb_translation_destroy(struct mgl_arb_translation *translation);

struct mgl_program *mgl_arb_current_linked_program(struct mgl_context *ctx);
void mgl_arb_realize_uniforms(struct mgl_context *ctx, const struct mgl_program *program);
void mgl_arb_destroy_native_program(struct mgl_context *ctx);
void mgl_arb_invalidate_native_program(struct mgl_context *ctx);

#endif

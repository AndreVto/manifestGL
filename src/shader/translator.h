#ifndef MGL_SHADER_TRANSLATOR_H
#define MGL_SHADER_TRANSLATOR_H

#include "mgl/types.h"
#include <stddef.h>

#define MGL_COMPAT_SEM_POSITION 0u
#define MGL_COMPAT_SEM_NORMAL 1u
#define MGL_COMPAT_SEM_COLOR 2u
#define MGL_COMPAT_SEM_SECONDARY 3u
#define MGL_COMPAT_SEM_FOG 4u
#define MGL_COMPAT_SEM_TEXCOORD0 5u
#define MGL_COMPAT_SEM_COUNT 13u

extern const char *const mgl_compat_attribute_names[MGL_COMPAT_SEM_COUNT];

struct mgl_fragment_output_location {
    const char *name;
    GLuint location;
    GLuint index;
};

char *mgl_translate_glsl(GLenum shader_type, const char *source, char **translation_log);
char *mgl_fragment_output_locations(const char *source, size_t binding_count,
                                    const struct mgl_fragment_output_location *bindings);
char *mgl_fragment_output_locations_log(const char *source, size_t binding_count,
                                        const struct mgl_fragment_output_location *bindings,
                                        char **failure_log);

#endif

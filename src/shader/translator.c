#include "shader/translator.h"

#include "compiler/spirv_toolchain.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const mgl_compat_attribute_names[MGL_COMPAT_SEM_COUNT] = {
    "mgl_a_position",  "mgl_a_normal",    "mgl_a_color",     "mgl_a_secondary", "mgl_a_fog",
    "mgl_a_texcoord0", "mgl_a_texcoord1", "mgl_a_texcoord2", "mgl_a_texcoord3", "mgl_a_texcoord4",
    "mgl_a_texcoord5", "mgl_a_texcoord6", "mgl_a_texcoord7"};

struct text_builder {
    char *data;
    size_t len;
    size_t cap;
};

static int reserve(struct text_builder *b, size_t extra) {
    size_t needed;
    char *next;
    if (extra > SIZE_MAX - b->len - 1u)
        return 0;
    needed = b->len + extra + 1u;
    if (needed <= b->cap)
        return 1;
    if (b->cap == 0u)
        b->cap = 256u;
    while (b->cap < needed) {
        if (b->cap > SIZE_MAX / 2u) {
            b->cap = needed;
            break;
        }
        b->cap *= 2u;
    }
    next = realloc(b->data, b->cap);
    if (next == NULL)
        return 0;
    b->data = next;
    return 1;
}

static int append_n(struct text_builder *b, const char *s, size_t n) {
    if (!reserve(b, n))
        return 0;
    if (n != 0u)
        memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 1;
}

static int append(struct text_builder *b, const char *s) {
    return append_n(b, s, strlen(s));
}

static char *take(struct text_builder *b) {
    char *out;
    if (b->data == NULL) {
        out = malloc(1u);
        if (out != NULL)
            out[0] = '\0';
        return out;
    }
    out = b->data;
    b->data = NULL;
    b->len = b->cap = 0u;
    return out;
}

static int is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

struct ident_map {
    const char *from;
    const char *to;
};

static const char *mapped_identifier(const char *start, size_t len) {
    static const struct ident_map map[] = {
        {"gl_Vertex", "mgl_a_position"},
        {"gl_Normal", "mgl_a_normal"},
        {"gl_Color", "mgl_a_color"},
        {"gl_SecondaryColor", "mgl_a_secondary"},
        {"gl_MultiTexCoord0", "mgl_a_texcoord0"},
        {"gl_MultiTexCoord1", "mgl_a_texcoord1"},
        {"gl_MultiTexCoord2", "mgl_a_texcoord2"},
        {"gl_MultiTexCoord3", "mgl_a_texcoord3"},
        {"gl_MultiTexCoord4", "mgl_a_texcoord4"},
        {"gl_MultiTexCoord5", "mgl_a_texcoord5"},
        {"gl_MultiTexCoord6", "mgl_a_texcoord6"},
        {"gl_MultiTexCoord7", "mgl_a_texcoord7"},
        {"gl_ModelViewProjectionMatrix", "mgl_u_model_view_projection"},
        {"gl_ModelViewMatrix", "mgl_u_model_view"},
        {"gl_ProjectionMatrix", "mgl_u_projection"},
        {"gl_NormalMatrix", "mgl_u_normal_matrix"},
        {"gl_TextureMatrix", "mgl_u_texture_matrix"},
        {"gl_TexCoord", "mgl_v_texcoord"},
        {"gl_FrontColor", "mgl_v_front_color"},
        {"gl_BackColor", "mgl_v_back_color"},
        {"gl_FrontSecondaryColor", "mgl_v_front_secondary"},
        {"gl_BackSecondaryColor", "mgl_v_back_secondary"},
        {"gl_FogFragCoord", "mgl_v_fog_coord"}};
    size_t i;
    for (i = 0; i < sizeof(map) / sizeof(map[0]); ++i) {
        if (strlen(map[i].from) == len && memcmp(start, map[i].from, len) == 0)
            return map[i].to;
    }
    return NULL;
}

static int contains_identifier(const char *source, const char *needle) {
    const size_t n = strlen(needle);
    const char *p = source;
    while (*p != '\0') {
        if (p[0] == '/' && p[1] == '/') {
            while (*p != '\0' && *p != '\n')
                ++p;
        } else if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p != '\0' && !(p[0] == '*' && p[1] == '/'))
                ++p;
            if (*p != '\0')
                p += 2;
        } else if (*p == '"' || *p == '\'') {
            const char delim = *p++;
            while (*p != '\0' && *p != delim) {
                if (*p == '\\' && p[1] != '\0')
                    ++p;
                ++p;
            }
            if (*p != '\0')
                ++p;
        } else if (is_ident_start(*p)) {
            const char *start = p;
            while (is_ident_char(*p))
                ++p;
            if ((size_t)(p - start) == n && memcmp(start, needle, n) == 0)
                return 1;
        } else
            ++p;
    }
    return 0;
}

static char *replace_ftransform(const char *source) {
    const char target[] = "ftransform";
    const char replacement[] = "(mgl_u_model_view_projection * mgl_a_position)";
    struct text_builder out = {0};
    const char *p = source;
    while (*p != '\0') {
        if (strncmp(p, target, sizeof(target) - 1u) == 0 &&
            (p == source || !is_ident_char(p[-1]))) {
            const char *q = p + sizeof(target) - 1u;
            while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')
                ++q;
            if (*q == '(') {
                ++q;
                while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')
                    ++q;
                if (*q == ')') {
                    if (!append(&out, replacement))
                        goto fail;
                    p = q + 1;
                    continue;
                }
            }
        }
        if (!append_n(&out, p, 1u))
            goto fail;
        ++p;
    }
    return take(&out);
fail:
    free(out.data);
    return NULL;
}

static char *replace_identifiers(const char *source, int es300, GLenum shader_type) {
    struct text_builder out = {0};
    const char *p = source;
    while (*p != '\0') {
        if (is_ident_start(*p)) {
            const char *start = p;
            const char *mapped;
            size_t len;
            while (is_ident_char(*p))
                ++p;
            len = (size_t)(p - start);
            mapped = mapped_identifier(start, len);
            /* Desktop fragment gl_Color selects the facing primary color;
             * gl_FrontColor and gl_BackColor remain explicit inputs. */
            if (shader_type == GL_FRAGMENT_SHADER && len == 8u &&
                memcmp(start, "gl_Color", len) == 0)
                mapped = "(gl_FrontFacing ? mgl_v_front_color : mgl_v_back_color)";
            if (mapped == NULL && es300) {
                if (len == 9u && memcmp(start, "sampler1D", len) == 0)
                    mapped = "sampler2D";
                else if (len == 13u && memcmp(start, "sampler2DRect", len) == 0)
                    mapped = "sampler2D";
                else if (len == 9u && memcmp(start, "texture1D", len) == 0)
                    mapped = "mgl_legacy_texture1D";
                else if (len == 12u && memcmp(start, "texture1DLod", len) == 0)
                    mapped = "mgl_legacy_texture1DLod";
                else if (len == 13u && memcmp(start, "texture2DRect", len) == 0)
                    mapped = "mgl_legacy_texture2DRect";
                else if (len == 9u && memcmp(start, "texture2D", len) == 0)
                    mapped = "mgl_legacy_texture2D";
                else if (len == 14u && memcmp(start, "texture2DArray", len) == 0)
                    mapped = "mgl_legacy_texture2DArray";
                else if (len == 9u && memcmp(start, "texture3D", len) == 0)
                    mapped = "mgl_legacy_texture3D";
                else if (len == 11u && memcmp(start, "textureCube", len) == 0)
                    mapped = "mgl_legacy_textureCube";
                else if (len == 13u && memcmp(start, "texture2DProj", len) == 0)
                    mapped = "mgl_legacy_texture2DProj";
                else if (len == 12u && memcmp(start, "shadow2DProj", len) == 0)
                    mapped = "mgl_legacy_shadow2DProj";
                else if (len == 8u && memcmp(start, "shadow2D", len) == 0)
                    mapped = "mgl_legacy_shadow2D";
                else if (len == 13u && memcmp(start, "texture3DProj", len) == 0)
                    mapped = "mgl_legacy_texture3DProj";
                else if (len == 12u && memcmp(start, "texture2DLod", len) == 0)
                    mapped = "mgl_legacy_texture2DLod";
                else if (len == 9u && memcmp(start, "attribute", len) == 0 &&
                         shader_type == GL_VERTEX_SHADER)
                    mapped = "in";
                else if (len == 7u && memcmp(start, "varying", len) == 0)
                    mapped = shader_type == GL_VERTEX_SHADER ? "out" : "in";
                else if (len == 12u && memcmp(start, "gl_FragColor", len) == 0 &&
                         shader_type == GL_FRAGMENT_SHADER)
                    mapped = "mgl_frag_color";
                else if (len == 11u && memcmp(start, "gl_FragData", len) == 0 &&
                         shader_type == GL_FRAGMENT_SHADER)
                    mapped = "mgl_frag_data";
            }
            if (mapped != NULL) {
                if (!append(&out, mapped))
                    goto fail;
            } else if (!append_n(&out, start, len))
                goto fail;
        } else {
            if (!append_n(&out, p, 1u))
                goto fail;
            ++p;
        }
    }
    return take(&out);
fail:
    free(out.data);
    return NULL;
}

/* Insert generated declarations after the leading preprocessor prologue, never ahead of #extension
 * directives. */
static const char *glsl_prologue_end(const char *body) {
    const char *line = body;
    int block_comment = 0;
    while (*line != '\0') {
        const char *p = line;
        const char *line_end = strchr(line, '\n');
        const char *end = line_end != NULL ? line_end : line + strlen(line);
        int directive = 0;
        while (p < end) {
            if (block_comment) {
                if (p + 1 < end && p[0] == '*' && p[1] == '/') {
                    block_comment = 0;
                    p += 2;
                } else
                    ++p;
            } else if (*p == ' ' || *p == '\t' || *p == '\r')
                ++p;
            else if (p + 1 < end && p[0] == '/' && p[1] == '*') {
                block_comment = 1;
                p += 2;
            } else if (p + 1 < end && p[0] == '/' && p[1] == '/') {
                p = end;
            } else if (*p == '#') {
                directive = 1;
                p = end;
            } else {
                return line;
            }
        }
        if (line_end == NULL)
            return end;
        line = line_end + 1;

        if (directive && end > body && end[-1] == '\\') {
            while (*line != '\0') {
                const char *next = strchr(line, '\n');
                const char *continued_end = next != NULL ? next : line + strlen(line);
                if (next == NULL)
                    return continued_end;
                line = next + 1;
                if (continued_end == line - 1 || continued_end[-1] != '\\')
                    break;
            }
        }
    }
    return line;
}

/* ESSL 3.00 provides texture arrays natively. Remove only the preprocessed legacy extension
 * directive, retaining line positions. */
static int append_es300_prologue(struct text_builder *out, const char *start, size_t length) {
    const char *p = start;
    const char *end = start + length;
    while (p < end) {
        const char *line_end = memchr(p, '\n', (size_t)(end - p));
        const char *stop = line_end != NULL ? line_end : end;
        const char *q = p;
        const char *name;
        const char *name_end;
        while (q < stop && (*q == ' ' || *q == '\t'))
            ++q;
        if (q < stop && *q == '#') {
            ++q;
            while (q < stop && (*q == ' ' || *q == '\t'))
                ++q;
            if (stop - q >= 9 && memcmp(q, "extension", 9u) == 0 &&
                (q + 9 == stop || isspace((unsigned char)q[9]))) {
                q += 9;
                while (q < stop && (*q == ' ' || *q == '\t'))
                    ++q;
                name = q;
                while (q < stop && (is_ident_char(*q)))
                    ++q;
                name_end = q;
                if (((size_t)(name_end - name) == sizeof("GL_EXT_texture_array") - 1u &&
                     memcmp(name, "GL_EXT_texture_array", sizeof("GL_EXT_texture_array") - 1u) ==
                         0) ||
                    ((size_t)(name_end - name) == sizeof("GL_ARB_texture_rectangle") - 1u &&
                     memcmp(name, "GL_ARB_texture_rectangle",
                            sizeof("GL_ARB_texture_rectangle") - 1u) == 0)) {
                    while (q < stop && (*q == ' ' || *q == '\t'))
                        ++q;
                    if (q < stop && *q++ == ':') {

                        while (q < stop && (*q == ' ' || *q == '\t'))
                            ++q;
                        {
                            const char *behavior = q;
                            while (q < stop && isalpha((unsigned char)*q))
                                ++q;
                            const size_t behavior_len = (size_t)(q - behavior);
                            while (q < stop && isspace((unsigned char)*q))
                                ++q;
                            if (q == stop &&
                                ((behavior_len == 6u && memcmp(behavior, "enable", 6u) == 0) ||
                                 (behavior_len == 7u && memcmp(behavior, "require", 7u) == 0) ||
                                 (behavior_len == 4u && memcmp(behavior, "warn", 4u) == 0))) {
                                if (line_end != NULL && !append_n(out, "\n", 1u))
                                    return 0;
                                p = line_end != NULL ? line_end + 1 : end;
                                continue;
                            }
                        }
                    }
                }
            }
        }
        if (!append_n(out, p, (size_t)(stop - p)))
            return 0;
        if (line_end != NULL && !append_n(out, "\n", 1u))
            return 0;
        p = line_end != NULL ? line_end + 1 : end;
    }
    return 1;
}

static char *lower_point_coord(const char *source) {
    struct text_builder out = {0};
    const char *body = source;
    const char *p;
    const char *start;
    const char *version_end = strchr(source, '\n');
    if (version_end != NULL && strncmp(source, "#version", 8u) == 0)
        body = glsl_prologue_end(version_end + 1);
    if (!append_n(&out, source, (size_t)(body - source)) ||
        !append(&out, "uniform int mgl_u_point_coord_flip;\n"
                      "vec2 mgl_point_coord(){ vec2 c=gl_PointCoord; return vec2(c.x, "
                      "mgl_u_point_coord_flip!=0 ? 1.0-c.y : c.y); }\n"))
        goto fail;
    p = body;
    while (*p != '\0') {
        start = p;
        if (is_ident_start(*p)) {
            while (is_ident_char(*p))
                ++p;
            if ((size_t)(p - start) == sizeof("gl_PointCoord") - 1u &&
                memcmp(start, "gl_PointCoord", sizeof("gl_PointCoord") - 1u) == 0) {
                if (!append(&out, "mgl_point_coord()"))
                    goto fail;
            } else if (!append_n(&out, start, (size_t)(p - start)))
                goto fail;
        } else {
            if (!append_n(&out, p, 1u))
                goto fail;
            ++p;
        }
    }
    return take(&out);
fail:
    free(out.data);
    return NULL;
}

static int parse_version_and_body(const char *source, int *version, const char **body) {
    const char *p = source;
    for (;;) {
        while (isspace((unsigned char)*p))
            ++p;
        if (p[0] == '/' && p[1] == '/') {
            p += 2;
            while (*p != '\0' && *p != '\n')
                ++p;
        } else if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p != '\0' && !(p[0] == '*' && p[1] == '/'))
                ++p;
            if (*p == '\0')
                return 0;
            p += 2;
        } else {
            break;
        }
    }
    *version = 110;
    *body = source;
    if (strncmp(p, "#version", 8u) == 0 && isspace((unsigned char)p[8])) {
        char *end = NULL;
        long v;
        p += 8;
        while (*p == ' ' || *p == '\t')
            ++p;
        v = strtol(p, &end, 10);
        if (end == p || v <= 0 || v > 9999)
            return 0;
        *version = (int)v;
        while (*end != '\0' && *end != '\n')
            ++end;
        if (*end == '\n')
            ++end;
        *body = end;
    }
    return 1;
}

/* Avoid generating a second fragment output at location 0; count only global declarations, not
 * locals or macro text. */
struct fragment_output {
    char first[128];
    char location_zero[128];
    unsigned count;
    char by_location[4][128];
    char first_unlocated[128];
};

static int keyword_is(const char *p, size_t n, const char *keyword) {
    return strlen(keyword) == n && memcmp(p, keyword, n) == 0;
}

static const char *skip_glsl_space(const char *p) {
    while (*p != '\0' && isspace((unsigned char)*p))
        ++p;
    return p;
}

static int qualifier_is(const char *p, size_t n) {
    return keyword_is(p, n, "highp") || keyword_is(p, n, "mediump") || keyword_is(p, n, "lowp") ||
           keyword_is(p, n, "flat") || keyword_is(p, n, "smooth") || keyword_is(p, n, "centroid") ||
           keyword_is(p, n, "noperspective") || keyword_is(p, n, "sample") ||
           keyword_is(p, n, "invariant");
}

static int preceding_output_location(const char *begin, const char *at) {
    const char *p = at;
    while (p > begin && p[-1] != ';' && p[-1] != '}' && p[-1] != '{')
        --p;
    while (p < at) {
        if ((p == begin || !is_ident_char(p[-1])) && (size_t)(at - p) >= 8u &&
            strncmp(p, "location", 8u) == 0 && (p + 8 == at || !is_ident_char(p[8]))) {
            const char *q = skip_glsl_space(p + 8);
            if (*q == '=') {
                char *after;
                unsigned long loc;
                q = skip_glsl_space(q + 1);
                if (!isdigit((unsigned char)*q)) {
                    ++p;
                    continue;
                }
                loc = strtoul(q, &after, 10);
                if (after > q && loc < 4u && after <= at && (after == at || !is_ident_char(*after)))
                    return (int)loc;
            }
        }
        ++p;
    }
    return -1;
}

static void find_fragment_outputs(const char *source, struct fragment_output *info) {
    const char *p = source;
    int braces = 0, parens = 0, line_start = 1;
    memset(info, 0, sizeof(*info));
    while (*p != '\0') {
        if (*p == '\n') {
            line_start = 1;
            ++p;
            continue;
        }
        if (line_start && (*p == ' ' || *p == '\t' || *p == '\r')) {
            ++p;
            continue;
        }
        if (line_start && *p == '#') {
            do {
                while (*p != '\0' && *p != '\n')
                    ++p;
            } while (*p == '\n' && p > source && p[-1] == '\\' && ++p);
            continue;
        }
        line_start = 0;
        if (p[0] == '/' && p[1] == '/') {
            while (*p != '\0' && *p != '\n')
                ++p;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p != '\0' && !(p[0] == '*' && p[1] == '/'))
                ++p;
            if (*p != '\0')
                p += 2;
            continue;
        }
        if (*p == '{') {
            ++braces;
            ++p;
            continue;
        }
        if (*p == '}') {
            if (braces > 0)
                --braces;
            ++p;
            continue;
        }
        if (*p == '(') {
            ++parens;
            ++p;
            continue;
        }
        if (*p == ')') {
            if (parens > 0)
                --parens;
            ++p;
            continue;
        }
        if (braces == 0 && parens == 0 && is_ident_start(*p)) {
            const char *token = p;
            const char *q;
            size_t n;
            while (is_ident_char(*p))
                ++p;
            n = (size_t)(p - token);
            if (!keyword_is(token, n, "out"))
                continue;
            q = skip_glsl_space(p);
            while (is_ident_start(*q)) {
                const char *word = q;
                size_t len;
                while (is_ident_char(*q))
                    ++q;
                len = (size_t)(q - word);
                if (!qualifier_is(word, len)) {
                    if (!keyword_is(word, len, "vec4"))
                        break;
                    q = skip_glsl_space(q);
                    if (is_ident_start(*q)) {
                        const char *name = q;
                        size_t name_len;
                        while (is_ident_char(*q))
                            ++q;
                        name_len = (size_t)(q - name);
                        /* Fragment-output arrays consume consecutive color
                         * locations.  Reserve their actual elements instead
                         * of allocating overlapping synthetic MRT outputs. */
                        {
                            const char *after_name = skip_glsl_space(q);
                            unsigned count = 1u, element;
                            int array = *after_name == '[';
                            if (array) {
                                char *after_count;
                                unsigned long array_size =
                                    strtoul(skip_glsl_space(after_name + 1), &after_count, 10);
                                if (array_size >= 1u && array_size <= 4u &&
                                    *skip_glsl_space(after_count) == ']')
                                    count = (unsigned)array_size;
                            }
                            if (name_len + 4u < sizeof(info->first)) {
                                char chosen[sizeof(info->first)];
                                int loc = preceding_output_location(source, token);
                                if (array)
                                    (void)snprintf(chosen, sizeof(chosen), "%.*s[0]", (int)name_len,
                                                   name);
                                else {
                                    memcpy(chosen, name, name_len);
                                    chosen[name_len] = '\0';
                                }
                                if (info->count == 0u)
                                    strcpy(info->first, chosen);
                                ++info->count;
                                if (loc == 0)
                                    strcpy(info->location_zero, chosen);
                                if (loc < 0 && !info->first_unlocated[0])
                                    strcpy(info->first_unlocated, chosen);
                                if (loc >= 0) {
                                    for (element = 0u;
                                         element < count && (unsigned)loc + element < 4u;
                                         ++element) {
                                        if (array)
                                            (void)snprintf(info->by_location[loc + (int)element],
                                                           sizeof(info->by_location[0]), "%.*s[%u]",
                                                           (int)name_len, name, element);
                                        else
                                            strcpy(info->by_location[loc], chosen);
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
                q = skip_glsl_space(q);
            }
        } else
            ++p;
    }
}

static unsigned fragdata_literal_mask(const char *source) {
    unsigned mask = 0u;
    const char *p = source;
    static const char name[] = "gl_FragData";
    while ((p = strstr(p, name)) != NULL) {
        const char *q = p + sizeof(name) - 1u;
        if ((p == source || !is_ident_char(p[-1])) && !is_ident_char(*q)) {
            q = skip_glsl_space(q);
            if (*q == '[') {
                q = skip_glsl_space(q + 1);
                if (q[0] >= '0' && q[0] <= '3') {
                    unsigned index = (unsigned)(q[0] - '0');
                    q = skip_glsl_space(q + 1);
                    if (*q == ']')
                        mask |= 1u << index;
                }
            }
        }
        p += sizeof(name) - 1u;
    }
    return mask;
}

/* Apply fragment-output remapping after identifier lowering, including
 * replacements inside macro definitions. */
static char *remap_mixed_fragdata(const char *source, char names[4][128]) {
    static const char name[] = "mgl_frag_data";
    const size_t name_len = sizeof(name) - 1u;
    const char *p = source;
    struct text_builder out = {0};
    while (*p != '\0') {
        if (strncmp(p, name, name_len) == 0 && (p == source || !is_ident_char(p[-1])) &&
            !is_ident_char(p[name_len])) {
            const char *q = skip_glsl_space(p + name_len);
            if (*q == '[') {
                q = skip_glsl_space(q + 1);
                if (q[0] >= '0' && q[0] <= '3') {
                    unsigned index = (unsigned)(q[0] - '0');
                    const char *end = skip_glsl_space(q + 1);
                    if (*end == ']' && names[index][0] != '\0') {
                        if (!append(&out, names[index]))
                            goto fail;
                        p = end + 1;
                        continue;
                    }
                }
            }
        }
        if (!append_n(&out, p, 1u))
            goto fail;
        ++p;
    }
    return take(&out);
fail:
    free(out.data);
    return NULL;
}

/* Legacy texture calls use wrappers to preserve name resolution when user symbols shadow GLES
 * built-ins. */
static int append_legacy_texture_functions(struct text_builder *out, const char *source,
                                           GLenum shader_type) {
    struct legacy_function {
        const char *name;
        const char *definition;
    };
    static const struct legacy_function functions[] = {
        {"texture1D", "vec4 mgl_legacy_texture1D(sampler2D samp, float uv) { return texture(samp, "
                      "vec2(uv, 0.5)); }\n"},
        {"texture2DRect", "vec4 mgl_legacy_texture2DRect(sampler2D samp, vec2 uv) { return "
                          "texture(samp, uv / vec2(textureSize(samp, 0))); }\n"},
        {"texture2D",
         "vec4 mgl_legacy_texture2D(sampler2D samp, vec2 uv) { return texture(samp, uv); }\n"},
        {"texture3D",
         "vec4 mgl_legacy_texture3D(sampler3D samp, vec3 uv) { return texture(samp, uv); }\n"},
        {"textureCube",
         "vec4 mgl_legacy_textureCube(samplerCube samp, vec3 uv) { return texture(samp, uv); }\n"},
        {"texture2DArray", "vec4 mgl_legacy_texture2DArray(sampler2DArray samp, vec3 uv) { return "
                           "texture(samp, uv); }\n"},
        {"texture2DProj", ("vec4 mgl_legacy_texture2DProj(sampler2D samp, vec3 uv) { return "
                           "textureProj(samp, uv); }\n"
                           "vec4 mgl_legacy_texture2DProj(sampler2D samp, vec4 uv) { return "
                           "textureProj(samp, uv); }\n")},
        {"texture3DProj", "vec4 mgl_legacy_texture3DProj(sampler3D samp, vec4 uv) { return "
                          "textureProj(samp, uv); }\n"},
        /* Legacy shadow wrappers expand the GLES scalar comparison result into desktop GLSL vec4
           form. */
        {"shadow2D", "vec4 mgl_legacy_shadow2D(sampler2DShadow samp, vec3 uv) { return "
                     "vec4(vec3(texture(samp, uv)), 1.0); }\n"},
        {"shadow2DProj", "vec4 mgl_legacy_shadow2DProj(sampler2DShadow samp, vec4 uv) { return "
                         "vec4(vec3(textureProj(samp, uv)), 1.0); }\n"}};
    static const char *const bias_functions[] = {
        "vec4 mgl_legacy_texture1D(sampler2D samp, float uv, float bias) { return texture(samp, "
        "vec2(uv, 0.5), bias); }\n",
        "vec4 mgl_legacy_texture2DRect(sampler2D samp, vec2 uv, float bias) { return texture(samp, "
        "uv / vec2(textureSize(samp, 0)), bias); }\n",
        "vec4 mgl_legacy_texture2D(sampler2D samp, vec2 uv, float bias) { return texture(samp, uv, "
        "bias); }\n",
        "vec4 mgl_legacy_texture3D(sampler3D samp, vec3 uv, float bias) { return texture(samp, uv, "
        "bias); }\n",
        "vec4 mgl_legacy_textureCube(samplerCube samp, vec3 uv, float bias) { return texture(samp, "
        "uv, bias); }\n",
        "vec4 mgl_legacy_texture2DArray(sampler2DArray samp, vec3 uv, float bias) { return "
        "texture(samp, uv, bias); }\n",
        ("vec4 mgl_legacy_texture2DProj(sampler2D samp, vec3 uv, float bias) { return "
         "textureProj(samp, uv, bias); }\n"
         "vec4 mgl_legacy_texture2DProj(sampler2D samp, vec4 uv, float bias) { return "
         "textureProj(samp, uv, bias); }\n"),
        "vec4 mgl_legacy_texture3DProj(sampler3D samp, vec4 uv, float bias) { return "
        "textureProj(samp, uv, bias); }\n",
        "vec4 mgl_legacy_shadow2D(sampler2DShadow samp, vec3 uv, float bias) { return "
        "vec4(vec3(texture(samp, uv, bias)), 1.0); }\n",
        "vec4 mgl_legacy_shadow2DProj(sampler2DShadow samp, vec4 uv, float bias) { return "
        "vec4(vec3(textureProj(samp, uv, bias)), 1.0); }\n"};
    size_t i;
    for (i = 0u; i < sizeof(functions) / sizeof(functions[0]); ++i) {
        if (!contains_identifier(source, functions[i].name))
            continue;
        if (!append(out, functions[i].definition))
            return 0;
        /* Fragment-stage legacy texture calls retain their optional bias operand. */
        if (shader_type == GL_FRAGMENT_SHADER && !append(out, bias_functions[i]))
            return 0;
    }
    /* Vertex-stage legacy LOD lookups map to GLSL ES textureLod through a distinct compatibility
     * wrapper. */
    if (contains_identifier(source, "texture1DLod") &&
        !append(out, "vec4 mgl_legacy_texture1DLod(sampler2D samp, float uv, float lod) { return "
                     "textureLod(samp, vec2(uv, 0.5), lod); }\n"))
        return 0;
    if (contains_identifier(source, "texture2DLod") &&
        !append(out, "vec4 mgl_legacy_texture2DLod(sampler2D samp, vec2 uv, float lod) { return "
                     "textureLod(samp, uv, lod); }\n"))
        return 0;
    return 1;
}

static int append_compat_declarations(struct text_builder *out, GLenum type, const char *source) {
    unsigned i;
    const char *qualifier = type == GL_VERTEX_SHADER ? "out" : "in";
    if (type == GL_VERTEX_SHADER) {
        if (contains_identifier(source, "gl_Vertex") || strstr(source, "ftransform") != NULL)
            if (!append(out, "in vec4 mgl_a_position;\n"))
                return 0;
        if (contains_identifier(source, "gl_Normal"))
            if (!append(out, "in vec3 mgl_a_normal;\n"))
                return 0;
        if (contains_identifier(source, "gl_Color"))
            if (!append(out, "in vec4 mgl_a_color;\n"))
                return 0;
        if (contains_identifier(source, "gl_SecondaryColor"))
            if (!append(out, "in vec4 mgl_a_secondary;\n"))
                return 0;
        for (i = 0; i < 8u; ++i) {
            char old_name[32];
            (void)snprintf(old_name, sizeof(old_name), "gl_MultiTexCoord%u", i);
            if (contains_identifier(source, old_name)) {
                char decl[64];
                (void)snprintf(decl, sizeof(decl), "in vec4 mgl_a_texcoord%u;\n", i);
                if (!append(out, decl))
                    return 0;
            }
        }
    }
    if (contains_identifier(source, "gl_ModelViewProjectionMatrix") ||
        strstr(source, "ftransform") != NULL)
        if (!append(out, "uniform mat4 mgl_u_model_view_projection;\n"))
            return 0;
    if (contains_identifier(source, "gl_ModelViewMatrix"))
        if (!append(out, "uniform mat4 mgl_u_model_view;\n"))
            return 0;
    if (contains_identifier(source, "gl_ProjectionMatrix"))
        if (!append(out, "uniform mat4 mgl_u_projection;\n"))
            return 0;
    if (contains_identifier(source, "gl_NormalMatrix"))
        if (!append(out, "uniform mat3 mgl_u_normal_matrix;\n"))
            return 0;
    if (contains_identifier(source, "gl_TextureMatrix"))
        if (!append(out, "uniform mat4 mgl_u_texture_matrix[8];\n"))
            return 0;
    if (contains_identifier(source, "gl_TexCoord")) {
        if (!append(out, qualifier) || !append(out, " vec4 mgl_v_texcoord[8];\n"))
            return 0;
    }
    /* Fragment gl_Color needs both inputs for front/back primitives.  A
     * vertex shader writing either legacy color exports both so a link with
     * a fragment gl_Color reader has matching varying declarations. */
    {
        const int front = contains_identifier(source, "gl_FrontColor");
        const int back = contains_identifier(source, "gl_BackColor");
        const int facing_color =
            type == GL_FRAGMENT_SHADER && contains_identifier(source, "gl_Color");
        if (front || facing_color || (type == GL_VERTEX_SHADER && back)) {
            if (!append(out, qualifier) || !append(out, " vec4 mgl_v_front_color;\n"))
                return 0;
        }
        if (back || facing_color || (type == GL_VERTEX_SHADER && front)) {
            if (!append(out, qualifier) || !append(out, " vec4 mgl_v_back_color;\n"))
                return 0;
        }
    }
    if (contains_identifier(source, "gl_FrontSecondaryColor")) {
        if (!append(out, qualifier) || !append(out, " vec4 mgl_v_front_secondary;\n"))
            return 0;
    }
    if (contains_identifier(source, "gl_BackSecondaryColor")) {
        if (!append(out, qualifier) || !append(out, " vec4 mgl_v_back_secondary;\n"))
            return 0;
    }
    if (contains_identifier(source, "gl_FogFragCoord")) {
        if (!append(out, qualifier) || !append(out, " float mgl_v_fog_coord;\n"))
            return 0;
    }
    if (type == GL_FRAGMENT_SHADER) {
        if (!append(out, "uniform int mgl_u_alpha_test_enabled;\n"
                         "uniform int mgl_u_alpha_test_func;\n"
                         "uniform float mgl_u_alpha_test_ref;\n"
                         "bool mgl_alpha_test_pass(float a){\n"
                         "  if(mgl_u_alpha_test_func==512) return false;\n"
                         "  if(mgl_u_alpha_test_func==513) return a<mgl_u_alpha_test_ref;\n"
                         "  if(mgl_u_alpha_test_func==514) return a==mgl_u_alpha_test_ref;\n"
                         "  if(mgl_u_alpha_test_func==515) return a<=mgl_u_alpha_test_ref;\n"
                         "  if(mgl_u_alpha_test_func==516) return a>mgl_u_alpha_test_ref;\n"
                         "  if(mgl_u_alpha_test_func==517) return a!=mgl_u_alpha_test_ref;\n"
                         "  if(mgl_u_alpha_test_func==518) return a>=mgl_u_alpha_test_ref;\n"
                         "  return true;\n"
                         "}\n"))
            return 0;
    }
    return 1;
}

static int fragment_source_uses_index_layout(const char *source) {
    const char *p = source;
    while (p != NULL && (p = strstr(p, "layout")) != NULL) {
        const char *open;
        const char *close;
        const char *q;
        if ((p == source || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) &&
            !(isalnum((unsigned char)p[6]) || p[6] == '_')) {
            open = p + 6;
            while (*open != '\0' && isspace((unsigned char)*open))
                ++open;
            if (*open == '(' && (close = strchr(open + 1, ')')) != NULL) {
                q = open + 1;
                while (q < close) {
                    q = strstr(q, "index");
                    if (q == NULL || q >= close)
                        break;
                    if ((q == open + 1 || !(isalnum((unsigned char)q[-1]) || q[-1] == '_')) &&
                        q + 5 <= close &&
                        !(q + 5 < close && (isalnum((unsigned char)q[5]) || q[5] == '_'))) {
                        const char *r = q + 5;
                        while (r < close && isspace((unsigned char)*r))
                            ++r;
                        if (r < close && *r == '=')
                            return 1;
                    }
                    q += 5;
                }
            }
        }
        p += 6;
    }
    return 0;
}

static char *lower_point_size(const char *source, int mirror_front_to_back) {
    struct text_builder out = {0};
    const char *body = source;
    const char *line_end = strchr(source, '\n');
    if (line_end != NULL && strncmp(source, "#version", 8u) == 0)
        body = glsl_prologue_end(line_end + 1);
    if (!append_n(&out, source, (size_t)(body - source)) ||
        !append(&out, "uniform float mgl_u_point_size;\n"
                      "uniform int mgl_u_program_point_size;\n"
                      "#define main mgl_user_main\n") ||
        !append(&out, body) ||
        !append(&out, "\n#undef main\n"
                      "void main(){ mgl_user_main(); ") ||
        (mirror_front_to_back && !append(&out, "mgl_v_back_color = mgl_v_front_color; ")) ||
        !append(&out, "if(mgl_u_program_point_size==0) gl_PointSize=mgl_u_point_size; }\n")) {
        free(out.data);
        return NULL;
    }
    return take(&out);
}

/* Desktop uniform initializers are carried as metadata because GLES rejects their source syntax. */
static char *lower_scalar_uniform_initializers(const char *src) {
    struct text_builder out = {0};
    struct text_builder defaults = {0};
    const char *p = src;
    while (*p) {
        const char *start = p;
        const char *q, *type, *name, *literal, *end;
        size_t type_len, name_len, lit_len;
        char numeric[96];
        char *parse_end;
        if ((p != src && is_ident_char(p[-1])) || strncmp(p, "uniform", 7) != 0 ||
            is_ident_char(p[7])) {
            if (!append_n(&out, p++, 1))
                goto fail;
            continue;
        }
        q = p + 7;
        while (*q == ' ' || *q == '\t' || *q == '\n')
            ++q;
        if (strncmp(q, "highp", 5) == 0 || strncmp(q, "mediump", 7) == 0 ||
            strncmp(q, "lowp", 4) == 0) {
            while (is_ident_char(*q))
                ++q;
            while (*q == ' ' || *q == '\t')
                ++q;
        }
        type = q;
        while (is_ident_char(*q))
            ++q;
        type_len = (size_t)(q - type);
        if (!((type_len == 5 && memcmp(type, "float", 5) == 0) ||
              (type_len == 3 && memcmp(type, "int", 3) == 0) ||
              (type_len == 4 && memcmp(type, "bool", 4) == 0)))
            goto ordinary;
        while (*q == ' ' || *q == '\t')
            ++q;
        name = q;
        if (!is_ident_start(*q))
            goto ordinary;
        while (is_ident_char(*q))
            ++q;
        name_len = (size_t)(q - name);
        if (name_len == 0 || name_len >= 96)
            goto ordinary;
        while (*q == ' ' || *q == '\t')
            ++q;
        if (*q++ != '=')
            goto ordinary;
        while (*q == ' ' || *q == '\t')
            ++q;
        literal = q;
        if (*q == '+' || *q == '-')
            ++q;
        if (type_len == 4) {
            if (strncmp(q, "true", 4) == 0)
                q += 4;
            else if (strncmp(q, "false", 5) == 0)
                q += 5;
            else
                goto ordinary;
        } else {
            while (isdigit((unsigned char)*q))
                ++q;
            if (*q == '.') {
                ++q;
                while (isdigit((unsigned char)*q))
                    ++q;
            }
            if (*q == 'e' || *q == 'E') {
                ++q;
                if (*q == '+' || *q == '-')
                    ++q;
                if (!isdigit((unsigned char)*q))
                    goto ordinary;
                while (isdigit((unsigned char)*q))
                    ++q;
            }
        }
        end = q;
        lit_len = (size_t)(end - literal);
        if (lit_len == 0 || lit_len >= sizeof(numeric))
            goto ordinary;
        while (*q == ' ' || *q == '\t')
            ++q;
        if (*q != ';')
            goto ordinary;
        memcpy(numeric, literal, lit_len);
        numeric[lit_len] = '\0';
        if (type_len != 4) {
            (void)strtod(numeric, &parse_end);
            if (*parse_end != '\0')
                goto ordinary;
        }

        if (!append_n(&out, start, (size_t)(name + name_len - start)) || !append(&out, ";"))
            goto fail;
        {
            char line[256];
            int n = snprintf(line, sizeof(line), "\n/*MGL_SCALAR_UNIFORM_DEFAULT %.*s %.*s %s*/\n",
                             (int)type_len, type, (int)name_len, name, numeric);
            if (n < 0 || (size_t)n >= sizeof(line) || !append(&defaults, line))
                goto fail;
        }
        p = q + 1;
        continue;
    ordinary:
        if (!append_n(&out, p++, 1))
            goto fail;
    }
    if (defaults.data && !append(&out, defaults.data))
        goto fail;
    free(defaults.data);
    return take(&out);
fail:
    free(defaults.data);
    free(out.data);
    return NULL;
}

static char *translate_glsl_expanded(GLenum shader_type, const char *source,
                                     char **translation_log) {
    int version;
    const char *body;
    int legacy;
    int es300;
    struct text_builder out = {0};
    char *stage1 = NULL;
    char *stage2 = NULL;
    char *preprocessor = NULL;
    const char *version_line;
    int uses_point_coord;
    int uses_frag_color, uses_frag_data;
    struct fragment_output frag_outputs;
    const char *alpha_output = NULL;
    char alpha_output_text[160];
    char fragdata_names[4][128] = {{0}};
    unsigned fragdata_mask = 0u;
    int mixed_fragdata = 0;

    if (translation_log != NULL)
        *translation_log = NULL;
    if (source == NULL)
        return NULL;
    if (!parse_version_and_body(source, &version, &body))
        return NULL;
    legacy = version <= 120;
    uses_point_coord =
        shader_type == GL_FRAGMENT_SHADER && contains_identifier(body, "gl_PointCoord");
    uses_frag_color =
        shader_type == GL_FRAGMENT_SHADER && contains_identifier(body, "gl_FragColor");
    uses_frag_data = shader_type == GL_FRAGMENT_SHADER && contains_identifier(body, "gl_FragData");
    find_fragment_outputs(body, &frag_outputs);
    mixed_fragdata = uses_frag_data && frag_outputs.count != 0u;
    if (mixed_fragdata) {
        unsigned i;
        fragdata_mask = fragdata_literal_mask(body);
        for (i = 0u; i < 4u; ++i) {
            const char *existing = frag_outputs.by_location[i];
            if (i == 0u && !*existing && *frag_outputs.first_unlocated)
                existing = frag_outputs.first_unlocated;
            if (*existing)
                strcpy(fragdata_names[i], existing);
            else
                (void)snprintf(fragdata_names[i], sizeof(fragdata_names[i]), "mgl_frag_data_%u", i);
        }
    }
    if (!legacy) {
        unsigned target_es = 320u;
        char *toolchain_source = NULL;
        char *toolchain_log = NULL;
        if (mgl_spirv_translate_glsl(shader_type, source, target_es, &toolchain_source,
                                     &toolchain_log)) {
            if (shader_type == GL_VERTEX_SHADER) {
                char *lowered = lower_point_size(toolchain_source, 0);
                free(toolchain_source);
                toolchain_source = lowered;
                if (toolchain_source == NULL) {
                    free(toolchain_log);
                    return NULL;
                }
            }
            if (uses_point_coord) {
                char *lowered = lower_point_coord(toolchain_source);
                free(toolchain_source);
                toolchain_source = lowered;
                if (toolchain_source == NULL) {
                    free(toolchain_log);
                    return NULL;
                }
            }
            if (translation_log != NULL)
                *translation_log = toolchain_log;
            else
                free(toolchain_log);
            return toolchain_source;
        }
        /* Use source-based translation when the SPIR-V compiler rejects a stage
         * supported by the compatibility translator. */
        if (shader_type == GL_GEOMETRY_SHADER || shader_type == GL_TESS_CONTROL_SHADER ||
            shader_type == GL_TESS_EVALUATION_SHADER) {
            if (translation_log != NULL)
                *translation_log = toolchain_log;
            else
                free(toolchain_log);
            return NULL;
        }
        free(toolchain_log);
    }
    es300 = 1;
    version_line = "#version 320 es\n";
    if (!append(&out, version_line))
        goto fail;
    if (!legacy && shader_type == GL_FRAGMENT_SHADER && fragment_source_uses_index_layout(source) &&
        !append(&out, "#extension GL_EXT_blend_func_extended : require\n"))
        goto fail;
    /* Keep source #extension and #define directives before generated declarations and
     * compatibility helpers. */
    {
        const char *code = glsl_prologue_end(body);
        struct text_builder prefix = {0};
        if (!append_n(&prefix, body, (size_t)(code - body)))
            goto fail;
        preprocessor = replace_identifiers(prefix.data, es300, shader_type);
        free(prefix.data);
        if (preprocessor == NULL ||
            !append_es300_prologue(&out, preprocessor, strlen(preprocessor)))
            goto fail;
        free(preprocessor);
        preprocessor = NULL;
        body = code;
    }
    /* GLSL ES requires explicit sampler precision, including vertex shaders. */
    if (!append(&out, "precision highp float;\nprecision highp int;\n"))
        goto fail;
    {
        static const char *const samplers[] = {
            "sampler2D",       "sampler3D",         "samplerCube",          "sampler2DArray",
            "sampler2DShadow", "samplerCubeShadow", "sampler2DArrayShadow", "isampler2D",
            "isampler3D",      "isamplerCube",      "isampler2DArray",      "usampler2D",
            "usampler3D",      "usamplerCube",      "usampler2DArray"};
        size_t i;
        for (i = 0u; i < sizeof(samplers) / sizeof(samplers[0]); ++i) {
            if (contains_identifier(source, samplers[i])) {
                if (!append(&out, "precision highp ") || !append(&out, samplers[i]) ||
                    !append(&out, ";\n"))
                    goto fail;
            }
        }
    }
    if (legacy && !append_compat_declarations(&out, shader_type, source))
        goto fail;
    if (es300 && !append_legacy_texture_functions(&out, source, shader_type))
        goto fail;
    if (shader_type == GL_FRAGMENT_SHADER) {
        const char *explicit_output =
            frag_outputs.location_zero[0] ? frag_outputs.location_zero : frag_outputs.first;
        if (uses_frag_data) {
            if (mixed_fragdata) {
                unsigned i;
                for (i = 0u; i < 4u; ++i) {
                    char declaration[96];
                    if (!(fragdata_mask & (1u << i)))
                        continue;
                    if (frag_outputs.by_location[i][0] ||
                        (i == 0u && frag_outputs.first_unlocated[0]))
                        continue;
                    (void)snprintf(declaration, sizeof(declaration),
                                   "layout(location=%u) out vec4 mgl_frag_data_%u;\n", i, i);
                    if (!append(&out, declaration))
                        goto fail;
                }
                alpha_output = *explicit_output ? explicit_output : fragdata_names[0];
            } else {

                if (!append(&out, "layout(location=0) out vec4 mgl_frag_data[4];\n"))
                    goto fail;
                alpha_output = "mgl_frag_data[0]";
            }
        }
        if (uses_frag_color) {
            if (uses_frag_data) {
                if (!append(&out, "#define mgl_frag_color mgl_frag_data[0]\n"))
                    goto fail;
            } else if (*explicit_output != '\0') {
                /* Use an application-declared fragment output when present, including references
                 * defined through macros. */
                if (!append(&out, "#define mgl_frag_color ") || !append(&out, explicit_output) ||
                    !append(&out, "\n"))
                    goto fail;
                alpha_output = explicit_output;
            } else {
                if (!append(&out, "layout(location=0) out vec4 mgl_frag_color;\n"))
                    goto fail;
                alpha_output = "mgl_frag_color";
            }
        } else if (!uses_frag_data && *explicit_output != '\0') {
            alpha_output = explicit_output;
        }
    }

    stage1 = replace_ftransform(body);
    if (stage1 == NULL)
        goto fail;
    stage2 = replace_identifiers(stage1, es300, shader_type);
    free(stage1);
    stage1 = NULL;
    if (stage2 == NULL)
        goto fail;
    if (legacy && shader_type == GL_FRAGMENT_SHADER && alpha_output != NULL) {
        int n =
            snprintf(alpha_output_text, sizeof(alpha_output_text),
                     "  if(mgl_u_alpha_test_enabled!=0 && !mgl_alpha_test_pass(%s.a)) discard;\n",
                     alpha_output);
        if (n <= 0 || (size_t)n >= sizeof(alpha_output_text))
            goto fail;
        if (!append(&out, "#define main mgl_user_main\n") || !append(&out, stage2) ||
            !append(&out, "\n#undef main\nvoid main(){\n  mgl_user_main();\n") ||
            !append(&out, alpha_output_text) || !append(&out, "}\n"))
            goto fail;
    } else if (!append(&out, stage2))
        goto fail;
    free(stage2);
    stage2 = NULL;
    if (mixed_fragdata) {
        char *unmapped = take(&out);
        char *mapped;
        if (unmapped == NULL)
            return NULL;
        mapped = remap_mixed_fragdata(unmapped, fragdata_names);
        free(unmapped);
        if (mapped == NULL)
            return NULL;
        if (!append(&out, mapped)) {
            free(mapped);
            goto fail;
        }
        free(mapped);
    }

    if (translation_log != NULL) {
        const char *message = legacy ? "desktop GLSL compatibility source lowered to GLSL ES 3.00"
                                     : "desktop GLSL source translated to GLSL ES 3.00 baseline";
        *translation_log = malloc(strlen(message) + 1u);
        if (*translation_log != NULL)
            strcpy(*translation_log, message);
    }
    if (uses_point_coord) {
        char *translated = take(&out);
        char *lowered;
        if (translated == NULL)
            return NULL;
        lowered = lower_point_coord(translated);
        free(translated);
        return lowered;
    }
    if (shader_type == GL_VERTEX_SHADER) {
        char *translated = take(&out);
        char *lowered;
        if (translated == NULL)
            return NULL;
        lowered =
            lower_point_size(translated, legacy && contains_identifier(source, "gl_FrontColor") &&
                                             !contains_identifier(source, "gl_BackColor"));
        free(translated);
        return lowered;
    }
    return take(&out);
fail:
    free(stage1);
    free(stage2);
    free(preprocessor);
    free(out.data);
    return NULL;
}

static char *translate_glsl_raw(GLenum shader_type, const char *source, char **translation_log) {
    int version;
    const char *body;
    char *expanded = NULL;
    char *preprocess_log = NULL;
    char *translated;
    if (translation_log != NULL)
        *translation_log = NULL;
    if (source == NULL || !parse_version_and_body(source, &version, &body))
        return NULL;
    (void)body;
    if (version <= 120) {
        /* Resolve desktop preprocessor conditions before built-in replacement
         * and GLSL ES lowering. */
        if (!mgl_preprocess_desktop_glsl(shader_type, source, (unsigned)version, &expanded,
                                         &preprocess_log)) {
            if (translation_log != NULL)
                *translation_log = preprocess_log;
            else
                free(preprocess_log);
            return NULL;
        }
        translated = translate_glsl_expanded(shader_type, expanded, translation_log);
        free(expanded);
        free(preprocess_log);
        return translated;
    }
    return translate_glsl_expanded(shader_type, source, translation_log);
}

/* Typed GLSL validation resolves implicit-conversion semantics. */
static char *mgl_typed_legacy_essl(GLenum shader_type, const char *source) {
    static const char es_header[] = "#version 320 es\n";
    static const char desktop_header[] = "#version 330 core\n";
    const char *uniform_defaults;
    size_t size;
    char *desktop;
    char *typed = NULL;
    char *compiler_log = NULL;
    if (strncmp(source, es_header, sizeof(es_header) - 1u) != 0)
        return NULL;
    size = strlen(source) - (sizeof(es_header) - 1u) + sizeof(desktop_header);
    desktop = malloc(size);
    if (desktop == NULL)
        return NULL;
    memcpy(desktop, desktop_header, sizeof(desktop_header) - 1u);
    strcpy(desktop + sizeof(desktop_header) - 1u, source + sizeof(es_header) - 1u);
    if (!mgl_spirv_translate_glsl(shader_type, desktop, 320u, &typed, &compiler_log)) {
        free(desktop);
        free(compiler_log);
        return NULL;
    }
    free(desktop);
    free(compiler_log);
    /* Restore desktop uniform initializers from compatibility metadata after
     * SPIRV-Cross translation, which drops source comments. */
    uniform_defaults = strstr(source, "/*MGL_SCALAR_UNIFORM_DEFAULT ");
    if (uniform_defaults != NULL) {
        struct text_builder output = {0};
        if (!append(&output, typed) || !append(&output, "\n") ||
            !append(&output, uniform_defaults)) {
            free(output.data);
            free(typed);
            return NULL;
        }
        free(typed);
        typed = take(&output);
    }
    return typed;
}

char *mgl_translate_glsl(GLenum shader_type, const char *source, char **translation_log) {
    char *raw = translate_glsl_raw(shader_type, source, translation_log);
    char *defaults;
    if (raw == NULL)
        return NULL;
    defaults = lower_scalar_uniform_initializers(raw);
    free(raw);
    if (defaults == NULL)
        return NULL;
    {
        int version;
        const char *body;
        if (source != NULL && parse_version_and_body(source, &version, &body) && version <= 120 &&
            shader_type != GL_COMPUTE_SHADER && !mgl_validate_gles_glsl(shader_type, defaults)) {
            char *typed = mgl_typed_legacy_essl(shader_type, defaults);
            if (typed != NULL) {
                free(defaults);
                return typed;
            }
            /* When typed compilation fails, retain the native compiler diagnostic. */
        }
    }
    return defaults;
}

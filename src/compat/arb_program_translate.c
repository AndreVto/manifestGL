#include "mgl/gl.h"

#include "compat/arb_program.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct text_builder {
    char *data;
    size_t size;
    size_t capacity;
};

static int builder_reserve(struct text_builder *b, size_t extra) {
    size_t required;
    size_t capacity;
    char *grown;
    if (b == NULL || extra > SIZE_MAX - b->size - 1u)
        return 0;
    required = b->size + extra + 1u;
    if (required <= b->capacity)
        return 1;
    capacity = b->capacity != 0u ? b->capacity : 512u;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2u) {
            capacity = required;
            break;
        }
        capacity *= 2u;
    }
    grown = realloc(b->data, capacity);
    if (grown == NULL)
        return 0;
    b->data = grown;
    b->capacity = capacity;
    return 1;
}

static int builder_append_n(struct text_builder *b, const char *text, size_t count) {
    if (!builder_reserve(b, count))
        return 0;
    if (count != 0u)
        memcpy(b->data + b->size, text, count);
    b->size += count;
    b->data[b->size] = '\0';
    return 1;
}

static int builder_append(struct text_builder *b, const char *text) {
    return text != NULL ? builder_append_n(b, text, strlen(text)) : 0;
}

static int builder_printf(struct text_builder *b, const char *fmt, ...) {
    va_list ap;
    va_list copy;
    int needed;
    if (b == NULL || fmt == NULL)
        return 0;
    va_start(ap, fmt);
    va_copy(copy, ap);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0 || !builder_reserve(b, (size_t)needed)) {
        va_end(ap);
        return 0;
    }
    (void)vsnprintf(b->data + b->size, b->capacity - b->size, fmt, ap);
    va_end(ap);
    b->size += (size_t)needed;
    return 1;
}

static char *builder_take(struct text_builder *b) {
    char *out;
    if (b == NULL)
        return NULL;
    if (b->data == NULL) {
        b->data = malloc(1u);
        if (b->data == NULL)
            return NULL;
        b->data[0] = '\0';
    }
    out = b->data;
    b->data = NULL;
    b->size = 0u;
    b->capacity = 0u;
    return out;
}

static void builder_clear(struct text_builder *b) {
    if (b == NULL)
        return;
    free(b->data);
    memset(b, 0, sizeof(*b));
}

static char *dup_n(const char *text, size_t count) {
    char *out = malloc(count + 1u);
    if (out == NULL)
        return NULL;
    if (count != 0u)
        memcpy(out, text, count);
    out[count] = '\0';
    return out;
}

static char *trim(char *text) {
    char *end;
    if (text == NULL)
        return NULL;
    while (isspace((unsigned char)*text))
        ++text;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
        --end;
    *end = '\0';
    return text;
}

static int valid_target(GLenum target) {
    return target == GL_VERTEX_PROGRAM_ARB || target == GL_FRAGMENT_PROGRAM_ARB;
}

static int valid_color_alias_suffix(const char *suffix) {
    unsigned count = 0u;
    if (suffix == NULL || *suffix == '\0')
        return 1;
    if (*suffix++ != '.')
        return 0;
    while ((*suffix == 'x' || *suffix == 'y' || *suffix == 'z' || *suffix == 'w') && count < 4u) {
        ++suffix;
        ++count;
    }
    return count != 0u && *suffix == '\0';
}

static int append_operand(struct text_builder *out, const char *operand, GLenum target,
                          struct mgl_arb_translation *translation, int destination) {
    char buffer[512];
    char mapped[512];
    const char *p;
    char *end;
    int negate = 0;
    size_t len;

    if (out == NULL || operand == NULL || translation == NULL)
        return 0;
    (void)snprintf(buffer, sizeof(buffer), "%s", operand);
    p = trim(buffer);
    if (!destination && *p == '-') {
        negate = 1;
        ++p;
        while (isspace((unsigned char)*p))
            ++p;
    }

    mapped[0] = '\0';
    {
        char *numeric_end;
        (void)strtod(p, &numeric_end);
        if (numeric_end != p && *numeric_end == '\0')
            (void)snprintf(mapped, sizeof(mapped), "vec4(float(%s))", p);
    }
    if (mapped[0] != '\0') {
    } else if (strncmp(p, "vertex.position", 15u) == 0) {
        (void)snprintf(mapped, sizeof(mapped), "mgl_a_position%s", p + 15);
    } else if (strncmp(p, "vertex.normal", 13u) == 0) {
        (void)snprintf(mapped, sizeof(mapped), "mgl_a_normal%s", p + 13);
    } else if (strncmp(p, "vertex.color.primary", 20u) == 0) {
        (void)snprintf(mapped, sizeof(mapped), "mgl_a_color%s", p + 20);
    } else if (strncmp(p, "vertex.color.secondary", 22u) == 0) {
        (void)snprintf(mapped, sizeof(mapped), "mgl_a_secondary%s", p + 22);
    } else if (strncmp(p, "vertex.color", 12u) == 0) {
        if (!valid_color_alias_suffix(p + 12))
            return 0;
        (void)snprintf(mapped, sizeof(mapped), "mgl_a_color%s", p + 12);
    } else if (strncmp(p, "vertex.texcoord[", 16u) == 0) {
        unsigned index = 0u;
        int consumed = 0;
        if (sscanf(p + 16, "%u]%n", &index, &consumed) != 1 || index >= 8u)
            return 0;
        (void)snprintf(mapped, sizeof(mapped), "mgl_a_texcoord%u%s", index, p + 16 + consumed);
    } else if (strncmp(p, "vertex.attrib[", 14u) == 0) {
        unsigned index = 0u;
        int consumed = 0;
        if (sscanf(p + 14, "%u]%n", &index, &consumed) != 1 || index >= 32u)
            return 0;
        translation->generic_mask |= UINT32_C(1) << index;
        (void)snprintf(mapped, sizeof(mapped), "mgl_g_attr%u%s", index, p + 14 + consumed);
    } else if (strncmp(p, "fragment.position", 17u) == 0) {
        if (target != GL_FRAGMENT_PROGRAM_ARB || destination || !valid_color_alias_suffix(p + 17))
            return 0;
        (void)snprintf(mapped, sizeof(mapped), "gl_FragCoord%s", p + 17);
    } else if (strncmp(p, "fragment.color.primary", 22u) == 0) {
        (void)snprintf(mapped, sizeof(mapped), "mgl_arb_v_color%s", p + 22);
    } else if (strncmp(p, "fragment.color.secondary", 24u) == 0) {
        (void)snprintf(mapped, sizeof(mapped), "mgl_arb_v_secondary%s", p + 24);
    } else if (strncmp(p, "fragment.color", 14u) == 0) {
        if (!valid_color_alias_suffix(p + 14))
            return 0;
        (void)snprintf(mapped, sizeof(mapped), "mgl_arb_v_color%s", p + 14);
    } else if (strncmp(p, "fragment.texcoord[", 18u) == 0) {
        unsigned index = 0u;
        int consumed = 0;
        if (sscanf(p + 18, "%u]%n", &index, &consumed) != 1 || index >= 8u)
            return 0;
        (void)snprintf(mapped, sizeof(mapped), "mgl_arb_v_texcoord%u%s", index, p + 18 + consumed);
    } else if (strncmp(p, "program.env[", 12u) == 0) {
        unsigned index = 0u;
        int consumed = 0;
        const char *name = target == GL_VERTEX_PROGRAM_ARB ? "mgl_arb_vp_env" : "mgl_arb_fp_env";
        if (sscanf(p + 12, "%u]%n", &index, &consumed) != 1 ||
            index >= MGL_ARB_MAX_PROGRAM_PARAMETERS)
            return 0;
        translation->uses_env = GL_TRUE;
        if (translation->max_env_index == UINT_MAX || index > translation->max_env_index)
            translation->max_env_index = index;
        (void)snprintf(mapped, sizeof(mapped), "%s[%u]%s", name, index, p + 12 + consumed);
    } else if (strncmp(p, "program.local[", 14u) == 0) {
        unsigned index = 0u;
        int consumed = 0;
        const char *name =
            target == GL_VERTEX_PROGRAM_ARB ? "mgl_arb_vp_local" : "mgl_arb_fp_local";
        if (sscanf(p + 14, "%u]%n", &index, &consumed) != 1 ||
            index >= MGL_ARB_MAX_PROGRAM_PARAMETERS)
            return 0;
        translation->uses_local = GL_TRUE;
        if (translation->max_local_index == UINT_MAX || index > translation->max_local_index)
            translation->max_local_index = index;
        (void)snprintf(mapped, sizeof(mapped), "%s[%u]%s", name, index, p + 14 + consumed);
    } else if (strncmp(p, "state.matrix.", 13u) == 0) {
        const char *matrix_name;
        const char *row_string;
        unsigned row = 0u;
        int consumed = 0;
        if (strncmp(p, "state.matrix.mvp.row[", 21u) == 0) {
            matrix_name = "mgl_u_model_view_projection";
            row_string = p + 21;
        } else if (strncmp(p, "state.matrix.modelview.row[", 27u) == 0) {
            matrix_name = "mgl_u_model_view";
            row_string = p + 27;
        } else if (strncmp(p, "state.matrix.projection.row[", 28u) == 0) {
            matrix_name = "mgl_u_projection";
            row_string = p + 28;
        } else {
            return 0;
        }
        if (sscanf(row_string, "%u]%n", &row, &consumed) != 1 || consumed == 0 || row >= 4u ||
            !valid_color_alias_suffix(row_string + consumed))
            return 0;
        (void)snprintf(mapped, sizeof(mapped), "vec4(%s[0][%u],%s[1][%u],%s[2][%u],%s[3][%u])%s",
                       matrix_name, row, matrix_name, row, matrix_name, row, matrix_name, row,
                       row_string + consumed);
    } else if (strncmp(p, "result.position", 15u) == 0) {
        if (target != GL_VERTEX_PROGRAM_ARB)
            return 0;
        (void)snprintf(mapped, sizeof(mapped), "gl_Position%s", p + 15);
    } else if (strncmp(p, "result.color.secondary", 22u) == 0) {
        if (target == GL_VERTEX_PROGRAM_ARB)
            (void)snprintf(mapped, sizeof(mapped), "mgl_arb_v_secondary%s", p + 22);
        else
            (void)snprintf(mapped, sizeof(mapped), "gl_FragColor%s", p + 22);
    } else if (strncmp(p, "result.color", 12u) == 0) {
        if (target == GL_VERTEX_PROGRAM_ARB)
            (void)snprintf(mapped, sizeof(mapped), "mgl_arb_v_color%s", p + 12);
        else
            (void)snprintf(mapped, sizeof(mapped), "gl_FragColor%s", p + 12);
    } else if (strncmp(p, "result.texcoord[", 16u) == 0) {
        unsigned index = 0u;
        int consumed = 0;
        if (target != GL_VERTEX_PROGRAM_ARB || sscanf(p + 16, "%u]%n", &index, &consumed) != 1 ||
            index >= 8u)
            return 0;
        (void)snprintf(mapped, sizeof(mapped), "mgl_arb_v_texcoord%u%s", index, p + 16 + consumed);
    } else if (*p == '{') {
        const char *close = strchr(p, '}');
        char inside[384];
        if (close == NULL || (size_t)(close - p - 1) >= sizeof(inside))
            return 0;
        memcpy(inside, p + 1, (size_t)(close - p - 1));
        inside[close - p - 1] = '\0';
        (void)snprintf(mapped, sizeof(mapped), "vec4(%s)%s", inside, close + 1);
    } else {
        (void)snprintf(mapped, sizeof(mapped), "%s", p);
    }

    len = strlen(mapped);
    if (len == 0u)
        return 0;
    if (!destination && len >= 2u && mapped[len - 2u] == '.' &&
        (strchr("xyzw", mapped[len - 1u]) != NULL ||
         (target == GL_FRAGMENT_PROGRAM_ARB && strchr("rgba", mapped[len - 1u]) != NULL))) {
        char component = mapped[len - 1u];
        if (len + 3u >= sizeof(mapped))
            return 0;
        mapped[len] = component;
        mapped[len + 1u] = component;
        mapped[len + 2u] = component;
        mapped[len + 3u] = '\0';
        len += 3u;
    }
    if (negate && !builder_append(out, "-("))
        return 0;
    if (!builder_append(out, mapped))
        return 0;
    if (negate && !builder_append(out, ")"))
        return 0;
    end = mapped + len;
    (void)end;
    return 1;
}

static int split_operands(char *text, char **operands, unsigned max_operands, unsigned *out_count) {
    unsigned count = 0u;
    char *start = text;
    char *p = text;
    unsigned braces = 0u;
    if (out_count == NULL)
        return 0;
    while (*p != '\0') {
        if (*p == '{')
            ++braces;
        else if (*p == '}' && braces > 0u)
            --braces;
        else if (*p == ',' && braces == 0u) {
            *p = '\0';
            if (count >= max_operands)
                return 0;
            operands[count++] = trim(start);
            start = p + 1;
        }
        ++p;
    }
    if (*trim(start) != '\0') {
        if (count >= max_operands)
            return 0;
        operands[count++] = trim(start);
    }
    *out_count = count;
    return 1;
}

static const char *destination_mask(const char *operand) {
    const char *dot;
    const char *p;
    size_t count = 0u;
    if (operand == NULL)
        return NULL;
    dot = strrchr(operand, '.');
    if (dot == NULL || dot[1] == '\0')
        return NULL;
    for (p = dot + 1; *p != '\0'; ++p) {
        if (*p != 'x' && *p != 'y' && *p != 'z' && *p != 'w')
            return NULL;
        ++count;
    }
    return count >= 1u && count <= 4u ? dot + 1 : NULL;
}

struct mgl_arb_instruction_stats {
    char temps[MGL_ARB_MAX_PROGRAM_TEMPORARIES][128];
    unsigned count;
    uint64_t output;
    uint64_t alu;
};

static int stats_temp(const struct mgl_arb_instruction_stats *stats, const char *operand) {
    unsigned i;
    const char *p = operand;
    if (p == NULL)
        return -1;
    while (*p == '-' || isspace((unsigned char)*p))
        ++p;
    for (i = 0u; i < stats->count; ++i) {
        size_t len = strlen(stats->temps[i]);
        if (strncmp(p, stats->temps[i], len) == 0 && (p[len] == '\0' || p[len] == '.'))
            return (int)i;
    }
    return -1;
}

static void stats_instruction(struct mgl_arb_instruction_stats *stats,
                              struct mgl_arb_translation *translation, int texture,
                              char *const *operands, unsigned count) {
    int dst = count != 0u ? stats_temp(stats, operands[0]) : -1;
    int coord = count > 1u ? stats_temp(stats, operands[1]) : -1;
    unsigned i;
    if (texture) {
        if ((coord >= 0 && (stats->output & (UINT64_C(1) << coord)) != 0u) ||
            (dst >= 0 && (stats->alu & (UINT64_C(1) << dst)) != 0u)) {
            ++translation->texture_indirections;
            stats->output = 0u;
            stats->alu = 0u;
        }
        ++translation->texture_count;
    } else {
        for (i = 0u; i < count; ++i) {
            int index = stats_temp(stats, operands[i]);
            if (index >= 0)
                stats->alu |= UINT64_C(1) << index;
        }
    }
    if (dst >= 0 && !(count == 1u))
        stats->output |= UINT64_C(1) << dst;
}

static int stats_declare_temps(struct mgl_arb_instruction_stats *stats, const char *source) {
    char *list = dup_n(source, strlen(source));
    char *cursor;
    if (list == NULL)
        return 0;
    cursor = list;
    while (*cursor != '\0') {
        char *comma = strchr(cursor, ',');
        char *name;
        size_t length;
        if (comma != NULL)
            *comma = '\0';
        name = trim(cursor);
        length = strlen(name);
        if (length == 0u || length >= sizeof(stats->temps[0]) ||
            stats->count >= MGL_ARB_MAX_PROGRAM_TEMPORARIES) {
            free(list);
            return 0;
        }
        memcpy(stats->temps[stats->count++], name, length + 1u);
        if (comma == NULL)
            break;
        cursor = comma + 1;
    }
    free(list);
    return 1;
}

static unsigned stats_parameter_bindings(const char *statement) {
    const char *end = strchr(statement, '=');
    const char *open = strchr(statement, '[');
    unsigned count = 0u;
    int consumed = 0;
    if (end == NULL || open == NULL || open > end)
        return 1u;
    if (sscanf(open, "[%u]%n", &count, &consumed) != 1 || consumed == 0 || open + consumed > end ||
        count == 0u)
        return 1u;
    return count;
}

static int translate_instruction(struct text_builder *body, char *statement, GLenum target,
                                 struct mgl_arb_translation *translation, int position_invariant,
                                 struct mgl_arb_instruction_stats *stats) {
    char *p = trim(statement);
    char *space;
    char opcode[24];
    char *operands[5] = {0};
    unsigned count = 0u;
    int saturate = 0;
    struct text_builder dst = {0};
    struct text_builder src[3] = {{0}};
    const char *expr = NULL;
    const char *write_mask = NULL;
    struct text_builder expr_builder = {0};
    unsigned i;

    space = p;
    while (*space != '\0' && !isspace((unsigned char)*space))
        ++space;
    if ((size_t)(space - p) >= sizeof(opcode))
        return 0;
    memcpy(opcode, p, (size_t)(space - p));
    opcode[space - p] = '\0';
    if (strlen(opcode) > 4u && strcmp(opcode + strlen(opcode) - 4u, "_SAT") == 0) {
        opcode[strlen(opcode) - 4u] = '\0';
        saturate = 1;
    }
    p = trim(space);
    if (!split_operands(p, operands, 5u, &count))
        return 0;
    if (count > 0u)
        write_mask = destination_mask(operands[0]);
    if (position_invariant && target == GL_VERTEX_PROGRAM_ARB && count > 0u &&
        strncmp(operands[0], "result.position", 15u) == 0 &&
        (operands[0][15] == '\0' || operands[0][15] == '.'))
        goto fail;

    if (strcmp(opcode, "ARL") == 0) {
        if (target != GL_VERTEX_PROGRAM_ARB || count != 2u || write_mask == NULL ||
            strcmp(write_mask, "x") != 0 ||
            !append_operand(&dst, operands[0], target, translation, 1) ||
            !append_operand(&src[0], operands[1], target, translation, 0) ||
            !builder_printf(body, "%s=floor((%s).x);\n", dst.data, src[0].data))
            goto fail;
        builder_clear(&dst);
        builder_clear(&src[0]);
        return 1;
    }

    if (strcmp(opcode, "KIL") == 0) {
        if (target != GL_FRAGMENT_PROGRAM_ARB || count != 1u ||
            !append_operand(&src[0], operands[0], target, translation, 0))
            goto fail;
        if (!builder_printf(body, "if(any(lessThan(%s,vec4(0.0)))) discard;\n", src[0].data))
            goto fail;
        stats_instruction(stats, translation, 0, operands, count);
        builder_clear(&src[0]);
        return 1;
    }

    if (strcmp(opcode, "TEX") == 0 || strcmp(opcode, "TXP") == 0 || strcmp(opcode, "TXB") == 0) {
        unsigned unit = 0u;
        int consumed = 0;
        GLenum texture_target;
        const char *texture_function;
        const char *coord_suffix;
        int projected = strcmp(opcode, "TXP") == 0;
        int biased = strcmp(opcode, "TXB") == 0;
        if (target != GL_FRAGMENT_PROGRAM_ARB || count != 4u ||
            sscanf(operands[2], "texture[%u]%n", &unit, &consumed) != 1 || consumed == 0 ||
            operands[2][consumed] != '\0' || unit >= MGL_TEXTURE_UNITS)
            goto fail;
        if (strcmp(operands[3], "2D") == 0) {
            texture_target = GL_TEXTURE_2D;
            texture_function = "texture2D";
            coord_suffix = ".xy";
        } else if (strcmp(operands[3], "CUBE") == 0) {
            if (projected)
                goto fail;
            texture_target = GL_TEXTURE_CUBE_MAP;
            texture_function = "textureCube";
            coord_suffix = ".xyz";
        } else {
            goto fail;
        }
        if (translation->texture_targets[unit] != 0u &&
            translation->texture_targets[unit] != texture_target)
            goto fail;
        if (!append_operand(&dst, operands[0], target, translation, 1) ||
            !append_operand(&src[0], operands[1], target, translation, 0))
            goto fail;
        if (!builder_printf(&expr_builder, "%s(mgl_arb_texture%u,(%s)%s", texture_function, unit,
                            src[0].data, coord_suffix))
            goto fail;
        if (projected && !builder_printf(&expr_builder, "/(%s).w", src[0].data))
            goto fail;
        if (biased && !builder_printf(&expr_builder, ",(%s).w", src[0].data))
            goto fail;
        if (!builder_append(&expr_builder, ")"))
            goto fail;
        if (write_mask != NULL) {
            if (!builder_printf(body, "%s=%s(%s).%s%s;\n", dst.data, saturate ? "clamp(" : "",
                                expr_builder.data, write_mask, saturate ? ",0.0,1.0)" : ""))
                goto fail;
        } else if (!builder_printf(body, "%s=%s%s%s;\n", dst.data, saturate ? "clamp(" : "",
                                   expr_builder.data, saturate ? ",0.0,1.0)" : ""))
            goto fail;
        stats_instruction(stats, translation, 1, operands, count);
        translation->texture_targets[unit] = texture_target;
        if (unit == 0u)
            translation->uses_texture0 = GL_TRUE;
        builder_clear(&dst);
        builder_clear(&src[0]);
        builder_clear(&expr_builder);
        return 1;
    }

    if (count < 2u || !append_operand(&dst, operands[0], target, translation, 1))
        goto fail;
    for (i = 1u; i < count && i <= 3u; ++i)
        if (!append_operand(&src[i - 1u], operands[i], target, translation, 0))
            goto fail;

    if (strcmp(opcode, "MOV") == 0 && count == 2u) {
        expr = src[0].data;
    } else if (strcmp(opcode, "ADD") == 0 && count == 3u) {
        if (!builder_printf(&expr_builder, "(%s+%s)", src[0].data, src[1].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "SUB") == 0 && count == 3u) {
        if (!builder_printf(&expr_builder, "(%s-%s)", src[0].data, src[1].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "MUL") == 0 && count == 3u) {
        if (!builder_printf(&expr_builder, "(%s*%s)", src[0].data, src[1].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "MAD") == 0 && count == 4u) {
        if (!builder_printf(&expr_builder, "(%s*%s+%s)", src[0].data, src[1].data, src[2].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "DP3") == 0 && count == 3u) {
        if (!builder_printf(&expr_builder, "vec4(dot((%s).xyz,(%s).xyz))", src[0].data,
                            src[1].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "DP4") == 0 && count == 3u) {
        if (!builder_printf(&expr_builder, "vec4(dot(%s,%s))", src[0].data, src[1].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "MIN") == 0 && count == 3u) {
        if (!builder_printf(&expr_builder, "min(%s,%s)", src[0].data, src[1].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "MAX") == 0 && count == 3u) {
        if (!builder_printf(&expr_builder, "max(%s,%s)", src[0].data, src[1].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "RCP") == 0 && count == 2u) {
        if (!builder_printf(&expr_builder, "vec4(1.0/(%s).x)", src[0].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "RSQ") == 0 && count == 2u) {
        if (!builder_printf(&expr_builder, "vec4(inversesqrt(abs((%s).x)))", src[0].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "ABS") == 0 && count == 2u) {
        if (!builder_printf(&expr_builder, "abs(%s)", src[0].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "FLR") == 0 && count == 2u) {
        if (!builder_printf(&expr_builder, "floor(%s)", src[0].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "FRC") == 0 && count == 2u) {
        if (!builder_printf(&expr_builder, "fract(%s)", src[0].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "EX2") == 0 && count == 2u) {
        if (!builder_printf(&expr_builder, "vec4(exp2((%s).x))", src[0].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "LG2") == 0 && count == 2u) {
        if (!builder_printf(&expr_builder, "vec4(log2(abs((%s).x)))", src[0].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "POW") == 0 && count == 3u) {
        if (!builder_printf(&expr_builder, "vec4(pow(abs((%s).x),(%s).x))", src[0].data,
                            src[1].data))
            goto fail;
        expr = expr_builder.data;
    } else if (strcmp(opcode, "XPD") == 0 && count == 3u) {
        if (!builder_printf(&expr_builder, "vec4(cross((%s).xyz,(%s).xyz),1.0)", src[0].data,
                            src[1].data))
            goto fail;
        expr = expr_builder.data;
    } else {
        goto fail;
    }

    if (write_mask != NULL) {
        if (saturate) {
            if (!builder_printf(body, "%s=clamp((%s).%s,0.0,1.0);\n", dst.data, expr, write_mask))
                goto fail;
        } else if (!builder_printf(body, "%s=(%s).%s;\n", dst.data, expr, write_mask)) {
            goto fail;
        }
    } else if (saturate) {
        if (!builder_printf(body, "%s=clamp(%s,0.0,1.0);\n", dst.data, expr))
            goto fail;
    } else if (!builder_printf(body, "%s=%s;\n", dst.data, expr)) {
        goto fail;
    }
    stats_instruction(stats, translation, 0, operands, count);
    builder_clear(&dst);
    for (i = 0u; i < 3u; ++i)
        builder_clear(&src[i]);
    builder_clear(&expr_builder);
    return 1;

fail:
    builder_clear(&dst);
    for (i = 0u; i < 3u; ++i)
        builder_clear(&src[i]);
    builder_clear(&expr_builder);
    return 0;
}

static char *statement_keyword(char *statement, const char *keyword) {
    size_t size = strlen(keyword);
    if (strncmp(statement, keyword, size) != 0 || !isspace((unsigned char)statement[size]))
        return NULL;
    return trim(statement + size);
}

static int translate_declaration(struct text_builder *defs, struct text_builder *locals,
                                 char *statement, GLenum target,
                                 struct mgl_arb_translation *translation) {
    char *p = trim(statement);
    char *temp = statement_keyword(p, "TEMP");
    char *param = statement_keyword(p, "PARAM");
    char *attrib = statement_keyword(p, "ATTRIB");
    char *output = statement_keyword(p, "OUTPUT");
    char *address = statement_keyword(p, "ADDRESS");
    if (temp != NULL) {
        char *names = temp;
        char *cursor = names;
        while (*cursor != '\0') {
            char *comma = strchr(cursor, ',');
            char saved = '\0';
            char *name;
            if (comma != NULL) {
                saved = *comma;
                *comma = '\0';
            }
            name = trim(cursor);
            if (*name == '\0' || !builder_printf(locals, "vec4 %s=vec4(0.0);\n", name))
                return 0;
            if (comma == NULL)
                break;
            *comma = saved;
            cursor = comma + 1;
        }
        return 1;
    }

    if (param != NULL && strstr(param, "state.matrix.") != NULL) {
        char param_name[64];
        unsigned param_count = 0u;
        char *eq = strchr(param, '=');
        if (eq != NULL) {
            char saved = *eq;
            char *lhs;
            char *rhs;
            *eq = '\0';
            lhs = trim(param);
            rhs = trim(eq + 1);
            const char *matrix_name = NULL;
            if (strcmp(rhs, "{ state.matrix.mvp }") == 0)
                matrix_name = "mgl_u_model_view_projection";
            else if (strcmp(rhs, "{ state.matrix.modelview }") == 0)
                matrix_name = "mgl_u_model_view";
            else if (strcmp(rhs, "{ state.matrix.projection }") == 0)
                matrix_name = "mgl_u_projection";
            if (sscanf(lhs, "%63[^[][%u]", param_name, &param_count) == 2 && param_count == 4u &&
                matrix_name != NULL) {
                unsigned row;
                if (!builder_printf(locals, "vec4 %s[4];\n", trim(param_name))) {
                    *eq = saved;
                    return 0;
                }
                for (row = 0u; row < 4u; ++row) {
                    if (!builder_printf(locals,
                                        "%s[%u]=vec4(%s[0][%u],%s[1][%u],%s[2][%u],%s[3][%u]);\n",
                                        trim(param_name), row, matrix_name, row, matrix_name, row,
                                        matrix_name, row, matrix_name, row)) {
                        *eq = saved;
                        return 0;
                    }
                }
                *eq = saved;
                return 1;
            }
            *eq = saved;
        }
    }

    if (attrib != NULL || output != NULL || param != NULL) {
        char *declaration = attrib != NULL ? attrib : output != NULL ? output : param;
        char *eq = strchr(declaration, '=');
        char *name;
        char *value;
        struct text_builder mapped = {0};
        if (eq == NULL)
            return 0;
        *eq = '\0';
        name = trim(declaration);
        value = trim(eq + 1);
        if (*name == '\0' || !append_operand(&mapped, value, target, translation, output != NULL)) {
            builder_clear(&mapped);
            return 0;
        }
        if (!builder_printf(defs, "#define %s (%s)\n", name, mapped.data)) {
            builder_clear(&mapped);
            return 0;
        }
        builder_clear(&mapped);
        return 1;
    }
    if (address != NULL) {
        char *name = address;
        return *name != '\0' && builder_printf(locals, "vec4 %s=vec4(0.0);\n", name);
    }
    return 0;
}

static int translate_option(const char *option, GLenum target, int *position_invariant,
                            unsigned *fog_option, unsigned *precision_option) {
    if (target == GL_VERTEX_PROGRAM_ARB) {
        if (strcmp(option, "ARB_position_invariant") != 0)
            return 0;
        *position_invariant = 1;
        return 1;
    }
    if (target != GL_FRAGMENT_PROGRAM_ARB)
        return 0;
    if (strcmp(option, "ARB_precision_hint_fastest") == 0) {
        if (*precision_option == 2u)
            return 0;
        *precision_option = 1u;
        return 1;
    }
    if (strcmp(option, "ARB_precision_hint_nicest") == 0) {
        if (*precision_option == 1u)
            return 0;
        *precision_option = 2u;
        return 1;
    }
    if (strcmp(option, "ARB_fog_linear") == 0 || strcmp(option, "ARB_fog_exp") == 0 ||
        strcmp(option, "ARB_fog_exp2") == 0) {
        unsigned mode = strcmp(option, "ARB_fog_linear") == 0 ? 1u
                        : strcmp(option, "ARB_fog_exp") == 0  ? 2u
                                                              : 3u;
        if (*fog_option != 0u && *fog_option != mode)
            return 0;
        *fog_option = mode;
        return 1;
    }
    return 0;
}

int mgl_arb_translate(GLenum target, const char *source, struct mgl_arb_translation *out,
                      GLint *error_position, char *error_text, size_t error_text_size) {
    const char *header;
    const char *body_start;
    char *copy = NULL;
    char *cursor;
    struct text_builder defs = {0};
    struct text_builder locals = {0};
    struct text_builder body = {0};
    struct text_builder result = {0};
    unsigned i;
    int saw_end = 0;
    int saw_statement = 0;
    int position_invariant = 0;
    unsigned fog_option = 0u;
    unsigned precision_option = 0u;
    struct mgl_arb_instruction_stats stats = {0};
    GLint resource_position = 0;

    if (out == NULL || source == NULL || !valid_target(target))
        return 0;
    memset(out, 0, sizeof(*out));
    out->max_env_index = UINT_MAX;
    out->max_local_index = UINT_MAX;
    out->texture_indirections = target == GL_FRAGMENT_PROGRAM_ARB ? 1u : 0u;
    if (error_position != NULL)
        *error_position = -1;
    if (error_text != NULL && error_text_size != 0u)
        error_text[0] = '\0';

    header = target == GL_VERTEX_PROGRAM_ARB ? "!!ARBvp1.0" : "!!ARBfp1.0";
    while (isspace((unsigned char)*source))
        ++source;
    if (strncmp(source, header, strlen(header)) != 0) {
        if (error_position != NULL)
            *error_position = 0;
        if (error_text != NULL && error_text_size != 0u)
            (void)snprintf(error_text, error_text_size, "mGL: expected %s header", header);
        return 0;
    }
    body_start = source + strlen(header);
    copy = dup_n(body_start, strlen(body_start));
    if (copy == NULL)
        return 0;

    /* Replace ARB # comments with spaces to preserve source byte offsets in parser diagnostics. */
    {
        char *p = copy;
        while (*p != '\0') {
            if (*p == '#') {
                while (*p != '\0' && *p != '\n')
                    *p++ = ' ';
            } else {
                ++p;
            }
        }
    }

    cursor = copy;
    while (*cursor != '\0') {
        char *semi = strchr(cursor, ';');
        char *statement;
        ptrdiff_t statement_offset;
        if (semi == NULL) {
            char *tail = trim(cursor);
            if (*tail != '\0' && strcmp(tail, "END") != 0) {
                if (error_position != NULL)
                    *error_position = (GLint)(strlen(header) + (size_t)(tail - copy));
                if (error_text != NULL && error_text_size != 0u)
                    (void)snprintf(error_text, error_text_size,
                                   "mGL: unterminated ARB program statement");
                goto fail;
            }
            if (strcmp(tail, "END") == 0)
                saw_end = 1;
            break;
        }
        *semi = '\0';
        statement = trim(cursor);
        statement_offset = statement - copy;
        resource_position = (GLint)(strlen(header) + (size_t)statement_offset);
        cursor = semi + 1;
        if (*statement == '\0')
            continue;
        if (strcmp(statement, "END") == 0) {
            saw_end = 1;
            break;
        }
        {
            char *option = statement_keyword(statement, "OPTION");
            int declaration;
            int instruction;
            char *param_declaration = statement_keyword(statement, "PARAM");
            unsigned declared_parameters =
                param_declaration != NULL ? stats_parameter_bindings(param_declaration) : 0u;
            if (option != NULL && !saw_statement &&
                translate_option(option, target, &position_invariant, &fog_option,
                                 &precision_option))
                continue;
            declaration =
                option == NULL && translate_declaration(&defs, &locals, statement, target, out);
            instruction =
                option == NULL && !declaration &&
                translate_instruction(&body, statement, target, out, position_invariant, &stats);
            if (!declaration && !instruction) {
                if (error_position != NULL)
                    *error_position = (GLint)(strlen(header) + (size_t)statement_offset);
                if (error_text != NULL && error_text_size != 0u)
                    (void)snprintf(error_text, error_text_size,
                                   "mGL: unsupported or malformed ARB statement: %.120s",
                                   statement);
                goto fail;
            }
            if (instruction) {
                ++out->instruction_count;
            } else {
                char *temp = statement_keyword(statement, "TEMP");
                char *param = statement_keyword(statement, "PARAM");
                char *attrib = statement_keyword(statement, "ATTRIB");
                char *address = statement_keyword(statement, "ADDRESS");
                if (temp != NULL) {
                    if (!stats_declare_temps(&stats, temp))
                        goto resource_limit;
                    out->temporary_count = stats.count;
                }
                if (param != NULL)
                    out->parameter_count += declared_parameters;
                if (attrib != NULL)
                    ++out->attrib_count;
                if (address != NULL)
                    ++out->address_count;
            }
            if (out->instruction_count > MGL_ARB_MAX_PROGRAM_INSTRUCTIONS ||
                out->temporary_count > MGL_ARB_MAX_PROGRAM_TEMPORARIES ||
                out->parameter_count > MGL_ARB_MAX_PROGRAM_PARAMETERS ||
                out->attrib_count > MGL_ARB_MAX_PROGRAM_ATTRIBS ||
                out->texture_count > MGL_ARB_MAX_PROGRAM_TEX_INSTRUCTIONS ||
                out->address_count > (target == GL_VERTEX_PROGRAM_ARB ? 1u : 0u))
                goto resource_limit;
        }
        saw_statement = 1;
    }
    if (!saw_end) {
        if (error_position != NULL)
            *error_position = (GLint)strlen(source);
        if (error_text != NULL && error_text_size != 0u)
            (void)snprintf(error_text, error_text_size, "mGL: missing END");
        goto fail;
    }

    if (!builder_append(&result, "#version 100\n"))
        goto oom;
    if (target == GL_FRAGMENT_PROGRAM_ARB &&
        !builder_append(&result, "precision mediump float;\nprecision mediump int;\n"
                                 "uniform int mgl_u_alpha_test_enabled;\n"
                                 "uniform int mgl_u_alpha_test_func;\n"
                                 "uniform float mgl_u_alpha_test_ref;\n"
                                 "bool mgl_arb_alpha_pass(float a){"
                                 "if(mgl_u_alpha_test_func==512)return false;"
                                 "if(mgl_u_alpha_test_func==513)return a<mgl_u_alpha_test_ref;"
                                 "if(mgl_u_alpha_test_func==514)return a==mgl_u_alpha_test_ref;"
                                 "if(mgl_u_alpha_test_func==515)return a<=mgl_u_alpha_test_ref;"
                                 "if(mgl_u_alpha_test_func==516)return a>mgl_u_alpha_test_ref;"
                                 "if(mgl_u_alpha_test_func==517)return a!=mgl_u_alpha_test_ref;"
                                 "if(mgl_u_alpha_test_func==518)return a>=mgl_u_alpha_test_ref;"
                                 "return true;}\n"))
        goto oom;

    if (!builder_append(&result, "uniform mat4 mgl_u_model_view_projection;\n"
                                 "uniform mat4 mgl_u_model_view;\n"
                                 "uniform mat4 mgl_u_projection;\n"))
        goto oom;
    if (target == GL_VERTEX_PROGRAM_ARB) {
        if (!builder_append(&result, "attribute vec4 mgl_a_position;\n"
                                     "attribute vec4 mgl_a_normal;\n"
                                     "attribute vec4 mgl_a_color;\n"
                                     "attribute vec4 mgl_a_secondary;\n"
                                     "attribute vec4 mgl_a_texcoord0;\n"
                                     "attribute vec4 mgl_a_texcoord1;\n"
                                     "attribute vec4 mgl_a_texcoord2;\n"
                                     "attribute vec4 mgl_a_texcoord3;\n"
                                     "attribute vec4 mgl_a_texcoord4;\n"
                                     "attribute vec4 mgl_a_texcoord5;\n"
                                     "attribute vec4 mgl_a_texcoord6;\n"
                                     "attribute vec4 mgl_a_texcoord7;\n"))
            goto oom;
        for (i = 0u; i < 32u; ++i) {
            if ((out->generic_mask & (UINT32_C(1) << i)) != 0u &&
                !builder_printf(&result, "attribute vec4 mgl_g_attr%u;\n", i))
                goto oom;
        }
    }

    if (out->uses_env) {
        const char *name = target == GL_VERTEX_PROGRAM_ARB ? "mgl_arb_vp_env" : "mgl_arb_fp_env";
        if (!builder_printf(&result, "uniform vec4 %s[%u];\n", name, out->max_env_index + 1u))
            goto oom;
    }
    if (out->uses_local) {
        const char *name =
            target == GL_VERTEX_PROGRAM_ARB ? "mgl_arb_vp_local" : "mgl_arb_fp_local";
        if (!builder_printf(&result, "uniform vec4 %s[%u];\n", name, out->max_local_index + 1u))
            goto oom;
    }
    if (target == GL_FRAGMENT_PROGRAM_ARB) {
        for (i = 0u; i < MGL_TEXTURE_UNITS; ++i) {
            if (out->texture_targets[i] != 0u &&
                !builder_printf(&result, "uniform %s mgl_arb_texture%u;\n",
                                out->texture_targets[i] == GL_TEXTURE_CUBE_MAP ? "samplerCube"
                                                                               : "sampler2D",
                                i))
                goto oom;
        }
    }

    if (!builder_append(&result, "varying vec4 mgl_arb_v_color;\n"
                                 "varying vec4 mgl_arb_v_secondary;\n"
                                 "varying vec4 mgl_arb_v_texcoord0;\n"
                                 "varying vec4 mgl_arb_v_texcoord1;\n"
                                 "varying vec4 mgl_arb_v_texcoord2;\n"
                                 "varying vec4 mgl_arb_v_texcoord3;\n"
                                 "varying vec4 mgl_arb_v_texcoord4;\n"
                                 "varying vec4 mgl_arb_v_texcoord5;\n"
                                 "varying vec4 mgl_arb_v_texcoord6;\n"
                                 "varying vec4 mgl_arb_v_texcoord7;\n"))
        goto oom;

    if (!builder_append_n(&result, defs.data != NULL ? defs.data : "", defs.size) ||
        !builder_append(&result, "void main(){\n") ||
        !builder_append_n(&result, locals.data != NULL ? locals.data : "", locals.size) ||
        (position_invariant &&
         !builder_append(&result, "gl_Position=mgl_u_model_view_projection*mgl_a_position;\n")) ||
        !builder_append_n(&result, body.data != NULL ? body.data : "", body.size))
        goto oom;
    if (target == GL_FRAGMENT_PROGRAM_ARB &&
        !builder_append(
            &result,
            "if(mgl_u_alpha_test_enabled!=0&&!mgl_arb_alpha_pass(gl_FragColor.a))discard;\n"))
        goto oom;
    if (!builder_append(&result, "}\n"))
        goto oom;

    out->source = builder_take(&result);
    if (out->source == NULL)
        goto oom;
    free(copy);
    builder_clear(&defs);
    builder_clear(&locals);
    builder_clear(&body);
    return 1;

resource_limit:
    if (error_position != NULL)
        *error_position = resource_position;
    if (error_text != NULL && error_text_size != 0u)
        (void)snprintf(error_text, error_text_size, "mGL: ARB program resource limit exceeded");
    goto fail;

oom:
    if (error_text != NULL && error_text_size != 0u)
        (void)snprintf(error_text, error_text_size, "mGL: out of memory translating ARB program");
fail:
    free(copy);
    builder_clear(&defs);
    builder_clear(&locals);
    builder_clear(&body);
    builder_clear(&result);
    mgl_arb_translation_destroy(out);
    return 0;
}

void mgl_arb_translation_destroy(struct mgl_arb_translation *translation) {
    if (translation == NULL)
        return;
    free(translation->source);
    memset(translation, 0, sizeof(*translation));
    translation->max_env_index = UINT_MAX;
    translation->max_local_index = UINT_MAX;
}

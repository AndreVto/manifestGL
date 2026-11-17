#include "texture/depth_readback.h"

#include "core/context.h"
#include "objects/object_store.h"
#include "shader/shader_store.h"
#include "texture/texture_store.h"

#include <stdlib.h>

static const GLchar depth_vs[] = "#version 300 es\n"
                                 "out vec2 vUV;\n"
                                 "void main(){\n"
                                 "  vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));\n"
                                 "  vUV=p*0.5; gl_Position=vec4(p*2.0-1.0,0.0,1.0);\n"
                                 "}\n";

static const GLchar depth_fs[] =
    "#version 300 es\n"
    "precision highp float;\n"
    "uniform sampler2D uDepth;\n"
    "uniform vec4 uRect;\n"
    "in vec2 vUV; layout(location=0) out vec4 outColor;\n"
    "void main(){\n"
    "  float d=clamp(texture(uDepth,uRect.xy+vUV*uRect.zw).r,0.0,1.0-1.0/16777216.0);\n"
    "  vec3 e=fract(d*vec3(1.0,256.0,65536.0));\n"
    "  e-=e.yzz*vec3(1.0/256.0,1.0/256.0,0.0);\n"
    "  outColor=vec4(e,1.0);\n"
    "}\n";

static void set_cap(struct mgl_context *ctx, GLenum cap, GLboolean enabled) {
    if (ctx->native_dispatch == NULL)
        return;
    if (enabled) {
        if (ctx->native_dispatch->glesEnable != NULL)
            ctx->native_dispatch->glesEnable(cap);
    } else if (ctx->native_dispatch->glesDisable != NULL)
        ctx->native_dispatch->glesDisable(cap);
}

static GLuint compile_shader(struct mgl_context *ctx, GLenum type, const GLchar *source) {
    GLuint shader;
    GLint ok = 0;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesCreateShader == NULL ||
        ctx->native_dispatch->glesShaderSource == NULL ||
        ctx->native_dispatch->glesCompileShader == NULL ||
        ctx->native_dispatch->glesGetShaderiv == NULL)
        return 0u;
    shader = ctx->native_dispatch->glesCreateShader(type);
    if (shader == 0u)
        return 0u;
    ctx->native_dispatch->glesShaderSource(shader, 1, &source, NULL);
    ctx->native_dispatch->glesCompileShader(shader);
    ctx->native_dispatch->glesGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        if (ctx->native_dispatch->glesDeleteShader != NULL)
            ctx->native_dispatch->glesDeleteShader(shader);
        return 0u;
    }
    return shader;
}

static GLuint build_program(struct mgl_context *ctx, GLuint *out_vs, GLuint *out_fs) {
    GLuint vs = 0u, fs = 0u, program = 0u;
    GLint ok = 0;
    vs = compile_shader(ctx, GL_VERTEX_SHADER, depth_vs);
    if (vs == 0u)
        return 0u;
    fs = compile_shader(ctx, GL_FRAGMENT_SHADER, depth_fs);
    if (fs == 0u)
        goto fail;
    if (ctx->native_dispatch->glesCreateProgram == NULL ||
        ctx->native_dispatch->glesAttachShader == NULL ||
        ctx->native_dispatch->glesLinkProgram == NULL ||
        ctx->native_dispatch->glesGetProgramiv == NULL)
        goto fail;
    program = ctx->native_dispatch->glesCreateProgram();
    if (program == 0u)
        goto fail;
    ctx->native_dispatch->glesAttachShader(program, vs);
    ctx->native_dispatch->glesAttachShader(program, fs);
    ctx->native_dispatch->glesLinkProgram(program);
    ctx->native_dispatch->glesGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
        goto fail;
    *out_vs = vs;
    *out_fs = fs;
    return program;
fail:
    if (program != 0u && ctx->native_dispatch->glesDeleteProgram != NULL) {
        ctx->draw_native_program_valid = GL_FALSE;
        ctx->native_dispatch->glesDeleteProgram(program);
    }
    if (fs != 0u && ctx->native_dispatch->glesDeleteShader != NULL)
        ctx->native_dispatch->glesDeleteShader(fs);
    if (vs != 0u && ctx->native_dispatch->glesDeleteShader != NULL)
        ctx->native_dispatch->glesDeleteShader(vs);
    return 0u;
}

static GLuint framebuffer_native(struct mgl_context *ctx, GLuint name) {
    struct mgl_framebuffer *fb = mgl_framebuffer_find(ctx, name);
    return fb != NULL ? fb->native_name : 0u;
}

static void restore_native_state(struct mgl_context *ctx) {
    struct mgl_program *program = mgl_program_find(ctx, ctx->current_program);
    GLuint draw_fb = framebuffer_native(ctx, ctx->draw_framebuffer);
    GLuint read_fb = framebuffer_native(ctx, ctx->read_framebuffer);
    if (ctx->native_dispatch == NULL)
        return;
    if (ctx->native_dispatch->glesBindFramebuffer != NULL) {
        if (ctx->draw_framebuffer == ctx->read_framebuffer)
            ctx->native_dispatch->glesBindFramebuffer(GL_FRAMEBUFFER, draw_fb);
        else {
            ctx->native_dispatch->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fb);
            ctx->native_dispatch->glesBindFramebuffer(GL_READ_FRAMEBUFFER, read_fb);
        }
    }
    if (ctx->native_dispatch->glesViewport != NULL)
        ctx->native_dispatch->glesViewport(
            ctx->canonical_state.viewport[0], ctx->canonical_state.viewport[1],
            ctx->canonical_state.viewport[2], ctx->canonical_state.viewport[3]);
    if (ctx->native_dispatch->glesScissor != NULL)
        ctx->native_dispatch->glesScissor(
            ctx->canonical_state.scissor[0], ctx->canonical_state.scissor[1],
            ctx->canonical_state.scissor[2], ctx->canonical_state.scissor[3]);
    set_cap(ctx, GL_SCISSOR_TEST, ctx->canonical_state.scissor_test);
    set_cap(ctx, GL_DEPTH_TEST, ctx->canonical_state.depth_test);
    set_cap(ctx, GL_STENCIL_TEST, ctx->canonical_state.stencil_test);
    set_cap(ctx, GL_BLEND, ctx->canonical_state.blend);
    set_cap(ctx, GL_CULL_FACE, ctx->canonical_state.cull_face);
    if (ctx->native_dispatch->glesColorMask != NULL)
        ctx->native_dispatch->glesColorMask(
            ctx->canonical_state.color_mask[0], ctx->canonical_state.color_mask[1],
            ctx->canonical_state.color_mask[2], ctx->canonical_state.color_mask[3]);
    if (ctx->native_dispatch->glesUseProgram != NULL)
        mgl_draw_use_native_program(ctx, program != NULL ? program->native_name : 0u);
    if (ctx->native_dispatch->glesBindVertexArray != NULL)
        ctx->native_dispatch->glesBindVertexArray(
            ctx->current_vao != NULL ? ctx->current_vao->native_name : 0u);
    mgl_texture_realize_bindings(ctx);
}

int mgl_depth_texture_readback(struct mgl_context *ctx, struct mgl_texture *texture, GLint level,
                               GLint x, GLint y, GLsizei width, GLsizei height,
                               GLfloat *out_depth) {
    struct mgl_texture_level *src;
    GLuint color_tex = 0u, color_fbo = 0u, vs = 0u, fs = 0u, program = 0u;
    GLint sampler_loc, rect_loc;
    unsigned char *rgba = NULL;
    size_t pixels, bytes, i;
    GLenum err = GL_NO_ERROR;
    int ok = 0;
    if (ctx == NULL || texture == NULL || out_depth == NULL || level < 0 ||
        level >= (GLint)MGL_MAX_TEXTURE_LEVELS || width < 0 || height < 0 || x < 0 || y < 0)
        return 0;
    src = &texture->levels[level];
    if (!src->defined || src->format != GL_DEPTH_COMPONENT || texture->native_name == 0u ||
        x > src->width || y > src->height || width > src->width - x || height > src->height - y)
        return 0;
    if ((size_t)width != 0u && (size_t)height > SIZE_MAX / (size_t)width)
        return 0;
    pixels = (size_t)width * (size_t)height;
    if (pixels > SIZE_MAX / 4u)
        return 0;
    bytes = pixels * 4u;
    rgba = malloc(bytes != 0u ? bytes : 1u);
    if (rgba == NULL)
        return 0;
    if (ctx->native_dispatch == NULL || ctx->native_dispatch->glesGenTextures == NULL ||
        ctx->native_dispatch->glesGenFramebuffers == NULL ||
        ctx->native_dispatch->glesBindTexture == NULL ||
        ctx->native_dispatch->glesTexImage2D == NULL ||
        ctx->native_dispatch->glesTexParameteri == NULL ||
        ctx->native_dispatch->glesBindFramebuffer == NULL ||
        ctx->native_dispatch->glesFramebufferTexture2D == NULL ||
        ctx->native_dispatch->glesCheckFramebufferStatus == NULL ||
        ctx->native_dispatch->glesDrawArrays == NULL ||
        ctx->native_dispatch->glesReadPixels == NULL ||
        ctx->native_dispatch->glesUseProgram == NULL ||
        ctx->native_dispatch->glesGetUniformLocation == NULL ||
        ctx->native_dispatch->glesUniform1i == NULL || ctx->native_dispatch->glesUniform4f == NULL)
        goto done;

    if (ctx->native_dispatch->glesGetError != NULL)
        while (ctx->native_dispatch->glesGetError() != GL_NO_ERROR) {}
    program = build_program(ctx, &vs, &fs);
    if (program == 0u)
        goto done;
    ctx->native_dispatch->glesGenTextures(1, &color_tex);
    ctx->native_dispatch->glesGenFramebuffers(1, &color_fbo);
    if (color_tex == 0u || color_fbo == 0u)
        goto done;
    if (ctx->native_dispatch->glesActiveTexture != NULL)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0);
    mgl_texture_bind_native(ctx, GL_TEXTURE_2D, color_tex);
    ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                            (GLint)GL_NEAREST);
    ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                            (GLint)GL_NEAREST);
    ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                            (GLint)GL_CLAMP_TO_EDGE);
    ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                            (GLint)GL_CLAMP_TO_EDGE);
    ctx->native_dispatch->glesTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA8, width, height, 0,
                                         GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    ctx->native_dispatch->glesBindFramebuffer(GL_FRAMEBUFFER, color_fbo);
    ctx->native_dispatch->glesFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                   GL_TEXTURE_2D, color_tex, 0);
    if (ctx->native_dispatch->glesCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        goto done;
    if (ctx->native_dispatch->glesViewport != NULL)
        ctx->native_dispatch->glesViewport(0, 0, width, height);
    set_cap(ctx, GL_SCISSOR_TEST, GL_FALSE);
    set_cap(ctx, GL_DEPTH_TEST, GL_FALSE);
    set_cap(ctx, GL_STENCIL_TEST, GL_FALSE);
    set_cap(ctx, GL_BLEND, GL_FALSE);
    set_cap(ctx, GL_CULL_FACE, GL_FALSE);
    if (ctx->native_dispatch->glesColorMask != NULL)
        ctx->native_dispatch->glesColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (ctx->native_dispatch->glesBindVertexArray != NULL)
        ctx->native_dispatch->glesBindVertexArray(0u);
    ctx->draw_enabled_mask_valid = GL_FALSE;
    ctx->draw_native_pointer_valid = 0u;
    mgl_draw_use_native_program(ctx, program);

    mgl_texture_bind_native(ctx, GL_TEXTURE_2D, texture->native_name);
    ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, level);
    ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, level);
    ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                            (GLint)GL_NEAREST);
    ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                            (GLint)GL_NEAREST);
    sampler_loc = ctx->native_dispatch->glesGetUniformLocation(program, "uDepth");
    rect_loc = ctx->native_dispatch->glesGetUniformLocation(program, "uRect");
    if (sampler_loc >= 0)
        ctx->native_dispatch->glesUniform1i(sampler_loc, 0);
    if (rect_loc >= 0)
        ctx->native_dispatch->glesUniform4f(
            rect_loc, (GLfloat)x / (GLfloat)src->width, (GLfloat)y / (GLfloat)src->height,
            (GLfloat)width / (GLfloat)src->width, (GLfloat)height / (GLfloat)src->height);
    ctx->native_dispatch->glesDrawArrays(GL_TRIANGLES, 0, 3);
    if (ctx->native_dispatch->glesBindBuffer != NULL)
        mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_PACK_BUFFER, 0u);
    if (ctx->native_dispatch->glesPixelStorei != NULL)
        ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, 1);
    if (bytes != 0u)
        ctx->native_dispatch->glesReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    if (ctx->native_dispatch->glesGetError != NULL)
        err = ctx->native_dispatch->glesGetError();
    if (err != GL_NO_ERROR)
        goto done;
    for (i = 0u; i < pixels; ++i) {
        double r = (double)rgba[i * 4u + 0u] / 255.0;
        double g = (double)rgba[i * 4u + 1u] / (255.0 * 256.0);
        double b = (double)rgba[i * 4u + 2u] / (255.0 * 65536.0);
        double value = r + g + b;
        if (value > 1.0)
            value = 1.0;
        out_depth[i] = (GLfloat)value;
    }
    ok = 1;
done:
    if (ctx != NULL && ctx->native_dispatch != NULL && texture != NULL &&
        texture->native_name != 0u && ctx->native_dispatch->glesBindTexture != NULL) {
        if (ctx->native_dispatch->glesActiveTexture != NULL)
            mgl_native_select_active_texture(ctx, GL_TEXTURE0);
        mgl_texture_bind_native(ctx, GL_TEXTURE_2D, texture->native_name);
        if (ctx->native_dispatch->glesTexParameteri != NULL) {
            ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL,
                                                    texture->base_level);
            ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
                                                    texture->max_level);
            ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                                    texture->min_filter);
            ctx->native_dispatch->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                                    texture->mag_filter);
        }
    }
    if (ctx != NULL && ctx->native_dispatch != NULL) {
        if (color_fbo != 0u && ctx->native_dispatch->glesDeleteFramebuffers != NULL)
            ctx->native_dispatch->glesDeleteFramebuffers(1, &color_fbo);
        if (color_tex != 0u && ctx->native_dispatch->glesDeleteTextures != NULL)
            ctx->native_dispatch->glesDeleteTextures(1, &color_tex);
        if (program != 0u && ctx->native_dispatch->glesDeleteProgram != NULL) {
            ctx->draw_native_program_valid = GL_FALSE;
            ctx->native_dispatch->glesDeleteProgram(program);
        }
        if (fs != 0u && ctx->native_dispatch->glesDeleteShader != NULL)
            ctx->native_dispatch->glesDeleteShader(fs);
        if (vs != 0u && ctx->native_dispatch->glesDeleteShader != NULL)
            ctx->native_dispatch->glesDeleteShader(vs);
        if (ctx->native_dispatch->glesPixelStorei != NULL)
            ctx->native_dispatch->glesPixelStorei(GL_PACK_ALIGNMENT, ctx->pack_alignment);
        if (ctx->native_dispatch->glesBindBuffer != NULL) {
            struct mgl_buffer *pack = mgl_buffer_find(ctx, ctx->pixel_pack_buffer);
            mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_PACK_BUFFER,
                                            pack != NULL ? pack->native_name : 0u);
        }
        restore_native_state(ctx);
    }
    free(rgba);
    return ok;
}

/* Renderbuffers and default depth attachments are not sampleable; stage depth through a GLES
 * texture and a depth-only pass. */
static const GLchar depth_copy_fs[] = "#version 300 es\n"
                                      "precision highp float;\n"
                                      "uniform sampler2D uDepth;\n"
                                      "in vec2 vUV;\n"
                                      "void main(){ gl_FragDepth=texture(uDepth,vUV).r; }\n";

void mgl_depth_copy_cleanup(struct mgl_context *ctx) {
    struct mgl_depth_copy_cache *cache;
    struct mgl_native_dispatch *b;
    if (ctx == NULL || ctx->native_dispatch == NULL)
        return;
    cache = &ctx->depth_copy;
    b = ctx->native_dispatch;
    if (cache->source_texture != 0u && b->glesDeleteTextures != NULL)
        b->glesDeleteTextures(1, &cache->source_texture);
    if (cache->source_framebuffer != 0u && b->glesDeleteFramebuffers != NULL)
        b->glesDeleteFramebuffers(1, &cache->source_framebuffer);
    if (cache->destination_framebuffer != 0u && b->glesDeleteFramebuffers != NULL)
        b->glesDeleteFramebuffers(1, &cache->destination_framebuffer);
    if (cache->program != 0u && b->glesDeleteProgram != NULL) {
        ctx->draw_native_program_valid = GL_FALSE;
        b->glesDeleteProgram(cache->program);
    }
    if (cache->vertex_shader != 0u && b->glesDeleteShader != NULL)
        b->glesDeleteShader(cache->vertex_shader);
    if (cache->fragment_shader != 0u && b->glesDeleteShader != NULL)
        b->glesDeleteShader(cache->fragment_shader);
    *cache = (struct mgl_depth_copy_cache){0};
}

static GLenum framebuffer_depth_format(struct mgl_context *ctx) {
    struct mgl_framebuffer *fb = mgl_framebuffer_find(ctx, ctx->read_framebuffer);
    struct mgl_renderbuffer *rb;
    struct mgl_texture *texture;
    struct mgl_texture_level *level;
    if (fb == NULL)
        return GL_DEPTH24_STENCIL8;
    rb = mgl_renderbuffer_find(ctx, fb->depth_renderbuffer);
    if (rb != NULL) {
        switch (rb->internal_format) {
            case GL_DEPTH_COMPONENT16:
                return GL_DEPTH_COMPONENT16;
            case GL_DEPTH_COMPONENT24:
                return GL_DEPTH_COMPONENT24;
            case GL_DEPTH_COMPONENT32F:
                return GL_DEPTH_COMPONENT32F;
            case GL_DEPTH24_STENCIL8:
                return GL_DEPTH24_STENCIL8;
            default:
                break;
        }
    }
    texture = mgl_texture_find(ctx, fb->depth_texture);
    if (texture != NULL && fb->depth_texture_level >= 0 &&
        fb->depth_texture_level < (GLint)MGL_MAX_TEXTURE_LEVELS) {
        level = &texture->levels[fb->depth_texture_level];
        return level->internal_format;
    }
    return 0u;
}

int mgl_depth_copy_framebuffer_to_texture(struct mgl_context *ctx, struct mgl_texture *texture,
                                          GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                          GLint x, GLint y, GLsizei width, GLsizei height) {
    struct mgl_depth_copy_cache *cache;
    struct mgl_texture_level *dst;
    struct mgl_native_dispatch *b;
    GLenum source_format, upload_format, upload_type, attach, none = GL_NONE, error = GL_NO_ERROR;
    GLuint read_fb;
    GLint sampler;
    int ok = 0;
    if (ctx == NULL || ctx->native_dispatch == NULL || texture == NULL || target != GL_TEXTURE_2D ||
        level < 0 || level >= (GLint)MGL_MAX_TEXTURE_LEVELS || xoffset < 0 || yoffset < 0 ||
        width < 0 || height < 0)
        return 0;
    dst = &texture->levels[level];
    if (!dst->defined || dst->compressed || dst->format != GL_DEPTH_COMPONENT ||
        texture->native_name == 0u || width > dst->width - xoffset ||
        height > dst->height - yoffset || xoffset > dst->width || yoffset > dst->height)
        return 0;
    if (width == 0 || height == 0)
        return 1;
    b = ctx->native_dispatch;
    if (b->glesGenTextures == NULL || b->glesDeleteTextures == NULL ||
        b->glesGenFramebuffers == NULL || b->glesDeleteFramebuffers == NULL ||
        b->glesBindTexture == NULL || b->glesTexImage2D == NULL || b->glesTexParameteri == NULL ||
        b->glesFramebufferTexture2D == NULL || b->glesBindFramebuffer == NULL ||
        b->glesBlitFramebuffer == NULL || b->glesCheckFramebufferStatus == NULL ||
        b->glesDrawBuffers == NULL || b->glesUseProgram == NULL || b->glesDrawArrays == NULL ||
        b->glesEnable == NULL || b->glesDisable == NULL || b->glesDepthFunc == NULL ||
        b->glesDepthMask == NULL || b->glesGetError == NULL || b->glesBindBuffer == NULL)
        return 0;
    mgl_texture_refresh_current_framebuffers(ctx);
    source_format = framebuffer_depth_format(ctx);
    if (source_format == 0u)
        return 0;
    attach =
        source_format == GL_DEPTH24_STENCIL8 ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
    if (attach == GL_DEPTH_STENCIL_ATTACHMENT) {
        upload_format = GL_DEPTH_STENCIL;
        upload_type = 0x84FAu;
    } else {
        upload_format = GL_DEPTH_COMPONENT;
        upload_type = source_format == GL_DEPTH_COMPONENT16   ? GL_UNSIGNED_SHORT
                      : source_format == GL_DEPTH_COMPONENT24 ? GL_UNSIGNED_INT
                                                              : GL_FLOAT;
    }
    cache = &ctx->depth_copy;
    if (cache->program == 0u) {
        GLuint vertex = compile_shader(ctx, GL_VERTEX_SHADER, depth_vs);
        GLuint fragment = compile_shader(ctx, GL_FRAGMENT_SHADER, depth_copy_fs);
        GLuint program = 0u;
        GLint linked = GL_FALSE;
        if (vertex != 0u && fragment != 0u && b->glesCreateProgram != NULL &&
            b->glesAttachShader != NULL && b->glesLinkProgram != NULL &&
            b->glesGetProgramiv != NULL) {
            program = b->glesCreateProgram();
            if (program != 0u) {
                b->glesAttachShader(program, vertex);
                b->glesAttachShader(program, fragment);
                b->glesLinkProgram(program);
                b->glesGetProgramiv(program, GL_LINK_STATUS, &linked);
            }
        }
        if (!linked) {
            if (program && b->glesDeleteProgram)
                b->glesDeleteProgram(program);
            if (vertex && b->glesDeleteShader)
                b->glesDeleteShader(vertex);
            if (fragment && b->glesDeleteShader)
                b->glesDeleteShader(fragment);
            goto done;
        }
        cache->vertex_shader = vertex;
        cache->fragment_shader = fragment;
        cache->program = program;
    }
    if (cache->source_texture == 0u)
        b->glesGenTextures(1, &cache->source_texture);
    if (cache->source_framebuffer == 0u)
        b->glesGenFramebuffers(1, &cache->source_framebuffer);
    if (cache->destination_framebuffer == 0u)
        b->glesGenFramebuffers(1, &cache->destination_framebuffer);
    if (!cache->source_texture || !cache->source_framebuffer || !cache->destination_framebuffer)
        goto done;
    if (b->glesActiveTexture)
        mgl_native_select_active_texture(ctx, GL_TEXTURE0);
    mgl_texture_bind_native(ctx, GL_TEXTURE_2D, cache->source_texture);
    if (cache->width != width || cache->height != height || cache->source_format != source_format) {
        b->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        b->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        b->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        b->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        struct mgl_buffer *unpack = mgl_buffer_find(ctx, ctx->pixel_unpack_buffer);
        b->glesTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_UNPACK_BUFFER, 0u);
        b->glesTexImage2D(GL_TEXTURE_2D, 0, (GLint)source_format, width, height, 0, upload_format,
                          upload_type, NULL);
        mgl_native_dispatch_bind_buffer(ctx, GL_PIXEL_UNPACK_BUFFER,
                                        unpack != NULL ? unpack->native_name : 0u);
        cache->width = width;
        cache->height = height;
        cache->source_format = source_format;
    }
    b->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER, cache->source_framebuffer);
    b->glesFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attach, GL_TEXTURE_2D, cache->source_texture,
                                0);
    b->glesDrawBuffers(1, &none);
    if (b->glesCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        goto done;
    read_fb = framebuffer_native(ctx, ctx->read_framebuffer);
    b->glesBindFramebuffer(GL_READ_FRAMEBUFFER, read_fb);
    b->glesBlitFramebuffer(x, y, x + width, y + height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT,
                           GL_NEAREST);
    if (b->glesGetError() != GL_NO_ERROR)
        goto done;
    b->glesBindFramebuffer(GL_DRAW_FRAMEBUFFER, cache->destination_framebuffer);
    b->glesFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                texture->native_name, level);
    b->glesDrawBuffers(1, &none);
    if (b->glesCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        goto done;
    if (b->glesViewport)
        b->glesViewport(xoffset, yoffset, width, height);
    set_cap(ctx, GL_SCISSOR_TEST, GL_FALSE);
    set_cap(ctx, GL_STENCIL_TEST, GL_FALSE);
    set_cap(ctx, GL_BLEND, GL_FALSE);
    set_cap(ctx, GL_CULL_FACE, GL_FALSE);
    set_cap(ctx, GL_DEPTH_TEST, GL_TRUE);
    b->glesDepthFunc(GL_ALWAYS);
    b->glesDepthMask(GL_TRUE);
    if (b->glesBindVertexArray)
        b->glesBindVertexArray(0u);
    ctx->draw_enabled_mask_valid = GL_FALSE;
    ctx->draw_native_pointer_valid = 0u;
    mgl_draw_use_native_program(ctx, cache->program);
    sampler = b->glesGetUniformLocation ? b->glesGetUniformLocation(cache->program, "uDepth") : -1;
    if (sampler >= 0 && b->glesUniform1i)
        b->glesUniform1i(sampler, 0);
    b->glesDrawArrays(GL_TRIANGLES, 0, 3);
    error = b->glesGetError();
    ok = error == GL_NO_ERROR;
done:
    if (b->glesDepthFunc)
        b->glesDepthFunc(ctx->canonical_state.depth_func);
    if (b->glesDepthMask)
        b->glesDepthMask(ctx->canonical_state.depth_mask);
    mgl_texture_mark_all_bindings_dirty(ctx);
    restore_native_state(ctx);
    return ok;
}

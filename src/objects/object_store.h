#ifndef MGL_OBJECT_STORE_H
#define MGL_OBJECT_STORE_H

#include "core/context.h"

struct mgl_buffer *mgl_buffer_find(struct mgl_context *ctx, GLuint name);
struct mgl_buffer *mgl_buffer_ensure(struct mgl_context *ctx, GLuint name, int generate_native);
void mgl_buffer_remove(struct mgl_context *ctx, GLuint name);
void mgl_buffer_retain_texture(struct mgl_buffer *buffer);
void mgl_buffer_release_texture(struct mgl_context *ctx, struct mgl_buffer *buffer);
GLuint *mgl_buffer_binding_slot(struct mgl_context *ctx, GLenum target);
struct mgl_buffer *mgl_bound_buffer(struct mgl_context *ctx, GLenum target);
int mgl_buffer_shadow_ensure(struct mgl_context *ctx, struct mgl_buffer *obj);
int mgl_buffer_read_native_bytes(struct mgl_context *ctx, struct mgl_buffer *obj,
                                 size_t offset, size_t length, void *data);
int mgl_buffer_write_bytes(struct mgl_context *ctx, struct mgl_buffer *obj, size_t offset,
                           size_t length, const void *data, int gpu_copy);
void mgl_restore_element_array_binding(struct mgl_context *ctx, GLuint name);

void mgl_vertex_array_init(struct mgl_vertex_array *vao);
void mgl_vertex_array_resolve(struct mgl_vertex_array *vao, GLuint index);
void mgl_vertex_array_resolve_slot(struct mgl_vertex_array *vao, GLuint slot);
struct mgl_vertex_array *mgl_vao_find(struct mgl_context *ctx, GLuint name);
struct mgl_vertex_array *mgl_vao_create_reserved(struct mgl_context *ctx, GLuint name);
void mgl_vao_remove(struct mgl_context *ctx, GLuint name);

struct mgl_framebuffer *mgl_framebuffer_find(struct mgl_context *ctx, GLuint name);
struct mgl_framebuffer *mgl_framebuffer_ensure(struct mgl_context *ctx, GLuint name,
                                               int generate_native);
void mgl_framebuffer_remove(struct mgl_context *ctx, GLuint name);
int mgl_framebuffer_draw_route_begin(struct mgl_context *ctx);
int mgl_framebuffer_draw_route_begin_for(struct mgl_context *ctx, struct mgl_framebuffer *fb);
void mgl_framebuffer_draw_route_end(struct mgl_context *ctx);
GLuint *mgl_framebuffer_binding_slot(struct mgl_context *ctx, GLenum target);
struct mgl_framebuffer *mgl_bound_framebuffer(struct mgl_context *ctx, GLenum target);

struct mgl_renderbuffer *mgl_renderbuffer_find(struct mgl_context *ctx, GLuint name);
struct mgl_renderbuffer *mgl_renderbuffer_ensure(struct mgl_context *ctx, GLuint name,
                                                 int generate_native);
void mgl_renderbuffer_remove(struct mgl_context *ctx, GLuint name);

#endif

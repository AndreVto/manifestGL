#ifndef MGL_ANDROID_AHB_PRESENTATION_H
#define MGL_ANDROID_AHB_PRESENTATION_H
struct mgl_context;
struct mgl_ahb_presentation;
/* Only returns a swapchain if Android AHB import, native fence synchronization,
 * SurfaceControl transactions and release-fence callbacks are all supported. */
struct mgl_ahb_presentation *
mgl_ahb_presentation_create(struct mgl_context *ctx, void *native_window, int width, int height);
/* A buffer is reusable only after SurfaceFlinger signals its release fence. */
int mgl_ahb_presentation_present(struct mgl_ahb_presentation *chain, struct mgl_context *ctx);
int mgl_ahb_presentation_dimensions_match(const struct mgl_ahb_presentation *chain, int width,
                                          int height);
unsigned mgl_ahb_presentation_framebuffer(const struct mgl_ahb_presentation *chain);
void mgl_ahb_presentation_destroy(struct mgl_ahb_presentation *chain, struct mgl_context *ctx);
#endif

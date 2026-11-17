#ifndef MGL_LOADER_H
#define MGL_LOADER_H

#include <stddef.h>

struct mgl_library {
    void *handle;
    char path[512];
};

int mgl_library_open(struct mgl_library *lib, const char *env_name, const char *const *fallbacks,
                     size_t fallback_count);
int mgl_library_open_filtered(struct mgl_library *lib, const char *env_name,
                              const char *const *fallbacks, size_t fallback_count,
                              const char *reject_symbol);
void *mgl_library_symbol(const struct mgl_library *lib, const char *name);
int mgl_library_preload_system_vulkan(struct mgl_library *lib, const char *absolute_path);
/* Detect a globally visible Vulkan entry point from a different loader. */
int mgl_library_system_vulkan_preempted(const struct mgl_library *lib);
void mgl_library_close(struct mgl_library *lib);

#endif

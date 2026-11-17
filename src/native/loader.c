#define _GNU_SOURCE
#include "native/loader.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int try_open(struct mgl_library *lib, const char *path, const char *reject_symbol) {
    void *handle;
    if (path == NULL || path[0] == '\0')
        return 0;
    handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL)
        return 0;
    if (reject_symbol != NULL && dlsym(handle, reject_symbol) != NULL) {
        (void)dlclose(handle);
        return 0;
    }
    lib->handle = handle;
    (void)snprintf(lib->path, sizeof(lib->path), "%s", path);
    return 1;
}

int mgl_library_open_filtered(struct mgl_library *lib, const char *env_name,
                              const char *const *fallbacks, size_t fallback_count,
                              const char *reject_symbol) {
    const char *override_path;
    size_t i;
    if (lib == NULL)
        return 0;
    memset(lib, 0, sizeof(*lib));
    override_path = env_name != NULL ? getenv(env_name) : NULL;
    if (try_open(lib, override_path, reject_symbol))
        return 1;
    for (i = 0; i < fallback_count; ++i) {
        if (try_open(lib, fallbacks[i], reject_symbol))
            return 1;
    }
    return 0;
}

int mgl_library_open(struct mgl_library *lib, const char *env_name, const char *const *fallbacks,
                     size_t fallback_count) {
    return mgl_library_open_filtered(lib, env_name, fallbacks, fallback_count, NULL);
}

void *mgl_library_symbol(const struct mgl_library *lib, const char *name) {
    if (lib == NULL || lib->handle == NULL || name == NULL)
        return NULL;
    return dlsym(lib->handle, name);
}

void mgl_library_close(struct mgl_library *lib) {
    if (lib == NULL)
        return;
    if (lib->handle != NULL)
        (void)dlclose(lib->handle);
    memset(lib, 0, sizeof(*lib));
}


int mgl_library_preload_system_vulkan(struct mgl_library *lib, const char *absolute_path) {
    void *handle;
    if (lib == NULL)
        return 0;
    memset(lib, 0, sizeof(*lib));
    if (absolute_path == NULL || absolute_path[0] != '/')
        return 0;
    handle = dlopen(absolute_path, RTLD_NOW | RTLD_GLOBAL);
    if (handle == NULL)
        return 0;
    if (dlsym(handle, "vkGetInstanceProcAddr") == NULL) {
        (void)dlclose(handle);
        return 0;
    }
    lib->handle = handle;
    (void)snprintf(lib->path, sizeof(lib->path), "%s", absolute_path);
    return 1;
}

int mgl_library_system_vulkan_preempted(const struct mgl_library *lib) {
    void *intended;
    void *global;
    if (lib == NULL || lib->handle == NULL)
        return 0;
    intended = dlsym(lib->handle, "vkGetInstanceProcAddr");
    global = dlsym(RTLD_DEFAULT, "vkGetInstanceProcAddr");
    return intended != NULL && global != NULL && intended != global;
}

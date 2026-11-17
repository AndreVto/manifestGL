#include "native/loader.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

typedef int (*test_angle_identity_t)(void);

int main(int argc, char **argv) {
    struct mgl_library sys;
    void *wrapper;
    void *angle;
    test_angle_identity_t probe = NULL;
    int global_wrapper;
    if (argc != 5 && argc != 6)
        return 2;
    global_wrapper = argc == 6 && strcmp(argv[5], "--global-wrapper") == 0;
    if (mgl_library_preload_system_vulkan(&sys, "libvulkan.so"))
        return 3;
    if (sys.handle != NULL)
        return 4;
    if (mgl_library_preload_system_vulkan(&sys, argv[4]))
        return 5;
    if (sys.handle != NULL)
        return 6;

    wrapper = dlopen(argv[2], RTLD_NOW | (global_wrapper ? RTLD_GLOBAL : RTLD_LOCAL));
    if (wrapper == NULL) {
        fprintf(stderr, "mock wrapper did not load: %s\n", dlerror());
        return 7;
    }

    /* Preloading the real Vulkan loader must detect a wrapper exporting Vulkan symbols globally. */
    if (!mgl_library_preload_system_vulkan(&sys, argv[1])) {
        fprintf(stderr, "system Vulkan preload failed\n");
        return 8;
    }
    if (strcmp(sys.path, argv[1]) != 0)
        return 9;
    if (mgl_library_symbol(&sys, "vkGetInstanceProcAddr") == NULL)
        return 10;
    if (mgl_library_system_vulkan_preempted(&sys) != global_wrapper) {
        fprintf(stderr, "Vulkan preemption detection failed\n");
        return 14;
    }
    angle = dlopen(argv[3], RTLD_NOW | RTLD_LOCAL);
    if (angle == NULL) {
        fprintf(stderr, "ANGLE lookup failed: %s\n", dlerror());
        return 11;
    }
    *(void **)(&probe) = dlsym(angle, "mgl_test_angle_vulkan_identity");
    if (probe == NULL || probe() != (global_wrapper ? 0x57524b : 0x41564b)) {
        fprintf(stderr, "ANGLE dependency ordering unexpected\n");
        return 12;
    }
    (void)dlclose(angle);
    mgl_library_close(&sys);
    if (sys.handle != NULL || sys.path[0] != '\0')
        return 13;
    (void)dlclose(wrapper);
    return 0;
}

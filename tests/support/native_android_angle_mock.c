
extern int vkGetInstanceProcAddr(void);

int mgl_test_angle_vulkan_identity(void) {
    return vkGetInstanceProcAddr();
}

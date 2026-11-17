#include <stdint.h>
#include <stddef.h>

static uintptr_t current_window = ((uintptr_t)0x1000u);
static unsigned acquire_count;
static unsigned peek_count;
static unsigned release_count;
static unsigned retire_count;
static uint64_t last_hwnd;
static uint64_t last_owner;
static uint32_t last_drawable;
static uint32_t last_whole;

void *android_direct_present_acquire_gl(uint64_t hwnd, uint32_t drawable, uint32_t whole,
                                        uint64_t owner, uint32_t timeout_ms) {
    (void)timeout_ms;
    ++acquire_count;
    last_hwnd = hwnd;
    last_drawable = drawable;
    last_whole = whole;
    last_owner = owner;
    return (void *)current_window;
}

void *android_direct_present_peek(uint64_t hwnd, uint64_t owner) {
    ++peek_count;
    last_hwnd = hwnd;
    last_owner = owner;
    return (void *)current_window;
}

void android_direct_present_release(void *window) {
    if (window != NULL)
        ++release_count;
}

void android_direct_present_retire(uint64_t hwnd, uint64_t owner) {
    ++retire_count;
    last_hwnd = hwnd;
    last_owner = owner;
}

void mgl_test_bridge_set_window(uintptr_t window) {
    current_window = window;
}

unsigned mgl_test_bridge_acquire_count(void) {
    return acquire_count;
}

unsigned mgl_test_bridge_peek_count(void) {
    return peek_count;
}

unsigned mgl_test_bridge_release_count(void) {
    return release_count;
}

unsigned mgl_test_bridge_retire_count(void) {
    return retire_count;
}

uint64_t mgl_test_bridge_last_hwnd(void) {
    return last_hwnd;
}

uint64_t mgl_test_bridge_last_owner(void) {
    return last_owner;
}

uint32_t mgl_test_bridge_last_drawable(void) {
    return last_drawable;
}

uint32_t mgl_test_bridge_last_whole(void) {
    return last_whole;
}

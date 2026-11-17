#include "core/runtime.h"

#include <pthread.h>

static struct mgl_native_dispatch native;
static pthread_once_t once = PTHREAD_ONCE_INIT;

static void initialize_native(void) {
    (void)mgl_native_dispatch_init(&native);
}

struct mgl_native_dispatch *mgl_runtime_native_dispatch(void) {
    (void)pthread_once(&once, initialize_native);
    return &native;
}

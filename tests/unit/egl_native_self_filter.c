#include "native/loader.h"

#include <stdio.h>

int main(int argc, char **argv) {
    struct mgl_library lib;
    const char *paths[1];

    if (argc != 3)
        return 2;
    paths[0] = argv[1];
    if (mgl_library_open_filtered(&lib, NULL, paths, 1, "mgl_public_egl_interface")) {
        fprintf(stderr, "public EGL was accepted as native\n");
        mgl_library_close(&lib);
        return 1;
    }
    if (!mgl_library_open(&lib, NULL, paths, 1)) {
        fprintf(stderr, "public EGL did not open unfiltered\n");
        return 1;
    }
    if (!mgl_library_symbol(&lib, "mgl_public_egl_interface")) {
        fprintf(stderr, "public EGL marker missing\n");
        mgl_library_close(&lib);
        return 1;
    }
    mgl_library_close(&lib);

    paths[0] = argv[2];
    if (!mgl_library_open_filtered(&lib, NULL, paths, 1, "mgl_public_egl_interface")) {
        fprintf(stderr, "distinct native EGL native was rejected\n");
        return 1;
    }
    if (mgl_library_symbol(&lib, "mgl_public_egl_interface") != NULL) {
        fprintf(stderr, "fake native native unexpectedly carries public marker\n");
        mgl_library_close(&lib);
        return 1;
    }
    if (mgl_library_symbol(&lib, "eglInitialize") == NULL) {
        fprintf(stderr, "fake native native missing eglInitialize\n");
        mgl_library_close(&lib);
        return 1;
    }
    mgl_library_close(&lib);
    return 0;
}

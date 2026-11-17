#ifndef MGL_CORE_VERSION_H
#define MGL_CORE_VERSION_H

#define MGL_PROJECT_VERSION_STRING "0.1.0-dev"

#define MGL_OPENGL_VERSION_MAJOR 4
#define MGL_OPENGL_VERSION_MINOR 6
#define MGL_OPENGL_VERSION_STRING "4.6"
#define MGL_GLSL_VERSION_STRING "4.60"

#define MGL_OPENGL_VERSION_IDENTITY                                                                \
    MGL_OPENGL_VERSION_STRING " ManifestGL " MGL_PROJECT_VERSION_STRING
#define MGL_GLSL_VERSION_IDENTITY MGL_GLSL_VERSION_STRING " ManifestGL"

static inline int mgl_opengl_version_supported(int major, int minor) {
    if (minor < 0)
        return 0;
    switch (major) {
        case 1:
            return minor <= 5;
        case 2:
            return minor <= 1;
        case 3:
            return minor <= 3;
        case 4:
            return minor <= MGL_OPENGL_VERSION_MINOR;
        default:
            return 0;
    }
}

#endif

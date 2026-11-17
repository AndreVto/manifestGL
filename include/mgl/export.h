#ifndef MGL_EXPORT_H
#define MGL_EXPORT_H

#if defined(_WIN32)
#if defined(MGL_BUILDING_LIBRARY)
#define MGL_EXPORT __declspec(dllexport)
#else
#define MGL_EXPORT __declspec(dllimport)
#endif
#else
#define MGL_EXPORT __attribute__((visibility("default")))
#endif

#endif

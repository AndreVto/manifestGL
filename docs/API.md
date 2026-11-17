# Public API

ManifestGL exports desktop OpenGL and GLX through `libGL.so.1`, and an EGL interface through `libEGL.so.1`. Public declarations are in `include/mgl/`.

| Implementation | Responsibility |
|---|---|
| `src/abi/gl_exports.c` | Core GL entry points |
| `src/abi/proc_address.c` | GL and GLX procedure address lookup |
| `src/glx/glx_exports.c` | GLX context, drawable, and window-system operations |
| `src/egl/egl_interface.c` | Public EGL operations |
| `src/core/capabilities.c` | Advertised GL extension list |
| `src/abi/unimplemented_exports.c` | Exported unsupported entry points that report `GL_INVALID_OPERATION` with diagnostic output |

`mgl_runtime_services_get()` is exported by `libGL` for the companion public EGL library. The two libraries are built and deployed together. The optional native-window registration ABI is specified in [Presentation](PRESENTATION.md).

Interface verification commands and behavioral tests are described in [Testing](TESTING.md).

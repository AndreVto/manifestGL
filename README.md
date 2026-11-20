# ManifestGL

ManifestGL (mGL) is an experimental desktop OpenGL 4.6 compatibility layer with GLX over an EGL/OpenGL ES 3.2 runtime. The reported version does not imply full OpenGL 4.6 conformance.
mGL maintains application-visible OpenGL state, realizes rendering commands for GLES, and dispatches native EGL/GLES operations.

| Shared library | Interface | ELF SONAME |
|---|---|---|
| `libGL` | Desktop OpenGL and GLX | `libGL.so.1` |
| `libEGL` | Public EGL API | `libEGL.so.1` |

The libraries share context and presentation operations through `mgl_runtime_services_get()`. Public declarations are under `include/mgl/`; the implementation is under `src/`.

## Build and test

```sh
python3 tools/generate_gles_trace.py
cmake -S . -B build-host -DMGL_BUILD_TESTS=ON
cmake --build build-host -j
ctest --test-dir build-host --output-on-failure
```

Host builds require X11 development headers and libxcb. The required glslang and SPIRV-Cross sources are provided in `third_party/`. Shader compilation uses source-based GLSL translation and the GLSL → SPIR-V → GLSL ES path. Android ARM64 libraries are built with `tools/build_android_arm64.sh`.

## Technical reference

- [Architecture](docs/ARCHITECTURE.md) — API interface, canonical runtime, state realization, native dispatch, and presentation.
- [Public API](docs/API.md) — exports, procedure resolution, and library integration.
- [Compatibility](docs/COMPATIBILITY.md) — translation trade-offs, extension-dependent behavior, and known limitations.
- [Presentation](docs/PRESENTATION.md) — window registration, native-window acquisition, and resource lifetime.
- [Android build](docs/BUILD_ANDROID.md) — toolchain, configuration, and packaging.
- [Diagnostics](docs/DIAGNOSTICS.md) — runtime options, errors, and timing output.
- [Testing](docs/TESTING.md) — host tests, desktop OpenGL comparisons, and native GLES probes.
- [Third-party dependencies](docs/THIRD_PARTY.md) — vendored sources and build dependencies.

## Development and references

ManifestGL was developed with AI assistance.

ManifestGL uses the [Khronos OpenGL and OpenGL ES specifications](https://registry.khronos.org/OpenGL/) as the authoritative references for API behavior. The [Mesa 3D Graphics Library](https://gitlab.freedesktop.org/mesa/mesa) is also consulted as an independent implementation reference for validation, edge cases, and compatibility behavior.

# Testing

## Host suite

```sh
python3 tools/generate_gles_trace.py
cmake -S . -B build-host \
  -DMGL_BUILD_TESTS=ON \
  -DMGL_WARNINGS_AS_ERRORS=ON
cmake --build build-host -j
ctest --test-dir build-host --output-on-failure
```

The glslang/SPIRV-Cross translation path and its associated tests are built with the required shader toolchain. The source-based GLSL compatibility translator is also exercised by the host suite.

## Desktop OpenGL conformance comparisons

```sh
ctest --test-dir build-host -R mesa_conformance --output-on-failure
```

Tests under `tests/conformance/` run common OpenGL behavior cases in two executables. Files named `*_manifestgl.c` link to `mgl_test_runtime`, which can use mocked native dispatch. Files named `*_mesa_reference.c` link against the desktop OpenGL library found by CMake. `compare_conformance_traces.py` executes both and checks exit codes and exact standard-output traces.

Mesa trace comparisons require Python 3, a desktop OpenGL library, and `xvfb-run`. Enabling `MGL_ENABLE_REAL_GLES_PROBE_CTEST` also registers the shader-object alias comparison when the desktop reference dependencies are available. The reference process runs under Xvfb with `LIBGL_ALWAYS_SOFTWARE=1` and without the inherited `LD_LIBRARY_PATH`. The system's `libGL` selection determines the reference implementation. A Mesa software renderer is the intended reference environment.

The traces cover selected API state, errors, and returned data. They do not compare rendered framebuffer images or establish native Android presentation behavior.

## Native GLES integration

`MGL_ENABLE_REAL_GLES_PROBE_CTEST=ON` registers the `gles_texture_framebuffer_probe` test. It requires a functional surfaceless EGL/GLES provider on the host and exercises shader compilation, texture uploads/readback, framebuffer operations, and draw behavior. It does not create Android windows or test SurfaceControl presentation.

For Android, build the libraries with:

```sh
sh tools/build_android_arm64.sh android
sh tools/build_android_arm64.sh package
```

Device testing must exercise the actual native provider, context binding, registered window surfaces, swapping, window replacement, and presentation recovery. The Android build helper verifies payload SONAMEs, source fingerprints, and package contents; it does not run device graphics tests.

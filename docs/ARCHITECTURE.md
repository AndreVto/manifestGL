# Architecture

ManifestGL implements OpenGL and GLX entry points over an EGL/OpenGL ES 3.2 execution environment. `libGL.so.1` owns the shared runtime. `libEGL.so.1` accesses context and presentation services through the exported `mgl_runtime_services_get()` table in `src/core/runtime_services.c`.

## Components

| Component | Sources | Responsibility |
|---|---|---|
| API Interface | `src/abi/`, `src/glx/`, `src/egl/` | Entry points, validation, error reporting, and procedure lookup |
| Canonical Runtime | `src/core/`, `src/state/`, `src/objects/` | Context binding, application-visible state, object identity, and shared resource lifetime |
| Compatibility | `src/compat/`, `src/list/`, `src/texture/` | Fixed-function operations, immediate mode, display lists, texture formats, and data conversion |
| State Realization | `src/vertex/`, `src/shader/`, `src/compiler/` | Native vertex input, shader translation, program selection, uniform upload, and draw preparation |
| Native Dispatch | `src/native/` | Provider loading, EGL/GLES function resolution, and driver calls |
| Presentation | `src/presentation/`, `src/glx/`, `src/egl/` | Drawables, native-window acquisition, surfaces, frame submission, and retirement |

## Contexts and state

`struct mgl_context` owns per-context OpenGL state. Its `canonical_state` member contains raster state; additional bindings and compatibility state are stored in domain-specific context fields. `struct mgl_share_group` owns objects shared between compatible contexts. Context binding and teardown are implemented in `src/core/context.c`.

The runtime tracks native GLES state to reduce redundant calls. `native_texture_names`, `native_active_texture`, and `native_buffer_bindings` cache the bindings submitted through native dispatch. State realization updates those values as it configures GLES. Context transitions and direct native operations invalidate affected cache entries when subsequent operations must re-establish the bindings.

## Shader and draw realization

`src/shader/translator.c` supports source-based desktop GLSL compatibility translation alongside a glslang → SPIR-V → SPIRV-Cross → GLSL ES compiler path. Shader translation adapts legacy shadow lookups and rectangle-texture coordinates. Fixed-function and ARB program compatibility uses generated or translated shaders. Draw realization prepares vertex attributes, index data, shaders, and uniforms before submitting GLES commands.

The OpenGL identity is defined in `src/core/version.h`: experimental OpenGL 4.6 and GLSL 4.60. Advertised extensions are enumerated in `src/core/capabilities.c`. GL vendor and renderer queries forward to the active GLES provider; context creation requests GLES 3.2 and validates the reported version in `src/core/context.c`.

## Provider dispatch

`struct mgl_native_dispatch` in `src/native/dispatch.h` contains EGL/GLES function pointers and provider-library handles. `src/native/loader.c` implements dynamic loading; `src/native/dispatch.c` resolves the required and optional functions. `MGL_EGL_LIBRARY` and `MGL_GLES_LIBRARY` are first-choice provider paths; if opening an override fails, the loader tries its built-in candidates.

The public `libEGL` implementation is excluded from native provider selection using its `mgl_public_egl_interface` export. This keeps native GLES execution routed through a provider that supplies the required EGL entry points.

## Presentation lifecycle

The GLX and EGL interfaces manage drawable and surface handles. The presentation subsystem associates registered windows with native resources, acquires or replaces surfaces, submits frames, and retires resources when owners are destroyed. Direct Android presentation uses host callback symbols resolved by `src/presentation/android_direct.c`; the AHardwareBuffer path is implemented in `src/presentation/android_ahb_presentation.c`.

See [Presentation](PRESENTATION.md) for the registration ABI and lifecycle, and [Compatibility](COMPATIBILITY.md) for supported translation behavior.

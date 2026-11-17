# Native presentation

ManifestGL supports GLX window drawables, GLX pbuffers, and public EGL window surfaces. The public EGL interface exposes one configuration and does not support pixmap surfaces. The Android presentation path can acquire an `ANativeWindow` from a host-provided callback table. `src/presentation/android_direct.c` manages direct-window acquisition; `src/presentation/android_ahb_presentation.c` implements AHardwareBuffer-backed submission and optional recording handoff.

## Window registration ABI

`libGL` exports:

```c
int wine_android_register_glx_window(void *display,
                                    unsigned long drawable,
                                    uint64_t hwnd,
                                    uint32_t whole_window);
```

`libEGL` exports:

```c
int wine_android_register_egl_window(void *display,
                                    void *surface,
                                    uint64_t hwnd,
                                    uint32_t whole_window);
```

The functions return nonzero for a successful registration. GLX requires an existing GLX window drawable. EGL requires an initialized X11-platform window surface. Registration associates the surface with a presentation owner and native window identity. The exported symbol spellings are part of the integration ABI.

## Direct-window callback resolution

`src/presentation/android_direct.c` resolves the following callback symbols:

```text
android_direct_present_acquire_gl
android_direct_present_peek
android_direct_present_release
android_direct_present_retire
```

When `WINE_ANDROID_DIRECT_PRESENT_LIB` names a library, the callbacks are resolved from that library. Otherwise the loader resolves them from the process's existing exported symbols. All four callbacks are required for direct presentation.

Presentation owner identifiers begin in the `0x474c` namespace. Each acquired window reference is associated with its owner generation and released when the generation is replaced or retired.

## Window and surface lifetime

1. The GLX or EGL registration function associates the drawable with a native window identity.
2. Presentation acquires a native window and creates or updates the EGL rendering resources for the active drawable.
3. Rendering uses GLES through native dispatch. The selected presentation path submits the completed frame.
4. Window loss or replacement triggers resource teardown, recovery, or recreation as appropriate for the active path.
5. Drawable destruction retires the owner and releases its native-window references.

AHardwareBuffer presentation uses SurfaceControl transactions and native release fences for buffer reuse. Optional recording handoff uses callbacks and shared control state supplied by the host.

## Environment variables

| Variable | Effect |
|---|---|
| `WINE_ANDROID_DIRECT_PRESENT_LIB` | Shared library that exports the direct-window callback symbols |
| `WINE_ANDROID_DIRECT_PRESENT` | Disables direct presentation for `0`, `false`, `off`, or `no` |
| `WINE_ANDROID_GL_SWAP_INTERVAL` | Nonnegative swap-interval override for direct presentation |
| `WINE_ANDROID_PRESENT_CONTROL_SHM` | Path to the shared presentation/recording control mapping |
| `WRAPPER_MAX_IMAGE_COUNT` | AHardwareBuffer image count, default `5`; values `1`–`2` use `3`, `3`–`64` are accepted, and zero, invalid, or values above `64` use the default |

The registration and callback interfaces are integration points for environments that provide native Android windows. For an unregistered X11 window bound to the current GLX context, double-buffered swaps fall back to CPU `glReadPixels` followed by `XPutImage` when the window has a TrueColor or DirectColor visual. This fallback does not implement GLX buffer-sharing semantics. GLX pbuffer rendering uses the offscreen context path.

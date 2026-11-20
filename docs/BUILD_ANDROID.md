# Android ARM64 build

`tools/build_android_arm64.sh` cross-compiles ManifestGL for Android/Bionic, stages `libGL` and `libEGL`, and optionally creates a `.wcp` archive. Run it from a Linux or WSL environment with the Android NDK installed.

## Requirements

- Android NDK with platform API level 26 or newer. The default path is `$HOME/Android/Sdk/ndk/27.3.13750724`.
- CMake, Make, Python 3, curl, tar, xz, zstd, pkg-config, Autotools, and a host C/C++ toolchain.
- Android/Bionic X11 libraries and `libandroid-shmem`. The helper builds these dependencies when the configured prefix is incomplete; `MGL_X11_INCLUDE_DIR` and `MGL_X11_LIBRARY` can select a prebuilt cross-compilation installation.

## Commands

```sh
sh tools/build_android_arm64.sh config
sh tools/build_android_arm64.sh all
```

| Target | Action |
|---|---|
| `config` | Print resolved build settings |
| `x11` | Build the Android X11 dependency stack |
| `android` | Build and stage the two shared libraries |
| `package` | Verify staged binaries and create the component archive |
| `all` | Prepare dependencies, build libraries, and package |
| `clean` | Remove the configured build directory |

The `android` and `all` targets generate `src/native/dispatch_trace.generated.inc` from the GLES dispatch declarations before invoking CMake.

## Build outputs

The default staging directory is `build-android-arm64/output/`:

```text
libGL.so.1.7.0
libEGL.so.1.0.0
components/opengl-manifestgl-0.1.0-dev.wcp
```

The `.wcp` file is produced by the `package` and `all` targets. It is a zstd-compressed tar archive containing `profile.json`, `usr/lib/libGL.so.1.7.0`, and `usr/lib/libEGL.so.1.0.0`. The profile declares type `OpenGL`, a version name/code, and the library destinations. The payloads use ELF SONAMEs `libGL.so.1` and `libEGL.so.1`.

The `android` target records SHA-256 hashes of its staged binaries and a fingerprint of the implementation sources, public headers, third-party files, CMake configuration, and build helper. `package` verifies those records and checks the extracted archive contents. Changes to these build inputs require rebuilding the libraries before packaging.

## Configuration

| Environment variable | Default or role |
|---|---|
| `ANDROID_NDK_HOME` | `$HOME/Android/Sdk/ndk/27.3.13750724` |
| `MGL_NDK_HOST_TAG` | `linux-x86_64`; selects the NDK prebuilt toolchain host directory |
| `MGL_ANDROID_PLATFORM` | `android-26` |
| `MGL_ANDROID_BUILD_TYPE` | `Release` |
| `MGL_JOBS` | `4` |
| `MGL_ANDROID_BUILD_DIR` | `build-android-arm64/` |
| `MGL_ANDROID_DEPS_DIR` | Dependency source and build directory |
| `MGL_X11_PREFIX` | Android X11 dependency installation |
| `MGL_X11_INCLUDE_DIR`, `MGL_X11_LIBRARY` | Explicit X11 header and library paths; set both |
| `MGL_ANDROID_OUTPUT_DIR` | Staged library output directory |
| `MGL_OPENGL_PACKAGE_DIR` | `.wcp` output directory |
| `MGL_OPENGL_VERSION_NAME` | `manifestgl-0.1.0-dev` |
| `MGL_OPENGL_VERSION_CODE` | Current Unix epoch seconds; must fit a positive signed 32-bit integer |

## Runtime requirements

The libraries require compatible Android/Bionic X11 dependencies and native EGL/GLES providers. Provider selection checks `MGL_EGL_LIBRARY` and `MGL_GLES_LIBRARY` before trying built-in library candidates. Device validation is required for GLES context creation, window registration, and frame presentation.

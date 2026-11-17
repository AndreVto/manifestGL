# Third-party dependencies

The required GLSL/SPIR-V compiler toolchain builds the following vendored sources:

| Dependency | Use | License location |
|---|---|---|
| glslang | Desktop GLSL parsing and SPIR-V generation | `third_party/glslang/LICENSE.txt` |
| SPIRV-Cross | SPIR-V translation to GLSL ES | `third_party/SPIRV-Cross/LICENSE`, `third_party/SPIRV-Cross/LICENSES/` |

`third_party/bcdec.h` provides block-compressed texture decoding; its attribution and license text are retained in the header.

The Android build helper downloads and builds X11 dependencies and `libandroid-shmem` from the release versions specified in `tools/build_android_arm64.sh`. Their licenses are provided by the corresponding upstream distributions.

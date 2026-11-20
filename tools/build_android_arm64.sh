#!/usr/bin/env sh
set -eu

ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/27.3.13750724}"
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${MGL_ANDROID_BUILD_DIR:-"$ROOT/build-android-arm64"}
PLATFORM=${MGL_ANDROID_PLATFORM:-android-26}
BUILD_TYPE=${MGL_ANDROID_BUILD_TYPE:-Release}
JOBS=${MGL_JOBS:-4}

case "$PLATFORM" in
    android-*) API=${PLATFORM#android-} ;;
    *) API=$PLATFORM; PLATFORM="android-$PLATFORM" ;;
esac
case "$API" in
    ''|*[!0-9]*) echo "ERROR: MGL_ANDROID_PLATFORM must be android-N or N" >&2; exit 1 ;;
esac
[ "$API" -ge 26 ] || { echo "ERROR: Android API level must be 26 or newer" >&2; exit 1; }

NDK_HOST_TAG=${MGL_NDK_HOST_TAG:-linux-x86_64}
TOOLCHAIN_BIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$NDK_HOST_TAG/bin"
CC="$TOOLCHAIN_BIN/aarch64-linux-android${API}-clang"
CXX="$TOOLCHAIN_BIN/aarch64-linux-android${API}-clang++"

DEPS_DIR=${MGL_ANDROID_DEPS_DIR:-"$BUILD_DIR/deps"}
DOWNLOAD_DIR="$DEPS_DIR/downloads"
WORK_DIR="$DEPS_DIR/work"
X11_PREFIX=${MGL_X11_PREFIX:-"$DEPS_DIR/x11-prefix"}
OUTPUT_DIR=${MGL_ANDROID_OUTPUT_DIR:-"$BUILD_DIR/output"}
PRODUCT_DIR="$BUILD_DIR/mgl-products"
PACKAGE_DIR=${MGL_OPENGL_PACKAGE_DIR:-"$OUTPUT_DIR/components"}
VERSION_NAME=${MGL_OPENGL_VERSION_NAME:-manifestgl-0.1.0-dev}
VERSION_CODE=${MGL_OPENGL_VERSION_CODE:-$(date +%s)}
PAYLOAD_NAME=libGL.so.1.7.0
EGL_PAYLOAD_NAME=libEGL.so.1.0.0

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

have() { command -v "$1" >/dev/null 2>&1; }

check_build_deps() {
    missing=""
    for cmd in curl tar xz zstd cmake make pkg-config python3 sed grep sha256sum; do
        have "$cmd" || missing="$missing $cmd"
    done
    [ -z "$missing" ] || fail "Missing host commands:$missing\nOn Ubuntu/WSL2 install: curl xz-utils zstd cmake make pkg-config python3 autoconf automake libtool"
    [ -f "$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" ] || fail "Android NDK CMake toolchain not found under $ANDROID_NDK_HOME"
    [ -x "$CC" ] || fail "Android NDK compiler not found: $CC"
    [ -x "$CXX" ] || fail "Android NDK C++ compiler not found: $CXX"
}

check_package_deps() {
    missing=""
    for cmd in tar zstd python3 sed sha256sum; do
        have "$cmd" || missing="$missing $cmd"
    done
    [ -z "$missing" ] || fail "Missing package commands:$missing"
}

fetch() {
    url=$1
    dest=$2
    [ -s "$dest" ] && return 0
    mkdir -p "$(dirname -- "$dest")"
    printf '  Downloading %s\n' "$(basename -- "$dest")"
    curl -L --fail --retry 3 "$url" -o "$dest.tmp"
    mv "$dest.tmp" "$dest"
}

extract_once() {
    archive=$1
    dest=$2
    marker="$dest/.mgl-extract-ok"
    [ -f "$marker" ] && return 0
    rm -rf "$dest"
    mkdir -p "$WORK_DIR"
    tar -xf "$archive" -C "$WORK_DIR"
    [ -d "$dest" ] || fail "Expected extracted directory not found: $dest"
    : > "$marker"
}

x11_pkgdir() {
    printf '%s:%s\n' "$X11_PREFIX/lib/pkgconfig" "$X11_PREFIX/share/pkgconfig"
}

cross_configure() {
    src=$1
    prefix=$2
    shift 2
    [ -x "$src/configure" ] || fail "configure script not found: $src/configure"
    CC="$CC" \
    CXX="$CXX" \
    AR="$TOOLCHAIN_BIN/llvm-ar" \
    RANLIB="$TOOLCHAIN_BIN/llvm-ranlib" \
    STRIP="$TOOLCHAIN_BIN/llvm-strip" \
    PKG_CONFIG="$(command -v pkg-config)" \
    PKG_CONFIG_LIBDIR="$(x11_pkgdir)" \
    PKG_CONFIG_SYSROOT_DIR="" \
    CFLAGS="${CFLAGS:-} -fPIC -Wno-error -Wno-implicit-function-declaration" \
    CPPFLAGS="${CPPFLAGS:-}" \
    "$src/configure" \
        --prefix="$prefix" \
        --host=aarch64-linux-android \
        --build=x86_64-linux-gnu \
        --enable-shared \
        --disable-static \
        "$@"
}

build_util_macros() {
    ver=1.20.0
    src="$WORK_DIR/util-macros-$ver"
    archive="$DOWNLOAD_DIR/util-macros-$ver.tar.xz"
    fetch "https://xorg.freedesktop.org/archive/individual/util/util-macros-$ver.tar.xz" "$archive"
    extract_once "$archive" "$src"
    rm -rf "$src/build-android" && mkdir -p "$src/build-android"
    (cd "$src/build-android" && "$src/configure" --prefix="$X11_PREFIX" --host=aarch64-linux-android --build=x86_64-linux-gnu && make -j"$JOBS" install)
}

build_xorgproto() {
    ver=2024.1
    src="$WORK_DIR/xorgproto-$ver"
    archive="$DOWNLOAD_DIR/xorgproto-$ver.tar.xz"
    fetch "https://xorg.freedesktop.org/archive/individual/proto/xorgproto-$ver.tar.xz" "$archive"
    extract_once "$archive" "$src"
    rm -rf "$src/build-android" && mkdir -p "$src/build-android"
    (cd "$src/build-android" && \
        PKG_CONFIG="$(command -v pkg-config)" PKG_CONFIG_LIBDIR="$(x11_pkgdir)" PKG_CONFIG_SYSROOT_DIR="" \
        "$src/configure" --prefix="$X11_PREFIX" --datarootdir="$X11_PREFIX/share" \
            --host=aarch64-linux-android --build=x86_64-linux-gnu \
            --without-xmlto --without-fop --without-xsltproc && \
        make -j"$JOBS" install)
}

build_xtrans() {
    ver=1.5.2
    src="$WORK_DIR/xtrans-$ver"
    archive="$DOWNLOAD_DIR/xtrans-$ver.tar.xz"
    fetch "https://xorg.freedesktop.org/releases/individual/lib/xtrans-$ver.tar.xz" "$archive"
    extract_once "$archive" "$src"
    rm -rf "$src/build-android" && mkdir -p "$src/build-android"
    (cd "$src/build-android" && \
        PKG_CONFIG="$(command -v pkg-config)" PKG_CONFIG_LIBDIR="$(x11_pkgdir)" PKG_CONFIG_SYSROOT_DIR="" \
        "$src/configure" --prefix="$X11_PREFIX" --host=aarch64-linux-android --build=x86_64-linux-gnu \
            --without-xmlto --without-fop --without-xsltproc && \
        make -j"$JOBS" install)
}

build_xcb_proto() {
    ver=1.17.0
    src="$WORK_DIR/xcb-proto-$ver"
    archive="$DOWNLOAD_DIR/xcb-proto-$ver.tar.xz"
    fetch "https://xcb.freedesktop.org/dist/xcb-proto-$ver.tar.xz" "$archive"
    extract_once "$archive" "$src"
    rm -rf "$src/build-android" && mkdir -p "$src/build-android"
    (cd "$src/build-android" && \
        PKG_CONFIG="$(command -v pkg-config)" PKG_CONFIG_LIBDIR="$(x11_pkgdir)" PKG_CONFIG_SYSROOT_DIR="" \
        "$src/configure" --prefix="$X11_PREFIX" --host=aarch64-linux-android --build=x86_64-linux-gnu \
            --datarootdir="$X11_PREFIX/share" && \
        make -j"$JOBS" install)
}

build_libxau() {
    ver=1.0.11
    src="$WORK_DIR/libXau-$ver"
    archive="$DOWNLOAD_DIR/libXau-$ver.tar.xz"
    fetch "https://xorg.freedesktop.org/archive/individual/lib/libXau-$ver.tar.xz" "$archive"
    extract_once "$archive" "$src"
    rm -rf "$src/build-android" && mkdir -p "$src/build-android"
    (cd "$src/build-android" && cross_configure "$src" "$X11_PREFIX" && make -j"$JOBS" install)
}

build_libxcb() {
    ver=1.17.0
    src="$WORK_DIR/libxcb-$ver"
    archive="$DOWNLOAD_DIR/libxcb-$ver.tar.xz"
    fetch "https://xcb.freedesktop.org/dist/libxcb-$ver.tar.xz" "$archive"
    extract_once "$archive" "$src"
    rm -rf "$src/build-android" && mkdir -p "$src/build-android"
    (cd "$src/build-android" && \
        XCBPROTO_XCBINCLUDEDIR="$X11_PREFIX/share/xcb" \
        cross_configure "$src" "$X11_PREFIX" \
            --disable-composite --disable-damage --disable-dpms --disable-dri2 --enable-dri3 \
            --disable-glx --enable-present --disable-record --disable-resource --disable-screensaver \
            --disable-xfree86-dri --disable-xinerama --disable-xinput --disable-xtest --disable-xv --disable-xvmc \
            --enable-randr --enable-render --enable-shape --enable-shm --enable-sync --enable-xfixes \
            --with-queue-size=32768 && \
        make -j"$JOBS" install)
}

build_libx11() {
    ver=1.8.10
    src="$WORK_DIR/libX11-$ver"
    archive="$DOWNLOAD_DIR/libX11-$ver.tar.xz"
    fetch "https://xorg.freedesktop.org/archive/individual/lib/libX11-$ver.tar.xz" "$archive"
    extract_once "$archive" "$src"
    rm -rf "$src/build-android" && mkdir -p "$src/build-android" "$X11_PREFIX/lib/stub"
    printf 'INPUT(-lc)\n' > "$X11_PREFIX/lib/stub/libpthread.so"
    (cd "$src/build-android" && \
        ac_cv_func_malloc_0_nonnull=yes ac_cv_func_realloc_0_nonnull=yes ac_cv_func_calloc_0_nonnull=yes \
        ac_cv_func_mmap_fixed_mapped=yes ac_cv_have_decl_getpwuid_r=yes cross_compiling=yes \
        LDFLAGS="-L$X11_PREFIX/lib/stub" \
        cross_configure "$src" "$X11_PREFIX" \
            --disable-xf86bigfont --disable-xlocale --disable-xlocaledir --disable-loadable-i18n \
            --disable-loadable-xcursor --disable-specs --without-xmlto --without-fop --without-xsltproc \
            --with-keysymdefdir="$X11_PREFIX/include/X11" --disable-malloc0returnsnull && \
        make -j"$JOBS" install)
}

build_libxext() {
    ver=1.3.6
    src="$WORK_DIR/libXext-$ver"
    archive="$DOWNLOAD_DIR/libXext-$ver.tar.xz"
    fetch "https://xorg.freedesktop.org/archive/individual/lib/libXext-$ver.tar.xz" "$archive"
    extract_once "$archive" "$src"
    rm -rf "$src/build-android" && mkdir -p "$src/build-android"
    (cd "$src/build-android" && LDFLAGS="-L$X11_PREFIX/lib/stub" \
        cross_configure "$src" "$X11_PREFIX" --without-xmlto --without-fop --without-xsltproc --disable-malloc0returnsnull && \
        make -j"$JOBS" install)
}

build_android_shmem() {
    ver=0.7
    src="$WORK_DIR/libandroid-shmem-$ver"
    archive="$DOWNLOAD_DIR/libandroid-shmem-$ver.tar.gz"
    fetch "https://github.com/termux/libandroid-shmem/archive/refs/tags/v$ver.tar.gz" "$archive"
    extract_once "$archive" "$src"
    mkdir -p "$X11_PREFIX/lib"
    compat="$WORK_DIR/libandroid-shmem-paths-compat.h"
    cat > "$compat" <<'EOF'
#include <paths.h>
#ifndef _PATH_TMP
#define _PATH_TMP "/tmp/"
#endif
EOF
    "$CC" -O2 -fPIC -shared -include "$compat" -I"$src" "$src/shmem.c" \
        -Wl,-soname,libandroid-shmem.so -Wl,--no-undefined -landroid -llog \
        -o "$X11_PREFIX/lib/libandroid-shmem.so"
}

x11_stack_ready() {
    [ -f "$X11_PREFIX/include/X11/Xlib.h" ] && \
    [ -f "$X11_PREFIX/lib/libX11.so" ] && \
    [ -f "$X11_PREFIX/lib/libXext.so" ] && \
    [ -f "$X11_PREFIX/lib/libxcb.so" ] && \
    [ -f "$X11_PREFIX/lib/libxcb-dri3.so" ] && \
    [ -f "$X11_PREFIX/lib/libxcb-present.so" ] && \
    [ -f "$X11_PREFIX/lib/libxcb-sync.so" ] && \
    [ -f "$X11_PREFIX/lib/libandroid-shmem.so" ]
}

build_x11_stack() {
    printf '%s\n' '==> Building Android/Bionic X11 dependency stack'
    mkdir -p "$DOWNLOAD_DIR" "$WORK_DIR" "$X11_PREFIX"
    build_util_macros
    build_xorgproto
    build_xtrans
    build_xcb_proto
    build_libxau
    build_libxcb
    build_libx11
    build_libxext
    build_android_shmem
    x11_stack_ready || fail "Android/Bionic X11 dependency build did not produce the expected libraries"
    printf '==> X11 prefix ready: %s\n' "$X11_PREFIX"
}

ensure_x11_stack() {
    if [ -n "${MGL_X11_INCLUDE_DIR:-}" ] || [ -n "${MGL_X11_LIBRARY:-}" ]; then
        [ -n "${MGL_X11_INCLUDE_DIR:-}" ] && [ -n "${MGL_X11_LIBRARY:-}" ] || fail "Set both MGL_X11_INCLUDE_DIR and MGL_X11_LIBRARY, or neither"
        [ -f "$MGL_X11_INCLUDE_DIR/X11/Xlib.h" ] || fail "X11 headers not found under MGL_X11_INCLUDE_DIR=$MGL_X11_INCLUDE_DIR"
        [ -f "$MGL_X11_LIBRARY" ] || fail "MGL_X11_LIBRARY not found: $MGL_X11_LIBRARY"
        return 0
    fi
    if ! x11_stack_ready; then
        build_x11_stack
    else
        printf '==> Reusing Android/Bionic X11 stack: %s\n' "$X11_PREFIX"
    fi
    MGL_X11_INCLUDE_DIR="$X11_PREFIX/include"
    MGL_X11_LIBRARY="$X11_PREFIX/lib/libX11.so"
    export MGL_X11_INCLUDE_DIR MGL_X11_LIBRARY
}

# CMake target output paths for the staged shared-library payloads.
GL_PRODUCT="$PRODUCT_DIR/$PAYLOAD_NAME"
EGL_PRODUCT="$PRODUCT_DIR/$EGL_PAYLOAD_NAME"

component_archive() {
    safe_version=$(printf '%s' "$VERSION_NAME" | tr -cs 'A-Za-z0-9._-' '-' | sed 's/^-//;s/-$//')
    [ -n "$safe_version" ] || safe_version=manifestgl
    printf '%s/opengl-%s.wcp\n' "$PACKAGE_DIR" "$safe_version"
}

# Fingerprint build inputs and payload hashes for package verification.
source_fingerprint() {
    find "$ROOT/src" "$ROOT/include" "$ROOT/third_party" "$ROOT/CMakeLists.txt" \
        "$ROOT/tools/build_android_arm64.sh" \
        -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum | sha256sum | cut -d ' ' -f 1
}

record_build_provenance() {
    {
        source_fingerprint
        sha256sum "$OUTPUT_DIR/$PAYLOAD_NAME" | cut -d ' ' -f 1
        sha256sum "$OUTPUT_DIR/$EGL_PAYLOAD_NAME" | cut -d ' ' -f 1
    } > "$OUTPUT_DIR/.mgl-build-provenance.tmp"
    mv "$OUTPUT_DIR/.mgl-build-provenance.tmp" "$OUTPUT_DIR/.mgl-build-provenance"
}

verify_build_provenance() {
    provenance="$OUTPUT_DIR/.mgl-build-provenance"
    [ -f "$provenance" ] || fail "Missing verified build provenance; run '$0 all' first"
    { IFS= read -r built_source || :; IFS= read -r built_gl || :; IFS= read -r built_egl || :; } < "$provenance"
    [ -n "$built_source" ] && [ -n "$built_gl" ] && [ -n "$built_egl" ] || fail "Malformed build provenance"
    [ "$built_source" = "$(source_fingerprint)" ] || fail "ManifestGL sources changed since payload build; run '$0 all' to rebuild"
    gl_hash=$(sha256sum "$OUTPUT_DIR/$PAYLOAD_NAME" | cut -d ' ' -f 1)
    egl_hash=$(sha256sum "$OUTPUT_DIR/$EGL_PAYLOAD_NAME" | cut -d ' ' -f 1)
    [ "$built_gl" = "$gl_hash" ] || fail "OpenGL payload changed since build; rebuild before packaging"
    [ "$built_egl" = "$egl_hash" ] || fail "EGL payload changed since build; rebuild before packaging"
}

read_soname() {
    lib=$1
    if [ -x "$TOOLCHAIN_BIN/llvm-readelf" ]; then
        re="$TOOLCHAIN_BIN/llvm-readelf"
    elif have readelf; then
        re=$(command -v readelf)
    else
        fail "No readelf implementation is available to verify the ELF SONAME"
    fi
    "$re" -d "$lib" 2>/dev/null | sed -n 's/.*(SONAME).*\[\(.*\)\].*/\1/p' | head -n 1
}

prepare_output_library() {
    lib="$GL_PRODUCT"
    [ -f "$lib" ] || fail "CMake target mgl did not produce $lib"
    mkdir -p "$OUTPUT_DIR"
    cp -L "$lib" "$OUTPUT_DIR/$PAYLOAD_NAME"
    if [ -x "$TOOLCHAIN_BIN/llvm-strip" ]; then
        "$TOOLCHAIN_BIN/llvm-strip" --strip-unneeded "$OUTPUT_DIR/$PAYLOAD_NAME" || true
    fi
    soname=$(read_soname "$OUTPUT_DIR/$PAYLOAD_NAME")
    [ "$soname" = "libGL.so.1" ] || fail "Unexpected SONAME: ${soname:-<none>} (expected libGL.so.1)"
    ln -sfn "$PAYLOAD_NAME" "$OUTPUT_DIR/libGL.so.1"
    ln -sfn libGL.so.1 "$OUTPUT_DIR/libGL.so"

    # The public EGL interface is distinct from mGL's system EGL/GLES native.
    egl_lib="$EGL_PRODUCT"
    [ -f "$egl_lib" ] || fail "CMake target mgl_egl did not produce $egl_lib"
    cp -L "$egl_lib" "$OUTPUT_DIR/$EGL_PAYLOAD_NAME"
    if [ -x "$TOOLCHAIN_BIN/llvm-strip" ]; then
        "$TOOLCHAIN_BIN/llvm-strip" --strip-unneeded "$OUTPUT_DIR/$EGL_PAYLOAD_NAME" || true
    fi
    egl_soname=$(read_soname "$OUTPUT_DIR/$EGL_PAYLOAD_NAME")
    [ "$egl_soname" = "libEGL.so.1" ] || fail "Unexpected EGL SONAME: ${egl_soname:-<none>} (expected libEGL.so.1)"
    ln -sfn "$EGL_PAYLOAD_NAME" "$OUTPUT_DIR/libEGL.so.1"
    ln -sfn libEGL.so.1 "$OUTPUT_DIR/libEGL.so"
}

build_mgl() {
    ensure_x11_stack
    mkdir -p "$BUILD_DIR"
    # Invalidate old staged copies immediately. A failed configure/build must
    # never leave a seemingly ready-to-package payload behind.
    rm -f "$OUTPUT_DIR/.mgl-build-provenance" \
        "$OUTPUT_DIR/$PAYLOAD_NAME" "$OUTPUT_DIR/$EGL_PAYLOAD_NAME" \
        "$(component_archive)"
    python3 "$ROOT/tools/generate_gles_trace.py"
    cmake -S "$ROOT" -B "$BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a \
        -DANDROID_PLATFORM="$PLATFORM" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DMGL_BUILD_TESTS=OFF \
        -DMGL_X11_INCLUDE_DIR="$MGL_X11_INCLUDE_DIR" \
        -DMGL_X11_LIBRARY="$MGL_X11_LIBRARY"
    rm -f "$GL_PRODUCT" "$EGL_PRODUCT"
    cmake --build "$BUILD_DIR" --target mgl_egl -j"$JOBS"
    prepare_output_library
    record_build_provenance
    printf '%s\n' "Built mGL in $BUILD_DIR"
    printf '%s\n' "Android OpenGL payload: $OUTPUT_DIR/$PAYLOAD_NAME"
    printf '%s\n' "Android public EGL payload: $OUTPUT_DIR/$EGL_PAYLOAD_NAME"
}

valid_version_code() {
    case "$VERSION_CODE" in ''|*[!0-9]*) return 1 ;; esac
    [ "$VERSION_CODE" -gt 0 ] && [ "$VERSION_CODE" -le 2147483647 ]
}

package_component() {
    # Clear the component output before package validation.
    archive=$(component_archive)
    rm -f "$archive"
    check_package_deps
    lib="$OUTPUT_DIR/$PAYLOAD_NAME"
    [ -f "$lib" ] || fail "OpenGL payload is missing: $lib (run '$0 android' first)"
    soname=$(read_soname "$lib")
    [ "$soname" = "libGL.so.1" ] || fail "Unexpected SONAME: ${soname:-<none>} (expected libGL.so.1)"
    egl_lib="$OUTPUT_DIR/$EGL_PAYLOAD_NAME"
    [ -f "$egl_lib" ] || fail "Public EGL payload is missing: $egl_lib (run '$0 android' first)"
    egl_soname=$(read_soname "$egl_lib")
    [ "$egl_soname" = "libEGL.so.1" ] || fail "Unexpected EGL SONAME: ${egl_soname:-<none>} (expected libEGL.so.1)"
    valid_version_code || fail "MGL_OPENGL_VERSION_CODE must be an integer between 1 and 2147483647"
    verify_build_provenance

    stage="$BUILD_DIR/component-stage"
    verify="$BUILD_DIR/component-verify"
    rm -rf "$stage" "$verify"
    mkdir -p "$stage/usr/lib" "$PACKAGE_DIR" "$verify"
    cp -L "$lib" "$stage/usr/lib/$PAYLOAD_NAME"
    cp -L "$egl_lib" "$stage/usr/lib/$EGL_PAYLOAD_NAME"

    python3 - "$stage/profile.json" "$VERSION_NAME" "$VERSION_CODE" "$PAYLOAD_NAME" "$EGL_PAYLOAD_NAME" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
version_name = sys.argv[2]
version_code = int(sys.argv[3])
payload = sys.argv[4]
egl_payload = sys.argv[5]
profile = {
    "type": "OpenGL",
    "versionName": version_name,
    "versionCode": version_code,
    "description": "ManifestGL Android OpenGL driver",
    "files": [
        {
            "source": f"usr/lib/{payload}",
            "target": "${libdir}/libGL.so.1.7.0",
        },
        {
            "source": f"usr/lib/{egl_payload}",
            "target": "${libdir}/libEGL.so.1.0.0",
        },
    ],
}
path.write_text(json.dumps(profile, indent=2) + "\n", encoding="utf-8")
PY

    archive=$(component_archive)
    tar --sort=name --owner=0 --group=0 --numeric-owner -C "$stage" -cf - profile.json usr \
        | zstd -q -19 -T0 -f -o "$archive"

    zstd -q -dc "$archive" | tar -xf - -C "$verify"
    python3 - "$verify" "$OUTPUT_DIR" "$PAYLOAD_NAME" "$EGL_PAYLOAD_NAME" <<'PY'
import hashlib, json, pathlib, sys
root = pathlib.Path(sys.argv[1])
output = pathlib.Path(sys.argv[2])
payload = sys.argv[3]
egl_payload = sys.argv[4]
p = json.loads((root / "profile.json").read_text(encoding="utf-8"))
assert p["type"] == "OpenGL"
assert len(p["files"]) == 2
assert p["files"][0]["source"] == f"usr/lib/{payload}"
assert p["files"][0]["target"] == "${libdir}/libGL.so.1.7.0"
assert p["files"][1]["source"] == f"usr/lib/{egl_payload}"
assert p["files"][1]["target"] == "${libdir}/libEGL.so.1.0.0"
for name in (payload, egl_payload):
    extracted = root / "usr/lib" / name
    produced = output / name
    assert extracted.is_file() and produced.is_file()
    assert hashlib.sha256(extracted.read_bytes()).digest() == hashlib.sha256(produced.read_bytes()).digest(), f"Packaged {name} does not match verified build output"
PY
    printf '==> OpenGL component ready: %s\n' "$archive"
    sha256sum "$archive"
}

show_config() {
    cat <<EOF
Source:         $ROOT
NDK:            $ANDROID_NDK_HOME
Platform:       $PLATFORM
Build type:     $BUILD_TYPE
Build dir:      $BUILD_DIR
Dependency dir: $DEPS_DIR
X11 prefix:     $X11_PREFIX
CMake products: $PRODUCT_DIR
Output:         $OUTPUT_DIR
Package dir:    $PACKAGE_DIR
Version name:   $VERSION_NAME
Version code:   $VERSION_CODE
EOF
}

usage() {
    cat <<EOF
Usage: $(basename "$0") [target]

Targets:
  all       Build Android/Bionic X11 dependencies if needed, build mGL, and create the .wcp package (default)
  x11       Build only the Android/Bionic X11 + shared-memory dependency stack
  android   Build mGL for Android ARM64 and prepare libGL.so.1.7.0
  package   Package verified, source-matched GL/EGL outputs (fails after source changes until rebuilt)
  config    Print resolved build/package configuration
  clean     Remove the Android build/dependency/output tree
  help      Show this help

Important environment overrides:
  ANDROID_NDK_HOME=$HOME/Android/Sdk/ndk/27.3.13750724
  MGL_ANDROID_PLATFORM=android-26
  MGL_ANDROID_BUILD_TYPE=Release
  MGL_ANDROID_BUILD_DIR=/path/to/build
  MGL_ANDROID_DEPS_DIR=/path/to/deps
  MGL_X11_PREFIX=/path/to/android-x11-prefix
  MGL_X11_INCLUDE_DIR=/path/to/android-x11/include
  MGL_X11_LIBRARY=/path/to/android-x11/lib/libX11.so
  MGL_ANDROID_OUTPUT_DIR=/path/to/output
  MGL_OPENGL_PACKAGE_DIR=/path/to/components
  MGL_OPENGL_VERSION_NAME=manifestgl-0.1.0-dev
  MGL_OPENGL_VERSION_CODE=123
  MGL_JOBS=4
EOF
}

target=${1:-all}
case "$target" in
    all)
        check_build_deps
        show_config
        ensure_x11_stack
        build_mgl
        package_component
        ;;
    x11)
        check_build_deps
        show_config
        build_x11_stack
        ;;
    android|build)
        check_build_deps
        show_config
        build_mgl
        ;;
    package)
        show_config
        package_component
        ;;
    config)
        show_config
        ;;
    clean)
        printf 'Removing: %s\n' "$BUILD_DIR"
        rm -rf "$BUILD_DIR"
        ;;
    help|-h|--help)
        usage
        ;;
    *)
        usage >&2
        fail "Unknown target: $target"
        ;;
esac

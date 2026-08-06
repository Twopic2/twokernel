#!/usr/bin/env bash
set -euo pipefail

# macOS ships bash 3.2, so everything below stays 3.2-compatible:
# no ${var^} capitalisation, and empty arrays expand via the ${a[@]+...}
# guard because a bare "${a[@]}" trips `set -u` before bash 4.4.

cd "$(dirname "${BASH_SOURCE[0]}")"

BUILD_TYPE="debug"
CMAKE_TYPE="Debug"
QEMU_FLAGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --release)  BUILD_TYPE="release"; CMAKE_TYPE="Release" ;;
        --debug)    BUILD_TYPE="debug";   CMAKE_TYPE="Debug" ;;
        # wait for a debugger to attach on port 1234
        --gdb)      QEMU_FLAGS+=(-s -S) ;;
        # log every interrupt/exception the CPU services to qemu.log
        --intlog)   QEMU_FLAGS+=(-d int -D qemu.log) ;;
        *) echo "unknown option: $1" >&2; exit 1 ;;
    esac
    shift
done

PRESET="general-${BUILD_TYPE}-build"
BUILD_DIR="build/${PRESET}"
ISO="$BUILD_DIR/twokernel.iso"

# CMakePresets.json defines both presets, so a missing tree is recoverable.
if [[ ! -d "$BUILD_DIR" ]]; then
    echo "==> configuring $PRESET"
    cmake --preset "$PRESET"
fi

# The directory name is only a convention -- confirm the cache really is
# the build type that was asked for.
if ! grep -q "^CMAKE_BUILD_TYPE:STRING=${CMAKE_TYPE}\$" "$BUILD_DIR/CMakeCache.txt"; then
    echo "$BUILD_DIR is not a ${CMAKE_TYPE} build" >&2
    exit 1
fi

echo "==> building $BUILD_TYPE ($BUILD_DIR)"
cmake --build "$BUILD_DIR" --target iso

echo "==> booting $BUILD_TYPE ($ISO)"
qemu-system-x86_64 \
    -cdrom "$ISO" \
    -m 256M \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    ${QEMU_FLAGS[@]+"${QEMU_FLAGS[@]}"}

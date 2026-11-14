#!/usr/bin/env bash
set -euo pipefail

# macOS ships bash 3.2, so everything below stays 3.2-compatible:
# no ${var^} capitalisation, and empty arrays expand via the ${a[@]+...}
# guard because a bare "${a[@]}" trips `set -u` before bash 4.4.

cd "$(dirname "${BASH_SOURCE[0]}")"

BUILD_TYPE="debug"
CMAKE_TYPE="Debug"
QEMU_FLAGS=()
TRACE=OFF

while [[ $# -gt 0 ]]; do
    case "$1" in
        --release)  BUILD_TYPE="release"; CMAKE_TYPE="Release" ;;
        --debug)    BUILD_TYPE="debug";   CMAKE_TYPE="Debug" ;;
        --trace)    TRACE=ON ;;
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
    cmake --preset "$PRESET" -DTWOKERNEL_ENABLE_TRACE="$TRACE"
fi

# Keep tracing opt-in even when this build directory last captured a trace.
if ! grep -q "^TWOKERNEL_ENABLE_TRACE:BOOL=${TRACE}\$" "$BUILD_DIR/CMakeCache.txt"; then
    cmake -S . -B "$BUILD_DIR" -DTWOKERNEL_ENABLE_TRACE="$TRACE"
fi

# The directory name is only a convention -- confirm the cache really is
# the build type that was asked for.
if ! grep -q "^CMAKE_BUILD_TYPE:STRING=${CMAKE_TYPE}\$" "$BUILD_DIR/CMakeCache.txt"; then
    echo "$BUILD_DIR is not a ${CMAKE_TYPE} build" >&2
    exit 1
fi

echo "==> building $BUILD_TYPE ($BUILD_DIR)"
cmake --build "$BUILD_DIR" --target iso

if [[ "$TRACE" == ON ]]; then
    if ! command -v python3 >/dev/null 2>&1; then
        echo "--trace requires python3 to convert the capture" >&2
        exit 1
    fi
    rm -f -- "$BUILD_DIR/trace.json"
    : > "$BUILD_DIR/trace.jsonl"
    QEMU_FLAGS+=(-debugcon "file:$BUILD_DIR/trace.jsonl" -global isa-debugcon.iobase=0xe9)
    finish_trace() {
        if [[ -f "$BUILD_DIR/trace.jsonl" ]]; then
            python3 tools/trace_to_perfetto.py "$BUILD_DIR/trace.jsonl" "$BUILD_DIR/trace.json"
        fi
    }
    trap finish_trace EXIT
    echo "==> recording to $BUILD_DIR/trace.jsonl (converted when QEMU exits)"
fi

echo "==> booting $BUILD_TYPE ($ISO)"
qemu-system-x86_64 \
    -cdrom "$ISO" \
    -m 256M \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    ${QEMU_FLAGS[@]+"${QEMU_FLAGS[@]}"}

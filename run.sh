#!/bin/bash
set -e

BUILD_DIR="build/general-debug-build"
ISO="$BUILD_DIR/twokernel.iso"

# Build the ISO
cmake --build "$BUILD_DIR" --target iso

# Run in QEMU (pass --debug to wait for lldb on port 1234)
DEBUG_FLAGS=""
if [[ "$1" == "--debug" ]]; then
    DEBUG_FLAGS="-s -S"
fi

qemu-system-x86_64 \
    -cdrom "$ISO" \
    -m 256M \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    $DEBUG_FLAGS

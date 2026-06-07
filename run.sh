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

# pass --intlog to log every interrupt/exception the CPU services to qemu.log
INT_FLAGS=""
if [[ "$1" == "--intlog" || "$2" == "--intlog" ]]; then
    INT_FLAGS="-d int -D qemu.log"
fi

qemu-system-x86_64 \
    -cdrom "$ISO" \
    -m 256M \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    $DEBUG_FLAGS \
    $INT_FLAGS

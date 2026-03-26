#!/bin/bash
set -e

BUILD_DIR="build/general-debug-build"
ISO="$BUILD_DIR/twokernel.iso"

# Build the ISO
cmake --build "$BUILD_DIR" --target iso

# Run in QEMU
qemu-system-x86_64 \
    -cdrom "$ISO" \
    -m 256M \
    -serial stdio \
    -no-reboot \
    -no-shutdown

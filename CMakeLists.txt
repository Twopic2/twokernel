cmake_minimum_required(VERSION 3.28)

# ── Compiler selection (must be before project()) ─────────────────────────────
# Apple's clang injects -arch and -isysroot which break bare-metal cross-compilation.
# On macOS we require homebrew LLVM; on Linux clang is upstream and works as-is.
if(CMAKE_HOST_APPLE)
    find_program(LLVM_CLANG   clang   PATHS /opt/homebrew/opt/llvm/bin
                                            /usr/local/opt/llvm/bin
                                      NO_DEFAULT_PATH REQUIRED)
    find_program(LLVM_CLANGXX clang++ PATHS /opt/homebrew/opt/llvm/bin
                                            /usr/local/opt/llvm/bin
                                      NO_DEFAULT_PATH REQUIRED)
    set(CMAKE_C_COMPILER   ${LLVM_CLANG})
    set(CMAKE_CXX_COMPILER ${LLVM_CLANGXX})
else()
    set(CMAKE_C_COMPILER   clang)
    set(CMAKE_CXX_COMPILER clang++)
endif()

# Tell CMake this is a cross-compilation for a bare-metal x86_64 target.
# "Generic" prevents CMake frm applying any host OS platform flags.
# STATIC_LIBRARY skips the link step in compiler detection.
set(CMAKE_SYSTEM_NAME             Generic)
set(CMAKE_SYSTEM_PROCESSOR        x86_64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(twokernel LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LIMINE_PROTO_DIR ${CMAKE_SOURCE_DIR}/dependencies/limine-proto)
set(LIMINE_BIN_DIR ${CMAKE_SOURCE_DIR}/dependencies/limine-bin)
set(FLANTERM_DIR ${CMAKE_SOURCE_DIR}/dependencies/flanterm)

if(NOT EXISTS ${LIMINE_PROTO_DIR}/include/limine.h)
    message(FATAL_ERROR
        "limine.h not found. Run:\n"
        "  git clone --depth 1 "
        "https://codeberg.org/Limine/limine-protocol.git dependencies/limine-proto\n"
        "  git clone --depth 1 --branch v10.x-binary "
        "https://codeberg.org/Limine/Limine.git dependencies/limine-bin")
endif()

# Re-configure when limine.h changes so compile_commands.json stays current.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${LIMINE_PROTO_DIR}/include/limine.h
)

# ── Kernel compile flags ──────────────────────────────────────────────────────
set(CROSS_FLAGS "\
    -target x86_64-unknown-none-elf \
    -ffreestanding \
    -fno-stack-protector \
    -fno-PIC \
    -mcmodel=kernel \
    -mno-red-zone \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -Wall -Wextra \
")
set(CMAKE_C_FLAGS   "${CROSS_FLAGS}")
set(CMAKE_CXX_FLAGS "${CROSS_FLAGS} -fno-exceptions -fno-rtti")

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/src/kernel/arch/x86-64/linker.lds)

set(KERNEL_SOURCES
    src/kmain.cpp
    src/kernel/boot/boot.cpp
    src/kernel/limine/requests.cpp
    src/kernel/drivers/framebuffer.cpp
    src/kernel/drivers/tty.cpp
    src/kernel/util/kernel_logger.cpp
)

set(FLANTERM_SOURCES
    ${FLANTERM_DIR}/src/flanterm.c
    ${FLANTERM_DIR}/src/flanterm_backends/fb.c
)

set(LIBC_SOURCES
    src/libc/string.cpp
)

add_executable(twokernel.elf ${KERNEL_SOURCES} ${FLANTERM_SOURCES} ${LIBC_SOURCES}) 

if(CMAKE_HOST_APPLE)
    # Bypass clang++ for linking — it injects macOS SDK flags (-arch, -platform_version,
    # -syslibroot) even with -target x86_64-unknown-none-elf, which confuses ld.lld.
    # Invoking ld.lld directly avoids all host-platform contamination.
    set(CMAKE_CXX_LINK_EXECUTABLE
        "/opt/homebrew/bin/ld.lld <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>"
    )
    target_link_options(twokernel.elf PRIVATE
        -T ${LINKER_SCRIPT}
        -nostdlib
        -static
        -z max-page-size=0x1000
    )
else()
    target_link_options(twokernel.elf PRIVATE
        -target x86_64-unknown-none-elf
        -T ${LINKER_SCRIPT}
        -nostdlib
        -static
        -fuse-ld=lld
        -Wl,-z,max-page-size=0x1000
    )
endif()

target_include_directories(twokernel.elf PRIVATE
    src
    src/kernel
    src/include
    ${LIMINE_PROTO_DIR}/include
    ${FLANTERM_DIR}/src
)

# Use -nostdinc++ so the host libc++ (which assumes a hosted OS) is never
# pulled in. Our src/include/cstdint and src/include/cstddef wrappers
# forward to the compiler's built-in freestanding <stdint.h> / <stddef.h>.
target_compile_options(twokernel.elf PRIVATE -nostdinc++)

# ── Limine host tool ──────────────────────────────────────────────────────────
add_custom_command(
    OUTPUT  ${CMAKE_BINARY_DIR}/limine
    COMMAND cc -g -O2 -pipe -std=c99 ${LIMINE_BIN_DIR}/limine.c -o ${CMAKE_BINARY_DIR}/limine
    DEPENDS ${LIMINE_BIN_DIR}/limine.c ${LIMINE_PROTO_DIR}/include/limine.h
    COMMENT "Building limine host tool"
)
add_custom_target(limine_host DEPENDS ${CMAKE_BINARY_DIR}/limine)

# ── ISO image ─────────────────────────────────────────────────────────────────
set(ISO_ROOT  ${CMAKE_BINARY_DIR}/iso_root)
set(ISO_OUT   ${CMAKE_BINARY_DIR}/twokernel.iso)
set(LIMINE_CONF ${CMAKE_SOURCE_DIR}/src/kernel/limine.conf)

add_custom_target(iso
    DEPENDS twokernel.elf limine_host
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${ISO_ROOT}
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${ISO_ROOT}/boot/limine
        ${ISO_ROOT}/EFI/BOOT

    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:twokernel.elf>
        ${ISO_ROOT}/boot/kernel

    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_CONF} ${ISO_ROOT}/boot/limine/limine.conf

    COMMAND ${CMAKE_COMMAND} -E copy
        ${LIMINE_BIN_DIR}/limine-bios.sys
        ${LIMINE_BIN_DIR}/limine-bios-cd.bin
        ${LIMINE_BIN_DIR}/limine-uefi-cd.bin
        ${ISO_ROOT}/boot/limine/
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_BIN_DIR}/BOOTX64.EFI  ${ISO_ROOT}/EFI/BOOT/
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_BIN_DIR}/BOOTIA32.EFI ${ISO_ROOT}/EFI/BOOT/

    COMMAND xorriso -as mkisofs -R -r -J
        -b boot/limine/limine-bios-cd.bin
        -no-emul-boot -boot-load-size 4 -boot-info-table
        -hfsplus -apm-block-size 2048
        --efi-boot boot/limine/limine-uefi-cd.bin
        -efi-boot-part --efi-boot-image --protective-msdos-label
        ${ISO_ROOT} -o ${ISO_OUT}

    COMMAND ${CMAKE_BINARY_DIR}/limine bios-install ${ISO_OUT}

    COMMENT "Building twokernel.iso"
)

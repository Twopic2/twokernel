cmake_minimum_required(VERSION 3.20)

project(twokernel LANGUAGES CXX ASM_NASM)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "\
    -target x86_64-elf \
    -ffreestanding \
    -fno-exceptions \
    -fno-rtti \
    -fno-stack-protector \
    -fno-pic \
    -mno-red-zone \
    -Wall -Wextra \
")

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/src/kernel/arch/linker-x86_64.ld)

set(CMAKE_ASM_NASM_FLAGS "-f elf64")

set(KERNEL_SOURCES
    src/kernel/kernel_main.cpp
    src/kernel/drivers/vga.cpp
)

set(LIBC_SOURCES
    src/libc/string.cpp
)

set(ASM_SOURCES
    src/kernel/arch/x86_64/boot.asm
)

add_executable(twokernel.elf ${KERNEL_SOURCES} ${LIBC_SOURCES} ${ASM_SOURCES})

target_link_options(twokernel.elf PRIVATE
    -T ${LINKER_SCRIPT}
    -nostdlib
    -static
    -fuse-ld=lld
)

target_include_directories(twokernel.elf PRIVATE
    src/kernel
    src/include
)

add_custom_command(TARGET twokernel.elf POST_BUILD
    COMMAND llvm-objcopy -O binary twokernel.elf twokernel.bin
    COMMENT "Creating flat binary twokernel.bin"
)

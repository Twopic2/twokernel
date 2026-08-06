#include <tests/ktest.hpp>
#include <exe/elf.hpp>

#include <cstddef>
#include <cstdint>

/*
    ELF64 validation tests, modeled on serenity/Tests/LibELF/test-elf.cpp.

    Each case builds a tiny in-memory ELF64 executable (header + one LOAD
    segment + 16 bytes of "code") and corrupts one field. The local
    validate_header()/validate_phdrs() are an executable spec of what
    elf_load() must reject — once the loader has its own validation,
    point these tests at it and delete the local copies.
*/

namespace Tests {
    namespace {
        using namespace Exe;

        bool validate_header(const void* data, std::size_t size) {
            if (size < sizeof(Elf::Elf64_Header)) {
                return false;
            }

            const auto& header = *static_cast<const Elf::Elf64_Header*>(data);

            if (header.e_ident[Elf::EI_MAG0] != Elf::ELFMAG0
                || header.e_ident[Elf::EI_MAG1] != Elf::ELFMAG1
                || header.e_ident[Elf::EI_MAG2] != Elf::ELFMAG2
                || header.e_ident[Elf::EI_MAG3] != Elf::ELFMAG3) {
                return false;
            }

            if (header.e_ident[Elf::EI_CLASS] != Elf::ELFCLASS64) {
                return false;
            }
            if (header.e_ident[Elf::EI_DATA] != Elf::ELFDATA2LSB) {
                return false;
            }
            if (header.e_ident[Elf::EI_VERSION] != Elf::EV_CURRENT) {
                return false;
            }
            if (header.e_machine != Elf::ELFMACHINE) {
                return false;
            }
            if (header.e_type != Elf::ElfType::Exec) {
                return false;
            }
            if (header.e_phentsize != sizeof(Elf::Elf64_Phdr)) {
                return false;
            }

            return true;
        }

        bool validate_phdrs(const void* data, std::size_t size) {
            if (!validate_header(data, size)) {
                return false;
            }

            const auto& header = *static_cast<const Elf::Elf64_Header*>(data);

            if (header.e_phoff > size
                || header.e_phnum > (size - header.e_phoff) / sizeof(Elf::Elf64_Phdr)) {
                return false;
            }

            auto* phdrs = reinterpret_cast<const Elf::Elf64_Phdr*>(
                static_cast<const std::uint8_t*>(data) + header.e_phoff);

            for (std::uint16_t i = 0; i < header.e_phnum; i++) {
                const auto& phdr = phdrs[i];
                if (phdr.p_type != Elf::ProgramType::Load) {
                    continue;
                }
                if (phdr.p_filesz > phdr.p_memsz) {
                    return false;
                }
                if (phdr.p_offset > size || phdr.p_filesz > size - phdr.p_offset) {
                    return false;
                }
            }

            return true;
        }

        struct TestImage {
            Elf::Elf64_Header header;
            Elf::Elf64_Phdr phdr;
            std::uint8_t code[16];
        };

        // The loader casts raw file bytes into these structs, so they must
        // match the ELF64 spec byte-for-byte.
        static_assert(sizeof(Elf::Elf64_Header) == 64);
        static_assert(sizeof(Elf::Elf64_Phdr) == 56);
        static_assert(offsetof(Elf::Elf64_Header, e_type) == 16);
        static_assert(offsetof(Elf::Elf64_Header, e_entry) == 24);
        static_assert(offsetof(Elf::Elf64_Header, e_phoff) == 32);
        static_assert(offsetof(Elf::Elf64_Header, e_phnum) == 56);
        static_assert(offsetof(Elf::Elf64_Phdr, p_offset) == 8);
        static_assert(offsetof(Elf::Elf64_Phdr, p_vaddr) == 16);
        static_assert(offsetof(Elf::Elf64_Phdr, p_filesz) == 32);
        static_assert(offsetof(Elf::Elf64_Phdr, p_memsz) == 40);
        static_assert(offsetof(TestImage, phdr) == sizeof(Elf::Elf64_Header));

        TestImage make_valid_image() {
            TestImage image {};
            auto& header = image.header;

            header.e_ident[Elf::EI_MAG0] = Elf::ELFMAG0;
            header.e_ident[Elf::EI_MAG1] = Elf::ELFMAG1;
            header.e_ident[Elf::EI_MAG2] = Elf::ELFMAG2;
            header.e_ident[Elf::EI_MAG3] = Elf::ELFMAG3;
            header.e_ident[Elf::EI_CLASS] = Elf::ELFCLASS64;
            header.e_ident[Elf::EI_DATA] = Elf::ELFDATA2LSB;
            header.e_ident[Elf::EI_VERSION] = Elf::EV_CURRENT;

            header.e_type = Elf::ElfType::Exec;
            header.e_machine = Elf::ELFMACHINE;
            header.e_version = Elf::EV_CURRENT;
            header.e_entry = 0x400000;
            header.e_phoff = offsetof(TestImage, phdr);
            header.e_ehsize = sizeof(Elf::Elf64_Header);
            header.e_phentsize = sizeof(Elf::Elf64_Phdr);
            header.e_phnum = 1;

            auto& phdr = image.phdr;
            phdr.p_type = Elf::ProgramType::Load;
            phdr.p_flags = Elf::PF_R | Elf::PF_X;
            phdr.p_offset = offsetof(TestImage, code);
            phdr.p_vaddr = 0x400000;
            phdr.p_paddr = 0x400000;
            phdr.p_filesz = sizeof(image.code);
            phdr.p_memsz = sizeof(image.code);
            phdr.p_align = 0x1000;

            return image;
        }
    }

    TEST_CASE("a well-formed executable passes validation", "[elf]") {
        auto image = make_valid_image();
        CHECK(validate_header(&image, sizeof(image)));
        CHECK(validate_phdrs(&image, sizeof(image)));
    }

    TEST_CASE("an empty or truncated image is rejected", "[elf]") {
        auto image = make_valid_image();
        CHECK_FALSE(validate_header(&image, 0));
        CHECK_FALSE(validate_phdrs(&image, 0));
        CHECK_FALSE(validate_header(&image, sizeof(Elf::Elf64_Header) / 2));
    }

    TEST_CASE("a bad magic number is rejected", "[elf]") {
        auto image = make_valid_image();
        image.header.e_ident[Elf::EI_MAG0] = 0x7E;
        CHECK_FALSE(validate_header(&image, sizeof(image)));
    }

    TEST_CASE("a 32-bit class is rejected", "[elf]") {
        auto image = make_valid_image();
        image.header.e_ident[Elf::EI_CLASS] = Elf::ELFCLASS32;
        CHECK_FALSE(validate_header(&image, sizeof(image)));
    }

    TEST_CASE("a big-endian image is rejected", "[elf]") {
        auto image = make_valid_image();
        image.header.e_ident[Elf::EI_DATA] = Elf::ELFDATA2MSB;
        CHECK_FALSE(validate_header(&image, sizeof(image)));
    }

    TEST_CASE("a bad ident version is rejected", "[elf]") {
        auto image = make_valid_image();
        image.header.e_ident[Elf::EI_VERSION] = 0;
        CHECK_FALSE(validate_header(&image, sizeof(image)));
    }

    TEST_CASE("a non-x86_64 machine is rejected", "[elf]") {
        auto image = make_valid_image();
        image.header.e_machine = 0xB7; // aarch64
        CHECK_FALSE(validate_header(&image, sizeof(image)));
    }

    TEST_CASE("a relocatable object is rejected", "[elf]") {
        auto image = make_valid_image();
        image.header.e_type = Elf::ElfType::Rel;
        CHECK_FALSE(validate_header(&image, sizeof(image)));
    }

    TEST_CASE("a wrong e_phentsize is rejected", "[elf]") {
        auto image = make_valid_image();
        image.header.e_phentsize = 32;
        CHECK_FALSE(validate_header(&image, sizeof(image)));
    }

    TEST_CASE("a phdr table past the end of the file is rejected", "[elf]") {
        auto image = make_valid_image();
        image.header.e_phoff = sizeof(image);
        REQUIRE(validate_header(&image, sizeof(image)));
        CHECK_FALSE(validate_phdrs(&image, sizeof(image)));
    }

    TEST_CASE("an overflowing e_phoff is rejected", "[elf]") {
        auto image = make_valid_image();
        image.header.e_phoff = ~std::uint64_t(0) - 8;
        CHECK_FALSE(validate_phdrs(&image, sizeof(image)));
    }

    TEST_CASE("an e_phnum past the end of the file is rejected", "[elf]") {
        auto image = make_valid_image();
        image.header.e_phnum = 1000;
        CHECK_FALSE(validate_phdrs(&image, sizeof(image)));
    }

    TEST_CASE("a LOAD segment past the end of the file is rejected", "[elf]") {
        auto image = make_valid_image();
        image.phdr.p_offset = sizeof(image);
        image.phdr.p_filesz = 1;
        CHECK_FALSE(validate_phdrs(&image, sizeof(image)));
    }

    TEST_CASE("an overflowing p_offset is rejected", "[elf]") {
        auto image = make_valid_image();
        image.phdr.p_offset = ~std::uint64_t(0) - 8;
        image.phdr.p_filesz = 16;
        CHECK_FALSE(validate_phdrs(&image, sizeof(image)));
    }

    TEST_CASE("p_filesz larger than p_memsz is rejected", "[elf]") {
        auto image = make_valid_image();
        image.phdr.p_memsz = image.phdr.p_filesz - 1;
        CHECK_FALSE(validate_phdrs(&image, sizeof(image)));
    }

    TEST_CASE("a bss-only LOAD segment is accepted", "[elf]") {
        // .bss: no file bytes, only memory the loader zero-fills
        auto image = make_valid_image();
        image.phdr.p_filesz = 0;
        image.phdr.p_memsz = 0x2000;
        CHECK(validate_phdrs(&image, sizeof(image)));
    }

    TEST_CASE("non-LOAD segments are not bounds-checked", "[elf]") {
        // Bounds rules only apply to segments the loader will map
        auto image = make_valid_image();
        image.phdr.p_type = Elf::ProgramType::Null;
        image.phdr.p_offset = ~std::uint64_t(0);
        image.phdr.p_filesz = ~std::uint64_t(0);
        CHECK(validate_phdrs(&image, sizeof(image)));
    }
}

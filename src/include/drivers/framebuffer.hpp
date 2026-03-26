#pragma once

#include <limine.h>
#include <std/cstddef>
#include <std/cstdint>

/*
Reference:
    struct limine_framebuffer {
        LIMINE_PTR(void *) address;      // Pointer to start of VRAM — write pixels here
        uint64_t width;                   // Screen width in pixels
        uint64_t height;                  // Screen height in pixels (number of rows)
        uint64_t pitch;                   // Bytes to skip to move one row down (>= width * bpp/8, may include padding)
        uint16_t bpp;                     // Bits per pixel (e.g. 32 = 4 bytes per pixel)
        uint8_t memory_model;             // Pixel format — 1 = RGB
        uint8_t red_mask_size;            // How many bits the red channel uses (usually 8)
        uint8_t red_mask_shift;           // Bit position where red starts (e.g. 16 = bits 16-23)
        uint8_t green_mask_size;          // How many bits the green channel uses
        uint8_t green_mask_shift;         // Bit position where green starts (e.g. 8 = bits 8-15)
        uint8_t blue_mask_size;           // How many bits the blue channel uses
        uint8_t blue_mask_shift;          // Bit position where blue starts (e.g. 0 = bits 0-7)
        uint8_t unused[7];               // Padding, ignore
        uint64_t edid_size;              // Size of EDID data blob in bytes
        LIMINE_PTR(void *) edid;         // Pointer to raw EDID data (monitor info — resolution, manufacturer, etc.)
        uint64_t mode_count;             // Number of available video modes
        LIMINE_PTR(struct limine_video_mode **) modes; // Array of pointers to available resolutions/depths
    };
*/

namespace Drivers {
    struct ColorRGB {
        std::uint8_t r;
        std::uint8_t g;
        std::uint8_t b;
    };

    class FrameBuffer {
    protected:
        struct limine_framebuffer* m_fb {};

    public:
        FrameBuffer(struct limine_framebuffer* fb);

        ~FrameBuffer() = default;

        void put_pixel(std::uint64_t x, std::uint64_t y, const ColorRGB& color);
        void put_pixel(std::uint64_t x, std::uint64_t y, std::uint32_t packed_color);
        void fill_rect(std::uint64_t width, std::uint64_t height, const ColorRGB& color);
        
        // 0x00RRGGBB
        constexpr std::uint64_t get_addr(const ColorRGB& color) const {
            return (static_cast<std::uint64_t>(color.r) << m_fb->red_mask_shift)
                 | (static_cast<std::uint64_t>(color.g) << m_fb->green_mask_shift)
                 | (static_cast<std::uint64_t>(color.b) << m_fb->blue_mask_shift);
        }
    };
}

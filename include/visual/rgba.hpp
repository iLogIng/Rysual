#pragma once

#include <cstdint>
namespace rysual
{

struct Irgba {

    uint8_t r, g, b, a;

    Irgba() = default;

    constexpr Irgba(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_)
        : r(r_), g(g_), b(b_), a(a_) {}

    constexpr Irgba(uint32_t argb)
        : r((argb >> 24) & 0xFF)
        , g((argb >> 16) & 0xFF)
        , b((argb >> 8) & 0xFF)
        , a(argb & 0xFF) {}

    operator uint32_t() {
        uint32_t color = 0x00;
        color |= r << 24;
        color |= g << 16;
        color |= b << 8;
        color |= a << 0;
        return color;
    }

}; // struct Irgba

} // namespace rysual


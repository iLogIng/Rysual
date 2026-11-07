#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>

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

    // 插值
    static Irgba lerp(const Irgba& x, const Irgba& y, float t) {
        t = std::clamp(t, 0.f, 1.f);
        auto lerp_channel = [t](uint8_t a, uint8_t b) -> uint8_t {
            float val = std::lerp(static_cast<float>(a), static_cast<float>(b), t);
            return static_cast<uint8_t>(std::clamp(std::round(val), 0.0f, 255.0f));
        };
        return Irgba(
            lerp_channel(x.r, y.r),
            lerp_channel(x.g, y.g),
            lerp_channel(x.b, y.b),
            lerp_channel(x.a, y.a)
        );
    }

    static Irgba fromHSV(float h, float s, float v) {
        float c = v * s;
        float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;
        float r = 0, g = 0, b = 0;
        if (h < 60)       { r = c; g = x; }
        else if (h < 120) { r = x; g = c; }
        else if (h < 180) { g = c; b = x; }
        else if (h < 240) { g = x; b = c; }
        else if (h < 300) { r = x; b = c; }
        else              { r = c; b = x; }
        return Irgba(static_cast<uint8_t>((r + m) * 255), 
                     static_cast<uint8_t>((g + m) * 255), 
                     static_cast<uint8_t>((b + m) * 255), 255);
    }

    static Irgba fromHSL(float h, float s, float l, uint8_t alpha = 0xFF) {

        h = std::fmod(h, 360.0f);
        if (h < 0.0f) h += 360.0f;
        s = std::clamp(s, 0.0f, 1.0f);
        l = std::clamp(l, 0.0f, 1.0f);

        if (s == 0.0f) {
            uint8_t gray = static_cast<uint8_t>(l * 255.0f);
            return Irgba(gray, gray, gray, 255);
        }

        float c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
        float h_prime = h / 60.0f;
        float x = c * (1.0f - std::abs(std::fmod(h_prime, 2.0f) - 1.0f));
        float m = l - c / 2.0f;

        float r_prime = 0.0f, g_prime = 0.0f, b_prime = 0.0f;
        if (h_prime >= 0.0f && h_prime < 1.0f) {
            r_prime = c; g_prime = x; b_prime = 0.0f;
        } else if (h_prime >= 1.0f && h_prime < 2.0f) {
            r_prime = x; g_prime = c; b_prime = 0.0f;
        } else if (h_prime >= 2.0f && h_prime < 3.0f) {
            r_prime = 0.0f; g_prime = c; b_prime = x;
        } else if (h_prime >= 3.0f && h_prime < 4.0f) {
            r_prime = 0.0f; g_prime = x; b_prime = c;
        } else if (h_prime >= 4.0f && h_prime < 5.0f) {
            r_prime = x; g_prime = 0.0f; b_prime = c;
        } else if (h_prime >= 5.0f && h_prime < 6.0f) {
            r_prime = c; g_prime = 0.0f; b_prime = x;
        }

        uint8_t r = static_cast<uint8_t>(std::round((r_prime + m) * 255.0f));
        uint8_t g = static_cast<uint8_t>(std::round((g_prime + m) * 255.0f));
        uint8_t b = static_cast<uint8_t>(std::round((b_prime + m) * 255.0f));

        return Irgba(r, g, b, alpha);
    }


}; // struct Irgba

} // namespace rysual


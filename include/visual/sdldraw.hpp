#pragma once

#include <cmath>
#include "utils.hpp"
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>

#include "rgba.hpp"

namespace rysual
{

inline void drawRectLT(SDL_Renderer *renderer, SDL_FRect rect, Irgba color, bool filled = true) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    if (filled) {
        SDL_RenderFillRect(renderer, &rect);
    }
    else {
        SDL_RenderRect(renderer, &rect);
    }
}

inline void drawRectLB(SDL_Renderer *renderer, SDL_FRect rect, Irgba color, bool filled = true) {
    SDL_FRect rectangle = {
        .x = rect.x,
        .y = rect.y - rect.h,
        .w = rect.w,
        .h = rect.h,
    };
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    if (filled) {
        SDL_RenderFillRect(renderer, &rectangle);
    }
    else {
        SDL_RenderRect(renderer, &rectangle);
    }
}

inline void drawRectCR(SDL_Renderer* renderer, SDL_FRect rect, Irgba color, bool filled = true) {
    SDL_FRect rectangle = {
        .x = rect.x - rect.w / 2,
        .y = rect.y - rect.h / 2,
        .w = rect.w,
        .h = rect.h,
    };
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    if (filled) {
        SDL_RenderFillRect(renderer, &rectangle);
    }
    else {
        SDL_RenderRect(renderer, &rectangle);
    }
}

// 绘制正弦波模拟
inline void drawSinRectSimulat(SDL_Renderer *renderer, int64_t tick_ms, int wWidth, int wHeight, float rect_count, float rect_height, Irgba base_color, float lo, float hi) {
    const float rect_width = static_cast<float>(wWidth) / rect_count;
    SDL_FRect rect = {.x=rect_width/2.f, .y=static_cast<float>(wHeight)/2,
                      .w=rect_width, .h=rect_height};
    for (int i = 0; i < static_cast<int>((wWidth + rect.w) / rect.w); ++i) {
        float curHeight = rect_height * (std::sin(rect.x + static_cast<float>(tick_ms)/1'000.f) + 1.f) / 2.f;
        rect.h = curHeight * scaleFactor(0, rect_height, lo, hi) + lo;
        rysual::Irgba curColor = static_cast<uint32_t>((std::sin<uint32_t>(rect.x) + 1.f) / 2.f * base_color);
        rysual::drawRectCR(renderer, rect, curColor);
        rect.x += rect.w;
    }
}

} // namespace rysual


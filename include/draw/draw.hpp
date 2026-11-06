#pragma once

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

} // namespace rysual


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

// 绘制波形线
// 计算采样的 RMS 均方差
inline void renderWaveform(SDL_Renderer* renderer, const std::vector<float>& samples, float rms, int width, int height)
{
    if (samples.empty() || width <= 0 || height <= 0) {
        return;
    }

    const float centerY = static_cast<float>(height) * 0.5f;
    const float amplitude = static_cast<float>(height) * 0.35f;

    std::vector<SDL_FPoint> points;
    points.reserve(static_cast<size_t>(width));

    for (int x = 0; x < width; ++x) {
        const size_t begin =
            static_cast<size_t>(x) * samples.size() / static_cast<size_t>(width);
        size_t end =
            static_cast<size_t>(x + 1) * samples.size() / static_cast<size_t>(width);
        if (end <= begin) {
            end = begin + 1;
        }

        // 取该列内绝对值最大的样本，保留正负号
        float value = 0.0f;
        for (size_t i = begin; i < end && i < samples.size(); ++i) {
            if (std::fabs(samples[i]) > std::fabs(value)) {
                value = samples[i];
            }
        }

        const float y = centerY - value * amplitude;
        points.push_back({static_cast<float>(x), y});
    }

    // 背景中心线
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderLine(renderer, 0.0f, centerY, static_cast<float>(width), centerY);

    // 波形
    const Uint8 brightness = static_cast<Uint8>( 120 + std::min(rms * 400.0f, 135.0f));

    SDL_SetRenderDrawColor(renderer, 0x66, 0xcc, brightness, 0xFF);
    SDL_RenderLines(renderer, points.data(), static_cast<int>(points.size()));
}

// 绘制波形线
// amplitude_scale: 控制振幅，建议 0.3f ~ 0.8f 之间
inline void renderWaveform_v2(SDL_Renderer* renderer, const std::vector<float>& samples, float rms, int width, int height, float amplitude_scale = 0.35f, bool dc_off = true)
{
    if (samples.empty() || width <= 0 || height <= 0) {
        return;
    }

    const float centerY = static_cast<float>(height) * 0.5f;
    // 修改振幅，通过参数传入系数
    const float amplitude = static_cast<float>(height) * amplitude_scale;

    // 去除直流偏移，计算均值并减去，保证波形绝对居中
    float dc_mean = 0.f;
    if (dc_off) {
        for (float s : samples) {
            dc_mean += s;
        }
        dc_mean /= samples.size();
    }

    std::vector<SDL_FPoint> points;
    points.reserve(static_cast<size_t>(width));

    for (int x = 0; x < width; ++x) {
        const size_t begin = static_cast<size_t>(x) * samples.size() / static_cast<size_t>(width);
        size_t end = static_cast<size_t>(x + 1) * samples.size() / static_cast<size_t>(width);
        if (end <= begin) end = begin + 1;

        // 取该列内绝对值最大的样本，保留正负号
        float value = 0.0f;
        for (size_t i = begin; i < end && i < samples.size(); ++i) {
            if (std::fabs(samples[i]) > std::fabs(value)) {
                value = samples[i];
                if (dc_off) {
                    value -= dc_mean;
                }
            }
        }

        float y = centerY - value * amplitude;
        
        // 防止振幅过大导致画到屏幕外面（削波）
        y = std::clamp(y, 0.0f, static_cast<float>(height));

        points.push_back({static_cast<float>(x), y});
    }

    // 背景中心线 (基线)
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderLine(renderer, 0.0f, centerY, static_cast<float>(width), centerY);

    // 波形绘制
    const Uint8 brightness = static_cast<Uint8>(120 + std::min(rms * 400.0f, 135.0f));
    SDL_SetRenderDrawColor(renderer, 0x66, 0xcc, brightness, 0xFF);

    // 使用循环连接相邻的两个点
    for (size_t i = 0; i < points.size() - 1; ++i) {
        SDL_RenderLine(renderer, points[i].x, points[i].y, points[i+1].x, points[i+1].y);
    }
}

} // namespace rysual


#pragma once

#include <cmath>
#include "utils.hpp"
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>

#include "rgba.hpp"

namespace rysual
{

inline void drawRectLT(SDL_Renderer *renderer, const SDL_FRect *rect, Irgba color, bool filled = true) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    if (filled) {
        SDL_RenderFillRect(renderer, rect);
    }
    else {
        SDL_RenderRect(renderer, rect);
    }
}

inline void drawRectLB(SDL_Renderer *renderer, const SDL_FRect *rect, Irgba color, bool filled = true) {
    SDL_FRect rectangle = {
        .x = rect->x,
        .y = rect->y - rect->h,
        .w = rect->w,
        .h = rect->h,
    };
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    if (filled) {
        SDL_RenderFillRect(renderer, &rectangle);
    }
    else {
        SDL_RenderRect(renderer, &rectangle);
    }
}

inline void drawRectCR(SDL_Renderer* renderer, const SDL_FRect *rect, Irgba color, bool filled = true) {
    SDL_FRect rectangle = {
        .x = rect->x - rect->w / 2,
        .y = rect->y - rect->h / 2,
        .w = rect->w,
        .h = rect->h,
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
inline void drawSinRectSimulat(SDL_Renderer *renderer, int64_t tick_ms, const SDL_FRect& area, float rect_count, float rect_height, Irgba base_color, float lo, float hi) {
    const float rect_width = static_cast<float>(area.w) / rect_count;
    SDL_FRect rect = {.x=rect_width/2.f, .y=static_cast<float>(area.h)/2,
                      .w=rect_width, .h=rect_height};
    for (int i = 0; i < static_cast<int>((area.w + rect.w) / rect.w); ++i) {
        float curHeight = rect_height * (std::sin(rect.x + static_cast<float>(tick_ms)/1'000.f) + 1.f) / 2.f;
        rect.h = curHeight * scaleFactor(0, rect_height, lo, hi) + lo;
        rysual::Irgba curColor = static_cast<uint32_t>((std::sin<uint32_t>(rect.x) + 1.f) / 2.f * base_color);
        rysual::drawRectCR(renderer, &rect, curColor);
        rect.x += rect.w;
    }
}

// 绘制频谱柱
inline void drawSpectrumBars(SDL_Renderer* renderer, const std::vector<float>& levels, const SDL_FRect& area)
{
    if (levels.empty() || area.w <= 0.f || area.h <= 0.f) {
        return;
    }

    const float bar_width = area.w / static_cast<float>(levels.size());
    float bar_offset_x = 0.5f * bar_width;
    for (size_t i = 0; i < levels.size(); ++i) {
        const float level = std::clamp(levels[i], 0.f, 1.f);
        const float bar_height = area.h * level;

        const SDL_FRect bar = {
            .x = area.x + bar_offset_x,
            .y = area.h / 2.f,
            .w = bar_width * 0.8f,
            .h = bar_height,
        };
        bar_offset_x += bar_width;

        float freq_ratio = static_cast<float>(i) / levels.size();
        float h = 222.0f + 120.0f * freq_ratio; 
        float s = 1.0f - (level * 0.2f); 
        float l = 0.5f + (level * 0.3f); 

        Irgba color = Irgba::fromHSL(h, s, l);
        drawRectCR(renderer, &bar, color);
    }
}

// 绘制波形线
// 计算采样的 RMS 均方差
inline void renderWaveform(SDL_Renderer* renderer, const std::vector<float>& samples, float rms, SDL_FRect area)
{
    if (samples.empty() || area.w <= 0 || area.h <= 0) {
        return;
    }

    const float centerY = static_cast<float>(area.h) * 0.5f;
    const float amplitude = static_cast<float>(area.h) * 0.35f;

    std::vector<SDL_FPoint> points;
    points.reserve(static_cast<size_t>(area.w));

    for (int x = 0; x < area.w; ++x) {
        const size_t begin =
            static_cast<size_t>(x) * samples.size() / static_cast<size_t>(area.w);
        size_t end =
            static_cast<size_t>(x + 1) * samples.size() / static_cast<size_t>(area.w);
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
    SDL_RenderLine(renderer, 0.0f, centerY, static_cast<float>(area.w), centerY);

    // 波形
    const Uint8 brightness = static_cast<Uint8>( 120 + std::min(rms * 400.0f, 135.0f));

    SDL_SetRenderDrawColor(renderer, 0x66, 0xcc, brightness, 0xFF);
    SDL_RenderLines(renderer, points.data(), static_cast<int>(points.size()));
}

// 绘制波形线
// amplitude_scale: 控制振幅，建议 0.3f - 0.6f 之间
inline void renderWaveform_v2(SDL_Renderer* renderer, const std::vector<float>& samples, float rms, const SDL_FRect& area, float amplitude_scale = 0.35f, bool dc_off = true)
{
    if (samples.empty() || area.w <= 0 || area.h <= 0) {
        return;
    }

    const float centerY = static_cast<float>(area.h) * 0.5f;
    // 修改振幅，通过参数传入系数
    const float amplitude = static_cast<float>(area.h) * amplitude_scale;

    // 去除直流偏移，计算均值并减去，保证波形绝对居中
    float dc_mean = 0.f;
    if (dc_off) {
        for (float s : samples) {
            dc_mean += s;
        }
        dc_mean /= samples.size();
    }

    std::vector<SDL_FPoint> points;
    points.reserve(static_cast<size_t>(area.w));

    for (int x = 0; x < area.w; ++x) {
        const size_t begin = static_cast<size_t>(x) * samples.size() / static_cast<size_t>(area.w);
        size_t end = static_cast<size_t>(x + 1) * samples.size() / static_cast<size_t>(area.w);
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
        y = std::clamp(y, 0.0f, static_cast<float>(area.h));

        points.push_back({static_cast<float>(x), y});
    }

    // 背景中心线 (基线)
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderLine(renderer, 0.0f, centerY, static_cast<float>(area.w), centerY);

    // 波形绘制 使用循环连接相邻的两个点
    for (size_t i = 0; i < points.size() - 1; ++i) {
        float x_ratio = static_cast<float>(i) / points.size();
        float hue = 187.0f + 120.0f * x_ratio;
        float lightness = 0.4f + std::min(rms * 2.0f, 0.4f); 
        Irgba color = Irgba::fromHSL(hue, 1.0f, lightness);
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderLine(renderer, points[i].x, points[i].y, points[i+1].x, points[i+1].y);
    }
}

} // namespace rysual


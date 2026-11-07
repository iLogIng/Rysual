#pragma once

#include <algorithm>
#include <cmath>
extern "C" {
#include <libavutil/samplefmt.h>
}

#include <vector>
#include <SDL3/SDL_audio.h>

namespace rysual
{

inline AVSampleFormat toAVSampleFormat(SDL_AudioFormat format) {
    switch (format) {
        case SDL_AUDIO_U8: return AV_SAMPLE_FMT_U8;
        case SDL_AUDIO_S16: return AV_SAMPLE_FMT_S16;
        case SDL_AUDIO_S32: return AV_SAMPLE_FMT_S32;
        case SDL_AUDIO_F32: return AV_SAMPLE_FMT_FLT;
        default: return AV_SAMPLE_FMT_NONE;
    };
}

inline SDL_AudioFormat toSdlAudioFormat(AVSampleFormat format) {
    switch (format) {
        case AV_SAMPLE_FMT_U8: return SDL_AUDIO_U8;
        case AV_SAMPLE_FMT_S16: return SDL_AUDIO_S16;
        case AV_SAMPLE_FMT_S32: return SDL_AUDIO_S32;
        case AV_SAMPLE_FMT_FLT: return SDL_AUDIO_F32;
        default: return SDL_AUDIO_UNKNOWN;
    };
}

inline float scaleFactor(float from_lo, float from_hi, float to_lo, float to_hi) {
    return (to_hi - to_lo) / (from_hi - from_lo);
}

inline float computeRMS(const std::vector<float>& samples) {
    if (samples.empty()) {
        return 0.f;
    }
    double sum = 0.f;
    for (const auto& s : samples) {
        sum += static_cast<double>(s) * s;
    }
    const float rms = static_cast<float>(std::sqrt(sum / samples.size()));
    return rms;
}


} // namespace rysual


#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
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

// 周期性 Hann 窗: w[i] = 0.5 * (1 - cos(2*pi*i/N))
inline std::vector<float> makeHannWindow(size_t size) {
    std::vector<float> window(size, 0.f);
    if (size == 0) {
        return window;
    }

    const float two_pi = 2.f * std::numbers::pi_v<float>;
    for (size_t i = 0; i < size; ++i) {
        window[i] = 0.5f * (
            1.f - std::cos(two_pi * static_cast<float>(i) / static_cast<float>(size)));
    }
    return window;
}

// 对样本应用窗函数，输出长度始终等于窗口长度；样本不足时补 0。
inline void applyHann(const std::vector<float>& samples,
                      const std::vector<float>& window,
                      std::vector<float>& out) {
    out.assign(window.size(), 0.f);
    const size_t n = std::min(samples.size(), window.size());
    for (size_t i = 0; i < n; ++i) {
        out[i] = samples[i] * window[i];
    }
}

// FFT 频段区间
struct FftBand {
    size_t begin = 0;
    size_t end = 0;
};

// 生成对数频段；FFT 输出长度为 fft_size / 2 + 1
inline std::vector<FftBand> makeFftBands(int sample_rate,
                                         size_t fft_size,
                                         size_t band_count) {
    std::vector<FftBand> bands;
    if (sample_rate <= 0 || fft_size < 2 || band_count == 0) {
        return bands;
    }

    const float nyquist = static_cast<float>(sample_rate) * 0.5f;
    const float f_max = std::min(16000.f, nyquist);
    if (f_max <= 0.f) {
        return bands;
    }
    const float f_min = std::min(30.f, f_max * 0.5f);
    const size_t max_bin = fft_size / 2;

    bands.reserve(band_count);
    for (size_t i = 0; i < band_count; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(band_count);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(band_count);

        const float f0 = f_min * std::pow(f_max / f_min, t0);
        const float f1 = f_min * std::pow(f_max / f_min, t1);

        const float scale = static_cast<float>(fft_size) / static_cast<float>(sample_rate);
        size_t bin0 = static_cast<size_t>(std::llround(f0 * scale));
        size_t bin1 = static_cast<size_t>(std::llround(f1 * scale));

        bin0 = std::clamp(bin0, size_t(1), max_bin);
        bin1 = std::clamp(bin1, bin0 + 1, max_bin + 1);
        bands.push_back({bin0, bin1});
    }
    return bands;
}

// 将 FFT 结果映射为归一化频段高度
inline void updateBandLevels(const std::vector<std::complex<float>>& spectrum,
                             const std::vector<FftBand>& bands,
                             size_t fft_size,
                             std::vector<float>& levels,
                             float min_db = -80.f,
                             float max_db = 0.f,
                             float release = 0.85f) {
    if (levels.size() != bands.size()) {
        levels.assign(bands.size(), 0.f);
    }
    if (spectrum.empty() || fft_size == 0 || max_db <= min_db) {
        return;
    }

    const float scale = 2.f / static_cast<float>(fft_size);

    for (size_t b = 0; b < bands.size(); ++b) {
        float max_mag = 0.f;
        for (size_t k = bands[b].begin;
             k < bands[b].end && k < spectrum.size();
             ++k) {
            max_mag = std::max(max_mag, std::abs(spectrum[k]));
        }

        const float db = 20.f * std::log10(max_mag * scale + 1e-9f);
        const float target = std::clamp(
            (db - min_db) / (max_db - min_db), 0.f, 1.f);

        levels[b] = std::max(target, levels[b] * release);
    }
}

} // namespace rysual


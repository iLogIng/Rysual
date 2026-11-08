#pragma once

#include "output/AudioTap.hpp"
#include "utils.hpp"
#include "visual/FFT.hpp"
#include "visual/rgba.hpp"

#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>

#include <complex>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rysual
{

// 可视化显示模块
//
// 负责：
//   - 当前显示模式
//   - FFT / Hann / 频段 / 平滑状态
//   - 从 AudioTap 读取当前播放窗口
//   - 三种显示模式的分派与绘制
class Visualize
{
public:
    enum class Mode
    {
        Spectrum,
        Waveform,
        Simulated,
    };

    explicit Visualize(size_t fft_size = 2048, size_t band_count = 48);
    ~Visualize() = default;

    Visualize(const Visualize&) = delete;
    Visualize& operator=(const Visualize&) = delete;

    bool configure(int sample_rate);

    void setMode(Mode mode) noexcept { m_mode = mode; }
    Mode mode() const noexcept { return m_mode; }

    void setBandCount(size_t n) { m_band_count = n; }

    bool valid() const noexcept { return m_valid; }

    // 从 AudioTap 读取 position_us 对应的窗口，并更新当前模式的分析状态
    void update(const AudioTap& tap, int64_t position_us);

    // 清屏并绘制当前模式
    void render(SDL_Renderer *renderer, const SDL_FRect& area, const Irgba& background) const;

private:
    void updateSpectrum();
    void renderSpectrum(SDL_Renderer *renderer, const SDL_FRect& area) const;
    void renderWaveform(SDL_Renderer *renderer, const SDL_FRect& area) const;
    void renderSimulated(SDL_Renderer *renderer, const SDL_FRect& area) const;

    Mode m_mode = Mode::Spectrum;

    size_t m_fft_size = 0;
    size_t m_band_count = 0;
    bool m_valid = false;

    FFT m_fft;
    std::vector<float> m_hann;
    std::vector<float> m_samples;
    std::vector<float> m_windowed;
    std::vector<std::complex<float>> m_spectrum;
    std::vector<FftBand> m_bands;
    std::vector<float> m_levels;
    float m_rms_display = 0.f;

}; // class Visualize

} // namespace rysual

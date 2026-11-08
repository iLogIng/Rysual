#include "visual/Visualize.hpp"

#include "visual/sdldraw.hpp"

#include <SDL3/SDL_timer.h>

#include <algorithm>

namespace
{

SDL_Rect toRect(const SDL_FRect& area)
{
    return SDL_Rect{
        static_cast<int>(area.x),
        static_cast<int>(area.y),
        static_cast<int>(area.w),
        static_cast<int>(area.h),
    };
}

} // namespace

namespace rysual
{

Visualize::Visualize(size_t fft_size, size_t band_count)
    : m_fft_size(fft_size)
    , m_band_count(band_count)
    , m_fft(fft_size)
    , m_hann(makeHannWindow(fft_size))
{
    m_samples.reserve(fft_size);
    m_windowed.reserve(fft_size);
}

bool Visualize::configure(int sample_rate)
{
    if (sample_rate <= 0) {
        m_valid = false;
        return false;
    }

    m_bands = makeFftBands(sample_rate, m_fft_size, m_band_count);
    if (m_bands.empty()) {
        m_valid = false;
        return false;
    }

    m_levels.assign(m_bands.size(), 0.f);
    m_rms_display = 0.f;
    m_valid = true;
    return true;
}

void Visualize::update(const AudioTap& tap, int64_t position_us)
{
    if (!m_valid || m_mode == Mode::Simulated) {
        return;
    }

    if (!tap.valid() ||
        !tap.readWindow(position_us, m_fft_size, m_samples)) {
        m_samples.assign(m_fft_size, 0.f);
    }

    switch (m_mode) {
    case Mode::Spectrum:
        updateSpectrum();
        break;

    case Mode::Waveform: {
        const float rms = computeRMS(m_samples);
        m_rms_display = std::max(rms, m_rms_display * 0.90f);
        break;
    }

    case Mode::Simulated:
        break;
    }
}

void Visualize::updateSpectrum()
{
    applyHann(m_samples, m_hann, m_windowed);
    m_fft.forward(m_windowed.data(), m_spectrum);
    updateBandLevels(m_spectrum, m_bands, m_fft_size, m_levels);
}

void Visualize::render(SDL_Renderer *renderer, const SDL_FRect& area, const Irgba& background) const
{
    SDL_SetRenderDrawColor(renderer, background.r, background.g, background.b, background.a);
    SDL_RenderClear(renderer);

    switch (m_mode) {
    case Mode::Spectrum:
        renderSpectrum(renderer, area);
        break;

    case Mode::Waveform:
        renderWaveform(renderer, area);
        break;

    case Mode::Simulated:
        renderSimulated(renderer, area);
        break;
    }
}

void Visualize::renderSpectrum(SDL_Renderer *renderer, const SDL_FRect& area) const
{
    const SDL_Rect viewport = toRect(area);
    if (viewport.w <= 0 || viewport.h <= 0) {
        return;
    }

    SDL_SetRenderViewport(renderer, &viewport);
    drawSpectrumBars(renderer, m_levels, area);
    SDL_SetRenderViewport(renderer, nullptr);
}

void Visualize::renderWaveform(SDL_Renderer *renderer, const SDL_FRect& area) const
{
    const SDL_Rect viewport = toRect(area);
    if (viewport.w <= 0 || viewport.h <= 0) {
        return;
    }

    SDL_SetRenderViewport(renderer, &viewport);
    renderWaveform_Geometry(renderer, m_samples, m_rms_display, area);
    SDL_SetRenderViewport(renderer, nullptr);
}

void Visualize::renderSimulated(SDL_Renderer *renderer, const SDL_FRect& area) const
{
    const SDL_Rect viewport = toRect(area);
    if (viewport.w <= 0 || viewport.h <= 0) {
        return;
    }

    SDL_SetRenderViewport(renderer, &viewport);
    drawSinRectSimulat(renderer, static_cast<int64_t>(SDL_GetTicks()),
                       area, 25.f, 300.f,
                       Irgba{0xFFFFFFFFu},
                       0.f, 500.f);
    SDL_SetRenderViewport(renderer, nullptr);
}

} // namespace rysual

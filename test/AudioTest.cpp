#include "media/RingBuffer.hpp"
#include "output/AudioTap.hpp"
#include "utils.hpp"
#include "visual/FFT.hpp"

#include <SDL3/SDL_audio.h>

#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const char *name)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) {
        ++g_failures;
    }
}

rysual::PcmChunk makeF32Chunk(const std::vector<float>& samples,
                              uint64_t pts_us,
                              int channels)
{
    rysual::PcmChunk chunk;
    chunk.nb_samples = static_cast<int>(samples.size() / channels);
    chunk.pts_us = pts_us;
    chunk.data.resize(samples.size() * sizeof(float));
    std::memcpy(chunk.data.data(), samples.data(), chunk.data.size());
    return chunk;
}

void testRingBuffer()
{
    rysual::RingBuffer ring(8);
    check(ring.capacity() == 8, "RingBuffer capacity");
    check(ring.availableRead() == 0, "RingBuffer initially empty");

    check(ring.write(reinterpret_cast<const uint8_t *>("abcdef"), 6), "RingBuffer write 6");
    check(ring.write(reinterpret_cast<const uint8_t *>("gh"), 2), "RingBuffer fill");
    check(!ring.write(reinterpret_cast<const uint8_t *>("i"), 1), "RingBuffer write on full fails");

    uint8_t out[8] = {};
    check(ring.read(out, 3) == 3 && std::memcmp(out, "abc", 3) == 0,
          "RingBuffer read abc");

    check(ring.write(reinterpret_cast<const uint8_t *>("ijk"), 3), "RingBuffer write with wrap");
    std::memset(out, 0, sizeof(out));
    check(ring.read(out, 8) == 8 && std::memcmp(out, "defghijk", 8) == 0,
          "RingBuffer wrap order");

    ring.write(reinterpret_cast<const uint8_t *>("1234"), 4);
    ring.clear();
    check(ring.availableRead() == 0, "RingBuffer clear");
    check(ring.availableWrite() == 8, "RingBuffer clear frees space");
}

void testAudioTapF32()
{
    rysual::AudioTap tap;
    check(!tap.valid(), "AudioTap initially invalid");

    std::vector<float> out;
    check(!tap.readWindow(0, 1, out), "AudioTap read before configure fails");
    check(tap.configure(1000, 1, SDL_AUDIO_F32, 8), "AudioTap configure F32");

    check(tap.push(makeF32Chunk({1, 2, 3, 4}, 0, 1)), "AudioTap push chunk 1");
    check(tap.readWindow(4000, 4, out) &&
          out.size() == 4 && out[0] == 1 && out[3] == 4,
          "AudioTap read chunk 1");

    check(tap.push(makeF32Chunk({5, 6, 7, 8}, 4000, 1)), "AudioTap push chunk 2");
    check(tap.readWindow(8000, 8, out) && out.size() == 8 && out[7] == 8,
          "AudioTap read full history");

    check(tap.push(makeF32Chunk({9, 10, 11, 12}, 8000, 1)), "AudioTap overwrite");
    check(tap.readWindow(12000, 8, out) && out.size() == 8 &&
          out[0] == 5 && out[7] == 12,
          "AudioTap keeps newest samples");

    tap.clear();
    check(tap.size() == 0, "AudioTap clear");
    check(tap.readWindow(12000, 4, out) &&
          out.size() == 4 && out[0] == 0 && out[3] == 0,
          "AudioTap silence after clear");
}

void testAudioTapS16()
{
    rysual::AudioTap tap;
    check(tap.configure(1000, 2, SDL_AUDIO_S16, 4), "AudioTap configure S16 stereo");

    rysual::PcmChunk chunk;
    chunk.nb_samples = 2;
    chunk.pts_us = 0;
    const int16_t interleaved[] = {1000, 3000, -1000, -3000};
    chunk.data.resize(sizeof(interleaved));
    std::memcpy(chunk.data.data(), interleaved, sizeof(interleaved));

    check(tap.push(chunk), "AudioTap push S16 stereo");

    std::vector<float> out;
    check(tap.readWindow(2000, 2, out), "AudioTap read S16 stereo");
    const float expected = 2000.0f / 32768.0f;
    check(out.size() == 2 &&
          std::fabs(out[0] - expected) < 1e-6f &&
          std::fabs(out[1] + expected) < 1e-6f,
          "AudioTap S16 mono downmix");

    check(!tap.configure(1000, 1, SDL_AUDIO_UNKNOWN, 8),
          "AudioTap rejects unsupported format");
}

void testFFT()
{
    rysual::FFT fft(8);
    std::vector<float> impulse(8, 0.f);
    impulse[0] = 1.f;

    std::vector<std::complex<float>> output;
    fft.forward(impulse.data(), output);
    check(output.size() == 5, "FFT output size");

    bool flat = true;
    for (const auto& value : output) {
        if (std::fabs(std::abs(value) - 1.f) > 1e-4f) {
            flat = false;
        }
    }
    check(flat, "FFT impulse has flat magnitude");

    rysual::FFT fft64(64);
    std::vector<float> sine(64);
    for (size_t n = 0; n < sine.size(); ++n) {
        sine[n] = std::sin(
            2.f * std::numbers::pi_v<float> * 4.f
            * static_cast<float>(n) / static_cast<float>(sine.size()));
    }
    fft64.forward(sine.data(), output);
    check(output.size() == 33, "FFT sine output size");
    check(std::abs(output[4]) > 30.f, "FFT sine peak at expected bin");
    check(std::abs(output[3]) < 1.f, "FFT sine has low neighbor bin");

    bool threw = false;
    try {
        rysual::FFT invalid(10);
        (void)invalid;
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "FFT rejects non power-of-two size");
}

void testFftBands()
{
    constexpr size_t kFftSize = 2048;
    constexpr int kSampleRate = 44100;

    const std::vector<float> hann = rysual::makeHannWindow(kFftSize);
    const std::vector<rysual::FftBand> bands =
        rysual::makeFftBands(kSampleRate, kFftSize, 48);
    check(bands.size() == 48, "FFT band count");

    rysual::FFT fft(kFftSize);
    std::vector<float> sine(kFftSize);
    for (size_t n = 0; n < sine.size(); ++n) {
        sine[n] = std::sin(
            2.f * std::numbers::pi_v<float> * 440.f
            * static_cast<float>(n) / static_cast<float>(kSampleRate));
    }

    std::vector<float> windowed;
    rysual::applyHann(sine, hann, windowed);

    std::vector<std::complex<float>> spectrum;
    fft.forward(windowed.data(), spectrum);

    std::vector<float> levels(bands.size(), 0.f);
    rysual::updateBandLevels(spectrum, bands, kFftSize, levels);

    float max_level = 0.f;
    for (float level : levels) {
        max_level = std::max(max_level, level);
    }
    check(max_level > 0.2f, "FFT bands detect 440 Hz tone");

    std::vector<float> silence(kFftSize, 0.f);
    rysual::applyHann(silence, hann, windowed);
    fft.forward(windowed.data(), spectrum);
    std::fill(levels.begin(), levels.end(), 0.f);
    rysual::updateBandLevels(spectrum, bands, kFftSize, levels);

    float silence_max = 0.f;
    for (float level : levels) {
        silence_max = std::max(silence_max, level);
    }
    check(silence_max < 0.01f, "FFT bands stay silent on silence");
}

} // namespace

int main()
{
    testRingBuffer();
    testAudioTapF32();
    testAudioTapS16();
    testFFT();
    testFftBands();

    std::printf("failures=%d\n", g_failures);
    return g_failures ? 1 : 0;
}

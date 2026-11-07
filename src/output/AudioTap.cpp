#include "output/AudioTap.hpp"

#include <cmath>
#include <cstring>

namespace rysual
{

bool AudioTap::isFormatSupported(SDL_AudioFormat format) noexcept
{
    switch (format) {
    case SDL_AUDIO_U8:
    case SDL_AUDIO_S16:
    case SDL_AUDIO_S32:
    case SDL_AUDIO_F32:
        return true;
    default:
        return false;
    }
}

float AudioTap::decodeSample(const uint8_t *data, SDL_AudioFormat format) noexcept
{
    switch (format) {
    case SDL_AUDIO_U8: {
        uint8_t value = 0;
        std::memcpy(&value, data, sizeof(value));
        return (static_cast<float>(value) - 128.0f) / 128.0f;
    }
    case SDL_AUDIO_S16: {
        int16_t value = 0;
        std::memcpy(&value, data, sizeof(value));
        return static_cast<float>(value) / 32768.0f;
    }
    case SDL_AUDIO_S32: {
        int32_t value = 0;
        std::memcpy(&value, data, sizeof(value));
        return static_cast<float>(
            static_cast<double>(value) / 2147483648.0);
    }
    case SDL_AUDIO_F32: {
        float value = 0.0f;
        std::memcpy(&value, data, sizeof(value));
        return value;
    }
    default:
        return 0.0f;
    }
}

bool AudioTap::configure(int sample_rate, int channels, SDL_AudioFormat format, size_t capacity_samples) {
    if (sample_rate <= 0 || channels <= 0 || capacity_samples == 0) {
        return false;
    }
    if (SDL_AUDIO_BYTESIZE(format) == 0 || !isFormatSupported(format)) {
        return false;
    }

    m_sample_rate = sample_rate;
    m_channels = channels;
    m_format = format;
    m_capacity = capacity_samples;
    m_ring.assign(m_capacity, 0.0f);

    m_write_index = 0;
    m_size = 0;
    m_total_samples = 0;
    m_end_us = 0;
    m_valid = true;
    return true;
}

void AudioTap::clear()
{
    m_write_index = 0;
    m_size = 0;
    m_total_samples = 0;
    m_end_us = 0;
}

bool AudioTap::push(const PcmChunk& chunk)
{
    if (!m_valid) {
        return false;
    }
    if (chunk.nb_samples <= 0 || chunk.data.empty()) {
        return true;
    }

    const size_t sample_bytes = SDL_AUDIO_BYTESIZE(m_format);
    if (sample_bytes == 0) {
        return false;
    }

    // 期望的数据量
    const size_t expected_bytes =
        static_cast<size_t>(chunk.nb_samples)
        * static_cast<size_t>(m_channels)
        * sample_bytes;
    if (chunk.data.size() < expected_bytes) {
        return false;
    }

    const bool first_chunk = (m_total_samples == 0);

    // 下混
    for (int i = 0; i < chunk.nb_samples; ++i) {
        float sum = 0.0f;
        for (int channel = 0; channel < m_channels; ++channel) {
            // stride = m_channels
            size_t stride = static_cast<size_t>(m_channels);
            const size_t offset =
                (static_cast<size_t>(i) * stride + static_cast<size_t>(channel)) * sample_bytes;
            sum += decodeSample(chunk.data.data() + offset, m_format);
        }

        // 平均后写入环形缓冲
        m_ring[m_write_index] = sum / m_channels;
        m_write_index = (m_write_index + 1) % m_capacity;
        if (m_size < m_capacity) {
            ++m_size;
        }
        ++m_total_samples;
    }

    // 时间戳更新
    const int64_t chunk_us = static_cast<int64_t>(chunk.nb_samples) * 1'000'000 / m_sample_rate;

    // flush 产生的 chunk 可能 pts_us == 0，此时按连续时间累加
    if (chunk.pts_us > 0 || first_chunk) {
        m_end_us = static_cast<int64_t>(chunk.pts_us) + chunk_us;
    } else {
        m_end_us += chunk_us;
    }

    return true;
}

bool AudioTap::readWindow(int64_t end_us, size_t count, std::vector<float>& out) const {
    out.clear();

    if (!m_valid) {
        return false;
    }

    out.assign(count, 0.0f);
    if (count == 0 || m_size == 0) {
        return true;
    }

    // 时间差 => 样本差
    const int64_t delta_us = end_us - m_end_us;
    const int64_t delta_samples = static_cast<int64_t>(
        std::llround(static_cast<double>(delta_us) * static_cast<double>(m_sample_rate) / 1'000'000.0));

    // 计算绝对索引
    const int64_t total = static_cast<int64_t>(m_total_samples);
    const int64_t end_index = total + delta_samples;
    const int64_t start_index = end_index - static_cast<int64_t>(count);
    const int64_t oldest_index = total - static_cast<int64_t>(m_size);

    // 读取
    for (size_t i = 0; i < count; ++i) {
        const int64_t index = start_index + static_cast<int64_t>(i);
        if (index < oldest_index || index >= total) {
            continue;
        }

        const size_t ring_index = static_cast<size_t>(index) % m_capacity;
        out[i] = m_ring[ring_index];
    }

    return true;
}

} // namespace rysual

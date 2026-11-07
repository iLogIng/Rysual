#pragma once

#include "../media/PcmChunk.hpp"

#include <SDL3/SDL_audio.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rysual
{

// 音频可视化采样窗口
//
// 保存最近一段 PCM，并按时间戳提供非破坏性窗口读取。
// 数据统一转换为 mono float，方便波形和 FFT 分析。
//
// 线程安全: 非线程安全。
//           当前应在播放/渲染同一线程使用。
class AudioTap
{
public:
    AudioTap() = default;
    ~AudioTap() = default;

    AudioTap(const AudioTap&) = delete;
    AudioTap& operator=(const AudioTap&) = delete;

    // sample_rate      输出采样率
    // channels         交错 PCM 的声道数
    // format           PCM 样本格式
    // capacity_samples 环形缓冲容量（单声道样本数）
    bool configure(int sample_rate, int channels, SDL_AudioFormat format, size_t capacity_samples);

    void clear();

    // 将一块交错 PCM 下混为 mono float 并写入历史缓冲；
    // 写满时覆盖最旧数据
    bool push(const PcmChunk& chunk);

    // 读取 end_us 之前 count 个 mono 样本，数据不足时补 0；
    // 不消费历史数据
    bool readWindow(int64_t end_us, size_t count, std::vector<float>& out) const;

    size_t size() const { return m_size; }
    size_t capacity() const { return m_capacity; }
    int sampleRate() const { return m_sample_rate; }
    bool valid() const { return m_valid; }

private:
    static bool isFormatSupported(SDL_AudioFormat format) noexcept;
    static float decodeSample(const uint8_t *data, SDL_AudioFormat format) noexcept;

    std::vector<float> m_ring;

    size_t m_write_index = 0;
    size_t m_size = 0;
    size_t m_capacity = 0;
    size_t m_total_samples = 0;

    int m_sample_rate = 0;
    int m_channels = 0;
    SDL_AudioFormat m_format = SDL_AUDIO_UNKNOWN;

    int64_t m_end_us = 0;
    bool m_valid = false;

}; // class AudioTap

} // namespace rysual

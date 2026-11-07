#pragma once

#include "sync/MasterClock.hpp"

#include <cstdint>

namespace rysual
{

// 音频主时钟
//
// 位置计算：
//   position = base_pts
//            + 已写入帧数 * 1e6 / sample_rate
//            - 输出延迟
//
// 输出延迟 = 队列中的音频时长 + 设备缓冲时长。
// 每帧调用 advance(0, latency) 可以只刷新延迟，让位置随播放推进。
class AudioClock : public MasterClock
{
public:
    AudioClock() = default;
    ~AudioClock() = default;

    AudioClock(AudioClock&&) = delete;
    AudioClock& operator =(AudioClock&&) = delete;

    void reset(int sample_rate, int64_t base_pts_us) override
    {
        m_sample_rate = sample_rate > 0 ? sample_rate : 0;
        m_base_pts_us = base_pts_us;
        m_written_frames = 0;
        m_latency_us = 0;
        m_valid = m_sample_rate > 0;
        m_paused = true;
    }

    bool advance(int64_t frames, int64_t latency_us) override
    {
        if (m_sample_rate <= 0 || frames < 0 || latency_us < 0) {
            return false;
        }

        m_written_frames += frames;
        m_latency_us = latency_us;
        m_valid = true;
        return true;
    }

    void setPaused(bool paused) override
    {
        m_paused = paused;
    }

    int64_t positionUs() const override
    {
        if (!m_valid || m_sample_rate <= 0) {
            return 0;
        }

        const int64_t written_us =
            m_written_frames * 1'000'000 / m_sample_rate;

        const int64_t position =
            m_base_pts_us + written_us - m_latency_us;

        return position > 0 ? position : 0;
    }

    bool paused() const override
    {
        return m_paused || !m_valid;
    }

    bool valid() const override
    {
        return m_valid;
    }

private:
    int64_t m_sample_rate = 0;
    int64_t m_base_pts_us = 0;
    int64_t m_written_frames = 0;
    int64_t m_latency_us = 0;
    bool m_valid = false;
    bool m_paused = true;

}; // class AudioClock

} // namespace rysual

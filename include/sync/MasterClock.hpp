#pragma once

#include <chrono>
#include <cstdint>

namespace rysual
{

// 主时钟抽象
// 音视频同步以主时钟为基准 从时钟负责等待/丢帧
class MasterClock
{
public:
    MasterClock() = default;
    virtual ~MasterClock() = default;

    MasterClock(MasterClock&&) = delete;
    MasterClock& operator =(MasterClock&&) = delete;

    // sample_rate  输出采样率
    // base_pts_us  时钟起点（seek 后为目标位置）
    virtual void reset(int sample_rate, int64_t base_pts_us) = 0;

    // 推进已写入的帧数，并刷新当前输出延迟
    // frames      自上次调用以来写入的帧数，可为 0（仅刷新延迟）
    // latency_us  当前输出延迟：队列时长 + 设备缓冲时长
    virtual bool advance(int64_t frames, int64_t latency_us) = 0;

    virtual void setPaused(bool paused) = 0;

    virtual int64_t positionUs() const = 0;
    virtual bool paused() const = 0;
    virtual bool valid() const = 0;

    static int64_t nowNs() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    static int64_t nowUs() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    static int64_t nowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

}; // class MasterClock

} // namespace rysual

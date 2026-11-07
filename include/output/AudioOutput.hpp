#pragma once

#include "../Log.hpp"
#include "../SdlRAII.hpp"
#include "../media/PcmChunk.hpp"

#include <SDL3/SDL_audio.h>

#include <climits>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace rysual
{

// SDL3 音频输出封装
//
// 职责：
//   - 持有 SDL_AudioStream / 音频设备
//   - 格式协商、PCM 入队、背压查询、暂停/恢复、清空、音量/速度
//
// 不负责：
//   - 解码、重采样、事件轮询、时钟策略
//
// 前置条件：
//   - 调用方已完成 SDL_Init(SDL_INIT_AUDIO)
//   - SDLGuard 的生命周期长于 AudioOutput
class AudioOutput
{
public:
    AudioOutput() = default;
    ~AudioOutput() = default;

    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator =(const AudioOutput&) = delete;

    // 查询设备推荐格式
    static SDL_AudioSpec preferredSpec(SDL_AudioDeviceID device = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK)
    {
        SDL_AudioSpec spec{};
        if (!SDL_GetAudioDeviceFormat(device, &spec, nullptr)) {
            logging::FAILED_LOG("AudioOutput::preferredSpec()", SDL_GetError());
            throw std::runtime_error(SDL_GetError());
        }
        return spec;
    }

    // 打开后设备保持 paused，调用方可先预填数据再 resume()
    void open(const SDL_AudioSpec& spec, SDL_AudioDeviceID device = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK)
    {
        if (m_stream) {
            logging::FAILED_LOG("AudioOutput::open()", "already open");
            throw std::logic_error("AudioOutput::open()::already open");
        }
        if (spec.freq <= 0 || spec.channels <= 0 || SDL_AUDIO_BYTESIZE(spec.format) == 0) {
            logging::FAILED_LOG("AudioOutput::open()", "invalid audio spec");
            throw std::invalid_argument("AudioOutput::open()::invalid audio spec");
        }

        // 音频设备流
        SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(device, &spec, nullptr, nullptr);
        if (!stream) {
            logging::FAILED_LOG("AudioOutput::open()", SDL_GetError());
            throw std::runtime_error(SDL_GetError());
        }
        m_stream.reset(stream);

        // 查询应用侧实际输入格式
        SDL_AudioSpec src_spec{};
        if (!SDL_GetAudioStreamFormat(m_stream.get(), &src_spec, nullptr)) {
            logging::FAILED_LOG("AudioOutput::open()", SDL_GetError());
            close();
            throw std::runtime_error("AudioOutput::open()::SDL_GetAudioStreamFormat()");
        }
        if (src_spec.freq <= 0 || src_spec.channels <= 0 ||
            SDL_AUDIO_BYTESIZE(src_spec.format) == 0) {
            logging::FAILED_LOG("AudioOutput::open()", "invalid stream format");
            close();
            throw std::runtime_error("AudioOutput::open()::invalid stream format");
        }

        m_spec = src_spec;
        m_frames_bytes = static_cast<size_t>(m_spec.channels) * SDL_AUDIO_BYTESIZE(m_spec.format);
        m_bytes_per_second = static_cast<size_t>(m_spec.freq) * m_frames_bytes;
        m_device = SDL_GetAudioStreamDevice(m_stream.get());

        // 设备缓冲大小仅用于延迟估算，查询失败不影响播放
        SDL_AudioSpec device_spec{};
        int device_buffer_frames = 0;
        if (m_device != 0 &&
            SDL_GetAudioDeviceFormat(m_device, &device_spec, &device_buffer_frames)) {
            m_device_buffer_frames = device_buffer_frames;
            if (device_spec.freq > 0 && device_buffer_frames > 0) {
                // 设备缓冲帧数位于设备采样率，必须用设备侧 freq 换算时长
                m_device_buffer_duration_us =
                    static_cast<int64_t>(device_buffer_frames)
                    * 1'000'000 / device_spec.freq;
            }
        }
    }

    // 可从任意状态调用，幂等
    void close() noexcept
    {
        m_stream.reset();
        m_spec = {};
        m_device = 0;
        m_frames_bytes = 0;
        m_bytes_per_second = 0;
        m_device_buffer_frames = 0;
        m_device_buffer_duration_us = 0;
        m_speed = 1.0f;
    }

    bool isOpen() const noexcept
    {
        return m_stream != nullptr;
    }

    // PCM 入队；SDL 会复制数据，chunk 可以立即复用
    bool write(const void *data, size_t bytes)
    {
        if (!m_stream) {
            return false;
        }
        if (bytes == 0) {
            return true;
        }
        if (!data || bytes > static_cast<size_t>(INT_MAX)) {
            logging::FAILED_LOG("AudioOutput::write()", "invalid buffer");
            return false;
        }
        if (!SDL_PutAudioStreamData(m_stream.get(), data, static_cast<int>(bytes))) {
            logging::FAILED_LOG("AudioOutput::write()", SDL_GetError());
            return false;
        }
        return true;
    }

    bool write(const PcmChunk& chunk)
    {
        return write(chunk.data.data(), chunk.data.size());
    }

    // 文件结束：让 SDL 把内部转换缓冲输出
    bool endOfStream()
    {
        if (!m_stream) {
            return false;
        }
        if (!SDL_FlushAudioStream(m_stream.get())) {
            logging::FAILED_LOG("AudioOutput::endOfStream()", SDL_GetError());
            return false;
        }
        return true;
    }

    // seek/切歌：丢弃已排队音频
    bool clear()
    {
        if (!m_stream) {
            return false;
        }
        if (!SDL_ClearAudioStream(m_stream.get())) {
            logging::FAILED_LOG("AudioOutput::clear()", SDL_GetError());
            return false;
        }
        return true;
    }

    bool pause()
    {
        if (!m_stream) {
            return false;
        }
        if (!SDL_PauseAudioStreamDevice(m_stream.get())) {
            logging::FAILED_LOG("AudioOutput::pause()", SDL_GetError());
            return false;
        }
        return true;
    }

    bool resume()
    {
        if (!m_stream) {
            return false;
        }
        if (!SDL_ResumeAudioStreamDevice(m_stream.get())) {
            logging::FAILED_LOG("AudioOutput::resume()", SDL_GetError());
            return false;
        }
        return true;
    }

    bool isPaused() const noexcept
    {
        return m_stream && SDL_AudioStreamDevicePaused(m_stream.get());
    }

    bool setGain(float gain)
    {
        if (!m_stream) {
            return false;
        }
        if (!SDL_SetAudioStreamGain(m_stream.get(), gain)) {
            logging::FAILED_LOG("AudioOutput::setGain()", SDL_GetError());
            return false;
        }
        return true;
    }

    // 失败时返回 -1
    float gain() const
    {
        return m_stream ? SDL_GetAudioStreamGain(m_stream.get()) : -1.0f;
    }

    // 变速会同时改变音高，范围 [0.01, 100.0]
    bool setSpeed(float ratio)
    {
        if (!m_stream || ratio < 0.01f || ratio > 100.0f) {
            return false;
        }
        if (!SDL_SetAudioStreamFrequencyRatio(m_stream.get(), ratio)) {
            logging::FAILED_LOG("AudioOutput::setSpeed()", SDL_GetError());
            return false;
        }
        m_speed = ratio;
        return true;
    }

    float speed() const
    {
        return m_stream ? m_speed : 0.0f;
    }

    // 失败时返回 -1
    int queuedBytes() const
    {
        return m_stream ? SDL_GetAudioStreamQueued(m_stream.get()) : -1;
    }

    // 已入队音频时长，失败时返回 -1
    int64_t queuedDurationUs() const
    {
        const int queued = queuedBytes();
        if (queued < 0 || m_bytes_per_second == 0 || m_speed <= 0.0f) {
            return -1;
        }
        const double bytes_per_second =
            static_cast<double>(m_bytes_per_second) * static_cast<double>(m_speed);
        return static_cast<int64_t>(
            static_cast<double>(queued) * 1'000'000.0 / bytes_per_second);
    }

    size_t bytesPerSecond() const noexcept
    {
        return m_bytes_per_second;
    }

    int deviceBufferFrames() const noexcept
    {
        return m_device_buffer_frames;
    }

    int64_t deviceBufferDurationUs() const noexcept
    {
        return m_device_buffer_duration_us;
    }

    // 队列 + 设备缓冲，失败时返回 -1
    int64_t latencyUs() const
    {
        const int64_t queued = queuedDurationUs();
        if (queued < 0) {
            return -1;
        }
        return queued + deviceBufferDurationUs();
    }

    const SDL_AudioSpec& spec() const noexcept
    {
        return m_spec;
    }

    SDL_AudioDeviceID device() const noexcept
    {
        return m_device;
    }

private:
    SDLAudioStreamPtr m_stream = nullptr;
    SDL_AudioSpec m_spec{};
    SDL_AudioDeviceID m_device = 0;

    size_t m_frames_bytes = 0;
    size_t m_bytes_per_second = 0;
    int m_device_buffer_frames = 0;
    int64_t m_device_buffer_duration_us = 0;
    float m_speed = 1.0f;

}; // class AudioOutput

} // namespace rysual

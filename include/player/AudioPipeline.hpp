#pragma once

#include "FFmpegRAII.hpp"
#include "media/AudioDecoder.hpp"
#include "media/PcmChunk.hpp"
#include "media/Resampler.hpp"

#include <SDL3/SDL_audio.h>

namespace rysual
{

class AudioPipeline
{
public:
    AudioPipeline() = default;
    ~AudioPipeline() = default;

    AudioPipeline(AudioPipeline&&) = delete;
    AudioPipeline& operator =(AudioPipeline&&) = delete;

    // 打开解码器，并按输出设备格式配置重采样器
    void open(const AVStream *stream, AVRational time_base, const SDL_AudioSpec& output_spec);

    void close();

    // 处理一个 packet；out 每次调用都会被清空
    // 非音频流 packet 返回 true，out 为空
    bool decode(const AVPacket *packet, std::vector<PcmChunk>& out);

    // 冲刷解码器和重采样器，可能产生多块 PcmChunk
    bool flush(std::vector<PcmChunk>& out);

    // seek 后调用：清空解码器和重采样器内部状态
    bool reset();

    bool isOpen() const { return m_state != State::Closed; }

private:
    enum class State : uint8_t
    {
        Closed,
        Ready,
        Done,
    }; // enum class State

    bool receiveFrames(std::vector<PcmChunk>& out);

    State m_state = State::Closed;
    int m_stream_index = -1;

    AudioDecoder m_decoder;
    Resampler m_resampler;
    AVFramePtr m_frame;

}; // class AudioPipeline

} // namespace rysual

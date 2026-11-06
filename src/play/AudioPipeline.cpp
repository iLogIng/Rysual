#include "player/AudioPipeline.hpp"

#include "Log.hpp"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/rational.h>
}

namespace rysual
{

// 打开解码器，并按输出设备格式配置重采样器
void AudioPipeline::open(const AVStream *stream, AVRational time_base, const SDL_AudioSpec& output_spec)
{
    if (m_state != State::Closed) {
        throw std::logic_error("AudioPipeline::open()::already open");
    }
    if (!stream) {
        throw std::invalid_argument("AudioPipeline::open()::null stream");
    }

    m_stream_index = stream->index;

    try {
        m_decoder.open(stream);
        m_resampler.configure(m_decoder.avCodecContext(), output_spec, time_base);
        m_frame = makeAVFrame();
    } catch (...) {
        m_decoder.close();
        m_resampler.close();
        m_frame.reset();
        m_stream_index = -1;
        throw;
    }

    m_state = State::Ready;
}

void AudioPipeline::close()
{
    if (m_state == State::Closed) {
        return;
    }

    m_decoder.close();
    m_resampler.close();
    m_frame.reset();
    m_stream_index = -1;
    m_state = State::Closed;
}

// 处理一个 packet；out 每次调用都会被清空
// 非音频流 packet 返回 true，out 为空
bool AudioPipeline::decode(const AVPacket *packet, std::vector<PcmChunk>& out)
{
    out.clear();

    if (m_state != State::Ready || !packet) {
        return false;
    }
    if (packet->stream_index != m_stream_index) {
        return true;
    }

    int ret = m_decoder.send(packet);
    while (ret == AVERROR(EAGAIN)) {
        if (!receiveFrames(out)) {
            return false;
        }
        ret = m_decoder.send(packet);
    }
    if (ret < 0 && ret != AVERROR_EOF) {
        logging::FAILED_LOG("AudioPipeline::decode()", "m_decoder.send()");
        return false;
    }
    return receiveFrames(out);
}


bool AudioPipeline::flush(std::vector<PcmChunk>& out)
{
    out.clear();

    if (m_state == State::Closed) {
        return false;
    }
    if (m_state == State::Done) {
        return true;
    }

    // 冲刷解码器
    const int ret = m_decoder.send(nullptr);
    if (ret < 0 && ret != AVERROR_EOF) {
        logging::FAILED_LOG("AudioPipeline::flush()", "m_decoder.send()");
        return false;
    }
    if (!receiveFrames(out)) {
        return false;
    }

    // 冲刷重采样器
    while (true) {
        PcmChunk chunk;
        if (!m_resampler.flush(chunk)) {
            logging::FAILED_LOG("AudioPipeline::flush()", "m_resampler.flush()");
            return false;
        }
        if (chunk.data.empty()) {
            break;
        }
        out.push_back(std::move(chunk));
    }

    m_state = State::Done;
    return true;
}

// seek 后调用：清空解码器和重采样器内部状态
bool AudioPipeline::reset()
{
    if (m_state == State::Closed) {
        return false;
    }

    m_decoder.reset();
    if (!m_resampler.reset()) {
        logging::FAILED_LOG("AudioPipeline::reset()", "m_resampler.reset()");
        return false;
    }

    m_state = State::Ready;
    return true;
}

bool AudioPipeline::receiveFrames(std::vector<PcmChunk>& out)
{
    while (true) {
        const int ret = m_decoder.receive(m_frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return true;
        }
        if (ret < 0) {
            logging::FAILED_LOG("AudioPipeline::receiveFrames()", "m_decoder.receive()");
            return false;
        }

        PcmChunk chunk;
        const bool ok = m_resampler.convert(m_frame.get(), chunk);
        av_frame_unref(m_frame.get());
        if (!ok) {
            logging::FAILED_LOG("AudioPipeline::receiveFrames()", "m_resampler.convert()");
            return false;
        }
        if (!chunk.data.empty()) {
            out.push_back(std::move(chunk));
        }
    }
}

} // namespace rysual


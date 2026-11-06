#pragma once

#include <filesystem>
#include <stdexcept>

#include "FFmpegRAII.hpp"
#include "Log.hpp"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/packet.h>
#include <libavutil/rational.h>
#include <libavutil/error.h>
}

namespace rysual
{

class Demuxer
{
public:
    Demuxer() = default;
    ~Demuxer() = default;

    Demuxer(Demuxer&&) = delete;
    Demuxer& operator =(Demuxer&&) = delete;

    void open(const std::filesystem::path& filepath) {
        m_audio_stream_idx = -1;
        AVFormatContext *raw_avfmt_ctx = nullptr;

        if (avformat_open_input(&raw_avfmt_ctx, filepath.c_str(), nullptr, nullptr) < 0) {
            logging::FAILED_LOG("Demuxer::open()", "avformat_open_input");
            throw std::runtime_error("Demuxer::open()::avformat_open_input");
        }
        m_avfmt_ctx.reset(raw_avfmt_ctx);

        if (avformat_find_stream_info(m_avfmt_ctx.get(), nullptr) < 0) {
            m_avfmt_ctx.reset();
            logging::FAILED_LOG("Demuxer::open()", "avformat_find_stream_info");
            throw std::runtime_error("Demuxer::open()::avformat_find_stream_info");
        }
        m_audio_stream_idx = av_find_best_stream(m_avfmt_ctx.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (m_audio_stream_idx < 0) {
            m_avfmt_ctx.reset();
            logging::FAILED_LOG("Demuxer::open()", "av_find_best_stream");
            throw std::runtime_error("Demuxer::open()::av_find_best_stream");
        }
    }

    void close() {
        m_avfmt_ctx.reset();
        m_audio_stream_idx = -1;
    }

    int64_t durationUs() const {
        if (!m_avfmt_ctx || m_avfmt_ctx->duration == AV_NOPTS_VALUE) {
            return 0;
        }
        return m_avfmt_ctx->duration;
    }

    int readPacket(AVPacket *pkt) {
        if (!pkt || !m_avfmt_ctx) {
            return AVERROR_EOF;
        }
        return av_read_frame(m_avfmt_ctx.get(), pkt);
    }

    const AVStream *audioStreamInfo() const {
        if (!m_avfmt_ctx || m_audio_stream_idx < 0) {
            return nullptr;
        }
        return m_avfmt_ctx->streams[m_audio_stream_idx];
    }

    AVRational audioTimeBase() const {
        if (audioStreamIdx() < 0 || !m_avfmt_ctx) {
            return AVRational{0, 1};
        }
        return audioStreamInfo()->time_base;
    }

    int audioStreamIdx() const { return m_audio_stream_idx; }

    AVFormatContext *avFormatContext() const { return m_avfmt_ctx.get(); }

private:
    AVFormatContextPtr m_avfmt_ctx = nullptr;
    int m_audio_stream_idx = -1;

}; // class Demuxer

} // namespace rysual


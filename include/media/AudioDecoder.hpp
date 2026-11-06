#pragma once

#include "FFmpegRAII.hpp"
#include "Log.hpp"
#include <stdexcept>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/packet.h>
#include <libavutil/error.h>
}

namespace rysual
{

class AudioDecoder
{
public:

    AudioDecoder() = default;
    ~AudioDecoder() = default;

    AudioDecoder(AudioDecoder&&) = delete;
    AudioDecoder& operator =(AudioDecoder&&) = delete;

    void open(const AVStream *avstream) {
        if (!avstream || !avstream->codecpar) {
            logging::FAILED_LOG("AudioDecoder::open()", "invalid stream");
            throw std::invalid_argument("AudioDecoder::open()::invalid stream");
        }

        const AVCodec *codec = avcodec_find_decoder(avstream->codecpar->codec_id);
        if (!codec) {
            logging::FAILED_LOG("AudioDecoder::open()", "avcodec_find_decoder");
            throw std::runtime_error("AudioDecoder::open()::avcodec_find_decoder");
        }
        m_avcodec_ctx.reset(avcodec_alloc_context3(codec));
        if (!m_avcodec_ctx) {
            logging::FAILED_LOG("AudioDecoder::open()", "avcodec_alloc_context3");
            throw std::runtime_error("AudioDecoder::open()::avcodec_alloc_context3");
        }
        if (avcodec_parameters_to_context(m_avcodec_ctx.get(), avstream->codecpar) < 0) {
            m_avcodec_ctx.reset();
            logging::FAILED_LOG("AudioDecoder::open()", "avcodec_parameters_to_context");
            throw std::runtime_error("AudioDecoder::open()::avcodec_parameters_to_context");
        }
        if (avcodec_open2(m_avcodec_ctx.get(), codec, nullptr) < 0) {
            m_avcodec_ctx.reset();
            logging::FAILED_LOG("AudioDecoder::open()", "avcodec_open2");
            throw std::runtime_error("AudioDecoder::open()::avcodec_open2");
        }

        if (m_avcodec_ctx->sample_rate <= 0 || m_avcodec_ctx->sample_fmt == AV_SAMPLE_FMT_NONE) {
            logging::FAILED_LOG("AudioDecoder::open()", "invalid audio parameters");
            throw std::runtime_error("AudioDecoder::open()::invalid audio parameters");
        }
    }

    void close() {
        m_avcodec_ctx.reset();
    }

    int send(const AVPacket *pkt) {
        if (!m_avcodec_ctx) {
            return AVERROR_EOF;
        }
        return avcodec_send_packet(m_avcodec_ctx.get(), pkt);
    }

    int receive(AVFrame *frame) {
        if (!m_avcodec_ctx) {
            return AVERROR_EOF;
        }
        return avcodec_receive_frame(m_avcodec_ctx.get(), frame);
    }

    void reset() {
        if (m_avcodec_ctx) {
            avcodec_flush_buffers(m_avcodec_ctx.get());
        }
    }

    AVCodecContext *avCodecContext() const { return m_avcodec_ctx.get(); }

private:

    AVCodecContextPtr m_avcodec_ctx = nullptr;


}; // class AudioDecoder

} // namespace rysual


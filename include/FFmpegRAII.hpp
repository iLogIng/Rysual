#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
}

#include <memory>
#include <new>

namespace rysual
{

struct AVFormatContextDeleter {
    void operator ()(AVFormatContext *fmt_ctx) const {
        if (fmt_ctx) {
            avformat_close_input(&fmt_ctx);
        }
    }
};

struct AVCodecContextDeleter {
    void operator ()(AVCodecContext *codec_ctx) const {
        if (codec_ctx) {
            avcodec_free_context(&codec_ctx);
        }
    }
};

struct AVPacketDeleter {
    void operator ()(AVPacket *pkt) const {
        if (pkt) {
            av_packet_free(&pkt);
        }
    }
};

struct AVFrameDeleter {
    void operator ()(AVFrame *frame) const {
        if (frame) {
            av_frame_free(&frame);
        }
    }
};

struct SwrContextDeleter {
    void operator ()(SwrContext *swr_ctx) const {
        if (swr_ctx) {
            swr_free(&swr_ctx);
        }
    }
};

using AVFormatContextPtr    = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVCodecContextPtr     = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using AVPacketPtr           = std::unique_ptr<AVPacket, AVPacketDeleter>;
using AVFramePtr            = std::unique_ptr<AVFrame, AVFrameDeleter>;
using SwrContextPtr         = std::unique_ptr<SwrContext, SwrContextDeleter>;

inline AVPacketPtr makeAVPacket() {
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        throw std::bad_alloc();
    }
    return AVPacketPtr(pkt);
}

inline AVFramePtr makeAVFrame() {
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        throw std::bad_alloc();
    }
    return AVFramePtr(frame);
}

} // namespace rysual


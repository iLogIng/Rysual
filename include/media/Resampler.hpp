#pragma once

#include "FFmpegRAII.hpp"
#include "media/PcmChunk.hpp"

#include <SDL3/SDL_audio.h>

namespace rysual
{

// 重采样器
class Resampler
{
public:
    Resampler() = default;
    ~Resampler() = default;

    Resampler(Resampler&&) = delete;
    Resampler& operator =(Resampler&&) = delete;

    // decoder      解码器上下文
    // output_spec  SDL3 输出设备描述
    // time_base    FFmpeg 数据包基准时间类型
    void configure(const AVCodecContext *decoder, const SDL_AudioSpec& output_spec, AVRational time_base);

    bool convert(const AVFrame *frame, PcmChunk& out);

    // 取出重采样器中残留的样本 data 为空表示已经取完
    bool flush(PcmChunk& out);

    bool reset()
    {
        return m_swr_ctx && swr_init(m_swr_ctx.get()) >= 0;
    }

    void close() noexcept
    {
        m_swr_ctx.reset();
    }

    SwrContext *swrContext()    const { return m_swr_ctx.get(); }
    AVSampleFormat outFormat()  const { return m_out_fmt; }
    AVRational timeBase()       const { return m_time_base; }
    int outSampleRate()         const { return m_out_sample_rate; }
    int outChannels()           const { return m_out_channels; }
    int inSampleRate()          const { return m_in_sample_rate; }

private:
    SwrContextPtr m_swr_ctx     = nullptr;              // 软件重采样上下文
    AVRational m_time_base      = {1, AV_TIME_BASE};    // 基准时间
    AVSampleFormat m_out_fmt    = AV_SAMPLE_FMT_NONE;   // PCM采样格式
    int m_out_sample_rate           = 0;            // 输出采样率
    int m_out_channels              = 0;            // 输出声道数
    int m_in_sample_rate            = 0;            // 输入采样率

}; // class Resampler

} // namespace rysual


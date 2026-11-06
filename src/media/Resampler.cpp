#include "media/Resampler.hpp"

#include "Log.hpp"
#include "utils.hpp"

extern "C" {
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
}

namespace rysual
{

// decoder      解码器上下文
// output_spec  SDL3 输出设备描述
// time_base    FFmpeg 数据包基准时间类型
void Resampler::configure(const AVCodecContext *decoder, const SDL_AudioSpec& output_spec, AVRational time_base) {
    // 前处理
    if (!decoder) {
        logging::FAILED_LOG("Resampler::configure()", "AVCodecContext*");
        throw std::invalid_argument("Resampler::configure()::AVCodecContext");
    }
    if (decoder->sample_rate <= 0 || decoder->sample_fmt == AV_SAMPLE_FMT_NONE) {
        logging::FAILED_LOG("Resampler::configure()", "invalid decoder audio parameters");
        throw std::invalid_argument("Resampler::configure()::invalid decoder audio parameters");
    }

    if (time_base.num > 0 && time_base.den > 0) {
        m_time_base = time_base;
    }

    // 输出格式转换 SDL3 => FFmpeg
    m_out_fmt = toAVSampleFormat(output_spec.format);
    if (m_out_fmt == AV_SAMPLE_FMT_NONE) {
        logging::FAILED_LOG("Resampler::configure()", "unsupported output sample format");
        throw std::runtime_error("Resampler::configure():: unsupported output sample format");
    }
    if (output_spec.freq <= 0 || output_spec.channels <= 0) {
        logging::FAILED_LOG("Resampler::configure()", "invalid output audio spec");
        throw std::invalid_argument("Resampler::configure()::invalid output audio spec");
    }

    m_out_sample_rate = output_spec.freq;
    m_out_channels = output_spec.channels;
    m_in_sample_rate = decoder->sample_rate;

    // 输入声道布局可能未指定，此时按声道数使用默认布局
    AVChannelLayout in_chlayout{};
    if (decoder->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
        if (decoder->ch_layout.nb_channels <= 0) {
            logging::FAILED_LOG("Resampler::configure()", "invalid input channel layout");
            throw std::invalid_argument("Resampler::configure()::invalid input channel layout");
        }
        av_channel_layout_default(&in_chlayout, decoder->ch_layout.nb_channels);
    } else if (av_channel_layout_copy(&in_chlayout, &decoder->ch_layout) < 0) {
        logging::FAILED_LOG("Resampler::configure()", "av_channel_layout_copy");
        throw std::runtime_error("Resampler::configure()::av_channel_layout_copy()");
    }

    // 声道数据排布格式
    AVChannelLayout out_chlayout{};
    av_channel_layout_default(&out_chlayout, m_out_channels);

    SwrContext *swr_ctx = nullptr;
    // 1 重采样操作上下文
    // 3 输出 声道数 采样格式 采样率
    // 3 输入 声道数 采样格式 采样率
    // 2 日志输出
    int ret = swr_alloc_set_opts2(
            &swr_ctx,
            &out_chlayout, m_out_fmt, m_out_sample_rate,
            &in_chlayout, decoder->sample_fmt, m_in_sample_rate,
            0, 0);
    // 恢复默认排布
    av_channel_layout_uninit(&out_chlayout);
    av_channel_layout_uninit(&in_chlayout);
    if (ret < 0 || !swr_ctx) {
        if (swr_ctx) {
            swr_free(&swr_ctx);
        }
        logging::FAILED_LOG("Resampler::configure()", "swr_alloc_set_opts2");
        throw std::runtime_error("Resampler::configure()::swr_alloc_set_opts2()");
    }

    m_swr_ctx.reset(swr_ctx);
    if (swr_init(m_swr_ctx.get()) < 0) {
        m_swr_ctx.reset();
        logging::FAILED_LOG("Resampler::configure()", "can not init swr context");
        throw std::runtime_error("Resampler::configure():: can not init swr context");
    }
}

bool Resampler::convert(const AVFrame *frame, PcmChunk& out) {
    out.data.clear();
    out.nb_samples = 0;
    out.pts_us = 0;

    if (!frame || !m_swr_ctx || m_in_sample_rate <= 0) {
        return false;
    }

    // 重采样后的最大可能样本数
    const int out_max_sample = static_cast<int>(av_rescale_rnd(
        swr_get_delay(m_swr_ctx.get(), m_in_sample_rate) + frame->nb_samples,
        m_out_sample_rate, m_in_sample_rate, AV_ROUND_UP));
    if (out_max_sample <= 0) {
        return true;
    }
    // 输出缓冲长度: 采样数 * 声道数 * 每采样字节数
    const int out_buffer_size = av_samples_get_buffer_size(nullptr,
        m_out_channels, out_max_sample, m_out_fmt, 1);
    if (out_buffer_size <= 0) {
        return false;
    }
    out.data.resize(static_cast<size_t>(out_buffer_size));
    uint8_t *out_data = out.data.data();

    // 采样率转换
    const int converted_samples = swr_convert(
        m_swr_ctx.get(),
        &out_data, out_max_sample,
        frame->extended_data, frame->nb_samples);

    if (converted_samples < 0) {
        out.data.clear();
        return false;
    }
    if (converted_samples == 0) {
        out.data.clear();
        return true;
    }

    // 实际的样本数量
    const int actual_size = av_samples_get_buffer_size(
        nullptr, m_out_channels, converted_samples, m_out_fmt, 1);
    if (actual_size < 0) {
        out.data.clear();
        return false;
    }

    out.data.resize(actual_size);
    out.nb_samples = converted_samples;

    const int64_t timestamp = frame->best_effort_timestamp != AV_NOPTS_VALUE
                            ? frame->best_effort_timestamp
                            : frame->pts;

    if (timestamp != AV_NOPTS_VALUE && m_time_base.num > 0 && m_time_base.den > 0) {
        out.pts_us = av_rescale_q(timestamp, m_time_base, AV_TIME_BASE_Q);
    } else {
        out.pts_us = 0;
    }
    return true;
}

// 取出重采样器中残留的样本 data 为空表示已经取完
bool Resampler::flush(PcmChunk& out) {
    out.data.clear();
    out.nb_samples = 0;
    out.pts_us = 0;

    if (!m_swr_ctx) {
        return false;
    }

    const int out_max_sample = swr_get_out_samples(m_swr_ctx.get(), 0);
    if (out_max_sample < 0) {
        return false;
    }
    if (out_max_sample == 0) {
        return true;
    }

    const int out_buffer_size = av_samples_get_buffer_size(nullptr,
        m_out_channels, out_max_sample, m_out_fmt, 1);
    if (out_buffer_size <= 0) {
        return false;
    }
    out.data.resize(static_cast<size_t>(out_buffer_size));
    uint8_t *out_data = out.data.data();

    // 转换
    const int converted_samples = swr_convert(
        m_swr_ctx.get(), &out_data, out_max_sample, nullptr, 0);

    if (converted_samples < 0) {
        out.data.clear();
        return false;
    }
    if (converted_samples == 0) {
        out.data.clear();
        return true;
    }

    // 实际的样本数量
    const int actual_size = av_samples_get_buffer_size(
        nullptr, m_out_channels, converted_samples, m_out_fmt, 1);
    if (actual_size < 0) {
        out.data.clear();
        return false;
    }

    out.data.resize(actual_size);
    out.nb_samples = converted_samples;
    return true;
}

} // namespace rysual


#include "player/AudioPlayer.hpp"
#include <libavcodec/packet.h>

namespace rysual
{

AudioPlayer::~AudioPlayer() {
    close();
}

void AudioPlayer::open(const std::filesystem::path& filepath, SDL_AudioDeviceID device) {
    if (m_state != State::Closed) {
        throw std::logic_error("AudioPlayer is already opened");
    }
    try {
        // AVFormatContext
        m_demuxer.open(filepath);
        // SDL Audio
        m_output.open(AudioOutput::preferredSpec(device), device);
        // Audio Pipeline
        m_pipeline.open(m_demuxer.audioStreamInfo(), m_demuxer.audioTimeBase(), m_output.spec());

        m_clock.reset(m_output.spec().freq, 0);

        const SDL_AudioSpec& spec = m_output.spec();
        if (!m_tap.configure(spec.freq, spec.channels, spec.format, static_cast<size_t>(spec.freq) * 2)) {
            logging::ERROR_LOG("AudioPlayer::open()", "AudioTap::configure()");
        }

        m_avpacket = makeAVPacket();
        m_chunks.reserve(8);
        // 1s queue buffer
        m_max_queued_bytes = static_cast<int>(m_output.bytesPerSecond());
        
        m_started = false;
        m_state = State::Ready;
    } catch(...) {
        close();
        throw;
    }
}

AudioPlayer::StepState AudioPlayer::step() {
    if (m_state == State::Closed || m_state == State::Error) {
        return StepState::Error;
    }

    // 每帧刷新输出延迟；即使没有新数据写入，时钟也会随队列消耗而前进
    if (m_output.isOpen()) {
        const int64_t latency = m_output.latencyUs();
        if (latency >= 0) {
            m_clock.advance(0, latency);
        }
    }

    if (m_state == State::Finished) {
        return StepState::Finished;
    }

    // State::Draining
    if (m_state == State::Draining) {
        const int queued = m_output.queuedBytes();
        if (queued < 0) {
            return fail("queuedBytes");
        }
        if (queued == 0) {
            m_state = State::Finished;
            return StepState::Finished;
        }
        return StepState::Draining;
    }

    if (m_state == State::Ready || m_state == State::Paused) {
        return StepState::Ok;
    }

    // State::Playing
    static constexpr int kMaxPacketPerStep = 64;
    for (int i = 0; i < kMaxPacketPerStep; ++i) {
        const int queued = m_output.queuedBytes();
        if (queued < 0) {
            return fail("queuedBytes");
        }
        if (queued >= m_max_queued_bytes) {
            break;
        }

        const int ret = m_demuxer.readPacket(m_avpacket.get());

        if (ret == AVERROR_EOF) {
            if (!m_pipeline.flush(m_chunks)) {
                return fail("pipline.flush");
            }
            if (!queueChunks()) {
                return fail("queuedChunks");
            }
            if (!m_output.endOfStream()) {
                return fail("output.endOfStream");
            }
            m_state = State::Draining;
            break;
        }

        if (ret < 0) {
            return fail("readPacket");
        }
        const bool ok = m_pipeline.decode(m_avpacket.get(), m_chunks);
        av_packet_unref(m_avpacket.get());
        if (!ok) {
            return fail("pipeline.decode");
        }
        if (!queueChunks()) {
            return fail("queueChunks");
        }
    }

    return StepState::Ok;
}


// 预缓冲
bool AudioPlayer::queueChunks() {
    for (const auto& chunk : m_chunks) {
        if (!m_output.write(chunk)) {
            return false;
        }

        const int64_t latency = m_output.latencyUs();
        if (latency >= 0) {
            m_clock.advance(chunk.nb_samples, latency);
        }

        // 可视化数据缺失不应中断播放
        m_tap.push(chunk);

        if (!m_started) {
            if (!m_output.resume()) {
                return false;
            }
            m_started = true;
        }
    }
    return true;
}

bool AudioPlayer::play() {
    if (m_state == State::Closed || m_state == State::Error || m_state == State::Finished) {
        return false;
    }
    m_state = State::Playing;
    if (m_started && m_output.isPaused()) {
        if (!m_output.resume()) {
            return false;
        }
    }
    m_clock.setPaused(false);
    return true;
}

bool AudioPlayer::pause() {
    if (m_state != State::Playing) {
        return false;
    }
    if (!m_output.pause()) {
        return false;
    }
    m_state = State::Paused;
    m_clock.setPaused(true);
    return true;
}

bool AudioPlayer::togglePause() {
    if (m_state == State::Playing) {
    return pause();
    }
    if (m_state == State::Paused || m_state == State::Ready) {
    return play();
    }
    return false;
}

int64_t AudioPlayer::positionUs() const {
    return m_clock.positionUs();
}

int64_t AudioPlayer::bufferedDurationUs() const {
    const int64_t latency = m_output.latencyUs();
    return latency < 0 ? 0 : latency;
}

int64_t AudioPlayer::durationUs() const {
    return m_demuxer.durationUs();
}

void AudioPlayer::close() {
    m_clock.reset(0, 0);
    m_tap.clear();
    m_pipeline.close();
    m_output.close();
    m_demuxer.close();
    m_avpacket.reset();
    m_started = false;
    m_state = State::Closed;
    m_max_queued_bytes = 0;
    m_chunks.clear();
}

bool AudioPlayer::seek(int64_t position_us) {
    if (m_state == State::Closed || m_state == State::Finished) {
        return false;
    }
    if (position_us < 0) {
        position_us = 0;
    }
    const int64_t duration = m_demuxer.durationUs();
    const bool at_end = duration > 0 && position_us >= duration;
    if (at_end) {
        position_us = duration;
    }
    if (duration > 0 && position_us > duration) {
        position_us = duration;
    }
    const bool was_playing = (m_state == State::Playing);

    m_started = false;
    if (!m_output.pause()) {
        return false;
    }
    if (!m_output.clear()) {
        return false;
    }

    // 到达结尾: 不进入正常 seek 流程，直接进入收尾状态
    if (at_end) {
        m_chunks.clear();
        m_tap.clear();
        av_packet_unref(m_avpacket.get());

        m_clock.reset(m_output.spec().freq, duration);
        m_clock.setPaused(false);

        // 下一次 step() 会返回 Finished，主循环自动退出
        m_state = State::Finished;
        return true;
    }

    if (!m_demuxer.seek(position_us)) {
        return false;
    }
    if (!m_pipeline.reset()) {
        m_state = State::Error;
        return false;
    }

    av_packet_unref(m_avpacket.get());
    m_chunks.clear();
    m_tap.clear();
    m_clock.reset(m_output.spec().freq, position_us);
    m_clock.setPaused(!was_playing);

    if (m_state == State::Draining || m_state == State::Finished) {
        m_state = was_playing ? State::Playing : State::Paused;
    }

    return true;
}

} // namespace rysual


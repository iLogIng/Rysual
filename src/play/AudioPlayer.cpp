#include "player/AudioPlayer.hpp"

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
        m_output.open(AudioOutput::preferredSpec(), device);
        // Audio Pipeline
        m_pipeline.open(m_demuxer.audioStreamInfo(), m_demuxer.audioTimeBase(), m_output.spec());

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
        return m_output.resume();
    }
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
    return true;
}

void AudioPlayer::close() {
    m_pipeline.close();
    m_output.close();
    m_demuxer.close();
    m_avpacket.reset();
    m_started = false;
    m_state = State::Closed;
    m_max_queued_bytes = 0;
    m_chunks.clear();
}

} // namespace rysual


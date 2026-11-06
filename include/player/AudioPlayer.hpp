#pragma once

#include "FFmpegRAII.hpp"
#include "Log.hpp"
#include "media/Demuxer.hpp"
#include "media/PcmChunk.hpp"
#include "output/AudioOutput.hpp"
#include "player/AudioPipeline.hpp"

#include <SDL3/SDL_audio.h>

#include <filesystem>

namespace rysual
{

class AudioPlayer
{
public:
    AudioPlayer() = default;
    ~AudioPlayer();

    AudioPlayer(AudioPlayer&&) = delete;
    AudioPlayer& operator =(AudioPlayer&&) = delete;

    enum class StepState
    {
        Ok,         // 正常推进
        Draining,   // EOF 正在清空缓冲队列
        Finished,   // 播放完毕
        Error,      // 错误
    };

    void open(const std::filesystem::path& filepath, SDL_AudioDeviceID device = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK);

    bool isOpen() const { return m_pipeline.isOpen(); }

    bool isPlaying() const { return m_state == State::Playing; }

    bool isPaused() const { return m_state == State::Paused; }

    bool isFinished() const { return m_state == State::Finished; }

    StepState step();

    // 预缓冲
    bool queueChunks();

    bool setSpeed(float ratio) { return m_output.setSpeed(ratio); }

    bool setGain(float gain) { return m_output.setGain(gain); }

    float gain() const { return m_output.gain(); }

    // 播放控制
    bool play();

    bool pause();

    bool togglePause();

    // 状态查询
    int64_t positionUs() const;

    int64_t bufferedDurationUs() const { return m_output.deviceBufferDurationUs(); }

    int64_t durationUs() const { return positionUs() + m_output.deviceBufferDurationUs(); }

    bool seek(int64_t position_us);

    void close();

private:
    enum class State
    {
        Closed,
        Ready,
        Playing,
        Paused,
        Draining,
        Finished,
        Error,
    };

    StepState fail(const char *what) {
        logging::FAILED_LOG("AudioPlayer", what);
        m_state = State::Error;
        return StepState::Error;
    }

    Demuxer m_demuxer;
    AudioOutput m_output;
    AudioPipeline m_pipeline;

    AVPacketPtr m_avpacket;
    std::vector<PcmChunk> m_chunks;

    State m_state = State::Closed;
    int m_max_queued_bytes = 0;
    bool m_started = false;
}; // class AudioPlayer

} // namespace rysual


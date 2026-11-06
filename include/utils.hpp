#pragma once

extern "C" {
#include <libavutil/samplefmt.h>
}

#include <SDL3/SDL_audio.h>


inline AVSampleFormat toAVSampleFormat(SDL_AudioFormat format) {
    switch (format) {
        case SDL_AUDIO_U8: return AV_SAMPLE_FMT_U8;
        case SDL_AUDIO_S16: return AV_SAMPLE_FMT_S16;
        case SDL_AUDIO_S32: return AV_SAMPLE_FMT_S32;
        case SDL_AUDIO_F32: return AV_SAMPLE_FMT_FLT;
        default: return AV_SAMPLE_FMT_NONE;
    };
}

inline SDL_AudioFormat toSdlAudioFormat(AVSampleFormat format) {
    switch (format) {
        case AV_SAMPLE_FMT_U8: return SDL_AUDIO_U8;
        case AV_SAMPLE_FMT_S16: return SDL_AUDIO_S16;
        case AV_SAMPLE_FMT_S32: return SDL_AUDIO_S32;
        case AV_SAMPLE_FMT_FLT: return SDL_AUDIO_F32;
        default: return SDL_AUDIO_UNKNOWN;
    };
}



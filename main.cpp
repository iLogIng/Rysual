#include "Log.hpp"
#include "SdlRAII.hpp"
#include "player/AudioPlayer.hpp"
#include "visual/Visualize.hpp"

#include <SDL3/SDL.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_timer.h>

#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

constexpr float kGainStep = 0.0500f;
constexpr int64_t kSeekStepUs = 5'000'000;

int main(int argc, char *argv[]) try
{
    std::vector<fs::path> playlist;

    const auto is_audio_file = [](const fs::directory_entry& entry) {
        if (!fs::is_regular_file(entry)) {
            return false;
        }
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return std::tolower(c); });
        return ext == ".mp3" || ext == ".mp4" || 
               ext == ".flac" || ext == ".wav";
    };
    const auto process_directory = [&is_audio_file, &playlist](auto&& iterator) {
        for (const auto &entry : iterator) {
            if (is_audio_file(entry)) {
                playlist.emplace_back(entry.path());
            }
        }
    };

    for (int i = 1; i < argc; ++i) {
        fs::path path(argv[i]);
        bool need_recusive = false;
        if (!fs::exists(path)) {
            continue;
        }
        if (fs::is_regular_file(path)) {
            playlist.emplace_back(std::move(path));
            continue;
        }
        if (::strcmp(argv[i], "-r")) {
            need_recusive = true;
            ++i;
        }
        if (fs::is_directory(path)) {
            try {
                if (need_recusive) {
                    process_directory(fs::recursive_directory_iterator(path));
                }
                else {
                    process_directory(fs::directory_iterator(path));
                }
            } catch (const fs::filesystem_error& e) {
                std::cerr << "failed: directory traveling, " << e.what() << "\n";
            }
            continue;
        }
    }

    if (playlist.empty()) {
        rysual::logging::FAILED_LOG("main()", "invalid argument");
        std::cerr
            << "Usage: "
            << fs::path(argv[0]).filename().string()
            << " <file|[-r ]directory...>\n";
        return 1;
    }
    else {
        std::cerr << "Get " << playlist.size() << " Music\n";
    }
    rysual::logging::INFO_LOG("main()", "rysual");

    int winWidth = 1080;
    int winHeight = 720;

    // SDL 初始化
    rysual::SDLGuard sdl_guard;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        rysual::logging::FAILED_LOG("main()", "SDL_Init()");
        return 1;
    }
    rysual::SDLWindowPtr window(SDL_CreateWindow("Rysual", winWidth, winHeight, SDL_WINDOW_RESIZABLE));
    if (!window) {
        rysual::logging::FAILED_LOG("main()", "SDL_CreateWindow()");
        return 1;
    }
    rysual::SDLRendererPtr renderer(SDL_CreateRenderer(window.get(), nullptr));
    if (!renderer) {
        rysual::logging::FAILED_LOG("main()", "SDL_CreateRenderer()");
        return 1;
    }

    // Audio Player
    rysual::AudioPlayer player;
    rysual::Visualize visualize;
    size_t playlist_idx = 0;

    const auto print_song_info = [&playlist, &playlist_idx, &player]() -> void {
        const int64_t cur_us = player.durationUs();
        const int64_t cur_ms = cur_us/1'000;
        const int64_t cur_s  = cur_ms/1'000;
        const int64_t cur_m  = cur_s/60;
        const int64_t cur_h  = cur_m/60;
        std::string name = playlist[playlist_idx].filename().string();
        ::fprintf(stderr,
            "[%ld/%ld: %s | "
            "%02ld:%02ld:%02ld.%03ld]\n",
            playlist_idx + 1, playlist.size(), name.c_str(),
            cur_h % 24, cur_m % 60, cur_s % 60, cur_ms % 1'000);
    };

    const auto load_track = [&](size_t index) -> bool {
        float old_gain = player.gain();
        player.close();
        player.open(playlist.at(index));
        player.play();
        player.setGain(old_gain);

        if (!visualize.configure(player.tap().sampleRate())) {
            rysual::logging::FAILED_LOG("main()", "Visualize::configure()");
            return false;
        }

        playlist_idx = index;
        print_song_info();
        return true;
    };

    const auto print_pos_log = [](int64_t position_us) -> void {
        const int64_t cur_us = position_us;
        const int64_t cur_ms = cur_us/1'000;
        const int64_t cur_s  = cur_ms/1'000;
        const int64_t cur_m  = cur_s/60;
        const int64_t cur_h  = cur_m/60;
        ::fprintf(stderr, "[POS: %02ld:%02ld:%02ld.%03ld]\n", cur_h, cur_m % 60, cur_s % 60, cur_ms % 1'000);
    };

    // 第一首
    if (!load_track(playlist_idx)) {
        return 1;
    }

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                switch (event.key.key) {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                case SDLK_1:
                    visualize.setMode(rysual::Visualize::Mode::Spectrum);
                    break;
                case SDLK_2:
                    visualize.setMode(rysual::Visualize::Mode::Waveform);
                    break;
                case SDLK_3:
                    visualize.setMode(rysual::Visualize::Mode::Simulated);
                    break;
                case SDLK_SPACE:
                    player.togglePause();
                    break;
                case SDLK_N:
                    if (!load_track((playlist_idx + 1) % playlist.size())) {
                        return 1;
                    }
                    break;
                case SDLK_P:
                    if (!load_track((playlist_idx + playlist.size() - 1) % playlist.size())) {
                        return 1;
                    }
                    break;
                case SDLK_MINUS: case SDLK_KP_MINUS:
                    player.setGain(std::clamp(player.gain() - kGainStep, 0.f, 1.f));
                    ::fprintf(stderr, "[GAIN: %d%%]\n", static_cast<int>(player.gain() * 100.f));
                    break;
                case SDLK_EQUALS: case SDLK_KP_PLUS:
                    player.setGain(std::clamp(player.gain() + kGainStep, 0.f, 1.f));
                    ::fprintf(stderr, "[GAIN: %d%%]\n", static_cast<int>(player.gain() * 100.f));
                    break;
                case SDLK_COMMA: case SDLK_LEFT: {
                    player.seek(player.positionUs() - kSeekStepUs);
                    print_pos_log(player.positionUs());
                    break;
                 }
                case SDLK_PERIOD: case SDLK_RIGHT: {
                    player.seek(player.positionUs() + kSeekStepUs);
                    print_pos_log(player.positionUs());
                    break;
                }
                default:
                    break;
                }
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                SDL_GetWindowSize(window.get(), &winWidth, &winHeight);
                SDL_SetRenderLogicalPresentation(renderer.get(), winWidth, winHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX);
                break;
            default:
                break;
            }
        }

        const auto result = player.step();
        if (result == rysual::AudioPlayer::StepState::Error) {
            return 1;
        }
        if (result == rysual::AudioPlayer::StepState::Finished) {
            if (!load_track((playlist_idx + 1) % playlist.size())) {
                return 1;
            }
        }


        const float gain = player.gain();
        visualize.setGain(gain >= 0.f ? gain : 1.f);
        visualize.update(player.tap(), player.positionUs());

        const SDL_FRect area = {
            .x = 0.f,
            .y = 0.f,
            .w = static_cast<float>(winWidth),
            .h = static_cast<float>(winHeight),
        };
        visualize.render(renderer.get(), area, 0x212425FF);

        SDL_RenderPresent(renderer.get());
        SDL_Delay(1000 / 60);
    }

    return 0;
}
catch (const std::exception &e)
{
    rysual::logging::FAILED_LOG("main()", e.what());
    return 1;
}

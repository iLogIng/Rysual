#include "Log.hpp"
#include "SdlRAII.hpp"
#include "player/AudioPlayer.hpp"
#include "visual/Visualize.hpp"

#include <SDL3/SDL.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_timer.h>

#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, char *argv[]) try
{
    if (argc < 2) {
        rysual::logging::FAILED_LOG("main()", "invalid argument");
        std::cerr << "Usage: " << argv[0] << " <file>\n";
        return 1;
    }
    rysual::logging::INFO_LOG("main()", "rysual");

    const std::filesystem::path filepath(argv[1]);

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
    player.open(filepath);
    player.play();

    // Visualize
    rysual::Visualize visualize;
    if (!visualize.configure(player.tap().sampleRate())) {
        rysual::logging::FAILED_LOG("main()", "Visualize::configure()");
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
            break;
        }

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

#include "Log.hpp"
#include "SdlRAII.hpp"
#include "draw/rgba.hpp"
#include "player/AudioPlayer.hpp"
#include "draw/draw.hpp"
#include "utils.hpp"

#include <SDL3/SDL.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_render.h>

#include <exception>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

constexpr rysual::Irgba kBackground = 0x212425FF;

int main(int argc, char *argv[]) try
{
    if (argc < 2) {
        rysual::logging::FAILED_LOG("main()", "invalid argument");
        std::cerr << "Usage: " << argv[0] << " <file>\n";
        return 1;
    }
    rysual::logging::INFO_LOG("main()", "rysual");

    fs::path filepath(argv[1]);

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

    bool running = true;

    int64_t preTick = SDL_GetTicks();
    uint64_t frameCount = 0;
    // main loop
    while (running) {
        const int64_t curTick = SDL_GetTicks();
        const int64_t frameTick = curTick - preTick;
        if (frameCount % 25 == 0) {
            SDL_Log("main()::LOOP | frame tick: %2ld ms | frame %4lu", frameTick, frameCount);
        }
        ++frameCount;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
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

        auto result = player.step();
        if (result == rysual::AudioPlayer::StepState::Error) {
            return 1;
        }
        if (result == rysual::AudioPlayer::StepState::Finished) {
            break;
        }

        preTick = curTick;

        SDL_SetRenderDrawColor(renderer.get(), kBackground.r, kBackground.g, kBackground.b, kBackground.a);
        SDL_RenderClear(renderer.get());

        const float kRectCount = 25.f;
        const float kRectWidth = static_cast<float>(winWidth) / kRectCount;
        const float kRectHeight = 300.f;
        SDL_FRect rect = {.x=kRectWidth/2.f, .y=static_cast<float>(winHeight)/2,
                          .w=kRectWidth, .h=kRectHeight};
        rysual::Irgba kColor = 0xFF'FF'FF'FF;

        for (int i = 0; i < static_cast<int>((winWidth + rect.w) / rect.w); ++i) {
            float curHeight = kRectHeight * (std::sin(rect.x + static_cast<float>(curTick)/1'000.f) + 1.f) / 2.f;
            rect.h = curHeight * rysual::scaleFactor(0, kRectHeight, 10.f, 500.f) + 10.f;
            rysual::Irgba curColor = static_cast<uint32_t>((std::sin<uint32_t>(rect.x) + 1.f) / 2.f * kColor);
            rysual::drawRectCR(renderer.get(), rect, curColor);
            rect.x += rect.w;
        }

        SDL_RenderPresent(renderer.get());
        SDL_Delay(1000 / 60);
        SDL_SetRenderDrawColor(renderer.get(), 0x00, 0x00, 0x00, 0x00);
    }

    return 0;
}
catch (const std::exception &e)
{
    rysual::logging::FAILED_LOG("main()", e.what());
    return 1;
}

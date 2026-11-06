#pragma once

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <memory>

namespace rysual
{

struct SDLWindowDeleter
{
    void operator()(SDL_Window *window) const
    {
        if (window) {
            SDL_DestroyWindow(window);
        }
    }
};

struct SDLRendererDeleter
{
    void operator()(SDL_Renderer *renderer) const
    {
        if (renderer) {
            SDL_DestroyRenderer(renderer);
        }
    }
};

struct SDLAudioStreamDeleter
{
    void operator()(SDL_AudioStream *stream) const
    {
        if (stream) {
            SDL_DestroyAudioStream(stream);
        }
    }
};

using SDLWindowPtr      = std::unique_ptr<SDL_Window, SDLWindowDeleter>;
using SDLRendererPtr    = std::unique_ptr<SDL_Renderer, SDLRendererDeleter>;
using SDLAudioStreamPtr = std::unique_ptr<SDL_AudioStream, SDLAudioStreamDeleter>;

// 确保 SDL_Quit() 在所有 SDL 资源之后执行
struct SDLGuard
{
    SDLGuard() = default;
    SDLGuard(const SDLGuard &) = delete;
    SDLGuard &operator=(const SDLGuard &) = delete;

    ~SDLGuard()
    {
        SDL_Quit();
    }
};

}


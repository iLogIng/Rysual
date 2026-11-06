#pragma once

#include <SDL3/SDL_log.h>

namespace rysual
{
namespace logging
{

enum class Category : int
{
    App   = SDL_LOG_CATEGORY_APPLICATION,
    Audio = SDL_LOG_CATEGORY_AUDIO,
    Video = SDL_LOG_CATEGORY_VIDEO,
};

void FAILED_LOG(const char *component, const char *msg);

void ERROR_LOG(const char *component, const char *msg);

void INFO_LOG(const char *component, const char *msg);

} // namespace logging

} // namespace rysual

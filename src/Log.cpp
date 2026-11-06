#include "../include/Log.hpp"
#include <SDL3/SDL_log.h>

namespace rysual
{
namespace logging
{

// [STATUS|module]: message
inline constexpr const char *log_format = "[%s|%s]: %s";

void FAILED_LOG(const char *component, const char *msg) {
    SDL_LogError(static_cast<int>(Category::App), log_format, "FAILED", component, msg);
}
void ERROR_LOG(const char *component, const char *msg) {
    SDL_LogError(static_cast<int>(Category::App), log_format, "ERROR", component, msg);
}
void INFO_LOG(const char *component, const char *msg) {
    SDL_LogInfo(static_cast<int>(Category::App), log_format, "INFO", component, msg);
}

} // namespace logging

} // namespace rysual


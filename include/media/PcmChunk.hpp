#pragma once

#include <vector>
#include <cstdint>

namespace rysual
{

// PCM 数据块
struct PcmChunk
{
    std::vector<uint8_t>    data;
    uint64_t                pts_us      = 0;
    int                     nb_samples  = 0;
}; // struct PcmChunk

} // namespace rysual


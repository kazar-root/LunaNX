// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=8df744e637ab99da163b
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include <algorithm>
#include <cstdint>

namespace lunanx::audio
{

constexpr int ClampTargetBufferMs(int target_buffer_ms)
{
    return std::clamp(target_buffer_ms, 30, 100);
}

constexpr int PrimePacketCount(int target_buffer_ms)
{
    return ClampTargetBufferMs(target_buffer_ms) / 10;
}

constexpr std::uint64_t InitialJitterHoldUs(int target_buffer_ms)
{
    return static_cast<std::uint64_t>(ClampTargetBufferMs(target_buffer_ms)) * 1000;
}

constexpr std::uint64_t ResyncJitterHoldUs(int target_buffer_ms)
{
    return static_cast<std::uint64_t>(
        std::max(30, ClampTargetBufferMs(target_buffer_ms) / 2)) * 1000;
}

} // namespace lunanx::audio

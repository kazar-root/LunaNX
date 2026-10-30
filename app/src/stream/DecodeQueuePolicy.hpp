#pragma once

#include <algorithm>
#include <cstddef>

namespace lunanx::video
{

// Keep roughly 80 ms of complete pictures. The extra frame absorbs short
// retransmission/decode bursts without building a latency-heavy backlog.
constexpr std::size_t MaximumQueuedAccessUnits(int frames_per_second)
{
    const int safe_fps = std::max(1, frames_per_second);
    return static_cast<std::size_t>(
        std::clamp((safe_fps + 11) / 12, 2, 6));
}

constexpr std::size_t ReadyFrameQueueCapacity()
{
    return 3;
}

constexpr std::size_t StartupReadyFrameDepth()
{
    return 2;
}

// Wait only when the ready queue is empty. This adaptive micro-reserve catches
// a decoder completion racing the UI frame without stalling input/render for a
// large part of the refresh interval. Runtime prediction may shorten or skip it.
constexpr int FrameWaitBudgetMicroseconds(int frames_per_second)
{
    const int safe_fps = std::max(1, frames_per_second);
    return std::clamp((1000000 / safe_fps) * 2 / 5, 2500, 6500);
}

} // namespace lunanx::video

#pragma once

#include <cstdint>

namespace lunanx::video {

enum class FrameDecision {
    Present,
    HoldPrevious,
    DropSuperseded,
};

struct FrameTimingPolicy {
    // Half of a 60 Hz frame. This tolerates scheduling quantization without
    // presenting a full frame ahead of the audio-derived media clock.
    int32_t maximum_early_ticks = 750;
};

inline int32_t AdaptiveEarlyToleranceTicks(int32_t producer_interval_ticks)
{
    // Keep normal presentation within roughly 10% of the source period.
    // This prevents consuming a future frame early and then repeating the
    // previous picture on the following display tick.
    if (producer_interval_ticks <= 0)
        return 180;
    return producer_interval_ticks < 900
               ? 90
               : producer_interval_ticks > 3000
                     ? 300
                     : producer_interval_ticks / 10;
}

inline int32_t RecoveryEarlyToleranceTicks(int32_t producer_interval_ticks)
{
    // After one repeated display tick, allow a near-future frame through.
    // This bounds repeat streaks without carrying a permanent extra frame.
    if (producer_interval_ticks <= 0)
        return 500;
    const int32_t tolerance = producer_interval_ticks / 3;
    return tolerance < 300 ? 300 : tolerance > 600 ? 600 : tolerance;
}

inline int32_t RtpDelta(uint32_t timestamp, uint32_t target)
{
    return static_cast<int32_t>(timestamp - target);
}

inline FrameDecision DecideFrame(uint32_t front_timestamp,
                                 bool has_next,
                                 uint32_t next_timestamp,
                                 bool has_previous,
                                 uint32_t target_timestamp,
                                 const FrameTimingPolicy& policy = {})
{
    if (has_previous && RtpDelta(front_timestamp, target_timestamp) > policy.maximum_early_ticks)
        return FrameDecision::HoldPrevious;

    // If another frame is already due, the front frame missed its display
    // opportunity. Presenting it would add latency and create a catch-up burst.
    if (has_next && RtpDelta(next_timestamp, target_timestamp) <= policy.maximum_early_ticks)
        return FrameDecision::DropSuperseded;

    return FrameDecision::Present;
}

} // namespace lunanx::video

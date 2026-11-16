// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=474466336e4d35bf92c6
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include "controller_delivery_policy.hpp"

namespace lunanx::input
{

struct StreamMenuState
{
    bool toggle_stats = false;
    bool start = false;
    bool guide = false;
    bool back = false;
};

// Réserve + et − jusqu'à leur relâchement après un accord. Le léger délai sur −
// permet aussi de reconnaître les deux touches pressées à quelques images d'écart.
class StreamMenuButtons
{
  public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    StreamMenuState Update(bool plus, bool minus, TimePoint now)
    {
        if (plus && minus && !suppress_)
        {
            suppress_ = true;
            plus_was_down_ = minus_was_down_ = false;
            start_pulse_ = {};
            back_pulse_ = {};
            return {.toggle_stats = true};
        }
        if (suppress_)
        {
            if (!plus && !minus)
                suppress_ = false;
            return {};
        }
        if (plus && !plus_was_down_)
        {
            plus_at_ = now;
            plus_long_ = false;
        }
        if (plus && now - plus_at_ >= std::chrono::milliseconds(500))
            plus_long_ = true;
        if (!plus && plus_was_down_ && !plus_long_)
            start_pulse_.Queue(now);
        if (minus && !minus_was_down_)
            minus_at_ = now;
        const bool held_back = minus && now - minus_at_ >= std::chrono::milliseconds(100);
        if (!minus && minus_was_down_ && now - minus_at_ < std::chrono::milliseconds(100))
            back_pulse_.Queue(now);
        plus_was_down_ = plus;
        minus_was_down_ = minus;
        return {false, start_pulse_.IsActive(now), plus && plus_long_,
                held_back || back_pulse_.IsActive(now)};
    }

    bool OnReportDelivered(bool start, bool back, TimePoint now)
    {
        const bool start_changed = start && start_pulse_.OnReportDelivered(now);
        const bool back_changed = back && back_pulse_.OnReportDelivered(now);
        return start_changed || back_changed;
    }

  private:
    bool suppress_ = false;
    bool plus_was_down_ = false;
    bool minus_was_down_ = false;
    bool plus_long_ = false;
    TimePoint plus_at_ {}, minus_at_ {};
    StartDeliveryPulse start_pulse_, back_pulse_;
};

} // namespace lunanx::input

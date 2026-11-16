// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=a52055662cea0dda33c8
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include <chrono>

namespace lunanx::input
{

class StartDeliveryPulse
{
  public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    void Queue(TimePoint now)
    {
        pending_ = true;
        delivery_deadline_ = now + std::chrono::seconds(2);
        hold_until_ = {};
    }

    bool IsActive(TimePoint now)
    {
        if (pending_ && now >= delivery_deadline_)
            pending_ = false;
        return pending_ ||
               (hold_until_.time_since_epoch().count() != 0 && now < hold_until_);
    }

    bool OnReportDelivered(TimePoint now)
    {
        if (!pending_ || now >= delivery_deadline_)
            return false;
        pending_ = false;
        hold_until_ = now + std::chrono::milliseconds(180);
        return true;
    }

  private:
    bool pending_ = false;
    TimePoint delivery_deadline_ {};
    TimePoint hold_until_ {};
};

} // namespace lunanx::input

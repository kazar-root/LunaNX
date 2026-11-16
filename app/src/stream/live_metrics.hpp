// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=39d41c501fa564654c0a
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <optional>

namespace lunanx
{

struct StreamCounters
{
    uint64_t received_bytes = 0;
    uint64_t presented_frames = 0;
};

class StreamRateSampler
{
  public:
    using Clock = std::chrono::steady_clock;
    void Update(Clock::time_point now, StreamCounters current)
    {
        const double seconds = std::chrono::duration<double>(now - sampled_at_).count();
        if (!initialized_ || seconds > 5 || current.received_bytes < previous_.received_bytes ||
            current.presented_frames < previous_.presented_frames)
        {
            initialized_ = true;
            ready_ = false;
            sampled_at_ = now;
            previous_ = current;
            return;
        }
        if (seconds < 1)
            return;
        mbps_ = (current.received_bytes - previous_.received_bytes) * 8.0 / seconds / 1000000;
        fps_ = (current.presented_frames - previous_.presented_frames) / seconds;
        ready_ = true;
        sampled_at_ = now;
        previous_ = current;
    }
    bool ready() const { return ready_; }
    double mbps() const { return mbps_; }
    double fps() const { return fps_; }

  private:
    bool initialized_ = false, ready_ = false;
    Clock::time_point sampled_at_ {};
    StreamCounters previous_;
    double mbps_ = 0, fps_ = 0;
};

// Estimation à partir de la décharge réelle, sans supposer une durée nominale.
class BatteryEstimate
{
  public:
    using Clock = std::chrono::steady_clock;
    std::optional<int> Update(Clock::time_point now, double percentage, bool charging)
    {
        if (charging || !std::isfinite(percentage) || percentage <= 0 || percentage > 100)
        {
            samples_.clear();
            return {};
        }
        if (!samples_.empty() && (now - samples_.back().at > std::chrono::seconds(30) ||
                                  now < samples_.back().at || percentage > samples_.back().percentage + 0.5))
            samples_.clear();
        samples_.push_back({now, percentage});
        while (samples_.size() > 1 && now - samples_.front().at > std::chrono::minutes(5))
            samples_.pop_front();
        const double elapsed = std::chrono::duration<double>(now - samples_.front().at).count();
        const double drop = samples_.front().percentage - percentage;
        if (elapsed < 60 || drop < 0.3)
            return {};
        const double minutes = percentage * elapsed / drop / 60;
        if (minutes > 1440)
            return {};
        return std::max(1, static_cast<int>(std::round(minutes)));
    }

  private:
    struct Sample { Clock::time_point at; double percentage; };
    std::deque<Sample> samples_;
};

} // namespace lunanx

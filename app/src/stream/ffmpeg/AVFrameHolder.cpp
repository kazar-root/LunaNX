//
//  DiscoverManager.cpp
//  Moonlight
//
// Created by XITRIX on 31.03.2025.
//

#include "AVFrameHolder.hpp"
#include "VideoFrameTiming.hpp"

#include "borealis.hpp"

#include <cstdlib>

AVFrameQueue::AVFrameQueue() {}

AVFrameQueue::~AVFrameQueue() {
    cleanup();
}

namespace
{

AVFrame* referenceFrame(const AVFrame* source)
{
    if (!source)
        return nullptr;
    return av_frame_clone(source);
}

} // namespace

void AVFrameQueue::push(const AVFrame* item) {
    AVFrame* copy = referenceFrame(item);
    if (!copy) {
        brls::Logger::warning("Frame queue: failed to copy decoded frame");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto now = std::chrono::steady_clock::now();
        if (lastPushAt.time_since_epoch().count() != 0) {
            const int64_t interval_us =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    now - lastPushAt).count();
            // Ignore pauses and duplicate burst completions. The EWMA follows
            // real 30/60 FPS cadence without reacting to a single late frame.
            if (interval_us >= 5000 && interval_us <= 50000) {
                if (observedProducerIntervalUs == 0) {
                    observedProducerIntervalUs = interval_us;
                } else {
                    const int64_t deviation_us =
                        std::llabs(interval_us - observedProducerIntervalUs);
                    observedProducerJitterUs =
                        observedProducerJitterUs == 0
                            ? deviation_us
                            : (observedProducerJitterUs * 7 + deviation_us) / 8;
                    observedProducerIntervalUs =
                        (observedProducerIntervalUs * 7 + interval_us) / 8;
                }
            }
        }
        lastPushAt = now;
        if (item->pts != AV_NOPTS_VALUE) {
            const uint32_t timestamp = static_cast<uint32_t>(item->pts);
            if (hasLastPushedRtpTimestamp) {
                const int32_t interval_ticks =
                    lunanx::video::RtpDelta(timestamp, lastPushedRtpTimestamp);
                // RTP video uses a 90 kHz clock. Ignore duplicates, decode
                // reordering and long discontinuities while following genuine
                // 30-60 FPS source cadence and short server-side frame gaps.
                if (interval_ticks >= 300 && interval_ticks <= 9000) {
                    observedProducerRtpTicks =
                        observedProducerRtpTicks == 0
                            ? interval_ticks
                            : (observedProducerRtpTicks * 7 + interval_ticks) / 8;
                }
            }
            lastPushedRtpTimestamp = timestamp;
            hasLastPushedRtpTimestamp = true;
        }
        queue.push_back(copy);
        generations.push_back(++nextGeneration);
        framesCopiedStat++;

        if (queue.size() > limit) {
            AVFrame* dropped = queue.front();
            queue.pop_front();
            generations.pop_front();
            av_frame_free(&dropped);
            framesDroppedStat++;
            overflowDroppedStat++;
        }
    }
    m_frame_available.notify_one();
}

AVFrame* AVFrameQueue::pop(bool& reused, uint64_t& generation, int64_t target_pts) {
    std::unique_lock<std::mutex> lock(m_mutex);

    if (!presentationStarted && bufferFrame == nullptr &&
        queue.size() < startupDepth) {
        const auto startup_wait = std::min(
            std::chrono::microseconds(5000), emptyWaitBudget * 2);
        briefWaitStat++;
        if (m_frame_available.wait_for(lock, startup_wait, [this] {
                return queue.size() >= startupDepth;
            })) {
            briefWaitRecoveredStat++;
        }
        if (queue.size() < startupDepth) {
            startupHoldStat++;
            reused = false;
            generation = 0;
            return nullptr;
        }
    }

    if (queue.empty() && bufferFrame != nullptr &&
        emptyWaitBudget.count() > 0) {
        auto predicted_wait = emptyWaitBudget;
        if (observedProducerIntervalUs > 0 &&
            lastPushAt.time_since_epoch().count() != 0) {
            const int64_t elapsed_us =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - lastPushAt).count();
            const int64_t until_expected_us =
                observedProducerIntervalUs - elapsed_us;
            const int64_t cap_us = emptyWaitBudget.count();
            const int64_t jitter_guard_us = std::clamp<int64_t>(
                observedProducerJitterUs * 2 + 500, 750, 3000);

            // Wait only when the next decoded frame is expected near the
            // current presentation deadline. The measured deviation adds a
            // bounded guard for bursty server output. A real source pause is
            // still reused immediately instead of blocking every UI frame.
            if (until_expected_us < -cap_us || until_expected_us > cap_us) {
                predicted_wait = std::chrono::microseconds(0);
                predictiveWaitSkippedStat++;
            } else {
                predicted_wait = std::chrono::microseconds(
                    std::clamp<int64_t>(
                        until_expected_us + jitter_guard_us, 750, cap_us));
            }
        }

        if (predicted_wait.count() > 0) {
            briefWaitStat++;
            const auto wait_started_at = std::chrono::steady_clock::now();
            const bool recovered = m_frame_available.wait_for(
                lock, predicted_wait, [this] {
                    return !queue.empty();
                });
            const uint64_t waited_us = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - wait_started_at).count());
            briefWaitTotalUs += waited_us;
            briefWaitMaximumUs = std::max(briefWaitMaximumUs, waited_us);
            if (recovered) {
                briefWaitRecoveredStat++;
            } else {
                briefWaitTimeoutStat++;
            }
        }
    }

    const bool external_clock_present = target_pts != AV_NOPTS_VALUE;
    if (external_clock_present) {
        while (!queue.empty() && queue.front()->pts != AV_NOPTS_VALUE) {
            const bool has_next = queue.size() > 1 && queue[1]->pts != AV_NOPTS_VALUE;
            const int32_t normal_tolerance =
                lunanx::video::AdaptiveEarlyToleranceTicks(
                    observedProducerRtpTicks);
            pacingEarlyToleranceTicks =
                currentConsecutiveReuse > 0
                    ? lunanx::video::RecoveryEarlyToleranceTicks(
                          observedProducerRtpTicks)
                    : normal_tolerance;
            const uint32_t front_timestamp =
                static_cast<uint32_t>(queue.front()->pts);
            lastPacingPhaseTicks = lunanx::video::RtpDelta(
                front_timestamp, static_cast<uint32_t>(target_pts));
            const lunanx::video::FrameTimingPolicy policy {
                pacingEarlyToleranceTicks
            };
            const auto decision = lunanx::video::DecideFrame(
                front_timestamp, has_next,
                has_next ? static_cast<uint32_t>(queue[1]->pts) : 0,
                bufferFrame != nullptr,
                static_cast<uint32_t>(target_pts),
                policy);
            if (decision == lunanx::video::FrameDecision::HoldPrevious) {
                fakeFrameUsedStat++;
                timingHoldStat++;
                currentConsecutiveReuse++;
                maximumConsecutiveReuse =
                    std::max(maximumConsecutiveReuse, currentConsecutiveReuse);
                reused = true;
                generation = bufferGeneration;
                return bufferFrame;
            }
            if (decision != lunanx::video::FrameDecision::DropSuperseded)
                break;
            AVFrame* dropped = queue.front();
            queue.pop_front();
            generations.pop_front();
            av_frame_free(&dropped);
            framesDroppedStat++;
            timingDroppedStat++;
        }
    } else {
        // RTP timestamps preserve ordering, but they are not presentation
        // deadlines until WebRTC supplies a correlated media clock.
        pacingEarlyToleranceTicks = 0;
        lastPacingPhaseTicks = 0;
    }

    if (!queue.empty()) {
        if (bufferFrame)
            av_frame_free(&bufferFrame);

        AVFrame* item = queue.front();
        queue.pop_front();
        bufferGeneration = generations.front();
        generations.pop_front();
        bufferFrame = item;
        presentationStarted = true;
        if (external_clock_present)
            sourceClockPresentStat++;
        else
            clockBypassPresentStat++;
        currentConsecutiveReuse = 0;
        reused = false;
    } else {
        fakeFrameUsedStat++;
        queueEmptyReuseStat++;
        currentConsecutiveReuse++;
        maximumConsecutiveReuse =
            std::max(maximumConsecutiveReuse, currentConsecutiveReuse);
        reused = true;
    }

    generation = bufferGeneration;
    return bufferFrame;
}

size_t AVFrameQueue::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return queue.size();
}

size_t AVFrameQueue::getFakeFrameUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return fakeFrameUsedStat;
}

size_t AVFrameQueue::getFramesDropStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return framesDroppedStat;
}

size_t AVFrameQueue::getTimingDropStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return timingDroppedStat;
}

size_t AVFrameQueue::getOverflowDropStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return overflowDroppedStat;
}

size_t AVFrameQueue::getTimingHoldStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return timingHoldStat;
}

size_t AVFrameQueue::getQueueEmptyReuseStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return queueEmptyReuseStat;
}

size_t AVFrameQueue::getBriefWaitStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return briefWaitStat;
}

size_t AVFrameQueue::getBriefWaitRecoveredStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return briefWaitRecoveredStat;
}

size_t AVFrameQueue::getBriefWaitTimeoutStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return briefWaitTimeoutStat;
}

size_t AVFrameQueue::getPredictiveWaitSkippedStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return predictiveWaitSkippedStat;
}

size_t AVFrameQueue::getStartupHoldStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return startupHoldStat;
}

uint64_t AVFrameQueue::getBriefWaitTotalUs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return briefWaitTotalUs;
}

uint64_t AVFrameQueue::getBriefWaitMaximumUs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return briefWaitMaximumUs;
}

int64_t AVFrameQueue::getObservedProducerIntervalUs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return observedProducerIntervalUs;
}

int64_t AVFrameQueue::getObservedProducerJitterUs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return observedProducerJitterUs;
}

int32_t AVFrameQueue::getObservedProducerRtpTicks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return observedProducerRtpTicks;
}

int32_t AVFrameQueue::getPacingEarlyToleranceTicks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return pacingEarlyToleranceTicks;
}

int32_t AVFrameQueue::getLastPacingPhaseTicks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return lastPacingPhaseTicks;
}

size_t AVFrameQueue::getCurrentConsecutiveReuse() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return currentConsecutiveReuse;
}

size_t AVFrameQueue::getMaximumConsecutiveReuse() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return maximumConsecutiveReuse;
}

size_t AVFrameQueue::getSourceClockPresentStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return sourceClockPresentStat;
}

size_t AVFrameQueue::getClockBypassPresentStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return clockBypassPresentStat;
}

void AVFrameQueue::cleanup() {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!queue.empty()) {
        AVFrame* frame = queue.front();
        queue.pop_front();
        av_frame_free(&frame);
    }
    while (!generations.empty())
        generations.pop_front();

    if (bufferFrame)
        av_frame_free(&bufferFrame);

    fakeFrameUsedStat = 0;
    framesDroppedStat = 0;
    timingDroppedStat = 0;
    overflowDroppedStat = 0;
    timingHoldStat = 0;
    queueEmptyReuseStat = 0;
    briefWaitStat = 0;
    briefWaitRecoveredStat = 0;
    briefWaitTimeoutStat = 0;
    predictiveWaitSkippedStat = 0;
    startupHoldStat = 0;
    briefWaitTotalUs = 0;
    briefWaitMaximumUs = 0;
    framesCopiedStat = 0;
    nextGeneration = 0;
    bufferGeneration = 0;
    bufferFrame = nullptr;
    presentationStarted = false;
    lastPushAt = {};
    observedProducerIntervalUs = 0;
    observedProducerJitterUs = 0;
    hasLastPushedRtpTimestamp = false;
    lastPushedRtpTimestamp = 0;
    observedProducerRtpTicks = 0;
    pacingEarlyToleranceTicks = 180;
    lastPacingPhaseTicks = 0;
    currentConsecutiveReuse = 0;
    maximumConsecutiveReuse = 0;
    sourceClockPresentStat = 0;
    clockBypassPresentStat = 0;
}

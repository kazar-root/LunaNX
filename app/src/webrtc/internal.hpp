#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace lunanx::webrtc::internal
{

bool StartsWith(const std::string& value, const char* prefix);
void RecordLatency(std::array<std::atomic<uint64_t>, 10>& buckets, uint64_t latency_us);
uint64_t LatencyPercentile95(const std::array<std::atomic<uint64_t>, 10>& buckets);

template <typename T>
void AtomicMax(std::atomic<T>& target, T value)
{
    T previous = target.load(std::memory_order_relaxed);
    while (previous < value &&
           !target.compare_exchange_weak(previous, value, std::memory_order_relaxed)) {
    }
}

std::string HexPreview(const uint8_t* data, size_t size, size_t max_bytes = 12);
bool ContainsH264Idr(const uint8_t* data, size_t size);
uint64_t NowUs();

// Journal de session : sdmc:/switch/LunaNX/stream.log (réinitialisé à chaque stream).
void ResetStreamLog();
void AppendStreamLog(const std::string& line);

} // namespace lunanx::webrtc::internal

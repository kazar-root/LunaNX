// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=1f3aa63fcc3ad5659b63
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

struct PeerAudioPacket;

class AudioPipeline {
public:
    AudioPipeline();
    ~AudioPipeline();

    bool start();
    void configure(int volume_percent, int target_buffer_ms);
    void stop();
    void submit(const PeerAudioPacket& packet);
    void set_sender_report(uint32_t ssrc, uint64_t ntp_us, uint32_t rtp_timestamp);
    std::string debug_info() const;
    int64_t playback_ntp_us() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

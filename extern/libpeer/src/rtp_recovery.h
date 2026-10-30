#ifndef RTP_RECOVERY_H_
#define RTP_RECOVERY_H_

#include <stdint.h>

#define RTP_RECOVERY_DEFAULT_GAP_HOLD_MS 52u
#define RTP_RECOVERY_MIN_GAP_HOLD_MS 36u
#define RTP_RECOVERY_MAX_GAP_HOLD_MS 72u
#define RTP_RECOVERY_MIN_NACK_RETRY_MS 10u
#define RTP_RECOVERY_MAX_NACK_RETRY_MS 25u
#define RTP_RECOVERY_MAX_NACK_RETRIES 3u
#define RTP_RECOVERY_MAX_BURST_NACK_RETRIES 4u
#define RTP_RECOVERY_BURST_GAP_DISTANCE 64u
#define RTP_RECOVERY_BURST_MIN_HOLD_MS 56u
#define RTP_RECOVERY_BURST_HOLD_BONUS_MS 12u
#define RTP_RECOVERY_STARTUP_ACCESS_UNITS 180u
#define RTP_RECOVERY_STARTUP_HOLD_BONUS_MS 8u
#define RTP_RECOVERY_STARTUP_REBASE_DISTANCE 128u
#define RTP_RECOVERY_STARTUP_EARLY_REBASE_ACCESS_UNITS 30u
#define RTP_RECOVERY_STARTUP_EARLY_REBASE_DISTANCE 256u
#define RTP_RECOVERY_STARTUP_LATE_REBASE_DISTANCE 384u
#define RTP_RECOVERY_STARTUP_REBASE_LIMIT 2u

static inline uint32_t rtp_recovery_clamp_u32(uint32_t value,
                                              uint32_t minimum,
                                              uint32_t maximum) {
  if (value < minimum)
    return minimum;
  if (value > maximum)
    return maximum;
  return value;
}

static inline uint32_t rtp_recovery_gap_hold_ms(uint32_t rtt_ms,
                                                uint32_t jitter_ms) {
  if (rtt_ms == 0)
    return RTP_RECOVERY_DEFAULT_GAP_HOLD_MS;

  return rtp_recovery_clamp_u32(
      rtt_ms + (jitter_ms * 2u) + 8u,
      RTP_RECOVERY_MIN_GAP_HOLD_MS,
      RTP_RECOVERY_MAX_GAP_HOLD_MS);
}

static inline uint32_t rtp_recovery_burst_gap_hold_ms(
    uint32_t regular_hold_ms, uint32_t rtt_ms, uint32_t jitter_ms) {
  const uint32_t measured_hold_ms =
      rtt_ms == 0
          ? RTP_RECOVERY_BURST_MIN_HOLD_MS
          : rtt_ms + (jitter_ms * 3u) +
                RTP_RECOVERY_BURST_HOLD_BONUS_MS;
  const uint32_t burst_hold_ms = rtp_recovery_clamp_u32(
      measured_hold_ms, RTP_RECOVERY_BURST_MIN_HOLD_MS,
      RTP_RECOVERY_MAX_GAP_HOLD_MS);
  return burst_hold_ms > regular_hold_ms ? burst_hold_ms : regular_hold_ms;
}

static inline uint32_t rtp_recovery_nack_retry_ms(uint32_t rtt_ms) {
  if (rtt_ms == 0)
    return 16u;

  return rtp_recovery_clamp_u32(
      rtt_ms / 3u,
      RTP_RECOVERY_MIN_NACK_RETRY_MS,
      RTP_RECOVERY_MAX_NACK_RETRY_MS);
}

static inline int rtp_recovery_startup_active(
    uint32_t completed_access_units) {
  return completed_access_units < RTP_RECOVERY_STARTUP_ACCESS_UNITS;
}

static inline uint32_t rtp_recovery_startup_gap_hold_ms(
    uint32_t regular_hold_ms, uint32_t completed_access_units) {
  if (!rtp_recovery_startup_active(completed_access_units))
    return regular_hold_ms;

  return rtp_recovery_clamp_u32(
      regular_hold_ms + RTP_RECOVERY_STARTUP_HOLD_BONUS_MS,
      RTP_RECOVERY_MIN_GAP_HOLD_MS, RTP_RECOVERY_MAX_GAP_HOLD_MS);
}

static inline uint16_t rtp_recovery_nack_evidence_packets(
    uint32_t completed_access_units) {
  return rtp_recovery_startup_active(completed_access_units) ? 2u : 3u;
}

static inline int rtp_recovery_should_wait_for_gap(uint16_t distance,
                                                   uint32_t gap_age_ms,
                                                   uint32_t hold_ms,
                                                   uint16_t hard_limit) {
  return distance < hard_limit && gap_age_ms < hold_ms;
}

static inline int rtp_recovery_should_fast_forward(uint16_t distance,
                                                   uint32_t gap_age_ms,
                                                   uint32_t hold_ms,
                                                   uint16_t hard_limit) {
  return distance >= hard_limit || gap_age_ms >= hold_ms;
}

static inline int rtp_recovery_should_rebase_startup(
    uint16_t distance, uint32_t completed_access_units,
    uint32_t startup_rebases) {
  if (!rtp_recovery_startup_active(completed_access_units) ||
      startup_rebases >= RTP_RECOVERY_STARTUP_REBASE_LIMIT)
    return 0;

  uint16_t minimum_distance = RTP_RECOVERY_STARTUP_LATE_REBASE_DISTANCE;
  if (completed_access_units == 0) {
    minimum_distance = RTP_RECOVERY_STARTUP_REBASE_DISTANCE;
  } else if (completed_access_units <
             RTP_RECOVERY_STARTUP_EARLY_REBASE_ACCESS_UNITS) {
    minimum_distance = RTP_RECOVERY_STARTUP_EARLY_REBASE_DISTANCE;
  }
  return distance >= minimum_distance;
}

#endif  // RTP_RECOVERY_H_

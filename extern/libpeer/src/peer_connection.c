#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "agent.h"
#include "config.h"
#include "dtls_srtp.h"
#include "ice_candidate_pair_policy.h"
#include "peer_connection.h"
#include "ports.h"
#include "rtcp.h"
#include "rtp.h"
#include "rtp_recovery.h"
#include "sctp.h"
#include "sdp.h"

#define STATE_CHANGED(pc, curr_state)                                  \
  if (pc->state != curr_state) {                                       \
    pc->state = curr_state;                                            \
    if (pc->oniceconnectionstatechange) {                              \
      pc->oniceconnectionstatechange(curr_state, pc->config.user_data); \
    }                                                                 \
  }

// Per-stream RTP receive statistics (RFC 3550 appendix A.3) for RTCP receiver reports.
typedef struct RtpReceiveStats {
  uint32_t ssrc;
  int initialized;
  uint16_t max_seq;
  uint32_t cycles;
  uint32_t base_seq;
  uint32_t received;
  uint32_t expected_prior;
  uint32_t received_prior;
  uint32_t last_sr_ntp_mid;
  uint32_t last_sr_received_ms;
} RtpReceiveStats;

struct PeerConnection {
  PeerConfiguration config;
  PeerConnectionState state;
  Agent agent;
  DtlsSrtp dtls_srtp;
  Sctp sctp;

  char sdp[CONFIG_SDP_BUFFER_SIZE];

  void (*onicecandidate)(char* sdp, void* user_data);
  void (*oniceconnectionstatechange)(PeerConnectionState state, void* user_data);
  void (*on_connected)(void* userdata);
  void (*on_receiver_packet_loss)(float fraction_loss, uint32_t total_loss, void* user_data);

  uint8_t temp_buf[CONFIG_MTU];
  uint8_t agent_buf[CONFIG_MTU];
  int agent_ret;
  int b_local_description_created;
  int remote_ice_lite;

  RtpEncoder artp_encoder;
  RtpEncoder vrtp_encoder;
  RtpDecoder vrtp_decoder;
  RtpDecoder artp_decoder;

  uint32_t remote_assrc;
  uint32_t remote_vssrc;
  uint32_t remote_vrtx_ssrc;
  int video_payload_type;
  int video_rtx_payload_type;
  uint16_t video_last_nack_sequence;
  uint16_t video_last_nack_bitmask;
  int video_has_last_nack_sequence;
  uint8_t video_nack_retry_count;
  uint8_t video_nack_burst;
  uint32_t video_last_nack_sent_ms;
  uint32_t video_nack_requests;
  uint32_t video_nack_packets_requested;
  uint32_t video_nack_retries;
  uint32_t video_rtx_packets_received;
  uint32_t video_rtx_packets_recovered;
  uint32_t video_rtx_packets_invalid;

  int dtls_handshake_attempts;
  uint32_t dtls_handshake_started_ms;
  int sctp_create_attempted;
  int completed_udp_packets;
  int completed_rtcp_packets;
  int completed_dtls_packets;
  int completed_rtp_packets;
  int completed_unknown_packets;
  int rtp_decrypt_failures;
  int rtcp_decrypt_failures;
  int sctp_read_events;

  // Feedback to the sender: without RR or REMB it can neither measure loss nor
  // ramp its bitrate up.
  RtpReceiveStats video_rx_stats;
  RtpReceiveStats audio_rx_stats;
  uint32_t last_receiver_report_ms;
  uint32_t remb_max_bps;
  uint32_t remb_current_bps;
  uint32_t receiver_reports_sent;
};

static int peer_connection_send_rtcp_nack(PeerConnection* pc, uint32_t ssrc,
                                          uint16_t packet_id, uint16_t bitmask);
static void peer_connection_maybe_send_video_nack(PeerConnection* pc);

static void peer_connection_diag_log(const char* fmt, ...) {
  FILE* file = fopen("sdmc:/switch/LunaNX/signaling.log", "a");
  FILE* trace = fopen("sdmc:/switch/LunaNX/stream_trace.log", "a");
  const int input_relevant = strstr(fmt, "sctp") != NULL ||
                             strstr(fmt, "dtls") != NULL ||
                             strstr(fmt, "transport_completed") != NULL;
  FILE* input = input_relevant ? fopen("sdmc:/switch/LunaNX/input.log", "a") : NULL;

  if (file) {
    fputs("LIBPEER ", file);
  }
  if (trace) {
    fputs("LIBPEER ", trace);
  }
  if (input) {
    fputs("LIBPEER ", input);
  }

  va_list args;
  va_start(args, fmt);
  if (file) {
    va_list copy;
    va_copy(copy, args);
    vfprintf(file, fmt, copy);
    va_end(copy);
  }
  if (trace) {
    va_list copy;
    va_copy(copy, args);
    vfprintf(trace, fmt, copy);
    va_end(copy);
  }
  if (input) {
    vfprintf(input, fmt, args);
  }
  va_end(args);

  if (file) {
    fputc('\n', file);
    fclose(file);
  }
  if (trace) {
    fputc('\n', trace);
    fclose(trace);
  }
  if (input) {
    fputc('\n', input);
    fclose(input);
  }
}

static void peer_connection_hex_preview(const uint8_t* data, int len, char* out, size_t out_len) {
  if (!out || out_len == 0) {
    return;
  }
  out[0] = '\0';
  if (!data || len <= 0) {
    return;
  }

  int count = len < 12 ? len : 12;
  size_t pos = 0;
  for (int i = 0; i < count && pos + 4 < out_len; ++i) {
    pos += snprintf(out + pos, out_len - pos, "%s%02x", i == 0 ? "" : " ", data[i]);
  }
  if (len > count && pos + 5 < out_len) {
    snprintf(out + pos, out_len - pos, " ...");
  }
}

static void peer_connection_outgoing_rtp_packet(uint8_t* data, size_t size, void* user_data) {
  PeerConnection* pc = (PeerConnection*)user_data;
  dtls_srtp_encrypt_rtp_packet(&pc->dtls_srtp, data, (int*)&size);
  agent_send(&pc->agent, data, size);
}

static int peer_connection_dtls_srtp_recv(void* ctx, unsigned char* buf, size_t len) {
  DtlsSrtp* dtls_srtp = (DtlsSrtp*)ctx;
  PeerConnection* pc = (PeerConnection*)dtls_srtp->user_data;

  if (pc->agent_ret > 0 && pc->agent_ret <= len) {
    memcpy(buf, pc->agent_buf, pc->agent_ret);
    return pc->agent_ret;
  }

  // DTLS is advanced by peer_connection_loop(). Blocking here starves that
  // loop, so a missing datagram must be reported as "try again", not -1.
  const int ret = agent_recv(&pc->agent, buf, len);
  if (ret > 0)
    return ret;

  return MBEDTLS_ERR_SSL_WANT_READ;
}

static int peer_connection_dtls_srtp_send(void* ctx, const uint8_t* buf, size_t len) {
  DtlsSrtp* dtls_srtp = (DtlsSrtp*)ctx;
  PeerConnection* pc = (PeerConnection*)dtls_srtp->user_data;

  // LOGD("send %.4x %.4x, %ld", *(uint16_t*)buf, *(uint16_t*)(buf + 2), len);
  return agent_send(&pc->agent, buf, len);
}

static void rtp_receive_stats_update(RtpReceiveStats* stats, uint32_t ssrc, uint16_t seq) {
  if (!stats->initialized || stats->ssrc != ssrc) {
    memset(stats, 0, sizeof(*stats));
    stats->ssrc = ssrc;
    stats->initialized = 1;
    stats->max_seq = seq;
    stats->base_seq = seq;
  }
  const uint16_t delta = (uint16_t)(seq - stats->max_seq);
  if (delta < 0x8000) {
    if (seq < stats->max_seq)
      stats->cycles += 65536u;
    stats->max_seq = seq;
  }
  stats->received++;
}

static void rtp_receive_stats_on_sender_report(RtpReceiveStats* stats, uint32_t ssrc,
                                               uint32_t ntp_seconds, uint32_t ntp_fraction) {
  if (!stats->initialized || stats->ssrc != ssrc)
    return;
  stats->last_sr_ntp_mid = (ntp_seconds << 16) | (ntp_fraction >> 16);
  stats->last_sr_received_ms = ports_get_epoch_time();
}

// Writes a 24-byte report block and returns the fraction lost since the previous report (Q8).
static uint8_t rtp_receive_stats_write_block(RtpReceiveStats* stats, uint8_t* out, uint32_t now_ms) {
  const uint32_t extended_max = stats->cycles + stats->max_seq;
  const uint32_t expected = extended_max - stats->base_seq + 1;
  int32_t lost = (int32_t)(expected - stats->received);
  if (lost < 0)
    lost = 0;
  if (lost > 0x7FFFFF)
    lost = 0x7FFFFF;
  const uint32_t expected_interval = expected - stats->expected_prior;
  const uint32_t received_interval = stats->received - stats->received_prior;
  stats->expected_prior = expected;
  stats->received_prior = stats->received;
  uint8_t fraction = 0;
  if (expected_interval > 0 && expected_interval > received_interval)
    fraction = (uint8_t)(((expected_interval - received_interval) << 8) / expected_interval);

  uint32_t dlsr = 0;
  if (stats->last_sr_received_ms != 0)
    dlsr = (uint32_t)(((uint64_t)(now_ms - stats->last_sr_received_ms) * 65536u) / 1000u);
  const uint32_t words[6] = {
      htonl(stats->ssrc),
      htonl(((uint32_t)fraction << 24) | (uint32_t)lost),
      htonl(extended_max),
      0,
      htonl(stats->last_sr_ntp_mid),
      htonl(stats->last_sr_ntp_mid ? dlsr : 0),
  };
  memcpy(out, words, sizeof(words));
  return fraction;
}

static void peer_connection_maybe_send_receiver_report(PeerConnection* pc) {
  const uint32_t now_ms = ports_get_epoch_time();
  if (pc->last_receiver_report_ms != 0 && now_ms - pc->last_receiver_report_ms < 500)
    return;
  pc->last_receiver_report_ms = now_ms;

  RtpReceiveStats* streams[2];
  int count = 0;
  if (pc->video_rx_stats.initialized)
    streams[count++] = &pc->video_rx_stats;
  if (pc->audio_rx_stats.initialized)
    streams[count++] = &pc->audio_rx_stats;
  if (count == 0)
    return;

  uint8_t buf[256];
  memset(buf, 0, sizeof(buf));
  const uint32_t sender_ssrc = htonl(pc->vrtp_encoder.ssrc);

  // Receiver Report (PT 201), one block per received stream.
  int size = 8 + 24 * count;
  buf[0] = (uint8_t)(0x80 | count);
  buf[1] = RTCP_RR;
  buf[2] = 0;
  buf[3] = (uint8_t)(size / 4 - 1);
  memcpy(buf + 4, &sender_ssrc, 4);
  uint8_t video_fraction = 0;
  for (int i = 0; i < count; ++i) {
    const uint8_t fraction = rtp_receive_stats_write_block(streams[i], buf + 8 + 24 * i, now_ms);
    if (streams[i] == &pc->video_rx_stats)
      video_fraction = fraction;
  }

  // REMB (PSFB FMT 15): advertised maximum bitrate, backed off on video loss.
  if (pc->remb_max_bps > 0 && pc->video_rx_stats.initialized) {
    if (pc->remb_current_bps == 0)
      pc->remb_current_bps = pc->remb_max_bps;
    if (video_fraction > 25)  // > 10 % loss
      pc->remb_current_bps = pc->remb_current_bps / 100u * 85u;
    else if (video_fraction < 5)  // < 2 % loss
      pc->remb_current_bps = pc->remb_current_bps / 100u * 105u;
    if (pc->remb_current_bps > pc->remb_max_bps)
      pc->remb_current_bps = pc->remb_max_bps;
    if (pc->remb_current_bps < 1000000u)
      pc->remb_current_bps = 1000000u;

    uint32_t mantissa = pc->remb_current_bps;
    uint32_t exponent = 0;
    while (mantissa > 0x3FFFFu) {
      mantissa >>= 1;
      exponent++;
    }
    uint8_t* remb = buf + size;
    remb[0] = 0x80 | 15;
    remb[1] = RTCP_PSFB;
    remb[2] = 0;
    remb[3] = 5;
    memcpy(remb + 4, &sender_ssrc, 4);
    memset(remb + 8, 0, 4);
    memcpy(remb + 12, "REMB", 4);
    const uint32_t bitrate = htonl((1u << 24) | (exponent << 18) | mantissa);
    memcpy(remb + 16, &bitrate, 4);
    const uint32_t media_ssrc = htonl(pc->video_rx_stats.ssrc);
    memcpy(remb + 20, &media_ssrc, 4);
    size += 24;
  }

  dtls_srtp_encrypt_rctp_packet(&pc->dtls_srtp, buf, &size);
  agent_send(&pc->agent, buf, size);
  pc->receiver_reports_sent++;
  if (pc->receiver_reports_sent <= 3 || pc->receiver_reports_sent % 120 == 0) {
    peer_connection_diag_log("rtcp_rr sent=%u streams=%d videoLossQ8=%u remb=%u",
                             pc->receiver_reports_sent, count, video_fraction,
                             pc->remb_current_bps);
  }
}

static void peer_connection_incoming_rtcp(PeerConnection* pc, uint8_t* buf, size_t len) {
  RtcpHeader* rtcp_header;
  size_t pos = 0;

  while (pos < len) {
    rtcp_header = (RtcpHeader*)(buf + pos);

    switch (rtcp_header->type) {
      case RTCP_SR:
        if (pos + 20 <= len) {
          uint32_t ssrc_net, seconds_net, fraction_net;
          memcpy(&ssrc_net, buf + pos + 4, 4);
          memcpy(&seconds_net, buf + pos + 8, 4);
          memcpy(&fraction_net, buf + pos + 12, 4);
          rtp_receive_stats_on_sender_report(&pc->video_rx_stats, ntohl(ssrc_net),
                                             ntohl(seconds_net), ntohl(fraction_net));
          rtp_receive_stats_on_sender_report(&pc->audio_rx_stats, ntohl(ssrc_net),
                                             ntohl(seconds_net), ntohl(fraction_net));
        }
        if (pos + 20 <= len && pc->config.onrtpsenderreport) {
          uint32_t sender_ssrc_net, ntp_seconds_net, ntp_fraction_net, rtp_timestamp_net;
          memcpy(&sender_ssrc_net, buf + pos + 4, 4);
          memcpy(&ntp_seconds_net, buf + pos + 8, 4);
          memcpy(&ntp_fraction_net, buf + pos + 12, 4);
          memcpy(&rtp_timestamp_net, buf + pos + 16, 4);
          const uint32_t sender_ssrc = ntohl(sender_ssrc_net);
          const uint32_t ntp_seconds = ntohl(ntp_seconds_net);
          const uint32_t ntp_fraction = ntohl(ntp_fraction_net);
          const uint32_t rtp_timestamp = ntohl(rtp_timestamp_net);
          const uint64_t ntp_us = (uint64_t)ntp_seconds * 1000000ULL +
                                  (((uint64_t)ntp_fraction * 1000000ULL) >> 32);
          pc->config.onrtpsenderreport(sender_ssrc, ntp_us, rtp_timestamp, pc->config.user_data);
        }
        break;
      case RTCP_RR:
        LOGD("RTCP_PR");
        if (rtcp_header->rc > 0) {
// TODO: REMB, GCC ...etc
#if 0
          RtcpRr rtcp_rr = rtcp_parse_rr(buf);
          uint32_t fraction = ntohl(rtcp_rr.report_block[0].flcnpl) >> 24;
          uint32_t total = ntohl(rtcp_rr.report_block[0].flcnpl) & 0x00FFFFFF;
          if(pc->on_receiver_packet_loss && fraction > 0) {

            pc->on_receiver_packet_loss((float)fraction/256.0, total, pc->config.user_data);
          }
#endif
        }
        break;
      case RTCP_PSFB: {
        int fmt = rtcp_header->rc;
        LOGD("RTCP_PSFB %d", fmt);
        // PLI and FIR
        if ((fmt == 1 || fmt == 4) && pc->config.on_request_keyframe) {
          pc->config.on_request_keyframe(pc->config.user_data);
        }
      }
      default:
        break;
    }

    pos += 4 * ntohs(rtcp_header->length) + 4;
  }
}

const char* peer_connection_state_to_string(PeerConnectionState state) {
  switch (state) {
    case PEER_CONNECTION_NEW:
      return "new";
    case PEER_CONNECTION_CHECKING:
      return "checking";
    case PEER_CONNECTION_CONNECTED:
      return "connected";
    case PEER_CONNECTION_COMPLETED:
      return "completed";
    case PEER_CONNECTION_FAILED:
      return "failed";
    case PEER_CONNECTION_CLOSED:
      return "closed";
    case PEER_CONNECTION_DISCONNECTED:
      return "disconnected";
    default:
      return "unknown";
  }
}

PeerConnectionState peer_connection_get_state(PeerConnection* pc) {
  return pc->state;
}

void* peer_connection_get_sctp(PeerConnection* pc) {
  return &pc->sctp;
}

PeerConnection* peer_connection_create(PeerConfiguration* config) {
  PeerConnection* pc = calloc(1, sizeof(PeerConnection));
  if (!pc) {
    return NULL;
  }

  memcpy(&pc->config, config, sizeof(PeerConfiguration));
  pc->state = PEER_CONNECTION_NEW;
  pc->video_payload_type = PT_H264;
  pc->video_rtx_payload_type = -1;

  agent_create(&pc->agent);

  memset(&pc->sctp, 0, sizeof(pc->sctp));

  if (pc->config.audio_codec) {
    rtp_encoder_init(&pc->artp_encoder, pc->config.audio_codec,
                     peer_connection_outgoing_rtp_packet, (void*)pc);

    rtp_decoder_init(&pc->artp_decoder, pc->config.audio_codec,
                     pc->config.onaudiotrack, pc->config.user_data);
    rtp_decoder_set_audio_callback(&pc->artp_decoder, pc->config.onaudiopacket);
  }

  if (pc->config.video_codec) {
    rtp_encoder_init(&pc->vrtp_encoder, pc->config.video_codec,
                     peer_connection_outgoing_rtp_packet, (void*)pc);

    rtp_decoder_init(&pc->vrtp_decoder, pc->config.video_codec,
                     pc->config.onvideotrack, pc->config.user_data);
    rtp_decoder_set_video_callback(&pc->vrtp_decoder, pc->config.onvideopacket);
  }

  return pc;
}

void peer_connection_destroy(PeerConnection* pc) {
  if (pc) {
    rtp_decoder_cleanup(&pc->artp_decoder);
    rtp_decoder_cleanup(&pc->vrtp_decoder);
    sctp_destroy_association(&pc->sctp);
    dtls_srtp_deinit(&pc->dtls_srtp);
    agent_destroy(&pc->agent);
    free(pc);
    pc = NULL;
  }
}

void peer_connection_close(PeerConnection* pc) {
  pc->state = PEER_CONNECTION_CLOSED;
}

int peer_connection_send_audio(PeerConnection* pc, const uint8_t* buf, size_t len) {
  if (pc->state != PEER_CONNECTION_COMPLETED) {
    // LOGE("dtls_srtp not connected");
    return -1;
  }
  return rtp_encoder_encode(&pc->artp_encoder, buf, len);
}

int peer_connection_send_video(PeerConnection* pc, const uint8_t* buf, size_t len) {
  if (pc->state != PEER_CONNECTION_COMPLETED) {
    // LOGE("dtls_srtp not connected");
    return -1;
  }
  return rtp_encoder_encode(&pc->vrtp_encoder, buf, len);
}

int peer_connection_datachannel_send(PeerConnection* pc, char* message, size_t len) {
  return peer_connection_datachannel_send_sid(pc, message, len, 0);
}

int peer_connection_datachannel_send_sid(PeerConnection* pc, char* message, size_t len, uint16_t sid) {
  if (!sctp_is_connected(&pc->sctp)) {
    LOGE("sctp not connected");
    return -1;
  }
  if (pc->config.datachannel == DATA_CHANNEL_STRING)
    return sctp_outgoing_data(&pc->sctp, message, len, PPID_STRING, sid);
  else
    return sctp_outgoing_data(&pc->sctp, message, len, PPID_BINARY, sid);
}

int peer_connection_datachannel_send_binary_sid(PeerConnection* pc, char* message, size_t len, uint16_t sid) {
  if (!sctp_is_connected(&pc->sctp)) {
    LOGE("sctp not connected");
    return -1;
  }
  return sctp_outgoing_data(&pc->sctp, message, len, PPID_BINARY, sid);
}

int peer_connection_create_datachannel(PeerConnection* pc, DecpChannelType channel_type, uint16_t priority, uint32_t reliability_parameter, char* label, char* protocol) {
  return peer_connection_create_datachannel_sid(pc, channel_type, priority, reliability_parameter, label, protocol, 0);
}

int peer_connection_create_datachannel_sid(PeerConnection* pc, DecpChannelType channel_type, uint16_t priority, uint32_t reliability_parameter, char* label, char* protocol, uint16_t sid) {
  int rtrn = -1;

  if (!sctp_is_connected(&pc->sctp)) {
#if !CONFIG_USE_USRSCTP
    peer_connection_diag_log("sctp_not_connected waiting_handshake sid=%u label=%s", sid, label ? label : "");
#endif
    LOGE("sctp not connected");
    return rtrn;
  }

  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |  Message Type |  Channel Type |            Priority           |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                    Reliability Parameter                      |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |         Label Length          |       Protocol Length         |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                                                               |
  // |                             Label                             |
  // |                                                               |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                                                               |
  // |                            Protocol                           |
  // |                                                               |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  int msg_size = 12 + strlen(label) + strlen(protocol);
  uint16_t priority_big_endian = htons(priority);
  uint32_t reliability_big_endian = htonl(reliability_parameter);
  uint16_t label_length = htons(strlen(label));
  uint16_t protocol_length = htons(strlen(protocol));
  char* msg = calloc(1, msg_size);
  if (!msg) {
    return rtrn;
  }

  msg[0] = DATA_CHANNEL_OPEN;
  msg[1] = (char)channel_type;
  memcpy(msg + 2, &priority_big_endian, sizeof(uint16_t));
  memcpy(msg + 4, &reliability_big_endian, sizeof(uint32_t));
  memcpy(msg + 8, &label_length, sizeof(uint16_t));
  memcpy(msg + 10, &protocol_length, sizeof(uint16_t));
  memcpy(msg + 12, label, strlen(label));
  memcpy(msg + 12 + strlen(label), protocol, strlen(protocol));

  rtrn = sctp_outgoing_data(&pc->sctp, msg, msg_size, PPID_CONTROL, sid);
  if (rtrn >= 0) {
    sctp_add_stream_mapping(&pc->sctp, label, sid);
  }
  free(msg);
  return rtrn;
}

static char* peer_connection_dtls_role_setup_value(DtlsSrtpRole d) {
  return d == DTLS_SRTP_ROLE_SERVER ? "a=setup:passive" : "a=setup:active";
}

static int peer_connection_loop_internal(PeerConnection* pc,
                                         int completed_poll_timeout_ms) {
  uint32_t ssrc = 0;
  int packet_processed = 0;
  memset(pc->agent_buf, 0, sizeof(pc->agent_buf));
  pc->agent_ret = -1;

  switch (pc->state) {
    case PEER_CONNECTION_NEW:
      break;

    case PEER_CONNECTION_CHECKING:
      if (pc->agent.candidate_pairs_num == 0) {
        break;
      }
      if (agent_select_candidate_pair(&pc->agent) < 0) {
        STATE_CHANGED(pc, PEER_CONNECTION_FAILED);
      } else if (agent_connectivity_check(&pc->agent) == 0) {
        STATE_CHANGED(pc, PEER_CONNECTION_CONNECTED);
      }
      break;

    case PEER_CONNECTION_CONNECTED:
      if (pc->dtls_handshake_attempts == 0) {
        pc->dtls_handshake_started_ms = ports_get_epoch_time();
        peer_connection_diag_log("dtls_start datachannel=%d candidatePairs=%d",
                                 pc->config.datachannel,
                                 pc->agent.candidate_pairs_num);
      }

      pc->dtls_handshake_attempts++;

      int dtls_ret = dtls_srtp_handshake(&pc->dtls_srtp, NULL);
      if (dtls_ret == 0) {
        LOGD("DTLS-SRTP handshake done");
        peer_connection_diag_log("dtls_done attempts=%d datachannel=%d",
                                 pc->dtls_handshake_attempts,
                                 pc->config.datachannel);

        if (pc->config.datachannel && !pc->sctp_create_attempted) {
          LOGI("SCTP create socket");
#if CONFIG_USE_USRSCTP
          peer_connection_diag_log("sctp_backend=usrsctp");
#else
          peer_connection_diag_log("sctp_backend=internal");
#endif
          peer_connection_diag_log("sctp_create_start");
          pc->sctp_create_attempted = 1;
          int sctp_ret = sctp_create_association(&pc->sctp, &pc->dtls_srtp);
          peer_connection_diag_log("sctp_create_done ret=%d", sctp_ret);
          if (sctp_ret == 0) {
            pc->sctp.userdata = pc->config.user_data;
          } else {
            LOGE("SCTP create socket failed");
          }
        }

        peer_connection_diag_log("transport_completed");
        STATE_CHANGED(pc, PEER_CONNECTION_COMPLETED);
      } else if ((uint32_t)(ports_get_epoch_time() - pc->dtls_handshake_started_ms) >= 3500) {
        int failed_pairs = agent_fail_nominated_remote(&pc->agent);
        peer_connection_diag_log("dtls_endpoint_timeout attempts=%d failedPairs=%d; trying next remote",
                                 pc->dtls_handshake_attempts,
                                 failed_pairs);
        dtls_srtp_reset_session(&pc->dtls_srtp);
        pc->dtls_handshake_attempts = 0;
        pc->dtls_handshake_started_ms = 0;
        if (failed_pairs > 0) {
          STATE_CHANGED(pc, PEER_CONNECTION_CHECKING);
        } else {
          STATE_CHANGED(pc, PEER_CONNECTION_FAILED);
        }
      } else if (pc->dtls_handshake_attempts == 1 || pc->dtls_handshake_attempts % 600 == 0) {
        peer_connection_diag_log("dtls_pending ret=%d attempts=%d",
                                 dtls_ret,
                                 pc->dtls_handshake_attempts);
      }
      break;
    case PEER_CONNECTION_COMPLETED:
      if ((pc->agent_ret = agent_recv_timeout(&pc->agent, pc->agent_buf,
                                              sizeof(pc->agent_buf),
                                              completed_poll_timeout_ms)) > 0) {
        packet_processed = 1;
        LOGD("agent_recv %d", pc->agent_ret);
        pc->completed_udp_packets++;
        char preview[64];
        peer_connection_hex_preview(pc->agent_buf, pc->agent_ret, preview, sizeof(preview));

        if (dtls_srtp_probe(pc->agent_buf)) {
          pc->completed_dtls_packets++;
          if (pc->completed_dtls_packets <= 10 || pc->completed_dtls_packets % 600 == 0) {
            peer_connection_diag_log("udp_class=dtls count=%d bytes=%d preview=%s",
                                     pc->completed_dtls_packets,
                                     pc->agent_ret,
                                     preview);
          }
          int ret = dtls_srtp_read(&pc->dtls_srtp, pc->temp_buf, sizeof(pc->temp_buf));
          LOGD("Got DTLS data %d", ret);

          if (ret > 0) {
            pc->sctp_read_events++;
            if (pc->sctp_read_events <= 10 || pc->sctp_read_events % 600 == 0) {
              char sctp_preview[64];
              peer_connection_hex_preview(pc->temp_buf, ret, sctp_preview, sizeof(sctp_preview));
              peer_connection_diag_log("dtls_read_appdata ret=%d sctpEvents=%d preview=%s",
                                       ret,
                                       pc->sctp_read_events,
                                       sctp_preview);
            }
            sctp_incoming_data(&pc->sctp, (char*)pc->temp_buf, ret);
          } else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            peer_connection_diag_log("dtls_peer_close_notify packets=%d sctpConnected=%d",
                                     pc->completed_dtls_packets,
                                     sctp_is_connected(&pc->sctp));
            STATE_CHANGED(pc, PEER_CONNECTION_CLOSED);
          } else if (ret < 0 && (pc->completed_dtls_packets <= 10 || pc->completed_dtls_packets % 600 == 0)) {
            peer_connection_diag_log("dtls_read_no_appdata ret=%d dtlsPackets=%d",
                                     ret,
                                     pc->completed_dtls_packets);
          }

        } else if (rtcp_probe(pc->agent_buf, pc->agent_ret)) {
          LOGD("Got RTCP packet");
          pc->completed_rtcp_packets++;
          if (pc->completed_rtcp_packets <= 5 || pc->completed_rtcp_packets % 600 == 0) {
            peer_connection_diag_log("udp_class=rtcp count=%d bytes=%d preview=%s",
                                     pc->completed_rtcp_packets,
                                     pc->agent_ret,
                                     preview);
          }
          if (dtls_srtp_decrypt_rtcp_packet(&pc->dtls_srtp, pc->agent_buf, &pc->agent_ret) != 0) {
            pc->rtcp_decrypt_failures++;
            peer_connection_diag_log("rtcp_decrypt_failed failures=%d bytesAfter=%d",
                                     pc->rtcp_decrypt_failures,
                                     pc->agent_ret);
            break;
          }
          peer_connection_incoming_rtcp(pc, pc->agent_buf, pc->agent_ret);

        } else if (rtp_packet_validate(pc->agent_buf, pc->agent_ret)) {
          LOGD("Got RTP packet");
          pc->completed_rtp_packets++;

          if (pc->completed_rtp_packets <= 10 || pc->completed_rtp_packets % 1200 == 0) {
            peer_connection_diag_log("udp_class=rtp count=%d bytes=%d preview=%s",
                                     pc->completed_rtp_packets,
                                     pc->agent_ret,
                                     preview);
          }

          if (dtls_srtp_decrypt_rtp_packet(&pc->dtls_srtp, pc->agent_buf, &pc->agent_ret) != 0) {
            pc->rtp_decrypt_failures++;
            peer_connection_diag_log("rtp_decrypt_failed failures=%d bytesAfter=%d",
                                     pc->rtp_decrypt_failures,
                                     pc->agent_ret);
            break;
          }

          ssrc = rtp_get_ssrc(pc->agent_buf);
          RtpPacket* rtp = (RtpPacket*)pc->agent_buf;
          const uint32_t regular_gap_hold_ms =
              rtp_recovery_gap_hold_ms(pc->agent.rtt_current_ms,
                                       pc->agent.rtt_jitter_q4 / 4u);
          const uint32_t startup_gap_hold_ms =
              rtp_recovery_startup_gap_hold_ms(
                  regular_gap_hold_ms,
                  pc->vrtp_decoder.access_units_completed);
          rtp_decoder_set_reorder_grace_ms(
              &pc->vrtp_decoder, startup_gap_hold_ms);
          rtp_decoder_set_reorder_burst_grace_ms(
              &pc->vrtp_decoder,
              rtp_recovery_burst_gap_hold_ms(
                  startup_gap_hold_ms, pc->agent.rtt_current_ms,
                  pc->agent.rtt_jitter_q4 / 4u));
          const int is_audio_payload = rtp->header.type == PT_PCMU ||
                                       rtp->header.type == PT_PCMA ||
                                       rtp->header.type == PT_OPUS ||
                                       rtp->header.type == 63;
          if (ssrc == pc->remote_assrc || (pc->remote_assrc == 0 && is_audio_payload)) {
            if (pc->remote_assrc == 0) {
              pc->remote_assrc = ssrc;
              LOGI("Learned remote audio SSRC from RTP: %" PRIu32, pc->remote_assrc);
              peer_connection_diag_log("learned_audio_ssrc=%" PRIu32 " pt=%u",
                                       pc->remote_assrc, rtp->header.type);
            }
            rtp_receive_stats_update(&pc->audio_rx_stats, ssrc, ntohs(rtp->header.seq_number));
            rtp_decoder_decode(&pc->artp_decoder, pc->agent_buf, pc->agent_ret);
          } else if (pc->remote_vrtx_ssrc != 0 &&
                     ssrc == pc->remote_vrtx_ssrc &&
                     pc->video_rtx_payload_type >= 0 &&
                     rtp->header.type == (uint8_t)pc->video_rtx_payload_type) {
            pc->video_rtx_packets_received++;
            size_t recovered_size = (size_t)pc->agent_ret;
            if (rtp_packet_unwrap_rtx(pc->agent_buf, &recovered_size,
                                      (uint8_t)pc->video_payload_type,
                                      pc->remote_vssrc) == 0) {
              pc->agent_ret = (int)recovered_size;
              pc->video_rtx_packets_recovered++;
              rtp_receive_stats_update(&pc->video_rx_stats, pc->remote_vssrc,
                                       ntohs(((RtpPacket*)pc->agent_buf)->header.seq_number));
              rtp_decoder_decode(&pc->vrtp_decoder, pc->agent_buf,
                                 pc->agent_ret);
            } else {
              pc->video_rtx_packets_invalid++;
            }
            if (pc->video_rtx_packets_received <= 4 ||
                pc->video_rtx_packets_received % 600 == 0) {
              peer_connection_diag_log(
                  "rtx received/recovered/invalid=%u/%u/%u pt=%d ssrc=%" PRIu32,
                  pc->video_rtx_packets_received,
                  pc->video_rtx_packets_recovered,
                  pc->video_rtx_packets_invalid,
                  pc->video_rtx_payload_type,
                  pc->remote_vrtx_ssrc);
            }
          } else if (ssrc == pc->remote_vssrc ||
                     (pc->remote_vssrc == 0 &&
                      rtp->header.type != PT_PCMU &&
                      rtp->header.type != PT_PCMA &&
                      rtp->header.type != PT_OPUS &&
                      rtp->header.type != 63)) {
            if (pc->remote_vssrc == 0) {
              pc->remote_vssrc = ssrc;
              LOGI("Learned remote video SSRC from RTP: %" PRIu32, pc->remote_vssrc);
            }
            rtp_receive_stats_update(&pc->video_rx_stats, ssrc, ntohs(rtp->header.seq_number));
            rtp_decoder_decode(&pc->vrtp_decoder, pc->agent_buf, pc->agent_ret);
            if (pc->completed_rtp_packets % 6000 == 0) {
              peer_connection_diag_log(
                  "rtp_h264 packets=%u gaps=%u reordered=%u late=%u forced=%u startupRebase=%u buffered=%u adaptiveHold/recovered/expired/fastForward=%u/%u/%u/%u nack/retry=%u/%u nalus=%u auOk=%u auDrop=%u",
                  pc->vrtp_decoder.packets_received,
                  pc->vrtp_decoder.sequence_gaps,
                  pc->vrtp_decoder.reordered_packets,
                  pc->vrtp_decoder.late_packets_dropped,
                  pc->vrtp_decoder.forced_sequence_skips,
                  pc->vrtp_decoder.startup_sequence_rebases,
                  pc->vrtp_decoder.reorder_buffered_packets,
                  pc->vrtp_decoder.reorder_adaptive_holds,
                  pc->vrtp_decoder.reorder_gap_recoveries,
                  pc->vrtp_decoder.reorder_gap_expirations,
                  pc->vrtp_decoder.reorder_burst_fast_forwards,
                  pc->video_nack_requests,
                  pc->video_nack_retries,
                  pc->vrtp_decoder.nal_units_completed,
                  pc->vrtp_decoder.access_units_completed,
                  pc->vrtp_decoder.access_units_dropped);
            }
          }
          peer_connection_maybe_send_video_nack(pc);

        } else {
          pc->completed_unknown_packets++;
          if (pc->completed_unknown_packets <= 10 || pc->completed_unknown_packets % 600 == 0) {
            peer_connection_diag_log("udp_class=unknown count=%d bytes=%d preview=%s totals udp/rtcp/dtls/rtp/unknown=%d/%d/%d/%d/%d",
                                     pc->completed_unknown_packets,
                                     pc->agent_ret,
                                     preview,
                                     pc->completed_udp_packets,
                                     pc->completed_rtcp_packets,
                                     pc->completed_dtls_packets,
                                     pc->completed_rtp_packets,
                                     pc->completed_unknown_packets);
          }
          LOGW("Unknown data");
        }
      }

      agent_rtt_probe_tick(&pc->agent);
      peer_connection_maybe_send_receiver_report(pc);

      if (CONFIG_KEEPALIVE_TIMEOUT > 0 &&
          pc->agent.binding_request_time > 0 &&
          (ports_get_epoch_time() - pc->agent.binding_request_time) > CONFIG_KEEPALIVE_TIMEOUT) {
        LOGI("binding request timeout; keeping completed transport alive");
        pc->agent.binding_request_time = ports_get_epoch_time();
      }

      break;
    case PEER_CONNECTION_FAILED:
      break;
    case PEER_CONNECTION_DISCONNECTED:
      break;
    case PEER_CONNECTION_CLOSED:
      break;
    default:
      break;
  }

  return packet_processed;
}

int peer_connection_loop(PeerConnection* pc) {
  return peer_connection_loop_internal(pc, 1);
}

int peer_connection_loop_nonblocking(PeerConnection* pc) {
  return peer_connection_loop_internal(pc, 0);
}

void peer_connection_set_remote_description(PeerConnection* pc, const char* sdp, SdpType type) {
  char* start = (char*)sdp;
  char* line = NULL;
  char buf[1024];
  char* val_start = NULL;
  uint32_t* ssrc = NULL;
  DtlsSrtpRole role = DTLS_SRTP_ROLE_SERVER;
  int is_update = 0;
  Agent* agent = &pc->agent;

  while ((line = strstr(start, "\r\n"))) {
    line = strstr(start, "\r\n");
    size_t line_len = (size_t)(line - start);
    if (line_len >= sizeof(buf))
      line_len = sizeof(buf) - 1;
    memcpy(buf, start, line_len);
    buf[line_len] = '\0';

    if (strstr(buf, "a=setup:passive")) {
      role = DTLS_SRTP_ROLE_CLIENT;
    }

    if (strstr(buf, "a=fingerprint")) {
      strncpy(pc->dtls_srtp.remote_fingerprint, buf + 22, DTLS_SRTP_FINGERPRINT_LENGTH);
    }

    if (strstr(buf, "a=ice-ufrag") &&
        strlen(agent->remote_ufrag) != 0 &&
        (strncmp(buf + strlen("a=ice-ufrag:"), agent->remote_ufrag, strlen(agent->remote_ufrag)) == 0)) {
      is_update = 1;
    }

    if (strstr(buf, "m=video")) {
      ssrc = &pc->remote_vssrc;
    } else if (strstr(buf, "m=audio")) {
      ssrc = &pc->remote_assrc;
    }

    if ((val_start = strstr(buf, "a=ssrc:")) && ssrc) {
      uint32_t parsed_ssrc = strtoul(val_start + 7, NULL, 10);
      if (*ssrc == 0) {
        *ssrc = parsed_ssrc;
        LOGD("SSRC: %" PRIu32, *ssrc);
        peer_connection_diag_log("remote_ssrc_selected media=%s ssrc=%" PRIu32,
                                 ssrc == &pc->remote_vssrc ? "video" : "audio",
                                 *ssrc);
      } else if (*ssrc != parsed_ssrc) {
        peer_connection_diag_log("remote_ssrc_ignored media=%s selected=%" PRIu32 " ignored=%" PRIu32,
                                 ssrc == &pc->remote_vssrc ? "video" : "audio",
                                 *ssrc,
                                 parsed_ssrc);
      }
    }

    if (ssrc == &pc->remote_vssrc &&
        sscanf(buf, "a=ssrc-group:FID %" SCNu32 " %" SCNu32,
               &pc->remote_vssrc, &pc->remote_vrtx_ssrc) == 2) {
      peer_connection_diag_log(
          "remote_video_fid primary=%" PRIu32 " rtx=%" PRIu32,
          pc->remote_vssrc, pc->remote_vrtx_ssrc);
    }

    if (ssrc == &pc->remote_vssrc && strstr(buf, "a=fmtp:")) {
      int rtx_pt = -1;
      int apt = -1;
      if (sscanf(buf, "a=fmtp:%d apt=%d", &rtx_pt, &apt) == 2 &&
          apt == PT_H264) {
        pc->video_payload_type = apt;
        pc->video_rtx_payload_type = rtx_pt;
        peer_connection_diag_log("remote_video_rtx_mapping mediaPt=%d rtxPt=%d",
                                 apt, rtx_pt);
      }
    }

    start = line + 2;
  }

  if (is_update) {
    return;
  }

  pc->remote_ice_lite = strstr(sdp, "a=ice-lite") != NULL;
  agent_set_remote_description(&pc->agent, (char*)sdp);
  if (type == SDP_TYPE_ANSWER) {
    agent_update_candidate_pairs(&pc->agent);
    STATE_CHANGED(pc, PEER_CONNECTION_CHECKING);
  }
}

static const char* peer_connection_create_sdp(PeerConnection* pc, SdpType sdp_type) {
  char* description = (char*)pc->temp_buf;

  memset(pc->temp_buf, 0, sizeof(pc->temp_buf));
  DtlsSrtpRole role = DTLS_SRTP_ROLE_SERVER;

  pc->sctp.connected = 0;

  switch (sdp_type) {
    case SDP_TYPE_OFFER:
      role = DTLS_SRTP_ROLE_SERVER;
      agent_clear_candidates(&pc->agent);
      pc->agent.mode = AGENT_MODE_CONTROLLING;
      break;
    case SDP_TYPE_ANSWER:
      role = DTLS_SRTP_ROLE_CLIENT;
      pc->agent.mode = pc->remote_ice_lite ? AGENT_MODE_CONTROLLING : AGENT_MODE_CONTROLLED;
      break;
    default:
      break;
  }

  dtls_srtp_reset_session(&pc->dtls_srtp);
  pc->dtls_handshake_attempts = 0;
  pc->dtls_handshake_started_ms = 0;
  dtls_srtp_init(&pc->dtls_srtp, role, pc);
  pc->dtls_srtp.udp_recv = peer_connection_dtls_srtp_recv;
  pc->dtls_srtp.udp_send = peer_connection_dtls_srtp_send;

  memset(pc->sdp, 0, sizeof(pc->sdp));
  // TODO: check if we have video or audio codecs
  sdp_create(pc->sdp,
             pc->config.video_codec != CODEC_NONE,
             pc->config.audio_codec != CODEC_NONE,
             pc->config.datachannel);

  agent_create_ice_credential(&pc->agent);
  sdp_append(pc->sdp, "a=ice-ufrag:%s", pc->agent.local_ufrag);
  sdp_append(pc->sdp, "a=ice-pwd:%s", pc->agent.local_upwd);
  sdp_append(pc->sdp, "a=fingerprint:sha-256 %s", pc->dtls_srtp.local_fingerprint);
  sdp_append(pc->sdp, peer_connection_dtls_role_setup_value(role));

  if (pc->config.video_codec == CODEC_H264) {
    sdp_append_h264(pc->sdp);
  }

  switch (pc->config.audio_codec) {
    case CODEC_PCMA:
      sdp_append_pcma(pc->sdp);
      break;
    case CODEC_PCMU:
      sdp_append_pcmu(pc->sdp);
      break;
    case CODEC_OPUS:
      sdp_append_opus(pc->sdp);
    default:
      break;
  }

  if (pc->config.datachannel) {
    sdp_append_datachannel(pc->sdp);
  }

  pc->b_local_description_created = 1;

  agent_gather_candidate(&pc->agent, NULL, NULL, NULL);  // host address
  for (int i = 0; i < sizeof(pc->config.ice_servers) / sizeof(pc->config.ice_servers[0]); ++i) {
    if (pc->config.ice_servers[i].urls) {
      LOGI("ice server: %s", pc->config.ice_servers[i].urls);
      agent_gather_candidate(&pc->agent, pc->config.ice_servers[i].urls, pc->config.ice_servers[i].username, pc->config.ice_servers[i].credential);
    }
  }

  agent_get_local_description(&pc->agent, description, sizeof(pc->temp_buf));
  sdp_append(pc->sdp, description);

  if (pc->onicecandidate) {
    pc->onicecandidate(pc->sdp, pc->config.user_data);
  }

  return pc->sdp;
}

const char* peer_connection_create_offer(PeerConnection* pc) {
  return peer_connection_create_sdp(pc, SDP_TYPE_OFFER);
}

const char* peer_connection_create_answer(PeerConnection* pc) {
  const char* sdp = peer_connection_create_sdp(pc, SDP_TYPE_ANSWER);
  agent_update_candidate_pairs(&pc->agent);
  STATE_CHANGED(pc, PEER_CONNECTION_CHECKING);
  return sdp;
}

int peer_connection_send_rtcp_pil(PeerConnection* pc, uint32_t ssrc) {
  uint8_t plibuf[128];
  int size = 12;

  if (!pc || pc->state != PEER_CONNECTION_COMPLETED || ssrc == 0)
    return -1;

  if (rtcp_get_pli(plibuf, size, pc->vrtp_encoder.ssrc, ssrc) != size)
    return -1;

  dtls_srtp_encrypt_rctp_packet(&pc->dtls_srtp, plibuf, &size);
  const int ret = agent_send(&pc->agent, plibuf, size);
  peer_connection_diag_log("rtcp_pli sent=%d mediaSsrc=%" PRIu32 " bytes=%d",
                           ret,
                           ssrc,
                           size);
  return ret;
}

static int peer_connection_send_rtcp_nack(PeerConnection* pc, uint32_t ssrc,
                                          uint16_t packet_id, uint16_t bitmask) {
  uint8_t nackbuf[64];
  int size = 16;

  if (!pc || pc->state != PEER_CONNECTION_COMPLETED || ssrc == 0)
    return -1;
  if (rtcp_get_nack(nackbuf, size, pc->vrtp_encoder.ssrc, ssrc,
                    packet_id, bitmask) != size)
    return -1;

  dtls_srtp_encrypt_rctp_packet(&pc->dtls_srtp, nackbuf, &size);
  return agent_send(&pc->agent, nackbuf, size);
}

static uint32_t peer_connection_nack_packet_count(uint16_t bitmask) {
  uint32_t count = 1;
  while (bitmask != 0) {
    count += bitmask & 1u;
    bitmask >>= 1u;
  }
  return count;
}

static void peer_connection_reset_video_nack(PeerConnection* pc) {
  pc->video_has_last_nack_sequence = 0;
  pc->video_last_nack_bitmask = 0;
  pc->video_nack_retry_count = 0;
  pc->video_nack_burst = 0;
  pc->video_last_nack_sent_ms = 0;
}

static void peer_connection_maybe_send_video_nack(PeerConnection* pc) {
  uint16_t packet_id = 0;
  uint16_t bitmask = 0;
  if (!pc || pc->remote_vssrc == 0 ||
      !rtp_decoder_get_nack(&pc->vrtp_decoder, &packet_id, &bitmask)) {
    if (pc)
      peer_connection_reset_video_nack(pc);
    return;
  }

  const uint32_t now_ms = ports_get_epoch_time();
  const uint32_t retry_interval_ms =
      rtp_recovery_nack_retry_ms(pc->agent.rtt_current_ms);
  const int new_base = !pc->video_has_last_nack_sequence ||
                       pc->video_last_nack_sequence != packet_id;
  const int expanded_mask =
      !new_base && pc->video_last_nack_bitmask != bitmask;

  if (new_base) {
    pc->video_last_nack_sequence = packet_id;
    pc->video_last_nack_bitmask = bitmask;
    pc->video_nack_retry_count = 0;
  } else if (expanded_mask) {
    pc->video_last_nack_bitmask = bitmask;
  }
  if (pc->vrtp_decoder.reorder_buffered_packets >=
      RTP_RECOVERY_BURST_GAP_DISTANCE) {
    pc->video_nack_burst = 1;
  }

  const uint32_t elapsed_ms =
      pc->video_last_nack_sent_ms == 0
          ? retry_interval_ms
          : now_ms - pc->video_last_nack_sent_ms;
  const int first_request = new_base || !pc->video_has_last_nack_sequence;
  const int mask_update_ready =
      expanded_mask && elapsed_ms >= RTP_RECOVERY_MIN_NACK_RETRY_MS;
  const int retry_ready =
      !first_request && !mask_update_ready &&
      pc->video_nack_retry_count <
          (pc->video_nack_burst
               ? RTP_RECOVERY_MAX_BURST_NACK_RETRIES
               : RTP_RECOVERY_MAX_NACK_RETRIES) &&
      elapsed_ms >= retry_interval_ms;
  if (!first_request && !mask_update_ready && !retry_ready)
    return;

  const int sent = peer_connection_send_rtcp_nack(
      pc, pc->remote_vssrc, packet_id, pc->video_last_nack_bitmask);
  if (sent < 0)
    return;

  if (!first_request) {
    pc->video_nack_retry_count++;
    pc->video_nack_retries++;
  }
  pc->video_has_last_nack_sequence = 1;
  pc->video_last_nack_sent_ms = now_ms;
  pc->video_nack_requests++;
  pc->video_nack_packets_requested +=
      peer_connection_nack_packet_count(pc->video_last_nack_bitmask);
}

void peer_connection_set_max_receive_bitrate(PeerConnection* pc, uint32_t bps) {
  if (!pc)
    return;
  pc->remb_max_bps = bps;
  pc->remb_current_bps = bps;
}

int peer_connection_request_video_keyframe(PeerConnection* pc) {
  if (!pc)
    return -1;
  return peer_connection_send_rtcp_pil(pc, pc->remote_vssrc);
}

// callbacks
void peer_connection_on_connected(PeerConnection* pc, void (*on_connected)(void* userdata)) {
  pc->on_connected = on_connected;
}

void peer_connection_on_receiver_packet_loss(PeerConnection* pc,
                                             void (*on_receiver_packet_loss)(float fraction_loss, uint32_t total_loss, void* userdata)) {
  pc->on_receiver_packet_loss = on_receiver_packet_loss;
}

void peer_connection_onicecandidate(PeerConnection* pc, void (*onicecandidate)(char* sdp, void* userdata)) {
  pc->onicecandidate = onicecandidate;
}

void peer_connection_oniceconnectionstatechange(PeerConnection* pc,
                                                void (*oniceconnectionstatechange)(PeerConnectionState state, void* userdata)) {
  pc->oniceconnectionstatechange = oniceconnectionstatechange;
}

void peer_connection_ondatachannel(PeerConnection* pc,
                                   void (*onmessage)(char* msg, size_t len, void* userdata, uint16_t sid),
                                   void (*onopen)(void* userdata),
                                   void (*onclose)(void* userdata)) {
  if (pc) {
    sctp_onopen(&pc->sctp, onopen);
    sctp_onclose(&pc->sctp, onclose);
    sctp_onmessage(&pc->sctp, onmessage);
  }
}

int peer_connection_lookup_sid(PeerConnection* pc, const char* label, uint16_t* sid) {
  for (int i = 0; i < pc->sctp.stream_count; i++) {
    if (strncmp(pc->sctp.stream_table[i].label, label, sizeof(pc->sctp.stream_table[i].label)) == 0) {
      *sid = pc->sctp.stream_table[i].sid;
      return 0;
    }
  }
  return -1;  // Not found
}

char* peer_connection_lookup_sid_label(PeerConnection* pc, uint16_t sid) {
  for (int i = 0; i < pc->sctp.stream_count; i++) {
    if (pc->sctp.stream_table[i].sid == sid) {
      return pc->sctp.stream_table[i].label;
    }
  }
  return NULL;  // Not found
}

int peer_connection_add_ice_candidate(PeerConnection* pc, char* candidate) {
  Agent* agent = &pc->agent;
  if (agent->remote_candidates_count >= AGENT_MAX_CANDIDATES) {
    LOGE("Remote ICE candidate table is full");
    return -1;
  }
  if (ice_candidate_from_description(&agent->remote_candidates[agent->remote_candidates_count], candidate, candidate + strlen(candidate)) != 0) {
    return -1;
  }
  LOGD("Add candidate: %s", candidate);
  IceCandidate* remote = &agent->remote_candidates[agent->remote_candidates_count];
  agent->remote_candidates_count++;

  for (int i = 0; i < agent->local_candidates_count; i++) {
    if (agent->local_candidates[i].addr.family != remote->addr.family)
      continue;
    if (agent->candidate_pairs_num >= AGENT_MAX_CANDIDATE_PAIRS) {
      LOGE("ICE candidate pair table is full");
      return -1;
    }

    IceCandidatePair* pair = &agent->candidate_pairs[agent->candidate_pairs_num++];
    pair->local = &agent->local_candidates[i];
    pair->remote = remote;
    pair->priority = ice_candidate_pair_priority(
        pair->local->priority,
        pair->remote->priority,
        agent->mode == AGENT_MODE_CONTROLLING);
    pair->state = ICE_CANDIDATE_STATE_FROZEN;
  }
  agent_sort_candidate_pairs(agent);

  return 0;
}

int peer_connection_get_ice_candidate_pair_stats(PeerConnection* pc,
                                                 int* total,
                                                 int* frozen,
                                                 int* inprogress,
                                                 int* succeeded,
                                                 int* failed) {
  if (!pc) {
    return -1;
  }

  AgentCandidatePairStats stats;
  agent_get_candidate_pair_stats(&pc->agent, &stats);
  if (total)
    *total = stats.total;
  if (frozen)
    *frozen = stats.frozen;
  if (inprogress)
    *inprogress = stats.inprogress;
  if (succeeded)
    *succeeded = stats.succeeded;
  if (failed)
    *failed = stats.failed;
  return 0;
}

int peer_connection_get_rtt_ms(PeerConnection* pc) {
  PeerNetworkRttStats stats;
  return peer_connection_get_rtt_stats(pc, &stats) == 0
             ? stats.current_ms
             : -1;
}

int peer_connection_get_rtt_stats(PeerConnection* pc,
                                  PeerNetworkRttStats* stats) {
  AgentRttStats stun_stats;
  int sctp_rtt_ms;
  if (!pc || !stats) {
    return -1;
  }

  memset(stats, 0, sizeof(*stats));
  stats->current_ms = -1;
  stats->stun_current_ms = -1;
  stats->sctp_current_ms = -1;
  stats->min_ms = -1;
  stats->p95_ms = -1;
  stats->jitter_ms = -1;
  stats->source = PEER_RTT_SOURCE_NONE;

  agent_get_rtt_stats(&pc->agent, &stun_stats);
  sctp_rtt_ms = sctp_get_rtt_ms(&pc->sctp);
  stats->stun_current_ms = stun_stats.current_ms;
  stats->sctp_current_ms = sctp_rtt_ms;
  stats->min_ms = stun_stats.min_ms;
  stats->p95_ms = stun_stats.p95_ms;
  stats->jitter_ms = stun_stats.jitter_ms;
  stats->stun_sample_age_ms = stun_stats.sample_age_ms;
  stats->samples = stun_stats.samples;
  stats->probes_sent = stun_stats.probes_sent;
  stats->probes_matched = stun_stats.probes_matched;
  stats->probes_lost = stun_stats.probes_lost;
  stats->probes_unmatched = stun_stats.probes_unmatched;
  stats->probe_in_flight = stun_stats.probe_in_flight;

  // A recent matched STUN response measures the selected media path directly.
  // If probes are delayed or lost, SCTP SRTT keeps the displayed value live.
  if (stun_stats.current_ms >= 0 && stun_stats.sample_age_ms <= 1500) {
    stats->current_ms = stun_stats.current_ms;
    stats->source = PEER_RTT_SOURCE_STUN;
  } else if (sctp_rtt_ms >= 0) {
    stats->current_ms = sctp_rtt_ms;
    stats->source = PEER_RTT_SOURCE_SCTP;
  } else if (stun_stats.current_ms >= 0) {
    stats->current_ms = stun_stats.current_ms;
    stats->source = PEER_RTT_SOURCE_STUN;
  }
  return 0;
}

int peer_connection_get_ice_candidate_pair_count(PeerConnection* pc) {
  return pc ? pc->agent.candidate_pairs_num : 0;
}

int peer_connection_get_ice_candidate_pair_info(PeerConnection* pc,
                                                int index,
                                                PeerIceCandidatePairInfo* info) {
  if (!pc || !info || index < 0 || index >= pc->agent.candidate_pairs_num)
    return -1;

  memset(info, 0, sizeof(*info));
  IceCandidatePair* pair = &pc->agent.candidate_pairs[index];
  info->priority = pair->priority;
  info->state = pair->state;
  info->connectivity_checks = pair->conncheck;
  info->selected = pc->agent.selected_pair == pair;
  info->nominated = pc->agent.nominated_pair == pair;

  if (pair->local) {
    info->local_type = pair->local->type;
    info->local_port = pair->local->addr.port;
    addr_to_string(&pair->local->addr, info->local_address, sizeof(info->local_address));
  }
  if (pair->remote) {
    info->remote_type = pair->remote->type;
    info->remote_port = pair->remote->addr.port;
    addr_to_string(&pair->remote->addr, info->remote_address, sizeof(info->remote_address));
  }
  return 0;
}

int peer_connection_get_video_rtp_stats(PeerConnection* pc, PeerVideoRtpStats* stats) {
  if (pc == NULL || stats == NULL)
    return -1;
  stats->packets_received = pc->vrtp_decoder.packets_received;
  stats->sequence_gaps = pc->vrtp_decoder.sequence_gaps;
  stats->reordered_packets = pc->vrtp_decoder.reordered_packets;
  stats->late_packets_dropped = pc->vrtp_decoder.late_packets_dropped;
  stats->forced_sequence_skips = pc->vrtp_decoder.forced_sequence_skips;
  stats->reorder_buffered_packets = pc->vrtp_decoder.reorder_buffered_packets;
  stats->startup_sequence_rebases = pc->vrtp_decoder.startup_sequence_rebases;
  stats->reorder_gap_hold_ms = pc->vrtp_decoder.reorder_gap_hold_ms;
  stats->reorder_burst_gap_hold_ms =
      pc->vrtp_decoder.reorder_burst_gap_hold_ms;
  stats->reorder_adaptive_holds = pc->vrtp_decoder.reorder_adaptive_holds;
  stats->reorder_gap_recoveries = pc->vrtp_decoder.reorder_gap_recoveries;
  stats->reorder_gap_expirations = pc->vrtp_decoder.reorder_gap_expirations;
  stats->reorder_burst_fast_forwards =
      pc->vrtp_decoder.reorder_burst_fast_forwards;
  stats->access_units_completed = pc->vrtp_decoder.access_units_completed;
  stats->access_units_dropped = pc->vrtp_decoder.access_units_dropped;
  stats->nack_requests = pc->video_nack_requests;
  stats->nack_packets_requested = pc->video_nack_packets_requested;
  stats->nack_retries = pc->video_nack_retries;
  stats->rtx_packets_received = pc->video_rtx_packets_received;
  stats->rtx_packets_recovered = pc->video_rtx_packets_recovered;
  stats->rtx_packets_invalid = pc->video_rtx_packets_invalid;
  return 0;
}

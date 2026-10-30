#include "luna_stream_session.hpp"

#include "core/app_paths.hpp"
#include "core/i18n.hpp"
#include "core/runtime_journal.hpp"
#include "input/luna_client_messages.hpp"
#include "internal.hpp"
#include "luna/luna_sdp.hpp"
#include "stream/DecodeQueuePolicy.hpp"
#include "stream/audio/AudioRtpUtils.hpp"
#include "stream/deko3d/DKVideoRenderer.hpp"
#include "stream/ffmpeg/AVFrameHolder.hpp"
#include "stream/network_loop_policy.hpp"
#include "stream/stream_diagnostics.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>

using namespace lunanx::webrtc::internal;

namespace lunanx
{
namespace
{

// Présent pendant qu'un flux NVDEC est actif : s'il reste après un plantage, le
// prochain lancement passe automatiquement en décodage logiciel.
std::string NvdecMarkerPath()
{
    return AppHomePath() + "/nvdec_active.marker";
}

bool ConsumeNvdecCrashMarker()
{
    std::ifstream marker(NvdecMarkerPath(), std::ios::binary);
    const bool exists = marker.good();
    marker.close();
    if (exists)
        std::remove(NvdecMarkerPath().c_str());
    return exists;
}

void CreateNvdecMarker()
{
    std::ofstream marker(NvdecMarkerPath(), std::ios::trunc);
    marker << "NVDEC actif\n";
}

LunaStreamSession* Self(void* userdata)
{
    return static_cast<LunaStreamSession*>(userdata);
}

extern "C" {
void on_peer_state_change_cb(PeerConnectionState state, void* userdata)
{
    Self(userdata)->on_peer_state_change(state);
}
void on_datachannel_message_cb(char* msg, size_t len, void* userdata, uint16_t sid)
{
    Self(userdata)->on_datachannel_message(msg, len, sid);
}
void on_datachannel_open_cb(void* userdata)
{
    Self(userdata)->on_datachannel_open();
}
void on_datachannel_close_cb(void* userdata)
{
    Self(userdata)->on_datachannel_close();
}
void on_video_packet_cb(const PeerVideoPacket* packet, void* userdata)
{
    if (packet)
        Self(userdata)->on_video_packet(*packet);
}
void on_audio_packet_cb(const PeerAudioPacket* packet, void* userdata)
{
    if (packet)
        Self(userdata)->on_audio_packet(*packet);
}
void on_rtp_sender_report_cb(uint32_t ssrc, uint64_t ntp_us, uint32_t rtp_timestamp, void* userdata)
{
    Self(userdata)->on_rtp_sender_report(ssrc, ntp_us, rtp_timestamp);
}
}

} // namespace

LunaStreamSession::LunaStreamSession(std::shared_ptr<const luna::LunaClient> client, luna::Game game)
    : client_(std::move(client)),
      game_(std::move(game)),
      settings_(LoadStreamSettings())
{
    SetStreamDiagnosticsEnabled(settings_.debug_diagnostics);
    peer_init();
    renderer_ = std::make_unique<DKVideoRenderer>();
    audio_ = std::make_unique<AudioPipeline>();
    audio_->configure(settings_.audio_volume, settings_.audio_buffer_ms);
    setup_video_decoder();
}

LunaStreamSession::~LunaStreamSession()
{
    stop();
    peer_deinit();
}

void LunaStreamSession::setup_video_decoder()
{
    const bool previous_crash = settings_.video_backend == "Auto" && ConsumeNvdecCrashMarker();
    const bool force_software = settings_.video_backend == "Software" || previous_crash;
    const bool require_hardware = settings_.video_backend == "NVDEC";
    const int flags = force_software ? VIDEO_DECODER_FORCE_SOFTWARE
                    : require_hardware ? VIDEO_DECODER_REQUIRE_HARDWARE
                                       : VIDEO_DECODER_PREFER_HARDWARE;

    decoder_ = std::make_unique<FFmpegVideoDecoder>();
    int result = decoder_->setup(VIDEO_FORMAT_H264, settings_.width, settings_.height,
                                 settings_.fps, nullptr, flags);
    if (result != 0 && !force_software && !require_hardware)
    {
        AppendStreamLog("VIDEO nvdec_failed " + decoder_->setup_diagnostics());
        decoder_->cleanup();
        decoder_ = std::make_unique<FFmpegVideoDecoder>();
        result = decoder_->setup(VIDEO_FORMAT_H264, settings_.width, settings_.height,
                                 settings_.fps, nullptr, VIDEO_DECODER_FORCE_SOFTWARE);
    }

    if (result != 0)
        video_backend_name_ = Tr("échec d'initialisation du décodeur");
    else if (decoder_->uses_hardware_frames())
        video_backend_name_ = Tr("NVDEC (matériel)");
    else
        video_backend_name_ = previous_crash ? Tr("logiciel (mode sans échec)") : Tr("logiciel");

    if (result == 0 && decoder_->uses_hardware_frames())
        CreateNvdecMarker();
}

bool LunaStreamSession::setup_peer_connection()
{
    PeerConfiguration config = {};
    config.video_codec = CODEC_H264;
    config.audio_codec = CODEC_OPUS;
    config.datachannel = DATA_CHANNEL_BINARY;
    config.onvideopacket = on_video_packet_cb;
    config.onaudiopacket = on_audio_packet_cb;
    config.onrtpsenderreport = on_rtp_sender_report_cb;
    config.user_data = this;
    // Candidat réflexif pour les box qui filtrent l'UDP entrant ; le serveur Luna
    // annonce de toute façon un candidat public direct dans sa réponse.
    config.ice_servers[0].urls = "stun:stun.l.google.com:19302";

    pc_ = peer_connection_create(&config);
    if (!pc_)
        return false;
    peer_connection_oniceconnectionstatechange(pc_, on_peer_state_change_cb);
    peer_connection_ondatachannel(pc_, on_datachannel_message_cb, on_datachannel_open_cb,
                                  on_datachannel_close_cb);
    return true;
}

void LunaStreamSession::start()
{
    ResetStreamLog();
    AppendStreamLog("SESSION start game=" + game_.title + " preset=" + settings_.label +
                    " decoder=" + video_backend_name_);
    stop_requested_.store(false);

    if (settings_.audio_enabled)
        audio_->start();

    decoder_running_.store(true);
    decoder_thread_ = std::thread(&LunaStreamSession::decoder_loop, this);
    network_running_.store(true);
    network_thread_ = std::thread(&LunaStreamSession::network_loop, this);
    signaling_thread_ = std::thread(&LunaStreamSession::signaling_loop, this);
}

bool LunaStreamSession::sleep_unless_stopped(std::chrono::milliseconds duration)
{
    std::unique_lock<std::mutex> lock(stop_mutex_);
    return !stop_cv_.wait_for(lock, duration, [this] { return stop_requested_.load(); });
}

void LunaStreamSession::set_status(const std::string& status)
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_ = status;
}

void LunaStreamSession::fail(const std::string& reason)
{
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        failure_ = reason;
        status_ = reason;
    }
    AppendStreamLog("SESSION failed " + reason);
    LogRuntimeEvent("stream", "failed", reason);
    phase_.store(Phase::Failed, std::memory_order_release);
}

void LunaStreamSession::signaling_loop()
{
    try
    {
        set_status(TrFormat("Préparation de « {} »…", {game_.title}));
        if (game_.play_token.empty())
            game_ = client_->FetchGameDetail(game_);

        set_status(Tr("Recherche du serveur le plus proche…"));
        const auto regions = client_->MeasureRegions();
        for (const auto& region : regions)
            AppendStreamLog("REGION " + region.region + " " + std::to_string(region.latency_ms) + " ms");

        try
        {
            if (client_->IsPinChallengeRequired())
                AppendStreamLog("PIN contrôle parental demandé : le lancement peut être refusé");
        }
        catch (const luna::LunaError& ex)
        {
            AppendStreamLog(std::string("PIN vérification impossible : ") + ex.what());
        }

        std::string offer;
        {
            std::lock_guard<std::recursive_mutex> lock(peer_mutex_);
            if (!setup_peer_connection())
                throw luna::LunaError(Tr("Impossible de créer la connexion WebRTC"));
            const char* local = peer_connection_create_offer(pc_);
            const auto transport = luna::ParseLocalTransport(local ? local : "");
            if (transport.ice_ufrag.empty() || transport.fingerprint.empty())
                throw luna::LunaError(Tr("Offre WebRTC locale incomplète"));
            offer = luna::BuildLunaOffer(transport);
        }
        AppendStreamLog("SDP offer\n" + offer);
        if (stop_requested_.load())
            return;

        set_status(Tr("Réservation d'une machine Luna…"));
        phase_.store(Phase::WaitingForServer);
        const std::string job_id = client_->CreateGameSessionJob(
            game_, offer, input::LunaInputEncoder::RequestedChannels(), regions, settings_.region);
        AppendStreamLog("JOB " + job_id);

        if (!sleep_unless_stopped(std::chrono::milliseconds(1500)))
            return;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(15);
        luna::SessionJob job;
        while (!stop_requested_.load())
        {
            job = client_->GetGameSessionJob(job_id);
            AppendStreamLog("JOB status=" + job.raw_status);
            if (job.status != luna::SessionJobStatus::Pending)
                break;
            set_status(job.raw_status.find("QUEUE") != std::string::npos
                ? Tr("En file d'attente…")
                : Tr("Démarrage de la machine distante…"));
            if (std::chrono::steady_clock::now() > deadline)
                throw luna::LunaError(Tr("Le serveur Luna ne répond pas (délai dépassé)"));
            if (!sleep_unless_stopped(std::chrono::milliseconds(std::clamp(job.retry_after_ms, 500, 5000))))
                return;
        }
        if (stop_requested_.load())
            return;
        if (job.status == luna::SessionJobStatus::Failed)
            throw luna::LunaError(TrFormat("Luna a refusé la session : {}", {job.error}));

        const std::string answer = luna::NormalizeLunaAnswer(job.answer_sdp);
        AppendStreamLog("SDP answer region=" + job.region + " routing=" + job.routing +
                        " h264pt=" + std::to_string(luna::AnsweredH264PayloadType(answer)) + "\n" + answer);
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            server_region_ = job.region;
        }
        {
            std::lock_guard<std::recursive_mutex> lock(peer_mutex_);
            peer_connection_set_remote_description(pc_, answer.c_str(), SDP_TYPE_ANSWER);
            // Rapports RTCP + REMB : sans eux le serveur reste bloqué à bas débit.
            const uint32_t max_bitrate = static_cast<uint32_t>(std::max(settings_.bitrate_kbps, 3000)) * 1000u;
            peer_connection_set_max_receive_bitrate(pc_, max_bitrate);
            AppendStreamLog("REMB max=" + std::to_string(max_bitrate / 1000) + " kbit/s");
            remote_description_set_.store(true, std::memory_order_release);
        }
        phase_.store(Phase::Connecting);
        set_status(Tr("Connexion au flux vidéo…"));
    }
    catch (const luna::SignInRequired& ex)
    {
        fail(TrFormat("Session Amazon expirée : reconnecte-toi. ({})", {ex.what()}));
    }
    catch (const std::exception& ex)
    {
        fail(ex.what());
    }
}

void LunaStreamSession::network_loop()
{
    while (network_running_.load(std::memory_order_acquire))
    {
        int batch = 0;
        bool can_run = false;
        {
            std::lock_guard<std::recursive_mutex> lock(peer_mutex_);
            can_run = pc_ && remote_description_set_.load(std::memory_order_acquire);
            if (can_run)
            {
                constexpr int kMaximumDatagramsPerBatch = 24;
                for (int i = 0; i < kMaximumDatagramsPerBatch; ++i)
                {
                    const int processed = i == 0 ? peer_connection_loop(pc_)
                                                 : peer_connection_loop_nonblocking(pc_);
                    if (processed == 0)
                        break;
                    ++batch;
                    // Laisse passer immédiatement un rapport manette en attente.
                    if (interactive_input_pending_.load(std::memory_order_acquire))
                        break;
                }
            }
        }
        const int backoff = network::LoopBackoffMilliseconds(can_run, batch);
        if (backoff > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
        else
            std::this_thread::yield();
    }
}

void LunaStreamSession::poll()
{
    if (stop_requested_.load() || !remote_description_set_.load(std::memory_order_acquire))
        return;

    std::lock_guard<std::recursive_mutex> lock(peer_mutex_);
    if (!pc_)
        return;
    const PeerConnectionState state = peer_connection_get_state(pc_);
    if (state == PEER_CONNECTION_COMPLETED)
    {
        if (!peer_ever_completed_)
        {
            peer_ever_completed_ = true;
            streaming_since_ = std::chrono::steady_clock::now();
            AppendStreamLog("PEER completed");
            request_keyframe("startup");
        }
        open_datachannels();
        maybe_send_ping();
        if (frames_decoded_.load() > 0 && phase() != Phase::Streaming)
        {
            phase_.store(Phase::Streaming);
            set_status(Tr("En jeu"));
        }
        if (keyframe_needed_.exchange(false) && decoder_resync_required_.load())
            request_keyframe("decoder_resync");
        maybe_recover_rtp_damage();
        maybe_recover_decode_stall();
    }
    else if (state == PEER_CONNECTION_FAILED || state == PEER_CONNECTION_CLOSED ||
             state == PEER_CONNECTION_DISCONNECTED)
    {
        if (phase() != Phase::Failed && phase() != Phase::Ended)
        {
            if (peer_ever_completed_)
            {
                phase_.store(Phase::Ended);
                set_status(Tr("La partie est terminée ou la connexion a été perdue."));
                AppendStreamLog("PEER ended state=" + std::string(peer_connection_state_to_string(state)));
            }
            else
            {
                fail(TrFormat("Connexion WebRTC impossible ({})", {peer_connection_state_to_string(state)}));
            }
        }
    }
}

void LunaStreamSession::open_datachannels()
{
    // Les deux canaux du client web : entrées (SCTP 1) et messages client (SCTP 3).
    const std::string labels[] = {input::LunaInputEncoder::InputChannelLabel(),
                                  input::LunaInputEncoder::ClientMessageChannelLabel()};
    if (datachannels_requested_ || datachannel_open_attempts_ >= 60)
        return;
    // L'association SCTP s'établit après DTLS : on réessaie toutes les 750 ms.
    const auto now = std::chrono::steady_clock::now();
    if (now - last_datachannel_attempt_at_ < std::chrono::milliseconds(750))
        return;
    last_datachannel_attempt_at_ = now;
    ++datachannel_open_attempts_;

    bool all_open = true;
    char protocol[] = "";
    for (size_t i = 0; i < std::size(labels); ++i)
    {
        if (datachannel_opened_mask_ & (1u << i))
            continue;
        // Nous sommes serveur DTLS : les identifiants SCTP impairs nous reviennent (RFC 8832).
        const uint16_t sid = static_cast<uint16_t>(1 + 2 * i);
        std::vector<char> name(labels[i].begin(), labels[i].end());
        name.push_back('\0');
        // Le SCTP embarqué ne négocie pas Forward-TSN : canaux fiables uniquement.
        const int result = peer_connection_create_datachannel_sid(
            pc_, DATA_CHANNEL_RELIABLE, 0, 0, name.data(), protocol, sid);
        AppendStreamLog("DC open label=" + labels[i] + " sid=" + std::to_string(sid) +
                        " attempt=" + std::to_string(datachannel_open_attempts_) +
                        " result=" + std::to_string(result));
        if (result < 0)
        {
            all_open = false;
            continue;
        }
        datachannel_opened_mask_ |= (1u << i);
        if (labels[i] == input::LunaInputEncoder::InputChannelLabel())
        {
            input_sid_ = sid;
            send_on_channel(input_sid_, input::LunaInputEncoder::EncodeInputReady());
        }
        else
        {
            client_message_sid_ = sid;
            send_client_startup_messages();
        }
    }
    datachannels_requested_ = all_open;
}

bool LunaStreamSession::send_on_channel(int sid, const std::vector<uint8_t>& message)
{
    if (!pc_ || sid < 0)
        return false;
    std::vector<uint8_t> copy = message;
    return peer_connection_datachannel_send_binary_sid(
               pc_, reinterpret_cast<char*>(copy.data()), copy.size(), static_cast<uint16_t>(sid)) >= 0;
}

void LunaStreamSession::send_client_startup_messages()
{
    // Sans la configuration d'affichage, le serveur n'envoie que le son.
    input::DisplayRequest display;
    display.width = settings_.width;
    display.height = settings_.height;
    display.fps = settings_.fps;
    if (settings_.bitrate_kbps > 0)
        display.start_bitrate = static_cast<uint32_t>(settings_.bitrate_kbps) * 1000u;
    if (display.max_bitrate < display.start_bitrate)
        display.max_bitrate = display.start_bitrate;

    const std::vector<uint8_t> messages[] = {
        input::EncodeSessionHello(),
        input::EncodeDisplayRequest(display),
        input::EncodeClientInfo(),
    };
    const char* names[] = {"hello", "display", "client_info"};
    for (size_t i = 0; i < std::size(messages); ++i)
    {
        const bool sent = send_on_channel(client_message_sid_, messages[i]);
        AppendStreamLog(std::string("CLIENT send ") + names[i] + " ok=" + (sent ? "1" : "0") +
                        " hex=" + HexPreview(messages[i].data(), messages[i].size(), 72));
    }
    AppendStreamLog("CLIENT display " + std::to_string(display.width) + "x" +
                    std::to_string(display.height) + "@" + std::to_string(display.fps));
    last_ping_at_ = std::chrono::steady_clock::now();
}

void LunaStreamSession::maybe_send_ping()
{
    if (client_message_sid_ < 0)
        return;
    const auto now = std::chrono::steady_clock::now();
    if (now - last_ping_at_ < std::chrono::seconds(1))
        return;
    last_ping_at_ = now;
    send_on_channel(client_message_sid_, input::EncodePing(input::WallClockMilliseconds()));
}

void LunaStreamSession::on_peer_state_change(PeerConnectionState state)
{
    peer_state_ = state;
    AppendStreamLog(std::string("PEER state=") + peer_connection_state_to_string(state));
}

void LunaStreamSession::on_datachannel_message(const char* msg, size_t len, uint16_t sid)
{
    // Journalise les premiers messages de chaque canal : c'est ce qui permettra
    // d'ajuster le protocole d'entrée et de contrôle de Luna.
    int& logged = datachannel_rx_logged_[sid % datachannel_rx_logged_.size()];
    if (logged < 40)
    {
        ++logged;
        const char* label = pc_ ? peer_connection_lookup_sid_label(pc_, sid) : nullptr;
        const auto* bytes = reinterpret_cast<const uint8_t*>(msg);
        AppendStreamLog("DC rx sid=" + std::to_string(sid) + " label=" + (label ? label : "?") +
                        " len=" + std::to_string(len) +
                        " field=" + std::to_string(input::ServerMessageField(bytes, len)) +
                        " hex=" + HexPreview(bytes, len, 64));
    }
    input::RumbleCommand rumble;
    if (input::ParseRumble(reinterpret_cast<const uint8_t*>(msg), len, rumble))
    {
        rumble_.Send(rumble.heavy, rumble.light);
        if ((rumble.heavy || rumble.light) && rumble_events_logged_ < 5)
        {
            ++rumble_events_logged_;
            AppendStreamLog("RUMBLE heavy=" + std::to_string(rumble.heavy) +
                            " light=" + std::to_string(rumble.light));
        }
        return;
    }
    // Le serveur peut ouvrir lui-même le canal d'entrée : on s'y rattache.
    if (input_sid_ < 0 && pc_)
    {
        const char* label = peer_connection_lookup_sid_label(pc_, sid);
        if (label && input::LunaInputEncoder::InputChannelLabel() == label)
            input_sid_ = sid;
    }
}

void LunaStreamSession::on_datachannel_open()
{
    AppendStreamLog("DC sctp_open");
}

void LunaStreamSession::on_datachannel_close()
{
    AppendStreamLog("DC sctp_closed");
    rumble_.Stop();
    input_sid_ = -1;
    client_message_sid_ = -1;
}

bool LunaStreamSession::send_gamepad(const input::GamepadState& state)
{
    if (input_sid_ < 0)
        return false;
    interactive_input_pending_.store(true, std::memory_order_release);
    std::lock_guard<std::recursive_mutex> lock(peer_mutex_);
    interactive_input_pending_.store(false, std::memory_order_release);
    if (!pc_ || input_sid_ < 0)
        return false;

    std::vector<uint8_t> report = input_encoder_.EncodeGamepad(state);
    const int sent = peer_connection_datachannel_send_binary_sid(
        pc_, reinterpret_cast<char*>(report.data()), report.size(), static_cast<uint16_t>(input_sid_));
    if (sent < 0)
        return false;
    const uint64_t count = gamepad_reports_sent_.fetch_add(1) + 1;
    if (count <= 3)
        AppendStreamLog("INPUT gamepad hex=" + HexPreview(report.data(), report.size(), 48));
    return true;
}

void LunaStreamSession::request_stop()
{
    if (stop_requested_.exchange(true))
        return;
    stop_cv_.notify_all();
    network_running_.store(false);
    decoder_running_.store(false);
    decoder_queue_cv_.notify_all();
}

void LunaStreamSession::stop()
{
    request_stop();
    if (signaling_thread_.joinable())
        signaling_thread_.join();
    if (network_thread_.joinable())
        network_thread_.join();
    // Après l'arrêt du réseau, pour qu'aucun message tardif ne relance la vibration.
    rumble_.Stop();
    if (audio_)
        audio_->stop();
    if (decoder_thread_.joinable())
        decoder_thread_.join();
    {
        std::lock_guard<std::mutex> lock(decoder_queue_mutex_);
        clear_decoder_queue_locked();
        decoder_buffer_pool_.clear();
    }

    std::lock_guard<std::recursive_mutex> lock(peer_mutex_);
    if (pc_)
    {
        peer_connection_close(pc_);
        peer_connection_destroy(pc_);
        pc_ = nullptr;
    }
    // Les surfaces deko3d référencent la mémoire NVDEC : libérer le rendu avant le décodeur.
    renderer_.reset();
    if (decoder_)
    {
        const bool hardware = decoder_->uses_hardware_frames();
        decoder_->cleanup();
        decoder_.reset();
        if (hardware)
            std::remove(NvdecMarkerPath().c_str());
        AppendStreamLog("SESSION stopped packets=" + std::to_string(packets_received_.load()) +
                        " frames=" + std::to_string(frames_decoded_.load()) +
                        " gamepadReports=" + std::to_string(gamepad_reports_sent_.load()));
    }
}

void LunaStreamSession::decoder_loop()
{
    for (;;)
    {
        DecodeUnit unit;
        {
            std::unique_lock<std::mutex> lock(decoder_queue_mutex_);
            decoder_queue_cv_.wait(lock, [this] {
                return !decoder_running_.load(std::memory_order_acquire) || !decoder_queue_.empty();
            });
            if (!decoder_running_.load(std::memory_order_acquire))
                break;
            unit = std::move(decoder_queue_.front());
            decoder_queue_.pop_front();
        }

        if (decoder_reset_requested_.exchange(false) && decoder_)
            decoder_->reset_stream();
        const int decoded = decoder_
            ? decoder_->submit_decode_unit(unit.data.data(), static_cast<int>(unit.data.size()),
                                           unit.rtp_timestamp)
            : -1;
        if (decoded > 0)
        {
            last_decoded_frame_at_us_.store(NowUs(), std::memory_order_release);
            if (frames_decoded_.fetch_add(decoded) == 0)
                AppendStreamLog("DECODE first_frame");
        }
        else if (decoded < 0)
        {
            decode_errors_.fetch_add(1);
            decoder_resync_required_.store(true);
            if (!decoder_ || !decoder_->uses_hardware_frames())
                decoder_reset_requested_.store(true);
            keyframe_needed_.store(true);
        }
        if (unit.idr && decoded >= 0)
        {
            decoder_resync_required_.store(false);
            keyframe_needed_.store(false);
        }

        std::lock_guard<std::mutex> lock(decoder_queue_mutex_);
        recycle_decode_buffer_locked(std::move(unit.data));
    }
}

void LunaStreamSession::recycle_decode_buffer_locked(std::vector<uint8_t>&& buffer)
{
    constexpr size_t kPoolLimit = 8;
    constexpr size_t kMaximumReusableCapacity = 2 * 1024 * 1024;
    if (decoder_buffer_pool_.size() >= kPoolLimit || buffer.capacity() > kMaximumReusableCapacity)
        return;
    buffer.clear();
    decoder_buffer_pool_.push_back(std::move(buffer));
}

void LunaStreamSession::clear_decoder_queue_locked()
{
    while (!decoder_queue_.empty())
    {
        recycle_decode_buffer_locked(std::move(decoder_queue_.front().data));
        decoder_queue_.pop_front();
    }
}

void LunaStreamSession::enqueue_decode_unit(const uint8_t* data, size_t size, uint32_t rtp_timestamp)
{
    if (!data || size == 0 || !decoder_running_.load(std::memory_order_acquire))
        return;

    const bool idr = ContainsH264Idr(data, size);
    std::lock_guard<std::mutex> lock(decoder_queue_mutex_);
    if (decoder_resync_required_.load() && !idr)
    {
        decoder_queue_drops_++;
        keyframe_needed_.store(true, std::memory_order_release);
        return;
    }

    // File bornée (~67 ms) : au-delà, on repart de la prochaine image clé plutôt
    // que d'accumuler de la latence visible.
    if (decoder_queue_.size() >= video::MaximumQueuedAccessUnits(settings_.fps))
    {
        decoder_queue_drops_ += static_cast<int>(decoder_queue_.size());
        clear_decoder_queue_locked();
        decoder_resync_required_.store(true);
        if (!decoder_ || !decoder_->uses_hardware_frames())
            decoder_reset_requested_.store(true);
        keyframe_needed_.store(true);
        if (!idr)
            return;
    }

    std::vector<uint8_t> buffer;
    if (!decoder_buffer_pool_.empty())
    {
        buffer = std::move(decoder_buffer_pool_.front());
        decoder_buffer_pool_.pop_front();
    }
    buffer.assign(data, data + size);
    decoder_queue_.push_back({std::move(buffer), idr, rtp_timestamp, std::chrono::steady_clock::now()});
    decoder_queue_cv_.notify_one();
}

void LunaStreamSession::request_keyframe(const char* reason)
{
    const auto now = std::chrono::steady_clock::now();
    const auto minimum_interval = decoder_resync_required_.load()
        ? std::chrono::milliseconds(frames_decoded_.load() == 0 ? 600 : 900)
        : std::chrono::milliseconds(1500);
    if (last_keyframe_request_at_.time_since_epoch().count() != 0 &&
        now - last_keyframe_request_at_ < minimum_interval)
    {
        if (decoder_resync_required_.load())
            keyframe_needed_.store(true);
        return;
    }
    last_keyframe_request_at_ = now;
    ++keyframe_request_attempts_;
    const int result = pc_ ? peer_connection_request_video_keyframe(pc_) : -1;
    AppendStreamLog(std::string("KEYFRAME reason=") + reason + " pli=" + std::to_string(result));
    if (decoder_resync_required_.load())
        keyframe_needed_.store(true);
}

void LunaStreamSession::maybe_recover_decode_stall()
{
    const uint64_t last_decoded = last_decoded_frame_at_us_.load(std::memory_order_acquire);
    if (!pc_ || frames_decoded_.load() == 0 || last_decoded == 0)
        return;
    const uint64_t now_us = NowUs();
    if (now_us < last_decoded || (now_us - last_decoded) / 1000 < 1500)
        return;
    const auto now = std::chrono::steady_clock::now();
    if (last_decode_stall_request_at_.time_since_epoch().count() != 0 &&
        now - last_decode_stall_request_at_ < std::chrono::seconds(2))
        return;
    last_decode_stall_request_at_ = now;
    {
        std::lock_guard<std::mutex> lock(decoder_queue_mutex_);
        clear_decoder_queue_locked();
    }
    decoder_resync_required_.store(true);
    if (!decoder_ || !decoder_->uses_hardware_frames())
        decoder_reset_requested_.store(true);
    request_keyframe("decode_stall");
}

void LunaStreamSession::maybe_recover_rtp_damage()
{
    PeerVideoRtpStats stats {};
    if (!pc_ || peer_connection_get_video_rtp_stats(pc_, &stats) != 0 ||
        stats.access_units_dropped <= last_rtp_access_units_dropped_)
        return;
    last_rtp_access_units_dropped_ = stats.access_units_dropped;
    // Une image de référence manque : on ignore les P suivantes jusqu'à la prochaine IDR.
    if (!decoder_resync_required_.exchange(true))
        keyframe_needed_.store(true);
}

void LunaStreamSession::on_video_packet(const PeerVideoPacket& packet)
{
    PeerVideoRtpStats stats {};
    if (pc_ && peer_connection_get_video_rtp_stats(pc_, &stats) == 0)
    {
        uint32_t previous = last_rtp_damage_gate_.load(std::memory_order_relaxed);
        if (stats.access_units_dropped > previous &&
            last_rtp_damage_gate_.compare_exchange_strong(previous, stats.access_units_dropped))
        {
            decoder_resync_required_.store(true, std::memory_order_release);
            keyframe_needed_.store(true, std::memory_order_release);
        }
    }

    video_ssrc_ = packet.ssrc;
    for (const auto& report : sender_reports_)
    {
        if (report.ssrc == video_ssrc_)
        {
            video_sr_ntp_us_ = report.ntp_us;
            video_sr_rtp_timestamp_ = report.rtp_timestamp;
            have_video_sender_report_ = true;
            break;
        }
    }
    if (packets_received_.fetch_add(1) == 0)
        AppendStreamLog("VIDEO first_access_unit size=" + std::to_string(packet.size) +
                        " idr=" + std::to_string(ContainsH264Idr(packet.data, packet.size) ? 1 : 0));
    enqueue_decode_unit(packet.data, packet.size, packet.timestamp);
}

void LunaStreamSession::on_audio_packet(const PeerAudioPacket& packet)
{
    const bool new_ssrc = audio_ssrc_ != packet.ssrc;
    audio_ssrc_ = packet.ssrc;
    if (!audio_)
        return;
    audio_->submit(packet);
    if (new_ssrc)
        for (const auto& report : sender_reports_)
            if (report.ssrc == audio_ssrc_)
                audio_->set_sender_report(report.ssrc, report.ntp_us, report.rtp_timestamp);
}

void LunaStreamSession::on_rtp_sender_report(uint32_t ssrc, uint64_t ntp_us, uint32_t rtp_timestamp)
{
    std::lock_guard<std::recursive_mutex> lock(peer_mutex_);
    auto found = std::find_if(sender_reports_.begin(), sender_reports_.end(),
                              [ssrc](const SenderReport& r) { return r.ssrc == ssrc; });
    if (found == sender_reports_.end())
        sender_reports_.push_back({ssrc, ntp_us, rtp_timestamp});
    else
        *found = {ssrc, ntp_us, rtp_timestamp};
    if (ssrc == audio_ssrc_ && audio_)
        audio_->set_sender_report(ssrc, ntp_us, rtp_timestamp);
    if (ssrc == video_ssrc_)
    {
        video_sr_ntp_us_ = ntp_us;
        video_sr_rtp_timestamp_ = rtp_timestamp;
        have_video_sender_report_ = true;
    }
}

int64_t LunaStreamSession::video_target_rtp_timestamp() const
{
    std::lock_guard<std::recursive_mutex> lock(peer_mutex_);
    if (!audio_ || !have_video_sender_report_)
        return AV_NOPTS_VALUE;
    const int64_t audio_ntp = audio_->playback_ntp_us();
    if (audio_ntp < 0)
        return AV_NOPTS_VALUE;
    return audio::RtpTimestampAtNtp(video_sr_rtp_timestamp_, static_cast<int64_t>(video_sr_ntp_us_),
                                    audio_ntp, 90000);
}

void LunaStreamSession::draw(NVGcontext* vg, int width, int height, AVFrame* frame, uint64_t generation)
{
    if (!renderer_ || !frame)
        return;
    rendered_width_.store(frame->width, std::memory_order_relaxed);
    rendered_height_.store(frame->height, std::memory_order_relaxed);
    renderer_->drawLatest(vg, width, height, frame, VIDEO_FORMAT_H264, generation);
}

std::string LunaStreamSession::status_text() const
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}

std::string LunaStreamSession::debug_text() const
{
    std::string region;
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        region = server_region_;
    }
    return TrFormat("Région {} · décodeur {} · {}x{} · paquets {} · images {} · erreurs {} · manette {}",
                    {region.empty() ? std::string("?") : region, video_backend_name_,
                     std::to_string(rendered_width_.load()), std::to_string(rendered_height_.load()),
                     std::to_string(packets_received_.load()), std::to_string(frames_decoded_.load()),
                     std::to_string(decode_errors_.load()), Tr(input_sid_ >= 0 ? "prête" : "en attente")});
}

} // namespace lunanx

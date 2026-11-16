// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=18697a5bf5413a90c9a3
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include "input/controller_delivery_policy.hpp"
#include "input/stream_menu_buttons.hpp"
#include "input/luna_input_encoder.hpp"
#include "luna/luna_client.hpp"
#include "webrtc/luna_stream_session.hpp"

#include <borealis.hpp>

struct AVFrame;

#include <chrono>
#include <memory>

namespace lunanx
{

// Plein écran pendant une partie : vidéo, état de connexion et manette.
// Quitter : ZL + ZR + Moins. Batterie/heure/débit/FPS : Plus + Moins.
class StreamView : public brls::Box
{
  public:
    StreamView(std::shared_ptr<const luna::LunaClient> client, luna::Game game);
    ~StreamView() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

  private:
    void HandleInput(const brls::ControllerState& state);
    void DrawStatus(NVGcontext* vg, float x, float y, float width, float height);
    void DrawDebugOverlay(NVGcontext* vg, float x, float y, float width);
    void UpdateQuickStats(std::chrono::steady_clock::time_point now);
    void DrawQuickStats(NVGcontext* vg, float x, float y, float width);
    void DrawLoadingDots(NVGcontext* vg, float cx, float cy);
    void UpdateGameVisibility(const AVFrame* frame);
    void Exit();

    std::unique_ptr<LunaStreamSession> session_;
    std::string title_;
    std::unique_ptr<brls::Image> launch_logo_;
    std::string controller_layout_;
    bool exiting_ = false;
    // Le jeu est « visible » dès qu'une image n'est plus noire (les jeux démarrent sur du noir).
    bool game_visible_ = false;
    std::chrono::steady_clock::time_point first_frame_at_ {};
    std::chrono::steady_clock::time_point ended_at_ {};
    bool show_debug_ = false;
    bool debug_combo_was_down_ = false;
    bool show_quick_stats_ = false;
    input::StreamMenuButtons menu_buttons_;
    StreamRateSampler rates_;
    BatteryEstimate battery_estimate_;
    bool quick_stats_sampled_ = false;
    std::chrono::steady_clock::time_point quick_stats_at_ {};
    std::string quick_clock_, quick_battery_, quick_rates_;
    std::chrono::steady_clock::time_point last_report_at_ {};
    input::GamepadState last_state_ {};
    bool state_sent_ = false;
};

} // namespace lunanx

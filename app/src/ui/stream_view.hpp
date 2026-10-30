#pragma once

#include "input/controller_delivery_policy.hpp"
#include "input/luna_input_encoder.hpp"
#include "luna/luna_client.hpp"
#include "webrtc/luna_stream_session.hpp"

#include <borealis.hpp>

#include <chrono>
#include <memory>

namespace lunanx
{

// Plein écran pendant une partie : vidéo, état de connexion et manette.
// Quitter : ZL + ZR + Moins. Superposition technique : L3 + R3.
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
    void Exit();

    std::unique_ptr<LunaStreamSession> session_;
    std::string title_;
    std::string controller_layout_;
    bool exiting_ = false;
    bool show_debug_ = false;
    bool debug_combo_was_down_ = false;
    bool plus_was_down_ = false;
    bool plus_long_press_ = false;
    bool back_hold_armed_ = true;
    std::chrono::steady_clock::time_point plus_pressed_at_ {};
    std::chrono::steady_clock::time_point last_report_at_ {};
    input::StartDeliveryPulse start_pulse_;
    input::GamepadState last_state_ {};
    bool state_sent_ = false;
};

} // namespace lunanx

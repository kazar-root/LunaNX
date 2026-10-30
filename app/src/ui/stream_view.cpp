#include "stream_view.hpp"

#include "core/i18n.hpp"
#include "input/controller_layout.hpp"
#include "stream/ffmpeg/AVFrameHolder.hpp"

#include <algorithm>
#include <cmath>

namespace lunanx
{
namespace
{

void ApplyRadialDeadzone(float& x, float& y)
{
    constexpr float kDeadzone = 0.12f;
    const float magnitude = std::sqrt(x * x + y * y);
    if (magnitude <= kDeadzone)
    {
        x = 0.0f;
        y = 0.0f;
        return;
    }
    const float normalized = std::min(1.0f, (magnitude - kDeadzone) / (1.0f - kDeadzone));
    const float scale = normalized / magnitude;
    x *= scale;
    y *= scale;
}

void DrawCenteredText(NVGcontext* vg, float cx, float y, float size, NVGcolor color, const std::string& text)
{
    nvgFontSize(vg, size);
    nvgFontFaceId(vg, brls::Application::getDefaultFont());
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, color);
    nvgText(vg, cx, y, text.c_str(), nullptr);
}

} // namespace

StreamView::StreamView(std::shared_ptr<const luna::LunaClient> client, luna::Game game)
    : brls::Box(brls::Axis::COLUMN)
    , title_(game.title)
{
    setGrow(1.0f);
    setFocusable(true);
    setHideHighlight(true);
    session_ = std::make_unique<LunaStreamSession>(std::move(client), std::move(game));
    controller_layout_ = session_->settings().controller_layout;
    show_debug_ = session_->settings().debug_diagnostics;
    session_->start();
}

StreamView::~StreamView()
{
    if (session_)
        session_->stop();
    AVFrameHolder::instance().cleanup();
}

void StreamView::Exit()
{
    if (exiting_)
        return;
    exiting_ = true;
    if (session_)
        session_->request_stop();
    brls::sync([] { brls::Application::popActivity(); });
}

void StreamView::HandleInput(const brls::ControllerState& state)
{
    const auto now = std::chrono::steady_clock::now();
    const auto phase = session_->phase();

    // Quitter : ZL + ZR + Moins à tout moment ; B suffit tant que le jeu n'a pas démarré.
    const bool exit_combo = state.buttons[brls::BUTTON_LT] && state.buttons[brls::BUTTON_RT] &&
                            state.buttons[brls::BUTTON_BACK];
    const bool not_playing = phase != LunaStreamSession::Phase::Streaming &&
                             phase != LunaStreamSession::Phase::Connecting;
    if (exit_combo || (not_playing && state.buttons[brls::BUTTON_B]))
    {
        Exit();
        return;
    }

    // L3 + R3 n'affiche/masque les infos techniques que si l'option est activée dans les
    // réglages : en jeu, les deux sticks sont souvent enfoncés ensemble sans le vouloir.
    const bool debug_combo = session_->settings().debug_diagnostics &&
                             state.buttons[brls::BUTTON_LSB] && state.buttons[brls::BUTTON_RSB];
    if (debug_combo && !debug_combo_was_down_)
        show_debug_ = !show_debug_;
    debug_combo_was_down_ = debug_combo;

    if (phase != LunaStreamSession::Phase::Streaming && phase != LunaStreamSession::Phase::Connecting)
        return;

    // Plus court = Start (impulsion garantie), Plus long (> 0,5 s) = bouton Xbox.
    const bool plus_down = state.buttons[brls::BUTTON_START];
    if (plus_down && !plus_was_down_)
    {
        plus_pressed_at_ = now;
        plus_long_press_ = false;
    }
    if (plus_down && !plus_long_press_ && now - plus_pressed_at_ >= std::chrono::milliseconds(500))
        plus_long_press_ = true;
    if (!plus_down && plus_was_down_ && !plus_long_press_)
        start_pulse_.Queue(now);
    plus_was_down_ = plus_down;

    input::GamepadState pad;
    if (state.buttons[brls::BUTTON_UP]) pad.buttons |= input::kDpadUp;
    if (state.buttons[brls::BUTTON_DOWN]) pad.buttons |= input::kDpadDown;
    if (state.buttons[brls::BUTTON_LEFT]) pad.buttons |= input::kDpadLeft;
    if (state.buttons[brls::BUTTON_RIGHT]) pad.buttons |= input::kDpadRight;
    if (start_pulse_.IsActive(now)) pad.buttons |= input::kStart;
    if (plus_down && plus_long_press_) pad.buttons |= input::kGuide;
    if (state.buttons[brls::BUTTON_BACK]) pad.buttons |= input::kBack;
    if (state.buttons[brls::BUTTON_LSB]) pad.buttons |= input::kLeftThumb;
    if (state.buttons[brls::BUTTON_RSB]) pad.buttons |= input::kRightThumb;
    if (state.buttons[brls::BUTTON_LB]) pad.buttons |= input::kLeftShoulder;
    if (state.buttons[brls::BUTTON_RB]) pad.buttons |= input::kRightShoulder;
    pad.buttons |= MapFaceButtons(controller_layout_,
                                         state.buttons[brls::BUTTON_A], state.buttons[brls::BUTTON_B],
                                         state.buttons[brls::BUTTON_X], state.buttons[brls::BUTTON_Y]);
    pad.left_trigger = state.buttons[brls::BUTTON_LT] ? 0xff : 0x00;
    pad.right_trigger = state.buttons[brls::BUTTON_RT] ? 0xff : 0x00;

    float lx = state.axes[brls::LEFT_X];
    float ly = state.axes[brls::LEFT_Y];
    float rx = state.axes[brls::RIGHT_X];
    float ry = state.axes[brls::RIGHT_Y];
    ApplyRadialDeadzone(lx, ly);
    ApplyRadialDeadzone(rx, ry);
    // La croix directionnelle pilote aussi le stick gauche quand celui-ci est au repos.
    if (lx == 0.0f && ly == 0.0f)
    {
        lx = static_cast<float>(state.buttons[brls::BUTTON_RIGHT]) - static_cast<float>(state.buttons[brls::BUTTON_LEFT]);
        ly = static_cast<float>(state.buttons[brls::BUTTON_DOWN]) - static_cast<float>(state.buttons[brls::BUTTON_UP]);
        if (lx != 0.0f && ly != 0.0f)
        {
            lx *= 0.7071f;
            ly *= 0.7071f;
        }
    }
    // Borealis : vers le bas = positif ; XInput : vers le haut = positif.
    pad.left_x = input::AxisToXInput(lx);
    pad.left_y = input::AxisToXInput(-ly);
    pad.right_x = input::AxisToXInput(rx);
    pad.right_y = input::AxisToXInput(-ry);

    // Envoi à chaque changement, plus un rappel toutes les 100 ms.
    const bool changed = !state_sent_ || pad != last_state_;
    const bool keepalive = now - last_report_at_ >= std::chrono::milliseconds(100);
    if (!changed && !keepalive)
        return;
    if (session_->send_gamepad(pad))
    {
        state_sent_ = true;
        last_state_ = pad;
        last_report_at_ = now;
        if ((pad.buttons & input::kStart) && start_pulse_.OnReportDelivered(now))
            state_sent_ = false;
    }
    else
    {
        state_sent_ = false;
    }
}

void StreamView::draw(NVGcontext* vg, float x, float y, float width, float height,
                      brls::Style style, brls::FrameContext* ctx)
{
    if (!session_)
        return;

    brls::ControllerState state {};
    brls::Application::getPlatform()->getInputManager()->updateUnifiedControllerState(&state);
    HandleInput(state);
    if (exiting_)
        return;
    session_->poll();

    // Jeu en streaming : on affiche toujours l'image la plus récente plutôt que de la caler
    // sur l'horloge du son, qui ajoutait la profondeur du tampon audio à la latence.
    bool got_frame = false;
    AVFrameHolder::instance().get(
        [this, vg, width, height, &got_frame](AVFrame* frame, uint64_t generation, bool) {
            session_->draw(vg, static_cast<int>(width), static_cast<int>(height), frame, generation);
            got_frame = true;
        });

    // La vidéo est dessinée directement par deko3d, alors que NanoVG n'envoie ses dessins
    // au GPU qu'en fin d'image : tout fond opaque dessiné ici recouvrirait la vidéo.
    if (!got_frame)
    {
        nvgBeginPath(vg);
        nvgRect(vg, x, y, width, height);
        nvgFillColor(vg, nvgRGB(0, 0, 0));
        nvgFill(vg);
        DrawStatus(vg, x, y, width, height);
    }
    if (show_debug_)
        DrawDebugOverlay(vg, x, y, width);
}

void StreamView::DrawStatus(NVGcontext* vg, float x, float y, float width, float height)
{
    const float cx = x + width / 2.0f;
    const float cy = y + height / 2.0f;
    const auto phase = session_->phase();
    const bool failed = phase == LunaStreamSession::Phase::Failed;
    const bool ended = phase == LunaStreamSession::Phase::Ended;

    DrawCenteredText(vg, cx, cy - 70, 34, nvgRGB(245, 246, 248), title_);

    if (!failed && !ended)
    {
        // Petit indicateur d'activité : trois points qui pulsent.
        const double t = brls::getCPUTimeUsec() / 1000000.0;
        for (int i = 0; i < 3; ++i)
        {
            const float alpha = 0.35f + 0.65f * static_cast<float>(0.5 + 0.5 * std::sin(t * 4.0 - i * 0.7));
            nvgBeginPath(vg);
            nvgCircle(vg, cx - 24 + i * 24, cy - 10, 7);
            nvgFillColor(vg, nvgRGBAf(0.63f, 0.47f, 1.0f, alpha));
            nvgFill(vg);
        }
    }

    const NVGcolor status_color = failed ? nvgRGB(255, 120, 120) : nvgRGB(200, 204, 212);
    nvgFontSize(vg, 22);
    nvgFontFaceId(vg, brls::Application::getDefaultFont());
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, status_color);
    nvgTextBox(vg, x + width * 0.15f, cy + 30, width * 0.7f, session_->status_text().c_str(), nullptr);

    const std::string hint = Tr(failed || ended ? "B : retour à la bibliothèque"
                                : phase == LunaStreamSession::Phase::Connecting ? "ZL + ZR + Moins : quitter"
                                                                                 : "B : annuler");
    DrawCenteredText(vg, cx, y + height - 40, 18, nvgRGB(130, 134, 145), hint);
}

void StreamView::DrawDebugOverlay(NVGcontext* vg, float x, float y, float width)
{
    const std::string text = session_->debug_text();
    nvgBeginPath(vg);
    nvgRect(vg, x, y, width, 30);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 170));
    nvgFill(vg);
    nvgFontSize(vg, 16);
    nvgFontFaceId(vg, brls::Application::getDefaultFont());
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, nvgRGB(200, 255, 210));
    nvgText(vg, x + 12, y + 15, text.c_str(), nullptr);
}

} // namespace lunanx

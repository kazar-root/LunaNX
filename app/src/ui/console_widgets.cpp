// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=313f1a2e3931c53c1a8b
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).
#include "core/source_origin.hpp"
LUNANX_SOURCE_ORIGIN(u313f1a2e3931c53c1a8b, "313f1a2e3931c53c1a8b")

#include "console_widgets.hpp"

#include "core/i18n.hpp"
#include "cover_image_cache.hpp"

#include <borealis/core/touch/tap_gesture.hpp>

#if defined(__SWITCH__) && defined(BOREALIS_USE_DEKO3D)
#include <borealis/platforms/switch/switch_video.hpp>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace lunanx::ui
{

void DrawIcon(NVGcontext* vg, Icon icon, float x, float y, float size, NVGcolor color)
{
    nvgSave(vg);
    nvgTranslate(vg, x, y);
    nvgScale(vg, size / 24, size / 24);
    nvgStrokeWidth(vg, 1.9f);
    nvgLineCap(vg, NVG_ROUND);
    nvgLineJoin(vg, NVG_ROUND);
    nvgStrokeColor(vg, color);
    nvgFillColor(vg, color);
    nvgBeginPath(vg);
    switch (icon)
    {
        case Icon::Menu:
            for (float row : {6.0f, 12.0f, 18.0f})
            {
                nvgMoveTo(vg, 4, row);
                nvgLineTo(vg, 20, row);
            }
            break;
        case Icon::AllGames:
            nvgRoundedRect(vg, 3.5f, 3.5f, 7, 7, 1.8f);
            nvgRoundedRect(vg, 13.5f, 3.5f, 7, 7, 1.8f);
            nvgRoundedRect(vg, 3.5f, 13.5f, 7, 7, 1.8f);
            nvgRoundedRect(vg, 13.5f, 13.5f, 7, 7, 1.8f);
            break;
        case Icon::Recent:
            nvgCircle(vg, 12, 12, 8.5f);
            nvgMoveTo(vg, 12, 7);
            nvgLineTo(vg, 12, 12);
            nvgLineTo(vg, 15.5f, 14);
            break;
        case Icon::Top:
            // Coupe : vasque, anses, pied.
            nvgMoveTo(vg, 7, 4);
            nvgLineTo(vg, 17, 4);
            nvgLineTo(vg, 17, 9);
            nvgBezierTo(vg, 17, 13, 14.5f, 15, 12, 15);
            nvgBezierTo(vg, 9.5f, 15, 7, 13, 7, 9);
            nvgClosePath(vg);
            nvgMoveTo(vg, 7, 6);
            nvgBezierTo(vg, 3, 6, 3, 11, 7.5f, 11.5f);
            nvgMoveTo(vg, 17, 6);
            nvgBezierTo(vg, 21, 6, 21, 11, 16.5f, 11.5f);
            nvgMoveTo(vg, 12, 15);
            nvgLineTo(vg, 12, 19);
            nvgMoveTo(vg, 8, 20);
            nvgLineTo(vg, 16, 20);
            break;
        case Icon::Premium:
            // Couronne.
            nvgMoveTo(vg, 3.5f, 8);
            nvgLineTo(vg, 8, 12.5f);
            nvgLineTo(vg, 12, 5);
            nvgLineTo(vg, 16, 12.5f);
            nvgLineTo(vg, 20.5f, 8);
            nvgLineTo(vg, 18.5f, 18);
            nvgLineTo(vg, 5.5f, 18);
            nvgClosePath(vg);
            nvgMoveTo(vg, 6, 21);
            nvgLineTo(vg, 18, 21);
            break;
        case Icon::Search:
            nvgCircle(vg, 10, 10, 6.5f);
            nvgMoveTo(vg, 15, 15);
            nvgLineTo(vg, 21, 21);
            break;
        case Icon::Settings:
            nvgCircle(vg, 12, 12, 6.8f);
            for (int i = 0; i < 8; ++i)
            {
                const float a = i * 0.785398f;
                nvgMoveTo(vg, 12 + std::cos(a) * 7, 12 + std::sin(a) * 7);
                nvgLineTo(vg, 12 + std::cos(a) * 10, 12 + std::sin(a) * 10);
            }
            nvgCircle(vg, 12, 12, 2.6f);
            break;
        case Icon::Account:
            nvgCircle(vg, 12, 8, 4);
            nvgMoveTo(vg, 4, 21);
            nvgBezierTo(vg, 4, 11, 20, 11, 20, 21);
            break;
        case Icon::Lock:
            nvgRoundedRect(vg, 5, 11, 14, 10, 2);
            nvgMoveTo(vg, 8, 11);
            nvgLineTo(vg, 8, 8);
            nvgBezierTo(vg, 8, 3, 16, 3, 16, 8);
            nvgLineTo(vg, 16, 11);
            break;
        case Icon::Play:
            nvgMoveTo(vg, 7, 4);
            nvgLineTo(vg, 20, 12);
            nvgLineTo(vg, 7, 20);
            nvgClosePath(vg);
            nvgFill(vg);
            nvgRestore(vg);
            return;
    }
    nvgStroke(vg);
    nvgRestore(vg);
}

void DrawHalo(NVGcontext* vg, float x, float y, float w, float h, float r, float amount)
{
    if (amount <= 0)
        return;
    for (int i = 2; i >= 1; --i)
    {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x - i * 2, y - i * 2, w + i * 4, h + i * 4, r + i * 2);
        nvgStrokeWidth(vg, 3 + i * 2);
        nvgStrokeColor(vg, Opacity(Accent(), amount * (i == 1 ? 0.16f : 0.06f)));
        nvgStroke(vg);
    }
}

bool DrawImage(NVGcontext* vg, brls::Image& image, float x, float y, float w, float h, bool fill,
               float opacity, float radius)
{
    const int texture = image.getTexture();
    const float iw = image.getOriginalImageWidth();
    const float ih = image.getOriginalImageHeight();
    if (!texture || w <= 0 || h <= 0 || iw <= 0 || ih <= 0 || !std::isfinite(iw) || !std::isfinite(ih))
        return false;
    const float scale = fill ? std::max(w / iw, h / ih) : std::min(w / iw, h / ih);
    const float dx = x + (w - iw * scale) / 2;
    const float dy = y + (h - ih * scale) / 2;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, fill ? x : dx, fill ? y : dy, fill ? w : iw * scale, fill ? h : ih * scale, radius);
    nvgFillPaint(vg, nvgImagePattern(vg, dx, dy, iw * scale, ih * scale, 0, texture, opacity));
    nvgFill(vg);
    return true;
}

void RetireImage(std::unique_ptr<brls::Image> image)
{
    if (!image)
        return;
    brls::sync([retired = std::shared_ptr<brls::Image>(std::move(image))]() mutable {
#if defined(__SWITCH__) && defined(BOREALIS_USE_DEKO3D)
        if (auto* context = dynamic_cast<brls::SwitchVideoContext*>(
                brls::Application::getPlatform()->getVideoContext()))
            context->getQueue().waitIdle();
#endif
        retired.reset();
    });
}

ConsoleButton::ConsoleButton(Icon icon, std::string title, std::function<void()> action, bool primary)
    : brls::Box(brls::Axis::ROW)
    , icon_(icon)
    , title_(std::move(title))
    , primary_(primary)
{
    setWidth(primary ? 220 : 100);
    setHeight(primary ? 52 : 72);
    setShrink(0);
    setFocusable(true);
    setHideHighlight(true);
    setHideClickAnimation(true);
    registerClickAction([action = std::move(action)](brls::View*) {
        if (action)
            action();
        return true;
    });
    updateActionHint(brls::BUTTON_A, title_);
    addGestureRecognizer(new brls::TapGestureRecognizer(this));
}

ConsoleButton::~ConsoleButton()
{
    RetireImage(std::move(image_));
}

void ConsoleButton::SetImageUrl(const std::string& url)
{
    if (url == image_url_)
        return;
    image_url_ = url;
    RetireImage(std::move(image_));
    if (!url.empty())
    {
        image_ = std::make_unique<brls::Image>();
        SetCachedAvatarImage(image_.get(), url);
    }
}

void ConsoleButton::SetTitle(const std::string& title)
{
    title_ = title;
    updateActionHint(brls::BUTTON_A, title);
}

void ConsoleButton::onFocusGained()
{
    brls::Box::onFocusGained();
    focus_.reset();
    focus_.addStep(1, 160, brls::EasingFunction::cubicOut);
    focus_.start();
}

void ConsoleButton::onFocusLost()
{
    brls::Box::onFocusLost();
    focus_.reset();
    focus_.addStep(0, 160, brls::EasingFunction::cubicOut);
    focus_.start();
}

void ConsoleButton::draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style style,
                         brls::FrameContext* ctx)
{
    nvgSave(vg);
    nvgGlobalAlpha(vg, getAlpha());
    const float focus = focus_.getValue();
    // Le libellé fait partie de la zone tactile, sous le cercle.
    const float button_width = w;
    const float button_x = x;
    const float button_y = y;
    if (!primary_)
    {
        w = h = 46;
        x += (button_width - w) / 2;
    }
    const float growth = primary_ ? 0.03f * focus : 0;
    x -= w * growth / 2;
    y -= h * growth / 2;
    w *= 1 + growth;
    h *= 1 + growth;
    const float radius = h / 2;
    DrawHalo(vg, x, y, w, h, radius, std::max(focus, active_ ? 0.5f : 0.0f));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, radius);
    if (primary_)
        nvgFillPaint(vg, nvgLinearGradient(vg, x, y, x + w, y + h, Text(), nvgRGB(232, 226, 255)));
    else
        nvgFillColor(vg, Opacity(active_ ? RaisedFocus() : Raised(), 0.88f));
    nvgFill(vg);
    nvgStrokeWidth(vg, focus > 0.05f ? 2.5f : 1);
    nvgStrokeColor(vg, focus > 0.05f || active_ ? Opacity(Accent(), std::max(focus, 0.7f))
                                                : Opacity(Text(), 0.16f));
    nvgStroke(vg);

    const NVGcolor ink = primary_ ? Ground() : has_tint_ ? tint_ : Text();
    if (primary_)
    {
        const float text_width = TextWidth(title_, 20, FontRole::Display);
        const float start = x + (w - text_width - 30) / 2;
        DrawIcon(vg, icon_, start, y + (h - 18) / 2, 18, ink);
        nvgFontFaceId(vg, Font(FontRole::Display));
        nvgFontSize(vg, 20);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, ink);
        nvgText(vg, start + 30, y + h / 2 + 1, title_.c_str(), nullptr);
    }
    else
    {
        if (!image_ || !DrawImage(vg, *image_, x + 2, y + 2, w - 4, h - 4, true, 1, (h - 4) / 2))
            DrawIcon(vg, icon_, x + (w - 22) / 2, y + (h - 22) / 2, 22, ink);
        const float size = std::clamp(14.0f * (button_width - 4) /
                                         std::max(1.0f, TextWidth(title_, 14, FontRole::Heading)),
                                     10.0f, 14.0f);
        nvgFontFaceId(vg, Font(FontRole::Heading));
        nvgFontSize(vg, size);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, active_ || focus > 0.05f ? Accent() : Text());
        nvgText(vg, button_x + button_width / 2, button_y + 61, title_.c_str(), nullptr);
    }
    if (badge_)
    {
        nvgBeginPath(vg);
        nvgCircle(vg, x + w - 8, y + 8, 5);
        nvgFillColor(vg, Premium());
        nvgFill(vg);
    }
    nvgRestore(vg);
    brls::Box::draw(vg, x, y, w, h, style, ctx);
}

DynamicBackdrop::~DynamicBackdrop()
{
    RetireImage(std::move(current_));
    RetireImage(std::move(next_));
}

void DynamicBackdrop::Clear()
{
    blend_.stop();
    blend_ = 0;
    waiting_ = false;
    transitioning_ = false;
    requested_.clear();
    displayed_.clear();
    RetireImage(std::move(current_));
    RetireImage(std::move(next_));
}

void DynamicBackdrop::Select(const std::string& url)
{
    if (url == requested_)
        return;
    // Fondu à moitié fait : le nouveau visuel devient le visuel courant.
    if (transitioning_ && blend_.getValue() > 0.5f)
    {
        RetireImage(std::move(current_));
        current_ = std::move(next_);
        displayed_ = requested_;
    }
    blend_.stop();
    blend_ = 0;
    transitioning_ = false;
    requested_ = url;
    if (current_ && displayed_ == url)
    {
        RetireImage(std::move(next_));
        waiting_ = false;
        return;
    }
    RetireImage(std::move(next_));
    if (url.empty())
    {
        waiting_ = true;
        return;
    }
    next_ = std::make_unique<brls::Image>();
    SetCachedGalleryImage(next_.get(), url);
    waiting_ = true;
}

void DynamicBackdrop::Draw(NVGcontext* vg, float x, float y, float w, float h)
{
    nvgSave(vg);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillPaint(vg, nvgLinearGradient(vg, x, y, x + w, y + h, nvgLerpRGBA(Ground(), SecondaryAccent(), 0.18f),
                                       Ground()));
    nvgFill(vg);

    const bool next_ready = next_ && next_->getTexture() != 0;
    if (waiting_ && (requested_.empty() || next_ready))
    {
        blend_ = 0;
        blend_.addStep(1, 320, brls::EasingFunction::cubicOut);
        blend_.start();
        waiting_ = false;
        transitioning_ = true;
    }
    const float mix = transitioning_ ? blend_.getValue() : 0;
    if (current_)
        DrawImage(vg, *current_, x, y, w, h, true, requested_.empty() ? 1 - mix : 1);
    if (next_)
        DrawImage(vg, *next_, x, y, w, h, true, mix);

    // Voile sombre : couleurs d'origine gardées, texte lisible en bas et en haut.
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillColor(vg, nvgRGBA(6, 4, 16, 96));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillPaint(vg, nvgLinearGradient(vg, x, y + h * 0.30f, x, y + h, nvgRGBA(6, 4, 16, 0),
                                       nvgRGBA(6, 4, 16, 248)));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h * 0.26f);
    nvgFillPaint(vg, nvgLinearGradient(vg, x, y, x, y + h * 0.26f, nvgRGBA(6, 4, 16, 190), nvgRGBA(6, 4, 16, 0)));
    nvgFill(vg);

    if (transitioning_ && mix >= 0.999f)
    {
        RetireImage(std::move(current_));
        current_ = std::move(next_);
        displayed_ = requested_;
        transitioning_ = false;
    }
    nvgRestore(vg);
}

StatusPill::StatusPill()
{
    setWidth(330);
    setHeight(48);
    setShrink(0);
}

void StatusPill::SetPlan(const std::string& label, bool premium)
{
    plan_ = label;
    premium_ = premium;
    setWidth(plan_.empty() ? 250 : std::min(250 + TextWidth(plan_, 14, FontRole::Heading) + 26, 430.0f));
}

void StatusPill::draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style, brls::FrameContext*)
{
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now - sampled_ >= 1.0)
    {
        sampled_ = now;
        const std::time_t time = std::time(nullptr);
        std::tm local {};
        localtime_r(&time, &local);
        char value[8] {};
        std::strftime(value, sizeof(value), "%H:%M", &local);
        clock_ = value;
        auto* platform = brls::Application::getPlatform();
        battery_ = platform->canShowBatteryLevel() ? platform->getBatteryLevel() : -1;
        charging_ = battery_ >= 0 && platform->isBatteryCharging();
#ifdef __SWITCH__
        // « Communication enabled » indique une autorisation, pas le lien utilisé.
        NifmInternetConnectionType type {};
        NifmInternetConnectionStatus status {};
        u32 strength = 0;
        const bool connected = R_SUCCEEDED(nifmGetInternetConnectionStatus(&type, &strength, &status)) &&
                               status == NifmInternetConnectionStatus_Connected;
        ethernet_ = connected && type == NifmInternetConnectionType_Ethernet;
        wifi_ = connected && type == NifmInternetConnectionType_WiFi;
        wifi_level_ = wifi_ ? std::min(strength, 3u) : 0;
#else
        ethernet_ = platform->hasEthernetConnection();
        wifi_ = platform->hasWirelessConnection();
        wifi_level_ = wifi_ ? platform->getWirelessLevel() : 0;
#endif
    }

    nvgSave(vg);
    nvgGlobalAlpha(vg, getAlpha());
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, h / 2);
    nvgFillColor(vg, Opacity(Raised(), 0.86f));
    nvgFill(vg);
    nvgStrokeWidth(vg, 1);
    nvgStrokeColor(vg, Opacity(Text(), 0.16f));
    nvgStroke(vg);

    nvgFontFaceId(vg, Font(FontRole::Medium));
    nvgFontSize(vg, 15);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, Text());
    nvgText(vg, x + 18, y + h / 2, clock_.c_str(), nullptr);

    float cursor = x + 78;
    if (battery_ >= 0 && battery_ <= 100)
    {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, cursor, y + h / 2 - 6, 22, 12, 2);
        nvgStrokeWidth(vg, 1.2f);
        nvgStrokeColor(vg, Text());
        nvgStroke(vg);
        nvgBeginPath(vg);
        nvgRect(vg, cursor + 23, y + h / 2 - 3, 2, 6);
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgRect(vg, cursor + 3, y + h / 2 - 3, 16.0f * battery_ / 100, 6);
        nvgFillColor(vg, battery_ <= 15 && !charging_ ? Danger() : Text());
        nvgFill(vg);
        nvgFillColor(vg, Text());
        const std::string text = std::to_string(battery_) + "%" + (charging_ ? "+" : "");
        nvgText(vg, cursor + 31, y + h / 2, text.c_str(), nullptr);
    }
    cursor = x + 168;
    const bool connected = ethernet_ || wifi_;
    for (int bar = 0; bar < 3; ++bar)
    {
        const bool lit = ethernet_ || (wifi_ && wifi_level_ >= bar + 1);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, cursor + bar * 5, y + h / 2 + 5 - (4 + bar * 4), 3, 4 + bar * 4, 1);
        nvgFillColor(vg, Opacity(Text(), lit ? 1.0f : 0.25f));
        nvgFill(vg);
    }
    const std::string network = !connected ? Tr("Hors ligne") : ethernet_ ? "LAN" : "Wi-Fi";
    nvgFillColor(vg, connected ? Text() : Danger());
    nvgText(vg, cursor + 22, y + h / 2, network.c_str(), nullptr);

    if (!plan_.empty())
    {
        const float separator = x + 250;
        nvgBeginPath(vg);
        nvgMoveTo(vg, separator - 10, y + 13);
        nvgLineTo(vg, separator - 10, y + h - 13);
        nvgStrokeColor(vg, Opacity(Text(), 0.2f));
        nvgStrokeWidth(vg, 1);
        nvgStroke(vg);
        nvgScissor(vg, separator, y + 6, w - (separator - x) - 14, h - 12);
        if (premium_)
            DrawIcon(vg, Icon::Premium, separator, y + h / 2 - 8, 16, Premium());
        nvgFontFaceId(vg, Font(FontRole::Heading));
        nvgFontSize(vg, 14);
        nvgFillColor(vg, premium_ ? Premium() : Accent());
        nvgText(vg, separator + (premium_ ? 22 : 0), y + h / 2, plan_.c_str(), nullptr);
    }
    nvgRestore(vg);
}

} // namespace lunanx::ui

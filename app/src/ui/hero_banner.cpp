#include "hero_banner.hpp"

#include "core/i18n.hpp"
#include "cover_image_cache.hpp"

#include <utility>

namespace lunanx
{

HeroBanner::HeroBanner(const luna::Game& game, std::function<void()> on_click)
    : brls::Box(brls::Axis::COLUMN)
    , title_(game.title)
{
    setWidthPercentage(100);
    setHeight(300);
    setMarginBottom(26);
    setCornerRadius(18);
    setBorderThickness(3);
    setBorderColor(nvgRGBA(255, 255, 255, 18));
    setBackgroundColor(nvgRGB(28, 24, 44));
    setFocusable(true);
    setHideHighlight(true);

    auto* image = new brls::Image();
    image->setPositionType(brls::PositionType::ABSOLUTE);
    image->setPositionTop(0);
    image->setPositionLeft(0);
    image->setWidthPercentage(100);
    image->setHeight(300);
    image->setCornerRadius(18);
    image->setScalingType(brls::ImageScalingType::FILL);
    SetCachedGalleryImage(image, !game.image_landscape.empty() ? game.image_landscape : game.image_portrait);
    addView(image);

    registerClickAction([on_click = std::move(on_click)](brls::View*) {
        if (on_click)
            on_click();
        return true;
    });
    addGestureRecognizer(new brls::TapGestureRecognizer(this));
}

void HeroBanner::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                      brls::FrameContext* ctx)
{
    brls::Box::draw(vg, x, y, width, height, style, ctx);

    // Dégradé sombre à gauche pour que le texte reste lisible sur n'importe quel visuel.
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, 18);
    nvgFillPaint(vg, nvgLinearGradient(vg, x, y, x + width * 0.62f, y,
                                       nvgRGBA(10, 8, 20, 235), nvgRGBA(10, 8, 20, 0)));
    nvgFill(vg);

    nvgFontFaceId(vg, brls::Application::getFont(brls::FONT_REGULAR));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

    nvgFontSize(vg, 18);
    nvgFillColor(vg, nvgRGB(176, 150, 255));
    nvgText(vg, x + 36, y + 70, Tr("À LA UNE SUR LUNA").c_str(), nullptr);

    nvgFontSize(vg, 40);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgTextBox(vg, x + 36, y + 100, width * 0.5f, title_.c_str(), nullptr);

    // Bouton « Jouer » dessiné : plein quand la bannière a le focus.
    const float button_x = x + 36;
    const float button_y = y + height - 82;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, button_x, button_y, 210, 50, 25);
    nvgFillColor(vg, focused_ ? nvgRGB(124, 92, 255) : nvgRGBA(255, 255, 255, 36));
    nvgFill(vg);
    nvgFontSize(vg, 22);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, button_x + 105, button_y + 25, ("\uE0E0  " + Tr("Jouer")).c_str(), nullptr);
}

void HeroBanner::onFocusGained()
{
    brls::Box::onFocusGained();
    focused_ = true;
    setBorderColor(nvgRGB(176, 150, 255));
}

void HeroBanner::onFocusLost()
{
    brls::Box::onFocusLost();
    focused_ = false;
    setBorderColor(nvgRGBA(255, 255, 255, 18));
}

} // namespace lunanx

#include "poster_card.hpp"

#include "cover_image_cache.hpp"

#include <utility>

namespace lunanx
{

PosterCard::PosterCard(const luna::Game& game, float width, int rank, std::function<void()> on_click)
    : brls::Box(brls::Axis::COLUMN)
{
    const float image_height = width * 4.0f / 3.0f;
    setWidth(width);
    setMarginRight(16);
    setMarginBottom(14);
    setFocusable(true);
    setHideHighlight(true);

    // Cadre : la bordure violette signale le focus sans redimensionner la jaquette.
    frame_ = new brls::Box(brls::Axis::COLUMN);
    frame_->setWidth(width);
    frame_->setHeight(image_height);
    frame_->setCornerRadius(12);
    frame_->setBorderThickness(3);
    frame_->setBackgroundColor(nvgRGB(28, 24, 44));

    auto* image = new brls::Image();
    image->setWidth(width);
    image->setHeight(image_height);
    image->setCornerRadius(12);
    image->setScalingType(brls::ImageScalingType::FILL);
    SetCachedPosterImage(image, !game.image_portrait.empty() ? game.image_portrait : game.image_landscape);
    frame_->addView(image);

    if (rank > 0)
    {
        auto* badge = new brls::Box(brls::Axis::ROW);
        badge->setPositionType(brls::PositionType::ABSOLUTE);
        badge->setPositionTop(8);
        badge->setPositionLeft(8);
        badge->setPadding(2, 10, 2, 10);
        badge->setCornerRadius(10);
        badge->setBackgroundColor(nvgRGBA(124, 92, 255, 235));
        auto* number = new brls::Label();
        number->setText("#" + std::to_string(rank));
        number->setFontSize(rank < 10 ? 22 : 20);
        number->setTextColor(nvgRGB(255, 255, 255));
        badge->addView(number);
        frame_->addView(badge);
    }
    addView(frame_);

    title_ = new brls::Label();
    title_->setText(game.title);
    title_->setFontSize(15);
    title_->setSingleLine(true);
    title_->setWidth(width);
    title_->setMarginTop(8);
    addView(title_);

    UpdateChrome(false);
    registerClickAction([on_click = std::move(on_click)](brls::View*) {
        if (on_click)
            on_click();
        return true;
    });
    addGestureRecognizer(new brls::TapGestureRecognizer(this));
}

void PosterCard::onFocusGained()
{
    brls::Box::onFocusGained();
    UpdateChrome(true);
}

void PosterCard::onFocusLost()
{
    brls::Box::onFocusLost();
    UpdateChrome(false);
}

void PosterCard::UpdateChrome(bool focused)
{
    frame_->setBorderColor(focused ? nvgRGB(176, 150, 255) : nvgRGBA(255, 255, 255, 18));
    title_->setTextColor(focused ? nvgRGB(255, 255, 255) : nvgRGB(170, 172, 184));
}

} // namespace lunanx

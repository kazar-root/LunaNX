// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=0d5c0c4c651c5963ec9b
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).
#include "core/source_origin.hpp"
LUNANX_SOURCE_ORIGIN(u0d5c0c4c651c5963ec9b, "0d5c0c4c651c5963ec9b")

#include "game_carousel.hpp"

#include "core/i18n.hpp"
#include "cover_image_cache.hpp"

#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/core/touch/pan_gesture.hpp>

#include <algorithm>
#include <cmath>

namespace lunanx
{
namespace
{

// Pas entre deux jaquettes, position de la jaquette sélectionnée, taille d'une jaquette 3:4.
constexpr float kStep = 214;
constexpr float kAnchor = 168;
constexpr float kWidth = 190;
constexpr float kHeight = 253;
constexpr float kRadius = 16;

void DrawBadge(NVGcontext* vg, float x, float y, const std::string& text, NVGcolor background, NVGcolor ink,
               ui::Icon icon, bool with_icon)
{
    const float size = 14;
    const float width = ui::TextWidth(text, size, ui::FontRole::Display) + (with_icon ? 34 : 18);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, 26, 13);
    nvgFillColor(vg, background);
    nvgFill(vg);
    float cursor = x + 9;
    if (with_icon)
    {
        ui::DrawIcon(vg, icon, cursor, y + 5, 16, ink);
        cursor += 18;
    }
    nvgFontFaceId(vg, ui::Font(ui::FontRole::Display));
    nvgFontSize(vg, size);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, ink);
    nvgText(vg, cursor, y + 14, text.c_str(), nullptr);
}

} // namespace

GameCarouselView::GameCarouselView(std::function<void(const std::string&)> select, std::function<void()> activate,
                                   std::function<void()> next_page, std::function<void()> previous_page)
    : select_(std::move(select))
    , activate_(std::move(activate))
    , next_page_(std::move(next_page))
    , previous_page_(std::move(previous_page))
{
    setHeight(300);
    setShrink(0);
    setFocusable(true);
    setHideHighlight(true);
    setHideClickAnimation(true);
    registerAction({}, brls::BUTTON_NAV_LEFT, [this](brls::View*) {
        MoveSelection(-1);
        return true;
    }, true, true);
    registerAction({}, brls::BUTTON_NAV_RIGHT, [this](brls::View*) {
        MoveSelection(1);
        return true;
    }, true, true);
    registerClickAction([this](brls::View*) {
        ActivateSelected();
        return true;
    });
    UpdateHint();
    // Le glissement prend la main sur le tap dès qu'un mouvement horizontal est reconnu.
    addGestureRecognizer(new brls::PanGestureRecognizer([this](brls::PanGestureStatus status, brls::Sound*) {
        if (SlotCount() == 0)
            return;
        if (status.state == brls::GestureState::START)
        {
            brls::Application::giveFocus(this);
            drag_origin_ = position_.getValue();
            position_.stop();
        }
        if (status.state != brls::GestureState::STAY && status.state != brls::GestureState::END)
            return;
        const float offset = has_previous_page_ ? 1.0f : 0.0f;
        const float position = std::clamp(drag_origin_ - (status.position.x - status.startPosition.x) / kStep,
                                          -offset, static_cast<float>(SlotCount() - 1) - offset);
        Select(static_cast<std::size_t>(std::llround(position + offset)), false);
        position_ = position;
        if (status.state == brls::GestureState::END)
        {
            position_.addStep(static_cast<float>(selected_) - offset, 180, brls::EasingFunction::cubicOut);
            position_.start();
            if (IsNextPageSelected() || IsPreviousPageSelected())
                ActivateSelected();
        }
    }, brls::PanAxis::HORIZONTAL));
    addGestureRecognizer(new brls::TapGestureRecognizer([this](brls::TapGestureStatus status, brls::Sound*) {
        if (status.state != brls::GestureState::END || SlotCount() == 0)
            return;
        const float relative = (status.position.x - getFrame().getMinX() - kAnchor) / kStep;
        const long long target = std::llround(position_.getValue() + relative) + (has_previous_page_ ? 1 : 0);
        if (target < 0 || target >= static_cast<long long>(SlotCount()))
            return;
        const float center = getFrame().getMinX() + kAnchor +
                             (target - (has_previous_page_ ? 1 : 0) - position_.getValue()) * kStep;
        const float scale = static_cast<std::size_t>(target) == selected_ ? 1.07f : 1.0f;
        const float top = getFrame().getMinY() + (getHeight() - kHeight * scale) / 2;
        if (std::abs(status.position.x - center) > kWidth * scale / 2 ||
            status.position.y < top || status.position.y > top + kHeight * scale)
            return;
        brls::Application::giveFocus(this);
        Select(static_cast<std::size_t>(target), true);
        if (IsNextPageSelected() || IsPreviousPageSelected())
            ActivateSelected();
        // Une jaquette ne lance jamais de partie au toucher : utiliser le bouton JOUER.
    }));
}

GameCarouselView::~GameCarouselView()
{
    for (auto& slot : slots_)
        ui::RetireImage(std::move(slot.image));
}

std::size_t GameCarouselView::SelectedItemIndex() const
{
    if (items_.empty() || IsPreviousPageSelected())
        return 0;
    return std::min(selected_ - (has_previous_page_ ? 1 : 0), items_.size() - 1);
}

std::string GameCarouselView::SelectedId() const
{
    if (items_.empty() || IsPreviousPageSelected() || IsNextPageSelected())
        return {};
    return items_[SelectedItemIndex()].id;
}

void GameCarouselView::UpdateHint()
{
    if (IsPreviousPageSelected())
        updateActionHint(brls::BUTTON_A, Tr("Page précédente"));
    else if (IsNextPageSelected())
        updateActionHint(brls::BUTTON_A, Tr("Page suivante"));
    else
        updateActionHint(brls::BUTTON_A, Tr("Jouer"));
}

void GameCarouselView::ActivateSelected()
{
    if (IsPreviousPageSelected())
    {
        if (previous_page_)
            previous_page_();
    }
    else if (IsNextPageSelected())
    {
        if (next_page_)
            next_page_();
    }
    else if (!items_.empty() && activate_)
    {
        activate_();
    }
}

void GameCarouselView::SetGames(std::vector<CarouselItem> games, const std::string& selected_id, bool has_next_page,
                                bool has_previous_page)
{
    if (games.size() > kCarouselPageSize)
        games.resize(kCarouselPageSize);
    items_ = std::move(games);
    has_next_page_ = has_next_page && !items_.empty() && static_cast<bool>(next_page_);
    has_previous_page_ = has_previous_page && !items_.empty() && static_cast<bool>(previous_page_);
    if (items_.empty())
    {
        // État vide complet : aucune animation, texture ou sélection de l'ancienne page.
        position_ = 0;
        selection_mix_ = 1;
        selected_ = previous_selected_ = 0;
        for (auto& slot : slots_)
            ui::RetireImage(std::move(slot.image));
        slots_.clear();
        setFocusable(false);
        UpdateHint();
        return;
    }
    selected_ = has_previous_page_ ? 1 : 0;
    for (std::size_t i = 0; i < items_.size(); ++i)
        if (!selected_id.empty() && items_[i].id == selected_id)
            selected_ = i + (has_previous_page_ ? 1 : 0);
    previous_selected_ = selected_;
    selection_mix_ = 1;
    position_ = static_cast<float>(selected_) - (has_previous_page_ ? 1.0f : 0.0f);
    setFocusable(!items_.empty());
    ReconcileTextures();
    UpdateHint();
    if (!items_.empty() && select_)
        select_(items_[SelectedItemIndex()].id);
}

void GameCarouselView::MoveSelection(int delta)
{
    if (SlotCount() == 0)
        return;
    const long long target = std::clamp(static_cast<long long>(selected_) + delta, 0LL,
                                        static_cast<long long>(SlotCount()) - 1);
    Select(static_cast<std::size_t>(target), true);
}

void GameCarouselView::Select(std::size_t index, bool animate)
{
    if (index >= SlotCount() || index == selected_)
        return;
    previous_selected_ = selected_;
    selected_ = index;
    selection_mix_ = 0;
    selection_mix_.addStep(1, 180, brls::EasingFunction::cubicOut);
    selection_mix_.start();
    const float target = static_cast<float>(index) - (has_previous_page_ ? 1.0f : 0.0f);
    if (animate)
    {
        position_.reset();
        position_.addStep(target, 220, brls::EasingFunction::cubicOut);
        position_.start();
    }
    else
    {
        position_ = target;
    }
    UpdateHint();
    if (!IsNextPageSelected() && !IsPreviousPageSelected() && select_)
        select_(items_[SelectedItemIndex()].id);
}

void GameCarouselView::ReconcileTextures()
{
    // Jaquettes qui ne sont plus sur la page : libérées. Les autres sont gardées telles quelles.
    for (auto it = slots_.begin(); it != slots_.end();)
    {
        const bool kept = std::any_of(items_.begin(), items_.end(), [&](const CarouselItem& item) {
            return item.id == it->id && item.image_url == it->url;
        });
        if (kept)
        {
            ++it;
            continue;
        }
        ui::RetireImage(std::move(it->image));
        it = slots_.erase(it);
    }
    // Nouvelles jaquettes, en commençant par la sélection puis ses voisines.
    std::vector<std::size_t> order;
    const std::size_t center = SelectedItemIndex();
    if (!items_.empty())
        order.push_back(center);
    for (std::size_t distance = 1; distance < items_.size(); ++distance)
    {
        if (center + distance < items_.size())
            order.push_back(center + distance);
        if (center >= distance)
            order.push_back(center - distance);
    }
    for (const std::size_t index : order)
    {
        const auto& item = items_[index];
        if (std::any_of(slots_.begin(), slots_.end(), [&](const Slot& slot) { return slot.id == item.id; }))
            continue;
        auto image = std::make_unique<brls::Image>();
        SetCachedPosterImage(image.get(), item.image_url);
        slots_.push_back({item.id, item.image_url, std::move(image)});
    }
}

void GameCarouselView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style,
                            brls::FrameContext*)
{
    if (items_.empty())
        return;
    nvgSave(vg);
    nvgGlobalAlpha(vg, getAlpha());
    nvgIntersectScissor(vg, x - 40, y, width + 80, height);
    const float position = position_.getValue();
    const float mix = selection_mix_.getValue();
    const int first = has_previous_page_ ? -1 : 0;
    const int last = static_cast<int>(items_.size()) + (has_next_page_ ? 1 : 0);
    for (int i = first; i < last; ++i)
    {
        const std::size_t slot_index = static_cast<std::size_t>(i + (has_previous_page_ ? 1 : 0));
        const float emphasis = slot_index == selected_ ? mix : slot_index == previous_selected_ ? 1 - mix : 0;
        const float scale = 1 + 0.07f * emphasis;
        const float w = kWidth * scale;
        const float h = kHeight * scale;
        const float left = x + kAnchor + (static_cast<float>(i) - position) * kStep - w / 2;
        const float top = y + (height - h) / 2;
        if (left + w < x - 40 || left > x + width + 40)
            continue;
        const bool previous_card = i < 0;
        const bool page_card = previous_card || i == static_cast<int>(items_.size());

        ui::DrawHalo(vg, left, top, w, h, kRadius, emphasis);
        // Ombre portée puis fond de carte.
        nvgBeginPath(vg);
        nvgRoundedRect(vg, left + 2, top + 10, w, h, kRadius);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 90));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, left, top, w, h, kRadius);
        nvgFillColor(vg, ui::Raised());
        nvgFill(vg);

        if (page_card)
        {
            const float cx = left + w / 2;
            const float cy = top + h * 0.42f;
            const float direction = previous_card ? -1.0f : 1.0f;
            nvgBeginPath(vg);
            nvgMoveTo(vg, cx - 26 * direction, cy);
            nvgLineTo(vg, cx + 26 * direction, cy);
            nvgMoveTo(vg, cx + 8 * direction, cy - 18);
            nvgLineTo(vg, cx + 26 * direction, cy);
            nvgLineTo(vg, cx + 8 * direction, cy + 18);
            nvgStrokeWidth(vg, 4);
            nvgLineCap(vg, NVG_ROUND);
            nvgLineJoin(vg, NVG_ROUND);
            nvgStrokeColor(vg, ui::Accent());
            nvgStroke(vg);
            const std::string caption = Tr(previous_card ? "Page précédente" : "Page suivante");
            nvgFontFaceId(vg, ui::Font(ui::FontRole::Heading));
            nvgFontSize(vg, ui::TextWidth(caption, 18, ui::FontRole::Heading) > w - 20 ? 14 : 18);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, ui::Text());
            nvgText(vg, cx, top + h * 0.68f, caption.c_str(), nullptr);
            nvgFontFaceId(vg, ui::Font(ui::FontRole::Body));
            nvgFontSize(vg, 15);
            nvgFillColor(vg, ui::Muted());
            nvgText(vg, cx, top + h * 0.80f, Tr("Plus de jeux").c_str(), nullptr);
        }
        else
        {
            const CarouselItem& item = items_[static_cast<std::size_t>(i)];
            const auto found = std::find_if(slots_.begin(), slots_.end(),
                                            [&](const Slot& slot) { return slot.id == item.id; });
            const bool drawn = found != slots_.end() && found->image &&
                               ui::DrawImage(vg, *found->image, left, top, w, h, false, 1, kRadius - 1);
            if (!drawn)
            {
                // Jaquette en cours de chargement : titre centré sur la carte.
                nvgFontFaceId(vg, ui::Font(ui::FontRole::Heading));
                nvgFontSize(vg, 17);
                nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgFillColor(vg, ui::Muted());
                nvgTextBox(vg, left + 14, top + h / 2 - 20, w - 28, item.title.c_str(), nullptr);
            }
            if (item.locked)
            {
                nvgBeginPath(vg);
                nvgRoundedRect(vg, left, top, w, h, kRadius);
                nvgFillColor(vg, nvgRGBA(6, 4, 16, 120));
                nvgFill(vg);
                nvgBeginPath(vg);
                nvgCircle(vg, left + w / 2, top + h / 2, 26);
                nvgFillColor(vg, nvgRGBA(6, 4, 16, 200));
                nvgFill(vg);
                ui::DrawIcon(vg, ui::Icon::Lock, left + w / 2 - 13, top + h / 2 - 14, 26, ui::Premium());
            }
            if (item.rank > 0)
                DrawBadge(vg, left + 10, top + 10, "#" + std::to_string(item.rank), ui::Opacity(ui::SecondaryAccent(), 0.95f),
                          ui::Text(), ui::Icon::Top, false);
            if (item.premium)
                DrawBadge(vg, left + 10, top + h - 36, "PREMIUM", ui::Opacity(ui::Premium(), 0.96f), ui::Ground(),
                          ui::Icon::Premium, true);
        }

        nvgBeginPath(vg);
        nvgRoundedRect(vg, left - 1.5f, top - 1.5f, w + 3, h + 3, kRadius + 1);
        nvgStrokeWidth(vg, 1 + 1.5f * emphasis);
        nvgStrokeColor(vg, emphasis > 0.01f ? ui::Opacity(ui::Accent(), emphasis) : ui::Opacity(ui::Text(), 0.12f));
        nvgStroke(vg);
    }
    nvgRestore(vg);
}

} // namespace lunanx

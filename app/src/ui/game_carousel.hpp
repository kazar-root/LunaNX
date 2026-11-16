// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=cd2f915b7eaa003e2726
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include "console_widgets.hpp"

#include <borealis.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lunanx
{

// Une page affiche au plus 10 jaquettes : la mémoire des textures reste bornée quel que
// soit le nombre de jeux du catalogue.
inline constexpr std::size_t kCarouselPageSize = 10;

struct CarouselItem
{
    std::string id;
    std::string title;
    std::string image_url;
    int rank = 0;            // classement Top 10 (0 : aucun)
    bool premium = false;    // jeu Luna Premium
    bool locked = false;     // non inclus dans l'abonnement du compte
};

// Rangée de jaquettes défilant horizontalement ; la jaquette sélectionnée est agrandie et
// entourée d'un halo. Des cartes « page précédente / suivante » encadrent la page.
class GameCarouselView final : public brls::View
{
  public:
    GameCarouselView(std::function<void(const std::string&)> select, std::function<void()> activate,
                     std::function<void()> next_page, std::function<void()> previous_page);
    ~GameCarouselView() override;

    // Remplace la page (au plus kCarouselPageSize jeux) ; garde la sélection si possible.
    void SetGames(std::vector<CarouselItem> games, const std::string& selected_id, bool has_next_page,
                  bool has_previous_page);
    void MoveSelection(int delta);
    std::string SelectedId() const;
    std::size_t ItemCount() const { return items_.size(); }
    void draw(NVGcontext*, float, float, float, float, brls::Style, brls::FrameContext*) override;

  private:
    struct Slot
    {
        std::string id;
        std::string url;
        std::unique_ptr<brls::Image> image;
    };

    bool IsPreviousPageSelected() const { return has_previous_page_ && selected_ == 0; }
    bool IsNextPageSelected() const
    {
        return has_next_page_ && selected_ == items_.size() + (has_previous_page_ ? 1 : 0);
    }
    std::size_t SlotCount() const { return items_.size() + has_next_page_ + has_previous_page_; }
    std::size_t SelectedItemIndex() const;
    void Select(std::size_t index, bool animate);
    void ActivateSelected();
    void ReconcileTextures();
    void UpdateHint();

    std::vector<CarouselItem> items_;
    std::vector<Slot> slots_;
    std::function<void(const std::string&)> select_;
    std::function<void()> activate_;
    std::function<void()> next_page_;
    std::function<void()> previous_page_;
    bool has_next_page_ = false;
    bool has_previous_page_ = false;
    std::size_t selected_ = 0;
    std::size_t previous_selected_ = 0;
    brls::Animatable position_ = 0;
    brls::Animatable selection_mix_ = 1;
    float drag_origin_ = 0;
};

} // namespace lunanx

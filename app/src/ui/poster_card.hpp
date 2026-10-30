#pragma once

#include "luna/luna_models.hpp"

#include <borealis.hpp>

#include <functional>

namespace lunanx
{

// Jaquette verticale d'un jeu : visuel arrondi, titre, badge de classement facultatif.
// L'image n'est téléchargée qu'à la création de la carte (10 cartes à la fois au plus).
class PosterCard : public brls::Box
{
  public:
    PosterCard(const luna::Game& game, float width, int rank, std::function<void()> on_click);

    void onFocusGained() override;
    void onFocusLost() override;

  private:
    void UpdateChrome(bool focused);

    brls::Box* frame_ = nullptr;
    brls::Label* title_ = nullptr;
};

} // namespace lunanx

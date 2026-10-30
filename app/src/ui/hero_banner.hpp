#pragma once

#include "luna/luna_models.hpp"

#include <borealis.hpp>

#include <functional>
#include <string>

namespace lunanx
{

// Grande bannière « À la une » : visuel paysage, dégradé, titre et invitation à jouer.
class HeroBanner : public brls::Box
{
  public:
    HeroBanner(const luna::Game& game, std::function<void()> on_click);

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;
    void onFocusGained() override;
    void onFocusLost() override;

  private:
    std::string title_;
    bool focused_ = false;
};

} // namespace lunanx

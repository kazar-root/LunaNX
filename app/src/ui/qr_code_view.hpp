#pragma once

#include <borealis.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace lunanx
{

// QR code carré (fond blanc, marge de 2 modules) dessiné avec NanoVG.
class QrCodeView : public brls::Box
{
  public:
    QrCodeView(const std::string& text, float size);

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    int modules_ = 0;
    std::vector<uint8_t> dark_;
};

} // namespace lunanx

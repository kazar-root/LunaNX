// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=4a46895b4c31f1ac910e
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

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

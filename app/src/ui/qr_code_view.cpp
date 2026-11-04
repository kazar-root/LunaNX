#include "qr_code_view.hpp"

#include <qrcodegen.h>

#include <cmath>

namespace lunanx
{

QrCodeView::QrCodeView(const std::string& text, float size)
{
    setWidth(size);
    setHeight(size);
    setFocusable(false);

    std::vector<uint8_t> qr(qrcodegen_BUFFER_LEN_MAX);
    std::vector<uint8_t> temp(qrcodegen_BUFFER_LEN_MAX);
    if (!qrcodegen_encodeText(text.c_str(), temp.data(), qr.data(), qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN,
                              qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true))
        return;
    modules_ = qrcodegen_getSize(qr.data());
    dark_.resize(static_cast<size_t>(modules_ * modules_));
    for (int y = 0; y < modules_; ++y)
        for (int x = 0; x < modules_; ++x)
            dark_[static_cast<size_t>(y * modules_ + x)] = qrcodegen_getModule(qr.data(), x, y) ? 1 : 0;
}

void QrCodeView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                      brls::FrameContext* ctx)
{
    brls::Box::draw(vg, x, y, width, height, style, ctx);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, 10);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgFill(vg);
    if (modules_ == 0)
        return;

    // Modules alignés sur des pixels entiers pour rester nets et faciles à scanner.
    const int total = modules_ + 4;
    const float cell = std::floor(std::fmin(width, height) / static_cast<float>(total));
    const float origin_x = std::floor(x + (width - cell * static_cast<float>(modules_)) / 2.0f);
    const float origin_y = std::floor(y + (height - cell * static_cast<float>(modules_)) / 2.0f);

    nvgBeginPath(vg);
    for (int row = 0; row < modules_; ++row)
        for (int col = 0; col < modules_; ++col)
            if (dark_[static_cast<size_t>(row * modules_ + col)])
                nvgRect(vg, origin_x + static_cast<float>(col) * cell, origin_y + static_cast<float>(row) * cell,
                        cell, cell);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFill(vg);
}

} // namespace lunanx

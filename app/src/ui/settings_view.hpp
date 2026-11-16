// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=16c22767c1d10371cab6
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include "core/stream_settings.hpp"

#include <borealis.hpp>

#include <memory>
#include <functional>

namespace lunanx
{

// Réglages du flux : qualité, région, son, décodeur, disposition de la manette.
class SettingsView : public brls::Box
{
  public:
    explicit SettingsView(std::function<void()> on_signed_out = {});
    ~SettingsView() override;

  private:
    void Save();

    StreamSettings settings_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    bool checking_update_ = false;
};

} // namespace lunanx

#pragma once

#include "core/stream_settings.hpp"

#include <borealis.hpp>

#include <memory>

namespace lunanx
{

// Réglages du flux : qualité, région, son, décodeur, disposition de la manette.
class SettingsView : public brls::Box
{
  public:
    SettingsView();
    ~SettingsView() override;

  private:
    void Save();

    StreamSettings settings_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    bool checking_update_ = false;
};

} // namespace lunanx

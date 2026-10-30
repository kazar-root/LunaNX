#pragma once

#include "core/stream_settings.hpp"

#include <borealis.hpp>

namespace lunanx
{

// Réglages du flux : qualité, région, son, décodeur, disposition de la manette.
class SettingsView : public brls::Box
{
  public:
    SettingsView();

  private:
    void Save();

    StreamSettings settings_;
};

} // namespace lunanx

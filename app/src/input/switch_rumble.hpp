#pragma once

#include <cstdint>

namespace lunanx::input
{

// Vibration HD des manettes Switch (mode portable, Joy-Con, Pro Controller).
// Reçoit les deux moteurs XInput du jeu : gauche = basse fréquence (lourd),
// droit = haute fréquence (léger), chacun de 0 à 255.
class SwitchRumble
{
  public:
    void Send(std::uint8_t heavy, std::uint8_t light);
    void Stop() { Send(0, 0); }
};

} // namespace lunanx::input

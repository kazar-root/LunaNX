// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=9668b99e15dd9605b66c
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

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

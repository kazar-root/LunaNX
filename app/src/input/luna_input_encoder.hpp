#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lunanx::input
{

// Masque de boutons XInput (manette Xbox 360 / One) : c'est exactement le champ « boutons »
// des messages manette de Luna (capture navigateur : A = 0x1000, X = 0x4000…).
enum XInputButton : std::uint16_t
{
    kDpadUp = 0x0001,
    kDpadDown = 0x0002,
    kDpadLeft = 0x0004,
    kDpadRight = 0x0008,
    kStart = 0x0010,
    kBack = 0x0020,
    kLeftThumb = 0x0040,
    kRightThumb = 0x0080,
    kLeftShoulder = 0x0100,
    kRightShoulder = 0x0200,
    kGuide = 0x0400,
    kA = 0x1000,
    kB = 0x2000,
    kX = 0x4000,
    kY = 0x8000,
};

struct GamepadState
{
    std::uint16_t buttons = 0;
    std::uint8_t left_trigger = 0;
    std::uint8_t right_trigger = 0;
    std::int16_t left_x = 0;
    std::int16_t left_y = 0;   // positif = haut (convention XInput)
    std::int16_t right_x = 0;
    std::int16_t right_y = 0;

    bool operator==(const GamepadState& other) const
    {
        return buttons == other.buttons && left_trigger == other.left_trigger &&
               right_trigger == other.right_trigger && left_x == other.left_x &&
               left_y == other.left_y && right_x == other.right_x && right_y == other.right_y;
    }
    bool operator!=(const GamepadState& other) const { return !(*this == other); }
};

// Messages du data channel « inputChannel » (protobuf, relevé sur le client web) :
//   champ 104 = état manette { 2: séquence, 3: boutons XInput, 4/5: gâchettes,
//                              6/7: stick gauche X/Y, 8/9: stick droit X/Y,
//                              12: { 1: horodatage ms, 3: 4, 8: horodatage ms } }
//   Les axes vont de 0 à 65534 (32767 au repos), Y croissant vers le haut comme XInput.
//   champ 36 = message vide envoyé une fois à l'ouverture du canal.
// À confirmer : échelle des gâchettes (non capturées).
class LunaInputEncoder
{
  public:
    std::vector<std::uint8_t> EncodeGamepad(const GamepadState& state);
    static std::vector<std::uint8_t> EncodeInputReady();

    // Canaux créés par le client, dans l'ordre des identifiants SCTP 1 et 3.
    static const std::string& InputChannelLabel();
    static const std::string& ClientMessageChannelLabel();
    // Valeurs « requestedChannels » de la demande de session.
    static const std::vector<std::string>& RequestedChannels();

  private:
    std::uint32_t sequence_ = 0;
};

// Conversion d'un axe Borealis [-1, 1] vers l'échelle XInput.
std::int16_t AxisToXInput(float value);

// Horloge murale en millisecondes (horodatage des messages Luna).
std::uint64_t WallClockMilliseconds();

} // namespace lunanx::input

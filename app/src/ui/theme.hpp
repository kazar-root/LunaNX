// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=1f59df66f2414bcef73f
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include <borealis.hpp>

#include <string>

namespace lunanx::ui
{

// Palette LunaNX : fond nuit, violet Luna, or réservé à Luna Premium.
inline NVGcolor Ground() { return nvgRGB(10, 8, 22); }
inline NVGcolor Raised() { return nvgRGB(25, 20, 44); }
inline NVGcolor RaisedFocus() { return nvgRGB(39, 31, 66); }
inline NVGcolor Text() { return nvgRGB(255, 255, 255); }
inline NVGcolor Muted() { return nvgRGB(186, 180, 210); }
inline NVGcolor Rule() { return nvgRGB(54, 45, 84); }
inline NVGcolor Accent() { return nvgRGB(150, 120, 255); }
inline NVGcolor SecondaryAccent() { return nvgRGB(110, 72, 240); }
inline NVGcolor Premium() { return nvgRGB(255, 196, 77); }
inline NVGcolor Included() { return nvgRGB(120, 222, 170); }
inline NVGcolor Danger() { return nvgRGB(255, 120, 96); }

NVGcolor Opacity(NVGcolor color, float alpha);

enum class FontRole
{
    Body,       // Nunito Medium
    Medium,     // Nunito SemiBold
    Heading,    // Nunito Bold
    Display,    // Nunito ExtraBold
};

// Charge les polices Nunito (repli sur les polices de la Switch pour le chinois, le coréen
// et les symboles des boutons) et applique la palette aux vues Borealis.
void InitializeTheme();
int Font(FontRole role);

// Libellé dans une police Nunito.
class StyledLabel : public brls::Label
{
  public:
    explicit StyledLabel(FontRole role = FontRole::Body);
    void SetRole(FontRole role);
};

brls::Label* MakeLabel(const std::string& text, float size, NVGcolor color = Text(),
                       FontRole role = FontRole::Body);
float TextWidth(const std::string& text, float size, FontRole role);

// Carte arrondie des écrans de connexion et des messages.
brls::Box* MakeCard();

} // namespace lunanx::ui

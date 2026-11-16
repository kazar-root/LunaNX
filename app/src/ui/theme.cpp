// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=a6d0ff48736db914084a
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).
#include "core/source_origin.hpp"
LUNANX_SOURCE_ORIGIN(ua6d0ff48736db914084a, "a6d0ff48736db914084a")

#include "theme.hpp"

#include <borealis/core/assets.hpp>
#include <borealis/core/font.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace lunanx::ui
{
namespace
{

constexpr std::array<const char*, 4> kFontNames {"lunanx-medium", "lunanx-semibold", "lunanx-bold",
                                                  "lunanx-extrabold"};
bool g_fonts_loaded = false;

} // namespace

NVGcolor Opacity(NVGcolor color, float alpha)
{
    color.a *= alpha;
    return color;
}

void InitializeTheme()
{
    const std::array<const char*, 4> paths {"font/Nunito-Medium.ttf", "font/Nunito-SemiBold.ttf",
                                            "font/Nunito-Bold.ttf", "font/Nunito-ExtraBold.ttf"};
    g_fonts_loaded = true;
    for (size_t index = 0; index < paths.size(); ++index)
        if (!brls::Application::loadFontFromFile(kFontNames[index], std::string(BRLS_ASSET("")) + paths[index]))
            g_fonts_loaded = false;

    if (g_fonts_loaded)
    {
        // Nunito couvre le latin et le cyrillique ; le reste vient des polices de la console.
        std::vector<int> fallbacks;
        for (const auto& name : {brls::FONT_REGULAR, brls::FONT_CHINESE_SIMPLIFIED, brls::FONT_CHINESE_SIMPLIFIED_EXT,
                                 brls::FONT_CHINESE_TRADITIONAL, brls::FONT_KOREAN_REGULAR, brls::FONT_SWITCH_ICONS,
                                 brls::FONT_MATERIAL_ICONS})
            fallbacks.push_back(brls::Application::getFont(name));
        NVGcontext* vg = brls::Application::getNVGContext();
        for (const char* name : kFontNames)
        {
            const int base = brls::Application::getFont(name);
            for (int fallback : fallbacks)
                if (fallback != brls::FONT_INVALID && fallback != base)
                    nvgAddFallbackFontId(vg, base, fallback);
        }
    }

    for (brls::Theme* theme : {&brls::Theme::getDarkTheme(), &brls::Theme::getLightTheme()})
    {
        theme->addColor("brls/background", Ground());
        theme->addColor("brls/text", Text());
        theme->addColor("brls/text_disabled", Muted());
        theme->addColor("brls/accent", Accent());
        theme->addColor("brls/highlight/color1", Accent());
        theme->addColor("brls/highlight/color2", SecondaryAccent());
        theme->addColor("brls/highlight/background", Raised());
        theme->addColor("brls/click_pulse", Opacity(Accent(), 0.14f));
        theme->addColor("brls/button/primary_enabled_background", Text());
        theme->addColor("brls/button/primary_enabled_text", Ground());
        theme->addColor("brls/button/default_enabled_background", Raised());
        theme->addColor("brls/button/default_enabled_text", Text());
        theme->addColor("brls/button/highlight_enabled_text", Accent());
        theme->addColor("brls/button/highlight_disabled_text", Muted());
        theme->addColor("brls/button/enabled_border_color", Rule());
        theme->addColor("brls/list/listItem_value_color", Accent());
        theme->addColor("brls/sidebar/background", Ground());
        theme->addColor("brls/sidebar/active_item", Accent());
        theme->addColor("brls/slider/line_filled", Accent());
        theme->addColor("brls/header/subtitle", Muted());
    }
    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
    brls::getStyle().addMetric("brls/highlight/stroke_width", 2.0f);
}

int Font(FontRole role)
{
    if (g_fonts_loaded)
    {
        const int handle = brls::Application::getFont(kFontNames[static_cast<size_t>(role)]);
        if (handle != brls::FONT_INVALID)
            return handle;
    }
    return brls::Application::getDefaultFont();
}

StyledLabel::StyledLabel(FontRole role)
{
    SetRole(role);
    setAutoAnimate(false);
}

void StyledLabel::SetRole(FontRole role)
{
    font = Font(role);
}

brls::Label* MakeLabel(const std::string& text, float size, NVGcolor color, FontRole role)
{
    auto* label = new StyledLabel(role);
    label->setText(text);
    label->setFontSize(size);
    label->setTextColor(color);
    label->setLineHeight(1.3f);
    label->setAutoAnimate(false);
    return label;
}

float TextWidth(const std::string& text, float size, FontRole role)
{
    NVGcontext* vg = brls::Application::getNVGContext();
    nvgSave(vg);
    nvgFontFaceId(vg, Font(role));
    nvgFontSize(vg, size);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    float bounds[4] {};
    const float advance = nvgTextBounds(vg, 0, 0, text.c_str(), nullptr, bounds);
    nvgRestore(vg);
    return std::ceil(std::max(advance, bounds[2]) - std::min(0.0f, bounds[0]));
}

brls::Box* MakeCard()
{
    auto* card = new brls::Box(brls::Axis::COLUMN);
    card->setPadding(30, 36, 30, 36);
    card->setCornerRadius(18);
    card->setBorderThickness(1);
    card->setBorderColor(Rule());
    card->setBackgroundColor(Opacity(Raised(), 0.94f));
    return card;
}

} // namespace lunanx::ui

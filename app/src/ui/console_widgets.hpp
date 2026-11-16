// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=65286e3f72fc89daeeee
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include "theme.hpp"

#include <borealis.hpp>

#include <functional>
#include <memory>
#include <string>

namespace lunanx::ui
{

enum class Icon
{
    Menu,
    AllGames,
    Recent,
    Top,
    Premium,
    Search,
    Settings,
    Account,
    Play,
    Lock,
};

// Icônes vectorielles dessinées en NanoVG (grille 24 × 24 mise à l'échelle).
void DrawIcon(NVGcontext* vg, Icon icon, float x, float y, float size, NVGcolor color);
// Halo violet autour d'un élément focalisé : deux traits translucides, sans flou coûteux.
void DrawHalo(NVGcontext* vg, float x, float y, float width, float height, float radius, float amount);
// Dessine la texture d'une image (remplie et recadrée si `fill`, sinon contenue).
bool DrawImage(NVGcontext* vg, brls::Image& image, float x, float y, float width, float height,
               bool fill, float opacity = 1, float radius = 0);
// Libère une image après la fin de l'image en cours (NanoVG garde ses textures jusqu'à
// nvgEndFrame) et après le GPU.
void RetireImage(std::unique_ptr<brls::Image> image);

// Bouton rond à icône (ou pilule avec texte quand `primary`), avec halo au focus.
class ConsoleButton final : public brls::Box
{
  public:
    ConsoleButton(Icon icon, std::string title, std::function<void()> action, bool primary = false);
    ~ConsoleButton() override;
    void SetTitle(const std::string& title);
    void SetImageUrl(const std::string& url);
    void SetIcon(Icon icon) { icon_ = icon; }
    void SetActive(bool active) { active_ = active; }
    void SetBadge(bool badge) { badge_ = badge; }
    void SetTint(NVGcolor tint) { tint_ = tint; has_tint_ = true; }
    void onFocusGained() override;
    void onFocusLost() override;
    void draw(NVGcontext*, float, float, float, float, brls::Style, brls::FrameContext*) override;

  private:
    Icon icon_;
    std::string title_;
    bool primary_;
    bool active_ = false;
    bool badge_ = false;
    bool has_tint_ = false;
    NVGcolor tint_ {};
    brls::Animatable focus_ = 0;
    std::string image_url_;
    std::unique_ptr<brls::Image> image_;
};

// Fond de l'écran : visuel du jeu sélectionné en fondu enchaîné, assombri pour le texte.
class DynamicBackdrop
{
  public:
    ~DynamicBackdrop();
    void Select(const std::string& url);
    void Clear();
    void Draw(NVGcontext* vg, float x, float y, float width, float height);

  private:
    std::unique_ptr<brls::Image> current_, next_;
    std::string requested_, displayed_;
    brls::Animatable blend_ = 0;
    bool waiting_ = false;
    bool transitioning_ = false;
};

// Pastille d'état en haut à droite : heure, batterie, réseau et abonnement Luna.
class StatusPill final : public brls::View
{
  public:
    StatusPill();
    void SetPlan(const std::string& label, bool premium);
    void draw(NVGcontext*, float, float, float, float, brls::Style, brls::FrameContext*) override;

  private:
    std::string clock_;
    std::string plan_;
    bool premium_ = false;
    int battery_ = -1;
    bool charging_ = false;
    bool ethernet_ = false;
    bool wifi_ = false;
    int wifi_level_ = 0;
    double sampled_ = -10;
};

} // namespace lunanx::ui

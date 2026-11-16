// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=0f85f0189296228fbd14
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include "console_widgets.hpp"
#include "game_carousel.hpp"
#include "luna/luna_client.hpp"

#include <borealis.hpp>

#include <atomic>
#include <map>
#include <memory>

namespace lunanx
{

// Écran principal : connexion au compte, puis jeux Luna présentés en carrousel sur le visuel
// du jeu sélectionné (même présentation que NxNow). Les jeux affichés sont ceux de
// l'abonnement du compte ; les jeux Luna Premium sont regroupés à part quand le compte
// ne l'a pas.
class LibraryView : public brls::Box
{
  public:
    LibraryView();
    ~LibraryView() override;

    brls::View* getDefaultFocus() override;
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;
    // Retour sur l'écran (fin de partie, réglages) : met à jour « Reprendre » et l'affichage.
    void OnResume();

  private:
    enum class Section
    {
        All,
        Recent,
        Top,
        Premium,
    };

    // En-tête et zones de contenu.
    void BuildHeader();
    void BuildLibraryPanel();
    void RefreshTexts();
    void ShowLibraryPanel(bool visible);
    void SetStatus(const std::string& text);
    void ResetBackdrop();

    // Connexion et chargement.
    void Reload();
    void ShowLoading(const std::string& text);
    void ShowMessage(const std::string& title, const std::string& body);
    void ShowSignIn(const std::string& reason);
    void ShowSignedOut();
    void ShowStorePicker();
    void ChooseStore(const std::string& market_code);
    void SignInWithCode();
    void ShowDeviceCode(const luna::DeviceCodeChallenge& challenge);
    void StopCodeSignIn();
    brls::Box* ResetScroller(bool cancel_code_sign_in = true);

    // Bibliothèque.
    void ShowCatalog(const std::vector<luna::Shelf>& shelves);
    void ShowGameList();
    void AddListCell(const luna::Game& game);
    void SelectSection(Section section);
    void ApplySection(Section section);
    void RebuildSectionGames();
    void RefreshRecentGames();
    void RefreshCarousel(bool keep_selection);
    void ChangePage(int delta);
    void CycleSection(int delta);
    void SelectGame(const std::string& id);
    void UpdateSectionHeader();
    void UpdateHeaderButtons();
    void BeginSearch();
    void ClearSearch();
    void ShowMainMenu();
    void ShowAccount();
    void OpenSettings();
    void PlaySelected();
    const luna::Game* FindSectionGame(const std::string& id) const;
    bool IsLocked(const luna::Game& game) const;
    void StartCatalogExploration();
    void AddDiscoveredGames(std::vector<luna::Game> games);
    void Launch(const luna::Game& game);

    // En-tête.
    ui::ConsoleButton* menu_button_ = nullptr;
    ui::ConsoleButton* all_button_ = nullptr;
    ui::ConsoleButton* recent_button_ = nullptr;
    ui::ConsoleButton* top_button_ = nullptr;
    ui::ConsoleButton* search_button_ = nullptr;
    ui::ConsoleButton* account_button_ = nullptr;
    ui::ConsoleButton* settings_button_ = nullptr;
    ui::StatusPill* status_pill_ = nullptr;

    // Bibliothèque en carrousel.
    brls::Box* library_panel_ = nullptr;
    brls::Label* section_title_ = nullptr;
    brls::Label* section_subtitle_ = nullptr;
    brls::Label* status_label_ = nullptr;
    GameCarouselView* carousel_ = nullptr;
    brls::Box* info_ = nullptr;
    brls::Label* game_title_ = nullptr;
    brls::Label* game_meta_ = nullptr;
    brls::Label* game_offer_ = nullptr;
    ui::ConsoleButton* play_button_ = nullptr;
    brls::Label* page_label_ = nullptr;
    brls::Label* empty_label_ = nullptr;

    // Écrans de connexion, messages et liste simple.
    brls::ScrollingFrame* scroller_ = nullptr;
    brls::Box* content_ = nullptr;
    brls::Label* loading_label_ = nullptr;
    brls::View* first_card_ = nullptr;

    ui::DynamicBackdrop backdrop_;
    std::string backdrop_url_;
    int64_t backdrop_requested_at_ = 0;
    bool backdrop_pending_ = false;
    std::string language_;
    std::shared_ptr<luna::LunaClient> client_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    bool loading_ = false;
    uint64_t account_generation_ = 0;
    std::shared_ptr<std::atomic_bool> code_sign_in_cancel_;

    // Catalogue : tous les jeux connus, sans doublon (ASIN ou titre normalisé).
    std::vector<luna::Shelf> shelves_;
    std::vector<luna::Game> catalog_;
    std::map<std::string, size_t> catalog_index_;
    std::string featured_id_;
    std::string rendered_layout_;
    bool recent_changed_ = false;
    std::vector<luna::Shelf> recent_shelves_;
    bool recent_loaded_ = false;
    bool recent_loading_ = false;
    uint64_t recent_request_generation_ = 0;
    luna::AccountPlan plan_ = luna::AccountPlan::Unknown;
    std::string plan_label_;

    // Rubrique affichée.
    Section section_ = Section::Top;
    bool startup_selection_pending_ = true;
    std::vector<luna::Game> section_games_;
    size_t page_ = 0;
    std::string selected_id_;
    std::string search_query_;
    size_t premium_count_ = 0;
    brls::Label* list_heading_ = nullptr;

    std::atomic<uint64_t> explore_generation_ {0};
    bool exploration_done_ = false;
};

// Activité de la bibliothèque : relaie le retour sur l'écran à la vue.
class LibraryActivity : public brls::Activity
{
  public:
    explicit LibraryActivity(LibraryView* view)
        : brls::Activity(view)
        , view_(view)
    {
    }

    void onResume() override { view_->OnResume(); }

  private:
    LibraryView* view_;
};

} // namespace lunanx

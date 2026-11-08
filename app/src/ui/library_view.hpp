#pragma once

#include "luna/luna_client.hpp"

#include <borealis.hpp>

#include <atomic>
#include <map>
#include <memory>

namespace lunanx
{

class PhoneSignInServer;

// Écran principal : connexion au compte, puis bibliothèque Luna (jaquettes ou liste).
class LibraryView : public brls::Box
{
  public:
    LibraryView();
    ~LibraryView() override;

    brls::View* getDefaultFocus() override;
    // Retour sur l'écran (fin de partie, réglages) : met à jour « Reprendre » et l'affichage.
    void OnResume();

  private:
    void Reload();
    void ShowMessage(const std::string& title, const std::string& body);
    void ShowSignIn(const std::string& reason);
    // Compte supprimé depuis les réglages : oublie la session et le catalogue affichés.
    void ShowSignedOut();
    void ShowStorePicker();
    void ChooseStore(const std::string& market_code);
    void RefreshHeaderLanguage();
    void SignInWithAmazon();
    // Connexion depuis un téléphone (QR code) : n'utilise pas le navigateur de la Switch.
    void SignInWithPhone();
    void StopPhoneSignIn();
    // Échange le code d'autorisation contre un enregistrement d'appareil, puis recharge.
    void CompleteSignIn(const luna::PendingSignIn& pending, const std::string& code);
    void ShowShelves(const std::vector<luna::Shelf>& shelves);
    void ShowGameList(const std::vector<luna::Game>& games);
    void ShowCovers(const std::vector<luna::Shelf>& shelves, const std::vector<luna::Game>& games);
    void AddRow(const std::string& title, const std::string& subtitle,
                const std::vector<luna::Game>& games, bool ranked);
    void AddListCell(const luna::Game& game);
    void ShowGridPage(size_t page, bool focus_first_card);
    void AppendToGridPage();
    void UpdatePager();
    void UpdateGameCount();
    void StartCatalogExploration();
    void AddDiscoveredGames(std::vector<luna::Game> games);
    void Launch(const luna::Game& game);

    brls::Label* status_label_ = nullptr;
    brls::Button* refresh_button_ = nullptr;
    brls::Button* settings_button_ = nullptr;
    std::string language_;
    brls::ScrollingFrame* scroller_ = nullptr;
    brls::Box* content_ = nullptr;
    brls::View* first_card_ = nullptr;
    std::shared_ptr<luna::LunaClient> client_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    bool loading_ = false;
    // Serveur local de la connexion par téléphone, actif seulement sur son écran.
    std::unique_ptr<PhoneSignInServer> phone_server_;

    // Catalogue affiché, gardé pour redessiner sans retélécharger.
    std::vector<luna::Shelf> shelves_;
    std::string rendered_layout_;
    bool recent_changed_ = false;
    // Tous les jeux connus, sans doublon, et leurs clés ASIN / titre normalisé.
    std::vector<luna::Game> catalog_;
    std::map<std::string, size_t> catalog_index_;
    // Jeux de « Tous les jeux » (grille ou liste).
    std::vector<luna::Game> grid_games_;
    // Grille « Tous les jeux » paginée : seules les 10 jaquettes de la page existent, pour
    // borner la mémoire des textures quel que soit le nombre de jeux.
    size_t grid_page_ = 0;
    size_t grid_shown_end_ = 0;
    brls::Box* grid_ = nullptr;
    brls::Box* grid_page_box_ = nullptr;
    brls::Box* grid_row_ = nullptr;
    brls::Box* pager_ = nullptr;
    brls::Button* prev_button_ = nullptr;
    brls::Button* next_button_ = nullptr;
    brls::Label* page_label_ = nullptr;
    brls::Label* all_games_count_ = nullptr;
    // Exploration du catalogue en arrière-plan ; un nouveau chargement annule la précédente.
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

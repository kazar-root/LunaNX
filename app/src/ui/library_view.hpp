#pragma once

#include "luna/luna_client.hpp"

#include <borealis.hpp>

#include <memory>

namespace lunanx
{

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
    void SignInWithAmazon();
    void ShowShelves(const std::vector<luna::Shelf>& shelves);
    void ShowGameList(const std::vector<luna::Game>& games);
    void ShowCovers(const std::vector<luna::Shelf>& shelves, const std::vector<luna::Game>& games);
    void AddRow(const std::string& title, const std::string& subtitle,
                const std::vector<luna::Game>& games, bool ranked);
    void ShowMoreGames(bool focus_new_cards);
    void Launch(const luna::Game& game);

    brls::Label* status_label_ = nullptr;
    brls::ScrollingFrame* scroller_ = nullptr;
    brls::Box* content_ = nullptr;
    brls::View* first_card_ = nullptr;
    std::shared_ptr<luna::LunaClient> client_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    bool loading_ = false;

    // Catalogue affiché, gardé pour redessiner sans retélécharger.
    std::vector<luna::Shelf> shelves_;
    std::string rendered_layout_;
    bool recent_changed_ = false;
    // Grille « Tous les jeux » : 10 jaquettes ajoutées à la fois.
    std::vector<luna::Game> grid_games_;
    size_t grid_shown_ = 0;
    brls::Box* grid_ = nullptr;
    brls::Button* more_button_ = nullptr;
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

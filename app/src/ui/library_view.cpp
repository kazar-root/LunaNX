#include "library_view.hpp"

#include "amazon_sign_in.hpp"
#include "core/app_paths.hpp"
#include "core/app_version.hpp"
#include "core/i18n.hpp"
#include "core/runtime_journal.hpp"
#include "core/stream_settings.hpp"
#include "luna/entitlement_policy.hpp"
#include "luna/recent_games.hpp"
#include "phone_sign_in.hpp"
#include "qr_code_view.hpp"
#include "settings_view.hpp"
#include "stream_view.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <optional>
#include <set>

namespace lunanx
{
namespace
{

std::string CookieHelpText()
{
    return TrFormat("Solution de secours si la connexion directe échoue : exporte une seule fois la "
                    "session de ton navigateur.\n1. Sur ton PC, connecte-toi sur luna.amazon.fr.\n"
                    "2. Avec l'extension « Get cookies.txt LOCALLY », clique sur « Export » (fichier "
                    "cookies.txt).\n3. Copie ce fichier dans {}/ puis appuie sur Y.\nATTENTION : ce "
                    "fichier donne un accès complet à ton compte Amazon. Ne le partage jamais.",
                    {AppHomePath()});
}

// Clés d'un jeu : son ASIN et son titre normalisé (deux tuiles du même jeu n'ont pas
// toujours toutes les deux un ASIN).
std::vector<std::string> GameKeys(const luna::Game& game)
{
    std::vector<std::string> keys;
    if (!game.asin.empty())
        keys.push_back("a:" + game.asin);
    const std::string title = luna::NormalizedGameTitle(game.title);
    if (!title.empty())
        keys.push_back("t:" + title);
    return keys;
}

// Identifiant stable d'un jeu dans le carrousel.
std::string GameId(const luna::Game& game)
{
    return !game.asin.empty() ? "a:" + game.asin : "t:" + luna::NormalizedGameTitle(game.title);
}

std::string Lowercase(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// Ajoute le jeu au catalogue s'il n'y est pas encore (même ASIN ou même titre normalisé).
bool AddToCatalog(std::vector<luna::Game>& catalog, std::map<std::string, size_t>& index, const luna::Game& game)
{
    const std::vector<std::string> keys = GameKeys(game);
    if (game.title.empty() || keys.empty())
        return false;
    size_t slot = catalog.size();
    for (const auto& key : keys)
    {
        const auto found = index.find(key);
        if (found != index.end())
        {
            slot = found->second;
            break;
        }
    }
    const bool added = slot == catalog.size();
    if (added)
        catalog.push_back(game);
    else
    {
        // Même jeu vu dans une autre rangée : on garde l'étiquette d'offre si elle manquait.
        luna::Game& known = catalog[slot];
        if (known.offer_tag.empty() && !game.offer_tag.empty())
            known.offer_tag = game.offer_tag;
        if (known.offer_icon.empty() && !game.offer_icon.empty())
            known.offer_icon = game.offer_icon;
        known.premium_only = known.premium_only || game.premium_only;
        if (known.publisher.empty())
            known.publisher = game.publisher;
        if (known.genres.empty())
            known.genres = game.genres;
    }
    for (const auto& key : keys)
        index.emplace(key, slot);
    return added;
}

constexpr size_t kTopCount = 10;
constexpr size_t kRecentCount = 10;

void AppendUnique(std::vector<luna::Game>& out, std::set<std::string>& seen, const luna::Game& game, size_t limit)
{
    if (out.size() < limit && !game.title.empty() && luna::InsertGameKeys(seen, game))
        out.push_back(game);
}

// Données à jour d'un jeu tiré d'une rangée ou de l'historique : celles du catalogue.
luna::Game Fresh(const luna::Game& game, const std::vector<luna::Game>& catalog,
                 const std::map<std::string, size_t>& index)
{
    for (const auto& key : GameKeys(game))
    {
        const auto found = index.find(key);
        if (found != index.end() && found->second < catalog.size())
            return catalog[found->second];
    }
    return game;
}

// Rangée de classement Luna (« Top du moment ») ; à défaut, une rangée dont le titre parle de top.
std::vector<luna::Game> PickTopGames(const std::vector<luna::Shelf>& shelves)
{
    const luna::Shelf* ranking = nullptr;
    for (const auto& shelf : shelves)
        if (shelf.row_type == "RANKINGS_ROW")
        {
            ranking = &shelf;
            break;
        }
    if (!ranking)
        for (const auto& shelf : shelves)
            if (Lowercase(shelf.title).find("top") != std::string::npos)
            {
                ranking = &shelf;
                break;
            }
    std::vector<luna::Game> top;
    std::set<std::string> seen;
    if (ranking)
        for (const auto& game : ranking->games)
            AppendUnique(top, seen, game, kTopCount);
    return top;
}

// « Reprendre » : historique local d'abord, puis une éventuelle rangée Luna de jeux récents.
std::vector<luna::Game> PickRecentGames(const std::vector<luna::Shelf>& shelves)
{
    std::vector<luna::Game> recent;
    std::set<std::string> seen;
    for (const auto& played : luna::LoadRecentGames())
        if (!played.page_uri.empty() || !played.asin.empty())
            AppendUnique(recent, seen, played, kRecentCount);
    for (const auto& shelf : shelves)
    {
        const std::string title = Lowercase(shelf.title);
        for (const char* keyword : {"cemment", "reprendre", "continuer", "continue", "recent"})
            if (title.find(keyword) != std::string::npos)
            {
                for (const auto& game : shelf.games)
                    AppendUnique(recent, seen, game, kRecentCount);
                break;
            }
    }
    return recent;
}

// Jeu mis en avant : premier visuel « À la une » de Luna, sinon le n°1 du classement.
std::optional<luna::Game> PickFeaturedGame(const std::vector<luna::Shelf>& shelves)
{
    for (const auto& shelf : shelves)
        if (shelf.row_type.find("FEATURED") != std::string::npos)
            for (const auto& game : shelf.games)
                if (!game.image_landscape.empty() && !game.title.empty())
                    return game;
    for (const auto& game : PickTopGames(shelves))
        if (!game.image_landscape.empty())
            return game;
    return std::nullopt;
}

std::string BackdropUrl(const luna::Game& game)
{
    return !game.image_landscape.empty() ? game.image_landscape : game.image_portrait;
}

brls::Button* MakeCardButton(const std::string& text, bool primary)
{
    auto* button = new brls::Button();
    button->setText(text);
    button->setStyle(primary ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_BORDERED);
    button->setCornerRadius(24);
    button->setHeight(50);
    return button;
}

static bool HasStoredAccount()
{
    luna::DeviceRegistration registration;
    luna::CookieJar cookies;
    return luna::AmazonAuth::Load(registration) || cookies.LoadDefault();
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Construction de l'écran

LibraryView::LibraryView()
    : brls::Box(brls::Axis::COLUMN)
{
    setGrow(1.0f);
    language_ = UiLanguage();

    BuildHeader();

    auto* body = new brls::Box(brls::Axis::COLUMN);
    body->setGrow(1.0f);
    body->setMinHeight(0);
    BuildLibraryPanel();
    body->addView(library_panel_);
    scroller_ = new brls::ScrollingFrame();
    scroller_->setGrow(1.0f);
    scroller_->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    content_ = new brls::Box(brls::Axis::COLUMN);
    content_->setPadding(6, 64, 24, 64);
    scroller_->setContentView(content_);
    body->addView(scroller_);
    addView(body);

    BuildFooter();

    registerAction(Tr("Actualiser"), brls::BUTTON_Y, [this](brls::View*) {
        Reload();
        return true;
    });
    registerAction(Tr("Réglages"), brls::BUTTON_X, [](brls::View*) {
        brls::Application::pushActivity(new brls::Activity(new SettingsView()));
        return true;
    });
    registerAction(Tr("Menu principal"), brls::BUTTON_BACK, [this](brls::View*) {
        ShowMainMenu();
        return true;
    });
    registerAction(Tr("Quitter"), brls::BUTTON_START, [](brls::View*) {
        brls::Application::quit();
        return true;
    });
    registerAction({}, brls::BUTTON_LB, [this](brls::View*) {
        ChangePage(-1);
        return true;
    }, true);
    registerAction({}, brls::BUTTON_RB, [this](brls::View*) {
        ChangePage(1);
        return true;
    }, true);
    registerAction({}, brls::BUTTON_LT, [this](brls::View*) {
        CycleSection(-1);
        return true;
    }, true);
    registerAction({}, brls::BUTTON_RT, [this](brls::View*) {
        CycleSection(1);
        return true;
    }, true);
    registerAction({}, brls::BUTTON_B, [this](brls::View*) {
        if (!(library_panel_->getVisibility() == brls::Visibility::VISIBLE) && !catalog_.empty())
            return false;
        if (!search_query_.empty())
        {
            ClearSearch();
            return true;
        }
        if (section_ != Section::All && !catalog_.empty())
        {
            SelectSection(Section::All);
            return true;
        }
        return false;
    }, true);

    UpdateHeaderButtons();
    ShowLoading(Tr("Connexion…"));
    Reload();
}

LibraryView::~LibraryView()
{
    *alive_ = false;
    StopPhoneSignIn();
}

void LibraryView::BuildHeader()
{
    auto* header = new brls::Box(brls::Axis::ROW);
    header->setHeight(92);
    header->setShrink(0);
    header->setPadding(18, 40, 6, 40);
    header->setAlignItems(brls::AlignItems::CENTER);

    auto* logo = new brls::Image();
    logo->setImageFromRes("img/lunanx_logo.png");
    logo->setWidth(54);
    logo->setHeight(54);
    logo->setCornerRadius(14);
    logo->setMarginRight(10);
    header->addView(logo);
    auto* brand = ui::MakeLabel("LunaNX", 24, ui::Text(), ui::FontRole::Display);
    brand->setMarginRight(22);
    header->addView(brand);

    auto add_button = [header](ui::Icon icon, const std::string& title, std::function<void()> action) {
        auto* button = new ui::ConsoleButton(icon, title, std::move(action));
        button->setMarginRight(10);
        header->addView(button);
        return button;
    };
    menu_button_ = add_button(ui::Icon::Menu, Tr("Menu principal"), [this] { ShowMainMenu(); });
    menu_button_->setMarginRight(22);
    all_button_ = add_button(ui::Icon::AllGames, Tr("Tous les jeux"), [this] { SelectSection(Section::All); });
    recent_button_ = add_button(ui::Icon::Recent, Tr("Reprendre"), [this] { SelectSection(Section::Recent); });
    top_button_ = add_button(ui::Icon::Top, "Top 10", [this] { SelectSection(Section::Top); });
    premium_button_ = add_button(ui::Icon::Premium, "Luna Premium", [this] { SelectSection(Section::Premium); });
    premium_button_->SetTint(ui::Premium());
    search_button_ = add_button(ui::Icon::Search, Tr("Rechercher"), [this] { BeginSearch(); });

    auto* spacer = new brls::Box();
    spacer->setGrow(1.0f);
    header->addView(spacer);

    status_pill_ = new ui::StatusPill();
    status_pill_->setMarginRight(10);
    header->addView(status_pill_);
    account_button_ = new ui::ConsoleButton(ui::Icon::Account, Tr("Compte"), [this] { ShowAccount(); });
    account_button_->setMarginRight(10);
    header->addView(account_button_);
    settings_button_ = new ui::ConsoleButton(ui::Icon::Settings, Tr("Réglages"), [] {
        brls::Application::pushActivity(new brls::Activity(new SettingsView()));
    });
    header->addView(settings_button_);
    addView(header);
}

void LibraryView::BuildLibraryPanel()
{
    library_panel_ = new brls::Box(brls::Axis::COLUMN);
    library_panel_->setGrow(1.0f);
    library_panel_->setMinHeight(0);

    auto* heading = new brls::Box(brls::Axis::ROW);
    heading->setHeight(40);
    heading->setShrink(0);
    heading->setPadding(0, 64, 0, 64);
    heading->setAlignItems(brls::AlignItems::CENTER);
    auto* bar = new brls::Box();
    bar->setWidth(4);
    bar->setHeight(20);
    bar->setCornerRadius(2);
    bar->setBackgroundColor(ui::Accent());
    bar->setMarginRight(12);
    heading->addView(bar);
    section_title_ = ui::MakeLabel({}, 19, ui::Text(), ui::FontRole::Heading);
    section_title_->setMarginRight(14);
    section_title_->setSingleLine(true);
    heading->addView(section_title_);
    section_subtitle_ = ui::MakeLabel({}, 15, ui::Muted());
    section_subtitle_->setSingleLine(true);
    section_subtitle_->setGrow(1.0f);
    section_subtitle_->setShrink(1.0f);
    heading->addView(section_subtitle_);
    status_label_ = ui::MakeLabel({}, 14, ui::Muted());
    status_label_->setSingleLine(true);
    status_label_->setMarginLeft(14);
    heading->addView(status_label_);
    library_panel_->addView(heading);

    carousel_ = new GameCarouselView([this](const std::string& id) { SelectGame(id); }, [this] { PlaySelected(); },
                                     [this] { ChangePage(1); }, [this] { ChangePage(-1); });
    carousel_->setMarginTop(4);
    carousel_->setMarginLeft(40);
    carousel_->setMarginRight(40);
    library_panel_->addView(carousel_);

    empty_label_ = ui::MakeLabel({}, 22, ui::Text(), ui::FontRole::Heading);
    empty_label_->setMargins(60, 64, 0, 64);
    empty_label_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    empty_label_->setVisibility(brls::Visibility::GONE);
    library_panel_->addView(empty_label_);

    info_ = new brls::Box(brls::Axis::COLUMN);
    info_->setPadding(4, 64, 0, 64);
    info_->setShrink(0);
    game_title_ = ui::MakeLabel({}, 34, ui::Text(), ui::FontRole::Display);
    game_title_->setSingleLine(true);
    game_title_->setHeight(46);
    info_->addView(game_title_);
    auto* meta_row = new brls::Box(brls::Axis::ROW);
    meta_row->setAlignItems(brls::AlignItems::CENTER);
    meta_row->setHeight(26);
    game_offer_ = ui::MakeLabel({}, 15, ui::Included(), ui::FontRole::Heading);
    game_offer_->setSingleLine(true);
    game_offer_->setMarginRight(14);
    meta_row->addView(game_offer_);
    game_meta_ = ui::MakeLabel({}, 15, ui::Muted());
    game_meta_->setSingleLine(true);
    game_meta_->setShrink(1.0f);
    meta_row->addView(game_meta_);
    info_->addView(meta_row);

    auto* actions = new brls::Box(brls::Axis::ROW);
    actions->setAlignItems(brls::AlignItems::CENTER);
    actions->setMarginTop(14);
    actions->setHeight(60);
    play_button_ = new ui::ConsoleButton(ui::Icon::Play, Tr("JOUER"), [this] { PlaySelected(); }, true);
    play_button_->setMarginRight(22);
    actions->addView(play_button_);
    page_label_ = ui::MakeLabel({}, 15, ui::Muted(), ui::FontRole::Medium);
    page_label_->setSingleLine(true);
    actions->addView(page_label_);
    info_->addView(actions);
    library_panel_->addView(info_);

    carousel_->setCustomNavigationRoute(brls::FocusDirection::DOWN, play_button_);
    play_button_->setCustomNavigationRoute(brls::FocusDirection::UP, carousel_);
    play_button_->setCustomNavigationRoute(brls::FocusDirection::LEFT, play_button_);
    play_button_->setCustomNavigationRoute(brls::FocusDirection::RIGHT, play_button_);
    play_button_->setCustomNavigationRoute(brls::FocusDirection::DOWN, play_button_);
    carousel_->setCustomNavigationRoute(brls::FocusDirection::UP, all_button_);
}

void LibraryView::BuildFooter()
{
    auto* footer = new brls::Box(brls::Axis::ROW);
    footer->setHeight(46);
    footer->setShrink(0);
    footer->setPadding(0, 40, 0, 40);
    footer->setAlignItems(brls::AlignItems::CENTER);
    version_label_ = ui::MakeLabel(std::string("LunaNX · ") + kAppVersion, 13, ui::Muted(), ui::FontRole::Medium);
    version_label_->setWidth(150);
    version_label_->setShrink(0);
    footer->addView(version_label_);
    auto* hints = new brls::Hints();
    hints->setGrow(1.0f);
    hints->setAllowAButtonTouch(true);
    hints->setJustifyContent(brls::JustifyContent::CENTER);
    footer->addView(hints);
    auto* balance = new brls::Box();
    balance->setWidth(150);
    balance->setShrink(0);
    footer->addView(balance);
    addView(footer);
}

void LibraryView::RefreshTexts()
{
    language_ = UiLanguage();
    updateActionHint(brls::BUTTON_Y, Tr("Actualiser"));
    updateActionHint(brls::BUTTON_X, Tr("Réglages"));
    updateActionHint(brls::BUTTON_BACK, Tr("Menu principal"));
    updateActionHint(brls::BUTTON_START, Tr("Quitter"));
    menu_button_->SetTitle(Tr("Menu principal"));
    all_button_->SetTitle(Tr("Tous les jeux"));
    recent_button_->SetTitle(Tr("Reprendre"));
    search_button_->SetTitle(Tr("Rechercher"));
    account_button_->SetTitle(Tr("Compte"));
    settings_button_->SetTitle(Tr("Réglages"));
    play_button_->SetTitle(Tr("JOUER"));
}

brls::View* LibraryView::getDefaultFocus()
{
    if ((library_panel_->getVisibility() == brls::Visibility::VISIBLE))
        return carousel_->ItemCount() ? static_cast<brls::View*>(carousel_) : static_cast<brls::View*>(all_button_);
    return first_card_ ? first_card_ : brls::Box::getDefaultFocus();
}

void LibraryView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                       brls::FrameContext* ctx)
{
    // Fond du jeu sélectionné : demandé quand la sélection est stable, pour ne pas télécharger
    // le visuel de chaque jeu survolé pendant un défilement rapide.
    if (backdrop_pending_ && brls::getCPUTimeUsec() - backdrop_requested_at_ >= 180000)
    {
        backdrop_pending_ = false;
        backdrop_.Select(backdrop_url_);
    }
    backdrop_.Draw(vg, x, y, width, height);
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

void LibraryView::ShowLibraryPanel(bool visible)
{
    library_panel_->setVisibility(visible ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    scroller_->setVisibility(visible ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    if (!visible)
        ResetBackdrop();
    UpdateHeaderButtons();
}

void LibraryView::UpdateHeaderButtons()
{
    const bool covers = !catalog_.empty() && rendered_layout_ != "List" &&
                        library_panel_->getVisibility() == brls::Visibility::VISIBLE;
    const auto shown = [](bool visible) { return visible ? brls::Visibility::VISIBLE : brls::Visibility::GONE; };
    all_button_->setVisibility(shown(covers));
    recent_button_->setVisibility(shown(covers));
    top_button_->setVisibility(shown(covers && !PickTopGames(shelves_).empty()));
    premium_button_->setVisibility(shown(covers && premium_count_ > 0));
    search_button_->setVisibility(shown(covers));
    all_button_->SetActive(section_ == Section::All);
    recent_button_->SetActive(section_ == Section::Recent);
    top_button_->SetActive(section_ == Section::Top);
    premium_button_->SetActive(section_ == Section::Premium);
    search_button_->SetActive(!search_query_.empty());
    // Abonnement du compte dans la pastille d'état.
    std::string plan;
    if (plan_ == luna::AccountPlan::Premium)
        plan = "Luna Premium";
    else if (plan_ == luna::AccountPlan::Standard)
        plan = plan_label_.empty() ? "Prime" : plan_label_;
    status_pill_->SetPlan(client_ ? plan : std::string(), plan_ == luna::AccountPlan::Premium);
    account_button_->SetBadge(plan_ == luna::AccountPlan::Premium);
    // Haut du carrousel : bouton de la rubrique affichée (ou le premier bouton visible).
    brls::View* up = all_button_;
    if (section_ == Section::Recent)
        up = recent_button_;
    else if (section_ == Section::Top && (top_button_->getVisibility() == brls::Visibility::VISIBLE))
        up = top_button_;
    else if (section_ == Section::Premium && (premium_button_->getVisibility() == brls::Visibility::VISIBLE))
        up = premium_button_;
    carousel_->setCustomNavigationRoute(brls::FocusDirection::UP, covers ? up : menu_button_);
}

void LibraryView::ResetBackdrop()
{
    backdrop_pending_ = false;
    backdrop_url_.clear();
    backdrop_.Select({});
}

void LibraryView::SetStatus(const std::string& text)
{
    if (loading_label_)
        loading_label_->setText(text);
    if (status_label_)
        status_label_->setText(text);
}

// ---------------------------------------------------------------------------------------------
// Connexion et messages

brls::Box* LibraryView::ResetScroller()
{
    StopPhoneSignIn();
    content_->clearViews();
    first_card_ = nullptr;
    loading_label_ = nullptr;
    list_heading_ = nullptr;
    ShowLibraryPanel(false);
    return content_;
}

void LibraryView::ShowLoading(const std::string& text)
{
    ResetScroller();
    auto* card = ui::MakeCard();
    card->setMarginTop(80);
    card->setAlignItems(brls::AlignItems::CENTER);
    card->setFocusable(true);
    card->setHideHighlight(true);
    auto* title = ui::MakeLabel(Tr("Chargement du catalogue…"), 26, ui::Text(), ui::FontRole::Display);
    title->setMarginBottom(10);
    card->addView(title);
    loading_label_ = ui::MakeLabel(text, 17, ui::Accent(), ui::FontRole::Medium);
    card->addView(loading_label_);
    content_->addView(card);
    first_card_ = card;
    brls::Application::giveFocus(card);
}

void LibraryView::ShowMessage(const std::string& title, const std::string& body)
{
    ResetScroller();
    auto* card = ui::MakeCard();
    card->setMarginTop(40);
    card->setFocusable(true);
    card->setHideHighlight(true);
    auto* heading = ui::MakeLabel(title, 26, ui::Text(), ui::FontRole::Display);
    heading->setMarginBottom(14);
    card->addView(heading);
    card->addView(ui::MakeLabel(body, 18, ui::Muted()));
    content_->addView(card);
    first_card_ = card;
    brls::Application::giveFocus(card);
}

void LibraryView::ShowSignedOut()
{
    ++explore_generation_;
    exploration_done_ = false;
    client_.reset();
    shelves_.clear();
    catalog_.clear();
    catalog_index_.clear();
    section_games_.clear();
    featured_id_.clear();
    selected_id_.clear();
    search_query_.clear();
    section_ = Section::All;
    page_ = 0;
    premium_count_ = 0;
    plan_ = luna::AccountPlan::Unknown;
    plan_label_.clear();
    rendered_layout_.clear();
    recent_changed_ = false;
    carousel_->SetGames({}, {}, false, false);
    backdrop_pending_ = false;
    backdrop_.Clear();
    SetStatus("");
    ShowSignIn({});
}

void LibraryView::Reload()
{
    if (loading_)
        return;
    loading_ = true;
    ++explore_generation_;
    if (catalog_.empty())
        ShowLoading(Tr("Connexion…"));
    SetStatus(Tr("Connexion…"));

    auto alive = alive_;
    brls::async([this, alive]() {
        luna::CookieJar cookies;
        luna::DeviceRegistration registration;
        const bool registered = luna::AmazonAuth::Load(registration);
        bool has_cookies = false;
        if (registered)
        {
            cookies.SetCookies(registration.cookies);
            has_cookies = true;
        }
        else
        {
            has_cookies = cookies.LoadDefault();
        }
        auto client = std::make_shared<luna::LunaClient>(
            cookies, registered ? std::optional<luna::DeviceRegistration>(registration) : std::nullopt);
        std::vector<luna::Shelf> shelves;
        std::string error_title;
        std::string error_body;
        std::string sign_in_reason;

        try
        {
            if (!has_cookies)
                throw luna::SignInRequired("Aucun fichier de cookies");
            client->SignIn([this, alive](const std::string& status) {
                brls::sync([this, alive, status] {
                    if (*alive)
                        SetStatus(status);
                });
            });
            brls::sync([this, alive] {
                if (*alive)
                    SetStatus(Tr("Chargement du catalogue…"));
            });
            // Catalogue dans la langue choisie ; si la boutique ne la propose pas, langue locale.
            if (client->SetContentLanguage(UiLanguage()))
            {
                try
                {
                    shelves = client->FetchShelves();
                }
                catch (const luna::SignInRequired&)
                {
                    throw;
                }
                catch (const std::exception& ex)
                {
                    LogRuntimeEvent("library", "localized_catalog_failed", ex.what());
                }
                if (shelves.empty())
                {
                    client->SetContentLanguage({});
                    shelves = client->FetchShelves();
                }
            }
            else
            {
                shelves = client->FetchShelves();
            }
        }
        catch (const luna::SignInRequired& ex)
        {
            if (registered)
                luna::AmazonAuth::Erase();
            sign_in_reason = has_cookies ? TrFormat("Ta session Amazon a expiré ({}). Reconnecte-toi.", {ex.what()})
                                         : std::string();
            error_title = "sign-in";
        }
        catch (const std::exception& ex)
        {
            LogRuntimeEvent("library", "load_failed", ex.what());
            error_title = Tr("Impossible de charger Luna");
            error_body = TrFormat("{}\n\nVérifie la connexion Internet puis appuie sur Y.", {ex.what()});
        }

        brls::sync([this, alive, client, shelves = std::move(shelves), error_title, error_body, sign_in_reason]() {
            if (!*alive)
                return;
            loading_ = false;
            // Déconnexion demandée pendant ce chargement : son résultat ne doit pas s'afficher.
            if (error_title != "sign-in" && !HasStoredAccount())
            {
                ShowSignedOut();
                return;
            }
            if (error_title == "sign-in")
            {
                SetStatus("");
                ShowSignIn(sign_in_reason);
                return;
            }
            if (!error_title.empty())
            {
                SetStatus("");
                // Bibliothèque déjà affichée : on la garde, l'erreur est signalée en haut.
                if (!catalog_.empty() && (library_panel_->getVisibility() == brls::Visibility::VISIBLE))
                {
                    brls::Application::notify(error_title);
                    return;
                }
                ShowMessage(error_title, error_body);
                return;
            }
            client_ = client;
            plan_ = client_->Plan();
            plan_label_ = client_->PlanLabel();
            SetStatus("");
            ShowCatalog(shelves);
            StartCatalogExploration();
        });
    });
}

void LibraryView::ShowStorePicker()
{
    auto* content = ResetScroller();
    auto* card = ui::MakeCard();
    card->setMarginTop(10);
    auto* heading = ui::MakeLabel(Tr("Choisis ta boutique Amazon"), 26, ui::Text(), ui::FontRole::Display);
    heading->setMarginBottom(10);
    card->addView(heading);
    auto* explanation = ui::MakeLabel(Tr("Luna n'est disponible que dans ces pays. LunaNX se connectera à la "
                                         "boutique choisie et utilisera sa langue (modifiable ensuite dans les "
                                         "Réglages)."),
                                      17, ui::Muted());
    explanation->setMarginBottom(18);
    card->addView(explanation);

    // Boutiques Luna, par ordre alphabétique de leur nom dans leur propre langue.
    std::vector<luna::AmazonMarket> markets = luna::AmazonMarkets();
    // « Österreich » se range avec les O (l'ordre des octets UTF-8 la placerait en dernier).
    auto sort_key = [](std::string name) {
        if (name.compare(0, 2, "\xC3\x96") == 0)
            name.replace(0, 2, "O");
        return name;
    };
    std::sort(markets.begin(), markets.end(), [&sort_key](const luna::AmazonMarket& a, const luna::AmazonMarket& b) {
        return sort_key(a.native_name) < sort_key(b.native_name);
    });
    const std::string current = LoadStreamSettings().amazon_market;
    for (const auto& market : markets)
    {
        auto* button = MakeCardButton(market.native_name + "  ·  amazon." + market.domain, market.code == current);
        button->setMarginBottom(10);
        const std::string code = market.code;
        button->registerClickAction([this, code](brls::View*) {
            ChooseStore(code);
            return true;
        });
        card->addView(button);
        if (!first_card_ || market.code == current)
            first_card_ = button;
    }
    content->addView(card);
    brls::Application::giveFocus(first_card_);
}

void LibraryView::ChooseStore(const std::string& market_code)
{
    // La boutique fixe le point d'accès Luna ; sa langue devient celle de LunaNX, du catalogue
    // et des jeux (x-amz-locale). La langue reste modifiable ensuite dans les réglages.
    StreamSettings settings = LoadStreamSettings();
    settings.amazon_market = market_code;
    settings.language = luna::FindAmazonMarket(market_code).language;
    SaveStreamSettings(settings);
    SetUiLanguage(settings.language);
    RefreshTexts();
    // Différé : le bouton qui a déclenché le choix ne doit pas être détruit pendant son clic.
    auto alive = alive_;
    brls::sync([this, alive]() {
        if (*alive)
            ShowSignIn({});
    });
}

void LibraryView::ShowSignIn(const std::string& reason)
{
    // Premier lancement : la boutique Amazon doit être choisie avant la connexion.
    if (LoadStreamSettings().amazon_market.empty())
    {
        ShowStorePicker();
        return;
    }
    auto* content = ResetScroller();
    const luna::AmazonMarket market = luna::FindAmazonMarket(LoadStreamSettings().amazon_market);

    auto* card = ui::MakeCard();
    card->setMarginTop(10);
    auto* heading = ui::MakeLabel(Tr("Connexion à ton compte Amazon"), 26, ui::Text(), ui::FontRole::Display);
    heading->setMarginBottom(12);
    card->addView(heading);
    if (!reason.empty())
    {
        auto* warning = ui::MakeLabel(reason, 17, ui::Danger(), ui::FontRole::Medium);
        warning->setMarginBottom(10);
        card->addView(warning);
    }
    card->addView(ui::MakeLabel(
        TrFormat("La page de connexion officielle d'Amazon ({}) va s'ouvrir dans le navigateur de la "
                 "Switch. Saisis ton e-mail, ton mot de passe et, si besoin, le code de validation reçu.\n"
                 "Ton mot de passe n'est jamais lu ni enregistré par LunaNX : la console est simplement "
                 "ajoutée à tes appareils Amazon (« LunaNX »), et tu peux la retirer à tout moment depuis "
                 "ton compte ou dans les Réglages.\nCompte d'un autre pays ? Change-le dans les Réglages (X).",
                 {Tr(market.label)}),
        17, ui::Muted()));

    auto* button = MakeCardButton(Tr("Se connecter avec Amazon"), true);
    button->setMarginTop(20);
    button->setMarginBottom(12);
    button->registerClickAction([this](brls::View*) {
        SignInWithAmazon();
        return true;
    });
    card->addView(button);

    // Alternative sans le navigateur de la Switch (indisponible sur certaines consoles).
    auto* phone_button = MakeCardButton(Tr("Se connecter avec mon téléphone"), true);
    phone_button->setMarginBottom(8);
    phone_button->registerClickAction([this](brls::View*) {
        auto alive = alive_;
        brls::sync([this, alive]() {
            if (*alive)
                SignInWithPhone();
        });
        return true;
    });
    card->addView(phone_button);
    auto* phone_hint = ui::MakeLabel(Tr("Le navigateur de la Switch ne s'ouvre pas ou affiche « This feature is not "
                                        "available » ? Connecte-toi depuis ton téléphone : scanne un QR code, puis "
                                        "connecte-toi sur la vraie page Amazon."),
                                     15, ui::Muted());
    phone_hint->setMarginBottom(20);
    card->addView(phone_hint);

    auto* store_button = MakeCardButton(Tr("Changer de boutique") + "  ·  amazon." + market.domain, false);
    store_button->setMarginBottom(20);
    store_button->registerClickAction([this](brls::View*) {
        auto alive = alive_;
        brls::sync([this, alive]() {
            if (*alive)
                ShowStorePicker();
        });
        return true;
    });
    card->addView(store_button);

    card->addView(ui::MakeLabel(CookieHelpText(), 14, ui::Opacity(ui::Muted(), 0.8f)));
    content->addView(card);
    first_card_ = button;
    brls::Application::giveFocus(button);
}

void LibraryView::SignInWithAmazon()
{
    if (loading_)
        return;
    luna::PendingSignIn pending;
    std::string final_url;
    try
    {
        pending = luna::AmazonAuth::BeginSignIn(LoadStreamSettings().amazon_market);
        final_url = OpenAmazonWebSignIn(pending);
    }
    catch (const std::exception& ex)
    {
        ShowSignIn(ex.what());
        return;
    }
    const std::string code = luna::AmazonAuth::ExtractAuthorizationCode(final_url);
    if (code.empty())
    {
        ShowSignIn(final_url.empty() ? Tr("Connexion annulée.") : Tr("Amazon n'a pas renvoyé de code de connexion."));
        return;
    }
    CompleteSignIn(pending, code);
}

void LibraryView::CompleteSignIn(const luna::PendingSignIn& pending, const std::string& code)
{
    loading_ = true;
    SetStatus(Tr("Enregistrement de la console sur ton compte…"));
    auto alive = alive_;
    brls::async([this, alive, pending, code]() {
        std::string error;
        try
        {
            const auto registration = luna::AmazonAuth::Register(pending, code);
            if (!luna::AmazonAuth::Save(registration))
                error = Tr("Impossible d'enregistrer la session sur la carte SD.");
        }
        catch (const std::exception& ex)
        {
            error = ex.what();
        }
        brls::sync([this, alive, error]() {
            if (!*alive)
                return;
            loading_ = false;
            if (!error.empty())
            {
                SetStatus("");
                ShowSignIn(error);
                return;
            }
            Reload();
        });
    });
}

void LibraryView::StopPhoneSignIn()
{
    if (!phone_server_)
        return;
    phone_server_->Stop();
    phone_server_.reset();
}

void LibraryView::SignInWithPhone()
{
    if (loading_)
        return;
    StopPhoneSignIn();

    luna::PendingSignIn pending;
    try
    {
        pending = luna::AmazonAuth::BeginSignIn(LoadStreamSettings().amazon_market);
    }
    catch (const std::exception& ex)
    {
        ShowSignIn(ex.what());
        return;
    }

    // Le code arrive sur le fil du serveur : la suite se fait sur le fil de l'interface.
    auto alive = alive_;
    auto server = std::make_unique<PhoneSignInServer>(pending, [this, alive, pending](const std::string& code) {
        brls::sync([this, alive, pending, code]() {
            if (!*alive)
                return;
            ShowMessage(Tr("Connexion à ton compte Amazon"), Tr("Enregistrement de la console sur ton compte…"));
            CompleteSignIn(pending, code);
        });
    });
    std::string error;
    if (!server->Start(error))
    {
        ShowSignIn(error);
        return;
    }
    const std::string address = server->Address();

    auto* content = ResetScroller();
    phone_server_ = std::move(server);

    auto* card = ui::MakeCard();
    card->setMarginTop(10);
    auto* heading = ui::MakeLabel(Tr("Connexion avec ton téléphone"), 26, ui::Text(), ui::FontRole::Display);
    heading->setMarginBottom(18);
    card->addView(heading);

    auto* row = new brls::Box(brls::Axis::ROW);
    row->setAlignItems(brls::AlignItems::CENTER);
    auto* qr = new QrCodeView(address, 250);
    qr->setMarginRight(32);
    qr->setShrink(0);
    row->addView(qr);

    auto* steps = new brls::Box(brls::Axis::COLUMN);
    steps->setGrow(1.0f);
    steps->setShrink(1.0f);
    steps->addView(ui::MakeLabel(
        TrFormat("1. Connecte ton téléphone au même réseau Wi-Fi que la Switch.\n"
                 "2. Scanne ce QR code avec l'appareil photo, ou ouvre cette adresse :\n{}\n"
                 "3. Touche « Ouvrir la page de connexion Amazon » et connecte-toi.\n"
                 "4. Copie l'adresse de la dernière page affichée, reviens sur la page LunaNX et colle-la.",
                 {address}),
        18, ui::Text()));
    auto* waiting = ui::MakeLabel(Tr("En attente de ton téléphone…"), 17, ui::Accent(), ui::FontRole::Heading);
    waiting->setMarginTop(18);
    steps->addView(waiting);
    row->addView(steps);
    card->addView(row);

    auto* cancel = MakeCardButton(Tr("Annuler"), false);
    cancel->setMarginTop(24);
    cancel->registerClickAction([this](brls::View*) {
        auto alive = alive_;
        brls::sync([this, alive]() {
            if (*alive)
                ShowSignIn("");
        });
        return true;
    });
    card->addView(cancel);

    content->addView(card);
    first_card_ = cancel;
    brls::Application::giveFocus(cancel);
}

// ---------------------------------------------------------------------------------------------
// Bibliothèque

void LibraryView::ShowCatalog(const std::vector<luna::Shelf>& shelves)
{
    if (&shelves != &shelves_)
        shelves_ = shelves;
    recent_changed_ = false;

    catalog_.clear();
    catalog_index_.clear();
    for (const auto& shelf : shelves_)
        for (const auto& game : shelf.games)
            AddToCatalog(catalog_, catalog_index_, game);
    if (catalog_.empty())
    {
        rendered_layout_.clear();
        ShowMessage(Tr("Aucun jeu trouvé"), Tr("Le catalogue Luna n'a renvoyé aucun jeu.\n\nVérifie ton abonnement Luna, "
                                              "puis appuie sur Y pour réessayer."));
        return;
    }
    premium_count_ = static_cast<size_t>(std::count_if(catalog_.begin(), catalog_.end(),
                                                       [](const luna::Game& game) { return game.premium_only; }));
    const auto featured = PickFeaturedGame(shelves_);
    featured_id_ = featured ? GameId(Fresh(*featured, catalog_, catalog_index_)) : std::string();
    LogRuntimeEvent("library", "catalog",
                    "games=" + std::to_string(catalog_.size()) + " premium=" + std::to_string(premium_count_) +
                        " plan=" + (plan_ == luna::AccountPlan::Premium    ? "premium"
                                    : plan_ == luna::AccountPlan::Standard ? "standard"
                                                                           : "unknown"));

    rendered_layout_ = LoadStreamSettings().library_layout;
    if (rendered_layout_ == "List")
    {
        ShowGameList();
        return;
    }
    StopPhoneSignIn();
    if (section_ == Section::Premium && premium_count_ == 0)
        section_ = Section::All;
    ShowLibraryPanel(true);
    RebuildSectionGames();
    RefreshCarousel(true);
    // Focus donné au carrousel avant de supprimer la carte de chargement qui l'avait.
    brls::Application::giveFocus(getDefaultFocus());
    content_->clearViews();
    first_card_ = nullptr;
    loading_label_ = nullptr;
    list_heading_ = nullptr;
}

void LibraryView::ShowGameList()
{
    // Liste simple : un jeu par ligne, par ordre alphabétique (jeux de l'abonnement du compte).
    auto* content = ResetScroller();
    std::vector<luna::Game> games;
    for (const auto& game : catalog_)
        if (luna::IsIncludedInPlan(game, plan_))
            games.push_back(game);
    std::sort(games.begin(), games.end(), [](const luna::Game& a, const luna::Game& b) { return a.title < b.title; });
    list_heading_ = ui::MakeLabel(TrFormat("{} jeux disponibles", {std::to_string(games.size())}), 21, ui::Text(),
                                  ui::FontRole::Heading);
    list_heading_->setMarginBottom(8);
    content->addView(list_heading_);
    for (const auto& game : games)
        AddListCell(game);
    if (first_card_)
        brls::Application::giveFocus(first_card_);
}

void LibraryView::AddListCell(const luna::Game& game)
{
    auto* cell = new brls::DetailCell();
    cell->setText(game.title);
    cell->setDetailText(game.premium_only ? "Luna Premium" : Tr("Jouer"));
    cell->setDetailTextColor(game.premium_only ? ui::Premium() : ui::Accent());
    cell->registerClickAction([this, game](brls::View*) {
        Launch(game);
        return true;
    });
    if (!first_card_)
        first_card_ = cell;
    content_->addView(cell);
}

bool LibraryView::IsLocked(const luna::Game& game) const
{
    return !luna::IsIncludedInPlan(game, plan_);
}

void LibraryView::RebuildSectionGames()
{
    section_games_.clear();
    switch (section_)
    {
        case Section::All:
            for (const auto& game : catalog_)
                if (luna::IsIncludedInPlan(game, plan_))
                    section_games_.push_back(game);
            // Jeu « À la une » en tête : c'est lui qui accueille l'utilisateur.
            if (!featured_id_.empty())
            {
                const auto found = std::find_if(section_games_.begin(), section_games_.end(),
                                                [this](const luna::Game& game) { return GameId(game) == featured_id_; });
                if (found != section_games_.end())
                    std::rotate(section_games_.begin(), found, found + 1);
            }
            break;
        case Section::Premium:
            for (const auto& game : catalog_)
                if (game.premium_only)
                    section_games_.push_back(game);
            break;
        case Section::Top:
            for (const auto& game : PickTopGames(shelves_))
                section_games_.push_back(Fresh(game, catalog_, catalog_index_));
            break;
        case Section::Recent:
            for (const auto& game : PickRecentGames(shelves_))
                section_games_.push_back(Fresh(game, catalog_, catalog_index_));
            break;
    }
    if (!search_query_.empty())
    {
        const std::string query = Lowercase(search_query_);
        section_games_.erase(std::remove_if(section_games_.begin(), section_games_.end(),
                                            [&query](const luna::Game& game) {
                                                return Lowercase(game.title).find(query) == std::string::npos;
                                            }),
                             section_games_.end());
    }
}

void LibraryView::RefreshCarousel(bool keep_selection)
{
    const size_t pages = std::max<size_t>(1, (section_games_.size() + kCarouselPageSize - 1) / kCarouselPageSize);
    page_ = std::min(page_, pages - 1);
    const size_t first = page_ * kCarouselPageSize;
    const size_t last = std::min(first + kCarouselPageSize, section_games_.size());
    std::vector<CarouselItem> items;
    for (size_t i = first; i < last; ++i)
    {
        const luna::Game& game = section_games_[i];
        CarouselItem item;
        item.id = GameId(game);
        item.title = game.title;
        item.image_url = !game.image_portrait.empty() ? game.image_portrait : game.image_landscape;
        item.rank = section_ == Section::Top && search_query_.empty() ? static_cast<int>(i + 1) : 0;
        item.premium = game.premium_only;
        item.locked = IsLocked(game);
        items.push_back(std::move(item));
    }
    const bool empty = items.empty();
    if (empty && brls::Application::getCurrentFocus() == carousel_)
        brls::Application::giveFocus((all_button_->getVisibility() == brls::Visibility::VISIBLE) ? all_button_ : menu_button_);
    carousel_->setVisibility(empty ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    info_->setVisibility(empty ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    empty_label_->setVisibility(empty ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (empty)
    {
        selected_id_.clear();
        empty_label_->setText(!search_query_.empty()            ? Tr("Aucun jeu ne correspond à ta recherche.")
                              : section_ == Section::Recent ? Tr("Aucune partie récente pour l'instant : lance un jeu pour le retrouver ici.")
                                                            : Tr("Aucun jeu dans cette rubrique."));
        ResetBackdrop();
    }
    carousel_->SetGames(std::move(items), keep_selection ? selected_id_ : std::string(), page_ + 1 < pages, page_ > 0);
    UpdateSectionHeader();
    UpdateHeaderButtons();
}

void LibraryView::UpdateSectionHeader()
{
    std::string title;
    switch (section_)
    {
        case Section::All: title = Tr("Tous les jeux"); break;
        case Section::Recent: title = Tr("Reprendre"); break;
        case Section::Top: title = "Top 10"; break;
        case Section::Premium: title = "Luna Premium"; break;
    }
    section_title_->setText(title);
    std::string subtitle = TrFormat("{} jeux", {std::to_string(section_games_.size())});
    if (section_ == Section::All && plan_ == luna::AccountPlan::Standard)
        subtitle += "  ·  " + Tr("Inclus dans ton abonnement");
    if (section_ == Section::Premium && plan_ == luna::AccountPlan::Standard)
        subtitle += "  ·  " + Tr("Nécessite Luna Premium");
    if (!search_query_.empty())
        subtitle += "  ·  " + TrFormat("Recherche : « {} »", {search_query_});
    section_subtitle_->setText(subtitle);
    const size_t pages = std::max<size_t>(1, (section_games_.size() + kCarouselPageSize - 1) / kCarouselPageSize);
    page_label_->setText(pages > 1 ? TrFormat("Page {} / {}", {std::to_string(page_ + 1), std::to_string(pages)}) +
                                         "   ·   L / R"
                                   : std::string());
}

void LibraryView::SelectSection(Section section)
{
    if (catalog_.empty() || rendered_layout_ == "List")
        return;
    if (section == Section::Premium && premium_count_ == 0)
        section = Section::All;
    const bool changed = section != section_;
    section_ = section;
    if (changed)
    {
        page_ = 0;
        selected_id_.clear();
        search_query_.clear();
    }
    RebuildSectionGames();
    RefreshCarousel(!changed);
    if (carousel_->ItemCount())
        brls::Application::giveFocus(carousel_);
}

void LibraryView::CycleSection(int delta)
{
    if (catalog_.empty() || rendered_layout_ == "List" || !(library_panel_->getVisibility() == brls::Visibility::VISIBLE))
        return;
    std::vector<Section> order {Section::All, Section::Recent};
    if ((top_button_->getVisibility() == brls::Visibility::VISIBLE))
        order.push_back(Section::Top);
    if (premium_count_ > 0)
        order.push_back(Section::Premium);
    auto current = std::find(order.begin(), order.end(), section_);
    const long index = current == order.end() ? 0 : static_cast<long>(current - order.begin());
    const long size = static_cast<long>(order.size());
    SelectSection(order[static_cast<size_t>(((index + delta) % size + size) % size)]);
}

void LibraryView::ChangePage(int delta)
{
    if (!(library_panel_->getVisibility() == brls::Visibility::VISIBLE))
        return;
    const size_t pages = (section_games_.size() + kCarouselPageSize - 1) / kCarouselPageSize;
    if (pages < 2)
        return;
    const long next = std::clamp(static_cast<long>(page_) + delta, 0L, static_cast<long>(pages) - 1);
    if (static_cast<size_t>(next) == page_)
        return;
    page_ = static_cast<size_t>(next);
    selected_id_.clear();
    // Page précédente : on arrive sur son dernier jeu, comme en faisant défiler.
    if (delta < 0)
        selected_id_ = GameId(section_games_[std::min(section_games_.size(), (page_ + 1) * kCarouselPageSize) - 1]);
    RefreshCarousel(true);
    brls::Application::giveFocus(carousel_);
}

const luna::Game* LibraryView::FindSectionGame(const std::string& id) const
{
    const auto found = std::find_if(section_games_.begin(), section_games_.end(),
                                    [&id](const luna::Game& game) { return GameId(game) == id; });
    return found == section_games_.end() ? nullptr : &*found;
}

void LibraryView::SelectGame(const std::string& id)
{
    selected_id_ = id;
    const luna::Game* game = FindSectionGame(id);
    if (!game)
    {
        game_title_->setText({});
        game_meta_->setText({});
        game_offer_->setText({});
        ResetBackdrop();
        return;
    }
    game_title_->setText(game->title);

    std::string meta;
    auto append = [&meta](const std::string& part) {
        if (!part.empty())
            meta += (meta.empty() ? "" : "  ·  ") + part;
    };
    append(game->publisher);
    for (size_t i = 0; i < game->genres.size() && i < 2; ++i)
        append(game->genres[i]);
    append(game->age_rating);
    game_meta_->setText(meta);

    std::string offer;
    NVGcolor color = ui::Included();
    if (game->premium_only)
    {
        const bool locked = IsLocked(*game);
        offer = locked ? Tr("Nécessite Luna Premium") : Tr("Inclus avec Luna Premium");
        color = ui::Premium();
    }
    else if (plan_ != luna::AccountPlan::Unknown)
    {
        offer = Tr("Inclus dans ton abonnement");
    }
    else if (!game->offer_tag.empty())
    {
        offer = game->offer_tag;
        color = ui::Accent();
    }
    game_offer_->setText(offer);
    game_offer_->setTextColor(color);
    game_offer_->setVisibility(offer.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    play_button_->SetIcon(IsLocked(*game) ? ui::Icon::Lock : ui::Icon::Play);
    backdrop_url_ = BackdropUrl(*game);
    backdrop_requested_at_ = brls::getCPUTimeUsec();
    backdrop_pending_ = true;
}

void LibraryView::BeginSearch()
{
    if (catalog_.empty())
        return;
    auto alive = alive_;
    brls::Application::getImeManager()->openForText(
        [this, alive](std::string text) {
            if (!*alive)
                return;
            search_query_ = std::move(text);
            page_ = 0;
            selected_id_.clear();
            RebuildSectionGames();
            RefreshCarousel(false);
            brls::Application::giveFocus(getDefaultFocus());
        },
        Tr("Rechercher un jeu"), Tr("Saisis le titre d'un jeu"), 64, search_query_);
}

void LibraryView::ClearSearch()
{
    search_query_.clear();
    page_ = 0;
    RebuildSectionGames();
    RefreshCarousel(true);
    brls::Application::giveFocus(getDefaultFocus());
}

void LibraryView::ShowMainMenu()
{
    std::vector<std::string> labels;
    std::vector<std::function<void()>> actions;
    if (!catalog_.empty() && rendered_layout_ != "List")
    {
        labels.push_back(Tr("Tous les jeux"));
        actions.push_back([this] { SelectSection(Section::All); });
        labels.push_back(Tr("Reprendre"));
        actions.push_back([this] { SelectSection(Section::Recent); });
        if ((top_button_->getVisibility() == brls::Visibility::VISIBLE))
        {
            labels.push_back("Top 10");
            actions.push_back([this] { SelectSection(Section::Top); });
        }
        if (premium_count_ > 0)
        {
            labels.push_back("Luna Premium");
            actions.push_back([this] { SelectSection(Section::Premium); });
        }
        labels.push_back(Tr("Rechercher"));
        actions.push_back([this] { BeginSearch(); });
    }
    labels.push_back(Tr("Actualiser"));
    actions.push_back([this] { Reload(); });
    labels.push_back(Tr("Compte"));
    actions.push_back([this] { ShowAccount(); });
    labels.push_back(Tr("Réglages"));
    actions.push_back([] { brls::Application::pushActivity(new brls::Activity(new SettingsView())); });
    labels.push_back(Tr("Quitter"));
    actions.push_back([] { brls::Application::quit(); });

    auto alive = alive_;
    auto* dropdown = new brls::Dropdown(Tr("Menu principal"), labels, [](int) {}, 0,
                                        [alive, actions](int choice) {
                                            if (!*alive || choice < 0 || choice >= static_cast<int>(actions.size()))
                                                return;
                                            // Après la fermeture du menu, pour que le focus revienne d'abord.
                                            const auto action = actions[static_cast<size_t>(choice)];
                                            brls::sync([alive, action] {
                                                if (*alive)
                                                    action();
                                            });
                                        });
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void LibraryView::ShowAccount()
{
    const std::string market_code = LoadStreamSettings().amazon_market;
    std::string text = Tr("Ton compte");
    if (!market_code.empty())
        text += "\n\n" + TrFormat("Boutique : {}", {"amazon." + luna::FindAmazonMarket(market_code).domain});
    if (!client_)
        text += "\n" + Tr("Non connecté");
    else
    {
        std::string plan = Tr("Inconnu (tous les jeux sont affichés)");
        if (plan_ == luna::AccountPlan::Premium)
            plan = "Luna Premium";
        else if (plan_ == luna::AccountPlan::Standard)
            plan = plan_label_.empty() ? "Prime" : plan_label_;
        text += "\n" + TrFormat("Abonnement Luna : {}", {plan});
        text += "\n" + TrFormat("{} jeux disponibles", {std::to_string(std::count_if(
                                    catalog_.begin(), catalog_.end(),
                                    [this](const luna::Game& game) { return luna::IsIncludedInPlan(game, plan_); }))});
    }
    auto* dialog = new brls::Dialog(text);
    dialog->addButton(Tr("Fermer"), [] {});
    dialog->addButton(Tr("Réglages"), [] {
        brls::sync([] { brls::Application::pushActivity(new brls::Activity(new SettingsView())); });
    });
    dialog->setCancelable(true);
    dialog->open();
}

void LibraryView::PlaySelected()
{
    const luna::Game* selected = FindSectionGame(selected_id_);
    if (!selected)
        return;
    const luna::Game game = *selected;
    if (!IsLocked(game))
    {
        Launch(game);
        return;
    }
    // Jeu Luna Premium sur un compte sans Premium : on prévient, sans interdire (la détection
    // repose sur les textes de Luna et le lancement dira de toute façon si le jeu est refusé).
    auto alive = alive_;
    auto* dialog = new brls::Dialog(Tr("Ce jeu nécessite Luna Premium") + "\n\n" +
                                    Tr("D'après Luna, ce jeu n'est pas inclus dans ton abonnement actuel. Tu peux "
                                       "quand même essayer de le lancer."));
    dialog->addButton(Tr("Annuler"), [] {});
    dialog->addButton(Tr("Essayer quand même"), [this, alive, game] {
        brls::sync([this, alive, game] {
            if (*alive)
                Launch(game);
        });
    });
    dialog->setCancelable(true);
    dialog->open();
}

void LibraryView::StartCatalogExploration()
{
    if (!client_ || shelves_.empty())
        return;
    std::set<std::string> known;
    for (const auto& game : catalog_)
        luna::InsertGameKeys(known, game);

    exploration_done_ = false;
    const uint64_t generation = explore_generation_.load();
    SetStatus(Tr("Recherche d'autres jeux…"));
    auto alive = alive_;
    auto client = client_;
    brls::async([this, alive, client, known, generation]() {
        client->ExploreCatalog(
            known,
            [this, alive, generation](std::vector<luna::Game> games) {
                brls::sync([this, alive, generation, games = std::move(games)]() mutable {
                    if (*alive && generation == explore_generation_.load())
                        AddDiscoveredGames(std::move(games));
                });
            },
            [this, alive, generation]() { return *alive && generation == explore_generation_.load(); });
        brls::sync([this, alive, generation]() {
            if (*alive && generation == explore_generation_.load())
            {
                exploration_done_ = true;
                SetStatus("");
            }
        });
    });
}

void LibraryView::AddDiscoveredGames(std::vector<luna::Game> games)
{
    // Gardés avec le catalogue : ils réapparaissent si l'écran est redessiné.
    shelves_.push_back(luna::Shelf {"", "EXPLORED", games});
    std::vector<luna::Game> shown;
    for (const auto& game : games)
        if (AddToCatalog(catalog_, catalog_index_, game))
            shown.push_back(game);
    if (shown.empty())
        return;
    premium_count_ = static_cast<size_t>(std::count_if(catalog_.begin(), catalog_.end(),
                                                       [](const luna::Game& game) { return game.premium_only; }));
    if (rendered_layout_ == "List")
    {
        if (!list_heading_)
            return;
        for (const auto& game : shown)
            if (luna::IsIncludedInPlan(game, plan_))
                AddListCell(game);
        const size_t count = static_cast<size_t>(std::count_if(
            catalog_.begin(), catalog_.end(), [this](const luna::Game& game) { return luna::IsIncludedInPlan(game, plan_); }));
        list_heading_->setText(TrFormat("{} jeux disponibles", {std::to_string(count)}));
        return;
    }
    if (!(library_panel_->getVisibility() == brls::Visibility::VISIBLE))
        return;
    if (section_ != Section::All && section_ != Section::Premium)
    {
        UpdateHeaderButtons();
        return;
    }
    // Ajoutés à la fin de la rubrique : la page affichée n'est redessinée que si elle change.
    const size_t old_pages = (section_games_.size() + kCarouselPageSize - 1) / kCarouselPageSize;
    const bool page_was_full = (page_ + 1) * kCarouselPageSize <= section_games_.size();
    RebuildSectionGames();
    const size_t new_pages = (section_games_.size() + kCarouselPageSize - 1) / kCarouselPageSize;
    if (!page_was_full || old_pages != new_pages)
        RefreshCarousel(true);
    else
    {
        UpdateSectionHeader();
        UpdateHeaderButtons();
    }
}

void LibraryView::OnResume()
{
    // Déconnexion depuis les réglages : retour à l'écran de connexion, après le retour du focus.
    if (client_ && !loading_ && !HasStoredAccount())
    {
        if (UiLanguage() != language_)
            RefreshTexts();
        auto alive = alive_;
        brls::sync([this, alive]() {
            if (*alive)
                ShowSignedOut();
        });
        return;
    }
    // Langue changée dans les réglages : textes de l'écran, puis catalogue dans cette langue.
    if (UiLanguage() != language_)
    {
        RefreshTexts();
        auto alive = alive_;
        brls::sync([this, alive]() {
            if (*alive)
                Reload();
        });
        return;
    }
    // Redessine après le retour du focus par Borealis, jamais pendant.
    const bool layout_changed = !rendered_layout_.empty() && LoadStreamSettings().library_layout != rendered_layout_;
    if (shelves_.empty() || loading_ || (!recent_changed_ && !layout_changed))
        return;
    auto alive = alive_;
    brls::sync([this, alive]() {
        if (!*alive || loading_)
            return;
        ShowCatalog(shelves_);
        // Exploration interrompue par une partie : elle reprend (les jeux déjà trouvés restent).
        if (!exploration_done_)
            StartCatalogExploration();
    });
}

void LibraryView::Launch(const luna::Game& game)
{
    if (!client_)
        return;
    LogRuntimeEvent("library", "launch", game.title + (game.premium_only ? " (premium)" : ""));
    // Pas d'exploration du catalogue pendant une partie : elle reprendra au retour.
    ++explore_generation_;
    luna::RecordRecentGame(game);
    recent_changed_ = true;
    brls::Application::pushActivity(new brls::Activity(new StreamView(client_, game)),
                                    brls::TransitionAnimation::FADE);
}

} // namespace lunanx

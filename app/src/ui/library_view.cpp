#include "library_view.hpp"

#include "amazon_sign_in.hpp"
#include "core/app_paths.hpp"
#include "core/i18n.hpp"
#include "core/stream_settings.hpp"
#include "core/runtime_journal.hpp"
#include "hero_banner.hpp"
#include "luna/recent_games.hpp"
#include "poster_card.hpp"
#include "settings_view.hpp"
#include "stream_view.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <set>

namespace lunanx
{
namespace
{


brls::Label* MakeLabel(const std::string& text, float size, NVGcolor color)
{
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(size);
    label->setTextColor(color);
    return label;
}

std::string CookieHelpText()
{
    return TrFormat("Solution de secours si la connexion directe échoue : exporte une seule fois la "
                    "session de ton navigateur.\n1. Sur ton PC, connecte-toi sur luna.amazon.fr.\n"
                    "2. Avec l'extension « Get cookies.txt LOCALLY », clique sur « Export » (fichier "
                    "cookies.txt).\n3. Copie ce fichier dans {}/ puis appuie sur Y.\nATTENTION : ce "
                    "fichier donne un accès complet à ton compte Amazon. Ne le partage jamais.",
                    {AppHomePath()});
}

constexpr size_t kGamesPerPage = 10;
constexpr size_t kGridColumns = 5;
constexpr float kGridCardWidth = 212.0f;
constexpr float kRowCardWidth = 170.0f;

std::string GameKey(const luna::Game& game)
{
    return !game.asin.empty() ? game.asin : game.title;
}

std::string Lowercase(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// Tous les jeux du catalogue, sans doublon, dans l'ordre des rangées Luna.
std::vector<luna::Game> UniqueGames(const std::vector<luna::Shelf>& shelves)
{
    std::vector<luna::Game> games;
    std::set<std::string> seen;
    for (const auto& shelf : shelves)
        for (const auto& game : shelf.games)
            if (!game.title.empty() && seen.insert(GameKey(game)).second)
                games.push_back(game);
    return games;
}

void AppendUnique(std::vector<luna::Game>& out, std::set<std::string>& seen, const luna::Game& game)
{
    if (out.size() < kGamesPerPage && !game.title.empty() && seen.insert(GameKey(game)).second)
        out.push_back(game);
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
            AppendUnique(top, seen, game);
    return top;
}

// « Reprendre » : historique local d'abord (avec les données fraîches du catalogue), puis
// une éventuelle rangée Luna de jeux récents.
std::vector<luna::Game> PickRecentGames(const std::vector<luna::Shelf>& shelves,
                                        const std::vector<luna::Game>& catalog)
{
    std::vector<luna::Game> recent;
    std::set<std::string> seen;
    for (const auto& played : luna::LoadRecentGames())
    {
        const auto match = std::find_if(catalog.begin(), catalog.end(), [&played](const luna::Game& game) {
            return (!played.asin.empty() && game.asin == played.asin) || game.title == played.title;
        });
        if (match != catalog.end())
            AppendUnique(recent, seen, *match);
        else if (!played.page_uri.empty())
            AppendUnique(recent, seen, played);
    }
    for (const auto& shelf : shelves)
    {
        const std::string title = Lowercase(shelf.title);
        for (const char* keyword : {"cemment", "reprendre", "continuer", "nouveau", "recent"})
            if (title.find(keyword) != std::string::npos)
            {
                for (const auto& game : shelf.games)
                    AppendUnique(recent, seen, game);
                break;
            }
    }
    return recent;
}

// Jeu mis en avant : premier visuel « À la une » de Luna, sinon le n°1 du classement.
std::optional<luna::Game> PickFeaturedGame(const std::vector<luna::Shelf>& shelves,
                                           const std::vector<luna::Game>& top)
{
    for (const auto& shelf : shelves)
        if (shelf.row_type.find("FEATURED") != std::string::npos)
            for (const auto& game : shelf.games)
                if (!game.image_landscape.empty() && !game.title.empty())
                    return game;
    for (const auto& game : top)
        if (!game.image_landscape.empty())
            return game;
    return std::nullopt;
}

brls::Box* SectionHeader(const std::string& title, const std::string& subtitle)
{
    auto* header = new brls::Box(brls::Axis::ROW);
    header->setAlignItems(brls::AlignItems::FLEX_END);
    header->setMarginBottom(12);
    auto* label = MakeLabel(title, 26, nvgRGB(245, 246, 248));
    label->setMarginRight(14);
    header->addView(label);
    if (!subtitle.empty())
        header->addView(MakeLabel(subtitle, 16, nvgRGB(130, 134, 150)));
    return header;
}

} // namespace

LibraryView::LibraryView()
    : brls::Box(brls::Axis::COLUMN)
{
    setGrow(1.0f);
    setBackgroundColor(nvgRGB(12, 11, 18));
    setPadding(26, 40, 10, 40);

    auto* header = new brls::Box(brls::Axis::ROW);
    header->setAlignItems(brls::AlignItems::CENTER);
    header->setMarginBottom(14);
    auto* logo = new brls::Image();
    logo->setImageFromRes("img/lunanx_logo.png");
    logo->setWidth(52);
    logo->setHeight(52);
    logo->setCornerRadius(12);
    logo->setMarginRight(12);
    header->addView(logo);
    auto* title = MakeLabel("LunaNX", 32, nvgRGB(176, 150, 255));
    title->setMarginRight(18);
    header->addView(title);
    status_label_ = MakeLabel("", 17, nvgRGB(150, 154, 166));
    status_label_->setGrow(1.0f);
    header->addView(status_label_);

    // Boutons cliquables (tactile ou manette) en plus des raccourcis X / Y.
    auto make_button = [header](const std::string& text, std::function<void()> action) {
        auto* button = new brls::Button();
        button->setText(text);
        button->setStyle(&brls::BUTTONSTYLE_BORDERED);
        button->setMarginLeft(12);
        button->setHeight(44);
        button->registerClickAction([action](brls::View*) {
            action();
            return true;
        });
        header->addView(button);
        return button;
    };
    make_button(Tr("Actualiser"), [this] { Reload(); });
    make_button(Tr("Réglages"), [] {
        brls::Application::pushActivity(new brls::Activity(new SettingsView()));
    });
    addView(header);

    scroller_ = new brls::ScrollingFrame();
    scroller_->setGrow(1.0f);
    scroller_->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    content_ = new brls::Box(brls::Axis::COLUMN);
    scroller_->setContentView(content_);
    addView(scroller_);

    registerAction(Tr("Actualiser"), brls::BUTTON_Y, [this](brls::View*) {
        Reload();
        return true;
    });
    registerAction(Tr("Réglages"), brls::BUTTON_X, [](brls::View*) {
        brls::Application::pushActivity(new brls::Activity(new SettingsView()));
        return true;
    });
    registerAction(Tr("Quitter"), brls::BUTTON_START, [](brls::View*) {
        brls::Application::quit();
        return true;
    });

    Reload();
}

LibraryView::~LibraryView()
{
    *alive_ = false;
}

brls::View* LibraryView::getDefaultFocus()
{
    return first_card_ ? first_card_ : brls::Box::getDefaultFocus();
}

void LibraryView::ShowMessage(const std::string& title, const std::string& body)
{
    content_->clearViews();
    first_card_ = nullptr;
    auto* card = new brls::Box(brls::Axis::COLUMN);
    card->setPadding(28, 32, 28, 32);
    card->setCornerRadius(14);
    card->setBackgroundColor(nvgRGB(24, 22, 34));
    card->setFocusable(true);
    card->setHideHighlight(true);
    auto* heading = MakeLabel(title, 24, nvgRGB(245, 246, 248));
    heading->setMarginBottom(14);
    card->addView(heading);
    card->addView(MakeLabel(body, 18, nvgRGB(200, 204, 212)));
    content_->addView(card);
    brls::Application::giveFocus(card);
}

void LibraryView::Reload()
{
    if (loading_)
        return;
    loading_ = true;
    status_label_->setText(Tr("Connexion…"));

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
                        status_label_->setText(status);
                });
            });
            brls::sync([this, alive] {
                if (*alive)
                    status_label_->setText(Tr("Chargement du catalogue…"));
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
            sign_in_reason = has_cookies
                ? TrFormat("Ta session Amazon a expiré ({}). Reconnecte-toi.", {ex.what()})
                : std::string();
            error_title = "sign-in";
        }
        catch (const std::exception& ex)
        {
            LogRuntimeEvent("library", "load_failed", ex.what());
            error_title = Tr("Impossible de charger Luna");
            error_body = TrFormat("{}\n\nVérifie la connexion Internet puis appuie sur Y.", {ex.what()});
        }

        brls::sync([this, alive, client, shelves = std::move(shelves), error_title, error_body,
                    sign_in_reason]() {
            if (!*alive)
                return;
            loading_ = false;
            if (error_title == "sign-in")
            {
                status_label_->setText("");
                ShowSignIn(sign_in_reason);
                return;
            }
            if (!error_title.empty())
            {
                status_label_->setText("");
                ShowMessage(error_title, error_body);
                return;
            }
            client_ = client;
            status_label_->setText(client_->account().profile_token.empty()
                                       ? Tr("Connecté")
                                       : Tr("Connecté · profil Luna actif"));
            ShowShelves(shelves);
        });
    });
}

void LibraryView::ShowSignIn(const std::string& reason)
{
    content_->clearViews();
    first_card_ = nullptr;
    const auto& market = luna::FindAmazonMarket(LoadStreamSettings().amazon_market);

    auto* card = new brls::Box(brls::Axis::COLUMN);
    card->setPadding(28, 32, 28, 32);
    card->setCornerRadius(14);
    card->setBackgroundColor(nvgRGB(24, 22, 34));

    auto* heading = MakeLabel(Tr("Connexion à ton compte Amazon"), 24, nvgRGB(245, 246, 248));
    heading->setMarginBottom(12);
    card->addView(heading);
    if (!reason.empty())
    {
        auto* warning = MakeLabel(reason, 17, nvgRGB(255, 170, 120));
        warning->setMarginBottom(10);
        card->addView(warning);
    }
    card->addView(MakeLabel(
        TrFormat("La page de connexion officielle d'Amazon ({}) va s'ouvrir dans le navigateur de la "
                 "Switch. Saisis ton e-mail, ton mot de passe et, si besoin, le code de validation reçu.\n"
                 "Ton mot de passe n'est jamais lu ni enregistré par LunaNX : la console est simplement "
                 "ajoutée à tes appareils Amazon (« LunaNX »), et tu peux la retirer à tout moment depuis "
                 "ton compte ou dans les Réglages.\nCompte d'un autre pays ? Change-le dans les Réglages (X).",
                 {Tr(market.label)}),
        17, nvgRGB(200, 204, 212)));

    auto* button = new brls::Button();
    button->setText(Tr("Se connecter avec Amazon"));
    button->setStyle(&brls::BUTTONSTYLE_PRIMARY);
    button->setMarginTop(20);
    button->setMarginBottom(20);
    button->registerClickAction([this](brls::View*) {
        SignInWithAmazon();
        return true;
    });
    card->addView(button);

    card->addView(MakeLabel(CookieHelpText(), 14, nvgRGB(130, 134, 145)));
    content_->addView(card);
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

    loading_ = true;
    status_label_->setText(Tr("Enregistrement de la console sur ton compte…"));
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
                status_label_->setText("");
                ShowSignIn(error);
                return;
            }
            Reload();
        });
    });
}

void LibraryView::ShowShelves(const std::vector<luna::Shelf>& shelves)
{
    if (&shelves != &shelves_)
        shelves_ = shelves;
    content_->clearViews();
    first_card_ = nullptr;
    grid_ = nullptr;
    more_button_ = nullptr;
    recent_changed_ = false;

    const std::vector<luna::Game> games = UniqueGames(shelves_);
    if (games.empty())
    {
        rendered_layout_.clear();
        ShowMessage(Tr("Aucun jeu trouvé"),
                    Tr("Le catalogue Luna n'a renvoyé aucun jeu.\n\nVérifie ton abonnement Luna, puis "
                       "appuie sur Y pour réessayer."));
        return;
    }

    rendered_layout_ = LoadStreamSettings().library_layout;
    if (rendered_layout_ == "List")
        ShowGameList(games);
    else
        ShowCovers(shelves_, games);
    if (first_card_)
        brls::Application::giveFocus(first_card_);
}

void LibraryView::ShowGameList(const std::vector<luna::Game>& catalog)
{
    // Liste simple : un jeu par ligne, par ordre alphabétique (affichage de la version 1.0).
    std::vector<luna::Game> games = catalog;
    std::sort(games.begin(), games.end(), [](const luna::Game& a, const luna::Game& b) {
        return a.title < b.title;
    });

    auto* heading = MakeLabel(TrFormat("{} jeux disponibles", {std::to_string(games.size())}), 21, nvgRGB(235, 236, 240));
    heading->setMarginBottom(8);
    content_->addView(heading);

    for (const auto& game : games)
    {
        auto* cell = new brls::DetailCell();
        cell->setText(game.title);
        cell->setDetailText(Tr("Jouer"));
        cell->setDetailTextColor(nvgRGB(176, 150, 255));
        cell->registerClickAction([this, game](brls::View*) {
            Launch(game);
            return true;
        });
        if (!first_card_)
            first_card_ = cell;
        content_->addView(cell);
    }
}

void LibraryView::ShowCovers(const std::vector<luna::Shelf>& shelves, const std::vector<luna::Game>& games)
{
    const std::vector<luna::Game> top = PickTopGames(shelves);
    const std::vector<luna::Game> recent = PickRecentGames(shelves, games);

    if (const auto featured = PickFeaturedGame(shelves, top))
    {
        auto* hero = new HeroBanner(*featured, [this, game = *featured]() { Launch(game); });
        content_->addView(hero);
        first_card_ = hero;
    }
    if (!recent.empty())
        AddRow(Tr("Reprendre"), Tr("Tes dernières parties"), recent, false);
    if (!top.empty())
        AddRow("Top 10", Tr("Les jeux du moment sur Luna"), top, true);

    content_->addView(SectionHeader(Tr("Tous les jeux"),
                                    TrFormat("{} jeux disponibles", {std::to_string(games.size())})));
    grid_games_ = games;
    grid_shown_ = 0;
    grid_ = new brls::Box(brls::Axis::COLUMN);
    content_->addView(grid_);

    more_button_ = new brls::Button();
    more_button_->setStyle(&brls::BUTTONSTYLE_BORDERED);
    more_button_->setMarginTop(6);
    more_button_->setMarginBottom(30);
    more_button_->registerClickAction([this](brls::View*) {
        ShowMoreGames(true);
        return true;
    });
    content_->addView(more_button_);
    ShowMoreGames(false);
}

void LibraryView::AddRow(const std::string& title, const std::string& subtitle,
                         const std::vector<luna::Game>& games, bool ranked)
{
    content_->addView(SectionHeader(title, subtitle));
    auto* frame = new brls::HScrollingFrame();
    frame->setWidthPercentage(100);
    frame->setHeight(kRowCardWidth * 4.0f / 3.0f + 46.0f);
    frame->setMarginBottom(20);
    frame->setScrollingIndicatorVisible(false);
    // Mode centré : une rangée sortie de l'écran propose toujours une jaquette au focus
    // (le mode naturel renvoyait le focus directement à l'en-tête).
    frame->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    auto* row = new brls::Box(brls::Axis::ROW);
    for (size_t i = 0; i < games.size(); ++i)
    {
        const luna::Game game = games[i];
        auto* card = new PosterCard(game, kRowCardWidth, ranked ? static_cast<int>(i + 1) : 0,
                                    [this, game]() { Launch(game); });
        if (!first_card_)
            first_card_ = card;
        row->addView(card);
    }
    frame->setContentView(row);
    content_->addView(frame);
}

void LibraryView::ShowMoreGames(bool focus_new_cards)
{
    if (!grid_ || !more_button_)
        return;
    // Les jaquettes ne sont créées (et téléchargées) que par paquets de 10.
    const size_t end = std::min(grid_shown_ + kGamesPerPage, grid_games_.size());
    brls::Box* row = nullptr;
    brls::View* first_new = nullptr;
    for (size_t i = grid_shown_; i < end; ++i)
    {
        if (!row || i % kGridColumns == 0)
        {
            row = new brls::Box(brls::Axis::ROW);
            grid_->addView(row);
        }
        const luna::Game game = grid_games_[i];
        auto* card = new PosterCard(game, kGridCardWidth, 0, [this, game]() { Launch(game); });
        row->addView(card);
        if (!first_new)
            first_new = card;
    }
    grid_shown_ = end;

    const size_t remaining = grid_games_.size() - grid_shown_;
    if (remaining == 0)
        more_button_->setVisibility(brls::Visibility::GONE);
    else
        more_button_->setText(TrFormat("Afficher {} jeux de plus  ({} restants)",
                                       {std::to_string(std::min(remaining, kGamesPerPage)),
                                        std::to_string(remaining)}));

    if (!first_card_)
        first_card_ = first_new;
    if (focus_new_cards && first_new)
        brls::Application::giveFocus(first_new);
}

void LibraryView::OnResume()
{
    // Redessine après le retour du focus par Borealis, jamais pendant.
    const bool layout_changed = !rendered_layout_.empty() &&
                                LoadStreamSettings().library_layout != rendered_layout_;
    if (shelves_.empty() || loading_ || (!recent_changed_ && !layout_changed))
        return;
    auto alive = alive_;
    brls::sync([this, alive]() {
        if (*alive && !loading_)
            ShowShelves(shelves_);
    });
}

void LibraryView::Launch(const luna::Game& game)
{
    if (!client_)
        return;
    LogRuntimeEvent("library", "launch", game.title);
    luna::RecordRecentGame(game);
    recent_changed_ = true;
    brls::Application::pushActivity(new brls::Activity(new StreamView(client_, game)),
                                    brls::TransitionAnimation::FADE);
}

} // namespace lunanx

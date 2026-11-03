#include "library_view.hpp"

#include "amazon_sign_in.hpp"
#include "core/app_paths.hpp"
#include "core/i18n.hpp"
#include "core/stream_settings.hpp"
#include "core/runtime_journal.hpp"
#include "hero_banner.hpp"
#include "luna/recent_games.hpp"
#include "phone_sign_in.hpp"
#include "poster_card.hpp"
#include "qr_code_view.hpp"
#include "settings_view.hpp"
#include "stream_view.hpp"

#include <algorithm>
#include <cctype>
#include <map>
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

std::string Lowercase(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// Ajoute le jeu au catalogue s'il n'y est pas encore (même ASIN ou même titre normalisé).
bool AddToCatalog(std::vector<luna::Game>& catalog, std::map<std::string, size_t>& index,
                  const luna::Game& game)
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
    for (const auto& key : keys)
        index.emplace(key, slot);
    return added;
}

void AppendUnique(std::vector<luna::Game>& out, std::set<std::string>& seen, const luna::Game& game)
{
    if (out.size() < kGamesPerPage && !game.title.empty() && luna::InsertGameKeys(seen, game))
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

brls::Box* SectionHeader(const std::string& title, const std::string& subtitle,
                         brls::Label** subtitle_label = nullptr)
{
    auto* header = new brls::Box(brls::Axis::ROW);
    header->setAlignItems(brls::AlignItems::FLEX_END);
    header->setMarginBottom(12);
    auto* label = MakeLabel(title, 26, nvgRGB(245, 246, 248));
    label->setMarginRight(14);
    header->addView(label);
    if (!subtitle.empty())
    {
        auto* sub = MakeLabel(subtitle, 16, nvgRGB(130, 134, 150));
        header->addView(sub);
        if (subtitle_label)
            *subtitle_label = sub;
    }
    return header;
}

} // namespace

LibraryView::LibraryView()
    : brls::Box(brls::Axis::COLUMN)
{
    setGrow(1.0f);
    setBackgroundColor(nvgRGB(12, 11, 18));
    setPadding(26, 40, 10, 40);
    language_ = UiLanguage();

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
    refresh_button_ = make_button(Tr("Actualiser"), [this] { Reload(); });
    settings_button_ = make_button(Tr("Réglages"), [] {
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
    registerAction(Tr("Page précédente"), brls::BUTTON_LB, [this](brls::View*) {
        if (grid_ && grid_page_ > 0)
            ShowGridPage(grid_page_ - 1, true);
        return true;
    });
    registerAction(Tr("Page suivante"), brls::BUTTON_RB, [this](brls::View*) {
        if (grid_ && (grid_page_ + 1) * kGamesPerPage < grid_games_.size())
            ShowGridPage(grid_page_ + 1, true);
        return true;
    });

    Reload();
}

LibraryView::~LibraryView()
{
    *alive_ = false;
    StopPhoneSignIn();
}

brls::View* LibraryView::getDefaultFocus()
{
    return first_card_ ? first_card_ : brls::Box::getDefaultFocus();
}

void LibraryView::ShowMessage(const std::string& title, const std::string& body)
{
    StopPhoneSignIn();
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
    ++explore_generation_;
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
            StartCatalogExploration();
        });
    });
}

void LibraryView::ShowStorePicker()
{
    StopPhoneSignIn();
    content_->clearViews();
    first_card_ = nullptr;

    auto* card = new brls::Box(brls::Axis::COLUMN);
    card->setPadding(28, 32, 28, 32);
    card->setCornerRadius(14);
    card->setBackgroundColor(nvgRGB(24, 22, 34));
    auto* heading = MakeLabel(Tr("Choisis ta boutique Amazon"), 24, nvgRGB(245, 246, 248));
    heading->setMarginBottom(10);
    card->addView(heading);
    auto* explanation = MakeLabel(Tr("Luna n'est disponible que dans ces pays. LunaNX se connectera à la "
                                     "boutique choisie et utilisera sa langue (modifiable ensuite dans les "
                                     "Réglages)."),
                                  17, nvgRGB(200, 204, 212));
    explanation->setMarginBottom(18);
    card->addView(explanation);

    // Boutiques Luna, par ordre alphabétique de leur nom dans leur propre langue.
    std::vector<luna::AmazonMarket> markets = luna::AmazonMarkets();
    std::sort(markets.begin(), markets.end(), [](const luna::AmazonMarket& a, const luna::AmazonMarket& b) {
        return a.native_name < b.native_name;
    });
    const std::string current = LoadStreamSettings().amazon_market;
    for (const auto& market : markets)
    {
        auto* button = new brls::Button();
        button->setText(market.native_name + "  ·  amazon." + market.code);
        button->setStyle(market.code == current ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_BORDERED);
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
    content_->addView(card);
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
    RefreshHeaderLanguage();
    // Différé : le bouton qui a déclenché le choix ne doit pas être détruit pendant son clic.
    auto alive = alive_;
    brls::sync([this, alive]() {
        if (*alive)
            ShowSignIn({});
    });
}

void LibraryView::RefreshHeaderLanguage()
{
    language_ = UiLanguage();
    refresh_button_->setText(Tr("Actualiser"));
    settings_button_->setText(Tr("Réglages"));
    updateActionHint(brls::BUTTON_Y, Tr("Actualiser"));
    updateActionHint(brls::BUTTON_X, Tr("Réglages"));
    updateActionHint(brls::BUTTON_START, Tr("Quitter"));
    updateActionHint(brls::BUTTON_LB, Tr("Page précédente"));
    updateActionHint(brls::BUTTON_RB, Tr("Page suivante"));
}

void LibraryView::ShowSignIn(const std::string& reason)
{
    // Premier lancement : la boutique Amazon doit être choisie avant la connexion.
    if (LoadStreamSettings().amazon_market.empty())
    {
        ShowStorePicker();
        return;
    }
    StopPhoneSignIn();
    content_->clearViews();
    first_card_ = nullptr;
    const luna::AmazonMarket market = luna::FindAmazonMarket(LoadStreamSettings().amazon_market);

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

    // Alternative sans le navigateur de la Switch (indisponible sur certaines consoles).
    auto* phone_button = new brls::Button();
    phone_button->setText(Tr("Se connecter avec mon téléphone"));
    phone_button->setStyle(&brls::BUTTONSTYLE_PRIMARY);
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
    auto* phone_hint = MakeLabel(Tr("Le navigateur de la Switch ne s'ouvre pas ou affiche « This feature is not "
                                    "available » ? Connecte-toi depuis ton téléphone : scanne un QR code, puis "
                                    "connecte-toi sur la vraie page Amazon."),
                                 15, nvgRGB(160, 164, 176));
    phone_hint->setMarginBottom(20);
    card->addView(phone_hint);

    auto* store_button = new brls::Button();
    store_button->setText(Tr("Changer de boutique") + "  ·  amazon." + market.code);
    store_button->setStyle(&brls::BUTTONSTYLE_BORDERED);
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
    CompleteSignIn(pending, code);
}

void LibraryView::CompleteSignIn(const luna::PendingSignIn& pending, const std::string& code)
{
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

    content_->clearViews();
    first_card_ = nullptr;
    phone_server_ = std::move(server);

    auto* card = new brls::Box(brls::Axis::COLUMN);
    card->setPadding(28, 32, 28, 32);
    card->setCornerRadius(14);
    card->setBackgroundColor(nvgRGB(24, 22, 34));

    auto* heading = MakeLabel(Tr("Connexion avec ton téléphone"), 24, nvgRGB(245, 246, 248));
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
    steps->addView(MakeLabel(
        TrFormat("1. Connecte ton téléphone au même réseau Wi-Fi que la Switch.\n"
                 "2. Scanne ce QR code avec l'appareil photo, ou ouvre cette adresse :\n{}\n"
                 "3. Touche « Ouvrir la page de connexion Amazon » et connecte-toi.\n"
                 "4. Copie l'adresse de la dernière page affichée, reviens sur la page LunaNX et colle-la.",
                 {address}),
        18, nvgRGB(220, 222, 230)));
    auto* waiting = MakeLabel(Tr("En attente de ton téléphone…"), 17, nvgRGB(176, 150, 255));
    waiting->setMarginTop(18);
    steps->addView(waiting);
    row->addView(steps);
    card->addView(row);

    auto* cancel = new brls::Button();
    cancel->setText(Tr("Annuler"));
    cancel->setStyle(&brls::BUTTONSTYLE_BORDERED);
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

    content_->addView(card);
    first_card_ = cancel;
    brls::Application::giveFocus(cancel);
}

void LibraryView::ShowShelves(const std::vector<luna::Shelf>& shelves)
{
    if (&shelves != &shelves_)
        shelves_ = shelves;
    StopPhoneSignIn();
    content_->clearViews();
    first_card_ = nullptr;
    grid_ = nullptr;
    grid_page_box_ = nullptr;
    grid_row_ = nullptr;
    pager_ = nullptr;
    prev_button_ = nullptr;
    next_button_ = nullptr;
    page_label_ = nullptr;
    all_games_count_ = nullptr;
    recent_changed_ = false;

    catalog_.clear();
    catalog_index_.clear();
    for (const auto& shelf : shelves_)
        for (const auto& game : shelf.games)
            AddToCatalog(catalog_, catalog_index_, game);
    if (catalog_.empty())
    {
        rendered_layout_.clear();
        ShowMessage(Tr("Aucun jeu trouvé"),
                    Tr("Le catalogue Luna n'a renvoyé aucun jeu.\n\nVérifie ton abonnement Luna, puis "
                       "appuie sur Y pour réessayer."));
        return;
    }

    grid_games_ = catalog_;

    rendered_layout_ = LoadStreamSettings().library_layout;
    if (rendered_layout_ == "List")
        ShowGameList(grid_games_);
    else
        ShowCovers(shelves_, grid_games_);
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
    all_games_count_ = heading;

    for (const auto& game : games)
        AddListCell(game);
}

void LibraryView::AddListCell(const luna::Game& game)
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
                                    TrFormat("{} jeux disponibles", {std::to_string(games.size())}),
                                    &all_games_count_));
    grid_ = new brls::Box(brls::Axis::COLUMN);
    content_->addView(grid_);

    // Navigation entre les pages de 10 jaquettes (boutons, ou L / R).
    pager_ = new brls::Box(brls::Axis::ROW);
    pager_->setAlignItems(brls::AlignItems::CENTER);
    pager_->setMarginTop(6);
    pager_->setMarginBottom(30);
    auto make_pager_button = [this](const std::string& text, bool forward) {
        auto* button = new brls::Button();
        button->setText(text);
        button->setStyle(&brls::BUTTONSTYLE_BORDERED);
        button->setMarginRight(16);
        button->registerClickAction([this, forward](brls::View*) {
            if (forward)
                ShowGridPage(grid_page_ + 1, true);
            else if (grid_page_ > 0)
                ShowGridPage(grid_page_ - 1, true);
            return true;
        });
        pager_->addView(button);
        return button;
    };
    prev_button_ = make_pager_button(Tr("Page précédente"), false);
    next_button_ = make_pager_button(Tr("Page suivante"), true);
    page_label_ = MakeLabel("", 17, nvgRGB(150, 154, 166));
    pager_->addView(page_label_);
    content_->addView(pager_);
    ShowGridPage(0, false);
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

void LibraryView::ShowGridPage(size_t page, bool focus_first_card)
{
    if (!grid_)
        return;
    const size_t pages = std::max<size_t>(1, (grid_games_.size() + kGamesPerPage - 1) / kGamesPerPage);
    grid_page_ = std::min(page, pages - 1);

    // Nouvelle page construite et focalisée avant la suppression de l'ancienne : ses
    // jaquettes (et leurs textures) sont libérées, la mémoire reste celle de 10 jaquettes.
    brls::Box* old_page = grid_page_box_;
    grid_page_box_ = new brls::Box(brls::Axis::COLUMN);
    grid_row_ = nullptr;
    grid_shown_end_ = grid_page_ * kGamesPerPage;
    grid_->addView(grid_page_box_);
    AppendToGridPage();
    brls::View* first_new = grid_page_box_->getChildren().empty() ? nullptr : grid_page_box_->getChildren().front();
    if (auto* row = dynamic_cast<brls::Box*>(first_new))
        first_new = row->getChildren().empty() ? nullptr : row->getChildren().front();
    // Sans bannière ni rangée, la première jaquette sert de focus par défaut : elle suit la page.
    bool first_card_in_old_page = false;
    for (brls::View* view = first_card_; view && old_page; view = view->getParent())
        if (view == old_page)
            first_card_in_old_page = true;
    if (!first_card_ || first_card_in_old_page)
        first_card_ = first_new;
    if (focus_first_card && first_new)
        brls::Application::giveFocus(first_new);
    if (old_page)
        grid_->removeView(old_page, true);
    UpdatePager();
}

void LibraryView::AppendToGridPage()
{
    // Complète la page affichée (jeux découverts pendant que la page n'est pas pleine).
    const size_t end = std::min((grid_page_ + 1) * kGamesPerPage, grid_games_.size());
    for (size_t i = grid_shown_end_; i < end; ++i)
    {
        if (!grid_row_ || i % kGridColumns == 0)
        {
            grid_row_ = new brls::Box(brls::Axis::ROW);
            grid_page_box_->addView(grid_row_);
        }
        const luna::Game game = grid_games_[i];
        grid_row_->addView(new PosterCard(game, kGridCardWidth, 0, [this, game]() { Launch(game); }));
    }
    grid_shown_end_ = std::max(grid_shown_end_, end);
}

void LibraryView::UpdatePager()
{
    if (!pager_)
        return;
    const size_t pages = (grid_games_.size() + kGamesPerPage - 1) / kGamesPerPage;
    pager_->setVisibility(pages > 1 ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    prev_button_->setVisibility(grid_page_ > 0 ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    next_button_->setVisibility(grid_page_ + 1 < pages ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    page_label_->setText(TrFormat("Page {} / {}", {std::to_string(grid_page_ + 1), std::to_string(std::max<size_t>(pages, 1))}));
}

void LibraryView::UpdateGameCount()
{
    if (all_games_count_)
        all_games_count_->setText(TrFormat("{} jeux disponibles", {std::to_string(grid_games_.size())}));
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
    const std::string connected_text = status_label_->getFullText();
    status_label_->setText(Tr("Recherche d'autres jeux…"));
    auto alive = alive_;
    auto client = client_;
    brls::async([this, alive, client, known, generation, connected_text]() {
        client->ExploreCatalog(
            known,
            [this, alive, generation](std::vector<luna::Game> games) {
                brls::sync([this, alive, generation, games = std::move(games)]() mutable {
                    if (*alive && generation == explore_generation_.load())
                        AddDiscoveredGames(std::move(games));
                });
            },
            [this, alive, generation]() { return *alive && generation == explore_generation_.load(); });
        brls::sync([this, alive, generation, connected_text]() {
            if (*alive && generation == explore_generation_.load())
            {
                exploration_done_ = true;
                status_label_->setText(connected_text);
            }
        });
    });
}

void LibraryView::AddDiscoveredGames(std::vector<luna::Game> games)
{
    // Gardés avec le catalogue : ils réapparaissent si l'écran est redessiné.
    shelves_.push_back(luna::Shelf {"", "EXPLORED", games});
    // Dernier filtre anti-doublon : seuls les jeux absents du catalogue sont ajoutés.
    std::vector<luna::Game> shown;
    for (const auto& game : games)
        if (AddToCatalog(catalog_, catalog_index_, game))
            shown.push_back(game);
    if (shown.empty())
        return;
    grid_games_.insert(grid_games_.end(), shown.begin(), shown.end());
    UpdateGameCount();
    if (rendered_layout_ == "List")
    {
        for (const auto& game : shown)
            AddListCell(game);
        return;
    }
    if (!grid_)
        return;
    // Ajoutés à la fin de « Tous les jeux » sans redessiner l'écran ni déplacer le focus.
    AppendToGridPage();
    UpdatePager();
}

void LibraryView::OnResume()
{
    // Langue changée dans les réglages : textes de l'écran, puis catalogue dans cette langue.
    if (UiLanguage() != language_)
    {
        RefreshHeaderLanguage();
        auto alive = alive_;
        brls::sync([this, alive]() {
            if (*alive)
                Reload();
        });
        return;
    }
    // Redessine après le retour du focus par Borealis, jamais pendant.
    const bool layout_changed = !rendered_layout_.empty() &&
                                LoadStreamSettings().library_layout != rendered_layout_;
    if (shelves_.empty() || loading_ || (!recent_changed_ && !layout_changed))
        return;
    auto alive = alive_;
    brls::sync([this, alive]() {
        if (!*alive || loading_)
            return;
        ShowShelves(shelves_);
        // Exploration interrompue par une partie : elle reprend (les jeux déjà trouvés restent).
        if (!exploration_done_)
            StartCatalogExploration();
    });
}

void LibraryView::Launch(const luna::Game& game)
{
    if (!client_)
        return;
    LogRuntimeEvent("library", "launch", game.title);
    // Pas d'exploration du catalogue pendant une partie : elle reprendra au retour.
    ++explore_generation_;
    luna::RecordRecentGame(game);
    recent_changed_ = true;
    brls::Application::pushActivity(new brls::Activity(new StreamView(client_, game)),
                                    brls::TransitionAnimation::FADE);
}

} // namespace lunanx

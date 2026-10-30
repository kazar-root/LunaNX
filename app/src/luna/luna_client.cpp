#include "luna_client.hpp"

#include "core/app_paths.hpp"
#include "core/i18n.hpp"
#include "core/runtime_journal.hpp"

#include <jansson.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <chrono>
#include <functional>
#include <memory>
#include <random>
#include <set>
#include <thread>
#include <mutex>
#include <atomic>
#include <map>

namespace lunanx::luna
{
namespace
{

using JsonPtr = std::unique_ptr<json_t, decltype(&json_decref)>;

JsonPtr Adopt(json_t* value)
{
    return JsonPtr(value, &json_decref);
}

std::string Str(json_t* object, const char* key)
{
    json_t* value = json_object_get(object, key);
    return json_is_string(value) ? json_string_value(value) : std::string();
}

std::string FirstStr(json_t* object, std::initializer_list<const char*> keys)
{
    for (const char* key : keys)
    {
        std::string value = Str(object, key);
        if (!value.empty())
            return value;
    }
    return {};
}

std::string RandomHex(size_t length)
{
    static thread_local std::mt19937_64 rng(std::random_device {}() ^
        static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(length);
    for (size_t i = 0; i < length; ++i)
        out.push_back(digits[rng() & 0xf]);
    return out;
}

std::string RandomUuid()
{
    std::string hex = RandomHex(32);
    return hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-4" + hex.substr(13, 3) + "-a" +
           hex.substr(17, 3) + "-" + hex.substr(20, 12);
}

// Format des identifiants Amazon « 123-1234567-1234567 ».
std::string RandomAmazonId()
{
    static thread_local std::mt19937 rng(std::random_device {}());
    std::uniform_int_distribution<int> digit(0, 9);
    std::string out;
    for (int i = 0; i < 17; ++i)
    {
        if (i == 3 || i == 10)
            out.push_back('-');
        out.push_back(static_cast<char>('0' + digit(rng)));
    }
    return out;
}

// Parcourt récursivement un document JSON (pages Luna = arbres de widgets).
void Walk(json_t* node, const std::function<void(json_t*)>& visit)
{
    if (json_is_object(node))
    {
        visit(node);
        const char* key = nullptr;
        json_t* child = nullptr;
        json_object_foreach(node, key, child)
            Walk(child, visit);
    }
    else if (json_is_array(node))
    {
        size_t index = 0;
        json_t* child = nullptr;
        json_array_foreach(node, index, child)
            Walk(child, visit);
    }
}

std::string QueryParam(const std::string& url, const std::string& name)
{
    const std::string needle = name + "=";
    size_t pos = url.find("?" + needle);
    if (pos == std::string::npos)
        pos = url.find("&" + needle);
    if (pos == std::string::npos)
        return {};
    pos += needle.size() + 1;
    const size_t end = url.find('&', pos);
    return url.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
}

// « https://luna.amazon.fr/game/goat-simulator-3/B0G4SQ18L7 » → « game/goat-simulator-3/B0G4SQ18L7 »
std::string PageUriFromProductUrl(const std::string& url)
{
    const size_t pos = url.find("/game/");
    if (pos == std::string::npos)
        return {};
    std::string uri = url.substr(pos + 1);
    const size_t query = uri.find_first_of("?#");
    if (query != std::string::npos)
        uri.erase(query);
    return uri;
}

std::string AsinFromPageUri(const std::string& page_uri)
{
    const size_t slash = page_uri.find_last_of('/');
    return slash == std::string::npos ? std::string() : page_uri.substr(slash + 1);
}

bool IsIgnoredWidgetType(const std::string& type)
{
    for (const char* prefix : {"NAV_", "DIALOG", "BASIC_DIALOG", "SEO_", "FOOTER", "SURVEY",
                               "NOTIFY", "TRANSLATION", "INSTALL_PWA", "GLOBAL_OVERLAY", "LOADING"})
        if (type.rfind(prefix, 0) == 0)
            return true;
    return type.find("PLAYER") != std::string::npos;
}

std::string FirstActionToken(json_t* widget)
{
    size_t index = 0;
    json_t* action = nullptr;
    json_array_foreach(json_object_get(widget, "actions"), index, action)
    {
        const std::string token = Str(action, "serviceToken");
        if (!token.empty())
            return token;
    }
    return {};
}

// Parcourt toutes les chaînes d'un widget (clé, valeur), à n'importe quelle profondeur.
void WalkStrings(json_t* node, const std::function<void(const char*, const std::string&)>& visit,
                 const char* key = "")
{
    if (json_is_string(node))
        visit(key, json_string_value(node));
    else if (json_is_object(node))
    {
        const char* child_key = nullptr;
        json_t* child = nullptr;
        json_object_foreach(node, child_key, child)
            WalkStrings(child, visit, child_key);
    }
    else if (json_is_array(node))
    {
        size_t index = 0;
        json_t* child = nullptr;
        json_array_foreach(node, index, child)
            WalkStrings(child, visit, key);
    }
}

bool LooksLikeAsin(const std::string& value)
{
    if (value.size() != 10 || value[0] != 'B')
        return false;
    return std::all_of(value.begin(), value.end(), [](char c) {
        return std::isdigit(static_cast<unsigned char>(c)) || std::isupper(static_cast<unsigned char>(c));
    });
}

bool IsGameArt(const std::string& value)
{
    return value.find("media-amazon.com/images/I/") != std::string::npos &&
           value.find(".mp4") == std::string::npos;
}

// Extrait un jeu d'un élément de rangée, quelle que soit la forme exacte du widget :
// titre, visuels, adresse de fiche (game/<slug>/<ASIN>) et jetons d'action.
Game ExtractGame(json_t* widget)
{
    json_t* data = json_object_get(widget, "presentationData");
    json_t* hover = json_object_get(data, "hoverDetails");
    Game game;
    game.title = FirstStr(data, {"title", "productTitle", "gameTitle", "name"});
    if (game.title.empty())
        game.title = FirstStr(hover, {"title", "productTitle"});
    game.game_id = FirstStr(data, {"gameId", "productId"});
    game.asin = Str(data, "asin");
    game.image_landscape = FirstStr(data, {"imageLandscape", "tileImageLandscape", "focusedImageLandscape",
                                           "focusedImage", "backgroundImageLandscape", "imageUrlLandscape"});
    game.image_portrait = FirstStr(data, {"imagePortrait", "tileImagePortrait", "focusedImagePortrait",
                                          "productBackgroundPortrait", "imageUrl", "image"});
    game.publisher = FirstStr(hover, {"publishersText"});
    game.detail_service_token = FirstActionToken(widget);
    game.play_service_token = Str(hover, "primaryButtonServiceToken");

    std::string any_art;
    WalkStrings(widget, [&](const char* key, const std::string& value) {
        const std::string k = key ? key : "";
        if (game.page_uri.empty() && value.find("/game/") != std::string::npos &&
            value.find("amazon.") != std::string::npos)
            game.page_uri = PageUriFromProductUrl(value);
        if (game.asin.empty() && value.find("asin=") != std::string::npos)
            game.asin = QueryParam(value, "asin");
        if (game.asin.empty() && (k == "asin" || k == "productAsin") && LooksLikeAsin(value))
            game.asin = value;
        if (game.game_id.empty() && (k == "gameId" || k == "productId"))
            game.game_id = value;
        if (game.play_service_token.empty() && k.find("ServiceToken") != std::string::npos)
            game.play_service_token = value;
        if (any_art.empty() && IsGameArt(value))
            any_art = value;
        if (game.title.empty() && (k == "title" || k == "productTitle" || k == "gameTitle"))
            game.title = value;
    });
    if (game.image_landscape.empty() && game.image_portrait.empty())
        game.image_landscape = any_art;
    if (game.asin.empty())
        game.asin = AsinFromPageUri(game.page_uri);
    if (game.asin.empty() && LooksLikeAsin(game.game_id))
        game.asin = game.game_id;
    if (game.page_uri.empty() && !game.asin.empty() && !game.title.empty())
        game.page_uri = MakeGamePageUri(game.title, game.asin);

    size_t index = 0;
    json_t* genre = nullptr;
    json_array_foreach(json_object_get(data, "genreTags"), index, genre)
        if (json_is_string(genre))
            game.genres.push_back(json_string_value(genre));
    return game;
}

bool CanOpen(const Game& game)
{
    return !game.title.empty() &&
           (!game.page_uri.empty() || !game.detail_service_token.empty() || !game.play_service_token.empty());
}

std::string RowTitle(json_t* row, const std::string& fallback)
{
    json_t* data = json_object_get(row, "presentationData");
    std::string title = FirstStr(data, {"title", "headerText", "header", "heading", "rowTitle",
                                        "displayTitle", "text", "primaryText", "label"});
    if (!title.empty())
        return title;
    const std::string type = Str(row, "type");
    if (type == "FEATURED_ROTATOR_V3")
        return Tr("À la une");
    if (type == "RANKINGS_ROW")
        return Tr("Top du moment");
    if (type.find("UPSELL") != std::string::npos)
        return Tr("Avec les abonnements partenaires");
    return fallback;
}

std::string TabTitle(const std::string& page_type, const std::string& label)
{
    if (!label.empty())
        return label;
    if (page_type == "landing_home")
        return Tr("Accueil");
    if (page_type == "landing_game_night")
        return Tr("Soirée jeux");
    if (page_type == "landing_library")
        return Tr("Ma bibliothèque");
    if (page_type == "landing_about")
        return Tr("Découvrir Luna");
    return "Luna";
}

// Luna envoie souvent « presentationData » sous forme de texte JSON encodé : on le
// remplace par l'objet décodé dans tout l'arbre, pour que la suite le lise normalement.
void DecodeEmbeddedJson(json_t* root)
{
    Walk(root, [](json_t* node) {
        for (const char* key : {"presentationData", "payload"})
        {
            json_t* value = json_object_get(node, key);
            if (!json_is_string(value))
                continue;
            const char* text = json_string_value(value);
            if (!text || (text[0] != '{' && text[0] != '['))
                continue;
            json_error_t error {};
            json_t* parsed = json_loads(text, 0, &error);
            if (parsed)
                json_object_set_new(node, key, parsed);
        }
    });
}

// Lit une fiche de jeu : titre, visuel et, surtout, l'action « Jouer » (GAME_SESSION_ASYNC)
// dont le payload contient le playToken nécessaire au lancement.
void ParseGameDetail(json_t* page, Game& game)
{
    DecodeEmbeddedJson(page);
    Walk(page, [&game](json_t* node) {
        const std::string type = Str(node, "type");
        json_t* data = json_object_get(node, "presentationData");
        if (type == "PRODUCT_DETAIL" && json_is_object(data))
        {
            if (game.title.empty())
                game.title = Str(data, "title");
            game.description = Str(data, "description");
            game.publisher = Str(data, "publishersText");
            game.developer = Str(data, "developersText");
            game.age_rating = Str(json_object_get(data, "ageRating"), "categoryText");
            if (game.image_landscape.empty())
                game.image_landscape = FirstStr(data, {"tileImageLandscape", "backgroundImageLandscape"});
            if (game.image_portrait.empty())
                game.image_portrait = FirstStr(data, {"tileImagePortrait", "productIcon"});
            if (game.asin.empty())
                game.asin = Str(data, "asin");
        }
        if (Str(node, "target") == "GAME_SESSION_ASYNC" && game.play_token.empty())
        {
            json_t* payload = json_object_get(node, "payload");
            game.play_token = Str(payload, "playToken");
            if (!Str(payload, "gameId").empty())
                game.game_id = Str(payload, "gameId");
            if (!Str(payload, "productStage").empty())
                game.product_stage = Str(payload, "productStage");
            if (!Str(payload, "asin").empty())
                game.asin = Str(payload, "asin");
            if (game.title.empty())
                game.title = Str(payload, "title");
            if (game.image_portrait.empty())
                game.image_portrait = Str(payload, "productIcon");
        }
    });
}

// Détecte la boutique Amazon d'origine des cookies (amazon.fr, .de, .co.uk, .com...).
struct Marketplace
{
    const char* cookie_suffix;
    const char* origin;
    const char* marketplace_id;
    const char* locale;
    const char* country;
    const char* timezone;
};

constexpr Marketplace kMarketplaces[] = {
    {"acbfr", "https://luna.amazon.fr", "A13V1IB3VIYZZH", "fr_FR", "FR", "Europe/Paris"},
    {"acbde", "https://luna.amazon.de", "A1PA6795UKMFR9", "de_DE", "DE", "Europe/Berlin"},
    {"acbuk", "https://luna.amazon.co.uk", "A1F83G8C2ARO7P", "en_GB", "GB", "Europe/London"},
    {"acbes", "https://luna.amazon.es", "A1RKKUPIHCS9HS", "es_ES", "ES", "Europe/Madrid"},
    {"acbit", "https://luna.amazon.it", "APJ6JRA9NG5V4", "it_IT", "IT", "Europe/Rome"},
    {"acbca", "https://luna.amazon.ca", "A2EUQ1WTGCTBG2", "en_CA", "CA", "America/Toronto"},
    {"main", "https://luna.amazon.com", "ATVPDKIKX0DER", "en_US", "US", "America/New_York"},
};

} // namespace

std::string MakeGamePageUri(const std::string& title, const std::string& asin)
{
    std::string slug;
    for (unsigned char c : title)
    {
        if (std::isalnum(c))
            slug.push_back(static_cast<char>(std::tolower(c)));
        else if (!slug.empty() && slug.back() != '-')
            slug.push_back('-');
    }
    while (!slug.empty() && slug.back() == '-')
        slug.pop_back();
    return "game/" + (slug.empty() ? std::string("game") : slug) + "/" + asin;
}

LunaClient::LunaClient(CookieJar cookies, std::optional<DeviceRegistration> registration)
    : cookies_(std::move(cookies)), registration_(std::move(registration))
{
    for (const auto& market : kMarketplaces)
    {
        if (!cookies_.Value(std::string("at-") + market.cookie_suffix).empty() ||
            !cookies_.Value(std::string("ubid-") + market.cookie_suffix).empty())
        {
            account_.luna_origin = market.origin;
            account_.marketplace_id = market.marketplace_id;
            account_.locale = market.locale;
            account_.country = market.country;
            account_.timezone = market.timezone;
            account_.device_serial = cookies_.Value(std::string("ubid-") + market.cookie_suffix);
            break;
        }
    }
    account_.session_id = cookies_.Value("session-id");
    if (account_.device_serial.empty())
        account_.device_serial = RandomAmazonId();
    if (account_.session_id.empty())
        account_.session_id = RandomAmazonId();
    account_.tdi = RandomUuid();
    ApplyMarketDefaults();
}

void LunaClient::ApplyMarketDefaults()
{
    if (!registration_)
        return;
    const AmazonMarket& market = FindAmazonMarket(registration_->market_code);
    account_.luna_origin = "https://luna.amazon." + market.code;
    account_.marketplace_id = market.marketplace_id;
    for (const auto& known : kMarketplaces)
    {
        if (std::string(known.marketplace_id) == market.marketplace_id)
        {
            account_.locale = known.locale;
            account_.country = known.country;
            account_.timezone = known.timezone;
        }
    }
}

bool LunaClient::SetContentLanguage(const std::string& language)
{
    if (market_locale_.empty())
        market_locale_ = account_.locale;
    const std::string& country = account_.country;
    std::string locale = market_locale_;
    if (language == "en")
        locale = country == "US" ? "en_US" : country == "CA" ? "en_CA" : "en_GB";
    else if (language == "fr" && (country == "FR" || country == "CA"))
        locale = country == "CA" ? "fr_CA" : "fr_FR";
    account_.locale = locale;
    return locale != market_locale_;
}

std::string LunaClient::HostOf(const std::string& url) const
{
    const size_t scheme = url.find("://");
    const size_t start = scheme == std::string::npos ? 0 : scheme + 3;
    const size_t end = url.find_first_of("/:?", start);
    return url.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

std::string LunaClient::FetchAccessToken() const
{
    const std::string url = account_.luna_origin + "/token";
    const std::string cookie = cookies_.HeaderFor(HostOf(url));
    if (cookie.empty())
        throw SignInRequired("Aucun cookie Amazon pour " + HostOf(url));

    const HttpResponse response = http_.Get(url, kBrowserUserAgent, {
        "Cookie: " + cookie,
        "Accept: application/json",
        "Content-Type: application/json",
        "Referer: " + account_.luna_origin + "/",
    });
    if (response.status_code == 401 || response.status_code == 403)
        throw SignInRequired(TrFormat("Session Amazon refusée (HTTP {})", {std::to_string(response.status_code)}));
    if (response.status_code != 200)
        throw LunaError(TrFormat("/token a répondu HTTP {}", {std::to_string(response.status_code)}));

    json_error_t error {};
    auto root = Adopt(json_loads(response.body.c_str(), 0, &error));
    if (!root)
        throw LunaError(Tr("/token : réponse illisible"));
    const std::string token = Str(root.get(), "token");
    if (!json_is_true(json_object_get(root.get(), "valid")) || token.empty())
    {
        const std::string reason = Str(root.get(), "invalidReason");
        throw SignInRequired(Tr("Session Amazon expirée") + (reason.empty() ? "" : " (" + reason + ")"));
    }
    return token;
}

std::vector<std::string> LunaClient::ProxyHeaders() const
{
    std::vector<std::string> headers = {
        "Content-Type: text/plain;charset=UTF-8",
        "Accept: */*",
        "Origin: " + account_.luna_origin,
        "Referer: " + account_.luna_origin + "/",
        "x-amz-client-version: -",
        "x-amz-country-of-residence: " + account_.country,
        "x-amz-device-serial-number: " + account_.device_serial,
        "x-amz-device-type: browser",
        "x-amz-locale: " + account_.locale,
        "x-amz-marketplace-id: " + account_.marketplace_id,
        "x-amz-platform: web",
        "x-amz-react-version: 1.0.37245.0",
        "x-amz-session-id: " + account_.session_id,
        "x-amz-tdi: " + account_.tdi,
        "x-amz-timezone: " + account_.timezone,
        "x-amzn-requestid: " + RandomUuid(),
    };
    if (!account_.access_token.empty())
        headers.push_back("x-amz-access-token: " + account_.access_token);
    if (!account_.profile_token.empty())
        headers.push_back("x-amz-profile-token: " + account_.profile_token);
    return headers;
}

json_t* LunaClient::Call(const std::string& operation, json_t* body) const
{
    auto owned_body = Adopt(body ? body : json_object());
    char* text = json_dumps(owned_body.get(), JSON_COMPACT);
    const std::string payload = text ? text : "{}";
    free(text);

    const std::string url = account_.proxy_url + "/" + operation;
    const HttpResponse response = http_.Post(url, kBrowserUserAgent, ProxyHeaders(), payload);
    LogRuntimeEvent("luna", "call", operation + " status=" + std::to_string(response.status_code));

    if (response.status_code == 401 || response.status_code == 403)
        throw SignInRequired(TrFormat("{} : accès refusé (HTTP {})",
                                      {operation, std::to_string(response.status_code)}));
    if (response.status_code < 200 || response.status_code >= 300)
    {
        LogRuntimeEvent("luna", "call_failed",
                        operation + " status=" + std::to_string(response.status_code) +
                            " errorType=" + (response.error_type.empty() ? "-" : response.error_type) +
                            " profileToken=" + (account_.profile_token.empty() ? "absent" : "present") +
                            " body=" + response.body.substr(0, 200));
        throw LunaError(TrFormat("{} a répondu HTTP {}", {operation, std::to_string(response.status_code)}) +
                        (response.error_type.empty() ? "" : " (" + response.error_type + ")") +
                        " : " + response.body.substr(0, 200));
    }

    json_error_t error {};
    json_t* root = json_loads(response.body.empty() ? "{}" : response.body.c_str(), 0, &error);
    if (!root)
        throw LunaError(TrFormat("{} : réponse JSON illisible", {operation}));
    return root;
}

json_t* LunaClient::ClientContext() const
{
    json_t* browser = json_pack(
        "{s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "browserClientRole", "browser",
        "browserType", "Chrome",
        "browserVersion", "141.0.0.0",
        "deviceModel", "Windows",
        "deviceType", "Windows",
        "osName", "Windows",
        "osVersion", "10");
    return json_pack("{s:o, s:n, s:n}", "browserMetadata", browser, "refMarker", "referrer");
}

void LunaClient::LoadAppConfig()
{
    auto config = Adopt(Call("getAppConfig", json_pack("{s:s}", "stageOverride", "Prod")));
    json_t* root = config.get();

    json_t* endpoints = json_object_get(root, "endpoints");
    const std::string proxy = Str(endpoints, "clientProxy");
    if (!proxy.empty())
        account_.proxy_url = proxy;
    const std::string origin = Str(endpoints, "horizonte");
    if (!origin.empty())
        account_.luna_origin = origin;

    account_.regions.clear();
    size_t index = 0;
    json_t* item = nullptr;
    json_array_foreach(json_object_get(endpoints, "supportedRegions"), index, item)
    {
        Region region;
        region.region = Str(item, "region");
        region.endpoint = Str(item, "endpoint");
        region.endpoint_url = Str(item, "endpointUrl");
        if (!region.region.empty() && !region.endpoint_url.empty())
            account_.regions.push_back(region);
    }

    const std::string marketplace = Str(root, "marketplaceId");
    if (!marketplace.empty())
        account_.marketplace_id = marketplace;
    const std::string tdi = Str(root, "tdi");
    if (!tdi.empty())
        account_.tdi = tdi;

    json_t* initial = json_object_get(root, "initialPageContext");
    account_.initial_page_id = Str(initial, "pageId");
    if (!Str(initial, "pageType").empty())
        account_.initial_page_type = Str(initial, "pageType");
    json_t* profile = json_object_get(root, "profilePageContext");
    account_.profile_page_id = Str(profile, "pageId");
    if (!Str(profile, "pageType").empty())
        account_.profile_page_type = Str(profile, "pageType");

    account_.protocol_features.clear();
    const char* name = nullptr;
    json_t* feature = nullptr;
    json_object_foreach(json_object_get(root, "features"), name, feature)
    {
        json_t* value = json_object_get(feature, "value");
        if (json_is_true(json_object_get(feature, "forwardToProtocol")) && json_is_string(value))
            account_.protocol_features.emplace_back(name, json_string_value(value));
    }
}

void LunaClient::SelectProfile(const Status& status)
{
    if (status)
        status(Tr("Sélection du profil Luna…"));

    // La page « player_selection » liste les profils du compte ; on prend le premier.
    if (!account_.profile_page_id.empty())
    {
        try
        {
            auto context = json_pack("{s:s, s:s}", "pageId", account_.profile_page_id.c_str(),
                                     "pageType", account_.profile_page_type.c_str());
            auto page = Adopt(GetPage(context));
            // Comme pour le catalogue, presentationData / payload arrivent en texte JSON.
            DecodeEmbeddedJson(page.get());
            Walk(page.get(), [this](json_t* node) {
                if (account_.profile_id.empty())
                    account_.profile_id = FirstStr(node, {"profileId", "playerProfileId"});
            });
        }
        catch (const LunaError& ex)
        {
            LogRuntimeEvent("luna", "profile_page_failed", ex.what());
        }
    }

    if (account_.profile_id.empty())
    {
        LogRuntimeEvent("luna", "profile", "aucun profileId trouvé, poursuite sans jeton de profil");
        return;
    }

    try
    {
        auto login = Adopt(Call("player/playerLogin",
                                json_pack("{s:s}", "profileId", account_.profile_id.c_str())));
        const std::string token = Str(login.get(), "token");
        if (!token.empty())
            account_.profile_token = token;
        LogRuntimeEvent("luna", "profile", token.empty() ? "playerLogin sans jeton" : "jeton de profil obtenu");
    }
    catch (const LunaError& ex)
    {
        // Le catalogue reste consultable sans profil ; seul le lancement en a besoin.
        LogRuntimeEvent("luna", "profile_login_failed", ex.what());
    }
}

void LunaClient::SignIn(const Status& status)
{
    if (cookies_.Empty())
        throw SignInRequired("Aucun fichier de cookies trouvé");

    if (status)
        status(Tr("Connexion au compte Amazon…"));
    try
    {
        account_.access_token = FetchAccessToken();
    }
    catch (const SignInRequired&)
    {
        // Appareil enregistré : on régénère des cookies neufs sans redemander le mot de passe.
        if (!registration_)
            throw;
        if (status)
            status(Tr("Renouvellement de la session Amazon…"));
        registration_->cookies = AmazonAuth::RefreshCookies(*registration_);
        AmazonAuth::Save(*registration_);
        cookies_.SetCookies(registration_->cookies);
        account_.access_token = FetchAccessToken();
    }

    if (status)
        status(Tr("Chargement de la configuration Luna…"));
    LoadAppConfig();
    SelectProfile(status);
    LogRuntimeEvent("luna", "signed_in",
                    "origin=" + account_.luna_origin + " regions=" +
                        std::to_string(account_.regions.size()) +
                        " profile=" + (account_.profile_token.empty() ? "none" : "ok"));
}

json_t* LunaClient::GetPage(json_t* page_context, const std::string& service_token) const
{
    json_t* body = json_pack("{s:i, s:s, s:o, s:{s:[]}, s:[]}",
                             "timeout", 10000,
                             "featureScheme", "WEB_V1",
                             "clientContext", ClientContext(),
                             "inputContext", "gamepadTypes",
                             "dynamicFeatures");
    if (page_context)
        json_object_set_new(body, "pageContext", page_context);
    if (!service_token.empty())
        json_object_set_new(body, "serviceToken", json_string(service_token.c_str()));
    return Call("getPage", body);
}

std::vector<Shelf> LunaClient::FetchShelves() const
{
    std::vector<Shelf> shelves;
    // Tuiles sans titre ni visuel : on ouvre leur fiche (en parallèle) pour les compléter.
    struct Pending
    {
        size_t shelf;
        size_t slot;
    };
    std::vector<Pending> pending;
    constexpr size_t kMaximumDetailFetches = 40;
    std::set<std::string> seen_tokens;

    auto collect = [&](json_t* page, const std::string& tab_title) {
        DecodeEmbeddedJson(page);
        std::vector<json_t*> rows;
        Walk(page, [&](json_t* node) {
            if (json_is_array(json_object_get(node, "widgets")))
                rows.push_back(node);
        });

        for (json_t* row : rows)
        {
            const std::string row_type = Str(row, "type");
            if (IsIgnoredWidgetType(row_type) || row_type.find("FOOTER") != std::string::npos)
                continue;
            Shelf shelf;
            shelf.title = RowTitle(row, tab_title);
            shelf.row_type = row_type;
            std::vector<size_t> row_pending;
            size_t index = 0;
            json_t* child = nullptr;
            json_array_foreach(json_object_get(row, "widgets"), index, child)
            {
                const std::string type = Str(child, "type");
                if (IsIgnoredWidgetType(type) || type.find("GENRE") != std::string::npos ||
                    type.find("BANNER") != std::string::npos || type.find("EMPTY") != std::string::npos ||
                    type.find("BUTTON") != std::string::npos)
                    continue;
                json_t* nested = json_object_get(child, "widgets");
                if (json_is_array(nested) && json_array_size(nested) > 0)
                    continue;
                Game game = ExtractGame(child);
                const std::string key = !game.detail_service_token.empty() ? game.detail_service_token
                                      : !game.asin.empty() ? game.asin : game.title;
                if (key.empty() || !seen_tokens.insert(shelf.title + "|" + key).second)
                    continue;
                if (CanOpen(game) && (!game.image_landscape.empty() || !game.image_portrait.empty()))
                {
                    shelf.games.push_back(std::move(game));
                }
                else if (!game.detail_service_token.empty() || !game.play_service_token.empty())
                {
                    // Titre/visuel absents : complétés plus bas depuis la fiche du jeu.
                    row_pending.push_back(shelf.games.size());
                    shelf.games.push_back(std::move(game));
                }
            }
            if (shelf.games.empty())
                continue;
            for (size_t slot : row_pending)
                pending.push_back({shelves.size(), slot});
            shelves.push_back(std::move(shelf));
        }
    };

    json_t* nav_context = account_.initial_page_id.empty()
        ? json_pack("{s:s}", "pageType", account_.initial_page_type.c_str())
        : json_pack("{s:s, s:s}", "pageId", account_.initial_page_id.c_str(),
                    "pageType", account_.initial_page_type.c_str());
    auto navigation = Adopt(GetPage(nav_context));
    DecodeEmbeddedJson(navigation.get());

    std::vector<std::pair<std::string, std::string>> tabs;
    std::set<std::string> tab_tokens;
    Walk(navigation.get(), [&](json_t* node) {
        const std::string type = Str(node, "type");
        if (type != "NAV_ITEM" && type != "NAV_SECONDARY_ITEM")
            return;
        const std::string label = FirstStr(json_object_get(node, "presentationData"), {"text", "title", "ariaLabel"});
        const std::string token = FirstActionToken(node);
        if (!token.empty() && tab_tokens.insert(token).second)
            tabs.emplace_back(label, token);
    });

    for (size_t i = 0; i < tabs.size() && i < 6; ++i)
    {
        try
        {
            auto page = Adopt(GetPage(nullptr, tabs[i].second));
            const std::string page_type = Str(json_object_get(page.get(), "pageContext"), "pageType");
            if (page_type == "landing_search" || page_type == "landing_remote_play" || page_type == "landing_about")
                continue;
            collect(page.get(), TabTitle(page_type, tabs[i].first));
        }
        catch (const SignInRequired&)
        {
            throw;
        }
        catch (const LunaError& ex)
        {
            LogRuntimeEvent("luna", "tab_failed", tabs[i].first + " " + ex.what());
        }
    }

    // Fiches des tuiles incomplètes, une par une, pour ne pas multiplier les connexions.
    if (pending.size() > kMaximumDetailFetches)
        pending.resize(kMaximumDetailFetches);
    for (const auto& item : pending)
    {
        Game& game = shelves[item.shelf].games[item.slot];
        if (!game.title.empty())
            continue;
        try
        {
            const std::string token = !game.detail_service_token.empty() ? game.detail_service_token
                                                                         : game.play_service_token;
            auto page = Adopt(GetPage(nullptr, token));
            ParseGameDetail(page.get(), game);
        }
        catch (const SignInRequired&)
        {
            throw;
        }
        catch (const LunaError& ex)
        {
            LogRuntimeEvent("luna", "tile_detail_failed", ex.what());
        }
    }

    // On ne garde que les jeux qu'on sait afficher et ouvrir.
    size_t total = 0;
    for (auto& shelf : shelves)
    {
        shelf.games.erase(std::remove_if(shelf.games.begin(), shelf.games.end(),
                                         [](const Game& g) { return !CanOpen(g); }),
                          shelf.games.end());
        total += shelf.games.size();
    }
    shelves.erase(std::remove_if(shelves.begin(), shelves.end(),
                                 [](const Shelf& sh) { return sh.games.empty(); }),
                  shelves.end());

    LogRuntimeEvent("luna", "catalog", "tabs=" + std::to_string(tabs.size()) +
                    " shelves=" + std::to_string(shelves.size()) + " games=" + std::to_string(total) +
                    " detailFetches=" + std::to_string(pending.size()));
    return shelves;
}

Game LunaClient::FetchGameDetail(const Game& input) const
{
    Game game = input;
    if (!game.play_token.empty())
        return game;
    JsonPtr page(nullptr, &json_decref);
    // 1. Jeton d'action de la tuile (chemin du site), sinon adresse « game/<slug>/<ASIN> ».
    if (!game.detail_service_token.empty())
    {
        try
        {
            page = Adopt(GetPage(nullptr, game.detail_service_token));
        }
        catch (const SignInRequired&)
        {
            throw;
        }
        catch (const LunaError& ex)
        {
            LogRuntimeEvent("luna", "detail_by_token_failed", ex.what());
        }
    }
    if (!page && !game.page_uri.empty())
    {
        try
        {
            json_t* context = account_.initial_page_id.empty()
                ? json_pack("{s:s}", "pageUri", game.page_uri.c_str())
                : json_pack("{s:s, s:s}", "pageUri", game.page_uri.c_str(),
                            "pageId", account_.initial_page_id.c_str());
            page = Adopt(GetPage(context));
        }
        catch (const SignInRequired&)
        {
            throw;
        }
        catch (const LunaError& ex)
        {
            LogRuntimeEvent("luna", "detail_by_uri_failed", ex.what());
        }
    }
    if (!page)
        throw LunaError(TrFormat("Impossible d'ouvrir la fiche de « {} »", {game.title}));

    ParseGameDetail(page.get(), game);

    // Certaines tuiles mènent directement au bouton « Jouer » plutôt qu'à la fiche.
    if (game.play_token.empty() && !game.play_service_token.empty())
    {
        auto play_page = Adopt(GetPage(nullptr, game.play_service_token));
        ParseGameDetail(play_page.get(), game);
    }

    if (game.play_token.empty())
        throw LunaError(Tr("Ce jeu ne peut pas être lancé avec ce compte (non inclus dans l'abonnement ?)"));
    return game;
}

std::vector<Region> LunaClient::MeasureRegions() const
{
    std::vector<Region> regions = account_.regions;
    for (auto& region : regions)
    {
        int best = -1;
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            const auto started = std::chrono::steady_clock::now();
            const HttpResponse response = http_.Get(
                region.endpoint_url + "?timestamp=" + std::to_string(now_ms),
                kBrowserUserAgent, {"Origin: " + account_.luna_origin}, 3, 4);
            const int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count());
            if (response.status_code == 200 && (best < 0 || elapsed < best))
                best = elapsed;
        }
        region.latency_ms = best;
    }
    std::stable_sort(regions.begin(), regions.end(), [](const Region& a, const Region& b) {
        if ((a.latency_ms < 0) != (b.latency_ms < 0))
            return a.latency_ms >= 0;
        return a.latency_ms < b.latency_ms;
    });
    return regions;
}

bool LunaClient::IsPinChallengeRequired() const
{
    auto result = Adopt(Call("isPinChallengeRequired",
                             json_pack("{s:[s]}", "actions", "LAUNCH_GAME_10PLUS")));
    json_t* challenge = json_object_get(result.get(), "challenge");
    if (json_is_boolean(challenge))
        return json_is_true(challenge);
    const std::string value = json_is_string(challenge) ? json_string_value(challenge) : "";
    return !(value.empty() || value == "NONE" || value == "false" || value == "FALSE");
}

std::string LunaClient::CreateGameSessionJob(
    const Game& game,
    const std::string& offer_sdp,
    const std::vector<std::string>& requested_channels,
    const std::vector<Region>& regions,
    const std::string& preferred_region) const
{
    json_t* features = json_array();
    for (const auto& feature : account_.protocol_features)
        json_array_append_new(features, json_pack("{s:s, s:s}", "name", feature.first.c_str(),
                                                  "value", feature.second.c_str()));
    json_t* channels = json_array();
    for (const auto& channel : requested_channels)
        json_array_append_new(channels, json_string(channel.c_str()));

    json_t* client_context = ClientContext();
    json_t* browser = json_incref(json_object_get(client_context, "browserMetadata"));
    json_t* signaling = json_pack("{s:s, s:s, s:{s:o, s:o, s:o}}",
                                  "type", "offer",
                                  "sdp", offer_sdp.c_str(),
                                  "extras",
                                  "features", features,
                                  "clientContext", browser,
                                  "requestedChannels", channels);
    // Le serveur attend l'offre sous forme de JSON sérialisé dans une chaîne, pas d'objet.
    char* signaling_text = json_dumps(signaling, JSON_COMPACT);
    json_decref(signaling);
    if (!signaling_text)
        throw LunaError(Tr("Impossible de préparer l'offre WebRTC"));
    json_t* signaling_string = json_string(signaling_text);
    free(signaling_text);

    json_t* latencies = json_array();
    for (const auto& region : regions)
    {
        if (region.latency_ms < 0)
            continue;
        json_array_append_new(latencies, json_pack(
            "{s:s, s:s, s:s, s:i}",
            "endpoint", region.endpoint.c_str(),
            "endpointUrl", region.endpoint_url.c_str(),
            "region", region.region.c_str(),
            "latencyMs", region.latency_ms));
    }
    json_t* preferences = json_array();
    std::string chosen = preferred_region;
    if ((chosen.empty() || chosen == "Auto") && !regions.empty() && regions.front().latency_ms >= 0)
        chosen = regions.front().region;
    if (!chosen.empty() && chosen != "Auto")
        json_array_append_new(preferences, json_pack("{s:s}", "region", chosen.c_str()));

    json_t* body = json_pack(
        "{s:i, s:s, s:s, s:s, s:o, s:o, s:o, s:b, s:b, s:{s:s, s:[s]}, s:o}",
        "timeout", 10000,
        "gameId", game.game_id.c_str(),
        "playToken", game.play_token.c_str(),
        "productStage", game.product_stage.c_str(),
        "signalingRequest", signaling_string,
        "regionLatencies", latencies,
        "regionPreferences", preferences,
        "transferRequest", 0,
        "revokeProfileActivityRequest", 0,
        "clientFeatures", "dataUsage", "STANDARD", "routing", "TURN",
        "clientContext", client_context);
    if (!body)
        throw LunaError(Tr("Impossible de préparer la requête de session"));

    {
        // Types et tailles des champs uniquement (aucun jeton ni identifiant).
        std::string shape;
        const char* key = nullptr;
        json_t* value = nullptr;
        json_object_foreach(body, key, value)
        {
            shape += std::string(shape.empty() ? "" : " ") + key + "=";
            if (json_is_string(value))
                shape += "str(" + std::to_string(json_string_length(value)) + ")";
            else if (json_is_array(value))
                shape += "arr(" + std::to_string(json_array_size(value)) + ")";
            else if (json_is_object(value))
                shape += "obj(" + std::to_string(json_object_size(value)) + ")";
            else if (json_is_boolean(value))
                shape += "bool";
            else if (json_is_integer(value))
                shape += "int";
            else
                shape += "null";
        }
        LogRuntimeEvent("luna", "create_request",
                        shape + " productStage=" + game.product_stage + " features=" +
                            std::to_string(account_.protocol_features.size()) + " profileToken=" +
                            (account_.profile_token.empty() ? "absent" : "present"));
    }
    auto result = Adopt(Call("createGameSessionJob", body));
    const std::string job_id = Str(result.get(), "gameSessionJobId");
    if (job_id.empty())
        throw LunaError(Tr("Luna n'a pas créé de session de jeu"));
    return job_id;
}

SessionJob LunaClient::GetGameSessionJob(const std::string& job_id) const
{
    auto result = Adopt(Call("getGameSessionJob",
                             json_pack("{s:s}", "gameSessionJobId", job_id.c_str())));
    json_t* root = result.get();

    SessionJob job;
    job.job_id = job_id;
    job.raw_status = Str(root, "status");
    json_t* retry = json_object_get(root, "retryAfterMs");
    if (json_is_integer(retry))
        job.retry_after_ms = static_cast<int>(json_integer_value(retry));

    json_t* session = json_object_get(root, "gameSession");
    if (json_is_object(session))
    {
        job.session_id = Str(session, "sessionId");
        job.connection_id = Str(session, "connectionId");
        job.region = Str(session, "region");
        job.routing = Str(session, "routing");
        json_t* answer = json_object_get(session, "signalingResponse");
        // Normalement un objet {type, sdp} ; on accepte aussi une chaîne JSON.
        JsonPtr parsed(nullptr, &json_decref);
        if (json_is_string(answer))
        {
            json_error_t error {};
            parsed.reset(json_loads(json_string_value(answer), 0, &error));
            answer = parsed.get();
        }
        job.answer_sdp = Str(answer, "sdp");
    }

    if (job.raw_status == "COMPLETED_SUCCEEDED" && !job.answer_sdp.empty())
        job.status = SessionJobStatus::Ready;
    else if (job.raw_status.find("FAIL") != std::string::npos ||
             job.raw_status.find("CANCEL") != std::string::npos ||
             job.raw_status == "COMPLETED_SUCCEEDED")
    {
        job.status = SessionJobStatus::Failed;
        job.error = FirstStr(root, {"failureReason", "errorCode", "message"});
        if (job.error.empty())
            job.error = job.raw_status;
    }
    return job;
}

} // namespace lunanx::luna

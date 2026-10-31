#pragma once

#include "amazon_auth.hpp"
#include "cookie_jar.hpp"
#include "core/http_client.hpp"
#include "luna_models.hpp"

#include <functional>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

typedef struct json_t json_t;

namespace lunanx::luna
{

class LunaError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

// Les cookies ont expiré ou sont absents : il faut les réexporter depuis le navigateur.
class SignInRequired : public LunaError
{
  public:
    using LunaError::LunaError;
};

class LunaClient
{
  public:
    using Status = std::function<void(const std::string&)>;

    explicit LunaClient(CookieJar cookies, std::optional<DeviceRegistration> registration = {});

    // 1. /token (cookies → jeton d'accès)  2. getAppConfig  3. choix du profil + playerLogin.
    void SignIn(const Status& status = {});
    // Langue du catalogue et des sessions (en-tête x-amz-locale) : code de langue de
    // l'interface, ou vide pour la langue de la boutique. Renvoie true si
    // elle diffère de la langue de la boutique (pour pouvoir revenir à celle-ci en cas d'échec).
    bool SetContentLanguage(const std::string& language);
    bool SignedIn() const { return !account_.access_token.empty(); }
    const Account& account() const { return account_; }

    // Rayons de la page d'accueil et des onglets de navigation.
    std::vector<Shelf> FetchShelves() const;
    // Parcourt en direct les pages liées au catalogue (« Tout voir », genres, collections,
    // chaînes…) repérées par FetchShelves, pour trouver les jeux absents des rangées de
    // l'accueil. Les nouveaux jeux (absents de `known`, clés de InsertGameKeys) sont transmis
    // par lots à `on_games` ; l'exploration s'arrête dès que `keep_going` renvoie false.
    void ExploreCatalog(std::set<std::string> known,
                        const std::function<void(std::vector<Game>)>& on_games,
                        const std::function<bool()>& keep_going) const;
    // Complète un jeu avec sa fiche (description, playToken).
    Game FetchGameDetail(const Game& game) const;

    std::vector<Region> MeasureRegions() const;
    bool IsPinChallengeRequired() const;

    std::string CreateGameSessionJob(
        const Game& game,
        const std::string& offer_sdp,
        const std::vector<std::string>& requested_channels,
        const std::vector<Region>& regions,
        const std::string& preferred_region) const;
    SessionJob GetGameSessionJob(const std::string& job_id) const;

    // Appel brut au proxy (POST JSON) ; lève LunaError en cas d'échec. Le résultat doit
    // être libéré avec json_decref.
    json_t* Call(const std::string& operation, json_t* body) const;

  private:
    std::string FetchAccessToken() const;
    void LoadAppConfig();
    void SelectProfile(const Status& status);
    // Page Luna désignée soit par un contexte (pageId/pageUri), soit par le jeton de service
    // porté par l'action d'un widget (c'est ainsi que le site web navigue).
    json_t* GetPage(json_t* page_context, const std::string& service_token = {}) const;
    void ApplyMarketDefaults();
    std::string market_locale_;
    // Liens de navigation repérés par FetchShelves, point de départ de ExploreCatalog.
    mutable std::mutex catalog_links_mutex_;
    mutable std::vector<std::string> catalog_links_;
    json_t* ClientContext() const;
    std::vector<std::string> ProxyHeaders() const;
    std::string HostOf(const std::string& url) const;

    CookieJar cookies_;
    std::optional<DeviceRegistration> registration_;
    Account account_;
    HttpClient http_;
};

// Construit l'URI de fiche « game/<slug>/<ASIN> » à partir d'un titre.
std::string MakeGamePageUri(const std::string& title, const std::string& asin);

} // namespace lunanx::luna

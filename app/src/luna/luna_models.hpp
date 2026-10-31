#pragma once

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace lunanx::luna
{

struct Game
{
    std::string game_id;
    std::string asin;
    std::string title;
    std::string page_uri;        // ex. « game/goat-simulator-3/B0G4SQ18L7 »
    // Jetons de navigation Luna : l'action d'une tuile ouvre la fiche (detail) ou lance
    // directement le jeu (play) quand on renvoie ce jeton à getPage.
    std::string detail_service_token;
    std::string play_service_token;
    std::string image_portrait;
    std::string image_landscape;
    std::string description;
    std::string publisher;
    std::string developer;
    std::string age_rating;
    std::vector<std::string> genres;
    // Renseignés par la fiche du jeu, nécessaires au lancement.
    std::string play_token;
    std::string product_stage = "Release";
};

// Deux tuiles désignent le même jeu si leur ASIN ou leur titre normalisé (sans casse,
// ponctuation ni symboles ™ ®) sont identiques.
std::string NormalizedGameTitle(const std::string& title);
// Ajoute l'ASIN et le titre normalisé du jeu à « seen » ; faux si l'un des deux y était déjà.
bool InsertGameKeys(std::set<std::string>& seen, const Game& game);

struct Shelf
{
    std::string title;
    std::string row_type;        // type de rangée Luna (« RANKINGS_ROW » = classement)
    std::vector<Game> games;
};

struct Region
{
    std::string region;          // ex. « eu-west-3 »
    std::string endpoint;        // ex. « dynamodb.eu-west-3.amazonaws.com »
    std::string endpoint_url;    // URL de pingRegion
    int latency_ms = -1;
};

// Tout ce qu'il faut pour signer un appel au proxy Luna.
struct Account
{
    std::string luna_origin = "https://luna.amazon.fr";
    std::string proxy_url = "https://proxy-prod.eu-west-1.tempo.digital.a2z.com";
    std::string marketplace_id = "A13V1IB3VIYZZH";
    std::string locale = "fr_FR";
    std::string country = "FR";
    std::string timezone = "Europe/Paris";
    std::string access_token;    // « Atza|… », obtenu via /token et les cookies
    std::string profile_token;   // JWT renvoyé par player/playerLogin
    std::string profile_id;
    std::string device_serial;
    std::string session_id;
    std::string tdi;
    std::string initial_page_id;
    std::string initial_page_type = "multistate_navigation";
    std::string profile_page_id;
    std::string profile_page_type = "player_selection";
    std::vector<Region> regions;
    // Fonctions annoncées par getAppConfig avec forwardToProtocol=true, renvoyées
    // dans l'offre de signalisation (ex. BROWSER_FLEX_FEC=enabled).
    std::vector<std::pair<std::string, std::string>> protocol_features;
};

enum class SessionJobStatus
{
    Pending,
    Ready,
    Failed,
};

struct SessionJob
{
    std::string job_id;
    SessionJobStatus status = SessionJobStatus::Pending;
    std::string raw_status;
    int retry_after_ms = 1500;
    std::string session_id;
    std::string connection_id;
    std::string region;
    std::string routing;
    std::string answer_sdp;
    std::string error;
};

} // namespace lunanx::luna

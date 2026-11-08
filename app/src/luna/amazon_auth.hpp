#pragma once

#include "cookie_jar.hpp"

#include <string>
#include <vector>

namespace lunanx::luna
{

// Pays Luna proposé au choix : détermine les domaines de connexion et de Luna, et ce que
// LunaNX annonce à Luna (pays de résidence, langue du catalogue, fuseau horaire).
struct AmazonMarket
{
    // Identifiant enregistré dans les réglages et la session : « fr », « de », « co.uk », « es »,
    // « it », « pl », « nl », « se », « com.be », « at », « pt », « lu », « com », « ca ».
    std::string code;
    std::string label;          // affiché dans les réglages
    std::string audible_suffix;
    std::string marketplace_id;
    std::string language;       // langue de LunaNX appliquée quand on choisit ce pays
    std::string native_name;    // nom du pays dans sa langue, pour le choix au premier lancement
    // Boutique Amazon du compte (amazon.<domain>, cookies .amazon.<domain>, luna.amazon.<domain>).
    // Autriche, Luxembourg et Portugal n'ont pas de boutique propre : leurs abonnés Prime
    // passent par amazon.de ou amazon.es, comme sur le site Luna.
    std::string domain;
    // Domaine de la connexion et de l'enregistrement (www.amazon.<x>, api.amazon.<x>). C'est celui
    // de la boutique, sauf pour amazon.pl, .nl, .se et .com.be : la page de connexion Audible n'y
    // existe pas (et api.amazon.pl, .se, .com.be non plus). Les comptes européens étant communs,
    // la connexion passe par amazon.de ; les cookies restent ceux de la boutique.
    std::string identity_code;
    std::string country;        // pays de résidence annoncé à Luna (x-amz-country-of-residence)
    std::string locale;         // langue du catalogue Luna par défaut (x-amz-locale)
    std::string timezone;       // x-amz-timezone
};

const std::vector<AmazonMarket>& AmazonMarkets();
const AmazonMarket& FindAmazonMarket(const std::string& code);

// Paramètres d'une connexion en cours (PKCE + identité de l'appareil).
struct PendingSignIn
{
    std::string market_code;
    std::string device_serial;
    std::string code_verifier;
    std::string sign_in_url;
    std::string callback_url;
};

// Appareil enregistré sur le compte : permet de régénérer les cookies sans mot de passe.
struct DeviceRegistration
{
    std::string market_code;
    std::string device_serial;
    std::string refresh_token;
    std::string customer_name;
    std::vector<Cookie> cookies;
};

class AmazonAuth
{
  public:
    // Prépare l'URL de la vraie page de connexion Amazon (OAuth 2 + PKCE).
    static PendingSignIn BeginSignIn(const std::string& market_code);
    // Extrait le code d'autorisation de l'URL finale (« …/ap/maplanding?…authorization_code=… »).
    static std::string ExtractAuthorizationCode(const std::string& final_url);
    // Échange le code contre un enregistrement d'appareil (jeton + cookies du site).
    static DeviceRegistration Register(const PendingSignIn& pending, const std::string& authorization_code);
    // Nouveaux cookies du site à partir du jeton de renouvellement.
    static std::vector<Cookie> RefreshCookies(const DeviceRegistration& registration);
    // Retire l'appareil du compte Amazon (déconnexion).
    static void Deregister(const DeviceRegistration& registration);

    static bool Load(DeviceRegistration& registration);
    static bool Save(const DeviceRegistration& registration);
    // Enregistre des cookies renouvelés, sauf si l'utilisateur s'est déconnecté entre-temps.
    static bool UpdateStored(const DeviceRegistration& registration);
    static void Erase();
    // Supprime de la carte SD toutes les données du compte (session, cookies, jeux récents).
    // Renvoie false si un fichier n'a pas pu être supprimé.
    static bool EraseAccountData();
    static std::string StoragePath();
};

} // namespace lunanx::luna

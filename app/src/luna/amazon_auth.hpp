#pragma once

#include "cookie_jar.hpp"

#include <string>
#include <vector>

namespace lunanx::luna
{

// Boutique Amazon du compte : détermine les domaines de connexion et de Luna.
struct AmazonMarket
{
    std::string code;        // « fr », « de », « co.uk », « es », « it », « com », « ca »
    std::string label;       // affiché dans les réglages
    std::string audible_suffix;
    std::string marketplace_id;
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
    static void Erase();
    static std::string StoragePath();
};

} // namespace lunanx::luna

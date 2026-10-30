#include "amazon_auth.hpp"

#include "core/app_paths.hpp"
#include "core/atomic_file_replace.hpp"
#include "core/http_client.hpp"
#include "core/i18n.hpp"
#include "core/runtime_journal.hpp"
#include "luna_client.hpp"

#include <jansson.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <cctype>
#include <cstdio>
#include <memory>
#include <random>

namespace lunanx::luna
{
namespace
{

using JsonPtr = std::unique_ptr<json_t, decltype(&json_decref)>;

// L'enregistrement d'appareil passe par le service d'identité des applications mobiles
// Amazon (« MAP »), avec le profil de l'application Audible pour iOS : c'est le profil
// public le plus utilisé par les clients libres et il délivre des cookies valables sur
// tout le site Amazon, donc pour Luna.
constexpr const char* kDeviceType = "A2CZJZGLK2JJVM";
constexpr const char* kAppName = "Audible";
constexpr const char* kAppVersion = "3.56.2";
constexpr const char* kSoftwareVersion = "35602678";
constexpr const char* kOsVersion = "15.0.0";
constexpr const char* kMobileUserAgent =
    "Mozilla/5.0 (iPhone; CPU iPhone OS 15_0 like Mac OS X) AppleWebKit/605.1.15 "
    "(KHTML, like Gecko) Mobile/15E148";

void RandomBytes(unsigned char* out, size_t size)
{
#ifdef __SWITCH__
    randomGet(out, size);
#else
    std::random_device device;
    for (size_t i = 0; i < size; ++i)
        out[i] = static_cast<unsigned char>(device());
#endif
}

std::string Base64Url(const unsigned char* data, size_t size)
{
    std::string out(4 * ((size + 2) / 3) + 1, '\0');
    size_t written = 0;
    mbedtls_base64_encode(reinterpret_cast<unsigned char*>(out.data()), out.size(), &written, data, size);
    out.resize(written);
    for (char& c : out)
    {
        if (c == '+')
            c = '-';
        else if (c == '/')
            c = '_';
    }
    while (!out.empty() && out.back() == '=')
        out.pop_back();
    return out;
}

std::string Hex(const std::string& data, bool upper = false)
{
    static const char* lower_digits = "0123456789abcdef";
    static const char* upper_digits = "0123456789ABCDEF";
    const char* digits = upper ? upper_digits : lower_digits;
    std::string out;
    for (unsigned char c : data)
    {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 0xf]);
    }
    return out;
}

std::string UrlEncode(const std::string& value)
{
    static const char* digits = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : value)
    {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out.push_back(static_cast<char>(c));
        else
        {
            out.push_back('%');
            out.push_back(digits[c >> 4]);
            out.push_back(digits[c & 0xf]);
        }
    }
    return out;
}

std::string UrlDecode(const std::string& value)
{
    std::string out;
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '%' && i + 2 < value.size())
        {
            out.push_back(static_cast<char>(std::stoi(value.substr(i + 1, 2), nullptr, 16)));
            i += 2;
        }
        else
            out.push_back(value[i] == '+' ? ' ' : value[i]);
    }
    return out;
}

std::string ClientId(const std::string& serial)
{
    return Hex(serial + "#" + kDeviceType);
}

std::string Str(json_t* object, const char* key)
{
    json_t* value = json_object_get(object, key);
    return json_is_string(value) ? json_string_value(value) : std::string();
}

std::vector<Cookie> ParseCookies(json_t* array, const std::string& fallback_domain)
{
    std::vector<Cookie> cookies;
    size_t index = 0;
    json_t* item = nullptr;
    json_array_foreach(array, index, item)
    {
        Cookie cookie;
        cookie.name = Str(item, "Name");
        cookie.value = Str(item, "Value");
        cookie.domain = Str(item, "Domain");
        cookie.path = Str(item, "Path");
        if (cookie.domain.empty())
            cookie.domain = fallback_domain;
        if (cookie.path.empty())
            cookie.path = "/";
        // Amazon renvoie parfois les valeurs entre guillemets : le navigateur les envoie telles
        // quelles, sauf pour les jetons « Atza|… » qu'on transmet nus.
        if (cookie.value.size() >= 2 && cookie.value.front() == '"' && cookie.value.back() == '"' &&
            cookie.value.find('|') != std::string::npos)
            cookie.value = cookie.value.substr(1, cookie.value.size() - 2);
        if (!cookie.name.empty())
            cookies.push_back(std::move(cookie));
    }
    return cookies;
}

std::string ErrorFrom(const HttpResponse& response)
{
    json_error_t error {};
    JsonPtr root(json_loads(response.body.c_str(), 0, &error), &json_decref);
    json_t* err = json_object_get(json_object_get(root.get(), "response"), "error");
    const std::string message = Str(err, "message");
    const std::string code = Str(err, "code");
    if (!message.empty())
        return message + (code.empty() ? "" : " (" + code + ")");
    return "HTTP " + std::to_string(response.status_code);
}

} // namespace

const std::vector<AmazonMarket>& AmazonMarkets()
{
    static const std::vector<AmazonMarket> markets = {
        {"fr", "France (amazon.fr)", "fr", "A13V1IB3VIYZZH"},
        {"de", "Allemagne (amazon.de)", "de", "A1PA6795UKMFR9"},
        {"co.uk", "Royaume-Uni (amazon.co.uk)", "uk", "A1F83G8C2ARO7P"},
        {"es", "Espagne (amazon.es)", "es", "A1RKKUPIHCS9HS"},
        {"it", "Italie (amazon.it)", "it", "APJ6JRA9NG5V4"},
        {"com", "États-Unis (amazon.com)", "us", "ATVPDKIKX0DER"},
        {"ca", "Canada (amazon.ca)", "ca", "A2EUQ1WTGCTBG2"},
    };
    return markets;
}

const AmazonMarket& FindAmazonMarket(const std::string& code)
{
    for (const auto& market : AmazonMarkets())
        if (market.code == code)
            return market;
    return AmazonMarkets().front();
}

PendingSignIn AmazonAuth::BeginSignIn(const std::string& market_code)
{
    const AmazonMarket& market = FindAmazonMarket(market_code);
    PendingSignIn pending;
    pending.market_code = market.code;

    unsigned char serial_bytes[16];
    RandomBytes(serial_bytes, sizeof(serial_bytes));
    pending.device_serial = Hex(std::string(reinterpret_cast<char*>(serial_bytes), sizeof(serial_bytes)), true);

    unsigned char verifier_bytes[32];
    RandomBytes(verifier_bytes, sizeof(verifier_bytes));
    pending.code_verifier = Base64Url(verifier_bytes, sizeof(verifier_bytes));
    unsigned char digest[32];
    mbedtls_sha256(reinterpret_cast<const unsigned char*>(pending.code_verifier.data()),
                   pending.code_verifier.size(), digest, 0);
    const std::string challenge = Base64Url(digest, sizeof(digest));

    const std::string base = "https://www.amazon." + market.code;
    pending.callback_url = base + "/ap/maplanding";
    const std::string identifier = "http://specs.openid.net/auth/2.0/identifier_select";
    pending.sign_in_url = base + "/ap/signin"
        "?openid.oa2.response_type=code"
        "&openid.oa2.code_challenge_method=S256"
        "&openid.oa2.code_challenge=" + challenge +
        "&openid.return_to=" + UrlEncode(pending.callback_url) +
        "&openid.assoc_handle=amzn_audible_ios_" + market.audible_suffix +
        "&openid.identity=" + UrlEncode(identifier) +
        "&pageId=amzn_audible_ios"
        "&accountStatusPolicy=P1"
        "&openid.claimed_id=" + UrlEncode(identifier) +
        "&openid.mode=checkid_setup"
        "&openid.ns.oa2=" + UrlEncode("http://www.amazon.com/ap/ext/oauth/2") +
        "&openid.oa2.client_id=device:" + ClientId(pending.device_serial) +
        "&openid.ns.pape=" + UrlEncode("http://specs.openid.net/extensions/pape/1.0") +
        "&marketPlaceId=" + market.marketplace_id +
        "&openid.oa2.scope=device_auth_access"
        "&forceMobileLayout=true"
        "&openid.ns=" + UrlEncode("http://specs.openid.net/auth/2.0") +
        "&openid.pape.max_auth_age=0";
    return pending;
}

std::string AmazonAuth::ExtractAuthorizationCode(const std::string& final_url)
{
    const std::string key = "openid.oa2.authorization_code=";
    const size_t pos = final_url.find(key);
    if (pos == std::string::npos)
        return {};
    const size_t start = pos + key.size();
    const size_t end = final_url.find_first_of("&#", start);
    return UrlDecode(final_url.substr(start, end == std::string::npos ? std::string::npos : end - start));
}

DeviceRegistration AmazonAuth::Register(const PendingSignIn& pending, const std::string& authorization_code)
{
    const AmazonMarket& market = FindAmazonMarket(pending.market_code);
    const std::string domain = "." + std::string("amazon.") + market.code;

    JsonPtr body(json_pack(
        "{s:[s,s,s], s:{s:[], s:s}, s:{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}, "
        "s:{s:s, s:s, s:s, s:s, s:s}, s:[s,s]}",
        "requested_token_type", "bearer", "website_cookies", "store_authentication_cookie",
        "cookies", "website_cookies", "domain", domain.c_str(),
        "registration_data",
        "domain", "Device",
        "app_version", kAppVersion,
        "device_serial", pending.device_serial.c_str(),
        "device_type", kDeviceType,
        "device_name", "%FIRST_NAME%%FIRST_NAME_POSSESSIVE_STRING%%DUPE_STRATEGY_1ST%LunaNX",
        "os_version", kOsVersion,
        "software_version", kSoftwareVersion,
        "device_model", "iPhone",
        "app_name", kAppName,
        "auth_data",
        "client_id", ClientId(pending.device_serial).c_str(),
        "authorization_code", authorization_code.c_str(),
        "code_verifier", pending.code_verifier.c_str(),
        "code_algorithm", "SHA-256",
        "client_domain", "DeviceLegacy",
        "requested_extensions", "device_info", "customer_info"), &json_decref);
    char* text = json_dumps(body.get(), JSON_COMPACT);
    const std::string payload = text ? text : "{}";
    free(text);

    HttpClient http;
    const HttpResponse response = http.Post(
        "https://api.amazon." + market.code + "/auth/register", kMobileUserAgent,
        {"Content-Type: application/json", "Accept: application/json",
         "x-amzn-identity-auth-domain: api.amazon." + market.code},
        payload);
    if (response.status_code != 200)
        throw LunaError(TrFormat("Enregistrement de l'appareil refusé : {}", {ErrorFrom(response)}));

    json_error_t error {};
    JsonPtr root(json_loads(response.body.c_str(), 0, &error), &json_decref);
    json_t* success = json_object_get(json_object_get(root.get(), "response"), "success");
    json_t* tokens = json_object_get(success, "tokens");

    DeviceRegistration registration;
    registration.market_code = market.code;
    registration.device_serial = pending.device_serial;
    registration.refresh_token = Str(json_object_get(tokens, "bearer"), "refresh_token");
    registration.customer_name = Str(json_object_get(json_object_get(success, "extensions"), "customer_info"), "given_name");
    if (registration.customer_name.empty())
        registration.customer_name = Str(json_object_get(json_object_get(success, "extensions"), "customer_info"), "name");

    json_t* website_cookies = json_object_get(tokens, "website_cookies");
    registration.cookies = ParseCookies(website_cookies, domain);
    if (registration.refresh_token.empty())
        throw LunaError(Tr("Réponse d'enregistrement incomplète (aucun jeton)"));
    if (registration.cookies.empty())
        registration.cookies = RefreshCookies(registration);
    LogRuntimeEvent("auth", "registered", "market=" + market.code +
                    " cookies=" + std::to_string(registration.cookies.size()));
    return registration;
}

std::vector<Cookie> AmazonAuth::RefreshCookies(const DeviceRegistration& registration)
{
    const AmazonMarket& market = FindAmazonMarket(registration.market_code);
    const std::string domain = ".amazon." + market.code;
    const std::string body =
        "app_name=" + std::string(kAppName) +
        "&app_version=" + kAppVersion +
        "&source_token=" + UrlEncode(registration.refresh_token) +
        "&requested_token_type=auth_cookies"
        "&source_token_type=refresh_token"
        "&domain=" + UrlEncode(domain);

    HttpClient http;
    const HttpResponse response = http.Post(
        "https://www.amazon." + market.code + "/ap/exchangetoken/cookies", kMobileUserAgent,
        {"Content-Type: application/x-www-form-urlencoded",
         "x-amzn-identity-auth-domain: api.amazon." + market.code},
        body);
    if (response.status_code == 400 || response.status_code == 401)
        throw SignInRequired(Tr("L'appareil a été déconnecté du compte Amazon"));
    if (response.status_code != 200)
        throw LunaError(TrFormat("Renouvellement de la session impossible : {}", {ErrorFrom(response)}));

    json_error_t error {};
    JsonPtr root(json_loads(response.body.c_str(), 0, &error), &json_decref);
    json_t* by_domain = json_object_get(
        json_object_get(json_object_get(json_object_get(root.get(), "response"), "tokens"), "cookies"),
        domain.c_str());
    auto cookies = ParseCookies(by_domain, domain);
    if (cookies.empty())
        throw LunaError(Tr("Amazon n'a renvoyé aucun cookie de session"));
    LogRuntimeEvent("auth", "cookies_refreshed", "count=" + std::to_string(cookies.size()));
    return cookies;
}

void AmazonAuth::Deregister(const DeviceRegistration& registration)
{
    if (registration.refresh_token.empty())
        return;
    const AmazonMarket& market = FindAmazonMarket(registration.market_code);
    HttpClient http;
    // Un jeton d'accès frais est nécessaire : on l'obtient à partir du jeton de renouvellement.
    const HttpResponse token = http.Post(
        "https://api.amazon." + market.code + "/auth/token", kMobileUserAgent,
        {"Content-Type: application/x-www-form-urlencoded"},
        "app_name=" + std::string(kAppName) + "&app_version=" + kAppVersion +
            "&source_token=" + UrlEncode(registration.refresh_token) +
            "&requested_token_type=access_token&source_token_type=refresh_token");
    json_error_t error {};
    JsonPtr root(json_loads(token.body.c_str(), 0, &error), &json_decref);
    const std::string access_token = Str(root.get(), "access_token");
    if (access_token.empty())
        return;
    http.Post("https://api.amazon." + market.code + "/auth/deregister", kMobileUserAgent,
              {"Content-Type: application/json", "Authorization: Bearer " + access_token},
              "{\"deregister_all_existing_accounts\":false}");
    LogRuntimeEvent("auth", "deregistered", "market=" + market.code);
}

std::string AmazonAuth::StoragePath()
{
    return AppHomePath() + "/amazon_device.json";
}

bool AmazonAuth::Load(DeviceRegistration& registration)
{
    json_error_t error {};
    JsonPtr root(json_load_file(StoragePath().c_str(), 0, &error), &json_decref);
    if (!root)
        return false;
    registration.market_code = Str(root.get(), "market");
    registration.device_serial = Str(root.get(), "device_serial");
    registration.refresh_token = Str(root.get(), "refresh_token");
    registration.customer_name = Str(root.get(), "customer_name");
    registration.cookies.clear();
    size_t index = 0;
    json_t* item = nullptr;
    json_array_foreach(json_object_get(root.get(), "cookies"), index, item)
    {
        Cookie cookie;
        cookie.domain = Str(item, "domain");
        cookie.path = Str(item, "path");
        cookie.name = Str(item, "name");
        cookie.value = Str(item, "value");
        if (!cookie.name.empty())
            registration.cookies.push_back(std::move(cookie));
    }
    return !registration.refresh_token.empty();
}

bool AmazonAuth::Save(const DeviceRegistration& registration)
{
    PrepareAppStorage();
    JsonPtr cookies(json_array(), &json_decref);
    for (const auto& cookie : registration.cookies)
        json_array_append_new(cookies.get(), json_pack(
            "{s:s, s:s, s:s, s:s}", "domain", cookie.domain.c_str(), "path", cookie.path.c_str(),
            "name", cookie.name.c_str(), "value", cookie.value.c_str()));
    JsonPtr root(json_pack("{s:s, s:s, s:s, s:s, s:O}",
                           "market", registration.market_code.c_str(),
                           "device_serial", registration.device_serial.c_str(),
                           "refresh_token", registration.refresh_token.c_str(),
                           "customer_name", registration.customer_name.c_str(),
                           "cookies", cookies.get()), &json_decref);
    char* text = json_dumps(root.get(), JSON_INDENT(1));
    if (!text)
        return false;
    const bool ok = WriteFileAtomically(StoragePath(), text);
    free(text);
    return ok;
}

void AmazonAuth::Erase()
{
    std::remove(StoragePath().c_str());
}

} // namespace lunanx::luna

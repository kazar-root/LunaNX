#include "cookie_jar.hpp"

#include "core/app_paths.hpp"

#include <jansson.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <memory>
#include <sstream>

namespace lunanx::luna
{
namespace
{

std::string Trim(std::string value)
{
    const auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool EndsWith(const std::string& value, const std::string& suffix)
{
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Règle de correspondance des domaines (RFC 6265) : « .amazon.fr » couvre luna.amazon.fr.
bool DomainMatches(const std::string& cookie_domain, const std::string& host)
{
    std::string domain = cookie_domain;
    if (!domain.empty() && domain.front() == '.')
        domain.erase(0, 1);
    return host == domain || EndsWith(host, "." + domain);
}

bool IsAmazonDomain(const std::string& domain)
{
    return domain.find("amazon.") != std::string::npos ||
           domain.find("a2z.com") != std::string::npos;
}

} // namespace

bool CookieJar::LoadDefault()
{
    const std::string home = AppHomePath();
    return LoadNetscape(home + "/cookies.txt") || LoadJson(home + "/cookies.json");
}

bool CookieJar::LoadNetscape(const std::string& path)
{
    std::ifstream stream(path);
    if (!stream.is_open())
        return false;

    cookies_.clear();
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        // Les cookies HttpOnly sont préfixés « #HttpOnly_ » : ce ne sont pas des commentaires.
        const std::string http_only = "#HttpOnly_";
        if (line.rfind(http_only, 0) == 0)
            line.erase(0, http_only.size());
        else if (line.empty() || line[0] == '#')
            continue;

        std::vector<std::string> fields;
        std::stringstream parts(line);
        std::string field;
        while (std::getline(parts, field, '\t'))
            fields.push_back(field);
        if (fields.size() < 7)
            continue;

        Cookie cookie;
        cookie.domain = Trim(fields[0]);
        cookie.path = fields[2].empty() ? "/" : fields[2];
        cookie.secure = fields[3] == "TRUE";
        cookie.expires = std::atoll(fields[4].c_str());
        cookie.name = Trim(fields[5]);
        cookie.value = Trim(fields[6]);
        Add(std::move(cookie));
    }
    source_path_ = path;
    return !cookies_.empty();
}

bool CookieJar::LoadJson(const std::string& path)
{
    json_error_t error {};
    std::unique_ptr<json_t, decltype(&json_decref)> root(
        json_load_file(path.c_str(), 0, &error), &json_decref);
    if (!root || !json_is_array(root.get()))
        return false;

    cookies_.clear();
    size_t index = 0;
    json_t* item = nullptr;
    json_array_foreach(root.get(), index, item)
    {
        const char* domain = json_string_value(json_object_get(item, "domain"));
        const char* name = json_string_value(json_object_get(item, "name"));
        const char* value = json_string_value(json_object_get(item, "value"));
        if (!domain || !name || !value)
            continue;

        Cookie cookie;
        cookie.domain = domain;
        cookie.name = name;
        cookie.value = value;
        if (const char* cookie_path = json_string_value(json_object_get(item, "path")))
            cookie.path = cookie_path;
        json_t* expires = json_object_get(item, "expirationDate");
        if (json_is_number(expires))
            cookie.expires = static_cast<long long>(json_number_value(expires));
        Add(std::move(cookie));
    }
    source_path_ = path;
    return !cookies_.empty();
}

void CookieJar::SetCookies(const std::vector<Cookie>& cookies)
{
    cookies_.clear();
    for (const auto& cookie : cookies)
        Add(cookie);
    source_path_ = "amazon_device.json";
}

void CookieJar::Add(Cookie cookie)
{
    // Seuls les cookies Amazon sont utiles : on ignore le reste de l'export.
    if (cookie.name.empty() || !IsAmazonDomain(cookie.domain))
        return;
    auto existing = std::find_if(cookies_.begin(), cookies_.end(), [&cookie](const Cookie& c) {
        return c.name == cookie.name && c.domain == cookie.domain && c.path == cookie.path;
    });
    if (existing != cookies_.end())
        *existing = std::move(cookie);
    else
        cookies_.push_back(std::move(cookie));
}

std::string CookieJar::HeaderFor(const std::string& host) const
{
    const long long now = static_cast<long long>(std::time(nullptr));
    std::string header;
    for (const auto& cookie : cookies_)
    {
        // Expiration 0 = cookie de session, conservé.
        if (cookie.expires > 0 && now > 1600000000 && cookie.expires < now)
            continue;
        if (!DomainMatches(cookie.domain, host))
            continue;
        if (!header.empty())
            header += "; ";
        header += cookie.name + "=" + cookie.value;
    }
    return header;
}

std::string CookieJar::Value(const std::string& name) const
{
    for (const auto& cookie : cookies_)
        if (cookie.name == name)
            return cookie.value;
    return {};
}

bool CookieJar::HasAmazonSession() const
{
    const bool has_session_token = !Value("session-token").empty();
    const bool has_auth = std::any_of(cookies_.begin(), cookies_.end(), [](const Cookie& c) {
        return c.name.rfind("at-", 0) == 0 || c.name.rfind("x-", 0) == 0;
    });
    return has_session_token && has_auth;
}

} // namespace lunanx::luna

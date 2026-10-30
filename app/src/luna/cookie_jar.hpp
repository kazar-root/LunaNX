#pragma once

#include <string>
#include <vector>

namespace lunanx::luna
{

struct Cookie
{
    std::string domain;
    std::string path = "/";
    std::string name;
    std::string value;
    bool secure = true;
    long long expires = 0;
};

// Cookies Amazon importés depuis le navigateur. Deux formats acceptés :
//  - cookies.txt (format Netscape, extensions « Get cookies.txt »)
//  - cookies.json (tableau d'objets {domain, name, value, ...}, extensions « Cookie-Editor »)
class CookieJar
{
  public:
    // Cherche cookies.txt puis cookies.json dans le dossier de l'application.
    bool LoadDefault();
    bool LoadNetscape(const std::string& path);
    bool LoadJson(const std::string& path);

    void SetCookies(const std::vector<Cookie>& cookies);
    bool Empty() const { return cookies_.empty(); }
    const std::string& SourcePath() const { return source_path_; }

    // En-tête « Cookie: » pour un hôte donné (respect du domaine et de l'expiration).
    std::string HeaderFor(const std::string& host) const;
    std::string Value(const std::string& name) const;
    // Vrai si les cookies de connexion Amazon indispensables sont présents.
    bool HasAmazonSession() const;

  private:
    void Add(Cookie cookie);

    std::vector<Cookie> cookies_;
    std::string source_path_;
};

} // namespace lunanx::luna

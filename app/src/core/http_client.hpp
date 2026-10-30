#pragma once

#include <string>
#include <vector>
#include <functional>

namespace lunanx
{

struct HttpResponse
{
    long status_code = 0;
    std::string body;
    // En-têtes d'erreur AWS (x-amzn-ErrorType…) : type d'erreur seulement, jamais d'identifiant.
    std::string error_type;
};

// Luna est un client web : on se présente comme un Chrome de bureau récent.
constexpr const char* kBrowserUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36";

class HttpClient
{
  public:
    HttpResponse Request(
        const std::string& method,
        const std::string& url,
        const std::string& user_agent,
        const std::vector<std::string>& headers = {},
        const std::string& body = {},
        long connect_timeout_seconds = 10,
        long timeout_seconds = 30) const;

    HttpResponse Get(
        const std::string& url,
        const std::string& user_agent,
        const std::vector<std::string>& headers = {},
        long connect_timeout_seconds = 10,
        long timeout_seconds = 30) const;

    HttpResponse Post(
        const std::string& url,
        const std::string& user_agent,
        const std::vector<std::string>& headers,
        const std::string& body) const;

    void DownloadToFile(
        const std::string& url,
        const std::string& destination,
        const std::string& user_agent,
        const std::vector<std::string>& headers = {},
        const std::function<void(long long, long long)>& on_progress = {},
        const std::function<bool()>& is_cancelled = {}) const;

    int MeasureTcpHandshakeLatencyMs(
        const std::string& url,
        long timeout_ms = 3000) const noexcept;
};

} // namespace lunanx

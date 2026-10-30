#include "http_client.hpp"

#include "runtime_journal.hpp"

#include <curl/curl.h>

#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <memory>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>

namespace lunanx
{
namespace
{

size_t WriteBody(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    const size_t bytes = size * nmemb;
    auto* body         = static_cast<std::string*>(userdata);
    body->append(ptr, bytes);
    return bytes;
}

bool IsArtworkRequest(const std::string& url)
{
    return url.find("media-amazon.com") != std::string::npos ||
           url.find("images-amazon.com") != std::string::npos;
}

bool IsSessionPollRequest(
    const std::string& method, const std::string& url)
{
    return method == "POST" &&
           url.find("/getGameSessionJob") != std::string::npos;
}

struct DownloadProgressContext
{
    const std::function<void(long long, long long)>* on_progress = nullptr;
    const std::function<bool()>* is_cancelled = nullptr;
};

int DownloadProgress(
    void* userdata,
    curl_off_t total,
    curl_off_t downloaded,
    curl_off_t,
    curl_off_t)
{
    auto* context = static_cast<DownloadProgressContext*>(userdata);
    if (context->is_cancelled && *context->is_cancelled &&
        (*context->is_cancelled)())
        return 1;
    if (context->on_progress && *context->on_progress)
        (*context->on_progress)(
            static_cast<long long>(downloaded),
            static_cast<long long>(total));
    return 0;
}

size_t CollectErrorHeader(char* buffer, size_t size, size_t count, void* user_data)
{
    const size_t length = size * count;
    std::string line(buffer, length);
    std::string lower = line;
    for (auto& c : lower)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower.rfind("x-amzn-errortype:", 0) == 0)
    {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        *static_cast<std::string*>(user_data) = line.substr(line.find(':') + 1);
    }
    return length;
}

} // namespace

HttpResponse HttpClient::Request(
    const std::string& method,
    const std::string& url,
    const std::string& user_agent,
    const std::vector<std::string>& headers,
    const std::string& body,
    long connect_timeout_seconds,
    long timeout_seconds) const
{
    const bool artwork_request = IsArtworkRequest(url);
    const bool session_poll_request = IsSessionPollRequest(method, url);
    const bool journal_request = !artwork_request && !session_poll_request;
    const auto started_at = std::chrono::steady_clock::now();
    const std::string safe_url = SanitizeRuntimeUrl(url);
    const std::uint64_t operation_id = journal_request
        ? BeginRuntimeOperation("http", method, "url=" + safe_url)
        : 0;

    CURL* raw = curl_easy_init();
    if (!raw)
    {
        if (journal_request)
            EndRuntimeOperation(operation_id, "http", method, "error", "curl_easy_init_failed");
        throw std::runtime_error("curl_easy_init failed");
    }

    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(raw, &curl_easy_cleanup);
    std::string response_body;
    std::string error_type;
    std::array<char, CURL_ERROR_SIZE> error_buffer {};

    curl_slist* raw_headers = nullptr;
    for (const auto& header : headers)
        raw_headers = curl_slist_append(raw_headers, header.c_str());

    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> header_list(
        raw_headers,
        &curl_slist_free_all);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, CollectErrorHeader);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &error_type);

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, user_agent.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, connect_timeout_seconds);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteBody);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, error_buffer.data());

    if (header_list)
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, header_list.get());

    if (method == "POST")
    {
        curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    }
    else if (method != "GET")
    {
        curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, method.c_str());
        if (!body.empty())
        {
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        }
    }

    const CURLcode result = curl_easy_perform(curl.get());
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at).count();
    if (result != CURLE_OK)
    {
        long verify_result = 0;
        char* primary_ip = nullptr;
        curl_easy_getinfo(curl.get(), CURLINFO_SSL_VERIFYRESULT, &verify_result);
        curl_easy_getinfo(curl.get(), CURLINFO_PRIMARY_IP, &primary_ip);
        std::ostringstream detail;
        detail << "url=" << safe_url
               << " curl_code=" << static_cast<int>(result)
               << " curl_error=" << curl_easy_strerror(result)
               << " ssl_verify=" << verify_result
               << " elapsed_ms=" << elapsed_ms;
        if (error_buffer[0] != '\0')
            detail << " backend_error=" << error_buffer.data();
        if (primary_ip && *primary_ip)
            detail << " primary_ip=" << primary_ip;

        if (journal_request)
            EndRuntimeOperation(operation_id, "http", method, "error", detail.str());
        else if (artwork_request)
            LogRuntimeEvent("http", "artwork_error", detail.str());
        else
            LogRuntimeEvent("http", "session_poll_error", detail.str());
        throw std::runtime_error(
            "HTTP " + method + " failed for " + safe_url + ": " +
            curl_easy_strerror(result));
    }

    long status_code = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status_code);
    if (journal_request)
    {
        EndRuntimeOperation(
            operation_id, "http", method, "ok",
            "url=" + safe_url +
            " status=" + std::to_string(status_code) +
            " bytes=" + std::to_string(response_body.size()) +
            " elapsed_ms=" + std::to_string(elapsed_ms));
    }

    return HttpResponse{
        .status_code = status_code,
        .body        = std::move(response_body),
        .error_type  = std::move(error_type),
    };
}

HttpResponse HttpClient::Get(
    const std::string& url,
    const std::string& user_agent,
    const std::vector<std::string>& headers,
    long connect_timeout_seconds,
    long timeout_seconds) const
{
    return Request(
        "GET", url, user_agent, headers, {},
        connect_timeout_seconds, timeout_seconds);
}

HttpResponse HttpClient::Post(
    const std::string& url,
    const std::string& user_agent,
    const std::vector<std::string>& headers,
    const std::string& body) const
{
    return Request("POST", url, user_agent, headers, body);
}

void HttpClient::DownloadToFile(
    const std::string& url,
    const std::string& destination,
    const std::string& user_agent,
    const std::vector<std::string>& headers,
    const std::function<void(long long, long long)>& on_progress,
    const std::function<bool()>& is_cancelled) const
{
    FILE* output = std::fopen(destination.c_str(), "wb");
    if (!output)
        throw std::runtime_error("Could not create the advertisement media file");
    std::unique_ptr<FILE, decltype(&std::fclose)> file(output, &std::fclose);

    CURL* raw = curl_easy_init();
    if (!raw)
        throw std::runtime_error("curl_easy_init failed for media download");
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(raw, &curl_easy_cleanup);

    curl_slist* raw_headers = nullptr;
    for (const auto& header : headers)
        raw_headers = curl_slist_append(raw_headers, header.c_str());
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> header_list(
        raw_headers, &curl_slist_free_all);
    std::array<char, CURL_ERROR_SIZE> error_buffer {};
    DownloadProgressContext progress {&on_progress, &is_cancelled};

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, user_agent.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 12L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, file.get());
    curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, error_buffer.data());
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, DownloadProgress);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &progress);
    if (header_list)
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, header_list.get());

    const CURLcode result = curl_easy_perform(curl.get());
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    file.reset();
    if (result != CURLE_OK || status < 200 || status >= 300)
    {
        std::remove(destination.c_str());
        if (result == CURLE_ABORTED_BY_CALLBACK && is_cancelled && is_cancelled())
            throw std::runtime_error("Advertisement download cancelled");
        const std::string detail = error_buffer[0] != '\0'
            ? std::string(error_buffer.data())
            : std::string(curl_easy_strerror(result));
        throw std::runtime_error(
            "Advertisement download failed (HTTP " + std::to_string(status) +
            "): " + detail);
    }
}

int HttpClient::MeasureTcpHandshakeLatencyMs(
    const std::string& url,
    long timeout_ms) const noexcept
{
    CURL* raw = curl_easy_init();
    if (!raw)
        return -1;

    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(raw, &curl_easy_cleanup);
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_CONNECT_ONLY, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_FRESH_CONNECT, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);

    if (curl_easy_perform(curl.get()) != CURLE_OK)
        return -1;

    // A region estimate should reflect the network path, not DNS lookup or
    // variable TLS negotiation. TCP connect is approximately one network RTT.
    double lookup_seconds = 0.0;
    double connect_seconds = 0.0;
    if (curl_easy_getinfo(curl.get(), CURLINFO_NAMELOOKUP_TIME, &lookup_seconds) != CURLE_OK)
        return -1;
    if (curl_easy_getinfo(curl.get(), CURLINFO_CONNECT_TIME, &connect_seconds) != CURLE_OK)
        return -1;
    const double handshake_seconds = connect_seconds - lookup_seconds;
    if (handshake_seconds <= 0.0)
        return -1;

    return static_cast<int>(std::lround(handshake_seconds * 1000.0));
}

} // namespace lunanx

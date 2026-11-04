#include "phone_sign_in.hpp"

#include "core/i18n.hpp"
#include "core/runtime_journal.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <utility>

namespace lunanx
{
namespace
{

constexpr int kFirstPort = 8080;
constexpr int kPortCount = 10;
constexpr size_t kMaxRequestBytes = 32 * 1024;

std::string RandomToken()
{
    // Sans caractères ambigus (0/o, 1/l/i) pour pouvoir taper l'adresse à la main.
    static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
    unsigned char bytes[6];
#ifdef __SWITCH__
    randomGet(bytes, sizeof(bytes));
#else
    std::random_device device;
    for (auto& byte : bytes)
        byte = static_cast<unsigned char>(device());
#endif
    std::string token;
    for (unsigned char byte : bytes)
        token.push_back(alphabet[byte % (sizeof(alphabet) - 1)]);
    return token;
}

std::string HtmlEscape(const std::string& text)
{
    std::string out;
    for (char c : text)
    {
        switch (c)
        {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        case '\n': out += "<br>"; break;
        default: out.push_back(c);
        }
    }
    return out;
}

// Décodage d'un champ de formulaire (application/x-www-form-urlencoded).
std::string FormDecode(const std::string& value)
{
    std::string out;
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '+')
            out.push_back(' ');
        else if (value[i] == '%' && i + 2 < value.size() && std::isxdigit(static_cast<unsigned char>(value[i + 1])) &&
                 std::isxdigit(static_cast<unsigned char>(value[i + 2])))
        {
            out.push_back(static_cast<char>(std::stoi(value.substr(i + 1, 2), nullptr, 16)));
            i += 2;
        }
        else
            out.push_back(value[i]);
    }
    return out;
}

std::string FormField(const std::string& body, const std::string& name)
{
    size_t start = 0;
    while (start <= body.size())
    {
        size_t end = body.find('&', start);
        if (end == std::string::npos)
            end = body.size();
        const std::string pair = body.substr(start, end - start);
        const size_t equals = pair.find('=');
        if (equals != std::string::npos && pair.substr(0, equals) == name)
            return FormDecode(pair.substr(equals + 1));
        start = end + 1;
    }
    return {};
}

std::string StorePage(const std::string& body_html)
{
    return "<!doctype html><html lang=\"" + UiLanguage() +
           "\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
           "<title>LunaNX</title><style>"
           "body{font-family:-apple-system,system-ui,Roboto,sans-serif;background:#120f1c;color:#eceaf6;"
           "margin:0;padding:18px;line-height:1.45}"
           ".card{max-width:540px;margin:0 auto;background:#1c1830;border-radius:16px;padding:20px}"
           "h1{font-size:1.3em;margin:0 0 6px}.store{color:#b096ff;margin:0 0 14px}"
           "a.btn,button{display:block;width:100%;box-sizing:border-box;text-align:center;"
           "background:#7c5cff;color:#fff;border:0;border-radius:26px;padding:15px;font-size:1.05em;"
           "font-weight:600;text-decoration:none;margin:14px 0}"
           "textarea{width:100%;box-sizing:border-box;min-height:120px;border-radius:12px;"
           "border:1px solid #574e80;background:#0d0b16;color:#fff;padding:10px;font-size:.95em}"
           ".err{background:#4a1f24;color:#ffc9b8;padding:12px;border-radius:12px;margin:10px 0}"
           ".ok{font-size:1.15em;text-align:center;padding:24px 4px}p{margin:10px 0}"
           "</style></head><body><div class=\"card\">" +
           body_html + "</div></body></html>";
}

std::string HttpResponse(int status, const std::string& body)
{
    const char* reason = status == 200 ? "OK" : "Not Found";
    return "HTTP/1.1 " + std::to_string(status) + " " + reason +
           "\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\n"
           "Connection: close\r\nContent-Length: " +
           std::to_string(body.size()) + "\r\n\r\n" + body;
}

void SendAll(int client, const std::string& data)
{
    size_t sent = 0;
    while (sent < data.size())
    {
        const ssize_t n = send(client, data.data() + sent, data.size() - sent, 0);
        if (n <= 0)
            return;
        sent += static_cast<size_t>(n);
    }
}

std::string ToLower(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

} // namespace

PhoneSignInServer::PhoneSignInServer(luna::PendingSignIn pending, CodeCallback on_code)
    : pending_(std::move(pending))
    , on_code_(std::move(on_code))
    , token_(RandomToken())
{
}

PhoneSignInServer::~PhoneSignInServer()
{
    Stop();
}

bool PhoneSignInServer::Start(std::string& error)
{
    std::string ip;
#ifdef __SWITCH__
    u32 address = 0;
    if (R_FAILED(nifmGetCurrentIpAddress(&address)) || address == 0)
    {
        error = Tr("La Switch n'est pas connectée au réseau. Vérifie le Wi-Fi, puis réessaie.");
        return false;
    }
    char text[32];
    std::snprintf(text, sizeof(text), "%u.%u.%u.%u", address & 0xff, (address >> 8) & 0xff,
                  (address >> 16) & 0xff, (address >> 24) & 0xff);
    ip = text;
#else
    ip = "127.0.0.1";
#endif

    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0)
    {
        error = TrFormat("Impossible de démarrer la connexion par téléphone ({}).", {"socket"});
        return false;
    }
    int yes = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    int port = -1;
    for (int candidate = kFirstPort; candidate < kFirstPort + kPortCount; ++candidate)
    {
        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(candidate));
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0)
        {
            port = candidate;
            break;
        }
    }
    if (port < 0 || listen(listen_fd_, 4) != 0)
    {
        close(listen_fd_);
        listen_fd_ = -1;
        error = TrFormat("Impossible de démarrer la connexion par téléphone ({}).", {"bind"});
        return false;
    }

    address_ = "http://" + ip + ":" + std::to_string(port) + "/" + token_;

    // Pages servies au téléphone, préparées ici dans la langue de LunaNX.
    const luna::AmazonMarket& market = luna::FindAmazonMarket(pending_.market_code);
    auto page = [&](const std::string& error_html) {
        return StorePage(
            "<h1>" + HtmlEscape(Tr("Connexion à Amazon pour LunaNX")) + "</h1>"
            "<p class=\"store\">amazon." + HtmlEscape(market.code) + "</p>" + error_html +
            "<a class=\"btn\" href=\"" + HtmlEscape(pending_.sign_in_url) + "\" target=\"_blank\" rel=\"noopener\">" +
            HtmlEscape(Tr("1. Ouvrir la page de connexion Amazon")) + "</a>"
            "<p>" + HtmlEscape(Tr("2. Connecte-toi avec ton e-mail, ton mot de passe et, si besoin, le code reçu. "
                                  "À la fin, Amazon affiche une page introuvable ou une erreur : c'est normal.")) +
            "</p><p>" +
            HtmlEscape(Tr("3. Copie l'adresse complète de cette dernière page (barre d'adresse), reviens sur cet "
                          "onglet et colle-la ici :")) +
            "</p><form method=\"post\"><textarea name=\"url\" autocapitalize=\"off\" autocorrect=\"off\" "
            "spellcheck=\"false\" placeholder=\"https://www.amazon." + HtmlEscape(market.code) +
            "/ap/maplanding?...\"></textarea><button type=\"submit\">" + HtmlEscape(Tr("Envoyer à la Switch")) +
            "</button></form>");
    };
    page_html_ = page("");
    page_error_html_ = page("<div class=\"err\">" +
                            HtmlEscape(Tr("Cette adresse ne contient pas de code de connexion Amazon. Copie bien "
                                          "l'adresse complète de la dernière page, puis réessaie.")) +
                            "</div>");
    done_html_ = StorePage("<div class=\"ok\">" +
                           HtmlEscape(Tr("C'est bon ! Retourne sur ta Switch : LunaNX termine la connexion.")) +
                           "</div>");

    stop_ = false;
    thread_ = std::thread(&PhoneSignInServer::Run, this);
    LogRuntimeEvent("auth", "phone_signin_started", "market=" + market.code + " port=" + std::to_string(port));
    return true;
}

void PhoneSignInServer::Stop()
{
    stop_ = true;
    if (thread_.joinable())
        thread_.join();
    if (listen_fd_ >= 0)
    {
        close(listen_fd_);
        listen_fd_ = -1;
    }
}

void PhoneSignInServer::Run()
{
    while (!stop_)
    {
        pollfd descriptor {};
        descriptor.fd = listen_fd_;
        descriptor.events = POLLIN;
        const int ready = poll(&descriptor, 1, 250);
        if (ready < 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        if (ready == 0 || (descriptor.revents & POLLIN) == 0)
            continue;
        const int client = accept(listen_fd_, nullptr, nullptr);
        if (client < 0)
            continue;
        HandleClient(client);
        close(client);
    }
}

void PhoneSignInServer::HandleClient(int client)
{
    timeval timeout {};
    timeout.tv_sec = 3;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    std::string request;
    size_t header_end = std::string::npos;
    size_t content_length = 0;
    char buffer[2048];
    while (request.size() < kMaxRequestBytes && !stop_)
    {
        const ssize_t n = recv(client, buffer, sizeof(buffer), 0);
        if (n <= 0)
            break;
        request.append(buffer, static_cast<size_t>(n));
        if (header_end == std::string::npos)
        {
            header_end = request.find("\r\n\r\n");
            if (header_end != std::string::npos)
            {
                const std::string headers = ToLower(request.substr(0, header_end));
                const size_t pos = headers.find("\r\ncontent-length:");
                if (pos != std::string::npos)
                    content_length = std::strtoul(headers.c_str() + pos + 17, nullptr, 10);
                if (content_length > kMaxRequestBytes)
                    content_length = kMaxRequestBytes;
            }
        }
        if (header_end != std::string::npos && request.size() >= header_end + 4 + content_length)
            break;
    }
    if (header_end == std::string::npos)
        return;

    // Ligne de requête : « GET /jeton HTTP/1.1 ».
    const size_t line_end = request.find("\r\n");
    const std::string line = request.substr(0, line_end);
    const size_t first_space = line.find(' ');
    const size_t second_space = line.find(' ', first_space + 1);
    if (first_space == std::string::npos || second_space == std::string::npos)
        return;
    const std::string method = line.substr(0, first_space);
    std::string path = line.substr(first_space + 1, second_space - first_space - 1);
    path = path.substr(0, path.find('?'));
    if (path.size() > 1 && path.back() == '/')
        path.pop_back();

    if (path != "/" + token_)
    {
        SendAll(client, HttpResponse(404, "Not found"));
        return;
    }
    if (code_received_)
    {
        SendAll(client, HttpResponse(200, done_html_));
        return;
    }
    if (method != "POST")
    {
        LogRuntimeEvent("auth", "phone_page_served");
        SendAll(client, HttpResponse(200, page_html_));
        return;
    }

    const std::string body = request.substr(header_end + 4, content_length);
    const std::string pasted = FormField(body, "url");
    const std::string code = luna::AmazonAuth::ExtractAuthorizationCode(pasted);
    if (code.empty())
    {
        // Jamais le contenu collé dans le journal : seulement sa taille.
        LogRuntimeEvent("auth", "phone_url_without_code", "length=" + std::to_string(pasted.size()));
        SendAll(client, HttpResponse(200, page_error_html_));
        return;
    }
    if (code_received_.exchange(true))
    {
        SendAll(client, HttpResponse(200, done_html_));
        return;
    }
    LogRuntimeEvent("auth", "phone_code_received");
    SendAll(client, HttpResponse(200, done_html_));
    if (on_code_)
        on_code_(code);
}

} // namespace lunanx

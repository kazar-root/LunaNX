#pragma once

#include "luna/amazon_auth.hpp"

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace lunanx
{

// Connexion depuis un téléphone, sans le navigateur de la Switch.
//
// La Switch ouvre un petit serveur web sur le réseau Wi-Fi local. Le téléphone y accède
// (QR code), ouvre la vraie page de connexion Amazon dans son propre navigateur, puis colle
// l'adresse de la page finale, qui contient le code d'autorisation. La suite est identique à
// la connexion par le navigateur de la Switch (AmazonAuth::Register).
// LunaNX ne voit jamais le mot de passe : il est saisi uniquement sur le site d'Amazon.
class PhoneSignInServer
{
  public:
    // Appelé (depuis le fil du serveur) quand un code d'autorisation valide est reçu.
    using CodeCallback = std::function<void(const std::string& authorization_code)>;

    PhoneSignInServer(luna::PendingSignIn pending, CodeCallback on_code);
    ~PhoneSignInServer();

    PhoneSignInServer(const PhoneSignInServer&) = delete;
    PhoneSignInServer& operator=(const PhoneSignInServer&) = delete;

    // Ouvre le port d'écoute et démarre le serveur ; false (avec un message) en cas d'échec.
    bool Start(std::string& error);
    // Arrête le serveur et attend la fin de son fil. Sans effet s'il est déjà arrêté.
    void Stop();

    // Adresse à ouvrir sur le téléphone, par exemple « http://192.168.1.20:8080/k7m2qx ».
    const std::string& Address() const { return address_; }

  private:
    void Run();
    void HandleClient(int client);

    luna::PendingSignIn pending_;
    CodeCallback on_code_;
    std::string token_;
    std::string address_;
    int listen_fd_ = -1;
    std::thread thread_;
    std::atomic<bool> stop_ {false};
    std::atomic<bool> code_received_ {false};

    // Pages préparées sur le fil principal (les traductions n'y sont lues qu'une fois).
    std::string page_html_;
    std::string page_error_html_;
    std::string done_html_;
};

} // namespace lunanx

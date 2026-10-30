#pragma once

#include "luna/amazon_auth.hpp"

#include <string>

namespace lunanx
{

// Ouvre la page de connexion Amazon dans le navigateur intégré de la Switch et renvoie
// l'URL finale (celle qui contient le code d'autorisation). Chaîne vide si l'utilisateur
// a fermé le navigateur. Lève std::runtime_error si le navigateur est indisponible.
std::string OpenAmazonWebSignIn(const luna::PendingSignIn& pending);

} // namespace lunanx

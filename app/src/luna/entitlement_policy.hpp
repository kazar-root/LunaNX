// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=422be2513c87f835fc78
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include "luna_models.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace lunanx::luna
{

// Règles d'accès aux jeux selon l'abonnement Luna du compte.
//
// Luna propose un catalogue inclus avec Prime (« Luna Standard ») et des jeux réservés à
// l'abonnement Luna Premium (anciennement Luna+). Les tuiles des jeux Premium portent une
// étiquette (upTopTag / badge) qui le dit ; le menu du joueur donne le statut du compte
// (customerPlanStatus). Les deux sont des textes localisés : on y cherche les marqueurs
// connus de l'offre Premium, dans toutes les langues de Luna.

inline std::string LowercaseAscii(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// Vrai si le texte (étiquette, nom de fichier d'icône, statut) désigne l'offre Luna Premium.
inline bool IsPremiumMarker(const std::string& text)
{
    if (text.empty())
        return false;
    const std::string lower = LowercaseAscii(text);
    for (const char* marker : {"premium", "luna+", "luna +", "luna plus", "lunaplus", "luna_plus",
                               "luna-plus", "luna_premium", "lunapremium"})
        if (lower.find(marker) != std::string::npos)
            return true;
    return false;
}

inline AccountPlan PlanFromStatus(const std::string& status)
{
    if (status.empty())
        return AccountPlan::Unknown;
    return IsPremiumMarker(status) ? AccountPlan::Premium : AccountPlan::Standard;
}

// Le jeu fait partie de l'abonnement du compte. Statut inconnu : on ne masque rien.
inline bool IsIncludedInPlan(const Game& game, AccountPlan plan)
{
    return !game.premium_only || plan != AccountPlan::Standard;
}

} // namespace lunanx::luna

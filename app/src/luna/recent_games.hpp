#pragma once

#include "luna_models.hpp"

#include <vector>

namespace lunanx::luna
{

// Historique local des derniers jeux lancés depuis LunaNX (« Reprendre »), du plus récent
// au plus ancien. Seules les informations publiques du jeu sont gardées (titre, ASIN,
// adresse de la fiche, visuels) : les jetons de session sont redemandés au lancement.
std::vector<Game> LoadRecentGames();
void RecordRecentGame(const Game& game);

} // namespace lunanx::luna

// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=fc197239ca550783fd0a
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

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
// Derniers lancements locaux d'abord, puis « Continuer à jouer » de Luna, sans doublon.
// La limite de dix jeux concerne le stockage local, pas la liste fournie par Luna.
// Une rangée « récemment ajoutés » ne constitue pas un historique de parties.
// Retrouver les liens actuels du catalogue avant de filtrer une ancienne entrée sans ASIN.
std::vector<Game> RecentGamesForShelves(const std::vector<Shelf>& shelves, const std::vector<Game>& local,
                                      const std::vector<Game>& catalog = {});

} // namespace lunanx::luna

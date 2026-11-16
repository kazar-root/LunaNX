// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=701f5434421ccd37daf3
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include "luna_models.hpp"

namespace lunanx::luna
{

// Un jeu arrive souvent d'abord en bannière, puis en jaquette verticale dans une autre
// rangée. La déduplication doit conserver les deux formats pour leur usage respectif.
inline bool MergeGameArtwork(Game& known, const Game& incoming)
{
    bool changed = false;
    if (known.image_portrait.empty() && !incoming.image_portrait.empty())
    {
        known.image_portrait = incoming.image_portrait;
        changed = true;
    }
    if (known.image_landscape.empty() && !incoming.image_landscape.empty())
    {
        known.image_landscape = incoming.image_landscape;
        changed = true;
    }
    return changed;
}

} // namespace lunanx::luna

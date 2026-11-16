// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=7a9372b0852e2d09fe29
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include <string>

namespace lunanx
{

// Dossier de travail sur la carte SD : réglages, cookies, cache, journaux.
const std::string& AppHomePath();
void PrepareAppStorage();

} // namespace lunanx

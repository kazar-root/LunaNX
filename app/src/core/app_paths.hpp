#pragma once

#include <string>

namespace lunanx
{

// Dossier de travail sur la carte SD : réglages, cookies, cache, journaux.
const std::string& AppHomePath();
void PrepareAppStorage();

} // namespace lunanx

#pragma once

#include <initializer_list>
#include <string>

namespace lunanx
{

// Langue de l'interface et du catalogue : "en" (par défaut) ou "fr".
void SetUiLanguage(const std::string& code);
const std::string& UiLanguage();

// Le texte français du code sert de clé : renvoie sa traduction anglaise quand l'interface
// est en anglais (le texte français tel quel sinon, ou si la traduction manque).
std::string Tr(const std::string& french);

// Comme Tr, puis remplace chaque « {} » par l'argument suivant.
std::string TrFormat(const std::string& french, std::initializer_list<std::string> args);

} // namespace lunanx

#pragma once

#include <initializer_list>
#include <string>
#include <vector>

namespace lunanx
{

// Langue proposée dans les réglages : code interne et nom affiché dans sa propre langue.
struct UiLanguageOption
{
    const char* code;
    const char* name;
};

// Langues de l'interface, dans l'ordre alphabétique de leur nom français.
const std::vector<UiLanguageOption>& UiLanguageOptions();
bool IsSupportedUiLanguage(const std::string& code);

// Langue de l'interface et du catalogue : "en" par défaut, ou un code de UiLanguageOptions().
void SetUiLanguage(const std::string& code);
const std::string& UiLanguage();

// Le texte français du code sert de clé : renvoie sa traduction dans la langue de l'interface
// (l'anglais si la traduction manque, le français tel quel si l'interface est en français).
std::string Tr(const std::string& french);

// Comme Tr, puis remplace chaque « {} » par l'argument suivant.
std::string TrFormat(const std::string& french, std::initializer_list<std::string> args);

} // namespace lunanx

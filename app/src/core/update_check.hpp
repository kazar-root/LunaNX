// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=ffb8084129be4a1e0791
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace lunanx
{

struct UpdateCheckResult
{
    bool ok = false;
    bool update_available = false;
    std::string latest_version;
};

// Numéros d'une version (« v2.0.2 » → 2, 0, 2) ; vide si le texte n'en contient pas.
inline std::vector<int> ParseVersionNumbers(const std::string& text)
{
    std::vector<int> numbers;
    size_t i = !text.empty() && (text[0] == 'v' || text[0] == 'V') ? 1 : 0;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])))
    {
        int value = 0;
        for (; i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])); ++i)
            value = std::min(value * 10 + (text[i] - '0'), 99999999);
        numbers.push_back(value);
        if (i >= text.size() || text[i] != '.')
            break;
        ++i;
    }
    return numbers;
}

// Négatif si a < b, zéro si égales, positif si a > b (2.0 et 2.0.0 sont égales).
inline int CompareVersions(const std::string& a, const std::string& b)
{
    const auto left = ParseVersionNumbers(a);
    const auto right = ParseVersionNumbers(b);
    for (size_t i = 0; i < std::max(left.size(), right.size()); ++i)
    {
        const int l = i < left.size() ? left[i] : 0;
        const int r = i < right.size() ? right[i] : 0;
        if (l != r)
            return l < r ? -1 : 1;
    }
    return 0;
}

// Interroge la dernière version publiée sur GitHub (bloquant : à appeler hors de l'interface).
UpdateCheckResult CheckForUpdate();

} // namespace lunanx

// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=8332bea8242e59299192
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include <string>

namespace lunanx
{

// Les images Amazon acceptent un suffixe de redimensionnement : « nom._SX420_.png ».
// Télécharger directement la bonne taille économise la mémoire et la bande passante.
inline std::string ResizeAmazonImageUrl(std::string image_url, int width)
{
    if (width <= 0 || image_url.find("media-amazon.com/images/") == std::string::npos)
        return image_url;
    const size_t suffix = image_url.find_first_of("?#");
    const size_t path_end = suffix == std::string::npos ? image_url.size() : suffix;
    const size_t slash = image_url.rfind('/', path_end);
    const size_t dot = image_url.find('.', slash == std::string::npos ? 0 : slash);
    if (dot == std::string::npos || dot >= path_end)
        return image_url;
    const size_t extension = image_url.rfind('.', path_end);
    if (dot != extension)
    {
        const std::string transform = image_url.substr(dot + 1, extension - dot - 1);
        // Les recettes Amazon peuvent ajouter un logo ou définir un recadrage. Les
        // supprimer change le visuel (ex. GameNight) ; seule une largeur simple est remplacée.
        bool width_only = transform.size() > 4 && transform.rfind("_SX", 0) == 0 && transform.back() == '_';
        for (size_t i = 3; width_only && i + 1 < transform.size(); ++i)
            width_only = transform[i] >= '0' && transform[i] <= '9';
        if (!width_only)
            return image_url;
    }
    return image_url.substr(0, dot) + "._SX" + std::to_string(width) + "_" +
           image_url.substr(extension);
}

} // namespace lunanx

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
    const size_t slash = image_url.find_last_of('/');
    const size_t dot = image_url.find('.', slash == std::string::npos ? 0 : slash);
    if (dot == std::string::npos)
        return image_url;
    const size_t extension = image_url.find_last_of('.');
    return image_url.substr(0, dot) + "._SX" + std::to_string(width) + "_" +
           image_url.substr(extension);
}

} // namespace lunanx

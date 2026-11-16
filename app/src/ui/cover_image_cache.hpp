// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=cf5024b1ee9a7e1c2287
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include "image_url_policy.hpp"

#include <borealis.hpp>

#include <functional>
#include <string>

namespace lunanx
{

// Jaquette verticale de la bibliothèque, téléchargée en petite taille.
void SetCachedPosterImage(brls::Image* image, const std::string& image_url);
void SetCachedAvatarImage(brls::Image* image, const std::string& image_url);
void SetCachedGalleryImage(
    brls::Image* image,
    const std::string& image_url,
    std::function<bool()> should_apply = {});
void ShutdownImageCache();

} // namespace lunanx

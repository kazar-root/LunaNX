#pragma once

#include "image_url_policy.hpp"

#include <borealis.hpp>

#include <functional>
#include <string>

namespace lunanx
{

// Jaquette verticale de la bibliothèque, téléchargée en petite taille.
void SetCachedPosterImage(brls::Image* image, const std::string& image_url);
void SetCachedGalleryImage(
    brls::Image* image,
    const std::string& image_url,
    std::function<bool()> should_apply = {});
void ShutdownImageCache();

} // namespace lunanx

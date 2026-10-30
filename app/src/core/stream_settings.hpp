#pragma once

#include <string>
#include <vector>

namespace lunanx
{

struct StreamSettings
{
    std::string preset_id = "720p60";
    std::string label     = "720p · 60 i/s";
    int width             = 1280;
    int height            = 720;
    int fps               = 60;
    int bitrate_kbps      = 10000;
    // "Auto" ou un code de région Luna (ex. "eu-west-3").
    std::string region    = "Auto";
    bool audio_enabled    = true;
    int audio_volume      = 1200;
    int audio_buffer_ms   = 40;
    // Fixé à "Software" (voir Normalize) ; "Auto" et "NVDEC" restent gérés par le décodeur.
    std::string video_backend = "Software";
    bool debug_diagnostics = false;
    // "Switch" : A Switch = A Xbox (étiquette, par défaut) ; "Xbox" : A Switch = B Xbox (position).
    std::string controller_layout = "Switch";
    std::string image_quality_mode = "Adaptive";
    // Boutique Amazon utilisée pour la connexion directe (« fr », « de », « com »...).
    std::string amazon_market = "fr";
    // Bibliothèque : "Covers" (jaquettes, Top 10, Reprendre) ou "List" (liste simple de la 1.0).
    std::string library_layout = "Covers";
};

const std::vector<StreamSettings>& StreamPresets();
StreamSettings LoadStreamSettings();
bool SaveStreamSettings(const StreamSettings& settings);
StreamSettings ApplyStreamPreset(const StreamSettings& current, const std::string& preset_id);
std::string FormatStreamSettings(const StreamSettings& settings);

} // namespace lunanx

#include "stream_settings.hpp"

#include "app_paths.hpp"
#include "atomic_file_replace.hpp"

#include <jansson.h>

#include <algorithm>
#include <memory>

namespace lunanx
{
namespace
{

using JsonPtr = std::unique_ptr<json_t, decltype(&json_decref)>;

std::string SettingsPath()
{
    return AppHomePath() + "/stream_settings.json";
}

std::string JsonField(json_t* object, const char* key, const std::string& fallback)
{
    json_t* value = json_object_get(object, key);
    return json_is_string(value) ? json_string_value(value) : fallback;
}

int JsonInt(json_t* object, const char* key, int fallback)
{
    json_t* value = json_object_get(object, key);
    return json_is_integer(value) ? static_cast<int>(json_integer_value(value)) : fallback;
}

bool JsonBool(json_t* object, const char* key, bool fallback)
{
    json_t* value = json_object_get(object, key);
    return json_is_boolean(value) ? json_is_true(value) : fallback;
}

bool OneOf(const std::string& value, std::initializer_list<const char*> allowed)
{
    return std::any_of(allowed.begin(), allowed.end(),
                       [&value](const char* item) { return value == item; });
}

StreamSettings Normalize(StreamSettings settings)
{
    // Réglages fixes, non proposés dans l'interface : 720p 60 i/s, décodeur logiciel et
    // traitement adaptatif, la combinaison la plus fluide sur Switch. Ils remplacent toute
    // valeur enregistrée par une version précédente.
    settings.preset_id = "720p60";
    settings.video_backend = "Software";
    settings.image_quality_mode = "Adaptive";
    settings = ApplyStreamPreset(settings, settings.preset_id);
    settings.audio_volume = std::clamp(settings.audio_volume, 100, 1600);
    settings.audio_buffer_ms = std::clamp(settings.audio_buffer_ms, 30, 100);
    if (!OneOf(settings.controller_layout, {"Xbox", "Switch"}))
        settings.controller_layout = "Switch";
    if (!OneOf(settings.library_layout, {"Covers", "List"}))
        settings.library_layout = "Covers";
    if (settings.region.empty())
        settings.region = "Auto";
    return settings;
}

} // namespace

const std::vector<StreamSettings>& StreamPresets()
{
    static const std::vector<StreamSettings> presets = [] {
        StreamSettings p720;
        p720.preset_id = "720p60";
        p720.label = "720p · 60 i/s";
        p720.width = 1280;
        p720.height = 720;
        p720.bitrate_kbps = 10000;

        StreamSettings p1080;

        return std::vector<StreamSettings> {p720, p1080};
    }();
    return presets;
}

StreamSettings ApplyStreamPreset(const StreamSettings& current, const std::string& preset_id)
{
    StreamSettings result = current;
    const auto& presets = StreamPresets();
    auto found = std::find_if(presets.begin(), presets.end(),
                              [&preset_id](const StreamSettings& p) { return p.preset_id == preset_id; });
    const StreamSettings& preset = found != presets.end() ? *found : presets.back();
    result.preset_id = preset.preset_id;
    result.label = preset.label;
    result.width = preset.width;
    result.height = preset.height;
    result.fps = preset.fps;
    result.bitrate_kbps = preset.bitrate_kbps;
    return result;
}

StreamSettings LoadStreamSettings()
{
    StreamSettings settings;
    json_error_t error {};
    JsonPtr root(json_load_file(SettingsPath().c_str(), 0, &error), &json_decref);
    if (!root || !json_is_object(root.get()))
        return Normalize(settings);

    json_t* r = root.get();
    settings.preset_id = JsonField(r, "preset", settings.preset_id);
    settings.region = JsonField(r, "region", settings.region);
    settings.audio_enabled = JsonBool(r, "audio_enabled", settings.audio_enabled);
    settings.audio_volume = JsonInt(r, "audio_volume", settings.audio_volume);
    settings.audio_buffer_ms = JsonInt(r, "audio_buffer_ms", settings.audio_buffer_ms);
    settings.video_backend = JsonField(r, "video_backend", settings.video_backend);
    settings.debug_diagnostics = JsonBool(r, "debug_diagnostics", settings.debug_diagnostics);
    settings.controller_layout = JsonField(r, "controller_layout", settings.controller_layout);
    settings.image_quality_mode = JsonField(r, "image_quality_mode", settings.image_quality_mode);
    settings.library_layout = JsonField(r, "library_layout", settings.library_layout);
    settings.amazon_market = JsonField(r, "amazon_market", settings.amazon_market);
    return Normalize(settings);
}

bool SaveStreamSettings(const StreamSettings& settings)
{
    PrepareAppStorage();
    const StreamSettings clean = Normalize(settings);
    JsonPtr root(json_object(), &json_decref);
    json_t* r = root.get();
    json_object_set_new(r, "preset", json_string(clean.preset_id.c_str()));
    json_object_set_new(r, "region", json_string(clean.region.c_str()));
    json_object_set_new(r, "audio_enabled", json_boolean(clean.audio_enabled));
    json_object_set_new(r, "audio_volume", json_integer(clean.audio_volume));
    json_object_set_new(r, "audio_buffer_ms", json_integer(clean.audio_buffer_ms));
    json_object_set_new(r, "video_backend", json_string(clean.video_backend.c_str()));
    json_object_set_new(r, "debug_diagnostics", json_boolean(clean.debug_diagnostics));
    json_object_set_new(r, "controller_layout", json_string(clean.controller_layout.c_str()));
    json_object_set_new(r, "image_quality_mode", json_string(clean.image_quality_mode.c_str()));
    json_object_set_new(r, "library_layout", json_string(clean.library_layout.c_str()));
    json_object_set_new(r, "amazon_market", json_string(clean.amazon_market.c_str()));

    char* text = json_dumps(r, JSON_INDENT(2));
    if (!text)
        return false;
    const bool ok = WriteFileAtomically(SettingsPath(), text);
    free(text);
    return ok;
}

std::string FormatStreamSettings(const StreamSettings& settings)
{
    return settings.label + " · " + std::to_string(settings.bitrate_kbps / 1000) +
           " Mb/s · région " + settings.region;
}

} // namespace lunanx

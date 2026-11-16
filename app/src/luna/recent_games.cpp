// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=ef06ff4acc3c7ac2c7f7
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).
#include "core/source_origin.hpp"
LUNANX_SOURCE_ORIGIN(uef06ff4acc3c7ac2c7f7, "ef06ff4acc3c7ac2c7f7")

#include "recent_games.hpp"

#include "core/app_paths.hpp"
#include "core/atomic_file_replace.hpp"
#include "core/runtime_journal.hpp"

#include <jansson.h>

#include <cstdlib>
#include <memory>
#include <algorithm>
#include <cctype>
#include <set>

namespace lunanx::luna
{
namespace
{

constexpr size_t kMaximumRecentGames = 10;

std::string RecentGamesPath()
{
    return AppHomePath() + "/recent_games.json";
}

std::string Field(json_t* object, const char* key)
{
    const char* value = json_string_value(json_object_get(object, key));
    return value ? value : "";
}

bool SameGame(const Game& a, const Game& b)
{
    if (!a.asin.empty() && !b.asin.empty())
        return a.asin == b.asin;
    if (!a.game_id.empty() && !b.game_id.empty())
        return a.game_id == b.game_id;
    const auto title = NormalizedGameTitle(a.title);
    return !title.empty() && title == NormalizedGameTitle(b.title);
}

bool IsContinuePlayingShelf(const Shelf& shelf)
{
    if (shelf.recently_played || shelf.row_type.rfind("RECENTLY_PLAYED", 0) == 0 ||
        shelf.row_type.rfind("CONTINUE_PLAYING", 0) == 0)
        return true;

    // Luna utilise aussi un CAROUSEL générique, identifié par son titre localisé.
    // Normaliser les espaces insécables et les accents, y compris leur forme décomposée.
    std::string title = shelf.title;
    for (const auto& [from, to] : {
            std::pair {"\u00a0", " "}, {"\u202f", " "}, {"à", "a"}, {"À", "a"},
            {"é", "e"}, {"É", "e"}, {"\u0300", ""}, {"\u0301", ""}})
    {
        size_t position = 0;
        while ((position = title.find(from, position)) != std::string::npos)
        {
            title.replace(position, std::char_traits<char>::length(from), to);
            position += std::char_traits<char>::length(to);
        }
    }
    std::string normalized;
    bool space = false;
    for (unsigned char c : title)
    {
        if (std::isspace(c))
        {
            space = !normalized.empty();
            continue;
        }
        if (space)
            normalized.push_back(' ');
        space = false;
        normalized.push_back(static_cast<char>(std::tolower(c)));
    }
    for (const char* label : {"reprendre", "recently played", "continue playing", "joues recemment",
                              "joue recemment", "continuer a jouer"})
        if (normalized.find(label) != std::string::npos)
            return true;
    return false;
}

} // namespace

std::vector<Game> LoadRecentGames()
{
    std::vector<Game> games;
    json_error_t error {};
    std::unique_ptr<json_t, decltype(&json_decref)> root(
        json_load_file(RecentGamesPath().c_str(), 0, &error), &json_decref);
    if (!json_is_array(root.get()))
        return games;

    size_t index = 0;
    json_t* item = nullptr;
    json_array_foreach(root.get(), index, item)
    {
        Game game;
        game.asin = Field(item, "asin");
        game.game_id = Field(item, "game_id");
        game.title = Field(item, "title");
        game.page_uri = Field(item, "page_uri");
        game.image_portrait = Field(item, "image_portrait");
        game.image_landscape = Field(item, "image_landscape");
        if (!game.title.empty() && games.size() < kMaximumRecentGames)
            games.push_back(std::move(game));
    }
    return games;
}

std::vector<Game> RecentGamesForShelves(const std::vector<Shelf>& shelves, const std::vector<Game>& local,
                                      const std::vector<Game>& catalog)
{
    std::vector<Game> games;
    std::set<std::string> seen;
    auto append = [&](const Game& game) {
        if (!game.title.empty() &&
            (!game.asin.empty() || !game.page_uri.empty() || !game.detail_service_token.empty() ||
             !game.play_service_token.empty() || (!game.play_token.empty() && !game.game_id.empty())) && InsertGameKeys(seen, game))
            games.push_back(game);
    };
    // Le dernier lancement sur la console reste en tête, comme dans la version GitHub,
    // même si la mise à jour de l'historique distant n'a pas encore atteint le serveur.
    for (const auto& played : local)
    {
        // Comme dans le dépôt d'origine, chercher d'abord dans le catalogue. Une sauvegarde
        // conserve les identifiants publics, jamais les jetons nécessaires au lancement.
        const auto match = std::find_if(catalog.begin(), catalog.end(),
                                         [&played](const Game& current) { return SameGame(played, current); });
        if (match == catalog.end())
        {
            append(played);
            continue;
        }
        Game current = *match;
        if (current.asin.empty()) current.asin = played.asin;
        if (current.game_id.empty()) current.game_id = played.game_id;
        if (current.page_uri.empty()) current.page_uri = played.page_uri;
        if (current.image_portrait.empty()) current.image_portrait = played.image_portrait;
        if (current.image_landscape.empty()) current.image_landscape = played.image_landscape;
        append(current);
    }
    // Garder aussi la liste Luna complète : aucune limite locale ne doit la tronquer.
    for (const auto& shelf : shelves)
    {
        if (IsContinuePlayingShelf(shelf))
            for (const auto& game : shelf.games)
                append(game);
    }
    return games;
}

void RecordRecentGame(const Game& game)
{
    if (game.title.empty())
        return;
    std::vector<Game> games = LoadRecentGames();
    std::erase_if(games, [&game](const Game& other) { return SameGame(game, other); });
    games.insert(games.begin(), game);
    if (games.size() > kMaximumRecentGames)
        games.resize(kMaximumRecentGames);

    std::unique_ptr<json_t, decltype(&json_decref)> root(json_array(), &json_decref);
    for (const auto& entry : games)
    {
        json_array_append_new(root.get(), json_pack(
            "{s:s, s:s, s:s, s:s, s:s, s:s}",
            "asin", entry.asin.c_str(),
            "game_id", entry.game_id.c_str(),
            "title", entry.title.c_str(),
            "page_uri", entry.page_uri.c_str(),
            "image_portrait", entry.image_portrait.c_str(),
            "image_landscape", entry.image_landscape.c_str()));
    }
    char* text = json_dumps(root.get(), JSON_INDENT(1));
    if (!text)
        return;
    PrepareAppStorage();
    const bool saved = WriteFileAtomically(RecentGamesPath(), text);
    free(text);
    LogRuntimeEvent("history", saved ? "saved" : "save_failed", game.title);
}

} // namespace lunanx::luna

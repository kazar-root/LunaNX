#include "recent_games.hpp"

#include "core/app_paths.hpp"
#include "core/atomic_file_replace.hpp"

#include <jansson.h>

#include <cstdlib>
#include <memory>

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
    return a.title == b.title;
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
    WriteFileAtomically(RecentGamesPath(), text);
    free(text);
}

} // namespace lunanx::luna

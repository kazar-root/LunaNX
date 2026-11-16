#include "luna/recent_games.hpp"
#include "luna/luna_client.hpp"

#include <jansson.h>

#include <cassert>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <unistd.h>

namespace
{
std::string test_directory;
lunanx::luna::Game MakeGame(int index)
{
    lunanx::luna::Game game;
    game.title = "Game " + std::to_string(index);
    game.asin = "TEST" + std::to_string(index);
    game.page_uri = "game/test/" + game.asin;
    game.play_token = "do-not-persist";
    return game;
}
}
namespace lunanx
{
const std::string& AppHomePath() { return test_directory; }
void PrepareAppStorage() { std::filesystem::create_directories(test_directory); }
void LogRuntimeEvent(const std::string&, const std::string&, const std::string&) {}

// The response shape matches Luna's captured landing_home page. All games and
// navigation tokens below are synthetic; no account data or network is used.
HttpResponse HttpClient::Post(const std::string& url, const std::string&,
    const std::vector<std::string>&, const std::string& body) const
{
    assert(url.ends_with("/getPage"));
    json_t* request = json_loads(body.c_str(), 0, nullptr);
    assert(request);
    const bool home = json_is_string(json_object_get(request, "serviceToken"));
    if (home)
        assert(std::string(json_string_value(json_object_get(request, "serviceToken"))) == "test-home");
    json_decref(request);
    if (!home)
        return {200, R"({"pageMemberGroups":{"nav":{"widgets":[{"type":"NAV_ITEM","presentationData":"{\"text\":\"Accueil\"}","actions":[{"serviceToken":"test-home"}]}]}}})", {}};
    return {200, R"({"pageContext":{"pageType":"landing_home"},"pageMemberGroups":{"mainContent":{"widgets":[
        {"type":"CAROUSEL","presentationData":"{\"title\":\"Continuer à jouer\"}","widgets":[
            {"type":"GAME_TILE_VERTICAL","presentationData":"{\"title\":\"Luna resume game\",\"asin\":\"B000000001\",\"imagePortrait\":\"https://example.test/resume.jpg\"}","actions":[{"serviceToken":"test-detail"}]},
            {"type":"BASIC_CAROUSEL_TILE","presentationData":"{\"title\":\"Tout voir\"}","actions":[{"serviceToken":"test-more"}]}
        ]},
        {"type":"CAROUSEL","presentationData":"{\"title\":\"Ajouts récents\"}","widgets":[
            {"type":"GAME_TILE_VERTICAL","presentationData":"{\"title\":\"New catalog game\",\"asin\":\"B000000002\",\"imagePortrait\":\"https://example.test/new.jpg\"}","actions":[{"serviceToken":"test-new-detail"}]}
        ]}
    ]}}})", {}};
}
}

namespace lunanx::luna
{
std::string CookieJar::Value(const std::string&) const { return {}; }
const AmazonMarket& FindAmazonMarket(const std::string&)
{
    static const AmazonMarket unused {};
    return unused;
}
}

int main()
{
    using namespace lunanx::luna;
    char directory[] = "/tmp/lunanx-recent-test-XXXXXX";
    assert(mkdtemp(directory));
    test_directory = directory;
    assert(LoadRecentGames().empty());
    assert(RecentGamesForShelves({}, {}).empty());
    for (int i = 0; i < 12; ++i)
        RecordRecentGame(MakeGame(i));
    auto games = LoadRecentGames();
    assert(games.size() == 10 && games.front().asin == "TEST11" && games.back().asin == "TEST2");
    RecordRecentGame(MakeGame(5));
    games = LoadRecentGames();
    assert(games.size() == 10 && games.front().asin == "TEST5");
    std::ifstream saved(test_directory + "/recent_games.json");
    std::string content((std::istreambuf_iterator<char>(saved)), {});
    assert(content.find("do-not-persist") == std::string::npos);

    Shelf added {"Recently added", "GAMES_ROW", {MakeGame(1)}};
    assert(RecentGamesForShelves({added}, {}).empty());
    Shelf played {"Continue playing", "CONTINUE_PLAYING_ROW", {MakeGame(1), MakeGame(2)}};
    games = RecentGamesForShelves({played}, {MakeGame(1), MakeGame(3)});
    assert(games.size() == 3 && games[0].asin == "TEST1" && games[1].asin == "TEST3" && games[2].asin == "TEST2");
    played.title = "Reprendre";
    played.row_type = "GAMES_ROW";
    assert(RecentGamesForShelves({played}, {}).size() == 2);

    // Reproduce the missing Luna category with an already full local history.
    LunaClient client(CookieJar {});
    const auto shelves = client.FetchShelves();
    assert(shelves.size() == 2);
    assert(shelves.front().title == "Continuer à jouer" && shelves.front().row_type == "CAROUSEL");
    std::vector<lunanx::luna::Game> full_local;
    for (int i = 0; i < 10; ++i)
        full_local.push_back(MakeGame(i));
    games = RecentGamesForShelves(shelves, full_local);
    assert(games.size() == 11 && games.front().asin == "TEST0" && games.back().asin == "B000000001");
    assert(std::none_of(games.begin(), games.end(), [](const auto& game) { return game.asin == "B000000002"; }));
    assert(RecentGamesForShelves(shelves, {}).size() == 1);

    // The complete Luna row must survive even when it contains more than ten games.
    played.title = "Continuer à jouer";
    played.games.clear();
    for (int i = 0; i < 15; ++i)
        played.games.push_back(MakeGame(i));
    games = RecentGamesForShelves({played}, {MakeGame(3), MakeGame(20)});
    assert(games.size() == 16 && games.front().asin == "TEST3" && games[1].asin == "TEST20" && games.back().asin == "TEST14");
    for (const char* title : {"CONTINUER À JOUER", "Continuer\u00a0à\u00a0jouer", "Continuer a\u0300 jouer"})
    {
        played.title = title;
        assert(RecentGamesForShelves({played}, {}).size() == 15);
    }
    auto unavailable = MakeGame(0);
    unavailable.asin.clear();
    unavailable.page_uri.clear();
    assert(RecentGamesForShelves({}, {unavailable}).empty());

    // Regression: the on-disk schema intentionally has no launch/navigation tokens. A tile
    // without an ASIN/page URI must be looked up before testing whether it can be launched.
    Game id_only;
    id_only.game_id = "public-game-uuid";
    id_only.title = "Saved title";
    id_only.detail_service_token = "temporary-navigation-do-not-persist";
    RecordRecentGame(id_only);
    auto persisted = LoadRecentGames();
    assert(persisted.front().game_id == id_only.game_id);
    assert(persisted.front().detail_service_token.empty());
    assert(RecentGamesForShelves({}, {persisted.front()}).empty());
    Game current = id_only;
    current.title = "Updated catalog title";
    current.detail_service_token = "fresh-navigation-token";
    games = RecentGamesForShelves({}, {persisted.front()}, {current});
    assert(games.size() == 1 && games.front().detail_service_token == "fresh-navigation-token");
    assert(games.front().title == "Updated catalog title");
    // An old title-only save is also recovered, as in the GitHub library implementation.
    Game title_only;
    title_only.title = "UPDATED CATALOG TITLE™";
    games = RecentGamesForShelves({}, {title_only}, {current});
    assert(games.size() == 1 && games.front().game_id == "public-game-uuid");
    // Empty/unavailable remote history cannot erase local history, and refresh keeps its order.
    Game second = MakeGame(55);
    RecordRecentGame(second);
    games = RecentGamesForShelves({}, LoadRecentGames(), {current, second});
    assert(games.size() == 10 && games.front().asin == second.asin);
    assert(games[1].game_id == id_only.game_id);
    std::ifstream new_saved(test_directory + "/recent_games.json");
    content.assign(std::istreambuf_iterator<char>(new_saved), {});
    assert(content.find("temporary-navigation-do-not-persist") == std::string::npos);
    assert(content.find("do-not-persist") == std::string::npos);
    // Creating storage is part of recording, even before an account/catalog refresh.
    std::filesystem::remove_all(test_directory);
    RecordRecentGame(second);
    assert(LoadRecentGames().size() == 1);

    std::ofstream(test_directory + "/recent_games.json") << "not JSON";
    assert(LoadRecentGames().empty());
    std::ofstream(test_directory + "/recent_games.json") << R"([null,12,{}, {"title":"No launch target"}])";
    assert(RecentGamesForShelves({}, LoadRecentGames()).empty());
    std::filesystem::remove_all(test_directory);
    std::cout << "Recent games: Luna Continue playing extraction, full local history, ordering, deduplication and empty/corrupt data passed\n";
}

// Offline fixtures based on Luna's public WEB_V1 client; no real account or network.
#include "luna/luna_client.hpp"
#include "luna/recent_games.hpp"
#include "core/i18n.hpp"

#include <jansson.h>
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>

namespace
{
using namespace lunanx;
using namespace lunanx::luna;
const AmazonMarket* market;
std::string locale;
std::string title = "任意の履歴見出し";
bool by_id = false, icon_only = false, empty = false, fail = false, fail_home = false;
bool navigation_without_history = false;
int generation = 1, widget_calls = 0;

std::string Dump(json_t* value)
{
    assert(value);
    char* text = json_dumps(value, JSON_COMPACT);
    assert(text);
    const std::string result(text);
    free(text);
    json_decref(value);
    return result;
}
bool Header(const std::vector<std::string>& headers, const std::string& value)
{
    return std::find(headers.begin(), headers.end(), value) != headers.end();
}
json_t* Tile(int number)
{
    const std::string id = "test-game-" + std::to_string(number);
    const auto data = Dump(json_pack("{s:s,s:s,s:s}", "gameTitle", id.c_str(), "gameId", id.c_str(),
                                     "tileImageLandscape", "https://example.test/art.jpg"));
    return json_pack("{s:s,s:s,s:[{s:s}]}", "type", "RECENTLY_PLAYED_TILE", "presentationData", data.c_str(),
                     "actions", "serviceToken", ("test-detail-" + std::to_string(number)).c_str());
}
std::string Page(const std::string& type)
{
    auto presentation = json_pack("{s:s}", "title", title.c_str());
    if (icon_only)
        json_object_set_new(presentation, "image", json_string("https://example.test/icon_continue_playing_carousel.png"));
    else
        json_object_set_new(presentation, "subtype", json_string("RECENTLY_PLAYED_TILES"));
    auto row = json_pack("{s:s,s:s,s:s,s:s,s:[]}", "__type", "CollectionProxy:test", "id", type.c_str(),
                         "type", "CAROUSEL", "presentationData", Dump(presentation).c_str(), "widgets");
    if (!by_id)
        json_object_set_new(row, "serviceToken", json_string(("test-widget-" + type).c_str()));
    auto root = json_pack("{s:{s:s,s:s},s:{s:{s:[o]}}}", "pageContext", "pageType", type.c_str(),
        "pageId", "default", "pageMemberGroups", "mainContent", "widgets", row);
    // Public artwork shapes from Luna, with synthetic identifiers and no account data.
    const char* artwork = R"({"type":"CAROUSEL","presentationData":{"title":"Artwork"},"widgets":[
      {"type":"GAME_TILE_VERTICAL","presentationData":{"title":"Sonic Mania Plus","asin":"B000000101","imageLandscape":"https://m.media-amazon.com/images/I/81X9NudoMYL.png","imagePortrait":"https://m.media-amazon.com/images/I/81iH37m5+YL.png"}},
      {"type":"GAME_TILE_VERTICAL","presentationData":{"title":"Qui Veut Gagner Des Millions ?","asin":"B000000102","imagePortrait":"https://m.media-amazon.com/images/I/81ARXiu-0IL._PIGameNightGameLogo,BottomRight,0,0_.png"}},
      {"type":"GAME_TILE","presentationData":{"title":"Generic landscape","asin":"B000000103","image":"https://example.test/landscape.jpg"}}
    ]})";
    json_array_append_new(json_object_get(json_object_get(json_object_get(root, "pageMemberGroups"),
        "mainContent"), "widgets"), json_loads(artwork, 0, nullptr));
    return Dump(root);
}
}

namespace lunanx
{
const std::string& AppHomePath() { static const std::string path = "/tmp/lunanx-unused-history-fixture"; return path; }
void PrepareAppStorage() { assert(false && "History tests must not persist account data"); }
void LogRuntimeEvent(const std::string&, const std::string&, const std::string&) {}

HttpResponse HttpClient::Get(const std::string& url, const std::string&, const std::vector<std::string>&,
                             long, long) const
{
    assert(url == "https://luna.amazon." + market->domain + "/token");
    return {200, R"({"valid":true,"token":"test-access"})", {}};
}
HttpResponse HttpClient::Post(const std::string& url, const std::string&,
                              const std::vector<std::string>& headers, const std::string& body) const
{
    assert(Header(headers, "Origin: https://luna.amazon." + market->domain));
    assert(Header(headers, "x-amz-country-of-residence: " + market->country));
    assert(Header(headers, "x-amz-locale: " + locale));
    const std::string proxy = market->country == "US" || market->country == "CA"
        ? "https://proxy-prod.tempo.digital.a2z.com/" : "https://proxy-prod.eu-west-1.tempo.digital.a2z.com/";
    assert(url.starts_with(proxy));
    const std::string operation = url.substr(proxy.size());
    json_t* request = json_loads(body.c_str(), 0, nullptr);
    assert(request);
    if (operation == "getAppConfig")
    {
        json_decref(request);
        return {200, R"({"endpoints":{},"initialPageContext":{"pageType":"multistate_navigation","pageId":"default"},"profilePageContext":{"pageType":"player_selection","pageId":"profiles"}})", {}};
    }
    const std::string type = json_string_value(json_object_get(json_object_get(request, "pageContext"), "pageType"))
        ? json_string_value(json_object_get(json_object_get(request, "pageContext"), "pageType")) : "";
    if (type == "player_selection")
    {
        json_decref(request);
        return {200, R"({"profileId":"test-profile-id"})", {}};
    }
    if (operation == "player/playerLogin")
    {
        assert(std::string(json_string_value(json_object_get(request, "profileId"))) == "test-profile-id");
        json_decref(request);
        return {200, R"({"token":"test-profile-token"})", {}};
    }
    // Personalized history must carry the active Luna profile on both API calls.
    assert(Header(headers, "x-amz-profile-token: test-profile-token"));
    assert(Header(headers, "x-amz-access-token: test-access"));
    if (operation == "getPage")
    {
        const char* service = json_string_value(json_object_get(request, "serviceToken"));
        if (service && std::string(service).starts_with("test-detail-"))
        {
            json_decref(request);
            return {200, R"({"actions":[{"target":"GAME_SESSION_ASYNC","payload":"{\"gameId\":\"test-game-1\",\"playToken\":\"test-play-token\"}"}]})", {}};
        }
        if (type == "multistate_navigation")
        {
            json_decref(request);
            if (navigation_without_history)
            {
                auto page = json_loads(Page("landing_home").c_str(), 0, nullptr);
                json_array_remove(json_object_get(json_object_get(json_object_get(page,
                    "pageMemberGroups"), "mainContent"), "widgets"), 0);
                return {200, Dump(page), {}};
            }
            return {200, Page("landing_home"), {}};
        }
        assert(type == "landing_home" || type == "landing_library");
        json_decref(request);
        return {200, Page(type), {}};
    }
    assert(operation == "getWidget");
    ++widget_calls;
    const char* service = json_string_value(json_object_get(request, "serviceToken"));
    std::string page_type;
    if (by_id)
    {
        assert(!service);
        page_type = type;
        assert(std::string(json_string_value(json_object_get(request, "widgetId"))) == page_type);
    }
    else
    {
        assert(service);
        const std::string token(service);
        assert(token.starts_with("test-widget-"));
        page_type = token.substr(12);
    }
    json_decref(request);
    if (fail || (fail_home && page_type == "landing_home"))
        return {503, "{}", {}};
    auto tiles = json_array();
    if (!empty)
    {
        json_array_append_new(tiles, Tile(generation));
        if (page_type == "landing_library")
            json_array_append_new(tiles, Tile(generation + 1));
    }
    // No title/subtype in the refreshed object: retain the placeholder metadata.
    return {200, Dump(json_pack("{s:{s:s,s:{},s:o}}", "widget", "type", "CAROUSEL", "presentationData",
                                "widgets", tiles)), {}};
}
}

int main()
{
    using namespace lunanx;
    using namespace lunanx::luna;
    int markets_tested = 0, languages_tested = 0;
    for (const auto& selected : AmazonMarkets())
    {
        market = &selected;
        locale = market->locale;
        CookieJar cookies;
        cookies.SetCookies({Cookie {".amazon." + market->domain, "/", "session-id", "test-cookie"}});
        DeviceRegistration registration;
        registration.market_code = market->code;
        LunaClient client(cookies, registration);
        client.SignIn();
        widget_calls = 0;
        auto games = RecentGamesForShelves(client.FetchRecentShelves(), {});
        assert(widget_calls == 2 && games.size() == 2 && games.front().game_id == "test-game-1");
        assert(client.FetchGameDetail(games.front()).play_token == "test-play-token");
        // The initial page itself may contain personalized content without navigation tabs.
        const auto initial_shelves = client.FetchShelves();
        const auto initial = RecentGamesForShelves(initial_shelves, {});
        assert(initial.size() == 1 && initial.front().game_id == "test-game-1");
        int covers_checked = 0;
        for (const auto& shelf : initial_shelves)
            for (const auto& game : shelf.games)
            {
                if (game.asin == "B000000101")
                {
                    assert(game.image_portrait == "https://m.media-amazon.com/images/I/81iH37m5+YL.png");
                    ++covers_checked;
                }
                if (game.asin == "B000000102")
                {
                    assert(game.image_portrait.find("_PIGameNightGameLogo,") != std::string::npos);
                    ++covers_checked;
                }
                if (game.asin == "B000000103")
                {
                    assert(game.image_portrait.empty() && game.image_landscape == "https://example.test/landscape.jpg");
                    ++covers_checked;
                }
            }
        assert(covers_checked == 3);
        // Startup cannot infer an empty account from a navigation page containing only artwork.
        navigation_without_history = true;
        const auto browse_only = client.FetchShelves();
        assert(!browse_only.empty() && RecentGamesForShelves(browse_only, {}).empty());
        assert(RecentGamesForShelves(client.FetchRecentShelves(), {}).size() == 2);
        navigation_without_history = false;
        // Refresh the same account without relaunching/signing in: another device played a new game.
        generation = 5;
        games = RecentGamesForShelves(client.FetchRecentShelves(), {});
        assert(games.size() == 2 && games.front().game_id == "test-game-5");
        generation = 1;
        by_id = true;
        icon_only = true;
        games = RecentGamesForShelves(client.FetchRecentShelves(), {});
        assert(games.size() == 2);
        by_id = icon_only = false;
        empty = true;
        assert(RecentGamesForShelves(client.FetchRecentShelves(), {}).empty());
        empty = false;
        fail = true;
        bool threw = false;
        try { client.FetchRecentShelves(); } catch (const LunaError&) { threw = true; }
        assert(threw);
        fail = false;
        // A successful empty page must not mask an unavailable history on the other page.
        fail_home = true;
        empty = true;
        threw = false;
        try { client.FetchRecentShelves(); } catch (const LunaError&) { threw = true; }
        assert(threw);
        empty = false;
        assert(RecentGamesForShelves(client.FetchRecentShelves(), {}).size() == 2);
        fail_home = false;
        for (const auto& language : UiLanguageOptions())
        {
            client.SetContentLanguage(language.code);
            locale = client.account().locale;
            games = RecentGamesForShelves(client.FetchRecentShelves(), {});
            assert(games.size() == 2);
            ++languages_tested;
        }
        ++markets_tested;
    }
    std::cout << "Personalized Luna history: " << markets_tested << " markets, " << languages_tested
              << " locale combinations; profile headers, getWidget token/ID, localized-independent rows, refresh, launch, empty and failures passed\n";
}

#include "luna/game_artwork.hpp"
#include "ui/image_url_policy.hpp"

#include <cassert>
#include <iostream>

int main()
{
    using namespace lunanx;
    using namespace lunanx::luna;
    // A featured banner arrives first; the same game later has its actual vertical cover.
    Game banner;
    banner.title = "Sonic Mania Plus";
    banner.image_landscape = "https://m.media-amazon.com/images/I/81X9NudoMYL.png";
    Game portrait = banner;
    portrait.image_portrait = "https://m.media-amazon.com/images/I/81iH37m5+YL.png";
    assert(MergeGameArtwork(banner, portrait));
    assert(banner.image_portrait == portrait.image_portrait);
    assert(banner.image_landscape == portrait.image_landscape);
    assert(!MergeGameArtwork(banner, Game {}));
    assert(!MergeGameArtwork(banner, portrait));
    Game alternate = portrait;
    alternate.image_portrait = "https://example.test/alternate.png";
    assert(!MergeGameArtwork(banner, alternate));
    assert(banner.image_portrait == portrait.image_portrait);

    assert(ResizeAmazonImageUrl(portrait.image_portrait, 320) ==
           "https://m.media-amazon.com/images/I/81iH37m5+YL._SX320_.png");
    // The Millionaire cover carries an Amazon composition recipe that must survive.
    const std::string composed = "https://m.media-amazon.com/images/I/81ARXiu-0IL._PIGameNightGameLogo,BottomRight,0,0_.png";
    assert(ResizeAmazonImageUrl(composed, 320) == composed);
    assert(ResizeAmazonImageUrl("https://m.media-amazon.com/images/I/test._SX64_.jpg", 320) ==
           "https://m.media-amazon.com/images/I/test._SX320_.jpg");
    assert(ResizeAmazonImageUrl("https://m.media-amazon.com/images/I/test.png?key=a.b", 320) ==
           "https://m.media-amazon.com/images/I/test._SX320_.png?key=a.b");
    assert(ResizeAmazonImageUrl(portrait.image_portrait, 0) == portrait.image_portrait);
    assert(ResizeAmazonImageUrl("https://example.test/poster.png", 320) == "https://example.test/poster.png");
    std::cout << "Catalog artwork: late portrait merging, image recipes, resizing and URL suffixes passed\n";
}

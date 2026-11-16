# Host tests

`provenance_test.py` checks deterministic manifests, exclusion of private files, whole-file
and partial copies after identifier changes, string/comment parsing, embedded NRO metadata,
and detection of modified source or binary content. All fixtures are temporary and offline:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tests/provenance_test.py
```

`menu_and_metrics_test.cpp` checks the + / − chord (including staggered presses and releases),
standalone Start/Guide/Back delivery, measured stream rates and the battery estimate's warm-up,
charging and sleep handling. It has no dependencies beyond the standard library:

```sh
c++ -std=c++20 -Wall -Wextra -Werror -Iapp/src tests/menu_and_metrics_test.cpp -o /tmp/lunanx-menu-test
/tmp/lunanx-menu-test
```

`recent_games_test.cpp` checks missing, empty and malformed history, the ten-game cap,
local launch ordering, full remote rows, duplicate removal, and the distinction between recently played and
recently added shelves. It also runs the production FetchShelves parser against a synthetic
Luna-shaped French Continue playing carousel, with embedded presentationData JSON, a full local
history, and more than ten server games. Disk round-trip regressions also check catalog
rehydration for saves without an ASIN or page URI, legacy title-only entries, storage creation
and exclusion of temporary launch/navigation tokens. It writes only to its own temporary directory and uses native Jansson.
On macOS, with the native Jansson directories below:

```sh
c++ -std=c++20 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
  -Iapp/src -I"$JANSSON_SOURCE/src" -I"$JANSSON_BUILD/include" \
  tests/recent_games_test.cpp app/src/luna/recent_games.cpp app/src/luna/luna_client.cpp \
  app/src/core/i18n.cpp "$JANSSON_BUILD/lib/libjansson.a" -Wl,-dead_strip -o /tmp/lunanx-recent-test
/tmp/lunanx-recent-test
```

On Linux, replace `-Wl,-dead_strip` with `-Wl,--gc-sections`.

`luna_recent_catalog_test.cpp` exercises the production authenticated client with an offline
HTTP mock. It checks all 14 configured markets and 154 market/locale combinations, both
history pages, lazy collections by token or widget ID, locale-independent metadata, initial
page content, deduplication, refresh, launch, empty history and server failures. No Amazon
account is accessed and no credentials are written:

```sh
c++ -std=c++20 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
  -Iapp/src -I"$JANSSON_SOURCE/src" -I"$JANSSON_BUILD/include" \
  -I/opt/devkitpro/portlibs/switch/include \
  tests/luna_recent_catalog_test.cpp app/src/luna/recent_games.cpp app/src/luna/luna_client.cpp \
  app/src/luna/cookie_jar.cpp app/src/luna/amazon_auth.cpp app/src/core/i18n.cpp \
  "$JANSSON_BUILD/lib/libjansson.a" -Wl,-dead_strip -o /tmp/lunanx-luna-history-test
/tmp/lunanx-luna-history-test
```

## Offline authentication tests

`amazon_device_sign_in_test.cpp` compiles the production Amazon implementation against a
mock HTTP transport. It never contacts Amazon, opens an account or writes credentials.
The nine scenarios cover cancellation before the request, malformed data, invalid expiry,
cancellation after the challenge, expiry while waiting, rejected codes, pending then successful
registration with separate identity/store domains, transient network/server failures, and
successful registration received after cancellation (including device cleanup).

Build Jansson 2.14 for the host, then run:

```sh
cmake -S "$JANSSON_SOURCE" -B "$JANSSON_BUILD" \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DJANSSON_BUILD_DOCS=OFF \
  -DJANSSON_BUILD_SHARED_LIBS=OFF -DJANSSON_WITHOUT_TESTS=ON
cmake --build "$JANSSON_BUILD" --target jansson -j4
JANSSON_SOURCE="$JANSSON_SOURCE" JANSSON_BUILD="$JANSSON_BUILD" \
  bash tests/run-auth-host-tests.sh
```

`JANSSON_SOURCE` and `JANSSON_BUILD` must name a native source/build, separate from the
Switch build. Mbed TLS headers default to devkitPro's portlibs; override `MBEDTLS_INCLUDE_DIR`
if needed. Dead-code elimination removes the PKCE cryptography and persistence functions
that this device-code test does not use.

These tests and a successful NRO build do not replace tests on the console. In particular,
verify the header at 720p, Wi-Fi/LAN/offline transitions, Luna avatar loading, touch and page
swipes, sign-out/reconnect, and a complete Amazon code confirmation followed by gameplay.


## Catalog artwork

`catalog_artwork_test.cpp` checks late portrait artwork after a featured banner,
non-destructive merging, the Millionaire/GameNight composition recipe, Amazon resizing
and query suffixes. The personalized catalog fixture also parses the actual public
Sonic Mania/Millionaire artwork fields and distinguishes generic landscape images:

```sh
c++ -std=c++20 -Wall -Wextra -Werror -Iapp/src tests/catalog_artwork_test.cpp -o /tmp/lunanx-artwork-test
/tmp/lunanx-artwork-test
```

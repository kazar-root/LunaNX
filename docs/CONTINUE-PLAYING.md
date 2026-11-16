# Continue playing in LunaNX 3.0

Continue playing combines the last ten games launched from LunaNX with personalized history
returned by Luna. The latest console launches appear first; the remaining Luna games follow
without duplicates. The ten-game cap applies to local storage, not to the remote list.

## Local history

A launch records public game information in `recent_games.json`: title, ASIN, game identifier,
page address and artwork. Session and navigation tokens are not stored in this file. Cancelled
launch attempts can appear because recording happens when the launch is requested.

Some Luna tiles have no ASIN or page address. Saved entries are therefore matched to the
current catalog by ASIN, game ID or normalized title **before** launchability is checked.
The current catalog supplies fresh navigation data. Older six-field saves and title-only
entries remain usable when a matching catalog game exists.

## Luna account history

The web client uses `RECENTLY_PLAYED_TILE` and the `RECENTLY_PLAYED_TILES` carousel subtype.
Luna can load a collection separately from the initial page. LunaNX requests both
`landing_home` and `landing_library`, then resolves lazy collections through `getWidget`,
using a service token or widget identifier with its page context.

Recognition uses widget metadata and the historical Continue playing icon, with older
localized-title handling retained. It does not depend on the French title. Calls use the
active account/profile and its market, country, language and regional endpoint.

A successful empty page does not hide a failure on the other history page. Existing displayed
history is retained after a failed refresh. Newly added catalog games are not treated as played games.

## Initial library selection

The library opens on Top 10 if there is no playable recent history. It opens on Continue
playing when local entries, resolved against the current catalog, or Luna collections contain
games. If the initial page has no history, the initial load checks the separate account history
before showing the library. A failure of that optional check does not block the catalog.

This automatic choice happens once when opening the account. Later refreshes and returns
from Settings preserve the user's selected section.

## Loading and validation

History requests run outside the UI thread, with at most 16 widget requests and four passes
per page. Results from an old account are discarded after logout or account changes.

Host tests exercise the production client with synthetic responses across 14 market
configurations and 154 market/language combinations. They cover active profile headers,
collections addressed by token or ID, embedded JSON, missing ASINs, launch resolution,
refresh, duplicate removal, empty data, partial failures and missing history on the initial page.
Artwork fixtures include the public Sonic Mania and Millionaire image field shapes.

The creator confirmed the final version on a real Switch and supplied the release captures,
including a Continue playing view after a successful game session. Account-side history
availability still depends on the data returned by Luna; local history alone cannot reconstruct
sessions played only on a Mac or phone.

# Changelog

## v2.0.1

**New features**
- New countries requested by several users: all 14 Luna countries are now supported. New:
  Poland (amazon.pl), Netherlands (amazon.nl), Sweden (amazon.se), Belgium (amazon.com.be), and
  Austria, Portugal and Luxembourg, which use the amazon.de or amazon.es store while LunaNX tells
  Luna the player's real country. Choose your country at first launch or in Settings → Amazon
  account.
- Sign-in for amazon.pl, .nl, .se and .com.be accounts uses Amazon's European sign-in page on
  amazon.de, shown in the store's language (European Amazon accounts are shared).
- Interface in Polish, Dutch and Swedish (*Polski*, *Nederlands*, *Svenska*): 11 languages in
  total. The country's language is applied automatically when it is chosen.
- The Luna catalog and game texts now also follow the interface language for Dutch, Polish,
  Portuguese and Swedish (Portuguese catalog in European Portuguese, as on the Luna website).

**Known issue**
- Sign in with Amazon (Switch browser) does not work on firmware 23.x (CFW) for now. Use Sign in
  with my phone (QR code), or the `cookies.txt` fallback.

## v2.0

**New features**
- Sign in with your phone: a QR code on the Switch opens a LunaNX page on your phone or tablet
  (same Wi-Fi network). You sign in on the official Amazon page in the phone's browser, then
  paste the address of the last page into the LunaNX page. This sign-in does not use the Switch
  browser, which shows "This feature is not available" on some consoles. Your password is typed
  only on Amazon's site.
- Sign-in in the Switch browser is unchanged and still available.

## v1.2

**Fixes**
- Sign-in outside France: LunaNX always contacted the European Luna server, so United States
  and Canada accounts could get a "service unavailable" error after signing in. Accounts from
  amazon.com and amazon.ca now use the North American Luna server, the other stores the European
  one, with an automatic retry on the other server if the first one refuses.

## v1.1

**New features**
- Interface in 8 languages: Deutsch, English, Español, Français, Italiano, Português, Русский,
  简体中文 (English by default, changeable in Settings → Language). The Luna catalog and game
  texts follow the chosen language when the store offers it.
- Amazon store chosen at first launch (France, Germany, United Kingdom, Spain, Italy, United
  States, Canada); the store's language is applied automatically.
- Complete Luna catalog: live background exploration of the Luna pages ("All games", genres,
  collections) to show games missing from the home rows.
- "All games" grid shown as pages of 10 covers (Previous / Next page buttons, or L / R).
- Automatic return to the library when a game session ends.
- Loading animation (three dots) until the game picture appears.

**Fixes and optimizations**
- Fixed a crash when browsing to the last games: only the current page's covers are kept in
  memory, and oversized artwork is downscaled before display.
- Fixed duplicate games in the library (detection by Amazon ID or by title).
- Fixed a crash when changing the language in Settings.

## v1.0 — Initial release

**Account**
- Amazon sign-in from the console through the official Amazon page, with automatic session
  renewal and sign-out.
- Amazon stores: France, Germany, United Kingdom, Spain, Italy, United States, Canada.

**Library**
- Luna catalog loaded automatically.
- Featured banner, Continue playing (last 10 games), Top 10 and All games (covers loaded 10 at a
  time).
- Optional simple list view.

**Streaming**
- 720p 60 fps H.264 video with software decoding and adaptive image processing.
- Stereo audio.
- Automatic server selection by latency, with manual region choice.
- Bitrate adapted to the network.

**Controls**
- Joy-Con, Pro Controller and handheld mode, with rumble.
- Switch button labels by default; Xbox layout available.
- D-pad can drive the left stick.

**Interface**
- English interface.

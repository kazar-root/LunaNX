# Changelog

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

<p align="center">
  <img src="resources/img/lunanx_logo.png" width="220" alt="LunaNX logo">
</p>

<h1 align="center">LunaNX v2.0 — Amazon Luna on Nintendo Switch</h1>

<p align="center">
  <img src="https://img.shields.io/badge/version-2.0-7c5cff" alt="Version 2.0">
  <img src="https://img.shields.io/badge/platform-Nintendo%20Switch%20(homebrew)-e60012" alt="Nintendo Switch homebrew">
  <img src="https://img.shields.io/badge/license-GPL--3.0-blue" alt="License GPL-3.0">
</p>

**LunaNX** is a homebrew application (`.nro`) that lets you play **Amazon Luna** cloud games on a
Nintendo Switch running custom firmware. It signs in to your Amazon account, shows your Luna
library, and streams the game to the console with your controller as the input device.

> [!IMPORTANT]
> LunaNX is an **unofficial community project**. It is not affiliated with, endorsed by or
> sponsored by Amazon or Nintendo. Amazon Luna and Nintendo Switch are trademarks of their
> respective owners. You need your own Amazon account with access to Luna; the games available
> depend on your subscription and your country.

## What's new in v2.0

**Sign in with your phone.** On some consoles the Switch browser does not open the Amazon
sign-in page and shows **"This feature is not available"**. LunaNX v2.0 adds a second way to
sign in that does not use the Switch browser at all:

1. On the sign-in screen, choose **Sign in with my phone**.
2. Scan the QR code shown on the Switch with your phone or tablet (on the same Wi-Fi network).
3. Sign in on the **official Amazon page** in your phone's browser.
4. Copy the address of the last page Amazon shows, paste it into the LunaNX page and send it to
   the Switch. Your Luna library appears.

Your password is typed only on Amazon's site: LunaNX never sees it. No PC, no browser extension
and no `cookies.txt` file are needed.

If the Switch browser works on your console, **Sign in with Amazon** still lets you sign in
directly on the Switch, without a phone.

- **8 interface languages**: Deutsch, English, Español, Français, Italiano, Português, Русский
  and 简体中文. English by default, changeable at any time in **Settings → Language**. The Luna
  catalog and game texts follow the chosen language when your store offers it.
- **Store choice at first launch**: pick your Amazon store (France, Germany, United Kingdom,
  Spain, Italy, Poland, United States or Canada) before signing in. LunaNX then connects to the
  right Luna service and switches to the store's language.
- **Complete Luna catalog**: LunaNX now browses the Luna catalog live in the background ("All
  games", genres, collections), so games that are not on the home page (for example Sonic Mania)
  appear too, instead of only the games of the home rows.
- **Paged "All games" grid**: 10 covers per page, with **Previous page / Next page** buttons or
  the **L / R** buttons.
- **Automatic return to the library** when a game session ends.
- **Loading animation** (three purple dots) until the game picture appears.

LunaNX works with the Amazon stores of the countries where Luna is available: France, Germany,
United Kingdom, Spain, Italy, Poland, United States and Canada (with the North American Luna
server fix of v1.2).

I hope you'll enjoy this update and that it finally lets you make the most of Luna on your
Switch! — *KazaR-RooT*

Earlier versions: see [CHANGELOG.md](CHANGELOG.md).

## What's next

**Mouse and keyboard support** is planned for a next version. Until then, feel free to share
your feedback, remarks and bug reports in the [Issues](../../issues) page: they will shape the
next versions.

## Features

- **Amazon sign-in, two ways**: in the Switch browser, or **with your phone** (QR code) when the
  Switch browser is not available. Both use the official Amazon sign-in page; the session then
  renews automatically. Your password is never seen or stored by LunaNX.
- **Amazon stores**: France, Germany, United Kingdom, Spain, Italy, Poland, United States and
  Canada, chosen at first launch.
- **Interface in 8 languages**: Deutsch, English, Español, Français, Italiano, Português,
  Русский, 简体中文.
- **Luna library**, loaded automatically:
  - **Featured** banner,
  - **Continue playing** (your last 10 games launched from LunaNX),
  - **Top 10** (current Luna ranking, when Luna provides it for your account),
  - **All games**: the complete catalog, explored live, shown as pages of 10 covers,
  - optional **simple list** view.
- **Game streaming**: 720p at 60 fps, H.264 video, stereo audio, with bitrate adapted to your
  network.
- **Automatic server selection** based on measured latency (a server region can also be chosen
  manually).
- **Controllers**: Joy-Con, Pro Controller and handheld mode, with **rumble**. Switch button
  labels by default (Xbox layout available); the D-pad can also drive the left stick.
- **Settings**: language, Amazon store, sign out, library view, server region, button layout,
  sound, volume, audio buffer, optional in-game technical overlay.

## Screenshots

| Luna library | In game |
| --- | --- |
| ![LunaNX library](docs/screenshots/Screen%20LunaNX%20v1.0%201.jpg) | ![Game streamed with LunaNX](docs/screenshots/Screen%20LunaNX%20v1.0%202.jpg) |
| **Launching a game** | **Settings** |
| ![Connecting to the nearest Luna server](docs/screenshots/Screen%20LunaNX%20v1.0%203.jpg) | ![LunaNX settings](docs/screenshots/Screen%20LunaNX%20v1.0%204.jpg) |

## Requirements

- A Nintendo Switch able to run homebrew (custom firmware such as Atmosphère, with the
  Homebrew Menu).
- An internet connection (5 GHz Wi-Fi close to the router recommended).
- An Amazon account with access to Amazon Luna in your country.

## Installation

See **[INSTALLATION.md](INSTALLATION.md)** for step-by-step instructions. In short:

1. Copy `LunaNX.nro` to `sdmc:/switch/LunaNX/` on your SD card.
2. Start LunaNX from the Homebrew Menu **in application mode** (hold R while launching a game).
3. Choose your Amazon store, then **Sign in with Amazon** (Switch browser) or **Sign in with my
   phone** (QR code), and complete the sign-in on the official Amazon page.

## Usage

1. Your Luna library appears after sign-in.
2. Move with the D-pad or the left stick, then press **A** on a cover to start the game.
3. To leave a game, press **ZL + ZR + −** at the same time.

| Action | Button |
| --- | --- |
| Launch a game | A on a cover |
| Settings | **Settings** button (top right) or X |
| Refresh the library | **Refresh** button or Y |
| Previous / next page of "All games" | L / R |
| Quit LunaNX (library) | + |
| Start / Xbox button (in game) | + short press / long press |
| Left stick (in game) | left stick or D-pad |
| Technical overlay (in game, if enabled in Settings) | L3 + R3 |
| **Quit a game** | **ZL + ZR + −** |

## Known limitations

- Streaming quality is fixed at **720p 60 fps** with software decoding.
- **Application mode is required**: the Switch browser used for sign-in and the memory needed
  for streaming are not available from the Album (applet mode).
- **Phone sign-in**: the phone or tablet must be on the same Wi-Fi network as the Switch; guest
  networks that isolate devices from each other block it. Paste the address soon after signing
  in: Amazon's sign-in code is only valid for a short time.
- No keyboard or mouse support: games that require them cannot be controlled.
- Parental control PIN prompts from Luna are not supported.
- Game availability depends on your Luna subscription and your country. Luna does not say in
  its catalog which subscription includes each game: a game outside your subscription shows the
  message "This game cannot be launched with this account".
- Arabic and Hebrew interfaces are not available (no right-to-left text support).

## Compatibility

- Tested on a Nintendo Switch running custom firmware, launched from the Homebrew Menu in
  application mode.
- Not tested on every console model or firmware version: please report problems in the
  [Issues](../../issues) page.

## Security and privacy

- Your Amazon session is stored on your SD card in `sdmc:/switch/LunaNX/amazon_device.json`.
  Never share this file. Use **Settings → Sign out of Amazon** before lending or selling your
  console; you can also remove "LunaNX" from your Amazon account → **Manage your devices**.
- Log files in `sdmc:/switch/LunaNX/` can contain your IP address: remove it before sharing a
  log in an issue.

## Download and integrity check

Download `LunaNX.nro` from the [Releases](../../releases/latest) page. Its SHA-256 checksum is
given in the release notes; to check it on macOS or Linux:

```bash
shasum -a 256 LunaNX.nro
```

## License and source code

LunaNX is distributed under the **GNU General Public License v3.0** (see [LICENSE](LICENSE)).

This repository contains the complete source code of LunaNX:
[github.com/kazar-root/LunaNX](https://github.com/kazar-root/LunaNX).

Third-party components and their licenses are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), with the full license texts in
[licenses/](licenses/).

## Building from source

Requirements: [devkitPro](https://devkitpro.org/wiki/Getting_Started), CMake 3.18 or newer, and
Ninja. The packages below provide the Switch toolchain, the ported libraries, `switch-cmake`
(which pulls in the `uam` shader compiler and the `nacptool`/`elf2nro` tools), and bzip2.

### Windows

1. Run the [devkitPro graphical installer](https://github.com/devkitPro/installer/releases/latest)
   and select **Switch Development**. Keep the default `C:\devkitPro` location.
2. Open **devkitPro → MSYS** from the Start menu.
3. Update the package databases, then install the build dependencies (there is no `sudo` in the
   devkitPro MSYS shell):

   ```bash
   pacman -Syu --needed
   pacman -S --needed devkitA64 switch-dev switch-cmake uam switch-pkg-config \
     switch-curl switch-jansson switch-mbedtls switch-ffmpeg switch-dav1d switch-libopus \
     switch-glfw switch-libdrm_nouveau switch-zlib switch-bzip2 switch-freetype switch-libpng \
     switch-libwebp switch-sdl2 cmake ninja python
   ```

4. From the same MSYS window, build the project:

   ```bash
   cd /c/path/to/LunaNX
   ./build.sh
   ```

If `pacman -Syu` asks you to close the window, reopen **devkitPro → MSYS** and run the second
`pacman` command again.

### Linux and macOS

```bash
sudo dkp-pacman -S --needed devkitA64 switch-dev switch-cmake uam switch-pkg-config \
  switch-curl switch-jansson switch-mbedtls switch-ffmpeg switch-dav1d switch-libopus \
  switch-glfw switch-libdrm_nouveau switch-zlib switch-bzip2 switch-freetype switch-libpng \
  switch-libwebp switch-sdl2 ninja
python3 -m pip install --user cmake
./build.sh
```

The output file is `build/LunaNX.nro`.

## Acknowledgments

Thanks to the developers of the open-source projects LunaNX is built on:
[devkitPro, libnx and deko3d](https://devkitpro.org),
[Borealis](https://github.com/xfangfang/borealis),
[libpeer](https://github.com/sepfy/libpeer),
[Mbed TLS](https://github.com/Mbed-TLS/mbedtls),
[libsrtp](https://github.com/cisco/libsrtp),
[usrsctp](https://github.com/sctplab/usrsctp),
[FFmpeg](https://ffmpeg.org) and averne's
[NVTEGRA branch](https://github.com/averne/FFmpeg/tree/nvtegra),
[Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch),
[OpenNOW-Switch](https://github.com/OpenCloudGaming/OpenNOW-Switch),
[curl](https://curl.se), [Jansson](https://github.com/akheron/jansson),
[cJSON](https://github.com/DaveGamble/cJSON), [Yoga](https://github.com/facebook/yoga),
[NanoVG](https://github.com/memononen/nanovg), [fmt](https://github.com/fmtlib/fmt),
zlib, bzip2, Project Nayuki's [QR Code generator](https://www.nayuki.io/page/qr-code-generator-library)
and Google's Material Icons.

---

**Created by KazaR-RooT**

**Developed with the assistance of Claude Code (Anthropic).**

*"The only limit is your imagination. AI makes it possible."*

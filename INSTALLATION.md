# Installing LunaNX

## Before you start

- A Nintendo Switch running custom firmware (for example Atmosphère) with the Homebrew Menu.
- An Amazon account with access to Amazon Luna in your country.
- An internet connection (5 GHz Wi-Fi close to the router recommended).

## 1. Copy the file

1. Download `LunaNX.nro` from the [Releases](https://github.com/kazar-root/LunaNX/releases/tag/v3.0) page.
2. Optional: check the file with the SHA-256 checksum given in the release notes:
   ```bash
   shasum -a 256 LunaNX.nro
   ```
3. On your SD card, create the folder `switch/LunaNX/` if it does not exist.
4. Copy `LunaNX.nro` into `switch/LunaNX/`.

> [!NOTE]
> Keep a **single** `LunaNX.nro` on the SD card. The Homebrew Menu lists every `.nro` it finds,
> and all copies have the same name and icon.

## 2. Start LunaNX in application mode

LunaNX must run in **application mode**:

1. On the HOME menu, **hold R** while launching any game or application.
2. The Homebrew Menu opens: select **LunaNX**.

Do not launch it from the Album (applet mode): streaming needs the memory available in
application mode.

## 3. Sign in to Amazon

1. At first launch, choose your country among the 14 Luna countries (France, Germany, United
   Kingdom, Spain, Italy, Poland, Netherlands, Sweden, Belgium, Austria, Portugal, Luxembourg,
   United States, Canada). LunaNX switches to the country's language; you can change the
   language later in **Settings → Language**.

   > [!NOTE]
   > - **Austria, Portugal, Luxembourg**: these countries have no Amazon store of their own.
   >   LunaNX uses amazon.de (Austria, Luxembourg) or amazon.es (Portugal), the store where your
   >   Prime subscription is. In Luxembourg, if your Prime is on amazon.fr, choose **France**.
   > - **Poland, Netherlands, Sweden, Belgium**: code confirmation uses Amazon's shared
   >   European sign-in service on **amazon.de**. Use your usual Amazon account.
2. Choose **Simplified sign-in (Amazon code)**.
3. Scan the QR code or open the official Amazon `/code` address shown on the Switch.
4. Sign in to Amazon on your phone or PC and enter the short code from the Switch.
5. Wait for the library to appear automatically. The phone can use mobile data or another
   Wi-Fi network. Codes expire after about ten minutes.

Wrong country? Choose **Change country** on the sign-in screen, or change it later in
**Settings → Amazon account**.

The session renews automatically. Amazon may display **Audible for iPhone** on the sign-in
page or in device notifications because the current connection uses that compatibility profile.

## 4. Play

Select a cover with the controller or by touching it. Swipe horizontally to browse games and
change pages. Tap **PLAY** or press controller **A** to launch; touching a cover only selects
it. To leave a game, press **ZL + ZR + −** together.

During a game, press **+ and − together** to show or hide battery level, estimated remaining
battery life, the time, actual stream bitrate and FPS. An autonomy estimate appears after
sufficient discharge has been measured; while charging, the charging status is shown.

## Updating

Replace `switch/LunaNX/LunaNX.nro` with the new version. Your session and settings are kept.

## Uninstalling

1. In LunaNX, choose **Settings → Sign out of Amazon** (this also removes the console from your
   Amazon devices).
2. Delete the `switch/LunaNX/` folder from the SD card.

## Files created on the SD card

| File | Purpose |
| --- | --- |
| `amazon_device.json` | Amazon session (keep it private) |
| `stream_settings.json` | settings |
| `recent_games.json` | your last 10 games (title and artwork only) |
| `runtime.log`, `stream.log`, `signaling.log` | logs (may contain your IP address) |
| `cache/images/` | cover images |

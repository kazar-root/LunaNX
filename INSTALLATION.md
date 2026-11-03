# Installing LunaNX

## Before you start

- A Nintendo Switch running custom firmware (for example Atmosphère) with the Homebrew Menu.
- An Amazon account with access to Amazon Luna in your country.
- An internet connection (5 GHz Wi-Fi close to the router recommended).

## 1. Copy the file

1. Download `LunaNX.nro` from the [Releases](../../releases/latest) page.
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

Do not launch it from the Album (applet mode): the Switch browser needed for sign-in and the
memory needed for streaming are not available there.

## 3. Sign in to Amazon

1. At first launch, choose your Amazon store (France, Germany, United Kingdom, Spain, Italy,
   United States or Canada). LunaNX switches to the store's language; you can change the
   language later in **Settings → Language**.
2. Choose how to sign in:
   - **Sign in with Amazon**: the official Amazon sign-in page opens in the Switch browser. Enter
     your email, your password and, if asked, the verification code. The browser closes by
     itself and your Luna library appears.
   - **Sign in with my phone**, if the Switch browser does not open or shows "This feature is not
     available":
     1. Connect your phone or tablet to the same Wi-Fi network as the Switch.
     2. Scan the QR code shown on the Switch (or type the address shown under it).
     3. On the LunaNX page, tap **1. Open the Amazon sign-in page** and sign in on Amazon.
     4. At the end, Amazon shows a "page not found" page or an error: this is normal. Copy the
        full address of that page from the address bar.
     5. Go back to the LunaNX tab, paste the address and tap **Send to the Switch**. Your Luna
        library appears on the Switch.

Wrong store? Choose **Change store** on the sign-in screen, or change it later in
**Settings → Amazon account**.

The console is added to your Amazon devices as "LunaNX". The session renews automatically.

## 4. Play

Select a cover and press **A**. To leave a game, press **ZL + ZR + −** together.

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

# cod4-live 0.1 for Windows

First Windows multiplayer and dedicated-server release, using classic CoD4
simulation and the Win32/DX9 renderer. This is an early 0.1 build.

## Install

1. Keep **Call of Duty 4: Modern Warfare (2007)** installed through Steam
   (app **7940**) and sign in to the account that directly owns it.
2. Download and run **cod4-live-0.1-windows-setup.exe** from the assets below.
3. Setup detects the game's Steam library. Confirm that source folder or use
   **Browse** to select it manually.
4. Choose a **separate output folder** for cod4-live. A fresh install uses an
   empty folder; selecting an existing cod4-live installation offers repair.
5. Review the included Microsoft runtime terms, install, connect a controller,
   and launch **cod4-live.exe** from the output
   folder or the installer's completion screen.

The installer embeds the project binaries and controller/Bot Warfare additions.
It copies the retail assets from your Steam installation and leaves that source
untouched. It does not compile the game on your PC. No developer tools are needed.
Do not delete your vanilla Steam installation before installing cod4-live.

## Included

- Native controller menus and gameplay, IW4x glyphs, eight button profiles,
  sensitivity/deadzone settings, and optional Steam Input gamepad compatibility.
- Fixed slowdown/rotational assistance and visible-enemy snap on a fresh ADS press.
- Controller-only gameplay enforcement, a 30-second controller reconnect grace,
  and Steam ownership authentication.
- FOV 65 with scale up to 90/65; native ADS zoom and widescreen conversion remain.
- Bot Warfare with inline host count/difficulty settings and up to 63 bots in a
  64-player server, reserving a human slot.
- Dedicated hosting and protocol 10003. Other CoD4 clients are incompatible.

## Validation and limitations

Native USB DualSense menu-to-match play, one-bot Shipment aiming, pickup/reload/
killcam glyphs, 63-bot hosting, real Steam authentication, installation and repair
have been checked. Xbox hardware, PlayStation Bluetooth, Steam Input, additional
weapon/map combinations and two-machine Internet play still need broader testing.
Campaign is outside this release. There is no public master service; use direct
connections, Favorites or LAN. See INSTALL.txt and the included validation guide.

The Windows installer is unsigned. Controller input reports enforce the stock
client's rules; they cannot prove physical input against modified clients or
mouse-to-controller adapters.

The matching cod4-live and OpenAL Soft source archives, license notices and
SHA-256 checksums accompany this installer. Retail game data is not included.

# cod4-live

A controller-focused standalone multiplayer revival of classic Call of Duty 4,
built from KisakCOD with controller technology adapted from IW4x.

Players must directly own **Call of Duty 4: Modern Warfare (2007), Steam app 7940**.
Steam must be running and signed in. The client and server verify a Steam session
ticket before admitting gameplay; a copied game folder is not ownership proof.
Family-shared licenses do not qualify.

The Windows installer asks for the vanilla Steam CoD4 folder and a separate output
folder. It installs our compiled Win32/DX9 client/server and copies the player's
retail assets locally. It never modifies the original Steam installation. Retail
assets and proprietary middleware are excluded from this repository and installer.

Download the [Windows 0.1 installer](https://github.com/JarvisUltronSlop/cod4-live/releases/tag/v0.1)
to install without developer tools. Keep the vanilla Steam game installed, confirm
the detected source folder, and select a separate output folder for cod4-live.

## Controller play

- Native Xbox/XInput and PlayStation input, with a Steam Input compatibility mode.
- Native Controller menu pages, IW4x button artwork, eight button layouts, four
  stick layouts, ten sensitivity presets and custom sensitivity settings.
- Fixed controller slowdown, rotational assistance and weapon-ranged aim snap
  when ADS is pressed. Snap requires a visible enemy and a new ADS press.
- Keyboard/mouse remains available for chat and menus. Gameplay attempts are blocked
  immediately, warned, and disconnected after five seconds of continuing attempts.
  A clean 500 ms interval resets the warning window.
- A disconnected controller neutralizes input and has a 30-second reconnect grace.
- Base FOV is 65. Scale ranges from 1 to 90/65 (about 1.384615). These are CoD4's
  standard 4:3-reference values; native widescreen conversion is retained.
- Protocol 10003 requires matching cod4-live clients and servers.
- Bot Warfare is built in. Start New Server has inline bot count and difficulty rows; the
  bot limit follows the configured player capacity minus one reserved human slot.

Input reports describe what this client observes. They cannot establish physical
hardware identity against modified clients or mouse-to-gamepad adapters.

## Development

Install Git, Python 3, CMake 3.25+, and Visual Studio 2022 C++ tools/Windows SDK.
From PowerShell:

```powershell
./prepare.ps1 -GameDirectory 'D:\SteamLibrary\steamapps\common\Call of Duty 4'
./build.ps1
./scripts/package-installer.ps1 -SkipBuild
./scripts/package-release.ps1
```

Preparation restores pinned build dependencies and copies owned data to ignored
`runtime/`. The client and server are `runtime/cod4-live.exe` and
`runtime/cod4-live_server.exe`. Both use their installation folder when launched.
The standalone installer and corresponding source archive are written to `dist/`.

Read [installation and Steam Input](docs/INSTALLER.md),
[controller settings](docs/CONTROLLER.md), [server connections](docs/NETWORK.md),
[Bot Warfare](docs/BOT_WARFARE.md),
[third-party provenance](docs/THIRD-PARTY.md), and
[validation status](docs/VALIDATION.md). The exact upstream revisions are in
`upstream-lock.json`. Classic simulation is retained; the cod4x1khz 1,000 Hz engine
and Vulkan changes are not included.

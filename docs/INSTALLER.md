# Installing and playing cod4-live

Own and install Steam's Call of Duty 4: Modern Warfare (2007), app 7940.
Modern Warfare Remastered and later games do not provide the correct data.

1. Download the Windows installer from the [GitHub release](https://github.com/JarvisUltronSlop/cod4-live/releases)
   and run it (`cod4-live-0.1-windows-setup.exe` for version 0.1).
2. Select the vanilla Steam CoD4 directory containing `iw3mp.exe`, `main`, `zone`,
   and `localization.txt`. Setup reads Steam's app-7940 installation manifest
   across its libraries; use Browse if automatic detection cannot find it.
3. Choose a separate empty destination and review the included Microsoft runtime
   terms. Setup copies owned assets and middleware,
   validates its embedded payload, and commits the installation transaction.
4. Keep Steam signed in to the owning account, connect a controller, and launch
   `cod4-live.exe` from the destination.

Keep the vanilla Steam installation until setup has copied its data. This is a
precompiled installer: no Visual Studio, Python, CMake or other developer tools
are needed to install and play.

Running setup again against a recognized installation repairs project files while
preserving profiles and server configuration. Close the client and dedicated server
first. Cancellation and failures roll back the transaction; if rollback cannot
finish, setup identifies the retained recovery directory.

## Steam Input

Native mode supports XInput and compatible PlayStation controllers directly.
For Steam Input remapping, add `cod4-live.exe` as a non-Steam game in Steam, set its
working directory to the installation folder, enable Steam Input, select a
**Gamepad** template, and launch it through Steam. Select Steam Input compatibility
in cod4-live's controller settings. That mode consumes virtual XInput and prevents
duplicate physical HID input.

Mouse/keyboard emulation layouts are not accepted for gameplay. Avoid gyro-to-mouse
layouts. Virtual Xbox output may use Xbox prompts even with a PlayStation device.

## Hosting

The native **Start New Server** list has **BOTS** and **BOT DIFFICULTY** rows
directly below **Maximum Players**. Use left/right to adjust them. Zero disables
bots. One slot always remains available for a human;
the maximum follows the selected server capacity (31 bots at 32 slots, or 63 at
64 slots). **Controller** opens native settings pages with IW4x glyphs, layout
profiles, sensitivity presets and custom settings. Keyboard/mouse menus remain
available, while gameplay requires a controller.

Run the dedicated binary from the installed folder:

```powershell
./cod4-live_server.exe +set dedicated 2 +set sv_hostname "cod4-live" +map mp_crash
```

Use a normal CoD4 server configuration for maps and modes. Server authentication
uses Steam's game-server API and requires network access to Steam. Dedicated hosts
need Valve's Steam server runtime; an installed Steam client supplies it on a
gaming PC. A separate Windows host must install the dedicated redistributables
using the supplied `install-server-runtime.ps1` before starting the server. The
server authenticates anonymously and does not require a Steam account login.
Run this from the installed folder on the dedicated host:

```powershell
powershell -ExecutionPolicy Bypass -File .\install-server-runtime.ps1
```

The helper downloads app 1007 anonymously using Valve's SteamCMD and verifies
Valve signatures before installing its Win32 runtime. Re-run it with the server
stopped to update that runtime. These DLLs are downloaded by the host, not bundled
in cod4-live's installer. All human players must pass direct-ownership verification
and controller enforcement. This project does not
claim VAC integration.

## Distribution

The installer contains project binaries and redistributable dependencies, not
retail maps, textures, fastfiles, audio, or original executables. The player's
installation supplies Bink runtime files; audio uses the existing KisakCOD OpenAL backend. Distribute the matching
`cod4-live-source.zip` and `OpenAL-Soft-source.zip` alongside every installer to
provide corresponding source, including the statically linked audio library.
GitHub release assets add their version numbers to these source archive names.
Private GitHub backup alone does not provide source access to binary recipients.

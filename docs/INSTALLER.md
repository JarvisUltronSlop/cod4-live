# Installing and playing cod4-live

Own and install Steam's Call of Duty 4: Modern Warfare (2007), app 7940.
Modern Warfare Remastered and later games do not provide the correct data.

1. Run `cod4-live_setup.exe`.
2. Select the vanilla Steam CoD4 directory containing `iw3mp.exe`, `main`, `zone`,
   and `localization.txt`. Setup detects common Steam library locations.
3. Choose a separate empty destination. Setup copies owned assets and middleware,
   validates its embedded payload, and commits the installation transaction.
4. Keep Steam signed in to the owning account, connect a controller, and launch
   `cod4-live.exe` from the destination.

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

Run the dedicated binary from the installed folder:

```powershell
./cod4-live_server.exe +set dedicated 2 +set sv_hostname "cod4-live" +map mp_crash
```

Use a normal CoD4 server configuration for maps and modes. Server authentication
uses Steam's game-server API and requires network access to Steam. It does not
require a Steam client login on the dedicated host. All human players must pass
direct-ownership verification and controller enforcement. This project does not
claim VAC integration.

## Distribution

The installer contains project binaries and redistributable dependencies, not
retail maps, textures, fastfiles, audio, or original executables. The player's
installation supplies Bink runtime files; audio uses the existing KisakCOD OpenAL backend. Distribute the matching
`cod4-live-source.zip` alongside every installer to provide corresponding source.
Private GitHub backup alone does not provide source access to binary recipients.

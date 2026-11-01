# cod4-live 0.1.1 for Windows

Controller navigation and server-disconnect improvements for the Windows client.

- Server kicks display **Disconnected from the server.** and return to the main
  menu. Disconnecting from a locally hosted match shuts down that listen server.
  The underlying reason remains in the console log.
- Map and server lists accept controller navigation without mouse hover. D-pad
  Up/Down browse; A/Cross confirms the selected row.
- Start New Server adds a native **MAP** row below Bot Difficulty. Left/Right
  chooses a map and updates the preview; choices follow the selected Game Mode.
- **Controller** now appears in the main menu's normal list, between Controls
  and Options, with the same row styling and highlighting.
- Controller settings use CoD4's Video Settings frame, rows and category sidebar.
  B/Circle returns from settings to the category list, then exits on a second press.
  Input Backend changes require Apply and confirmation. An unavailable backend
  automatically reverts after 15 seconds; only a detected controller's backend
  is saved.
- L1/R1 (LB/RB) move between controls, including out of a focused list or slider
  and back to match settings or Start. They also exit name/password editing.
  The menu footer shows these controls.
- Graphics sliders, toggles and choices work with directional navigation.
  Single-choice settings retain focus, and disabled controls reject adjustments.
- Shared menu assets are extended once and restored before shutdown, fixing a
  crash during the return from a match to the frontend.

Download **cod4-live-0.1.1-windows-setup.exe**. Keep Steam CoD4 (2007, app 7940)
installed, confirm the detected source folder, and choose a separate output
folder. Close cod4-live before upgrading; select its existing installation to
update project files while retaining profiles and server configuration.

A new installation needs an empty destination, including hidden files. A source
checkout containing `.git` is not an empty game output directory. The installer
copies your own retail assets; none are included in the release download.

This remains an early Win32/DX9 multiplayer release. Steam direct ownership and
a controller are required. Protocol 10003 is unchanged. Use direct connections,
Favorites or LAN; no public master service is deployed. The installer is unsigned.
Matching source archives, license notices and SHA-256 checksums are included.

Validation: 16 automated suites pass. USB DualSense checks cover graphics Apply
and inline match setup. Remote and local policy kicks return to the frontend;
the local server stops. The real 0.1-to-0.1.1 installer upgrade and repair preserve
profiles/server configuration and leave the Steam source files unchanged.

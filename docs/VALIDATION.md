# Validation record

Tested on Windows on October 31, 2026 with Visual Studio 2022, Win32 Release,
classic KisakCOD simulation and DX9. This is an initial development build; the
remaining acceptance cases below are not claimed as passed.

## Build and automated checks

- Full multiplayer client and dedicated server compile and link successfully.
- Twelve engine CTest suites pass: controller-math, input_policy, bot_policy,
  damage_callback, script_variables, script_references, crosshair_scan,
  native_enum, controller_assets, usercmd_codec, server_input, and steam_auth. The separate
  installer suite also passes.
- Controller tests exercise the vendored IW4x calculations, radial deadzones,
  stick layouts, inversion, slowdown, relative-motion rotation, short-tap
  retention, command-source filtering, controller menu-focus preservation, all
  eight button profiles, inline glyph parsing, and unfocused device presence
  through the server's 30-second disconnect grace.
  Constant-turn integration is checked at 30/60/144/250/1000 Hz with acceleration
  disabled; this does not validate every accelerated aiming trace.
- ADS-snap production-state tests cover press/release/repress, no held retarget,
  blocked/frozen resets, target eligibility, range and region bounds, occlusion,
  smoke thresholds, weapon changes, angle wrapping, spawn/recoil angle offsets,
  the 200 ms activation limit,
  and frame-rate-independent convergence. Crosshair tests compile the production
  scan and cover actual-hit team classification, range, flashbang and smoke.
- Native enum tests compile the production menu key routing and value handler.
  They cover left/right, Enter, focus, release, hidden/disabled controls, and
  one-slot and 64-slot domains. Bot Count and Bot Difficulty now appear directly
  after Maximum Players in the host list and its controller focus order.
- Protocol tests compile production serialization code, cover all signed-byte
  axis pairs, both encoding branches, metadata-only changes, chained commands,
  malformed flags, and truncation. Server tests compile production enforcement
  and cover fresh-command timing, resets, disconnects, frozen pre-admission
  timestamps, replay/restart behavior, bot exemptions and ownership rejection.
- Installer tests cover separate destinations, payload hashes, cancellation,
  repair, rollback, user configuration preservation and unowned-file protection.
- The asset check verifies pinned IW4x source hashes and the lossless IWI8-to-IWI6
  conversion of 45 valid controller images. Eight malformed, unused upstream
  `*_big` images are retained as vendor source but excluded from runtime content.
- Production script allocator tests fill all 262,141 usable child and 65,533
  usable parent slots, exercise 120,000 hashed children, reject overflow, and
  reuse an animation-tree lookup whose child ID is greater than 65,535.
- HUD allocation fills and recycles all 8,192 multiplayer server objects while
  checking that the client's two 31-element HUD arrays are unchanged. Production
  string/vector reference tests reject overflow without corrupting packed flags,
  including concurrent string increments at the 16-bit boundary.

## Live evidence

- The real app-7940 Steam API accepted a directly owned ticket through the
  asynchronous GameServer callback and revoked it after cancellation. Anonymous
  dedicated authentication and destination-bound client tickets passed.
- A compiled dedicated server loaded mp_crash. UDP probes passed nonce exchange,
  rejection before verification, authenticated admission, cancellation disconnect,
  corrupted/full-size-invalid-ticket rejection, and reconnect after cleanup.
- The client launched from outside its installation directory, authenticated to
  the local server, loaded Crash and reached team selection with PlayStation
  prompts. Its profile and assets remained in the standalone runtime directory.
- A native USB DualSense produced 2,523 decoded states during a 30-second check
  through the production driver stack. Both sticks and triggers reached their
  full ranges, all 16 normal mapped buttons were observed, and idle returned to
  zero. Exactly one HID device and no virtual XInput device were used. A 250 ms
  vibration command was submitted; that diagnostic alone did not confirm
  physical vibration. The later gameplay check is recorded below.
- Disabling native HID by selecting Steam Input mode without a virtual XInput
  device triggered the live server's controller-disconnected timeout.
- The settings UI was visually checked, including FOV endpoints 65 and 90 and
  backend selection. A physical menu test exposed stationary mouse events
  overwriting controller focus; that path is fixed and has a regression test.
- After the fix, the user confirmed that DualSense menu selection and gameplay
  both work, including spawning and stick movement/look.
- The expanded native Controller pages passed command-driven debugger startup,
  renderer restart, and listen-server map/disconnect/reload checks without fatal
  exceptions, missing-glyph warnings, or native focus-script errors. The physical
  confirmation above predates these pages and image glyphs. Later visual checks
  verified the general Controller page and all 16 PlayStation mapping glyphs.
  A main-menu layering issue was fixed: the selectable entries now live in
  CoD4's foreground `main_text` menu, with regression coverage. Clicking Controller
  opens its settings page. The temporary Bot Warfare entry was subsequently
  replaced with inline host settings at the user's request.
- The user subsequently confirmed that DualSense navigation works in the new
  Controller menus. The Bot Warfare page was visually checked with separate count
  and difficulty rows, the capacity-minus-one limit, and PlayStation footer glyphs.
  The user confirmed both bot values can be changed with the DualSense; seven
  bots and Regular difficulty persisted across a client restart.
  A black capture was confirmed to be a minimized window; restoring it rendered
  the menus normally.
- The user completed the native host-menu-to-match flow with seven bots and
  answered positively to the gameplay/vibration check, while reporting that aim
  assistance felt incomplete. The user then explicitly requested LT/ADS snap,
  superseding the earlier no-snap plan; that addition needs its own playtest.
- Bot Warfare loaded its packaged scripts and Crash waypoints. Live checks
  verified movement, weapons, difficulty changes, count reduction/removal and
  re-addition, fast restart and full map reload. A 32-slot server sustained 31
  bots with combat scores and responsive status requests. The 64-slot/63-bot
  lifecycle test then passed after bounded script/HUD capacity fixes and
  equivalent early visibility checks: 63 bots plus a reserved human slot,
  difficulty 7, fast and full map restarts with resumed movement, reduction to
  one and zero, and re-adding moving bots. Full-restart validation waits for the
  server ID to change so it cannot accidentally inspect the old map.
  A final active-combat soak with score/time limits disabled only in the test
  configuration measured 12 local status responses at 1.39-24.12 ms (median
  4.04 ms); the number of bots with positive scores grew from 44 to 59. The
  process log had zero script errors. Orphaned projectile damage now uses the
  same world-attacker fallback for player targets as for other entities.
- A real compiled installer payload completed clean installation and repair
  using the owned Steam assets. Client/runtime hashes were restored, profiles
  and server configuration survived, and all 343 source paths, sizes and
  modification times remained unchanged. The installed dedicated executable
  loaded Crash and connected to Steam when launched outside its directory.
- Valve's dedicated runtime helper downloaded app 1007 directly from Valve,
  checked its signatures and x86 DLLs, and passed anonymous server API login
  while loading the downloaded local steamclient.dll.

## Acceptance still required

- Complete the new LT-snap playtest and inline host-count/difficulty checks.
  The earlier zero-only bot count was a one-player capacity left by a boundary
  test, confirmed from the live dvars. The next manual test starts at capacity
  64 with seven bots. Desktop control is currently paused after Escape.
- Broaden physical checks to short taps, held actions, scoreboard, all class/team
  menus, location selection, hotplug and vibration across the other backends.
- Xbox/XInput hardware, PlayStation Bluetooth, and Steam Input virtual-gamepad
  testing require those devices/configurations. Native USB decoding alone does
  not establish their end-to-end behavior.
- Live enemy/teammate and occlusion scenarios, every weapon class, hip/ADS/melee,
  FOV endpoints and multiple render rates need a playtest. Unit calculations do
  not establish that the full aiming feel is finished.
- Held keyboard/mouse and simultaneous stick attempts, clean resets, chat and
  console exemptions, packet loss/reordering, demo playback, and map-change
  behavior have automated/source coverage but need a broader live multiplayer
  acceptance session.
- No non-owning or Family Sharing Steam account was available. Rejection is
  tested in the ownership state machine, not with a second live account. Steam
  outage behavior and a completely clean dedicated host also need testing.
- No public master service is deployed. Use direct addresses, Favorites or LAN;
  see NETWORK.md. No two-machine WAN match has been validated.

Input metadata is a policy report from the client, not physical hardware
attestation. Modified clients and mouse-to-gamepad adapters can misreport it.
